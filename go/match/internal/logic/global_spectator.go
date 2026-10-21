package logic

import (
	"context"
	"database/sql"
	"errors"
	"strconv"
	"sync"
	"time"

	"github.com/google/uuid"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"match/internal/authority"
	"match/internal/constants"
	"match/internal/discovery"
	"match/internal/pkg/ctxkeys"
	"match/internal/svc"
	battlepb "proto/battle"
	pb "proto/match"
	"shared/safego"
)

func globalWatchBattle(l *WatchBattleLogic, in *pb.WatchBattleRequest, player uint64) (*pb.WatchBattleResponse, error) {
	fail := func() (*pb.WatchBattleResponse, error) {
		return &pb.WatchBattleResponse{ErrorMessage: tipErr(constants.ErrInternal, "观战原请求尚未确认，请稍后重试")}, nil
	}
	id, err := uuid.Parse(in.RequestId)
	if err != nil || id == uuid.Nil || id.String() != in.RequestId || (in.PlayerId != 0 && in.PlayerId != player) {
		return fail()
	}
	// 在读取current、清退旧op或选择随机战场之前，占定不可回退的原请求头。
	intent, err := l.svcCtx.Authority.SpectatorIntent(l.ctx, player, in)
	if err != nil {
		return fail()
	}
	if intent.State == 5 {
		original := &pb.MatchSpectatorRecord{}
		if len(intent.Original) != 0 && proto.Unmarshal(intent.Original, original) != nil {
			return fail()
		}
		return &pb.WatchBattleResponse{BattleId: original.BattleId, SpectatorOperationId: in.RequestId, ErrorMessage: tipErr(constants.ErrBattleNotWatchable, "原观战请求已关闭")}, nil
	}
	var original *pb.MatchSpectatorRecord
	if intent.State == 2 {
		original = &pb.MatchSpectatorRecord{}
		if err = proto.Unmarshal(intent.Original, original); err != nil {
			return fail()
		}
	} else {
		current, err := l.svcCtx.Authority.SpectatorForActor(l.ctx, player, in.RequestId)
		if err != nil {
			return fail()
		}
		if current != nil && current.State != 5 {
			if err = cancelGlobalSpectator(l.ctx, l.svcCtx, current, in.RequestId); err != nil {
				return fail()
			}
		}
		battle := in.BattleId
		if battle == 0 {
			battle, err = pickRandomBattle(l.svcCtx)
			if err != nil {
				return fail()
			}
		}
		if battle == 0 {
			if err = l.svcCtx.Authority.CloseUnpreparedSpectator(l.ctx, player, in); err != nil {
				return fail()
			}
			return &pb.WatchBattleResponse{SpectatorOperationId: in.RequestId, ErrorMessage: tipErr(constants.ErrNoWatchableBattle, "当前没有可观战的战斗")}, nil
		}
		formation, err := l.svcCtx.Authority.Formation(l.ctx, battle)
		if errors.Is(err, sql.ErrNoRows) || err == nil && formation.State != 4 {
			if err = l.svcCtx.Authority.CloseUnpreparedSpectator(l.ctx, player, in); err != nil {
				return fail()
			}
			return &pb.WatchBattleResponse{SpectatorOperationId: in.RequestId, ErrorMessage: tipErr(constants.ErrBattleNotWatchable, "该战斗当前无法观战")}, nil
		}
		if err != nil {
			return fail()
		}
		session, err := loadPlayerSession(l.svcCtx, player)
		if err != nil || !isSessionOnline(session) {
			return fail()
		}
		detail, present := ctxkeys.GetSessionDetails(l.ctx)
		if !present || detail == nil || detail.SessionId != session.SessionId || detail.GateInstanceId != session.GateInstanceId {
			return fail()
		}
		gate, err := strconv.ParseUint(session.GateId, 10, 32)
		if err != nil || gate == 0 || uint32(gate) != detail.GateNodeId {
			return fail()
		}
		// 公共显示名只来自角色资料，原登录账号不进入观众广播。
		name := challengePublicName(l.ctx, l.svcCtx, player)
		original, err = l.svcCtx.Authority.BeginSpectator(l.ctx, player, in, formation, &battlepb.AddObserverRequest{
			ObserverName: name, Routing: &battlepb.BattleRouting{SessionId: session.SessionId, GateNodeId: uint32(gate), GateInstanceId: session.GateInstanceId, ZoneId: l.observerZoneId(player)},
		})
		if err != nil {
			return fail()
		}
	}
	if err = resumeGlobalSpectator(l.ctx, l.svcCtx, original, in.RequestId); err != nil {
		return fail()
	}
	current, err := l.svcCtx.Authority.SpectatorForActor(l.ctx, player, in.RequestId)
	if err != nil || current == nil || current.OperationId != original.OperationId || current.State != 2 {
		return fail()
	}
	return &pb.WatchBattleResponse{BattleId: current.BattleId, SpectatorOperationId: current.OperationId}, nil
}

