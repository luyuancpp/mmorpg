package logic

// 请求截止之后不再提交(docs/design/client-request-ordering.md §6.1 / O11 / §14 第 4 条)的回归测试。
//
// gate 对同一会话的路由类请求单飞:路由服在 ForwardTimeoutMs 回超时,gate 就放行下一条。chat 的 handler
// 若在截止之后仍把历史写出去,这条写就会晚于下一条请求落地。这里用已取消的 ctx 模拟「请求已截止」:
//   - 截止在进入 SendChat 之前:幂等键、限速计数、历史一条都不写;
//   - 截止在占键与写历史之间:历史不写,而本次占住的幂等键必须用独立 ctx(WithoutCancel + 超时)释放,
//     否则同一 request_id 的重试会在 RequestIdTTLSeconds 内一直回限速码。
// 判据来自 go-redis v9.17.3:取连接前先检查 ctx(internal/pool/pool.go 的 waitTurn),ctx 已结束时命令不会发出。

import (
	"context"
	"strings"
	"testing"

	"chat/internal/constants"
	"chat/internal/svc"

	chatpb "proto/chat"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

// canceledPlayerCtx 是一个已经截止的请求 ctx(go-zero 超时拦截器到期后 handler 拿到的就是这种 ctx)。
func canceledPlayerCtx(playerId uint64) context.Context {
	ctx, cancel := context.WithCancel(playerCtx(playerId))
	cancel()
	return ctx
}

func TestSendChatAfterRequestDeadlineWritesNothing(t *testing.T) {
	for _, requestId := range []string{"deadline-req", ""} {
		t.Run("request_id="+requestId, func(t *testing.T) {
			svcCtx, mr := newTestSvcCtx(t)

			resp, err := NewChatLogic(canceledPlayerCtx(playerA), svcCtx).SendChat(&chatpb.SendChatRequest{
				Message:   &chatpb.ChatMessage{Channel: world, Content: "截止之后不得落库"},
				RequestId: requestId,
			})

			require.NoError(t, err, "业务失败必须 in-band")
			require.Equal(t, constants.ErrStorage, resp.GetErrorMessage().GetId(), "截止之后必须回错误,不能回成功")
			require.False(t, mr.Exists(worldLogKey), "截止之后不得写历史")
			require.False(t, mr.Exists(rateLimitKey(playerA)), "截止之后不得消耗限速额度")
			if requestId != "" {
				require.False(t, mr.Exists(requestIdKey(playerA, requestId)), "截止之后不得占幂等键")
			}
		})
	}
}

// cancelAfterScriptKey 是 ctx 上挂取消点的键:脚本正文包含 scriptContains 的那条 EVAL 执行完之后调用 cancel,
// 模拟请求恰好在这一刻截止(go-zero 超时拦截器取消的就是 handler 手里这个 ctx)。
type cancelAfterScriptKey struct{}

type cancelAfterScript struct {
	scriptContains string
	cancel         context.CancelFunc
}

// cancelAfterScriptHook 按 ctx 上的取消点在命令执行之后取消请求 ctx。只认带取消点的请求,
// 补偿路径的 WithoutCancel ctx 同样带着这个值,但 cancel 幂等,重复触发无害。
type cancelAfterScriptHook struct{}

func (cancelAfterScriptHook) DialHook(next redis.DialHook) redis.DialHook { return next }

func (cancelAfterScriptHook) ProcessPipelineHook(next redis.ProcessPipelineHook) redis.ProcessPipelineHook {
	return next
}

func (cancelAfterScriptHook) ProcessHook(next redis.ProcessHook) redis.ProcessHook {
	return func(ctx context.Context, cmd redis.Cmder) error {
		err := next(ctx, cmd)
		point, _ := ctx.Value(cancelAfterScriptKey{}).(*cancelAfterScript)
		if point == nil || cmd.Name() != "eval" || len(cmd.Args()) < 2 {
			return err
		}
		if script, _ := cmd.Args()[1].(string); strings.Contains(script, point.scriptContains) {
			point.cancel()
		}
		return err
	}
}

func newCancelAfterScriptSvc(t *testing.T) (*svc.ServiceContext, *miniredis.Miniredis) {
	t.Helper()
	mr := miniredis.RunT(t)
	rds := redis.MustNewRedis(redis.RedisConf{Host: mr.Addr(), Type: "node"}, redis.WithHook(cancelAfterScriptHook{}))
	return &svc.ServiceContext{Config: testConfig(), ChatRedis: rds, SharedRedis: rds}, mr
}

func TestSendChatDeadlineBetweenClaimAndAppendReleasesClaimWithDetachedCtx(t *testing.T) {
	svcCtx, mr := newCancelAfterScriptSvc(t)
	ctx, cancel := context.WithCancel(playerCtx(playerA))
	defer cancel()
	// 截止点放在限速脚本执行之后:幂等键已占、限速已计,下一步就是写历史。
	ctx = context.WithValue(ctx, cancelAfterScriptKey{}, &cancelAfterScript{
		scriptContains: `redis.call("INCR"`, cancel: cancel,
	})
	const requestId = "deadline-mid"

	resp, err := NewChatLogic(ctx, svcCtx).SendChat(&chatpb.SendChatRequest{
		Message:   &chatpb.ChatMessage{Channel: world, Content: "占键之后截止"},
		RequestId: requestId,
	})

	require.NoError(t, err)
	require.Error(t, ctx.Err(), "用例必须真正在占键之后截止")
	require.Equal(t, constants.ErrStorage, resp.GetErrorMessage().GetId())
	require.False(t, mr.Exists(worldLogKey), "截止之后不得写历史")
	require.False(t, mr.Exists(requestIdKey(playerA, requestId)),
		"本次占住的 pending 必须用独立 ctx 释放:沿用已取消的请求 ctx 会让释放必然失败")

	// 释放生效的可观察结果:同一 request_id 用新的请求重试能真正写入(而不是回限速码)。
	require.Zero(t, sendAs(t, svcCtx, playerA, world, 0, "占键之后截止", requestId))
	_, msgs := pullAs(t, svcCtx, playerB, world, 0, 20)
	require.Len(t, msgs, 1, "截止的那次一条没写,重试只写一条")
}
