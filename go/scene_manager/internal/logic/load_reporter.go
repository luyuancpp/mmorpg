package logic

import (
	"context"
	"encoding/json"
	"fmt"
	"strconv"
	"sync"
	"time"

	"scene_manager/internal/constants"
	"scene_manager/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
	clientv3 "go.etcd.io/etcd/client/v3"
)

const (
	NodeLoadKeyFmt     = "scene_nodes:zone:%d:load"
	NodeSceneCountKey  = "node:%s:scene_count"
	NodePlayerCountKey = "node:%s:player_count"
	// NodeSceneNodeTypeKey mirrors the scene_node_type declared by the C++
	// node (0=MainWorld, 1=Instance, 2=MainWorldCross, 3=InstanceCross). The
	// load reporter writes this when a node registers so selection logic can
	// filter by purpose without hitting etcd.
	NodeSceneNodeTypeKey = "node:%s:scene_node_type"
	LoadReportInterval   = 5 * time.Second
	sceneNodePrefix      = "SceneNodeService.rpc/"
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
// The field names match protobuf's MessageToJsonString camelCase encoding of
// proto/common/base/common.proto NodeInfo. `sceneNodeType` is the node's
// declared role (see constants.SceneNodeType*); `nodeType` is the coarse
// service class (SceneNodeService = 0x14).
type sceneNodeRegistration struct {
	NodeId        uint32 `json:"nodeId"`
	NodeType      uint32 `json:"nodeType"`
	SceneNodeType uint32 `json:"sceneNodeType"`
	Endpoint      struct {
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

// knownNodes tracks nodes currently registered in etcd, keyed by etcd key.
var (
	knownNodesMu sync.RWMutex
	knownNodes   = make(map[string]nodeEntry)
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

// updateNodeLoad reads the node's scene_count and player_count from Redis,
// writes the composite load score into the zone sorted set, and mirrors the
// scene_node_type so downstream purpose-based selection doesn't need to hit
// etcd. Score = α·scene_count + β·player_count; see Config weights.
func updateNodeLoad(svcCtx *svc.ServiceContext, entry nodeEntry) {
	sceneCount := readInt64(svcCtx, fmt.Sprintf(NodeSceneCountKey, entry.nodeID))
	playerCount := readInt64(svcCtx, fmt.Sprintf(NodePlayerCountKey, entry.nodeID))
	score := computeNodeLoadScore(svcCtx, sceneCount, playerCount)

	loadKey := nodeLoadKey(entry.reg.ZoneId)
	if _, err := svcCtx.Redis.Zadd(loadKey, int64(score), entry.nodeID); err != nil {
		logx.Errorf("[LoadReporter] failed to update load for node %s (zone %d): %v", entry.nodeID, entry.reg.ZoneId, err)
	}

	// Mirror scene_node_type into Redis so getNodesForPurpose can filter
	// without touching etcd on every CreateScene request.
	if err := svcCtx.Redis.Set(fmt.Sprintf(NodeSceneNodeTypeKey, entry.nodeID),
		strconv.FormatUint(uint64(entry.reg.SceneNodeType), 10)); err != nil {
		logx.Errorf("[LoadReporter] failed to mirror scene_node_type for node %s: %v", entry.nodeID, err)
	}
}

// computeNodeLoadScore combines scene_count and player_count using the
// configured weights. Weights default to 1.0 / 0.01 so scene_count dominates
// but many concurrent players still push a node down the preference list.
// Returns a float64 kept as int64 in the Redis sorted set for go-zero's
// typed API; the truncation rounds toward zero which is fine for load
// ordering (ties broken lexically by Redis).
func computeNodeLoadScore(svcCtx *svc.ServiceContext, sceneCount, playerCount int64) float64 {
	wScene := svcCtx.Config.NodeLoadWeightSceneCount
	wPlayer := svcCtx.Config.NodeLoadWeightPlayerCount
	if wScene <= 0 {
		wScene = 1.0
	}
	if wPlayer < 0 {
		wPlayer = 0
	}
	return wScene*float64(sceneCount) + wPlayer*float64(playerCount)
}

// readInt64 reads a string-encoded int from Redis, returning 0 on any error.
func readInt64(svcCtx *svc.ServiceContext, key string) int64 {
	s, err := svcCtx.Redis.Get(key)
	if err != nil || s == "" {
		return 0
	}
	v, _ := strconv.ParseInt(s, 10, 64)
	return v
}

// removeNodeFromRedis cleans up a node's load-set entry, cached connection,
// and mirrored scene_node_type. scene_count / player_count counters are
// intentionally left alone: they may still be referenced by in-flight
// instance destroys; the next node that reuses the id will overwrite them.
func removeNodeFromRedis(svcCtx *svc.ServiceContext, entry nodeEntry) {
	loadKey := nodeLoadKey(entry.reg.ZoneId)
	svcCtx.Redis.Zrem(loadKey, entry.nodeID)
	svcCtx.Redis.Del(fmt.Sprintf(NodeSceneNodeTypeKey, entry.nodeID))
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
				svcCtx.Redis.Del(fmt.Sprintf(NodeSceneNodeTypeKey, p.Key))
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
		removeNodeFromRedis(svcCtx, entry)
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


// GetBestNode selects the instance-hosting node with the lowest load from
// the given zone. Kept as the default selector for historical callers that
// request instance-style nodes (mirror fallbacks, ad-hoc instances).
//
// See GetBestNodeForPurpose for the purpose-aware entry point.
func GetBestNode(ctx context.Context, svcCtx *svc.ServiceContext, zoneId uint32) (string, error) {
	return GetBestNodeForPurpose(ctx, svcCtx, zoneId, constants.NodePurposeInstance)
}

// IsNodeAlive checks whether a node is present in the zone's Redis load sorted set.
func IsNodeAlive(svcCtx *svc.ServiceContext, zoneId uint32, nodeId string) bool {
	_, err := svcCtx.Redis.Zscore(nodeLoadKey(zoneId), nodeId)
	return err == nil
}
