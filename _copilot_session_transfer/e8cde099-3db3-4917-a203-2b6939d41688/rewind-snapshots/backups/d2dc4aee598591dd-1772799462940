package logic

import (
	"context"
	"fmt"

	"scene_manager/internal/svc"
	"scene_manager/scene_manager"

	"github.com/zeromicro/go-zero/core/logx"
)

type CreateSceneLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewCreateSceneLogic(ctx context.Context, svcCtx *svc.ServiceContext) *CreateSceneLogic {
	return &CreateSceneLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

// 在指定节点创建一个场景（由 Scene 节点实现具体创建）
func (l *CreateSceneLogic) CreateScene(in *scene_manager.CreateSceneRequest) (*scene_manager.CreateSceneResponse, error) {
	// Generate new Scene ID
	id, err := l.svcCtx.Redis.Incr("scene:id_counter")
	if err != nil {
		l.Logger.Errorf("Failed to generate scene id: %v", err)
		return &scene_manager.CreateSceneResponse{ErrorCode: 1, ErrorMessage: "Internal error"}, nil
	}
	sceneId := uint64(id)

	// Store mapping scene->node
	// We use the configured NodeID
	err = l.svcCtx.Redis.Set(fmt.Sprintf("scene:%d:node", sceneId), l.svcCtx.Config.NodeID)
	if err != nil {
		l.Logger.Errorf("Failed to register scene: %v", err)
		return &scene_manager.CreateSceneResponse{ErrorCode: 1, ErrorMessage: "Redis error"}, nil
	}

	l.Logger.Infof("Created scene %d on node %s", sceneId, l.svcCtx.Config.NodeID)
	return &scene_manager.CreateSceneResponse{SceneId: sceneId, ErrorCode: 0}, nil
}
