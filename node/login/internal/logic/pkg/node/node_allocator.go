package node

import (
	"context"
	"encoding/json"
	"fmt"
	"login/pb/game"
	"strconv"
	"strings"
	"time"

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
func (na *NodeAllocator) TryAllocateNodeID(ctx context.Context, desiredID uint32, info *game.NodeInfo, leaseID clientv3.LeaseID) (uint32, error) {
	// 获取现有节点
	resp, err := na.Client.Get(ctx, na.Prefix, clientv3.WithPrefix())
	if err != nil {
		return 0, err
	}

	usedIDs := make(map[uint32]bool)
	maxID := uint32(0)

	for _, kv := range resp.Kvs {
		idStr := strings.TrimPrefix(string(kv.Key), na.Prefix)
		result, err := strconv.ParseUint(idStr, 10, 32)
		id := uint32(result)
		if err == nil {
			usedIDs[id] = true
			if id > maxID {
				maxID = id
			}
		}
	}

	// 尝试期望 ID 是否可用
	if !usedIDs[desiredID] {
		ok, err := na.putIfAbsent(ctx, desiredID, info, leaseID)
		if err != nil {
			return 0, err
		}
		if ok {
			return desiredID, nil
		}
	}

	// 向上查找可用 ID
	for id := maxID + 1; id < 1_000_000; id++ {
		if !usedIDs[id] {
			ok, err := na.putIfAbsent(ctx, id, info, leaseID)
			if err != nil {
				return 0, err
			}
			if ok {
				return id, nil
			}
		}
	}

	return 0, fmt.Errorf("failed to allocate node ID")
}

func (na *NodeAllocator) putIfAbsent(ctx context.Context, nodeID uint32, info *game.NodeInfo, leaseID clientv3.LeaseID) (bool, error) {
	key := fmt.Sprintf("%s%d", na.Prefix, nodeID)
	info.NodeId = nodeID
	info.LaunchTime = uint64(time.Now().Unix())
	info.LeaseId = uint64(leaseID)

	data, _ := json.Marshal(info)

	txn := na.Client.Txn(ctx)
	txnResp, err := txn.If(
		clientv3.Compare(clientv3.Version(key), "=", 0),
	).Then(
		clientv3.OpPut(key, string(data), clientv3.WithLease(leaseID)),
	).Commit()

	if err != nil {
		return false, err
	}
	return txnResp.Succeeded, nil
}
