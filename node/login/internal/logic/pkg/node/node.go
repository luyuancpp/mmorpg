// internal/node/node.go
package node

import (
	"fmt"
	"github.com/golang/protobuf/proto"
	clientv3 "go.etcd.io/etcd/client/v3"
	"golang.org/x/net/context"
	"login/internal/config"
	etcd "login/internal/logic/pkg/etcd"
	"login/pb/game"
	"time"
)

type Node struct {
	Info   *game.NodeInfo
	reg    *etcd.NodeRegistry
	client *clientv3.Client
}

func NewNode(nodeType uint32, ip string, port uint32, ttl int64) *Node {
	// 创建节点信息
	info := &game.NodeInfo{
		NodeId:   uint32(time.Now().UnixNano()),
		NodeType: nodeType,
		Endpoint: &game.EndpointPBComponent{
			Ip:   ip,
			Port: port,
		},
		ZoneId:       config.AppConfig.ZoneID,
		LaunchTime:   uint64(time.Now().Unix()),
		ProtocolType: uint32(game.ENodeProtocolType_GRPC),
	}

	// 初始化 Etcd 客户端
	client, err := etcd.NewClient([]string{"localhost:2379"})
	if err != nil {
		return nil
	}

	// 创建节点注册表
	reg, err := etcd.NewNodeRegistry(client, ttl)
	if err != nil {
		return nil
	}

	allocator := NewNodeAllocator(client, game.ENodeType_name[int32(nodeType)]+".rpc")
	nodeID, err := allocator.TryAllocateNodeID(context.Background(), info, reg.Lease)
	if err != nil {
		return nil
	}

	info.NodeId = uint32(nodeID)

	return &Node{
		Info:   info,
		reg:    reg,
		client: client,
	}
}

func (n *Node) Register() error {
	// 序列化节点信息
	data, err := proto.Marshal(n.Info)
	if err != nil {
		return fmt.Errorf("failed to marshal node Info: %v", err)
	}

	// 注册节点
	key := fmt.Sprintf("services/%d", n.Info.NodeId)
	err = n.reg.RegisterNode(key, string(data))
	if err != nil {
		return fmt.Errorf("failed to register node: %v", err)
	}

	// 保持租约有效
	n.reg.KeepAlive()

	return nil
}
