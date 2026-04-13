package svc

import (
	"context"
	"fmt"

	"github.com/redis/go-redis/v9"
	kafkago "github.com/segmentio/kafka-go"
	"github.com/zeromicro/go-zero/core/logx"

	"player_locator/internal/config"
)

type ServiceContext struct {
	Config      config.Config
	RedisClient *redis.Client
	KafkaWriter *kafkago.Writer
}

func NewServiceContext(c config.Config) *ServiceContext {
	rdb := redis.NewClient(&redis.Options{
		Addr:     c.RedisClient.Host,
		Password: c.RedisClient.Password,
		DB:       c.RedisClient.DB,
	})

	if err := rdb.Ping(context.Background()).Err(); err != nil {
		panic(fmt.Errorf("failed to connect Redis: %w", err))
	}

	w := &kafkago.Writer{
		Addr:     kafkago.TCP(c.Kafka.Brokers...),
		Balancer: &kafkago.LeastBytes{},
	}

	return &ServiceContext{
		Config:      c,
		RedisClient: rdb,
		KafkaWriter: w,
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
