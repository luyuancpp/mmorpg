package data

import (
	"bytes"
	"context"
	"crypto/sha256"
	"database/sql"
	"errors"
	"sort"

	pb "proto/guild"
	matchpb "proto/match"
	"shared/accountdirectory"

	"google.golang.org/protobuf/proto"
)

const trialLaunchColumns = "request_id,battle_id,state,original_proto,request_digest,created_ms,updated_ms,cancel_receipt"

func (r *ActivityRepo) TrialLaunchEnabled() bool {
	return r != nil && r.guilds != nil && r.guilds.roleAuthority != ""
}

func decodeTrialLaunch(row *pb.GuildTrialLaunchRecord) (*pb.GuildTrialLaunchOriginal, error) {
	if row == nil || len(row.OriginalProto) == 0 || len(row.OriginalProto) > 64*1024 {
		return nil, ErrWriteConflict
	}
	sum := sha256.Sum256(row.OriginalProto)
	if !bytes.Equal(sum[:], row.RequestDigest) {
		return nil, ErrWriteConflict
	}
	out := &pb.GuildTrialLaunchOriginal{}
	if proto.Unmarshal(row.OriginalProto, out) != nil || out.RequestId != row.RequestId || out.GuildId == 0 || out.ActivityId == 0 || out.PeriodKey == 0 || out.GuildPeriodKey == 0 || out.InitiatorPlayerId == 0 || out.DungeonId == 0 || len(out.Roster) < minTrialLobbyRoster || len(out.Roster) > maxTrialLobbyRoster {
		return nil, ErrWriteConflict
	}
	seen := map[uint64]bool{}
	var previous uint64
	for _, v := range out.Roster {
		if v.GetPlayerId() == 0 || v.PlayerId <= previous || seen[v.PlayerId] || !accountdirectory.ValidAccount(v.Account) || v.OwnerRevision == 0 {
			return nil, ErrWriteConflict
		}
		seen[v.PlayerId] = true
		previous = v.PlayerId
	}
	if !seen[out.InitiatorPlayerId] {
		return nil, ErrWriteConflict
	}
	return out, nil
}
func scanTrialLaunch(row rowScanner) (*pb.GuildTrialLaunchRecord, error) {
	out := &pb.GuildTrialLaunchRecord{}
	err := row.Scan(&out.RequestId, &out.BattleId, &out.State, &out.OriginalProto, &out.RequestDigest, &out.CreatedMs, &out.UpdatedMs, &out.CancelReceipt)
	return out, err
}
func launchPlayers(original *pb.GuildTrialLaunchOriginal) []uint64 {
	if original == nil {
		return nil
	}
	ids := make([]uint64, 0, len(original.Roster))
	for _, p := range original.Roster {
		ids = append(ids, p.PlayerId)
	}
	return ids
}
func (r *ActivityRepo) TrialLaunch(ctx context.Context, id uint64) (*pb.GuildTrialLaunchRecord, *pb.GuildTrialLaunchOriginal, error) {
	row, err := scanTrialLaunch(r.db.QueryRowContext(ctx, "SELECT "+trialLaunchColumns+" FROM guild_trial_launch WHERE request_id=?", id))
	if err != nil {
		return nil, nil, err
	}
	original, err := decodeTrialLaunch(row)
	return row, original, err
}

