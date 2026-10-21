package logic

import (
	"context"
	"errors"
	"time"

	"guild/internal/data"
	battlepb "proto/battle"
	pb "proto/guild"
	matchpb "proto/match"

	"github.com/zeromicro/go-zero/core/logx"
)

type trialLaunchStore interface {
	TrialLaunchEnabled() bool
	PrepareTrialLaunch(context.Context, uint64, data.TrialBattleKey, uint32, []*pb.GuildTrialRoleIdentity, uint64) (*pb.GuildTrialLaunchOriginal, error)
	PendingTrialLaunches(context.Context, uint64, int) ([]*pb.GuildTrialLaunchOriginal, error)
	FinishCancelledTrialLaunch(context.Context, *pb.GuildTrialLaunchOriginal, *matchpb.CancelActivityBattleResponse, uint64) error
}

func prepareOriginalTrialLaunch(ctx context.Context, d *ActivityDeps, lobby *data.TrialLobby, battle *data.TrialBattleKey, dungeon uint32, nowMs uint64) error {
	store, ok := d.trials.(trialLaunchStore)
	if !ok || !store.TrialLaunchEnabled() {
		return nil
	}
	roster, err := d.Lobby.LoadConsents(ctx, lobby)
	if err != nil {
		return err
	}
	original, err := store.PrepareTrialLaunch(ctx, lobby.LobbyID, *battle, dungeon, roster, nowMs)
	if err != nil {
		return err
	}
	if original == nil {
		return errors.New("历练原身份合同未提交")
	}
	battle.LaunchRequestID = original.RequestId
	return nil
}

// 每个loop独占cursor；每轮最多20个原请求，单个依赖失败不挡后续，原件不重新捕获。
type trialLaunchRecovery struct {
	d     *ActivityDeps
	after uint64
}

func (r *trialLaunchRecovery) round(ctx context.Context) {
	store, ok := r.d.trials.(trialLaunchStore)
	if !ok || !store.TrialLaunchEnabled() || r.d.Cancellation == nil {
		return
	}
	page, err := store.PendingTrialLaunches(ctx, r.after, 20)
	if err != nil {
		logx.Errorf("[GuildTrial] 原开战合同扫描失败: %v", err)
		return
	}
	if len(page) == 0 {
		r.after = 0
		return
	}
	budget, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	for _, original := range page {
		if budget.Err() != nil {
			return
		}
		call, cancelOne := context.WithTimeout(budget, 2*time.Second)
		err := r.recoverOne(call, store, original)
		cancelOne()
		r.after = original.RequestId
		if err != nil {
			logx.Errorf("[GuildTrial] 原开战合同恢复失败 request_id=%d: %v", original.RequestId, err)
		}
	}
}
func (r *trialLaunchRecovery) recoverOne(ctx context.Context, store trialLaunchStore, original *pb.GuildTrialLaunchOriginal) error {
	battle := data.TrialBattleKey{GuildID: original.GuildId, ActivityID: original.ActivityId, PeriodKey: original.PeriodKey, GuildPeriodKey: original.GuildPeriodKey, InitiatorPlayerID: original.InitiatorPlayerId, LaunchRequestID: original.RequestId}
	// match要求发起人在名单首位；其余按原件升序，原请求字节不会受重启或当前owner变化影响。
	roster := []uint64{original.InitiatorPlayerId}
	for _, p := range original.Roster {
		if p.PlayerId != original.InitiatorPlayerId {
			roster = append(roster, p.PlayerId)
		}
	}
	lobby := &data.TrialLobby{LobbyID: original.RequestId, GuildID: original.GuildId, InitiatorID: original.InitiatorPlayerId, ActivityID: original.ActivityId, Roster: roster}
	in := &matchpb.StartActivityBattleRequest{RequestId: original.RequestId, BattleConfigId: original.DungeonId, MemberPlayerIds: lobby.Roster, ActivityContext: &battlepb.BattleActivityContext{Kind: battlepb.EBattleActivityKind_BATTLE_ACTIVITY_KIND_GUILD_TRIAL, GuildId: battle.GuildID, ActivityId: battle.ActivityID, PeriodKey: battle.PeriodKey, GuildPeriodKey: battle.GuildPeriodKey, InitiatorPlayerId: battle.InitiatorPlayerID, LaunchRequestId: battle.LaunchRequestID}}
	receipt, err := r.d.Cancellation.CancelOriginal(ctx, in)
	if err != nil {
		return err
	}
	if receipt.RequestCancelled && receipt.BattleId == 0 || receipt.FormationState == 5 {
		return store.FinishCancelledTrialLaunch(ctx, original, receipt, uint64(r.d.Now().UnixMilli()))
	}
	if receipt.BattleId == 0 {
		return errors.New("原match开战请求尚无终局证明，保留原件")
	}
	battle.BattleID = receipt.BattleId
	return r.d.trials.RegisterTrialBattle(ctx, battle, original.CreatedMs)
}
