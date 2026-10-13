package store

import (
	"context"
	"database/sql"
	"database/sql/driver"
	"errors"
	"fmt"
	"io"
	"reflect"
	"strings"
	"sync"
	"testing"

	"shared/placement"

	"github.com/go-sql-driver/mysql"
)

// RouteStore 的**对外语义**回归:哪种库内情形回哪个结局 / 错误、发哪几条语句(含 BEGIN / COMMIT / ROLLBACK)、
// 每条语句的文本与实参。仿 player_name_store_contract_test.go:真库用例需要 Docker、默认 skip,
// 只靠它们的话,一次"顺手改写 SQL 或调整分支"在 `go test ./...` 全绿的情况下就能把 W1 的契约改掉。
//
// 与 player_name 的假驱动相比多了两样:
//   - 事务:实现 driver.ConnBeginTx,BEGIN / COMMIT / ROLLBACK 与语句按发生顺序记进同一条序列;
//   - 参数转换:实现 driver.NamedValueChecker,按 go-sql-driver/mysql v1.9 的转换规则转换本 store 用得到的那几类实参
//     (见 mysqlLikeConvert,只复刻子集)。
//     不复刻的话 database/sql 会走默认转换器,uint64 被改成 int64(高位为 1 时直接报错)——
//     断言写的就不是生产驱动实际收到的形状,也证明不了「生产驱动收不收这个参数」。
//
// 假驱动**不模拟 InnoDB 的锁、唯一约束与 CHECK**:并发下谁赢、会不会 1213,只有真库能证
// (route_store_integration_test.go)。这里只证"给定库的应答,本层的判定、语句与参数不变"。

// fakeRouteReply 是一条语句的应答:Exec 看 affected,Query 看 rows(nil = 零行 → sql.ErrNoRows);
// err 非 nil 时其余字段被忽略。
type fakeRouteReply struct {
	affected int64
	rows     [][]driver.Value
	err      error
}

// fakeRouteCall 是真的发到驱动的一次调用。label 是语句标签(见 routeStmtLabel)或 BEGIN / COMMIT / ROLLBACK。
type fakeRouteCall struct {
	label string
	sql   string
	args  []driver.Value
}

// fakeRouteScript 是一次用例的应答脚本:按语句标签排队,按调用顺序消费;队列空了就报错
// (而不是给个默认值),免得"脚本写少了"被当成正常路径悄悄通过。
type fakeRouteScript struct {
	replies map[string][]fakeRouteReply
	// commitErrs 按顺序作为每次 COMMIT 的结果;用完之后 COMMIT 一律成功。
	commitErrs []error
	// onRollback 在第一次 ROLLBACK 被记下之后调用一次。只给"退避途中 ctx 结束"那个用例用:
	// 在事务已经回滚之后再取消,序列才是确定的(事务还开着时取消,database/sql 会在后台协程里回滚)。
	onRollback func()
}

type fakeRouteDB struct {
	mu     sync.Mutex
	script fakeRouteScript
	calls  []fakeRouteCall
}

var errFakeRouteScriptExhausted = errors.New("fake route db: 脚本用尽,被测代码发的语句比用例预期的多")

// 本用例集固定的 store 参数。
const (
	fakeRouteWriter        = "data_service@unit-test"
	fakeRouteShards uint32 = 8
)

func newFakeRouteDB(script fakeRouteScript) *fakeRouteDB {
	// 拷一份队列:表驱动用例的脚本会被消费,不能改到用例表本身。
	replies := make(map[string][]fakeRouteReply, len(script.replies))
	for k, v := range script.replies {
		replies[k] = append([]fakeRouteReply(nil), v...)
	}
	script.replies = replies
	script.commitErrs = append([]error(nil), script.commitErrs...)
	return &fakeRouteDB{script: script}
}

// newFakeRouteStore 把假驱动接成一个 RouteStore(同包,直接装字段,不跑探针),同时把假库还给用例。
func newFakeRouteStore(t *testing.T, script fakeRouteScript) (*RouteStore, *fakeRouteDB) {
	t.Helper()
	fake := newFakeRouteDB(script)
	db := sql.OpenDB(fakeRouteConnector{fake: fake})
	t.Cleanup(func() { _ = db.Close() })
	return &RouteStore{db: db, fenceShards: fakeRouteShards, writer: fakeRouteWriter}, fake
}

// routeStmtLabel 把 SQL 文本映射成规格 b.4 的编号。只认**逐字相同**的生产文本(动态语句认前缀,
// 用例再单独断言整段文本),认不出来就是空串 —— 假驱动对它报错。
func routeStmtLabel(query string) string {
	switch query {
	case routeProbeSessionSQL:
		return "PROBE"
	case "SELECT VERSION()": // serverIsTiDB(schema.go)
		return "VERSION"
	case routeProbeTiDBTxnModeSQL:
		return "TXN_MODE"
	case routeReadWritableSQL:
		return "R1"
	case routeLockFenceShardSQL:
		return "R2"
	case routeEnsureFenceShardSQL:
		return "R3"
	case routeInsertNewSQL:
		return "R4"
	case routeLockRowSQL:
		return "R5"
	case routeFillHomeSQL:
		return "R6a"
	case routeFillHomePlacementSQL:
		return "R6b"
	case routeImportFillSQL:
		return "R9"
	case routeInsertMergedIntoSQL:
		return "R10"
	case routeLockMergedIntoSQL:
		return "R11"
	case routeReadMergedIntoSQL:
		return "R11ro"
	case routeScanSQL:
		return "R12"
	case routeListMergedIntoSQL:
		return "R14"
	case routeMetaSeedSQL:
		return "R15"
	case routeReconcileSeedSQL:
		return "R16"
	case routeProbeShardCheckSQL:
		return "PROBE_CHECK"
	case routeTiDBPKTypeSQL:
		return "PK_TYPE"
	}
	switch {
	case strings.HasPrefix(query, routeImportInsertHeadSQL):
		return "R7"
	case strings.HasPrefix(query, routeSelectRowsHeadSQL) && strings.HasSuffix(query, " FOR UPDATE"):
		return "R8"
	case strings.HasPrefix(query, routeSelectRowsHeadSQL):
		return "R8ro"
	case strings.HasPrefix(query, "ALTER TABLE "): // tidb-prepare 的 T2(用例再逐字比对整段文本)
		return "ALTER_ATTR"
	case strings.HasPrefix(query, "SPLIT TABLE "): // tidb-prepare 的 T3
		return "SPLIT"
	}
	return ""
}

func (f *fakeRouteDB) next(label, query string, args []driver.Value) (fakeRouteReply, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.calls = append(f.calls, fakeRouteCall{label: label, sql: query, args: args})
	if label == "" {
		return fakeRouteReply{}, fmt.Errorf("fake route db: 未预期的语句 %q", query)
	}
	q := f.script.replies[label]
	if len(q) == 0 {
		return fakeRouteReply{}, fmt.Errorf("%w: %s %q", errFakeRouteScriptExhausted, label, query)
	}
	f.script.replies[label] = q[1:]
	return q[0], nil
}

func (f *fakeRouteDB) record(label string) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.calls = append(f.calls, fakeRouteCall{label: label})
}

func (f *fakeRouteDB) commit() error {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.calls = append(f.calls, fakeRouteCall{label: "COMMIT"})
	if len(f.script.commitErrs) == 0 {
		return nil
	}
	err := f.script.commitErrs[0]
	f.script.commitErrs = f.script.commitErrs[1:]
	return err
}

func (f *fakeRouteDB) rollback() error {
	f.mu.Lock()
	f.calls = append(f.calls, fakeRouteCall{label: "ROLLBACK"})
	hook := f.script.onRollback
	f.script.onRollback = nil
	f.mu.Unlock()
	if hook != nil {
		hook()
	}
	return nil
}

// labels / callsWithLabel 给断言用。
func (f *fakeRouteDB) labels() []string {
	f.mu.Lock()
	defer f.mu.Unlock()
	out := make([]string, 0, len(f.calls))
	for _, c := range f.calls {
		out = append(out, c.label)
	}
	return out
}

func (f *fakeRouteDB) snapshot() []fakeRouteCall {
	f.mu.Lock()
	defer f.mu.Unlock()
	return append([]fakeRouteCall(nil), f.calls...)
}

func (f *fakeRouteDB) callsWithLabel(label string) []fakeRouteCall {
	var out []fakeRouteCall
	for _, c := range f.snapshot() {
		if c.label == label {
			out = append(out, c)
		}
	}
	return out
}

// ── 驱动实现 ─────────────────────────────────────────────────────────────────────

type fakeRouteConnector struct{ fake *fakeRouteDB }

func (c fakeRouteConnector) Connect(context.Context) (driver.Conn, error) {
	return &fakeRouteConn{fake: c.fake}, nil
}

func (c fakeRouteConnector) Driver() driver.Driver { return fakeRouteDriver{} }

type fakeRouteDriver struct{}

func (fakeRouteDriver) Open(string) (driver.Conn, error) {
	return nil, errors.New("fake route db: 只支持 sql.OpenDB(connector)")
}

type fakeRouteConn struct{ fake *fakeRouteDB }

// Prepare 不会被用到:database/sql 对实现了 ExecerContext / QueryerContext 的连接不再走 Prepare。
func (c *fakeRouteConn) Prepare(string) (driver.Stmt, error) {
	return nil, errors.New("fake route db: 不支持 Prepare")
}

func (c *fakeRouteConn) Close() error { return nil }

func (c *fakeRouteConn) Begin() (driver.Tx, error) {
	return c.BeginTx(context.Background(), driver.TxOptions{})
}

func (c *fakeRouteConn) BeginTx(context.Context, driver.TxOptions) (driver.Tx, error) {
	c.fake.record("BEGIN")
	return fakeRouteTx{fake: c.fake}, nil
}

// CheckNamedValue 让 database/sql 用与 go-sql-driver 相同的规则转换参数(见 mysqlLikeConvert)。
func (c *fakeRouteConn) CheckNamedValue(nv *driver.NamedValue) error {
	v, err := mysqlLikeConvert(nv.Value)
	if err != nil {
		return err
	}
	nv.Value = v
	return nil
}

func (c *fakeRouteConn) ExecContext(_ context.Context, query string, args []driver.NamedValue) (driver.Result, error) {
	reply, err := c.fake.next(routeStmtLabel(query), query, namedToValues(args))
	if err != nil {
		return nil, err
	}
	if reply.err != nil {
		return nil, reply.err
	}
	return driver.RowsAffected(reply.affected), nil
}

