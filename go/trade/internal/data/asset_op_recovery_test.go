package data

// asset_op_recovery_test.go —— 落地恢复(docs/design/landing-recovery-sync-digest.md §5.3 / §6)在 trade outbox 上的回归,不连库。
//
// 被测对象是 AssetOpRepo 的三个可选能力(PlayerPendingLister / PendingClassCounter / Abandoner)、
// last_send_ms 契约(go/shared/assetop/abandon.go 文件头)的五处 SQL 写法,以及 ABANDONED 的状态映射与终态清理口径。
// 真库行为(InnoDB / TiDB 上的 CAS 与锁)不在这里;这里钉的是"发了什么语句、带了什么条件、
// 结果怎么解释"这些纯 SQL 层的事实 —— 它们写错了在编译期与单元测试之外都不会报错,只会让放弃"永远不发生"或"不该发生时发生"。

import (
	"context"
	"database/sql"
	"database/sql/driver"
	"errors"
	"fmt"
	"reflect"
	"strings"
	"sync"
	"testing"
	"time"

	assetpb "proto/common/asset"
	tradepb "proto/trade"

	"shared/assetop"

	"google.golang.org/protobuf/reflect/protoreflect"
)

// ── 录制驱动(只录语句与实参,按配置回结果)──────────────────────

// recovCall 是录音带上的一条:语句(已折叠空白)与驱动层拿到的实参。BEGIN / COMMIT / ROLLBACK 以伪语句入带。
type recovCall struct {
	sql  string
	args []driver.Value
}

// recovScript 是一次用例的脚本:每条写语句回 affected,每条查询由 rowsFor 给结果集(nil 或回 nil 列 = 空结果)。
type recovScript struct {
	mu       sync.Mutex
	calls    []recovCall
	affected int64
	rowsFor  func(query string, args []driver.Value) (cols []string, data [][]driver.Value)
}

