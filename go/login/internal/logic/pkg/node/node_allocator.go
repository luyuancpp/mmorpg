package node

import (
	"context"
	"encoding/json"
	"fmt"
	login_proto "login/proto/common/base"

	"github.com/zeromicro/go-zero/core/logx"
	clientv3 "go.etcd.io/etcd/client/v3"
	"google.golang.org/protobuf/encoding/protojson"
)

type NodeAllocator struct {
	Client *clientv3.Client
	Prefix string
}

func NewNodeAllocator(client *clientv3.Client, prefix string) *NodeAllocator {
	return &NodeAllocator{Client: client, Prefix: prefix}
}

// TryAllocateNodeID tries to allocate a node_id (reusing empty slots), searching upward for gaps.
func (na *NodeAllocator) TryAllocateNodeID(ctx context.Context, info *login_proto.NodeInfo, leaseID clientv3.LeaseID) (uint32, error) {
	// Get existing nodes
	resp, err := na.Client.Get(ctx, na.Prefix, clientv3.WithPrefix())
	if err != nil {
		return 0, err
	}

	usedIDs := make(map[uint32]bool)
	maxID := uint32(0)

	for _, kv := range resp.Kvs {
		data := &login_proto.NodeInfo{}
		err := json.Unmarshal(kv.Value, &data)
		if err != nil {
			continue
		}

		id := data.NodeId
		usedIDs[id] = true
		if id > maxID {
			maxID = id
		}
	}

	searchRange := maxID + 10

	// Search upward for an available ID
	for id := uint32(0); id < searchRange; id++ {
		if !usedIDs[id] {
			ok, err := na.putIfAbsent(ctx, id, info, leaseID)
			if err != nil {
				continue
			}
			if ok {
				return id, nil
			}
		}
	}

	return 0, fmt.Errorf("failed to allocate node ID")
}

func (na *NodeAllocator) putIfAbsent(ctx context.Context, nodeID uint32, info *login_proto.NodeInfo, leaseID clientv3.LeaseID) (bool, error) {
	key := BuildRpcPath(na.Prefix, info.ZoneId, info.NodeType, nodeID)

	info.NodeId = nodeID

	// Serialize with protojson
	result, err := protojson.Marshal(info)
	if err != nil {
		logx.Errorf("Error marshaling: %v", err)
		return false, err
	}

	resultStr := string(result)

	// Use Etcd transaction for compare-and-swap
	txn := na.Client.Txn(ctx)
	txnResp, err := txn.If(
		clientv3.Compare(clientv3.Version(key), "=", 0),
	).Then(
		clientv3.OpPut(key, resultStr, clientv3.WithLease(leaseID)),
	).Commit()

	if err != nil {
		return false, err
	}
	return txnResp.Succeeded, nil
}