// 原同意身份来自私有lobby值；首次提交前逐项核原account/revision，未知提交后只重放永久原件。
func (r *ActivityRepo) PrepareTrialLaunch(ctx context.Context, requestID uint64, battle TrialBattleKey, dungeon uint32, roster []*pb.GuildTrialRoleIdentity, nowMs uint64) (*pb.GuildTrialLaunchOriginal, error) {
	if r.guilds == nil || r.guilds.roleAuthority == "" {
		return nil, nil
	} // 旧独立仓储夹具；在线装配强制权威。
	if requestID == 0 || dungeon == 0 || len(roster) < minTrialLobbyRoster || len(roster) > maxTrialLobbyRoster {
		return nil, ErrWriteConflict
	}
	if battle.GuildID == 0 || battle.ActivityID == 0 || battle.PeriodKey == 0 || battle.GuildPeriodKey == 0 || battle.InitiatorPlayerID == 0 {
		return nil, ErrWriteConflict
	}
	proposed := &pb.GuildTrialLaunchOriginal{RequestId: requestID, GuildId: battle.GuildID, ActivityId: battle.ActivityID, PeriodKey: battle.PeriodKey, GuildPeriodKey: battle.GuildPeriodKey, InitiatorPlayerId: battle.InitiatorPlayerID, DungeonId: dungeon, CreatedMs: nowMs}
	for _, p := range roster {
		if p == nil {
			return nil, ErrWriteConflict
		}
		proposed.Roster = append(proposed.Roster, proto.Clone(p).(*pb.GuildTrialRoleIdentity))
	}
	sort.Slice(proposed.Roster, func(i, j int) bool { return proposed.Roster[i].PlayerId < proposed.Roster[j].PlayerId })
	initiator := false
	for _, p := range proposed.Roster {
		if p.PlayerId == proposed.InitiatorPlayerId {
			initiator = true
		}
	}
	if !initiator {
		return nil, ErrWriteConflict
	}
	var result *pb.GuildTrialLaunchOriginal
	err := r.guilds.inTx(withRoleTargets(ctx, launchPlayers(proposed)...), opTrialRegister, func(ctx context.Context, tx *sql.Tx) error {
		if _, _, err := lockTrialGuild(ctx, tx, battle.GuildID); err != nil {
			return err
		}
		if err := r.verifyTrialOwners(ctx, tx, proposed); err != nil {
			return err
		}
		for _, p := range proposed.Roster {
			var member uint64
			if err := tx.QueryRowContext(ctx, "SELECT player_id FROM guild_member WHERE guild_id=? AND player_id=? FOR UPDATE", battle.GuildID, p.PlayerId).Scan(&member); err != nil {
				return err
			}
		}
		row, err := scanTrialLaunch(tx.QueryRowContext(ctx, "SELECT "+trialLaunchColumns+" FROM guild_trial_launch WHERE request_id=? FOR UPDATE", requestID))
		if err == nil {
			saved, err := decodeTrialLaunch(row)
			if err != nil {
				return err
			}
			proposed.CreatedMs = saved.CreatedMs
			if row.State == 3 || !proto.Equal(proposed, saved) {
				return ErrWriteConflict
			}
			for _, p := range saved.Roster {
				var account string
				var revision uint64
				var state uint32
				if err := tx.QueryRowContext(ctx, "SELECT account,owner_revision,state FROM guild_trial_launch_role WHERE player_id=? AND request_id=? FOR UPDATE", p.PlayerId, saved.RequestId).Scan(&account, &revision, &state); err != nil {
					return err
				}
				if account != p.Account || revision != p.OwnerRevision || state != 1 {
					return ErrWriteConflict
				}
			}
			result = saved
			return nil
		}
		if !errors.Is(err, sql.ErrNoRows) {
			return err
		}
		raw, err := proto.MarshalOptions{Deterministic: true}.Marshal(proposed)
		if err != nil {
			return err
		}
		sum := sha256.Sum256(raw)
		_, err = tx.ExecContext(ctx, "INSERT INTO guild_trial_launch("+trialLaunchColumns+") VALUES(?,0,1,?,?,?,?,?)", requestID, raw, sum[:], nowMs, nowMs, []byte{})
		if err != nil {
			return err
		}
		for _, p := range proposed.Roster {
			if _, err = tx.ExecContext(ctx, "INSERT INTO guild_trial_launch_role(player_id,request_id,account,owner_revision,state) VALUES(?,?,?,?,1)", p.PlayerId, requestID, p.Account, p.OwnerRevision); err != nil {
				return err
			}
		}
		result = proposed
		return nil
	})
	return result, err
}
func (r *ActivityRepo) verifyTrialOwners(ctx context.Context, tx *sql.Tx, original *pb.GuildTrialLaunchOriginal) error {
	seen := map[uint64]bool{}
	for _, p := range original.Roster {
		if p.GetPlayerId() == 0 || seen[p.PlayerId] || !accountdirectory.ValidAccount(p.Account) || p.OwnerRevision == 0 {
			return ErrWriteConflict
		}
		seen[p.PlayerId] = true
		var account string
		var revision, listing uint64
		if err := tx.QueryRowContext(ctx, "SELECT account,owner_revision,trade_listing_id FROM `"+r.guilds.roleAuthority+"`.account_role WHERE player_id=? FOR UPDATE", p.PlayerId).Scan(&account, &revision, &listing); err != nil {
			return err
		}
		if account != p.Account || revision != p.OwnerRevision || listing != 0 {
			return ErrWriteConflict
		}
	}
	return nil
}

