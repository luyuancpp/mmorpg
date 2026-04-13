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
// Each scene_conf_id in config is assigned to a node via consistent hashing
// and created exactly once (idempotent — skips if already in Redis).
func InitMainScenes(ctx context.Context, svcCtx *svc.ServiceContext) {
	confIds := svcCtx.Config.MainSceneConfIds
	if len(confIds) == 0 {
		logx.Info("[MainScene] No MainSceneConfIds configured, skipping main scene init")
		return
	}

	// Wait for at least one scene node to be available.
	nodes := waitForSceneNodes(ctx, svcCtx)
	if len(nodes) == 0 {
		logx.Error("[MainScene] No scene nodes available after waiting, cannot init main scenes")
		return
	}

	logx.Infof("[MainScene] Initializing %d main world scenes across %d nodes", len(confIds), len(nodes))

	hashKey := mainScenesKey(svcCtx.Config.ZoneID)
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

		created++
		logx.Infof("[MainScene] Created main scene %d (conf=%d) on node %s", sceneId, confId, targetNode)
	}

	logx.Infof("[MainScene] Init done: created=%d, skipped(already exist)=%d", created, skipped)
}

// GetMainSceneId looks up the scene ID for a main-world config.
// Returns 0 if not found.
func GetMainSceneId(ctx context.Context, svcCtx *svc.ServiceContext, confId uint64) (uint64, error) {
	hashKey := mainScenesKey(svcCtx.Config.ZoneID)
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

// waitForSceneNodes polls etcd until at least one scene node is registered
// or the context is cancelled. Returns the sorted list of node ID strings.
func waitForSceneNodes(ctx context.Context, svcCtx *svc.ServiceContext) []string {
	for attempt := 0; attempt < 60; attempt++ {
		select {
		case <-ctx.Done():
			return nil
		default:
		}

		// Force a sync so the Redis sorted set is populated.
		syncSceneNodes(ctx, svcCtx)

		loadKey := nodeLoadKey(svcCtx.Config.ZoneID)
		pairs, err := svcCtx.Redis.ZrangeWithScores(loadKey, 0, -1)
		if err == nil && len(pairs) > 0 {
			nodes := make([]string, 0, len(pairs))
			for _, p := range pairs {
				nodes = append(nodes, p.Key)
			}
			sort.Strings(nodes)
			return nodes
		}

		logx.Infof("[MainScene] Waiting for scene nodes... attempt %d", attempt+1)
		time.Sleep(2 * time.Second)
	}
	return nil
}

// assignNodeByHash uses FNV-1a to consistently map a confId to a node.
// Deterministic: same confId + same node list -> same node.
func assignNodeByHash(confId uint64, sortedNodes []string) string {
	h := fnv.New32a()
	h.Write([]byte(fmt.Sprintf("%d", confId)))
	idx := int(h.Sum32()) % len(sortedNodes)
	return sortedNodes[idx]
}
