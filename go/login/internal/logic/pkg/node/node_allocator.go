package node

import (
	"context"
	"encoding/json"
	"fmt"
	"game/generated/pb/game"
	"github.com/zeromicro/go-zero/core/logx"
	clientv3 "go.etcd.io/etcd/client/v3"
)

type NodeAllocator struct {
	Client *clientv3.Client
	Prefix string
}

func NewNodeAllocator(client *clientv3.Client, prefix string) *NodeAllocator {
	return &NodeAllocator{Client: client, Prefix: prefix}
}

// TryAllocateNodeID 尝试分配一个 node_id（复用已有空位），最大向上找空位
func (na *NodeAllocator) TryAllocateNodeID(ctx context.Context, info *game.NodeInfo, leaseID clientv3.LeaseID) (uint32, error) {
	// 获取现有节点
	resp, err := na.Client.Get(ctx, na.Prefix, clientv3.WithPrefix())
	if err != nil {
		return 0, err
	}

	usedIDs := make(map[uint32]bool)
	maxID := uint32(0)

	for _, kv := range resp.Kvs {
		data := &game.NodeInfo{}
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

	// 向上查找可用 ID
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

func (na *NodeAllocator) putIfAbsent(ctx context.Context, nodeID uint32, info *game.NodeInfo, leaseID clientv3.LeaseID) (bool, error) {
	key := BuildRpcPath(na.Prefix, info.ZoneId, info.NodeType, nodeID)

	info.NodeId = nodeID

	marshaler := &jsonpb.Marshaler{}

	result, err := marshaler.MarshalToString(info)
	if err != nil {
		logx.Errorf("Error marshaling:", err)
		return false, err
	}

	txn := na.Client.Txn(ctx)
	txnResp, err := txn.If(
		clientv3.Compare(clientv3.Version(key), "=", 0),
	).Then(
		clientv3.OpPut(key, result, clientv3.WithLease(leaseID)),
	).Commit()

	if err != nil {
		return false, err
	}
	return txnResp.Succeeded, nil
}
