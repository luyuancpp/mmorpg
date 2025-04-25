package main

import (
	"deploy/internal/config"
	"deploy/internal/logic/pkg/node_id_etcd"
	deployserviceServer "deploy/internal/server/deployservice"
	"deploy/internal/svc"
	"deploy/pb/game"
	"flag"
	"fmt"
	"github.com/zeromicro/go-zero/core/logx"
	"time"

	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
)

var configFile = flag.String("config", "etc/deploy_service.yaml", "the config file")

func main() {
	flag.Parse()

	var c config.Config
	conf.MustLoad(*configFile, &c)
	ctx := svc.NewServiceContext(c)

	err := node_id_etcd.ClearAllIDs(ctx.NodeEtcdClient)
	if err != nil {
		logx.Error(err)
		return
	}

	// 检查是否处于临时维护模式
	if false {
		go func() {
			// 如果是临时维护模式，延迟60秒后再启动定时任务
			logx.Info("In maintenance mode, delaying periodic sweep start...")
			time.Sleep(10 * time.Second) // 延迟60秒
			go node_id_etcd.StartPeriodicSweep(ctx.NodeEtcdClient, 30*time.Second)
		}()

	} else {
		err := node_id_etcd.ClearAllIDs(ctx.NodeEtcdClient)
		if err != nil {
			logx.Error(err)
			return
		}

		// 在非维护模式下，启动定时任务
		go node_id_etcd.StartPeriodicSweep(ctx.NodeEtcdClient, 20*time.Second)
	}

	// 调用 StartPeriodicSweep 启动定时任务，每隔 60 秒调用一次 SweepExpiredIDs
	go node_id_etcd.StartPeriodicSweep(ctx.NodeEtcdClient, 30*time.Second)

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
