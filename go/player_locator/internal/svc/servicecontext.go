package svc

import (
	"context"
	"fmt"

	"github.com/redis/go-redis/v9"
	kafkago "github.com/segmentio/kafka-go"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/zrpc"

	"player_locator/internal/config"
	smpb "proto/scene_manager"
	"shared/generated/table"
	"shared/kafkacmd"
)

type ServiceContext struct {
	Config             config.Config
	RedisClient        *redis.Client
	KafkaWriter        *kafkago.Writer
	SceneManagerClient smpb.SceneManagerClient
}

func NewServiceContext(c config.Config) *ServiceContext {
	table.LoadTables(c.TableDir, false)

	if c.RedisClient.PoolSize < 0 {
		panic(fmt.Errorf("RedisClient.PoolSize=%d must be >= 0 (0 = go-redis default 10 x GOMAXPROCS)", c.RedisClient.PoolSize))
	}
	rdb := redis.NewClient(&redis.Options{
		Addr:     c.RedisClient.Host,
		Password: c.RedisClient.Password,
		DB:       c.RedisClient.DB,
		// 共享 Redis 连接总账按它记账(0 = go-redis 缺省 10 × GOMAXPROCS),见 config.RedisConf.PoolSize。
		PoolSize: c.RedisClient.PoolSize,
	})

	if err := rdb.Ping(context.Background()).Err(); err != nil {
		panic(fmt.Errorf("failed to connect Redis: %w", err))
	}
	// 生效值(缺省时是 go-redis 算出来的 10 × GOMAXPROCS):核对 ConfigMap 镜像的 PoolSize 是否真的被加载。
	logx.Infof("Redis client %s DB=%d pool_size=%d", c.RedisClient.Host, c.RedisClient.DB, rdb.Options().PoolSize)

	w := &kafkago.Writer{
		Addr: kafkago.TCP(c.Kafka.Brokers...),
		// 控制面命令 topic(gate-cmd_gN)必须按 node_id % P 落到**指定分区**:
		// 消费端 assign 的就是那一个分区,落错分区 = 目标 gate 永远收不到,
		// 而 Kafka 一个错都不报(docs/design/control-plane-topic-partitioning-20260908.md)。
		// kafka-go 的 Writer 在写入路径上忽略 Message.Partition、只问 Balancer。
		//
		// 其它 topic 仍走 Hash 按 Key(player_id)选分区,保证同一玩家的事件有序
		// (项目不变量:kafka key = 业务实体 ID)。LeastBytes 忽略 Key。
		Balancer: &kafkacmd.CommandPartitionBalancer{Fallback: &kafkago.Hash{}},
		// 必须显式设置:kafka-go 直接构造 Writer 时 RequiredAcks 零值是
		// RequireNone(fire-and-forget,写进 socket 即返回 nil,broker 端
		// leader 切换/落盘前崩溃全都不可见)。而 LeaseMonitor 的受理协议
		// 恰恰以 WriteMessages 返回 nil 作为"gate 副作用已完成"的凭据去
		// ack claim —— acks=0 时这个凭据是假的,清理命令会静默丢失且无重试。
		// 兄弟服务 scene_manager 的 servicecontext 已修过同一个坑(RequireOne),
		// 这里对齐。
		RequiredAcks: kafkago.RequireOne,
	}

	// SceneManager zrpc client (via etcd discovery).
	var smClient smpb.SceneManagerClient
	if len(c.SceneManagerRpc.Etcd.Hosts) > 0 {
		cli := zrpc.MustNewClient(c.SceneManagerRpc)
		smClient = smpb.NewSceneManagerClient(cli.Conn())
	}

	return &ServiceContext{
		Config:             c,
		RedisClient:        rdb,
		KafkaWriter:        w,
		SceneManagerClient: smClient,
	}
}

func (s *ServiceContext) Stop() {
	if s.KafkaWriter != nil {
		if err := s.KafkaWriter.Close(); err != nil {
			logx.Errorf("Failed to close Kafka writer: %v", err)
		}
	}
	if s.RedisClient != nil {
		if err := s.RedisClient.Close(); err != nil {
			logx.Errorf("Failed to close Redis client: %v", err)
		}
	}
}
