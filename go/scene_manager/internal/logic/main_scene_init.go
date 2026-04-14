package logic

import (
	"context"
	"fmt"
	"hash/fnv"
	"math"
	"sort"
	"strconv"

	"scene_manager/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
)

// Redis keys for main scene management.
const (
	// SET per (zone, confId): holds sceneId strings for each line/channel.
	MainSceneLinesKeyFmt = "main_scene_lines:zone:%d:%d"
)

func mainSceneLinesKey(zoneID uint32, confId uint64) string {
	return fmt.Sprintf(MainSceneLinesKeyFmt, zoneID, confId)
}

// initMainScenesForZone ensures main scenes (with N lines each) exist for a single zone.
// Called by fullSync (on startup / re-sync) and handleWatchEvent (on node PUT).
// Two-layer idempotency:
//   - Redis layer: only creates missing lines (if existing < LineCount).
//   - C++ layer: CreateScene is idempotent by scene_id (returns existing entity).
//
// Always sends CreateScene RPC for all lines to handle C++ node restarts.
func initMainScenesForZone(ctx context.Context, svcCtx *svc.ServiceContext, zoneId uint32, confIds []uint64) {
	nodes := getNodesForZone(svcCtx, zoneId)
	if len(nodes) == 0 {
		logx.Errorf("[MainScene] No nodes available for zone %d, skipping", zoneId)
		return
	}

	lineCount := svcCtx.Config.MainSceneLineCount
	if lineCount < 1 {
		lineCount = 1
	}

	logx.Infof("[MainScene] Zone %d: ensuring %d main world scenes x %d lines across %d nodes",
		zoneId, len(confIds), lineCount, len(nodes))

	created, ensured := 0, 0

	for _, confId := range confIds {
		lineSetKey := mainSceneLinesKey(zoneId, confId)

		// Get existing lines for this confId.
		existingMembers, _ := svcCtx.Redis.Smembers(lineSetKey)
		existingCount := len(existingMembers)

		// Create missing lines.
		for i := existingCount; i < lineCount; i++ {
			id, err := svcCtx.Redis.Incr("scene:id_counter")
			if err != nil {
				logx.Errorf("[MainScene] Failed to generate scene id for conf %d line %d: %v", confId, i, err)
				continue
			}
			sceneId := uint64(id)

			// Vary hash input per line so lines may land on different nodes.
			targetNode := assignNodeByHash(confId*1000+uint64(i), nodes)

			sceneNodeKey := fmt.Sprintf("scene:%d:node", sceneId)
			if err := svcCtx.Redis.Set(sceneNodeKey, targetNode); err != nil {
				logx.Errorf("[MainScene] Failed to store scene mapping for scene %d: %v", sceneId, err)
				continue
			}

			if _, err := svcCtx.Redis.Sadd(lineSetKey, fmt.Sprintf("%d", sceneId)); err != nil {
				logx.Errorf("[MainScene] Failed to add line to set for conf %d: %v", confId, err)
				continue
			}

			// Initialize player count.
			svcCtx.Redis.Set(fmt.Sprintf(InstancePlayerCountKey, sceneId), "0")

			sceneCountKey := fmt.Sprintf(NodeSceneCountKey, targetNode)
			svcCtx.Redis.Incr(sceneCountKey)

			created++
			logx.Infof("[MainScene] Allocated line %d (scene=%d, conf=%d) on node %s in zone %d",
				i, sceneId, confId, targetNode, zoneId)
		}

		// Always send CreateScene RPC for all lines — C++ deduplicates by scene_id.
		allMembers, _ := svcCtx.Redis.Smembers(lineSetKey)
		for _, sceneIdStr := range allMembers {
			sceneId, _ := strconv.ParseUint(sceneIdStr, 10, 64)
			nodeKey := fmt.Sprintf("scene:%d:node", sceneId)
			targetNode, _ := svcCtx.Redis.Get(nodeKey)
			if targetNode == "" {
				logx.Errorf("[MainScene] Missing node for scene %d, skipping RPC", sceneId)
				continue
			}
			if _, err := RequestNodeCreateScene(ctx, svcCtx, targetNode, uint32(confId), sceneId); err != nil {
				logx.Errorf("[MainScene] Failed to call CreateScene for conf %d scene %d: %v", confId, sceneId, err)
			}
			ensured++
		}
	}

	logx.Infof("[MainScene] Zone %d done: created=%d lines, ensured=%d RPCs", zoneId, created, ensured)
}

// GetBestMainSceneLine returns the (sceneId, nodeId) of the least-loaded line
// for the given main-world confId in the zone. Returns (0, "", nil) if no lines exist.
func GetBestMainSceneLine(ctx context.Context, svcCtx *svc.ServiceContext, confId uint64, zoneId uint32) (uint64, string, error) {
	lineSetKey := mainSceneLinesKey(zoneId, confId)
	members, err := svcCtx.Redis.Smembers(lineSetKey)
	if err != nil || len(members) == 0 {
		return 0, "", err
	}

	var bestSceneId uint64
	var bestNodeId string
	bestCount := int64(math.MaxInt64)

	for _, m := range members {
		sceneId, _ := strconv.ParseUint(m, 10, 64)
		if sceneId == 0 {
			continue
		}

		countStr, _ := svcCtx.Redis.Get(fmt.Sprintf(InstancePlayerCountKey, sceneId))
		count, _ := strconv.ParseInt(countStr, 10, 64)

		if count < bestCount {
			bestCount = count
			bestSceneId = sceneId
			nodeKey := fmt.Sprintf("scene:%d:node", sceneId)
			bestNodeId, _ = svcCtx.Redis.Get(nodeKey)
		}
	}

	return bestSceneId, bestNodeId, nil
}

// GetAllMainSceneLines returns all line sceneIds for a confId in a zone.
func GetAllMainSceneLines(ctx context.Context, svcCtx *svc.ServiceContext, confId uint64, zoneId uint32) ([]uint64, error) {
	lineSetKey := mainSceneLinesKey(zoneId, confId)
	members, err := svcCtx.Redis.Smembers(lineSetKey)
	if err != nil {
		return nil, err
	}
	ids := make([]uint64, 0, len(members))
	for _, m := range members {
		id, _ := strconv.ParseUint(m, 10, 64)
		if id > 0 {
			ids = append(ids, id)
		}
	}
	return ids, nil
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

// assignNodeByHash uses FNV-1a to consistently map a key to a node.
// Deterministic: same key + same node list -> same node.
func assignNodeByHash(key uint64, sortedNodes []string) string {
	h := fnv.New32a()
	h.Write([]byte(fmt.Sprintf("%d", key)))
	idx := int(h.Sum32()) % len(sortedNodes)
	return sortedNodes[idx]
}
