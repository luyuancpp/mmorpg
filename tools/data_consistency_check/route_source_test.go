package main

// route_source.go 与 checkFriendOrphans 的单测。本 module 只依赖 go-sql-driver/mysql 与 go-redis,没有 miniredis / sqlmock:
// Redis 经 mappingRedis 接口注入假实现;SQL 一侧 dbRouteSource 的循环经 routeTable 假实现驱动,
// sqlRouteTable 的语句与参数经下面注册的最小 database/sql 假驱动核对。真库语义见 route_source_integration_test.go。

import (
	"context"
	"database/sql"
	"database/sql/driver"
	"errors"
	"fmt"
	"go/ast"
	"go/parser"
	"go/token"
	"io"
	"os"
	"sort"
	"strconv"
	"strings"
	"sync"
	"testing"
)

// ── 假 Redis ──────────────────────────────────────────────────────

// fakeMapping:keys 是 SCAN 依次返回的键;values 是 MGET 的值(string,不在表里 = nil);
// types 是 TYPE 的结果(不在表里且不在 values 里 = "none")。
type fakeMapping struct {
	keys     []string
	values   map[string]any
	types    map[string]string
	pageSize int

	scanErr   error
	mgetErr   error
	typeErr   error
	existsErr error

	existsKeys []string
}

func (f *fakeMapping) Scan(_ context.Context, cursor uint64, match string, _ int64) ([]string, uint64, error) {
	if f.scanErr != nil {
		return nil, 0, f.scanErr
	}
	if match != homeZoneKeyPattern {
		return nil, 0, fmt.Errorf("unexpected match %q", match)
	}
	size := f.pageSize
	if size <= 0 {
		size = len(f.keys) + 1
	}
	start := int(cursor)
	end := start + size
	if end >= len(f.keys) {
		return append([]string(nil), f.keys[start:]...), 0, nil
	}
	return append([]string(nil), f.keys[start:end]...), uint64(end), nil
}

func (f *fakeMapping) MGet(_ context.Context, keys ...string) ([]any, error) {
	if f.mgetErr != nil {
		return nil, f.mgetErr
	}
	out := make([]any, len(keys))
	for i, k := range keys {
		out[i] = f.values[k]
	}
	return out, nil
}

func (f *fakeMapping) Type(_ context.Context, key string) (string, error) {
	if f.typeErr != nil {
		return "", f.typeErr
	}
	if t, ok := f.types[key]; ok {
		return t, nil
	}
	if _, ok := f.values[key]; ok {
		return "string", nil
	}
	return "none", nil
}

func (f *fakeMapping) Exists(_ context.Context, key string) (int64, error) {
	f.existsKeys = append(f.existsKeys, key)
	if f.existsErr != nil {
		return 0, f.existsErr
	}
	_, isString := f.values[key]
	_, isOther := f.types[key]
	if isString || isOther {
		return 1, nil
	}
	return 0, nil
}

func TestRedisRouteSourceDescribeSaysNotAuthoritative(t *testing.T) {
	got := redisRouteSource{r: &fakeMapping{}}.Describe()
	if got != "route source: redis (not authoritative under db_first)" {
		t.Fatalf("Describe() = %q", got)
	}
}

