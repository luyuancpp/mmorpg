package main

import (
	"db_server/pkg"
	"flag"
	"fmt"

	"db_server/internal/config"
	accountdbserviceServer "db_server/internal/server/accountdbservice"
	playerdbserviceServer "db_server/internal/server/playerdbservice"
	"db_server/internal/svc"
	"db_server/pb/game"

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

	config.Load()
	pkg.InitDB()

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
