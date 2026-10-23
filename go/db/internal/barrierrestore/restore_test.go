package barrierrestore

import (
	"context"
	"database/sql"
	"database/sql/driver"
	"errors"
	"fmt"
	"io"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/require"
	"shared/savebarrier"
)

// 录制驱动只隔离 SQL I/O；回归调用完整 Prepare/ApplyOne 与真实 Lua，核对提交前后可观察状态。
type testStore struct {
	mu         sync.Mutex
	rows       [2][]driver.Value
	commitFail bool
	onCommit   func()
	commits    int
	columns    []string
}
type storeDriver struct{ s *testStore }
type storeConn struct {
	s  *testStore
	tx *storeTx
}
type storeTx struct {
	c       *storeConn
	pending [2][]driver.Value
	writes  bool
}
type storeRows struct {
	values  []driver.Value
	columns []string
	done    bool
}

var driverID atomic.Uint64

func (d storeDriver) Open(string) (driver.Conn, error) { return &storeConn{s: d.s}, nil }
func (c *storeConn) Prepare(string) (driver.Stmt, error) {
	return nil, errors.New("不使用预备语句")
}
func (c *storeConn) Close() error { return nil }
func (c *storeConn) Begin() (driver.Tx, error) {
	return c.BeginTx(context.Background(), driver.TxOptions{})
}
func (c *storeConn) BeginTx(_ context.Context, o driver.TxOptions) (driver.Tx, error) {
	if o.Isolation != driver.IsolationLevel(sql.LevelReadCommitted) {
		return nil, errors.New("必须使用 RC")
	}
	c.s.mu.Lock()
	tx := &storeTx{c: c}
	for i := range tx.pending {
		tx.pending[i] = append([]driver.Value(nil), c.s.rows[i]...)
	}
	c.s.mu.Unlock()
	c.tx = tx
	return tx, nil
}
func (c *storeConn) QueryContext(_ context.Context, q string, _ []driver.NamedValue) (driver.Rows, error) {
	if c.tx == nil || !strings.HasSuffix(q, " WHERE player_id=? FOR UPDATE") {
		return nil, errors.New("必须按完整主键点锁")
	}
	i := 0
	if strings.Contains(q, "player_database_1 ") {
		i = 1
	}
	return &storeRows{values: append([]driver.Value(nil), c.tx.pending[i]...), columns: c.s.columns}, nil
}
func (c *storeConn) ExecContext(_ context.Context, q string, args []driver.NamedValue) (driver.Result, error) {
	if c.tx == nil || !strings.Contains(q, "WHERE player_id=? AND fence_owner_epoch=0 AND fence_save_seq=0") {
		return nil, errors.New("必须沿锁定行 CAS")
	}
	i := 0
	if strings.Contains(q, "player_database_1 ") {
		i = 1
	}
	c.tx.pending[i][1] = []byte(fmt.Sprint(args[0].Value))
	c.tx.pending[i][2] = []byte(fmt.Sprint(args[1].Value))
	c.tx.writes = true
	return driver.RowsAffected(1), nil
}
func (tx *storeTx) Commit() error {
	tx.c.s.mu.Lock()
	defer tx.c.s.mu.Unlock()
	if tx.c.s.commitFail {
		return errors.New("合成提交失败")
	}
	if tx.writes {
		tx.c.s.rows = tx.pending
		tx.c.s.commits++
		if tx.c.s.onCommit != nil {
			tx.c.s.onCommit()
		}
	}
	tx.c.tx = nil
	return nil
}
func (tx *storeTx) Rollback() error { tx.c.tx = nil; return nil }
func (r *storeRows) Columns() []string {
	if r.columns != nil {
		return r.columns
	}
	return []string{"player_id", "fence_owner_epoch", "fence_save_seq", "payload", "persist_schema_gen"}
}
func (r *storeRows) Close() error { return nil }
func (r *storeRows) Next(v []driver.Value) error {
	if r.done || len(r.values) == 0 {
		return io.EOF
	}
	copy(v, r.values)
	r.done = true
	return nil
}

