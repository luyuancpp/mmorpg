package logic

import (
	"context"
	"fmt"
	"hash/fnv"
	"sort"

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

// initMainScenesForZone ensures main scenes exist for a single zone.
// Called by fullSync (on startup / re-sync) and handleWatchEvent (on node PUT).
// Two-layer idempotency:
//   - Redis layer: skips scene ID allocation if confId already has a Redis entry.
//   - C++ layer: CreateScene is idempotent by config_id (returns existing entity).
//
// This means we always send the CreateScene RPC even for existing Redis entries,
// which handles the C++ node restart case (Redis has data, C++ lost ECS entities).
func initMainScenesForZone(ctx context.Context, svcCtx *svc.ServiceContext, zoneId uint32, confIds []uint64) {
	nodes := getNodesForZone(svcCtx, zoneId)
	if len(nodes) == 0 {
		logx.Errorf("[MainScene] No nodes available for zone %d, skipping", zoneId)
		return
	}

	logx.Infof("[MainScene] Zone %d: ensuring %d main world scenes across %d nodes", zoneId, len(confIds), len(nodes))

	hashKey := mainScenesKey(zoneId)
	created, ensured := 0, 0

	for _, confId := range confIds {
		confIdStr := fmt.Sprintf("%d", confId)
		targetNode := assignNodeByHash(confId, nodes)

		// Check if Redis already has a scene ID for this confId.
		existing, _ := svcCtx.Redis.Hget(hashKey, confIdStr)
		if existing == "" {
			// New scene — allocate ID and register in Redis.
			id, err := svcCtx.Redis.Incr("scene:id_counter")
			if err != nil {
				logx.Errorf("[MainScene] Failed to generate scene id for conf %d: %v", confId, err)
				continue
			}
			sceneId := uint64(id)

			sceneNodeKey := fmt.Sprintf("scene:%d:node", sceneId)
			if err := svcCtx.Redis.Set(sceneNodeKey, targetNode); err != nil {
				logx.Errorf("[MainScene] Failed to store scene mapping for scene %d: %v", sceneId, err)
				continue
			}

			if err := svcCtx.Redis.Hset(hashKey, confIdStr, fmt.Sprintf("%d", sceneId)); err != nil {
				logx.Errorf("[MainScene] Failed to store main scene hash for conf %d: %v", confId, err)
				continue
			}

			sceneCountKey := fmt.Sprintf(NodeSceneCountKey, targetNode)
			svcCtx.Redis.Incr(sceneCountKey)

			created++
			logx.Infof("[MainScene] Allocated main scene %d (conf=%d) on node %s in zone %d", sceneId, confId, targetNode, zoneId)
		} else {
			ensured++
		}

		// Always send CreateScene RPC — C++ deduplicates by config_id.
		// This ensures the C++ node has the ECS entity even after a restart.
		if _, err := RequestNodeCreateScene(ctx, svcCtx, targetNode, uint32(confId)); err != nil {
			logx.Errorf("[MainScene] Failed to call CreateScene on node %s for conf %d: %v", targetNode, confId, err)
		}
	}

	logx.Infof("[MainScene] Zone %d done: created=%d, ensured=%d", zoneId, created, ensured)
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
