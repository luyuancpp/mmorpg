package main

import (
	"flag"
	"fmt"

	"gateway/internal/config"
	"gateway/internal/handler"
	"gateway/internal/svc"

	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/rest"
)

var configFile = flag.String("f", "etc/gateway.yaml", "the config file path")

func main() {
	flag.Parse()

	conf.MustLoad(*configFile, &config.AppConfig)

	server := rest.MustNewServer(config.AppConfig.RestConf)
	defer server.Stop()

	svcCtx := svc.NewServiceContext(config.AppConfig)
	handler.RegisterRoutes(server, svcCtx)

	fmt.Println("\n=============================================================")
	fmt.Println("  GATEWAY SERVICE STARTED SUCCESSFULLY")
	fmt.Println("=============================================================")
	fmt.Printf("  REST API:    %s:%d\n", config.AppConfig.RestConf.Host, config.AppConfig.RestConf.Port)
	fmt.Printf("  etcd:        %v\n", config.AppConfig.Etcd.Hosts)
	fmt.Println("=============================================================")

	server.Start()
}
