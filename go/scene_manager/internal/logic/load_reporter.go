package logic

import (
	"context"
	"encoding/json"
	"fmt"
	"strconv"
	"time"

	"scene_manager/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
	clientv3 "go.etcd.io/etcd/client/v3"
)

const (
	NodeLoadKeyFmt     = "scene_nodes:zone:%d:load"
	NodeSceneCountKey  = "node:%s:scene_count"
	LoadReportInterval = 5 * time.Second

	// NodeLoadKey is the Redis key when ZoneID=0 (used in tests).
	NodeLoadKey = "scene_nodes:zone:0:load"
)

// missingSince tracks when each node was first observed missing from etcd.
// If a node reappears before the grace period expires, its entry is removed.
var missingSince = make(map[string]time.Time)

// nodeLoadKey returns the zone-scoped Redis sorted-set key.
func nodeLoadKey(zoneID uint32) string {
	return fmt.Sprintf(NodeLoadKeyFmt, zoneID)
}

// sceneNodeRegistration mirrors the JSON that C++ scene nodes write to etcd.
type sceneNodeRegistration struct {
	NodeId   uint32 `json:"nodeId"`
	NodeType uint32 `json:"nodeType"`
	Endpoint struct {
		IP   string `json:"ip"`
		Port uint32 `json:"port"`
	} `json:"endpoint"`
	ZoneId uint32 `json:"zoneId"`
}

// StartLoadReporter discovers C++ scene nodes from etcd and publishes their
// availability to the Redis sorted-set used by GetBestNode.
func StartLoadReporter(ctx context.Context, svcCtx *svc.ServiceContext) {
	ticker := time.NewTicker(LoadReportInterval)
	defer ticker.Stop()

	syncSceneNodes(ctx, svcCtx)

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			syncSceneNodes(ctx, svcCtx)
		}
	}
}

// syncSceneNodes reads SceneNodeService entries from etcd for this zone and
// updates the Redis sorted set with numeric node IDs.
func syncSceneNodes(ctx context.Context, svcCtx *svc.ServiceContext) {
	prefix := fmt.Sprintf("SceneNodeService.rpc/zone/%d/", svcCtx.Config.ZoneID)
	resp, err := svcCtx.Etcd.Get(ctx, prefix, clientv3.WithPrefix())
	if err != nil {
		logx.Errorf("etcd get %s failed: %v", prefix, err)
		return
	}

	seen := make(map[string]struct{})
	for _, kv := range resp.Kvs {
		var reg sceneNodeRegistration
		if err := json.Unmarshal(kv.Value, &reg); err != nil {
			logx.Errorf("failed to parse scene node registration %s: %v", string(kv.Key), err)
			continue
		}
		nodeIDStr := strconv.FormatUint(uint64(reg.NodeId), 10)
		seen[nodeIDStr] = struct{}{}

		// Node is present in etcd: clear any pending removal.
		delete(missingSince, nodeIDStr)

		// Read scene count as load indicator (0 if not set).
		sceneCountKey := fmt.Sprintf(NodeSceneCountKey, nodeIDStr)
		var load int64
		if s, e := svcCtx.Redis.Get(sceneCountKey); e == nil && s != "" {
			fmt.Sscanf(s, "%d", &load)
		}

		loadKey := nodeLoadKey(svcCtx.Config.ZoneID)
		if _, err := svcCtx.Redis.Zadd(loadKey, load, nodeIDStr); err != nil {
			logx.Errorf("failed to update load for node %s: %v", nodeIDStr, err)
		}
	}

	// Remove stale entries from the ZSet, respecting the grace period.
	graceDuration := time.Duration(svcCtx.Config.NodeRemovalGraceSeconds) * time.Second
	loadKey := nodeLoadKey(svcCtx.Config.ZoneID)
	pairs, err := svcCtx.Redis.ZrangeWithScores(loadKey, 0, -1)
	if err == nil {
		now := time.Now()
		for _, p := range pairs {
			if _, ok := seen[p.Key]; ok {
				continue
			}
			// Node is missing from etcd.
			if graceDuration <= 0 {
				// No grace period: remove immediately.
				svcCtx.Redis.Zrem(loadKey, p.Key)
				continue
			}
			firstMissing, tracked := missingSince[p.Key]
			if !tracked {
				// First time we noticed this node is missing. Start the clock.
				missingSince[p.Key] = now
				logx.Infof("scene node %s missing from etcd, grace period started (%ds)", p.Key, svcCtx.Config.NodeRemovalGraceSeconds)
				continue
			}
			if now.Sub(firstMissing) >= graceDuration {
				// Grace period expired: remove for real.
				logx.Infof("scene node %s grace period expired, removing from load set", p.Key)
				svcCtx.Redis.Zrem(loadKey, p.Key)
				delete(missingSince, p.Key)
			}
		}
	}
}

func reportLoad(ctx context.Context, svcCtx *svc.ServiceContext) {
	// Use the number of scenes hosted on this node as load indicator.
	// Each CreateScene increments the counter; each DestroyScene decrements it.
	sceneCountKey := fmt.Sprintf(NodeSceneCountKey, svcCtx.Config.NodeID)
	countStr, err := svcCtx.Redis.Get(sceneCountKey)
	var currentLoad int64
	if err == nil && countStr != "" {
		if _, scanErr := fmt.Sscanf(countStr, "%d", &currentLoad); scanErr != nil {
			logx.Errorf("Failed to parse scene count for %s: %v", svcCtx.Config.NodeID, scanErr)
		}
	}

	// Update Redis ZSet with score = load
	_, err = svcCtx.Redis.Zadd(nodeLoadKey(svcCtx.Config.ZoneID), currentLoad, svcCtx.Config.NodeID)
	if err != nil {
		logx.Errorf("Failed to update node load for %s: %v", svcCtx.Config.NodeID, err)
	}
}

// GetBestNode selects the node with the lowest load from Redis
func GetBestNode(ctx context.Context, svcCtx *svc.ServiceContext) (string, error) {
	pairs, err := svcCtx.Redis.ZrangeWithScores(nodeLoadKey(svcCtx.Config.ZoneID), 0, 0)
	if err != nil {
		return "", fmt.Errorf("redis zrange failed: %w", err)
	}

	if len(pairs) == 0 {
		return "", fmt.Errorf("no scene nodes available for zone %d", svcCtx.Config.ZoneID)
	}

	return pairs[0].Key, nil
}
