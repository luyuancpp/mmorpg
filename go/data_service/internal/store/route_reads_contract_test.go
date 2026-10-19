package store

import (
	"context"
	"database/sql"
	"database/sql/driver"
	"errors"
	"fmt"
	"reflect"
	"sync"
	"testing"

	"shared/placement"
)

// route_reads.go(RD1–RD4)的**对外语义**回归:库回什么 → 读方法回什么、发了哪条语句、实参是什么。
// 做法同 route_store_contract_test.go(假驱动 + mysqlLikeConvert 参数转换),但用一个只认 RD1–RD4 的独立假驱动:
// 那边的语句标签表(routeStmtLabel)属于写入方的契约,读方不往里加。
// 假驱动不模拟 InnoDB:LEFT JOIN 的 NULL 口径、索引命中只有真库能证(route_reads_integration_test.go)。

// readsReply 是一条语句的应答:rows(nil = 零行)或 err。
type readsReply struct {
	rows [][]driver.Value
	err  error
}

// readsCall 是真的发到驱动的一次查询。
type readsCall struct {
	label string
	args  []driver.Value
}

// readsFakeDB 按语句标签排队应答;认不出的语句、队列用尽都报错(脚本写少了不能被当成正常路径)。
type readsFakeDB struct {
	mu      sync.Mutex
	replies map[string][]readsReply
	calls   []readsCall
}

func readsStmtLabel(query string) string {
	switch query {
	case routeReadHomeFenceSQL:
		return "RD1"
	case routeScanByHomeSQL:
		return "RD2"
	case routeScanByStorageSQL:
		return "RD3"
	case routeListActiveFencesSQL:
		return "RD4"
	}
	return ""
}

func (f *readsFakeDB) next(query string, args []driver.Value) (readsReply, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	label := readsStmtLabel(query)
	f.calls = append(f.calls, readsCall{label: label, args: args})
	if label == "" {
		return readsReply{}, fmt.Errorf("reads fake db: unexpected statement %q", query)
	}
	q := f.replies[label]
	if len(q) == 0 {
		return readsReply{}, fmt.Errorf("reads fake db: script exhausted for %s", label)
	}
	f.replies[label] = q[1:]
	return q[0], nil
}

func (f *readsFakeDB) snapshot() []readsCall {
	f.mu.Lock()
	defer f.mu.Unlock()
	return append([]readsCall(nil), f.calls...)
}

// newReadsFakeStore 把假驱动接成 RouteStore(同包直接装字段,不跑探针)。replies 会被拷一份再消费。
func newReadsFakeStore(t *testing.T, replies map[string][]readsReply) (*RouteStore, *readsFakeDB) {
	t.Helper()
	fake := &readsFakeDB{replies: make(map[string][]readsReply, len(replies))}
	for k, v := range replies {
		fake.replies[k] = append([]readsReply(nil), v...)
	}
	db := sql.OpenDB(readsConnector{fake: fake})
	t.Cleanup(func() { _ = db.Close() })
	return &RouteStore{db: db, fenceShards: fakeRouteShards, writer: fakeRouteWriter}, fake
}

type readsConnector struct{ fake *readsFakeDB }

func (c readsConnector) Connect(context.Context) (driver.Conn, error) {
	return &readsConn{fake: c.fake}, nil
}

func (c readsConnector) Driver() driver.Driver {
	return readsDriver{}
}

type readsDriver struct{}

func (readsDriver) Open(string) (driver.Conn, error) {
	return nil, errors.New("reads fake db: only sql.OpenDB(connector) is supported")
}

type readsConn struct{ fake *readsFakeDB }

func (c *readsConn) Prepare(string) (driver.Stmt, error) {
	return nil, errors.New("reads fake db: Prepare is not supported")
}

func (c *readsConn) Close() error {
	return nil
}

// Begin 报错:读方法一律是事务外一致读,开事务就是写错了。
func (c *readsConn) Begin() (driver.Tx, error) {
	return nil, errors.New("reads fake db: the read paths must not open a transaction")
}

