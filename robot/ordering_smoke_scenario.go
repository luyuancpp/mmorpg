package main

// ordering-smoke 场景:单机器人验证「同一会话经路由服的请求按收包顺序执行」
// (docs/design/client-request-ordering.md §1.3 / §12 批次 R):
//
//	O1 登录进场;
//	O2 清场:GetQueueStatus(153)必须是 NOT_QUEUED;还在排队(上一轮残留)直接失败并说明,不自动取消;
//	O3 重复 orderingSmokeRounds 轮:**不等回包**,连发 JoinQueue(157,1V1)→ CancelQueue(148,不带票号),
//	   等两条回包都到,再查 GetQueueStatus。客户端的意图是「加入后取消」,所以最终必须不在队列里。
//	   执行顺序被颠倒(Cancel 先执行、找不到票据回成功,Join 后执行入队)时,这一轮会停在 QUEUED;
//	   本轮记为乱序,并带 Join 回包里的票号补一次 CancelQueue 收尾。
//
// 判据:任何一轮停在 QUEUED 即失败。gate 路由 lane(批次 C)上线之前,路由服按条随机选副本、业务服按条并发执行,
// 本场景**预期会间歇失败**(这正是它要证明的缺陷);上线之后必须每轮都通过。单副本也可能失败:同一 HTTP/2 连接上的
// 两个 unary 在服务端是两个 goroutine 并发执行(§1.2)。
//
// 结果约定(供外层脚本消费):
//
//	全过 → 日志一行 `ORDERING_SMOKE_OK player_id=… rounds=…`,退出码 0;
//	任一步失败 → `ORDERING_SMOKE_FAIL step=… reason=…`,退出码 1。每步通过另打一行 `[ordering-smoke] PASS step=…`。
//
// 不照做就会假绿或误报的几件事:
//   - 必须是路由模式(GATE_CLIENT_RPC_ROUTER=1):直连模式下 match 不回写会话头,gate 收不到 match 的回包
//     (client-request-ordering.md §7.2 第 1 条),O2 就会超时;直连模式本方案也不保序(O7);
//   - 环境里不能有别的玩家在排 1V1:1V1 两人凑对,另一个人在队列里时,本机器人可能在 Join 与 Cancel 之间被凑走,
//     Cancel 回「太迟」、状态变成 MATCHED,本场景按前置条件不满足失败(reason 里写明);
//   - 限流:157 / 148 / 153 都不在 MessageLimiter 表里,吃 gate 默认档(每个消息号滑动 1 秒内最多 3 次)。每轮每个号
//     各发 1 次(乱序轮多 1 次 148),轮间隔 orderingSmokeRoundSpacing 保证不超限。

import (
	"context"
	"errors"
	"fmt"
	"os"
	"strconv"
	"sync"
	"time"

	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"

	"proto/common/base"
	"proto/match"
	"robot/config"
	"robot/generated/pb/game"
	"robot/logic/gameobject"
	"robot/logic/handler"
	"robot/metrics"
	"robot/pkg"

	tiptable "shared/generated/pb/table"
)

// 账号前缀必须是 robot_ 才会命中 login 侧 DevPasswordAuth。99xx 与其它冒烟的账号段错开
// (battle 9001-9004、chat 9005/9006、属性 9101、宠物 9102、guild 92xx、team 93xx、trade 94xx、
// 资产通道 95xx、travel 9601、friend 97xx、channel 9801),避免并行冒烟互相顶号。账号不能在任何队伍里。
const orderingSmokeAccount = "robot_9901"

const (
	// 轮数:乱序是概率事件,轮数越多越容易在 lane 上线前暴露。20 轮约 30 秒(间隔 1.1s × 20 加往返)。
	orderingSmokeRounds = 20
	// 两轮之间的间隔:每个消息号每轮至多 2 次(148 在乱序轮会多发一次收尾),1.1s 保证滑动 1 秒内不超过 3 次。
	orderingSmokeRoundSpacing = 1100 * time.Millisecond
	// 等回包的预算:正常是毫秒级;8s 覆盖 gate→路由服 deadline(base_deploy_config.yaml 8000)。
	orderingSmokeReplyTimeout = 8 * time.Second
	// 首登等进场的预算,与 channel-smoke / team-smoke 同值。
	orderingSmokeSceneReadyTimeout = 15 * time.Second
	orderingSmokePollInterval      = 20 * time.Millisecond
)

// tip id 一律读导表器生成的枚举,不写字面量(AGENTS.md §7 不变量 5)。
var tipOrderingAlreadyQueued = uint32(tiptable.MatchError_kMatchAlreadyQueued)

var errOrderingSmokeTimeout = errors.New("在等待窗口内没有收到回包")

