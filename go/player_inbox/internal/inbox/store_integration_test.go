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
	"player_inbox/internal/metrics"

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

func (p itPlayers) PlayerExists(_ context.Context, id uint64) (bool, error) {
	return !p.missing[id], nil
}

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

// TestIntegrationAckEpochBoundReadFailureChangesNothing:Ack 需要上界 R(req > fence)而 R 读失败时按故障返回,
// 行与流都不改(设计 §7.8「R 读失败不改任何状态」,§11.4 验收)。
func TestIntegrationAckEpochBoundReadFailureChangesNothing(t *testing.T) {
	e := openIntegration(t)
	const pid = uint64(4003)
	a := e.mustEnqueue(t, pid, 1).Seq
	e.epochs.set(pid, 5)
	e.fetch(t, pid, 5, 0, 0) // FIRST → fence = 5,a 已投递
	before := e.stream(t, pid)

	e.epochs.mu.Lock()
	e.epochs.err = errors.New("redis down")
	e.epochs.mu.Unlock()
	_, err := e.store.Ack(itCtx(t), &inboxpb.AckPlayerInboxRequest{
		PlayerId: pid, Domain: gmDomain, OwnerEpoch: 6, Decisions: []*inboxpb.PlayerInboxDecisionReport{dec(a, outApplied, 0, 1)},
	})
	if err == nil {
		t.Fatal("R 读失败应返回错误(由服务层折成 kPlayerInboxStoreUnavailable)")
	}
	if after := e.stream(t, pid); !proto.Equal(before, after) {
		t.Fatalf("R 读失败不得改流行:before=%+v after=%+v", before, after)
	}
	if it := e.item(t, pid, a); it.GetState() != uint32(statePending) {
		t.Fatalf("R 读失败不得终结条目: %+v", it)
	}
	// 不需要 R 的 MATCH 不受 Redis 故障影响。
	if resp := e.ack(t, pid, 5, dec(a, outApplied, 0, 1)); resp.GetResults()[0].GetResult() != inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_FINALIZED {
		t.Fatalf("MATCH 不读 R,应照常 FINALIZED: %+v", resp)
	}
}

