package main

import (
	"deploy_server/client/deployservice"
	"flag"
	"fmt"
	"time"
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
	ticker := time.NewTicker(time.Millisecond * 500)
	defer ticker.Stop()
	for {
		select {
		case <-ticker.C:
			deploy := deployservice.NewDeployService(client)
			resp, err := deploy.StartGs(context.Background(), &deployservice.StartGsRequest{})
			if err != nil {
				fmt.Println("X", err.Error())
			} else {
				fmt.Println("=>", resp.String())
			}
			break
		}
	}
}