// TestRedisLiveZonesCountsMalformedValues:畸形值(非数字、0、空串、超 uint32、超 maxHomeZone、非 string 类型)计数并列出样本,
// 不进活区;SCAN 与 MGET 之间被删的键只计 Vanished。分页跨越多次 SCAN。
func TestRedisLiveZonesCountsMalformedValues(t *testing.T) {
	f := &fakeMapping{
		pageSize: 3,
		keys: []string{
			"player:zone:1", "player:zone:2", "player:zone:3", "player:zone:4", "player:zone:5",
			"player:zone:6", "player:zone:7", "player:zone:8", "player:zone:9", "player:zone:10",
			"player:zone:11",
		},
		values: map[string]any{
			"player:zone:1":  "3",
			"player:zone:2":  "3",
			"player:zone:3":  "5",
			"player:zone:4":  "abc",
			"player:zone:5":  "0",
			"player:zone:6":  "",
			"player:zone:7":  "4294967296",
			"player:zone:10": "7",
			"player:zone:11": "1000000",
		},
		// player:zone:8 是 hash;player:zone:9 既不在 values 也不在 types = SCAN 后被删。
		types: map[string]string{"player:zone:8": "hash"},
	}
	zones, rep, err := redisRouteSource{r: f}.LiveZones(context.Background())
	if err != nil {
		t.Fatalf("LiveZones: %v", err)
	}
	if want := map[uint32]struct{}{3: {}, 5: {}, 7: {}}; fmt.Sprint(sortedKeys(zones)) != fmt.Sprint(sortedKeys(want)) {
		t.Fatalf("zones = %v, want %v", sortedKeys(zones), sortedKeys(want))
	}
	if rep.Scanned != 11 || rep.Malformed != 6 || rep.Vanished != 1 {
		t.Fatalf("report = %+v, want scanned=11 malformed=6 vanished=1", rep)
	}
	joined := strings.Join(rep.Samples, " | ")
	for _, want := range []string{`player:zone:4="abc"`, `player:zone:5="0"`, `player:zone:6=""`, `player:zone:7="4294967296"`, "player:zone:8 (type=hash)", `player:zone:11="1000000"`} {
		if !strings.Contains(joined, want) {
			t.Errorf("samples %q missing %q", joined, want)
		}
	}
	res := shapeCheckResult(rep)
	if res.Severity != "block" || res.BadCount != 6 || res.TotalCount != 11 {
		t.Fatalf("shape result = %+v, want block 6/11", res)
	}
}

func TestRedisLiveZonesSampleLimit(t *testing.T) {
	f := &fakeMapping{values: map[string]any{}}
	for i := 0; i < shapeSampleLimit+5; i++ {
		k := "player:zone:" + strconv.Itoa(i+1)
		f.keys = append(f.keys, k)
		f.values[k] = "bad"
	}
	_, rep, err := redisRouteSource{r: f}.LiveZones(context.Background())
	if err != nil {
		t.Fatalf("LiveZones: %v", err)
	}
	if rep.Malformed != int64(shapeSampleLimit+5) || len(rep.Samples) != shapeSampleLimit {
		t.Fatalf("malformed=%d samples=%d, want %d / %d", rep.Malformed, len(rep.Samples), shapeSampleLimit+5, shapeSampleLimit)
	}
}

func TestRedisLiveZonesCleanIsInfo(t *testing.T) {
	f := &fakeMapping{keys: []string{"player:zone:1"}, values: map[string]any{"player:zone:1": "2"}}
	_, rep, err := redisRouteSource{r: f}.LiveZones(context.Background())
	if err != nil {
		t.Fatalf("LiveZones: %v", err)
	}
	if res := shapeCheckResult(rep); res.Severity != "info" || res.BadCount != 0 {
		t.Fatalf("shape result = %+v, want info", res)
	}
}

// TestRedisRouteSourceErrorsAreErrors:任何一步读失败都报错,不跳过、不当成缺席。
func TestRedisRouteSourceErrorsAreErrors(t *testing.T) {
	boom := errors.New("connection reset")
	base := func() *fakeMapping {
		return &fakeMapping{keys: []string{"player:zone:1", "player:zone:2"}, values: map[string]any{"player:zone:1": "3"}}
	}
	cases := map[string]func(*fakeMapping){
		"scan": func(f *fakeMapping) { f.scanErr = boom },
		"mget": func(f *fakeMapping) { f.mgetErr = boom },
		"type": func(f *fakeMapping) { f.typeErr = boom }, // player:zone:2 的值是 nil,要 TYPE
	}
	for name, mutate := range cases {
		t.Run(name, func(t *testing.T) {
			f := base()
			mutate(f)
			if _, _, err := (redisRouteSource{r: f}).LiveZones(context.Background()); !errors.Is(err, boom) {
				t.Fatalf("LiveZones err = %v, want %v", err, boom)
			}
		})
	}
	f := base()
	f.scanErr = boom
	if _, err := (redisRouteSource{r: f}).CountPlayers(context.Background()); !errors.Is(err, boom) {
		t.Fatalf("CountPlayers err = %v, want %v", err, boom)
	}
	f = base()
	f.existsErr = boom
	has, err := redisRouteSource{r: f}.HasHome(context.Background(), 42)
	if !errors.Is(err, boom) || has {
		t.Fatalf("HasHome = %v, %v; want false + %v", has, err, boom)
	}
}

