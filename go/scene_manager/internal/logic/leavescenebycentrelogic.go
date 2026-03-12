package logic

import (
	"context"

	"scene_manager/base"
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
func (l *LeaveSceneByCentreLogic) LeaveSceneByCentre(in *scene_manager.LeaveSceneByCentreRequest) (*base.Empty, error) {
	// 1. Check if player is actually in this scene (idempotency)
	currentLoc, err := GetPlayerLocation(l.ctx, l.svcCtx, in.PlayerId)
	if err != nil || currentLoc == nil {
		l.Logger.Infof("Player %d requested leave scene %d but has no location info", in.PlayerId, in.SceneId)
		return &base.Empty{}, nil
	}

	if currentLoc.SceneId != in.SceneId {
		l.Logger.Infof("Player %d requested leave scene %d but is in scene %d, ignoring",
			in.PlayerId, in.SceneId, currentLoc.SceneId)
		return &base.Empty{}, nil
	}

	// 2. Delete player location from Redis
	if err := DeletePlayerLocation(l.ctx, l.svcCtx, in.PlayerId); err != nil {
		l.Logger.Errorf("Failed to delete player %d location: %v", in.PlayerId, err)
		return &base.Empty{}, nil
	}

	l.Logger.Infof("Player %d left scene %d on node %s", in.PlayerId, in.SceneId, currentLoc.NodeId)
	return &base.Empty{}, nil
}
