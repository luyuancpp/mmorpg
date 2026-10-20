package main

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"time"

	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"

	"proto/common/base"
	"robot/metrics"
	"robot/pkg"

	tiptable "shared/generated/pb/table"
)

// smokeTipServiceUnavailable 是 gate 选不到路由服时以 SendTipToClient 推下来的码。
var smokeTipServiceUnavailable = uint32(tiptable.CommonError_kServiceUnavailable)

// errSmokeGateUnavailable 是 smokeCaller 因 gate 推 kServiceUnavailable 而中止等待的原因。
var errSmokeGateUnavailable = errors.New("gate SendTipToClient kServiceUnavailable")

// smokeCaller 是各冒烟场景共用的「发一个请求、只等它自己的回包」:
// 走 pkg.GameClient.CallSync,回包按 MessageContent.id 认领(docs/design/client-rpc-callback.md §6)。
//
// 改造前各场景各自按消息号认领「本次请求发出后的第一个同号回包」,上一次超时请求的迟到回包会被
// 当成本次结果(friend / mail / rank 靠 awaiting 标记只兜住一部分,其余没有)。现在迟到回包由 GameClient
// 计数后丢弃,不再进场景的 onMessage。
//
// 一个 smokeCaller 同一时刻只服务一次等待,与场景「一个机器人只由一个 goroutine 驱动」的约定一致。
type smokeCaller struct {
	gc      *pkg.GameClient
	stats   *metrics.Stats
	account string
	timeout time.Duration
	// timeoutErr 是场景自己的超时哨兵(errGuildSmokeTimeout 等):断言「请求没回包」的步骤按它 errors.Is。
	timeoutErr error

	mu    sync.Mutex
	abort context.CancelCauseFunc // 当前这次等待;nil = 没在等
}

// newSmokeCaller 构造一个 smokeCaller。stats 可为 nil(不计数,战斗直连等路径的 stats 本就可选);
// timeoutErr 为 nil 时超时包裹 pkg.ErrRPCTimeout,场景没有自己的超时哨兵时用。
func newSmokeCaller(gc *pkg.GameClient, stats *metrics.Stats, account string, timeout time.Duration, timeoutErr error) *smokeCaller {
	if timeoutErr == nil {
		timeoutErr = pkg.ErrRPCTimeout
	}
	return &smokeCaller{gc: gc, stats: stats, account: account, timeout: timeout, timeoutErr: timeoutErr}
}

// call 发请求并等它的结局。返回的 envelopeTip 非 0 表示 gate / 路由服级拒绝(信封 error_message,body 为空),
// 此时 response 未填充;为 0 时 response 已解码。超时返回包裹 timeoutErr 的 error;
// gate 推 kServiceUnavailable 立刻返回 error,不等满超时。
func (c *smokeCaller) call(messageId uint32, request, response proto.Message) (envelopeTip uint32, err error) {
	return c.callWithin(messageId, request, response, c.timeout)
}

// callWithin 与 call 相同,只是这一次的超时预算用 timeout 而不是构造时给的默认值
// (同一场景里个别步骤要等得更久或更短时用)。timeout 必须为正,否则本地不发出、返回 ErrRPCNotSent。
func (c *smokeCaller) callWithin(messageId uint32, request, response proto.Message, timeout time.Duration) (envelopeTip uint32, err error) {
	return c.exchange(messageId, request, response, timeout, nil)
}

// callDispatch 与 callWithin 相同,另外把本次的成功回包在派发它的 goroutine(RecvLoop)上交给 dispatch,
// dispatch 执行完才返回。用于迁移前靠生成 handler 把回包写进 Player 状态的等待:回包带请求号后不再进 onMessage,
// 改由这里显式交给原来的分发函数(通常是 handler.MessageBodyHandler,或场景自己的收包回调),副作用与迁移前
// 同一 goroutine、同一收包顺序,不会与之后到达的推送交错(pkg.GameClient.CallSyncThen)。
//
// 信封错误**不交** dispatch:body 为空,生成 handler 会把它解成一份全零响应、看上去像受理;它照常作为 envelopeTip 返回。
// dispatch 不得阻塞。response 为 nil 时不解码响应体(只要副作用的调用方)。
func (c *smokeCaller) callDispatch(messageId uint32, request, response proto.Message, timeout time.Duration,
	dispatch func(*pkg.GameClient, *base.MessageContent)) (envelopeTip uint32, err error) {
	if dispatch == nil {
		panic("smokeCaller.callDispatch: nil dispatch")
	}
	return c.exchange(messageId, request, response, timeout, func(reply *base.MessageContent, err error) {
		if err == nil {
			dispatch(c.gc, reply)
		}
	})
}

