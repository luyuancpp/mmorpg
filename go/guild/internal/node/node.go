package node

import (
	"context"
	"fmt"
	"time"

	"github.com/google/uuid"
	"github.com/zeromicro/go-zero/core/logx"
	clientv3 "go.etcd.io/etcd/client/v3"
	"google.golang.org/protobuf/encoding/protojson"

	"guild/internal/config"
	base "proto/common/base"
)

type Node struct {
	Info       *base.NodeInfo
	client     *clientv3.Client
	leaseID    clientv3.LeaseID
	cancelFunc context.CancelFunc
}

func rpcPrefix(nodeType uint32) string {
	return fmt.Sprintf("%s.rpc", base.ENodeType_name[int32(nodeType)])
}

func rpcPath(nodeType, zoneId, nodeId uint32) string {
	prefix := rpcPrefix(nodeType)
	return fmt.Sprintf("%s/zone/%d/node_type/%d/node_id/%d", prefix, zoneId, nodeType, nodeId)
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
		client.Close()
		return nil, err
	}
	info.NodeId = uint32(nodeID)

	return &Node{
		Info:    info,
		client:  client,
		leaseID: grant.ID,
	}, nil
}

func allocateNodeID(ctx context.Context, client *clientv3.Client, prefix string, info *base.NodeInfo, leaseID clientv3.LeaseID) (uint32, error) {
	resp, err := client.Get(ctx, prefix, clientv3.WithPrefix())
	if err != nil {
		return 0, fmt.Errorf("etcd get failed: %w", err)
	}

	usedIDs := make(map[uint32]bool)
	var maxID uint32
	for _, kv := range resp.Kvs {
		var n base.NodeInfo
		if err := protojson.Unmarshal(kv.Value, &n); err != nil {
			continue
		}
		usedIDs[n.NodeId] = true
		if n.NodeId > maxID {
			maxID = n.NodeId
		}
	}

	for id := uint32(0); id < maxID+10; id++ {
		if usedIDs[id] {
			continue
		}
		info.NodeId = id
		data, err := protojson.Marshal(info)
		if err != nil {
			continue
		}
		key := rpcPath(info.NodeType, info.ZoneId, id)
		txnResp, err := client.Txn(ctx).
			If(clientv3.Compare(clientv3.Version(key), "=", 0)).
			Then(clientv3.OpPut(key, string(data), clientv3.WithLease(leaseID))).
			Commit()
		if err != nil {
			continue
		}
		if txnResp.Succeeded {
			return id, nil
		}
	}

	return 0, fmt.Errorf("failed to allocate node ID")
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
		for range ch {
			// drain keep-alive responses
		}
		logx.Error("Guild node lease expired")
	}()

	return nil
}

func (n *Node) Close() error {
	if n.cancelFunc != nil {
		n.cancelFunc()
	}

	key := rpcPath(n.Info.NodeType, n.Info.ZoneId, n.Info.NodeId)
	_, err := n.client.Delete(context.Background(), key)
	if err != nil {
		logx.Errorf("deregister node %s: %v", key, err)
	}

	_, _ = n.client.Revoke(context.Background(), n.leaseID)
	return n.client.Close()
}