func TestRedisCountAndHasHome(t *testing.T) {
	f := &fakeMapping{pageSize: 1, keys: []string{"player:zone:1", "player:zone:2", "player:zone:3"},
		values: map[string]any{"player:zone:42": "3"}}
	n, err := redisRouteSource{r: f}.CountPlayers(context.Background())
	if err != nil || n != 3 {
		t.Fatalf("CountPlayers = %d, %v; want 3", n, err)
	}
	src := redisRouteSource{r: f}
	if has, err := src.HasHome(context.Background(), 42); err != nil || !has {
		t.Fatalf("HasHome(42) = %v, %v; want true", has, err)
	}
	if has, err := src.HasHome(context.Background(), 43); err != nil || has {
		t.Fatalf("HasHome(43) = %v, %v; want false", has, err)
	}
	if fmt.Sprint(f.existsKeys) != "[player:zone:42 player:zone:43]" {
		t.Fatalf("exists keys = %v", f.existsKeys)
	}
}

// ── 假 routeTable(驱动 dbRouteSource 的循环)─────────────────────

// fakeRouteTable:homes 是 player_id → home_zone(nil 指针 = NULL);next / page 非 nil 时覆盖默认实现;
// errOn 是 HomeOf 返回的错误;pageCalls 记下每次 PageByHome 的游标。
type fakeRouteTable struct {
	homes map[uint64]*uint32

	next  func(from uint64) (uint32, bool, error)
	page  func(zone uint32, after routeKey, limit int) ([]routeKey, error)
	errOn error

	pageCalls []routeKey
}

func zonePtr(z uint32) *uint32 {
	return &z
}

func (f *fakeRouteTable) NextHomeZone(_ context.Context, from uint64) (uint32, bool, error) {
	if f.next != nil {
		return f.next(from)
	}
	best, found := uint32(0), false
	for _, h := range f.homes {
		if h == nil || uint64(*h) < from {
			continue
		}
		if !found || *h < best {
			best, found = *h, true
		}
	}
	return best, found, nil
}

func (f *fakeRouteTable) PageByHome(_ context.Context, zone uint32, after routeKey, limit int) ([]routeKey, error) {
	f.pageCalls = append(f.pageCalls, after)
	if f.page != nil {
		return f.page(zone, after, limit)
	}
	var keys []routeKey
	for id, h := range f.homes {
		if h == nil || *h != zone {
			continue
		}
		k := routeKey{Shard: id % routeRowShards, PlayerID: id}
		if after.less(k) {
			keys = append(keys, k)
		}
	}
	sort.Slice(keys, func(i, j int) bool { return keys[i].less(keys[j]) })
	if len(keys) > limit {
		keys = keys[:limit]
	}
	return keys, nil
}

func (f *fakeRouteTable) HomeOf(_ context.Context, playerID uint64) (uint32, bool, error) {
	if f.errOn != nil {
		return 0, false, f.errOn
	}
	h, ok := f.homes[playerID]
	if !ok || h == nil {
		return 0, false, nil
	}
	return *h, true, nil
}

