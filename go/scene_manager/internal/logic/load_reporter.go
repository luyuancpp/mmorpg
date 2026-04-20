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
	sceneNodePrefix    = "SceneNodeService.rpc/"
)

// activeZones holds the zone IDs derived from currently known nodes.
var (
	activeZonesMu sync.RWMutex
	activeZones   []uint32
)

// GetActiveZones returns the zone IDs from the current known node set.
func GetActiveZones() []uint32 {
	activeZonesMu.RLock()
	defer activeZonesMu.RUnlock()
	result := make([]uint32, len(activeZones))
	copy(result, activeZones)
	return result
}

// nodeLoadKey returns the zone-scoped Redis sorted-set key.
func nodeLoadKey(zoneID uint32) string {
	return fmt.Sprintf(NodeLoadKeyFmt, zoneID)
}

// sceneNodeRegistration mirrors the JSON that C++ scene nodes write to etcd.
type sceneNodeRegistration struct {
	NodeId       uint32 `json:"nodeId"`
	NodeType     uint32 `json:"nodeType"`
	Endpoint     struct {
		IP   string `json:"ip"`
		Port uint32 `json:"port"`
	} `json:"endpoint"`
	GrpcEndpoint struct {
		IP   string `json:"ip"`
		Port uint32 `json:"port"`
	} `json:"grpcEndpoint"`
	ZoneId uint32 `json:"zoneId"`
}

// nodeEntry stores a parsed node with its string ID.
type nodeEntry struct {
	reg    sceneNodeRegistration
	nodeID string
}

// pendingRemoval tracks a deleted node waiting out its grace period.
type pendingRemoval struct {
	since time.Time
	entry nodeEntry
}

// knownNodes tracks nodes currently registered in etcd, keyed by etcd key.
var (
	knownNodesMu sync.RWMutex
	knownNodes   = make(map[string]nodeEntry)
)

// pendingRemovals tracks deleted nodes pending grace-period expiry.
var (
	pendingRemovalsMu sync.Mutex
	pendingRemovals   = make(map[string]pendingRemoval) // nodeID → removal info
)

// parseNodeEntry parses a raw etcd value into a nodeEntry.
func parseNodeEntry(value []byte) (nodeEntry, bool) {
	var reg sceneNodeRegistration
	if err := json.Unmarshal(value, &reg); err != nil {
		logx.Errorf("[LoadReporter] failed to parse node registration: %v", err)
		return nodeEntry{}, false
	}
	return nodeEntry{
		reg:    reg,
		nodeID: strconv.FormatUint(uint64(reg.NodeId), 10),
	}, true
}

// updateNodeLoad reads the node's scene count from Redis and updates the load set.
func updateNodeLoad(svcCtx *svc.ServiceContext, entry nodeEntry) {
	sceneCountKey := fmt.Sprintf(NodeSceneCountKey, entry.nodeID)
	var load int64
	if s, e := svcCtx.Redis.Get(sceneCountKey); e == nil && s != "" {
		fmt.Sscanf(s, "%d", &load)
	}
	loadKey := nodeLoadKey(entry.reg.ZoneId)
	if _, err := svcCtx.Redis.Zadd(loadKey, load, entry.nodeID); err != nil {
		logx.Errorf("[LoadReporter] failed to update load for node %s (zone %d): %v", entry.nodeID, entry.reg.ZoneId, err)
	}
}

// removeNodeFromRedis cleans up a node's load-set entry and cached connection.
func removeNodeFromRedis(svcCtx *svc.ServiceContext, entry nodeEntry) {
	loadKey := nodeLoadKey(entry.reg.ZoneId)
	svcCtx.Redis.Zrem(loadKey, entry.nodeID)
	RemoveNodeConn(entry.nodeID)
	logx.Infof("[LoadReporter] removed node %s from Redis load set (zone %d)", entry.nodeID, entry.reg.ZoneId)
}

// rebuildActiveZones derives activeZones from knownNodes.
func rebuildActiveZones() {
	knownNodesMu.RLock()
	zoneSet := make(map[uint32]struct{})
	for _, entry := range knownNodes {
		zoneSet[entry.reg.ZoneId] = struct{}{}
	}
	knownNodesMu.RUnlock()

	zones := make([]uint32, 0, len(zoneSet))
	for z := range zoneSet {
		zones = append(zones, z)
	}
	activeZonesMu.Lock()
	activeZones = zones
	activeZonesMu.Unlock()
}

