package main

// rank-smoke 场景:两个机器人跨区打一局 1V1,验证整条写榜链 —— 对局 → match 评分入账 → 评分快照(Kafka match-rating-snapshot)
// → rank 消费写榜 → 经 gate → client_rpc_router 读榜(设计 docs/design/leaderboard-system.md §8.2;前置条件写在 etc/rank_smoke.yaml 文件头)。
//
//	0. A 登 zone_a、B 登 zone_b、N 登 zone_a(N 从不排位);断言 A、B **当前连着的** gate 不同;
//	1. A、B 各 GetRankPage 一次,记下"打之前"的 self(未上榜时 rank = 0;N 到第 4 步才读);
//	2. A、B 双双 JoinQueue(1V1),复用 battle-smoke cross_zone 的 runCrossZoneFighter 自动打到终局;断言两人在同一场对局;
//	3. 轮询(update_timeout_ms 内,每次间隔 request_interval_ms)直到 A、B 的 self 都"新打了一局":
//	   打之前已上榜 → games 恰好 +1;打之前未上榜 → 上榜且 games ≥ 1(写榜是异步的:match 发快照 → rank 消费);
//	4. 断言:
//	   - 胜负分明(SIDE_A_WIN / SIDE_B_WIN)且两人打之前都在榜上时,恰好一人涨分、另一人不涨(0 分下限时可以不变);
//	   - **跨区名字补全**(R2):B(zone_b)按 A 的名次算出页码读那一页,里面有 A(zone_a)的条目,且 A 的条目与 B 自己的 self 名字都非空;
//	   - 名次与分数一致:用这同一份回包里的 A 条目与 B 的 self 比,分数高的名次靠前;
//	     本地 RankRedis 回落共享库时两个 zone 读的是同一个 ZSET,"两边看到同一张榜"恒绿,真正会跨区出错的是名字;
//	   - 页内名次从 (page − 1) × page_size + 1 起连续、分数不增(读 Lua 的排序与名次);
//	   - N 未上榜:self.rank = 0、error_message 为空、entries 照常返回(T6 / R4,确定不在榜上的会话);
//	5. (可选,rate_limit_check = true)N 在一个窗口内连发 rate_limit_per_second + 1 次,最后一次被 gate 限流(R5;
//	   要等 MessageLimiter 按 rank 导过表,否则默认档 3 次就会拒)。
//
// 结果约定(供外层脚本消费):
//
//	全过 → 日志一行 `RANK_SMOKE_OK player_a=… player_b=… battle_id=… rank_a=… rank_b=… …`,退出码 0;
//	任一步失败 → `RANK_SMOKE_FAIL step=… reason=…`,退出码 1。
//
// 可重复运行:每轮都是新的一局,games 只增不减;不做预清理(榜键清空要按 §3.5 的 runbook,冒烟不碰)。
//
// 消息号常量 game.ClientPlayerRankGetRankPageMessageId 来自 robot/generated/pb/game/message_id.go(2026-10-17 全量 proto-gen 后为 275);
// 它是无类型常量,绑定到变量时要显式写 uint32(见 call)。

import (
	"context"
	"errors"
	"fmt"
	"net"
	"os"
	"strconv"
	"sync"
	"time"

	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"

	"proto/battle"
	"proto/common/base"
	"proto/match"
	rankpb "proto/rank"
	"robot/config"
	"robot/generated/pb/game"
	"robot/logic/gameobject"
	"robot/logic/handler"
	"robot/metrics"
	"robot/pkg"

	tiptable "shared/generated/pb/table"
)

// 账号前缀必须是 robot_ 才会命中 login 侧 DevPasswordAuth。98xx 段:9801 channel-smoke、9811/9812 mail-smoke,982x 归本场景。
// 归属区在建角时登记:A、N 首次在 zone_a 建角,B 首次在 zone_b 建角(否则"跨区名字补全"证明不了任何东西)。
const (
	rankSmokeAccountA     = "robot_9821" // zone_a 的参战方
	rankSmokeAccountB     = "robot_9822" // zone_b 的参战方,也是"跨区读名字"的读者
	rankSmokeAccountNever = "robot_9823" // 从不排位,验证 self.rank = 0
)