// TestDBRouteSourceLiveZonesAndCount:越界 zone 计畸形、不进活区但玩家照样计数;NULL 归属不计;
// 一个 zone 跨两页时第二页的游标是第一页的最后一行。
func TestDBRouteSourceLiveZonesAndCount(t *testing.T) {
	homes := map[uint64]*uint32{}
	for i := uint64(1); i <= routePageSize+500; i++ {
		homes[i] = zonePtr(3)
	}
	homes[900001] = zonePtr(5)
	homes[900002] = zonePtr(0)
	homes[900003] = zonePtr(maxHomeZone + 1)
	homes[900004] = nil
	homes[900005] = nil
	f := &fakeRouteTable{homes: homes}
	src := dbRouteSource{t: f, dbName: "testdb"}

	zones, rep, err := src.LiveZones(context.Background())
	if err != nil {
		t.Fatalf("LiveZones: %v", err)
	}
	if fmt.Sprint(sortedKeys(zones)) != "[3 5]" {
		t.Fatalf("zones = %v, want [3 5]", sortedKeys(zones))
	}
	if rep.Scanned != 4 || rep.Malformed != 2 || rep.Vanished != 0 {
		t.Fatalf("report = %+v, want scanned=4 malformed=2", rep)
	}
	if got := strings.Join(rep.Samples, ","); got != "home_zone=0,home_zone=1000000" {
		t.Fatalf("samples = %q", got)
	}

	f.pageCalls = nil
	n, err := src.CountPlayers(context.Background())
	if err != nil {
		t.Fatalf("CountPlayers: %v", err)
	}
	if want := int64(routePageSize + 500 + 3); n != want {
		t.Fatalf("CountPlayers = %d, want %d", n, want)
	}
	// zone 0 / 3 / 5 / 1000000 各从 {0,0} 起一页,zone 3 另有第二页。
	if len(f.pageCalls) != 5 {
		t.Fatalf("page calls = %d, want 5: %v", len(f.pageCalls), f.pageCalls)
	}
	var zone3Second routeKey
	for _, c := range f.pageCalls {
		if c != (routeKey{}) {
			zone3Second = c
		}
	}
	if zone3Second == (routeKey{}) {
		t.Fatal("zone 3 的第二页没有从第一页的最后一行续上")
	}
	if !strings.Contains(src.Describe(), `"testdb"`) || !strings.Contains(src.Describe(), "route source: db") {
		t.Fatalf("Describe() = %q", src.Describe())
	}
}

// TestDBRouteSourceRejectsBrokenOrdering:游标不前进时报错,不死循环、不重复计数。
func TestDBRouteSourceRejectsBrokenOrdering(t *testing.T) {
	stuck := &fakeRouteTable{next: func(uint64) (uint32, bool, error) { return 3, true, nil }}
	if _, _, err := (dbRouteSource{t: stuck}).LiveZones(context.Background()); err == nil {
		t.Fatal("home_zone 不递增必须报错")
	}

	once := false
	repeat := &fakeRouteTable{
		next: func(from uint64) (uint32, bool, error) {
			if from <= 3 {
				return 3, true, nil
			}
			return 0, false, nil
		},
		page: func(_ uint32, after routeKey, limit int) ([]routeKey, error) {
			if !once {
				once = true
				page := make([]routeKey, limit)
				for i := range page {
					page[i] = routeKey{Shard: 1, PlayerID: uint64(i + 1)}
				}
				return page, nil
			}
			return []routeKey{after}, nil // 第二页回到游标本身
		},
	}
	if _, err := (dbRouteSource{t: repeat}).CountPlayers(context.Background()); err == nil {
		t.Fatal("页内的行不在游标之后必须报错")
	}
}

