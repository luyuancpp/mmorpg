package debugutil

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"net"
	"os"
	"strings"

	"github.com/go-sql-driver/mysql"
	"github.com/redis/go-redis/v9"
)

// ── 连接参数与覆盖 ─────────────────────────────────────────────────────
//
// 线上排查的固定做法是 port-forward 到工作站,再用仓库外的一份 yaml 副本连过去。yaml 里的地址是集群内
// 地址、口令也不该明文落盘,所以每类连接都允许用命令行覆盖地址、用环境变量覆盖口令(只读环境变量名,
// 口令本身不上命令行、不进输出)。

// MySQLEndpoint 是一个 MySQL / TiDB 连接目标。
type MySQLEndpoint struct {
	Hosts    string
	User     string
	Password string
	Net      string
	DBName   string
}

// RedisEndpoint 是一个 Redis 连接目标(db.yaml 的 RedisClient / Placement.Redis 形状)。
type RedisEndpoint struct {
	Hosts    string `json:"Hosts,optional"`
	Password string `json:"Password,optional"`
	DB       int    `json:"DB,optional"`
}

// Describe 返回不含口令的 host:port/db,写进导出包的 sources。
func (e RedisEndpoint) Describe() string { return fmt.Sprintf("%s/%d", e.Hosts, e.DB) }

// Describe 返回不含口令的 host/库名。
func (e MySQLEndpoint) Describe() string { return e.Hosts + "/" + e.DBName }

// PasswordFromEnv 读环境变量里的口令;envName 为空时返回 (fallback, nil)。变量名给了却没设置是操作失误,报错而不是退回 yaml。
func PasswordFromEnv(envName, fallback string) (string, error) {
	envName = strings.TrimSpace(envName)
	if envName == "" {
		return fallback, nil
	}
	value, ok := os.LookupEnv(envName)
	if !ok {
		return "", fmt.Errorf("环境变量 %s 未设置", envName)
	}
	return value, nil
}

// OpenMySQL 打开一个连接池并 ping。库名写进 DSN(不靠 USE 切库);maxOpen 至少为 1。
//
// interpolateParams 保持默认关闭:参数走预处理语句的二进制协议,BIGINT UNSIGNED 读成 uint64,不经十进制字符串往返。
func OpenMySQL(ctx context.Context, ep MySQLEndpoint, maxOpen int) (*sql.DB, error) {
	cfg, err := mysqlConfig(ep)
	if err != nil {
		return nil, err
	}
	connector, err := mysql.NewConnector(cfg)
	if err != nil {
		return nil, fmt.Errorf("build mysql connector: %w", err)
	}
	db := sql.OpenDB(connector)
	if maxOpen < 1 {
		maxOpen = 1
	}
	db.SetMaxOpenConns(maxOpen)
	db.SetMaxIdleConns(maxOpen)
	if err := db.PingContext(ctx); err != nil {
		_ = db.Close()
		return nil, fmt.Errorf("ping mysql %s: %w", ep.Describe(), err)
	}
	return db, nil
}

// mysqlConfig 构造驱动配置。字符集必须走 mysql.Charset 选项:直接塞进 Params 的键会在每条连接上被原样拼成
// `SET charset = utf8mb4` 执行(v1.8 起只有 ParseDSN 会把 charset 转成 SET NAMES),服务端报错、建连全部失败。
func mysqlConfig(ep MySQLEndpoint) (*mysql.Config, error) {
	cfg := mysql.NewConfig()
	cfg.User = ep.User
	cfg.Passwd = ep.Password
	cfg.Net = strings.TrimSpace(ep.Net)
	if cfg.Net == "" {
		cfg.Net = "tcp"
	}
	cfg.Addr = ep.Hosts
	cfg.DBName = ep.DBName
	cfg.ParseTime = true
	if err := cfg.Apply(mysql.Charset("utf8mb4", "")); err != nil {
		return nil, fmt.Errorf("set mysql charset: %w", err)
	}
	return cfg, nil
}

