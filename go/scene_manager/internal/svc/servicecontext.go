package svc

import (
	"scene_manager/internal/config"

	"github.com/segmentio/kafka-go"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

type ServiceContext struct {
	Config config.Config
	Redis  *redis.Redis
	Kafka  *kafka.Writer
}

func NewServiceContext(c config.Config) *ServiceContext {
	return &ServiceContext{
		Config: c,
		Redis:  redis.MustNewRedis(c.Redis),
		Kafka: &kafka.Writer{
			Addr:     kafka.TCP(c.Kafka.Brokers...),
			Balancer: &kafka.LeastBytes{},
		},
	}
}