func fixture(t *testing.T, epoch, seq string) (Restorer, *testStore, *miniredis.Miniredis) {
	t.Helper()
	// 只描述正常迁移后列默认0的录制结果，不冒称真实SQL迁移已执行。
	s := &testStore{rows: [2][]driver.Value{{[]byte("7"), []byte(epoch), []byte(seq), []byte("真实资产甲"), []byte("0")}, {[]byte("7"), []byte(epoch), []byte(seq), []byte("真实资产乙"), []byte("0")}}}
	name := fmt.Sprintf("barrier-restore-%d", driverID.Add(1))
	sql.Register(name, storeDriver{s})
	db, err := sql.Open(name, "")
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { db.Close() })
	m := miniredis.RunT(t)
	rc := redis.NewClient(&redis.Options{Addr: m.Addr(), MaxRetries: -1})
	t.Cleanup(func() { rc.Close() })
	m.Set(MaintenanceKey(17), "维护工单")
	m.Set("player:7:owner_epoch", "9")
	m.Set("player:zone:7", "17")
	m.Set("player:placement:7", "17:1")
	return Restorer{DB: db, Redis: rc, RoutingMode: RoutingModeRedis}, s, m
}
func prepare(t *testing.T, r Restorer, legacy bool) Plan {
	t.Helper()
	p, err := r.Prepare(context.Background(), "zone_17_db", 17, "维护工单", []string{"7"}, legacy)
	if err != nil {
		t.Fatal(err)
	}
	return p
}

func TestRestoreNonzeroAndReplay(t *testing.T) {
	r, s, m := fixture(t, "8", "90")
	p := prepare(t, r, false)
	if len(m.Keys()) != 4 {
		t.Fatal("plan 写了 Redis")
	}
	for i := 0; i < 2; i++ {
		if err := r.ApplyOne(context.Background(), p, 0); err != nil {
			t.Fatal(err)
		}
	}
	v := savebarrier.Version(8, 90)
	for _, field := range []string{"pending", "player_database", "player_database_1"} {
		if got := m.HGet("player:{7}:save_barrier", field); got != v {
			t.Fatalf("%s=%s", field, got)
		}
	}
	if s.commits != 0 {
		t.Fatal("非零恢复不应修改 SQL")
	}
}

func TestLegacyMigrationCommitsBeforePublicationAndRechecks(t *testing.T) {
	r, s, m := fixture(t, "0", "0")
	p := prepare(t, r, true)
	s.onCommit = func() {
		if m.Exists("player:{7}:save_barrier") {
			t.Error("SQL 提交前已发布屏障")
		}
	}
	if err := r.ApplyOne(context.Background(), p, 0); err != nil {
		t.Fatal(err)
	}
	if s.commits != 1 || string(s.rows[0][3].([]byte)) != "真实资产甲" || string(s.rows[1][3].([]byte)) != "真实资产乙" {
		t.Fatal("迁移改了资产或未原子提交")
	}
	if got := m.HGet("player:{7}:save_barrier", "pending"); got != savebarrier.Version(9, 1) {
		t.Fatal(got)
	}
	if err := r.ApplyOne(context.Background(), p, 0); err != nil {
		t.Fatal(err)
	}
	if s.commits != 1 {
		t.Fatal("重投再次修改 SQL")
	}
}

func TestLegacyCommitFailureNeverPublishes(t *testing.T) {
	r, s, m := fixture(t, "0", "0")
	p := prepare(t, r, true)
	s.commitFail = true
	if err := r.ApplyOne(context.Background(), p, 0); err == nil {
		t.Fatal("预期提交失败")
	}
	if m.Exists("player:{7}:save_barrier") {
		t.Fatal("失败提交冒充成功 ACK")
	}
	if string(s.rows[0][1].([]byte)) != "0" || string(s.rows[1][1].([]byte)) != "0" {
		t.Fatal("失败事务部分写入")
	}
}

func TestLegacyInterruptedAfterCommitCanResume(t *testing.T) {
	r, s, m := fixture(t, "0", "0")
	p := prepare(t, r, true)
	s.onCommit = func() { m.Set(MaintenanceKey(17), "其他维护") }
	if err := r.ApplyOne(context.Background(), p, 0); err == nil {
		t.Fatal("维护号变更应拒绝")
	}
	if s.commits != 1 || m.Exists("player:{7}:save_barrier") {
		t.Fatal("SQL 提交与屏障阶段未隔离")
	}
	m.Set(MaintenanceKey(17), "维护工单")
	s.onCommit = nil
	if err := r.ApplyOne(context.Background(), p, 0); err != nil {
		t.Fatal(err)
	}
	if s.commits != 1 {
		t.Fatal("同计划续跑重复迁移")
	}
}

func TestLegacyRechecksSQLAfterCommit(t *testing.T) {
	for _, change := range []struct {
		name string
		run  func(*testStore)
	}{
		{"资产变化", func(s *testStore) { s.rows[0][3] = []byte("提交后发生了其他写") }},
		{"行删除", func(s *testStore) { s.rows[1] = nil }},
		{"围栏变化", func(s *testStore) { s.rows[0][2] = []byte("2"); s.rows[1][2] = []byte("2") }},
	} {
		t.Run(change.name, func(t *testing.T) {
			r, s, m := fixture(t, "0", "0")
			p := prepare(t, r, true)
			s.onCommit = func() { change.run(s) }
			if err := r.ApplyOne(context.Background(), p, 0); err == nil {
				t.Fatal("重锁没有识别 SQL 改变")
			}
			if m.Exists("player:{7}:save_barrier") {
				t.Fatal("给变化后的 SQL 发布了旧证据")
			}
		})
	}
}

