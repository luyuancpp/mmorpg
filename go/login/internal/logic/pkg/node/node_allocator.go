package node

import (
	"context"
	"encoding/json"
	"fmt"
	"github.com/zeromicro/go-zero/core/logx"
	clientv3 "go.etcd.io/etcd/client/v3"
	"google.golang.org/protobuf/encoding/protojson"
	login_proto "login/proto/common"
)

type NodeAllocator struct {
	Client *clientv3.Client
	Prefix string
}

func NewNodeAllocator(client *clientv3.Client, prefix string) *NodeAllocator {
	return &NodeAllocator{Client: client, Prefix: prefix}
}

// TryAllocateNodeID 尝试分配一个 node_id（复用已有空位），最大向上找空位
func (na *NodeAllocator) TryAllocateNodeID(ctx context.Context, info *login_proto.NodeInfo, leaseID clientv3.LeaseID) (uint32, error) {
	// 获取现有节点
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

func (na *NodeAllocator) putIfAbsent(ctx context.Context, nodeID uint32, info *login_proto.NodeInfo, leaseID clientv3.LeaseID) (bool, error) {
	key := BuildRpcPath(na.Prefix, info.ZoneId, info.NodeType, nodeID)

	info.NodeId = nodeID

	// 使用 protojson.Marshal 序列化
	result, err := protojson.Marshal(info)
	if err != nil {
		logx.Errorf("Error marshaling: %v", err)
		return false, err
	}

	// 将 []byte 转换为 string
	resultStr := string(result)

	// 使用转换后的字符串执行 Etcd 操作
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
