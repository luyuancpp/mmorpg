package main

import (
	"db/internal/config"
	"db/internal/kafka"
	"db/internal/logic/pkg/proto_sql"
	server "db/internal/server/db"
	"db/internal/svc"
	db_grpc "db/proto/service/go/grpc/db"
	"flag"
	"fmt"
	"os"
	"os/signal"
	"syscall"

	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
)

var configFile = flag.String("f", "etc/db.yaml", "the config file")

func main() {
	flag.Parse()

	// 加载配置
	conf.MustLoad(*configFile, &config.AppConfig)
	ctx := svc.NewServiceContext()

	// 初始化Kafka消费者（替代Asynq Server）
	kafkaConsumer, err := kafka.NewKeyOrderedKafkaConsumer(
		config.AppConfig,
		ctx.RedisClient, // Redis客户端
	)
	if err != nil {
		panic(fmt.Sprintf("初始化Kafka消费者失败: %v", err))
	}
	defer kafkaConsumer.Stop()

	// 启动Kafka消费者（替代Asynq Start）
	if err := kafkaConsumer.Start(); err != nil {
		panic(fmt.Sprintf("启动Kafka消费者失败: %v", err))
	}

	// 初始化数据库（保持不变）
	proto_sql.InitDB()

	// 启动gRPC服务（保持不变）
	s := zrpc.MustNewServer(config.AppConfig.RpcServerConf, func(grpcServer *grpc.Server) {
		db_grpc.RegisterDbServer(grpcServer, server.NewDbServer(ctx))
		if config.AppConfig.Mode == service.DevMode || config.AppConfig.Mode == service.TestMode {
			reflection.Register(grpcServer)
		}
	})
	defer s.Stop()

	// 等待退出信号（优雅关闭）
	fmt.Printf("Starting rpc server at %s...\n", config.AppConfig.ListenOn)
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit
	fmt.Println("开始优雅关闭服务...")
}
