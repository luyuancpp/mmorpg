//go:build integration

package inbox

// 真库集成测试:go test -tags integration ./internal/inbox/... -count=1 -v
//
// PLAYER_INBOX_TEST_MYSQL_DSN 为空则整组 Skip。⚠ 只能指向**一次性**测试库:每个用例都会 DROP 并重建
// player_inbox_stream / player_inbox_item / player_inbox_audit / schema_migrations。DSN 必须带库名,例如
//   appuser:apppass123@tcp(127.0.0.1:3306)/player_inbox_it?parseTime=true&charset=utf8mb4
// 表结构先经 schemamigrate.Up 按 proto 建出,与生产同一条路径。MySQL 需 binlog_format=ROW(RC 事务的前提)。
//
// # 验收开关 PLAYER_INBOX_REQUIRE_MYSQL_TESTS(与 go/trade 的 TRADE_REQUIRE_MYSQL_TESTS 同一口径)
//
// 它不是第二个门控,只决定「跳过算不算失败」:设了之后 DSN 为空 → 判红,不再整组静默 Skip。验收一律这样跑
// (PowerShell,工作目录 go/player_inbox;密码见 deploy/docker-compose.yml,不要写进任何被跟踪的文件):
//
//   $env:PLAYER_INBOX_TEST_MYSQL_DSN='root:<root 密码>@tcp(127.0.0.1:3306)/player_inbox_it?parseTime=true&charset=utf8mb4'
//   $env:PLAYER_INBOX_REQUIRE_MYSQL_TESTS='1'
//   go test -tags integration ./internal/inbox/... -count=1 -v
//   go test -tags integration ./internal/inbox/... -run 'Concurrent|Race' -count=20
//
// ⚠ 不带 -tags integration 时本文件不参与编译,go test 照样输出 ok —— 验收必须带标签,并在 -v 输出里逐条看到 PASS。

import (
	"context"
	"database/sql"
	"errors"
	"os"
	"sort"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"player_inbox/internal/constants"
	"player_inbox/internal/data"

	componentpb "proto/common/component"
	inboxpb "proto/player_inbox"

	"schemamigrate"

	_ "github.com/go-sql-driver/mysql"
	"google.golang.org/protobuf/proto"
)

const requireMySQLEnv = "PLAYER_INBOX_REQUIRE_MYSQL_TESTS"

type itClock struct{ ms atomic.Int64 }

func (c *itClock) Now() time.Time    { return time.UnixMilli(c.ms.Load()) }
func (c *itClock) set(ms uint64)     { c.ms.Store(int64(ms)) }
func (c *itClock) advance(ms uint64) { c.ms.Add(int64(ms)) }

type itPlayers struct{ missing map[uint64]bool }

func (p itPlayers) PlayerExists(_ context.Context, id uint64) (bool, error) { return !p.missing[id], nil }

type itEpochs struct {
	mu  sync.Mutex
	m   map[uint64]uint64
	err error
}

func (e *itEpochs) OwnerEpoch(_ context.Context, id uint64) (uint64, error) {
	e.mu.Lock()
	defer e.mu.Unlock()
	if e.err != nil {
		return 0, e.err
	}
	return e.m[id], nil
}

func (e *itEpochs) set(id, epoch uint64) {
	e.mu.Lock()
	defer e.mu.Unlock()
	e.m[id] = epoch
}

type itNotifier struct {
	mu     sync.Mutex
	result NotifyResult
	calls  int
}

func (n *itNotifier) Notify(context.Context, uint64, inboxpb.PlayerInboxDomain) NotifyResult {
	n.mu.Lock()
	defer n.mu.Unlock()
	n.calls++
	return n.result
}

func (n *itNotifier) setResult(r NotifyResult) {
	n.mu.Lock()
	defer n.mu.Unlock()
	n.result = r
}

type itEnv struct {
	db       *sql.DB
	store    *Store
	clock    *itClock
	epochs   *itEpochs
	notifier *itNotifier
	missing  map[uint64]bool
}

func openIntegration(t *testing.T) *itEnv {
	t.Helper()
	dsn := os.Getenv("PLAYER_INBOX_TEST_MYSQL_DSN")
	if dsn == "" {
		if os.Getenv(requireMySQLEnv) != "" {
			t.Fatalf("%s 已设置(验收模式),但 PLAYER_INBOX_TEST_MYSQL_DSN 为空", requireMySQLEnv)
		}
		t.Skip("PLAYER_INBOX_TEST_MYSQL_DSN 未设置,跳过收件箱存储集成测试(只能指向一次性库)")
	}
	db, err := sql.Open("mysql", dsn)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = db.Close() })
	db.SetMaxOpenConns(64)

	ctx, cancel := context.WithTimeout(context.Background(), 60*time.Second)
	defer cancel()
	var dbName string
	if err := db.QueryRowContext(ctx, "SELECT DATABASE()").Scan(&dbName); err != nil || dbName == "" {
		t.Fatalf("DSN 必须带库名: %v", err)
	}
	for _, stmt := range []string{
		"DROP TABLE IF EXISTS player_inbox_audit",
		"DROP TABLE IF EXISTS player_inbox_item",
		"DROP TABLE IF EXISTS player_inbox_stream",
		"DROP TABLE IF EXISTS schema_migrations",
	} {
		if _, err := db.ExecContext(ctx, stmt); err != nil {
			t.Fatalf("%s: %v", stmt, err)
		}
	}
	report, err := schemamigrate.Up(ctx, db, schemamigrate.Options{Database: dbName, Tables: data.Tables(), Logf: t.Logf})
	if err != nil {
		t.Fatalf("schemamigrate.Up: %v", err)
	}
	if len(report.Manual) != 0 {
		t.Fatalf("新建库不应出现需人工项: %v", report.Manual)
	}

	reg, err := NewRegistry()
	if err != nil {
		t.Fatal(err)
	}
	env := &itEnv{
		db:       db,
		clock:    &itClock{},
		epochs:   &itEpochs{m: map[uint64]uint64{}},
		notifier: &itNotifier{result: NotifySent},
		missing:  map[uint64]bool{},
	}
	env.clock.set(testNowMs)
	env.store, err = NewStore(StoreDeps{
		DB:       db,
		Registry: reg,
		Allowed:  allTypesAllowed(),
		Players:  itPlayers{missing: env.missing},
		Epochs:   env.epochs,
		Notifier: env.notifier,
		Now:      env.clock.Now,
	})
	if err != nil {
		t.Fatal(err)
	}
	return env
}

