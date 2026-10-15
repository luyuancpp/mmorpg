package store

import (
	"errors"
	"fmt"
	"go/ast"
	"go/parser"
	"go/token"
	"io/fs"
	"path/filepath"
	"regexp"
	"slices"
	"strconv"
	"strings"
	"testing"

	"shared/placement"

	"github.com/go-sql-driver/mysql"
	"github.com/luyuancpp/proto2mysql"
)

// RouteStore 不连库的单测:DSN、DDL 契约、SQL 文本形状、写入点守卫、导入判定、行的编解码、写者标识。
// 「库应答 → 结局 / 语句 / 参数」的契约在 route_store_contract_test.go(假驱动);
// 锁、ODKU 行数、CHECK、并发只有真库能证,在 route_store_integration_test.go(-tags=integration)。

// TestRouteDSNSetsReadCommittedAndLockWaitWithoutClientFoundRows 钉住 RouteStore 池的三条前提:
//   - 会话 RC:锁推演(规格 c.2「为什么这套锁不会成环」)只在 RC 下成立;
//   - innodb_lock_wait_timeout=1:ctx 到期后服务端那个等锁的线程最多再占 1s 围栏分片锁(默认 50s);
//   - 不带 clientFoundRows:ODKU no-op 靠「撞键 = 受影响 0 行」判定,带了它 no-op 也回 1,撞键会被当成插入成功。
//
// 同时要求它是在 buildDSN 之上追加的:严格模式(sql_mode)是全部 store 共用的写入兜底,不能因为另开一个池就丢了。
func TestRouteDSNSetsReadCommittedAndLockWaitWithoutClientFoundRows(t *testing.T) {
	base := MySQLConfig{Host: "127.0.0.1:3306", User: "u", Password: "p", DBName: "d"}
	dsn := buildRouteDSN(base)
	if !strings.HasPrefix(dsn, buildDSN(base)) {
		t.Fatalf("buildRouteDSN 不是在 buildDSN 之上追加的:\n got=%q\nbase=%q", dsn, buildDSN(base))
	}
	cfg, err := mysql.ParseDSN(dsn)
	if err != nil {
		t.Fatalf("buildRouteDSN 产出的 DSN 解析失败: %v", err)
	}
	if cfg.ClientFoundRows {
		t.Fatal("buildRouteDSN 设了 clientFoundRows=true:R4 / R7 / R10 / R15 的 ODKU no-op 会回 1 行,撞键被误判成插入成功")
	}
	if got := cfg.Params["transaction_isolation"]; got != "'READ-COMMITTED'" {
		t.Fatalf("transaction_isolation = %q, want 'READ-COMMITTED'(带单引号:驱动原样拼进 SET)", got)
	}
	if routeLockWaitTimeoutSec != 1 {
		t.Fatalf("routeLockWaitTimeoutSec = %d:规格 c.2 的预算分配按 1s 算(服务端 2s 预算 = 锁等待 1s + 发布)", routeLockWaitTimeoutSec)
	}
	if got := cfg.Params["innodb_lock_wait_timeout"]; got != strconv.Itoa(routeLockWaitTimeoutSec) {
		t.Fatalf("innodb_lock_wait_timeout = %q, want %d", got, routeLockWaitTimeoutSec)
	}
	if got := cfg.Params["sql_mode"]; got != "'STRICT_TRANS_TABLES'" {
		t.Fatalf("sql_mode = %q:路由池丢了 buildDSN 的严格模式", got)
	}
}