func (s *recovScript) record(stmt string, args []driver.NamedValue) []driver.Value {
	vals := make([]driver.Value, 0, len(args))
	for _, a := range args {
		vals = append(vals, a.Value)
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	s.calls = append(s.calls, recovCall{sql: collapseSpaces(stmt), args: vals})
	return vals
}

// recordedCalls 返回录下的全部调用(拷贝)。
func (s *recovScript) recordedCalls() []recovCall {
	s.mu.Lock()
	defer s.mu.Unlock()
	return append([]recovCall(nil), s.calls...)
}

// callsWithPrefix 返回以 prefix 打头的调用,保持发出顺序。
func (s *recovScript) callsWithPrefix(prefix string) []recovCall {
	var out []recovCall
	for _, c := range s.recordedCalls() {
		if strings.HasPrefix(c.sql, prefix) {
			out = append(out, c)
		}
	}
	return out
}

func (s *recovScript) repo(t *testing.T) *AssetOpRepo {
	t.Helper()
	db := sql.OpenDB(recovConnector{s: s})
	t.Cleanup(func() { _ = db.Close() })
	repo, err := NewAssetOpRepo(db, time.Second)
	if err != nil {
		t.Fatalf("NewAssetOpRepo: %v", err)
	}
	return repo
}

type recovConnector struct{ s *recovScript }

func (c recovConnector) Connect(context.Context) (driver.Conn, error) {
	return &recovConn{s: c.s}, nil
}

func (c recovConnector) Driver() driver.Driver { return recovDriver{s: c.s} }

type recovDriver struct{ s *recovScript }

func (d recovDriver) Open(string) (driver.Conn, error) { return &recovConn{s: d.s}, nil }

type recovConn struct{ s *recovScript }

func (c *recovConn) Prepare(string) (driver.Stmt, error) {
	return nil, errors.New("录制驱动不支持 Prepare:被测路径全部走 QueryContext / ExecContext")
}
func (c *recovConn) Close() error { return nil }
func (c *recovConn) Begin() (driver.Tx, error) {
	return nil, errors.New("录制驱动只支持 BeginTx")
}

func (c *recovConn) BeginTx(context.Context, driver.TxOptions) (driver.Tx, error) {
	c.s.record("BEGIN", nil)
	return recovTx{s: c.s}, nil
}

func (c *recovConn) QueryContext(_ context.Context, query string, args []driver.NamedValue) (driver.Rows, error) {
	vals := c.s.record(query, args)
	if c.s.rowsFor == nil {
		return &seqFakeRows{cols: []string{"op_id"}}, nil
	}
	cols, data := c.s.rowsFor(collapseSpaces(query), vals)
	if cols == nil {
		cols = []string{"op_id"}
	}
	return &seqFakeRows{cols: cols, data: data}, nil
}

func (c *recovConn) ExecContext(_ context.Context, query string, args []driver.NamedValue) (driver.Result, error) {
	c.s.record(query, args)
	c.s.mu.Lock()
	defer c.s.mu.Unlock()
	return driver.RowsAffected(c.s.affected), nil
}

type recovTx struct{ s *recovScript }

func (t recovTx) Commit() error {
	t.s.record("COMMIT", nil)
	return nil
}

func (t recovTx) Rollback() error {
	t.s.record("ROLLBACK", nil)
	return nil
}

// argInt 把驱动层实参解成 uint64(database/sql 把各宽度整数统一转成 int64)。
func argInt(t *testing.T, v driver.Value) uint64 {
	t.Helper()
	n, ok := v.(int64)
	if !ok {
		t.Fatalf("实参不是整数: %#v", v)
	}
	return uint64(n)
}

// ── 状态映射与列 ────────────────────────────────────────────

// TestStatusToRecordIsABijection:assetop.Status(语义)与 TradeAssetOpStatus(库值)必须一一对应 ——
// 每个合法语义值落成一个互不相同、非 UNSPECIFIED 的库值,每个非 UNSPECIFIED 的库值恰好被一个语义值命中。
// 两边任一边加了状态却没同改映射(这次的 ABANDONED 就是例子),放弃出来的行会落成 UNSPECIFIED、客服查不到结局。
func TestStatusToRecordIsABijection(t *testing.T) {
	seen := map[tradepb.TradeAssetOpStatus]assetop.Status{}
	for s := assetop.StatusPending; s <= assetop.StatusAbandoned; s++ {
		rec := StatusToRecord(s)
		if rec == tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_UNSPECIFIED {
			t.Errorf("语义值 %s 映射成 UNSPECIFIED", s)
			continue
		}
		if prev, dup := seen[rec]; dup {
			t.Errorf("语义值 %s 与 %s 映射到同一个库值 %v", s, prev, rec)
		}
		seen[rec] = s
	}
	values := tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_UNSPECIFIED.Descriptor().Values()
	for i := 0; i < values.Len(); i++ {
		rec := tradepb.TradeAssetOpStatus(values.Get(i).Number())
		if rec == tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_UNSPECIFIED {
			continue
		}
		if _, ok := seen[rec]; !ok {
			t.Errorf("库值 %v 没有任何语义值映射过来:加了库内状态却没同改 StatusToRecord", rec)
		}
	}
	if got := StatusToRecord(assetop.StatusAbandoned); got != tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_ABANDONED {
		t.Errorf("StatusToRecord(Abandoned) = %v, want ABANDONED", got)
	}
}

// TestAssetOpHasLastSendMsColumn:last_send_ms 是自动放弃条件 4 的唯一依据。proto 缺字段或手写列清单漏了它,
// Claim 回读的 LastSendMs 恒为 0,放弃路径一律按"Store 没维护"跳过 —— 不出错,只是放弃永远不发生。
func TestAssetOpHasLastSendMsColumn(t *testing.T) {
	field := (&tradepb.TradeAssetOpRecord{}).ProtoReflect().Descriptor().Fields().ByName(protoreflect.Name("last_send_ms"))
	if field == nil {
		t.Fatal("TradeAssetOpRecord 缺 last_send_ms")
	}
	if field.Number() != 24 {
		t.Errorf("last_send_ms 字段号 = %d, want 24(字段号上线后不复用)", field.Number())
	}
	if !strings.Contains(assetOpColumns, "`last_send_ms`") {
		t.Error("assetOpColumns 缺 last_send_ms:插行与回读都碰不到这一列")
	}
}

// TestInsertLastSendMsNeverBelowCreated:LastSendMs 契约第 1 条 —— 插行写的值不小于 created_ms
// (插行即持租约,提交后的同步投递紧接着就可能发出);生产者给了更晚的值照用(只许偏大)。
func TestInsertLastSendMsNeverBelowCreated(t *testing.T) {
	cases := []struct {
		name              string
		created, lastSend uint64
		want              uint64
	}{
		{"生产者漏填", 1000, 0, 1000},
		{"与创建同刻", 1000, 1000, 1000},
		{"生产者给了更晚的时刻", 1000, 1500, 1500},
		{"生产者给了更早的时刻(不许偏小)", 1000, 900, 1000},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			rec := &tradepb.TradeAssetOpRecord{CreatedMs: tc.created, LastSendMs: tc.lastSend}
			if got := insertLastSendMs(rec); got != tc.want {
				t.Errorf("insertLastSendMs = %d, want %d", got, tc.want)
			}
		})
	}
}

