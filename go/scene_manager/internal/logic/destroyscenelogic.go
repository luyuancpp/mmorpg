package logic

import (
	"context"

	"scene_manager/internal/svc"
	"scene_manager/scene_manager"

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

// 销毁场景
func (l *DestroySceneLogic) DestroyScene(in *scene_manager.DestroySceneRequest) (*scene_manager.Empty, error) {
	// todo: add your logic here and delete this line

	return &scene_manager.Empty{}, nil
}
