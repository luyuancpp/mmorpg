package logic

import (
	"bytes"
	"context"
	"database/sql"
	"errors"
	"math"
	"time"

	"github.com/google/uuid"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
	"match/internal/authority"
	"match/internal/constants"
	"match/internal/discovery"
	"match/internal/metrics"
	"match/internal/pkg/ctxkeys"
	"match/internal/svc"
	battlepb "proto/battle"
	pb "proto/match"
	"shared/generated/pb/table"
	"shared/safego"
)

func globalJoin(l *JoinQueueLogic, in *pb.JoinQueueRequest, required, zone uint32) (*pb.JoinQueueResponse, error) {
	if parsed, e := uuid.Parse(in.RequestId); e != nil || parsed == uuid.Nil || parsed.String() != in.RequestId {
		return nil, status.Error(codes.InvalidArgument, "匹配请求编号无效")
	}
	if old, e := l.svcCtx.Authority.Ticket(l.ctx, authoritativePlayerID(l.ctx, in.PlayerId)); e != nil {
		return nil, e
	} else if old != nil && old.State == 3 {
		if _, e = releaseGlobalFinished(l.ctx, l.svcCtx, old); e != nil {
			return nil, e
		}
	}
	d, _ := ctxkeys.GetSessionDetails(l.ctx)
	binding := authority.SessionBinding(l.ctx, d)
	if binding == "" || in.RequestId == "" || len(in.RequestId) > 64 || in.NetworkPlanId == "" {
		return &pb.JoinQueueResponse{ErrorCode: constants.ErrInternal, ErrorMessage: tipErr(constants.ErrInternal, "请完成地域延迟探测后重试")}, nil
	}

	t := &pb.MatchAuthorityTicketRecord{PlayerId: d.PlayerId, Ticket: uuid.NewString(), RequestId: in.RequestId, Mode: uint32(in.Mode), ConfigId: in.BattleConfigId, TeamSize: required, ZoneId: zone, RatingCenti: int64(math.Round(loadRatingOrDefault(l.svcCtx, d.PlayerId) * 100))}
	prepared, exit, pending, e := l.svcCtx.Authority.PrepareJoin(l.ctx, t, in, binding)
	if e != nil {
		return &pb.JoinQueueResponse{ErrorCode: constants.ErrInternal, ErrorMessage: tipErr(constants.ErrInternal, "匹配原意图暂不可用")}, nil
	}
	if prepared.RequestId != in.RequestId {
		return &pb.JoinQueueResponse{QueueTicket: prepared.Ticket, ErrorCode: constants.ErrAlreadyQueued, ErrorMessage: tipErr(constants.ErrAlreadyQueued, "已在匹配队列中")}, nil
	}
	if pending && exit != nil {
		if e = closeCapturedSpectatorExit(l.ctx, l.svcCtx, exit); e != nil {
			return &pb.JoinQueueResponse{ErrorCode: constants.ErrInternal, ErrorMessage: tipErr(constants.ErrInternal, "原观战退出尚待确认，请复用原请求重试")}, nil
		}
	}
	t = prepared
	old, created, e := l.svcCtx.Authority.Join(l.ctx, t, in.NetworkPlanId, binding)
	if e != nil {
		return &pb.JoinQueueResponse{ErrorCode: constants.ErrInternal, ErrorMessage: tipErr(constants.ErrInternal, "匹配权威暂不可用或探测已过期")}, nil
	}
	metrics.ObserveGlobalFlow("join", "accepted")
	if !created && old.RequestId != in.RequestId {
		return &pb.JoinQueueResponse{QueueTicket: old.Ticket, ErrorCode: constants.ErrAlreadyQueued, ErrorMessage: tipErr(constants.ErrAlreadyQueued, "已在匹配队列中")}, nil
	}
	return &pb.JoinQueueResponse{QueueTicket: old.Ticket}, nil
}