func TestDBRouteSourceErrorsAreErrors(t *testing.T) {
	boom := errors.New("Error 1045: access denied")
	nextErr := &fakeRouteTable{next: func(uint64) (uint32, bool, error) { return 0, false, boom }}
	if _, _, err := (dbRouteSource{t: nextErr}).LiveZones(context.Background()); !errors.Is(err, boom) {
		t.Fatalf("LiveZones err = %v", err)
	}
	if _, err := (dbRouteSource{t: nextErr}).CountPlayers(context.Background()); !errors.Is(err, boom) {
		t.Fatalf("CountPlayers err = %v", err)
	}
	pageErr := &fakeRouteTable{homes: map[uint64]*uint32{1: zonePtr(3)},
		page: func(uint32, routeKey, int) ([]routeKey, error) { return nil, boom }}
	if _, err := (dbRouteSource{t: pageErr}).CountPlayers(context.Background()); !errors.Is(err, boom) {
		t.Fatalf("CountPlayers err = %v", err)
	}
	homeErr := &fakeRouteTable{errOn: boom}
	if has, err := (dbRouteSource{t: homeErr}).HasHome(context.Background(), 1); !errors.Is(err, boom) || has {
		t.Fatalf("HasHome = %v, %v", has, err)
	}
}

func TestDBRouteSourceHasHome(t *testing.T) {
	src := dbRouteSource{t: &fakeRouteTable{homes: map[uint64]*uint32{1: zonePtr(3), 2: nil}}}
	for id, want := range map[uint64]bool{1: true, 2: false, 3: false} {
		has, err := src.HasHome(context.Background(), id)
		if err != nil || has != want {
			t.Errorf("HasHome(%d) = %v, %v; want %v", id, has, err, want)
		}
	}
}

// ── 最小 database/sql 假驱动 ──────────────────────────────────────
//
// 每个 DSN 对应一个 fakeSQLServer;连接实现 QueryerContext,database/sql 直接把语句与(已转换的)参数交给 handle。

type fakeSQLQuery struct {
	query string
	args  []any
}

type fakeSQLServer struct {
	mu      sync.Mutex
	handle  func(query string, args []any) (cols []string, rows [][]driver.Value, err error)
	queries []fakeSQLQuery
}

// fakeSQLServers:dsn → *fakeSQLServer。
var fakeSQLServers sync.Map

var fakeSQLRegister sync.Once

const fakeSQLDriverTag = "dcc_fake_sql"

type fakeSQLDriver struct{}

func (fakeSQLDriver) Open(name string) (driver.Conn, error) {
	v, ok := fakeSQLServers.Load(name)
	if !ok {
		return nil, fmt.Errorf("fake sql: unknown dsn %q", name)
	}
	return &fakeSQLConn{srv: v.(*fakeSQLServer)}, nil
}

type fakeSQLConn struct {
	srv *fakeSQLServer
}

func (c *fakeSQLConn) Prepare(string) (driver.Stmt, error) {
	return nil, errors.New("fake sql: prepare not supported")
}

func (c *fakeSQLConn) Close() error {
	return nil
}

func (c *fakeSQLConn) Begin() (driver.Tx, error) {
	return nil, errors.New("fake sql: tx not supported")
}

func (c *fakeSQLConn) QueryContext(_ context.Context, query string, named []driver.NamedValue) (driver.Rows, error) {
	args := make([]any, len(named))
	for i, nv := range named {
		args[i] = nv.Value
	}
	c.srv.mu.Lock()
	c.srv.queries = append(c.srv.queries, fakeSQLQuery{query: query, args: args})
	c.srv.mu.Unlock()
	cols, rows, err := c.srv.handle(query, args)
	if err != nil {
		return nil, err
	}
	return &fakeSQLRows{cols: cols, rows: rows}, nil
}

type fakeSQLRows struct {
	cols []string
	rows [][]driver.Value
	i    int
}

func (r *fakeSQLRows) Columns() []string {
	return r.cols
}

func (r *fakeSQLRows) Close() error {
	return nil
}

func (r *fakeSQLRows) Next(dest []driver.Value) error {
	if r.i >= len(r.rows) {
		return io.EOF
	}
	copy(dest, r.rows[r.i])
	r.i++
	return nil
}

