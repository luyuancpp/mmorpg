package logic

import (
	"context"
	"fmt"
	"time"

	"proto/scene_manager"
	"scene_manager/internal/constants"
	"scene_manager/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
)

// Redis key for the active-instance sorted set (score = create timestamp).
const (
	ActiveInstancesKeyFmt  = "instances:zone:%d:active"
	InstancePlayerCountKey = "instance:%d:player_count"
)

func activeInstancesKey(zoneID uint32) string {
	return fmt.Sprintf(ActiveInstancesKeyFmt, zoneID)
}

type CreateSceneLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewCreateSceneLogic(ctx context.Context, svcCtx *svc.ServiceContext) *CreateSceneLogic {
	return &CreateSceneLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

// CreateScene routes to world or instance creation based on scene type.
// If scene_type is unset, it auto-detects from the World table.
func (l *CreateSceneLogic) CreateScene(in *scene_manager.CreateSceneRequest) (*scene_manager.CreateSceneResponse, error) {
	sceneType := l.resolveSceneType(in)

	switch sceneType {
	case constants.SceneTypeMainWorld:
		return l.createMainWorldScene(in)
	case constants.SceneTypeInstance:
		return l.createInstance(in)
	default:
		return &scene_manager.CreateSceneResponse{
			ErrorCode:    constants.ErrInvalidSceneType,
			ErrorMessage: "unknown scene type",
		}, nil
	}
}

// resolveSceneType determines the scene type from the request or config.
func (l *CreateSceneLogic) resolveSceneType(in *scene_manager.CreateSceneRequest) uint32 {
	// Proto enum: 1 = main world, 2 = instance. 0 = auto-detect.
	if in.SceneType > 0 {
		return uint32(in.SceneType)
	}
	if IsWorldConf(l.svcCtx, in.SceneConfId) {
		return constants.SceneTypeMainWorld
	}
	return constants.SceneTypeInstance
}

// createMainWorldScene returns the least-loaded channel for a world scene.
// Channels are pre-created by initWorldScenesForZone at startup; this is a fallback
// that also creates channels on-demand if they somehow don't exist yet.
func (l *CreateSceneLogic) createMainWorldScene(in *scene_manager.CreateSceneRequest) (*scene_manager.CreateSceneResponse, error) {
	// Fast path: channels already exist — return the least-loaded one.
	sceneId, nodeId, _ := GetBestWorldChannel(l.ctx, l.svcCtx, in.SceneConfId, in.ZoneId)
	if sceneId > 0 {
		l.Logger.Infof("[MainWorld] Best channel: scene=%d conf=%d node=%s", sceneId, in.SceneConfId, nodeId)
		return &scene_manager.CreateSceneResponse{SceneId: sceneId, NodeId: nodeId}, nil
	}

	// Slow path: no channels exist -- create them now (startup race or missing init).
	l.Logger.Infof("[MainWorld] No channels for conf %d in zone %d, creating on demand", in.SceneConfId, in.ZoneId)
	initWorldScenesForZone(l.ctx, l.svcCtx, in.ZoneId, []uint64{in.SceneConfId})

	sceneId, nodeId, _ = GetBestWorldChannel(l.ctx, l.svcCtx, in.SceneConfId, in.ZoneId)
	if sceneId > 0 {
		return &scene_manager.CreateSceneResponse{SceneId: sceneId, NodeId: nodeId}, nil
	}

	return &scene_manager.CreateSceneResponse{
		ErrorCode:    constants.ErrNoAvailableNode,
		ErrorMessage: "failed to create main world channels",
	}, nil
}

// createInstance creates an on-demand instance with lifecycle tracking.
func (l *CreateSceneLogic) createInstance(in *scene_manager.CreateSceneRequest) (*scene_manager.CreateSceneResponse, error) {
	// Load-balance: pick the least-loaded node.
	targetNode := in.TargetNodeId
	if targetNode == "" {
		bestNode, err := GetBestNode(l.ctx, l.svcCtx, in.ZoneId)
		if err != nil {
			l.Logger.Errorf("[Instance] No available node: %v", err)
			return &scene_manager.CreateSceneResponse{ErrorCode: constants.ErrNoAvailableNode, ErrorMessage: err.Error()}, nil
		}
		targetNode = bestNode
	}

	sceneId, err := l.allocateScene(in.SceneConfId, targetNode)
	if err != nil {
		return &scene_manager.CreateSceneResponse{ErrorCode: constants.ErrRedis, ErrorMessage: err.Error()}, nil
	}

	// Track in active instances sorted set (score = creation timestamp).
	nowUnix := time.Now().Unix()
	instKey := activeInstancesKey(in.ZoneId)
	if _, err := l.svcCtx.Redis.Zadd(instKey, nowUnix, fmt.Sprintf("%d", sceneId)); err != nil {
		l.Logger.Errorf("[Instance] Failed to track instance %d: %v", sceneId, err)
	}

	// Initialize player count to 0.
	l.svcCtx.Redis.Set(fmt.Sprintf(InstancePlayerCountKey, sceneId), "0")

	// Notify the C++ scene node to instantiate the ECS scene entity.
	if _, err := RequestNodeCreateScene(l.ctx, l.svcCtx, targetNode, uint32(in.SceneConfId), sceneId); err != nil {
		l.Logger.Errorf("[Instance] Failed to call CreateScene on node %s for instance %d: %v (Redis state committed)", targetNode, sceneId, err)
	}

	l.Logger.Infof("[Instance] Created instance %d (conf=%d) on node %s", sceneId, in.SceneConfId, targetNode)
	return &scene_manager.CreateSceneResponse{SceneId: sceneId, NodeId: targetNode}, nil
}

// allocateScene generates a scene ID and registers it in Redis.
// Shared by both main-world and instance creation.
func (l *CreateSceneLogic) allocateScene(confId uint64, targetNode string) (uint64, error) {
	id, err := l.svcCtx.Redis.Incr("scene:id_counter")
	if err != nil {
		l.Logger.Errorf("Failed to generate scene id: %v", err)
		return 0, fmt.Errorf("redis incr failed: %w", err)
	}
	sceneId := uint64(id)

	// scene -> node mapping.
	if err := l.svcCtx.Redis.Set(fmt.Sprintf("scene:%d:node", sceneId), targetNode); err != nil {
		return 0, fmt.Errorf("redis set scene node failed: %w", err)
	}

	// Increment node scene count for load tracking.
	sceneCountKey := fmt.Sprintf(NodeSceneCountKey, targetNode)
	if _, err := l.svcCtx.Redis.Incr(sceneCountKey); err != nil {
		l.Logger.Errorf("Failed to increment scene count for node %s: %v", targetNode, err)
	}

	return sceneId, nil
}