func itCtx(t *testing.T) context.Context {
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	t.Cleanup(cancel)
	return ctx
}

func gainPayload(t *testing.T, currency uint32) []byte {
	return marshal(t, &inboxpb.GmCurrencyGainPayload{CurrencyType: currency})
}

// enqueue 用 GM 封锁类型入箱,同事务写审计。
func (e *itEnv) enqueue(t *testing.T, playerID, requestID, issuedAt uint64, payload []byte) (EnqueueResult, error) {
	ctx := itCtx(t)
	return e.store.Enqueue(ctx, EnqueueParams{
		PlayerID:   playerID,
		Type:       gmBlock,
		Producer:   inboxpb.PlayerInboxProducerKind_PLAYER_INBOX_PRODUCER_GM,
		RequestID:  requestID,
		IssuedAtMs: issuedAt,
		Payload:    payload,
		Operator:   "ops_it",
		Note:       "it",
	}, func(tx *sql.Tx, seq uint64) error {
		return InsertAudit(ctx, tx, &inboxpb.PlayerInboxAudit{
			PlayerId: playerID, Domain: uint32(gmDomain), Seq: seq,
			Action:   uint32(inboxpb.PlayerInboxAuditAction_PLAYER_INBOX_AUDIT_SANCTION),
			Operator: "ops_it", PeerAddr: "127.0.0.1:1", Reason: "it", CreatedMs: testNowMs,
		})
	})
}

func (e *itEnv) mustEnqueue(t *testing.T, playerID, requestID uint64) EnqueueResult {
	t.Helper()
	res, err := e.enqueue(t, playerID, requestID, uint64(e.clock.ms.Load()), gainPayload(t, 1))
	if err != nil || res.Code != 0 || !res.Created {
		t.Fatalf("enqueue player=%d request=%d: res=%+v err=%v", playerID, requestID, res, err)
	}
	return res
}

func (e *itEnv) stream(t *testing.T, playerID uint64) *inboxpb.PlayerInboxStream {
	t.Helper()
	st, err := readStream(itCtx(t), e.db, RowShard(playerID), playerID, gmDomain)
	if err != nil {
		t.Fatal(err)
	}
	return st
}

func (e *itEnv) item(t *testing.T, playerID, seq uint64) *inboxpb.PlayerInboxItemRow {
	t.Helper()
	it, err := readItem(itCtx(t), e.db, RowShard(playerID), playerID, gmDomain, seq)
	if err != nil {
		t.Fatal(err)
	}
	return it
}

func (e *itEnv) fetch(t *testing.T, playerID, epoch, decidedThrough uint64, maxItems uint32) *inboxpb.FetchPlayerInboxResponse {
	t.Helper()
	resp, err := e.store.Fetch(itCtx(t), &inboxpb.FetchPlayerInboxRequest{
		PlayerId: playerID, Domain: gmDomain, OwnerEpoch: epoch, CorrelationId: 99,
		MaxItems: maxItems, ConsumerDecidedThrough: decidedThrough,
	})
	if err != nil {
		t.Fatalf("Fetch: %v", err)
	}
	if resp.GetPlayerId() != playerID || resp.GetCorrelationId() != 99 || resp.GetDomain() != gmDomain {
		t.Fatalf("回显字段不对: %+v", resp)
	}
	return resp
}

func (e *itEnv) ack(t *testing.T, playerID, epoch uint64, reports ...*inboxpb.PlayerInboxDecisionReport) *inboxpb.AckPlayerInboxResponse {
	t.Helper()
	resp, err := e.store.Ack(itCtx(t), &inboxpb.AckPlayerInboxRequest{
		PlayerId: playerID, Domain: gmDomain, OwnerEpoch: epoch, CorrelationId: 7, Decisions: reports,
	})
	if err != nil {
		t.Fatalf("Ack: %v", err)
	}
	return resp
}

func dec(seq uint64, outcome inboxpb.PlayerInboxOutcome, reason uint32, requestID uint64) *inboxpb.PlayerInboxDecisionReport {
	return &inboxpb.PlayerInboxDecisionReport{Seq: seq, Outcome: outcome, Reason: reason, RequestId: requestID}
}