type globalChoice struct {
	group        []*pb.MatchAuthorityTicketRecord
	node         discovery.NodeEntry
	region       uint32
	scannedUntil *pb.MatchAuthorityTicketRecord
}

func reserveGlobalDirect(ctx context.Context, s *svc.ServiceContext, key *pb.MatchDirectIntentRecord, payload []byte, mode pb.MatchMode, config uint32, members []uint64, tickets map[uint64]string, opts gatherOptions) (*pb.MatchFormationRecord, error) {
	if old, original, e := s.Authority.FindDirect(ctx, key); e != nil {
		return nil, e
	} else if old != nil {
		if !bytes.Equal(original, payload) {
			return nil, errors.New("相同业务意图的开局内容不同")
		}
		return old, nil
	}
	prepared, err := s.Authority.DirectAdmission(ctx, key)
	if err != nil {
		return nil, err
	}
	var f *pb.MatchFormationRecord
	var group []*pb.MatchAuthorityTicketRecord
	if prepared == nil || !prepared.ExitPrepared {
		f, group, err = buildGlobalDirect(ctx, s, mode, config, members, tickets, opts)
		if err != nil {
			return nil, err
		}
	}
	prepared, err = s.Authority.PrepareDirectAdmission(ctx, key, payload, f, group)
	if err != nil {
		return nil, err
	}
	return resumeGlobalDirectAdmission(ctx, s, prepared)
}

func buildGlobalDirect(ctx context.Context, s *svc.ServiceContext, mode pb.MatchMode, config uint32, members []uint64, tickets map[uint64]string, opts gatherOptions) (*pb.MatchFormationRecord, []*pb.MatchAuthorityTicketRecord, error) {
	n, e := s.Authority.Now(ctx)
	if e != nil {
		return nil, nil, e
	}
	lat := map[uint64]map[uint32]uint32{}
	profiles := map[uint64]*pb.MatchNetworkProfile{}
	for _, pid := range members {
		p, expires, e := s.Authority.Profile(ctx, pid)
		if e != nil || expires <= n {
			return nil, nil, errors.New("成员地域探测失效")
		}
		profiles[pid] = p
		lat[pid] = authority.Latencies(p, n, s.Config.GlobalMatch.HardLatencyMs)
	}
	var target discovery.NodeEntry
	bestMax, bestSum := uint32(math.MaxUint32), uint64(math.MaxUint64)
	for _, node := range s.BattleNodes.Snapshot() {
		if node.PhysicalRegion == 0 || node.NodeUuid == "" {
			continue
		}
		maxRTT := uint32(0)
		sum := uint64(0)
		eligible := true
		for _, pid := range members {
			rtt, ok := lat[pid][node.PhysicalRegion]
			if !ok {
				eligible = false
				break
			}
			maxRTT = max(maxRTT, rtt)
			sum += uint64(rtt)
		}
		if eligible && (maxRTT < bestMax || (maxRTT == bestMax && (sum < bestSum || (sum == bestSum && node.PhysicalRegion < target.PhysicalRegion)))) {
			target = node
			bestMax = maxRTT
			bestSum = sum
		}
	}
	if target.PhysicalRegion == 0 {
		return nil, nil, errors.New("没有满足全员硬延迟的共同机房")
	}
	id := opts.presetBattleID
	if id == 0 {
		id, e = s.BattleIDGen.Generate()
		if e != nil {
			return nil, nil, e
		}
	}
	f := &pb.MatchFormationRecord{BattleId: id, OperationId: uuid.NewString(), PhysicalRegion: target.PhysicalRegion, TargetNodeId: target.NodeId, TargetInstanceId: target.NodeUuid, TargetEndpoint: target.Endpoint, Mode: uint32(mode), ConfigId: config}
	if opts.activityContext != nil {
		f.ActivityContext, e = proto.Marshal(opts.activityContext)
		if e != nil {
			return nil, nil, e
		}
	}
	var group []*pb.MatchAuthorityTicketRecord
	for _, pid := range members {
		ticket := tickets[pid]
		if ticket == "" {
			ticket = uuid.NewString()
		}
		group = append(group, &pb.MatchAuthorityTicketRecord{PlayerId: pid, Ticket: ticket, RequestId: f.OperationId, Mode: uint32(mode), ConfigId: config, TeamSize: uint32(len(members)), TeamId: opts.directTeamID, OwnerAccount: profiles[pid].OwnerAccount, OwnerRevision: profiles[pid].OwnerRevision, RatingCenti: int64(math.Round(loadRatingOrDefault(s, pid) * 100))})
	}
	return f, group, nil
}