// orderingSmokeRecord 是一条留底的回包。gate 级拒绝(限流 / 无路由服)走信封 error_message,body 为空;
// 批次 C 之前,gate 的 gRPC 失败回执是 SendTipToClient(23),不带请求 id,也一并留底。
type orderingSmokeRecord struct {
	messageId   uint32
	envelopeTip uint32
	body        []byte
}

type orderingSmokeBot struct {
	account string
	gate    string
	gc      *pkg.GameClient
	player  *gameobject.Player
	stats   *metrics.Stats

	// RecvLoop goroutine 写、主流程读,mu 保护。records 只追加。
	mu      sync.Mutex
	records []orderingSmokeRecord
}

// RunOrderingSmoke 是 main.go `mode: ordering-smoke` 的入口。任一步失败以退出码 1 结束进程。
func RunOrderingSmoke(cfg *config.Config) {
	// loginAndEnterScenario 从这个包级变量读认证方式(password / satoken)
	loginTestCfg = cfg

	stats := robotStatsRef
	if stats == nil {
		stats = metrics.NewStats()
	}

	var bot *orderingSmokeBot
	cleanup := func() {
		if bot == nil {
			return
		}
		_ = leaveGame(bot.gc, stats)
		gameobject.PlayerList.Delete(bot.gc.PlayerId)
		bot.gc.Close()
	}
	fail := func(step, format string, args ...any) {
		zap.L().Error(fmt.Sprintf("ORDERING_SMOKE_FAIL step=%s reason=%s", step, fmt.Sprintf(format, args...)))
		cleanup()
		_ = zap.L().Sync()
		os.Exit(1)
	}
	pass := func(step, format string, args ...any) {
		zap.L().Info(fmt.Sprintf("[ordering-smoke] PASS step=%s %s", step, fmt.Sprintf(format, args...)))
	}

	// ---- O1:登录进场 ----
	var err error
	bot, err = orderingSmokeLogin(cfg, orderingSmokeAccount, stats)
	if err != nil {
		fail("O1-login", "account=%s zone=%d err=%v", orderingSmokeAccount, cfg.ZoneID, err)
	}
	pass("O1-login", "player_id=%d gate=%s", bot.gc.PlayerId, bot.gate)

	// ---- O2:清场 ----
	state, err := bot.queueState()
	if err != nil {
		fail("O2-precheck", "GetQueueStatus: %v(需要路由模式 GATE_CLIENT_RPC_ROUTER=1,见文件头)", err)
	}
	if state != match.QueueState_QUEUE_STATE_NOT_QUEUED && state != match.QueueState_QUEUE_STATE_UNSPECIFIED {
		fail("O2-precheck", "进场后队列状态是 %s,需要先清掉上一轮残留(QUEUED 时等票据 TTL 或手工取消;MATCHED / READY 时等那一局收尾)", state)
	}
	pass("O2-precheck", "queue_state=%s", state)

	// ---- O3:流水线连发 Join → Cancel ----
	reordered := make([]int, 0)
	for round := 1; round <= orderingSmokeRounds; round++ {
		if round > 1 {
			time.Sleep(orderingSmokeRoundSpacing)
		}
		ticket, err := bot.pipelineJoinThenCancel()
		if err != nil {
			fail("O3-pipeline", "round=%d/%d: %v", round, orderingSmokeRounds, err)
		}
		state, err := bot.queueState()
		if err != nil {
			fail("O3-status", "round=%d/%d GetQueueStatus: %v", round, orderingSmokeRounds, err)
		}
		switch state {
		case match.QueueState_QUEUE_STATE_NOT_QUEUED, match.QueueState_QUEUE_STATE_UNSPECIFIED:
			// 符合发送顺序:加入后取消。
		case match.QueueState_QUEUE_STATE_QUEUED:
			// Cancel 先于 Join 执行。带 Join 回包里的票号收尾,下一轮才是干净的起点。
			reordered = append(reordered, round)
			zap.L().Warn("[ordering-smoke] Execution order reversed: Cancel ran before Join, player is still queued",
				zap.Int("round", round), zap.String("ticket", ticket))
			if err := bot.cancelWithTicket(ticket); err != nil {
				fail("O3-cleanup", "round=%d/%d 乱序后收尾取消失败 ticket=%s: %v", round, orderingSmokeRounds, ticket, err)
			}
		default:
			fail("O3-status", "round=%d/%d 队列状态 %s:环境里有别的玩家在排 1V1,本机器人在 Join 与 Cancel 之间被凑走(前置条件不满足,见文件头)",
				round, orderingSmokeRounds, state)
		}
	}
	if len(reordered) > 0 {
		fail("O3-order", "%d/%d 轮执行顺序被颠倒(先发的 JoinQueue 晚于后发的 CancelQueue 执行),乱序轮次=%v。"+
			"gate 路由 lane(client-request-ordering.md 批次 C)上线之前这是预期结果;上线之后出现即为缺陷",
			len(reordered), orderingSmokeRounds, reordered)
	}
	pass("O3-order", "rounds=%d 全部按发送顺序执行", orderingSmokeRounds)

	zap.L().Info(fmt.Sprintf("ORDERING_SMOKE_OK player_id=%d rounds=%d", bot.gc.PlayerId, orderingSmokeRounds))
	cleanup()
	_ = zap.L().Sync()
}

