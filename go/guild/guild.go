package main

import (
	"flag"
	"fmt"
	"net"
	"strconv"

	"github.com/bwmarrin/snowflake"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"

	"guild/internal/config"
	"guild/internal/data"
	"guild/internal/logic"
	"guild/internal/node"
	"guild/internal/server"
	"guild/internal/svc"
	base "proto/common/base"
	pb "proto/guild"
	"shared/grpcstats"
)

var configFile = flag.String("f", "etc/guild.yaml", "config file path")

const nodeType = uint32(base.ENodeType_GuildNodeService)

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
	logx.Infof("Guild node registered: id=%d uuid=%s", n.Info.NodeId, n.Info.NodeUuid)

	// Initialize snowflake for guild ID generation
	sf, err := snowflake.NewNode(int64(n.Info.NodeId))
	if err != nil {
		logx.Must(fmt.Errorf("create snowflake: %w", err))
	}

	// Initialize data repo with singleflight + cache-aside
	repo := data.NewGuildRepo(svcCtx.RedisClient, svcCtx.DB, config.AppConfig.Cache.DefaultTTL)
	guildLogic := logic.NewGuildLogic(repo, sf)

	// Start gRPC server
	s := zrpc.MustNewServer(config.AppConfig.RpcServerConf, func(grpcServer *grpc.Server) {
		pb.RegisterGuildServiceServer(grpcServer, server.NewGuildServer(guildLogic))
		if config.AppConfig.Mode == service.DevMode || config.AppConfig.Mode == service.TestMode {
			reflection.Register(grpcServer)
		}
	})
	s.AddUnaryInterceptors(grpcstats.New(grpcstats.Options{}).UnaryServerInterceptor())
	defer s.Stop()

	logx.Infof("Starting Guild RPC server at %s...", config.AppConfig.ListenOn)
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