// 后台入口只能恢复已经在业务成功回包之前提交的成局，绝不现场再发一个 operation。
func globalDirectGather(s *svc.ServiceContext, mode pb.MatchMode, config uint32, members []uint64, tickets map[uint64]string, opts gatherOptions) bool {
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()
	var f *pb.MatchFormationRecord
	var e error
	if opts.presetBattleID != 0 {
		f, e = s.Authority.Formation(ctx, opts.presetBattleID)
	} else if len(members) > 0 {
		t, err := s.Authority.Ticket(ctx, members[0])
		e = err
		if err == nil && t != nil && t.FormationId != 0 && t.Ticket == tickets[members[0]] {
			f, e = s.Authority.Formation(ctx, t.FormationId)
		}
	}
	if e != nil || f == nil || f.Mode != uint32(mode) || f.ConfigId != config || len(f.Members) != len(members) {
		return false
	}
	for i, m := range f.Members {
		if m.PlayerId != members[i] || (len(tickets) > 0 && m.Ticket != tickets[m.PlayerId]) {
			return false
		}
	}
	if f.State == 4 {
		return true
	}
	if f.State == 5 {
		return false
	}
	runGlobalFormation(s, f)
	read, stop := context.WithTimeout(context.Background(), 3*time.Second)
	defer stop()
	f, e = s.Authority.Formation(read, f.BattleId)
	return e == nil && f.State == 4
}

func releaseGlobalFinished(ctx context.Context, s *svc.ServiceContext, t *pb.MatchAuthorityTicketRecord) (bool, error) {
	f, e := s.Authority.Formation(ctx, t.FormationId)
	if e != nil {
		return false, e
	}
	conn, e := discovery.DialEndpoint(f.TargetEndpoint)
	if e != nil {
		return false, nil
	}
	call, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()
	client := battlepb.NewBattleNodeClient(conn)
	query := &battlepb.QueryBattleCreationRequest{BattleId: f.BattleId, PlacementOperationId: f.OperationId, TargetInstanceId: f.TargetInstanceId}
	q, e := client.QueryBattleCreation(call, query)
	if e != nil {
		return false, nil
	}
	// ABSENT不是拒绝迟到Create的墓碑；先固定原operation的取消，再只认可ABORTED。
	if q.State == battlepb.BattleCreationState_BATTLE_CREATION_ABSENT {
		_, e = client.DestroyBattle(call, &battlepb.DestroyBattleRequest{BattleId: f.BattleId, PlacementOperationId: f.OperationId, TargetInstanceId: f.TargetInstanceId, Reason: "ready_recovery_original_cancel"})
		if e != nil {
			return false, nil
		}
		q, e = client.QueryBattleCreation(call, query)
		if e != nil {
			return false, nil
		}
	}
	if q.State != battlepb.BattleCreationState_BATTLE_CREATION_ABORTED {
		return false, nil
	}
	return true, s.Authority.ReleaseReady(ctx, t.PlayerId, t.Ticket, t.FormationId)
}