// TestRouteBootstrapDDLContract 把路由表的手写 DDL 与本 store / 规格 b.3 / G1a 的假设钉在一起(不连库)。
func TestRouteBootstrapDDLContract(t *testing.T) {
	ddls := map[string]string{
		PlayerRouteTableName:    playerRouteBootstrapDDL,
		ZoneMergeFenceTableName: zoneMergeFenceBootstrapDDL,
		ZoneMergedIntoTableName: zoneMergedIntoBootstrapDDL,
		RouteMetaTableName:      routeMetaBootstrapDDL,
		RouteReconcileTableName: routeReconcileBootstrapDDL,
	}
	for name, ddl := range ddls {
		if !strings.HasPrefix(ddl, "CREATE TABLE IF NOT EXISTS `"+name+"` (") {
			t.Fatalf("%s 的 DDL 不以 CREATE TABLE IF NOT EXISTS `%s` 开头(迁移每次部署都跑,必须幂等):%q", name, name, ddl)
		}
		if bootstrapTables[name] != ddl {
			t.Fatalf("bootstrapTables[%s] 不是 %s 的 DDL 常量:迁移不会建这张表", name, name)
		}
		if strings.Contains(strings.ToUpper(ddl), "INSERT") {
			t.Fatalf("%s 的 DDL 里混进了写语句", name)
		}
		// 排序规则显式化(G1a):只写 CHARSET 时 MySQL 与 TiDB 的默认排序规则不同,比较语义会分叉。
		if !strings.Contains(ddl, "DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin") {
			t.Fatalf("%s 的 DDL 没有显式 COLLATE=utf8mb4_bin(MySQL 默认 utf8mb4_0900_ai_ci、TiDB 默认 utf8mb4_bin)", name)
		}
		if n, m := strings.Count(ddl, "CHARACTER SET ascii"), strings.Count(ddl, "CHARACTER SET ascii COLLATE ascii_bin"); n != m {
			t.Fatalf("%s 有 %d 个 ascii 列,只有 %d 个显式 COLLATE ascii_bin(MySQL 默认 ascii_general_ci、TiDB 只有 ascii_bin)", name, n, m)
		}
	}
	if got := len(bootstrapTables); got != 7 {
		t.Fatalf("bootstrapTables 有 %d 项,want 7(id_segment、player_name + 五张路由表)", got)
	}

	// player_route 的聚簇主键 (route_shard, player_id) 与 64 片(G1a;模数 2026-10-13 由 16 改为 64,数值由
	// placement 的 TestRouteRowShard 钉住)。chk_route_shard 里的模数与 placement.RouteRowShards 必须一致:
	// 不一致时一部分写入撞 3819,或 CHECK 根本钉不住应用算出的片。
	shards := strconv.FormatUint(uint64(placement.RouteRowShards), 10)
	for _, want := range []string{
		"`route_shard` tinyint unsigned NOT NULL COMMENT 'pb:10'",
		"PRIMARY KEY (`route_shard`, `player_id`) /*T![clustered_index] CLUSTERED */",
		"KEY `idx_route_home` (`home_zone`, `route_shard`, `player_id`)",
		"KEY `idx_route_storage` (`storage_id`, `route_shard`, `player_id`)",
		"KEY `idx_route_updated` (`route_shard`, `updated_at_ms`, `player_id`)",
		"CONSTRAINT `chk_route_shard` CHECK (`route_shard` = `player_id` % " + shards + ")",
		"CONSTRAINT `chk_route_home` CHECK (`home_zone` IS NULL OR `home_zone` BETWEEN 1 AND " + strconv.FormatUint(uint64(RouteHomeZoneMax), 10) + ")",
		"CONSTRAINT `chk_route_place` CHECK",
		"CONSTRAINT `chk_route_ver` CHECK",
		"CONSTRAINT `chk_route_frozen` CHECK",
		"CONSTRAINT `chk_route_rev` CHECK (`rev` >= 1)",
	} {
		if !strings.Contains(playerRouteBootstrapDDL, want) {
			t.Fatalf("player_route 的 DDL 丢了 %q", want)
		}
	}
	// 聚簇表不能再用 SHARD_ROW_ID_BITS / PRE_SPLIT_REGIONS;NONCLUSTERED 会让主键退化回单调的唯一索引尾部。
	for _, banned := range []string{"SHARD_ROW_ID_BITS", "PRE_SPLIT_REGIONS", "NONCLUSTERED"} {
		if strings.Contains(playerRouteBootstrapDDL, banned) {
			t.Fatalf("player_route 的 DDL 里出现了 %s(G1a 之后是聚簇 (route_shard, player_id),预切由 route_tool tidb-prepare 做)", banned)
		}
	}
	if n := strings.Count(playerRouteBootstrapDDL, "CONSTRAINT `chk_route_"); n != 6 {
		t.Fatalf("player_route 有 %d 个 chk_route_* 约束,want 6", n)
	}
	if n := strings.Count(playerRouteBootstrapDDL, "KEY `idx_route_"); n != 3 {
		t.Fatalf("player_route 有 %d 个 idx_route_* 索引,want 3", n)
	}
	if !strings.Contains(zoneMergeFenceBootstrapDDL, "PRIMARY KEY (`zone_id`, `shard`) /*T![clustered_index] CLUSTERED */") {
		t.Fatal("zone_merge_fence 的主键丢了 CLUSTERED:TiDB 建表后不能再改聚簇属性,非聚簇就没法按 zone 的值 SPLIT Region(规格 g.2 第 16 条)")
	}

	// route_meta 只留很少写的开关;常写的租约 / 水位在 route_reconcile(G1a 拆表)。
	for _, col := range []string{"watermark_ms", "cache_verified_ms", "leader", "lease_until_ms"} {
		if strings.Contains(routeMetaBootstrapDDL, "`"+col+"`") {
			t.Fatalf("route_meta 的 DDL 里又出现了 %s:每次 W1 都读的开关行不能和 1b leader 每拍写的列放在同一行", col)
		}
		if !strings.Contains(routeReconcileBootstrapDDL, "`"+col+"`") {
			t.Fatalf("route_reconcile 的 DDL 丢了 %s", col)
		}
	}

	// routeTableShapes 的键恰好是这几张表,每一列 / 主键都在对应 DDL 里出现。
	if len(routeTableShapes) != len(ddls) {
		t.Fatalf("routeTableShapes 有 %d 张表,want %d", len(routeTableShapes), len(ddls))
	}
	for name, shape := range routeTableShapes {
		ddl, ok := ddls[name]
		if !ok {
			t.Fatalf("routeTableShapes 里多了 %s", name)
		}
		for _, col := range shape.columns {
			if !strings.Contains(ddl, "`"+col+"` ") {
				t.Fatalf("routeTableShapes[%s] 的列 %s 不在 DDL 里", name, col)
			}
		}
		pk := "PRIMARY KEY (`" + strings.Join(shape.primaryKey, "`, `") + "`)"
		if !strings.Contains(ddl, pk) {
			t.Fatalf("routeTableShapes[%s] 的主键 %v 与 DDL 不符(want %q)", name, shape.primaryKey, pk)
		}
	}

	// 路由表没有 proto,不能出现在 TableMessages 里(否则会被 proto 同步循环与 TestGlobalTableDDLContract 误伤)。
	for _, m := range TableMessages() {
		name, _ := proto2mysql.TableNameFromDescriptor(m.ProtoReflect().Descriptor())
		if _, isRoute := routeTableShapes[name]; isRoute {
			t.Fatalf("路由表 %s 出现在 TableMessages 里", name)
		}
	}
}

