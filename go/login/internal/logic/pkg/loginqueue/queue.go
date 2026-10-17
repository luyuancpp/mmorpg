// Package loginqueue implements a Redis ZSET-backed FIFO queue for AssignGate.
//
// Why this lives here (not in Java Gateway):
//   - Out-of-queue admission requires reading EnterGame state + reconnect
//     decision from sessionmanager + player_locator, both of which are
//     in-process for go-zero login.
//   - Java Gateway's existing RateLimitDecision.QUEUE is a Bucket4j retry
//     hint, not a real ordered queue. The two layers compose: Bucket4j
//     filters obvious spam first, the surviving traffic enters this queue.
//
// Redis layout (per zone):
//
//	queue:zone:{zoneId}        ZSET   member=queueId, score=enqueue unix ms
//	admit:{queueId}            STRING JSON AdmitSlot{node_id, ip, port, zone_id},TTL = AdmitTTL
//	                           (只记放行时选好的 gate,不含签名;PopAdmit 把它与在途占位排进同一个 MULTI 写入,
//	                           gate 票在取走时由 consumeAdmitAndSign 现签。R17 R1(2026-06-04)之前存的是
//	                           已签好的 {ip, port, payload, signature, deadline},票在 Redis 里就开始耗有效期)
//	admitted:zset:zone:{zoneId} ZSET member=queueId | fast:<uuid>, score=占位到期 unix ms
//	                           (在途占位:成员逐个到期,计数前清掉;整键 TTL 只做垃圾回收、只增不减。
//	                           G4b-G 之前是 SET admitted:zone:{zoneId} + 整键滑动 TTL,新版本不再读写旧键)
//	queue:meta:{queueId}       HASH   {account, device, zone_id, createdAt, admitted_at}
//	                           (admitted_at:放行时刻 unix 毫秒,PopAdmit 与放行槽同一个 MULTI 写入,
//	                           入队时没有;Lookup 靠它区分「放行窗口已过」与「在途窗口」,2026-10-15 起)
//	queue:token:{queueToken}   STRING queueId  (token → queueId reverse index)
//
// Lifecycle:
//
//	Enqueue   → ZADD + meta + token mapping (one Redis round-trip via pipeline)
//	Rank      → ZRANK + ZCARD               (cheap, hot path for QueryQueueStatus)
//	Admit     → ZPOPMIN N → write admit:{queueId} + meta.admitted_at + 占位(到期 = 现在 + AdmitTTL) (dispatcher)
//	Consume   → GETDEL admit:{queueId}, 签 gate 票, 占位改为「现在 + handoffGrace」到期(不撤)
//	FastPath  → 一段 Lua:清过期 + 计数 < budget + 占位(到期 = 现在 + handoffGrace)
//
// 放行之后客户端拿到的是确定的终态:放行槽被取走一次即 ADMITTED;之后再查(重复轮询、回包丢失后重试),
// 或放行槽没人来取、按 AdmitTTL 过期之后再查,Lookup 看到 meta.admitted_at 而放行槽已不在,回 EXPIRED
// (gateway 映射 410),客户端丢掉 token 重走 /assign-gate。2026-10-15 之前这里写的是「dropped admit
// 只是让客户端重新入队」,实际 Lookup 回的是「排队中、名次 0」,客户端一直轮询到 meta 按 QueueEntryTTL
// (1h)过期。PopAdmit 在写放行槽之前丢掉的条目(写放行槽的 MULTI 失败等)没有 admitted_at,仍回名次 0
// 直到 meta 过期,这是既有边角(login-queue-2026-05.md §14 第 7 条)。queue:meta TTL is the global ceiling.
//
// (2026-10-15 更正)上文「PopAdmit 把它与在途占位排进同一个 MULTI」「admitted_at 与放行槽同一个 MULTI 写入」
// 已不成立:放行槽、admitted_at、在途占位改由 admitEntryScript 一段脚本写,先判 meta 在不在、admitted_at
// 有没有。meta 不在(条目在队里待满 QueueEntryTTL,token 索引与排队令牌同时失效,查询本来就回 EXPIRED)
// 的条目整条丢弃,不写放行槽、不占名额、不计入放行数。ZPOPMIN 之后的写入也不再随调用方 ctx 取消(停服、
// 丢租约),见 popAdmitWriteBudget。仍会丢条目、停在名次 0 的路径只剩:取消之后写入超出这个预算(预算自
// PopAdmit 看到取消时起算,没有取消时不截断)、Redis 出错(含 noeviction 写满)、进程被强杀,以及 pickFn
// 回 (nil, nil)、Marshal 失败;写入出错每批汇总一行 ERROR。
package loginqueue

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"time"

	"github.com/google/uuid"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

// Status mirrors AssignGateResponse.status / QueryQueueStatusResponse.status.
type Status uint32

const (
	StatusAdmitted Status = 0
	StatusQueueing Status = 1
	StatusError    Status = 2
	StatusExpired  Status = 3
)

// AdmitToken is what the consume-time gate-token signer produces and what
// the client retrieves on its next AssignGate / QueryQueueStatus call. The
// fields mirror AssignGateResponse so handlers can copy directly.
//
// IMPORTANT: AdmitToken is **never written to Redis** in the queue path.
// What goes into admit:{queueId} is an AdmitSlot (gate-pick only) — the
// HMAC + expire is computed at consume time so the 5-min token TTL starts
// when the client redeems the admit, not when the dispatcher issued it.
// See `consumeAdmitAndSign` for the rationale (R17 R1 root cause).
//
// The fast-path /assign-gate handler (no queue) builds AdmitToken inline
// via PickAndSignGateToken because there is no Redis caching between sign
// and use; the field shape is shared for handler-side simplicity.
type AdmitToken struct {
	IP             string `json:"ip"`
	Port           uint32 `json:"port"`
	TokenPayload   []byte `json:"token_payload"`
	TokenSignature []byte `json:"token_signature"`
	TokenDeadline  int64  `json:"token_deadline"`
}

// AdmitSlot is what dispatcher writes to admit:{queueId} after picking a
// gate but BEFORE signing the gate token. It carries everything needed to
// produce a fresh AdmitToken at consume time — the gate endpoint identity
// plus the zone it belongs to. No HMAC, no expire.
//
// Why split this out from AdmitToken (R17 R1 fix, 2026-06-04):
//   - dispatcher.PopAdmit may run minutes ahead of the client's polling
//     consume. If we put a signed AdmitToken in Redis at PopAdmit time,
//     its expire_timestamp starts counting immediately and the client
//     can receive a token with only a few seconds of life left.
//   - Splitting "decide which gate" (admit-time) from "issue the HMAC"
//     (consume-time) means the 5-min token TTL window always starts
//     when the client is about to use it.
//
// Memory cost: AdmitSlot is smaller than AdmitToken (no payload bytes,
// no 64-byte hex signature), so this is also strictly cheaper for Redis.
type AdmitSlot struct {
	NodeID uint32 `json:"node_id"`
	IP     string `json:"ip"`
	Port   uint32 `json:"port"`
	ZoneID uint32 `json:"zone_id"`
}