func runGlobalRound(ctx context.Context, s *svc.ServiceContext) {
	pending, e := s.Authority.Pending(ctx)
	if e != nil {
		logx.Error("全球匹配恢复查询失败")
		return
	}
	for _, f := range pending {
		launchGlobalFormation(s, f)
	}
	// 候选投影故障不能挡住已经提交的formation恢复。
	if !s.GlobalRegionRepairActive {
		throughMs, throughPlayer, err := s.Authority.CandidateRegionRepairHighWater(ctx)
		if err != nil {
			logx.Error("全球匹配候选投影尾界读取失败")
			return
		}
		s.GlobalRegionRepairThroughMs, s.GlobalRegionRepairThroughPlayer = throughMs, throughPlayer
		s.GlobalRegionRepairActive = true
	}
	repairMs, repairPlayer, repairErr := s.Authority.RepairCandidateRegionsThrough(ctx, s.GlobalRegionRepairMs, s.GlobalRegionRepairPlayer, s.GlobalRegionRepairThroughMs, s.GlobalRegionRepairThroughPlayer)
	if repairErr != nil {
		logx.Error("全球匹配候选投影维护失败")
		return
	}
	s.GlobalRegionRepairMs, s.GlobalRegionRepairPlayer = repairMs, repairPlayer
	if repairMs == 0 && repairPlayer == 0 {
		s.GlobalRegionProjectionReady.Store(true)
		s.GlobalRegionRepairActive = false
	}
	if s.GlobalGatherSlots != nil && len(s.GlobalGatherSlots) == cap(s.GlobalGatherSlots) {
		return
	}
	all, e := s.Authority.CandidatesAfter(ctx, s.GlobalScanMs, s.GlobalScanPlayer)
	if e != nil {
		logx.Error("全球匹配候选读取失败")
		return
	}
	if len(all) == 0 {
		s.GlobalScanMs = 0
		s.GlobalScanPlayer = 0
		return
	}
	n, e := s.Authority.Now(ctx)
	if e != nil {
		return
	}
	choice, e := pickIndexedGlobalGroup(ctx, s, all, s.BattleNodes.Snapshot(), n, s.GlobalRegionProjectionReady.Load(), s.Authority.CandidatePeers)
	if e != nil {
		logx.Error("全球匹配机房候选读取失败")
		return
	}
	last := all[len(all)-1]
	if choice != nil && choice.scannedUntil != nil {
		last = choice.scannedUntil
	}
	s.GlobalScanMs, s.GlobalScanPlayer = last.EnqueuedMs, last.PlayerId
	if choice == nil {
		return
	}
	id, e := s.BattleIDGen.Generate()
	if e != nil {
		return
	}
	a := choice.group[0]
	f := &pb.MatchFormationRecord{BattleId: id, OperationId: uuid.NewString(), PhysicalRegion: choice.region, TargetNodeId: choice.node.NodeId, TargetInstanceId: choice.node.NodeUuid, TargetEndpoint: choice.node.Endpoint, Mode: a.Mode, ConfigId: a.ConfigId, RequeueOnFailure: true}
	if e = s.Authority.Claim(ctx, f, choice.group); e != nil {
		metrics.ObserveGlobalFlow("claim", "conflict")
		return
	}
	metrics.ObserveGlobalFlow("claim", "committed")
	launchGlobalFormation(s, f)
}

func launchGlobalFormation(s *svc.ServiceContext, f *pb.MatchFormationRecord) {
	if s.GlobalGatherSlots == nil {
		runGlobalFormation(s, f)
		return
	}
	select {
	case s.GlobalGatherSlots <- struct{}{}:
	default:
		return
	}
	safego.Go("match.global.gather", func() { defer func() { <-s.GlobalGatherSlots }(); runGlobalFormation(s, f) })
}

