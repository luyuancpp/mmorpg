package main

import (
	"context"
	"flag"
	"fmt"
	"net"
	"strconv"

	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"

	"player_locator/internal/config"
	"player_locator/internal/logic"
	"player_locator/internal/node"
	"player_locator/internal/server"
	"player_locator/internal/svc"
	proto_common "player_locator/proto/common"
	pb "player_locator/proto/player_locator"
)

var configFile = flag.String("f", "etc/player_locator.yaml", "config file path")

const nodeType = uint32(proto_common.ENodeType_PlayerLocatorNodeService)

func main() {
	flag.Parse()
	conf.MustLoad(*configFile, &config.AppConfig)

	svcCtx := svc.NewServiceContext(config.AppConfig)
	defer svcCtx.Stop()

	// Register node with etcd
	host, port, err := splitHostPort(config.AppConfig.ListenOn)
	if err != nil {
		logx.Must(fmt.Errorf("parse listen address: %w", err))
	}

	n, err := node.NewNode(nodeType, host, port)
	if err != nil {
		logx.Must(fmt.Errorf("create node: %w", err))
	}
	defer n.Close()

	if err := n.KeepAlive(); err != nil {
		logx.Must(fmt.Errorf("keep alive: %w", err))
	}
	logx.Infof("Node registered: id=%d uuid=%s", n.Info.NodeId, n.Info.NodeUuid)

	// Start background lease monitor
	leaseCtx, leaseCancel := context.WithCancel(context.Background())
	defer leaseCancel()

	go logic.StartLeaseMonitor(
		leaseCtx,
		svcCtx,
		config.AppConfig.Lease.PollInterval,
		config.AppConfig.Lease.BatchSize,
	)

	// Start gRPC server
	s := zrpc.MustNewServer(config.AppConfig.RpcServerConf, func(grpcServer *grpc.Server) {
		pb.RegisterPlayerLocatorServer(grpcServer, server.NewPlayerLocatorServer(svcCtx))
		if config.AppConfig.Mode == service.DevMode || config.AppConfig.Mode == service.TestMode {
			reflection.Register(grpcServer)
		}
	})
	defer s.Stop()

	logx.Infof("Starting player_locator RPC server at %s...", config.AppConfig.ListenOn)
	s.Start()
}

func splitHostPort(address string) (string, uint32, error) {
	host, portStr, err := net.SplitHostPort(address)
	if err != nil {
		return "", 0, err
	}
	portInt, err := strconv.Atoi(portStr)
	if err != nil {
		return "", 0, err
	}
	return host, uint32(portInt), nil
}
