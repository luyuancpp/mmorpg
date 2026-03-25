package svc

import (
	"context"
	"database/sql"
	"fmt"

	_ "github.com/go-sql-driver/mysql"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"

	"friend/internal/config"
)

type ServiceContext struct {
	Config      config.Config
	RedisClient *redis.Client
	DB          *sql.DB
}

func NewServiceContext(c config.Config) *ServiceContext {
	rdb := redis.NewClient(&redis.Options{
		Addr:     c.RedisClient.Host,
		Password: c.RedisClient.Password,
		DB:       c.RedisClient.DB,
	})

	if err := rdb.Ping(context.Background()).Err(); err != nil {
		panic(fmt.Errorf("failed to connect global Redis: %w", err))
	}

	db, err := sql.Open("mysql", c.MySQL.DataSource)
	if err != nil {
		panic(fmt.Errorf("failed to connect MySQL: %w", err))
	}
	if err := db.Ping(); err != nil {
		panic(fmt.Errorf("failed to ping MySQL: %w", err))
	}

	return &ServiceContext{
		Config:      c,
		RedisClient: rdb,
		DB:          db,
	}
}

func (s *ServiceContext) Stop() {
	if s.RedisClient != nil {
		if err := s.RedisClient.Close(); err != nil {
			logx.Errorf("Failed to close Redis client: %v", err)
		}
	}
	if s.DB != nil {
		if err := s.DB.Close(); err != nil {
			logx.Errorf("Failed to close MySQL: %v", err)
		}
	}
}
