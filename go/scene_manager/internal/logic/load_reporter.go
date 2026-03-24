package logic

import (
	"context"
	"fmt"
	"time"

	"scene_manager/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
)

const (
	NodeLoadKey        = "scene_nodes:load"
	NodeSceneCountKey  = "node:%s:scene_count"
	LoadReportInterval = 5 * time.Second
)

// StartLoadReporter starts a background task to report this node's load to Redis
func StartLoadReporter(ctx context.Context, svcCtx *svc.ServiceContext) {
	ticker := time.NewTicker(LoadReportInterval)
	defer ticker.Stop()

	// Initial report
	reportLoad(ctx, svcCtx)

	for {
		select {
		case <-ctx.Done():
			// Remove node from the load set on shutdown
			if _, err := svcCtx.Redis.Zrem(NodeLoadKey, svcCtx.Config.NodeID); err != nil {
				logx.Errorf("Failed to remove node %s from load set on shutdown: %v", svcCtx.Config.NodeID, err)
			}
			return
		case <-ticker.C:
			reportLoad(ctx, svcCtx)
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
	_, err = svcCtx.Redis.Zadd(NodeLoadKey, currentLoad, svcCtx.Config.NodeID)
	if err != nil {
		logx.Errorf("Failed to update node load for %s: %v", svcCtx.Config.NodeID, err)
	}
}

// GetBestNode selects the node with the lowest load from Redis
func GetBestNode(ctx context.Context, svcCtx *svc.ServiceContext) (string, error) {
	// Get the node with the lowest score (load)
	// ZRANGE key 0 0 WITHSCORES
	pairs, err := svcCtx.Redis.ZrangeWithScores(NodeLoadKey, 0, 0)
	if err != nil {
		return "", err
	}

	if len(pairs) == 0 {
		// If no nodes are reporting, fallback to self
		return svcCtx.Config.NodeID, nil
	}

	bestNode := pairs[0].Key
	return bestNode, nil
}