// EnqueueResult bundles everything the AssignGate handler needs to build a
// queueing response in one place.
type EnqueueResult struct {
	QueueID    string
	QueueToken string
	Rank       uint32 // 0-based
	Total      uint32
}

// QueueState is the snapshot returned by Lookup, used by QueryQueueStatus.
type QueueState struct {
	Status Status
	Admit  *AdmitToken // populated when Status == StatusAdmitted
	Rank   uint32      // populated when Status == StatusQueueing
	Total  uint32      // populated when Status == StatusQueueing
}

// defaultHandoffGrace:gate 票交到客户端之后,在途占位再保留多久。
//
// 票一交出去(排队路径:客户端轮询取走放行槽;快速通道:assign-gate 当场回票),这个人还要
// 「连上 gate → gate 把人数报进 etcd」才出现在 online 里,这段时间只有占位替他占着名额。
// C++ gate 每 10 秒上报一次 player_count(cpp/nodes/gate/main.cpp 的 playerCountReportTimer),
// login 每次算容量都直接读 etcd(pkg/node/watcher.go FetchAllNodes,无缓存),最坏滞后 =
// 建连耗时 + 10s。20s = 两个上报周期:一个周期的上报滞后,再留 10s 建连与传播余量。
// 改 gate 的上报周期必须同步改这里。
//
// 取小了:占位先于人数到期,空档里多放人(旧实现取走即撤占位,相当于 0s,开区时每拍都按
// 「在线 0」再放一满批)。取大了:人已进 online、占位还在,同一个人算两遍,少放人(20s 时平均约 14s)。
// Java 版 LoginQueue.TAKEN_GRACE 是 15s、Java gate 每 5s 发布,两版的建连余量同为 10s。
// 2026-10-15 负责人定为 20s(spec_G4b-G §11 #1),待压测确认(L-14)。
const defaultHandoffGrace = 20 * time.Second

// admittedKeyGCSlack:在途占位 ZSET 的整键 TTL 比最长成员寿命多留的时间。
// 成员是否有效只看分数;整键 TTL 只负责在本 zone 没有任何 login 再来清理时(下线 / 合服)
// 把键收走。多留一分钟,免得 login 时钟(分数)与 Redis 时钟(TTL)的偏差让整键先于成员消失。
const admittedKeyGCSlack = time.Minute

// Queue wraps Redis ZSET operations behind a small typed surface. The zero
// value is unusable; construct via New.
type Queue struct {
	rdb        *redis.Client
	entryTTL   time.Duration
	admitTTL   time.Duration
	hmacSecret []byte // signs queue tokens; same secret as gate tokens is fine

	// handoffGrace:gate 票交到客户端之后占位再留多久。生产恒为 defaultHandoffGrace;
	// 只有同包测试会改它(New 不收这个参数)。
	handoffGrace time.Duration
	// now 是在途占位记账用的时钟:占位的到期分数、清过期成员时的「现在」都读它。
	// 生产是 time.Now;同包测试换成可拨的假时钟。排队令牌、入队分数等其它时间不走它。
	now func() time.Time

	// afterAdmitMiss:只给同包测试用的注入点,生产恒为 nil(New 不设它)。Lookup 第一次取放行槽落空之后、
	// 读 meta 之前调用,用来确定地模拟「dispatcher 写放行槽的 MULTI 恰好落在这两步之间」。
	// (2026-10-15 更正)写放行槽的已是 admitEntryScript 一段脚本,不再是 MULTI;模拟的时序不变。
	afterAdmitMiss func(queueID string)

	// popAdmitWriteBudget:PopAdmit 看到调用方 ctx 已取消之后,写完已弹出条目最多再用多久(2026-10-15)。
	// 生产恒为同名常量 popAdmitWriteBudget(理由见该常量);只有同包测试会改它(New 不收这个参数),
	// 设成 0 即「取消一被看到预算就到期」,不依赖墙钟。
	popAdmitWriteBudget time.Duration
}

// New constructs a Queue. hmacSecret is used to sign opaque queue tokens —
// reusing the gate token secret is acceptable because the queue token is
// useless to a gate (different message shape) and useless to login without
// a matching queueId entry, so the threat model is just "client tampering",
// which HMAC over (queueId|expire) defeats.
//
// entryTTL、admitTTL 必须为正,New 本身不校验;生产路径应先调 ValidateTTLs(2026-10-15)。
func New(rdb *redis.Client, entryTTL, admitTTL time.Duration, hmacSecret []byte) *Queue {
	return &Queue{
		rdb:          rdb,
		entryTTL:     entryTTL,
		admitTTL:     admitTTL,
		hmacSecret:   hmacSecret,
		handoffGrace: defaultHandoffGrace,
		now:          time.Now,

		popAdmitWriteBudget: popAdmitWriteBudget,
	}
}

// ValidateTTLs 校验 New 的两个时长参数(2026-10-15)。应由起服装配在 New 之前调用、失败即起服失败,
// 与 NewDispatcher 对 interval / lockTTL 的校验同级;配置的 default 只在键缺省时生效,显式写 0s 不回落。
// 两者都必须为正:
//   - admitTTL <= 0:放行脚本的 SET PX 报错,弹出的条目会全部丢失(PopAdmit 因此在弹出之前也调本函数,
//     不出队);旧写法(go-redis 的 Set(..., 0))则是写出永不过期的放行槽。在途占位一写即到期,FreeSlots 看不到
//     已放行、未取走的人,超发。
//   - entryTTL <= 0:go-redis 的 Expire(..., <=0) 当场删键,每次 Enqueue 都把整个 zone 的队列与 meta 删掉,
//     token 索引却不带 TTL、永久残留;排队令牌签发即过期。
//
// 不要求 admitTTL < entryTTL:放行槽写在放行时刻、自带 TTL,排队久的条目本来就可能比 meta 晚到期,
// 没有不变量依赖这个大小关系;Java 版 QueueSettings 同样只校验为正。
//
// (2026-10-15 补注)admitTTL 还须不小于 1ms:admitEntryScript 以 admitTTL.Milliseconds() 作 SET PX,
// 不足 1ms 取整为 0,Redis 拒绝 SET PX 0,弹出的条目全部丢失、停在名次 0 直到 meta 过期;旧写法经 go-redis
// 的 formatMs 截为 1ms,不受影响。entryTTL 不加这条下界:Enqueue 走 go-redis 的 Expire / Set,不足 1s / 1ms
// 会被截成 1s / 1ms。报错串仍含 "ttl must be positive",起服装配与用例按这个子串匹配。
func ValidateTTLs(entryTTL, admitTTL time.Duration) error {
	if entryTTL <= 0 {
		return fmt.Errorf("loginqueue: queue entry ttl must be positive, got %v", entryTTL)
	}
	if admitTTL < time.Millisecond {
		return fmt.Errorf("loginqueue: admit ttl must be positive and at least 1ms, got %v", admitTTL)
	}
	return nil
}