// 单次 GetRankPage 的等待预算:大于路由服 ForwardTimeoutMs(5s)与 rank zrpc Timeout(4000ms)。
const rankSmokeRpcTimeout = 10 * time.Second

// 等进场的预算,与 prepareBehaviorClient 同值。
const rankSmokeSceneReadyTimeout = 15 * time.Second

// 协议上限(go/rank/internal/logic 的 MaxPageSize):超过按上限钳制,这里直接用上限读"打之前 / 打之后"的 self。
const rankSmokeMaxPageSize uint32 = 50

var (
	tipRankInvalidParameter   = uint32(tiptable.CommonError_kInvalidParameter)
	tipRankServiceUnavailable = uint32(tiptable.CommonError_kServiceUnavailable)
	tipRankRateLimited        = uint32(tiptable.CommonError_kRateLimitExceeded)
)

var errRankSmokeTimeout = errors.New("等回包超时")

// rankSmokeBot 是一个已登录进场的机器人会话。battle 是同一会话的 battle-smoke 视图(runCrossZoneFighter 要它)。
type rankSmokeBot struct {
	account string
	zone    uint32
	gc      *pkg.GameClient
	player  *gameobject.Player
	stats   *metrics.Stats
	battle  *battleSmokeBot

	requestSpacing time.Duration

	// RecvLoop goroutine 写、场景主流程读,mu 保护。
	mu       sync.Mutex
	replies  map[uint32]*base.MessageContent
	awaiting uint32

	lastRequestAt time.Time // 仅主流程读写
}