// TestRouteShardModulusProbeDiscriminates 钉住建池探针 P5(probeRouteShardModulus)的判别表:按 64 片合法的两行
// (0, 64)、(63, 63),在 CHECK 为 route_shard = player_id % M 的表上,只有 M = 64 时两行都被接受。
// accepts 是对这条 CHECK 的模拟(不连库);另钉住两行在本二进制的模数下合法、片放得进 tinyint unsigned、按聚簇键升序。
func TestRouteShardModulusProbeDiscriminates(t *testing.T) {
	keys := routeShardModulusProbeKeys()
	// 期望写成字面量(64 片):被测函数用的就是 placement.RouteRowShards,期望再引用它等于自己证自己。
	if want := []RouteScanCursor{{Shard: 0, PlayerID: 64}, {Shard: 63, PlayerID: 63}}; !slices.Equal(keys, want) {
		t.Fatalf("routeShardModulusProbeKeys() = %+v, want %+v", keys, want)
	}
	for i, k := range keys {
		if k.Shard != placement.RouteRowShard(k.PlayerID) {
			t.Fatalf("探针第 %d 行 %+v 在本二进制的模数下不合法:正确的库上也会被拒、误报模数不一致", i+1, k)
		}
		if k.Shard > 255 {
			t.Fatalf("探针第 %d 行的片 %d 放不进 route_shard(tinyint unsigned)", i+1, k.Shard)
		}
		if i > 0 && !keys[i-1].Less(k) {
			t.Fatalf("探针行没有按聚簇键升序:%+v 排在 %+v 之后(多副本同时探针时取锁顺序要一致)", k, keys[i-1])
		}
	}

	accepts := func(modulus uint32, k RouteScanCursor) bool { return uint64(k.Shard) == k.PlayerID%uint64(modulus) }
	// 判别表(与 probeRouteShardModulus 注释里的表一致):true = 被接受。
	for _, tc := range []struct {
		modulus uint32
		want    [2]bool // (0, 64)、(63, 63)
	}{
		{16, [2]bool{true, false}},
		{32, [2]bool{true, false}},
		{64, [2]bool{true, true}},
		{128, [2]bool{false, true}},
		{192, [2]bool{false, true}},
		{256, [2]bool{false, true}},
	} {
		if got := [2]bool{accepts(tc.modulus, keys[0]), accepts(tc.modulus, keys[1])}; got != tc.want {
			t.Fatalf("表模数 %d:两行被接受 = %v, want %v", tc.modulus, got, tc.want)
		}
	}
	// 穷举 route_shard 放得下的全部模数 1..256:两行都被接受当且仅当模数就是本二进制的模数 —— 不止上表几个点。
	for m := uint32(1); m <= 256; m++ {
		all := true
		for _, k := range keys {
			all = all && accepts(m, k)
		}
		if want := m == placement.RouteRowShards; all != want {
			t.Fatalf("表模数 %d:两行都被接受 = %t, want %t", m, all, want)
		}
	}
}

// TestOpenRouteStoreHintByCause:建 route store 失败时给运维的提示按原因分流(OpenRouteStoreHint,db_first 的
// data_service、debugutil、route_tool 三处共用)。表按另一个行片模数建的(ErrRouteShardModulusMismatch,经 NewRouteStore
// 包了几层也要认得出)不能再提示「先跑迁移」—— 只重跑 -migrate 修不好它;其余原因保持 route_tool 原来的文案,字节不变。
// 建池失败本身要真库,三个入口的整条路径在这里测不到,只测这个纯函数。
func TestOpenRouteStoreHintByCause(t *testing.T) {
	mismatch := fmt.Errorf("route store on 127.0.0.1:3306/testdb: %w",
		fmt.Errorf("%w: probe row (route_shard=63, player_id=63) is legal under %% 64 but chk_route_shard rejected it", ErrRouteShardModulusMismatch))
	hint := OpenRouteStoreHint("x.yaml", mismatch)
	for _, want := range []string{"another route_shard modulus", "§9.3"} {
		if !strings.Contains(hint, want) {
			t.Fatalf("模数不一致时的提示 %q 缺 %q", hint, want)
		}
	}
	if strings.Contains(hint, "-migrate` first") {
		t.Fatalf("模数不一致时只重跑迁移修不好,不能提示先跑迁移:%q", hint)
	}

	const want = "run `data_service -f x.yaml -migrate` first if route_meta is missing"
	for _, cause := range []error{ErrRouteMetaMissing, ErrRouteCheckNotEnforced, errors.New("dial tcp: connection refused")} {
		if got := OpenRouteStoreHint("x.yaml", cause); got != want {
			t.Fatalf("OpenRouteStoreHint(%v) = %q, want %q", cause, got, want)
		}
	}
}

// allRouteSQLForTest 是本 store 发出的全部语句(动态语句取几个代表长度;tidb-prepare 的语句取一组代表入参)。
func allRouteSQLForTest() map[string]string {
	out := map[string]string{
		"probe_session":   routeProbeSessionSQL,
		"probe_txn_mode":  routeProbeTiDBTxnModeSQL,
		"R1":              routeReadWritableSQL,
		"R2":              routeLockFenceShardSQL,
		"R3":              routeEnsureFenceShardSQL,
		"R4":              routeInsertNewSQL,
		"R5":              routeLockRowSQL,
		"R6a":             routeFillHomeSQL,
		"R6b":             routeFillHomePlacementSQL,
		"R7(1)":           routeImportInsertSQL(1),
		"R7(3)":           routeImportInsertSQL(3),
		"R8(2)":           routeSelectRowsSQL(2, true),
		"R8ro(2)":         routeSelectRowsSQL(2, false),
		"R9":              routeImportFillSQL,
		"R10":             routeInsertMergedIntoSQL,
		"R11":             routeLockMergedIntoSQL,
		"R11ro":           routeReadMergedIntoSQL,
		"R12":             routeScanSQL,
		"R14":             routeListMergedIntoSQL,
		"R15":             routeMetaSeedSQL,
		"R16":             routeReconcileSeedSQL,
		"P4":              routeProbeShardCheckSQL,
		"P5":              routeProbeShardModulusSQL,
		"T1":              routeTiDBPKTypeSQL,
		"ddl_route":       playerRouteBootstrapDDL,
		"ddl_fence":       zoneMergeFenceBootstrapDDL,
		"ddl_merged_into": zoneMergedIntoBootstrapDDL,
		"ddl_meta":        routeMetaBootstrapDDL,
		"ddl_reconcile":   routeReconcileBootstrapDDL,
	}
	for i, stmt := range routeTiDBPrepareStatements([]uint32{1, 2}, []uint32{1000000}) {
		out["T2/T3("+strconv.Itoa(i)+")"] = stmt
	}
	return out
}

