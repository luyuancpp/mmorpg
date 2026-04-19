// Package noderegistry handles C++ convention etcd registration for SceneManager.
// C++ nodes discover services by watching etcd key prefixes like
// "SceneManagerNodeService.rpc/zone/{zoneId}/node_type/{nodeType}/node_id/{nodeId}".
// The value is a protojson-serialized NodeInfo message.
package noderegistry

import (
	"context"
	"fmt"
	"time"

	base "proto/common/base"

	"github.com/google/uuid"
	"github.com/zeromicro/go-zero/core/logx"
	clientv3 "go.etcd.io/etcd/client/v3"
	"google.golang.org/protobuf/encoding/protojson"
)

// NodeRegistration manages etcd registration with lease and keepalive.
type NodeRegistration struct {
	client   *clientv3.Client
	leaseID  clientv3.LeaseID
	ttl      int64
	key      string
	value    string
	cancelFn context.CancelFunc
	Info     *base.NodeInfo
}

// rpcPrefix returns the etcd key prefix for a given node type, e.g. "SceneManagerNodeService.rpc".
func rpcPrefix(nodeType uint32) string {
	return fmt.Sprintf("%s.rpc", base.ENodeType_name[int32(nodeType)])
}

// rpcPath returns the full etcd key for a node instance.
func rpcPath(prefix string, zoneId, nodeType, nodeId uint32) string {
	return fmt.Sprintf("%s/zone/%d/node_type/%d/node_id/%d", prefix, zoneId, nodeType, nodeId)
}

// Register creates a NodeInfo entry in etcd with a lease, using atomic CAS for node_id allocation.
func Register(cli *clientv3.Client, nodeType, zoneId uint32, ip string, port uint32, ttl int64) (*NodeRegistration, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	// Grant lease
	leaseResp, err := cli.Grant(ctx, ttl)
	if err != nil {
		return nil, fmt.Errorf("etcd lease grant failed: %w", err)
	}

	prefix := rpcPrefix(nodeType)
	info := &base.NodeInfo{
		NodeType: nodeType,
		Endpoint: &base.EndpointComp{
			Ip:   ip,
			Port: port,
		},
		GrpcEndpoint: &base.EndpointComp{
			Ip:   ip,
			Port: port,
		},
		ZoneId:       zoneId,
		LaunchTime:   uint64(time.Now().Unix()),
		ProtocolType: uint32(base.ENodeProtocolType_PROTOCOL_GRPC),
		NodeUuid:     uuid.New().String(),
	}

	// Allocate node_id via CAS
	nodeID, err := allocateNodeID(ctx, cli, prefix, info, leaseResp.ID)
	if err != nil {
		return nil, fmt.Errorf("node ID allocation failed: %w", err)
	}
	info.NodeId = nodeID

	key := rpcPath(prefix, zoneId, nodeType, nodeID)
	value, err := protojson.Marshal(info)
	if err != nil {
		return nil, fmt.Errorf("protojson marshal failed: %w", err)
	}

	return &NodeRegistration{
		client:  cli,
		leaseID: leaseResp.ID,
		ttl:     ttl,
		key:     key,
		value:   string(value),
		Info:    info,
	}, nil
}

// allocateNodeID finds the smallest unused node_id under the prefix via etcd CAS.
func allocateNodeID(ctx context.Context, cli *clientv3.Client, prefix string, info *base.NodeInfo, leaseID clientv3.LeaseID) (uint32, error) {
	resp, err := cli.Get(ctx, prefix, clientv3.WithPrefix())
	if err != nil {
		return 0, err
	}

	usedIDs := make(map[uint32]bool)
	maxID := uint32(0)
	for _, kv := range resp.Kvs {
		var ni base.NodeInfo
		if err := protojson.Unmarshal(kv.Value, &ni); err != nil {
			continue
		}
		usedIDs[ni.NodeId] = true
		if ni.NodeId > maxID {
			maxID = ni.NodeId
		}
	}

	searchRange := maxID + 10
	for id := uint32(0); id < searchRange; id++ {
		if usedIDs[id] {
			continue
		}
		info.NodeId = id
		val, err := protojson.Marshal(info)
		if err != nil {
			continue
		}
		key := rpcPath(prefix, info.ZoneId, info.NodeType, id)
		txnResp, err := cli.Txn(ctx).
			If(clientv3.Compare(clientv3.Version(key), "=", 0)).
			Then(clientv3.OpPut(key, string(val), clientv3.WithLease(leaseID))).
			Commit()
		if err != nil {
			continue
		}
		if txnResp.Succeeded {
			return id, nil
		}
	}
	return 0, fmt.Errorf("no available node_id in range [0, %d)", searchRange)
}

// KeepAlive starts a background goroutine that maintains the etcd lease
// and re-registers on lease loss.
func (nr *NodeRegistration) KeepAlive() {
	ctx, cancel := context.WithCancel(context.Background())
	nr.cancelFn = cancel

	ch, err := nr.client.KeepAlive(ctx, nr.leaseID)
	if err != nil {
		logx.Errorf("[NodeRegistry] KeepAlive start failed: %v", err)
		return
	}

	go func() {
		for {
			select {
			case ka := <-ch:
				if ka == nil {
					logx.Error("[NodeRegistry] lease lost, attempting re-registration")
					nr.reRegister(ctx)
					return
				}
			case <-ctx.Done():
				return
			}
		}
	}()
}

// reRegister grants a new lease and re-puts the node key.
func (nr *NodeRegistration) reRegister(ctx context.Context) {
	backoff := time.Second
	for {
		select {
		case <-ctx.Done():
			return
		default:
		}

		resp, err := nr.client.Grant(ctx, nr.ttl)
		if err != nil {
			logx.Errorf("[NodeRegistry] re-grant lease failed: %v, retrying in %v", err, backoff)
			time.Sleep(backoff)
			backoff = min(backoff*2, 30*time.Second)
			continue
		}
		nr.leaseID = resp.ID

		_, err = nr.client.Put(ctx, nr.key, nr.value, clientv3.WithLease(nr.leaseID))
		if err != nil {
			logx.Errorf("[NodeRegistry] re-put failed: %v, retrying in %v", err, backoff)
			time.Sleep(backoff)
			backoff = min(backoff*2, 30*time.Second)
			continue
		}

		ch, err := nr.client.KeepAlive(ctx, nr.leaseID)
		if err != nil {
			logx.Errorf("[NodeRegistry] restart keepalive failed: %v", err)
			return
		}

		go func() {
			for {
				select {
				case ka := <-ch:
					if ka == nil {
						logx.Error("[NodeRegistry] lease lost again, re-registering")
						nr.reRegister(ctx)
						return
					}
				case <-ctx.Done():
					return
				}
			}
		}()

		logx.Info("[NodeRegistry] re-registration completed")
		return
	}
}

// Close stops keepalive, deletes the etcd key, and revokes the lease.
func (nr *NodeRegistration) Close() {
	if nr.cancelFn != nil {
		nr.cancelFn()
	}

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	if _, err := nr.client.Delete(ctx, nr.key); err != nil {
		logx.Errorf("[NodeRegistry] failed to delete key: %v", err)
	}
	if _, err := nr.client.Revoke(ctx, nr.leaseID); err != nil {
		logx.Errorf("[NodeRegistry] failed to revoke lease: %v", err)
	}
}