// CheckNamedValue 与 route_store_contract_test.go 同口径:按 go-sql-driver 的规则转换实参(uint64 原样放行)。
func (c *readsConn) CheckNamedValue(nv *driver.NamedValue) error {
	v, err := mysqlLikeConvert(nv.Value)
	if err != nil {
		return err
	}
	nv.Value = v
	return nil
}

func (c *readsConn) QueryContext(_ context.Context, query string, args []driver.NamedValue) (driver.Rows, error) {
	reply, err := c.fake.next(query, namedToValues(args))
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

// 用例共用的值。readsPlayer:行片 4242 % 64 = 18。
const (
	readsPlayer  uint64 = 4242
	readsHome    uint32 = 7
	readsStorage uint32 = 1000000
	readsNowMs   int64  = 1_760_000_123_456
)

var errReadsBoom = errors.New("connection reset by peer")

// assertReadsCalls 断言发出的语句标签与实参逐条相同。
func assertReadsCalls(t *testing.T, fake *readsFakeDB, want []readsCall) {
	t.Helper()
	got := fake.snapshot()
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("calls mismatch:\n got=%#v\nwant=%#v", got, want)
	}
}

// TestGetHomeAndFenceContract:RD1 的每一种库应答 → GetHomeAndFence 的结果,以及实参(围栏片 63、行片、id)。
// 要害是「SQL 故障 ≠ 缺席」:错误必须原样回给调用方,路由层据此回 Unavailable,绝不 NotFound。
func TestGetHomeAndFenceContract(t *testing.T) {
	wantArgs := []driver.Value{uint64(placement.MaxRouteFenceShards - 1), uint64(placement.RouteRowShard(readsPlayer)), readsPlayer}
	for _, tc := range []struct {
		name    string
		reply   readsReply
		want    RouteHomeFence
		wantErr bool
	}{
		{name: "no row is absence", reply: readsReply{}, want: RouteHomeFence{}},
		{name: "home NULL is absence", reply: readsReply{rows: [][]driver.Value{{nil, int64(0)}}}, want: RouteHomeFence{}},
		{name: "home without fence", reply: readsReply{rows: [][]driver.Value{{int64(readsHome), int64(0)}}},
			want: RouteHomeFence{HomeZone: readsHome}},
		{name: "home with active fence", reply: readsReply{rows: [][]driver.Value{{int64(readsHome), int64(1)}}},
			want: RouteHomeFence{HomeZone: readsHome, Fenced: true}},
		{name: "fence NULL (run_id set, expiry NULL) is fenced", reply: readsReply{rows: [][]driver.Value{{int64(readsHome), nil}}},
			want: RouteHomeFence{HomeZone: readsHome, Fenced: true}},
		{name: "home above RouteHomeZoneMax is an error", reply: readsReply{rows: [][]driver.Value{{int64(RouteHomeZoneMax) + 1, int64(0)}}},
			wantErr: true},
		{name: "home 0 is an error", reply: readsReply{rows: [][]driver.Value{{int64(0), int64(0)}}}, wantErr: true},
		{name: "SQL failure is an error, not absence", reply: readsReply{err: errReadsBoom}, wantErr: true},
	} {
		t.Run(tc.name, func(t *testing.T) {
			st, fake := newReadsFakeStore(t, map[string][]readsReply{"RD1": {tc.reply}})
			got, err := st.GetHomeAndFence(context.Background(), readsPlayer)
			if tc.wantErr {
				if err == nil {
					t.Fatalf("want an error, got %+v", got)
				}
				if got != (RouteHomeFence{}) {
					t.Fatalf("an error must come with the zero value, got %+v", got)
				}
			} else {
				if err != nil {
					t.Fatalf("unexpected error: %v", err)
				}
				if got != tc.want {
					t.Fatalf("got %+v, want %+v", got, tc.want)
				}
				if got.HasHome() != (tc.want.HomeZone != 0) {
					t.Fatalf("HasHome() = %v for %+v", got.HasHome(), got)
				}
			}
			assertReadsCalls(t, fake, []readsCall{{label: "RD1", args: wantArgs}})
		})
	}
	if t.Failed() {
		return
	}
	// SQL 故障的错误链保留驱动错误(调用方用 errors.Is 判,不靠文案)。
	st, _ := newReadsFakeStore(t, map[string][]readsReply{"RD1": {{err: errReadsBoom}}})
	if _, err := st.GetHomeAndFence(context.Background(), readsPlayer); !errors.Is(err, errReadsBoom) {
		t.Fatalf("SQL error must stay in the chain, got %v", err)
	}
}

