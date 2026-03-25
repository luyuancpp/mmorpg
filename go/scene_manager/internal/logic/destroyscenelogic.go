package logic

import (
	"context"
	"fmt"

	"scene_manager/internal/svc"
	"proto/common/base"
	"proto/scene_manager"

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

	// Get the node this scene belongs to before deleting (for load tracking)
	nodeId, _ := l.svcCtx.Redis.Get(key)

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
	return &base.Empty{}, nil
}