func TestPlanBindsHomeZoneAndRedisTarget(t *testing.T) {
	r, _, _ := fixture(t, "8", "90")
	p := prepare(t, r, false)
	if _, err := r.Prepare(context.Background(), "zone_17_db", 18, "维护工单", []string{"7"}, false); err == nil {
		t.Fatal("错区预检未拒绝")
	}
	if _, err := r.Prepare(context.Background(), "player_store_1000000_db", 17, "维护工单", []string{"7"}, false); err == nil {
		t.Fatal("无权威落点证明却恢复异区库")
	}
	p.RedisDB = 1
	if err := r.ApplyOne(context.Background(), p, 0); err == nil {
		t.Fatal("Redis DB 偏离计划未拒绝")
	}
	p.RedisDB = 0
	p.LocalMySQL = true
	if err := r.ApplyOne(context.Background(), p, 0); err == nil {
		t.Fatal("SQL 强读模式偏离计划未拒绝")
	}
	p.LocalMySQL = false
	p.RedisAddr = "127.0.0.1:1"
	if err := r.ApplyOne(context.Background(), p, 0); err == nil {
		t.Fatal("Redis 地址偏离计划未拒绝")
	}
}

func TestLegacyRefusesPrematureBarrier(t *testing.T) {
	r, _, m := fixture(t, "0", "0")
	v := savebarrier.Version(9, 1)
	m.HSet("player:{7}:save_barrier", "pending", v, "player_database", v, "player_database_1", v)
	if _, err := r.Prepare(context.Background(), "zone_17_db", 17, "维护工单", []string{"7"}, true); err == nil {
		t.Fatal("零 SQL 围栏不能接受提前发布的 ACK")
	}
}

func TestFailClosedDriftAndRedisConflicts(t *testing.T) {
	cases := []struct {
		name   string
		change func(*testStore, *miniredis.Miniredis, []string)
	}{
		{"资产改变", func(s *testStore, _ *miniredis.Miniredis, _ []string) { s.rows[0][3] = []byte("别的资产") }},
		{"两表不同版", func(s *testStore, _ *miniredis.Miniredis, _ []string) { s.rows[1][2] = []byte("91") }},
		{"行缺失", func(s *testStore, _ *miniredis.Miniredis, _ []string) { s.rows[1] = nil }},
		{"归属改变", func(_ *testStore, m *miniredis.Miniredis, k []string) { m.Set(k[1], "10") }},
		{"归属丢失", func(_ *testStore, m *miniredis.Miniredis, k []string) { m.Del(k[1]) }},
		{"维护号改变", func(_ *testStore, m *miniredis.Miniredis, k []string) { m.Set(k[0], "新维护") }},
		{"维护号有过期", func(_ *testStore, m *miniredis.Miniredis, k []string) { m.SetTTL(k[0], time.Minute) }},
		{"归属有过期", func(_ *testStore, m *miniredis.Miniredis, k []string) { m.SetTTL(k[1], time.Minute) }},
		{"未提交意图", func(_ *testStore, m *miniredis.Miniredis, k []string) {
			m.HSet(k[7], "outbox_version", savebarrier.Version(8, 90))
		}},
		{"屏障版本冲突", func(_ *testStore, m *miniredis.Miniredis, k []string) {
			m.HSet(k[7], "pending", savebarrier.Version(8, 91))
		}},
		{"末桶未排空", func(_ *testStore, m *miniredis.Miniredis, k []string) { m.ZAdd(k[71], 0, "其他玩家") }},
		{"桶错型", func(_ *testStore, m *miniredis.Miniredis, k []string) { m.Set(k[8], "错型") }},
		{"父键存在", func(_ *testStore, m *miniredis.Miniredis, k []string) { m.Set(k[2], "旧父快照") }},
		{"子缓存存在", func(_ *testStore, m *miniredis.Miniredis, k []string) { m.Set(k[4], "旧子表") }},
		{"旧资产证据", func(_ *testStore, m *miniredis.Miniredis, k []string) { m.Set(k[5], "旧证据") }},
		{"交接仍在", func(_ *testStore, m *miniredis.Miniredis, k []string) { m.Set(k[6], "8:1") }},
		{"home区变化", func(_ *testStore, m *miniredis.Miniredis, k []string) { m.Set(k[72], "18") }},
		{"落点变化", func(_ *testStore, m *miniredis.Miniredis, k []string) { m.Set(k[73], "1000000:2") }},
		{"映射缺失", func(_ *testStore, m *miniredis.Miniredis, k []string) { m.Del(k[73]) }},
		{"落点冻结", func(_ *testStore, m *miniredis.Miniredis, k []string) { m.Set(k[73], "17:1:frozen:run1") }},
		{"合服在途", func(_ *testStore, m *miniredis.Miniredis, k []string) { m.Set(k[74], "维护中") }},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			r, s, m := fixture(t, "8", "90")
			p := prepare(t, r, false)
			tc.change(s, m, recoveryKeys(17, 7))
			before := m.Dump()
			if err := r.ApplyOne(context.Background(), p, 0); err == nil {
				t.Fatal("冲突未拒绝")
			}
			if m.Dump() != before || s.commits != 0 {
				t.Fatal("拒绝留下副作用")
			}
		})
	}
}