// TestIntegrationCountScannedRows 钉住 fetch_scanned_rows 数的是锁路径走过的行(设计 §4.2):
// 终态行夹在 PENDING 行前面时也要数进去;读满 LIMIT 时只数到最后读出那一行。
func TestIntegrationCountScannedRows(t *testing.T) {
	e := openIntegration(t)
	const pid = uint64(4004)
	var seqs []uint64
	for i := uint64(1); i <= 4; i++ {
		seqs = append(seqs, e.mustEnqueue(t, pid, i).Seq)
	}
	e.epochs.set(pid, 1)
	e.fetch(t, pid, 1, 0, 2)
	e.ack(t, pid, 1, dec(seqs[0], outApplied, 0, 1), dec(seqs[1], outApplied, 0, 2)) // 前两行终态,后两行 PENDING
	ctx := itCtx(t)
	shard := RowShard(pid)

	if n, err := e.store.countScannedRows(ctx, shard, pid, gmDomain, 0, true); err != nil || n != 4 {
		t.Fatalf("整条流 = %d (%v), want 4(含两条终态行)", n, err)
	}
	if n, err := e.store.countScannedRows(ctx, shard, pid, gmDomain, seqs[2], false); err != nil || n != 3 {
		t.Fatalf("到 seqs[2] 为止 = %d (%v), want 3", n, err)
	}
	// 与 Fetch 的口径对上:LIMIT 2(max_items=1)读满时停在 seqs[3],走过 4 行;LIMIT 3 读不满 → 整条流。
	resp := e.fetch(t, pid, 1, 0, 1)
	if len(resp.GetItems()) != 1 || !resp.GetHasMore() || resp.GetItems()[0].GetSeq() != seqs[2] {
		t.Fatalf("max_items=1 应只投递 seqs[2] 且 has_more: %+v", resp)
	}
	if n, err := e.store.countScannedRows(ctx, shard, pid, gmDomain, seqs[3], false); err != nil || n != 4 {
		t.Fatalf("到 seqs[3] 为止 = %d (%v), want 4", n, err)
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
		}, nil) // 普通模式不读账本
		if err != nil {
			t.Fatal(err)
		}
		if resp.GetForceVerdict() != inboxpb.PlayerInboxForceVoidVerdict_PLAYER_INBOX_FORCE_VOID_UNSPECIFIED {
			t.Fatalf("普通模式不应带强制证明结论: %+v", resp)
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

// auditRow 读一条审计行;不存在时 ok=false。
func auditRow(t *testing.T, db *sql.DB, playerID, seq uint64, action inboxpb.PlayerInboxAuditAction) (operator, peer, reason string, ok bool) {
	t.Helper()
	err := db.QueryRowContext(itCtx(t), "SELECT `operator`,`peer_addr`,`reason` FROM player_inbox_audit"+
		" WHERE `shard` = ? AND `player_id` = ? AND `domain` = ? AND `seq` = ? AND `action` = ?",
		RowShard(playerID), playerID, uint32(gmDomain), seq, uint32(action)).Scan(&operator, &peer, &reason)
	if errors.Is(err, sql.ErrNoRows) {
		return "", "", "", false
	}
	if err != nil {
		t.Fatal(err)
	}
	return operator, peer, reason, true
}

// TestIntegrationForceVoid 钉住 VoidItem 强制模式在真库上的读写(设计 §7.9,评审 M1):
//   - 证明不成立的每种情形(从未投递 / 读不到存档 / 回放 / UNKNOWN / 水位没到)都不改行、不写审计,读账本失败回故障;
//   - 证明成立 → VOIDED(finalized_by=ADMIN、delivery_count 与先前挂的指令原样保留),open_count−1,
//     审计 action=FORCE_VOID(与先前挂指令时的 VOID 审计并存,不被 INSERT IGNORE 吞掉);
//   - 终结后 Fetch 不再投递它、对账不再报 PENDING_BUT_DECIDED;重复调用回 StateConflict,不再写审计。
func TestIntegrationForceVoid(t *testing.T) {
	e := openIntegration(t)
	const pid = uint64(5002)
	const ledgerRequestAtB = uint64(999) // 消费方在 b 这个 seq 上实际裁决的是另一条(库恢复后旧行与账本对不上)
	a := e.mustEnqueue(t, pid, 1).Seq    // 已执行并确认
	b := e.mustEnqueue(t, pid, 2).Seq    // scene 判 MISMATCH 的那一行
	c := e.mustEnqueue(t, pid, 3).Seq    // 从未投递
	e.epochs.set(pid, 2)
	e.fetch(t, pid, 2, 0, 2) // 只投递 a、b
	if ack := e.ack(t, pid, 2, dec(a, outApplied, 0, 1)); ack.GetResults()[0].GetState() != stateApplied {
		t.Fatalf("a 应终结为 APPLIED: %+v", ack.GetResults())
	}
	ctx := itCtx(t)
	const note = "工单 #42:库恢复后旧行,账本同 seq 为另一 request_id"
	force := func(seq uint64, ledgers LedgerReader) (*inboxpb.VoidPlayerInboxItemResponse, error) {
		return e.store.Void(ctx, VoidParams{
			PlayerID: pid, Domain: gmDomain, Seq: seq, Reason: inboxpb.PlayerInboxVoidReason_PLAYER_INBOX_VOID_QUARANTINED,
			Operator: "ops_force", Note: note, PeerAddr: "127.0.0.1:3",
			Mode: inboxpb.PlayerInboxVoidMode_PLAYER_INBOX_VOID_MODE_FORCE_MISMATCH,
		}, ledgers)
	}
	proving := itLedgers{found: true, ledger: fvLedger(fvCursor(gmDomain, b, fvDecision(a, 1), fvDecision(b, ledgerRequestAtB)))}

	// 终结之前,对账把 b 报成 PENDING_BUT_DECIDED(账本水位已过、收件箱仍 PENDING)。
	rec, err := e.store.ReconcilePlayer(ctx, &inboxpb.ReconcilePlayerInboxRequest{PlayerId: pid}, proving)
	if err != nil || len(rec.GetDiffs()) != 1 || rec.GetDiffs()[0].GetSeq() != b ||
		rec.GetDiffs()[0].GetKind() != inboxpb.PlayerInboxReconcileDiffKind_PLAYER_INBOX_RECONCILE_PENDING_BUT_DECIDED {
		t.Fatalf("终结前的对账: %+v %v", rec.GetDiffs(), err)
	}

	auditsBefore := auditCount(t, e.db, pid)
	refusals := []struct {
		name    string
		seq     uint64
		ledgers LedgerReader
		want    inboxpb.PlayerInboxForceVoidVerdict
	}{
		{"从未投递", c, proving, forceVoidNotDelivered},
		{"读不到存档", b, itLedgers{found: false}, forceVoidLedgerAbsent},
		{"回放:环里同 seq 同 request_id", b, itLedgers{found: true, ledger: fvLedger(fvCursor(gmDomain, b, fvDecision(a, 1), fvDecision(b, 2)))},
			forceVoidRequestInLedger},
		{"UNKNOWN:水位已过、环里没有 b", b, itLedgers{found: true, ledger: fvLedger(fvCursor(gmDomain, b, fvDecision(a, 1)))},
			forceVoidSeqNotInRing},
		{"水位没到 b", b, itLedgers{found: true, ledger: fvLedger(fvCursor(gmDomain, a, fvDecision(a, 1)))}, forceVoidNotDecided},
	}
	for _, r := range refusals {
		resp, err := force(r.seq, r.ledgers)
		if err != nil || resp.GetErrorMessage().GetId() != constants.ErrItemStateConflict || resp.GetForceVerdict() != r.want ||
			resp.GetState() != statePending || resp.GetVoidRequested() {
			t.Fatalf("%s: resp=%+v err=%v, want StateConflict + %v,行仍 PENDING", r.name, resp, err, r.want)
		}
		if it := e.item(t, pid, r.seq); it.GetState() != uint32(statePending) || it.GetFinalizedBy() != 0 {
			t.Fatalf("%s: 证明不成立不得改行: %+v", r.name, it)
		}
	}
	if _, err := force(b, itLedgers{err: errors.New("data_service down")}); err == nil {
		t.Fatal("读账本失败应回故障(适配层折成 StoreUnavailable)")
	}
	if st := e.stream(t, pid); st.GetOpenCount() != 2 {
		t.Fatalf("拒绝路径不得动 open_count: %d, want 2(b、c)", st.GetOpenCount())
	}
	if n := auditCount(t, e.db, pid); n != auditsBefore {
		t.Fatalf("拒绝路径不得写审计: %d, want %d", n, auditsBefore)
	}

	// 按 runbook 先对 b 挂过 VOID 指令(scene 照样判 MISMATCH),再强制终结。
	if resp, err := e.store.Void(ctx, VoidParams{
		PlayerID: pid, Domain: gmDomain, Seq: b, Reason: inboxpb.PlayerInboxVoidReason_PLAYER_INBOX_VOID_QUARANTINED,
		Operator: "ops_force", Note: note, PeerAddr: "127.0.0.1:3",
	}, nil); err != nil || !resp.GetVoidRequested() {
		t.Fatalf("普通模式挂指令: %+v %v", resp, err)
	}
	resp, err := force(b, proving)
	if err != nil || resp.GetErrorMessage() != nil || resp.GetState() != stateVoided || resp.GetVoidRequested() ||
		resp.GetForceVerdict() != forceVoidProven {
		t.Fatalf("证明成立应终结为 VOIDED: %+v %v", resp, err)
	}
	it := e.item(t, pid, b)
	if it.GetState() != uint32(stateVoided) ||
		it.GetFinalizedBy() != uint32(inboxpb.PlayerInboxFinalizer_PLAYER_INBOX_FINALIZER_ADMIN) ||
		it.GetVoidReason() != uint32(inboxpb.PlayerInboxVoidReason_PLAYER_INBOX_VOID_QUARANTINED) ||
		it.GetDeliveryCount() != 1 || it.GetDirective() != uint32(directiveVoid) || it.GetRequestId() != 2 {
		t.Fatalf("强制终结后的行: %+v", it)
	}
	if st := e.stream(t, pid); st.GetOpenCount() != 1 {
		t.Fatalf("open_count = %d, want 1(只剩 c)", st.GetOpenCount())
	}
	if op, peer, reason, ok := auditRow(t, e.db, pid, b, inboxpb.PlayerInboxAuditAction_PLAYER_INBOX_AUDIT_FORCE_VOID); !ok ||
		op != "ops_force" || peer != "127.0.0.1:3" || reason != note {
		t.Fatalf("FORCE_VOID 审计行: ok=%v operator=%q peer=%q reason=%q", ok, op, peer, reason)
	}
	if _, _, _, ok := auditRow(t, e.db, pid, b, inboxpb.PlayerInboxAuditAction_PLAYER_INBOX_AUDIT_VOID); !ok {
		t.Fatal("先前挂指令的 VOID 审计行应原样保留")
	}
	if n := auditCount(t, e.db, pid); n != auditsBefore+2 {
		t.Fatalf("审计行 = %d, want %d(挂指令 + 强制终结各一行)", n, auditsBefore+2)
	}

	// 终结后:Fetch 只投递 c;对账不再报 b(管理员终结的行不比对)。
	fetched := e.fetch(t, pid, 2, b, 0)
	if len(fetched.GetItems()) != 1 || fetched.GetItems()[0].GetSeq() != c {
		t.Fatalf("强制终结后不应再投递 b: %+v", fetched.GetItems())
	}
	rec, err = e.store.ReconcilePlayer(ctx, &inboxpb.ReconcilePlayerInboxRequest{PlayerId: pid}, proving)
	if err != nil || len(rec.GetDiffs()) != 0 {
		t.Fatalf("终结后的对账: %+v %v", rec.GetDiffs(), err)
	}

	// 重复调用:终态 → StateConflict,不看账本、不再写审计;不存在 → NotFound。
	if resp, err := force(b, proving); err != nil || resp.GetErrorMessage().GetId() != constants.ErrItemStateConflict ||
		resp.GetState() != stateVoided || resp.GetForceVerdict() != forceVoidUnspecified {
		t.Fatalf("重复强制终结: %+v %v", resp, err)
	}
	if resp, err := force(b+99999, proving); err != nil || resp.GetErrorMessage().GetId() != constants.ErrItemNotFound {
		t.Fatalf("不存在: %+v %v", resp, err)
	}
	if n := auditCount(t, e.db, pid); n != auditsBefore+2 {
		t.Fatalf("重复调用不得再写审计: %d", n)
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

// TestIntegrationScanGauges 钉住扫描型 gauge 的 SQL 一侧(设计 §9.4;跨分片折叠另见 TestFoldScanGauges):
// 只数 PENDING 行;已投递未确认按最后投递时刻算最老;提醒积压只数 notify_due_ms 早于给定时刻的流。
func TestIntegrationScanGauges(t *testing.T) {
	e := openIntegration(t)
	// 玩家 A:两条 PENDING,投递其中一条两次后不确认。
	const pa, pb = uint64(9001), uint64(9066) // 9001 % 64 = 41,9066 % 64 = 42:两个不同分片,覆盖跨分片折叠
	e.mustEnqueue(t, pa, 1)
	e.mustEnqueue(t, pa, 2)
	e.epochs.set(pa, 1)
	e.clock.advance(1_000)
	e.fetch(t, pa, 1, 0, 1) // 第一条 delivery_count=1,last_delivered=T0+1s
	e.clock.advance(1_000)
	e.fetch(t, pa, 1, 0, 1) // 第一条 delivery_count=2,last_delivered=T0+2s
	// 玩家 B(另一分片):一条已确认(终态,不计入),一条 PENDING 从未投递;入箱后提醒失败,留下 notify_due_ms。
	e.clock.advance(1_000)
	b1 := e.mustEnqueue(t, pb, 1).Seq
	e.epochs.set(pb, 1)
	e.fetch(t, pb, 1, 0, 0)
	e.ack(t, pb, 1, dec(b1, outApplied, 0, 1))
	e.notifier.setResult(NotifyError)
	e.mustEnqueue(t, pb, 2) // created_ms = T0+3s,notify_due_ms = T0+3s

	now := testNowMs + 10_000
	g, err := e.store.ScanGauges(itCtx(t), now, testNowMs+3_000)
	if err != nil {
		t.Fatal(err)
	}
	want := metrics.ScanGauges{
		OpenItems:                     3, // A 两条 + B 一条 PENDING
		DeliveredUnacked:              1, // 只有 A 的第一条投递过
		MaxDeliveryCount:              2,
		OldestOpenAgeSeconds:          10, // A 的两条 created_ms = T0
		DeliveredUnackedOldestSeconds: 8,  // 最后投递 T0+2s
		NotifyBacklog:                 1,  // B 的流 notify_due_ms = T0+3s ≤ 截止
	}
	if g != want {
		t.Fatalf("ScanGauges = %+v, want %+v", g, want)
	}
	if g, err := e.store.ScanGauges(itCtx(t), now, testNowMs+2_999); err != nil || g.NotifyBacklog != 0 {
		t.Fatalf("截止早于 notify_due_ms 时不算积压: %+v %v", g, err)
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
	err    error
}

func (l itLedgers) ReadInboxLedger(context.Context, uint64) (*componentpb.PlayerInboxLedgerComp, bool, error) {
	if l.err != nil {
		return nil, false, l.err
	}
	return l.ledger, l.found, nil
}

// TestIntegrationReconcile 钉住两个对账入口的读库一侧(判定谓词另见 TestFinalizedLedgerGap)。
// 收件箱:a、b 已 APPLIED,c 已投递未确认。账本一律是真实形状(水位 = 环尾,rcCursor):
//   - b 的落盘丢了:环 [a]、水位 a → b 报 FINAL_NOT_IN_LEDGER;
//   - c 已执行、确认丢了:环 [a, b, c] → c 报 PENDING_BUT_DECIDED;
//   - b 的落盘丢了、之后 c 又执行(评审 M6):环 [a, c]、水位 c —— 只比水位会被掩盖,b 照样报 FINAL_NOT_IN_LEDGER;
//   - 账本结构不合法(环超过 32 条)→ ReconcilePlayer 回 error,定时对账计 invalid,都不是「无差异」。
func TestIntegrationReconcile(t *testing.T) {
	e := openIntegration(t)
	const pid = uint64(8001)
	a := e.mustEnqueue(t, pid, 1).Seq
	b := e.mustEnqueue(t, pid, 2).Seq
	e.epochs.set(pid, 1)
	e.fetch(t, pid, 1, 0, 0)
	e.ack(t, pid, 1, dec(a, outApplied, 0, 1), dec(b, outApplied, 0, 2))
	c := e.mustEnqueue(t, pid, 3).Seq
	e.fetch(t, pid, 1, b, 0) // 投递 c,不确认
	ctx := itCtx(t)

	da, db, dc := rcDecision(a, 1, outApplied, 0), rcDecision(b, 2, outApplied, 0), rcDecision(c, 3, outApplied, 0)
	lostB := rcLedger(rcCursor(gmDomain, da))
	ackLost := rcLedger(rcCursor(gmDomain, da, db, dc))
	masked := rcLedger(rcCursor(gmDomain, da, dc))
	broken := rcLedger(rcRing(1, ledgerRecentCapacity+1, 0))

	final := inboxpb.PlayerInboxReconcileDiffKind_PLAYER_INBOX_RECONCILE_FINAL_NOT_IN_LEDGER
	decided := inboxpb.PlayerInboxReconcileDiffKind_PLAYER_INBOX_RECONCILE_PENDING_BUT_DECIDED
	req := &inboxpb.ReconcilePlayerInboxRequest{PlayerId: pid}
	diffsOf := func(name string, ledger *componentpb.PlayerInboxLedgerComp) []*inboxpb.PlayerInboxReconcileDiff {
		t.Helper()
		resp, err := e.store.ReconcilePlayer(ctx, req, itLedgers{ledger: ledger, found: true})
		if err != nil || !resp.GetLedgerFound() {
			t.Fatalf("%s: ReconcilePlayer: %+v %v", name, resp, err)
		}
		return resp.GetDiffs()
	}
	if d := diffsOf("b 的落盘丢了", lostB); len(d) != 1 || d[0].GetSeq() != b || d[0].GetKind() != final {
		t.Fatalf("b 的落盘丢了: diffs = %+v", d)
	}
	if d := diffsOf("确认丢了", ackLost); len(d) != 1 || d[0].GetSeq() != c || d[0].GetKind() != decided {
		t.Fatalf("确认丢了: diffs = %+v", d)
	}
	if d := diffsOf("被 c 掩盖", masked); len(d) != 2 || d[0].GetSeq() != b || d[0].GetKind() != final ||
		d[1].GetSeq() != c || d[1].GetKind() != decided {
		t.Fatalf("被 c 掩盖: diffs = %+v, want b FINAL_NOT_IN_LEDGER + c PENDING_BUT_DECIDED", d)
	}
	if resp, err := e.store.ReconcilePlayer(ctx, req, itLedgers{ledger: broken, found: true}); err == nil {
		t.Fatalf("账本不合法应回 error(适配层折成 StoreUnavailable),不能回「无差异」: %+v", resp)
	}

	// 定时对账:同一个谓词,只核对消费方终结的 a、b。
	start := RowShard(pid)
	recent := func(name string, l LedgerReader, wantInconsistent int) {
		t.Helper()
		res, err := e.store.ReconcileRecent(ctx, 0, start, l)
		if err != nil || res.Inconsistent != wantInconsistent || res.PlayersPlanned != 1 || res.PlayersChecked != 1 || res.Truncated {
			t.Fatalf("%s: ReconcileRecent = %+v err=%v, want %d 处不一致、1 名玩家、未截断", name, res, err, wantInconsistent)
		}
	}
	recent("b 的落盘丢了", itLedgers{ledger: lostB, found: true}, 1)
	recent("被 c 掩盖", itLedgers{ledger: masked, found: true}, 1)
	recent("a、b 都在环里", itLedgers{ledger: ackLost, found: true}, 0)
	recent("读不到存档计 unknown", itLedgers{found: false}, 0)
	recent("账本不合法计 invalid", itLedgers{ledger: broken, found: true}, 0)
	recent("读失败计 error", itLedgers{err: errors.New("data_service down")}, 0)
}

// TestIntegrationRecentlyFinalizedOrder 钉住定时对账读终结行的 SQL(评审 M5):
//   - 按 (updated_ms, player_id, domain, seq) 降序。回归:原 SQL 没有 ORDER BY,LIMIT 截下的是索引
//     (shard, state, updated_ms) 上 state 最小的 APPLIED,更新的 REJECTED 被挤掉;
//   - 键集游标逐页不重不漏;sinceMs 之前的行不读;管理员终结的行不读;对账要的列都读出来。
func TestIntegrationRecentlyFinalizedOrder(t *testing.T) {
	e := openIntegration(t)
	const pid = uint64(8065) // 8065 % 64 = 1
	a := e.mustEnqueue(t, pid, 1).Seq
	b := e.mustEnqueue(t, pid, 2).Seq
	c := e.mustEnqueue(t, pid, 3).Seq
	d := e.mustEnqueue(t, pid, 4).Seq // 从未投递,管理员直接作废(finalized_by=ADMIN)
	e.epochs.set(pid, 1)
	e.fetch(t, pid, 1, 0, 3)                   // 投递 a、b、c
	e.ack(t, pid, 1, dec(a, outApplied, 0, 1)) // a:updated_ms = T0
	e.clock.advance(1_000)
	e.ack(t, pid, 1, dec(b, outRejected, 7, 2), dec(c, outApplied, 0, 3)) // b、c:updated_ms = T0+1s
	ctx := itCtx(t)
	if resp, err := e.store.Void(ctx, VoidParams{
		PlayerID: pid, Domain: gmDomain, Seq: d, Reason: inboxpb.PlayerInboxVoidReason_PLAYER_INBOX_VOID_QUARANTINED,
		Operator: "ops_it", Note: "it", PeerAddr: "127.0.0.1:1",
	}, nil); err != nil || resp.GetState() != stateVoided {
		t.Fatalf("直接作废 d: %+v %v", resp, err)
	}
	shard := RowShard(pid)
	page := func(sinceMs uint64, before *FinalizedRef, limit int) []FinalizedRef {
		t.Helper()
		refs, err := e.store.RecentlyFinalized(ctx, shard, sinceMs, before, limit)
		if err != nil {
			t.Fatal(err)
		}
		return refs
	}
	seqs := func(refs []FinalizedRef) []uint64 {
		out := make([]uint64, 0, len(refs))
		for _, f := range refs {
			out = append(out, f.Seq)
		}
		return out
	}

	all := page(0, nil, 10)
	if got := seqs(all); len(got) != 3 || got[0] != c || got[1] != b || got[2] != a {
		t.Fatalf("降序 = %v, want [c b a](同一毫秒按 seq 降序;管理员作废的 d 不读)", got)
	}
	if rb := all[1]; rb.State != stateRejected || rb.OutcomeReason != 7 || rb.RequestID != 2 || rb.MsgType != gmBlock ||
		rb.FinalizedBy != byConsumer || rb.UpdatedMs != testNowMs+1_000 || rb.PlayerID != pid || rb.Domain != gmDomain {
		t.Fatalf("b 的列: %+v", rb)
	}
	if got := seqs(page(0, nil, 2)); len(got) != 2 || got[0] != c || got[1] != b {
		t.Fatalf("LIMIT 2 = %v, want [c b]:更新的 REJECTED 不得被更旧的 APPLIED 挤掉", got)
	}
	// 逐页翻:每页 1 行,游标是上一页最后一行。
	var walked []uint64
	var before *FinalizedRef
	for i := 0; i < 5; i++ {
		p := page(0, before, 1)
		if len(p) == 0 {
			break
		}
		walked = append(walked, p[0].Seq)
		before = &p[0]
	}
	if len(walked) != 3 || walked[0] != c || walked[1] != b || walked[2] != a {
		t.Fatalf("键集翻页 = %v, want [c b a]", walked)
	}
	if got := seqs(page(testNowMs+1, nil, 10)); len(got) != 2 || got[0] != c || got[1] != b {
		t.Fatalf("sinceMs 之后 = %v, want [c b]", got)
	}
}
