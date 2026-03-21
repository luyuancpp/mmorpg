package scenemanagerlogic

import (
	"context"

	"scene_manager/internal/svc"
	"scene_manager/proto/scene_manager/scene_manager"

	"github.com/zeromicro/go-zero/core/logx"
)

type LeaveSceneLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewLeaveSceneLogic(ctx context.Context, svcCtx *svc.ServiceContext) *LeaveSceneLogic {
	return &LeaveSceneLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

// Login/player_locator requests a player to leave a scene
func (l *LeaveSceneLogic) LeaveScene(in *scene_manager.LeaveSceneRequest) (*scene_manager.Empty, error) {
	// todo: add your logic here and delete this line

	return &scene_manager.Empty{}, nil
}