// TestGetHomeAndFenceZeroIDIssuesNoStatement:id 为 0 不发语句、回缺席(与 GetRoutes 跳过 0 同口径)。
func TestGetHomeAndFenceZeroIDIssuesNoStatement(t *testing.T) {
	st, fake := newReadsFakeStore(t, nil)
	got, err := st.GetHomeAndFence(context.Background(), 0)
	if err != nil || got != (RouteHomeFence{}) {
		t.Fatalf("GetHomeAndFence(0) = %+v, %v; want zero, nil", got, err)
	}
	assertReadsCalls(t, fake, nil)
}

// readsRow 是 RD2 / RD3 读到的一行(聚簇键在前,列序同 routeRowColumnsSQL)。
func readsRow(player uint64, home, storage uint32) []driver.Value {
	var version driver.Value
	if storage != 0 {
		version = int64(1)
	}
	return []driver.Value{
		int64(placement.RouteRowShard(player)), int64(player), dv32(home), dv32(storage), version, nil, int64(3),
		fakeRouteOldWriter, fakeRouteCreatedMs, fakeRouteUpdatedMs,
	}
}

// TestScanRoutesByHomeContract:RD2 的实参顺序(zone、游标片 ×2、游标 id、limit)、行解码,以及每一种「整页作废」。
func TestScanRoutesByHomeContract(t *testing.T) {
	after := RouteScanCursor{Shard: 3, PlayerID: 131}
	wantArgs := []driver.Value{uint64(readsHome), uint64(3), uint64(3), uint64(131), int64(2)}
	// 两行:片 18 的 4242 与片 20 的 4244,均在 after 之后、按聚簇键升序。
	good := [][]driver.Value{readsRow(4242, readsHome, 0), readsRow(4244, readsHome, readsStorage)}

	t.Run("decodes rows in keyset order", func(t *testing.T) {
		st, fake := newReadsFakeStore(t, map[string][]readsReply{"RD2": {{rows: good}}})
		got, err := st.ScanRoutesByHome(context.Background(), readsHome, after, 2)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if len(got) != 2 || got[0].PlayerID != 4242 || got[1].PlayerID != 4244 ||
			got[0].HomeZone != readsHome || got[1].StorageID != readsStorage || got[1].PlacementVersion != 1 || got[0].Rev != 3 {
			t.Fatalf("decoded rows wrong: %+v", got)
		}
		assertReadsCalls(t, fake, []readsCall{{label: "RD2", args: wantArgs}})
	})

	for _, tc := range []struct {
		name  string
		reply readsReply
		is    error
	}{
		{name: "SQL failure", reply: readsReply{err: errReadsBoom}, is: errReadsBoom},
		{name: "row from another zone", reply: readsReply{rows: [][]driver.Value{readsRow(4242, readsHome+1, 0)}}},
		{name: "row not after the cursor", reply: readsReply{rows: [][]driver.Value{readsRow(130, readsHome, 0)}}},
		{name: "rows out of keyset order", reply: readsReply{rows: [][]driver.Value{good[1], good[0]}}},
		{name: "stored shard mismatch", reply: readsReply{rows: [][]driver.Value{
			append([]driver.Value{int64(5)}, readsRow(4242, readsHome, 0)[1:]...)}}, is: ErrRouteShardMismatch},
	} {
		t.Run(tc.name, func(t *testing.T) {
			st, _ := newReadsFakeStore(t, map[string][]readsReply{"RD2": {tc.reply}})
			got, err := st.ScanRoutesByHome(context.Background(), readsHome, after, 2)
			if err == nil {
				t.Fatalf("want an error, got %+v", got)
			}
			if got != nil {
				t.Fatalf("a failed page must return no rows, got %+v", got)
			}
			if tc.is != nil && !errors.Is(err, tc.is) {
				t.Fatalf("error %v must wrap %v", err, tc.is)
			}
		})
	}
}

