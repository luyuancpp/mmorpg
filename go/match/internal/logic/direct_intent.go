package logic

import (
	"bytes"
	"context"

	"google.golang.org/protobuf/proto"
	"match/internal/constants"
	"match/internal/metrics"
	"match/internal/svc"
	pb "proto/match"
)

// 成功回包只承诺原意图已经在 SQL 提交。之后丢响应/进程退出由同一 formation 恢复。
func (l *StartActivityBattleLogic) startGlobalActivity(in *pb.StartActivityBattleRequest) (*pb.StartActivityBattleResponse, error) {
	kind := activityKindLabel(in.GetActivityContext().GetKind())
	if in.RequestId == 0 {
		return l.reject(kind, pb.ActivityBattleReject_ACTIVITY_BATTLE_REJECT_INVALID_ARGUMENT, 0), nil
	}
	key := &pb.MatchDirectIntentRecord{Kind: 2, OwnerId: in.ActivityContext.GuildId, RequestLow: in.RequestId}
	payload, e := proto.MarshalOptions{Deterministic: true}.Marshal(in)
	if e != nil {
		return l.reject(kind, pb.ActivityBattleReject_ACTIVITY_BATTLE_REJECT_INTERNAL, 0), nil
	}
	old, original, e := l.svcCtx.Authority.FindDirect(l.ctx, key)
	if e != nil {
		return l.reject(kind, pb.ActivityBattleReject_ACTIVITY_BATTLE_REJECT_INTERNAL, 0), nil
	}
	if old != nil {
		if !bytes.Equal(original, payload) {
			return l.reject(kind, pb.ActivityBattleReject_ACTIVITY_BATTLE_REJECT_INVALID_ARGUMENT, 0), nil
		}
		if old.State < 4 {
			launchGlobalFormation(l.svcCtx, old)
		}
		return &pb.StartActivityBattleResponse{BattleId: old.BattleId}, nil
	}
	for _, pid := range in.MemberPlayerIds {
		reject, _ := l.precheckMember(pid)
		if reject != pb.ActivityBattleReject_ACTIVITY_BATTLE_REJECT_NONE {
			return l.reject(kind, reject, pid), nil
		}
	}
	f, e := reserveGlobalDirect(l.ctx, l.svcCtx, key, payload, pb.MatchMode_MATCH_MODE_PVE_TEAM, in.BattleConfigId, in.MemberPlayerIds, nil, gatherOptions{activityContext: in.ActivityContext})
	if e != nil {
		return l.reject(kind, pb.ActivityBattleReject_ACTIVITY_BATTLE_REJECT_INTERNAL, 0), nil
	}
	launchGlobalFormation(l.svcCtx, f)
	metrics.ObserveActivityBattle(kind, activityResultStarted)
	return &pb.StartActivityBattleResponse{BattleId: f.BattleId}, nil
}

func globalChallengeReplay(ctx context.Context, s *svc.ServiceContext, player uint64, in *pb.RespondChallengeRequest) (*pb.RespondChallengeResponse, bool) {
	if in.ChallengeId == 0 {
		return &pb.RespondChallengeResponse{ErrorMessage: tipErr(constants.ErrChallengeExpired, "切磋邀请无效")}, true
	}
	key := &pb.MatchDirectIntentRecord{Kind: 3, OwnerId: player, RequestLow: in.ChallengeId}
	f, payload, e := s.Authority.FindDirect(ctx, key)
	if e != nil {
		return &pb.RespondChallengeResponse{ErrorMessage: tipErr(constants.ErrInternal, "切磋原意图查询暂不可用")}, true
	}
	if f == nil && len(payload) == 0 {
		return nil, false
	}
	canonical := proto.Clone(in).(*pb.RespondChallengeRequest)
	canonical.PlayerId = player
	wanted, _ := proto.MarshalOptions{Deterministic: true}.Marshal(canonical)
	if !bytes.Equal(payload, wanted) {
		return &pb.RespondChallengeResponse{ErrorMessage: tipErr(constants.ErrChallengeExpired, "该切磋已经作出决定")}, true
	}
	if f != nil && f.State < 4 {
		launchGlobalFormation(s, f)
	}
	return &pb.RespondChallengeResponse{}, true
}

func (l *ChallengeLogic) decideGlobalChallenge(in *pb.RespondChallengeRequest, challenger, responder uint64, config uint32, expires uint64) (*pb.RespondChallengeResponse, error) {
	if expires > 0 && nowMs() >= expires {
		return &pb.RespondChallengeResponse{ErrorMessage: tipErr(constants.ErrChallengeExpired, "切磋邀请已过期")}, nil
	}
	canonical := proto.Clone(in).(*pb.RespondChallengeRequest)
	canonical.PlayerId = responder
	payload, e := proto.MarshalOptions{Deterministic: true}.Marshal(canonical)
	if e != nil {
		return nil, e
	}
	key := &pb.MatchDirectIntentRecord{Kind: 3, OwnerId: responder, RequestLow: in.ChallengeId}
	if !in.Accept {
		won, e := l.svcCtx.Authority.DeclineDirect(l.ctx, key, payload)
		if e != nil || !won {
			return &pb.RespondChallengeResponse{ErrorMessage: tipErr(constants.ErrChallengeExpired, "该切磋已决定或结果尚未确认")}, nil
		}
		deleteChallengeRecord(l.svcCtx, in.ChallengeId, responder)
		l.pushResult(challenger, in.ChallengeId, false, responder)
		return &pb.RespondChallengeResponse{}, nil
	}
	for _, pid := range []uint64{challenger, responder} {
		if locked, e := isPlayerBattleLocked(l.svcCtx, pid); e != nil || locked {
			return &pb.RespondChallengeResponse{ErrorMessage: tipErr(constants.ErrChallengeSelfBusy, "成员仍在其它战斗中")}, nil
		}
	}
	f, e := reserveGlobalDirect(l.ctx, l.svcCtx, key, payload, pb.MatchMode_MATCH_MODE_PVP_CHALLENGE, config, []uint64{challenger, responder}, nil, gatherOptions{})
	if e != nil {
		return &pb.RespondChallengeResponse{ErrorMessage: tipErr(constants.ErrInternal, "切磋开局结果尚未确认，请查询或重试原邀请")}, nil
	}
	// 提交之后不再依赖邀请缓存存活；删除失败也不会产生第二笔 SQL 成局。
	deleteChallengeRecord(l.svcCtx, in.ChallengeId, responder)
	l.pushResult(challenger, in.ChallengeId, true, responder)
	l.pushResult(responder, in.ChallengeId, true, responder)
	launchGlobalFormation(l.svcCtx, f)
	return &pb.RespondChallengeResponse{}, nil
}