func auditCount(t *testing.T, db *sql.DB, playerID uint64) int {
	t.Helper()
	var n int
	if err := db.QueryRowContext(itCtx(t), "SELECT COUNT(*) FROM player_inbox_audit WHERE `shard` = ? AND `player_id` = ?",
		RowShard(playerID), playerID).Scan(&n); err != nil {
		t.Fatal(err)
	}
	return n
}

// ── 入箱 ─────────────────────────────────────────────────────────────────

func TestIntegrationEnqueueCreateDuplicateConflict(t *testing.T) {
	e := openIntegration(t)
	const pid, rid = uint64(1001), uint64(5001)
	issued := testNowMs - 1000

	res, err := e.enqueue(t, pid, rid, issued, gainPayload(t, 1))
	if err != nil || res.Code != 0 || !res.Created {
		t.Fatalf("首次入箱: %+v %v", res, err)
	}
	if res.Seq != testNowMs<<12 {
		t.Fatalf("seq = %d, want now<<12 = %d", res.Seq, testNowMs<<12)
	}
	st := e.stream(t, pid)
	if st.GetNextSeq() != res.Seq+1 || st.GetOpenCount() != 1 {
		t.Fatalf("流行 = %+v", st)
	}
	if st.GetNotifyDueMs() != 0 {
		t.Fatalf("提醒送达后 notify_due_ms 应以 next_seq CAS 清零,got %d", st.GetNotifyDueMs())
	}

	// 原样重试 → DUPLICATE(原 seq、当前状态),不新增行与审计。
	dup, err := e.enqueue(t, pid, rid, issued, gainPayload(t, 1))
	if err != nil || dup.Code != 0 || dup.Created || dup.Seq != res.Seq || dup.State != statePending {
		t.Fatalf("重试应 DUPLICATE: %+v %v", dup, err)
	}
	// 刷新 issued_at 重试 → CONFLICT(看得见的错误,不会静默重复入箱)。
	conflict, err := e.enqueue(t, pid, rid, issued+1, gainPayload(t, 1))
	if err != nil || conflict.Code != constants.ErrIdempotencyConflict {
		t.Fatalf("issued_at 不同应 CONFLICT: %+v %v", conflict, err)
	}
	// 载荷不同 → CONFLICT。
	conflict, err = e.enqueue(t, pid, rid, issued, gainPayload(t, 2))
	if err != nil || conflict.Code != constants.ErrIdempotencyConflict {
		t.Fatalf("载荷不同应 CONFLICT: %+v %v", conflict, err)
	}
	if n := auditCount(t, e.db, pid); n != 1 {
		t.Fatalf("审计行 = %d, want 1(只有新建那次)", n)
	}
	if st := e.stream(t, pid); st.GetOpenCount() != 1 {
		t.Fatalf("重试不应改 open_count: %+v", st)
	}
}

func TestIntegrationEnqueueRejectionsWriteNothing(t *testing.T) {
	e := openIntegration(t)
	const pid = uint64(1002)
	e.missing[1003] = true
	ctx := itCtx(t)
	cases := []struct {
		name string
		p    EnqueueParams
		want uint32
	}{
		{"未知类型", EnqueueParams{PlayerID: pid, Type: inboxpb.PlayerInboxMessageType(99), RequestID: 1, IssuedAtMs: testNowMs, Operator: "o"}, constants.ErrUnknownType},
		{"玩家不存在", EnqueueParams{PlayerID: 1003, Type: gmBlock, RequestID: 2, IssuedAtMs: testNowMs, Operator: "o"}, constants.ErrPlayerNotFound},
		{"operator 超长", EnqueueParams{PlayerID: pid, Type: gmBlock, RequestID: 3, IssuedAtMs: testNowMs, Operator: strings.Repeat("x", 65)}, constants.ErrTextTooLong},
		{"note 超长", EnqueueParams{PlayerID: pid, Type: gmBlock, RequestID: 4, IssuedAtMs: testNowMs, Operator: "o", Note: strings.Repeat("n", 257)}, constants.ErrTextTooLong},
		{"签发太早", EnqueueParams{PlayerID: pid, Type: gmBlock, RequestID: 5, IssuedAtMs: testNowMs - uint64(constants.IssuedAtMaxPast.Milliseconds()) - 1, Operator: "o"}, constants.ErrIssuedOutOfWindow},
		{"签发在未来", EnqueueParams{PlayerID: pid, Type: gmBlock, RequestID: 6, IssuedAtMs: testNowMs + uint64(constants.IssuedAtMaxFuture.Milliseconds()) + 1, Operator: "o"}, constants.ErrIssuedOutOfWindow},
		{"request_id 为 0", EnqueueParams{PlayerID: pid, Type: gmBlock, RequestID: 0, IssuedAtMs: testNowMs, Operator: "o"}, constants.ErrInvalidParameter},
	}
	for _, tc := range cases {
		tc.p.Producer = inboxpb.PlayerInboxProducerKind_PLAYER_INBOX_PRODUCER_GM
		res, err := e.store.Enqueue(ctx, tc.p, nil)
		if err != nil || res.Code != tc.want {
			t.Errorf("%s: res=%+v err=%v, want code %d", tc.name, res, err, tc.want)
		}
	}
	if st := e.stream(t, pid); st != nil {
		t.Fatalf("被拒的入箱不应建流行: %+v", st)
	}
}