// TestFinalizeAndResolveRefuseAbandoned:ABANDONED 只能经 Abandon(带令牌、六条证据齐全)写入。
// Finalize / ResolveManually 必须在碰事务之前拒掉它 —— 本用例的仓库没有连接池,越过这一步会当场失败。
func TestFinalizeAndResolveRefuseAbandoned(t *testing.T) {
	repo, err := NewAssetOpRepo(nil, time.Second)
	if err != nil {
		t.Fatalf("NewAssetOpRepo: %v", err)
	}
	if done, err := repo.Finalize(context.Background(), assetop.Op{OpID: 1}, assetop.StatusAbandoned, assetop.Result{}, 1); err == nil || done {
		t.Errorf("Finalize(Abandoned) = (%v, %v),必须拒绝", done, err)
	}
	if done, err := repo.ResolveManually(context.Background(),
		assetop.ManualResolution{OpID: 1, Final: assetop.StatusAbandoned, Operator: "ops", Reason: "test"}, 1); err == nil || done {
		t.Errorf("ResolveManually(Abandoned) = (%v, %v),必须拒绝(人工判定放弃应按证据选 ABORTED)", done, err)
	}
}

// ── last_send_ms 契约的五处 SQL ──────────────────────────────

// TestClaimSQLFoldsOldLeaseBeforeOverwritingIt:契约第 2、5 条。last_send_ms 的赋值必须写在改写 lease_until_ms
// 与 updated_ms **之前**:MySQL 单表 UPDATE 从左到右求值,写在后面读到的就是本次新写的租约 / 时刻,
// 上一个没收尾的租约(发完请求就崩溃的那一次)就被盖掉了,放弃会撞上仍可能在途的请求。
func TestClaimSQLFoldsOldLeaseBeforeOverwritingIt(t *testing.T) {
	stmt := collapseSpaces(sqlClaimAssetOp)
	assign := strings.Index(stmt, "`last_send_ms` =")
	lease := strings.Index(stmt, "`lease_until_ms` = ?")
	updated := strings.Index(stmt, "`updated_ms` = ?")
	if assign < 0 || lease < 0 || updated < 0 {
		t.Fatalf("领取语句缺少必要的赋值: %s", stmt)
	}
	if assign > lease || assign > updated {
		t.Errorf("last_send_ms 必须先于 lease_until_ms / updated_ms 赋值: %s", stmt)
	}
	for _, want := range []string{
		// 存量行懒初始化(契约第 5 条):0 时取 GREATEST(created_ms, updated_ms)。
		"IF(`last_send_ms` = 0, GREATEST(`created_ms`, `updated_ms`), `last_send_ms`)",
		// 折旧租约(契约第 2 条)。
		", `lease_until_ms`),",
		// 领取条件不变:只看 status 与租约。
		"WHERE `op_id` = ? AND `status` = ? AND `lease_until_ms` < ?",
	} {
		if !strings.Contains(stmt, want) {
			t.Errorf("领取语句缺 %q: %s", want, stmt)
		}
	}
}

// TestRescheduleSQLAdvancesOnlyWhenSent:契约第 3 条。本地 NOT_HERE(res.Local)根本没发出请求,推进它会让
// 停放中的离线行永远"刚发过",放弃永远不会发生;非 Local 一律按"发过"推进到 nowMs。
func TestRescheduleSQLAdvancesOnlyWhenSent(t *testing.T) {
	stmt := collapseSpaces(sqlRescheduleAssetOp)
	if !strings.Contains(stmt, "`last_send_ms` = IF(?, GREATEST(`last_send_ms`, ?), `last_send_ms`)") {
		t.Errorf("重排语句的 last_send_ms 写法不符契约第 3 条: %s", stmt)
	}
	if !strings.Contains(stmt, "WHERE `op_id` = ? AND `status` = ? AND `lease_token` = ?") {
		t.Errorf("重排语句必须带令牌与 PENDING 条件: %s", stmt)
	}
}

