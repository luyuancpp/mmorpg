// cmd/loginservice/main.go
package main

import (
	"encoding/base64"
	"errors"
	"flag"
	"login/internal/config"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/node"
	loginserver "login/internal/server/clientplayerlogin"
	"login/internal/svc"
	login_proto "login/proto/common"
	login_proto_login "login/proto/service/go/grpc/login"
	"net"
	"strconv"

	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/zrpc"
	"golang.org/x/net/context"
	"google.golang.org/grpc"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/reflection"
	"google.golang.org/protobuf/proto"
)

var configFile = flag.String("loginService", "etc/login.yaml", "the config file path")

const nodeType = login_proto.ENodeType_LoginNodeService

func main() {
	flag.Parse()

	// 加载配置文件
	conf.MustLoad(*configFile, &config.AppConfig)

	ctx := svc.NewServiceContext()

	defer ctx.Stop()

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
				var detail login_proto.SessionDetails
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
		login_proto_login.RegisterClientPlayerLoginServer(grpcServer, loginserver.NewClientPlayerLoginServer(ctx))

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
