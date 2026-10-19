// save_barrier_restore 只在停写维护窗口运行；凭据只经环境变量输入，不写入计划和回执。
package main

import (
	"bytes"
	"context"
	"crypto/sha256"
	"db/internal/barrierrestore"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"os/signal"
	"shared/authoritysql"
	"strconv"
	"strings"
	"syscall"
	"time"

	"github.com/redis/go-redis/v9"
)

type options struct {
	auditedOwnerUpperBound                                                                       string
	mode, schema, players, maintenanceID, planFile, planSHA, redisAddr, dsnEnv, redisPasswordEnv string
	zone                                                                                         uint
	redisDB                                                                                      int
	routingMode                                                                                  string
	writersStopped, queuesDrained, allowLegacy, localMySQL                                       bool
	mappingAddr, mappingPasswordEnv, routeSchema                                                 string
	mappingDB                                                                                    int
}

func main() {
	var o options
	flag.StringVar(&o.mode, "mode", "plan", "guard | plan | apply；均不自动放流")
	flag.StringVar(&o.schema, "schema", "", "实际玩家 SQL 库名，必须显式指定")
	flag.UintVar(&o.zone, "zone", 0, "home zone，必须显式指定")
	flag.StringVar(&o.players, "players", "", "逗号分隔的玩家号，每批最多1000名")
	flag.StringVar(&o.maintenanceID, "maintenance-id", "", "已备案的停写维护工单号；须与永久 Redis 标记一致")
	flag.StringVar(&o.planFile, "plan", "", "计划文件；plan 新建，apply 只读")
	flag.StringVar(&o.planSHA, "plan-sha256", "", "apply 必填：已审查的原始计划文件 SHA-256")
	flag.StringVar(&o.redisAddr, "redis", "", "父缓存 Redis host:port；不支持 Cluster")
	flag.IntVar(&o.redisDB, "redis-db", 0, "父缓存逻辑 DB；映射仍固定 DB0")
	flag.StringVar(&o.routingMode, "routing-mode", "", "redis-same-instance | redis-split | db_first，必须与真实部署一致")
	flag.StringVar(&o.mappingAddr, "mapping-redis", "", "独立映射 Redis 地址；省略时复用父缓存实例 DB0")
	flag.IntVar(&o.mappingDB, "mapping-redis-db", 0, "映射固定 DB0")
	flag.StringVar(&o.mappingPasswordEnv, "mapping-password-env", "SAVE_BARRIER_RESTORE_MAPPING_PASSWORD", "独立映射 Redis 密码的环境变量名")
	flag.StringVar(&o.routeSchema, "route-schema", "", "db_first 的 SQL 路由库；必须与资产库同一 TiDB 实例")
	flag.StringVar(&o.auditedOwnerUpperBound, "audited-owner-upper-bound", "", "v2 必填：独立保留并审定的旧 owner 上界；当前计数器须严格高于它，不得猜测")
	flag.StringVar(&o.dsnEnv, "dsn-env", "SAVE_BARRIER_RESTORE_DSN", "SQL DSN 所在环境变量名，不传明文")
	flag.StringVar(&o.redisPasswordEnv, "redis-password-env", "SAVE_BARRIER_RESTORE_REDIS_PASSWORD", "Redis 密码所在环境变量名")
	flag.BoolVar(&o.writersStopped, "writers-stopped", false, "声明 scene/login/DB、scene_manager等归属发号者、路由/回填/迁移工具均已停且不能复活")
	flag.BoolVar(&o.queuesDrained, "queues-drained", false, "声明全部 Kafka 世代、retry、processing、dead 已审定排空")
	flag.BoolVar(&o.allowLegacy, "allow-legacy", false, "仅 plan：允许将历史零围栏双表元数据迁到现有 owner/seq1")
	flag.BoolVar(&o.localMySQL, "local-mysql", false, "仅本地 MySQL 验证；默认必须是 TiDB 强读会话")
	flag.Parse()
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	code := run(ctx, o, os.Stdout, os.Stderr)
	stop()
	os.Exit(code)
}