// ── Key helpers ────────────────────────────────────────────────────────

func zoneZSetKey(zoneID uint32) string     { return fmt.Sprintf("queue:zone:%d", zoneID) }
func admittedZSetKey(zoneID uint32) string { return fmt.Sprintf("admitted:zset:zone:%d", zoneID) }
func admitKey(queueID string) string       { return "admit:" + queueID }
func metaKey(queueID string) string        { return "queue:meta:" + queueID }
func tokenIndexKey(token string) string    { return "queue:token:" + token }

// metaFieldAdmittedAt 是 meta 里的放行时刻字段(unix 毫秒)。PopAdmit 写、Lookup 读,只看有没有,不看值。
const metaFieldAdmittedAt = "admitted_at"

// ── Enqueue ────────────────────────────────────────────────────────────

// Enqueue adds the caller to the zone queue and returns rank+token.
//
// NOT idempotent on (zoneId, account, deviceID): every call mints a fresh
// queueId. A client that loses its queueToken (e.g. due to a network blip
// before the response arrives) and re-calls AssignGate without the token
// WILL be appended at the tail again — the previous entry sits orphaned
// until its 1h TTL expires. The robot client mitigates this by saving
// queueToken from the FIRST AssignGate response and threading it through
// every subsequent retry; the server's reentry path (handleReentry) then
// resolves the saved token instead of creating a duplicate.
//
// We DON'T enqueue under a distributed lock; ZADD is atomic. Duplicates
// from missing-token retries are bounded (1 per retry round-trip) and
// the orphan TTLs out cheaply, so the cost of a (deviceID → queueId)
// reverse index would not pay back. If clients start spamming
// AssignGate-without-token under network failure, revisit.
func (q *Queue) Enqueue(ctx context.Context, zoneID uint32, account, deviceID string) (*EnqueueResult, error) {
	queueID := uuid.NewString()
	token, err := signQueueToken(q.hmacSecret, queueID, zoneID, time.Now().Add(q.entryTTL).Unix())
	if err != nil {
		return nil, fmt.Errorf("sign queue token: %w", err)
	}

	scoreMs := time.Now().UnixMilli()

	pipe := q.rdb.TxPipeline()
	pipe.ZAdd(ctx, zoneZSetKey(zoneID), redis.Z{Score: float64(scoreMs), Member: queueID})
	pipe.Expire(ctx, zoneZSetKey(zoneID), q.entryTTL)
	pipe.HSet(ctx, metaKey(queueID), map[string]any{
		"account":   account,
		"device":    deviceID,
		"zone_id":   zoneID,
		"createdAt": scoreMs,
	})
	pipe.Expire(ctx, metaKey(queueID), q.entryTTL)
	pipe.Set(ctx, tokenIndexKey(token), queueID, q.entryTTL)
	if _, err := pipe.Exec(ctx); err != nil {
		return nil, fmt.Errorf("enqueue pipeline: %w", err)
	}

	rank, err := q.rdb.ZRank(ctx, zoneZSetKey(zoneID), queueID).Result()
	if err != nil {
		// Race: ZADD succeeded but member was popped by the dispatcher between
		// the pipeline and ZRANK. Treat as rank 0 — the client will see it as
		// "next" and likely receive an admit on its first poll.
		if errors.Is(err, redis.Nil) {
			rank = 0
		} else {
			return nil, fmt.Errorf("zrank just-added member: %w", err)
		}
	}
	total, _ := q.rdb.ZCard(ctx, zoneZSetKey(zoneID)).Result()

	return &EnqueueResult{
		QueueID:    queueID,
		QueueToken: token,
		Rank:       uint32(rank),
		Total:      uint32(total),
	}, nil
}

// ── Lookup (QueryQueueStatus) ─────────────────────────────────────────

// SignAdmitFn is the consume-time gate-token signer. Lookup invokes it
// after GetDel admit:{queueId} succeeds, so the resulting AdmitToken's
// HMAC + expire begin at consume time rather than admit time. This is
// the heart of the R17 R1 fix: a 5-min token TTL must mean "5 min after
// the client receives it", not "5 min after the dispatcher picked the
// gate".
//
// The slot argument is whatever AdmitSlot the dispatcher stored at admit
// time (gate IP/Port/NodeID/ZoneID). Returning (nil, err) makes Lookup
// surface the error to the caller; returning (nil, nil) is not allowed —
// either we sign successfully or we have an error.
type SignAdmitFn func(slot *AdmitSlot) (*AdmitToken, error)