// exchange 是 callWithin / callDispatch 的实现。onReply 为 nil 时等同 CallSync。
func (c *smokeCaller) exchange(messageId uint32, request, response proto.Message, timeout time.Duration,
	onReply func(*base.MessageContent, error)) (envelopeTip uint32, err error) {
	ctx, cancel := context.WithCancelCause(context.Background())
	defer cancel(nil)
	c.mu.Lock()
	c.abort = cancel
	c.mu.Unlock()
	defer func() {
		c.mu.Lock()
		c.abort = nil
		c.mu.Unlock()
	}()

	reply, err := c.gc.CallSyncThen(ctx, messageId, request, timeout, onReply)
	if c.stats != nil && !errors.Is(err, pkg.ErrRPCNotSent) {
		c.stats.MsgSent()
	}
	var tipErr *pkg.ServerTipError
	switch {
	case err == nil:
		c.countRecv()
		if response == nil {
			return 0, nil
		}
		if err := proto.Unmarshal(reply.GetSerializedMessage(), response); err != nil {
			return 0, fmt.Errorf("decode message_id=%d: %w", messageId, err)
		}
		return 0, nil
	case errors.As(err, &tipErr):
		c.countRecv()
		return tipErr.Tip, nil
	case errors.Is(err, pkg.ErrRPCTimeout):
		return 0, fmt.Errorf("message_id=%d account=%s %w(%s)", messageId, c.account, c.timeoutErr, timeout)
	case errors.Is(err, errSmokeGateUnavailable):
		return 0, fmt.Errorf("gate SendTipToClient tip=%d(gate 选不到路由服实例,或未以 GATE_CLIENT_RPC_ROUTER=1 启动)",
			smokeTipServiceUnavailable)
	default:
		return 0, fmt.Errorf("message_id=%d account=%s: %w", messageId, c.account, err)
	}
}

func (c *smokeCaller) countRecv() {
	if c.stats != nil {
		c.stats.MsgRecv()
	}
}

// onGateTip 由场景的 onMessage(RecvLoop goroutine)在收到 SendTipToClient 时调用。
// kServiceUnavailable 让当前等待立刻失败:gate 不经路由 lane 的失败兜底推的是这条不带请求号的提示,
// 不中止就只能等满超时。其它提示码不影响等待。
func (c *smokeCaller) onGateTip(tip uint32) {
	if tip != smokeTipServiceUnavailable {
		return
	}
	c.mu.Lock()
	abort := c.abort
	c.mu.Unlock()
	if abort != nil {
		abort(errSmokeGateUnavailable)
	}
}

// onGateTipMessage 解出 SendTipToClient 的码交给 onGateTip。msg 必须是 SendTipToClient。
func (c *smokeCaller) onGateTipMessage(msg *base.MessageContent) {
	var tipInfo base.TipInfoMessage
	if err := proto.Unmarshal(msg.GetSerializedMessage(), &tipInfo); err == nil && tipInfo.GetId() != 0 {
		c.onGateTip(tipInfo.GetId())
	}
}

// smokeDropUncorrelatedReply 处理落到场景 onMessage 的「请求消息号 + id = 0」:请求回包本应带请求号、
// 由 GameClient 交给 Call,走到这里说明服务端没回显请求号,对应的调用将以超时收场。
// 丢弃而不交给通用分发:信封拒绝(body 为空)落进生成的 MessageBodyHandler 会被解成全零响应,看上去像受理。
func smokeDropUncorrelatedReply(account string, msg *base.MessageContent) {
	zap.L().Warn("[smoke] reply without request id dropped: server did not echo ClientRequest.id "+
		"(direct mode needs a gate with the 2026-10-19 rpc_reply_bridge fix); the waiting call will time out",
		zap.String("account", account),
		zap.Uint32("message_id", msg.GetMessageId()),
		zap.Uint32("envelope_tip", msg.GetErrorMessage().GetId()))
}
