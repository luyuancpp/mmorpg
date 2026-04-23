package logic

import (
	"context"
	"encoding/json"
	"fmt"
	"sync"

	scenepb "proto/scene"
	scenenodepb "proto/scene_manager"

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
// sceneId is the Go-allocated unique ID; C++ uses it for per-scene idempotency.
// If the node is unreachable, the error is returned but Redis state is already committed.
func RequestNodeCreateScene(ctx context.Context, svcCtx *svc.ServiceContext, nodeId string, configId uint32, sceneId uint64) (*scenepb.CreateSceneResponse, error) {
	return RequestNodeCreateSceneWithOptions(ctx, svcCtx, nodeId, configId, sceneId, 0, nil)
}

// RequestNodeCreateSceneWithOptions is the full-featured variant that also
// forwards mirror_config_id and creator_ids to the C++ SceneNode. The basic
// RequestNodeCreateScene remains for world-scene paths that never carry mirror
// or creator metadata.
func RequestNodeCreateSceneWithOptions(
	ctx context.Context,
	svcCtx *svc.ServiceContext,
	nodeId string,
	configId uint32,
	sceneId uint64,
	mirrorConfigId uint32,
	creatorIds []uint64,
) (*scenepb.CreateSceneResponse, error) {
	if svcCtx.Etcd == nil {
		return nil, fmt.Errorf("etcd client not available, skipping CreateScene RPC to node %s", nodeId)
	}

	conn, err := getOrDialNode(ctx, svcCtx, nodeId)
	if err != nil {
		return nil, fmt.Errorf("dial scene node %s: %w", nodeId, err)
	}

	client := scenenodepb.NewSceneNodeGrpcClient(conn)
	resp, err := client.CreateScene(ctx, &scenepb.CreateSceneRequest{
		ConfigId:       configId,
		SceneId:        sceneId,
		MirrorConfigId: mirrorConfigId,
		CreatorIds:     creatorIds,
	})
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

	client := scenenodepb.NewSceneNodeGrpcClient(conn)
	if _, err := client.DestroyScene(ctx, &scenepb.DestroySceneRequest{SceneId: sceneId}); err != nil {
		return fmt.Errorf("DestroyScene RPC to node %s: %w", nodeId, err)
	}

	return nil
}

// RequestNodeReleasePlayer dials the C++ scene node identified by nodeId
// (the player's previous scene node) and asks it to release the player so
// the new node can load fresh state from Redis. Used during cross-node
// scene switches; same-node switches do not need this.
func RequestNodeReleasePlayer(ctx context.Context, svcCtx *svc.ServiceContext, nodeId string, playerId uint64, targetSceneId uint64, targetNodeId string) error {
	if svcCtx.Etcd == nil {
		return fmt.Errorf("etcd client not available, skipping ReleasePlayer RPC to node %s", nodeId)
	}

	conn, err := getOrDialNode(ctx, svcCtx, nodeId)
	if err != nil {
		return fmt.Errorf("dial scene node %s: %w", nodeId, err)
	}

	client := scenenodepb.NewSceneNodeGrpcClient(conn)
	if _, err := client.ReleasePlayer(ctx, &scenenodepb.ReleasePlayerRequest{
		PlayerId:      playerId,
		TargetSceneId: targetSceneId,
		TargetNodeId:  targetNodeId,
	}); err != nil {
		return fmt.Errorf("ReleasePlayer RPC to node %s: %w", nodeId, err)
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

// resolveNodeEndpoint returns the "ip:port" gRPC address for a scene node.
// It first checks the in-memory knownNodes map (maintained by the watch loop),
// then falls back to a fresh etcd query.
func resolveNodeEndpoint(ctx context.Context, svcCtx *svc.ServiceContext, nodeId string) (string, error) {
	// Fast path: use the watch-maintained in-memory registry.
	if ep, ok := resolveFromKnownNodes(nodeId); ok {
		return ep, nil
	}

	// Slow path: query etcd directly (covers race at startup before first fullSync).
	prefix := "SceneNodeService.rpc/"
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
			if reg.GrpcEndpoint.Port > 0 {
				return fmt.Sprintf("%s:%d", reg.GrpcEndpoint.IP, reg.GrpcEndpoint.Port), nil
			}
			return "", fmt.Errorf("scene node %s has no grpc_endpoint (would fall back to raw TCP port, causing protocol mismatch)", nodeId)
		}
	}

	return "", fmt.Errorf("scene node %s not found in etcd", nodeId)
}

// resolveFromKnownNodes checks the in-memory node registry for the endpoint.
func resolveFromKnownNodes(nodeId string) (string, bool) {
	knownNodesMu.RLock()
	defer knownNodesMu.RUnlock()
	for _, entry := range knownNodes {
		if entry.nodeID == nodeId {
			if entry.reg.GrpcEndpoint.Port > 0 {
				return fmt.Sprintf("%s:%d", entry.reg.GrpcEndpoint.IP, entry.reg.GrpcEndpoint.Port), true
			}
			// Do NOT fall back to entry.reg.Endpoint -- that is the raw TCP protobuf
			// port. Dialing it with gRPC sends an HTTP/2 preface which the protobuf
			// codec rejects as InvalidLength.
			return "", false
		}
	}
	return "", false
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