func (c *fakeRouteConn) QueryContext(_ context.Context, query string, args []driver.NamedValue) (driver.Rows, error) {
	reply, err := c.fake.next(routeStmtLabel(query), query, namedToValues(args))
	if err != nil {
		return nil, err
	}
	if reply.err != nil {
		return nil, reply.err
	}
	cols := 1
	if len(reply.rows) > 0 {
		cols = len(reply.rows[0])
	}
	return &fakeRouteRows{cols: cols, rows: append([][]driver.Value(nil), reply.rows...)}, nil
}

type fakeRouteTx struct{ fake *fakeRouteDB }

func (t fakeRouteTx) Commit() error   { return t.fake.commit() }
func (t fakeRouteTx) Rollback() error { return t.fake.rollback() }

type fakeRouteRows struct {
	cols int
	rows [][]driver.Value
}

func (r *fakeRouteRows) Columns() []string {
	names := make([]string, r.cols)
	for i := range names {
		names[i] = fmt.Sprintf("c%d", i)
	}
	return names
}

func (r *fakeRouteRows) Close() error { return nil }

func (r *fakeRouteRows) Next(dest []driver.Value) error {
	if len(r.rows) == 0 {
		return io.EOF
	}
	copy(dest, r.rows[0])
	r.rows = r.rows[1:]
	return nil
}

// mysqlLikeConvert 只复刻 go-sql-driver/mysql v1.9 参数转换(connection.go 的 CheckNamedValue →
// statement.go 的 converter.ConvertValue)里**本 store 会走到的子集**:nil、int64、uint64、string、[]byte
// 与 sql.Null* 这几类实参。不是逐字复刻,换实参类型时要先回去对照驱动源码:
//   - driver.IsValue 的值原样放行(nil、int64、string、[]byte ...);
//   - driver.Valuer 先取 Value(),结果必须是 driver.IsValue 或 uint64,否则报 "non-Value type";
//   - 其余按反射:有符号整数 → int64,无符号整数 → uint64(与 database/sql 默认转换器不同,
//     后者把 uint64 转成 int64、高位为 1 时报错),浮点 → float64,bool,string,指针解引用。
//
// 刻意没复刻的两处(本 store 的实参走不到):驱动对 Valuer 走 callValuerValue,nil 指针 Valuer 回 nil 而不是
// 调 Value();以及 reflect.Slice 分支(json.RawMessage / 非 byte 切片的专门报错)。
func mysqlLikeConvert(v any) (driver.Value, error) {
	if driver.IsValue(v) {
		return v, nil
	}
	if vr, ok := v.(driver.Valuer); ok {
		sv, err := vr.Value()
		if err != nil {
			return nil, err
		}
		if driver.IsValue(sv) {
			return sv, nil
		}
		if u, ok := sv.(uint64); ok {
			return u, nil
		}
		return nil, fmt.Errorf("non-Value type %T returned from Value", sv)
	}
	rv := reflect.ValueOf(v)
	switch rv.Kind() {
	case reflect.Pointer:
		if rv.IsNil() {
			return nil, nil
		}
		return mysqlLikeConvert(rv.Elem().Interface())
	case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
		return rv.Int(), nil
	case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64:
		return rv.Uint(), nil
	case reflect.Float32, reflect.Float64:
		return rv.Float(), nil
	case reflect.Bool:
		return rv.Bool(), nil
	case reflect.String:
		return rv.String(), nil
	}
	return nil, fmt.Errorf("unsupported type %T, a %s", v, rv.Kind())
}

// ── 应答的小工具 ───────────────────────────────────────────────────────────────

// dv32 / dv64 / dvStr 把「0 / 空串 = NULL」翻成驱动吐出的列值(go-sql-driver 对小整数吐 int64)。
func dv32(v uint32) driver.Value {
	if v == 0 {
		return nil
	}
	return int64(v)
}

func dv64(v uint64) driver.Value {
	if v == 0 {
		return nil
	}
	return int64(v)
}

func dvStr(s string) driver.Value {
	if s == "" {
		return nil
	}
	return s
}

const (
	fakeRouteCreatedMs  int64 = 1_760_000_000_000
	fakeRouteUpdatedMs  int64 = 1_760_000_000_500
	fakeRouteOldWriter        = "data_service@old"
	fakeRouteFenceRunID       = "102-7-20261012T000000Z-deadbeef"
)

// r5Reply 是 R5 读到的一行(列序同 routeRowColumnsSQL)。
func r5Reply(home, storage uint32, version uint64, frozenRun string, rev uint64) fakeRouteReply {
	return fakeRouteReply{rows: [][]driver.Value{{
		dv32(home), dv32(storage), dv64(version), dvStr(frozenRun), int64(rev), fakeRouteOldWriter, fakeRouteCreatedMs, fakeRouteUpdatedMs,
	}}}
}

// routeRowValues 是 R8 / R12 / R13 读到的一行(最前面多两列聚簇键 route_shard, player_id;片按正确的算法给)。
func routeRowValues(player uint64, home, storage uint32, version uint64, frozenRun string, rev uint64) []driver.Value {
	return routeRowValuesWithShard(uint64(placement.RouteRowShard(player)), player, home, storage, version, frozenRun, rev)
}

// routeRowValuesWithShard 同 routeRowValues,但 route_shard 由用例指定:造「库里存的片与 player_id 不符」的行。
func routeRowValuesWithShard(shard, player uint64, home, storage uint32, version uint64, frozenRun string, rev uint64) []driver.Value {
	return []driver.Value{
		int64(shard), int64(player), dv32(home), dv32(storage), dv64(version), dvStr(frozenRun), int64(rev),
		fakeRouteOldWriter, fakeRouteCreatedMs, fakeRouteUpdatedMs,
	}
}

var (
	replyWritable  = fakeRouteReply{rows: [][]driver.Value{{int64(1)}}}
	replyFenceOpen = fakeRouteReply{rows: [][]driver.Value{{int64(0), nil}}}
	replyNoRows    = fakeRouteReply{}
)

func replyAffected(n int64) fakeRouteReply { return fakeRouteReply{affected: n} }

func replyErr(err error) fakeRouteReply { return fakeRouteReply{err: err} }

// ── W1:RegisterHome 的终局契约 ─────────────────────────────────────────────────

const (
	// contractPlayer:4242 % 8 = 4242 % 16 = 2 —— 本常量**区分不了**围栏片与行片(两者恰好相等),
	// 绑错片的实现在用它的用例里照样全绿;区分两者的是 TestRegisterHomeBindsRowShardNotFenceShard(4250)。
	contractPlayer  uint64 = 4242
	contractHome    uint32 = 7
	contractStorage uint32 = 1000000
)

// wantRegisterArgs 是每条 W1 语句应有的实参(经 mysqlLikeConvert:无符号整数一律 uint64,NULL 为 nil)。
// rowRev 是 R5 读到的 rev(R6 的 CAS 条件)。
// 两个「片」分开算:fenceShard 是围栏片(player_id % FenceShards,只进 R2 / R3),
// rowShard 是 player_route 的行片(player_id % 16,进 R4 / R5 / R6)。
func wantRegisterArgs(t *testing.T, label string, req RouteRegisterRequest, rowRev uint64) []driver.Value {
	t.Helper()
	fenceShard := req.PlayerID % uint64(fakeRouteShards)
	rowShard := uint64(placement.RouteRowShard(req.PlayerID))
	var storage, version driver.Value
	if req.StorageID != 0 {
		storage, version = uint64(req.StorageID), uint64(1)
	}
	switch label {
	case "BEGIN", "COMMIT", "ROLLBACK", "R1":
		return nil
	case "R2", "R3":
		return []driver.Value{uint64(req.HomeZone), fenceShard}
	case "R4":
		return []driver.Value{rowShard, req.PlayerID, uint64(req.HomeZone), storage, version, fakeRouteWriter}
	case "R5":
		return []driver.Value{rowShard, req.PlayerID}
	case "R6a":
		return []driver.Value{uint64(req.HomeZone), fakeRouteWriter, rowShard, req.PlayerID, rowRev}
	case "R6b":
		return []driver.Value{uint64(req.HomeZone), uint64(req.StorageID), fakeRouteWriter, rowShard, req.PlayerID, rowRev}
	default:
		t.Fatalf("W1 不该发出 %s", label)
		return nil
	}
}

func assertRegisterArgs(t *testing.T, fake *fakeRouteDB, req RouteRegisterRequest, rowRev uint64) {
	t.Helper()
	for i, c := range fake.snapshot() {
		want := wantRegisterArgs(t, c.label, req, rowRev)
		if len(want) == 0 && len(c.args) == 0 {
			continue
		}
		if !reflect.DeepEqual(c.args, want) {
			t.Fatalf("第 %d 次调用 %s 的实参不对:\n got=%#v\nwant=%#v", i, c.label, c.args, want)
		}
	}
}