func TestIntegrationEnqueueFullKeepsDuplicates(t *testing.T) {
	e := openIntegration(t)
	const pid = uint64(1004)
	spec, _ := e.store.registry.Domain(gmDomain)
	for i := uint64(1); i <= uint64(spec.GetMaxPending()); i++ {
		e.mustEnqueue(t, pid, i)
	}
	res, err := e.enqueue(t, pid, 9999, testNowMs, gainPayload(t, 1))
	if err != nil || res.Code != constants.ErrFull {
		t.Fatalf("满箱应 FULL: %+v %v", res, err)
	}
	// 满箱时原请求的重试仍是 DUPLICATE,不是 FULL。
	dup, err := e.enqueue(t, pid, 1, testNowMs, gainPayload(t, 1))
	if err != nil || dup.Code != 0 || dup.Created {
		t.Fatalf("满箱时重试应 DUPLICATE: %+v %v", dup, err)
	}
}

// TestIntegrationRaceRetryGetsDuplicateNotFull:差一条满箱时原请求与重试赛跑,输的一方必须拿到 DUPLICATE(设计 §6.2 7.2、§8 #2)。
func TestIntegrationRaceRetryGetsDuplicateNotFull(t *testing.T) {
	e := openIntegration(t)
	spec, _ := e.store.registry.Domain(gmDomain)
	for round := uint64(0); round < 5; round++ {
		pid := 2000 + round
		for i := uint64(1); i < uint64(spec.GetMaxPending()); i++ {
			e.mustEnqueue(t, pid, i)
		}
		const raceRID = uint64(777)
		payload := gainPayload(t, 1) // 在测试 goroutine 里序列化:子 goroutine 不调 t.Fatal
		var wg sync.WaitGroup
		results := make([]EnqueueResult, 2)
		errs := make([]error, 2)
		for g := 0; g < 2; g++ {
			wg.Add(1)
			go func(g int) {
				defer wg.Done()
				results[g], errs[g] = e.enqueue(t, pid, raceRID, testNowMs, payload)
			}(g)
		}
		wg.Wait()
		created, dup := 0, 0
		for g := 0; g < 2; g++ {
			if errs[g] != nil {
				t.Fatalf("round %d: %v", round, errs[g])
			}
			switch {
			case results[g].Code == 0 && results[g].Created:
				created++
			case results[g].Code == 0 && !results[g].Created:
				dup++
			default:
				t.Fatalf("round %d: 赛跑一方拿到 code=%d(不应是 FULL)", round, results[g].Code)
			}
		}
		if created != 1 || dup != 1 {
			t.Fatalf("round %d: created=%d dup=%d, want 1/1", round, created, dup)
		}
	}
}

// TestIntegrationConcurrentEnqueueSeqContiguous:同一毫秒并发入箱,seq 连续无空洞、无重复,next_seq / open_count 一致。
func TestIntegrationConcurrentEnqueueSeqContiguous(t *testing.T) {
	e := openIntegration(t)
	const pid, n = uint64(1005), 20
	payload := gainPayload(t, 1)
	var wg sync.WaitGroup
	seqs := make([]uint64, n)
	for i := 0; i < n; i++ {
		wg.Add(1)
		go func(i int) {
			defer wg.Done()
			res, err := e.enqueue(t, pid, uint64(100+i), testNowMs, payload)
			if err != nil || res.Code != 0 || !res.Created {
				t.Errorf("goroutine %d: %+v %v", i, res, err)
				return
			}
			seqs[i] = res.Seq
		}(i)
	}
	wg.Wait()
	sort.Slice(seqs, func(a, b int) bool { return seqs[a] < seqs[b] })
	floor := testNowMs << 12
	for i, s := range seqs {
		if s != floor+uint64(i) {
			t.Fatalf("seqs = %v, want %d..%d 连续", seqs, floor, floor+n-1)
		}
	}
	if st := e.stream(t, pid); st.GetNextSeq() != floor+n || st.GetOpenCount() != n {
		t.Fatalf("流行 = %+v", st)
	}
}

// ── Fetch ────────────────────────────────────────────────────────────────

func TestIntegrationFetchNoStreamAndFastPathWritesNothing(t *testing.T) {
	e := openIntegration(t)
	if resp := e.fetch(t, 3001, 1, 0, 0); resp.GetFence() != inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_NO_STREAM {
		t.Fatalf("无流应 NO_STREAM: %+v", resp)
	}

	const pid = uint64(3002)
	if err := e.store.ensureStream(itCtx(t), RowShard(pid), pid, gmDomain, testNowMs); err != nil {
		t.Fatal(err)
	}
	before := e.stream(t, pid)
	e.epochs.set(pid, 5)
	e.clock.advance(10_000)
	resp := e.fetch(t, pid, 5, 0, 0)
	if resp.GetFence() != inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_FIRST || len(resp.GetItems()) != 0 {
		t.Fatalf("快路径应按快照判 FIRST 且无条目: %+v", resp)
	}
	after := e.stream(t, pid)
	if !proto.Equal(before, after) {
		t.Fatalf("快路径不得写任何东西:before=%+v after=%+v", before, after)
	}
}