// TestPoisonAndReleaseLeaveLastSendMsAlone:契约第 4 条。毒行推迟与归还租约只清租约;
// 推进 last_send_ms 会让放弃方自己的领取 / 归还把"最后一次可能的发送"不断刷新,放弃永远不会发生。
func TestPoisonAndReleaseLeaveLastSendMsAlone(t *testing.T) {
	for name, stmt := range map[string]string{"毒行推迟": sqlPoisonAssetOp, "归还租约": sqlReleaseAssetOpLease} {
		s := collapseSpaces(stmt)
		if strings.Contains(s, "last_send_ms") {
			t.Errorf("%s 语句不得改 last_send_ms: %s", name, s)
		}
		if !strings.Contains(s, "`lease_until_ms` = 0") {
			t.Errorf("%s 语句必须清租约: %s", name, s)
		}
		if !strings.Contains(s, "`lease_token` = ?") || !strings.Contains(s, "`status` = ?") {
			t.Errorf("%s 语句必须以令牌与 PENDING 为条件: %s", name, s)
		}
	}
	release := collapseSpaces(sqlReleaseAssetOpLease)
	for _, col := range []string{"`attempts`", "`next_attempt_ms`"} {
		if strings.Contains(release, col) {
			t.Errorf("归还租约不得改 %s(Abandoner.ReleaseLease 契约): %s", col, release)
		}
	}
}

// TestClaimReadsBackCreatedAndLastSendMs:领取成功后回读的 Op 必须带上 created_ms 与 last_send_ms ——
// 自动放弃只认这两个回读值(0 一律 fail-closed 跳过)。漏填的表现是放弃永远不发生,且没有任何报错。
func TestClaimReadsBackCreatedAndLastSendMs(t *testing.T) {
	const (
		opID       = 77
		createdMs  = 1_000
		lastSendMs = 5_000
		token      = 99
	)
	s := &recovScript{affected: 1}
	s.rowsFor = func(query string, _ []driver.Value) ([]string, [][]driver.Value) {
		if !strings.HasPrefix(query, "SELECT `op_id`, `player_id`") {
			return nil, nil
		}
		cols := splitColumns(assetOpColumns)
		row := make([]driver.Value, len(cols))
		for i, col := range cols {
			switch col {
			case "op_id":
				row[i] = int64(opID)
			case "player_id":
				row[i] = int64(42)
			case "stream":
				row[i] = int64(assetpb.AssetOpStream_ASSET_OP_STREAM_TRADE_DEBIT)
			case "status":
				row[i] = int64(tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_PENDING)
			case "created_ms":
				row[i] = int64(createdMs)
			case "last_send_ms":
				row[i] = int64(lastSendMs)
			case "payload":
				row[i] = nil
			case "resolved_by", "resolve_reason":
				row[i] = ""
			default:
				row[i] = int64(0)
			}
		}
		return cols, [][]driver.Value{row}
	}
	repo := s.repo(t)

	op, ok, err := repo.Claim(context.Background(), opID, 2_000, 12_000, 3_602_000, token)
	if err != nil || !ok {
		t.Fatalf("Claim = (%v, %v)", ok, err)
	}
	if op.CreatedMs != createdMs || op.LastSendMs != lastSendMs {
		t.Errorf("回读 CreatedMs=%d LastSendMs=%d, want %d / %d", op.CreatedMs, op.LastSendMs, createdMs, lastSendMs)
	}
	if op.LeaseToken != token {
		t.Errorf("回读 LeaseToken=%d, want %d(Reschedule / Abandon / ReleaseLease 都要带回它)", op.LeaseToken, token)
	}
	claims := s.callsWithPrefix("UPDATE " + AssetOpTableName + " SET `last_send_ms`")
	if len(claims) != 1 {
		t.Fatalf("应发出恰一条领取 CAS,录到 %d 条: %+v", len(claims), s.recordedCalls())
	}
}