// TestRouteSQLNeverUsesInsertIgnore:INSERT IGNORE 会把 CHECK 违例降成 warning(提案 §4.1-3),
// 坏值(home_zone=0、只有 storage 没有 version)就静默进库了。新行一律 ODKU no-op。
func TestRouteSQLNeverUsesInsertIgnore(t *testing.T) {
	ws := regexp.MustCompile(`\s+`)
	for name, stmt := range allRouteSQLForTest() {
		norm := ws.ReplaceAllString(strings.ToUpper(stmt), " ")
		if strings.Contains(norm, "INSERT IGNORE") {
			t.Fatalf("%s 用了 INSERT IGNORE:%q", name, stmt)
		}
		if strings.Contains(norm, "REPLACE INTO") {
			t.Fatalf("%s 用了 REPLACE INTO(先删后插,会把 rev / created_at_ms 一起抹掉):%q", name, stmt)
		}
	}
}

// TestRouteODKUClausesAreNoOp:每条 ODKU 的更新子句都必须是 no-op(引用已有行自己的列值,不是 VALUES())。
// 一旦它真改了数据,受影响行数会变成 2,「1=插入 / 0=撞键」的口径就失效 —— 判定会安静地错。
func TestRouteODKUClausesAreNoOp(t *testing.T) {
	for _, tc := range []struct {
		name, stmt, clause string
	}{
		{"R3", routeEnsureFenceShardSQL, "zone_id = zone_id"},
		{"R4", routeInsertNewSQL, "player_id = player_id"},
		{"R7", routeImportInsertSQL(2), "player_id = player_id"},
		{"R10", routeInsertMergedIntoSQL, "src_zone = src_zone"},
		{"R15", routeMetaSeedSQL, "id = id"},
		{"R16", routeReconcileSeedSQL, "id = id"},
	} {
		const marker = "ON DUPLICATE KEY UPDATE"
		idx := strings.Index(tc.stmt, marker)
		if idx < 0 {
			t.Fatalf("%s 不再是 ODKU:%q", tc.name, tc.stmt)
		}
		if got := strings.TrimSpace(tc.stmt[idx+len(marker):]); got != tc.clause {
			t.Fatalf("%s 的更新子句是 %q,必须保持 no-op 的 %q", tc.name, got, tc.clause)
		}
		if strings.Contains(strings.ToUpper(tc.stmt), "VALUES(") {
			t.Fatalf("%s 引用了 VALUES():那会真的改数据,受影响行数变 2", tc.name)
		}
	}
}

// TestNoPlayerRouteWritesOutsideRouteStore:player_route 的写入只许出现在 internal/store/route_store.go
// (提案 §4.1-6「防绕过」)。绕过它的写者不走 rev、不走围栏、不发布,缓存与库会静默分叉。
//
// 用 go/parser 只检查**字符串字面量**(先 unquote),不看注释 —— 注释里讲解 SQL 是正当的。
// 局限:跨字面量拼接出来的语句("UPDATE " + table)查不到;生成代码(generated/)与 _test.go 不检查。
func TestNoPlayerRouteWritesOutsideRouteStore(t *testing.T) {
	root, err := filepath.Abs(filepath.Join("..", ".."))
	if err != nil {
		t.Fatalf("resolve go/data_service root: %v", err)
	}
	write := regexp.MustCompile("(?i)(INSERT\\s+INTO|UPDATE|DELETE\\s+FROM|REPLACE\\s+INTO)\\s+`?" + PlayerRouteTableName + "`?")
	const allowed = "internal/store/route_store.go"

	var files, allowedHits int
	var offenders []string
	err = filepath.WalkDir(root, func(path string, d fs.DirEntry, walkErr error) error {
		if walkErr != nil {
			return walkErr
		}
		if d.IsDir() {
			switch d.Name() {
			case "generated", "vendor", "testdata":
				return filepath.SkipDir
			}
			return nil
		}
		if !strings.HasSuffix(path, ".go") || strings.HasSuffix(path, "_test.go") {
			return nil
		}
		rel, err := filepath.Rel(root, path)
		if err != nil {
			return err
		}
		rel = filepath.ToSlash(rel)
		file, err := parser.ParseFile(token.NewFileSet(), path, nil, parser.SkipObjectResolution)
		if err != nil {
			return err
		}
		files++
		ast.Inspect(file, func(n ast.Node) bool {
			lit, ok := n.(*ast.BasicLit)
			if !ok || lit.Kind != token.STRING {
				return true
			}
			s, err := strconv.Unquote(lit.Value)
			if err != nil || !write.MatchString(s) {
				return true
			}
			if rel == allowed {
				allowedHits++
			} else {
				offenders = append(offenders, rel+": "+s)
			}
			return true
		})
		return nil
	})
	if err != nil {
		t.Fatalf("walk %s: %v", root, err)
	}
	if files == 0 {
		t.Fatalf("在 %s 下一个 .go 文件都没扫到:守卫失效", root)
	}
	if allowedHits == 0 {
		t.Fatalf("%s 里一条 player_route 写语句都没匹配到:正则失效,本守卫已经拦不住任何东西", allowed)
	}
	if len(offenders) > 0 {
		t.Fatalf("player_route 的写入只许出现在 %s(提案 §4.1-6),以下位置绕过了它:\n%s",
			allowed, strings.Join(offenders, "\n"))
	}
}

