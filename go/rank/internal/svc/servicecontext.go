// Package svc 是 rank 服务的依赖装配:etcd 客户端、榜键所在的 RankRedis(没配时回落共享库)、
// 名字补全用的 data_service 客户端。
//
// 与 mail / friend 一样,本包**不含** Prometheus 指标(指标在 internal/metrics,懒注册,由 main 调用)。
// rank v1 零 MySQL(设计 docs/design/leaderboard-system.md §5.3),所以这里没有数据库连接。
package svc

import (
	"fmt"
	"sync"
	"time"

	"rank/internal/config"
	"rank/internal/data"

	dspb "proto/data_service"

	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/stores/redis"
	"github.com/zeromicro/go-zero/zrpc"
	clientv3 "go.etcd.io/etcd/client/v3"
)

// etcdDialTimeout 与 friend / mail 同值:etcd 不可达时启动期要有上限,不让 K8s 的 startupProbe 先判失败。
const etcdDialTimeout = 5 * time.Second

// ServiceContext 持有全部依赖。构造后只读;Stop 只生效一次。
type ServiceContext struct {
	Config config.Config

	// Etcd 供 noderegistry 注册与 killswitch 监听共用(Etcd.Key 为空,go-zero 不写自己的 key,契约 D-13)。
	Etcd *clientv3.Client

	// RankRedis 是榜键 lb:{rating}:* 所在的句柄:RankRedis 配了用它,否则回落共享 Redis(启动时 WARN,§3.5)。
	// go-zero 的 redis.Redis 没有 Close,不由业务关。
	RankRedis *redis.Redis
	// Board 是 match 评分全服榜(v1 只有这一张,§3.4),读路径与快照消费者共用。
	Board *data.Board

	// DataService 是名字补全(BatchGetPlayerName)用的 data_service 客户端;dataServiceConn 是它的连接,Stop 时关。
	DataService     dspb.DataServiceClient
	dataServiceConn interface{ Close() error }

	stopOnce sync.Once
}

// NewServiceContext 建全部依赖。任何一项建不出来都返回错误(启动致命);已建的资源在返回前关闭。
//
// RankRedis 与 data_service 都按 NonBlock 建连:Redis 暂不可达时读请求 in-band 回 kServiceUnavailable、消费者退避重试;
// data_service 暂不可达时名字留空(fail-open)。只有 etcd 是硬依赖 —— 没有它注册不了,路由服看不见本节点。
func NewServiceContext(c config.Config) (*ServiceContext, error) {
	sc := &ServiceContext{Config: c}
	ok := false
	defer func() {
		if !ok {
			sc.Stop()
		}
	}()

	var err error
	sc.Etcd, err = clientv3.New(clientv3.Config{Endpoints: c.Etcd.Hosts, DialTimeout: etcdDialTimeout})
	if err != nil {
		return nil, fmt.Errorf("rank: create etcd client: %w", err)
	}

	rankRedisConf := c.EffectiveRankRedis()
	if c.RankRedisFallsBack() {
		// 本地 compose 可以回落;staging / prod 必须是独立实例且 noeviction —— 榜键无 TTL,在会淘汰的共享库上会被静默驱逐(§3.5)。
		logx.Errorf("[rank] WARN RankRedis not configured; leaderboard keys lb:{rating}:* fall back to the shared Redis host=%s type=%s -- "+
			"acceptable for local development only, production needs a dedicated noeviction instance (docs/design/leaderboard-system.md §3.5)",
			rankRedisConf.Host, rankRedisConf.Type)
	}
	sc.RankRedis, err = redis.NewRedis(rankRedisConf)
	if err != nil {
		return nil, fmt.Errorf("rank: RankRedis %s: %w", rankRedisConf.Host, err)
	}
	sc.Board = data.NewRatingBoard(sc.RankRedis)

	conn, err := zrpc.NewClient(c.DataServiceRpc)
	if err != nil {
		return nil, fmt.Errorf("rank: data_service client (BatchGetPlayerName for leaderboard names): %w", err)
	}
	sc.dataServiceConn = conn.Conn()
	sc.DataService = dspb.NewDataServiceClient(conn.Conn())

	ok = true
	return sc, nil
}

// Stop 关闭 data_service 连接与 etcd 客户端。幂等;构造失败时也会被调用,所以每一项都判空。
func (sc *ServiceContext) Stop() {
	sc.stopOnce.Do(func() {
		if sc.dataServiceConn != nil {
			if err := sc.dataServiceConn.Close(); err != nil {
				logx.Errorf("[rank] data_service connection close failed: %v", err)
			}
		}
		if sc.Etcd != nil {
			if err := sc.Etcd.Close(); err != nil {
				logx.Errorf("[rank] etcd client close failed: %v", err)
			}
		}
	})
}