// TestRescheduleBindsNotLocal:重排的 IF(?, …) 条件位绑定的必须是 !res.Local。绑反了,
// 真发过请求的行不推进(放弃可能撞上在途请求),本地 NOT_HERE 的行反而推进(放弃永远不发生)。
func TestRescheduleBindsNotLocal(t *testing.T) {
	for _, local := range []bool{true, false} {
		t.Run(fmt.Sprintf("local=%v", local), func(t *testing.T) {
			s := &recovScript{affected: 1}
			repo := s.repo(t)
			op := assetop.Op{OpID: 5, LeaseToken: 9}
			if err := repo.Reschedule(context.Background(), op, 60_000, assetop.Result{Local: local}, 1_234); err != nil {
				t.Fatalf("Reschedule: %v", err)
			}
			calls := s.callsWithPrefix("UPDATE " + AssetOpTableName + " SET `attempts`")
			if len(calls) != 1 {
				t.Fatalf("应发出恰一条重排语句,录到 %+v", s.recordedCalls())
			}
			args := calls[0].args
			// 实参顺序:next, durable, outcome, reason, updated_ms, sent, now, op_id, status, token。
			if len(args) != 10 {
				t.Fatalf("重排语句实参数 = %d, want 10", len(args))
			}
			sent, ok := args[5].(bool)
			if !ok {
				t.Fatalf("第 6 个实参应是布尔(!res.Local),得到 %#v", args[5])
			}
			if sent != !local {
				t.Errorf("sent = %v, want %v(= !res.Local)", sent, !local)
			}
			if got := argInt(t, args[6]); got != 1_234 {
				t.Errorf("推进到的时刻 = %d, want nowMs 1234", got)
			}
		})
	}
}

// ── Abandoner ───────────────────────────────────────────────

// TestAbandonSQLShape:放弃 CAS 以 (op_id, status=PENDING, lease_token) 为条件;不置 durable(不是 scene 确认的结局)、
// 不改 last_outcome(保留最后一次真实答复作证据)、不动 last_send_ms;next_attempt_ms 同写终结时刻(清理按它判龄)。
func TestAbandonSQLShape(t *testing.T) {
	stmt := collapseSpaces(sqlAbandonAssetOp)
	if !strings.Contains(stmt, "WHERE `op_id` = ? AND `status` = ? AND `lease_token` = ?") {
		t.Errorf("放弃 CAS 缺令牌或 PENDING 条件: %s", stmt)
	}
	if !strings.Contains(stmt, "`next_attempt_ms` = ?") || !strings.Contains(stmt, "`lease_until_ms` = 0") {
		t.Errorf("放弃必须清租约并把 next_attempt_ms 写成终结时刻: %s", stmt)
	}
	for _, col := range []string{"`durable`", "`last_outcome`", "`last_send_ms`", "`resolved_by`"} {
		if strings.Contains(stmt, col) {
			t.Errorf("放弃语句不得改 %s: %s", col, stmt)
		}
	}
}

// TestAbandonRefusesZeroToken:令牌 0 是"没有租约"的形态,拿它做 CAS 证明不了"这一刻租约在我手里",而放弃不可逆。
// 必须在碰库之前拒掉(本用例没有连接池)。
func TestAbandonRefusesZeroToken(t *testing.T) {
	repo, err := NewAssetOpRepo(nil, time.Second)
	if err != nil {
		t.Fatalf("NewAssetOpRepo: %v", err)
	}
	if err := repo.Abandon(context.Background(), assetop.Op{OpID: 1}, 1); err == nil {
		t.Fatal("令牌为 0 时必须拒绝放弃")
	}
}

// TestAbandonCommitsWithTokenAndReportsLeaseLost:RowsAffected==1 → 在一个事务里提交;
// RowsAffected==0 → 必须回 ErrLeaseLost(租约被接管,别人可能已经发出请求),并回滚。
func TestAbandonCommitsWithTokenAndReportsLeaseLost(t *testing.T) {
	op := assetop.Op{OpID: 11, LeaseToken: 22}

	t.Run("提交", func(t *testing.T) {
		s := &recovScript{affected: 1}
		repo := s.repo(t)
		if err := repo.Abandon(context.Background(), op, 3_000); err != nil {
			t.Fatalf("Abandon: %v", err)
		}
		calls := s.recordedCalls()
		if len(calls) != 3 || calls[0].sql != "BEGIN" || calls[2].sql != "COMMIT" {
			t.Fatalf("放弃应是 BEGIN → CAS → COMMIT,录到 %+v", calls)
		}
		args := calls[1].args
		// 实参顺序:status, next_attempt_ms, updated_ms, op_id, PENDING, token。
		if len(args) != 6 {
			t.Fatalf("放弃 CAS 实参数 = %d, want 6", len(args))
		}
		if got := argInt(t, args[0]); got != uint64(tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_ABANDONED) {
			t.Errorf("写入状态 = %d, want ABANDONED", got)
		}
		if got := argInt(t, args[1]); got != 3_000 {
			t.Errorf("next_attempt_ms = %d, want 终结时刻 3000", got)
		}
		if argInt(t, args[3]) != op.OpID || argInt(t, args[4]) != uint64(PendingStatus()) || argInt(t, args[5]) != op.LeaseToken {
			t.Errorf("CAS 条件实参 = %v, want (op_id=%d, PENDING, token=%d)", args[3:], op.OpID, op.LeaseToken)
		}
	})

	t.Run("租约丢失", func(t *testing.T) {
		s := &recovScript{affected: 0}
		repo := s.repo(t)
		err := repo.Abandon(context.Background(), op, 3_000)
		if !errors.Is(err, assetop.ErrLeaseLost) {
			t.Fatalf("RowsAffected==0 时 err = %v, want errors.Is(..., ErrLeaseLost)", err)
		}
		if len(s.callsWithPrefix("COMMIT")) != 0 {
			t.Error("租约丢失时不得提交")
		}
	})
}

