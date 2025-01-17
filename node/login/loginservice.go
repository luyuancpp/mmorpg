// cmd/loginservice/main.go
package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"time"

	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"

	"login/client/deployservice"
	"login/internal/config"
	loginserviceServer "login/internal/server/loginservice"
	"login/internal/svc"
	"login/pb/game"
)

var configFile = flag.String("loginService", "etc/login_service.yaml", "the config file")

const NodeType = 2

func main() {
	// 解析命令行参数
	flag.Parse()

	// 加载配置
	var c config.Config
	conf.MustLoad(*configFile, &c)

	// 创建服务上下文
	ctx := svc.NewServiceContext(c)

	// 获取节点 ID 和租约 ID
	deployService := deployservice.NewDeployService(*ctx.DeployClient)
	id, err := deployService.GetID(context.Background(), &game.GetIDRequest{NodeType: NodeType})
	if err != nil {
		log.Fatal(err)
		return
	}

	// 设置当前节点的 ID 和租约 ID
	ctx.SetNodeId(id.Id)
	ctx.NodeLeaseID = id.LeaseId

	// 启动一个 goroutine 每 10 秒更新一次租约
	go renewLeasePeriodically(deployService, ctx)

	// 程序退出时释放 ID
	defer releaseNodeID(deployService, id.Id)

	// 启动 gRPC 服务器
	startGRPCServer(c, ctx)
}

// renewLeasePeriodically 每 10 秒更新一次租约
func renewLeasePeriodically(deployService deployservice.DeployService, ctx *svc.ServiceContext) {
	ticker := time.NewTicker(10 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			_, err := deployService.RenewLease(context.Background(), &game.RenewLeaseIDRequest{LeaseId: ctx.NodeLeaseID})
			if err != nil {
				log.Fatal(err)
				return
			}
		}
	}
}

// releaseNodeID 释放节点 ID
func releaseNodeID(deployService deployservice.DeployService, nodeId uint64) {
	_, err := deployService.ReleaseID(context.Background(), &game.ReleaseIDRequest{NodeType: NodeType, Id: nodeId})
	if err != nil {
		log.Fatal(err)
		return
	}
}

// startGRPCServer 启动 gRPC 服务
func startGRPCServer(c config.Config, ctx *svc.ServiceContext) {
	s := zrpc.MustNewServer(c.RpcServerConf, func(grpcServer *grpc.Server) {
		// 注册 LoginService 服务
		game.RegisterLoginServiceServer(grpcServer, loginserviceServer.NewLoginServiceServer(ctx))

		// 在开发模式或测试模式下启用 gRPC 反射
		if c.Mode == service.DevMode || c.Mode == service.TestMode {
			reflection.Register(grpcServer)
		}
	})

	defer s.Stop()

	// 打印启动信息并启动 gRPC 服务器
	fmt.Printf("Starting rpc server at %s...\n", c.ListenOn)
	s.Start()
}
