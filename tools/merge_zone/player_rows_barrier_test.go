package main

// r2-mz#1(2026-10-20 修复)的回归:前向 copy 合服保留 player:{id}:save_barrier,撤销照删;-verify-merged 的
// verify:save_barrier 逐人核对屏障与两表行内围栏。本 module 没有 miniredis / sqlmock(见 gap_fixes_test.go 头注释):
// Redis 一侧用 route_publish_test.go 的 newFakeRouteRedis(go-redis 钩子,不拨号),SQL 一侧经 rowFenceReader 注入替身。
// 真 MySQL / Redis 上的读口径(行围栏 SELECT、HMGET 只取三字段)见 save_barrier_integration_test.go(build tag merge_integration)。

import (
	"context"
	"errors"
	"go/ast"
	"go/parser"
	"go/token"
	"sort"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/redis/go-redis/v9"
)

var barrierTestTables = []string{"player_centre_database", "player_database", "player_database_1"}

// ── 删除清单 ──────────────────────────────────────────────────

func TestPlayerCacheKeysKeepBarrier_IsTheUnmergeListMinusTheBarrier(t *testing.T) {
	keep := playerCacheKeysKeepBarrier(42, barrierTestTables)
	all := playerCacheKeys(42, barrierTestTables)
	if len(all) != len(keep)+1 || all[len(all)-1] != "player:{42}:save_barrier" {
		t.Fatalf("unmerge list %v must be the forward-merge list %v plus player:{42}:save_barrier", all, keep)
	}
	for i := range keep {
		if all[i] != keep[i] {
			t.Fatalf("the two lists diverge at %d: %q vs %q", i, all[i], keep[i])
		}
	}
	has := map[string]bool{}
	for _, k := range keep {
		has[k] = true
	}
	if has["player:{42}:save_barrier"] {
		t.Fatal("the forward copy merge must not delete the save barrier")
	}
	// 缓存与资产证据照删:屏障是唯一的例外。
	for _, want := range []string{"PlayerAllData:42", "player_database:42", "player_database_1:42", "player:{42}:asset_sql_commit"} {
		if !has[want] {
			t.Errorf("forward-merge list lost %q: %v", want, keep)
		}
	}
}

// deletedKeys 收集 fake Redis 收到的全部 DEL 键(按到达顺序)。
func deletedKeys(f *fakeRouteRedis) []string {
	f.mu.Lock()
	defer f.mu.Unlock()
	var out []string
	for _, p := range f.pipelines {
		for _, c := range p {
			if c.Name() == "del" {
				out = append(out, c.Args()[1].(string))
			}
		}
	}
	return out
}

func delOneEach(_ int, cmd redis.Cmder) (any, error) {
	if cmd.Name() != "del" {
		return nil, errors.New("unexpected command " + cmd.Name())
	}
	return int64(1), nil
}

func TestInvalidatePlayerCachesKeepBarrier_ForwardCopyLeavesTheBarrier(t *testing.T) {
	ctx := context.Background()
	ids := []uint64{42, 43}

	rdb, f := newFakeRouteRedis(t, delOneEach)
	n, err := invalidatePlayerCachesKeepBarrier(ctx, rdb, ids, barrierTestTables, false)
	if err != nil {
		t.Fatal(err)
	}
	keys := deletedKeys(f)
	if n != len(keys) || n != 2*len(playerCacheKeysKeepBarrier(42, barrierTestTables)) {
		t.Fatalf("deleted=%d keys=%v", n, keys)
	}
	for _, k := range keys {
		if strings.HasSuffix(k, ":save_barrier") {
			t.Fatalf("forward copy merge deleted %s — owner_epoch≠0 players would be locked out of the cold load", k)
		}
	}
	for _, want := range []string{"PlayerAllData:43", "player:{43}:asset_sql_commit"} {
		if !containsString(keys, want) {
			t.Errorf("forward copy merge must still delete %s (got %v)", want, keys)
		}
	}

	// 撤销:同一组缓存 + 屏障。
	rdb, f = newFakeRouteRedis(t, delOneEach)
	if _, err := invalidatePlayerCaches(ctx, rdb, ids, barrierTestTables, false); err != nil {
		t.Fatal(err)
	}
	keys = deletedKeys(f)
	for _, want := range []string{"player:{42}:save_barrier", "player:{43}:save_barrier", "PlayerAllData:42"} {
		if !containsString(keys, want) {
			t.Errorf("unmerge must delete %s (got %v)", want, keys)
		}
	}

	// dry-run 不发命令,只计数。
	rdb, f = newFakeRouteRedis(t, delOneEach)
	n, err = invalidatePlayerCachesKeepBarrier(ctx, rdb, ids, barrierTestTables, true)
	if err != nil || f.pipelineCount() != 0 || n != 2*len(playerCacheKeysKeepBarrier(42, barrierTestTables)) {
		t.Fatalf("dry-run: n=%d err=%v pipelines=%d", n, err, f.pipelineCount())
	}
}

