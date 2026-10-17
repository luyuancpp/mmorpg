// Package config 是 rank 服务的配置结构与启动期校验(docs/design/leaderboard-system.md §5.2)。
//
// 键名与 etc/rank.yaml、tools/scripts/k8s_deploy.ps1 的 rank ConfigMap 三处逐字一致;改名必须三处同改,
// 否则 ConfigMap 里的值会被 go-zero 当未知键静默忽略 —— 这类事故没有任何报错,只会表现为"线上跑的还是默认值"。
//
// rank 是全局服务(全服一份、多副本、进程无状态,契约 §2):ZoneId 只影响 etcd 注册路径,业务代码禁止读它做分支。
package config

import (
	"errors"
	"fmt"
	"net"
	"strconv"
	"time"

	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/core/stores/redis"
	"github.com/zeromicro/go-zero/zrpc"
)

// MaxRpcTimeoutMs / MinRpcTimeoutMs 是 zrpc 服务端整体超时的区间(毫秒),与 mail / friend 同一口径:
// 上限 = 路由服 ForwardTimeoutMs(默认 5000)− 1000(契约 §3);下限保证扣掉 InBandReplyReserve 后仍有 I/O 预算。
const (
	MaxRpcTimeoutMs int64 = 4000
	MinRpcTimeoutMs int64 = 1000
)

// InBandReplyReserve 是整请求业务预算相对 zrpc 服务端 Timeout 预留的余量:go-zero 的超时拦截器到点直接回
// DeadlineExceeded,handler 之后产出的 in-band kServiceUnavailable 会被丢掉(照 mail / friend)。
const InBandReplyReserve = 500 * time.Millisecond

// DefaultRatingConsumerGroup 是消费评分快照的消费组(设计 §2.4 / §3.5 的 offset 重放手册按这个名字写)。
const DefaultRatingConsumerGroup = "rank-rating"

// Config 是 rank 服务的全部配置。
//
// 共享 Redis **不在这里重新声明**:嵌入的 zrpc.RpcServerConf 已带 `Redis redis.RedisKeyConf`(yaml 顶层 `Redis:`)。
// rank 不读任何跨运行时契约 key,共享库只在 RankRedis 没配时作为回落目标(§3.5:本地 compose 回落并 WARN)。
type Config struct {
	zrpc.RpcServerConf

	// ZoneId:只影响 etcd 注册路径 <Prefix>/zone/<z>/...。必填且拒 0;业务代码禁止读它做分支。
	ZoneId uint32 `json:",optional"`

	// LeaseTTL:etcd 节点注册租约 TTL(秒),即崩溃场景的切换窗口(路由服 PickRandom 不看连接状态)。
	LeaseTTL int64 `json:",default=60"`

	// MetricsListenAddr:Prometheus /metrics 监听地址,留空关闭。rank 用 :9250(设计 §5.1)。
	// 必须是 yaml 顶层单行:go_services.ps1 -Zone 的端口位移正则只认这个形状。
	MetricsListenAddr string `json:",optional"`

	// KillSwitchPrefix:RPC 级热关停规则在 etcd 里的前缀,留空用 /mmorpg/killswitch/。
	KillSwitchPrefix string `json:",optional"`

	// RankRedis:排行榜私有键的句柄(契约 §4 的独立句柄;段名不能叫 Redis —— zrpc.RpcServerConf 已有同名字段)。
	// Host 为空 = 回落共享 Redis 并在启动时 WARN(§3.5);staging / prod 必须是独立实例、noeviction。
	RankRedis redis.RedisConf `json:",optional"`

	// Kafka:评分快照的 broker 地址。Brokers 为空合法 = 不消费(只读;横幅显示),榜不会更新。
	Kafka KafkaConf `json:"Kafka,optional"`

	// RatingSnapshotTopic / RatingSnapshotTopicPartitions:与 match 共享的 topic 契约(match_service.yaml 同名键必须同值)。
	// **不在这里配保留期**:保留期只由 match 声明,rank Ensure 时传 RetentionMs: -1(§2.4)。
	RatingSnapshotTopic           string `json:",default=match-rating-snapshot"`
	RatingSnapshotTopicPartitions int32  `json:",default=3"`

	// RatingConsumerGroup:消费组。键名刻意不叫 GroupID(go_services.ps1 -Zone 会给 GroupID 加 _z<N>,
	// 全局 topic 会被各 zone 的 rank 各消费一遍;§5.2)。
	RatingConsumerGroup string `json:",default=rank-rating"`

	// DataServiceRpc:名字补全(BatchGetPlayerName,fail-open)的 data_service 客户端。写 Key: dataservice.rpc 不违反 D-13
	// (data_service 按 zone 部署,同 go/trade)。不标 optional:没有它所有名字都是空的,应在启动期显式配置。
	DataServiceRpc zrpc.RpcClientConf `json:"DataServiceRpc"`

	// BoardSizeSampleInterval:rank_board_size 的 ZCARD 采样间隔(每个副本各自采样)。
	BoardSizeSampleInterval time.Duration `json:",default=30s"`
}

// KafkaConf 是评分快照 topic 的 broker 地址(形状与 friend / mail 一致)。
type KafkaConf struct {
	Brokers []string `json:"Brokers,optional"`
}

// RequestBudget 返回一次请求内全部 I/O(RankRedis、名字补全)共用的业务预算。Validate 保证结果恒为正。
func (c Config) RequestBudget() time.Duration {
	return time.Duration(c.Timeout)*time.Millisecond - InBandReplyReserve
}

// ConsumerEnabled 报告是否配了 broker(没配 = 只读服务,不消费快照)。
func (c Config) ConsumerEnabled() bool {
	return len(c.Kafka.Brokers) > 0
}