// RunRankSmoke 是 main.go `mode: rank-smoke` 的入口。任一步失败直接以退出码 1 结束进程;全部通过则正常返回(退出码 0)。
func RunRankSmoke(cfg *config.Config) {
	// loginAndEnterScenario 从这个包级变量读认证方式(password / satoken)
	loginTestCfg = cfg

	stats := robotStatsRef
	if stats == nil {
		stats = metrics.NewStats()
	}
	sc := cfg.RankSmoke
	spacing := time.Duration(sc.RequestIntervalMs) * time.Millisecond
	updateTimeout := time.Duration(sc.UpdateTimeoutMs) * time.Millisecond

	var bots []*rankSmokeBot
	cleanup := func() {
		for _, bot := range bots {
			_ = leaveGame(bot.gc, stats)
			sendDisconnectBestEffort(bot.gc)
			gameobject.PlayerList.Delete(bot.gc.PlayerId)
			bot.gc.Close()
		}
	}
	fail := func(step, format string, args ...any) {
		zap.L().Error(fmt.Sprintf("RANK_SMOKE_FAIL step=%s reason=%s", step, fmt.Sprintf(format, args...)))
		cleanup()
		_ = zap.L().Sync()
		os.Exit(1)
	}

	// ---- 步骤 0:登录(并发) ----
	type loginSpec struct {
		account string
		zone    uint32
	}
	specs := []loginSpec{{rankSmokeAccountA, sc.ZoneA}, {rankSmokeAccountB, sc.ZoneB}, {rankSmokeAccountNever, sc.ZoneA}}
	loggedIn := make([]*rankSmokeBot, len(specs))
	loginErrs := make([]error, len(specs))
	var wg sync.WaitGroup
	for i, spec := range specs {
		wg.Add(1)
		go func(idx int, spec loginSpec) {
			defer wg.Done()
			zoneCfg := *cfg
			zoneCfg.ZoneID = spec.zone
			loggedIn[idx], loginErrs[idx] = rankSmokeLogin(&zoneCfg, spec.account, spacing, stats)
		}(i, spec)
	}
	wg.Wait()
	for _, bot := range loggedIn {
		if bot != nil {
			bots = append(bots, bot)
		}
	}
	for i, err := range loginErrs {
		if err != nil {
			fail("login", "account=%s zone=%d err=%v", specs[i].account, specs[i].zone, err)
		}
	}
	a, b, never := loggedIn[0], loggedIn[1], loggedIn[2]
	if gateA, gateB := a.gateAddr(), b.gateAddr(); gateA == gateB {
		fail("zone-placement", "A and B are connected to the same gate %s (zone_a=%d zone_b=%d); this is not a cross-zone run",
			gateA, sc.ZoneA, sc.ZoneB)
	}
	zap.L().Info("[rank-smoke] robots in scene",
		zap.Uint64("a", a.gc.PlayerId), zap.String("gate_a", a.gateAddr()),
		zap.Uint64("b", b.gc.PlayerId), zap.String("gate_b", b.gateAddr()),
		zap.Uint64("never", never.gc.PlayerId))

	// ---- 步骤 1:打之前的 self ----
	beforeA, err := a.selfEntry()
	if err != nil {
		fail("read-before", "A: %v", err)
	}
	beforeB, err := b.selfEntry()
	if err != nil {
		fail("read-before", "B: %v", err)
	}
	zap.L().Info("[rank-smoke] step 1: self before the battle",
		zap.Uint32("a_rank", beforeA.GetRank()), zap.Uint64("a_games", beforeA.GetGames()), zap.Uint32("a_rating_centi", beforeA.GetRatingCenti()),
		zap.Uint32("b_rank", beforeB.GetRank()), zap.Uint64("b_games", beforeB.GetGames()), zap.Uint32("b_rating_centi", beforeB.GetRatingCenti()))

	// ---- 步骤 2:跨区 1V1 打到终局 ----
	aDone := make(chan crossZoneFighterResult, 1)
	bDone := make(chan crossZoneFighterResult, 1)
	go func() { aDone <- runCrossZoneFighter("A", a.battle, sc.ZoneA, match.MatchMode_MATCH_MODE_1V1, stats) }()
	go func() { bDone <- runCrossZoneFighter("B", b.battle, sc.ZoneB, match.MatchMode_MATCH_MODE_1V1, stats) }()
	collect := func(side string, done chan crossZoneFighterResult) crossZoneFighterResult {
		select {
		case res := <-done:
			if res.err != nil {
				fail("battle-"+res.step, "%v", res.err)
			}
			return res
		case <-time.After(battleSmokeStartTimeout + battleSmokeEndTimeout + 15*time.Second):
			fail("battle-"+side, "fighter goroutine did not finish in time")
		}
		return crossZoneFighterResult{}
	}
	resA := collect("a", aDone)
	resB := collect("b", bDone)
	if resA.battleId == 0 || resA.battleId != resB.battleId {
		fail("battle-id-mismatch", "A battle_id=%d B battle_id=%d: A and B must fight each other (keep other 1V1 queues quiet during the smoke)",
			resA.battleId, resB.battleId)
	}
	outcome := battle.EBattleOutcome(resA.outcome)
	zap.L().Info("[rank-smoke] step 2: battle finished", zap.Uint64("battle_id", resA.battleId), zap.String("outcome", outcome.String()))

	// ---- 步骤 3:等写榜(异步)----
	var afterA, afterB *rankpb.RankEntry
	deadline := time.Now().Add(updateTimeout)
	for {
		afterA, err = a.selfEntry()
		if err != nil {
			fail("read-after", "A: %v", err)
		}
		afterB, err = b.selfEntry()
		if err != nil {
			fail("read-after", "B: %v", err)
		}
		if rankSmokePlayedOneMore(beforeA, afterA) && rankSmokePlayedOneMore(beforeB, afterB) {
			break
		}
		if !time.Now().Before(deadline) {
			fail("board-update", "within %s the board did not record the battle: A games %d -> %d (rank %d), B games %d -> %d (rank %d). "+
				"Check match_service.yaml RatingEnabled / RatingSnapshotPublishEnabled = true (local only, do not commit), "+
				"rank is consuming (rank_last_consume_unixtime advancing) and redis is writable",
				updateTimeout, beforeA.GetGames(), afterA.GetGames(), afterA.GetRank(), beforeB.GetGames(), afterB.GetGames(), afterB.GetRank())
		}
	}
	zap.L().Info("[rank-smoke] step 3: board updated",
		zap.Uint32("a_rank", afterA.GetRank()), zap.Uint64("a_games", afterA.GetGames()), zap.Uint32("a_rating_centi", afterA.GetRatingCenti()),
		zap.Uint32("b_rank", afterB.GetRank()), zap.Uint64("b_games", afterB.GetGames()), zap.Uint32("b_rating_centi", afterB.GetRatingCenti()))

	// ---- 步骤 4:断言 ----
	// 胜负方向:只有两人打之前都在榜上才有可信的"打之前分数"(未上榜时 match 侧可能早有评分,不能假定 1500)。
	decisive := outcome == battle.EBattleOutcome_BATTLE_OUTCOME_SIDE_A_WIN || outcome == battle.EBattleOutcome_BATTLE_OUTCOME_SIDE_B_WIN
	if decisive && beforeA.GetRank() != 0 && beforeB.GetRank() != 0 {
		upA := afterA.GetRatingCenti() > beforeA.GetRatingCenti()
		upB := afterB.GetRatingCenti() > beforeB.GetRatingCenti()
		if upA == upB {
			fail("rating-direction", "decisive battle (%s) but rating moved A %d -> %d, B %d -> %d: expected exactly one winner to gain",
				outcome, beforeA.GetRatingCenti(), afterA.GetRatingCenti(), beforeB.GetRatingCenti(), afterB.GetRatingCenti())
		}
	}
	// 跨区名字补全:B 读 A 所在的那一页。
	pageOfA := (afterA.GetRank()-1)/rankSmokeMaxPageSize + 1
	pageResp, err := b.page(pageOfA, rankSmokeMaxPageSize)
	if err != nil {
		fail("cross-zone-name", "B reads page %d: %v", pageOfA, err)
	}
	if err := rankSmokeCheckPage(pageResp, pageOfA, rankSmokeMaxPageSize); err != nil {
		fail("page-order", "%v", err)
	}
	var entryA *rankpb.RankEntry
	for _, e := range pageResp.GetEntries() {
		if e.GetPlayerId() == a.gc.PlayerId {
			entryA = e
		}
	}
	if entryA == nil {
		fail("cross-zone-name", "A (player %d, rank %d) is not on page %d as read by B (the board may have moved; rerun)", a.gc.PlayerId, afterA.GetRank(), pageOfA)
	}
	// 跨区名字补全:同一份回包里同时核 A(页内条目,zone_a 建角)与 B(self,zone_b 建角)的名字。rank 是全局服务,路由服在所有
	// RankNodeService 实例里随机挑,应答这次请求的 rank 属于哪个 zone 事先不知道;两人里总有一个对它是跨区玩家,所以两个名字
	// 都非空才说明跨区补名真的通了(只核一个,在"全服一份 rank"的拓扑下可能恰好只测到同区)。
	if entryA.GetName() == "" || pageResp.GetSelf().GetName() == "" {
		fail("cross-zone-name", "in B's read of page %d, A (zone %d) name=%q and B's own self (zone %d) name=%q: both must be non-empty; "+
			"rank's BatchGetPlayerName failed for at least one of them (check rank_name_lookup_total{outcome=\"error\"} and data_service reachability)",
			pageOfA, sc.ZoneA, entryA.GetName(), sc.ZoneB, pageResp.GetSelf().GetName())
	}
	if pageResp.GetSelf().GetPlayerId() != b.gc.PlayerId || pageResp.GetSelf().GetRank() == 0 {
		fail("cross-zone-name", "B's self entry is player %d rank %d, want player %d ranked (B just played a match)",
			pageResp.GetSelf().GetPlayerId(), pageResp.GetSelf().GetRank(), b.gc.PlayerId)
	}

	// 名次与分数一致:用同一份回包里的 A 条目与 B 的 self 比(一页 + self 出自同一次只读 Lua,是同一时刻的榜);
	// 拿 A、B 两次独立读到的名次比,写榜恰好落在两次读之间时会误报。分数相同时按"先达到者在前",谁前都对,不判。
	selfB := pageResp.GetSelf()
	switch {
	case entryA.GetRatingCenti() > selfB.GetRatingCenti() && entryA.GetRank() >= selfB.GetRank(),
		selfB.GetRatingCenti() > entryA.GetRatingCenti() && selfB.GetRank() >= entryA.GetRank():
		fail("rank-order", "in one response A rating=%d rank=%d, B rating=%d rank=%d: higher rating must rank higher",
			entryA.GetRatingCenti(), entryA.GetRank(), selfB.GetRatingCenti(), selfB.GetRank())
	}

	// N 从不排位:未上榜不是错误。
	neverResp, err := never.page(1, rankSmokeMaxPageSize)
	if err != nil {
		fail("self-unranked", "%v", err)
	}
	if neverResp.GetErrorMessage().GetId() != 0 {
		fail("self-unranked", "unranked caller got tip=%d, expected none (not being ranked is not an error)", neverResp.GetErrorMessage().GetId())
	}
	if neverResp.GetSelf().GetRank() != 0 {
		fail("self-unranked", "%s never queued but self.rank=%d (first character must never have played 1V1)", rankSmokeAccountNever, neverResp.GetSelf().GetRank())
	}
	if len(neverResp.GetEntries()) == 0 {
		fail("self-unranked", "entries are empty for the unranked caller although the board has %d entries", neverResp.GetTotalCount())
	}
	if err := rankSmokeCheckPage(neverResp, 1, rankSmokeMaxPageSize); err != nil {
		fail("page-order", "%v", err)
	}

	// ---- 步骤 5(可选):限流 ----
	rateLimitChecked := false
	if sc.RateLimitCheck {
		if err := never.expectRateLimited(sc.RateLimitPerSecond); err != nil {
			fail("rate-limit", "%v", err)
		}
		rateLimitChecked = true
	}

	zap.L().Info(fmt.Sprintf("RANK_SMOKE_OK player_a=%d player_b=%d battle_id=%d outcome=%s rank_a=%d rank_b=%d games_a=%d games_b=%d total=%d name_cross_zone=%q rate_limit_checked=%v",
		a.gc.PlayerId, b.gc.PlayerId, resA.battleId, outcome, afterA.GetRank(), afterB.GetRank(), afterA.GetGames(), afterB.GetGames(),
		pageResp.GetTotalCount(), entryA.GetName(), rateLimitChecked),
		zap.String("gate_a", a.gateAddr()), zap.String("gate_b", b.gateAddr()))
	cleanup()
	_ = zap.L().Sync()
}