func TestPlanValidationBeforeIO(t *testing.T) {
	r, s, m := fixture(t, "0", "0")
	if _, err := r.Prepare(context.Background(), "zone_17_db", 17, "维护工单", []string{"7"}, false); err == nil {
		t.Fatal("未授权零围栏迁移")
	}
	for _, ids := range [][]string{{"7", "7"}, {"07"}, {"18446744073709551616"}, {"0"}} {
		if _, err := r.Prepare(context.Background(), "zone_17_db", 17, "维护工单", ids, true); err == nil {
			t.Fatal(ids)
		}
	}
	if _, err := r.Prepare(context.Background(), "zone_17_db;DROP TABLE player_database", 17, "维护工单", []string{"7"}, true); err == nil {
		t.Fatal("危险 schema 未拒绝")
	}
	if s.commits != 0 || len(m.Keys()) != 4 {
		t.Fatal("预检改了状态")
	}
}

// 保留全部四持久字段，另验永久撤销 floor；返回原文供完整 map 精确比较。
func assertRestoredFence(t *testing.T, raw, owner, instance, generation, admission string) string {
	t.Helper()
	parts := strings.Split(raw, "|")
	require.Len(t, parts, 6)
	require.Equal(t, []string{owner, instance, generation, admission, "R"}, parts[:5])
	require.Len(t, parts[5], 32)
	require.NotEqual(t, strings.Repeat("0", 32), parts[5])
	for _, ch := range parts[5] {
		require.True(t, ch >= '0' && ch <= '9' || ch >= 'a' && ch <= 'f')
	}
	return raw
}

func TestRestoreRevokesOriginalHandoffFloorAndRotatesEveryTime(t *testing.T) {
	for _, known := range []bool{false, true} {
		t.Run(fmt.Sprint("known=", known), func(t *testing.T) {
			r, _, cache := fixture(t, "8", "90")
			plan := prepare(t, r, false)
			admission := strings.Repeat("a", 32)
			if known {
				v := savebarrier.Version(8, 90)
				cache.HSet(savebarrier.Key(7), "pending", v, "player_database", v, "player_database_1", v,
					"handoff_fence", "8|scene|12|"+admission+"|H|"+strings.Repeat("b", 32))
			}
			last := ""
			for i := 0; i < 2; i++ {
				require.NoError(t, r.ApplyOne(context.Background(), plan, 0))
				raw := cache.HGet(savebarrier.Key(7), "handoff_fence")
				if known {
					assertRestoredFence(t, raw, "8", "scene", "12", admission)
				} else {
					assertRestoredFence(t, raw, "9", "", "0", strings.Repeat("0", 32))
				}
				require.NotEqual(t, last, raw)
				last = raw
				require.Equal(t, time.Duration(0), cache.TTL(savebarrier.Key(7)), "永久下限不随标记TTL失效")
			}
		})
	}
}

func TestRestoreRejectsDamagedHandoffFloorWithoutAnyMutation(t *testing.T) {
	for _, raw := range []string{"", "8|scene|1|bad|H|bad", "10|scene|1|" + strings.Repeat("a", 32) + "|H|" + strings.Repeat("b", 32)} {
		t.Run(raw, func(t *testing.T) {
			r, store, cache := fixture(t, "8", "90")
			p := prepare(t, r, false)
			v := savebarrier.Version(8, 90)
			cache.HSet(savebarrier.Key(7), "pending", v, "player_database", v, "player_database_1", v, "handoff_fence", raw)
			before := cache.Dump()
			require.Error(t, r.ApplyOne(context.Background(), p, 0))
			require.Equal(t, before, cache.Dump())
			require.Zero(t, store.commits)
		})
	}
}
