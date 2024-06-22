package main

import (
	"db/internal/logic/pkg"
	"flag"
	"fmt"

	"db/internal/config"
	accountdbserviceServer "db/internal/server/accountdbservice"
	playerdbserviceServer "db/internal/server/playerdbservice"
	"db/internal/svc"
	"db/pb/game"

	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
)

var configFile = flag.String("f", "etc/dbservice.yaml", "the config file")

func main() {
	flag.Parse()

	var c config.Config
	conf.MustLoad(*configFile, &c)
	ctx := svc.NewServiceContext(c)

	config.LoadConfig()
	pkg.Init()

	s := zrpc.MustNewServer(c.RpcServerConf, func(grpcServer *grpc.Server) {
		game.RegisterAccountDBServiceServer(grpcServer, accountdbserviceServer.NewAccountDBServiceServer(ctx))
		game.RegisterPlayerDBServiceServer(grpcServer, playerdbserviceServer.NewPlayerDBServiceServer(ctx))

		if c.Mode == service.DevMode || c.Mode == service.TestMode {
			reflection.Register(grpcServer)
		}
	})
	defer s.Stop()

	fmt.Printf("Starting rpc server at %s...\n", c.ListenOn)
	s.Start()
}