// caller已按全员P升序后持G；既有launch和其ACTIVE投影必须与永久原件完全一致。
func (r *ActivityRepo) lockTrialLaunch(ctx context.Context, tx *sql.Tx, battle TrialBattleKey, original *pb.GuildTrialLaunchOriginal, terminal bool, nowMs uint64) error {
	if r.guilds.roleAuthority == "" {
		return nil
	}
	if original == nil || battle.LaunchRequestID == 0 {
		return ErrWriteConflict
	}
	row, err := scanTrialLaunch(tx.QueryRowContext(ctx, "SELECT "+trialLaunchColumns+" FROM guild_trial_launch WHERE request_id=? FOR UPDATE", battle.LaunchRequestID))
	if err != nil {
		return err
	}
	saved, err := decodeTrialLaunch(row)
	if err != nil {
		return err
	}
	if !proto.Equal(saved, original) || saved.GuildId != battle.GuildID || saved.ActivityId != battle.ActivityID || saved.PeriodKey != battle.PeriodKey || saved.GuildPeriodKey != battle.GuildPeriodKey || saved.InitiatorPlayerId != battle.InitiatorPlayerID || row.BattleId != 0 && row.BattleId != battle.BattleID {
		return ErrWriteConflict
	}
	if row.State == 3 {
		if terminal {
			return nil
		}
		return ErrWriteConflict
	}
	if err := r.verifyTrialOwners(ctx, tx, saved); err != nil {
		return err
	}
	for _, p := range saved.Roster {
		var account string
		var revision uint64
		var state uint32
		if err := tx.QueryRowContext(ctx, "SELECT account,owner_revision,state FROM guild_trial_launch_role WHERE player_id=? AND request_id=? FOR UPDATE", p.PlayerId, saved.RequestId).Scan(&account, &revision, &state); err != nil {
			return err
		}
		if account != p.Account || revision != p.OwnerRevision || state != 1 {
			return ErrWriteConflict
		}
	}
	state, roleState := uint32(2), uint32(1)
	if terminal {
		state, roleState = 3, 2
	}
	if _, err = tx.ExecContext(ctx, "UPDATE guild_trial_launch SET battle_id=?,state=?,updated_ms=? WHERE request_id=?", battle.BattleID, state, nowMs, saved.RequestId); err != nil {
		return err
	}
	if terminal {
		for _, p := range saved.Roster {
			if _, err = tx.ExecContext(ctx, "UPDATE guild_trial_launch_role SET state=? WHERE player_id=? AND request_id=?", roleState, p.PlayerId, saved.RequestId); err != nil {
				return err
			}
		}
	}
	return nil
}
func (r *ActivityRepo) trialLaunchOriginal(ctx context.Context, battle *TrialBattleKey) (*pb.GuildTrialLaunchOriginal, error) {
	if r.guilds == nil || r.guilds.roleAuthority == "" {
		return nil, nil
	}
	if battle.LaunchRequestID == 0 && battle.BattleID != 0 {
		var id uint64
		if err := r.db.QueryRowContext(ctx, "SELECT request_id FROM guild_trial_launch WHERE battle_id=?", battle.BattleID).Scan(&id); err != nil {
			return nil, err
		}
		battle.LaunchRequestID = id
	}
	_, original, err := r.TrialLaunch(ctx, battle.LaunchRequestID)
	return original, err
}

// 解散持G后不会再有任何本帮发奖者；永久终局原件，防迟到恢复重新开战。EXPIRED则不调用它，因旧设计仍允许迟到结果。
func releaseGuildTrialLaunches(ctx context.Context, tx *sql.Tx, guildID, nowMs uint64) error {
	// PREPARED含尚未知晓是否发出的RPC，不能凭帮会消失释放；等待原request的持久终局证明。
	rows, err := tx.QueryContext(ctx, "SELECT "+trialLaunchColumns+" FROM guild_trial_launch WHERE state=2 ORDER BY request_id")
	if err != nil {
		return err
	}
	var originals []*pb.GuildTrialLaunchOriginal
	for rows.Next() {
		row, err := scanTrialLaunch(rows)
		if err != nil {
			rows.Close()
			return err
		}
		original, err := decodeTrialLaunch(row)
		if err != nil {
			rows.Close()
			return err
		}
		if original.GuildId == guildID {
			originals = append(originals, original)
		}
	}
	if err := rows.Err(); err != nil {
		rows.Close()
		return err
	}
	if err := rows.Close(); err != nil {
		return err
	}
	for _, original := range originals {
		if _, err = tx.ExecContext(ctx, "UPDATE guild_trial_launch SET state=3,updated_ms=? WHERE request_id=?", nowMs, original.RequestId); err != nil {
			return err
		}
		for _, p := range original.Roster {
			if _, err = tx.ExecContext(ctx, "UPDATE guild_trial_launch_role SET state=2 WHERE player_id=? AND request_id=?", p.PlayerId, original.RequestId); err != nil {
				return err
			}
		}
	}
	return nil
}

