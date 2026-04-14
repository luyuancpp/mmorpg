package logic

import (
	"context"
	"encoding/json"
	"fmt"
	"strconv"
	"sync"
	"time"

	"scene_manager/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
	clientv3 "go.etcd.io/etcd/client/v3"
)

const (
	NodeLoadKeyFmt     = "scene_nodes:zone:%d:load"
	NodeSceneCountKey  = "node:%s:scene_count"
	LoadReportInterval = 5 * time.Second
)

// missingSince tracks when each node was first observed missing from etcd.
// If a node reappears before the grace period expires, its entry is removed.
var (
	missingSinceMu sync.Mutex
	missingSince   = make(map[string]time.Time)
)

// knownZones tracks all zone IDs discovered from etcd scene node registrations.
// This allows the SceneManager to be zone-agnostic: any instance can serve any zone.
var (
	knownZonesMu sync.RWMutex
	knownZones   = make(map[uint32]struct{})
)

// GetKnownZones returns all zone IDs currently discovered from etcd.
func GetKnownZones() []uint32 {
	knownZonesMu.RLock()
	defer knownZonesMu.RUnlock()
	zones := make([]uint32, 0, len(knownZones))
	for z := range knownZones {
		zones = append(zones, z)
	}
	return zones
}

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
// availability to per-zone Redis sorted-sets used by GetBestNode.
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

// syncSceneNodes reads ALL SceneNodeService entries from etcd (across all zones)
// and updates per-zone Redis sorted sets with numeric node IDs.
func syncSceneNodes(ctx context.Context, svcCtx *svc.ServiceContext) {
	prefix := "SceneNodeService.rpc/"
	resp, err := svcCtx.Etcd.Get(ctx, prefix, clientv3.WithPrefix())
	if err != nil {
		logx.Errorf("etcd get %s failed: %v", prefix, err)
		return
	}

	// Track which nodes are seen per zone in this sync pass.
	seenByZone := make(map[uint32]map[string]struct{})

	for _, kv := range resp.Kvs {
		var reg sceneNodeRegistration
		if err := json.Unmarshal(kv.Value, &reg); err != nil {
			logx.Errorf("failed to parse scene node registration %s: %v", string(kv.Key), err)
			continue
		}
		nodeIDStr := strconv.FormatUint(uint64(reg.NodeId), 10)
		zoneId := reg.ZoneId

		if seenByZone[zoneId] == nil {
			seenByZone[zoneId] = make(map[string]struct{})
		}
		seenByZone[zoneId][nodeIDStr] = struct{}{}

		// Node is present in etcd: clear any pending removal.
		missingSinceMu.Lock()
		delete(missingSince, nodeIDStr)
		missingSinceMu.Unlock()

		// Read scene count as load indicator (0 if not set).
		sceneCountKey := fmt.Sprintf(NodeSceneCountKey, nodeIDStr)
		var load int64
		if s, e := svcCtx.Redis.Get(sceneCountKey); e == nil && s != "" {
			fmt.Sscanf(s, "%d", &load)
		}

		loadKey := nodeLoadKey(zoneId)
		if _, err := svcCtx.Redis.Zadd(loadKey, load, nodeIDStr); err != nil {
			logx.Errorf("failed to update load for node %s (zone %d): %v", nodeIDStr, zoneId, err)
		}
	}

	// Merge discovered zones into the known set.
	knownZonesMu.Lock()
	for z := range seenByZone {
		knownZones[z] = struct{}{}
	}
	allZones := make([]uint32, 0, len(knownZones))
	for z := range knownZones {
		allZones = append(allZones, z)
	}
	knownZonesMu.Unlock()

	// Remove stale entries per zone, respecting the grace period.
	graceDuration := time.Duration(svcCtx.Config.NodeRemovalGraceSeconds) * time.Second

	for _, zoneId := range allZones {
		seen := seenByZone[zoneId] // nil if no nodes in this zone this tick
		if seen == nil {
			seen = make(map[string]struct{})
		}

		loadKey := nodeLoadKey(zoneId)
		pairs, err := svcCtx.Redis.ZrangeWithScores(loadKey, 0, -1)
		if err != nil {
			continue
		}

		now := time.Now()
		missingSinceMu.Lock()
		for _, p := range pairs {
			if _, ok := seen[p.Key]; ok {
				continue
			}
			// Node is missing from etcd.
			if graceDuration <= 0 {
				// No grace period: remove immediately.
				svcCtx.Redis.Zrem(loadKey, p.Key)
				RemoveNodeConn(p.Key)
				continue
			}
			firstMissing, tracked := missingSince[p.Key]
			if !tracked {
				// First time we noticed this node is missing. Start the clock.
				missingSince[p.Key] = now
				logx.Infof("scene node %s missing from etcd (zone %d), grace period started (%ds)", p.Key, zoneId, svcCtx.Config.NodeRemovalGraceSeconds)
				continue
			}
			if now.Sub(firstMissing) >= graceDuration {
				// Grace period expired: remove for real.
				logx.Infof("scene node %s grace period expired (zone %d), removing from load set", p.Key, zoneId)
				svcCtx.Redis.Zrem(loadKey, p.Key)
				delete(missingSince, p.Key)
				RemoveNodeConn(p.Key)
			}
		}
		missingSinceMu.Unlock()
	}
}

// GetBestNode selects the node with the lowest load from the given zone's Redis sorted set.
func GetBestNode(ctx context.Context, svcCtx *svc.ServiceContext, zoneId uint32) (string, error) {
	pairs, err := svcCtx.Redis.ZrangeWithScores(nodeLoadKey(zoneId), 0, 0)
	if err != nil {
		return "", fmt.Errorf("redis zrange failed: %w", err)
	}

	if len(pairs) == 0 {
		return "", fmt.Errorf("no scene nodes available for zone %d", zoneId)
	}

	return pairs[0].Key, nil
}
