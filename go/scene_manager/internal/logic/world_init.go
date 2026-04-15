package logic

import (
	"context"
	"fmt"
	"hash/fnv"
	"math"
	"sort"
	"strconv"

	"scene_manager/internal/svc"

	"shared/generated/table"

	"github.com/zeromicro/go-zero/core/logx"
)

// Redis keys for world scene management.
const (
	// SET per (zone, confId): holds sceneId strings for each channel.
	WorldChannelsKeyFmt = "world_channels:zone:%d:%d"
)

func worldChannelsKey(zoneID uint32, confId uint64) string {
	return fmt.Sprintf(WorldChannelsKeyFmt, zoneID, confId)
}

// initWorldScenesForZone ensures world scenes (with N channels each) exist for a single zone.
// Called by fullSync (on startup / re-sync) and handleWatchEvent (on node PUT).
// Two-layer idempotency:
//   - Redis layer: only creates missing channels (if existing < ChannelCount).
//   - C++ layer: CreateScene is idempotent by scene_id (returns existing entity).
//
// Always sends CreateScene RPC for all channels to handle C++ node restarts.
func initWorldScenesForZone(ctx context.Context, svcCtx *svc.ServiceContext, zoneId uint32, confIds []uint64) {
	nodes := getNodesForZone(svcCtx, zoneId)
	if len(nodes) == 0 {
		logx.Errorf("[World] No nodes available for zone %d, skipping", zoneId)
		return
	}

	channelCount := svcCtx.Config.WorldChannelCount
	if channelCount < 1 {
		channelCount = 1
	}

	logx.Infof("[World] Zone %d: ensuring %d world scenes x %d channels across %d nodes",
		zoneId, len(confIds), channelCount, len(nodes))

	created, ensured := 0, 0

	for _, confId := range confIds {
		channelSetKey := worldChannelsKey(zoneId, confId)

		// Get existing channels for this confId.
		existingMembers, _ := svcCtx.Redis.Smembers(channelSetKey)
		existingCount := len(existingMembers)

		// Create missing channels.
		for i := existingCount; i < channelCount; i++ {
			sceneId := svcCtx.SceneIDGen.Generate()

			// Vary hash input per channel so channels may land on different nodes.
			targetNode := assignNodeByHash(confId*1000+uint64(i), nodes)

			sceneNodeKey := fmt.Sprintf("scene:%d:node", sceneId)
			if err := svcCtx.Redis.Set(sceneNodeKey, targetNode); err != nil {
				logx.Errorf("[World] Failed to store scene mapping for scene %d: %v", sceneId, err)
				continue
			}

			if _, err := svcCtx.Redis.Sadd(channelSetKey, fmt.Sprintf("%d", sceneId)); err != nil {
				logx.Errorf("[World] Failed to add channel to set for conf %d: %v", confId, err)
				continue
			}

			// Initialize player count.
			svcCtx.Redis.Set(fmt.Sprintf(InstancePlayerCountKey, sceneId), "0")

			sceneCountKey := fmt.Sprintf(NodeSceneCountKey, targetNode)
			svcCtx.Redis.Incr(sceneCountKey)

			created++
			logx.Infof("[World] Allocated channel %d (scene=%d, conf=%d) on node %s in zone %d",
				i, sceneId, confId, targetNode, zoneId)
		}

		// Always send CreateScene RPC for all channels — C++ deduplicates by scene_id.
		allMembers, _ := svcCtx.Redis.Smembers(channelSetKey)
		for _, sceneIdStr := range allMembers {
			sceneId, _ := strconv.ParseUint(sceneIdStr, 10, 64)
			nodeKey := fmt.Sprintf("scene:%d:node", sceneId)
			targetNode, _ := svcCtx.Redis.Get(nodeKey)
			if targetNode == "" {
				logx.Errorf("[World] Missing node for scene %d, skipping RPC", sceneId)
				continue
			}
			if _, err := RequestNodeCreateScene(ctx, svcCtx, targetNode, uint32(confId), sceneId); err != nil {
				logx.Errorf("[World] Failed to call CreateScene for conf %d scene %d: %v", confId, sceneId, err)
			}
			ensured++
		}
	}

	logx.Infof("[World] Zone %d done: created=%d channels, ensured=%d RPCs", zoneId, created, ensured)
}

// GetBestWorldChannel returns the (sceneId, nodeId) of the least-loaded channel
// for the given world confId in the zone. Returns (0, "", nil) if no channels exist.
func GetBestWorldChannel(ctx context.Context, svcCtx *svc.ServiceContext, confId uint64, zoneId uint32) (uint64, string, error) {
	channelSetKey := worldChannelsKey(zoneId, confId)
	members, err := svcCtx.Redis.Smembers(channelSetKey)
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

// GetAllWorldChannels returns all channel sceneIds for a confId in a zone.
func GetAllWorldChannels(ctx context.Context, svcCtx *svc.ServiceContext, confId uint64, zoneId uint32) ([]uint64, error) {
	channelSetKey := worldChannelsKey(zoneId, confId)
	members, err := svcCtx.Redis.Smembers(channelSetKey)
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

// worldConfIds extracts base scene config IDs from the loaded World table.
func worldConfIds() []uint64 {
	rows := table.WorldTableManagerInstance.FindAll()
	ids := make([]uint64, 0, len(rows))
	for _, row := range rows {
		ids = append(ids, uint64(row.SceneId))
	}
	return ids
}

// IsWorldConf checks if a scene_conf_id is a world scene.
func IsWorldConf(confId uint64) bool {
	for _, row := range table.WorldTableManagerInstance.FindAll() {
		if uint64(row.SceneId) == confId {
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