func run(ctx context.Context, o options, out, errOut io.Writer) int {
	if err := validateOptions(o); err != nil {
		fmt.Fprintln(errOut, err)
		return 2
	}
	var plan barrierrestore.Plan
	if o.mode == "apply" {
		var err error
		plan, err = readPlan(o.planFile, o.planSHA)
		if err != nil {
			fmt.Fprintln(errOut, err)
			return 2
		}
		target := o.target()
		if plan.Schema != o.schema || uint(plan.Zone) != o.zone || plan.MaintenanceID != o.maintenanceID || plan.RedisAddr != o.redisAddr || plan.RedisDB != o.redisDB || plan.RoutingMode != o.routingMode || plan.LocalMySQL != o.localMySQL || plan.RouteSchema != o.routeSchema || (plan.Format == barrierrestore.FormatV2 && (plan.MappingAddr != target.MappingAddr || plan.MappingDB != target.MappingDB)) {
			fmt.Fprintln(errOut, "计划目标与执行参数不符")
			return 2
		}
		if plan.AuditedOwnerUpperBound != o.auditedOwnerUpperBound {
			fmt.Fprintln(errOut, "独立审核旧代上界与审定计划不符")
			return 2
		}
	}
	ctx, cancel := context.WithTimeout(ctx, 2*time.Minute)
	defer cancel()
	rc := redis.NewClient(&redis.Options{Addr: o.redisAddr, DB: o.redisDB, Password: os.Getenv(o.redisPasswordEnv), MaxRetries: -1, DialTimeout: time.Second, ReadTimeout: time.Second, WriteTimeout: time.Second, ContextTimeoutEnabled: true})
	defer rc.Close()
	r := barrierrestore.Restorer{Redis: rc, RoutingMode: o.routingMode, LocalMySQL: o.localMySQL, RouteSchema: o.routeSchema}
	r.AuditedOwnerUpperBound = o.auditedOwnerUpperBound
	if o.mappingAddr != "" || o.redisDB != 0 {
		addr, password := o.mappingAddr, os.Getenv(o.mappingPasswordEnv)
		if addr == "" {
			addr, password = o.redisAddr, os.Getenv(o.redisPasswordEnv)
		}
		mapping := redis.NewClient(&redis.Options{Addr: addr, DB: o.mappingDB, Password: password, MaxRetries: -1, DialTimeout: time.Second, ReadTimeout: time.Second, WriteTimeout: time.Second, ContextTimeoutEnabled: true})
		defer mapping.Close()
		r.MappingRedis = mapping
	}
	if o.mode == "guard" {
		if err := r.Guard(ctx, o.target()); err != nil {
			fmt.Fprintln(errOut, "维护闸建立失败，保留已建标记，修复后核查重入")
			return 1
		}
		fmt.Fprintln(out, "永久维护与路由围栏已建立；不会自动停业务或冻结 SQL 路由，不得放流")
		return 0
	}
	db, err := authoritysql.Open(ctx, os.Getenv(o.dsnEnv), o.schema, !o.localMySQL, 1)
	if err != nil {
		fmt.Fprintln(errOut, "权威 SQL 连接或强读会话检查失败")
		return 2
	}
	defer db.Close()
	r.DB = db
	if o.mode == "plan" {
		plan, err = r.Prepare(ctx, o.schema, uint32(o.zone), o.maintenanceID, strings.Split(o.players, ","), o.allowLegacy)
		if err != nil {
			fmt.Fprintln(errOut, err)
			return 1
		}
		body, err := json.MarshalIndent(plan, "", "  ")
		if err != nil {
			fmt.Fprintln(errOut, "计划序列化失败")
			return 2
		}
		body = append(body, '\n')
		if err := writeNew(o.planFile, body); err != nil {
			fmt.Fprintln(errOut, "计划文件创建失败，已有文件不会覆盖")
			return 2
		}
		h := sha256.Sum256(body)
		fmt.Fprintf(out, "plan_sha256=%s players=%d；计划未写业务数据，维护标记保留\n", hex.EncodeToString(h[:]), len(plan.Players))
		return 0
	}
	enc := json.NewEncoder(out)
	for i, row := range plan.Players {
		err := r.ApplyOne(ctx, plan, i)
		status := "committed"
		if err != nil {
			status = "failed"
		}
		if encodeErr := enc.Encode(struct {
			PlayerID string `json:"player_id"`
			Status   string `json:"status"`
		}{row.ID, status}); encodeErr != nil {
			fmt.Fprintln(errOut, "回执写入失败，保持维护；按同计划重跑核查")
			return 1
		}
		if err != nil {
			fmt.Fprintf(errOut, "玩家 %s 恢复失败: %v；已完成前缀不回退，保持维护并按同计划重跑\n", row.ID, err)
			return 1
		}
	}
	fmt.Fprintln(errOut, "全部计划玩家屏障已恢复；维护标记保留，冷加载与资产核对通过前不得放流")
	return 0
}

