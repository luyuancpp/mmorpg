package authority

import (
	"bytes"
	"context"
	"database/sql"
	"errors"

	base "proto/common/base"
	teampb "proto/team"
	"shared/accountdirectory"

	"google.golang.org/protobuf/proto"
)

// ReadTeamAdmission 只接受scene已经在全局SQL登记的完整入场原件。
// 入口还须校验scene服务HMAC；本层绝不拼造gate会话或用当前owner补原actor。
func (s *Store) ReadTeamAdmission(ctx context.Context, admission *base.RoleAdmissionIdentity) (*teampb.TeamMembershipRecord, *teampb.TeamAuthorityRecord, error) {
	if s == nil || s.roleSchema == "" || !accountdirectory.ValidAdmission(admission) {
		return nil, nil, ErrOriginalPlayer
	}
	tx, err := s.DB.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelRepeatableRead})
	if err != nil {
		return nil, nil, err
	}
	defer tx.Rollback()
	var revision, listing uint64
	var account []byte
	if err = tx.QueryRowContext(ctx, "SELECT account,owner_revision,trade_listing_id FROM `"+s.roleSchema+"`.account_role WHERE player_id=? FOR UPDATE", admission.PlayerId).Scan(&account, &revision, &listing); err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			err = ErrOriginalPlayer
		}
		return nil, nil, err
	}
	if revision != admission.OwnerRevision || listing != 0 {
		return nil, nil, ErrOriginalPlayer
	}
	var highest, closed uint64
	var raw []byte
	if err = tx.QueryRowContext(ctx, "SELECT highest_generation,closed_through,identity_proto FROM `"+s.roleSchema+"`.account_role_admission WHERE player_id=? AND scene_instance_id=? FOR UPDATE", admission.PlayerId, []byte(admission.SceneInstanceId)).Scan(&highest, &closed, &raw); err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			err = ErrOriginalPlayer
		}
		return nil, nil, err
	}
	original := &base.RoleAdmissionIdentity{}
	if proto.Unmarshal(raw, original) != nil || highest != admission.Generation || closed >= admission.Generation || !proto.Equal(original, admission) {
		return nil, nil, ErrOriginalPlayer
	}
	member := &teampb.TeamMembershipRecord{PlayerId: admission.PlayerId}
	err = tx.QueryRowContext(ctx, "SELECT team_id,epoch FROM team_membership WHERE player_id=?", admission.PlayerId).Scan(&member.TeamId, &member.Epoch)
	if errors.Is(err, sql.ErrNoRows) {
		err = nil
	}
	if err != nil {
		return nil, nil, err
	}
	var record *teampb.TeamAuthorityRecord
	if member.TeamId != 0 {
		record = &teampb.TeamAuthorityRecord{TeamId: member.TeamId}
		if err = tx.QueryRowContext(ctx, "SELECT version,payload,state,idle_expires_ms FROM team_authority WHERE team_id=?", member.TeamId).Scan(&record.Version, &record.Payload, &record.State, &record.IdleExpiresMs); err != nil {
			return nil, nil, err
		}
		data := &teampb.TeamAuthorityData{}
		if record.State != 1 || record.Version == 0 || proto.Unmarshal(record.Payload, data) != nil || data.GetRecord().GetTeamId() != member.TeamId {
			return nil, nil, errors.New("team: SQL队伍原件损坏")
		}
		found := false
		for _, original := range data.Members {
			if original.PlayerId == member.PlayerId && original.OwnerRevision == admission.OwnerRevision && bytes.Equal(account, []byte(original.OwnerAccount)) {
				found = true
			}
		}
		memberFound := false
		for _, original := range data.GetRecord().GetMembers() {
			if original.PlayerId == member.PlayerId {
				memberFound = true
			}
		}
		if !found || !memberFound {
			return nil, nil, errors.New("team: SQL名册与原成员索引不一致")
		}
	}
	return member, record, tx.Commit()
}