// Lookup translates a queue token to its current state. The signFn is
// invoked iff admit:{queueId} is present, so callers always pass a
// non-nil function — see queuestatuslogic / assigngatelogic.
//
// 判定顺序(2026-10-15 起):
//   - 取到放行槽:现签 gate 票,StatusAdmitted。GETDEL 只成功一次。
//   - 没取到、meta 有 admitted_at:放行过,再取一次(覆盖「第一次 GETDEL 早于放行的 MULTI」);
//     仍取不到 = 放行槽已被取走或已按 AdmitTTL 过期,这个条目不会再被放行,回 StatusExpired
//     (原因 admit_window_passed)。
//   - 没取到、没有 admitted_at:ZRANK 命中回名次;落空是在途窗口(dispatcher 已 ZPOPMIN、写放行槽的
//     MULTI 还没落地,亚秒级),回 StatusQueueing、名次 0,客户端照常再查。
//   - token 索引或 meta 不在、meta 损坏:StatusExpired(原因 meta_gone)。
//
// (2026-10-15 更正)上面两处「MULTI」现为 admitEntryScript 一段脚本(放行槽、admitted_at、占位同时可见),判定顺序不变。
func (q *Queue) Lookup(ctx context.Context, token string, signFn SignAdmitFn) (*QueueState, error) {
	queueID, err := q.rdb.Get(ctx, tokenIndexKey(token)).Result()
	if errors.Is(err, redis.Nil) {
		// Token-index entry gone (e.g. evicted under memory pressure, or
		// the entry's 1h TTL fired). Surface as EXPIRED so the client
		// restarts from /assign-gate with a fresh queueId.
		recordExpired(0, ExpireReasonMetaGone)
		return &QueueState{Status: StatusExpired}, nil
	}
	if err != nil {
		return nil, fmt.Errorf("lookup token index: %w", err)
	}

	// Admitted? Take the admit slot, sign a fresh-TTL gate token now, and
	// clean up so a second poll doesn't double-admit (the dispatcher set
	// status='admitted' atomically; here we turn that into a one-shot
	// consume-and-sign).
	admit, err := q.consumeAdmitAndSign(ctx, queueID, signFn)
	if err != nil {
		return nil, err
	}
	if admit != nil {
		// 条目的生命到此为止。meta 与 token 索引留到 TTL(见 consumeAdmitAndSign 末尾);
		// 在途占位不撤,consumeAdmitAndSign 把它改成再留 handoffGrace。
		return &QueueState{Status: StatusAdmitted, Admit: admit}, nil
	}

	if q.afterAdmitMiss != nil {
		q.afterAdmitMiss(queueID)
	}

	// 没取到放行槽:一次 HMGET 读出 zone_id(查哪个队列)与 admitted_at(放行过没有),往返数与原来的 HGET 相同。
	fields, err := q.rdb.HMGet(ctx, metaKey(queueID), "zone_id", metaFieldAdmittedAt).Result()
	if err != nil {
		return nil, fmt.Errorf("lookup meta: %w", err)
	}
	if len(fields) != 2 {
		return nil, fmt.Errorf("lookup meta: HMGET returned %d fields, want 2", len(fields))
	}
	zoneIDStr, hasZone := fields[0].(string)
	admittedAt, _ := fields[1].(string)
	var zoneID uint32
	if hasZone {
		if n, _ := fmt.Sscanf(zoneIDStr, "%d", &zoneID); n != 1 {
			zoneID = 0
		}
	}

	if admittedAt != "" {
		// 放行过。PopAdmit 把放行槽与 admitted_at 写在同一个 MULTI 里,对其它客户端同时可见,所以看得到
		// admitted_at 就说明放行槽写过;第一次 GETDEL 落空可能只是它早于那个 MULTI,再取一次。
		// (2026-10-15 更正)「同一个 MULTI」现为 admitEntryScript 一段脚本,同样原子可见,推理不变。
		// 复用 consumeAdmitAndSign:签票与改占位只有一份实现,取到时与第一次取到完全相同。
		admit, err := q.consumeAdmitAndSign(ctx, queueID, signFn)
		if err != nil {
			return nil, err
		}
		if admit != nil {
			return &QueueState{Status: StatusAdmitted, Admit: admit}, nil
		}
		// 仍取不到:放行槽已被取走(之前的轮询、go-redis 超时重发的 GETDEL)或已按 AdmitTTL 过期。
		// 给终态,不回「名次 0」让客户端空等到 meta 过期(1h)。没取到就不改占位,它按已有的分数到期
		// (没人来取或取走时签票失败:放行时刻 + AdmitTTL;取走并签票成功:取走时刻 + handoffGrace),
		// 重复轮询不会给它续期。
		recordExpired(zoneID, ExpireReasonAdmitWindowPassed)
		return &QueueState{Status: StatusExpired}, nil
	}

	if !hasZone {
		// Meta TTL expired faster than token TTL — treat as expired.
		recordExpired(0, ExpireReasonMetaGone)
		return &QueueState{Status: StatusExpired}, nil
	}
	if zoneID == 0 {
		// Meta payload is corrupt — should never happen since Enqueue
		// writes zone_id under the same pipeline that creates the entry.
		// Surface as EXPIRED so the client restarts cleanly rather than
		// looking up "queue:zone:0" forever (rank would always be 0,
		// dispatcher would never see it, → silent stuck client).
		recordExpired(0, ExpireReasonMetaGone)
		return &QueueState{Status: StatusExpired}, nil
	}

	rank, err := q.rdb.ZRank(ctx, zoneZSetKey(zoneID), queueID).Result()
	if errors.Is(err, redis.Nil) {
		// Member vanished — likely just popped by dispatcher; advise client to
		// retry rather than mark expired.
		// (2026-10-15)走到这里时 meta 没有 admitted_at:dispatcher 已 ZPOPMIN、写放行槽的 MULTI 还没落地
		// (亚秒级在途窗口),下一次轮询就能取到。放行槽被取走或过期之后不会再走到这里(上面回 EXPIRED)。
		// 例外是 PopAdmit 在写放行槽之前丢掉的条目(MULTI 失败等,既有边角),它仍停在名次 0 直到 meta 过期。
		// (2026-10-15 补注)上面两处「MULTI」现为 admitEntryScript。停服或丢租约发出的取消已不再造成这类丢条目
		// (见 popAdmitWriteBudget);剩下的是取消之后写入超出预算、Redis 出错(含 noeviction 写满)与进程被强杀,
		// 写入出错时 PopAdmit 每批汇总一行 ERROR(见 PopAdmit 末尾)。
		return &QueueState{Status: StatusQueueing, Rank: 0}, nil
	}
	if err != nil {
		return nil, fmt.Errorf("zrank during lookup: %w", err)
	}
	total, _ := q.rdb.ZCard(ctx, zoneZSetKey(zoneID)).Result()
	return &QueueState{Status: StatusQueueing, Rank: uint32(rank), Total: uint32(total)}, nil
}

