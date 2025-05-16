// internal/etcd/client.go
package etcd

import (
	"go.etcd.io/etcd/client/v3"
	"log"
	"time"
)

func NewClient(endpoints []string) (*clientv3.Client, error) {
	client, err := clientv3.New(clientv3.Config{
		Endpoints:   endpoints,
		DialTimeout: 5 * time.Second,
	})
	if err != nil {
		log.Fatalf("Failed to connect to etcd: %v", err)
	}
	return client, nil
}