// ---------------------------------------------------------------------------
// StartLoadReporter — list-watch pattern (like K8s informer)
// ---------------------------------------------------------------------------

// StartLoadReporter uses etcd Watch to reactively discover scene node changes,
// with a periodic ticker for load-score refresh and grace-period checks.
// On Watch error or channel close, it re-lists and re-watches automatically.
func StartLoadReporter(ctx context.Context, svcCtx *svc.ServiceContext) {
	for {
		rev, err := fullSync(ctx, svcCtx)
		if err != nil {
			logx.Errorf("[LoadReporter] full sync failed: %v, retrying in 3s", err)
			select {
			case <-ctx.Done():
				return
			case <-time.After(3 * time.Second):
				continue
			}
		}

		watchAndRefresh(ctx, svcCtx, rev)

		if ctx.Err() != nil {
			return
		}
		logx.Info("[LoadReporter] watch interrupted, re-syncing")
	}
}

// fullSync does a complete etcd Get to build baseline state, updates Redis,
// and ensures main scenes for all discovered zones. Returns the etcd revision.
func fullSync(ctx context.Context, svcCtx *svc.ServiceContext) (int64, error) {
	resp, err := svcCtx.Etcd.Get(ctx, sceneNodePrefix, clientv3.WithPrefix())
	if err != nil {
		return 0, fmt.Errorf("etcd get %s: %w", sceneNodePrefix, err)
	}

	seenByZone := make(map[uint32]map[string]struct{})

	knownNodesMu.Lock()
	knownNodes = make(map[string]nodeEntry, len(resp.Kvs))
	for _, kv := range resp.Kvs {
		key := string(kv.Key)
		entry, ok := parseNodeEntry(kv.Value)
		if !ok {
			continue
		}
		knownNodes[key] = entry

		zoneId := entry.reg.ZoneId
		if seenByZone[zoneId] == nil {
			seenByZone[zoneId] = make(map[string]struct{})
		}
		seenByZone[zoneId][entry.nodeID] = struct{}{}

		updateNodeLoad(svcCtx, entry)
	}
	knownNodesMu.Unlock()

	// Update activeZones.
	zones := make([]uint32, 0, len(seenByZone))
	for z := range seenByZone {
		zones = append(zones, z)
	}
	activeZonesMu.Lock()
	activeZones = zones
	activeZonesMu.Unlock()

	// Clear pending removals — full sync rebuilds truth.
	pendingRemovalsMu.Lock()
	pendingRemovals = make(map[string]pendingRemoval)
	pendingRemovalsMu.Unlock()

	// Clean stale Redis entries not in current etcd snapshot.
	for _, zoneId := range zones {
		seen := seenByZone[zoneId]
		loadKey := nodeLoadKey(zoneId)
		pairs, err := svcCtx.Redis.ZrangeWithScores(loadKey, 0, -1)
		if err != nil {
			continue
		}
		for _, p := range pairs {
			if _, ok := seen[p.Key]; !ok {
				svcCtx.Redis.Zrem(loadKey, p.Key)
				RemoveNodeConn(p.Key)
			}
		}
	}

	// Ensure world scenes for all zones (handles SceneManager restart).
	if wids := worldConfIds(); len(wids) > 0 {
		for _, z := range zones {
			initWorldScenesForZone(ctx, svcCtx, z, wids)
		}
	}

	logx.Infof("[LoadReporter] full sync: %d nodes, %d zones, rev=%d",
		len(resp.Kvs), len(zones), resp.Header.Revision)
	return resp.Header.Revision, nil
}

