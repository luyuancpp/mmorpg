package main

import (
	"flag"
	"fmt"

	"db_sever/internal/config"
	accountdbserviceServer "db_sever/internal/server/accountdbservice"
	playerdbserviceServer "db_sever/internal/server/playerdbservice"
	"db_sever/internal/svc"
	"db_sever/pb/db"

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

	s := zrpc.MustNewServer(c.RpcServerConf, func(grpcServer *grpc.Server) {
		db.RegisterAccountDBServiceServer(grpcServer, accountdbserviceServer.NewAccountDBServiceServer(ctx))
		db.RegisterPlayerDBServiceServer(grpcServer, playerdbserviceServer.NewPlayerDBServiceServer(ctx))

		if c.Mode == service.DevMode || c.Mode == service.TestMode {
			reflection.Register(grpcServer)
		}
	})
	defer s.Stop()

	fmt.Printf("Starting rpc server at %s...\n", c.ListenOn)
	s.Start()
}