// TestDecideImport 覆盖规格 b.2 判定表的每一格,加上 rev 规则。
func TestDecideImport(t *testing.T) {
	const player uint64 = 90001
	stable := func(s uint32, v uint64) *placement.Record { return &placement.Record{StorageID: s, Version: v} }
	frozen := &placement.Record{StorageID: 7, Version: 1, Frozen: true, RunID: "run-a"}
	dbRow := func(home, storage uint32, version uint64, frozenRun string, rev uint64) *RouteRow {
		return &RouteRow{PlayerID: player, HomeZone: home, StorageID: storage, PlacementVersion: version,
			FrozenRunID: frozenRun, Rev: rev, UpdatedBy: "old", CreatedAtMs: 11, UpdatedAtMs: 22}
	}

	for _, tc := range []struct {
		name    string
		db      *RouteRow
		redis   RouteImportRow
		want    importAction
		wantRow RouteRow // 只对 insert / fill 有意义
	}{
		{
			name:    "库无行_无maprev_rev取1",
			redis:   RouteImportRow{PlayerID: player, HomeZone: 7, Placement: stable(7, 1)},
			want:    importInsert,
			wantRow: RouteRow{PlayerID: player, HomeZone: 7, StorageID: 7, PlacementVersion: 1, Rev: 1},
		},
		{
			name:    "库无行_rev取maprev",
			redis:   RouteImportRow{PlayerID: player, HomeZone: 7, MapRev: 5},
			want:    importInsert,
			wantRow: RouteRow{PlayerID: player, HomeZone: 7, Rev: 5},
		},
		{
			name:    "库无行_冻结落点原样插入",
			redis:   RouteImportRow{PlayerID: player, Placement: frozen},
			want:    importInsert,
			wantRow: RouteRow{PlayerID: player, StorageID: 7, PlacementVersion: 1, FrozenRunID: "run-a", Rev: 1},
		},
		{
			name:  "maprev大于库rev_疑似回档_不看列",
			db:    dbRow(7, 0, 0, "", 2),
			redis: RouteImportRow{PlayerID: player, HomeZone: 7, MapRev: 5},
			want:  importRevRegression,
		},
		{
			name:  "两列相等_match",
			db:    dbRow(7, 7, 1, "", 5),
			redis: RouteImportRow{PlayerID: player, HomeZone: 7, Placement: stable(7, 1), MapRev: 5},
			want:  importMatch,
		},
		{
			name:  "maprev小于库rev_按列比_match",
			db:    dbRow(7, 0, 0, "", 9),
			redis: RouteImportRow{PlayerID: player, HomeZone: 7, MapRev: 3},
			want:  importMatch,
		},
		{
			name:    "home库NULL_Redis有_fill",
			db:      dbRow(0, 7, 1, "", 2),
			redis:   RouteImportRow{PlayerID: player, HomeZone: 7, Placement: stable(7, 1)},
			want:    importFill,
			wantRow: RouteRow{PlayerID: player, HomeZone: 7, StorageID: 7, PlacementVersion: 1, Rev: 2, UpdatedBy: "old", CreatedAtMs: 11, UpdatedAtMs: 22},
		},
		{
			name:    "落点库NULL_Redis有_fill",
			db:      dbRow(7, 0, 0, "", 2),
			redis:   RouteImportRow{PlayerID: player, HomeZone: 7, Placement: frozen},
			want:    importFill,
			wantRow: RouteRow{PlayerID: player, HomeZone: 7, StorageID: 7, PlacementVersion: 1, FrozenRunID: "run-a", Rev: 2, UpdatedBy: "old", CreatedAtMs: 11, UpdatedAtMs: 22},
		},
		{
			name:  "home库有_Redis无键_dbAhead",
			db:    dbRow(7, 7, 1, "", 2),
			redis: RouteImportRow{PlayerID: player, Placement: stable(7, 1)},
			want:  importDBAhead,
		},
		{
			name:  "落点库有_Redis无键_dbAhead",
			db:    dbRow(7, 7, 1, "", 2),
			redis: RouteImportRow{PlayerID: player, HomeZone: 7},
			want:  importDBAhead,
		},
		{
			name:  "home两边不同_conflict",
			db:    dbRow(8, 0, 0, "", 2),
			redis: RouteImportRow{PlayerID: player, HomeZone: 7},
			want:  importConflict,
		},
		{
			name:  "落点稳定态与冻结态视为不同_conflict",
			db:    dbRow(7, 7, 1, "", 2),
			redis: RouteImportRow{PlayerID: player, HomeZone: 7, Placement: frozen},
			want:  importConflict,
		},
		{
			name:    "home要补_落点库领先_fill且保留库落点",
			db:      dbRow(0, 7, 3, "", 4),
			redis:   RouteImportRow{PlayerID: player, HomeZone: 7},
			want:    importFill,
			wantRow: RouteRow{PlayerID: player, HomeZone: 7, StorageID: 7, PlacementVersion: 3, Rev: 4, UpdatedBy: "old", CreatedAtMs: 11, UpdatedAtMs: 22},
		},
		{
			name:  "一列conflict一列fill_整行conflict",
			db:    dbRow(8, 0, 0, "", 2),
			redis: RouteImportRow{PlayerID: player, HomeZone: 7, Placement: stable(7, 1)},
			want:  importConflict,
		},
		{
			name:    "库里是墓碑_补home",
			db:      dbRow(0, 0, 0, "", 3),
			redis:   RouteImportRow{PlayerID: player, HomeZone: 7},
			want:    importFill,
			wantRow: RouteRow{PlayerID: player, HomeZone: 7, Rev: 3, UpdatedBy: "old", CreatedAtMs: 11, UpdatedAtMs: 22},
		},
		{
			name:  "库里的落点本身不合法_按conflict报给人",
			db:    dbRow(7, 7, 0, "", 2),
			redis: RouteImportRow{PlayerID: player, HomeZone: 7, Placement: stable(7, 1)},
			want:  importConflict,
		},
	} {
		t.Run(tc.name, func(t *testing.T) {
			got := decideImport(tc.db, tc.redis)
			if got.action != tc.want {
				t.Fatalf("decideImport = %s, want %s", got.action, tc.want)
			}
			if (tc.want == importInsert || tc.want == importFill) && got.row != tc.wantRow {
				t.Fatalf("decideImport row = %+v\nwant %+v", got.row, tc.wantRow)
			}
		})
	}
}