// TestReleaseLeaseReportsLeaseLost:归还租约落空(行已被接管或已终结)必须回 ErrLeaseLost,成功回 nil。
func TestReleaseLeaseReportsLeaseLost(t *testing.T) {
	op := assetop.Op{OpID: 3, LeaseToken: 4}
	for _, tc := range []struct {
		affected int64
		wantLost bool
	}{{1, false}, {0, true}} {
		s := &recovScript{affected: tc.affected}
		repo := s.repo(t)
		err := repo.ReleaseLease(context.Background(), op, 1)
		if got := errors.Is(err, assetop.ErrLeaseLost); got != tc.wantLost {
			t.Errorf("affected=%d: err = %v, want ErrLeaseLost=%v", tc.affected, err, tc.wantLost)
		}
		if !tc.wantLost && err != nil {
			t.Errorf("affected=1: err = %v, want nil", err)
		}
		calls := s.callsWithPrefix("UPDATE " + AssetOpTableName + " SET `lease_until_ms` = 0")
		if len(calls) != 1 {
			t.Fatalf("应发出恰一条归还语句,录到 %+v", s.recordedCalls())
		}
		// 实参顺序:updated_ms, op_id, PENDING, token。
		if args := calls[0].args; argInt(t, args[1]) != op.OpID || argInt(t, args[3]) != op.LeaseToken {
			t.Errorf("归还条件实参 = %v, want op_id=%d token=%d", args, op.OpID, op.LeaseToken)
		}
	}
}

// TestListAbandonCandidatesArgs:候选读是非锁读、按 op_id 翻页;limit ≤ 0 不发 SQL。
func TestListAbandonCandidatesArgs(t *testing.T) {
	s := &recovScript{}
	s.rowsFor = func(string, []driver.Value) ([]string, [][]driver.Value) {
		return []string{"op_id"}, [][]driver.Value{{int64(8)}, {int64(9)}}
	}
	repo := s.repo(t)
	if ids, err := repo.ListAbandonCandidates(context.Background(), 100, 7, 0); err != nil || ids != nil {
		t.Errorf("limit=0 = (%v, %v), want (nil, nil)", ids, err)
	}
	if n := len(s.recordedCalls()); n != 0 {
		t.Fatalf("limit=0 不应发 SQL,录到 %d 条", n)
	}
	ids, err := repo.ListAbandonCandidates(context.Background(), 100, 7, 2)
	if err != nil {
		t.Fatalf("ListAbandonCandidates: %v", err)
	}
	if !reflect.DeepEqual(ids, []uint64{8, 9}) {
		t.Errorf("ids = %v, want [8 9]", ids)
	}
	calls := s.recordedCalls()
	if len(calls) != 1 || strings.Contains(calls[0].sql, "FOR UPDATE") {
		t.Fatalf("候选读必须是一条非锁读,录到 %+v", calls)
	}
	// 实参顺序:PENDING, createdBefore, afterOpID, limit。
	args := calls[0].args
	if argInt(t, args[0]) != uint64(PendingStatus()) || argInt(t, args[1]) != 100 || argInt(t, args[2]) != 7 || argInt(t, args[3]) != 2 {
		t.Errorf("候选读实参 = %v, want (PENDING, 100, 7, 2)", args)
	}
}

// ── PlayerPendingLister ─────────────────────────────────────