func containsString(list []string, s string) bool {
	for _, v := range list {
		if v == s {
			return true
		}
	}
	return false
}

// TestBarrierWiring_CopyMergeKeepsUnmergeDeletes 钉住两处调用点:runMerge / runUnmerge 用 log.Fatalf 收尾,
// 不能在进程内调;这里只证接线没被改回去(-route-source=db / redis 走的是同一个调用点)。
func TestBarrierWiring_CopyMergeKeepsUnmergeDeletes(t *testing.T) {
	for _, c := range []struct {
		file, fn  string
		want, ban string
	}{
		{"merge_run.go", "runMerge", "invalidatePlayerCachesKeepBarrier", "invalidatePlayerCaches"},
		{"unmerge.go", "runUnmerge", "invalidatePlayerCaches", "invalidatePlayerCachesKeepBarrier"},
	} {
		calls := calledFuncs(t, c.file, c.fn)
		if !calls[c.want] || calls[c.ban] {
			t.Errorf("%s: %s must call %s and not %s (calls: %v)", c.file, c.fn, c.want, c.ban, sortedKeys(calls))
		}
	}
}

// calledFuncs 返回 file 里函数 fn 直接调用的标识符集合(只看 ident(...) 形式)。
func calledFuncs(t *testing.T, file, fn string) map[string]bool {
	t.Helper()
	f, err := parser.ParseFile(token.NewFileSet(), file, nil, parser.SkipObjectResolution)
	if err != nil {
		t.Fatalf("parse %s: %v", file, err)
	}
	out := map[string]bool{}
	for _, decl := range f.Decls {
		fd, ok := decl.(*ast.FuncDecl)
		if !ok || fd.Recv != nil || fd.Name.Name != fn {
			continue
		}
		ast.Inspect(fd.Body, func(n ast.Node) bool {
			if call, ok := n.(*ast.CallExpr); ok {
				if id, ok := call.Fun.(*ast.Ident); ok {
					out[id.Name] = true
				}
			}
			return true
		})
		return out
	}
	t.Fatalf("%s: function %s not found", file, fn)
	return nil
}

func sortedKeys(m map[string]bool) []string {
	out := make([]string, 0, len(m))
	for k := range m {
		out = append(out, k)
	}
	sort.Strings(out)
	return out
}

// ── 镜像字面量 ────────────────────────────────────────────────