// consumeAdmitAndSign atomically reads-and-deletes admit:{queueId},
// invokes signFn on the stored AdmitSlot to mint a fresh-TTL gate
// token, and returns the resulting AdmitToken. Returns (nil, nil) if no
// admit slot exists yet (caller treats as "still queueing").
// (2026-10-15 更正)上一句「caller treats as "still queueing"」已不成立:(nil, nil) 只表示这次 GETDEL 没取到
// 放行槽——可能还没放行,也可能已被取走或已按 AdmitTTL 过期。由 Lookup 读 meta 的 admitted_at 再下结论:
// 没有 admitted_at 才算仍在排队(ZRANK 命中回名次,落空回名次 0 的在途窗口);有 admitted_at 则再取一次,
// 仍取不到回 StatusExpired(原因 admit_window_passed)。见 Lookup 的判定顺序。
//
// The signFn argument is what makes this fix work: the gate token's
// HMAC + expire are computed HERE, at consume time, so the 5-min
// token TTL window begins when the client redeems the admit — not when
// the dispatcher picked the gate. Before this split (pre-2026-06-04)
// signing happened inside PopAdmit and the token could sit in Redis for
// 4-5 min before the client GetDel'd it, burning most of its TTL. See
// stress-1zone-45k-2026-06-03-round17.md §4 R1 for the diagnosis.
func (q *Queue) consumeAdmitAndSign(ctx context.Context, queueID string, signFn SignAdmitFn) (*AdmitToken, error) {
	// GETDEL is single-RTT; if the admit was issued under SET NX it still
	// resolves cleanly. If admit:{queueId} didn't exist, GETDEL returns
	// redis.Nil and we treat it as "still queueing".
	// (2026-10-15 更正)「we treat it as "still queueing"」不在这里决定:这里只回 (nil, nil),由 Lookup 按
	// meta 的 admitted_at 分「仍在排队」与「放行窗口已过」(回 EXPIRED)。另:放行槽现在不是 SET NX 写的
	// (此前是 MULTI 里的普通 SET,现为 admitEntryScript 里的 SET PX),GETDEL 对两者结果相同。
	val, err := q.rdb.GetDel(ctx, admitKey(queueID)).Bytes()
	if errors.Is(err, redis.Nil) {
		return nil, nil
	}
	if err != nil {
		return nil, fmt.Errorf("getdel admit: %w", err)
	}

	var slot AdmitSlot
	if err := json.Unmarshal(val, &slot); err != nil {
		return nil, fmt.Errorf("unmarshal admit slot: %w", err)
	}

	// Sign NOW so the token's expire starts ticking from this moment.
	if signFn == nil {
		return nil, fmt.Errorf("consumeAdmitAndSign: nil signFn (caller bug)")
	}
	admit, err := signFn(&slot)
	if err != nil {
		// We've already GETDEL'd the slot — the entry is gone from Redis.
		// Surface the signing error; the client will see code=500 and can
		// restart from /assign-gate to get re-enqueued. We could try to
		// put the slot back, but a signing failure usually means "no gate
		// candidates available" or HMAC-secret misconfig, neither of
		// which is fixed by retrying with the same slot.
		return nil, fmt.Errorf("sign admit token: %w", err)
	}

	// 在途占位与等待时长:best-effort,失败不让这次取走作废(票已经签好)。
	//
	// We do HMGET (not HGET) to grab createdAt and zone_id in one RTT so the
	// wait-time histogram observation below pays no extra Redis cost. Missing
	// createdAt is non-fatal — the wait observation is skipped.
	fields, _ := q.rdb.HMGet(ctx, metaKey(queueID), "zone_id", "createdAt").Result()
	var (
		zoneID    uint32
		createdAt int64
	)
	if len(fields) >= 1 {
		if s, ok := fields[0].(string); ok {
			_, _ = fmt.Sscanf(s, "%d", &zoneID)
		}
	}
	if len(fields) >= 2 {
		if s, ok := fields[1].(string); ok {
			_, _ = fmt.Sscanf(s, "%d", &createdAt)
		}
	}
	if zoneID != 0 {
		// 不撤占位,改成再留 handoffGrace(G4b-G):票已交给客户端,但他连上 gate、gate 把人数
		// 报进 etcd 之前不在任何 gate 的 online 里。此刻撤掉,下一拍会按「在线没变、在途为 0」
		// 再放一批(旧实现在这里 SREM,开区时能放进几倍容量)。
		// 用普通 ZADD 而不是 XX:占位若已按分数清掉(login 与 Redis 的时钟差),这个人照样要占名额。
		// zone 读不到(meta 被淘汰)时走不到这里:占位保持放行时的分数,最迟 AdmitTTL 到期。
		if err := q.holdAdmitted(ctx, q.rdb, zoneID, queueID, q.handoffGrace).Err(); err != nil {
			logx.Errorf("[loginqueue] hold admitted placeholder after consume zone=%d queueId=%s err=%v",
				zoneID, queueID, err)
		}
		if createdAt > 0 {
			waitMs := time.Now().UnixMilli() - createdAt
			recordWait(zoneID, time.Duration(waitMs)*time.Millisecond)
		}
	}
	// meta 与 token 索引不删,留到各自的 TTL(QueueEntryTTL)。之后同一令牌再查(回包丢失后重试、go-redis
	// 超时重发 GETDEL),Lookup 靠 meta 里的 admitted_at 认出「放行过、放行槽已不在」,回 StatusExpired
	// (原因 admit_window_passed),客户端按 410 丢掉 token 重走 /assign-gate。
	// (2026-10-15 更正)此前这里的英文注释就说重复轮询看到 StatusExpired,但 Lookup 实际回的是
	// 「排队中、名次 0」,直到 meta 过期;现在注释与行为一致。
	return admit, nil
}

// ── Dispatcher-facing (admit a batch) ─────────────────────────────────

// PickAdmitSlotFn is the dispatcher-side gate selector used by PopAdmit.
// It chooses which gate the queue entry will be assigned to without
// signing the gate token — the HMAC + expire happen later, at consume
// time, in consumeAdmitAndSign.
//
// Returning (nil, err) makes PopAdmit re-add the entry to the front of
// the ZSET so the next dispatcher tick retries; returning (nil, nil) is
// treated as a no-gate-available outcome and skips the entry entirely.
type PickAdmitSlotFn func(zoneID uint32, queueID string) (*AdmitSlot, error)