// TestListPendingByPlayer:落地唤醒按玩家列 PENDING 行,非锁读,**不**看 next_attempt_ms / lease_until_ms
// (唤醒就是要提前,租约由随后的 Claim CAS 判);limit ≤ 0 不发 SQL。
func TestListPendingByPlayer(t *testing.T) {
	stmt := collapseSpaces(sqlListPendingByPlayer)
	for _, banned := range []string{"next_attempt_ms", "lease_until_ms", "FOR UPDATE"} {
		if strings.Contains(stmt, banned) {
			t.Errorf("按玩家列未决行不得带 %s: %s", banned, stmt)
		}
	}

	s := &recovScript{}
	s.rowsFor = func(string, []driver.Value) ([]string, [][]driver.Value) {
		return []string{"op_id"}, [][]driver.Value{{int64(31)}, {int64(30)}}
	}
	repo := s.repo(t)
	if ids, err := repo.ListPendingByPlayer(context.Background(), 42, 0); err != nil || ids != nil {
		t.Errorf("limit=0 = (%v, %v), want (nil, nil)", ids, err)
	}
	if n := len(s.recordedCalls()); n != 0 {
		t.Fatalf("limit=0 不应发 SQL,录到 %d 条", n)
	}
	ids, err := repo.ListPendingByPlayer(context.Background(), 42, 32)
	if err != nil {
		t.Fatalf("ListPendingByPlayer: %v", err)
	}
	if !reflect.DeepEqual(ids, []uint64{31, 30}) {
		t.Errorf("ids = %v, want 按查询顺序原样返回 [31 30]", ids)
	}
	args := s.recordedCalls()[0].args
	// 实参顺序:player_id, PENDING, limit。
	if argInt(t, args[0]) != 42 || argInt(t, args[1]) != uint64(PendingStatus()) || argInt(t, args[2]) != 32 {
		t.Errorf("实参 = %v, want (42, PENDING, 32)", args)
	}
}

// ── PendingClassCounter ─────────────────────────────────────

// TestCountPendingByClassPagesAndUsesSharedClassifier:日对账按主键分页读全部 PENDING 行(满页才读下一页,
// 下一页从上一页最后一个 op_id 之后接着读),分类口径只用 assetop.PendingTally —— 期望值由同一个累加器算出,
// 这里只证明"每一行都喂进去了、一行不多一行不少",不在测试里另抄一份分类规则。
func TestCountPendingByClassPagesAndUsesSharedClassifier(t *testing.T) {
	const nowMs, abandonBeforeMs = 10_000_000, 5_000_000
	// 第一页恰满 auditPageSize 行,第二页 3 行:应恰好读两页。
	var facts []assetop.PendingFacts
	var pages [][][]driver.Value
	var page [][]driver.Value
	outcomes := []assetpb.AssetOpOutcome{
		assetpb.AssetOpOutcome_ASSET_OP_OUTCOME_UNKNOWN,
		assetpb.AssetOpOutcome_ASSET_OP_OUTCOME_NOT_HERE,
		assetpb.AssetOpOutcome_ASSET_OP_OUTCOME_RETRY,
	}
	streams := []assetpb.AssetOpStream{
		assetpb.AssetOpStream_ASSET_OP_STREAM_TRADE_DEBIT,
		assetpb.AssetOpStream_ASSET_OP_STREAM_TRADE_CREDIT,
	}
	total := auditPageSize + 3
	for i := 0; i < total; i++ {
		f := assetop.PendingFacts{
			Stream:      streams[i%len(streams)],
			CreatedMs:   uint64(1_000_000 * (i%10 + 1)),
			Attempts:    uint32(i % 4),
			LastOutcome: outcomes[i%len(outcomes)],
			LastReason:  0,
		}
		facts = append(facts, f)
		page = append(page, []driver.Value{
			int64(i + 1), int64(f.Stream), int64(f.CreatedMs), int64(f.Attempts), int64(f.LastOutcome), int64(f.LastReason),
		})
		if len(page) == auditPageSize {
			pages = append(pages, page)
			page = nil
		}
	}
	pages = append(pages, page)

	s := &recovScript{}
	s.rowsFor = func(_ string, args []driver.Value) ([]string, [][]driver.Value) {
		cols := []string{"op_id", "stream", "created_ms", "attempts", "last_outcome", "last_reason"}
		after := args[1].(int64)
		switch after {
		case 0:
			return cols, pages[0]
		case int64(auditPageSize):
			return cols, pages[1]
		default:
			return cols, nil
		}
	}
	repo := s.repo(t)

	got, err := repo.CountPendingByClass(context.Background(), nowMs, abandonBeforeMs)
	if err != nil {
		t.Fatalf("CountPendingByClass: %v", err)
	}
	want := assetop.NewPendingTally(nowMs, abandonBeforeMs)
	for _, f := range facts {
		want.Add(f)
	}
	if !reflect.DeepEqual(got, want.Counts()) {
		t.Errorf("计数 = %+v\nwant  %+v", got, want.Counts())
	}

	calls := s.recordedCalls()
	if len(calls) != 2 {
		t.Fatalf("应恰好读两页,录到 %d 条: %+v", len(calls), calls)
	}
	for i, c := range calls {
		if strings.Contains(c.sql, "FOR UPDATE") {
			t.Errorf("第 %d 页是锁定读: %s", i+1, c.sql)
		}
		// 实参顺序:PENDING, afterOpID, pageSize。
		if argInt(t, c.args[0]) != uint64(PendingStatus()) || argInt(t, c.args[2]) != auditPageSize {
			t.Errorf("第 %d 页实参 = %v", i+1, c.args)
		}
	}
	if argInt(t, calls[1].args[1]) != uint64(auditPageSize) {
		t.Errorf("第二页应从第一页最后一个 op_id(%d)之后接着读,得到 %v", auditPageSize, calls[1].args[1])
	}
}