func TestSaveBarrierMirrorsMatchTheSharedPackages(t *testing.T) {
	// go/shared/ownerepoch.OwnerEpochKey、go/shared/savebarrier.Key / Version 的输出,逐字。
	if got := playerOwnerEpochKey(123); got != "player:123:owner_epoch" {
		t.Errorf("owner_epoch key = %q", got)
	}
	if got := playerSaveBarrierKey("123"); got != "player:{123}:save_barrier" {
		t.Errorf("save_barrier key = %q", got)
	}
	if got := saveBarrierVersion(7, 3); got != "00000000000000000007:00000000000000000003" {
		t.Errorf("version = %q", got)
	}
	if e, s, ok := parseSaveBarrierVersion("18446744073709551615:00000000000000000001"); !ok || e != 18446744073709551615 || s != 1 {
		t.Errorf("max epoch must parse: (%d,%d,%v)", e, s, ok)
	}
	for _, bad := range []string{"", "7:3", "18446744073709551616:00000000000000000001", "0000000000000000000a:00000000000000000001",
		"00000000000000000007-00000000000000000003", "00000000000000000007:000000000000000000003"} {
		if _, _, ok := parseSaveBarrierVersion(bad); ok {
			t.Errorf("%q must not parse", bad)
		}
	}
}

// ── 判定 ──────────────────────────────────────────────────────

func committedObservation(owner string, epoch, seq uint64) saveBarrierObservation {
	v := saveBarrierVersion(epoch, seq)
	return saveBarrierObservation{OwnerRaw: owner, OwnerPresent: true, Pending: v, Acks: [2]string{v, v},
		RowFence: [2]string{v, v}, RowPresent: [2]bool{true, true}}
}

func TestJudgeSaveBarrier_MirrorsTheLoginColdLoad(t *testing.T) {
	stale := saveBarrierVersion(5, 2)
	for _, c := range []struct {
		name string
		edit func(o *saveBarrierObservation)
		want string
	}{
		{"稳态通过", func(*saveBarrierObservation) {}, barrierVerdictOK},
		{"从没进过场景不要求", func(o *saveBarrierObservation) { *o = saveBarrierObservation{} }, barrierVerdictOK},
		{"owner为0不要求", func(o *saveBarrierObservation) { *o = saveBarrierObservation{OwnerRaw: "0", OwnerPresent: true} }, barrierVerdictOK},
		{"owner非规范", func(o *saveBarrierObservation) { o.OwnerRaw = "09" }, barrierVerdictOwnerMalformed},
		{"屏障被删(旧二进制合服)", func(o *saveBarrierObservation) { o.Pending, o.Acks = "", [2]string{} }, barrierVerdictMissing},
		{"只剩ACK没有pending", func(o *saveBarrierObservation) { o.Pending = "" }, barrierVerdictMissing},
		{"pending畸形", func(o *saveBarrierObservation) { o.Pending = "garbage" }, barrierVerdictPendingMalformed},
		{"pending零版本", func(o *saveBarrierObservation) { o.Pending = saveBarrierVersion(0, 0) }, barrierVerdictPendingMalformed},
		{"owner落后pending", func(o *saveBarrierObservation) { o.OwnerRaw = "5" }, barrierVerdictOwnerBehind},
		{"一表ACK未齐", func(o *saveBarrierObservation) { o.Acks[1] = stale }, barrierVerdictUncommitted},
		{"落点记录畸形", func(o *saveBarrierObservation) { o.PlacementBad = true }, barrierVerdictPlacementBad},
		{"目标库缺行", func(o *saveBarrierObservation) { o.RowPresent[1] = false }, barrierVerdictRowMissing},
		{"目标库行围栏与屏障不同版", func(o *saveBarrierObservation) { o.RowFence[0] = stale }, barrierVerdictFenceMismatch},
	} {
		t.Run(c.name, func(t *testing.T) {
			o := committedObservation("9", 8, 90)
			c.edit(&o)
			if got := judgeSaveBarrier(o); got != c.want {
				t.Fatalf("judge = %q, want %q (obs %+v)", got, c.want, o)
			}
		})
	}
}

// ── verify:save_barrier ──────────────────────────────────────

// barrierWorld 是 verify:save_barrier 的替身世界:共享 Redis(owner_epoch / save_barrier)、落点读方、SQL 行围栏。
type barrierWorld struct {
	owners map[uint64]string
	// barriers:屏障 hash 的全部字段(字段名 → 值);没有的玩家 = hash 不存在。HMGET 按请求的字段逐个应答,
	// 所以 hash 里多出来的字段(schema_gen / schema_writer / outbox_*)与真 Redis 一样不影响结果。
	barriers map[uint64]map[string]string
	records  map[uint64]string // 落点记录原值;没有 = 无记录
	fences   map[string]map[uint64]string
	reads    []string   // readFences 收到的 "schema.table"
	hmgets   [][]string // 每条 HMGET 请求的字段
	hmgetErr error
}