// rankSmokePlayedOneMore 判断"打之后"是否恰好比"打之前"多记了一局:之前在榜 → games 恰好 +1;之前不在榜 → 上榜且 games ≥ 1。
func rankSmokePlayedOneMore(before, after *rankpb.RankEntry) bool {
	if after.GetRank() == 0 {
		return false
	}
	if before.GetRank() == 0 {
		return after.GetGames() >= 1
	}
	return after.GetGames() == before.GetGames()+1
}

// rankSmokeCheckPage 校验一页的形状:名次从 (page − 1) × size + 1 起连续、分数不增、条数不超过页长、page / page_count 自洽。
func rankSmokeCheckPage(resp *rankpb.GetRankPageResponse, wantPage, size uint32) error {
	if resp.GetPage() != wantPage {
		return fmt.Errorf("asked page %d, got page %d (page_count=%d)", wantPage, resp.GetPage(), resp.GetPageCount())
	}
	if resp.GetPageCount() < 1 || resp.GetPage() > resp.GetPageCount() {
		return fmt.Errorf("page=%d page_count=%d is not self-consistent", resp.GetPage(), resp.GetPageCount())
	}
	if uint32(len(resp.GetEntries())) > size {
		return fmt.Errorf("page has %d entries, page_size=%d", len(resp.GetEntries()), size)
	}
	first := (wantPage-1)*size + 1
	var prev uint32
	for i, e := range resp.GetEntries() {
		if want := first + uint32(i); e.GetRank() != want {
			return fmt.Errorf("entry %d has rank %d, expected %d (ranks must be consecutive)", i, e.GetRank(), want)
		}
		if i > 0 && e.GetRatingCenti() > prev {
			return fmt.Errorf("entry %d rating %d is higher than the one above it (%d): the board must be sorted by rating descending", i, e.GetRatingCenti(), prev)
		}
		if e.GetPlayerId() == 0 {
			return fmt.Errorf("entry %d has player_id 0", i)
		}
		prev = e.GetRatingCenti()
	}
	return nil
}