// TestRegisterHomeOutcomeContract 钉住「库回什么 → RegisterHome 回什么、发了哪些语句」,一格都不许动:
// 结局决定路由层发不发布、回哪个 gRPC 码(规格 c.1 / c.3)。
func TestRegisterHomeOutcomeContract(t *testing.T) {
	withStorage := RouteRegisterRequest{PlayerID: contractPlayer, HomeZone: contractHome, StorageID: contractStorage}
	noStorage := RouteRegisterRequest{PlayerID: contractPlayer, HomeZone: contractHome}
	known := RouteRegisterRequest{PlayerID: contractPlayer, HomeZone: contractHome, StorageID: contractStorage, ExistingRowOnly: true}
	boom := errors.New("connection reset during commit")

	for _, tc := range []struct {
		name         string
		req          RouteRegisterRequest
		replies      map[string][]fakeRouteReply
		commitErrs   []error
		rowRev       uint64
		wantOutcome  RouteRegisterOutcome
		wantRow      *RouteRow
		wantExisting uint32
		wantRunID    string
		wantErr      error // 非 nil:errors.Is
		anyErr       bool  // 只要求报错(没有专门的哨兵)
		wantLabels   []string
	}{
		{
			name: "R4插入1行_Inserted",
			req:  withStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyAffected(1)},
			},
			wantOutcome: RouteRegisterInserted,
			wantRow: &RouteRow{PlayerID: contractPlayer, HomeZone: contractHome, StorageID: contractStorage,
				PlacementVersion: 1, Rev: 1, UpdatedBy: fakeRouteWriter},
			wantLabels: []string{"BEGIN", "R1", "R2", "R4", "COMMIT"},
		},
		{
			name: "R4插入1行_不带落点",
			req:  noStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyAffected(1)},
			},
			wantOutcome: RouteRegisterInserted,
			wantRow:     &RouteRow{PlayerID: contractPlayer, HomeZone: contractHome, Rev: 1, UpdatedBy: fakeRouteWriter},
			wantLabels:  []string{"BEGIN", "R1", "R2", "R4", "COMMIT"},
		},
		{
			name: "R4撞键_R5同home_AlreadySame_回滚",
			req:  withStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyAffected(0)},
				"R5": {r5Reply(contractHome, contractStorage, 1, "", 3)},
			},
			wantOutcome: RouteRegisterAlreadySame,
			wantRow: &RouteRow{PlayerID: contractPlayer, HomeZone: contractHome, StorageID: contractStorage, PlacementVersion: 1,
				Rev: 3, UpdatedBy: fakeRouteOldWriter, CreatedAtMs: uint64(fakeRouteCreatedMs), UpdatedAtMs: uint64(fakeRouteUpdatedMs)},
			wantLabels: []string{"BEGIN", "R1", "R2", "R4", "R5", "ROLLBACK"},
		},
		{
			name: "R4受影响2行_fail_closed",
			req:  withStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyAffected(2)},
			},
			anyErr:     true,
			wantLabels: []string{"BEGIN", "R1", "R2", "R4", "ROLLBACK"},
		},
		{
			name: "R4报CHECK违例_不重试",
			req:  withStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyErr(mysqlErrNo(3819))},
			},
			anyErr:     true,
			wantLabels: []string{"BEGIN", "R1", "R2", "R4", "ROLLBACK"},
		},
		{
			name: "ExistingRowOnly_R5缺行_RowMissing_不插",
			req:  known,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R5": {replyNoRows},
			},
			wantOutcome: RouteRegisterRowMissing,
			wantRow:     &RouteRow{},
			wantLabels:  []string{"BEGIN", "R1", "R2", "R5", "ROLLBACK"},
		},
		{
			name: "R4撞键后R5缺行_行消失",
			req:  withStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyAffected(0)}, "R5": {replyNoRows},
			},
			wantErr:    ErrRouteRowVanished,
			wantLabels: []string{"BEGIN", "R1", "R2", "R4", "R5", "ROLLBACK"},
		},
		{
			name: "ExistingRowOnly_R5同home_AlreadySame",
			req:  known,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R5": {r5Reply(contractHome, 0, 0, "", 2)},
			},
			wantOutcome: RouteRegisterAlreadySame,
			wantRow: &RouteRow{PlayerID: contractPlayer, HomeZone: contractHome, Rev: 2, UpdatedBy: fakeRouteOldWriter,
				CreatedAtMs: uint64(fakeRouteCreatedMs), UpdatedAtMs: uint64(fakeRouteUpdatedMs)},
			wantLabels: []string{"BEGIN", "R1", "R2", "R5", "ROLLBACK"},
		},
		{
			name: "R5异home_Conflict_带既有值",
			req:  withStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyAffected(0)},
				"R5": {r5Reply(9, contractStorage, 1, "", 3)},
			},
			wantOutcome:  RouteRegisterConflict,
			wantRow:      &RouteRow{},
			wantExisting: 9,
			wantLabels:   []string{"BEGIN", "R1", "R2", "R4", "R5", "ROLLBACK"},
		},
		{
			name: "home为NULL_落点为NULL_请求带storage_R6b",
			req:  withStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyAffected(0)},
				"R5": {r5Reply(0, 0, 0, "", 4)}, "R6b": {replyAffected(1)},
			},
			rowRev:      4,
			wantOutcome: RouteRegisterFilledHome,
			wantRow: &RouteRow{PlayerID: contractPlayer, HomeZone: contractHome, StorageID: contractStorage, PlacementVersion: 1,
				Rev: 5, UpdatedBy: fakeRouteWriter, CreatedAtMs: uint64(fakeRouteCreatedMs)},
			wantLabels: []string{"BEGIN", "R1", "R2", "R4", "R5", "R6b", "COMMIT"},
		},
		{
			name: "home为NULL_已有落点_请求带storage_R6a_落点保留",
			req:  withStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyAffected(0)},
				"R5": {r5Reply(0, 55, 3, "", 4)}, "R6a": {replyAffected(1)},
			},
			rowRev:      4,
			wantOutcome: RouteRegisterFilledHomePlacementKept,
			wantRow: &RouteRow{PlayerID: contractPlayer, HomeZone: contractHome, StorageID: 55, PlacementVersion: 3,
				Rev: 5, UpdatedBy: fakeRouteWriter, CreatedAtMs: uint64(fakeRouteCreatedMs)},
			wantLabels: []string{"BEGIN", "R1", "R2", "R4", "R5", "R6a", "COMMIT"},
		},
		{
			name: "home为NULL_冻结落点_请求带storage_R6a_冻结态原样带回",
			req:  withStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyAffected(0)},
				"R5": {r5Reply(0, 55, 3, "run-x", 4)}, "R6a": {replyAffected(1)},
			},
			rowRev:      4,
			wantOutcome: RouteRegisterFilledHomePlacementKept,
			wantRow: &RouteRow{PlayerID: contractPlayer, HomeZone: contractHome, StorageID: 55, PlacementVersion: 3, FrozenRunID: "run-x",
				Rev: 5, UpdatedBy: fakeRouteWriter, CreatedAtMs: uint64(fakeRouteCreatedMs)},
			wantLabels: []string{"BEGIN", "R1", "R2", "R4", "R5", "R6a", "COMMIT"},
		},
		{
			name: "home为NULL_请求不带storage_R6a_FilledHome",
			req:  noStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyAffected(0)},
				"R5": {r5Reply(0, 55, 3, "", 4)}, "R6a": {replyAffected(1)},
			},
			rowRev:      4,
			wantOutcome: RouteRegisterFilledHome,
			wantRow: &RouteRow{PlayerID: contractPlayer, HomeZone: contractHome, StorageID: 55, PlacementVersion: 3,
				Rev: 5, UpdatedBy: fakeRouteWriter, CreatedAtMs: uint64(fakeRouteCreatedMs)},
			wantLabels: []string{"BEGIN", "R1", "R2", "R4", "R5", "R6a", "COMMIT"},
		},
		{
			name: "R6a受影响0行_fail_closed",
			req:  noStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyAffected(0)},
				"R5": {r5Reply(0, 0, 0, "", 4)}, "R6a": {replyAffected(0)},
			},
			rowRev:     4,
			anyErr:     true,
			wantLabels: []string{"BEGIN", "R1", "R2", "R4", "R5", "R6a", "ROLLBACK"},
		},
		{
			name: "R6b受影响2行_fail_closed",
			req:  withStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyAffected(0)},
				"R5": {r5Reply(0, 0, 0, "", 4)}, "R6b": {replyAffected(2)},
			},
			rowRev:     4,
			anyErr:     true,
			wantLabels: []string{"BEGIN", "R1", "R2", "R4", "R5", "R6b", "ROLLBACK"},
		},
		{
			name: "R2缺分片行_事务外R3补行_重开后插入",
			req:  withStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable, replyWritable}, "R2": {replyNoRows, replyFenceOpen},
				"R3": {replyAffected(1)}, "R4": {replyAffected(1)},
			},
			wantOutcome: RouteRegisterInserted,
			wantRow: &RouteRow{PlayerID: contractPlayer, HomeZone: contractHome, StorageID: contractStorage,
				PlacementVersion: 1, Rev: 1, UpdatedBy: fakeRouteWriter},
			wantLabels: []string{"BEGIN", "R1", "R2", "ROLLBACK", "R3", "BEGIN", "R1", "R2", "R4", "COMMIT"},
		},
		{
			name: "R2补行后仍缺_分片行消失",
			req:  withStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable, replyWritable}, "R2": {replyNoRows, replyNoRows}, "R3": {replyAffected(0)},
			},
			wantErr:    ErrRouteFenceShardVanished,
			wantLabels: []string{"BEGIN", "R1", "R2", "ROLLBACK", "R3", "BEGIN", "R1", "R2", "ROLLBACK"},
		},
		{
			name: "围栏生效_Fenced带run_id_零写入",
			req:  withStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {{rows: [][]driver.Value{{int64(1), fakeRouteFenceRunID}}}},
			},
			wantOutcome: RouteRegisterFenced,
			wantRow:     &RouteRow{},
			wantRunID:   fakeRouteFenceRunID,
			wantLabels:  []string{"BEGIN", "R1", "R2", "ROLLBACK"},
		},
		{
			name: "围栏active为NULL_按生效处理",
			req:  withStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {{rows: [][]driver.Value{{nil, "half-written"}}}},
			},
			wantOutcome: RouteRegisterFenced,
			wantRow:     &RouteRow{},
			wantRunID:   "half-written",
			wantLabels:  []string{"BEGIN", "R1", "R2", "ROLLBACK"},
		},
		{
			name:       "meta行缺失",
			req:        withStorage,
			replies:    map[string][]fakeRouteReply{"R1": {replyNoRows}},
			wantErr:    ErrRouteMetaMissing,
			wantLabels: []string{"BEGIN", "R1", "ROLLBACK"},
		},
		{
			name:       "writable为0_拒写",
			req:        withStorage,
			replies:    map[string][]fakeRouteReply{"R1": {{rows: [][]driver.Value{{int64(0)}}}}},
			wantErr:    ErrRouteNotWritable,
			wantLabels: []string{"BEGIN", "R1", "ROLLBACK"},
		},
		{
			name: "COMMIT失败_结果不确定_按错误返回不重试",
			req:  withStorage,
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyAffected(1)},
			},
			commitErrs: []error{boom},
			wantErr:    boom,
			wantLabels: []string{"BEGIN", "R1", "R2", "R4", "COMMIT"},
		},
	} {
		t.Run(tc.name, func(t *testing.T) {
			st, fake := newFakeRouteStore(t, fakeRouteScript{replies: tc.replies, commitErrs: tc.commitErrs})

			res, err := st.RegisterHome(context.Background(), tc.req)
			switch {
			case tc.wantErr != nil:
				if !errors.Is(err, tc.wantErr) {
					t.Fatalf("err = %v, want errors.Is %v (outcome=%s)", err, tc.wantErr, res.Outcome)
				}
			case tc.anyErr:
				if err == nil {
					t.Fatalf("want error, got outcome=%s row=%+v", res.Outcome, res.Row)
				}
			default:
				if err != nil {
					t.Fatalf("unexpected error: %v", err)
				}
				if res.Outcome != tc.wantOutcome {
					t.Fatalf("outcome = %s, want %s", res.Outcome, tc.wantOutcome)
				}
				if tc.wantRow != nil && res.Row != *tc.wantRow {
					t.Fatalf("row = %+v\nwant %+v", res.Row, *tc.wantRow)
				}
				if res.ExistingHome != tc.wantExisting {
					t.Fatalf("ExistingHome = %d, want %d(只有 Conflict 才带)", res.ExistingHome, tc.wantExisting)
				}
				if res.FenceRunID != tc.wantRunID {
					t.Fatalf("FenceRunID = %q, want %q(只有 Fenced 才带)", res.FenceRunID, tc.wantRunID)
				}
			}
			if got := fake.labels(); !reflect.DeepEqual(got, tc.wantLabels) {
				t.Fatalf("语句序列不对:\n got=%v\nwant=%v", got, tc.wantLabels)
			}
			assertRegisterArgs(t, fake, tc.req, tc.rowRev)
		})
	}
}