func TestIntegrationFetchLockedPathDeliversAndFences(t *testing.T) {
	e := openIntegration(t)
	const pid = uint64(3003)
	var seqs []uint64
	for i := uint64(1); i <= 3; i++ {
		seqs = append(seqs, e.mustEnqueue(t, pid, i).Seq)
	}
	e.epochs.set(pid, 5)

	resp := e.fetch(t, pid, 5, 0, 2)
	if resp.GetFence() != inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_FIRST || !resp.GetHasMore() || len(resp.GetItems()) != 2 {
		t.Fatalf("第一页: %+v", resp)
	}
	if resp.GetItems()[0].GetSeq() != seqs[0] || resp.GetItems()[1].GetSeq() != seqs[1] {
		t.Fatalf("应按 seq 升序: %+v", resp.GetItems())
	}
	if it := resp.GetItems()[0]; it.GetDeliveryCount() != 1 || len(it.GetPayload()) == 0 || it.GetRequestId() != 1 {
		t.Fatalf("条目字段: %+v", it)
	}
	if st := e.stream(t, pid); st.GetFenceEpoch() != 5 {
		t.Fatalf("FIRST 应写回 fence_epoch=5: %+v", st)
	}

	resp = e.fetch(t, pid, 5, 0, 0)
	if resp.GetFence() != inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_MATCH || resp.GetHasMore() || len(resp.GetItems()) != 3 {
		t.Fatalf("第二次拉取: %+v", resp)
	}
	if got := e.item(t, pid, seqs[0]); got.GetDeliveryCount() != 2 || got.GetLastDeliveredEpoch() != 5 {
		t.Fatalf("投递计数: %+v", got)
	}
}

func TestIntegrationFetchFenceRejections(t *testing.T) {
	e := openIntegration(t)
	const pid = uint64(3004)
	e.mustEnqueue(t, pid, 1)
	e.epochs.set(pid, 5)
	e.fetch(t, pid, 5, 0, 0) // FIRST → fence = 5

	if resp := e.fetch(t, pid, 4, 0, 0); resp.GetFence() != inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_STALE ||
		resp.GetErrorMessage().GetId() != constants.ErrStaleOwner || len(resp.GetItems()) != 0 {
		t.Fatalf("旧 epoch 应 STALE: %+v", resp)
	}
	if resp := e.fetch(t, pid, 0, 0, 0); resp.GetFence() != inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_EPOCH_REQUIRED {
		t.Fatalf("epoch 0 应 EPOCH_REQUIRED: %+v", resp)
	}
	e.epochs.set(pid, 6)
	if resp := e.fetch(t, pid, 7, 0, 0); resp.GetFence() != inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_EPOCH_AHEAD ||
		resp.GetErrorMessage().GetId() != constants.ErrEpochAhead {
		t.Fatalf("超过 R 应 EPOCH_AHEAD: %+v", resp)
	}
	if st := e.stream(t, pid); st.GetFenceEpoch() != 5 {
		t.Fatalf("拒绝结局不得改围栏: %+v", st)
	}
	// R 读失败:不改任何状态,按故障返回。
	e.epochs.mu.Lock()
	e.epochs.err = errors.New("redis down")
	e.epochs.mu.Unlock()
	if _, err := e.store.Fetch(itCtx(t), &inboxpb.FetchPlayerInboxRequest{PlayerId: pid, Domain: gmDomain, OwnerEpoch: 6}); err == nil {
		t.Fatal("R 读失败应返回错误")
	}
	if st := e.stream(t, pid); st.GetFenceEpoch() != 5 {
		t.Fatalf("R 读失败不得改围栏: %+v", st)
	}
}

func TestIntegrationFetchCursorAheadRaisesNextSeq(t *testing.T) {
	e := openIntegration(t)
	const pid = uint64(3005)
	res := e.mustEnqueue(t, pid, 1)
	e.epochs.set(pid, 2)
	watermark := res.Seq + 1000
	resp := e.fetch(t, pid, 2, watermark, 0)
	if !FenceAllows(resp.GetFence()) {
		t.Fatalf("CursorAhead 仍应照常投递: %+v", resp)
	}
	if st := e.stream(t, pid); st.GetNextSeq() != watermark+1 {
		t.Fatalf("next_seq 应抬到水位+1 = %d,got %d", watermark+1, st.GetNextSeq())
	}
	next := e.mustEnqueue(t, pid, 2)
	if next.Seq <= watermark {
		t.Fatalf("抬号后新 seq %d 应大于水位 %d", next.Seq, watermark)
	}
}

// ── Ack ──────────────────────────────────────────────────────────────────

