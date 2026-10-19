// internal/etcd/client.go
package etcd

import (
	"fmt"
	"go.etcd.io/etcd/client/v3"
	"login/internal/config"
	"time"
)

func NewClient() (*clientv3.Client, error) {
	endpoints := config.AppConfig.Registry.Etcd.Hosts
	if len(endpoints) == 0 {
		endpoints = config.AppConfig.Etcd.Hosts
	}
	if len(endpoints) == 0 {
		return nil, fmt.Errorf("etcd endpoints are empty: configure Registry.Etcd.Hosts or Etcd.Hosts")
	}

	dialTimeout := config.AppConfig.Registry.Etcd.DialTimeout
	if dialTimeout <= 0 {
		dialTimeout = config.AppConfig.Timeouts.EtcdDialTimeout
	}
	if dialTimeout <= 0 {
		dialTimeout = 5 * time.Second
	}

	client, err := clientv3.New(clientv3.Config{
		Endpoints:        endpoints,
		DialTimeout:      dialTimeout,
		AutoSyncInterval: 30 * time.Second,
		// gRPC keepalive(2026-10-18):有活动流(gate 目录 watch、节点租约续约)时每 10s 探一次,5s 无应答即断开重连。
		// 不开时,成员所在机器失联留下的半开连接要等 TCP 重传超时(约 15 分钟)才被发现,期间 watch 静默、
		// 续约卡住(评审 A2)。10s 不低于 grpc-go 客户端下限,也高于 etcd 服务端缺省 --grpc-keepalive-min-time(5s),
		// 不会被服务端以 too_many_pings 断开;PermitWithoutStream 保持缺省 false,空闲连接不探。
		DialKeepAliveTime:    10 * time.Second,
		DialKeepAliveTimeout: 5 * time.Second,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to connect etcd endpoints %v: %w", endpoints, err)
	}
	return client, nil
}
