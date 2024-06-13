package main

import (
	"deploy/internal/config"
	deployserviceServer "deploy/internal/server/deployservice"
	"deploy/internal/svc"
	"deploy/pb/game"
	"deploy/pkg"
	"flag"
	"fmt"

	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
)

var configFile = flag.String("config", "etc/deployservice.yaml", "the config file")
var dbConfigFile = flag.String("dbconfig", "etc/db.json", "the db config file")

func main() {
	flag.Parse()

	var c config.Config
	conf.MustLoad(*configFile, &c)
	ctx := svc.NewServiceContext(c)

	pkg.InitDB(*dbConfigFile)
	defer pkg.PbDb.Close()

	s := zrpc.MustNewServer(c.RpcServerConf, func(grpcServer *grpc.Server) {
		game.RegisterDeployServiceServer(grpcServer, deployserviceServer.NewDeployServiceServer(ctx))

		if c.Mode == service.DevMode || c.Mode == service.TestMode {
			reflection.Register(grpcServer)
		}
	})
	defer s.Stop()

	fmt.Printf("Starting rpc server at %s...\n", c.ListenOn)
	s.Start()
}