func orderingSmokeLogin(cfg *config.Config, account string, stats *metrics.Stats) (*orderingSmokeBot, error) {
	host, portStr, tokenPayload, tokenSig, err := resolveGateAddrLocal(cfg)
	if err != nil {
		return nil, fmt.Errorf("resolve gate address: %w", err)
	}
	port, err := strconv.Atoi(portStr)
	if err != nil {
		return nil, fmt.Errorf("bad gate port %q: %w", portStr, err)
	}
	gc, err := connectAndVerify(host, port, account, tokenPayload, tokenSig)
	if err != nil {
		return nil, fmt.Errorf("connect gate: %w", err)
	}
	if err := loginAndEnterScenario(gc, cfg.Password, stats); err != nil {
		gc.Close()
		return nil, fmt.Errorf("login and enter: %w", err)
	}

	bot := &orderingSmokeBot{
		account: account,
		gate:    host + ":" + portStr,
		gc:      gc,
		player:  gameobject.NewPlayer(gc.PlayerId),
		stats:   stats,
	}
	gameobject.PlayerList.Set(gc.PlayerId, bot.player)
	go gc.RecvLoop(bot.onMessage)

	waitCtx, waitCancel := context.WithTimeout(context.Background(), orderingSmokeSceneReadyTimeout)
	defer waitCancel()
	if err := bot.player.WaitSceneReady(waitCtx); err != nil {
		gameobject.PlayerList.Delete(gc.PlayerId)
		gc.Close()
		return nil, fmt.Errorf("wait scene ready(account=%s): %w", account, err)
	}
	return bot, nil
}

// orderingSmokeIsRecorded:本场景按到达顺序留底的消息号。
func orderingSmokeIsRecorded(messageId uint32) bool {
	switch messageId {
	case game.MatchServiceJoinQueueMessageId,
		game.MatchServiceCancelQueueMessageId,
		game.MatchServiceGetQueueStatusMessageId,
		game.SceneClientPlayerCommonSendTipToClientMessageId:
		return true
	}
	return false
}

// onMessage:先留底,再转发给通用分发(进场推送等靠通用 handler 更新 Player)。
func (b *orderingSmokeBot) onMessage(client *pkg.GameClient, msg *base.MessageContent) {
	b.stats.MsgRecv()
	if orderingSmokeIsRecorded(msg.GetMessageId()) {
		b.mu.Lock()
		b.records = append(b.records, orderingSmokeRecord{
			messageId:   msg.GetMessageId(),
			envelopeTip: msg.GetErrorMessage().GetId(),
			body:        msg.GetSerializedMessage(),
		})
		b.mu.Unlock()
	}
	handler.MessageBodyHandler(client, msg)
}

// mark 返回当前留底条数,作为"从这之后到达的回包"的扫描起点。必须在发请求之前调用。
func (b *orderingSmokeBot) mark() int {
	b.mu.Lock()
	defer b.mu.Unlock()
	return len(b.records)
}

// waitReplies 等 since 之后、按 wantIds 顺序各认领一条回包;收到 SendTipToClient(23)直接按失败返回
// (批次 C 之前 gate 的 gRPC 失败回执不带请求 id,分不清是哪一条)。返回与 wantIds 一一对应的记录。
func (b *orderingSmokeBot) waitReplies(since int, wantIds ...uint32) ([]orderingSmokeRecord, error) {
	deadline := time.Now().Add(orderingSmokeReplyTimeout)
	for {
		b.mu.Lock()
		got := make([]orderingSmokeRecord, len(wantIds))
		found := make([]bool, len(wantIds))
		var tipErr error
		for _, r := range b.records[since:] {
			if r.messageId == game.SceneClientPlayerCommonSendTipToClientMessageId {
				tip := &base.TipInfoMessage{}
				_ = proto.Unmarshal(r.body, tip)
				tipErr = fmt.Errorf("收到 gate 的失败回执 SendTipToClient tip=%d(没有请求 id,分不清是哪一条)", tip.GetId())
				continue
			}
			for i, id := range wantIds {
				if !found[i] && r.messageId == id {
					got[i], found[i] = r, true
					break
				}
			}
		}
		b.mu.Unlock()
		if tipErr != nil {
			return nil, tipErr
		}
		all := true
		for _, f := range found {
			all = all && f
		}
		if all {
			return got, nil
		}
		if time.Now().After(deadline) {
			return nil, fmt.Errorf("%w(等待 message_id=%v,已收到 %v)", errOrderingSmokeTimeout, wantIds, found)
		}
		time.Sleep(orderingSmokePollInterval)
	}
}

