package main

import (
	"flag"
	"fmt"
	"login_server/data"

	"login_server/internal/config"
	loginserviceServer "login_server/internal/server/loginservice"
	"login_server/internal/svc"
	"login_server/pb/game"

	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
)

var configFile = flag.String("loginService", "etc/loginservice.yaml", "the config file")

func main() {
	flag.Parse()

	var c config.Config
	conf.MustLoad(*configFile, &c)
	ctx := svc.NewServiceContext(c)

	data.Init()

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
