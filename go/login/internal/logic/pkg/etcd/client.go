// internal/etcd/client.go
package etcd

import (
	"game/login/internal/config"
	"go.etcd.io/etcd/client/v3"
	"log"
	"time"
)

func NewClient() (*clientv3.Client, error) {
	client, err := clientv3.New(clientv3.Config{
		Endpoints:   config.AppConfig.Registry.Etcd.Hosts,
		DialTimeout: time.Duration(config.AppConfig.Registry.Etcd.DialTimeoutMS),
	})
	if err != nil {
		log.Fatalf("Failed to connect to etcd: %v", err)
	}
	return client, nil
}
