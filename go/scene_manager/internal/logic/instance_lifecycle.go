package logic

import (
	"context"
	"fmt"
	"strconv"
	"time"

	"scene_manager/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
)

// StartInstanceLifecycleManager periodically scans active instances and
// destroys those that have been idle (0 players) longer than the configured timeout.
// Mirrors use MirrorIdleTimeoutSeconds (shorter) instead of the regular instance
// timeout, because every entry re-initializes NPCs — empty mirrors are pure waste.
func StartInstanceLifecycleManager(ctx context.Context, svcCtx *svc.ServiceContext) {
	instanceTimeout := svcCtx.Config.InstanceIdleTimeoutSeconds
	mirrorTimeout := resolveMirrorTimeout(svcCtx)

	if instanceTimeout <= 0 && mirrorTimeout <= 0 {
		logx.Info("[InstanceLifecycle] Both InstanceIdleTimeoutSeconds and MirrorIdleTimeoutSeconds=0, auto-destroy disabled")
		return
	}

	intervalSec := svcCtx.Config.InstanceCheckIntervalSeconds
	if intervalSec <= 0 {
		intervalSec = 30
	}
	interval := time.Duration(intervalSec) * time.Second

	logx.Infof("[InstanceLifecycle] Started: check every %ds, instance idle %ds, mirror idle %ds",
		intervalSec, instanceTimeout, mirrorTimeout)

	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			cleanupIdleInstances(ctx, svcCtx, instanceTimeout, mirrorTimeout)
		}
	}
}

// resolveMirrorTimeout returns the effective idle timeout for mirror scenes.
// A 0 value falls back to InstanceIdleTimeoutSeconds so operators can opt out
// of the shorter schedule by leaving the new field unset.
func resolveMirrorTimeout(svcCtx *svc.ServiceContext) int64 {
	if t := svcCtx.Config.MirrorIdleTimeoutSeconds; t > 0 {
		return t
	}
	return svcCtx.Config.InstanceIdleTimeoutSeconds
}

// cleanupIdleInstances iterates over all known zones and destroys
// instances that have had 0 players for longer than the timeout.
func cleanupIdleInstances(ctx context.Context, svcCtx *svc.ServiceContext, instanceTimeout, mirrorTimeout int64) {
	zones := GetActiveZones()
	for _, zoneId := range zones {
		cleanupZoneIdleInstances(ctx, svcCtx, zoneId, instanceTimeout, mirrorTimeout)
	}
}

// cleanupZoneIdleInstances scans the active-instance sorted set for a single zone
// and destroys instances that have been idle longer than their per-type timeout.
// Mirror scenes (scene:{id}:mirror == "1") use mirrorTimeout; the rest use instanceTimeout.
// A non-positive timeout for a given kind means "never auto-destroy this kind".
func cleanupZoneIdleInstances(ctx context.Context, svcCtx *svc.ServiceContext, zoneId uint32, instanceTimeout, mirrorTimeout int64) {
	instKey := activeInstancesKey(zoneId)

	// Get all active instances with their creation timestamps.
	pairs, err := svcCtx.Redis.ZrangeWithScores(instKey, 0, -1)
	if err != nil {
		logx.Errorf("[InstanceLifecycle] Failed to list active instances: %v", err)
		return
	}

	if len(pairs) == 0 {
		return
	}

	now := time.Now().Unix()
	destroyed := 0

	for _, p := range pairs {
		sceneId, err := strconv.ParseUint(p.Key, 10, 64)
		if err != nil {
			continue
		}

		// Check player count.
		playerCountStr, _ := svcCtx.Redis.Get(fmt.Sprintf(InstancePlayerCountKey, sceneId))
		playerCount := 0
		if playerCountStr != "" {
			fmt.Sscanf(playerCountStr, "%d", &playerCount)
		}

		if playerCount > 0 {
			// Instance is active, update its "last active" time.
			// (Re-add with current timestamp to reset the idle clock.)
			svcCtx.Redis.Zadd(instKey, now, p.Key)
			continue
		}

		// Pick the right idle budget for this scene kind.
		isMirror := false
		if flag, _ := svcCtx.Redis.Get(sceneMirrorFlagKey(sceneId)); flag == "1" {
			isMirror = true
		}
		timeoutSec := instanceTimeout
		kind := "instance"
		if isMirror {
			timeoutSec = mirrorTimeout
			kind = "mirror"
		}

		if timeoutSec <= 0 {
			// Auto-destroy disabled for this kind.
			continue
		}

		// Player count is 0. Check how long it has been idle.
		lastActiveTime := int64(p.Score)
		idleDuration := now - lastActiveTime

		if idleDuration < timeoutSec {
			continue
		}

		// Idle timeout exceeded — destroy this instance.
		logx.Infof("[InstanceLifecycle] Destroying idle %s %d (idle %ds > timeout %ds)",
			kind, sceneId, idleDuration, timeoutSec)

		destroyInstance(ctx, svcCtx, zoneId, sceneId)
		destroyed++
	}

	if destroyed > 0 {
		logx.Infof("[InstanceLifecycle] Zone %d cleanup done: destroyed %d idle instances", zoneId, destroyed)
	}
}

