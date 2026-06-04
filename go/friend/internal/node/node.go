package node

// 本文件中的 allocator 逻辑在 guild / friend / player_locator / scene_manager
// 四个服务里是逐字相同的副本(只有 import 包名不同)。
// 若需修改 allocator 行为,请同步修改另外三个服务的对应文件:
//   - go/guild/internal/node/node.go
//   - go/player_locator/internal/node/node.go
//   - go/scene_manager/internal/noderegistry/registry.go

import (
	"context"
	"fmt"
	"strconv"
	"strings"
	"time"

	"github.com/google/uuid"
	"github.com/zeromicro/go-zero/core/logx"
	clientv3 "go.etcd.io/etcd/client/v3"
	"google.golang.org/protobuf/encoding/protojson"

	"friend/internal/config"
	base "proto/common/base"
	"shared/snowflake"
)

// nodeIDMin 是合法 node_id 的下界。0 永远作为 "未分配 / 非法值" 保留,
// 不会被 allocator 分配出来。
const nodeIDMin uint32 = 1

// nodeIDMax 与 Snowflake worker id 位宽对齐(shared/snowflake.NodeMask)。
var nodeIDMax = uint32(snowflake.NodeMask)

type Node struct {
	Info       *base.NodeInfo
	client     *clientv3.Client
	leaseID    clientv3.LeaseID
	cancelFunc context.CancelFunc
}

func rpcPrefix(nodeType uint32) string {
	return fmt.Sprintf("%s.rpc", base.ENodeType_name[int32(nodeType)])
}

func rpcPath(prefix string, zoneId, nodeType, nodeId uint32) string {
	return fmt.Sprintf("%s/zone/%d/node_type/%d/node_id/%d", prefix, zoneId, nodeType, nodeId)
}

// allocationKey 是跨 zone 的全局占位 key —— 路径里不带 zone,
// 因此两个 zone 的实例不可能同时拿到同一个 (node_type, node_id),
// 修复了 Snowflake worker id 跨 zone 冲突。
func allocationKey(prefix string, nodeType, nodeID uint32) string {
	return fmt.Sprintf("%s/allocated/node_type/%d/node_id/%d", prefix, nodeType, nodeID)
}

func allocationKeyPrefix(prefix string, nodeType uint32) string {
	return fmt.Sprintf("%s/allocated/node_type/%d/node_id/", prefix, nodeType)
}

func NewNode(nodeType uint32, ip string, port uint32) (*Node, error) {
	cfg := config.AppConfig

	client, err := clientv3.New(clientv3.Config{
		Endpoints:   cfg.Registry.Etcd.Hosts,
		DialTimeout: cfg.Registry.Etcd.DialTimeout,
	})
	if err != nil {
		return nil, fmt.Errorf("etcd connect failed: %w", err)
	}

	grant, err := client.Grant(context.Background(), cfg.Node.LeaseTTL)
	if err != nil {
		client.Close()
		return nil, fmt.Errorf("etcd grant failed: %w", err)
	}

	info := &base.NodeInfo{
		NodeType: nodeType,
		Endpoint: &base.EndpointComp{
			Ip:   ip,
			Port: port,
		},
		ZoneId:       cfg.Node.ZoneId,
		LaunchTime:   uint64(time.Now().Unix()),
		ProtocolType: uint32(base.ENodeProtocolType_PROTOCOL_GRPC),
		NodeUuid:     uuid.New().String(),
	}

	prefix := rpcPrefix(nodeType)
	nodeID, err := allocateNodeID(context.Background(), client, prefix, info, grant.ID)
	if err != nil {
		_, _ = client.Revoke(context.Background(), grant.ID)
		client.Close()
		return nil, err
	}
	info.NodeId = nodeID

	return &Node{
		Info:    info,
		client:  client,
		leaseID: grant.ID,
	}, nil
}

