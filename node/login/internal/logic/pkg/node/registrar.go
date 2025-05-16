package node

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"login/pb/game"
	"time"

	clientv3 "go.etcd.io/etcd/client/v3"
)

type NodeRegistrar interface {
	Register(ctx context.Context, info *game.NodeInfo) error
}

type EtcdRegistrar struct {
	client *clientv3.Client
	ttl    int64
	prefix string
}

func NewEtcdRegistrar(endpoints []string, ttl int64, prefix string) (*EtcdRegistrar, error) {
	client, err := clientv3.New(clientv3.Config{
		Endpoints:   endpoints,
		DialTimeout: 5 * time.Second,
	})
	if err != nil {
		return nil, err
	}
	return &EtcdRegistrar{client: client, ttl: ttl, prefix: prefix}, nil
}

func (r *EtcdRegistrar) Register(ctx context.Context, info *game.NodeInfo) error {
	leaseResp, err := r.client.Grant(ctx, r.ttl)
	if err != nil {
		return err
	}
	info.LeaseId = uint64(leaseResp.ID)
	info.LaunchTime = uint64(time.Now().Unix())

	key := fmt.Sprintf("%s/%s", r.prefix, info.LeaseId)
	data, _ := json.Marshal(info)

	_, err = r.client.Put(ctx, key, string(data), clientv3.WithLease(leaseResp.ID))
	if err != nil {
		return err
	}

	// 自动续约
	ch, err := r.client.KeepAlive(ctx, leaseResp.ID)
	if err != nil {
		return err
	}

	go func() {
		for {
			select {
			case <-ctx.Done():
				log.Println("Lease KeepAlive canceled")
				return
			case ka, ok := <-ch:
				if !ok {
					log.Println("KeepAlive channel closed")
					return
				}
				log.Printf("Lease keepalive: TTL = %d", ka.TTL)
			}
		}
	}()

	return nil
}