// rankSmokeLogin 走完 AssignGate → 连接 → 登录 → 进场,挂上本场景自己的 RecvLoop 回调(与 mailSmokeLogin 同形)。
// 排行榜回包由本场景认领,其余消息(含战斗推送)交给通用分发,所以同一会话也能跑 runCrossZoneFighter。
func rankSmokeLogin(cfg *config.Config, account string, spacing time.Duration, stats *metrics.Stats) (*rankSmokeBot, error) {
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

	bot := &rankSmokeBot{
		account:        account,
		zone:           cfg.ZoneID,
		gc:             gc,
		player:         gameobject.NewPlayer(gc.PlayerId),
		stats:          stats,
		requestSpacing: spacing,
		replies:        make(map[uint32]*base.MessageContent),
	}
	bot.battle = &battleSmokeBot{account: account, gate: host + ":" + portStr, gc: gc, player: bot.player}
	gameobject.PlayerList.Set(gc.PlayerId, bot.player)
	go gc.RecvLoop(bot.onMessage)

	waitCtx, waitCancel := context.WithTimeout(context.Background(), rankSmokeSceneReadyTimeout)
	defer waitCancel()
	if err := bot.player.WaitSceneReady(waitCtx); err != nil {
		sendDisconnectBestEffort(gc)
		gameobject.PlayerList.Delete(gc.PlayerId)
		gc.Close()
		return nil, fmt.Errorf("wait scene ready: %w", err)
	}
	return bot, nil
}

