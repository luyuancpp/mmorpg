package scenemanagerlogic

import (
	"context"

	"scene_manager/internal/logic"
	"scene_manager/internal/svc"
	"scene_manager/proto/common/base"
	"scene_manager/scene_manager"

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
func (l *LeaveSceneLogic) LeaveScene(in *scene_manager.LeaveSceneRequest) (*base.Empty, error) {
	return logic.NewLeaveSceneLogic(l.ctx, l.svcCtx).LeaveScene(in)
}
