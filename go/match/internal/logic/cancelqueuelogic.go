package logic

import (
	"context"
	"github.com/google/uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"match/internal/constants"
	"match/internal/svc"

	base "proto/common/base"
	matchpb "proto/match"

	"github.com/zeromicro/go-zero/core/logx"
)

type CancelQueueLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewCancelQueueLogic(ctx context.Context, svcCtx *svc.ServiceContext) *CancelQueueLogic {
	return &CancelQueueLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

// CancelQueue 取消排队。语义:
//   - 无 ticket:幂等成功(客户端重复取消 / ticket 已被开局收尾);
//   - ticket 不匹配:视为客户端持有陈旧票据,不动当前排队,幂等返回;
//   - 已进入 matched 及之后状态:取消太迟,凑单已在途,不可撤(此时
//     开局失败的补偿路径会把人送回队首或删票,不需要客户端参与);
//   - queued 态:先删 ticket 再 Lrem 出队。顺序有意如此 —— matcher 弹出
//     后会校验 ticket 存在性,先删票保证并发弹出方一定能识别出已取消。
//     删票是带 ticket id 且要求仍为 queued 的 Lua CAS(ticketCancelScript):
//     读票与删票之间 matcher 可能已把他弹出推进 matched,无条件 DEL 会让
//     "取消太迟"只停留在 Go 侧的一次读,玩家在"取消成功"后仍被冻结进战斗。
//   - 队伍补位票(票据带 party_id):本人取消成功后整单一起退出(cancelParty)。
func (l *CancelQueueLogic) CancelQueue(in *matchpb.CancelQueueRequest) (*base.Empty, error) {
	playerId := authoritativePlayerID(l.ctx, in.PlayerId)
	if playerId == 0 {
		return &base.Empty{}, nil
	}

	if l.svcCtx.Authority != nil {
		if in.RequestId != "" {
			if parsed, e := uuid.Parse(in.RequestId); e != nil || parsed == uuid.Nil || parsed.String() != in.RequestId {
				return nil, status.Error(codes.InvalidArgument, "匹配请求编号无效")
			}
		}
		if in.RequestId == "" && in.QueueTicket == "" {
			return nil, status.Error(codes.InvalidArgument, "撤销需要原请求编号或票据")
		}
		if in.RequestId != "" {
			launch, e := l.svcCtx.Authority.TeamLaunch(l.ctx, playerId, in.RequestId)
			if e != nil {
				return nil, e
			}
			if launch != nil {
				members, e := l.svcCtx.Authority.CancelTeamLaunchForActor(l.ctx, playerId, launch)
				if e == nil && len(members) > 0 {
					if port := l.svcCtx.PartyTeam(); port != nil {
						port.PartyEnded(launch.TeamId, members, constants.PartyEndCanceled, playerId)
					}
				}
				return &base.Empty{}, e
			}
		}
		ticket, e := l.svcCtx.Authority.Ticket(l.ctx, playerId)
		if e != nil {
			return nil, e
		}
		if ticket != nil && ticket.TeamId != 0 && (in.QueueTicket == "" || in.QueueTicket == ticket.Ticket) && (in.RequestId == "" || in.RequestId == ticket.RequestId) {
			members, e := l.svcCtx.Authority.CancelTeamQueueForActor(l.ctx, playerId, ticket)
			if e == nil && len(members) > 0 {
				if port := l.svcCtx.PartyTeam(); port != nil {
					port.PartyEnded(ticket.TeamId, members, constants.PartyEndCanceled, playerId)
				}
			}
			return &base.Empty{}, e
		}
		return &base.Empty{}, l.svcCtx.Authority.CancelIntent(l.ctx, playerId, in.QueueTicket, in.RequestId)
	}
	ticket, err := loadTicket(l.svcCtx, playerId)
	if err != nil {
		l.Errorf("[match] CancelQueue failed to read ticket player=%d: %v", playerId, err)
		return nil, err
	}
	if ticket == nil {
		// 幂等:没有在途排队,直接成功。
		return &base.Empty{}, nil
	}
	if in.QueueTicket != "" && in.QueueTicket != ticket.Ticket {
		l.Infof("[match] CancelQueue ticket mismatch; ignoring player=%d req=%s cur=%s",
			playerId, in.QueueTicket, ticket.Ticket)
		return &base.Empty{}, nil
	}
	if ticket.State != ticketStateQueued {
		// matched/ready:凑单或开局已在途,取消太迟。开局失败自有补偿路径。
		l.Infof("[match] CancelQueue too late, state=%s player=%d", ticket.State, playerId)
		return &base.Empty{}, nil
	}

	// 删票与出队都用请求 ctx:gate 对同一会话单飞,路由服回超时就放行下一条请求;截止之后才落地的
	// 删票 / 出队会晚于下一条 JoinQueue 生效,把新入队的玩家摘掉(docs/design/client-request-ordering.md §6.1)。
	deleted, err := cancelTicketIfQueuedCtx(l.ctx, l.svcCtx, playerId, ticket.Ticket)
	if err != nil {
		l.Errorf("[match] CancelQueue failed to delete ticket player=%d: %v", playerId, err)
		return nil, err
	}
	if !deleted {
		// 读票之后被 matcher 弹出(matched)或已被替换:取消太迟,凑单在途。
		l.Infof("[match] CancelQueue too late; ticket was popped or replaced after read player=%d ticket=%s", playerId, ticket.Ticket)
		return &base.Empty{}, nil
	}
	if ticket.PartyId != "" {
		// 队伍补位票(team-backfill-matching.md §4.3):任何一名队员取消 = 整单退出匹配。自己的票已经按上面的
		// queued CAS 删掉(「取消太迟」的判定仍落在本人票据上),这里撤其余队员的票、把全单从 list 摘掉并通知 team。
		// 与弹组并发:matcher 推进 matched 时会发现本人票据缺失,整单不进 gather(requeueFront 的完整性规则)。
		// 撤其余人之前先登记原因:此刻 matcher 可能正扫到这一单(自己的票已删 → 判不完整),或正在对这一单
		// 逐人推进 matched(队友的票已 matched → 取消对他「太迟」,由 matcher 回队首时收尾并通知)。
		// 不论最后由谁通知,全队看到的都是「某某取消了匹配」,而不是「队员未就绪」。
		claimPartyEndReason(l.svcCtx, ticket.PartyId, constants.PartyEndCanceled, playerId)
		members := cancelParty(l.svcCtx, ticket, constants.PartyEndCanceled, playerId)
		l.Infof("[match] Party backfill cancelled by member player=%d team=%d config=%d queue=%s members=%v",
			playerId, ticket.TeamId, ticket.Config, ticket.QueueKey, members)
		return &base.Empty{}, nil
	}
	// 出队用票据里记录的 QueueKey(决策 D4,见 ticketQueueKey):list 与评分镜像 ZSET
	// 一条同 slot Lua 原子摘出(§11),集群安全。
	queueKey := ticketQueueKey(ticket)
	if err := dequeueMemberCtx(l.ctx, l.svcCtx, queueKey, playerId); err != nil {
		// 出队失败留下的残留队列项会被 matcher 的票据校验丢弃,只记日志。
		l.Errorf("[match] CancelQueue failed to dequeue (matcher ticket validation will handle remnants) player=%d queue=%s: %v",
			playerId, queueKey, err)
	}
	l.Infof("[match] Queue cancellation succeeded player=%d mode=%d config=%d queue=%s",
		playerId, ticket.Mode, ticket.Config, queueKey)
	return &base.Empty{}, nil
}