// 持久扫描按request_id轮转；失败也前进，原行仍保留等待下一圈，离线队头不阻挡后续合同。
func (r *ActivityRepo) PendingTrialLaunches(ctx context.Context, after uint64, limit int) ([]*pb.GuildTrialLaunchOriginal, error) {
	if r.guilds.roleAuthority == "" {
		return nil, nil
	}
	if limit < 1 || limit > 100 {
		return nil, ErrWriteConflict
	}
	rows, err := r.db.QueryContext(ctx, "SELECT "+trialLaunchColumns+" FROM guild_trial_launch WHERE state IN(1,2) AND request_id>? ORDER BY request_id LIMIT ?", after, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	out := []*pb.GuildTrialLaunchOriginal{}
	for rows.Next() {
		row, err := scanTrialLaunch(rows)
		if err != nil {
			return nil, err
		}
		original, err := decodeTrialLaunch(row)
		if err != nil {
			return nil, err
		}
		out = append(out, original)
	}
	return out, rows.Err()
}

// receipt仅由显式注入且验证match回复签名/原请求摘要的适配器传入，客户端无此入口。
// 终局证明与ACTIVE解除同事务保存；未知提交重读原launch，不发新Start/新名单。
func (r *ActivityRepo) FinishCancelledTrialLaunch(ctx context.Context, original *pb.GuildTrialLaunchOriginal, receipt *matchpb.CancelActivityBattleResponse, nowMs uint64) error {
	if original == nil || receipt == nil || !(receipt.RequestCancelled && receipt.BattleId == 0 || receipt.FormationState == 5 && receipt.BattleId != 0) {
		return ErrWriteConflict
	}
	raw, err := proto.MarshalOptions{Deterministic: true}.Marshal(receipt)
	if err != nil {
		return err
	}
	return r.guilds.inTx(withRoleTargets(ctx, launchPlayers(original)...), opTrialMark, func(ctx context.Context, tx *sql.Tx) error {
		// 已解散帮会也须消费可信取消证明；G缺失时不补造帮会或奖项。
		if _, _, err := lockTrialGuild(ctx, tx, original.GuildId); err != nil && !errors.Is(err, ErrGuildGone) {
			return err
		}
		row, err := scanTrialLaunch(tx.QueryRowContext(ctx, "SELECT "+trialLaunchColumns+" FROM guild_trial_launch WHERE request_id=? FOR UPDATE", original.RequestId))
		if err != nil {
			return err
		}
		saved, err := decodeTrialLaunch(row)
		if err != nil {
			return err
		}
		if !proto.Equal(saved, original) || row.BattleId != 0 && row.BattleId != receipt.BattleId {
			return ErrWriteConflict
		}
		if row.State == 3 {
			return nil
		}
		if err := r.verifyTrialOwners(ctx, tx, saved); err != nil {
			return err
		}
		for _, p := range saved.Roster {
			var account string
			var revision uint64
			var state uint32
			if err := tx.QueryRowContext(ctx, "SELECT account,owner_revision,state FROM guild_trial_launch_role WHERE player_id=? AND request_id=? FOR UPDATE", p.PlayerId, saved.RequestId).Scan(&account, &revision, &state); err != nil {
				return err
			}
			if account != p.Account || revision != p.OwnerRevision || state != 1 {
				return ErrWriteConflict
			}
		}
		if _, err = tx.ExecContext(ctx, "UPDATE guild_trial_launch SET battle_id=?,state=3,cancel_receipt=?,updated_ms=? WHERE request_id=?", receipt.BattleId, raw, nowMs, saved.RequestId); err != nil {
			return err
		}
		for _, p := range saved.Roster {
			if _, err = tx.ExecContext(ctx, "UPDATE guild_trial_launch_role SET state=2 WHERE player_id=? AND request_id=?", p.PlayerId, saved.RequestId); err != nil {
				return err
			}
		}
		if receipt.BattleId != 0 {
			if _, err = tx.ExecContext(ctx, "UPDATE guild_trial_battle SET state=?,settle_result=?,settled_ms=? WHERE battle_id=?", uint32(pb.GuildTrialBattleState_GUILD_TRIAL_BATTLE_STATE_SETTLED), uint32(pb.GuildTrialSettleResult_GUILD_TRIAL_SETTLE_RESULT_LOSS), nowMs, receipt.BattleId); err != nil {
				return err
			}
		}
		return nil
	})
}