// TestRegisterHomeBindsNullForAbsentPlacement:不带落点时 R4 的 storage / version 两个实参都是 NULL(nil),
// 带落点时是 uint64。绑定统一走 nullU32 / nullU64(「0 = NULL」的领域约定),不用 sql.Null[T]:后者的 Value()
// 随 Go 版本变化(见 route_store.go nullU32 的注释),这里只钉住生产代码实际交给驱动的形状。
func TestRegisterHomeBindsNullForAbsentPlacement(t *testing.T) {
	for _, tc := range []struct {
		name                     string
		storage                  uint32
		wantStorage, wantVersion driver.Value
	}{
		{"不带落点", 0, nil, nil},
		{"带落点", contractStorage, uint64(contractStorage), uint64(1)},
	} {
		t.Run(tc.name, func(t *testing.T) {
			st, fake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyAffected(1)},
			}})
			if _, err := st.RegisterHome(context.Background(),
				RouteRegisterRequest{PlayerID: contractPlayer, HomeZone: contractHome, StorageID: tc.storage}); err != nil {
				t.Fatalf("unexpected error: %v", err)
			}
			r4 := fake.callsWithLabel("R4")
			if len(r4) != 1 || len(r4[0].args) != 6 {
				t.Fatalf("R4 调用 = %+v, want 一次、6 个实参(行片在最前)", r4)
			}
			if r4[0].args[3] != tc.wantStorage || r4[0].args[4] != tc.wantVersion {
				t.Fatalf("R4 的 storage / version 实参 = (%#v, %#v), want (%#v, %#v)",
					r4[0].args[3], r4[0].args[4], tc.wantStorage, tc.wantVersion)
			}
		})
	}

	// nullU32 / nullU64 的产物在驱动的转换规则下原样放行:非零是 uint64(不经 int64,高位为 1 也不报错),零是 NULL。
	if v, err := mysqlLikeConvert(nullU32(7)); err != nil || v != uint64(7) {
		t.Fatalf("nullU32(7) 转换后 = (%#v, %v), want uint64(7)", v, err)
	}
	if v, err := mysqlLikeConvert(nullU64(0)); err != nil || v != nil {
		t.Fatalf("nullU64(0) 转换后 = (%#v, %v), want nil", v, err)
	}
}

// TestRegisterHomeBindsRowShardNotFenceShard:R2 / R3 绑围栏片(player_id % FenceShards),R4 / R5 / R6 绑行片
// (player_id % 16)。两者绑反了的后果:R4 对一半新号撞 chk_route_shard(3819)建角失败;R5 / R6 按错片点查读不到行
// → ErrRouteRowVanished。contractPlayer=4242 的两种片恰好相等,区分不了,这里用 4250(围栏片 2、行片 10)。
func TestRegisterHomeBindsRowShardNotFenceShard(t *testing.T) {
	const player uint64 = 4250
	const wantFence, wantRow = uint64(2), uint64(10)
	// 先确认用例本身没有退化:两种片必须不同,否则下面的断言什么也证明不了。
	probe := RouteRegisterRequest{PlayerID: player, HomeZone: contractHome}
	fenceArg := wantRegisterArgs(t, "R2", probe, 0)[1]
	rowArg := wantRegisterArgs(t, "R5", probe, 0)[0]
	if fenceArg != wantFence || rowArg != wantRow {
		t.Fatalf("wantRegisterArgs 算出的围栏片 / 行片 = (%#v, %#v), want (%d, %d)", fenceArg, rowArg, wantFence, wantRow)
	}
	if fenceArg == rowArg {
		t.Fatal("用例退化:围栏片与行片相等,区分不了绑错片的实现")
	}

	for _, tc := range []struct {
		name       string
		req        RouteRegisterRequest
		replies    map[string][]fakeRouteReply
		rowRev     uint64
		wantLabels []string
		// rowShardAt 是行片在各语句实参里的下标。
		rowShardAt map[string]int
	}{
		{
			name: "新插",
			req:  RouteRegisterRequest{PlayerID: player, HomeZone: contractHome, StorageID: contractStorage},
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyAffected(1)},
			},
			wantLabels: []string{"BEGIN", "R1", "R2", "R4", "COMMIT"},
			rowShardAt: map[string]int{"R4": 0},
		},
		{
			name: "补home",
			req:  RouteRegisterRequest{PlayerID: player, HomeZone: contractHome},
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyAffected(0)},
				"R5": {r5Reply(0, 55, 3, "", 4)}, "R6a": {replyAffected(1)},
			},
			rowRev:     4,
			wantLabels: []string{"BEGIN", "R1", "R2", "R4", "R5", "R6a", "COMMIT"},
			rowShardAt: map[string]int{"R4": 0, "R5": 0, "R6a": 2},
		},
		{
			name: "补home与落点",
			req:  RouteRegisterRequest{PlayerID: player, HomeZone: contractHome, StorageID: contractStorage},
			replies: map[string][]fakeRouteReply{
				"R1": {replyWritable}, "R2": {replyFenceOpen}, "R4": {replyAffected(0)},
				"R5": {r5Reply(0, 0, 0, "", 4)}, "R6b": {replyAffected(1)},
			},
			rowRev:     4,
			wantLabels: []string{"BEGIN", "R1", "R2", "R4", "R5", "R6b", "COMMIT"},
			rowShardAt: map[string]int{"R4": 0, "R5": 0, "R6b": 3},
		},
	} {
		t.Run(tc.name, func(t *testing.T) {
			st, fake := newFakeRouteStore(t, fakeRouteScript{replies: tc.replies})
			if _, err := st.RegisterHome(context.Background(), tc.req); err != nil {
				t.Fatalf("unexpected error: %v", err)
			}
			if got := fake.labels(); !reflect.DeepEqual(got, tc.wantLabels) {
				t.Fatalf("语句序列不对:\n got=%v\nwant=%v", got, tc.wantLabels)
			}
			r2 := fake.callsWithLabel("R2")
			if len(r2) != 1 || len(r2[0].args) != 2 || r2[0].args[1] != wantFence {
				t.Fatalf("R2 实参 = %+v, want 围栏片 %d 在下标 1", r2, wantFence)
			}
			for label, idx := range tc.rowShardAt {
				calls := fake.callsWithLabel(label)
				if len(calls) != 1 || len(calls[0].args) <= idx || calls[0].args[idx] != wantRow {
					t.Fatalf("%s 实参 = %+v, want 行片 %d 在下标 %d(绑成了围栏片 %d?)", label, calls, wantRow, idx, wantFence)
				}
			}
			assertRegisterArgs(t, fake, tc.req, tc.rowRev)
		})
	}
}

// TestRegisterHomeRejectsInvalidArgsBeforeAnyStatement:入参非法时一条语句都不许发(连 BEGIN 都没有)。
func TestRegisterHomeRejectsInvalidArgsBeforeAnyStatement(t *testing.T) {
	for _, tc := range []struct {
		name string
		req  RouteRegisterRequest
	}{
		{"player_id为0", RouteRegisterRequest{PlayerID: 0, HomeZone: contractHome}},
		{"home为0", RouteRegisterRequest{PlayerID: contractPlayer, HomeZone: 0}},
		{"home越界", RouteRegisterRequest{PlayerID: contractPlayer, HomeZone: RouteHomeZoneMax + 1}},
	} {
		t.Run(tc.name, func(t *testing.T) {
			st, fake := newFakeRouteStore(t, fakeRouteScript{})
			if _, err := st.RegisterHome(context.Background(), tc.req); !errors.Is(err, ErrRouteInvalidArgument) {
				t.Fatalf("err = %v, want ErrRouteInvalidArgument", err)
			}
			if got := fake.labels(); len(got) != 0 {
				t.Fatalf("入参非法却发了语句:%v", got)
			}
		})
	}
}

// TestRegisterHomeRetriesDeadlockAndWriteConflict:1213(InnoDB 死锁)与 9007(TiDB 写冲突)整事务已被回滚,
// 退避后重来并计数;用尽预算按存储错误返回,errors.As 仍能取到最后一次的 MySQLError。
func TestRegisterHomeRetriesDeadlockAndWriteConflict(t *testing.T) {
	req := RouteRegisterRequest{PlayerID: contractPlayer, HomeZone: contractHome, StorageID: contractStorage}

	t.Run("1213与9007各一次后成功", func(t *testing.T) {
		st, fake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
			"R1": {replyWritable, replyWritable, replyWritable},
			"R2": {replyFenceOpen, replyFenceOpen, replyFenceOpen},
			"R4": {replyErr(mysqlErrNo(mysqlErrDeadlock)), replyErr(mysqlErrNo(tidbErrWriteConflict)), replyAffected(1)},
		}})
		res, err := st.RegisterHome(context.Background(), req)
		if err != nil || res.Outcome != RouteRegisterInserted {
			t.Fatalf("RegisterHome = (%s, %v), want Inserted", res.Outcome, err)
		}
		if got := st.LockRetriesForTest(); got != 2 {
			t.Fatalf("lockRetries = %d, want 2", got)
		}
		want := []string{
			"BEGIN", "R1", "R2", "R4", "ROLLBACK",
			"BEGIN", "R1", "R2", "R4", "ROLLBACK",
			"BEGIN", "R1", "R2", "R4", "COMMIT",
		}
		if got := fake.labels(); !reflect.DeepEqual(got, want) {
			t.Fatalf("语句序列不对:\n got=%v\nwant=%v", got, want)
		}
	})

	t.Run("用尽预算", func(t *testing.T) {
		deadlocks := make([]fakeRouteReply, routeTxAttempts)
		writable := make([]fakeRouteReply, routeTxAttempts)
		fences := make([]fakeRouteReply, routeTxAttempts)
		for i := range deadlocks {
			deadlocks[i] = replyErr(mysqlErrNo(mysqlErrDeadlock))
			writable[i] = replyWritable
			fences[i] = replyFenceOpen
		}
		st, fake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
			"R1": writable, "R2": fences, "R4": deadlocks,
		}})
		_, err := st.RegisterHome(context.Background(), req)
		var me *mysql.MySQLError
		if !errors.As(err, &me) || me.Number != mysqlErrDeadlock {
			t.Fatalf("err = %v, want 包着 1213 的存储错误", err)
		}
		if got := len(fake.callsWithLabel("R4")); got != routeTxAttempts {
			t.Fatalf("R4 发了 %d 次,want %d", got, routeTxAttempts)
		}
		if got := st.LockRetriesForTest(); got != routeTxAttempts-1 {
			t.Fatalf("lockRetries = %d, want %d", got, routeTxAttempts-1)
		}
	})
}