// committedBarrier 是 pending 与两表 ACK 同版的屏障 hash(稳态)。
func committedBarrier(v string) map[string]string {
	return map[string]string{saveBarrierPendingField: v, saveBarrierAckTables[0]: v, saveBarrierAckTables[1]: v}
}

func (w *barrierWorld) reply(_ int, cmd redis.Cmder) (any, error) {
	args := cmd.Args()
	switch cmd.Name() {
	case "mget":
		out := make([]any, 0, len(args)-1)
		for _, a := range args[1:] {
			key := a.(string)
			var v any
			for id, owner := range w.owners {
				if key == playerOwnerEpochKey(id) {
					v = owner
				}
			}
			out = append(out, v)
		}
		return out, nil
	case "hmget":
		if w.hmgetErr != nil {
			return nil, w.hmgetErr
		}
		key := args[1].(string)
		fields := make([]string, 0, len(args)-2)
		for _, a := range args[2:] {
			fields = append(fields, a.(string))
		}
		w.hmgets = append(w.hmgets, fields)
		var hash map[string]string
		for id, b := range w.barriers {
			if key == playerSaveBarrierKey(strconv.FormatUint(id, 10)) {
				hash = b
			}
		}
		out := make([]any, len(fields))
		for i, f := range fields {
			if v, ok := hash[f]; ok {
				out[i] = v
			}
		}
		return out, nil
	}
	return nil, errors.New("unexpected command " + cmd.Name())
}

func (w *barrierWorld) readFences(_ context.Context, schema, table string, ids []uint64) (map[uint64]string, error) {
	w.reads = append(w.reads, schema+"."+table)
	out := map[uint64]string{}
	for _, id := range ids {
		if v, ok := w.fences[schema+"."+table][id]; ok {
			out[id] = v
		}
	}
	return out, nil
}

// barrierRoutes 是 routeReadSide 的替身,只实现 placements(verify:save_barrier 只用它)。
type barrierRoutes struct{ w *barrierWorld }

func (r barrierRoutes) placements(_ context.Context, ids []uint64) ([]placementRead, error) {
	out := make([]placementRead, len(ids))
	for i, id := range ids {
		raw, ok := r.w.records[id]
		if !ok {
			continue
		}
		rec, err := parsePlacement(raw)
		out[i] = placementRead{RecordRaw: raw, RecordPresent: true, Record: rec, RecordErr: err}
	}
	return out, nil
}
func (barrierRoutes) playersWithHome(context.Context, uint32) ([]uint64, error) {
	return nil, errors.New("not used by verify:save_barrier")
}
func (barrierRoutes) storagePlayers(context.Context, uint32) (storagePlayers, error) {
	return storagePlayers{}, errors.New("not used by verify:save_barrier")
}
func (barrierRoutes) source() string { return routeSourceRedis }

func (w *barrierWorld) setFence(schema, table string, id uint64, v string) {
	if w.fences == nil {
		w.fences = map[string]map[uint64]string{}
	}
	key := schema + "." + table
	if w.fences[key] == nil {
		w.fences[key] = map[uint64]string{}
	}
	w.fences[key][id] = v
}

// newBarrierWorld:清单 {1,2,3}。1、2 进过场景,屏障稳态;1 无落点记录(copy 拷到 zone_902_db),2 有记录钉在源区
// (copy 模式因已有记录而没拷);3 从没进过场景(owner_epoch 不存在)。
func newBarrierWorld() *barrierWorld {
	v1, v2 := saveBarrierVersion(8, 90), saveBarrierVersion(6, 4)
	w := &barrierWorld{
		owners:   map[uint64]string{1: "9", 2: "6"},
		barriers: map[uint64]map[string]string{1: committedBarrier(v1), 2: committedBarrier(v2)},
		records:  map[uint64]string{2: "901:1"},
	}
	for _, table := range saveBarrierAckTables {
		w.setFence("zone_902_db", table, 1, v1)
		w.setFence("zone_901_db", table, 2, v2)
	}
	return w
}