// ── 终态行清理 ──────────────────────────────────────────────

// TestCleanupStatusesTreatAbandonedAsTerminal:ABANDONED 与其它终态一样过保留期就删(设计 §6.3「状态与展示」);
// PENDING 永不删,APPLIED_PARTIAL 要人工补偿,证据不能自动消失。
func TestCleanupStatusesTreatAbandonedAsTerminal(t *testing.T) {
	got := map[int32]bool{}
	for _, st := range cleanupStatuses() {
		got[st] = true
	}
	for _, must := range []tradepb.TradeAssetOpStatus{
		tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_APPLIED,
		tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_REJECTED,
		tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_ABORTED,
		tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_ABANDONED,
	} {
		if !got[int32(must)] {
			t.Errorf("清理口径漏了终态 %v", must)
		}
	}
	for _, never := range []tradepb.TradeAssetOpStatus{
		tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_PENDING,
		tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_APPLIED_PARTIAL,
		tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_UNSPECIFIED,
	} {
		if got[int32(never)] {
			t.Errorf("清理口径不得包含 %v", never)
		}
	}
}

// TestRunCleanupRefusesInvalidConf:间隔或保留期非正时不启动、立刻返回(不碰库;本用例没有连接池)。
// 保留期 0 等于"删光所有终态行",不可能是有意的配置。
func TestRunCleanupRefusesInvalidConf(t *testing.T) {
	repo, err := NewAssetOpRepo(nil, time.Second)
	if err != nil {
		t.Fatalf("NewAssetOpRepo: %v", err)
	}
	for _, c := range []CleanupConf{
		{Interval: 0, TerminalRetention: 30 * 24 * time.Hour},
		{Interval: time.Minute, TerminalRetention: 0},
	} {
		done := make(chan struct{})
		go func() {
			defer close(done)
			repo.RunCleanup(context.Background(), c)
		}()
		select {
		case <-done:
		case <-time.After(time.Second):
			t.Fatalf("非法配置 %+v 下 RunCleanup 没有立刻返回", c)
		}
	}
}

// TestRunCleanupReturnsOnCancel:ctx 取消即返回(svc 的 stop 会等它退出,再去关库)。
// 间隔取 1 小时,随机初始延迟在本用例里不会到点,于是不会碰到没有连接池的仓库。
func TestRunCleanupReturnsOnCancel(t *testing.T) {
	repo, err := NewAssetOpRepo(nil, time.Second)
	if err != nil {
		t.Fatalf("NewAssetOpRepo: %v", err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan struct{})
	go func() {
		defer close(done)
		repo.RunCleanup(ctx, CleanupConf{Interval: time.Hour, TerminalRetention: 30 * 24 * time.Hour})
	}()
	cancel()
	select {
	case <-done:
	case <-time.After(time.Second):
		t.Fatal("ctx 取消后 RunCleanup 没有返回")
	}
}