// TestRegisterHomeDoesNotRetryLockWaitTimeout:1205 已经等满了封顶的 1s,原地再等只会吃掉服务端剩下的预算,
// 结论不会变 —— 只发一次,原样按存储错误返回(guild 先例)。
func TestRegisterHomeDoesNotRetryLockWaitTimeout(t *testing.T) {
	st, fake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
		"R1": {replyWritable, replyWritable}, "R2": {replyFenceOpen, replyFenceOpen},
		"R4": {replyErr(mysqlErrNo(mysqlErrLockWaitTimeout)), replyAffected(1)},
	}})
	_, err := st.RegisterHome(context.Background(),
		RouteRegisterRequest{PlayerID: contractPlayer, HomeZone: contractHome, StorageID: contractStorage})
	var me *mysql.MySQLError
	if !errors.As(err, &me) || me.Number != mysqlErrLockWaitTimeout {
		t.Fatalf("err = %v, want 包着 1205 的存储错误", err)
	}
	if got := len(fake.callsWithLabel("R4")); got != 1 {
		t.Fatalf("1205 被重试了:R4 发了 %d 次", got)
	}
	if got := st.LockRetriesForTest(); got != 0 {
		t.Fatalf("lockRetries = %d, want 0", got)
	}
}

// TestRegisterHomeRetryStopsOnCanceledContext:预算还在但 ctx 已结束时,退避必须立刻返回、不再开第二个事务。
func TestRegisterHomeRetryStopsOnCanceledContext(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	st, fake := newFakeRouteStore(t, fakeRouteScript{
		replies: map[string][]fakeRouteReply{
			// 第二轮的应答刻意留着:真的重来了就能看出来。
			"R1": {replyWritable, replyWritable}, "R2": {replyFenceOpen, replyFenceOpen},
			"R4": {replyErr(mysqlErrNo(mysqlErrDeadlock)), replyAffected(1)},
		},
		onRollback: cancel,
	})
	_, err := st.RegisterHome(ctx, RouteRegisterRequest{PlayerID: contractPlayer, HomeZone: contractHome})
	if !errors.Is(err, context.Canceled) {
		t.Fatalf("err = %v, want 带 context.Canceled", err)
	}
	if got := len(fake.callsWithLabel("BEGIN")); got != 1 {
		t.Fatalf("ctx 取消后仍在重试:BEGIN 发了 %d 次", got)
	}
	if got := st.LockRetriesForTest(); got != 1 {
		t.Fatalf("lockRetries = %d, want 1(这一次锁冲突要计数,只是退避被 ctx 打断)", got)
	}
}

// ── 导入 ─────────────────────────────────────────────────────────────────────────

// importContractRows 一批覆盖五种判定的导入行(player_id 升序)。
func importContractRows() []RouteImportRow {
	stable := func(s uint32, v uint64) *placement.Record { return &placement.Record{StorageID: s, Version: v} }
	return []RouteImportRow{
		{PlayerID: 10, HomeZone: 7, Placement: stable(1000000, 1), MapRev: 5}, // 库无行 → 插入,rev=5
		{PlayerID: 20, HomeZone: 7, Placement: stable(7, 1)},                  // 库 home 为 NULL → 补列
		{PlayerID: 30, HomeZone: 7},                                           // 相同 → match
		{PlayerID: 40, HomeZone: 7},                                           // 库 home=8 → conflict,不写
		{PlayerID: 50, HomeZone: 7, MapRev: 9},                                // 库 rev=2 < maprev → rev_regression,不写
	}
}

// importContractExisting 是 R8 读到的已有行(对应 importContractRows 的 20..50)。
func importContractExisting() [][]driver.Value {
	return [][]driver.Value{
		routeRowValues(20, 0, 7, 1, "", 2),
		routeRowValues(30, 7, 0, 0, "", 3),
		routeRowValues(40, 8, 0, 0, "", 1),
		routeRowValues(50, 7, 0, 0, "", 2),
	}
}

// TestImportRoutesStatementOrderAndAffectedMismatchRetries:R1 → R8(加锁)→ R7 → R9 → COMMIT,
// 冲突与 rev 倒退的行一字不写;R7 行数对不上(R8 之后有并发插入)时整批回滚重来,不计锁冲突。
// R8 的实参是按聚簇键 (route_shard, player_id) 升序交错的对:10..50 的行片是 10 / 4 / 14 / 8 / 2。
func TestImportRoutesStatementOrderAndAffectedMismatchRetries(t *testing.T) {
	ids := []driver.Value{
		uint64(2), uint64(50),
		uint64(4), uint64(20),
		uint64(8), uint64(40),
		uint64(10), uint64(10),
		uint64(14), uint64(30),
	}

	t.Run("语句顺序与实参", func(t *testing.T) {
		st, fake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
			"R1": {replyWritable},
			"R8": {{rows: importContractExisting()}},
			"R7": {replyAffected(1)},
			"R9": {replyAffected(1)},
		}})
		stats, err := st.ImportRoutes(context.Background(), importContractRows(), false)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if stats.Inserted != 1 || stats.Filled != 1 || stats.Matched != 1 || stats.DBAhead != 0 ||
			len(stats.Conflicts) != 1 || len(stats.RevRegressions) != 1 {
			t.Fatalf("stats = %+v, want inserted=1 filled=1 matched=1 conflicts=1 rev_regressions=1", stats)
		}
		if stats.Conflicts[0].PlayerID != 40 || stats.RevRegressions[0].PlayerID != 50 {
			t.Fatalf("冲突 / 倒退归错了人:%+v", stats)
		}
		want := []string{"BEGIN", "R1", "R8", "R7", "R9", "COMMIT"}
		if got := fake.labels(); !reflect.DeepEqual(got, want) {
			t.Fatalf("语句序列不对:\n got=%v\nwant=%v", got, want)
		}

		r8 := fake.callsWithLabel("R8")[0]
		if r8.sql != routeSelectRowsSQL(5, true) || !reflect.DeepEqual(r8.args, ids) {
			t.Fatalf("R8 = %q %#v, want %q %#v", r8.sql, r8.args, routeSelectRowsSQL(5, true), ids)
		}
		r7 := fake.callsWithLabel("R7")[0]
		wantR7 := []driver.Value{uint64(10), uint64(10), uint64(7), uint64(1000000), uint64(1), nil, uint64(5), fakeRouteWriter}
		if r7.sql != routeImportInsertSQL(1) || !reflect.DeepEqual(r7.args, wantR7) {
			t.Fatalf("R7 = %q %#v, want %q %#v(行片在最前;rev 必须是 maprev)", r7.sql, r7.args, routeImportInsertSQL(1), wantR7)
		}
		r9 := fake.callsWithLabel("R9")[0]
		wantR9 := []driver.Value{uint64(7), uint64(7), uint64(1), nil, fakeRouteWriter, uint64(4), uint64(20), uint64(2)}
		if !reflect.DeepEqual(r9.args, wantR9) {
			t.Fatalf("R9 实参 = %#v, want %#v(按聚簇键 (4, 20) 定位、按读到的 rev=2 做 CAS)", r9.args, wantR9)
		}
	})

	t.Run("R7行数不等_回滚后重来", func(t *testing.T) {
		rows := importContractRows()
		rows[0].MapRev = 0 // 重来时那一行已被并发的 W1 以 rev=1 插入,maprev 为 0 才是 match
		reread := append([][]driver.Value{routeRowValues(10, 7, 1000000, 1, "", 1)}, importContractExisting()...)
		st, fake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
			"R1": {replyWritable, replyWritable},
			"R8": {{rows: importContractExisting()}, {rows: reread}},
			"R7": {replyAffected(0)},
			"R9": {replyAffected(1)},
		}})
		stats, err := st.ImportRoutes(context.Background(), rows, false)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if stats.Inserted != 0 || stats.Matched != 2 || stats.Filled != 1 {
			t.Fatalf("stats = %+v, want 第二轮的判定(inserted=0 matched=2 filled=1)", stats)
		}
		want := []string{"BEGIN", "R1", "R8", "R7", "ROLLBACK", "BEGIN", "R1", "R8", "R9", "COMMIT"}
		if got := fake.labels(); !reflect.DeepEqual(got, want) {
			t.Fatalf("语句序列不对:\n got=%v\nwant=%v", got, want)
		}
		if got := st.LockRetriesForTest(); got != 0 {
			t.Fatalf("lockRetries = %d, want 0(并发插入不是锁冲突)", got)
		}
	})

	t.Run("一直对不上_用尽预算报错", func(t *testing.T) {
		writable := make([]fakeRouteReply, routeTxAttempts)
		reads := make([]fakeRouteReply, routeTxAttempts)
		short := make([]fakeRouteReply, routeTxAttempts)
		for i := range writable {
			writable[i], reads[i], short[i] = replyWritable, fakeRouteReply{rows: importContractExisting()}, replyAffected(0)
		}
		st, fake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
			"R1": writable, "R8": reads, "R7": short,
		}})
		if _, err := st.ImportRoutes(context.Background(), importContractRows(), false); !errors.Is(err, errRouteImportRaced) {
			t.Fatalf("err = %v, want errRouteImportRaced", err)
		}
		if got := len(fake.callsWithLabel("R7")); got != routeTxAttempts {
			t.Fatalf("R7 发了 %d 次,want %d", got, routeTxAttempts)
		}
		if got := len(fake.callsWithLabel("R9")); got != 0 {
			t.Fatalf("R7 对不上之后不该再补列:R9 发了 %d 次", got)
		}
	})
}

