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
	// 1. IDEMPOTENCY: Check if player is actually in this scene
	currentLoc, err := GetPlayerLocation(l.ctx, l.svcCtx, in.PlayerId)
	if err == nil && currentLoc != nil {
		if currentLoc.SceneId == in.SceneId && currentLoc.NodeId == l.svcCtx.Config.NodeID {
			// Log departure
			l.Logger.Infof("Player %d is leaving scene %d on node %s", in.PlayerId, in.SceneId, l.svcCtx.Config.NodeID)
		} else {
			// Already gone or moved
			l.Logger.Infof("Player %d requested leave scene %d but is at %d (node %s)", in.PlayerId, in.SceneId, currentLoc.SceneId, currentLoc.NodeId)
		}
	} else {
		// No location info
		l.Logger.Infof("Player %d requested leave scene %d but has no location info", in.PlayerId, in.SceneId)
	}

	return &base.Empty{}, nil
}
