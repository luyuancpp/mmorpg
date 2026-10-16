package logic

import (
	"context"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"time"

	"match/internal/constants"
	"match/internal/pkg/ctxkeys"
	"match/internal/playercontract"
	"match/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

// ticket 状态机(QueueState 五态映射见 getqueuestatuslogic.go):
//
//	queued  -> 在 Redis 队列里等待凑单(QUEUE_STATE_QUEUED)
//	matched -> 已被 matcher 弹出,gather 管线执行中(QUEUE_STATE_MATCHED,短 TTL 自愈)
//	ready   -> CreateBattle 成功,等 battle 推 BattleStartS2C(QUEUE_STATE_READY,短 TTL 自清)
//
// ticket 不存在 = QUEUE_STATE_NOT_QUEUED。ENTERING 是场景类匹配预留的进场态,
// 回合制战斗(含二期 5v5)没有进场步骤,不产生该状态。
const (
	ticketStateQueued  = "queued"
	ticketStateMatched = "matched"
	ticketStateReady   = "ready"
)

// ticket hash 字段名。
const (
	ticketFieldTicket     = "ticket"
	ticketFieldMode       = "mode"
	ticketFieldConfig     = "config"
	ticketFieldState      = "state"
	ticketFieldEnqueuedAt = "enqueued_at_ms"
	ticketFieldBattleId   = "battle_id"
	ticketFieldZoneId     = "zone_id"
	ticketFieldQueueKey   = "queue_key"
	ticketFieldRating     = "rating"
	// ticketFieldTeamId 整队开战(team-system.md §E.1 第 7 步)建票时写入的 team_id,
	// 只用于观测;非组队票据不写该字段(读回为 0)。
	ticketFieldTeamId = "team_id"
	// ticketFieldPartyId / ticketFieldPartyMembers 队伍补位票(team-backfill-matching.md §3.1)才写:
	// 同一单的 uuid,以及这一单的名单(逗号分隔的十进制 player_id,队长在前,即进战斗的站位顺序)。
	ticketFieldPartyId      = "party_id"
	ticketFieldPartyMembers = "party_members"
	// ticketFieldLastFailedBattle 上一局开局失败的 battle_id(gather 的失败出口回队首时写)。scene 的备战锁
	// battle:lock 的值就是 battle_id:matcher 看到锁值等于它,就知道这是那一局没清掉的残留锁,而不是
	// 「他进了别的战斗」(见 classifyBattleLock)。没经历过开局失败的票据没有该字段。
	ticketFieldLastFailedBattle = "last_failed_battle_id"
)

// queueTicket 是 match:ticket:{player_id} 的内存镜像。
type queueTicket struct {
	Ticket       string
	Mode         int32
	Config       uint32
	State        string
	EnqueuedAtMs uint64
	// ZoneId 入队时刻玩家所在 zone(取自 player:{id}:location),只用于
	// 可观测性与对局日志 —— 匹配池全局不分 zone(设计决策 D1/D4)。
	ZoneId uint32
	// QueueKey 入队时写入的队列 key:取消 / 回队首直接用它,不再重算,
	// key 格式演进时旧票据仍能正确出队(设计决策 D4)。PVE_SOLO 不入队,为空。
	QueueKey string
	// Rating 入队时刻读到的评分(§11):回队首时按它写回评分镜像 ZSET,
	// 日志 / 对局记录也用它。旧票据没有该字段,按 defaultRating。
	Rating float64
	// TeamId 整队开战(ClientPlayerTeam.StartTeamMatch)建票时的队伍 id,只用于观测
	// (team-system.md §C.1 / §E.5);单人排队 / PVE_SOLO 为 0,且不落盘该字段。
	// 队伍补位票也写它:撤单 / 弹组前复核时拿它去问 team 域。
	TeamId uint64
	// PartyId 非空 = 队伍补位票(team-backfill-matching.md):同一单的全部成员相同,matcher 据此把他们
	// 当成一个不可拆的单位。PartyMembers 是这一单的名单(队长在前、其余按 join_seq),含本人。
	// 单人排队 / 整队直接开战 / 活动票都不写这两个字段。
	PartyId      string
	PartyMembers []uint64
	// LastFailedBattleId 上一局开局失败的 battle_id;0 = 没有。
	LastFailedBattleId uint64
}

// enqueueScript / requeueScript:注册集 SADD、队列 push 与评分镜像 ZADD 一条 Lua
// 原子完成(KEYS=[index, queue, rank, versions, sequence],ARGV=[queueKey, playerId, rating];所有 key
// 同 {mq} slot)。分步做会留下"队列存在但不在注册集"的窗口:matcher 的空队列
// 剔除若恰好落在中间,该队列就再也不会被扫到;list 与 ZSET 分步写则任一步
// 崩溃 / 故障切换都会让两者脱节。回队首用 LPUSH 保持原相对顺序,ZADD 写回
// 原评分(ZSET 不表达顺序,只表达评分)。每次实际执行都分配新版本并 LREM 同 pid 的旧项,
// 透明重试也刷新版本且不留重复项。INCR 后 GET 保留原始十进制字符串,避免 Lua 浮点舍入。
const (
	enqueueScript = `redis.call("INCR", KEYS[5])
local version = redis.call("GET", KEYS[5])
redis.call("HSET", KEYS[4], ARGV[2], version)
redis.call("SADD", KEYS[1], ARGV[1])
redis.call("ZADD", KEYS[3], ARGV[3], ARGV[2])
redis.call("LREM", KEYS[2], 0, ARGV[2])
return redis.call("RPUSH", KEYS[2], ARGV[2])`
	requeueScript = `redis.call("INCR", KEYS[5])
local version = redis.call("GET", KEYS[5])
redis.call("HSET", KEYS[4], ARGV[2], version)
redis.call("SADD", KEYS[1], ARGV[1])
redis.call("ZADD", KEYS[3], ARGV[3], ARGV[2])
redis.call("LREM", KEYS[2], 0, ARGV[2])
return redis.call("LPUSH", KEYS[2], ARGV[2])`
)

// removeQueueMembersScript 把一批成员从队列 list、评分镜像与版本 hash 原子摘出。
// KEYS=[queue, rank, versions],ARGV=[guarded, member...] 或 ["1", member, expectedVersion...]。
// guarded=1 时仅摘版本仍等于快照的项:撤单删票后成员可能已成功重排,不能摘掉新项。
// 无版本的静态遗留项以空字符串比较,新 push 一定写非空版本;不支持旧实例继续无版本写入。
// 逐个 LREM 0(同一玩家的重复项一并
// 清掉,组内只留一份)+ ZREM,返回**实际从 list 摘出**的成员列表 —— 校验与
// 摘出之间被 CancelQueue 摘走的人不在其中,调用方据此判断组是否凑齐。
// 取消出队 / 无效成员剔除 / 弹组摘出三处共用,三 key 同 {mq} slot。
const removeQueueMembersScript = `local removed = {}
local guarded = ARGV[1] == "1"
local step = guarded and 2 or 1
for i = 2, #ARGV, step do
	local member = ARGV[i]
	if not guarded or (redis.call("HGET", KEYS[3], member) or "") == ARGV[i + 1] then
		if redis.call("LREM", KEYS[1], 0, member) > 0 then
			removed[#removed + 1] = member
		end
		redis.call("ZREM", KEYS[2], member)
		redis.call("HDEL", KEYS[3], member)
	end
end
return removed`

// queueMemberVersionsScript 必须在撤单读/删任何成员票据之前执行;票据与队列不在同一 slot,
// 因此只快照队列侧版本,不把 ticket key 放进这条 Lua。每次实际 push 都用 INCR 分配新版本,
// 包括 Redis 客户端重试原 Lua 的执行,避免复用 Go 调用参数里的版本产生 ABA。
const queueMemberVersionsScript = `local versions = {}
for i = 1, #ARGV do
	versions[#versions + 1] = redis.call("HGET", KEYS[1], ARGV[i]) or ""
end
return versions`

// queueSnapshotScript 读队列前 N 个成员及其在评分镜像里的 score
// (KEYS=[queue, rank],ARGV=[limit]),返回扁平数组 [member, score, member, score...],
// 镜像里没有的成员 score 为空串(滚动升级窗口内旧实例只写 list 不写 ZSET,
// matcher 据此补写)。一次往返拿到等待序与评分,弹组选人在 Go 侧完成。
const queueSnapshotScript = `local members = redis.call("LRANGE", KEYS[1], 0, tonumber(ARGV[1]) - 1)
local out = {}
for i, m in ipairs(members) do
	out[#out + 1] = m
	local s = redis.call("ZSCORE", KEYS[2], m)
	if s == false then s = "" end
	out[#out + 1] = s
end
return out`

// ticketCasScript 票据状态的带 ticket id 的 CAS 写(设计决策 D5):
// KEYS=[ticketKey],ARGV=[expectedTicketId, field, value, ttlSeconds(0=不改), (field, value)...]。
// 只有 hash 里的 ticket 字段等于 expected 才 HSET(+EXPIRE),返回 1;票据已过期
// (HGET 返回 nil)或已被玩家重排替换成新 ticket 时返回 0,什么都不写。
// matched 态短 TTL 让崩溃实例的票据自灭、玩家可重排;若旧实例其实没死只是
// gather 拖得久,它迟到的 matched / ready / 回队首写入不能盖到新票据上。
// ARGV[5..] 是可选的额外 (field, value) 对,供 ready 同时写 state 与 battle_id。
const ticketCasScript = `if redis.call("HGET", KEYS[1], "` + ticketFieldTicket + `") ~= ARGV[1] then
	return 0
end
redis.call("HSET", KEYS[1], ARGV[2], ARGV[3])
for i = 5, #ARGV, 2 do
	redis.call("HSET", KEYS[1], ARGV[i], ARGV[i + 1])
end
if tonumber(ARGV[4]) > 0 then
	redis.call("EXPIRE", KEYS[1], ARGV[4])
end
return 1`

// ticketDelCasScript 带 ticket id 的删票(KEYS=[ticketKey],ARGV=[expectedTicketId]):
// gather 失败路径删肇事者票据时用,同样不能误删玩家重排后的新票据。
const ticketDelCasScript = `if redis.call("HGET", KEYS[1], "` + ticketFieldTicket + `") ~= ARGV[1] then
	return 0
end
return redis.call("DEL", KEYS[1])`

// ticketCancelScript 取消排队的删票(KEYS=[ticketKey],ARGV=[expectedTicketId]):
// ticket id 一致 **且 state 仍是 queued** 才 DEL,返回 1;否则返回 0 什么都不写。
// "matched 之后取消太迟"必须在存储层成立:CancelQueue 的 loadTicket 与 DEL 之间,
// matcher 可能已把该玩家弹出并推进 matched,若此时无条件 DEL,玩家会在"取消成功"
// 后被一场他已退出的战斗冻结(setTicketMatched 的 CAS 失败只是日志)。
const ticketCancelScript = `if redis.call("HGET", KEYS[1], "` + ticketFieldTicket + `") ~= ARGV[1] then
	return 0
end
if redis.call("HGET", KEYS[1], "` + ticketFieldState + `") ~= "` + ticketStateQueued + `" then
	return 0
end
return redis.call("DEL", KEYS[1])`

// ticketCreateScript "不存在才创建"的票据写入(KEYS=[ticketKey],
// ARGV=[ttlSeconds, field, value, (field, value)...]):EXISTS 为 1 直接返回 0,
// 否则 HSET 全字段 + EXPIRE 返回 1。JoinQueue 的"读票判存在"与"写票"合成一条
// 原子命令 —— 同一玩家的两条 JoinQueue 落到两个实例时,分开做会各写一张票、
// 各 RPUSH 一次,队列里出现两份,弹组后第二次 PrepareBattle 被 scene 拒,整组
// 白冻结一轮。单 key,集群安全。
const ticketCreateScript = `if redis.call("EXISTS", KEYS[1]) == 1 then
	return 0
end
for i = 2, #ARGV, 2 do
	redis.call("HSET", KEYS[1], ARGV[i], ARGV[i + 1])
end
redis.call("EXPIRE", KEYS[1], ARGV[1])
return 1`

// queueContainsScript 队列成员存在性探测(KEYS=[queueKey],ARGV=[playerId]):
// LPOS(Redis ≥ 6.0.6;部署为 7.2)单 key O(N),队列很短。JoinQueue 用它识别
// "票据 queued 但队列里没有人"的崩溃残留(见 isQueuedTicketInQueue)。
const queueContainsScript = `if redis.call("LPOS", KEYS[1], ARGV[1]) == false then
	return 0
end
return 1`

// ticketSetQueueKeyScript 给已存在的票据补 queue_key(KEYS=[ticketKey],
// ARGV=[queueKey]):票据不存在时不写,避免 HSET 凭空造出一个没有 TTL 的 hash。
// 旧队列迁移(migrateLegacyQueues)用。
const ticketSetQueueKeyScript = `if redis.call("EXISTS", KEYS[1]) == 0 then
	return 0
end
return redis.call("HSET", KEYS[1], "` + ticketFieldQueueKey + `", ARGV[1])`

// 测试注入点(生产恒为 nil):把并发时序里的"另一实例"插进两步之间,证明
// 调换顺序后不再产生孤儿票据。照 runGatherFn / setQueueDepthFn 的包级变量模式。
//
//	requeueFrontHook  requeueFront 已把票据 CAS 回 queued、尚未 LPUSH 时调用;
//	matcherAfterPopHook  popGroup 弹出成组、尚未 setTicketMatched 时调用;
//	beforeAcquireWatchingHook  WatchBattle 已过入口互斥检查、尚未 SETNX 抢观战标记时调用
//	  (那条 ErrAlreadyWatching 出口只在真并发下可达,靠这个钩子做成确定性用例)。
var (
	requeueFrontHook          func(playerId uint64)
	matcherAfterPopHook       func(members []uint64)
	beforeAcquireWatchingHook func(playerId uint64)
)

// authoritativePlayerID 取权威 player_id:客户端直达协议(gate gRPC)时以
// x-session-detail-bin 里的身份为准,请求体里的 player_id 仅供内部调用使用
// (照 login 的 session metadata 模式)。带了 session 但与请求体不一致时记日志。
func authoritativePlayerID(ctx context.Context, reqPlayerId uint64) uint64 {
	if detail, ok := ctxkeys.GetSessionDetails(ctx); ok && detail.GetPlayerId() != 0 {
		if reqPlayerId != 0 && reqPlayerId != detail.GetPlayerId() {
			logx.Errorf("[match] Request player_id=%d differs from authoritative session identity %d; using session identity",
				reqPlayerId, detail.GetPlayerId())
		}
		return detail.GetPlayerId()
	}
	return reqPlayerId
}

// isPlayerBattleLocked 咨询性检查 battle:lock:{player_id}(scene 写,契约 key
// 走 SharedRedis)。权威判定仍在 scene 的 InBattleComp;这里只是提前挡明显
// 不合法的请求。Redis 出错时按"有锁"处理(fail-closed:宁可拒绝排队,不可放进双战斗)。
// 实现收口在 playercontract.IsBattleLocked(team-system.md §A.2);传 context.Background(),
// 与改造前 go-zero 无 ctx 的 Exists 行为一致。
func isPlayerBattleLocked(svcCtx *svc.ServiceContext, playerId uint64) (bool, error) {
	return playercontract.IsBattleLocked(context.Background(), svcCtx, playerId)
}

// playerBattleLockValue 读 battle:lock:{player_id} 的值(battle_id;没有锁为空串),见 playercontract.BattleLockValue。
func playerBattleLockValue(svcCtx *svc.ServiceContext, playerId uint64) (string, error) {
	return playercontract.BattleLockValue(context.Background(), svcCtx, playerId)
}

// loadTicket 读取玩家 ticket;不存在返回 (nil, nil)。
func loadTicket(svcCtx *svc.ServiceContext, playerId uint64) (*queueTicket, error) {
	return loadTicketCtx(context.Background(), svcCtx, playerId)
}

// loadTicketCtx 同 loadTicket,Redis 往返受调用方 ctx 约束(组队 RPC 的请求预算内读票)。
func loadTicketCtx(ctx context.Context, svcCtx *svc.ServiceContext, playerId uint64) (*queueTicket, error) {
	fields, err := svcCtx.MatchRedis.HgetallCtx(ctx, matchTicketKey(playerId))
	if err != nil {
		if errors.Is(err, redis.Nil) {
			return nil, nil
		}
		return nil, err
	}
	if len(fields) == 0 {
		return nil, nil
	}
	mode, _ := strconv.ParseInt(fields[ticketFieldMode], 10, 32)
	config, _ := strconv.ParseUint(fields[ticketFieldConfig], 10, 32)
	enqueuedAt, _ := strconv.ParseUint(fields[ticketFieldEnqueuedAt], 10, 64)
	zoneId, _ := strconv.ParseUint(fields[ticketFieldZoneId], 10, 32)
	teamId, _ := strconv.ParseUint(fields[ticketFieldTeamId], 10, 64)
	lastFailedBattle, _ := strconv.ParseUint(fields[ticketFieldLastFailedBattle], 10, 64)
	return &queueTicket{
		Ticket:             fields[ticketFieldTicket],
		Mode:               int32(mode),
		Config:             uint32(config),
		State:              fields[ticketFieldState],
		EnqueuedAtMs:       enqueuedAt,
		ZoneId:             uint32(zoneId),
		QueueKey:           fields[ticketFieldQueueKey],
		Rating:             parseRating(fields[ticketFieldRating]),
		TeamId:             teamId,
		PartyId:            fields[ticketFieldPartyId],
		PartyMembers:       parsePartyMembers(fields[ticketFieldPartyMembers]),
		LastFailedBattleId: lastFailedBattle,
	}, nil
}

// createTicketIfAbsent 不存在才创建 ticket hash 并挂 TTL(见 ticketCreateScript)。
// 返回 (创建了, err);false 表示已有票据(并发 JoinQueue 的后来者),调用方按
// ErrAlreadyQueued 处理。ttl 由调用方给:排队用长 TTL(只挡进程崩溃等异常残留,
// 正常路径由取消/开战收尾),PVE_SOLO 直接给 matched 短 TTL。
func createTicketIfAbsent(svcCtx *svc.ServiceContext, playerId uint64, t *queueTicket, ttl int) (bool, error) {
	return createTicketIfAbsentCtx(context.Background(), svcCtx, playerId, t, ttl)
}

// createTicketIfAbsentCtx 同 createTicketIfAbsent,Redis 往返受调用方 ctx 约束
// (整队开战在请求预算内建票,team-system.md §A.3 第 7 条)。无 ctx 版本传 Background,
// 与 go-zero Eval 行为完全一致。t.TeamId 非 0 才多写 team_id 字段,t.PartyId 非空才多写补位单的两个字段,
// 其余票据的字段集不变。
// 出错时服务端是否已写入未知(超时 / 断连 / 集群重试),调用方回滚时要把它算进去。
func createTicketIfAbsentCtx(ctx context.Context, svcCtx *svc.ServiceContext, playerId uint64, t *queueTicket, ttl int) (bool, error) {
	args := []any{
		strconv.Itoa(ttl),
		ticketFieldTicket, t.Ticket,
		ticketFieldMode, strconv.FormatInt(int64(t.Mode), 10),
		ticketFieldConfig, strconv.FormatUint(uint64(t.Config), 10),
		ticketFieldState, t.State,
		ticketFieldEnqueuedAt, strconv.FormatUint(t.EnqueuedAtMs, 10),
		ticketFieldZoneId, strconv.FormatUint(uint64(t.ZoneId), 10),
		ticketFieldQueueKey, t.QueueKey,
		ticketFieldRating, formatRating(t.Rating),
	}
	if t.TeamId != 0 {
		args = append(args, ticketFieldTeamId, strconv.FormatUint(t.TeamId, 10))
	}
	if t.PartyId != "" {
		args = append(args, ticketFieldPartyId, t.PartyId, ticketFieldPartyMembers, formatPartyMembers(t.PartyMembers))
	}
	res, err := svcCtx.MatchRedis.EvalCtx(ctx, ticketCreateScript, []string{matchTicketKey(playerId)}, args...)
	if err != nil {
		return false, err
	}
	n, _ := res.(int64)
	return n == 1, nil
}

// ticketQueueKeys 返回该票据的成员可能所在的队列 key:有 queue_key 就只看它;
// 旧票据(无该字段)新旧两种格式都看(见 keys.go legacyMatchQueueKey)。
func ticketQueueKeys(t *queueTicket) []string {
	if t.QueueKey != "" {
		return []string{t.QueueKey}
	}
	return []string{matchQueueKey(t.Mode, t.Config), legacyMatchQueueKey(t.Mode, t.Config)}
}

// isQueuedTicketInQueue 探测 queued 态票据的玩家是否真的在队列里(见 queueContainsScript)。
// 票据与队列分属不同 slot/master,没有跨 slot 原子性:集群故障切换丢掉队列里的
// RPUSH、实例在 popGroup 弹出与 setTicketMatched 之间崩溃、requeueFront 在 CAS 与
// LPUSH 之间崩溃,都会留下"票据 queued、队列无人"的残留 —— matcher 永远弹不到他,
// JoinQueue 一直 ErrAlreadyQueued,直到 6h 过期。JoinQueue 据此自愈。
func isQueuedTicketInQueue(svcCtx *svc.ServiceContext, playerId uint64, t *queueTicket) (bool, error) {
	member := strconv.FormatUint(playerId, 10)
	for _, key := range ticketQueueKeys(t) {
		res, err := svcCtx.MatchRedis.Eval(queueContainsScript, []string{key}, member)
		if err != nil {
			return false, err
		}
		if n, _ := res.(int64); n == 1 {
			return true, nil
		}
	}
	return false, nil
}

// cancelTicketIfQueued 取消排队的删票:ticket id 一致且仍是 queued 才删
// (见 ticketCancelScript)。返回 (删了, err)。无 ctx 版本给票据自愈等内部路径用,传 Background。
func cancelTicketIfQueued(svcCtx *svc.ServiceContext, playerId uint64, expectedTicket string) (bool, error) {
	return cancelTicketIfQueuedCtx(context.Background(), svcCtx, playerId, expectedTicket)
}

// cancelTicketIfQueuedCtx 同 cancelTicketIfQueued,Redis 往返受调用方 ctx 约束。客户端 CancelQueue 传请求 ctx:
// gate 对同一会话单飞,路由服回超时之后会放行下一条请求,请求 ctx 已取消时不能再发出删票
// (docs/design/client-request-ordering.md §6.1)。
func cancelTicketIfQueuedCtx(ctx context.Context, svcCtx *svc.ServiceContext, playerId uint64, expectedTicket string) (bool, error) {
	res, err := svcCtx.MatchRedis.EvalCtx(ctx, ticketCancelScript, []string{matchTicketKey(playerId)}, expectedTicket)
	if err != nil {
		return false, err
	}
	n, _ := res.(int64)
	return n == 1, nil
}

// ticketTTLSeconds 带缺省值的长 TTL 配置读取
// (直接构造 Config 的测试与漏配 yaml 都不会得到 0 TTL);matched 态见 matchedTicketTTLFor。
func ticketTTLSeconds(svcCtx *svc.ServiceContext) int {
	if ttl := int(svcCtx.Config.TicketTTLSeconds); ttl > 0 {
		return ttl
	}
	return 21600
}

// gatherCreateStageWorst 是 gather 从取 created_at_ms、写观战记录起,到最后一次 CreateBattle 返回
// 为止的最坏耗时,按 k8s-client-entry D82 换节点重试路径算(常规路径只走前一半):
//
//	写观战记录(spectateRecordWriteWorst)+ 首选节点 CreateBattle 被准入拒绝(createBattleTimeout)
//	+ 改写记录指向重试节点(spectateRecordWriteWorst)+ 重试节点 CreateBattle(createBattleTimeout)
//	= 2 × (6.1s + 5s) = 22.2s
//
// 准入拒绝通常在 battle 的分配许可等待(3s)内就回,这里仍按 createBattleTimeout 计:match 侧能保证的
// 上限只有 RPC 超时。两处引用:matchedTicketTTLFor 的预算,以及 WatchBattle 判定"记录已写、房间可能
// 还在建"的窗口(watchbattlelogic.go roomMayBeCreating)。
const gatherCreateStageWorst = 2 * (spectateRecordWriteWorst + createBattleTimeout)

// matchedTicketTTLFor 按组大小算 matched 态 TTL(设计决策 D5):
//
//	max(MatchedTicketTTLSeconds,
//	    ⌈required × (removeObserverTimeout + prepareBattleTimeout)
//	     + gatherCreateStageWorst + rollbackTimeout⌉ + 10s)
//
// 覆盖 gather 从弹组到"成功写 ready"或"进入补偿"之前的最坏链路:每人先观战
// 清退(RemoveObserver 阻塞 RPC,spectate.go)再串行 PrepareBattle;全齐后先写观战
// 记录再 CreateBattle,被节点级准入拒绝(D82)时改写记录、换节点再建一次
// (gatherCreateStageWorst);最后一次 CreateBattle 失败还要先 DestroyBattle
// (rollbackTimeout)才进 fail。10s 余量吸收没单列的 Redis 小操作(读位置、推进 ready)。
// 5v5 十人 = ⌈10×6 + 22.2 + 3⌉ + 10 = 96s。只算 prepare+create 的更早公式(45s)
// 会让票据在仍在跑的 gather 途中过期(复审 2026-09-02);不计写记录与 D82 的上一版(78s)
// 同样盖不住 D82 最坏链路(85.2s)。
// 同一个值也是 scene 侧 PREPARING 的作废期限(gather.go prepare_deadline_ms,PrepareBattle 时
// 已下发、事后改不了):预算偏小时 scene 可能在重试节点建房之前就按期限解冻玩家,所以 D82 路径
// 必须算进公式,不能指望事后给票据续期。
// 下游依赖(改公式或上调 MatchedTicketTTLSeconds 时一并核对):scene 备战锁 EX = 该期限 +
// PlayerBattleSystem::kLockExtraTtlSec(60s),十人组锁最长到 gather 起点 +156s;battle 的确认事件
// 补发窗口 kConfirmResendWindowMs(battle_room_manager.cpp)从建房起算,必须 ≥ 这个值。
// 补偿路径(逐人 CancelBattlePrepare)不计入这里:fail 闭包进入补偿前先用 CAS 把幸存者
// 票据续期(extendMatchedTickets / compensationTicketTTLFor),TTL 不必一次覆盖全部。
// 配置值只是下限,组越大窗口越长。常量与 gather.go / spectate.go 同源,不另写数字。
func matchedTicketTTLFor(svcCtx *svc.ServiceContext, required uint32) int {
	ttl := int(svcCtx.Config.MatchedTicketTTLSeconds)
	if ttl <= 0 {
		ttl = 30
	}
	chain := time.Duration(required)*(removeObserverTimeout+prepareBattleTimeout) +
		gatherCreateStageWorst + rollbackTimeout
	// 向上取整到秒:spectateRecordWriteWorst 带 100ms 零头,截断会把预算算小。
	worst := int((chain+time.Second-1)/time.Second) + 10
	if worst > ttl {
		return worst
	}
	return ttl
}

// compensationTicketTTLFor 补偿路径的票据续期窗口:已冻结者逐人
// CancelBattlePrepare(每人最长 rollbackTimeout)+ 回队首余量 10s。
func compensationTicketTTLFor(prepared int) int {
	return prepared*int(rollbackTimeout/time.Second) + 10
}

// extendMatchedTickets 进入补偿前给幸存者的 matched 票据续期(CAS,ticket id 不一致
// 或已过期不写)。不续期的话 10 人组的 destroy + 逐人解冻(≈33s)会让票据在
// requeueFront 之前过期,requeueFront 逐人 loadTicket 得 nil → 十个人全部从队列
// 消失且没有票据。返回续期成功的人数。
func extendMatchedTickets(svcCtx *svc.ServiceContext, members []uint64, tickets map[uint64]string, ttl int) int {
	extended := 0
	for _, playerId := range members {
		expected, known := tickets[playerId]
		if !known {
			continue
		}
		written, err := casTicketFields(svcCtx, playerId, expected, ttl, ticketFieldState, ticketStateMatched)
		if err != nil {
			logx.Errorf("[match] Failed to renew ticket before compensation player=%d: %v", playerId, err)
			continue
		}
		if !written {
			logx.Infof("[match] Ticket replaced or expired; skipping compensation renewal player=%d expected=%s", playerId, expected)
			continue
		}
		extended++
	}
	return extended
}

// casTicketFields 带 ticket id 的票据字段 CAS 写(见 ticketCasScript):
// fieldValues 形如 field, value[, field, value...],ttl>0 时一并 EXPIRE。
// 返回 (写入了, err);false 表示票据已过期或已被玩家重排替换,调用方只记 Info 不覆盖。
func casTicketFields(svcCtx *svc.ServiceContext, playerId uint64, expectedTicket string,
	ttl int, fieldValues ...string,
) (bool, error) {
	if len(fieldValues) < 2 || len(fieldValues)%2 != 0 {
		return false, fmt.Errorf("casTicketFields 参数必须是 (field, value) 对,实得 %d 个", len(fieldValues))
	}
	args := make([]any, 0, 2+len(fieldValues))
	args = append(args, expectedTicket, fieldValues[0], fieldValues[1], strconv.Itoa(ttl))
	for _, v := range fieldValues[2:] {
		args = append(args, v)
	}
	res, err := svcCtx.MatchRedis.Eval(ticketCasScript, []string{matchTicketKey(playerId)}, args...)
	if err != nil {
		return false, err
	}
	n, _ := res.(int64)
	return n == 1, nil
}

// enqueueAtomic 把玩家挂到队尾、写评分镜像并把队列 key 登记进注册集
// (一条 Lua,见 enqueueScript)。rating 由调用方在 Go 侧读出传入:评分 key
// 按玩家分布,与队列不同 slot,Lua 里不能 GET。
func enqueueAtomic(svcCtx *svc.ServiceContext, queueKey string, playerId uint64, rating float64) error {
	return enqueueAtomicCtx(context.Background(), svcCtx, queueKey, playerId, rating)
}

// enqueueAtomicCtx 同 enqueueAtomic,Redis 往返受调用方 ctx 约束。客户端 JoinQueue 传请求 ctx,
// 截止之后不再入队(docs/design/client-request-ordering.md §6.1)。
func enqueueAtomicCtx(ctx context.Context, svcCtx *svc.ServiceContext, queueKey string, playerId uint64, rating float64) error {
	rankKey, err := rankKeyForQueue(queueKey)
	if err != nil {
		return err
	}
	_, err = svcCtx.MatchRedis.EvalCtx(ctx, enqueueScript, []string{matchQueueIndexKey, queueKey, rankKey, queueVersionsKey(queueKey), matchQueueVersionSequenceKey},
		queueKey, strconv.FormatUint(playerId, 10), formatRating(rating))
	return err
}

// pushFrontRegistered 把一个原始队列元素放回队首、按原评分写回镜像,并保证
// 队列在注册集里(凑不满回队首 / 旧队列迁移共用;队列弹空后 key 会消失,
// 此刻别的实例可能已把它从注册集剔除,单独 LPUSH 会造出孤儿队列)。
func pushFrontRegistered(svcCtx *svc.ServiceContext, queueKey string, raw string, rating float64) error {
	rankKey, err := rankKeyForQueue(queueKey)
	if err != nil {
		return err
	}
	_, err = svcCtx.MatchRedis.Eval(requeueScript, []string{matchQueueIndexKey, queueKey, rankKey, queueVersionsKey(queueKey), matchQueueVersionSequenceKey},
		queueKey, raw, formatRating(rating))
	return err
}

// removeQueueMembers 把一批成员从队列 list 与评分镜像 ZSET 原子摘出(见
// removeQueueMembersScript),返回实际从 list 摘出的成员。
func removeQueueMembers(svcCtx *svc.ServiceContext, queueKey string, members []uint64) ([]uint64, error) {
	return removeQueueMembersCtx(context.Background(), svcCtx, queueKey, members)
}

// removeQueueMembersCtx 同 removeQueueMembers,Redis 往返受调用方 ctx 约束。
func removeQueueMembersCtx(ctx context.Context, svcCtx *svc.ServiceContext, queueKey string, members []uint64) ([]uint64, error) {
	return removeQueueMembersVersionedCtx(ctx, svcCtx, queueKey, members, nil)
}

// loadQueueMemberVersions 读取撤单的队列版本快照。失败时调用方只能放弃主动摘项,不能降级成无条件删除。
func loadQueueMemberVersions(svcCtx *svc.ServiceContext, queueKey string, members []uint64) (map[uint64]string, error) {
	args := make([]any, 0, len(members))
	for _, pid := range members {
		args = append(args, strconv.FormatUint(pid, 10))
	}
	res, err := svcCtx.MatchRedis.Eval(queueMemberVersionsScript, []string{queueVersionsKey(queueKey)}, args...)
	if err != nil {
		return nil, err
	}
	raws, ok := res.([]any)
	if !ok || len(raws) != len(members) {
		return nil, fmt.Errorf("队列版本快照数量不符: want=%d result=%v", len(members), res)
	}
	versions := make(map[uint64]string, len(members))
	for i, pid := range members {
		version, ok := raws[i].(string)
		if !ok {
			return nil, fmt.Errorf("队列版本类型无效 player=%d", pid)
		}
		versions[pid] = version
	}
	return versions, nil
}

// removeQueueMembersVersionedCtx 的 versions 非 nil 时按快照摘项,未出现在快照里的成员不删除。
func removeQueueMembersVersionedCtx(ctx context.Context, svcCtx *svc.ServiceContext, queueKey string, members []uint64, versions map[uint64]string) ([]uint64, error) {
	rankKey, err := rankKeyForQueue(queueKey)
	if err != nil {
		return nil, err
	}
	args := make([]any, 0, 1+2*len(members))
	if versions == nil {
		args = append(args, "0")
	} else {
		args = append(args, "1")
	}
	for _, pid := range members {
		if versions != nil {
			version, present := versions[pid]
			if present {
				args = append(args, strconv.FormatUint(pid, 10), version)
			}
			continue
		}
		args = append(args, strconv.FormatUint(pid, 10))
	}
	res, err := svcCtx.MatchRedis.EvalCtx(ctx, removeQueueMembersScript, []string{queueKey, rankKey, queueVersionsKey(queueKey)}, args...)
	if err != nil {
		return nil, err
	}
	raws, _ := res.([]any)
	removed := make([]uint64, 0, len(raws))
	for _, raw := range raws {
		s, _ := raw.(string)
		pid, err := strconv.ParseUint(s, 10, 64)
		if err != nil {
			continue
		}
		removed = append(removed, pid)
	}
	return removed, nil
}

// dequeueMemberCtx 取消 / 剔除时把单个玩家从队列摘出。新格式 key(带 hash tag,
// 有评分镜像)走 list+ZSET 原子 Lua;旧格式 key(`match:queue:<mode>:<config>`,
// 无 tag、从未有过镜像)只做单 key LREM —— 旧 key 与按它推出的 rank key 不同
// slot,集群下不能放进同一条 Lua。
//
// Redis 往返受调用方 ctx 约束。客户端 CancelQueue 传请求 ctx:
// 出队如果晚于 gate 放行的下一条 JoinQueue 落地,会把新入队的同一玩家 LREM 掉,
// 留下「票据 queued、队列无人」的孤儿(docs/design/client-request-ordering.md §6.1)。
// 截止之后不发出时只留下没有票的残留队列项,由 matcher 的票据校验摘掉,是良性的。
func dequeueMemberCtx(ctx context.Context, svcCtx *svc.ServiceContext, queueKey string, playerId uint64) error {
	if !strings.Contains(queueKey, matchQueueHashTag) {
		_, err := svcCtx.MatchRedis.LremCtx(ctx, queueKey, 0, strconv.FormatUint(playerId, 10))
		return err
	}
	_, err := removeQueueMembersCtx(ctx, svcCtx, queueKey, []uint64{playerId})
	return err
}

// setTicketMatched 把 ticket 推进 matched 态并收紧到短 TTL(设计决策 D5):
// 弹组实例若在 gather 中崩溃,票据在 ttl 秒后自灭,玩家可重排;ttl 由调用方按
// 组大小算(matchedTicketTTLFor)。带 expectedTicket 的 CAS 写:弹出时读到的
// ticket id 与当前不一致(已过期被重排)则不写,返回 false。
// 正常路径由 markTicketReady(成功)或 requeueFront / deleteTicket(失败)接管。
func setTicketMatched(svcCtx *svc.ServiceContext, playerId uint64, expectedTicket string, ttl int) (bool, error) {
	return casTicketFields(svcCtx, playerId, expectedTicket, ttl, ticketFieldState, ticketStateMatched)
}

// markTicketReady 把 ticket 置为 ready 并写 battle_id、收紧 TTL:短暂供 GetQueueStatus
// 查询后自清,战斗内状态改由 battle:lock / InBattleComp 表达。CAS 失败(票据已被
// 玩家重排替换或已过期)只记 Info,不覆盖新票据。
func markTicketReady(svcCtx *svc.ServiceContext, playerId uint64, expectedTicket string, battleId uint64) {
	ttl := int(svcCtx.Config.ReadyTicketTTLSeconds)
	if ttl <= 0 {
		ttl = 60
	}
	written, err := casTicketFields(svcCtx, playerId, expectedTicket, ttl,
		ticketFieldState, ticketStateReady,
		ticketFieldBattleId, strconv.FormatUint(battleId, 10))
	if err != nil {
		logx.Errorf("[match] Failed to update ticket to ready player=%d battle=%d: %v", playerId, battleId, err)
		return
	}
	if !written {
		logx.Infof("[match] Ticket replaced or expired; skipping ready update player=%d battle=%d expected=%s",
			playerId, battleId, expectedTicket)
	}
}

// deleteTicket 删除 ticket(取消 / matcher 弹出时发现已在战斗中的出局者)。
func deleteTicket(svcCtx *svc.ServiceContext, playerId uint64) {
	if _, err := svcCtx.MatchRedis.Del(matchTicketKey(playerId)); err != nil {
		logx.Errorf("[match] Failed to delete ticket player=%d: %v", playerId, err)
	}
}

// deleteTicketIfOwned 带 ticket id 的删票(gather 失败路径的出局者):票据已被
// 玩家重排替换时不动新票据,只记 Info(与 CAS 写同口径,见 ticketDelCasScript)。
func deleteTicketIfOwned(svcCtx *svc.ServiceContext, playerId uint64, expectedTicket string) {
	deleted, err := deleteTicketIfOwnedCtx(context.Background(), svcCtx, playerId, expectedTicket)
	if err != nil {
		logx.Errorf("[match] Failed to CAS-delete ticket player=%d: %v", playerId, err)
		return
	}
	if !deleted {
		logx.Infof("[match] Ticket replaced or expired; skipping ticket deletion player=%d expected=%s", playerId, expectedTicket)
	}
}

// deleteTicketIfOwnedCtx 带 ticket id 的删票(见 ticketDelCasScript),返回 (删了, err),不打日志。
// 整队开战的回滚用独立 ctx 调它(补偿不继承请求 ctx,team-system.md §E.1 第 7 步)。
func deleteTicketIfOwnedCtx(ctx context.Context, svcCtx *svc.ServiceContext, playerId uint64, expectedTicket string) (bool, error) {
	res, err := svcCtx.MatchRedis.EvalCtx(ctx, ticketDelCasScript, []string{matchTicketKey(playerId)}, expectedTicket)
	if err != nil {
		return false, err
	}
	n, _ := res.(int64)
	return n == 1, nil
}

// requeueFront 把成员按原相对顺序放回队首(补偿矩阵:组队场景失败成员回队首),
// 票据恢复 queued 态并恢复长 TTL(matched 态收紧过)。tickets 是弹组时读到的
// ticket id:当前票据与之不一致(matched TTL 到期后玩家已重排,或票据已过期)
// 就整个人跳过 —— 既不写票据也不入队,否则会把已重排的玩家在队列里塞成两份。
// 队列 key 优先用票据里的 QueueKey,缺失(旧票据)才用 fallbackQueueKey。
//
// 按「单位」逐个处理,从最后一个单位往前(最先弹出的成员最终仍在最前):
//   - 单人:CAS 回 queued → 读一次战斗锁(上限 battleLockReadTimeout)→ LPUSH(一条 Lua)。做法与失败半径与加补位之前
//     相同(1v1 / 5v5 全是单人);CAS 与 LPUSH 之间多了这一次读,「票据 queued 但人不在队列」的窗口相应变宽一点,
//     窗口里落下的取消 / 重排由 matcher 的票据校验与 JoinQueue 自愈收尾,与此前同一形状;
//   - 队伍补位单(相邻且同一 party_id 的成员):本单全员先 CAS 回 queued → 完整性与回队前复核 → 一条 Lua 整单压回
//     → 逐人复读票据。
//
// 顺序必须是 **先 CAS 回 queued、再 LPUSH**:gather 不持凑单锁,反过来做的话
// 别的实例的 matcher 可以在 LPUSH 与 CAS 之间 LPOP 到这个人,读到 state=matched
// 而静默丢弃(popGroup 的状态校验),随后本实例的 CAS 再把他改成 queued + 6h TTL
// —— 票据 queued、队列无人,玩家被 ErrAlreadyQueued 卡到自己取消。先 CAS 后 LPUSH
// 则窗口内被弹出时票据已是 queued,会被正常凑组;LPUSH 失败再用 CAS 删票,不留
// 反向孤儿(玩家可立即重排)。CAS 与 LPUSH 之间本实例崩溃的残留由 JoinQueue 的
// isQueuedTicketInQueue 自愈。
//
// 队伍补位单不可拆(team-backfill-matching.md §4.2 / §4.4),规则都落在 requeueUnit 里,
// matchQueueOnce 与 gather 的失败出口不需要认识补位单:
//   - 完整性:名单里只要有一个人回不来(不在 members 里 —— 弹出后票据 CAS 失败的人、gather 的肇事者;
//     票据已被替换 / 过期;或 CAS 回 queued 没成功),这一单其余人也不回队:按 ticket id 删票,并通知 team。
//     「补位单要么全员进同一场战斗、要么全员出局」就落在这一条上。
//   - 回队前复核:弹出到回队之间票据是 matched,那段时间的名单变化撤单按「太迟」不动;原样放回去就带着旧名单继续排。
//     所以回队前再问 team 一次,不过的整单删票并通知;读不到的照常回队(弹组前复核与票据自愈还会再把关)。
//     这一步必须在 CAS **之后**:名单变化的撤单只找 queued 的票(CancelParty),复核若放在 CAS 之前,
//     落在「复核读完 → CAS 完成」之间的那次撤单会扑空,整单带着旧名单回队。先 CAS 后复核则没有窗口 ——
//     提交早于复核的被复核读到,晚于复核的那次撤单此时已经找得到票。
//   - 压回后复读:逐人 CAS 回 queued 的那几次往返里,先改回来的人已经可以被取消 —— 取消方删得掉他的票,
//     却删不掉还是 matched 的队友(按「太迟」留给这里收尾,见 cancelParty)。所以整单压回之后逐人复读票据,
//     有人的票不在了就整单撤掉;通知沿用取消方登记的原因。
//
// 回队前还要看一眼战斗锁(单人同样,见 dropMembersInOtherBattle):弹出到回队之间进了别的战斗的人不回队,
// 他所在的补位单整单出局。
func requeueFront(svcCtx *svc.ServiceContext, fallbackQueueKey string, members []uint64, tickets map[uint64]string) {
	requeueFrontAfterBattle(svcCtx, fallbackQueueKey, members, tickets, 0)
}

// requeueFrontAfterBattle 同 requeueFront;failedBattleId 非 0 时(gather 的失败出口)把它记进每张回队票据的
// last_failed_battle_id:这些人刚被那一局冻结过,scene 侧的解冻若丢了,备战锁会残留到自己过期,
// matcher 靠这个字段把它和「真的进了别的战斗」区分开(classifyBattleLock)。
// 没有发生过备战的回队(弹出后票据 CAS 不足)传 0,不写该字段。
func requeueFrontAfterBattle(svcCtx *svc.ServiceContext, fallbackQueueKey string, members []uint64,
	tickets map[uint64]string, failedBattleId uint64,
) {
	// 读票并核对 ticket id(只读,不改任何东西),按原序切成回队单位。
	entries := make([]requeueEntry, 0, len(members))
	for _, playerId := range members {
		expected, known := tickets[playerId]
		if !known {
			logx.Errorf("[match] Missing ticket id captured when group was popped; skipping return to queue front player=%d", playerId)
			continue
		}
		ticket, err := loadTicket(svcCtx, playerId)
		if err != nil {
			logx.Errorf("[match] Failed to read ticket before return to queue front; skipping player=%d: %v", playerId, err)
			continue
		}
		if ticket == nil || ticket.Ticket != expected {
			logx.Infof("[match] Ticket replaced or expired; not returning to queue front player=%d expected=%s", playerId, expected)
			continue
		}
		queueKey := fallbackQueueKey
		if ticket.QueueKey != "" {
			queueKey = ticket.QueueKey
		}
		entries = append(entries, requeueEntry{playerId: playerId, ticket: ticket, queueKey: queueKey})
	}
	requeued := 0
	units := requeueUnits(entries)
	for i := len(units) - 1; i >= 0; i-- {
		requeued += requeueUnit(svcCtx, units[i], failedBattleId)
	}
	logx.Infof("[match] %d/%d members returned to queue front queue=%s", requeued, len(members), fallbackQueueKey)
}

// requeueEntry 一名待回队的成员:票据已读到且 ticket id 与弹组时一致。
type requeueEntry struct {
	playerId uint64
	ticket   *queueTicket // 回队前读到的票据(ticket id、评分、补位单信息)
	queueKey string
}

// requeueUnits 把 entries 按原序切成回队单位:相邻且同一 party_id、同一队列的成员是一个单位,其余每人一个。
// 进组时同单成员本来就相邻(popUnits 按单位拼的名单),这里不重排。
func requeueUnits(entries []requeueEntry) [][]requeueEntry {
	var units [][]requeueEntry
	for _, entry := range entries {
		if n := len(units); n > 0 && entry.ticket.PartyId != "" {
			last := units[n-1]
			if last[0].ticket.PartyId == entry.ticket.PartyId && last[0].queueKey == entry.queueKey {
				units[n-1] = append(last, entry)
				continue
			}
		}
		units = append(units, []requeueEntry{entry})
	}
	return units
}

// requeueUnit 把一个回队单位送回队首,返回成功回队的人数(见 requeueFront 注释)。
func requeueUnit(svcCtx *svc.ServiceContext, unit []requeueEntry, failedBattleId uint64) int {
	seed := unit[0].ticket
	queueKey := unit[0].queueKey
	isParty := seed.PartyId != ""

	// 1. 票据 CAS 回 queued。
	restored := make([]requeueEntry, 0, len(unit))
	for _, entry := range unit {
		fields := []string{ticketFieldState, ticketStateQueued}
		if failedBattleId != 0 {
			fields = append(fields, ticketFieldLastFailedBattle, strconv.FormatUint(failedBattleId, 10))
		}
		written, err := casTicketFields(svcCtx, entry.playerId, entry.ticket.Ticket, ticketTTLSeconds(svcCtx), fields...)
		if err != nil {
			logx.Errorf("[match] Failed to restore ticket state before return to queue front; skipping player=%d: %v", entry.playerId, err)
			continue
		}
		if !written {
			// 读票与 CAS 之间被替换的极窄窗口:不入队,新票据自己已在队列里。
			logx.Infof("[match] Ticket replaced before return to queue front; not enqueueing player=%d expected=%s",
				entry.playerId, entry.ticket.Ticket)
			continue
		}
		restored = append(restored, entry)
	}
	if len(restored) == 0 {
		return 0
	}
	// 1b. 战斗锁:弹出到回队之间进了别的战斗的人不回队(补位单整单出局)。
	restored, inBattle := dropMembersInOtherBattle(svcCtx, restored, failedBattleId)
	if inBattle != 0 && isParty {
		evictRequeueParty(svcCtx, unit, constants.PartyEndMemberNotReady, inBattle)
		return 0
	}
	if len(restored) == 0 {
		return 0
	}

	// 2. 补位单:完整性 + 回队前复核,任一项不过整单出局。删票覆盖整个单位:CAS 出错的人状态未知
	//    (可能已是 queued),一并按 ticket id 清掉。
	if isParty {
		if absent, incomplete := partyAbsentMember(seed, restored); incomplete {
			evictRequeueParty(svcCtx, unit, constants.PartyEndMemberNotReady, absent)
			return 0
		}
		if reason, culprit := recheckPartyBeforeRequeue(svcCtx, seed); reason != constants.PartyEndNone {
			evictRequeueParty(svcCtx, unit, reason, culprit)
			return 0
		}
	}

	// 3. 一条 Lua 压回队首。
	if requeueFrontHook != nil {
		for i := len(restored) - 1; i >= 0; i-- {
			requeueFrontHook(restored[i].playerId)
		}
	}
	pushes := make([]queuePush, 0, len(restored))
	ids := make([]uint64, 0, len(restored))
	ticketIds := make(map[uint64]string, len(restored))
	for _, entry := range restored {
		pushes = append(pushes, queuePush{playerId: entry.playerId, rating: entry.ticket.Rating})
		ids = append(ids, entry.playerId)
		ticketIds[entry.playerId] = entry.ticket.Ticket
	}
	if err := pushMembersRegistered(context.Background(), svcCtx, queueKey, pushes, true); err != nil {
		// 票据已是 queued 但人不在队列:删票让玩家可立即重排,不留孤儿。补位单整单删(不可拆)并通知。
		logx.Errorf("[match] Failed to return to queue front; deleting tickets to allow requeue queue=%s members=%v: %v",
			queueKey, ids, err)
		for _, entry := range restored {
			deleteTicketIfOwned(svcCtx, entry.playerId, entry.ticket.Ticket)
		}
		// 结果未知(超时时 Lua 可能已执行):把可能留下的队列项摘一次,不等下一次弹组校验才清。
		if _, rmErr := removeQueueMembers(svcCtx, queueKey, ids); rmErr != nil {
			logx.Errorf("[match] Failed to dequeue after return-to-front failure (matcher ticket validation will handle remnants) queue=%s: %v",
				queueKey, rmErr)
		}
		if isParty {
			notifyPartyEnded(svcCtx, seed, constants.PartyEndMemberNotReady, 0)
		}
		return 0
	}

	// 4. 补位单:压回之后逐人复读票据,第 1 步中途(以及第 2 步复核期间)落下的取消在这里收尾。
	if isParty {
		if absent, withdrawn := firstMissingPartyTicket(context.Background(), svcCtx, ids, ticketIds); withdrawn {
			logx.Infof("[match] Party member's ticket vanished while the party was returning to queue front; withdrawing whole party team=%d party=%s absent=%d",
				seed.TeamId, seed.PartyId, absent)
			cancelParty(svcCtx, seed, constants.PartyEndMemberNotReady, absent)
			return 0
		}
	}
	return len(restored)
}

// partyAbsentMember 这一单的名单里第一个不在 present 里的人;都在返回 (0, false)。
// 名单读不出来(损坏)按不完整处理,缺席者未知。
func partyAbsentMember(seed *queueTicket, present []requeueEntry) (uint64, bool) {
	if len(seed.PartyMembers) == 0 {
		return 0, true
	}
	here := make(map[uint64]bool, len(present))
	for _, entry := range present {
		here[entry.playerId] = true
	}
	for _, pid := range seed.PartyMembers {
		if !here[pid] {
			return pid, true
		}
	}
	return 0, false
}

// recheckPartyBeforeRequeue 回队首前向 team 复核一支补位单,返回失效原因(PartyEndNone = 可以回队)。
// 复核读不到、端口未注入都按可以回队处理。两个调用场景:gather 的失败出口(不持凑单锁)与 matchQueueOnce
// 票据推进不足时(持锁;每支补位单至多再花 partyCheckTimeout,不计入弹组那 3 秒复核预算 —— 这条路只在并发取消时走到)。
func recheckPartyBeforeRequeue(svcCtx *svc.ServiceContext, seed *queueTicket) (uint32, uint64) {
	port := svcCtx.PartyTeam()
	if port == nil {
		return constants.PartyEndNone, 0
	}
	ctx, cancel := context.WithTimeout(context.Background(), partyCheckTimeout)
	defer cancel()
	reason, culprit, err := port.CheckParty(ctx, seed.TeamId, seed.PartyMembers)
	if err != nil {
		logx.Errorf("[match] Failed to verify party before return to queue front; requeueing as is team=%d: %v", seed.TeamId, err)
		return constants.PartyEndNone, 0
	}
	return reason, culprit
}

// dropMembersInOtherBattle 回队首前逐人读一次战斗锁(与 matcher 校验排队成员同口径,见 classifyBattleLock),
// 返回仍可回队的成员,以及第一个「在别的战斗里」的人(0 = 没有)。在别的战斗里的人票据按 ticket id 删掉 ——
// 他已经不在排了。典型时序:组被弹出、票据 matched,他应战切磋并开打(leaveQueueForOtherBattle 不动 matched 票),
// 随后这一局败在别人身上;不拦的话他人在切磋里、票据却回到 queued,人数不够凑满的队列 matcher 不扫,
// 打完切磋就被直接拉进副本。
// failedBattleId 非 0 时它就是刚失败的那一局:锁值等于它的是这一局没清掉的备战锁,照常回队。
// 读锁失败的人照常回队(matcher 校验时还会再看)。
func dropMembersInOtherBattle(svcCtx *svc.ServiceContext, restored []requeueEntry, failedBattleId uint64) ([]requeueEntry, uint64) {
	kept := make([]requeueEntry, 0, len(restored))
	var first uint64
	for _, entry := range restored {
		ctx, cancel := context.WithTimeout(context.Background(), battleLockReadTimeout)
		lockValue, err := playercontract.BattleLockValue(ctx, svcCtx, entry.playerId)
		cancel()
		if err != nil {
			logx.Errorf("[match] Failed to read battle lock before return to queue front; requeueing as is player=%d: %v", entry.playerId, err)
			kept = append(kept, entry)
			continue
		}
		ticket := *entry.ticket
		if failedBattleId != 0 {
			ticket.LastFailedBattleId = failedBattleId
		}
		if classifyBattleLock(lockValue, &ticket) != battleLockOther {
			kept = append(kept, entry)
			continue
		}
		logx.Infof("[match] Member entered another battle before returning to queue front; leaving queue player=%d battle=%s",
			entry.playerId, lockValue)
		deleteTicketIfOwned(svcCtx, entry.playerId, entry.ticket.Ticket)
		if first == 0 {
			first = entry.playerId
		}
	}
	return kept, first
}

// evictRequeueParty 一支补位单不回队:entries 里的成员(人不在队列;票据刚改回 queued,CAS 出错的可能还是 matched)
// 按 ticket id 删票,通知 team 一次。
func evictRequeueParty(svcCtx *svc.ServiceContext, entries []requeueEntry, reason uint32, culprit uint64) {
	seed := entries[0].ticket
	for _, entry := range entries {
		logx.Infof("[match] Party not returned to queue front; member leaves backfill queue team=%d party=%s player=%d reason=%d culprit=%d",
			seed.TeamId, seed.PartyId, entry.playerId, reason, culprit)
		deleteTicketIfOwned(svcCtx, entry.playerId, entry.ticket.Ticket)
	}
	notifyPartyEnded(svcCtx, seed, reason, culprit)
}

// nowMs 返回当前 Unix 毫秒。
func nowMs() uint64 {
	return uint64(time.Now().UnixMilli())
}
