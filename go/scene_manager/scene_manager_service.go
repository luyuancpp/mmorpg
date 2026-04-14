package main

import (
	"context"
	"flag"
	"fmt"

	"proto/scene_manager"
	"scene_manager/internal/config"
	"scene_manager/internal/logic"
	"scene_manager/internal/server"
	"scene_manager/internal/svc"
	"shared/grpcstats"

	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
)

var configFile = flag.String("f", "etc/scene_manager_service.yaml", "the config file")

func main() {
	flag.Parse()

	var c config.Config
	conf.MustLoad(*configFile, &c)
	svcCtx := svc.NewServiceContext(c)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Start load reporter (discovers scene nodes from etcd).
	go logic.StartLoadReporter(ctx, svcCtx)

	// Initialize persistent main-world scenes (waits for nodes, then creates).
	go logic.InitMainScenes(ctx, svcCtx)

	// Start instance lifecycle manager (auto-destroys idle instances).
	go logic.StartInstanceLifecycleManager(ctx, svcCtx)

	s := zrpc.MustNewServer(c.RpcServerConf, func(grpcServer *grpc.Server) {
		scene_manager.RegisterSceneManagerServer(grpcServer, server.NewSceneManagerServer(svcCtx))

		if c.Mode == service.DevMode || c.Mode == service.TestMode {
			reflection.Register(grpcServer)
		}
	})
	s.AddUnaryInterceptors(grpcstats.New(grpcstats.Options{}).UnaryServerInterceptor())
	defer s.Stop()

	fmt.Println("\n=============================================================")
	fmt.Println("  SCENE_MANAGER SERVICE STARTED SUCCESSFULLY")
	fmt.Println("=============================================================")
	fmt.Printf("  Listen:      %s\n", c.ListenOn)
	fmt.Printf("  Mode:        %s\n", c.Mode)
	fmt.Printf("  multi-zone:  true (zone-agnostic)\n")
	if len(c.Etcd.Hosts) > 0 {
		fmt.Printf("  etcd:        %v\n", c.Etcd.Hosts)
	}
	fmt.Printf("  kafka:       %v\n", c.Kafka.Brokers)
	fmt.Println("=============================================================")
	s.Start()
}
