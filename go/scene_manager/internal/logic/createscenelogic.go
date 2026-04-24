package logic

import (
	"context"
	"fmt"
	"strconv"
	"time"

	"proto/scene_manager"
	"scene_manager/internal/constants"
	"scene_manager/internal/metrics"
	"scene_manager/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
)

// Redis key for the active-instance sorted set (score = create timestamp).
const (
	ActiveInstancesKeyFmt  = "instances:zone:%d:active"
	InstancePlayerCountKey = "instance:%d:player_count"
	SceneNodeKeyFmt        = "scene:%d:node"
	SceneZoneKeyFmt        = "scene:%d:zone"
	// SceneMirrorFlagKeyFmt marks a scene as a mirror so the lifecycle
	// manager can apply MirrorIdleTimeoutSeconds instead of the standard
	// InstanceIdleTimeoutSeconds. Value is "1" when present, key absent
	// for non-mirror scenes.
	SceneMirrorFlagKeyFmt = "scene:%d:mirror"
	// SceneSourceKeyFmt stores the source scene id of a mirror so we can
	// reverse-look up which source's `mirrors` set this scene is a member of
	// when destroying the mirror. Only present when the mirror was created
	// with source_scene_id > 0.
	SceneSourceKeyFmt = "scene:%d:source"
	// SceneMirrorsKeyFmt is the Redis SET of mirror scene ids whose source
	// is the given scene id. Populated at mirror creation, consumed by the
	// cascade-destroy path when a scene with mirrors is removed.
	SceneMirrorsKeyFmt = "scene:%d:mirrors"
	// NodeScenesKeyFmt is the Redis SET of scene ids currently hosted by a
	// scene node. Populated by allocateScene, drained by every destroy path.
	// Used by the node-death reconciliation loop to find orphan instances.
	NodeScenesKeyFmt = "node:%s:scenes"
)

func sceneMirrorFlagKey(sceneID uint64) string {
	return fmt.Sprintf(SceneMirrorFlagKeyFmt, sceneID)
}

func sceneSourceKey(sceneID uint64) string {
	return fmt.Sprintf(SceneSourceKeyFmt, sceneID)
}

func sceneMirrorsKey(sourceSceneID uint64) string {
	return fmt.Sprintf(SceneMirrorsKeyFmt, sourceSceneID)
}

func nodeScenesKey(nodeID string) string {
	return fmt.Sprintf(NodeScenesKeyFmt, nodeID)
}