// openFakeSQL 起一个假库,handle 按语句回结果。
func openFakeSQL(t *testing.T, handle func(query string, args []any) ([]string, [][]driver.Value, error)) (*sql.DB, *fakeSQLServer) {
	t.Helper()
	fakeSQLRegister.Do(func() { sql.Register(fakeSQLDriverTag, fakeSQLDriver{}) })
	srv := &fakeSQLServer{handle: handle}
	dsn := t.Name()
	fakeSQLServers.Store(dsn, srv)
	t.Cleanup(func() { fakeSQLServers.Delete(dsn) })
	db, err := sql.Open(fakeSQLDriverTag, dsn)
	if err != nil {
		t.Fatalf("open fake sql: %v", err)
	}
	t.Cleanup(func() { _ = db.Close() })
	return db, srv
}

// TestSQLRouteTableStatementsAndArgs:三条语句与参数顺序(含点查的行片 = player_id % 64)、NULL 与无行的口径。
func TestSQLRouteTableStatementsAndArgs(t *testing.T) {
	boom := errors.New("Error 2013: lost connection")
	var failNext bool
	db, srv := openFakeSQL(t, func(query string, args []any) ([]string, [][]driver.Value, error) {
		switch query {
		case sqlNextHomeZone:
			if failNext {
				return nil, nil, boom
			}
			if args[0] == int64(0) {
				return []string{"home_zone"}, [][]driver.Value{{int64(3)}}, nil
			}
			return []string{"home_zone"}, nil, nil
		case sqlPageByHome:
			return []string{"route_shard", "player_id"}, [][]driver.Value{{int64(5), int64(133)}, {int64(6), int64(6)}}, nil
		case sqlHomeOf:
			switch args[1] {
			case int64(130):
				return []string{"home_zone"}, [][]driver.Value{{int64(9)}}, nil
			case int64(131):
				return []string{"home_zone"}, [][]driver.Value{{nil}}, nil
			case int64(132):
				return nil, nil, boom
			}
			return []string{"home_zone"}, nil, nil
		}
		return nil, nil, fmt.Errorf("unexpected query %q", query)
	})
	tbl := sqlRouteTable{db: db}
	ctx := context.Background()

	if z, ok, err := tbl.NextHomeZone(ctx, 0); err != nil || !ok || z != 3 {
		t.Fatalf("NextHomeZone(0) = %d, %v, %v", z, ok, err)
	}
	if _, ok, err := tbl.NextHomeZone(ctx, 4); err != nil || ok {
		t.Fatalf("NextHomeZone(4) ok=%v err=%v, want no rows", ok, err)
	}
	failNext = true
	if _, _, err := tbl.NextHomeZone(ctx, 4); !errors.Is(err, boom) {
		t.Fatalf("NextHomeZone error = %v", err)
	}

	page, err := tbl.PageByHome(ctx, 3, routeKey{Shard: 5, PlayerID: 69}, routePageSize)
	if err != nil || fmt.Sprint(page) != "[{5 133} {6 6}]" {
		t.Fatalf("PageByHome = %v, %v", page, err)
	}

	if z, ok, err := tbl.HomeOf(ctx, 130); err != nil || !ok || z != 9 {
		t.Fatalf("HomeOf(130) = %d, %v, %v", z, ok, err)
	}
	if _, ok, err := tbl.HomeOf(ctx, 131); err != nil || ok {
		t.Fatalf("HomeOf(131) NULL home: ok=%v err=%v, want absent", ok, err)
	}
	if _, ok, err := tbl.HomeOf(ctx, 129); err != nil || ok {
		t.Fatalf("HomeOf(129) no row: ok=%v err=%v, want absent", ok, err)
	}
	if _, _, err := tbl.HomeOf(ctx, 132); !errors.Is(err, boom) {
		t.Fatalf("HomeOf(132) error = %v", err)
	}

	want := []fakeSQLQuery{
		{sqlNextHomeZone, []any{int64(0)}},
		{sqlNextHomeZone, []any{int64(4)}},
		{sqlNextHomeZone, []any{int64(4)}},
		{sqlPageByHome, []any{int64(3), int64(5), int64(5), int64(69), int64(routePageSize)}},
		{sqlHomeOf, []any{int64(130 % 64), int64(130)}},
		{sqlHomeOf, []any{int64(131 % 64), int64(131)}},
		{sqlHomeOf, []any{int64(129 % 64), int64(129)}},
		{sqlHomeOf, []any{int64(132 % 64), int64(132)}},
	}
	if fmt.Sprint(srv.queries) != fmt.Sprint(want) {
		t.Fatalf("queries =\n%v\nwant\n%v", srv.queries, want)
	}
}