// TestImportRoutesInsertsAndFillsInRouteKeyOrder:入参按 player_id 升序({2, 17}),写入按聚簇键升序 ——
// 17 的行片是 1、2 的行片是 2,所以 R7 的 VALUES 与 R9 的发出顺序都是 17 在前。任何一次锁多行的语句都按聚簇键
// 升序取锁(与 R8 同序),两个并发导入之间才不成环。
func TestImportRoutesInsertsAndFillsInRouteKeyOrder(t *testing.T) {
	rows := []RouteImportRow{{PlayerID: 2, HomeZone: 7}, {PlayerID: 17, HomeZone: 7}}

	t.Run("两行都新插_R7按聚簇键排", func(t *testing.T) {
		st, fake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
			"R1": {replyWritable}, "R8": {replyNoRows}, "R7": {replyAffected(2)},
		}})
		stats, err := st.ImportRoutes(context.Background(), rows, false)
		if err != nil || stats.Inserted != 2 {
			t.Fatalf("ImportRoutes = (%+v, %v), want inserted=2", stats, err)
		}
		r8 := fake.callsWithLabel("R8")[0]
		if want := []driver.Value{uint64(1), uint64(17), uint64(2), uint64(2)}; !reflect.DeepEqual(r8.args, want) {
			t.Fatalf("R8 实参 = %#v, want %#v(按聚簇键升序)", r8.args, want)
		}
		r7 := fake.callsWithLabel("R7")[0]
		want := []driver.Value{
			uint64(1), uint64(17), uint64(7), nil, nil, nil, uint64(1), fakeRouteWriter,
			uint64(2), uint64(2), uint64(7), nil, nil, nil, uint64(1), fakeRouteWriter,
		}
		if r7.sql != routeImportInsertSQL(2) || !reflect.DeepEqual(r7.args, want) {
			t.Fatalf("R7 = %q %#v\nwant %q %#v(17 的行片 1 < 2 的行片 2,必须排在前面)", r7.sql, r7.args, routeImportInsertSQL(2), want)
		}
	})

	t.Run("两行都补列_R9按聚簇键发", func(t *testing.T) {
		st, fake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
			"R1": {replyWritable},
			// 库里两行都是 home 为 NULL 的墓碑(应答顺序随意,判定按 player_id 取)。
			"R8": {{rows: [][]driver.Value{routeRowValues(2, 0, 0, 0, "", 3), routeRowValues(17, 0, 0, 0, "", 5)}}},
			"R9": {replyAffected(1), replyAffected(1)},
		}})
		stats, err := st.ImportRoutes(context.Background(), rows, false)
		if err != nil || stats.Filled != 2 {
			t.Fatalf("ImportRoutes = (%+v, %v), want filled=2", stats, err)
		}
		if got, want := fake.labels(), []string{"BEGIN", "R1", "R8", "R9", "R9", "COMMIT"}; !reflect.DeepEqual(got, want) {
			t.Fatalf("语句序列不对:\n got=%v\nwant=%v", got, want)
		}
		r9 := fake.callsWithLabel("R9")
		want := [][]driver.Value{
			{uint64(7), nil, nil, nil, fakeRouteWriter, uint64(1), uint64(17), uint64(5)},
			{uint64(7), nil, nil, nil, fakeRouteWriter, uint64(2), uint64(2), uint64(3)},
		}
		for i := range want {
			if !reflect.DeepEqual(r9[i].args, want[i]) {
				t.Fatalf("第 %d 次 R9 实参 = %#v, want %#v(补列按聚簇键升序:17 在前)", i, r9[i].args, want[i])
			}
		}
	})
}

// TestImportRoutesDryRunIssuesNoWriteAndNoLockingRead:dry-run 只发一条不加锁的读,不开事务,统计与真跑相同。
func TestImportRoutesDryRunIssuesNoWriteAndNoLockingRead(t *testing.T) {
	st, fake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
		"R8ro": {{rows: importContractExisting()}},
	}})
	stats, err := st.ImportRoutes(context.Background(), importContractRows(), true)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if stats.Inserted != 1 || stats.Filled != 1 || stats.Matched != 1 || len(stats.Conflicts) != 1 || len(stats.RevRegressions) != 1 {
		t.Fatalf("dry-run stats = %+v", stats)
	}
	if got := fake.labels(); !reflect.DeepEqual(got, []string{"R8ro"}) {
		t.Fatalf("dry-run 发了 %v,want 只有一条不加锁的读", got)
	}
	if got := fake.callsWithLabel("R8ro")[0].sql; got != routeSelectRowsSQL(5, false) || strings.Contains(got, "FOR UPDATE") {
		t.Fatalf("dry-run 的读 = %q", got)
	}
}

// TestImportRoutesRejectsInvalidBatchBeforeAnyStatement:批次不合法一条语句都不发。
func TestImportRoutesRejectsInvalidBatchBeforeAnyStatement(t *testing.T) {
	st, fake := newFakeRouteStore(t, fakeRouteScript{})
	bad := []RouteImportRow{{PlayerID: 2, HomeZone: 7}, {PlayerID: 1, HomeZone: 7}}
	for _, dryRun := range []bool{true, false} {
		if _, err := st.ImportRoutes(context.Background(), bad, dryRun); !errors.Is(err, ErrRouteInvalidArgument) {
			t.Fatalf("dryRun=%v: err = %v, want ErrRouteInvalidArgument", dryRun, err)
		}
	}
	if got := fake.labels(); len(got) != 0 {
		t.Fatalf("批次非法却发了语句:%v", got)
	}
}

// TestImportMergedIntoOutcomeContract:插入 / 已有且相同 / 已有但不同 三种结局,以及 dry-run 只读。
func TestImportMergedIntoOutcomeContract(t *testing.T) {
	rows := []MergedIntoRow{
		{SrcZone: 101, DstZone: 102, RunID: "101-102-a"},
		{SrcZone: 103, DstZone: 102, RunID: "103-102-b"},
		{SrcZone: 104, DstZone: 102, RunID: "104-102-c"},
	}
	st, fake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
		"R1":  {replyWritable, replyWritable, replyWritable},
		"R10": {replyAffected(1), replyAffected(0), replyAffected(0)},
		"R11": {
			{rows: [][]driver.Value{{int64(102), "103-102-b", int64(1)}}},
			{rows: [][]driver.Value{{int64(105), "104-105-z", int64(2)}}},
		},
	}})
	stats, err := st.ImportMergedInto(context.Background(), rows, false)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if stats.Inserted != 1 || stats.Matched != 1 || len(stats.Conflicts) != 1 {
		t.Fatalf("stats = %+v, want inserted=1 matched=1 conflicts=1", stats)
	}
	wantConflict := MergedIntoConflict{
		DB:    MergedIntoRow{SrcZone: 104, DstZone: 105, RunID: "104-105-z", MergedAtMs: 2},
		Redis: rows[2],
	}
	if stats.Conflicts[0] != wantConflict {
		t.Fatalf("conflict = %+v, want %+v", stats.Conflicts[0], wantConflict)
	}
	want := []string{
		"BEGIN", "R1", "R10", "COMMIT",
		"BEGIN", "R1", "R10", "R11", "ROLLBACK",
		"BEGIN", "R1", "R10", "R11", "ROLLBACK",
	}
	if got := fake.labels(); !reflect.DeepEqual(got, want) {
		t.Fatalf("语句序列不对:\n got=%v\nwant=%v", got, want)
	}
	if got := fake.callsWithLabel("R10")[0].args; !reflect.DeepEqual(got, []driver.Value{uint64(101), uint64(102), "101-102-a"}) {
		t.Fatalf("R10 实参 = %#v", got)
	}

	dry, dryFake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
		"R11ro": {replyNoRows},
	}})
	dryStats, err := dry.ImportMergedInto(context.Background(), rows[:1], true)
	if err != nil || dryStats.Inserted != 1 {
		t.Fatalf("dry-run = (%+v, %v), want inserted=1", dryStats, err)
	}
	if got := dryFake.labels(); !reflect.DeepEqual(got, []string{"R11ro"}) {
		t.Fatalf("dry-run 发了 %v,want 只有一条不加锁的读", got)
	}

	if _, err := dry.ImportMergedInto(context.Background(), []MergedIntoRow{{SrcZone: 1, DstZone: 1, RunID: "x"}}, true); !errors.Is(err, ErrRouteInvalidArgument) {
		t.Fatalf("src == dst: err = %v, want ErrRouteInvalidArgument", err)
	}
}