func validateOptions(o options) error {
	if o.mode != "plan" && o.mode != "apply" && o.mode != "guard" {
		return errors.New("mode 必须是 guard、plan 或 apply")
	}
	if o.zone == 0 || o.zone > 999999 || o.target().ValidateTarget() != nil || o.maintenanceID == "" || len(o.maintenanceID) > 128 || o.redisAddr == "" || o.dsnEnv == "" || o.redisPasswordEnv == "" {
		return errors.New("必须显式提供目标库、home zone、维护号、计划和连接环境变量名")
	}
	if !o.writersStopped || !o.queuesDrained {
		return errors.New("必须先停旧写者、阻止自动复活并审定全部 Kafka/retry/dead 排空；工具无法代替此停写门禁")
	}
	if o.mode == "guard" {
		if o.routingMode == barrierrestore.RoutingModeRedis || o.planFile != "" || o.planSHA != "" || o.players != "" || o.allowLegacy {
			return errors.New("guard 仅建立跨接缝维护闸，不接受计划或资产选项")
		}
		return nil
	}
	if o.planFile == "" {
		return errors.New("必须指定计划文件")
	}
	if o.routingMode != barrierrestore.RoutingModeRedis || o.schema != fmt.Sprintf("zone_%d_db", o.zone) || o.mappingAddr != "" || o.redisDB != 0 || o.auditedOwnerUpperBound != "" {
		n, err := strconv.ParseUint(o.auditedOwnerUpperBound, 10, 64)
		if err != nil || strconv.FormatUint(n, 10) != o.auditedOwnerUpperBound {
			return errors.New("v2 必须指定独立审核旧 owner 上界，不能从回退 SQL 猜测")
		}
	}
	if o.mode == "plan" && (o.players == "" || o.planSHA != "") {
		return errors.New("plan 必须指定玩家集合，不接受旧计划摘要")
	}
	if o.mode == "apply" && (o.planSHA == "" || o.players != "" || o.allowLegacy) {
		return errors.New("apply 必须提供计划摘要，玩家与迁移选项只能取自审定计划")
	}
	return nil
}

func (o options) target() barrierrestore.Plan {
	addr := o.mappingAddr
	if addr == "" {
		addr = o.redisAddr
	}
	return barrierrestore.Plan{Format: barrierrestore.FormatV2, Schema: o.schema, Zone: uint32(o.zone), RedisAddr: o.redisAddr, RedisDB: o.redisDB, RoutingMode: o.routingMode, MappingAddr: addr, MappingDB: o.mappingDB, RouteSchema: o.routeSchema, MaintenanceID: o.maintenanceID, LocalMySQL: o.localMySQL, AuditedOwnerUpperBound: o.auditedOwnerUpperBound}
}

func readPlan(path, digest string) (barrierrestore.Plan, error) {
	var p barrierrestore.Plan
	f, err := os.Open(path)
	if err != nil {
		return p, errors.New("无法读取审定计划")
	}
	defer f.Close()
	b, err := io.ReadAll(io.LimitReader(f, 2*1024*1024+1))
	if err != nil || len(b) > 2*1024*1024 {
		return p, errors.New("计划过大或读取失败")
	}
	h := sha256.Sum256(b)
	if hex.EncodeToString(h[:]) != digest {
		return p, errors.New("审定计划 SHA-256 不符")
	}
	d := json.NewDecoder(bytes.NewReader(b))
	d.DisallowUnknownFields()
	if err := d.Decode(&p); err != nil {
		return p, errors.New("计划格式无效")
	}
	var extra any
	if d.Decode(&extra) != io.EOF {
		return p, errors.New("计划包含额外内容")
	}
	return p, p.Validate()
}

func writeNew(path string, body []byte) error {
	f, err := os.OpenFile(path, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0600)
	if err != nil {
		return err
	}
	_, writeErr := f.Write(body)
	if writeErr == nil {
		writeErr = f.Sync()
	}
	closeErr := f.Close()
	if writeErr != nil {
		return writeErr
	}
	return closeErr
}
