package logic

import (
	"context"
	"fmt"
	"strconv"
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
	SceneNodeKeyFmt        = "scene:%d:node"
	// SceneMirrorFlagKeyFmt marks a scene as a mirror so the lifecycle
	// manager can apply MirrorIdleTimeoutSeconds instead of the standard
	// InstanceIdleTimeoutSeconds. Value is "1" when present, key absent
	// for non-mirror scenes.
	SceneMirrorFlagKeyFmt = "scene:%d:mirror"
)

func sceneMirrorFlagKey(sceneID uint64) string {
	return fmt.Sprintf(SceneMirrorFlagKeyFmt, sceneID)
}

func activeInstancesKey(zoneID uint32) string {
	return fmt.Sprintf(ActiveInstancesKeyFmt, zoneID)
}

func sceneNodeKey(sceneID uint64) string {
	return fmt.Sprintf(SceneNodeKeyFmt, sceneID)
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
	if IsWorldConf(in.SceneConfId) {
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
//
// Mirror co-location: when source_scene_id > 0 and the source node is alive
// and not over MirrorSourceNodeLoadCap, the mirror is created on the SAME
// node as its source scene. This reuses already-resident map/AI/spawn data
// and lets clients switch between source ↔ mirror without a cross-node
// handoff. Falls back to GetBestNode otherwise.
func (l *CreateSceneLogic) createInstance(in *scene_manager.CreateSceneRequest) (*scene_manager.CreateSceneResponse, error) {
	targetNode, err := l.pickInstanceNode(in)
	if err != nil {
		l.Logger.Errorf("[Instance] No available node: %v", err)
		return &scene_manager.CreateSceneResponse{ErrorCode: constants.ErrNoAvailableNode, ErrorMessage: err.Error()}, nil
	}

	sceneId, err := l.allocateScene(in.SceneConfId, targetNode, in.ZoneId)
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

	// Tag mirrors so the lifecycle manager can apply the shorter
	// MirrorIdleTimeoutSeconds. Mirrors re-init NPCs on every entry, so
	// lingering with 0 players is pure waste.
	if in.MirrorConfigId > 0 || in.SourceSceneId > 0 {
		if err := l.svcCtx.Redis.Set(sceneMirrorFlagKey(sceneId), "1"); err != nil {
			l.Logger.Errorf("[Instance] Failed to set mirror flag for scene %d: %v", sceneId, err)
		}
	}

	// Notify the C++ scene node to instantiate the ECS scene entity.
	if _, err := RequestNodeCreateSceneWithOptions(
		l.ctx, l.svcCtx, targetNode,
		uint32(in.SceneConfId), sceneId,
		in.MirrorConfigId, in.CreatorIds,
	); err != nil {
		l.Logger.Errorf("[Instance] Failed to call CreateScene on node %s for instance %d: %v (Redis state committed)", targetNode, sceneId, err)
	}

	if in.MirrorConfigId > 0 || in.SourceSceneId > 0 {
		l.Logger.Infof("[Instance] Created mirror %d (conf=%d mirror_conf=%d source_scene=%d) on node %s",
			sceneId, in.SceneConfId, in.MirrorConfigId, in.SourceSceneId, targetNode)
	} else {
		l.Logger.Infof("[Instance] Created instance %d (conf=%d) on node %s", sceneId, in.SceneConfId, targetNode)
	}
	return &scene_manager.CreateSceneResponse{SceneId: sceneId, NodeId: targetNode}, nil
}

// pickInstanceNode decides which scene node should host a new instance.
//
// Priority:
//   1. Explicit TargetNodeId from the caller (respects existing behaviour).
//   2. Mirror co-location: if SourceSceneId > 0, reuse the source scene's node
//      when it is alive and under the soft load cap.
//   3. Default: GetBestNode load-balancing.
//
// The mirror path logs its decision at INFO on success and WARN on fallback
// so operators can observe how often co-location is taking effect.
func (l *CreateSceneLogic) pickInstanceNode(in *scene_manager.CreateSceneRequest) (string, error) {
	if in.TargetNodeId != "" {
		return in.TargetNodeId, nil
	}

	if in.SourceSceneId > 0 {
		if node, ok := l.resolveMirrorSourceNode(in.SourceSceneId, in.ZoneId); ok {
			l.Logger.Infof("[Mirror] Co-locating mirror (conf=%d mirror_conf=%d) with source scene %d on node %s",
				in.SceneConfId, in.MirrorConfigId, in.SourceSceneId, node)
			return node, nil
		}
		// Reason already logged inside resolveMirrorSourceNode.
	}

	return GetBestNode(l.ctx, l.svcCtx, in.ZoneId)
}

// resolveMirrorSourceNode returns the node hosting sourceSceneId when that
// node is a viable co-location target. Returns (node, true) on success;
// (_, false) with a WARN-level log on any of the fallback conditions:
//   - source scene has no Redis mapping,
//   - source node is no longer registered in the zone's load set,
//   - source node is at/over MirrorSourceNodeLoadCap.
func (l *CreateSceneLogic) resolveMirrorSourceNode(sourceSceneId uint64, zoneId uint32) (string, bool) {
	nodeId, err := l.svcCtx.Redis.Get(sceneNodeKey(sourceSceneId))
	if err != nil || nodeId == "" {
		l.Logger.Infof("[Mirror] source scene %d has no node mapping (err=%v), falling back to GetBestNode", sourceSceneId, err)
		return "", false
	}

	if !IsNodeAlive(l.svcCtx, zoneId, nodeId) {
		l.Logger.Infof("[Mirror] source node %s (scene %d) is not alive in zone %d, falling back to GetBestNode",
			nodeId, sourceSceneId, zoneId)
		return "", false
	}

	loadCap := l.svcCtx.Config.MirrorSourceNodeLoadCap
	if loadCap > 0 {
		if load, ok := l.getNodeSceneCount(nodeId); ok && load >= loadCap {
			l.Logger.Infof("[Mirror] source node %s load=%d >= cap=%d, falling back to GetBestNode",
				nodeId, load, loadCap)
			return "", false
		}
	}

	return nodeId, true
}

// getNodeSceneCount reads the per-node scene_count counter used for soft load
// estimation. Returns (0, false) if the key is missing or unreadable so the
// caller treats the node as unknown-load rather than zero-load.
func (l *CreateSceneLogic) getNodeSceneCount(nodeId string) (int64, bool) {
	countStr, err := l.svcCtx.Redis.Get(fmt.Sprintf(NodeSceneCountKey, nodeId))
	if err != nil || countStr == "" {
		return 0, false
	}
	count, err := strconv.ParseInt(countStr, 10, 64)
	if err != nil {
		return 0, false
	}
	return count, true
}

// allocateScene generates a scene ID and registers it in Redis.
// Shared by both main-world and instance creation.
func (l *CreateSceneLogic) allocateScene(confId uint64, targetNode string, zoneId uint32) (uint64, error) {
	sceneId := l.svcCtx.SceneIDGen.Generate()

	// scene -> node mapping.
	if err := l.svcCtx.Redis.Set(sceneNodeKey(sceneId), targetNode); err != nil {
		return 0, fmt.Errorf("redis set scene node failed: %w", err)
	}
	// scene -> zone mapping for cross-zone lookups.
	l.svcCtx.Redis.Set(fmt.Sprintf("scene:%d:zone", sceneId), fmt.Sprintf("%d", zoneId))

	// Increment node scene count for load tracking.
	sceneCountKey := fmt.Sprintf(NodeSceneCountKey, targetNode)
	if _, err := l.svcCtx.Redis.Incr(sceneCountKey); err != nil {
		l.Logger.Errorf("Failed to increment scene count for node %s: %v", targetNode, err)
	}

	return sceneId, nil
}