func TestIntegrationAckResults(t *testing.T) {
	e := openIntegration(t)
	const pid = uint64(4001)
	a := e.mustEnqueue(t, pid, 11).Seq
	b := e.mustEnqueue(t, pid, 12).Seq
	c := e.mustEnqueue(t, pid, 13).Seq
	e.epochs.set(pid, 3)
	e.fetch(t, pid, 3, 0, 2) // 只投递 a、b

	resp := e.ack(t, pid, 3,
		dec(a, outApplied, 0, 11),
		dec(b, outSkipped, 1, 12),     // b 没有指令 → SKIP_NOT_REQUESTED
		dec(c, outApplied, 0, 13),     // c 未投递 → NOT_DELIVERED
		dec(c+1000, outApplied, 0, 1), // 不存在 → UNKNOWN_SEQ
	)
	want := []inboxpb.PlayerInboxAckResult{
		inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_FINALIZED,
		inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_SKIP_NOT_REQUESTED,
		inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_NOT_DELIVERED,
		inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_UNKNOWN_SEQ,
	}
	if len(resp.GetResults()) != len(want) {
		t.Fatalf("results = %+v", resp.GetResults())
	}
	for i, w := range want {
		if resp.GetResults()[i].GetResult() != w {
			t.Fatalf("results[%d] = %v, want %v", i, resp.GetResults()[i].GetResult(), w)
		}
	}
	if st := e.stream(t, pid); st.GetOpenCount() != 2 {
		t.Fatalf("只终结了 a,open_count 应为 2: %+v", st)
	}
	if it := e.item(t, pid, a); it.GetState() != uint32(stateApplied) || it.GetDecidedEpoch() != 3 ||
		it.GetFinalizedBy() != uint32(inboxpb.PlayerInboxFinalizer_PLAYER_INBOX_FINALIZER_CONSUMER) {
		t.Fatalf("a 终结字段: %+v", it)
	}

	resp = e.ack(t, pid, 3,
		dec(a, outApplied, 0, 11),  // 重复确认 → ALREADY_SAME
		dec(b, outApplied, 0, 999), // request_id 不一致 → MISMATCH
	)
	if resp.GetResults()[0].GetResult() != inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_ALREADY_SAME ||
		resp.GetResults()[1].GetResult() != inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_MISMATCH {
		t.Fatalf("第二轮: %+v", resp.GetResults())
	}
	resp = e.ack(t, pid, 3, dec(a, outRejected, 29003, 11)) // 终态不一致 → CONFLICT,永不翻转
	if resp.GetResults()[0].GetResult() != inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_CONFLICT {
		t.Fatalf("应 CONFLICT: %+v", resp.GetResults())
	}
	if it := e.item(t, pid, a); it.GetState() != uint32(stateApplied) {
		t.Fatalf("CONFLICT 不得翻转终态: %+v", it)
	}
	// UNKNOWN → SETTLED_UNKNOWN。
	resp = e.ack(t, pid, 3, dec(b, outUnknown, 0, 12))
	if resp.GetResults()[0].GetResult() != inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_FINALIZED ||
		resp.GetResults()[0].GetState() != stateUnknown {
		t.Fatalf("UNKNOWN 应收尾为 SETTLED_UNKNOWN: %+v", resp.GetResults())
	}
}

// TestIntegrationAckStaleAfterAdvance:新 owner 推进围栏之后,旧 owner 的 Ack 被判 STALE 且不改任何行(设计 §7.10)。
func TestIntegrationAckStaleAfterAdvance(t *testing.T) {
	e := openIntegration(t)
	const pid = uint64(4002)
	a := e.mustEnqueue(t, pid, 1).Seq
	e.epochs.set(pid, 5)
	e.fetch(t, pid, 5, 0, 0) // FIRST
	e.epochs.set(pid, 6)
	if resp := e.fetch(t, pid, 6, 0, 0); resp.GetFence() != inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_ADVANCE {
		t.Fatalf("新 owner 应 ADVANCE: %+v", resp)
	}
	resp := e.ack(t, pid, 5, dec(a, outApplied, 0, 1))
	if resp.GetFence() != inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_STALE || len(resp.GetResults()) != 0 {
		t.Fatalf("旧 owner 的 Ack 应 STALE 且无逐条结果: %+v", resp)
	}
	if it := e.item(t, pid, a); it.GetState() != uint32(statePending) {
		t.Fatalf("STALE 不得终结: %+v", it)
	}
	if resp := e.ack(t, pid, 6, dec(a, outApplied, 0, 1)); resp.GetResults()[0].GetResult() != inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_FINALIZED {
		t.Fatalf("新 owner 的 Ack 应 FINALIZED: %+v", resp)
	}
}

// ── 作废 ─────────────────────────────────────────────────────────────────

func TestIntegrationVoid(t *testing.T) {
	e := openIntegration(t)
	const pid = uint64(5001)
	a := e.mustEnqueue(t, pid, 1).Seq
	b := e.mustEnqueue(t, pid, 2).Seq
	e.epochs.set(pid, 2)
	e.fetch(t, pid, 2, 0, 1) // 只投递 a
	ctx := itCtx(t)
	void := func(seq uint64) *inboxpb.VoidPlayerInboxItemResponse {
		resp, err := e.store.Void(ctx, VoidParams{
			PlayerID: pid, Domain: gmDomain, Seq: seq, Reason: inboxpb.PlayerInboxVoidReason_PLAYER_INBOX_VOID_CANCELLED,
			Operator: "ops_it", Note: "wrong target", PeerAddr: "127.0.0.1:2",
		})
		if err != nil {
			t.Fatal(err)
		}
		return resp
	}

	// b 从未投递 → 直接 VOIDED(finalized_by=ADMIN),open_count−1。
	if resp := void(b); resp.GetErrorMessage() != nil || resp.GetState() != stateVoided || resp.GetVoidRequested() {
		t.Fatalf("未投递应直接作废: %+v", resp)
	}
	if it := e.item(t, pid, b); it.GetFinalizedBy() != uint32(inboxpb.PlayerInboxFinalizer_PLAYER_INBOX_FINALIZER_ADMIN) {
		t.Fatalf("finalized_by 应为 ADMIN: %+v", it)
	}
	// a 已投递 → 只挂指令,仍 PENDING;再作废一次幂等。
	if resp := void(a); resp.GetState() != statePending || !resp.GetVoidRequested() {
		t.Fatalf("已投递应只挂指令: %+v", resp)
	}
	if resp := void(a); resp.GetState() != statePending || !resp.GetVoidRequested() || resp.GetErrorMessage() != nil {
		t.Fatalf("重复作废应幂等: %+v", resp)
	}
	if st := e.stream(t, pid); st.GetOpenCount() != 1 {
		t.Fatalf("open_count = %d, want 1(挂指令的仍计入)", st.GetOpenCount())
	}
	// 再拉取:a 带 VOID 指令且不带载荷;消费方报 SKIPPED → VOIDED。
	resp := e.fetch(t, pid, 2, 0, 0)
	if len(resp.GetItems()) != 1 || resp.GetItems()[0].GetDirective() != directiveVoid || len(resp.GetItems()[0].GetPayload()) != 0 {
		t.Fatalf("挂指令的条目应不带载荷: %+v", resp.GetItems())
	}
	if ack := e.ack(t, pid, 2, dec(a, outSkipped, 1, 1)); ack.GetResults()[0].GetState() != stateVoided {
		t.Fatalf("SKIPPED 应终结为 VOIDED: %+v", ack.GetResults())
	}
	// 终态 → StateConflict;不存在 → NotFound。
	if resp := void(a); resp.GetErrorMessage().GetId() != constants.ErrItemStateConflict {
		t.Fatalf("终态作废应 StateConflict: %+v", resp)
	}
	if resp := void(a + 99999); resp.GetErrorMessage().GetId() != constants.ErrItemNotFound {
		t.Fatalf("不存在应 NotFound: %+v", resp)
	}
	if n := auditCount(t, e.db, pid); n != 4 { // 2 次入箱 + b 作废 + a 作废(重复作废 INSERT IGNORE)
		t.Fatalf("审计行 = %d, want 4", n)
	}
}

