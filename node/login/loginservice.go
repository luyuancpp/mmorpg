// cmd/loginservice/main.go
package main

import (
	"context"
	"flag"
	"fmt"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
	"log"
	"login/client/deployservice"
	"login/internal/config"
	loginserviceServer "login/internal/server/loginservice"
	"login/internal/svc"
	"login/pb/game"
)

var configFile = flag.String("loginService", "etc/login_service.yaml", "the config file")

const NodeType = 2

func main() {
	flag.Parse()

	// 加载配置
	var c config.Config
	conf.MustLoad(*configFile, &c)
	ctx := svc.NewServiceContext(c)

	// 获取ID
	deployService := deployservice.NewDeployService(*ctx.DeployClient)
	id, err := deployService.GetID(context.Background(), &game.GetIDRequest{NodeType: 2})
	if err != nil {
		log.Fatal(err)
		return
	}

	// 设置 Node ID
	ctx.SetNodeId(id.Id)

	// 程序退出时释放 ID
	defer func() {
		_, err := deployService.ReleaseID(context.Background(), &game.ReleaseIDRequest{NodeType: 2, Id: id.Id})
		if err != nil {
			log.Fatal(err)
			return
		}
	}()

	// 创建并启动 gRPC 服务器
	s := zrpc.MustNewServer(c.RpcServerConf, func(grpcServer *grpc.Server) {
		game.RegisterLoginServiceServer(grpcServer, loginserviceServer.NewLoginServiceServer(ctx))

		// 开发模式或测试模式时开启 gRPC 反射
		if c.Mode == service.DevMode || c.Mode == service.TestMode {
			reflection.Register(grpcServer)
		}
	})

	defer s.Stop()

	// 打印启动信息
	fmt.Printf("Starting rpc server at %s...\n", c.ListenOn)
	s.Start()
}
