package authority

import (
	"bytes"
	"context"
	"database/sql"
	"errors"
	"sort"

	pb "proto/match"
	teampb "proto/team"
	"shared/accountdirectory"
)

// LockTeamOriginals 复用匹配同一角色P围栏，调用方不得先持Team/launch/票锁。
func (s *Store) LockTeamOriginals(ctx context.Context, tx *sql.Tx, originals []*teampb.TeamOriginalRole) error {
	roles := make([]*pb.MatchAuthorityTicketRecord, 0, len(originals))
	players := make([]uint64, 0, len(originals))
	for _, role := range originals {
		players = append(players, role.GetPlayerId())
		roles = append(roles, &pb.MatchAuthorityTicketRecord{PlayerId: role.GetPlayerId(), OwnerAccount: role.GetOwnerAccount(), OwnerRevision: role.GetOwnerRevision()})
	}
	if err := s.lockTeamRoutes(ctx, tx, players); err != nil {
		return err
	}
	return s.lockOriginalPlayers(ctx, tx, roles)
}

// 私有只读列表仍须核原actor，并持P覆盖一致快照；它不修改任何带zone业务原件。
func (s *Store) LockTeamReadActor(ctx context.Context, tx *sql.Tx, player uint64) error {
	return s.lockActor(ctx, tx, player)
}

// CaptureTeamOriginals 只在新操作捕获新邀请对端。现存成员、申请/邀请与原actor
// 必须保持已捕获账号/修订；不允许以当前目录替换旧原件。全体P按uint64顺序一次获取。
func (s *Store) CaptureTeamOriginals(ctx context.Context, tx *sql.Tx, actor uint64, originals []*teampb.TeamOriginalRole, newTarget uint64) ([]*teampb.TeamOriginalRole, error) {
	if s == nil || s.roleSchema == "" || actor == 0 {
		return nil, ErrOriginalPlayer
	}
	caller := originalPlayer(ctx, actor)
	if !accountdirectory.ValidAccount(caller.OwnerAccount) || caller.OwnerRevision == 0 {
		return nil, ErrOriginalPlayer
	}
	byPlayer := make(map[uint64]*teampb.TeamOriginalRole, len(originals)+2)
	for _, role := range append(append([]*teampb.TeamOriginalRole(nil), originals...), &teampb.TeamOriginalRole{PlayerId: actor, OwnerAccount: caller.OwnerAccount, OwnerRevision: caller.OwnerRevision}) {
		if role.GetPlayerId() == 0 || !accountdirectory.ValidAccount(role.GetOwnerAccount()) || role.GetOwnerRevision() == 0 {
			return nil, ErrOriginalPlayer
		}
		if previous := byPlayer[role.PlayerId]; previous != nil && (previous.OwnerAccount != role.OwnerAccount || previous.OwnerRevision != role.OwnerRevision) {
			return nil, ErrOriginalPlayer
		}
		byPlayer[role.PlayerId] = role
	}
	ids := make([]uint64, 0, len(byPlayer)+1)
	for player := range byPlayer {
		ids = append(ids, player)
	}
	if newTarget != 0 && byPlayer[newTarget] == nil {
		ids = append(ids, newTarget)
	}
	sort.Slice(ids, func(i, j int) bool { return ids[i] < ids[j] })
	if err := s.lockTeamRoutes(ctx, tx, ids); err != nil {
		return nil, err
	}
	out := make([]*teampb.TeamOriginalRole, 0, len(ids))
	for _, player := range ids {
		var account []byte
		var revision, listing uint64
		if err := tx.QueryRowContext(ctx, "SELECT account,owner_revision,trade_listing_id FROM `"+s.roleSchema+"`.account_role WHERE player_id=? FOR UPDATE", player).Scan(&account, &revision, &listing); err != nil {
			if errors.Is(err, sql.ErrNoRows) {
				return nil, ErrOriginalPlayer
			}
			return nil, err
		}
		if revision == 0 || listing != 0 || !accountdirectory.ValidAccount(string(account)) {
			return nil, ErrOriginalPlayer
		}
		original := byPlayer[player]
		if original != nil {
			if !bytes.Equal(account, []byte(original.OwnerAccount)) || revision != original.OwnerRevision {
				return nil, ErrOriginalPlayer
			}
		} else {
			original = &teampb.TeamOriginalRole{PlayerId: player, OwnerAccount: string(account), OwnerRevision: revision}
		}
		out = append(out, original)
	}
	return out, nil
}