// TestValidateRouteImportRows:批次乱序 / 重复 / 0 / 越界 / 空行 / 坏落点 / 超限,一律 ErrRouteInvalidArgument。
func TestValidateRouteImportRows(t *testing.T) {
	ok := []RouteImportRow{{PlayerID: 1, HomeZone: 7}, {PlayerID: 2, Placement: &placement.Record{StorageID: 7, Version: 1}}}
	if err := validateRouteImportRows(ok); err != nil {
		t.Fatalf("合法批次被拒: %v", err)
	}
	if err := validateRouteImportRows(nil); err != nil {
		t.Fatalf("空批次应合法: %v", err)
	}
	for _, tc := range []struct {
		name string
		rows []RouteImportRow
	}{
		{"乱序", []RouteImportRow{{PlayerID: 2, HomeZone: 7}, {PlayerID: 1, HomeZone: 7}}},
		{"重复", []RouteImportRow{{PlayerID: 1, HomeZone: 7}, {PlayerID: 1, HomeZone: 7}}},
		{"player_id为0", []RouteImportRow{{PlayerID: 0, HomeZone: 7}}},
		{"home越界", []RouteImportRow{{PlayerID: 1, HomeZone: RouteHomeZoneMax + 1}}},
		{"既无home也无落点", []RouteImportRow{{PlayerID: 1, MapRev: 3}}},
		{"落点不合法", []RouteImportRow{{PlayerID: 1, Placement: &placement.Record{StorageID: 7}}}},
		{"超限", make([]RouteImportRow, RouteBatchLimit+1)},
	} {
		if err := validateRouteImportRows(tc.rows); !errors.Is(err, ErrRouteInvalidArgument) {
			t.Fatalf("%s: err = %v, want ErrRouteInvalidArgument", tc.name, err)
		}
	}
}

// TestRouteRowPlacementValue:行的落点三列 → 缓存值。坏行必须报 ErrMalformed,绝不发布垃圾。
func TestRouteRowPlacementValue(t *testing.T) {
	for _, tc := range []struct {
		name    string
		row     RouteRow
		want    string
		wantErr bool
	}{
		{"无落点", RouteRow{PlayerID: 1, HomeZone: 7}, "", false},
		{"稳定态", RouteRow{PlayerID: 1, StorageID: 1000000, PlacementVersion: 3}, "1000000:3", false},
		{"冻结态", RouteRow{PlayerID: 1, StorageID: 102, PlacementVersion: 3, FrozenRunID: "run-1"}, "102:3:frozen:run-1", false},
		{"version为0", RouteRow{PlayerID: 1, StorageID: 102}, "", true},
		{"冻结run_id不合法", RouteRow{PlayerID: 1, StorageID: 102, PlacementVersion: 1, FrozenRunID: "bad run"}, "", true},
		{"没有storage却带version", RouteRow{PlayerID: 1, PlacementVersion: 3}, "", true},
		{"没有storage却带run_id", RouteRow{PlayerID: 1, FrozenRunID: "run-1"}, "", true},
	} {
		t.Run(tc.name, func(t *testing.T) {
			got, err := tc.row.PlacementValue()
			if tc.wantErr {
				if !errors.Is(err, placement.ErrMalformed) {
					t.Fatalf("PlacementValue() = (%q, %v), want ErrMalformed", got, err)
				}
				return
			}
			if err != nil || got != tc.want {
				t.Fatalf("PlacementValue() = (%q, %v), want (%q, nil)", got, err, tc.want)
			}
		})
	}

	if got := (RouteRow{HomeZone: 0}).HomeValue(); got != "" {
		t.Fatalf("HomeValue of NULL home = %q, want empty", got)
	}
	if got := (RouteRow{HomeZone: 102}).HomeValue(); got != "102" {
		t.Fatalf("HomeValue = %q, want 102", got)
	}
	if !(RouteRow{}).IsTombstone() || (RouteRow{HomeZone: 1}).IsTombstone() || (RouteRow{StorageID: 1, PlacementVersion: 1}).IsTombstone() {
		t.Fatal("IsTombstone 的口径是「home 与落点都 NULL」")
	}
}

// TestNewRouteWriterID:写者标识只留 [A-Za-z0-9._@:-]、不超过 updated_by 列宽,host 为空时用 unknown。
func TestNewRouteWriterID(t *testing.T) {
	for _, tc := range []struct {
		name, component, host, want string
	}{
		{"常规", "data_service", "ds-0.svc:8080", "data_service@ds-0.svc:8080"},
		{"非ASCII被清洗", "data_service", "主机-1", "data_service@-1"},
		{"空白与斜杠被清洗", "route tool", "a/b c", "routetool@abc"},
		{"host为空", "data_service", "", "data_service@unknown"},
		{"host洗完为空", "data_service", "主机", "data_service@unknown"},
	} {
		got := NewRouteWriterID(tc.component, tc.host)
		if got != tc.want {
			t.Fatalf("%s: NewRouteWriterID(%q, %q) = %q, want %q", tc.name, tc.component, tc.host, got, tc.want)
		}
		if !validRouteWriter(got) {
			t.Fatalf("%s: NewRouteWriterID 的产物 %q 过不了 validRouteWriter", tc.name, got)
		}
	}
	long := NewRouteWriterID("data_service", strings.Repeat("h", 300))
	if len(long) != routeWriterMaxLen || !strings.HasPrefix(long, "data_service@hhh") || !validRouteWriter(long) {
		t.Fatalf("超长 host 没被截到 %d 字节:len=%d %q", routeWriterMaxLen, len(long), long)
	}
}

