package logic

import (
	"context"
	"time"

	"scene_manager/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
)

const (
	NodeLoadKey = "scene_nodes:load"
)

// StartLoadReporter starts a background task to report this node's load to Redis
func StartLoadReporter(ctx context.Context, svcCtx *svc.ServiceContext) {
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	// Initial report
	reportLoad(ctx, svcCtx)

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			reportLoad(ctx, svcCtx)
		}
	}
}

func reportLoad(ctx context.Context, svcCtx *svc.ServiceContext) {
	// Calculating Load:
	// 1. Get current active connections/players (metrics)
	// 2. Get system load (CPU/Memory)
	// Since we don't have easy access to these in a pure logic package without passing dependencies,
	// we will rely on a heuristic or placeholder.
	// For a real implementation, you should inject a metrics provider into ServiceContext.
	
	// Example: Use a random load for testing load balancing, or 0 if idle.
	// In production, replace 0 with `len(connectedPlayers)` + `cpuUsage * 10`
	currentLoad := 0 
	
	// Update Redis ZSet with score = load
	_, err := svcCtx.Redis.Zadd(NodeLoadKey, int64(currentLoad), svcCtx.Config.NodeID)
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