// RankRedisFallsBack 报告 RankRedis 是否没配、要回落共享库(§3.5:本地可以,线上必须独立实例)。
func (c Config) RankRedisFallsBack() bool {
	return c.RankRedis.Host == ""
}

// EffectiveRankRedis 返回实际用于榜键的 Redis 配置:RankRedis 配了用它,否则回落共享 Redis。
func (c Config) EffectiveRankRedis() redis.RedisConf {
	if c.RankRedisFallsBack() {
		return c.Redis.RedisConf
	}
	return c.RankRedis
}

// ListenHostPort 把 ListenOn 拆成 host 与端口号(net.SplitHostPort 正确处理 "[::]:51100")。
func (c *Config) ListenHostPort() (string, uint32, error) {
	host, portStr, err := net.SplitHostPort(c.ListenOn)
	if err != nil {
		return "", 0, fmt.Errorf("ListenOn 必须是 host:port,得到 %q: %w", c.ListenOn, err)
	}
	port, err := strconv.ParseUint(portStr, 10, 16)
	if err != nil || port == 0 {
		return "", 0, fmt.Errorf("ListenOn 端口非法: %q", c.ListenOn)
	}
	return host, uint32(port), nil
}

// IsRelaxedMode 报告 go-zero Mode 是否为 dev / test(与 friend / mail 同一判据);rank 只用它决定是否注册 gRPC reflection。
func IsRelaxedMode(mode string) bool {
	return mode == service.DevMode || mode == service.TestMode
}

// HasRpcTarget 报告一个 zrpc 客户端配置有没有可拨的目标(同 go/trade / go/mail 的判据)。
func HasRpcTarget(c zrpc.RpcClientConf) bool {
	return c.Etcd.Key != "" || len(c.Endpoints) > 0 || c.Target != ""
}

// Validate 做启动期整体校验(fail-fast)。conf.MustLoad 会自动调用它(go-zero v1.10.0)。
// 每一条拒绝的都是"进程能起来、但行为静默出错"的配置,理由写在各分支旁边。
//
// 先调内嵌 RpcServerConf 的 Validate:本方法会遮蔽那个被提升的方法,不显式调用的话加载期就少了它那项校验
// (Auth=true 时要求 Redis),失败时间点会推迟到 zrpc.NewServer(照 go/match 的同一处修法)。
func (c *Config) Validate() error {
	if err := c.RpcServerConf.Validate(); err != nil {
		return err
	}
	// ZoneId=0:注册路径变成 <Prefix>/zone/0/...,路由服看不见本节点,零报错。
	if c.ZoneId == 0 {
		return errors.New("ZoneId 必须非 0(K8s 全局服务用 ConfigMap 注入的 CurrentZoneId)")
	}
	if c.ListenOn == "" {
		return errors.New("ListenOn 不能为空")
	}
	if _, _, err := c.ListenHostPort(); err != nil {
		return err
	}
	if c.Timeout < MinRpcTimeoutMs || c.Timeout > MaxRpcTimeoutMs {
		return fmt.Errorf("Timeout(%d ms)必须在 [%d, %d] 内:上限是路由服 ForwardTimeoutMs − 1000(契约 §3),"+
			"下限保证扣掉 %v 回包余量后仍有 I/O 预算", c.Timeout, MinRpcTimeoutMs, MaxRpcTimeoutMs, InBandReplyReserve)
	}
	if len(c.Etcd.Hosts) == 0 {
		return errors.New("Etcd.Hosts 不能为空(节点注册与 killswitch 都依赖 etcd)")
	}
	// Etcd.Key 非空:go-zero 会额外注册 rank.rpc,并被 go_services.ps1 -Zone 按 .z<N> 切分 —— 与 noderegistry
	// 的全服一份语义冲突(D-13)。yaml 必须显式写 `Key: ""`,整行省略会在加载期 Fatal。
	if c.Etcd.Key != "" {
		return fmt.Errorf("Etcd.Key 必须留空(得到 %q):全局服务不注册 go-zero key,契约 D-13", c.Etcd.Key)
	}
	if c.LeaseTTL <= 0 {
		return fmt.Errorf("LeaseTTL 必须为正数,得到 %d", c.LeaseTTL)
	}

	// 榜键的句柄:RankRedis 与共享 Redis 至少有一个可用。两者都空 = 每个请求都读不到榜。
	if c.EffectiveRankRedis().Host == "" {
		return errors.New("RankRedis.Host 与 Redis.Host 都为空:榜键 lb:{rating}:* 没有可用的 Redis")
	}

	if c.ConsumerEnabled() {
		if c.RatingSnapshotTopic == "" {
			return errors.New("RatingSnapshotTopic 不能为空(与 match 的同名键同值)")
		}
		if c.RatingSnapshotTopicPartitions <= 0 {
			return fmt.Errorf("RatingSnapshotTopicPartitions 必须为正数,得到 %d(与 match 的同名键同值)", c.RatingSnapshotTopicPartitions)
		}
		if c.RatingConsumerGroup == "" {
			return errors.New("RatingConsumerGroup 不能为空(offset 重放手册按 " + DefaultRatingConsumerGroup + " 写)")
		}
	}

	if !HasRpcTarget(c.DataServiceRpc) {
		return errors.New("DataServiceRpc 必须配置 Etcd.Key / Endpoints / Target 之一:页内玩家的名字从 data_service 补全")
	}
	if c.BoardSizeSampleInterval <= 0 {
		return fmt.Errorf("BoardSizeSampleInterval 必须为正数,得到 %v", c.BoardSizeSampleInterval)
	}
	return nil
}