// gateAddr 返回当前连着的 gate("ip:port";重定向后是新 gate)。
func (b *rankSmokeBot) gateAddr() string {
	ip, port := b.gc.GateAddr()
	return net.JoinHostPort(ip, strconv.Itoa(port))
}

// onMessage:GetRankPage 的回包(含信封拒绝)由本场景认领;其余交给通用分发。
// ⚠ 信封拒绝(error_message 非空、body 为空)落进 MessageBodyHandler 会被解成全零响应、看上去像受理 —— 所以必须先认领。
func (b *rankSmokeBot) onMessage(client *pkg.GameClient, msg *base.MessageContent) {
	b.stats.MsgRecv()
	if id := msg.GetMessageId(); id == game.ClientPlayerRankGetRankPageMessageId {
		b.mu.Lock()
		if b.awaiting == id {
			if _, seen := b.replies[id]; !seen {
				b.replies[id] = msg
			}
		}
		// 没人在等的回包 = 上一次请求超时之后才到的那一个,丢掉,免得被下一次请求误读成本次的回包。
		b.mu.Unlock()
		return
	}
	handler.MessageBodyHandler(client, msg)
}

// call 发一次 GetRankPage 并等第一个回包。envelopeTip 非 0 = gate / 路由服级拒绝(此时 resp 未填充)。
func (b *rankSmokeBot) call(req *rankpb.GetRankPageRequest, resp *rankpb.GetRankPageResponse, honourSpacing bool) (envelopeTip uint32, err error) {
	if honourSpacing {
		if wait := b.requestSpacing - time.Since(b.lastRequestAt); wait > 0 {
			time.Sleep(wait)
		}
	}
	// 生成的消息号是无类型常量,这里显式落成 uint32(SendRequest / replies 的键都是 uint32;同 chat_smoke_scenario.go)。
	id := uint32(game.ClientPlayerRankGetRankPageMessageId)
	b.mu.Lock()
	delete(b.replies, id)
	b.awaiting = id
	b.mu.Unlock()
	defer func() {
		b.mu.Lock()
		b.awaiting = 0
		b.mu.Unlock()
	}()
	if err := b.gc.SendRequest(id, req); err != nil {
		return 0, fmt.Errorf("send GetRankPage: %w", err)
	}
	b.lastRequestAt = time.Now()
	b.stats.MsgSent()

	deadline := time.Now().Add(rankSmokeRpcTimeout)
	for time.Now().Before(deadline) {
		b.mu.Lock()
		reply := b.replies[id]
		b.mu.Unlock()
		if reply != nil {
			if tip := reply.GetErrorMessage().GetId(); tip != 0 {
				return tip, nil
			}
			if err := proto.Unmarshal(reply.GetSerializedMessage(), resp); err != nil {
				return 0, fmt.Errorf("decode GetRankPage: %w", err)
			}
			return 0, nil
		}
		time.Sleep(50 * time.Millisecond)
	}
	return 0, fmt.Errorf("account=%s GetRankPage %w(%s)", b.account, errRankSmokeTimeout, rankSmokeRpcTimeout)
}

