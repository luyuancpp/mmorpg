package routing

// db_first 下 W1 发布 + WAITAOF 专用的 go-redis 客户端(A2-1 / LR-4,2026-10-20,负责人拍板「现在就做」)。
//
// 为什么不再用 r.mappingRedis(go-zero 的 *redis.Redis):go-zero v1.10.0 按地址只建一个底层 go-redis 客户端
// (redisclientmanager.go getClient),池上限是 go-redis 缺省 10 × GOMAXPROCS(K8s 上 20 条),熔断钩子只在第一次建客户端时挂上。
// 发布与 GetPlayerHomeZone / BatchGetPlayerHomeZone / 写门禁的 R2 / 玩家锁 / 读脚本共用这一个池与这一个熔断器:
//   - 每次 W1 的 EVAL + WAITAOF 占住一条连接直到下一次 fsync(everysec 下平均约 0.5s),建角尖峰把池占满,读路径排队到 gRPC 截止;
//   - 发布脚本的确定性错误回复(畸形 maprev、WRONGTYPE)与取连接超时都计进同一个熔断器,熔断一开读路径被按概率丢弃
//     (Codex 10-19 复现:同一客户端上 8 次预期 Lua 错误后,20 轮里 7 轮 circuit breaker is open)。
// 同地址再 NewRedis / MustNewRedis 也没用:go-zero 会把它并回同一个池与熔断器。所以这里直接建 go-redis 客户端。
//
// 契约:
//   - 只在 RouteStore.Mode=db_first 时由 NewRouter 构造;redis 模式不建、不开连接(go-redis 懒连接,构造本身也不拨号)。
//   - 连接参数与 MappingRedis 同源:Host / User / Pass / Tls 取同一份 redis.RedisConf,按 go-zero getClient 的同一口径换算
//     (DB 恒 0、Tls=true 时 InsecureSkipVerify);Type 只许空或 node —— 发布脚本是多键脚本,WAITAOF 按单实例连接的写偏移定义。
//     不另设地址 / 口令配置键,不另写一套解析。
//   - 池:PoolSize = RouteStore.PublishRedisPoolSize(缺省 20,论证见 config.DefaultRouteStorePublishRedisPoolSize),MinIdleConns 0
//     (不预建空闲连接,不用就不占 mapping Redis 的连接)。
//   - 超时:ReadTimeout = WaitAOFTimeoutMs + 1s(WAITAOF 的服务端超时先于客户端读超时到期,同 go/db durable_enqueue.go 与
//     routereconcile.NewRedisCache);ContextTimeoutEnabled = true:WAITAOF 的预算在取连接**之前**按 ctx 剩余算好,池满时排队等连接
//     会吃掉一部分剩余时间,不随 ctx 截止的话 ctx 已到期 WAITAOF 还会把连接占到 ReadTimeout;取连接的排队本身恒受 ctx 约束。
//   - 重试:MaxRetries = -1 关掉 go-redis 对整条 pipeline 的自动重发(v9.18.0 options.go:-1 = 不重试),重发只由 publishRoute 的
//     PublishAttempts 一层负责(每次重算 WAITAOF 预算)。
//   - **不挂熔断器**:失败面已经由 PublishAttempts(≤ 5)、退避与 ctx 截止有界;发布脚本对单个玩家的确定性错误回复不该让别的玩家的
//     发布被按概率拒绝(那正是 go-zero 熔断器在这里造成的问题)。池满或 Redis 不可达时只有 W1 在 ctx 截止前排队 / 失败,读路径不受影响。
//   - 同一次发布的 EVAL 与 WAITAOF 仍在同一个 pipeline、同一条连接上(go-redis 单节点 pipeline 一次取一条连接跑完整批),
//     WAITAOF 确认的就是这次发布的写;TestPublishRoute_RealRedisSameConnection 在真 Redis 上验这一条。
//   - 生命周期:Router.Close 关闭它(注入的客户端也归 Router 所有)。

import (
	"crypto/tls"
	"errors"
	"fmt"
	"strings"
	"time"

	"data_service/internal/config"

	goredis "github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

// publishRedisReadTimeoutSlack 是发布客户端读超时在 WaitAOFTimeoutMs 之外多给的时间(同 go/db durableClientReadTimeoutSlack)。
const publishRedisReadTimeoutSlack = time.Second

// probeRedisPoolSize 是 ProbeWaitAOF 临时客户端的池大小:探针只发一条 WAITAOF,探完即关。
const probeRedisPoolSize = 1

// mappingRedisOptions 把 MappingRedis 配置换算成 go-redis 单实例客户端的参数(纯函数,单测直接测)。
// 连接身份(地址、账号、口令、TLS、DB)与 go-zero getClient 对同一份 redis.RedisConf 的换算逐项相同;池、超时与重试由调用方给。
func mappingRedisOptions(mapping redis.RedisConf, poolSize int, readTimeout time.Duration) (*goredis.Options, error) {
	if mapping.Type != "" && mapping.Type != redis.NodeType {
		return nil, fmt.Errorf("route publish needs a single-instance mapping redis (MappingRedis.Type=%q, want %q): "+
			"the publish script is multi-key and WAITAOF confirms one connection's writes on one instance", mapping.Type, redis.NodeType)
	}
	if strings.TrimSpace(mapping.Host) == "" {
		return nil, errors.New("route publish: MappingRedis.Host is empty")
	}
	if poolSize < 1 {
		return nil, fmt.Errorf("route publish: pool size must be positive, got %d", poolSize)
	}
	var tlsConfig *tls.Config
	if mapping.Tls {
		// 与 go-zero redisclientmanager.go 的 getClient 同口径(Tls=true 时 InsecureSkipVerify),同一份 MappingRedis 配置两边行为一致。
		tlsConfig = &tls.Config{InsecureSkipVerify: true} //nolint:gosec // 与 go-zero 对同一配置项的处理一致
	}
	return &goredis.Options{
		Addr:                  mapping.Host,
		Username:              mapping.User,
		Password:              mapping.Pass,
		DB:                    0,
		PoolSize:              poolSize,
		MinIdleConns:          0,
		MaxRetries:            -1,
		ReadTimeout:           readTimeout,
		ContextTimeoutEnabled: true,
		TLSConfig:             tlsConfig,
	}, nil
}

// publishRedisReadTimeout 是发布 / 探针客户端的读超时:WaitAOFTimeoutMs + publishRedisReadTimeoutSlack。rc 必须是 Normalize 过的。
func publishRedisReadTimeout(rc config.RouteStoreConfig) time.Duration {
	return time.Duration(rc.WaitAOFTimeoutMs)*time.Millisecond + publishRedisReadTimeoutSlack
}

// newPublishRedis 建 W1 发布专用客户端(不拨号)。rc 必须是 Normalize + Validate 过的 RouteStore 段。
func newPublishRedis(mapping redis.RedisConf, rc config.RouteStoreConfig) (*goredis.Client, error) {
	opts, err := mappingRedisOptions(mapping, rc.PublishRedisPoolSize, publishRedisReadTimeout(rc))
	if err != nil {
		return nil, err
	}
	return goredis.NewClient(opts), nil
}

// withPublishRedis 注入发布客户端(只给单测:接 miniredis、挂钩子、限池)。注入了 NewRouter 就不自建;注入的客户端归 Router 所有,
// Router.Close 时关闭。redis 模式下注入同样生效(只是 redis 模式不发布,用不到它)。
func withPublishRedis(c *goredis.Client) RouterOption {
	return func(r *Router) { r.publishRedis = c }
}
