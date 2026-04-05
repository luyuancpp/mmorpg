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
	})
	if err != nil {
		return nil, fmt.Errorf("failed to connect etcd endpoints %v: %w", endpoints, err)
	}
	return client, nil
}