// TestRouteStoreOptionsValidate:分片数只接受 [1, 64],写者必须是 NewRouteWriterID 的形状。
func TestRouteStoreOptionsValidate(t *testing.T) {
	good := RouteStoreOptions{FenceShards: placement.DefaultRouteFenceShards, Writer: NewRouteWriterID("data_service", "h")}
	if err := good.validate(); err != nil {
		t.Fatalf("合法选项被拒: %v", err)
	}
	for _, tc := range []struct {
		name string
		opts RouteStoreOptions
	}{
		{"分片数0", RouteStoreOptions{FenceShards: 0, Writer: good.Writer}},
		{"分片数65", RouteStoreOptions{FenceShards: placement.MaxRouteFenceShards + 1, Writer: good.Writer}},
		{"写者为空", RouteStoreOptions{FenceShards: 8}},
		{"写者含非法字符", RouteStoreOptions{FenceShards: 8, Writer: "a b"}},
		{"写者超长", RouteStoreOptions{FenceShards: 8, Writer: strings.Repeat("w", routeWriterMaxLen+1)}},
	} {
		if err := tc.opts.validate(); !errors.Is(err, ErrRouteInvalidArgument) {
			t.Fatalf("%s: err = %v, want ErrRouteInvalidArgument", tc.name, err)
		}
	}
}

// TestRouteRegisterOutcomeStringsAreStableLowCardinality:String() 进日志与指标 label,值必须稳定且低基数。
func TestRouteRegisterOutcomeStringsAreStableLowCardinality(t *testing.T) {
	for _, tc := range []struct {
		got, want string
	}{
		{RouteRegisterInserted.String(), "inserted"},
		{RouteRegisterFilledHome.String(), "filled_home"},
		{RouteRegisterFilledHomePlacementKept.String(), "filled_home_placement_kept"},
		{RouteRegisterAlreadySame.String(), "already_same"},
		{RouteRegisterConflict.String(), "conflict"},
		{RouteRegisterFenced.String(), "fenced"},
		{RouteRegisterRowMissing.String(), "row_missing"},
		{RouteRegisterOutcome(0).String(), "unknown"},
	} {
		if tc.got != tc.want {
			t.Fatalf("outcome label = %q, want %q", tc.got, tc.want)
		}
	}
}

// TestRouteLockRetryBudget:1213 / 9007 的重试最坏多睡 (attempts-1)×Max,必须远小于服务端 2s 预算。
func TestRouteLockRetryBudget(t *testing.T) {
	if routeTxAttempts < 2 {
		t.Fatalf("routeTxAttempts=%d:至少要能重来一次,1213 / 9007 的被牺牲方才能收敛", routeTxAttempts)
	}
	for i := 0; i < 1000; i++ {
		d := jitteredBackoff(routeLockRetryBackoffMin, routeLockRetryBackoffMax)
		if d < routeLockRetryBackoffMin || d > routeLockRetryBackoffMax {
			t.Fatalf("第 %d 次退避 %v 超出 [%v, %v]", i, d, routeLockRetryBackoffMin, routeLockRetryBackoffMax)
		}
	}
	if worst := (routeTxAttempts - 1) * routeLockRetryBackoffMax; worst.Milliseconds() > 200 {
		t.Fatalf("锁冲突重试最坏多睡 %v,超过 200ms:会吃掉 W1 服务端 2s 预算的一成以上", worst)
	}
}

// TestRouteScanCursorOrder:键集游标与排序都按聚簇键 (route_shard, player_id) —— 先比片、后比 id;
// 片只经 placement.RouteRowShard 算;零值游标小于任何合法键(player_id 为 0 的行不合法)。
func TestRouteScanCursorOrder(t *testing.T) {
	for _, tc := range []struct {
		a, b RouteScanCursor
		less bool
	}{
		// 字面量取 64 片下真实存在的键:1 与 129(= 2×64 + 1)在片 1,2 在片 2。
		{RouteScanCursor{Shard: 1, PlayerID: 129}, RouteScanCursor{Shard: 2, PlayerID: 2}, true}, // 片小者在前,不看 id
		{RouteScanCursor{Shard: 2, PlayerID: 2}, RouteScanCursor{Shard: 1, PlayerID: 129}, false},
		{RouteScanCursor{Shard: 1, PlayerID: 1}, RouteScanCursor{Shard: 1, PlayerID: 129}, true}, // 同片按 id
		{RouteScanCursor{Shard: 1, PlayerID: 129}, RouteScanCursor{Shard: 1, PlayerID: 129}, false},
	} {
		if got := tc.a.Less(tc.b); got != tc.less {
			t.Fatalf("%+v.Less(%+v) = %t, want %t", tc.a, tc.b, got, tc.less)
		}
	}
	for _, id := range []uint64{1, 15, 16, 17, 63, 64, 65, 4242, 1<<55 - 1} {
		key := RouteScanCursorAt(RouteRow{PlayerID: id})
		if key != (RouteScanCursor{Shard: placement.RouteRowShard(id), PlayerID: id}) {
			t.Fatalf("RouteScanCursorAt(%d) = %+v:片必须经 placement.RouteRowShard 算", id, key)
		}
		if !(RouteScanCursor{}).Less(key) {
			t.Fatalf("零值游标不小于 %+v:从头扫会漏掉这一行", key)
		}
	}

	// 64 片下:64 在片 0;1、65(64 + 1)、129(2×64 + 1)在片 1;2、66(64 + 2)在片 2。
	// 相差 64 才落同一片 —— 换成相差 16 的 id(17、33、18)在 64 片下各占一片,这条就只剩「按 id 排」了。
	ids := []uint64{2, 65, 129, 1, 66, 64}
	sortRouteIDsByKey(ids)
	if want := []uint64{64, 1, 65, 129, 2, 66}; !slices.Equal(ids, want) {
		t.Fatalf("sortRouteIDsByKey = %v, want %v(先片后 id)", ids, want)
	}
}