// destroyInstance removes all Redis state for an instance and notifies
// the C++ scene node to destroy the ECS entity.
func destroyInstance(ctx context.Context, svcCtx *svc.ServiceContext, zoneId uint32, sceneId uint64) {
	sceneIdStr := fmt.Sprintf("%d", sceneId)
	sceneNodeKey := fmt.Sprintf("scene:%d:node", sceneId)

	// Get node for load tracking and RPC before deletion.
	nodeId, _ := svcCtx.Redis.Get(sceneNodeKey)

	// Notify C++ node to destroy the ECS scene entity (skip if node is already dead).
	if nodeId != "" && IsNodeAlive(svcCtx, zoneId, nodeId) {
		if err := RequestNodeDestroyScene(ctx, svcCtx, nodeId, sceneId); err != nil {
			logx.Errorf("[InstanceLifecycle] Failed to call DestroyScene on node %s for scene %d: %v", nodeId, sceneId, err)
		}
	}

	// Remove from scene -> node mapping.
	svcCtx.Redis.Del(sceneNodeKey)

	// Remove from active instances sorted set.
	instKey := activeInstancesKey(zoneId)
	svcCtx.Redis.Zrem(instKey, sceneIdStr)

	// Read final player count BEFORE deleting so we can drain that many
	// from the per-node aggregate. This covers the rare case where an
	// instance is force-destroyed with players still in it (e.g. admin
	// shutdown, server crash recovery) — the per-node counter would
	// otherwise leak the residual forever.
	residualKey := fmt.Sprintf(InstancePlayerCountKey, sceneId)
	residualStr, _ := svcCtx.Redis.Get(residualKey)
	residual, _ := strconv.ParseInt(residualStr, 10, 64)
	svcCtx.Redis.Del(residualKey)

	// Remove mirror flag (no-op if this wasn't a mirror).
	svcCtx.Redis.Del(sceneMirrorFlagKey(sceneId))

	// Decrement node counters.
	if nodeId != "" {
		sceneCountKey := fmt.Sprintf(NodeSceneCountKey, nodeId)
		svcCtx.Redis.Incrby(sceneCountKey, -1)

		if residual > 0 {
			playerCountKey := fmt.Sprintf(NodePlayerCountKey, nodeId)
			newVal, err := svcCtx.Redis.Incrby(playerCountKey, -residual)
			if err == nil && newVal < 0 {
				svcCtx.Redis.Set(playerCountKey, "0")
			}
		}
	}
}

// IncrInstancePlayerCount increments both the per-scene and per-node player
// counters. The per-node counter feeds into GetBestNode's composite load
// score so nodes with many concurrent players become less attractive even
// when they host few scenes.
//
// Call order matters: scene counter first, then per-node aggregate. The
// two writes are not atomic — a crash between them skews the per-node
// counter by at most one, which the zset score refresher smooths out.
func IncrInstancePlayerCount(svcCtx *svc.ServiceContext, sceneId uint64) {
	svcCtx.Redis.Incr(fmt.Sprintf(InstancePlayerCountKey, sceneId))
	if nodeId := lookupSceneNode(svcCtx, sceneId); nodeId != "" {
		svcCtx.Redis.Incr(fmt.Sprintf(NodePlayerCountKey, nodeId))
	}
}

// DecrInstancePlayerCount mirrors IncrInstancePlayerCount. Both counters
// are clamped to 0 so a missed EnterScene or a stale LeaveScene can't drive
// them negative.
func DecrInstancePlayerCount(svcCtx *svc.ServiceContext, sceneId uint64) {
	key := fmt.Sprintf(InstancePlayerCountKey, sceneId)
	val, err := svcCtx.Redis.Incrby(key, -1)
	if err == nil && val < 0 {
		svcCtx.Redis.Set(key, "0")
	}

	if nodeId := lookupSceneNode(svcCtx, sceneId); nodeId != "" {
		nodeKey := fmt.Sprintf(NodePlayerCountKey, nodeId)
		nodeVal, err := svcCtx.Redis.Incrby(nodeKey, -1)
		if err == nil && nodeVal < 0 {
			svcCtx.Redis.Set(nodeKey, "0")
		}
	}
}

// lookupSceneNode resolves the node currently hosting a scene, returning ""
// when the mapping is missing. Used by the player-count helpers; a missing
// mapping is a soft error (per-node aggregate skew of at most one event).
func lookupSceneNode(svcCtx *svc.ServiceContext, sceneId uint64) string {
	nodeId, _ := svcCtx.Redis.Get(fmt.Sprintf(SceneNodeKeyFmt, sceneId))
	return nodeId
}