// watchAndRefresh starts an etcd Watch from the given revision and a periodic
// ticker for load-score refresh and grace-period expiry checks.
func watchAndRefresh(ctx context.Context, svcCtx *svc.ServiceContext, rev int64) {
	watchCh := svcCtx.Etcd.Watch(ctx, sceneNodePrefix,
		clientv3.WithPrefix(),
		clientv3.WithRev(rev+1),
		clientv3.WithPrevKV(),
	)

	ticker := time.NewTicker(LoadReportInterval)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case watchResp, ok := <-watchCh:
			if !ok {
				logx.Info("[LoadReporter] watch channel closed")
				return
			}
			if watchResp.Err() != nil {
				logx.Errorf("[LoadReporter] watch error: %v", watchResp.Err())
				return
			}
			for _, ev := range watchResp.Events {
				handleWatchEvent(ctx, svcCtx, ev)
			}
		case <-ticker.C:
			refreshLoadScores(svcCtx)
			checkGracePeriodExpirations(svcCtx)
		}
	}
}

// handleWatchEvent processes a single etcd PUT or DELETE event.
func handleWatchEvent(ctx context.Context, svcCtx *svc.ServiceContext, ev *clientv3.Event) {
	key := string(ev.Kv.Key)

	switch ev.Type {
	case clientv3.EventTypePut:
		entry, ok := parseNodeEntry(ev.Kv.Value)
		if !ok {
			return
		}

		knownNodesMu.Lock()
		_, existed := knownNodes[key]
		knownNodes[key] = entry
		knownNodesMu.Unlock()

		// Node (re-)appeared: cancel pending removal.
		pendingRemovalsMu.Lock()
		delete(pendingRemovals, entry.nodeID)
		pendingRemovalsMu.Unlock()

		// Clear stale gRPC connection: endpoint may have changed after restart.
		RemoveNodeConn(entry.nodeID)

		updateNodeLoad(svcCtx, entry)
		rebuildActiveZones()

		if wids := worldConfIds(); !existed && len(wids) > 0 {
			logx.Infof("[LoadReporter] Zone %d: node %s appeared (watch PUT)", entry.reg.ZoneId, entry.nodeID)
			initWorldScenesForZone(ctx, svcCtx, entry.reg.ZoneId, wids)
		}

	case clientv3.EventTypeDelete:
		knownNodesMu.Lock()
		entry, existed := knownNodes[key]
		if existed {
			delete(knownNodes, key)
		}
		knownNodesMu.Unlock()

		if !existed {
			return
		}

		rebuildActiveZones()

		graceDuration := time.Duration(svcCtx.Config.NodeRemovalGraceSeconds) * time.Second
		if graceDuration <= 0 {
			removeNodeFromRedis(svcCtx, entry)
			return
		}

		pendingRemovalsMu.Lock()
		pendingRemovals[entry.nodeID] = pendingRemoval{since: time.Now(), entry: entry}
		pendingRemovalsMu.Unlock()

		logx.Infof("[LoadReporter] node %s deleted (zone %d), grace period started (%ds)",
			entry.nodeID, entry.reg.ZoneId, svcCtx.Config.NodeRemovalGraceSeconds)
	}
}

// refreshLoadScores updates Redis load scores for all currently known nodes.
func refreshLoadScores(svcCtx *svc.ServiceContext) {
	knownNodesMu.RLock()
	defer knownNodesMu.RUnlock()
	for _, entry := range knownNodes {
		updateNodeLoad(svcCtx, entry)
	}
}

// checkGracePeriodExpirations removes nodes whose grace period has expired.
func checkGracePeriodExpirations(svcCtx *svc.ServiceContext) {
	graceDuration := time.Duration(svcCtx.Config.NodeRemovalGraceSeconds) * time.Second
	if graceDuration <= 0 {
		return
	}

	now := time.Now()
	pendingRemovalsMu.Lock()
	defer pendingRemovalsMu.Unlock()
	for nodeID, pr := range pendingRemovals {
		if now.Sub(pr.since) >= graceDuration {
			logx.Infof("[LoadReporter] node %s grace period expired (zone %d), removing",
				nodeID, pr.entry.reg.ZoneId)
			removeNodeFromRedis(svcCtx, pr.entry)
			delete(pendingRemovals, nodeID)
		}
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

// IsNodeAlive checks whether a node is present in the zone's Redis load sorted set.
func IsNodeAlive(svcCtx *svc.ServiceContext, zoneId uint32, nodeId string) bool {
	_, err := svcCtx.Redis.Zscore(nodeLoadKey(zoneId), nodeId)
	return err == nil
}