// page 读一页(按间隔节流);信封拒绝、超时、业务 tip 一律报错并给出可读原因。
func (b *rankSmokeBot) page(page, size uint32) (*rankpb.GetRankPageResponse, error) {
	resp := &rankpb.GetRankPageResponse{}
	envelopeTip, err := b.call(&rankpb.GetRankPageRequest{Board: rankpb.RankBoard_RANK_BOARD_MATCH_RATING, Page: page, PageSize: size}, resp, true)
	if err != nil {
		return nil, err
	}
	switch {
	case envelopeTip == tipRankRateLimited:
		return nil, fmt.Errorf("gate rate-limited GetRankPage (tip=%d): MessageLimiter not exported for rank yet, raise request_interval_ms", envelopeTip)
	case envelopeTip != 0:
		return nil, fmt.Errorf("gate/router envelope rejected GetRankPage tip=%d (tip=%d usually means the router has no rank instance or the "+
			"gate is not in router mode GATE_CLIENT_RPC_ROUTER=1)", envelopeTip, tipRankServiceUnavailable)
	}
	switch tip := resp.GetErrorMessage().GetId(); tip {
	case 0:
		return resp, nil
	case tipRankInvalidParameter:
		return nil, errors.New("rank rejected GetRankPage with kInvalidParameter: rank could not see the session identity " +
			"(check the router forwards x-session-detail-bin)")
	case tipRankServiceUnavailable:
		return nil, errors.New("rank answered kServiceUnavailable: the board Redis is unreachable (see rank logs 'board read failed')")
	default:
		return nil, fmt.Errorf("GetRankPage tip=%d", tip)
	}
}

// selfEntry 读第一页并返回 self(未上榜时 rank = 0)。
func (b *rankSmokeBot) selfEntry() (*rankpb.RankEntry, error) {
	resp, err := b.page(1, rankSmokeMaxPageSize)
	if err != nil {
		return nil, err
	}
	self := resp.GetSelf()
	if self == nil {
		self = &rankpb.RankEntry{}
	}
	if self.GetRank() != 0 && self.GetPlayerId() != b.gc.PlayerId {
		return nil, fmt.Errorf("self entry belongs to player %d, expected %d (rank must take \"me\" from the session, D-9)", self.GetPlayerId(), b.gc.PlayerId)
	}
	return self, nil
}

// expectRateLimited 不节流地连发 perSecond + 1 次,断言最后一次被 gate 限流、之前的都受理(R5)。
// 每次都等回包再发下一次(同功能单请求在途);整组要在一个限流窗口内发完,所以先等满一个窗口再开始。
func (b *rankSmokeBot) expectRateLimited(perSecond int) error {
	time.Sleep(2 * time.Second) // 让前面请求所在的窗口过去(窗口按秒粒度)
	for i := 1; i <= perSecond+1; i++ {
		resp := &rankpb.GetRankPageResponse{}
		envelopeTip, err := b.call(&rankpb.GetRankPageRequest{Board: rankpb.RankBoard_RANK_BOARD_MATCH_RATING, Page: 1, PageSize: 5}, resp, false)
		if err != nil {
			return fmt.Errorf("request %d: %v", i, err)
		}
		limited := envelopeTip == tipRankRateLimited
		if i <= perSecond && limited {
			return fmt.Errorf("request %d of %d was already rate-limited: MessageLimiter for GetRankPage is not %d/s (default tier is 3)", i, perSecond+1, perSecond)
		}
		if i <= perSecond && (envelopeTip != 0 || resp.GetErrorMessage().GetId() != 0) {
			// 窗口内的前 perSecond 次必须真被受理:别的信封错误(路由 / 熔断)或业务 tip 会让"最后一次被限流"的结论不成立。
			return fmt.Errorf("request %d of %d was not accepted: envelope tip=%d, response tip=%d", i, perSecond+1, envelopeTip, resp.GetErrorMessage().GetId())
		}
		if i == perSecond+1 && !limited {
			return fmt.Errorf("request %d within one window was not rate-limited (envelope tip=%d): MessageLimiter row for GetRankPage missing?", i, envelopeTip)
		}
	}
	return nil
}
