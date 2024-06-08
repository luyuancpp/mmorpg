package main

import (
	"deploy_server/client/deployservice"
	"flag"
)
import (
	"context"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/zrpc"
)

var configFile = flag.String("f", "etc/config.json", "the config file")

func main() {
	flag.Parse()

	var c zrpc.RpcClientConf
	conf.MustLoad(*configFile, &c)
	client := zrpc.MustNewClient(c)
	client1 := zrpc.MustNewClient(c)

	for i := 0; i < 1000; i++ {
		deploy := deployservice.NewDeployService(client)
		deploy.StartGs(context.Background(), &deployservice.StartGsRequest{ZoneId: uint32(i)})
		deploy.GetNodeInfo(context.Background(), &deployservice.NodeInfoRequest{ZoneId: uint32(i * 10)})

		deploy1 := deployservice.NewDeployService(client1)
		deploy1.StartGs(context.Background(), &deployservice.StartGsRequest{ZoneId: uint32(i * 100)})
		deploy1.GetNodeInfo(context.Background(), &deployservice.NodeInfoRequest{ZoneId: uint32(i * 1000)})
	}
}
