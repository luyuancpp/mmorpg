package logic

import (
	"context"

	"gateway/internal/svc"
	"gateway/internal/types"
)

type ServerListLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
}

func NewServerListLogic(ctx context.Context, svcCtx *svc.ServiceContext) *ServerListLogic {
	return &ServerListLogic{ctx: ctx, svcCtx: svcCtx}
}

func (l *ServerListLogic) ServerList() (*types.ServerListResponse, error) {
	nodes, err := l.svcCtx.GateWatcher.FetchAllNodes()
	if err != nil {
		return nil, err
	}

	seen := make(map[uint32]bool)
	var zones []types.ZoneInfo
	for _, n := range nodes {
		if n.Endpoint == nil || seen[n.ZoneId] {
			continue
		}
		seen[n.ZoneId] = true
		zones = append(zones, types.ZoneInfo{
			ZoneID: n.ZoneId,
			Name:   zoneDisplayName(n.ZoneId),
			Status: "normal",
		})
	}
	if len(zones) > 0 {
		zones[0].Recommended = true
	}
	return &types.ServerListResponse{Zones: zones}, nil
}

func zoneDisplayName(zoneId uint32) string {
	names := map[uint32]string{
		0: "default",
		1: "zone-1", 2: "zone-2", 3: "zone-3", 4: "zone-4", 5: "zone-5",
		6: "zone-6", 7: "zone-7", 8: "zone-8", 9: "zone-9", 10: "zone-10",
	}
	if name, ok := names[zoneId]; ok {
		return name
	}
	return "unknown"
}