// TestRouteReadQueriesShapes:ScanRoutes / GetRoutes / ListMergedInto 的语句、实参与 NULL / 大数扫描。
// placement_version 放一个超过 2^63 的值:NullInt64 装不下,这里钉住扫描用的是 sql.Null[uint64]。
func TestRouteReadQueriesShapes(t *testing.T) {
	const bigVersion uint64 = 1<<63 + 5
	st, fake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
		"R12": {{rows: [][]driver.Value{
			{int64(6), int64(6), nil, int64(7), bigVersion, nil, int64(3), "w", int64(1), int64(2)},
			routeRowValues(9, 7, 0, 0, "", 1),
		}}},
		"R8ro": {{rows: [][]driver.Value{routeRowValues(9, 7, 0, 0, "", 1)}}},
		"R14":  {{rows: [][]driver.Value{{int64(101), int64(102), "101-102-a", int64(5)}}}},
	}})
	ctx := context.Background()

	if _, err := st.ScanRoutes(ctx, RouteScanCursor{}, 0); !errors.Is(err, ErrRouteInvalidArgument) {
		t.Fatalf("limit=0: err = %v, want ErrRouteInvalidArgument", err)
	}
	if _, err := st.ScanRoutes(ctx, RouteScanCursor{}, RouteBatchLimit+1); !errors.Is(err, ErrRouteInvalidArgument) {
		t.Fatalf("limit 超限: err = %v, want ErrRouteInvalidArgument", err)
	}
	// 游标片越界:不拒绝的话查询静默回空页,audit 会当作已经扫完。
	if _, err := st.ScanRoutes(ctx, RouteScanCursor{Shard: placement.RouteRowShards}, 1); !errors.Is(err, ErrRouteInvalidArgument) {
		t.Fatalf("游标片 = %d: err = %v, want ErrRouteInvalidArgument", placement.RouteRowShards, err)
	}
	if got := fake.labels(); len(got) != 0 {
		t.Fatalf("入参非法却发了语句:%v", got)
	}
	scanned, err := st.ScanRoutes(ctx, RouteScanCursor{Shard: 5, PlayerID: 5}, 2)
	if err != nil {
		t.Fatalf("ScanRoutes: %v", err)
	}
	want := []RouteRow{
		{PlayerID: 6, StorageID: 7, PlacementVersion: bigVersion, Rev: 3, UpdatedBy: "w", CreatedAtMs: 1, UpdatedAtMs: 2},
		{PlayerID: 9, HomeZone: 7, Rev: 1, UpdatedBy: fakeRouteOldWriter, CreatedAtMs: uint64(fakeRouteCreatedMs), UpdatedAtMs: uint64(fakeRouteUpdatedMs)},
	}
	if !reflect.DeepEqual(scanned, want) {
		t.Fatalf("ScanRoutes = %+v\nwant %+v", scanned, want)
	}
	if got := fake.callsWithLabel("R12")[0].args; !reflect.DeepEqual(got, []driver.Value{uint64(5), uint64(5), uint64(5), int64(2)}) {
		t.Fatalf("R12 实参 = %#v, want (游标片, 游标片, 游标 player_id, limit)", got)
	}

	if got, err := st.GetRoutes(ctx, []uint64{0, 0}); err != nil || len(got) != 0 {
		t.Fatalf("全 0 入参应返回空且不查库: (%v, %v)", got, err)
	}
	if _, err := st.GetRoutes(ctx, make([]uint64, RouteBatchLimit+1)); !errors.Is(err, ErrRouteInvalidArgument) {
		t.Fatalf("GetRoutes 超限: err = %v", err)
	}
	got, err := st.GetRoutes(ctx, []uint64{9, 0, 11})
	if err != nil || len(got) != 1 || got[9].HomeZone != 7 {
		t.Fatalf("GetRoutes = (%+v, %v), want 只有 9(11 没有行,必须缺席)", got, err)
	}
	r8 := fake.callsWithLabel("R8ro")[0]
	if r8.sql != routeSelectRowsSQL(2, false) || !reflect.DeepEqual(r8.args, []driver.Value{uint64(9), uint64(9), uint64(11), uint64(11)}) {
		t.Fatalf("GetRoutes 的读 = %q %#v(实参是交错的 (行片, player_id);0 不许进实参)", r8.sql, r8.args)
	}

	merged, err := st.ListMergedInto(ctx)
	if err != nil || len(merged) != 1 || merged[0] != (MergedIntoRow{SrcZone: 101, DstZone: 102, RunID: "101-102-a", MergedAtMs: 5}) {
		t.Fatalf("ListMergedInto = (%+v, %v)", merged, err)
	}
}

// TestRouteReadsRejectShardMismatchRows:库里存的 route_shard 与 player_id % 16 不符的行(只有 CHECK 曾经不生效时
// 被绕过 store 的写者写进来),读到就 ErrRouteShardMismatch、整页作废,不能当成正常行交给 audit / 发布。
func TestRouteReadsRejectShardMismatchRows(t *testing.T) {
	ctx := context.Background()

	t.Run("ScanRoutes_错片行整页作废", func(t *testing.T) {
		st, _ := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
			"R12": {{rows: [][]driver.Value{
				routeRowValues(1, 7, 0, 0, "", 1),
				routeRowValuesWithShard(3, 6, 7, 0, 0, "", 1), // 6 % 16 = 6 ≠ 3
			}}},
		}})
		rows, err := st.ScanRoutes(ctx, RouteScanCursor{}, 10)
		if !errors.Is(err, ErrRouteShardMismatch) || rows != nil {
			t.Fatalf("ScanRoutes = (%+v, %v), want (nil, ErrRouteShardMismatch)", rows, err)
		}
	})

	// 纵深防御:只有假驱动造得出这种应答。真库上 R8 / R13 按「正确片 + id」点查,库里的错片行对它们不可见、
	// 表现为缺席(见 GetRoutes 的注释与真库用例 TestRouteStore_ShardMismatchRowFailsClosed)。这里只钉住
	// 「万一 SQL 文本被改错、读回了错片行,也不会被当成正常行」。
	t.Run("GetRoutes_纵深防御", func(t *testing.T) {
		st, _ := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
			"R8ro": {{rows: [][]driver.Value{routeRowValuesWithShard(3, 6, 7, 0, 0, "", 1)}}},
		}})
		got, err := st.GetRoutes(ctx, []uint64{6})
		if !errors.Is(err, ErrRouteShardMismatch) || got != nil {
			t.Fatalf("GetRoutes = (%+v, %v), want (nil, ErrRouteShardMismatch)", got, err)
		}
	})
}

// ── 探针 ─────────────────────────────────────────────────────────────────────────

// TestNewRouteStoreProbeFailClosed:会话隔离级别不对、锁等待不是 1、TiDB 是乐观事务、meta 行缺失 / 表不存在、
// chk_route_shard 不生效(P4),一律拒绝建 store 并关池;探针通过时 store 可用、池不关。
func TestNewRouteStoreProbeFailClosed(t *testing.T) {
	sessionOK := fakeRouteReply{rows: [][]driver.Value{{"READ-COMMITTED", int64(1)}}}
	mysqlVersion := fakeRouteReply{rows: [][]driver.Value{{"8.0.36"}}}
	tidbVersion := fakeRouteReply{rows: [][]driver.Value{{"8.0.11-TiDB-v7.5.0"}}}
	goodOpts := RouteStoreOptions{FenceShards: fakeRouteShards, Writer: fakeRouteWriter}
	// P4 期望的应答:3819 且点名 chk_route_shard(MySQL 8.0 的原文格式)。
	checkEnforced := replyErr(&mysql.MySQLError{Number: 3819, Message: "Check constraint 'chk_route_shard' is violated."})
	passedP1toP3 := map[string][]fakeRouteReply{"PROBE": {sessionOK}, "VERSION": {mysqlVersion}, "R1": {replyWritable}}
	withProbeCheck := func(reply fakeRouteReply) map[string][]fakeRouteReply {
		out := make(map[string][]fakeRouteReply, len(passedP1toP3)+1)
		for k, v := range passedP1toP3 {
			out[k] = v
		}
		out["PROBE_CHECK"] = []fakeRouteReply{reply}
		return out
	}
	probeCheckLabels := []string{"PROBE", "VERSION", "R1", "BEGIN", "PROBE_CHECK", "ROLLBACK"}

	for _, tc := range []struct {
		name       string
		opts       RouteStoreOptions
		replies    map[string][]fakeRouteReply
		wantErr    error // 非 nil:errors.Is;nil 且 wantErrNo == 0 表示应成功
		wantErrNo  uint16
		wantLabels []string
	}{
		{
			name:       "隔离级别不对",
			opts:       goodOpts,
			replies:    map[string][]fakeRouteReply{"PROBE": {{rows: [][]driver.Value{{"REPEATABLE-READ", int64(1)}}}}},
			wantErr:    ErrRouteSessionMisconfigured,
			wantLabels: []string{"PROBE"},
		},
		{
			name:       "锁等待不是1",
			opts:       goodOpts,
			replies:    map[string][]fakeRouteReply{"PROBE": {{rows: [][]driver.Value{{"READ-COMMITTED", int64(50)}}}}},
			wantErr:    ErrRouteSessionMisconfigured,
			wantLabels: []string{"PROBE"},
		},
		{
			name: "TiDB乐观事务",
			opts: goodOpts,
			replies: map[string][]fakeRouteReply{
				"PROBE": {sessionOK}, "VERSION": {tidbVersion}, "TXN_MODE": {{rows: [][]driver.Value{{"optimistic"}}}},
			},
			wantErr:    ErrRouteSessionMisconfigured,
			wantLabels: []string{"PROBE", "VERSION", "TXN_MODE"},
		},
		{
			name: "meta行缺失",
			opts: goodOpts,
			replies: map[string][]fakeRouteReply{
				"PROBE": {sessionOK}, "VERSION": {mysqlVersion}, "R1": {replyNoRows},
			},
			wantErr:    ErrRouteMetaMissing,
			wantLabels: []string{"PROBE", "VERSION", "R1"},
		},
		{
			name: "route_meta表不存在_原样带出1146",
			opts: goodOpts,
			replies: map[string][]fakeRouteReply{
				"PROBE": {sessionOK}, "VERSION": {mysqlVersion}, "R1": {replyErr(mysqlErrNo(1146))},
			},
			wantErrNo:  1146,
			wantLabels: []string{"PROBE", "VERSION", "R1"},
		},
		{
			name:    "选项非法_一条语句都不发",
			opts:    RouteStoreOptions{FenceShards: 0, Writer: fakeRouteWriter},
			wantErr: ErrRouteInvalidArgument,
		},
		{
			name: "MySQL_探针通过",
			opts: goodOpts,
			replies: map[string][]fakeRouteReply{
				"PROBE": {sessionOK}, "VERSION": {mysqlVersion}, "R1": {replyWritable}, "PROBE_CHECK": {checkEnforced},
			},
			wantLabels: probeCheckLabels,
		},
		{
			name: "TiDB悲观事务_探针通过",
			opts: goodOpts,
			replies: map[string][]fakeRouteReply{
				"PROBE": {sessionOK}, "VERSION": {tidbVersion}, "TXN_MODE": {{rows: [][]driver.Value{{"pessimistic"}}}},
				"R1": {replyWritable}, "PROBE_CHECK": {checkEnforced},
			},
			wantLabels: []string{"PROBE", "VERSION", "TXN_MODE", "R1", "BEGIN", "PROBE_CHECK", "ROLLBACK"},
		},
		{
			name: "writable为0不拒建store",
			opts: goodOpts,
			replies: map[string][]fakeRouteReply{
				"PROBE": {sessionOK}, "VERSION": {mysqlVersion}, "R1": {{rows: [][]driver.Value{{int64(0)}}}},
				"PROBE_CHECK": {checkEnforced},
			},
			wantLabels: probeCheckLabels,
		},
		{
			// CHECK 没生效(MySQL < 8.0.16、TiDB 没开 tidb_enable_check_constraint):违例行被接受。事务必须回滚。
			name:       "P4_违例行被接受_CHECK不生效",
			opts:       goodOpts,
			replies:    withProbeCheck(replyAffected(1)),
			wantErr:    ErrRouteCheckNotEnforced,
			wantLabels: probeCheckLabels,
		},
		{
			// 3819 但点名的是别的约束:探针行本该只违反 chk_route_shard,证不了它。
			name:       "P4_3819但点名别的约束",
			opts:       goodOpts,
			replies:    withProbeCheck(replyErr(&mysql.MySQLError{Number: 3819, Message: "Check constraint 'chk_route_rev' is violated."})),
			wantErr:    ErrRouteCheckNotEnforced,
			wantLabels: probeCheckLabels,
		},
		{
			name:       "P4_表不存在_原样带出1146",
			opts:       goodOpts,
			replies:    withProbeCheck(replyErr(mysqlErrNo(1146))),
			wantErrNo:  1146,
			wantLabels: probeCheckLabels,
		},
	} {
		t.Run(tc.name, func(t *testing.T) {
			fake := newFakeRouteDB(fakeRouteScript{replies: tc.replies})
			db := sql.OpenDB(fakeRouteConnector{fake: fake})
			t.Cleanup(func() { _ = db.Close() })
			ctx := context.Background()

			st, err := newRouteStoreFromDB(ctx, db, tc.opts)
			wantFail := tc.wantErr != nil || tc.wantErrNo != 0
			switch {
			case tc.wantErr != nil:
				if !errors.Is(err, tc.wantErr) {
					t.Fatalf("err = %v, want errors.Is %v", err, tc.wantErr)
				}
			case tc.wantErrNo != 0:
				var me *mysql.MySQLError
				if !errors.As(err, &me) || me.Number != tc.wantErrNo {
					t.Fatalf("err = %v, want MySQL %d", err, tc.wantErrNo)
				}
			default:
				if err != nil || st == nil {
					t.Fatalf("探针应通过:st=%v err=%v", st, err)
				}
				if st.fenceShards != tc.opts.FenceShards || st.writer != tc.opts.Writer {
					t.Fatalf("store 参数没装上:%+v", st)
				}
			}
			if wantFail && st != nil {
				t.Fatal("探针失败却返回了 store")
			}
			// 失败必须关池(调用方拿不到 store,也就没人会去 Close);成功不许关。
			if pingErr := db.PingContext(ctx); (pingErr != nil) != wantFail {
				t.Fatalf("Ping 之后 err=%v:探针失败=%v 时池应%s", pingErr, wantFail, map[bool]string{true: "已关", false: "仍开"}[wantFail])
			}
			if got := fake.labels(); !reflect.DeepEqual(got, tc.wantLabels) && !(len(got) == 0 && len(tc.wantLabels) == 0) {
				t.Fatalf("探针语句序列不对:\n got=%v\nwant=%v", got, tc.wantLabels)
			}
		})
	}
}

