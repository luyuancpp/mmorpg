package main

import (
	"deploy_server/pkg"
	"flag"
	"fmt"
	"log"

	"deploy_server/internal/config"
	deployserviceServer "deploy_server/internal/server/deployservice"
	"deploy_server/internal/svc"
	"deploy_server/pb/game"

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

	err := pkg.OpenDB(*dbConfigFile)
	if err != nil {
		log.Fatal(err)
	}
	defer pkg.PbDb.Close()

	pkg.InitDBTables()

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
