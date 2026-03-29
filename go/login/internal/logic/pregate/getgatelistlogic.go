package pregate

import (
	"context"
	"sort"

	"login/internal/svc"
	login "proto/login"

	"github.com/zeromicro/go-zero/core/logx"
)

// GetGateList returns all available Gate endpoints sorted by player_count (ascending).
// Clients call this before connecting to any Gate to pick the least-loaded one.
func GetGateList(ctx context.Context, svcCtx *svc.ServiceContext) (*login.GetGateListResponse, error) {
	nodes, err := svcCtx.GateWatcher.FetchAllNodes()
	if err != nil {
		logx.Errorf("GetGateList: failed to fetch gate nodes: %v", err)
		return &login.GetGateListResponse{}, nil
	}

	gates := make([]*login.GateEndpoint, 0, len(nodes))
	for _, n := range nodes {
		if n.Endpoint == nil {
			continue
		}
		gates = append(gates, &login.GateEndpoint{
			NodeId:      n.NodeId,
			Ip:          n.Endpoint.Ip,
			Port:        n.Endpoint.Port,
			PlayerCount: n.PlayerCount,
		})
	}

	// Sort by player_count ascending so client can pick gates[0] for least-loaded
	sort.Slice(gates, func(i, j int) bool {
		return gates[i].PlayerCount < gates[j].PlayerCount
	})

	return &login.GetGateListResponse{Gates: gates}, nil
}