// ── tidb-prepare ─────────────────────────────────────────────────────────────────

// TestPrepareTiDBContract:MySQL 上整步跳过(只问一次版本);TiDB 上先核对两张表都是聚簇主键,再按
// routeTiDBPrepareStatements 的顺序逐条发(文本逐字相同);非聚簇零 ALTER / SPLIT;dry-run 只读;
// 中途失败报出第几步、之后不再发。
func TestPrepareTiDBContract(t *testing.T) {
	ctx := context.Background()
	req := RouteTiDBPrepareRequest{Zones: []uint32{2, 1}, Storages: []uint32{1000000}}
	wantSQL := routeTiDBPrepareStatements([]uint32{1, 2}, []uint32{1000000})
	if len(wantSQL) != 8 {
		t.Fatalf("2 个 zone + 1 个 storage 应生成 8 条语句,got %d", len(wantSQL))
	}
	tidbVersion := fakeRouteReply{rows: [][]driver.Value{{"8.0.11-TiDB-v8.5.2"}}}
	clustered := fakeRouteReply{rows: [][]driver.Value{{"player_route", "CLUSTERED"}, {"zone_merge_fence", "CLUSTERED"}}}
	splitReply := func(k int64) fakeRouteReply {
		return fakeRouteReply{rows: [][]driver.Value{{k, float64(1)}}}
	}
	sixSplits := func() []fakeRouteReply {
		out := make([]fakeRouteReply, 6)
		for i := range out {
			out[i] = splitReply(int64(i + 1))
		}
		return out
	}

	t.Run("MySQL_跳过", func(t *testing.T) {
		st, fake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
			"VERSION": {{rows: [][]driver.Value{{"8.0.36"}}}},
		}})
		rep, err := st.PrepareTiDB(ctx, req)
		if err != nil || !rep.Skipped || rep.Engine != "mysql" || len(rep.Steps) != 0 {
			t.Fatalf("PrepareTiDB = (%+v, %v), want Skipped on mysql", rep, err)
		}
		if got := fake.labels(); !reflect.DeepEqual(got, []string{"VERSION"}) {
			t.Fatalf("MySQL 上发了 %v,want 只问一次版本", got)
		}
	})

	t.Run("TiDB_核对后逐条执行", func(t *testing.T) {
		st, fake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
			"VERSION": {tidbVersion}, "PK_TYPE": {clustered},
			"ALTER_ATTR": {replyAffected(0), replyAffected(0)}, "SPLIT": sixSplits(),
		}})
		rep, err := st.PrepareTiDB(ctx, req)
		if err != nil || rep.Skipped || rep.Engine != "tidb" {
			t.Fatalf("PrepareTiDB = (%+v, %v)", rep, err)
		}
		wantLabels := []string{"VERSION", "PK_TYPE", "ALTER_ATTR", "ALTER_ATTR", "SPLIT", "SPLIT", "SPLIT", "SPLIT", "SPLIT", "SPLIT"}
		if got := fake.labels(); !reflect.DeepEqual(got, wantLabels) {
			t.Fatalf("语句序列不对:\n got=%v\nwant=%v", got, wantLabels)
		}
		var issued []string
		for _, c := range fake.snapshot() {
			if c.label == "ALTER_ATTR" || c.label == "SPLIT" {
				issued = append(issued, c.sql)
			}
		}
		if !reflect.DeepEqual(issued, wantSQL) {
			t.Fatalf("发出的语句与 routeTiDBPrepareStatements 不同:\n got=%q\nwant=%q", issued, wantSQL)
		}
		if len(rep.Steps) != len(wantSQL) {
			t.Fatalf("Steps = %+v", rep.Steps)
		}
		for i, step := range rep.Steps {
			if !step.Executed || step.SQL != wantSQL[i] {
				t.Fatalf("第 %d 步 = %+v, want Executed 且 SQL 相同", i+1, step)
			}
		}
		if got := rep.SplitRegions(); got != 1+2+3+4+5+6 {
			t.Fatalf("SplitRegions() = %d, want 21(各条 SPLIT 首列之和;ALTER 记 0)", got)
		}
	})

	t.Run("非聚簇_零ALTER零SPLIT", func(t *testing.T) {
		st, fake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
			"VERSION": {tidbVersion},
			"PK_TYPE": {{rows: [][]driver.Value{{"player_route", "NONCLUSTERED"}, {"zone_merge_fence", "CLUSTERED"}}}},
		}})
		if _, err := st.PrepareTiDB(ctx, req); !errors.Is(err, ErrRouteTiDBNotClustered) {
			t.Fatalf("err = %v, want ErrRouteTiDBNotClustered", err)
		}
		if got := fake.labels(); !reflect.DeepEqual(got, []string{"VERSION", "PK_TYPE"}) {
			t.Fatalf("非聚簇时发了 %v", got)
		}
	})

	t.Run("dry-run_只读", func(t *testing.T) {
		st, fake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
			"VERSION": {tidbVersion}, "PK_TYPE": {clustered},
		}})
		dry := req
		dry.DryRun = true
		rep, err := st.PrepareTiDB(ctx, dry)
		if err != nil || len(rep.Steps) != len(wantSQL) {
			t.Fatalf("PrepareTiDB(dry-run) = (%+v, %v)", rep, err)
		}
		for i, step := range rep.Steps {
			if step.Executed || step.SQL != wantSQL[i] {
				t.Fatalf("dry-run 第 %d 步 = %+v, want 未执行且 SQL 相同", i+1, step)
			}
		}
		if got := fake.labels(); !reflect.DeepEqual(got, []string{"VERSION", "PK_TYPE"}) {
			t.Fatalf("dry-run 发了 %v,want 只有判引擎与核对聚簇两条只读", got)
		}
	})

	t.Run("第4条SPLIT失败_报第几步且不再发", func(t *testing.T) {
		splits := sixSplits()
		splits[3] = replyErr(mysqlErrNo(1105))
		st, fake := newFakeRouteStore(t, fakeRouteScript{replies: map[string][]fakeRouteReply{
			"VERSION": {tidbVersion}, "PK_TYPE": {clustered},
			"ALTER_ATTR": {replyAffected(0), replyAffected(0)}, "SPLIT": splits,
		}})
		rep, err := st.PrepareTiDB(ctx, req)
		if err == nil || !strings.Contains(err.Error(), "step 6/8") {
			t.Fatalf("err = %v, want 带 step 6/8", err)
		}
		var me *mysql.MySQLError
		if !errors.As(err, &me) || me.Number != 1105 {
			t.Fatalf("err = %v, want 包着原始 MySQLError", err)
		}
		if len(rep.Steps) != len(wantSQL) {
			t.Fatalf("失败时 Steps 仍应是完整清单:%+v", rep.Steps)
		}
		for i, step := range rep.Steps {
			if want := i < 5; step.Executed != want {
				t.Fatalf("第 %d 步 Executed = %t, want %t", i+1, step.Executed, want)
			}
		}
		if got := len(fake.callsWithLabel("SPLIT")); got != 4 {
			t.Fatalf("失败之后还在发:SPLIT 发了 %d 次,want 4", got)
		}
	})

	t.Run("入参非法_一条语句都不发", func(t *testing.T) {
		st, fake := newFakeRouteStore(t, fakeRouteScript{})
		for _, bad := range []RouteTiDBPrepareRequest{{}, {Zones: []uint32{0}}, {Zones: []uint32{1}, Storages: []uint32{0}}} {
			if _, err := st.PrepareTiDB(ctx, bad); !errors.Is(err, ErrRouteInvalidArgument) {
				t.Fatalf("PrepareTiDB(%+v) err = %v, want ErrRouteInvalidArgument", bad, err)
			}
		}
		if got := fake.labels(); len(got) != 0 {
			t.Fatalf("入参非法却发了语句:%v", got)
		}
	})
}