func advanceGlobalSpectator(ctx context.Context, s *svc.ServiceContext, original *pb.MatchSpectatorRecord, state uint32, expectedHead string) (bool, error) {
	if expectedHead != "" {
		return s.Authority.AdvanceSpectatorForHead(ctx, original, state, expectedHead)
	}
	return s.Authority.AdvanceSpectator(ctx, original, state)
}

// 调用者已有同业务永久意图绑定的原退出列表；不读取或清退现在的另一个watch。
func closeCapturedSpectatorExit(ctx context.Context, s *svc.ServiceContext, original *pb.MatchSpectatorRecord) error {
	closed, err := s.Authority.SpectatorExitClosed(ctx, original)
	if err != nil {
		return err
	}
	if closed {
		return nil
	}
	return cancelGlobalSpectator(ctx, s, original, "")
}

// 前台传本次新请求head；后台只推进原op。Remove必须固定原截止/实例/op，正证明才关闭SQL。
func cancelGlobalSpectator(ctx context.Context, s *svc.ServiceContext, original *pb.MatchSpectatorRecord, expectedHead string) error {
	if original.State == 5 {
		return nil
	}
	ok, err := advanceGlobalSpectator(ctx, s, original, 3, expectedHead)
	if err != nil {
		return err
	}
	if !ok {
		return authority.ErrSpectatorBusy
	}
	add := &battlepb.AddObserverRequest{}
	if err = proto.Unmarshal(original.AddRequest, add); err != nil {
		return err
	}
	conn, err := discovery.DialEndpoint(original.TargetEndpoint)
	if err != nil {
		return err
	}
	reply, err := battlepb.NewBattleNodeClient(conn).RemoveObserver(ctx, &battlepb.RemoveObserverRequest{
		BattleId: original.BattleId, ObserverPlayerId: original.PlayerId, Reason: "global_original_cancel", SpectatorOperationId: original.OperationId, AdmitBeforeMs: add.AdmitBeforeMs, TargetInstanceId: original.TargetInstanceId,
	})
	if err != nil {
		return err
	}
	if reply == nil || !reply.Cancelled || reply.SpectatorOperationId != original.OperationId || reply.TargetInstanceId != original.TargetInstanceId {
		return authority.ErrSpectatorBusy
	}
	ok, err = advanceGlobalSpectator(ctx, s, original, 5, expectedHead)
	if err != nil {
		return err
	}
	if !ok {
		return authority.ErrSpectatorBusy
	}
	return nil
}

