package logic

import (
	"context"

	"scene_manager/internal/svc"
	"scene_manager/scene_manager"

	"github.com/zeromicro/go-zero/core/logx"
)

type LeaveSceneByCentreLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewLeaveSceneByCentreLogic(ctx context.Context, svcCtx *svc.ServiceContext) *LeaveSceneByCentreLogic {
	return &LeaveSceneByCentreLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

// Centre 请求玩家离开场景（或切换场景前的离开）
func (l *LeaveSceneByCentreLogic) LeaveSceneByCentre(in *scene_manager.LeaveSceneByCentreRequest) (*scene_manager.Empty, error) {
	// todo: add your logic here and delete this line

	return &scene_manager.Empty{}, nil
}
