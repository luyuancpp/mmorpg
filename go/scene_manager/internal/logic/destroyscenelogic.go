package logic

import (
	"context"
	"fmt"

	"proto/common/base"
	"proto/scene_manager"
	"scene_manager/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
)

type DestroySceneLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewDestroySceneLogic(ctx context.Context, svcCtx *svc.ServiceContext) *DestroySceneLogic {
	return &DestroySceneLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *DestroySceneLogic) DestroyScene(in *scene_manager.DestroySceneRequest) (*base.Empty, error) {
	key := fmt.Sprintf("scene:%d:node", in.SceneId)

	// Get the node this scene belongs to before deleting (for load tracking + RPC)
	nodeId, _ := l.svcCtx.Redis.Get(key)

	// Notify C++ node to destroy the ECS scene entity (skip if node is already dead).
	if nodeId != "" && IsNodeAlive(l.svcCtx, in.ZoneId, nodeId) {
		if err := RequestNodeDestroyScene(l.ctx, l.svcCtx, nodeId, in.SceneId); err != nil {
			l.Logger.Errorf("Failed to call DestroyScene on node %s for scene %d: %v", nodeId, in.SceneId, err)
		}
	}

	_, err := l.svcCtx.Redis.Del(key)
	if err != nil {
		l.Logger.Errorf("Failed to destroy scene %d: %v", in.SceneId, err)
	} else {
		l.Logger.Infof("Destroyed scene %d", in.SceneId)
		// Decrement scene count on the node
		if nodeId != "" {
			sceneCountKey := fmt.Sprintf(NodeSceneCountKey, nodeId)
			if _, countErr := l.svcCtx.Redis.Incrby(sceneCountKey, -1); countErr != nil {
				l.Logger.Errorf("Failed to decrement scene count for node %s: %v", nodeId, countErr)
			}
		}
	}

	// Clean up instance tracking (no-op if this was a main scene).
	sceneIdStr := fmt.Sprintf("%d", in.SceneId)
	instKey := activeInstancesKey(in.ZoneId)
	l.svcCtx.Redis.Zrem(instKey, sceneIdStr)
	l.svcCtx.Redis.Del(fmt.Sprintf(InstancePlayerCountKey, in.SceneId))

	return &base.Empty{}, nil
}