// PopAdmit pops up to n entries from the front of zoneId's queue and
// writes admit:{queueId} = AdmitSlot for each. Caller (the dispatcher)
// supplies pickFn that selects a gate without signing — sign happens
// later at consume time so the token's 5-min TTL starts when the
// client redeems it (R17 R1 fix, see consumeAdmitAndSign).
//
// Returns the number of entries actually admitted (may be < n if the
// queue is shorter than expected, or if some pickFn calls fail).
// (2026-10-15 补注)meta 已过期的条目不写、不计入;写入出错的条目也不计入,每批汇总一行 ERROR。
func (q *Queue) PopAdmit(
	ctx context.Context,
	zoneID uint32,
	n int,
	pickFn PickAdmitSlotFn,
) (int, error) {
	if n <= 0 {
		return 0, nil
	}
	// (2026-10-15)TTL 不合法(非正,或 AdmitTTL 不足 1ms)时一个都不弹出,条目原样留在队列,由 dispatcher 每拍记 ERROR。
	// 起服装配(initLoginQueue)已在 New 之前调 ValidateTTLs、失败即起服失败,这里是纵深防御,挡住绕过装配
	// 直接 New 的调用方:否则放行脚本的 SET PX 0 报错,弹出的整批全部丢失(旧写法则是写出永不过期的放行槽)。
	if err := ValidateTTLs(q.entryTTL, q.admitTTL); err != nil {
		return 0, err
	}
	zs, err := q.rdb.ZPopMin(ctx, zoneZSetKey(zoneID), int64(n)).Result()
	if err != nil {
		return 0, fmt.Errorf("zpopmin: %w", err)
	}
	if len(zs) == 0 {
		return 0, nil
	}

	// 弹出之后的写入不随调用方 ctx 取消,只受 popAdmitWriteBudget 约束(理由见该常量)。ZPOPMIN 本身仍用 ctx:
	// 停服或丢租约之后不再弹出新的一批。
	// (2026-10-15 更正)预算不自弹出时起算,自本批第一次看到调用方 ctx 已结束时起算;没有取消时一批写多久都照写,
	// 与改动前(写入沿用没有截止的 drainCtx)一致。自弹出起算会在没有取消时截断开区大批次:例如 batch=5000、
	// 逐条往返只要超过 0.6ms 就超出 3s,尾部条目丢失、停在名次 0 约 1h;它们也不占在途名额,下一拍 free 不降,
	// 每拍的尾部都会这样丢。检查放在每次写之前(不在循环开头),pickFn 期间发生的取消在本条就生效。
	writeCtx := context.WithoutCancel(ctx)
	var cancelWrite context.CancelFunc
	defer func() {
		if cancelWrite != nil {
			cancelWrite()
		}
	}()
	armWriteBudget := func() {
		if cancelWrite != nil || ctx.Err() == nil {
			return
		}
		writeCtx, cancelWrite = context.WithTimeout(writeCtx, q.popAdmitWriteBudget)
	}

	var (
		admitted, metaGone, dropped int
		firstDroppedID              string
		firstDroppedErr             error
	)
	for _, z := range zs {
		queueID, ok := z.Member.(string)
		if !ok || queueID == "" {
			continue
		}
		slot, err := pickFn(zoneID, queueID)
		if err != nil {
			// Couldn't pick a gate — the entry is gone from ZSET. Best-effort
			// re-add at the front so the next dispatcher tick retries. Score
			// = 0 to put it ahead of newcomers.
			// (2026-10-15 补注)回填失败时这个条目同样丢了,记进循环后的汇总 ERROR(此前错误被忽略、零日志)。
			armWriteBudget()
			if err := q.rdb.ZAdd(writeCtx, zoneZSetKey(zoneID), redis.Z{Score: 0, Member: queueID}).Err(); err != nil {
				dropped++
				if firstDroppedErr == nil {
					firstDroppedID, firstDroppedErr = queueID, fmt.Errorf("requeue after pick failure: %w", err)
				}
			}
			continue
		}
		if slot == nil {
			// pickFn signaled "no gate available right now". Drop the entry —
			// the client will re-enqueue on its next /assign-gate retry.
			continue
		}
		blob, err := json.Marshal(slot)
		if err != nil {
			continue
		}

		// (2026-10-15 改)放行槽、admitted_at、在途占位由 admitEntryScript 一段脚本写,此前是一个
		// MULTI{SET, markAdmittedScript, admittedHoldScript}。三者对其它客户端同时可见,Lookup 据 admitted_at
		// 把「放行槽已被取走或已过期」(回 EXPIRED)与「在途窗口」(回名次 0)分开;占位没人来取时与放行槽同寿
		// (AdmitTTL),取走时改成再留 handoffGrace。meta 不在的条目什么都不写、不计入放行数(见脚本注释)。
		// pickFn 在脚本之前调用,这类条目仍让本拍批内的 gate 本地计数 +1,只影响一拍之内的分配均衡。
		// 出错不回填队列(见 popAdmitWriteBudget),条目不计入放行数(既有的丢条目路径,spec_G4b-G §5.3),
		// 循环结束后汇总一行 ERROR。
		armWriteBudget()
		res, err := q.admitEntry(writeCtx, zoneID, queueID, blob)
		if err != nil {
			dropped++
			if firstDroppedErr == nil {
				firstDroppedID, firstDroppedErr = queueID, err
			}
			continue
		}
		if res == admitEntryMetaGone {
			metaGone++
			continue
		}
		// admitEntryWritten,或 admitEntryAlreadyAdmitted(go-redis 重发了同一次脚本,第一次已写成):都算放行。
		admitted++
	}
	if dropped > 0 {
		// 每批一行,不逐条打:Redis 写满(noeviction)时每拍整批失败,逐条打会一拍刷几千行。
		logx.Errorf("[loginqueue] PopAdmit dropped entries after zpopmin zone=%d reason=write_failed popped=%d admitted=%d dropped=%d firstQueueId=%s firstErr=%v",
			zoneID, len(zs), admitted, dropped, firstDroppedID, firstDroppedErr)
	}
	if metaGone > 0 {
		logx.Infof("[loginqueue] PopAdmit dropped entries after zpopmin zone=%d reason=meta_gone dropped=%d",
			zoneID, metaGone)
	}
	return admitted, nil
}

// popAdmitWriteBudget:PopAdmit 在 ZPOPMIN 之后写放行(含 pickFn 出错时的回填)最多用多久(2026-10-15)。
//
// ZPOPMIN 之后,这批条目只存在于本进程的内存里:不在队列,也还没有放行槽、admitted_at 和占位。调用方传入的
// 是 dispatcher 的 drainCtx,停服(Stop → cancel)与丢租约(onLost → drainCancel)都会取消它;沿用它写,
// 剩下每一条都在 go-redis 取连接时(pool 的 waitTurn)就以 context.Canceled 返回、命令根本没发出,整批剩余
// 条目丢失,玩家停在名次 0 直到 meta 过期(1h)。开区时一拍放上千人,串行写与 1s 的拍长同一量级,滚动重启
// 或丢租约落在一个非空拍上并不罕见。所以这一段改用不随取消结束、但有上限的 ctx:
//   - 3s:go-zero 收到 SIGTERM 后先等 1s 收尾,到 5.5s 强杀;cancel 在收尾之后才发出,3s 落在剩余窗口里。
//     截止只在取连接时生效(客户端没开 ContextTimeoutEnabled),正在等回包的那一条另受 ReadTimeout
//     (默认 3s)约束。Stop 因此最多多等约一个预算。超出预算的条目照旧丢失,记进汇总的 ERROR。
//   - 丢租约后继续写自己已弹出的条目是安全的:ZPOPMIN 让每个条目只被一个副本弹出一次,不会重复写槽、重复签票;
//     新 leader 算空位时暂时看不到这些占位,最多多放一批,与 login-queue-2026-05.md §14 第 7 条的在途上界同类。
//   - 不要改成「写入出错就回填队列」:回填用的 ZADD 在 noeviction 写满时同样被拒;队列被弹空之后,普通 ZADD
//     还会建出没有 TTL 的队列键;读超时这类结果不确定的错误下脚本可能已经执行,回填只会让已放行的条目再被
//     弹出一次。
//
// (2026-10-15 更正)预算自 PopAdmit 第一次看到调用方 ctx 已结束时起算,不自弹出时起算(原因见 PopAdmit 里
// armWriteBudget 前的注释);没有取消时不设截止,与改动前一致。上文「cancel 在收尾之后才发出,3s 落在剩余窗口里」
// 也不严谨:go-zero 约在 SIGTERM 后 1s 通知 shutdown listener,服务退出后 defer 的 ctx.Stop 才取消 drainCtx,
// PopAdmit 要到下一次写之前才看到,预算通常在 SIGTERM 后约 4s 走完,落在 5.5s 强杀之前;但到期时正在等回包的
// 那一条还会再等到 ReadTimeout(默认 3s)。所以 Stop 最多多等「预算 + 一条命令的 ReadTimeout」,Redis 慢到这种
// 程度时会越过强杀点,被强杀时尾部条目照样丢失,也打不出汇总 ERROR。生产取值经 Queue.popAdmitWriteBudget 读取
// (New 设为本常量),只有同包测试会改。
const popAdmitWriteBudget = 3 * time.Second

