package logic

import (
	"context"
	"fmt"
	"hash/fnv"
	"sort"
	"time"

	"scene_manager/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
)

// Redis keys for main scene management.
const (
	// Hash: confId -> sceneId. Stores all main world scenes for this zone.
	MainScenesHashKey = "main_scenes:zone:%d"
)

func mainScenesKey(zoneID uint32) string {
	return fmt.Sprintf(MainScenesHashKey, zoneID)
}

// InitMainScenes creates persistent main-world scenes at startup.
// Discovers zones from etcd, then creates main scenes for each zone.
// Each scene_conf_id in config is assigned to a node via consistent hashing
// and created exactly once (idempotent — skips if already in Redis).
func InitMainScenes(ctx context.Context, svcCtx *svc.ServiceContext) {
	confIds := svcCtx.Config.MainSceneConfIds
	if len(confIds) == 0 {
		logx.Info("[MainScene] No MainSceneConfIds configured, skipping main scene init")
		return
	}

	// Wait for at least one zone to be discovered by LoadReporter.
	zones := waitForZones(ctx, svcCtx)
	if len(zones) == 0 {
		logx.Error("[MainScene] No zones discovered after waiting, cannot init main scenes")
		return
	}

	for _, zoneId := range zones {
		initMainScenesForZone(ctx, svcCtx, zoneId, confIds)
	}
}

// initMainScenesForZone creates main scenes for a single zone.
func initMainScenesForZone(ctx context.Context, svcCtx *svc.ServiceContext, zoneId uint32, confIds []uint64) {
	nodes := getNodesForZone(svcCtx, zoneId)
	if len(nodes) == 0 {
		logx.Errorf("[MainScene] No nodes available for zone %d, skipping", zoneId)
		return
	}

	logx.Infof("[MainScene] Zone %d: initializing %d main world scenes across %d nodes", zoneId, len(confIds), len(nodes))

	hashKey := mainScenesKey(zoneId)
	created, skipped := 0, 0

	for _, confId := range confIds {
		confIdStr := fmt.Sprintf("%d", confId)

		// Idempotency: check if this confId already has a scene.
		existing, _ := svcCtx.Redis.Hget(hashKey, confIdStr)
		if existing != "" {
			skipped++
			continue
		}

		// Assign to node via consistent hash.
		targetNode := assignNodeByHash(confId, nodes)

		// Allocate scene ID.
		id, err := svcCtx.Redis.Incr("scene:id_counter")
		if err != nil {
			logx.Errorf("[MainScene] Failed to generate scene id for conf %d: %v", confId, err)
			continue
		}
		sceneId := uint64(id)

		// Store scene -> node mapping.
		sceneNodeKey := fmt.Sprintf("scene:%d:node", sceneId)
		if err := svcCtx.Redis.Set(sceneNodeKey, targetNode); err != nil {
			logx.Errorf("[MainScene] Failed to store scene mapping for scene %d: %v", sceneId, err)
			continue
		}

		// Store confId -> sceneId in main scenes hash.
		if err := svcCtx.Redis.Hset(hashKey, confIdStr, fmt.Sprintf("%d", sceneId)); err != nil {
			logx.Errorf("[MainScene] Failed to store main scene hash for conf %d: %v", confId, err)
			continue
		}

		// Increment node scene count for load tracking.
		sceneCountKey := fmt.Sprintf(NodeSceneCountKey, targetNode)
		svcCtx.Redis.Incr(sceneCountKey)

		// Notify the C++ scene node to create the ECS entity.
		if _, err := RequestNodeCreateScene(ctx, svcCtx, targetNode, uint32(confId)); err != nil {
			logx.Errorf("[MainScene] Failed to call CreateScene on node %s for scene %d: %v", targetNode, sceneId, err)
		}

		created++
		logx.Infof("[MainScene] Created main scene %d (conf=%d) on node %s in zone %d", sceneId, confId, targetNode, zoneId)
	}

	logx.Infof("[MainScene] Zone %d init done: created=%d, skipped(already exist)=%d", zoneId, created, skipped)
}

// GetMainSceneId looks up the scene ID for a main-world config in the given zone.
// Returns 0 if not found.
func GetMainSceneId(ctx context.Context, svcCtx *svc.ServiceContext, confId uint64, zoneId uint32) (uint64, error) {
	hashKey := mainScenesKey(zoneId)
	val, err := svcCtx.Redis.Hget(hashKey, fmt.Sprintf("%d", confId))
	if err != nil || val == "" {
		return 0, err
	}
	var sceneId uint64
	fmt.Sscanf(val, "%d", &sceneId)
	return sceneId, nil
}

// IsMainSceneConf checks if a scene_conf_id is a main world scene.
func IsMainSceneConf(svcCtx *svc.ServiceContext, confId uint64) bool {
	for _, id := range svcCtx.Config.MainSceneConfIds {
		if id == confId {
			return true
		}
	}
	return false
}

// waitForZones polls GetKnownZones until at least one zone is discovered
// (populated by LoadReporter from etcd) or the context is cancelled.
func waitForZones(ctx context.Context, svcCtx *svc.ServiceContext) []uint32 {
	for attempt := 0; attempt < 60; attempt++ {
		select {
		case <-ctx.Done():
			return nil
		default:
		}

		// Force a sync so knownZones is populated.
		syncSceneNodes(ctx, svcCtx)

		zones := GetKnownZones()
		if len(zones) > 0 {
			return zones
		}

		logx.Infof("[MainScene] Waiting for scene nodes... attempt %d", attempt+1)
		time.Sleep(2 * time.Second)
	}
	return nil
}

// getNodesForZone returns sorted node IDs for a given zone from the Redis load set.
func getNodesForZone(svcCtx *svc.ServiceContext, zoneId uint32) []string {
	loadKey := nodeLoadKey(zoneId)
	pairs, err := svcCtx.Redis.ZrangeWithScores(loadKey, 0, -1)
	if err != nil || len(pairs) == 0 {
		return nil
	}
	nodes := make([]string, 0, len(pairs))
	for _, p := range pairs {
		nodes = append(nodes, p.Key)
	}
	sort.Strings(nodes)
	return nodes
}

// assignNodeByHash uses FNV-1a to consistently map a confId to a node.
// Deterministic: same confId + same node list -> same node.
func assignNodeByHash(confId uint64, sortedNodes []string) string {
	h := fnv.New32a()
	h.Write([]byte(fmt.Sprintf("%d", confId)))
	idx := int(h.Sum32()) % len(sortedNodes)
	return sortedNodes[idx]
}