func (b *orderingSmokeBot) send(messageId uint32, request proto.Message) error {
	if err := b.gc.SendRequest(messageId, request); err != nil {
		return fmt.Errorf("send message_id=%d account=%s: %w", messageId, b.account, err)
	}
	b.stats.MsgSent()
	return nil
}

// pipelineJoinThenCancel 不等回包连发 JoinQueue → CancelQueue(不带票号,模拟「点了匹配马上又点取消」),
// 等两条回包都到。返回 Join 回包里的票号(乱序时用来收尾)。
func (b *orderingSmokeBot) pipelineJoinThenCancel() (string, error) {
	since := b.mark()
	if err := b.send(game.MatchServiceJoinQueueMessageId, &match.JoinQueueRequest{Mode: match.MatchMode_MATCH_MODE_1V1}); err != nil {
		return "", err
	}
	if err := b.send(game.MatchServiceCancelQueueMessageId, &match.CancelQueueRequest{}); err != nil {
		return "", err
	}
	replies, err := b.waitReplies(since, game.MatchServiceJoinQueueMessageId, game.MatchServiceCancelQueueMessageId)
	if err != nil {
		return "", err
	}
	join, cancel := replies[0], replies[1]
	if join.envelopeTip != 0 || cancel.envelopeTip != 0 {
		return "", fmt.Errorf("gate / 路由服拒绝:JoinQueue 信封 tip=%d,CancelQueue 信封 tip=%d", join.envelopeTip, cancel.envelopeTip)
	}
	resp := &match.JoinQueueResponse{}
	if err := proto.Unmarshal(join.body, resp); err != nil {
		return "", fmt.Errorf("解析 JoinQueueResponse: %w", err)
	}
	if resp.GetErrorCode() != 0 {
		hint := ""
		if resp.GetErrorCode() == tipOrderingAlreadyQueued {
			hint = "(已在队列中:上一轮残留未清,见 O2)"
		}
		return "", fmt.Errorf("JoinQueue 业务拒绝 error_code=%d%s", resp.GetErrorCode(), hint)
	}
	return resp.GetQueueTicket(), nil
}

// queueState 查一次权威队列状态(GetQueueStatus)。
func (b *orderingSmokeBot) queueState() (match.QueueState, error) {
	since := b.mark()
	if err := b.send(game.MatchServiceGetQueueStatusMessageId, &match.GetQueueStatusRequest{}); err != nil {
		return match.QueueState_QUEUE_STATE_UNSPECIFIED, err
	}
	replies, err := b.waitReplies(since, game.MatchServiceGetQueueStatusMessageId)
	if err != nil {
		return match.QueueState_QUEUE_STATE_UNSPECIFIED, err
	}
	if replies[0].envelopeTip != 0 {
		return match.QueueState_QUEUE_STATE_UNSPECIFIED, fmt.Errorf("gate / 路由服拒绝 GetQueueStatus:信封 tip=%d", replies[0].envelopeTip)
	}
	resp := &match.GetQueueStatusResponse{}
	if err := proto.Unmarshal(replies[0].body, resp); err != nil {
		return match.QueueState_QUEUE_STATE_UNSPECIFIED, fmt.Errorf("解析 GetQueueStatusResponse: %w", err)
	}
	return resp.GetState(), nil
}

// cancelWithTicket 带票号取消(乱序轮的收尾)。
func (b *orderingSmokeBot) cancelWithTicket(ticket string) error {
	since := b.mark()
	if err := b.send(game.MatchServiceCancelQueueMessageId, &match.CancelQueueRequest{QueueTicket: ticket}); err != nil {
		return err
	}
	replies, err := b.waitReplies(since, game.MatchServiceCancelQueueMessageId)
	if err != nil {
		return err
	}
	if replies[0].envelopeTip != 0 {
		return fmt.Errorf("信封 tip=%d", replies[0].envelopeTip)
	}
	state, err := b.queueState()
	if err != nil {
		return err
	}
	if state == match.QueueState_QUEUE_STATE_QUEUED {
		return fmt.Errorf("带票号取消之后仍在排队")
	}
	return nil
}