// allocateNodeID 在 (node_type) 下找空闲 node_id 并通过 etcd Txn CAS 占住。
// 详细设计见 go/guild/internal/node/node.go 同名函数。
func allocateNodeID(ctx context.Context, client *clientv3.Client, prefix string, info *base.NodeInfo, leaseID clientv3.LeaseID) (uint32, error) {
	usedIDs, err := scanUsedNodeIDs(ctx, client, prefix, info.NodeType)
	if err != nil {
		return 0, err
	}

	for id := nodeIDMin; id <= nodeIDMax; id++ {
		if usedIDs[id] {
			continue
		}
		ok, err := tryClaimNodeID(ctx, client, prefix, id, info, leaseID)
		if err != nil {
			logx.Errorf("friend allocator: txn failed for node_id=%d: %v", id, err)
			continue
		}
		if ok {
			return id, nil
		}
		usedIDs[id] = true
	}
	return 0, fmt.Errorf("no available node_id in [%d, %d]", nodeIDMin, nodeIDMax)
}

func scanUsedNodeIDs(ctx context.Context, client *clientv3.Client, prefix string, nodeType uint32) (map[uint32]bool, error) {
	used := make(map[uint32]bool)

	allocPrefix := allocationKeyPrefix(prefix, nodeType)
	allocResp, err := client.Get(ctx, allocPrefix, clientv3.WithPrefix())
	if err != nil {
		return nil, fmt.Errorf("etcd get alloc prefix: %w", err)
	}
	for _, kv := range allocResp.Kvs {
		tail := strings.TrimPrefix(string(kv.Key), allocPrefix)
		if id, err := strconv.ParseUint(tail, 10, 32); err == nil {
			used[uint32(id)] = true
		}
	}

	rpcResp, err := client.Get(ctx, prefix, clientv3.WithPrefix())
	if err != nil {
		return nil, fmt.Errorf("etcd get rpc prefix: %w", err)
	}
	for _, kv := range rpcResp.Kvs {
		key := string(kv.Key)
		if strings.Contains(key, "/allocated/") {
			continue
		}
		var ni base.NodeInfo
		if err := protojson.Unmarshal(kv.Value, &ni); err != nil {
			continue
		}
		if ni.NodeId >= nodeIDMin {
			used[ni.NodeId] = true
		}
	}

	return used, nil
}

func tryClaimNodeID(ctx context.Context, client *clientv3.Client, prefix string, nodeID uint32, info *base.NodeInfo, leaseID clientv3.LeaseID) (bool, error) {
	allocKey := allocationKey(prefix, info.NodeType, nodeID)
	rpcKey := rpcPath(prefix, info.ZoneId, info.NodeType, nodeID)

	info.NodeId = nodeID
	value, err := protojson.Marshal(info)
	if err != nil {
		return false, fmt.Errorf("marshal node info: %w", err)
	}

	txnResp, err := client.Txn(ctx).
		If(clientv3.Compare(clientv3.Version(allocKey), "=", 0)).
		Then(
			clientv3.OpPut(allocKey, info.NodeUuid, clientv3.WithLease(leaseID)),
			clientv3.OpPut(rpcKey, string(value), clientv3.WithLease(leaseID)),
		).
		Commit()
	if err != nil {
		return false, err
	}
	return txnResp.Succeeded, nil
}

func (n *Node) KeepAlive() error {
	ctx, cancel := context.WithCancel(context.Background())
	n.cancelFunc = cancel

	ch, err := n.client.KeepAlive(ctx, n.leaseID)
	if err != nil {
		cancel()
		return fmt.Errorf("keep alive failed: %w", err)
	}

	go func() {
		for {
			select {
			case ka, ok := <-ch:
				if !ok {
					logx.Error("friend etcd keep alive channel closed")
					return
				}
				_ = ka
			case <-ctx.Done():
				return
			}
		}
	}()

	return nil
}

func (n *Node) Close() {
	if n.cancelFunc != nil {
		n.cancelFunc()
	}
	if n.client == nil {
		return
	}

	prefix := rpcPrefix(n.Info.NodeType)
	rpcKey := rpcPath(prefix, n.Info.ZoneId, n.Info.NodeType, n.Info.NodeId)
	allocKey := allocationKey(prefix, n.Info.NodeType, n.Info.NodeId)

	if _, err := n.client.Txn(context.Background()).
		Then(clientv3.OpDelete(rpcKey), clientv3.OpDelete(allocKey)).
		Commit(); err != nil {
		logx.Errorf("friend node close: delete keys failed: %v", err)
	}

	if _, err := n.client.Revoke(context.Background(), n.leaseID); err != nil {
		logx.Errorf("friend node close: revoke lease failed: %v", err)
	}
	n.client.Close()
}