func barrierVerifyConfig(t *testing.T, w *barrierWorld, stepDone bool) auditConfig {
	t.Helper()
	rdb, _ := newFakeRouteRedis(t, w.reply)
	m := newMergeManifest("r2-mz1", 901, 902, "test", time.Now())
	m.PlayerIDs = []uint64{1, 2, 3}
	m.PlayerRowsMode = playerRowsModeCopy
	if stepDone {
		m.markStep(stepPlayerRows, "x")
	}
	return auditConfig{src: 901, dst: 902, verify: true, manifest: m, sharedRDB: rdb, routes: barrierRoutes{w}}
}

func TestVerifySaveBarriers_PassesWhenEveryBarrierMatchesItsRows(t *testing.T) {
	w := newBarrierWorld()
	r := verifySaveBarriersWith(context.Background(), barrierVerifyConfig(t, w, true), w.readFences)
	if r.Severity != "info" || r.TargetCount != 2 || r.ConflictCount != 0 {
		t.Fatalf("got %+v", r)
	}
	// 行按有效落点去读:无记录者读目标区,有记录者读记录指向的库;两张表都读。
	sort.Strings(w.reads)
	want := []string{"zone_901_db.player_database", "zone_901_db.player_database_1",
		"zone_902_db.player_database", "zone_902_db.player_database_1"}
	if strings.Join(w.reads, ",") != strings.Join(want, ",") {
		t.Errorf("fence reads = %v, want %v", w.reads, want)
	}
}

// TestVerifySaveBarriers_AcceptsSchemaGenAndWriterFields:混版本持久字段保护第 3b 批起,C++ 严格屏障 Lua 会在 hash 里
// 多写 schema_gen(覆盖世代)与 schema_writer(写方令牌)。它们是合法字段:前向 copy 合服原样保留,verify 不按字段集合
// 判,只取 pending 与两表 ACK 三个字段,不得因多出这两个字段报 block。合服后新实体的令牌必然不同、照常做一次世代比较,
// 那是 C++ 存盘时的期望行为,不是 login 冷加载的拒绝条件。
func TestVerifySaveBarriers_AcceptsSchemaGenAndWriterFields(t *testing.T) {
	w := newBarrierWorld()
	w.barriers[1]["schema_gen"] = "3"
	w.barriers[1]["schema_writer"] = "6f1c2a9e-4b7d-4e0a-9c3f-2d8b1e5a7c40"
	w.barriers[2]["schema_gen"] = "1"
	w.barriers[2]["schema_writer"] = "0b9e8d7c-6a5f-4e3d-8c2b-1a0f9e8d7c6b"
	r := verifySaveBarriersWith(context.Background(), barrierVerifyConfig(t, w, true), w.readFences)
	if r.Severity != "info" || r.TargetCount != 2 || r.ConflictCount != 0 {
		t.Fatalf("schema_gen / schema_writer must not fail the check: %+v", r)
	}
	// 每名 owner_epoch≠0 的玩家一条 HMGET(1、2 两人),字段恰为这三个。
	want := strings.Join([]string{saveBarrierPendingField, saveBarrierAckTables[0], saveBarrierAckTables[1]}, ",")
	if len(w.hmgets) != 2 {
		t.Fatalf("want one HMGET per required player, got %v", w.hmgets)
	}
	for _, fields := range w.hmgets {
		if strings.Join(fields, ",") != want {
			t.Fatalf("verify must HMGET exactly %s (got %v)", want, fields)
		}
	}
}

