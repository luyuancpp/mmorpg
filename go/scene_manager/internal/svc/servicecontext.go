package svc

import (
	"scene_manager/internal/config"
	"time"

	"github.com/segmentio/kafka-go"
	"github.com/zeromicro/go-zero/core/stores/redis"
	clientv3 "go.etcd.io/etcd/client/v3"
)

type ServiceContext struct {
	Config config.Config
	Redis  *redis.Redis
	Kafka  *kafka.Writer
	Etcd   *clientv3.Client
}

func NewServiceContext(c config.Config) *ServiceContext {
	etcdCli, err := clientv3.New(clientv3.Config{
		Endpoints:   c.Etcd.Hosts,
		DialTimeout: 5 * time.Second,
	})
	if err != nil {
		panic("failed to create etcd client: " + err.Error())
	}

	return &ServiceContext{
		Config: c,
		Redis:  redis.MustNewRedis(c.Redis.RedisConf),
		Kafka: &kafka.Writer{
			Addr:                   kafka.TCP(c.Kafka.Brokers...),
			Balancer:               &kafka.LeastBytes{},
			AllowAutoTopicCreation: true,
		},
		Etcd: etcdCli,
	}
}
