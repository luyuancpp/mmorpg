// internal/etcd/registry.go
package etcd

import (
	"context"
	"fmt"

	"github.com/zeromicro/go-zero/core/logx"
	clientv3 "go.etcd.io/etcd/client/v3"
)

type NodeRegistry struct {
	client *clientv3.Client
	Lease  clientv3.LeaseID
}

func NewNodeRegistry(client *clientv3.Client, ttl int64) (*NodeRegistry, error) {
	resp, err := client.Grant(context.Background(), ttl)
	if err != nil {
		return nil, fmt.Errorf("failed to grant Lease: %v", err)
	}

	return &NodeRegistry{
		client: client,
		Lease:  resp.ID,
	}, nil
}

func (r *NodeRegistry) RegisterNode(key string, value string) error {
	_, err := r.client.Put(context.Background(), key, value, clientv3.WithLease(r.Lease))
	if err != nil {
		return fmt.Errorf("failed to register node: %v", err)
	}
	return nil
}

func (r *NodeRegistry) KeepAlive(ctx context.Context) {
	ch, err := r.client.KeepAlive(ctx, r.Lease)
	if err != nil {
		logx.Error("Failed to keep alive Lease: %v", err)
		return
	}

	go func() {
		for {
			select {
			case ka := <-ch:
				if ka == nil {
					logx.Info("Lease keep alive channel closed")
					return
				}
				logx.Debug("Lease TTL: %d", ka.TTL)
			case <-ctx.Done():
				logx.Info("KeepAlive context canceled")
				return
			}
		}
	}()
}

// RevokeLease revokes the etcd lease.
func (r *NodeRegistry) RevokeLease() error {
	_, err := r.client.Revoke(context.Background(), r.Lease)
	if err != nil {
		return fmt.Errorf("failed to revoke lease: %v", err)
	}
	logx.Info("Lease revoked successfully")
	return nil
}
