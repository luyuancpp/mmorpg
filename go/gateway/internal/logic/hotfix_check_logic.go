package logic

import (
	"context"

	"gateway/internal/svc"
	"gateway/internal/types"
)

type HotfixCheckLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
}

func NewHotfixCheckLogic(ctx context.Context, svcCtx *svc.ServiceContext) *HotfixCheckLogic {
	return &HotfixCheckLogic{ctx: ctx, svcCtx: svcCtx}
}

func (l *HotfixCheckLogic) HotfixCheck(req *types.HotfixCheckRequest) (*types.HotfixCheckResponse, error) {
	// TODO: replace with real version comparison logic (config center / DB)
	return &types.HotfixCheckResponse{
		NeedUpdate:  false,
		ForceUpdate: false,
		LatestVer:   "1.0.0",
	}, nil
}