// ── 后台循环的存储操作 ───────────────────────────────────────────────────

func TestIntegrationSweepSkipsPending(t *testing.T) {
	e := openIntegration(t)
	const pid = uint64(6001)
	// T0:入箱 a、b;只投递 a 并确认 APPLIED(a.updated_ms = T0)。
	a := e.mustEnqueue(t, pid, 1).Seq
	b := e.mustEnqueue(t, pid, 2).Seq
	e.epochs.set(pid, 1)
	e.fetch(t, pid, 1, 0, 1)
	e.ack(t, pid, 1, dec(a, outApplied, 0, 1))
	// T0+60s:投递并确认 b(b.updated_ms = T0+60s)。
	e.clock.advance(60_000)
	e.fetch(t, pid, 1, 0, 0)
	e.ack(t, pid, 1, dec(b, outApplied, 0, 2))
	// 回到 T0 再入箱 c:PENDING 且 updated_ms = T0(seq 由 next_seq 保证仍单调)。
	e.clock.set(testNowMs)
	c := e.mustEnqueue(t, pid, 3).Seq

	cutoff := testNowMs + 30_000 // 只有 a 是「保留期外的终态行」
	deleted, err := e.store.SweepTerminal(itCtx(t), cutoff, 500)
	if err != nil {
		t.Fatal(err)
	}
	if deleted != 1 {
		t.Fatalf("deleted = %d, want 1(只有旧终态行 a)", deleted)
	}
	if e.item(t, pid, a) != nil {
		t.Fatal("旧终态行 a 应被删")
	}
	if e.item(t, pid, b) == nil || e.item(t, pid, c) == nil {
		t.Fatal("保留期内的终态行 b 与 PENDING 行 c 不得删除")
	}
	if e.stream(t, pid) == nil || auditCount(t, e.db, pid) != 3 {
		t.Fatal("流行与审计行永不自动删除")
	}
}

// TestIntegrationNotifyCASSameMillisecond:同一毫秒两次入箱时,拿旧 next_seq 的补发不能清掉新入箱的提醒(设计 §6.3,无 ABA)。
func TestIntegrationNotifyCASSameMillisecond(t *testing.T) {
	e := openIntegration(t)
	const pid = uint64(6002)
	e.notifier.setResult(NotifyError) // 入箱后那次提醒失败,留给补发
	first := e.mustEnqueue(t, pid, 1)
	due, err := e.store.DueNotifications(itCtx(t), RowShard(pid), testNowMs, 100)
	if err != nil || len(due) != 1 || due[0].NextSeq != first.Seq+1 {
		t.Fatalf("due = %+v err=%v", due, err)
	}
	stale := due[0]
	second := e.mustEnqueue(t, pid, 2) // 同一毫秒(时钟固定)
	if st := e.stream(t, pid); st.GetNotifyDueMs() != testNowMs {
		t.Fatalf("两次 notify_due_ms 相同(同一毫秒):%+v", st)
	}

	e.notifier.setResult(NotifySent)
	e.store.ResendNotify(itCtx(t), stale)
	if st := e.stream(t, pid); st.GetNotifyDueMs() == 0 {
		t.Fatal("旧令牌的 CAS 不应清掉第二次入箱的提醒")
	}
	due, err = e.store.DueNotifications(itCtx(t), RowShard(pid), testNowMs, 100)
	if err != nil || len(due) != 1 || due[0].NextSeq != second.Seq+1 {
		t.Fatalf("due = %+v err=%v", due, err)
	}
	e.store.ResendNotify(itCtx(t), due[0])
	if st := e.stream(t, pid); st.GetNotifyDueMs() != 0 {
		t.Fatalf("当前令牌的 CAS 应清零: %+v", st)
	}
}

