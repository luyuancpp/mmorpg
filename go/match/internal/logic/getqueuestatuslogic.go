package logic

import (
	"context"
	"github.com/google/uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"match/internal/svc"

	matchpb "proto/match"

	"github.com/zeromicro/go-zero/core/logx"
)

type GetQueueStatusLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewGetQueueStatusLogic(ctx context.Context, svcCtx *svc.ServiceContext) *GetQueueStatusLogic {
	return &GetQueueStatusLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

// GetQueueStatus 查询排队状态。QueueState 五态映射:
//
//	ticket 不存在            -> QUEUE_STATE_NOT_QUEUED
//	state=queued             -> QUEUE_STATE_QUEUED
//	state=matched            -> QUEUE_STATE_MATCHED(gather 管线执行中)
//	state=ready              -> QUEUE_STATE_READY(战斗已建,等 BattleStartS2C)
//	QUEUE_STATE_ENTERING     -> 场景类匹配预留的进场态,回合制(含二期 5v5)
//	                            无进场步骤,不产生;保留枚举以兼容场景匹配。
func (l *GetQueueStatusLogic) GetQueueStatus(in *matchpb.GetQueueStatusRequest) (*matchpb.GetQueueStatusResponse, error) {
	playerId := authoritativePlayerID(l.ctx, in.PlayerId)
	if playerId == 0 {
		return &matchpb.GetQueueStatusResponse{
			State: matchpb.QueueState_QUEUE_STATE_NOT_QUEUED,
		}, nil
	}

	if l.svcCtx.Authority != nil {
		if in.RequestId != "" {
			if parsed, e := uuid.Parse(in.RequestId); e != nil || parsed == uuid.Nil || parsed.String() != in.RequestId {
				return nil, status.Error(codes.InvalidArgument, "匹配请求编号无效")
			}
		}
		cancelled, e := l.svcCtx.Authority.RequestCancelled(l.ctx, playerId, in.RequestId)
		if e != nil {
			return nil, e
		}
		t, e := l.svcCtx.Authority.Ticket(l.ctx, playerId)
		if e != nil {
			return nil, e
		}
		if t != nil && t.State == 3 {
			released, e := releaseGlobalFinished(l.ctx, l.svcCtx, t)
			if e != nil {
				return nil, e
			}
			if released {
				t.State = 4
			}
		}
		if t == nil || t.State == 0 || t.State == 4 {
			return &matchpb.GetQueueStatusResponse{State: matchpb.QueueState_QUEUE_STATE_NOT_QUEUED, RequestCancelled: cancelled}, nil
		}
		n, e := l.svcCtx.Authority.Now(l.ctx)
		if e != nil {
			return nil, e
		}
		state := matchpb.QueueState_QUEUE_STATE_MATCHED
		if t.State == 1 {
			state = matchpb.QueueState_QUEUE_STATE_QUEUED
		} else if t.State == 3 {
			state = matchpb.QueueState_QUEUE_STATE_READY
		}
		return &matchpb.GetQueueStatusResponse{State: state, QueueTicket: t.Ticket, BattleId: t.FormationId, SelectedRegion: t.SelectedRegion, QueuedSeconds: uint32((n - min(n, t.EnqueuedMs)) / 1000), RetryAfterMs: 3000, RequestCancelled: cancelled}, nil
	}
	ticket, err := loadTicket(l.svcCtx, playerId)
	if err != nil {
		l.Errorf("[match] GetQueueStatus failed to read ticket player=%d: %v", playerId, err)
		return nil, err
	}
	if ticket == nil {
		return &matchpb.GetQueueStatusResponse{
			State: matchpb.QueueState_QUEUE_STATE_NOT_QUEUED,
		}, nil
	}

	state := matchpb.QueueState_QUEUE_STATE_UNSPECIFIED
	switch ticket.State {
	case ticketStateQueued:
		state = matchpb.QueueState_QUEUE_STATE_QUEUED
	case ticketStateMatched:
		state = matchpb.QueueState_QUEUE_STATE_MATCHED
	case ticketStateReady:
		state = matchpb.QueueState_QUEUE_STATE_READY
	default:
		l.Errorf("[match] Unknown ticket state %q player=%d; returning NOT_QUEUED", ticket.State, playerId)
		state = matchpb.QueueState_QUEUE_STATE_NOT_QUEUED
	}

	var queuedSeconds uint32
	if ticket.EnqueuedAtMs > 0 {
		if elapsed := nowMs() - ticket.EnqueuedAtMs; elapsed > 0 {
			queuedSeconds = uint32(elapsed / 1000)
		}
	}

	return &matchpb.GetQueueStatusResponse{
		State: state,
		// 一期不做等待时长预估(需要历史凑单速率统计,二期随负载上报一起做)。
		EstimatedWaitSeconds: 0,
		QueuedSeconds:        queuedSeconds,
	}, nil
}
