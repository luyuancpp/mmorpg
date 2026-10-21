package authority

import (
	"bytes"
	"context"
	"database/sql"
	"errors"
	"regexp"
	"sort"

	pb "proto/match"
	"shared/accountdirectory"
	"shared/roleidentity"
)

var ErrOriginalPlayer = errors.New("匹配原会话角色归属已失效")

// ConfigureRoleAuthority只在装配时绑定同TiDB目录；不从DSN或玩家所在区猜权威库。
func (s *Store) ConfigureRoleAuthority(ctx context.Context, schema string) error {
	if !regexp.MustCompile(`^[a-z][a-z0-9_]{0,63}$`).MatchString(schema) {
		return errors.New("匹配全局角色目录schema未配置")
	}
	query := "SELECT account,owner_revision,trade_listing_id FROM `" + schema + "`.account_role"
	rows, err := s.DB.QueryContext(ctx, query+" LIMIT 0")
	if err != nil {
		return err
	}
	if err = rows.Close(); err != nil {
		return err
	}
	s.roleSchema = schema
	s.roleGuard = func(ctx context.Context, tx *sql.Tx, originals []*pb.MatchAuthorityTicketRecord) error {
		ordered := append([]*pb.MatchAuthorityTicketRecord(nil), originals...)
		for _, original := range ordered {
			if original == nil || original.PlayerId == 0 || !accountdirectory.ValidAccount(original.OwnerAccount) || original.OwnerRevision == 0 {
				return ErrOriginalPlayer
			}
		}
		sort.Slice(ordered, func(i, j int) bool { return ordered[i].PlayerId < ordered[j].PlayerId })
		var prior *pb.MatchAuthorityTicketRecord
		for _, original := range ordered {
			if prior != nil && prior.PlayerId == original.PlayerId {
				if prior.OwnerAccount != original.OwnerAccount || prior.OwnerRevision != original.OwnerRevision {
					return ErrOriginalPlayer
				}
				continue
			}
			var account []byte
			var revision, listing uint64
			if err := tx.QueryRowContext(ctx, query+" WHERE player_id=? FOR UPDATE", original.PlayerId).Scan(&account, &revision, &listing); err != nil {
				if errors.Is(err, sql.ErrNoRows) {
					return ErrOriginalPlayer
				}
				return err
			}
			if !bytes.Equal(account, []byte(original.OwnerAccount)) || revision != original.OwnerRevision || listing != 0 {
				return ErrOriginalPlayer
			}
			prior = original
		}
		return nil
	}
	return nil
}

// 角色P按uint64升序，先于所有匹配票/成局/意图行。测试只可显式注入独立目录替身。
func (s *Store) lockOriginalPlayers(ctx context.Context, tx *sql.Tx, originals []*pb.MatchAuthorityTicketRecord) error {
	if s == nil || s.roleGuard == nil || len(originals) == 0 {
		return ErrOriginalPlayer
	}
	return s.roleGuard(ctx, tx, originals)
}

func originalPlayer(ctx context.Context, player uint64) *pb.MatchAuthorityTicketRecord {
	original, ok := roleidentity.Original(ctx)
	if !ok || original.Role.GetPlayerId() != player {
		return &pb.MatchAuthorityTicketRecord{PlayerId: player}
	}
	return &pb.MatchAuthorityTicketRecord{PlayerId: player, OwnerAccount: original.Account, OwnerRevision: original.OwnerRevision}
}

func (s *Store) lockActor(ctx context.Context, tx *sql.Tx, player uint64) error {
	return s.lockOriginalPlayers(ctx, tx, []*pb.MatchAuthorityTicketRecord{originalPlayer(ctx, player)})
}

// 只用于首次创建邀请的新对端捕获；重放/应答必须沿持久原件，不能再次调用它补新owner。
func (s *Store) captureChallengeOriginals(ctx context.Context, tx *sql.Tx, actor, target uint64) (*pb.MatchAuthorityTicketRecord, *pb.MatchAuthorityTicketRecord, error) {
	if s.roleSchema == "" || actor == 0 || target == 0 || actor == target {
		return nil, nil, ErrOriginalPlayer
	}
	original := originalPlayer(ctx, actor)
	ids := []uint64{actor, target}
	sort.Slice(ids, func(i, j int) bool { return ids[i] < ids[j] })
	var peer *pb.MatchAuthorityTicketRecord
	for _, player := range ids {
		var account []byte
		var revision, listing uint64
		if err := tx.QueryRowContext(ctx, "SELECT account,owner_revision,trade_listing_id FROM `"+s.roleSchema+"`.account_role WHERE player_id=? FOR UPDATE", player).Scan(&account, &revision, &listing); err != nil {
			if errors.Is(err, sql.ErrNoRows) {
				return nil, nil, ErrOriginalPlayer
			}
			return nil, nil, err
		}
		if listing != 0 || revision == 0 || !accountdirectory.ValidAccount(string(account)) {
			return nil, nil, ErrOriginalPlayer
		}
		if player == actor {
			if !bytes.Equal(account, []byte(original.OwnerAccount)) || revision != original.OwnerRevision {
				return nil, nil, ErrOriginalPlayer
			}
		} else {
			peer = &pb.MatchAuthorityTicketRecord{PlayerId: player, OwnerAccount: string(account), OwnerRevision: revision}
		}
	}
	return original, peer, nil
}

func (s *Store) ticketForActor(ctx context.Context, player uint64) (*pb.MatchAuthorityTicketRecord, error) {
	tx, err := s.DB.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelReadCommitted})
	if err != nil {
		return nil, err
	}
	defer tx.Rollback()
	if err = s.lockActor(ctx, tx, player); err != nil {
		return nil, err
	}
	actor := originalPlayer(ctx, player)
	if _, err = tx.ExecContext(ctx, "UPDATE match_authority_ticket SET state=4 WHERE player_id=? AND owner_account=? AND owner_revision=? AND state=1 AND expires_ms<=CAST(UNIX_TIMESTAMP(CURRENT_TIMESTAMP(3))*1000 AS UNSIGNED)", player, actor.OwnerAccount, actor.OwnerRevision); err != nil {
		return nil, err
	}
	result, err := scanTicket(tx.QueryRowContext(ctx, "SELECT "+ticketColumns+" FROM match_authority_ticket WHERE player_id=?", player))
	if errors.Is(err, sql.ErrNoRows) {
		err = nil
		result = nil
	}
	if err != nil {
		return nil, err
	}
	if result != nil && (result.OwnerAccount != actor.OwnerAccount || result.OwnerRevision != actor.OwnerRevision) {
		result = nil
	}
	if err = tx.Commit(); err != nil {
		return nil, err
	}
	return result, nil
}