func runGlobalFormation(s *svc.ServiceContext, f *pb.MatchFormationRecord) {
	metrics.ObserveGlobalFlow("recover", "attempt")
	ctx, cancel := context.WithTimeout(context.Background(), 110*time.Second)
	defer cancel()
	var acquireErr error
	f, acquireErr = s.Authority.AcquireWork(ctx, f.BattleId)
	if acquireErr != nil {
		return
	}
	if f.State == 1 {
		members := make([]uint64, 0, len(f.Members))
		tickets := map[uint64]string{}
		for _, m := range f.Members {
			members = append(members, m.PlayerId)
			tickets[m.PlayerId] = m.Ticket
		}
		var actx *battlepb.BattleActivityContext
		if len(f.ActivityContext) > 0 {
			actx = &battlepb.BattleActivityContext{}
			if proto.Unmarshal(f.ActivityContext, actx) != nil {
				return
			}
		}
		runGatherWithOptions(s, pb.MatchMode(f.Mode), f.ConfigId, members, false, true, tickets, gatherOptions{presetBattleID: f.BattleId, formation: f, activityContext: actx})
		return
	}
	if len(f.CreateRequest) == 0 {
		if globalDestroy(s, f) {
			globalAbortPrepared(s, f)
		}
		return
	}
	conn, e := discovery.DialEndpoint(f.TargetEndpoint)
	if e != nil {
		_ = s.Authority.Unknown(ctx, f)
		return
	}
	client := battlepb.NewBattleNodeClient(conn)
	qctx, qcancel := context.WithTimeout(ctx, 3*time.Second)
	result, e := client.QueryBattleCreation(qctx, &battlepb.QueryBattleCreationRequest{BattleId: f.BattleId, PlacementOperationId: f.OperationId, TargetInstanceId: f.TargetInstanceId})
	qcancel()
	if e != nil || result.State == battlepb.BattleCreationState_BATTLE_CREATION_UNKNOWN {
		metrics.ObserveGlobalFlow("recover", "unknown")
		_ = s.Authority.Unknown(ctx, f)
		return
	}
	if result.State == battlepb.BattleCreationState_BATTLE_CREATION_PRESENT {
		metrics.ObserveGlobalFlow("recover", "present")
		_ = s.Authority.Finish(ctx, f, true)
		return
	}
	if result.State == battlepb.BattleCreationState_BATTLE_CREATION_ABORTED {
		globalAbortPrepared(s, f)
		return
	}
	request := &battlepb.CreateBattleRequest{}
	if proto.Unmarshal(f.CreateRequest, request) != nil {
		_ = s.Authority.Unknown(ctx, f)
		return
	}
	n, e := s.Authority.Now(ctx)
	if e != nil {
		return
	}
	if n < request.AdmitBeforeMs {
		if e = createBattle(discovery.NodeEntry{NodeId: f.TargetNodeId, NodeUuid: f.TargetInstanceId, Endpoint: f.TargetEndpoint, PhysicalRegion: f.PhysicalRegion}, request); e == nil {
			_ = s.Authority.Finish(ctx, f, true)
			return
		}
	}
	if globalDestroy(s, f) {
		globalAbortPrepared(s, f)
	} else {
		_ = s.Authority.Unknown(ctx, f)
	}
}

func globalDestroy(s *svc.ServiceContext, f *pb.MatchFormationRecord) bool {
	check, stop := context.WithTimeout(context.Background(), 3*time.Second)
	err := s.Authority.CheckWork(check, f)
	stop()
	if err != nil {
		return false
	}
	conn, e := discovery.DialEndpoint(f.TargetEndpoint)
	if e != nil {
		return false
	}
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()
	_, e = battlepb.NewBattleNodeClient(conn).DestroyBattle(ctx, &battlepb.DestroyBattleRequest{BattleId: f.BattleId, Reason: "global_match_abort_if_absent", PlacementOperationId: f.OperationId, TargetInstanceId: f.TargetInstanceId})
	if e != nil {
		return false
	}
	q, e := battlepb.NewBattleNodeClient(conn).QueryBattleCreation(ctx, &battlepb.QueryBattleCreationRequest{BattleId: f.BattleId, PlacementOperationId: f.OperationId, TargetInstanceId: f.TargetInstanceId})
	if e != nil {
		return false
	}
	if q.State == battlepb.BattleCreationState_BATTLE_CREATION_PRESENT {
		_ = s.Authority.Finish(ctx, f, true)
	}
	return q.State == battlepb.BattleCreationState_BATTLE_CREATION_ABORTED
}

