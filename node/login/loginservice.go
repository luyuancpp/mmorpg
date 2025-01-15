package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"login/client/deployservice"
	"login/internal/config"
	loginserviceServer "login/internal/server/loginservice"
	"login/internal/svc"
	"login/pb/game"

	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
)

var configFile = flag.String("loginService", "etc/login_service.yaml", "the config file")

const NodeType = 2

func main() {
	flag.Parse()

	var c config.Config
	conf.MustLoad(*configFile, &c)
	ctx := svc.NewServiceContext(c)

	deployService := deployservice.NewDeployService(*ctx.DeployClient)
	id, err := deployService.GetID(context.Background(), &game.GetIDRequest{ServerType: NodeType})
	if err != nil {
		log.Fatal(err)
		return
	}

	ctx.SetNodeId(id)

	s := zrpc.MustNewServer(c.RpcServerConf, func(grpcServer *grpc.Server) {
		game.RegisterLoginServiceServer(grpcServer, loginserviceServer.NewLoginServiceServer(ctx))

		if c.Mode == service.DevMode || c.Mode == service.TestMode {
			reflection.Register(grpcServer)
		}
	})

	defer s.Stop()

	fmt.Printf("Starting rpc server at %s...\n", c.ListenOn)
	s.Start()
}
