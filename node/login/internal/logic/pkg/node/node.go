// internal/node/node.go
package node

import (
	"fmt"
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
	Client *clientv3.Client
}

// BuildRpcPrefix 生成不包含 node_id 的路径
func BuildRpcPrefix(serviceName string, zoneId, nodeType uint32) string {
	return fmt.Sprintf("%s/zone/%d/node_type/%d/", serviceName, zoneId, nodeType)
}

// BuildRpcPath 生成包含 node_id 的完整路径
func BuildRpcPath(serviceName string, zoneId, nodeType, nodeId uint32) string {
	return fmt.Sprintf("%s/zone/%d/node_type/%d/node_id/%d", serviceName, zoneId, nodeType, nodeId)
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
		ProtocolType: uint32(game.ENodeProtocolType_PROTOCOL_GRPC),
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
		Client: client,
	}
}

func (n *Node) KeepAlive() error {
	// 保持租约有效
	n.reg.KeepAlive()

	return nil
}
