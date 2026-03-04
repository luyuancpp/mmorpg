package logic

import (
	"context"

	"scene_manager/internal/svc"
	"scene_manager/scene_manager"

	"github.com/zeromicro/go-zero/core/logx"
)

type EnterSceneByCentreLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewEnterSceneByCentreLogic(ctx context.Context, svcCtx *svc.ServiceContext) *EnterSceneByCentreLogic {
	return &EnterSceneByCentreLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

// Centre 请求某玩家进入场景，SceneManager 负责路由到具体 Scene 节点
func (l *EnterSceneByCentreLogic) EnterSceneByCentre(in *scene_manager.EnterSceneByCentreRequest) (*scene_manager.EnterSceneByCentreResponse, error) {
	// todo: add your logic here and delete this line

	return &scene_manager.EnterSceneByCentreResponse{}, nil
}
