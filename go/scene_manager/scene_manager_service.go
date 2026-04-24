package main

import (
	"context"
	"flag"
	"fmt"
	"strings"

	base "proto/common/base"
	"proto/scene_manager"
	"scene_manager/internal/config"
	"scene_manager/internal/logic"
	"scene_manager/internal/metrics"
	"scene_manager/internal/noderegistry"
	"scene_manager/internal/server"
	"scene_manager/internal/svc"
	"shared/generated/table"
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

	// Register debug HTTP handlers before starting the metrics server so
	// they are available from the moment the port opens. Currently only
	// the rebalance planner is exposed; add more via metrics.RegisterDebugHandler.
	logic.RegisterRebalanceDebugHandler(svcCtx)

	// Start Prometheus metrics endpoint (no-op when MetricsListenAddr is empty).
	metrics.Start(c.MetricsListenAddr)

	// Start load reporter (discovers scene nodes from etcd, inits main scenes for new zones).
	go logic.StartLoadReporter(ctx, svcCtx)

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

	// Register with etcd in C++ NodeInfo convention so Scene nodes can discover us.
	host, port := parseListenOn(c.ListenOn)
	nr, err := noderegistry.Register(
		svcCtx.Etcd,
		uint32(base.ENodeType_SceneManagerNodeService),
		c.ZoneId,
		host, port,
		c.LeaseTTL,
	)
	if err != nil {
		panic("failed to register SceneManager node in etcd: " + err.Error())
	}
	nr.KeepAlive()
	defer nr.Close()

	fmt.Println("\n=============================================================")
	fmt.Println("  SCENE_MANAGER SERVICE STARTED SUCCESSFULLY")
	fmt.Println("=============================================================")
	fmt.Printf("  Listen:      %s\n", c.ListenOn)
	fmt.Printf("  Mode:        %s\n", c.Mode)
	fmt.Printf("  multi-zone:  true (zone-agnostic)\n")
	fmt.Printf("  main scenes: %d (from World.json)\n", len(table.WorldTableManagerInstance.FindAll()))
	if len(c.Etcd.Hosts) > 0 {
		fmt.Printf("  etcd:        %v\n", c.Etcd.Hosts)
	}
	fmt.Printf("  node_id:     %d (etcd CAS)\n", nr.Info.NodeId)
	fmt.Printf("  node_uuid:   %s\n", nr.Info.NodeUuid)
	fmt.Printf("  kafka:       %v\n", c.Kafka.Brokers)
	fmt.Println("=============================================================")
	s.Start()
}

// parseListenOn splits "host:port" into its components.
func parseListenOn(listenOn string) (string, uint32) {
	parts := strings.SplitN(listenOn, ":", 2)
	host := parts[0]
	port := uint32(0)
	if len(parts) == 2 {
		fmt.Sscanf(parts[1], "%d", &port)
	}
	return host, port
}
