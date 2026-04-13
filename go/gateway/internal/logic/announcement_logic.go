package logic

import (
	"context"

	"gateway/internal/svc"
	"gateway/internal/types"
)

type AnnouncementLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
}

func NewAnnouncementLogic(ctx context.Context, svcCtx *svc.ServiceContext) *AnnouncementLogic {
	return &AnnouncementLogic{ctx: ctx, svcCtx: svcCtx}
}

func (l *AnnouncementLogic) GetAnnouncements() (*types.AnnouncementResponse, error) {
	// TODO: replace with real data source (Redis / DB / config center)
	items := []types.AnnouncementItem{
		{
			ID:      1,
			Title:   "Server Open",
			Content: "Welcome to the server!",
			Type:    "notice",
		},
	}
	return &types.AnnouncementResponse{Items: items}, nil
}
