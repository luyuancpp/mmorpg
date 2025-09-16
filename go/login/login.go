// cmd/loginservice/main.go
package main

import (
	"encoding/base64"
	"errors"
	"flag"
	"fmt"
	"github.com/google/uuid"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/zrpc"
	"golang.org/x/net/context"
	"google.golang.org/grpc"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/reflection"
	"google.golang.org/protobuf/proto"
	"login/generated/pb/game"
	"login/internal/config"
	"login/internal/logic/pkg/centre"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/node"
	loginserver "login/internal/server/clientplayerlogin"
	"login/internal/svc"
	"net"
	"strconv"
)

var configFile = flag.String("loginService", "etc/loginservice.yaml", "the config file path")

const nodeType = game.ENodeType_LoginNodeService

func main() {
	flag.Parse()

	// 加载配置文件
	conf.MustLoad(*configFile, &config.AppConfig)

	ctx := svc.NewServiceContext()

	// 启动 gRPC 服务
	if err := startGRPCServer(config.AppConfig, ctx); err != nil {
		logx.Errorf("Failed to start GRPC server: %v", err)
	}
}

// startGRPCServer 启动 Login gRPC 服务并注册到 etcd
func startGRPCServer(cfg config.Config, ctx *svc.ServiceContext) error {

	// 获取地址并启动 gRPC 服务
	host, port, err := splitHostPort(config.AppConfig.ListenOn)
	if err != nil {
		logx.Errorf("Failed to parse listen address: %v", err)
		return err
	}

	// 注册节点到 etcd
	loginNode := node.NewNode(uint32(nodeType), host, port, config.AppConfig.Node.LeaseTTL)
	if loginNode == nil {
		err = errors.New("failed to create node")
		logx.Errorf("Failed to create node: %v", err)
		return err
	}

	if err := loginNode.KeepAlive(); err != nil {
		logx.Errorf("Failed to keep node alive: %v", err)
		return err
	}

	defer func() {
		if err := loginNode.Close(); err != nil {
			logx.Errorf("Failed to close node: %v", err)
		}
	}()

	ctx.SetNodeId(int64(loginNode.Info.NodeId))
	logx.Infof("Login node registered: %+v", loginNode.Info.String())

	// 获取并连接到 Centre 节点
	if err := connectToCentreNodes(ctx, loginNode); err != nil {
		logx.Errorf("Failed to connect to centre nodes: %v", err)
		return err
	}

	// 启动 gRPC 服务器
	if err := startServer(cfg, ctx); err != nil {
		logx.Errorf("Failed to start gRPC server: %v", err)
		return err
	}

	// 正常启动后返回 nil
	return nil
}

func SessionInterceptor(
	ctx context.Context,
	req interface{},
	info *grpc.UnaryServerInfo,
	handler grpc.UnaryHandler,
) (interface{}, error) {
	md, ok := metadata.FromIncomingContext(ctx)
	if ok {
		if vals, exists := md["x-session-detail-bin"]; exists && len(vals) > 0 {
			// 解码 Base64
			bin, err := base64.StdEncoding.DecodeString(vals[0])
			if err != nil {
				logx.Error("Base64 decode error:", err)
			} else {
				var detail game.SessionDetails
				if err := proto.Unmarshal(bin, &detail); err != nil {
					logx.Error("Protobuf unmarshal error:", err)
				} else {
					ctx = ctxkeys.WithSessionDetails(ctx, &detail)
				}
			}
		}
	}

	// 执行实际的 handler
	resp, err := handler(ctx, req)

	// ---- 在这里加上返回的 session detail header ----
	if detail, ok := ctxkeys.GetSessionDetails(ctx); ok {
		if bin, err := proto.Marshal(detail); err == nil {
			logx.Infof("Session info: %+v", detail)
			val := base64.StdEncoding.EncodeToString(bin)
			header := metadata.Pairs("x-session-detail-bin", val)
			grpc.SendHeader(ctx, header)
		} else {
			logx.Error("Protobuf marshal error:", err)
		}
	}

	return resp, err
}

// startServer 启动并配置 gRPC 服务
func startServer(cfg config.Config, ctx *svc.ServiceContext) error {
	server := zrpc.MustNewServer(cfg.RpcServerConf, func(grpcServer *grpc.Server) {
		// 注册服务
		game.RegisterClientPlayerLoginServer(grpcServer, loginserver.NewClientPlayerLoginServer(ctx))

		// 在开发或测试模式下，启用反射
		if cfg.Mode == service.DevMode || cfg.Mode == service.TestMode {
			reflection.Register(grpcServer)
		}
	})

	server.AddUnaryInterceptors(SessionInterceptor)

	defer server.Stop()

	// 启动 gRPC 服务器
	logx.Infof("Starting Login RPC server at %s...", cfg.ListenOn)
	server.Start()

	return nil
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
	zoneId := config.AppConfig.Node.ZoneId
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
			client, err := centre.NewCentreClient(n.Endpoint.Ip, n.Endpoint.Port, n.NodeUuid)
			if err != nil {
				logx.Errorf("Failed to connect to centre node: %v", err)
			} else {
				ctx.SetCentreClient(client)
			}
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
					old := ctx.GetCentreClient()

					// 若已经是同一个 UUID，则跳过
					nodeUuid, err := uuid.Parse(event.Info.NodeUuid)
					if err != nil {
						logx.Errorf("Invalid UUID in event: %v", event.Info.NodeUuid)
						continue
					}

					if old != nil && old.NodeUuid == nodeUuid {
						continue
					}

					logx.Infof("New centre node detected: uuid=%s, info=%+v", event.Info.NodeUuid, event.Info)

					client, err := centre.NewCentreClient(event.Info.Endpoint.Ip, event.Info.Endpoint.Port, event.Info.NodeUuid)
					if err != nil {
						logx.Errorf("Failed to connect to centre node: %v", err)
						continue
					}

					if old != nil {
						old.Close()
					}

					ctx.SetCentreClient(client)
				}

			case node.NodeRemoved:
				if event.Info.ZoneId == zoneId {
					centreNode := ctx.GetCentreClient()
					nodeUuid, err := uuid.Parse(event.Info.NodeUuid)
					if err != nil {
						logx.Errorf("Invalid UUID in event: %v", event.Info.NodeUuid)
						continue
					}

					if centreNode != nil && centreNode.NodeUuid == nodeUuid {
						logx.Infof("Centre centreNode removed: %+v", event.Info.String())
						centreNode.Close()
						ctx.SetCentreClient(nil)
					}
				}
			}
		}
	}()

	return nil
}