// MySQLServerIdentity 返回实例身份(@@server_uuid)。取不到(例如 TiDB 不支持该变量)时返回空串,调用方按「未知」处理。
func MySQLServerIdentity(ctx context.Context, q Queryer) string {
	rows, err := q.QueryContext(ctx, "SELECT @@server_uuid")
	if err != nil {
		return ""
	}
	defer rows.Close()
	var id sql.NullString
	if !rows.Next() || rows.Scan(&id) != nil {
		return ""
	}
	return id.String
}

// RedisServerIdentity 返回 INFO server 里的 run_id(进程级唯一);取不到时返回空串。
func RedisServerIdentity(ctx context.Context, client redis.Cmdable) string {
	info, err := client.Info(ctx, "server").Result()
	if err != nil {
		return ""
	}
	for _, line := range strings.Split(info, "\n") {
		if value, ok := strings.CutPrefix(strings.TrimSpace(line), "run_id:"); ok {
			return value
		}
	}
	return ""
}

// OpenRedis 打开一个单实例客户端并 PING。导出与导入都只连单实例:父键、owner 与屏障在同一实例
// (login parent_commit_barrier 的前提),指向 Redis Cluster 时 MULTI 跨槽会直接报错。
func OpenRedis(ctx context.Context, ep RedisEndpoint) (*redis.Client, error) {
	if strings.TrimSpace(ep.Hosts) == "" {
		return nil, errors.New("redis 地址为空")
	}
	if strings.Contains(ep.Hosts, ",") {
		return nil, fmt.Errorf("redis 地址 %q 含多个节点;本工具只连单实例", ep.Hosts)
	}
	client := redis.NewClient(&redis.Options{Addr: ep.Hosts, Password: ep.Password, DB: ep.DB})
	if err := client.Ping(ctx).Err(); err != nil {
		_ = client.Close()
		return nil, fmt.Errorf("ping redis %s: %w", ep.Describe(), err)
	}
	return client, nil
}

// ── 写目标守卫 ─────────────────────────────────────────────────────────

// CheckLocalWriteTarget 是导入工具的写前闸门:目标主机必须是本机回环地址,或在 allowHosts(逗号分隔,
// 精确匹配主机名)里显式放行。线上数据只许进隔离环境,把生产 yaml 误当本地 yaml 是最可能的事故。
//
// 它挡不住「127.0.0.1 上是 port-forward 出来的生产实例」:导入端还要比对实例身份(导出包 sources 里记的
// Redis run_id / MySQL server_uuid),目标就是导出来源时拒绝,见 debug_import 的 checkNotExportSource。
func CheckLocalWriteTarget(what, hostPort, allowHosts string) error {
	host := hostPort
	if h, _, err := net.SplitHostPort(hostPort); err == nil {
		host = h
	}
	host = strings.Trim(strings.TrimSpace(host), "[]")
	if host == "" {
		return fmt.Errorf("%s 目标地址为空", what)
	}
	if IsLoopbackHost(host) {
		return nil
	}
	for _, allowed := range strings.Split(allowHosts, ",") {
		if allowed = strings.TrimSpace(allowed); allowed != "" && strings.EqualFold(allowed, host) {
			return nil
		}
	}
	return fmt.Errorf("%s 目标 %q 不是本机回环地址;导入只写隔离环境。确是本地 docker 服务名时用 -allow-target-host=%s 显式放行", what, hostPort, host)
}

// IsLoopbackHost 只认 localhost 与回环 IP;不做 DNS 解析(解析结果可被篡改,也会把线上域名误判成本机)。
func IsLoopbackHost(host string) bool {
	if strings.EqualFold(host, "localhost") {
		return true
	}
	ip := net.ParseIP(host)
	return ip != nil && ip.IsLoopback()
}