// 全球模式补签只读持久创建意图，Redis投影丢失不构成“战斗已结束”的证据。
func globalRequestBattleTicket(ctx context.Context, s *svc.ServiceContext, pid, id uint64) (*battlepb.RequestBattleTicketResponse, error) {
	observerOperation, authErr := s.Authority.AuthorizeBattleTicket(ctx, pid, id)
	if authErr != nil {
		return nil, authErr
	}
	f, e := s.Authority.Formation(ctx, id)
	if errors.Is(e, sql.ErrNoRows) {
		return &battlepb.RequestBattleTicketResponse{ErrorMessage: tipErr(uint32(table.CommonError_kInvalidParameter), "该战斗不存在或已结束")}, nil
	}
	if e != nil {
		return nil, e
	}
	// battle 自己验证参战者和已登记观众，SQL 落点不依赖 Redis 展示索引。
	if f.State == 5 {
		return &battlepb.RequestBattleTicketResponse{ErrorMessage: tipErr(uint32(table.CommonError_kInvalidParameter), "该战斗不存在或已结束")}, nil
	}
	conn, e := discovery.DialEndpoint(f.TargetEndpoint)
	if e != nil {
		return nil, e
	}
	qctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()
	client := battlepb.NewBattleNodeClient(conn)
	q, e := client.QueryBattleCreation(qctx, &battlepb.QueryBattleCreationRequest{BattleId: id, PlacementOperationId: f.OperationId, TargetInstanceId: f.TargetInstanceId})
	if e != nil {
		return nil, e
	}
	if q.State == battlepb.BattleCreationState_BATTLE_CREATION_UNKNOWN || f.State < 2 {
		return &battlepb.RequestBattleTicketResponse{ErrorMessage: tipErr(constants.ErrInternal, "战斗创建结果正在恢复，请稍后重试")}, nil
	}
	if q.State != battlepb.BattleCreationState_BATTLE_CREATION_PRESENT {
		if f.State != 4 && q.State != battlepb.BattleCreationState_BATTLE_CREATION_ABORTED {
			return &battlepb.RequestBattleTicketResponse{ErrorMessage: tipErr(constants.ErrInternal, "战斗尚未确认，请稍后重试")}, nil
		}
		return &battlepb.RequestBattleTicketResponse{ErrorMessage: tipErr(uint32(table.CommonError_kInvalidParameter), "该战斗不存在或已结束")}, nil
	}
	r, e := client.IssueBattleTicket(qctx, &battlepb.IssueBattleTicketRequest{BattleId: id, PlayerId: pid, SpectatorOperationId: observerOperation, TargetInstanceId: f.TargetInstanceId})
	if e != nil {
		return nil, e
	}
	return &battlepb.RequestBattleTicketResponse{ErrorMessage: r.ErrorMessage, Assignment: r.Assignment}, nil
}
func globalAbortPrepared(s *svc.ServiceContext, f *pb.MatchFormationRecord) {
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	latest, e := s.Authority.Formation(ctx, f.BattleId)
	cancel()
	if e != nil || latest.RetryAtMs != f.RetryAtMs {
		return
	}
	f = latest
	check, stop := context.WithTimeout(context.Background(), 3*time.Second)
	e = s.Authority.CheckWork(check, f)
	stop()
	if e != nil {
		return
	}
	var prepared []preparedMember
	for _, m := range f.Prepared {
		prepared = append(prepared, preparedMember{playerId: m.PlayerId, sceneEndpoint: m.Endpoint})
	}
	cancelPrepared(s, prepared, f.BattleId)
	finishCtx, finishCancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer finishCancel()
	if e := s.Authority.Finish(finishCtx, f, false); e != nil {
		logx.Error("全球匹配中止提交未确认")
	}
}