func resumeGlobalSpectator(ctx context.Context, s *svc.ServiceContext, original *pb.MatchSpectatorRecord, expectedHead string) error {
	if expectedHead == "" {
		current, superseded, err := s.Authority.SpectatorRecoveryCurrent(ctx, original)
		if err != nil {
			return err
		}
		original = current
		if original.State != 5 && superseded {
			return cancelGlobalSpectator(ctx, s, original, "")
		}
	}
	if original.State == 5 {
		return nil
	}
	add := &battlepb.AddObserverRequest{}
	if err := proto.Unmarshal(original.AddRequest, add); err != nil {
		return err
	}
	if original.State == 3 {
		return cancelGlobalSpectator(ctx, s, original, expectedHead)
	}
	conn, err := discovery.DialEndpoint(original.TargetEndpoint)
	if err != nil {
		return err
	}
	client := battlepb.NewBattleNodeClient(conn)
	if original.State == 2 {
		reply, err := client.QueryObserver(ctx, &battlepb.QueryObserverRequest{BattleId: original.BattleId, ObserverPlayerId: original.PlayerId, SpectatorOperationId: original.OperationId, TargetInstanceId: original.TargetInstanceId})
		if err != nil {
			return err
		}
		if reply == nil || reply.SpectatorOperationId != original.OperationId || reply.TargetInstanceId != original.TargetInstanceId {
			return authority.ErrSpectatorBusy
		}
		switch reply.State {
		case battlepb.ObserverAdmissionState_OBSERVER_ADMISSION_ACTIVE:
			return nil
		case battlepb.ObserverAdmissionState_OBSERVER_ADMISSION_ABSENT:
			return cancelGlobalSpectator(ctx, s, original, expectedHead)
		default:
			return authority.ErrSpectatorBusy
		}
	}
	stamp, err := s.Authority.Now(ctx)
	if err != nil {
		return err
	}
	if stamp >= add.AdmitBeforeMs {
		return cancelGlobalSpectator(ctx, s, original, expectedHead)
	}
	reply, err := client.AddObserver(ctx, add)
	if err != nil {
		// ACK未知保留相同Add/截止，不能清SQL占用；UNKNOWN本身也在恢复索引中。
		if _, persistErr := advanceGlobalSpectator(ctx, s, original, 4, expectedHead); persistErr != nil && !errors.Is(persistErr, authority.ErrSpectatorBusy) {
			logx.Error("全局观战ACK未知，原SQL占用仍保留")
		}
		return err
	}
	if reply == nil || reply.GetErrorMessage().GetId() != 0 {
		return cancelGlobalSpectator(ctx, s, original, expectedHead)
	}
	accepted, err := advanceGlobalSpectator(ctx, s, original, 2, expectedHead)
	if err != nil {
		return err
	}
	if !accepted {
		return authority.ErrSpectatorBusy
	}
	return nil
}

// 固定高水位覆盖旧占用；每状态索引页合并、两在途许可、单操作3秒，未知继续游标。
// 网络调用在SQL事务之外；新输入不会延长当前尾界，也不占撮合工作槽。
func StartGlobalSpectatorRecovery(ctx context.Context, s *svc.ServiceContext) {
	if s.Authority == nil {
		return
	}
	startGlobalDirectAdmissionRecovery(ctx, s)
	startGlobalJoinAdmissionRecovery(ctx, s)
	safego.Go("match.global_spectator_recovery", func() {
		ticker := time.NewTicker(500 * time.Millisecond)
		defer ticker.Stop()
		var after, through uint64
		active := false
		slots := make(chan struct{}, 2)
		var lastWarning time.Time
		var operationWarningMu sync.Mutex
		var lastOperationWarning time.Time
		for {
			select {
			case <-ctx.Done():
				return
			case <-ticker.C:
			}
			if len(slots) == cap(slots) {
				continue
			}
			work, cancel := context.WithTimeout(ctx, 3*time.Second)
			if !active {
				var err error
				through, err = s.Authority.SpectatorRecoveryHighWater(work)
				if err != nil {
					cancel()
					if time.Since(lastWarning) >= 10*time.Second {
						lastWarning = time.Now()
						logx.Error("全局观战恢复高水位读取失败")
					}
					continue
				}
				after, active = 0, true
			}
			rows, err := s.Authority.SpectatorsThrough(work, after, through)
			cancel()
			if err != nil {
				if time.Since(lastWarning) >= 10*time.Second {
					lastWarning = time.Now()
					logx.Error("全局观战恢复读取失败")
				}
				continue
			}
			if len(rows) == 0 {
				active = false
				continue
			}
			for _, row := range rows {
				if len(slots) == cap(slots) {
					break
				}
				slots <- struct{}{}
				after = row.PlayerId
				original := row
				safego.Go("match.global_spectator_operation", func() {
					defer func() { <-slots }()
					work, cancel := context.WithTimeout(ctx, 3*time.Second)
					defer cancel()
					// 原op CAS允许在新head之后收尾旧取消，但绝不推进别的current。
					if err := resumeGlobalSpectator(work, s, original, ""); err != nil {
						operationWarningMu.Lock()
						if time.Since(lastOperationWarning) >= 10*time.Second {
							lastOperationWarning = time.Now()
							logx.Info("全局观战原件尚未获得终局证明")
						}
						operationWarningMu.Unlock()
					}
				})
			}
		}
	})
}
