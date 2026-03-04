package logic

import (
	"context"

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
	// todo: add your logic here and delete this line

	return &scene_manager.CreateSceneResponse{}, nil
}
