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
func StartInstanceLifecycleManager(ctx context.Context, svcCtx *svc.ServiceContext) {
	timeoutSec := svcCtx.Config.InstanceIdleTimeoutSeconds
	if timeoutSec <= 0 {
		logx.Info("[InstanceLifecycle] InstanceIdleTimeoutSeconds=0, auto-destroy disabled")
		return
	}

	intervalSec := svcCtx.Config.InstanceCheckIntervalSeconds
	if intervalSec <= 0 {
		intervalSec = 30
	}
	interval := time.Duration(intervalSec) * time.Second

	logx.Infof("[InstanceLifecycle] Started: check every %ds, idle timeout %ds", intervalSec, timeoutSec)

	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			cleanupIdleInstances(ctx, svcCtx, timeoutSec)
		}
	}
}

// cleanupIdleInstances iterates over all known zones and destroys
// instances that have had 0 players for longer than the timeout.
func cleanupIdleInstances(ctx context.Context, svcCtx *svc.ServiceContext, timeoutSec int64) {
	zones := GetActiveZones()
	for _, zoneId := range zones {
		cleanupZoneIdleInstances(ctx, svcCtx, zoneId, timeoutSec)
	}
}

// cleanupZoneIdleInstances scans the active-instance sorted set for a single zone
// and destroys instances that have been idle longer than the timeout.
func cleanupZoneIdleInstances(ctx context.Context, svcCtx *svc.ServiceContext, zoneId uint32, timeoutSec int64) {
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

		// Player count is 0. Check how long it has been idle.
		lastActiveTime := int64(p.Score)
		idleDuration := now - lastActiveTime

		if idleDuration < timeoutSec {
			continue
		}

		// Idle timeout exceeded — destroy this instance.
		logx.Infof("[InstanceLifecycle] Destroying idle instance %d (idle %ds > timeout %ds)",
			sceneId, idleDuration, timeoutSec)

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

	// Notify C++ node to destroy the ECS scene entity.
	if nodeId != "" {
		if err := RequestNodeDestroyScene(ctx, svcCtx, nodeId, sceneId); err != nil {
			logx.Errorf("[InstanceLifecycle] Failed to call DestroyScene on node %s for scene %d: %v", nodeId, sceneId, err)
		}
	}

	// Remove from scene -> node mapping.
	svcCtx.Redis.Del(sceneNodeKey)

	// Remove from active instances sorted set.
	instKey := activeInstancesKey(zoneId)
	svcCtx.Redis.Zrem(instKey, sceneIdStr)

	// Remove player count tracker.
	svcCtx.Redis.Del(fmt.Sprintf(InstancePlayerCountKey, sceneId))

	// Decrement node scene count.
	if nodeId != "" {
		sceneCountKey := fmt.Sprintf(NodeSceneCountKey, nodeId)
		svcCtx.Redis.Incrby(sceneCountKey, -1)
	}
}

// IncrInstancePlayerCount increments the player count for an instance scene.
// Called when a player enters the scene.
func IncrInstancePlayerCount(svcCtx *svc.ServiceContext, sceneId uint64) {
	svcCtx.Redis.Incr(fmt.Sprintf(InstancePlayerCountKey, sceneId))
}

// DecrInstancePlayerCount decrements the player count for an instance scene.
// Called when a player leaves the scene. Guards against going below zero.
func DecrInstancePlayerCount(svcCtx *svc.ServiceContext, sceneId uint64) {
	key := fmt.Sprintf(InstancePlayerCountKey, sceneId)
	val, err := svcCtx.Redis.Incrby(key, -1)
	if err == nil && val < 0 {
		// Clamp to 0 — can happen if LeaveScene is called without a matching EnterScene.
		svcCtx.Redis.Set(key, "0")
	}
}
