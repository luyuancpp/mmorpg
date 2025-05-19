// cmd/loginservice/main.go
package main

import (
	"flag"
	"fmt"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/discov"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/zrpc"
	"golang.org/x/net/context"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
	"login/internal/config"
	"login/internal/logic/pkg/centre"
	"login/internal/logic/pkg/node"
	loginserviceServer "login/internal/server/loginservice"
	"login/internal/svc"
	"login/pb/game"
	"net"
	"strconv"
)

var configFile = flag.String("loginService", "etc/loginservice.yaml", "the config file path")

const nodeType = game.ENodeType_LoginNodeService

func main() {
	flag.Parse()

	// 加载配置文件
	var cfg config.Config
	conf.MustLoad(*configFile, &cfg)

	// 初始化服务上下文
	ctx := svc.NewServiceContext(cfg)

	// 启动 gRPC 服务
	startGRPCServer(cfg, ctx)
}

// startGRPCServer 启动 Login gRPC 服务并注册到 etcd
func startGRPCServer(cfg config.Config, ctx *svc.ServiceContext) {
	host, port, err := splitHostPort(ctx.Config.ListenOn)
	if err != nil {
		logx.Errorf("Failed to parse listen address: %v", err)
		return
	}

	// 注册到 etcd
	loginNode := node.NewNode(uint32(nodeType), host, port, discov.TimeToLive)
	if err := loginNode.KeepAlive(); err != nil {
		logx.Errorf("Failed to keep node alive: %v", err)
		return
	}

	defer func(loginNode *node.Node) {
		err := loginNode.Close()
		if err != nil {
			logx.Errorf("Failed to close node : %v", err)
			return
		}
	}(loginNode)

	ctx.SetNodeId(int64(loginNode.Info.NodeId))
	logx.Infof("Login node registered: %+v", loginNode.Info.String())

	// 获取并连接到 centre 节点
	if err := connectToCentreNodes(ctx, loginNode); err != nil {
		logx.Errorf("Failed to connect to centre nodes: %v", err)
		return
	}

	// 创建并启动 gRPC 服务
	server := zrpc.MustNewServer(cfg.RpcServerConf, func(grpcServer *grpc.Server) {
		game.RegisterLoginServiceServer(grpcServer, loginserviceServer.NewLoginServiceServer(ctx))

		if cfg.Mode == service.DevMode || cfg.Mode == service.TestMode {
			reflection.Register(grpcServer)
		}
	})
	defer server.Stop()

	logx.Infof("Starting Login RPC server at %s...", cfg.ListenOn)
	server.Start()
}

// splitHostPort 将 address 拆解为 host 和 uint32 类型的 port
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

func connectToCentreNodes(ctx *svc.ServiceContext, loginNode *node.Node) error {
	zoneId := config.AppConfig.ZoneID
	nodeType := uint32(game.ENodeType_CentreNodeService)

	prefix := node.BuildRpcPrefix(
		game.ENodeType_name[int32(game.ENodeType_CentreNodeService)],
		zoneId,
		nodeType,
	)

	watcher := node.NewNodeWatcher(loginNode.Client, prefix)

	// 1. 获取当前节点列表并连接
	nodes, err := watcher.Range()
	if err != nil {
		return fmt.Errorf("range centre nodes failed: %w", err)
	}

	for _, n := range nodes {
		if n.ZoneId == zoneId {
			logx.Infof("Connecting to centre node: %+v", n.String())
			ctx.SetCentreClient(centre.NewCentreClient(n.Endpoint.Ip, n.Endpoint.Port))
			break // 只连接一个，如需多连接可移除 break
		}
	}

	// 2. 实时监听中心节点的变动
	go func() {
		events := watcher.Watch(context.Background())
		for event := range events {
			switch event.Type {
			case node.NodeAdded:
				if event.Info.ZoneId == zoneId {
					logx.Infof("New centre node detected: %+v", event.Info.String())
					ctx.SetCentreClient(centre.NewCentreClient(event.Info.Endpoint.Ip, event.Info.Endpoint.Port))
				}
			case node.NodeRemoved:
				if event.Info.ZoneId == zoneId {
					logx.Infof("Centre node removed: %+v", event.Info.String())
					client := ctx.GetCentreClient()
					if client != nil {
						err := client.Close()
						if err != nil {
							logx.Errorf("Failed to close centre client: %v", err)
							return
						}
					}

				}
			}
		}
	}()

	return nil
}
