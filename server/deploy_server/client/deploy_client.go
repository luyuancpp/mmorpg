package main

import (
	"deploy_server/client/deployservice"
	"flag"
	"fmt"
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
	for i := 0; i < 1000; i++ {
		deploy := deployservice.NewDeployService(client)
		resp, err := deploy.StartGs(context.Background(), &deployservice.StartGsRequest{ZoneId: uint32(i)})
		if err != nil {
			fmt.Println("X", err.Error())
		} else {
			fmt.Println("=>", resp.String())
		}
		resp1, err := deploy.GetNodeInfo(context.Background(), &deployservice.NodeInfoRequest{ZoneId: uint32(i)})
		if err != nil {
			fmt.Println("X", err.Error())
		} else {
			fmt.Println("=>", resp1.String())
		}
	}
}