func TestVerifySaveBarriers_BlocksADeletedOrDivergentBarrier(t *testing.T) {
	// 旧二进制合服删过 1 的屏障;2 的目标库行在合服后被改过(围栏前进了,屏障没跟上)。
	w := newBarrierWorld()
	delete(w.barriers, 1)
	w.setFence("zone_901_db", "player_database_1", 2, saveBarrierVersion(7, 1))
	r := verifySaveBarriersWith(context.Background(), barrierVerifyConfig(t, w, true), w.readFences)
	if r.Severity != "block" || isInfraAudit(r) || r.ConflictCount != 2 {
		t.Fatalf("got %+v", r)
	}
	for _, want := range []string{barrierVerdictMissing + "=1 (first ids: [1])", barrierVerdictFenceMismatch + "=1 (first ids: [2])",
		"save_barrier_restore", "§10.1"} {
		if !strings.Contains(r.Notes, want) {
			t.Errorf("notes %q do not contain %q", r.Notes, want)
		}
	}
}

func TestVerifySaveBarriers_NobodyEnteredASceneNeedsNoSQL(t *testing.T) {
	w := newBarrierWorld()
	w.owners = map[uint64]string{2: "0"}
	cfg := barrierVerifyConfig(t, w, true)
	cfg.routes = nil // 不该走到读落点 / 读行
	r := verifySaveBarriersWith(context.Background(), cfg, w.readFences)
	if r.Severity != "info" || len(w.reads) != 0 {
		t.Fatalf("got %+v reads=%v", r, w.reads)
	}
}

func TestVerifySaveBarriers_UncheckableIsInfraOrNotVerified(t *testing.T) {
	ctx := context.Background()
	w := newBarrierWorld()
	// 玩家行这一步没做完:NOT VERIFIED(warn),不是通过。
	if r := verifySaveBarriersWith(ctx, barrierVerifyConfig(t, w, false), w.readFences); r.Severity != "warn" ||
		!strings.Contains(r.Notes, "NOT VERIFIED") {
		t.Errorf("rows step not done: %+v", r)
	}
	// 没有清单 / 没有共享 Redis / 屏障读失败 / 没有落点读方:一律 INFRA。
	if r := verifySaveBarriersWith(ctx, auditConfig{src: 901, dst: 902}, w.readFences); !isInfraAudit(r) {
		t.Errorf("no manifest: %+v", r)
	}
	cfg := barrierVerifyConfig(t, w, true)
	cfg.sharedRDB = nil
	if r := verifySaveBarriersWith(ctx, cfg, w.readFences); !isInfraAudit(r) {
		t.Errorf("no shared redis: %+v", r)
	}
	w.hmgetErr = fakeRedisReplyError("WRONGTYPE Operation against a key holding the wrong kind of value")
	if r := verifySaveBarriersWith(ctx, barrierVerifyConfig(t, w, true), w.readFences); !isInfraAudit(r) {
		t.Errorf("hmget error: %+v", r)
	}
	w.hmgetErr = nil
	cfg = barrierVerifyConfig(t, w, true)
	cfg.routes = nil
	if r := verifySaveBarriersWith(ctx, cfg, w.readFences); !isInfraAudit(r) {
		t.Errorf("no route read side: %+v", r)
	}
	// 生产入口(没有 MySQL 句柄)同样 INFRA,不当作「行都在」。
	if r := verifySaveBarriers(ctx, barrierVerifyConfig(t, w, true)); !isInfraAudit(r) {
		t.Errorf("no MySQL handle: %+v", r)
	}
}

func TestRunAuditMode_VerifyIncludesTheSaveBarrierCheck(t *testing.T) {
	names := map[string]bool{}
	for _, r := range runAuditMode(context.Background(), auditConfig{verify: true, tradeSchema: defaultTradeSchema,
		guildSchema: defaultGuildSchema, src: 901, dst: 902}) {
		names[r.Name] = true
	}
	if !names["verify:save_barrier"] {
		t.Fatalf("-verify-merged must include verify:save_barrier (got %v)", sortedKeys(names))
	}
}
