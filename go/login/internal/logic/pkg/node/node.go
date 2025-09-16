// internal/node/node.go
package node

import (
	"context"
	"fmt"
	"generated/pb/game/"
	"game/internal/config"
	etcd "game/internal/logic/pkg/etcd"
	"github.com/google/uuid"
	"github.com/zeromicro/go-zero/core/logx"
	clientv3 "go.etcd.io/etcd/client/v3"
	"time"
)

type Node struct {
	Info         *game.NodeInfo
	reg          *etcd.NodeRegistry
	Client       *clientv3.Client
	cancelFunc   context.CancelFunc // 用于取消 KeepAlive goroutine
	keepAliveCtx context.Context    // 控制 KeepAlive 的上下文
}

// GetRpcPrefix 根据 NodeType 返回对应的 RPC 路径
func GetRpcPrefix(nodeType uint32) string {
	// 获取对应的 NodeType 名称并拼接 ".rpc"
	return fmt.Sprintf("%s.rpc", game.ENodeType_name[int32(nodeType)])
}

// BuildRpcPrefix 生成不包含 node_id 的路径
func BuildRpcPrefix(serviceName string, zoneId, nodeType uint32) string {
	return fmt.Sprintf("%s.rpc/zone/%d/node_type/%d/", serviceName, zoneId, nodeType)
}

// BuildRpcPath 生成包含 node_id 的完整路径
func BuildRpcPath(serviceName string, zoneId, nodeType, nodeId uint32) string {
	return fmt.Sprintf("%s/zone/%d/node_type/%d/node_id/%d", serviceName, zoneId, nodeType, nodeId)
}

func NewNode(nodeType uint32, ip string, port uint32, ttl int64) *Node {
	// 初始化 Etcd 客户端
	client, err := etcd.NewClient()
	if err != nil {
		return nil
	}

	// 创建节点注册表
	reg, err := etcd.NewNodeRegistry(client, ttl)
	if err != nil {
		return nil
	}

	// 创建节点信息
	info := &game.NodeInfo{
		NodeId:   uint32(time.Now().UnixNano()),
		NodeType: nodeType,
		Endpoint: &game.EndpointPBComponent{
			Ip:   ip,
			Port: port,
		},
		ZoneId:       config.AppConfig.Node.ZoneId,
		LaunchTime:   uint64(time.Now().Unix()),
		ProtocolType: uint32(game.ENodeProtocolType_PROTOCOL_GRPC),
		NodeUuid:     uuid.New().String(),
	}

	allocator := NewNodeAllocator(client, GetRpcPrefix(nodeType))
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

// 新增 Node 类型的 Close 方法
func (n *Node) Close() error {
	// 停止 KeepAlive goroutine
	if n.cancelFunc != nil {
		n.cancelFunc()
	}

	// 删除节点注册信息的 key
	key := BuildRpcPath(GetRpcPrefix(n.Info.NodeType), n.Info.ZoneId, n.Info.NodeType, n.Info.NodeId)
	_, err := n.Client.Delete(context.Background(), key)
	if err != nil {
		logx.Error("Failed to delete node key from etcd: %v", err)
		return fmt.Errorf("failed to delete node key: %v", err)
	}

	// 取消租约
	err = n.reg.RevokeLease()
	if err != nil {
		logx.Error("Failed to revoke lease: %v", err)
		return fmt.Errorf("failed to revoke lease: %v", err)
	}

	return nil
}

// KeepAlive 更新以支持取消上下文
func (n *Node) KeepAlive() error {
	// 创建一个可以取消的 context
	n.keepAliveCtx, n.cancelFunc = context.WithCancel(context.Background())

	// 调用 NodeRegistry 中的 KeepAlive
	n.reg.KeepAlive(n.keepAliveCtx)

	return nil
}
