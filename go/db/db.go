package main

import (
	"db/generated/pb/game"
	"db/internal/config"
	"db/internal/logic/pkg/db"
	task2 "db/internal/logic/pkg/task"
	server "db/internal/server/db"
	"db/internal/svc"
	"flag"
	"fmt"
	"github.com/hibiken/asynq"

	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
)

var configFile = flag.String("f", "etc/dbservice.yaml", "the config file")

func buildPlayerQueues(shardCount int, concurrencyPerQueue int) map[string]int {
	queues := make(map[string]int, shardCount)
	for i := 0; i < shardCount; i++ {
		queueName := task2.GetQueueName(uint64(i)) // 保证一致性
		queues[queueName] = concurrencyPerQueue
	}
	return queues
}

func main() {
	flag.Parse()

	conf.MustLoad(*configFile, &config.AppConfig)
	ctx := svc.NewServiceContext()

	serverAsynq := asynq.NewServer(
		asynq.RedisClientOpt{
			Addr:     ctx.Config.ServerConfig.RedisClient.Hosts,
			Password: ctx.Config.ServerConfig.RedisClient.Password,
			DB:       ctx.Config.ServerConfig.RedisClient.DB,
		},
		asynq.Config{
			Concurrency: 10,                                                                       // 总并发数
			Queues:      buildPlayerQueues(int(config.AppConfig.ServerConfig.QueueShardCount), 1), // 每个队列处理一个任务，保证顺序性
		},
	)
	mux := asynq.NewServeMux()
	mux.HandleFunc("shard_task", task2.NewDBTaskHandler(ctx.RedisClient))

	if err := serverAsynq.Start(mux); err != nil {
		panic(err)
	}

	db.InitDB()

	s := zrpc.MustNewServer(config.AppConfig.RpcServerConf, func(grpcServer *grpc.Server) {
		game.RegisterDbServer(grpcServer, server.NewDbServer(ctx))
		if config.AppConfig.Mode == service.DevMode || config.AppConfig.Mode == service.TestMode {
			reflection.Register(grpcServer)
		}
	})
	defer s.Stop()

	fmt.Printf("Starting rpc server at %s...\n", config.AppConfig.ListenOn)
	s.Start()
}