func sceneZoneKey(sceneID uint64) string {
	return fmt.Sprintf(SceneZoneKeyFmt, sceneID)
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

// findExistingMirror picks one mirror id from scene:{sourceId}:mirrors. Used
// by the MirrorDedupBySource code path. Returns (0, false) when the set is
// empty or the read fails. The selection is arbitrary (Redis SET ordering is
// undefined), which matches the contract of "any existing mirror is fine".
func (l *CreateSceneLogic) findExistingMirror(sourceId uint64) (uint64, bool) {
	members, err := l.svcCtx.Redis.Smembers(sceneMirrorsKey(sourceId))
	if err != nil || len(members) == 0 {
		return 0, false
	}
	for _, m := range members {
		id, err := strconv.ParseUint(m, 10, 64)
		if err == nil && id > 0 {
			return id, true
		}
	}
	return 0, false
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
//
// Defensive checks performed before allocation:
//
//  1. "Source-clone" mirror — MirrorConfigId == 0 && SourceSceneId > 0 —
//     means the mirror inherits ALL state (map, NPC table, spawn data)
//     from the source. If the source is gone there is nothing to clone,
//     so we reject with ErrSourceSceneGone instead of silently producing
//     an unplayable scene. Mirrors with their own MirrorConfigId carry
//     their own definition and are fine even when the source disappears
//     (only co-location placement falls back).
//  2. MirrorDedupBySource enabled -> if any mirror already exists for the
//     given source, return that mirror's id instead of allocating a new
//     one. Off by default because the typical mirror use case is per-player
//     phasing where independent copies are intentional.
func (l *CreateSceneLogic) createInstance(in *scene_manager.CreateSceneRequest) (*scene_manager.CreateSceneResponse, error) {
	if in.SourceSceneId > 0 {
		// Only enforce source-existence when the source IS the template
		// (no MirrorConfigId provided). Otherwise the source is just a
		// co-location hint and pickInstanceNode handles the fallback.
		if in.MirrorConfigId == 0 {
			exists, err := l.svcCtx.Redis.Exists(sceneNodeKey(in.SourceSceneId))
			if err != nil {
				l.Logger.Errorf("[Instance] EXISTS scene:%d:node failed for source-clone mirror: %v", in.SourceSceneId, err)
			} else if !exists {
				l.Logger.Infof("[Instance] Refusing source-clone mirror: source scene %d is gone (caller=%v conf=%d)",
					in.SourceSceneId, in.CreatorIds, in.SceneConfId)
				metrics.ObserveMirrorSourceMissing(in.ZoneId)
				return &scene_manager.CreateSceneResponse{
					ErrorCode:    constants.ErrSourceSceneGone,
					ErrorMessage: fmt.Sprintf("source scene %d no longer exists", in.SourceSceneId),
				}, nil
			}
		}

		if l.svcCtx.Config.MirrorDedupBySource {
			if existingId, ok := l.findExistingMirror(in.SourceSceneId); ok {
				if nodeId, err := l.svcCtx.Redis.Get(sceneNodeKey(existingId)); err == nil && nodeId != "" {
					l.Logger.Infof("[Instance] Dedup: reusing mirror %d (source %d) on node %s",
						existingId, in.SourceSceneId, nodeId)
					metrics.ObserveMirrorDedup(in.ZoneId, "hit")
					return &scene_manager.CreateSceneResponse{SceneId: existingId, NodeId: nodeId}, nil
				}
				// Stale entry in the mirrors set — the mirror was destroyed
				// but its membership wasn't cleaned. Drop it and fall through
				// to a fresh allocation.
				l.svcCtx.Redis.Srem(sceneMirrorsKey(in.SourceSceneId), strconv.FormatUint(existingId, 10))
				metrics.ObserveMirrorDedup(in.ZoneId, "stale")
			} else {
				metrics.ObserveMirrorDedup(in.ZoneId, "miss")
			}
		}
	}

	targetNode, err := l.pickInstanceNode(in)
	if err != nil {
		l.Logger.Errorf("[Instance] No instance-hosting node for zone %d (strict=%v): %v",
			in.ZoneId, l.svcCtx.Config.StrictNodeTypeSeparation, err)
		// Use ErrNoNodeForPurpose when strict mode rejected the request — it
		// tells operators the pool was empty by policy, not by outage.
		code := constants.ErrNoAvailableNode
		if l.svcCtx.Config.StrictNodeTypeSeparation {
			code = constants.ErrNoNodeForPurpose
		}
		return &scene_manager.CreateSceneResponse{ErrorCode: code, ErrorMessage: err.Error()}, nil
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

	// Reverse indexes for cascade destroy and node-death reconciliation.
	// These use SET operations which are naturally idempotent — safe to
	// re-run on retry.
	if in.SourceSceneId > 0 {
		// scene:{sourceId}:mirrors gives us O(1) enumeration of "mirrors of
		// this scene" when the source is destroyed or the source node dies.
		if _, err := l.svcCtx.Redis.Sadd(sceneMirrorsKey(in.SourceSceneId), fmt.Sprintf("%d", sceneId)); err != nil {
			l.Logger.Errorf("[Instance] Failed to add mirror %d to source %d's mirror set: %v", sceneId, in.SourceSceneId, err)
		}
		// scene:{mirrorId}:source remembers the source so the mirror can
		// SREM itself from that set on destroy without a full table scan.
		l.svcCtx.Redis.Set(sceneSourceKey(sceneId), fmt.Sprintf("%d", in.SourceSceneId))
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
//  1. Explicit TargetNodeId from the caller (respects existing behaviour —
//     operators can always pin a target node, e.g. diagnostic sessions).
//  2. Mirror co-location: if SourceSceneId > 0, reuse the source scene's node
//     when it is alive and under the soft load cap. Intentionally bypasses
//     the instance-purpose filter so mirrors of main-world scenes can share
//     the world node's already-resident map/AI/spawn data. Without this
//     escape hatch StrictNodeTypeSeparation=true would force every mirror
//     to a fresh instance node, defeating the whole optimization.
//  3. Default: GetBestNodeForPurpose(Instance) — picks the lowest-load
//     instance-hosting node.
//
// The mirror path logs its decision at INFO on success and WARN on fallback
// so operators can observe how often co-location is taking effect.
func (l *CreateSceneLogic) pickInstanceNode(in *scene_manager.CreateSceneRequest) (string, error) {
	if in.TargetNodeId != "" {
		return in.TargetNodeId, nil
	}

	if in.SourceSceneId > 0 {
		if node, reason := l.resolveMirrorSourceNode(in.SourceSceneId, in.ZoneId); node != "" {
			metrics.ObserveMirrorColocate(in.ZoneId, "hit", "ok")
			l.Logger.Infof("[Mirror] Co-locating mirror (conf=%d mirror_conf=%d) with source scene %d on node %s (cross-type allowed)",
				in.SceneConfId, in.MirrorConfigId, in.SourceSceneId, node)
			return node, nil
		} else {
			metrics.ObserveMirrorColocate(in.ZoneId, "fallback", reason)
			// Reason already logged inside resolveMirrorSourceNode.
		}
	}

	return GetBestNodeForPurpose(l.ctx, l.svcCtx, in.ZoneId, constants.NodePurposeInstance)
}

// resolveMirrorSourceNode returns the node hosting sourceSceneId when that
// node is a viable co-location target. On success returns (node, "ok").
// On fallback returns ("", <reason>) where reason is one of:
//   - "no_mapping": source scene has no scene:{id}:node entry
//   - "node_dead":  source node is not in the zone's load set
//   - "overloaded": source node is at/over MirrorSourceNodeLoadCap
// The reason string is used as a Prometheus label and also logged so
// operators can diagnose why a mirror fell through to GetBestNode.
func (l *CreateSceneLogic) resolveMirrorSourceNode(sourceSceneId uint64, zoneId uint32) (string, string) {
	nodeId, err := l.svcCtx.Redis.Get(sceneNodeKey(sourceSceneId))
	if err != nil || nodeId == "" {
		l.Logger.Infof("[Mirror] source scene %d has no node mapping (err=%v), falling back to GetBestNode", sourceSceneId, err)
		return "", "no_mapping"
	}

	if !IsNodeAlive(l.svcCtx, zoneId, nodeId) {
		l.Logger.Infof("[Mirror] source node %s (scene %d) is not alive in zone %d, falling back to GetBestNode",
			nodeId, sourceSceneId, zoneId)
		return "", "node_dead"
	}

	loadCap := l.svcCtx.Config.MirrorSourceNodeLoadCap
	if loadCap > 0 {
		if load, ok := l.getNodeSceneCount(nodeId); ok && load >= loadCap {
			l.Logger.Infof("[Mirror] source node %s load=%d >= cap=%d, falling back to GetBestNode",
				nodeId, load, loadCap)
			return "", "overloaded"
		}
	}

	return nodeId, "ok"
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
	l.svcCtx.Redis.Set(sceneZoneKey(sceneId), fmt.Sprintf("%d", zoneId))

	// Increment node scene count for load tracking.
	sceneCountKey := fmt.Sprintf(NodeSceneCountKey, targetNode)
	if _, err := l.svcCtx.Redis.Incr(sceneCountKey); err != nil {
		l.Logger.Errorf("Failed to increment scene count for node %s: %v", targetNode, err)
	}

	// Reverse index: node -> scenes. The node-death reconciliation loop
	// uses this to find orphan scenes without scanning the entire keyspace.
	if _, err := l.svcCtx.Redis.Sadd(nodeScenesKey(targetNode), fmt.Sprintf("%d", sceneId)); err != nil {
		l.Logger.Errorf("Failed to add scene %d to node %s scenes set: %v", sceneId, targetNode, err)
	}

	return sceneId, nil
}