// admittedKeyTTL 是本副本每次写在途占位之后,整键至少还要剩的 TTL(记作 W)。只做垃圾回收,
// 见 admittedKeyGCSlack。
//
// 整键 TTL 只增不减:三条写路径(放行、取走、快速通道)都是「先 ZADD,剩余 PTTL < W 才 PEXPIRE W」。
// W 由各副本按自己的 AdmitTTL 算,排队开着时滚动调整 AdmitTTL 就有新旧两种 W 并存;若无条件
// PEXPIRE,小 W 副本的写入会把整键 TTL 压到大 W 副本写的占位之前,该 zone 随后一段时间没有写入时,
// 整键连同未到期的占位一起消失,下一拍最多多放一批。只增不减之后,整键到期时刻不早于任何一次写入的
// 「写入时刻 + 写入方的 W」,也就晚于那次写下的成员。代价:调小 AdmitTTL 之后,已续上的较大 TTL
// 要自然走完才收敛,只影响 zone 下线后键多留多久。
//
// 改脚本时的两个陷阱。其一,不能用 PEXPIRE … GT:没有 TTL 的键按「无限 TTL」处理,GT 永不成立,
// 刚由 ZADD 建出的键就永不过期。其二,必须先 ZADD 再读 PTTL:键不存在时 PTTL 回 -2,此时的
// PEXPIRE 打空(回 0),随后 ZADD 建出的键没有 TTL。先 ZADD 之后键一定存在,PTTL 只会是 -1(无 TTL)
// 或剩余毫秒,-1 < W 恒成立,新键一定被设上 TTL。
func (q *Queue) admittedKeyTTL() time.Duration {
	return max(q.admitTTL, q.handoffGrace) + admittedKeyGCSlack
}

// admittedHoldScript 把一个成员的在途占位记到给定的到期毫秒(成员已在则覆盖分数),并保证整键剩余
// TTL 不少于 W(只增不减,见 admittedKeyTTL)。放行与取走共用,写法与 reserveFastPathSlotScript 的
// 后两步相同,改一处要同步改另一处。
// (2026-10-15 更正)放行改由 admitEntryScript 写占位,本脚本只剩取走一处在用;admitEntryScript 的后两步
// 也是同一写法,现在是三处同步改。
//
//	KEYS[1] = admitted:zset:zone:{zone}
//	ARGV[1] = 占位到期毫秒   ARGV[2] = member(queueId)   ARGV[3] = W 毫秒
//	返回 1。不能不返回:脚本回 nil 时 go-redis 记为 redis.Nil,排在 MULTI 里会让 Exec 回 error。
var admittedHoldScript = redis.NewScript(`
redis.call('ZADD', KEYS[1], ARGV[1], ARGV[2])
if redis.call('PTTL', KEYS[1]) < tonumber(ARGV[3]) then
	redis.call('PEXPIRE', KEYS[1], ARGV[3])
end
return 1
`)

// holdAdmitted 把 member 在 zoneID 的在途占位记到「现在 + hold」到期(成员已在则覆盖分数),
// 并按只增不减续上整键的垃圾回收 TTL,两步在一段脚本里。
//
// c 传 q.rdb 时当场执行;传 MULTI 的 pipeline 时只是入队,结果由调用方的 Exec 给出。用 EVAL 而不是
// Script.Run:Run 先发 EVALSHA,遇到 NOSCRIPT 才回退 EVAL,而排在 pipeline 里的命令要到 Exec 才知道
// 结果,回退来不及;MULTI 里的 EVALSHA 若撞上 NOSCRIPT(Redis 重启、SCRIPT FLUSH 之后),放行槽照样
// 写进去、占位却没写。脚本只有几行,每次带上全文的开销可以忽略。
// (2026-10-15 补注)放行改用 admitEntryScript 之后,只剩取走路径调用,传的是 q.rdb(当场执行)。
func (q *Queue) holdAdmitted(ctx context.Context, c redis.Scripter, zoneID uint32, member string, hold time.Duration) *redis.Cmd {
	return admittedHoldScript.Eval(ctx, c,
		[]string{admittedZSetKey(zoneID)},
		q.now().Add(hold).UnixMilli(), member, q.admittedKeyTTL().Milliseconds(),
	)
}

// admitEntryScript 的返回值。
const (
	admitEntryMetaGone        = 0 // meta 不在,什么都没写
	admitEntryWritten         = 1 // 已写放行槽、admitted_at、在途占位
	admitEntryAlreadyAdmitted = 2 // admitted_at 已在,什么都没写(同一次放行被 go-redis 重发)
)