// TestRouteTiDBPrepareStatements:tidb-prepare 的语句逐字钉住(TiDB 上的写法未核实,改它要先在真 TiDB 上取证,
// 见 route_tidb.go 文件头),以及入参规范化(去重升序、越界拒绝)。
func TestRouteTiDBPrepareStatements(t *testing.T) {
	// 行数据与 idx_route_updated 每片一个切点:片 1..63(片 0 的起点就是表 / 索引的起点,不用切),共 63 个、64 段。
	// 63 写成字面量(= 64 − 1),故意不引用 placement.RouteRowShards:被测函数用的就是那个常量,期望再引用它等于自己证自己。
	perShard := func(suffix string) string {
		parts := make([]string, 0, 63)
		for sh := 1; sh <= 63; sh++ {
			parts = append(parts, "("+strconv.Itoa(sh)+suffix+")")
		}
		return strings.Join(parts, ", ")
	}
	// idx_route_home / idx_route_storage 每个 zone / 落点值 8 个切点:片 0、8、…、56(每段 8 片),逐字写出。
	want := []string{
		"ALTER TABLE `player_route` ATTRIBUTES 'merge_option=deny'",
		"ALTER TABLE `zone_merge_fence` ATTRIBUTES 'merge_option=deny'",
		"SPLIT TABLE `player_route` BY " + perShard(", 0"),
		"SPLIT TABLE `player_route` INDEX `idx_route_updated` BY " + perShard(", 0, 0"),
		"SPLIT TABLE `player_route` INDEX `idx_route_home` BY (1, 0, 0), (1, 8, 0), (1, 16, 0), (1, 24, 0), " +
			"(1, 32, 0), (1, 40, 0), (1, 48, 0), (1, 56, 0)",
		"SPLIT TABLE `player_route` INDEX `idx_route_home` BY (2, 0, 0), (2, 8, 0), (2, 16, 0), (2, 24, 0), " +
			"(2, 32, 0), (2, 40, 0), (2, 48, 0), (2, 56, 0)",
		"SPLIT TABLE `player_route` INDEX `idx_route_storage` BY (1000000, 0, 0), (1000000, 8, 0), (1000000, 16, 0), " +
			"(1000000, 24, 0), (1000000, 32, 0), (1000000, 40, 0), (1000000, 48, 0), (1000000, 56, 0)",
		"SPLIT TABLE `zone_merge_fence` BY (1, 0), (2, 0)",
	}
	// 期望自检:perShard 拼出来的头尾与个数(免得上面的循环自己写错了还和被测函数错到一处去)。
	if !strings.HasPrefix(want[2], "SPLIT TABLE `player_route` BY (1, 0), (2, 0), (3, 0), ") ||
		!strings.HasSuffix(want[2], ", (62, 0), (63, 0)") || strings.Count(want[2], "(") != 63 {
		t.Fatalf("用例自己的期望拼错了(行数据):%q", want[2])
	}
	if !strings.HasPrefix(want[3], "SPLIT TABLE `player_route` INDEX `idx_route_updated` BY (1, 0, 0), (2, 0, 0), ") ||
		!strings.HasSuffix(want[3], ", (62, 0, 0), (63, 0, 0)") || strings.Count(want[3], "(") != 63 {
		t.Fatalf("用例自己的期望拼错了(idx_route_updated):%q", want[3])
	}
	got := routeTiDBPrepareStatements([]uint32{1, 2}, []uint32{1000000})
	if len(got) != len(want) {
		t.Fatalf("routeTiDBPrepareStatements 给了 %d 条,want %d:\n%s", len(got), len(want), strings.Join(got, "\n"))
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("第 %d 条:\n got=%q\nwant=%q", i, got[i], want[i])
		}
	}
	if n := len(routeTiDBPrepareStatements([]uint32{7}, nil)); n != 2+2+1+0+1 {
		t.Fatalf("1 个 zone、0 个 storage 应生成 6 条语句,got %d", n)
	}
	// 切点密度的两个常量:8 个点、间隔 8 片(64 / 8);切点数必须整除行片数(route_tidb.go 另有编译期断言)。
	if routeTiDBPrefixSplitPoints != 8 || routeTiDBPrefixSplitStride != 8 || placement.RouteRowShards%routeTiDBPrefixSplitPoints != 0 {
		t.Fatalf("切点数 / 间隔 = %d / %d(行片 %d),want 8 / 8 且切点数整除行片数:改密度要同步容量模型 §4.1 与提案 H8",
			routeTiDBPrefixSplitPoints, routeTiDBPrefixSplitStride, placement.RouteRowShards)
	}

	zones, storages, err := normalizeRouteTiDBPrepareIDs([]uint32{3, 1, 3}, []uint32{1000000, 1000000})
	if err != nil || !slices.Equal(zones, []uint32{1, 3}) || !slices.Equal(storages, []uint32{1000000}) {
		t.Fatalf("normalize({3,1,3}, {1000000,1000000}) = (%v, %v, %v), want ({1,3}, {1000000}, nil)", zones, storages, err)
	}
	if _, storages, err := normalizeRouteTiDBPrepareIDs([]uint32{1}, nil); err != nil || len(storages) != 0 {
		t.Fatalf("storages 可空: (%v, %v)", storages, err)
	}
	tooMany := make([]uint32, RouteBatchLimit+1)
	for i := range tooMany {
		tooMany[i] = uint32(i + 1)
	}
	for _, tc := range []struct {
		name            string
		zones, storages []uint32
	}{
		{"zone为0", []uint32{0}, nil},
		{"zone越界", []uint32{RouteHomeZoneMax + 1}, nil},
		{"没有zone", nil, nil},
		{"zone超过上限", tooMany, nil},
		{"storage为0", []uint32{1}, []uint32{0}},
	} {
		if _, _, err := normalizeRouteTiDBPrepareIDs(tc.zones, tc.storages); !errors.Is(err, ErrRouteInvalidArgument) {
			t.Fatalf("%s: err = %v, want ErrRouteInvalidArgument", tc.name, err)
		}
	}
}
