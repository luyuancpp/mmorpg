// internal/etcd/registry.go
package etcd

import (
	"context"
	"fmt"
	"time"

	"github.com/zeromicro/go-zero/core/logx"
	clientv3 "go.etcd.io/etcd/client/v3"
)

type NodeRegistry struct {
	client *clientv3.Client
	Lease  clientv3.LeaseID
	ttl    int64

	// registeredKeys tracks key-value pairs for re-registration after lease loss.
	registeredKeys map[string]string
}

func NewNodeRegistry(client *clientv3.Client, ttl int64) (*NodeRegistry, error) {
	resp, err := client.Grant(context.Background(), ttl)
	if err != nil {
		return nil, fmt.Errorf("failed to grant Lease: %v", err)
	}

	return &NodeRegistry{
		client:         client,
		Lease:          resp.ID,
		ttl:            ttl,
		registeredKeys: make(map[string]string),
	}, nil
}

func (r *NodeRegistry) RegisterNode(key string, value string) error {
	_, err := r.client.Put(context.Background(), key, value, clientv3.WithLease(r.Lease))
	if err != nil {
		return fmt.Errorf("failed to register node: %v", err)
	}
	r.registeredKeys[key] = value
	return nil
}

func (r *NodeRegistry) KeepAlive(ctx context.Context) {
	ch, err := r.client.KeepAlive(ctx, r.Lease)
	if err != nil {
		logx.Errorf("Failed to keep alive Lease: %v", err)
		return
	}

	go func() {
		for {
			select {
			case ka := <-ch:
				if ka == nil {
					logx.Error("Lease keep alive channel closed, attempting re-registration")
					r.reRegister(ctx)
					return
				}
				logx.Debugf("Lease TTL: %d", ka.TTL)
			case <-ctx.Done():
				logx.Info("KeepAlive context canceled")
				return
			}
		}
	}()
}

// reRegister grants a new lease, re-puts all registered keys, and restarts KeepAlive.
func (r *NodeRegistry) reRegister(ctx context.Context) {
	const maxRetries = 0 // unlimited
	backoff := time.Second

	for attempt := 1; maxRetries == 0 || attempt <= maxRetries; attempt++ {
		select {
		case <-ctx.Done():
			logx.Info("Re-registration aborted: context canceled")
			return
		default:
		}

		logx.Infof("Re-registration attempt %d: granting new lease (ttl=%d)", attempt, r.ttl)

		resp, err := r.client.Grant(ctx, r.ttl)
		if err != nil {
			logx.Errorf("Re-registration: failed to grant lease: %v, retrying in %v", err, backoff)
			time.Sleep(backoff)
			backoff = min(backoff*2, 30*time.Second)
			continue
		}

		r.Lease = resp.ID
		logx.Infof("Re-registration: new lease granted, id=%d", r.Lease)

		allOk := true
		for key, value := range r.registeredKeys {
			_, err := r.client.Put(ctx, key, value, clientv3.WithLease(r.Lease))
			if err != nil {
				logx.Errorf("Re-registration: failed to re-put key=%s: %v", key, err)
				allOk = false
				break
			}
			logx.Infof("Re-registration: key=%s re-registered with new lease", key)
		}

		if !allOk {
			logx.Errorf("Re-registration: partial failure, retrying in %v", backoff)
			time.Sleep(backoff)
			backoff = min(backoff*2, 30*time.Second)
			continue
		}

		// Restart KeepAlive with the new lease
		r.KeepAlive(ctx)
		logx.Info("Re-registration: completed successfully")
		return
	}

	logx.Error("Re-registration: exhausted retries, node is no longer registered")
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
