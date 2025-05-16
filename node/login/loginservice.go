// cmd/loginservice/main.go
package main

import (
	"flag"
	"fmt"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
	"login/internal/config"
	"login/internal/logic/pkg/node"
	loginserviceServer "login/internal/server/loginservice"
	"login/internal/svc"
	"login/pb/game"
)

var configFile = flag.String("loginService", "etc/loginservice.yaml", "the config file")

const NodeType = 2

func main() {
	// 解析命令行参数
	flag.Parse()

	// 加载配置
	var c config.Config
	conf.MustLoad(*configFile, &c)

	// 创建服务上下文
	ctx := svc.NewServiceContext(c)

	// 启动 gRPC 服务器
	startGRPCServer(c, ctx)
}

// startGRPCServer 启动 gRPC 服务
func startGRPCServer(c config.Config, ctx *svc.ServiceContext) {

	loginEtcdNode := node.NewNode(uint32(game.ENodeType_LoginNodeService), "127.0.0.1", 2379, 1000)

	err := loginEtcdNode.Register()
	if err != nil {
		logx.Error(err)
		return
	}

	s := zrpc.MustNewServer(c.RpcServerConf, func(grpcServer *grpc.Server) {
		// 注册 LoginService 服务
		game.RegisterLoginServiceServer(grpcServer, loginserviceServer.NewLoginServiceServer(ctx))

		// 在开发模式或测试模式下启用 gRPC 反射
		if c.Mode == service.DevMode || c.Mode == service.TestMode {
			reflection.Register(grpcServer)
		}
	})

	defer s.Stop()

	// 打印启动信息并启动 gRPC 服务器
	fmt.Printf("Starting rpc server at %s...\n", c.ListenOn)
	s.Start()
}