// TestScanRoutesByStorageContract:RD3 与 RD2 同形,条件列换成 storage_id;每一行的 storage_id 必须等于参数。
func TestScanRoutesByStorageContract(t *testing.T) {
	wantArgs := []driver.Value{uint64(readsStorage), uint64(0), uint64(0), uint64(0), int64(RouteBatchLimit)}
	st, fake := newReadsFakeStore(t, map[string][]readsReply{"RD3": {
		{rows: [][]driver.Value{readsRow(4242, readsHome, readsStorage)}},
		{rows: [][]driver.Value{readsRow(4242, readsHome, readsStorage+1)}},
	}})
	got, err := st.ScanRoutesByStorage(context.Background(), readsStorage, RouteScanCursor{}, RouteBatchLimit)
	if err != nil || len(got) != 1 || got[0].StorageID != readsStorage {
		t.Fatalf("ScanRoutesByStorage = %+v, %v", got, err)
	}
	if _, err := st.ScanRoutesByStorage(context.Background(), readsStorage, RouteScanCursor{}, RouteBatchLimit); err == nil {
		t.Fatal("a row with another storage_id must fail the page")
	}
	assertReadsCalls(t, fake, []readsCall{{label: "RD3", args: wantArgs}, {label: "RD3", args: wantArgs}})
}

// TestScanRoutesByIndexRejectsInvalidArgsBeforeAnyStatement:非法 zone / storage / limit / 游标片一律 ErrRouteInvalidArgument,
// 且一条语句都不发(片 >= 64 的游标会让查询静默回空页,调用方会当作翻完)。
func TestScanRoutesByIndexRejectsInvalidArgsBeforeAnyStatement(t *testing.T) {
	ctx := context.Background()
	st, fake := newReadsFakeStore(t, nil)
	check := func(name string, rows []RouteRow, err error) {
		t.Helper()
		if !errors.Is(err, ErrRouteInvalidArgument) || rows != nil {
			t.Fatalf("%s: want ErrRouteInvalidArgument and no rows, got %v, %+v", name, err, rows)
		}
	}
	rows, err := st.ScanRoutesByHome(ctx, 0, RouteScanCursor{}, 10)
	check("home zone 0", rows, err)
	rows, err = st.ScanRoutesByHome(ctx, RouteHomeZoneMax+1, RouteScanCursor{}, 10)
	check("home zone too large", rows, err)
	rows, err = st.ScanRoutesByHome(ctx, readsHome, RouteScanCursor{}, 0)
	check("home limit 0", rows, err)
	rows, err = st.ScanRoutesByHome(ctx, readsHome, RouteScanCursor{}, RouteBatchLimit+1)
	check("home limit too large", rows, err)
	rows, err = st.ScanRoutesByHome(ctx, readsHome, RouteScanCursor{Shard: placement.RouteRowShards}, 10)
	check("home cursor shard 64", rows, err)
	rows, err = st.ScanRoutesByStorage(ctx, 0, RouteScanCursor{}, 10)
	check("storage 0", rows, err)
	rows, err = st.ScanRoutesByStorage(ctx, readsStorage, RouteScanCursor{}, 0)
	check("storage limit 0", rows, err)
	assertReadsCalls(t, fake, nil)
}

// fenceRow 是 RD4 的一行:now_ms、zone_id、run_id、purpose、src、dst、expires_at_ms、holder_json、fence_epoch。
func fenceRow(zone uint32, runID string, expires driver.Value, epoch int64) []driver.Value {
	return []driver.Value{readsNowMs, int64(zone), runID, "merge", int64(zone), int64(zone + 100), expires,
		`{"run_id":"` + runID + `","fence_epoch":` + fmt.Sprint(epoch) + `}`, epoch}
}

