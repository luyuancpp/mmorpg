package authority

import (
	"context"
	"database/sql"

	pb "proto/match"
	teampb "proto/team"
)

// TeamUnitHomeZone 返回完整unit的共同原home；混合unit为0，缺原home是损坏。
// home是角色归属，不能用实际战斗机房或队长所在zone替代。
func TeamUnitHomeZone(group []*pb.MatchAuthorityTicketRecord) (uint32, error) {
	if len(group) == 0 {
		return 0, ErrTeamRoute
	}
	var home uint32
	mixed := false
	seen := make(map[uint64]bool, len(group))
	for index, ticket := range group {
		if ticket == nil || ticket.PlayerId == 0 || ticket.ZoneId == 0 || seen[ticket.PlayerId] {
			return 0, ErrTeamRoute
		}
		seen[ticket.PlayerId] = true
		if index == 0 {
			home = ticket.ZoneId
		} else if ticket.ZoneId != home {
			mixed = true
		}
	}
	if mixed {
		return 0, nil
	}
	return home, nil
}

// 完整route/fence/P前缀已经由事务入口锁齐；这里只读复核，不追加任何锁集合。
func (s *Store) requireTeamHomeGroupTx(ctx context.Context, tx *sql.Tx, group []*pb.MatchAuthorityTicketRecord) error {
	home, err := TeamUnitHomeZone(group)
	if err != nil {
		return err
	}
	if !s.TeamConfig.AllowCrossZone && home == 0 {
		return ErrTeamRoute
	}
	for _, ticket := range group {
		if err = s.RequireTeamHomeZoneTx(ctx, tx, ticket.PlayerId, ticket.ZoneId); err != nil {
			return err
		}
	}
	return nil
}

func (s *Store) requireTeamLaunchHomesTx(ctx context.Context, tx *sql.Tx, data *teampb.TeamLaunchData) error {
	group := make([]*pb.MatchAuthorityTicketRecord, 0, len(data.Members))
	for _, member := range data.Members {
		group = append(group, &pb.MatchAuthorityTicketRecord{PlayerId: member.Original.PlayerId, ZoneId: data.Original.Zones[member.Original.PlayerId]})
	}
	return s.requireTeamHomeGroupTx(ctx, tx, group)
}