// TestSQLStatementsFollowReadTemplate:语句形状钉住提案 §11.2 G1a 的读方模板(键集游标用 OR 展开、按聚簇键排序、
// 点查带行片),改语句时要同时想清楚索引。
func TestSQLStatementsFollowReadTemplate(t *testing.T) {
	for _, want := range []string{
		"WHERE home_zone = ? AND (route_shard > ? OR (route_shard = ? AND player_id > ?))",
		"ORDER BY route_shard, player_id LIMIT ?",
	} {
		if !strings.Contains(sqlPageByHome, want) {
			t.Errorf("sqlPageByHome %q missing %q", sqlPageByHome, want)
		}
	}
	if !strings.Contains(sqlNextHomeZone, "WHERE home_zone >= ? ORDER BY home_zone LIMIT 1") {
		t.Errorf("sqlNextHomeZone %q is not a skip scan on idx_route_home", sqlNextHomeZone)
	}
	if !strings.Contains(sqlHomeOf, "WHERE route_shard = ? AND player_id = ?") {
		t.Errorf("sqlHomeOf %q must be a primary key point read", sqlHomeOf)
	}
	for _, q := range []string{sqlNextHomeZone, sqlPageByHome, sqlHomeOf} {
		if strings.Contains(strings.ToUpper(q), "FOR UPDATE") || strings.Contains(strings.ToUpper(q), "DISTINCT") {
			t.Errorf("read-only tool statement %q must not lock or use DISTINCT", q)
		}
	}
}

// TestMirroredRouteConstantsMatchSources:routeRowShards / maxHomeZone 与源头逐字一致,索引形状仍是语句所依赖的那样。
// 源头:go/shared/placement/route.go 的 RouteRowShards,go/data_service/internal/store/schema.go 的 player_route DDL。
func TestMirroredRouteConstantsMatchSources(t *testing.T) {
	fset := token.NewFileSet()
	file, err := parser.ParseFile(fset, "../../go/shared/placement/route.go", nil, 0)
	if err != nil {
		t.Fatalf("parse shared/placement/route.go: %v", err)
	}
	found := ""
	ast.Inspect(file, func(n ast.Node) bool {
		spec, ok := n.(*ast.ValueSpec)
		if !ok {
			return true
		}
		for i, name := range spec.Names {
			if name.Name == "RouteRowShards" && i < len(spec.Values) {
				if lit, ok := spec.Values[i].(*ast.BasicLit); ok {
					found = lit.Value
				}
			}
		}
		return true
	})
	if found != strconv.FormatUint(routeRowShards, 10) {
		t.Fatalf("placement.RouteRowShards = %q, mirrored routeRowShards = %d", found, routeRowShards)
	}

	schema, err := os.ReadFile("../../go/data_service/internal/store/schema.go")
	if err != nil {
		t.Fatalf("read data_service store/schema.go: %v", err)
	}
	for _, want := range []string{
		"KEY `idx_route_home` (`home_zone`, `route_shard`, `player_id`)",
		"PRIMARY KEY (`route_shard`, `player_id`)",
		"CHECK (`route_shard` = `player_id` % " + strconv.FormatUint(routeRowShards, 10) + ")",
		"`home_zone` BETWEEN 1 AND " + strconv.FormatUint(uint64(maxHomeZone), 10) + ")",
	} {
		if !strings.Contains(string(schema), want) {
			t.Errorf("player_route DDL no longer contains %q: update route_source.go together with it", want)
		}
	}
}