// TestListActiveMergeFencesContract:RD4 的应答形状 → 结果。没有生效围栏时也有一行(库时钟 + 全 NULL),结果为空、带库时钟;
// expires_at_ms 为 NULL 的生效行按生效列出(ExpiresAtMs=0);形状不对一律报错。
func TestListActiveMergeFencesContract(t *testing.T) {
	const run = "3-7-20261018T000000Z-0a1b2c3d"
	allNull := []driver.Value{readsNowMs, nil, nil, nil, nil, nil, nil, nil, nil}
	wantArgs := []driver.Value{uint64(placement.MaxRouteFenceShards - 1)}

	t.Run("no active fence still returns the clock", func(t *testing.T) {
		st, fake := newReadsFakeStore(t, map[string][]readsReply{"RD4": {{rows: [][]driver.Value{allNull}}}})
		got, err := st.ListActiveMergeFences(context.Background())
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if got.DBNowMs != uint64(readsNowMs) || len(got.Fences) != 0 {
			t.Fatalf("got %+v, want clock %d and no fence", got, readsNowMs)
		}
		assertReadsCalls(t, fake, []readsCall{{label: "RD4", args: wantArgs}})
	})

	t.Run("active fences decoded in zone order", func(t *testing.T) {
		expires := readsNowMs + 3_600_000
		st, _ := newReadsFakeStore(t, map[string][]readsReply{"RD4": {{rows: [][]driver.Value{
			fenceRow(5, run, expires, 2),
			fenceRow(6, run, nil, 4),
		}}}})
		got, err := st.ListActiveMergeFences(context.Background())
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		want := ActiveMergeFences{DBNowMs: uint64(readsNowMs), Fences: []ActiveMergeFence{
			{ZoneID: 5, RunID: run, Purpose: "merge", SrcZone: 5, DstZone: 105, ExpiresAtMs: uint64(expires),
				HolderJSON: `{"run_id":"` + run + `","fence_epoch":2}`, FenceEpoch: 2},
			{ZoneID: 6, RunID: run, Purpose: "merge", SrcZone: 6, DstZone: 106, ExpiresAtMs: 0,
				HolderJSON: `{"run_id":"` + run + `","fence_epoch":4}`, FenceEpoch: 4},
		}}
		if !reflect.DeepEqual(got, want) {
			t.Fatalf("got %+v\nwant %+v", got, want)
		}
	})

	for _, tc := range []struct {
		name  string
		reply readsReply
	}{
		{name: "SQL failure", reply: readsReply{err: errReadsBoom}},
		{name: "no row at all", reply: readsReply{}},
		{name: "all-NULL row next to a real one", reply: readsReply{rows: [][]driver.Value{allNull, fenceRow(5, run, nil, 1)}}},
		{name: "active row without run_id", reply: readsReply{rows: [][]driver.Value{
			{readsNowMs, int64(5), nil, nil, nil, nil, nil, nil, int64(1)}}}},
		{name: "active row without fence_epoch", reply: readsReply{rows: [][]driver.Value{
			{readsNowMs, int64(5), run, nil, nil, nil, nil, nil, nil}}}},
		{name: "clock 0", reply: readsReply{rows: [][]driver.Value{{int64(0), nil, nil, nil, nil, nil, nil, nil, nil}}}},
	} {
		t.Run(tc.name, func(t *testing.T) {
			st, _ := newReadsFakeStore(t, map[string][]readsReply{"RD4": {tc.reply}})
			if got, err := st.ListActiveMergeFences(context.Background()); err == nil {
				t.Fatalf("want an error, got %+v", got)
			}
		})
	}
}

// 读方法的签名就是 routing.RouteReader 的契约(那个接口在 routing 包,这里不 import 它以免成环):
// 用方法表达式钉住参数与返回类型,签名一变这里先编译失败,提醒同步改 routing.RouteReader 与 svc.PlayerRouteStore。
var _ func(*RouteStore, context.Context, uint64) (RouteHomeFence, error) = (*RouteStore).GetHomeAndFence

var _ func(*RouteStore, context.Context, []uint64) (map[uint64]RouteRow, error) = (*RouteStore).GetRoutes

var _ func(*RouteStore, context.Context, uint32, RouteScanCursor, int) ([]RouteRow, error) = (*RouteStore).ScanRoutesByHome

var _ func(*RouteStore, context.Context, uint32, RouteScanCursor, int) ([]RouteRow, error) = (*RouteStore).ScanRoutesByStorage

var _ func(*RouteStore, context.Context) (ActiveMergeFences, error) = (*RouteStore).ListActiveMergeFences
