package data

import (
	"bytes"
	"context"
	"errors"
	"strconv"
	"time"

	pb "proto/guild"
	"shared/accountdirectory"
	"shared/roleidentity"

	"google.golang.org/protobuf/proto"
)

func trialConsentKey(lobby, player uint64) string {
	return "guild:trial:lobby:identity:" + strconv.FormatUint(lobby, 10) + ":" + strconv.FormatUint(player, 10)
}

// 同意原件只由本人原会话产生；已有不同原件不能覆盖。Redis到期不是对SQL中PREPARED合同的撤销。
func (r *TrialLobbyRepo) rememberConsent(ctx context.Context, lobby *TrialLobby, player, nowMs uint64) error {
	original, present := roleidentity.Original(ctx)
	if !present {
		return nil
	} // 原隔离仓储夹具；在线launch缺原件仍拒绝，不以当前owner补。
	if original.Role.PlayerId != player || lobby == nil || !lobby.Has(player) {
		return ErrWriteConflict
	}
	value := &pb.GuildTrialLobbyConsent{LobbyId: lobby.LobbyID, Identity: &pb.GuildTrialRoleIdentity{PlayerId: player, Account: original.Account, OwnerRevision: original.OwnerRevision}}
	raw, err := proto.MarshalOptions{Deterministic: true}.Marshal(value)
	if err != nil {
		return err
	}
	key := trialConsentKey(lobby.LobbyID, player)
	// 邀请有效期+原展示宽限是有界寿命；launch提交前只认仍可读的完整同意原件。
	if lobby.ExpireAtMs <= nowMs {
		return ErrWriteConflict
	}
	ttl := time.Duration(lobby.ExpireAtMs-nowMs)*time.Millisecond + trialLobbyKeyGrace
	created, err := r.rdb.SetNX(ctx, key, raw, ttl).Result()
	if err != nil {
		return err
	}
	if created {
		return nil
	}
	old, err := r.rdb.Get(ctx, key).Bytes()
	if err != nil {
		return err
	}
	if !bytes.Equal(old, raw) {
		return ErrWriteConflict
	}
	return nil
}
func (r *TrialLobbyRepo) LoadConsents(ctx context.Context, lobby *TrialLobby) ([]*pb.GuildTrialRoleIdentity, error) {
	if lobby == nil || len(lobby.Roster) == 0 || len(lobby.Roster) > maxTrialLobbyRoster {
		return nil, ErrWriteConflict
	}
	out := make([]*pb.GuildTrialRoleIdentity, 0, len(lobby.Roster))
	for _, player := range lobby.Roster {
		raw, err := r.rdb.Get(ctx, trialConsentKey(lobby.LobbyID, player)).Bytes()
		if err != nil {
			return nil, err
		}
		if len(raw) > 4096 {
			return nil, ErrWriteConflict
		}
		value := &pb.GuildTrialLobbyConsent{}
		if proto.Unmarshal(raw, value) != nil || value.LobbyId != lobby.LobbyID || value.Identity.GetPlayerId() != player || !accountdirectory.ValidAccount(value.Identity.GetAccount()) || value.Identity.GetOwnerRevision() == 0 {
			return nil, errors.New("历练邀请原同意身份缺失或非法")
		}
		out = append(out, value.Identity)
	}
	return out, nil
}