// admitEntryScript 写一个条目的放行:放行槽、meta 的 admitted_at、在途占位,一段脚本内对其它客户端同时可见
// (2026-10-15 起,替代原来的 MULTI{SET, markAdmittedScript, admittedHoldScript})。
//
//	KEYS[1] = admit:{queueId}   KEYS[2] = queue:meta:{queueId}   KEYS[3] = admitted:zset:zone:{zone}
//	ARGV[1] = AdmitSlot JSON    ARGV[2] = AdmitTTL 毫秒          ARGV[3] = 字段名(metaFieldAdmittedAt)
//	ARGV[4] = 放行时刻 unix 毫秒(墙钟,与 meta 的 createdAt 同源)
//	ARGV[5] = 占位到期毫秒(Queue.now + AdmitTTL)   ARGV[6] = member(queueId)   ARGV[7] = W 毫秒(admittedKeyTTL)
//	返回 admitEntryMetaGone / admitEntryWritten / admitEntryAlreadyAdmitted,从不回 nil。
//
// 两道前置判定都只读,不过时一个键都不动:
//   - meta 不在:条目在队里待满了 QueueEntryTTL(整个队列键的 TTL 每次入队都续,meta 不续)。token 索引与 meta
//     由 Enqueue 在同一个 MULTI 里用同一个 TTL 设下、同时过期,排队令牌的 HMAC 也已到期,部署不淘汰键,
//     再没有人能取到这个放行槽;写了只是白占一个 AdmitTTL 的在途名额、虚增 admit_total。也不能直接 HSET:
//     会建出一个没有 TTL、永不过期的 meta。
//   - admitted_at 已在:这个条目已经放行过。ZPOPMIN 只弹出一次,走到这里只可能是 go-redis 重发了同一次脚本
//     (单条命令在读超时、连接断开后自动重发;原来的 MULTI 读失败不重发)。第一次已经执行,期间客户端可能已经
//     取走放行槽;再写一次就会签出第二张 gate 票,占位分数也会被拨回「放行时刻 + AdmitTTL」。
//
// meta 在时 HSET 不改它的 TTL,meta 仍按入队时的 QueueEntryTTL 到期。占位两步(先 ZADD,剩余 PTTL < W 才
// PEXPIRE W)与 admittedHoldScript、reserveFastPathSlotScript 同一写法,改一处要三处同步(两个陷阱见
// admittedKeyTTL)。脚本中途某条命令出错(占位键被写成别的类型、AdmitTTL 不足 1ms 时的 SET PX 0)时,
// 已执行的写不回滚,与原来 MULTI 里单条命令出错相同;SET 是第一条写,它出错时什么都没写。
// (2026-10-15 补注)AdmitTTL 不足 1ms 的情形已由 ValidateTTLs 挡在起服与弹出之前(见该函数)。
var admitEntryScript = redis.NewScript(`
if redis.call('EXISTS', KEYS[2]) == 0 then return 0 end
if redis.call('HEXISTS', KEYS[2], ARGV[3]) == 1 then return 2 end
redis.call('SET', KEYS[1], ARGV[1], 'PX', ARGV[2])
redis.call('HSET', KEYS[2], ARGV[3], ARGV[4])
redis.call('ZADD', KEYS[3], ARGV[5], ARGV[6])
if redis.call('PTTL', KEYS[3]) < tonumber(ARGV[7]) then
	redis.call('PEXPIRE', KEYS[3], ARGV[7])
end
return 1
`)

// admitEntry 对 queueID 跑一次 admitEntryScript,返回值见该脚本。只由 PopAdmit 调用。
// 不在 pipeline 里,用 Script.Run(先 EVALSHA,NOSCRIPT 时回退 EVAL):EVALSHA 撞上 NOSCRIPT 时脚本没有执行,
// 当场就能回退,不存在 holdAdmitted 注释里「排在 MULTI 里来不及回退」的问题。三个键都在排队族同一个实例上。
func (q *Queue) admitEntry(ctx context.Context, zoneID uint32, queueID string, blob []byte) (int64, error) {
	return admitEntryScript.Run(ctx, q.rdb,
		[]string{admitKey(queueID), metaKey(queueID), admittedZSetKey(zoneID)},
		blob, q.admitTTL.Milliseconds(), metaFieldAdmittedAt, time.Now().UnixMilli(),
		q.now().Add(q.admitTTL).UnixMilli(), queueID, q.admittedKeyTTL().Milliseconds(),
	).Int64()
}

// admittedCountScript 清掉已到期的占位再计数,两步在同一段脚本里。
//
//	KEYS[1] = admitted:zset:zone:{zone}
//	ARGV[1] = now_ms
//	返回未到期的成员数
var admittedCountScript = redis.NewScript(`
redis.call('ZREMRANGEBYSCORE', KEYS[1], '-inf', ARGV[1])
return redis.call('ZCARD', KEYS[1])
`)

// AdmittedCount 返回 zone 当前的在途占位数:已放行还没来取的,加上票已交出去、还在
// handoffGrace 之内的(排队取走与快速通道)。FreeSlots 用它扣容量,避免上一批还没出现在
// gate 的人数里就再放一批。先清过期成员再数,所以它是一条**写命令**。
func (q *Queue) AdmittedCount(ctx context.Context, zoneID uint32) (uint32, error) {
	n, err := admittedCountScript.Run(ctx, q.rdb,
		[]string{admittedZSetKey(zoneID)}, q.now().UnixMilli()).Int64()
	if err != nil {
		return 0, err
	}
	return uint32(n), nil
}

// reserveFastPathSlotScript 原子占位:先清掉已到期的占位,未到期的不足 budget 才记一枚
// 一次性占位成员。budget = cap - online,即「在当前在线数之上还允许多少在途准入」。
// 清理、判定、占位在同一段脚本里,并发的快速通道调用彼此可见,不会各读同一个 free>0
// 快照后全部旁路队列签票。
//
// 占位之后的整键续期与 admittedHoldScript 相同:先 ZADD,剩余 PTTL < W 才 PEXPIRE W(只增不减,
// 两个陷阱见 admittedKeyTTL)。
//
//	KEYS[1] = admitted:zset:zone:{zone}
//	ARGV[1] = now_ms   ARGV[2] = budget   ARGV[3] = member(fast:<uuid>)
//	ARGV[4] = 占位到期毫秒   ARGV[5] = W 毫秒(整键 TTL 下限,只做垃圾回收)
//	返回 1 = 占到,0 = 已满
var reserveFastPathSlotScript = redis.NewScript(`
redis.call('ZREMRANGEBYSCORE', KEYS[1], '-inf', ARGV[1])
if redis.call('ZCARD', KEYS[1]) >= tonumber(ARGV[2]) then return 0 end
redis.call('ZADD', KEYS[1], ARGV[4], ARGV[3])
if redis.call('PTTL', KEYS[1]) < tonumber(ARGV[5]) then
	redis.call('PEXPIRE', KEYS[1], ARGV[5])
end
return 1
`)

// TryReserveFastPathSlot 尝试为快速通道原子占用一个准入名额。budget 传 cap-online。
// 返回 true 表示占用成功(调用方可直接签发 gate token);false 表示已满(应入队)。
// 快速通道当场交票,等同排队路径的「已取走」,所以占位只留 handoffGrace,不是 AdmitTTL;
// 之后它按自己的分数到期,不需要、也没有回收步骤。
func (q *Queue) TryReserveFastPathSlot(ctx context.Context, zoneID uint32, budget uint32) (bool, error) {
	if budget == 0 {
		return false, nil
	}
	now := q.now()
	member := "fast:" + uuid.NewString()
	res, err := reserveFastPathSlotScript.Run(
		ctx, q.rdb,
		[]string{admittedZSetKey(zoneID)},
		now.UnixMilli(), budget, member,
		now.Add(q.handoffGrace).UnixMilli(), q.admittedKeyTTL().Milliseconds(),
	).Int()
	if err != nil {
		return false, err
	}
	return res == 1, nil
}

// QueueLen is a thin wrapper over ZCARD for capacity decisions.
func (q *Queue) QueueLen(ctx context.Context, zoneID uint32) (uint32, error) {
	n, err := q.rdb.ZCard(ctx, zoneZSetKey(zoneID)).Result()
	if err != nil {
		return 0, err
	}
	return uint32(n), nil
}
