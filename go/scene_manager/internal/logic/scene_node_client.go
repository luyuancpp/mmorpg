package logic

import (
	"context"
	"encoding/json"
	"fmt"
	"sync"

	scenepb "proto/scene"

	"scene_manager/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
	clientv3 "go.etcd.io/etcd/client/v3"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

// nodeConnCache caches gRPC connections to C++ scene nodes, keyed by nodeId.
// Connections are reused across calls and cleaned up on node removal.
var (
	nodeConnMu    sync.RWMutex
	nodeConnCache = make(map[string]*grpc.ClientConn)
)

// RequestNodeCreateScene dials the C++ scene node identified by nodeId and calls
// CreateScene to instantiate the ECS scene entity.
// If the node is unreachable, the error is returned but Redis state is already committed.
func RequestNodeCreateScene(ctx context.Context, svcCtx *svc.ServiceContext, nodeId string, configId uint32) (*scenepb.CreateSceneResponse, error) {
	if svcCtx.Etcd == nil {
		return nil, fmt.Errorf("etcd client not available, skipping CreateScene RPC to node %s", nodeId)
	}

	conn, err := getOrDialNode(ctx, svcCtx, nodeId)
	if err != nil {
		return nil, fmt.Errorf("dial scene node %s: %w", nodeId, err)
	}

	client := scenepb.NewSceneClient(conn)
	resp, err := client.CreateScene(ctx, &scenepb.CreateSceneRequest{ConfigId: configId})
	if err != nil {
		return nil, fmt.Errorf("CreateScene RPC to node %s: %w", nodeId, err)
	}

	return resp, nil
}

// RequestNodeDestroyScene dials the C++ scene node and calls DestroyScene
// to remove the ECS scene entity.
func RequestNodeDestroyScene(ctx context.Context, svcCtx *svc.ServiceContext, nodeId string, sceneId uint64) error {
	if svcCtx.Etcd == nil {
		return fmt.Errorf("etcd client not available, skipping DestroyScene RPC to node %s", nodeId)
	}

	conn, err := getOrDialNode(ctx, svcCtx, nodeId)
	if err != nil {
		return fmt.Errorf("dial scene node %s: %w", nodeId, err)
	}

	client := scenepb.NewSceneClient(conn)
	if _, err := client.DestroyScene(ctx, &scenepb.DestroySceneRequest{SceneId: sceneId}); err != nil {
		return fmt.Errorf("DestroyScene RPC to node %s: %w", nodeId, err)
	}

	return nil
}

// getOrDialNode returns a cached connection or discovers the node endpoint
// from etcd and dials it.
func getOrDialNode(ctx context.Context, svcCtx *svc.ServiceContext, nodeId string) (*grpc.ClientConn, error) {
	nodeConnMu.RLock()
	conn, ok := nodeConnCache[nodeId]
	nodeConnMu.RUnlock()
	if ok {
		return conn, nil
	}

	// Discover endpoint from etcd.
	endpoint, err := resolveNodeEndpoint(ctx, svcCtx, nodeId)
	if err != nil {
		return nil, err
	}

	conn, err = grpc.NewClient(endpoint,
		grpc.WithTransportCredentials(insecure.NewCredentials()),
	)
	if err != nil {
		return nil, fmt.Errorf("grpc dial %s: %w", endpoint, err)
	}

	nodeConnMu.Lock()
	// Double-check: another goroutine may have added it.
	if existing, ok := nodeConnCache[nodeId]; ok {
		nodeConnMu.Unlock()
		conn.Close()
		return existing, nil
	}
	nodeConnCache[nodeId] = conn
	nodeConnMu.Unlock()

	logx.Infof("[SceneNodeClient] Connected to scene node %s at %s", nodeId, endpoint)
	return conn, nil
}

// resolveNodeEndpoint looks up the SceneNode registration in etcd and returns
// the "ip:port" address string.
func resolveNodeEndpoint(ctx context.Context, svcCtx *svc.ServiceContext, nodeId string) (string, error) {
	prefix := fmt.Sprintf("SceneNodeService.rpc/zone/%d/", svcCtx.Config.ZoneID)
	resp, err := svcCtx.Etcd.Get(ctx, prefix, clientv3.WithPrefix())
	if err != nil {
		return "", fmt.Errorf("etcd get %s: %w", prefix, err)
	}

	for _, kv := range resp.Kvs {
		var reg sceneNodeRegistration
		if err := json.Unmarshal(kv.Value, &reg); err != nil {
			continue
		}
		if fmt.Sprintf("%d", reg.NodeId) == nodeId {
			return fmt.Sprintf("%s:%d", reg.Endpoint.IP, reg.Endpoint.Port), nil
		}
	}

	return "", fmt.Errorf("scene node %s not found in etcd", nodeId)
}

// RemoveNodeConn closes and removes the cached connection for a node.
// Called when a node is removed from etcd.
func RemoveNodeConn(nodeId string) {
	nodeConnMu.Lock()
	defer nodeConnMu.Unlock()
	if conn, ok := nodeConnCache[nodeId]; ok {
		conn.Close()
		delete(nodeConnCache, nodeId)
		logx.Infof("[SceneNodeClient] Removed connection to scene node %s", nodeId)
	}
}