func TestIntegrationSummaryListLookup(t *testing.T) {
	e := openIntegration(t)
	const pid = uint64(7001)
	a := e.mustEnqueue(t, pid, 1).Seq
	e.mustEnqueue(t, pid, 2)
	e.epochs.set(pid, 9)
	e.fetch(t, pid, 9, 0, 1)
	ctx := itCtx(t)

	sum, err := e.store.Summary(ctx, pid)
	if err != nil || len(sum.GetStreams()) != 1 {
		t.Fatalf("Summary: %+v %v", sum, err)
	}
	s := sum.GetStreams()[0]
	if s.GetOpenCount() != 2 || s.GetDeliveredUnackedCount() != 1 || s.GetFenceEpoch() != 9 || sum.GetMaxFenceEpoch() != 9 ||
		s.GetOldestPendingCreatedMs() != testNowMs {
		t.Fatalf("Summary 字段: %+v", sum)
	}

	list, err := e.store.List(ctx, &inboxpb.ListPlayerInboxItemsRequest{PlayerId: pid, Domain: gmDomain, Limit: 1})
	if err != nil || len(list.GetItems()) != 1 || !list.GetHasMore() || list.GetItems()[0].GetSeq() != a {
		t.Fatalf("List 第一页: %+v %v", list, err)
	}
	list, err = e.store.List(ctx, &inboxpb.ListPlayerInboxItemsRequest{PlayerId: pid, Domain: gmDomain, AfterSeq: a})
	if err != nil || len(list.GetItems()) != 1 || list.GetHasMore() {
		t.Fatalf("List 第二页: %+v %v", list, err)
	}

	look, err := e.store.Lookup(ctx, &inboxpb.LookupPlayerInboxRequest{
		PlayerId: pid, ProducerKind: inboxpb.PlayerInboxProducerKind_PLAYER_INBOX_PRODUCER_GM, RequestId: 1,
	})
	if err != nil || look.GetItem().GetSeq() != a || look.GetItem().GetOperatorName() != "ops_it" {
		t.Fatalf("Lookup: %+v %v", look, err)
	}
	look, err = e.store.Lookup(ctx, &inboxpb.LookupPlayerInboxRequest{
		PlayerId: pid, ProducerKind: inboxpb.PlayerInboxProducerKind_PLAYER_INBOX_PRODUCER_GM, RequestId: 404,
	})
	if err != nil || look.GetErrorMessage().GetId() != constants.ErrItemNotFound {
		t.Fatalf("Lookup 不存在: %+v %v", look, err)
	}
}

type itLedgers struct {
	ledger *componentpb.PlayerInboxLedgerComp
	found  bool
}

func (l itLedgers) ReadInboxLedger(context.Context, uint64) (*componentpb.PlayerInboxLedgerComp, bool, error) {
	return l.ledger, l.found, nil
}

func TestIntegrationReconcile(t *testing.T) {
	e := openIntegration(t)
	const pid = uint64(8001)
	a := e.mustEnqueue(t, pid, 1).Seq
	b := e.mustEnqueue(t, pid, 2).Seq
	e.epochs.set(pid, 1)
	e.fetch(t, pid, 1, 0, 0)
	e.ack(t, pid, 1, dec(a, outApplied, 0, 1), dec(b, outApplied, 0, 2))
	c := e.mustEnqueue(t, pid, 3).Seq

	// 账本只到 a(b 的落盘丢了),却已处理过 c(确认丢了)。
	ledger := &componentpb.PlayerInboxLedgerComp{Cursors: []*componentpb.PlayerInboxDomainCursor{
		{Domain: uint32(gmDomain), DecidedThroughSeq: a},
	}}
	resp, err := e.store.ReconcilePlayer(itCtx(t), &inboxpb.ReconcilePlayerInboxRequest{PlayerId: pid}, itLedgers{ledger: ledger, found: true})
	if err != nil || !resp.GetLedgerFound() {
		t.Fatalf("ReconcilePlayer: %+v %v", resp, err)
	}
	if len(resp.GetDiffs()) != 1 || resp.GetDiffs()[0].GetSeq() != b ||
		resp.GetDiffs()[0].GetKind() != inboxpb.PlayerInboxReconcileDiffKind_PLAYER_INBOX_RECONCILE_FINAL_NOT_IN_LEDGER {
		t.Fatalf("diffs = %+v", resp.GetDiffs())
	}
	ledger.Cursors[0].DecidedThroughSeq = c
	resp, err = e.store.ReconcilePlayer(itCtx(t), &inboxpb.ReconcilePlayerInboxRequest{PlayerId: pid}, itLedgers{ledger: ledger, found: true})
	if err != nil || len(resp.GetDiffs()) != 1 ||
		resp.GetDiffs()[0].GetKind() != inboxpb.PlayerInboxReconcileDiffKind_PLAYER_INBOX_RECONCILE_PENDING_BUT_DECIDED {
		t.Fatalf("diffs = %+v %v", resp.GetDiffs(), err)
	}

	mismatches, err := e.store.ReconcileRecent(itCtx(t), 0, itLedgers{ledger: &componentpb.PlayerInboxLedgerComp{Cursors: []*componentpb.PlayerInboxDomainCursor{
		{Domain: uint32(gmDomain), DecidedThroughSeq: a},
	}}, found: true})
	if err != nil || mismatches != 1 {
		t.Fatalf("ReconcileRecent mismatches = %d err=%v, want 1(b)", mismatches, err)
	}
	if n, err := e.store.ReconcileRecent(itCtx(t), 0, itLedgers{found: false}); err != nil || n != 0 {
		t.Fatalf("读不到存档计 unknown,不算不一致: %d %v", n, err)
	}
}