// ── checkFriendOrphans:归属查询失败整条判 fail ────────────────────

// fakeRoutes 是 routeSource 的最小假实现,只给好友检查用。
type fakeRoutes struct {
	has    map[uint64]bool
	errOn  map[uint64]error
	called []uint64
}

func (f *fakeRoutes) Describe() string {
	return "route source: fake"
}

func (f *fakeRoutes) LiveZones(context.Context) (map[uint32]struct{}, zoneShapeReport, error) {
	return nil, zoneShapeReport{}, errors.New("not used")
}

func (f *fakeRoutes) CountPlayers(context.Context) (int64, error) {
	return 0, errors.New("not used")
}

func (f *fakeRoutes) HasHome(_ context.Context, id uint64) (bool, error) {
	f.called = append(f.called, id)
	if err := f.errOn[id]; err != nil {
		return false, err
	}
	return f.has[id], nil
}

func friendDBWithSample(t *testing.T, pairs [][2]int64) *sql.DB {
	t.Helper()
	db, _ := openFakeSQL(t, func(query string, _ []any) ([]string, [][]driver.Value, error) {
		switch {
		case query == "SELECT COUNT(*) FROM friend":
			return []string{"n"}, [][]driver.Value{{int64(len(pairs))}}, nil
		case strings.HasPrefix(query, "SELECT player_id, friend_player_id FROM friend"):
			rows := make([][]driver.Value, len(pairs))
			for i, p := range pairs {
				rows[i] = []driver.Value{p[0], p[1]}
			}
			return []string{"player_id", "friend_player_id"}, rows, nil
		}
		return nil, nil, fmt.Errorf("unexpected query %q", query)
	})
	return db
}

// TestFriendOrphansRouteErrorFailsWholeCheck:以前 Exists(...).Val() 吞掉错误、把读失败数成孤儿;
// 现在读失败整条检查判 fail(进程退出 1),孤儿数不报。
func TestFriendOrphansRouteErrorFailsWholeCheck(t *testing.T) {
	routes := &fakeRoutes{
		has:   map[uint64]bool{11: false, 12: true},
		errOn: map[uint64]error{13: errors.New("i/o timeout")},
	}
	cfg := runConfig{friendDB: friendDBWithSample(t, [][2]int64{{1, 11}, {1, 12}, {1, 13}, {1, 14}}), routes: routes}

	r := checkFriendOrphans(context.Background(), cfg)
	if r.Severity != "fail" || r.BadCount != 0 {
		t.Fatalf("result = %+v, want fail with BadCount 0", r)
	}
	if !strings.Contains(r.SampleNotes, "friend_player_id=13") || !strings.Contains(r.SampleNotes, "i/o timeout") {
		t.Fatalf("notes = %q", r.SampleNotes)
	}
	if fmt.Sprint(routes.called) != "[11 12 13]" {
		t.Fatalf("lookups = %v, want stop at the first error", routes.called)
	}
	if n := printReport([]CheckResult{r}, "route source: fake"); n != 1 {
		t.Fatalf("printReport counted %d failing rows, want 1 (fail must exit non-zero)", n)
	}
}

func TestFriendOrphansCountsMissingHomes(t *testing.T) {
	routes := &fakeRoutes{has: map[uint64]bool{11: true, 12: true}}
	cfg := runConfig{friendDB: friendDBWithSample(t, [][2]int64{{1, 11}, {1, 12}, {1, 13}}), routes: routes}

	r := checkFriendOrphans(context.Background(), cfg)
	// 1/3 ≥ 5% → block。
	if r.Severity != "block" || r.BadCount != 1 {
		t.Fatalf("result = %+v, want block with 1 orphan", r)
	}
}
