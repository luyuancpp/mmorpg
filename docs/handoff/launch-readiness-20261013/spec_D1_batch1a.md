# 映射持久化第 1a 批实现规格（定稿，照着写代码用）

本规格只通过读文件、grep 和 git 核对写成，代码没有编译，也没有测试。按 AGENTS.md §10.1，Codex 跑完第 h 节的命令之前，不能说"编译通过"或"测试绿"。交付说明里必须写明"未编译，待 Codex 验证"。

**相对原规格的主要变化**（理由逐条见文末"评审处理记录"）：

- W1 的发布脚本会带上合服围栏键 `merge:in_progress:{home}`，在提交点再原子复核一次围栏（B1）。
- 缓存里没有 `player:maprev` 时，发布脚本不再覆盖遗留的 zone 和落点值（B2）。
- 路由连接池的 DSN 把锁等待上限钉成 `innodb_lock_wait_timeout=1`。遇到 1205 不在原地重试；可重试的错误号是 1213 和 9007（M1、m11）。
- `WaitAOFTimeoutMs` 默认改为 1500ms。发布失败时打 ERROR 日志；audit 新增"未发布孤儿行"分类；切到 db_first 之前，先要过 P2 门禁（M2）。
- 导入时一并读取 maprev，库里新建的行用 `rev = maprev`。库的 rev 小于 maprev 时计为 rev_regression，不写入。audit 新增 `redis_rev_ahead`（M3）。
- 写明 db_first 的安全前提：只能在冻结窗口内打开。这一点写进 yaml、ConfigMap、runbook、提案、启动 WARN 五处（M4，评审 2 的 blocking）。
- TiDB 热点：登记 `zone_merge_fence` 落在单个 Region 的问题，DDL 改为 `CLUSTERED`，并写进 TiDB 实施清单（M5）。
- 文档清单补齐（M6）。
- 新增假驱动契约单测，默认运行（评审 2 第 2 条 major）。
- 新测试的命名避开复验清单里的 `-run` 正则（评审 2 第 1 条 major）。
- 参数非法时改回 `Unavailable` 加码 14，gRPC 码的集合与今天完全相同（m2，评审 2 第 4 条 major）。

---

**开工前要知道的六件事**

1. **工作区里已有别人没提交的改动。** 第 0 批还在工作区，没有提交，包括 `go/data_service/internal/routing/router.go`、`router_test.go`、`internal/server/dataserviceserver.go`、`tools/scripts/k8s_zone_rollback.ps1` 等。`AGENTS.md`、`docs/design/global-data-layer-tidb-decision.md`、`docs/design/player-storage-placement.md`、`docs/ops/merge-zone-runbook.md` 也有未提交改动。本批直接在这些改动之上写，不回退，也不整理别人的段落。文档一律只做局部修改或追加（AGENTS.md §10.2）。
2. **改动面约 49 个文件，其中代码约 39 个、文档 10 个，超过 §10.2 的 30 文件门槛。** 提案 §11.1 第 1 行已经授权（`docs/design/player-route-mapping-durability-proposal.md:404`）。
3. **本批不改 proto，也不重编 C++。**
4. **db_first 在第 1a 批的硬前提：只能在冻结窗口内打开。** 第 1a 批里 merge_zone 仍然只写 Redis（`tools/merge_zone/merge_run.go:171-174` 立围栏，`:230` 从 Redis 收清单），从不写 `zone_merge_fence`。从任一 data_service 副本切到 db_first 起，到第 2 批落地为止，禁止 merge_zone 的所有写模式：merge、unmerge、relocate、pin-placement、backfill。这与提案 P1 的冻结窗口是同一个窗口（提案 `:300-301`）。本批所有档位固定 `Mode: redis`，切换属于 P2 的显式运维动作。
5. **第 i 节是文档落地清单。** 用户要求做完要记录文档，这一节是本批的验收项。
6. **有一处与任务原文不一致，需要负责人知情。** 任务原文写的是 EVALSHA，本规格用 EVAL（g.2 第 2 条）。

---

## a) 文件清单

### 新增（15 个）

| 文件 | 要点 |
|---|---|
| `go/shared/placement/route.go` | 新键常量 `player:maprev:{id}`、`mapping:route:ready`；围栏分片常量与取分片函数；rev 的格式化、校验与比较；`PublishRouteScript` 全文；`PublishRouteKeys`；回复分类函数。只依赖标准库，延续 `placement.go:19` 的约定 |
| `go/shared/placement/route_test.go` | 测试向量，风格同 `placement_test.go`，用包级字面量切片 |
| `go/data_service/internal/store/route_store.go` | 独立连接池（会话级 RC + 锁等待 1s）；启动探针；W1 事务 `RegisterHome`；导入 `ImportRoutes` / `ImportMergedInto`；`ScanRoutes` / `GetRoutes` / `ListMergedInto` |
| `go/data_service/internal/store/route_store_test.go` | 不连库的单测：DSN、DDL 契约、禁止 INSERT IGNORE、AST 守卫、`decideImport`、`RouteRow`、writer 清洗 |
| `go/data_service/internal/store/route_store_contract_test.go` | **新增**。假驱动，支持 BeginTx、Commit、Rollback，默认运行，钉住 `RegisterHome`、`ImportRoutes`、探针的"库应答 → 结局 / 语句 / 参数"。仿 `player_name_store_contract_test.go:1-80` |
| `go/data_service/internal/store/route_store_integration_test.go` | `//go:build integration`，用 storetest 建一次性真库 |
| `go/data_service/internal/routing/route_publish.go` | db_first 的 W1 编排；`publishRoute`（同一条连接上 pipeline 发 EVAL + WAITAOF）；`checkWaitAOFReply`、`waitAOFBudgetMs` 两个纯函数 |
| `go/data_service/internal/routing/route_publish_test.go` | miniredis + 假 RouteStore，28 个顶层用例（f 节） |
| `go/data_service/internal/routetool/redisread.go` | SCAN 翻页；键名后缀和值的规范形式校验；merged_into 的 JSON 解析 |
| `go/data_service/internal/routetool/importer.go` | `import-from-redis` 的逻辑，依赖接口 |
| `go/data_service/internal/routetool/auditor.go` | `audit` 的逻辑 |
| `go/data_service/internal/routetool/routetool_test.go` | miniredis + 假 routeDB |
| `go/data_service/internal/routetool/routetool_integration_test.go` | `//go:build integration`，真 MySQL + miniredis |
| `go/data_service/cmd/route_tool/main.go` | 薄入口：解析参数、装配、映射退出码 |
| `go/data_service/cmd/route_tool/main_test.go` | 参数解析与退出码 |

### 修改：代码与脚本（24 个）

| 文件 | 要点 |
|---|---|
| `go/data_service/internal/store/schema.go` | `bootstrapTables`（`:158-161`）加 4 段 DDL。新增 `routeTableShapes`（列名 + 主键守卫）。从 `assertColumnsPresent`（`:1677-1712`）抽出 `assertColumnNamesPresent`。迁移时用 ODKU 种 `route_meta(id=1)`。改写文件头注释（`:21-32`）和 `migrateSchemaOn` 注释（`:1417-1420`）："五张表"改成"proto 驱动的五张 + 手写 DDL 的 id_segment、player_name 与四张路由表" |
| `go/data_service/internal/store/mysql.go` | 拆出 `openMySQLWithDSN(cfg, dsn)`；新增 `buildRouteDSN` |
| `go/data_service/internal/store/export_test.go` | 导出路由 SQL 常量、`LockRetriesForTest`、`NewRouteStoreFromDBForTest` |
| `go/data_service/internal/routing/router.go` | `NewRouter(c, opts...)`，加选项 `WithRouteStore`；`RegisterPlayerZone`（`:152-163`）开头按 Mode 分流，redis 分支逐字不动；删除 `RemapHomeZoneForMerge` 和 `ErrMergeFenceMissing`（`:487-547`）；在 `DeletePlayerZone` 和 `DeletePlayerZoneIfLocked`（`:359-363`、`:619-643`）的注释里写明"生产路径已停用（提案 §4.3）" |
| `go/data_service/internal/routing/router_test.go` | 删除 3 个 remap 用例（`:469-528`） |
| `go/data_service/internal/routing/router_integration_test.go` | `itKeyPatterns`（`:50`）加 `player:maprev:*`；`TestMergeFence_RealRedisBlocksRegisterAndGatesRemap`（`:167-209`）删掉 remap 的几步，改名为 `TestMergeFence_RealRedisBlocksRegister`；新增 4 个用例（f 节） |
| `go/data_service/internal/server/dataserviceserver.go` | W1 的错误映射加两支（`:95-117`），同步改契约注释（`:83-94`）；W2 handler 改成恒拒（`:229-264`）；W3 映射成 FailedPrecondition（`:266-278`） |
| `go/data_service/internal/server/dataserviceserver_test.go` | 5 个 remap 用例（`:242-352`）替换成 1 个；新增 db_first 错误映射表和 W3 用例；新增测试辅助 `newDBFirstTestServer` |
| `go/data_service/internal/logic/data_logic.go` | `DeletePlayerData`（`:388-454`）入口加守卫；删掉从此不可达的 `if req.DeleteZoneMapping {...}`（`:436-450`）；新增哨兵 `ErrZoneMappingDeletionDisabled` |
| `go/data_service/internal/logic/data_logic_test.go` | `:313-333` 改写成"拒绝且零变更"；`:335-372` 不动（它直接调 Router，不受影响） |
| `go/data_service/internal/constants/error_codes.go` | 新增 `ErrCodeRouteStoreError uint32 = 30`；24 号码的注释写明"已停用，号不复用" |
| `go/data_service/internal/config/config.go` | 新增 `RouteStoreConfig`；改 AdminToken 注释（`:24-40`），写明 remap 已停用、token 现在只守 Rollback*；改"五张表"注释（`:48`、`:245-249`） |
| `go/data_service/internal/config/config_test.go` | 缺省值、拒绝表、仓库 yaml 三类用例 |
| `go/data_service/internal/svc/servicecontext.go` | 新增接口 `PlayerRouteStore`；装配；`MissingStores` 加条件项；Close |
| `go/data_service/data_service.go` | `-migrate` 分支（`:86-88`）之后校验 RouteStore；启动横幅加 db_first 的 WARN；预置指标（`:120-123` 附近）；改 flag 注释（`:43-46`）、迁移输出文案（`:268`）、故障码注释（`:190-193`，补上码 30） |
| `go/data_service/cmd/debugutil/debugutil.go` | `NewServiceContext`（`:117-136`）在 db_first 时装 route store，cleanup 时关闭 |
| `go/data_service/internal/metrics/metrics.go`、`metrics_test.go` | 新增两个计数器和 `PrimeRoute()` |
| `go/data_service/etc/data_service.yaml` | 新增 `RouteStore:` 段，显式写 `Mode: redis`；改 AdminToken 注释（`:127-139`）和"五张表"注释（`:163-165`） |
| `tools/scripts/k8s_deploy.ps1` | data-service ConfigMap 在 `Store:` 段（`:3523-3524`）之后加 `RouteStore: Mode: redis`；改 `:3505-3507` 的表清单注释；`:993`、`:3378`、`:5481`、`:5528` 的"四张表"改成不写数量的说法（只改注释） |
| `tools/scripts/tests/k8s_deploy_contract.tests.ps1` | 在 `$prodRun`（`:926`）之后新增一个 Test-Case |
| `tools/scripts/stress_round19.ps1` | 新增 `$GlobalDbName`；plan 打印路由表清理；`Invoke-Wipe` 里加清理，fail-closed |
| `tools/scripts/currency_crash_window.ps1` | 同上，加在 `Invoke-DataReset`（`:95-139`） |

### 修改：文档（10 个，见第 i 节）

提案 §11、`docs/PROGRESS.md`、`docs/README.md`、`docs/design/player-storage-placement.md`、`docs/ops/merge-zone-runbook.md`、`docs/design/server_merge_design.md`、`docs/design/enter-scene-zone-routing.md`、`docs/design/global-data-layer-tidb-decision.md`、`docs/design/ARCH.md`、`docs/handoff/placement-reverify-checklist-20261012.md`。

可选：`tools/scripts/tests/wipe_global_route_tables.tests.ps1`，静态检查两份清库脚本是否覆盖了 3 个表名和 `UPDATE route_meta`。

---

## b) 新类型、函数、SQL 与 Lua

### b.1 `go/shared/placement/route.go`

```go
const MapRevKeyPrefix = "player:maprev:"
func MapRevKey(playerID uint64) string            // "player:maprev:{id}"
const RouteReadyKey = "mapping:route:ready"       // 第 1a 批只定义,不写不读

const (
    DefaultRouteFenceShards uint32 = 8
    MaxRouteFenceShards     uint32 = 64 // 契约:围栏持有者(第 2 批)必须写满 0..63 全部分片行
)
func RouteFenceShard(playerID uint64, shards uint32) (uint32, error) // shards∉[1,64] → error

func FormatRev(rev uint64) string                  // rev==0 → ""(调用方先拒)
func ValidRev(s string) bool                       // ^[1-9][0-9]{0,19}$ 且 ≤ MaxUint64
func CompareRev(a, b string) (int, error)          // 先比长度再比字典序;任一非法 → ErrMalformed

// fenceZone != 0 时追加第 4 把键 MergeFenceKey(fenceZone)(W1 用);== 0 只给三把(第 2 批合服批量发布用)
func PublishRouteKeys(playerID uint64, fenceZone uint32) []string

type PublishOutcome uint8
const (
    PublishApplied        PublishOutcome = iota + 1 // {1, rev}
    PublishSuperseded                               // {0, cur>rev, h, p}
    PublishConfirmed                                // {0, cur==rev, 值相同}
    PublishConflict                                 // {0, cur==rev, 值不同}            → 写者按失败
    PublishLegacyConflict                           // {2, '', h, p}:无 maprev 且遗留值不同 → 写者按失败
    PublishFenced                                   // {3}:KEYS[4] 存在               → ErrZoneMergeInProgress
)
type PublishReply struct{ Outcome PublishOutcome; CurRev, CurHome, CurPlacement string }
func ClassifyPublishReply(raw any, rev, home, plc string) (PublishReply, error)
```

`ClassifyPublishReply` 只接受下面四种形状：

- `[]any{int64(1), rev}`，第二个元素必须等于 rev；
- `[]any{int64(0), cur, h, p}`，cur 必须通过 `ValidRev`，且 `cur >= rev`；
- `[]any{int64(2), "", h, p}`，且 `h != home || p != plc`；
- `[]any{int64(3)}`。

其它形状，例如 0 回复里 `cur < rev`、2 回复里值反而相同、cur 是 20 位数字但超过 MaxUint64，一律报错（不可能出现的状态，按 fail-closed 处理）。

这个包不放 Redis 的读写代码，理由见 g.2 第 4 条。

**`PublishRouteScript` 全文**

在提案 §4.2 的基础上加了五处：rev 校验、可选的围栏键（B1）、无 maprev 时的遗留值守卫（B2）、返回 0 时把 maprev 原值写回一次（touch）、逐字节比较（m12）。

```lua
-- route publish (set-if-newer),docs/design/player-route-mapping-durability-proposal.md §4.2(第 1a 批定稿)
-- KEYS: 1=player:zone:{id} 2=player:placement:{id} 3=player:maprev:{id} [4=merge:in_progress:{home},可选]
-- ARGV: 1=rev(十进制,无前导零,1..20 位) 2=home(''=DEL) 3=placement(''=DEL)
-- 返回: {1, rev}                          已写入三键
--       {0, cur, curHome, curPlacement}   缓存 rev >= 本次:映射未改(maprev 原值写回一次,见下)
--       {2, '', curHome, curPlacement}    缓存无 maprev 且遗留值与本次不同:零写入
--       {3}                               KEYS[4] 存在(合服围栏):零写入
local function valid_rev(s)
  return type(s) == 'string' and #s >= 1 and #s <= 20 and string.match(s, '^[1-9][0-9]*$') ~= nil
end
-- 两边都已过 valid_rev:先比长度再逐字节比。不经 Lua number(double),也不依赖 strcoll / locale。
local function rev_ge(a, b)
  if #a ~= #b then return #a > #b end
  for i = 1, #a do
    local x, y = string.byte(a, i), string.byte(b, i)
    if x ~= y then return x > y end
  end
  return true
end
local rev = ARGV[1]
if not valid_rev(rev) then
  return redis.error_reply('ERR route publish: bad rev argument')
end
-- 提交点复核合服围栏(与 router.go setPlayerZoneIfAbsentScript 的 EXISTS KEYS[2] 同一判据:键存在即封锁)
if KEYS[4] and redis.call('EXISTS', KEYS[4]) == 1 then
  return {3}
end
local cur = redis.call('GET', KEYS[3])
local curHome = redis.call('GET', KEYS[1])
local curPlc = redis.call('GET', KEYS[2])
if cur then
  if not valid_rev(cur) then
    return redis.error_reply('ERR route publish: malformed maprev')
  end
  if rev_ge(cur, rev) then
    -- 同值写回:让本连接随后的 WAITAOF 覆盖「此刻已在缓存里的状态」,
    -- 否则幂等重发(上次的 WAITAOF 超时)会在没确认落盘的情况下回成功。
    redis.call('SET', KEYS[3], cur)
    return {0, cur, curHome or '', curPlc or ''}
  end
else
  -- 无 maprev:这两把键要么从没写过,要么是 P1 导入之前、或绕过本脚本的写者(第 1a 批的 merge_zone)
  -- 留下的遗留值。遗留值存在且与本次不同时一律不覆盖 —— 静默改回库里的旧值就是写错库。
  if (curHome and curHome ~= ARGV[2]) or (curPlc and curPlc ~= ARGV[3]) then
    return {2, '', curHome or '', curPlc or ''}
  end
end
if ARGV[2] == '' then redis.call('DEL', KEYS[1]) else redis.call('SET', KEYS[1], ARGV[2]) end
if ARGV[3] == '' then redis.call('DEL', KEYS[2]) else redis.call('SET', KEYS[2], ARGV[3]) end
-- maprev 最后写,表示「三键已到这一版」。Redis 7 对没有 shebang 的脚本只在第一条写命令前做一次 OOM 检查,
-- 之后不会中途拒写,所以不存在 OOM 半写;顺序只为可读。
redis.call('SET', KEYS[3], rev)
return {1, rev}
```

说明：

- 遗留值守卫只在 `cur` 不存在时生效。`cur` 存在时，三把键都由本脚本写入。`cur < rev` 时直接覆盖，前提是写者纪律：没有人绕过本脚本去改这两把键（提案 §8.3 第 1 条）。`cur == rev` 且值不同时，由 Conflict 分支兜住。
- 不用 shebang：miniredis 是否兼容没有核实。

### b.2 `store/route_store.go`

```go
const (
    PlayerRouteTableName    = "player_route"
    ZoneMergeFenceTableName = "zone_merge_fence"
    ZoneMergedIntoTableName = "zone_merged_into"
    RouteMetaTableName      = "route_meta"
    RouteHomeZoneMax uint32 = 999999 // = chk_route_home 上限
    RouteBatchLimit         = 1000   // GetRoutes / ImportRoutes 单批上限
    routeMetaRowID          = 1
    routeTxAttempts         = 4      // 1 次 + 1213/9007 重试 3 次;1205 不重试(见 c.2)
    routeLockWaitTimeoutSec = 1      // 会话级 innodb_lock_wait_timeout,DSN 钉死(guild 先例)
    routeLockRetryBackoffMin = 10 * time.Millisecond
    routeLockRetryBackoffMax = 50 * time.Millisecond
    routeProbeTimeout        = 5 * time.Second
    routeWriterMaxLen        = 96    // = updated_by 列宽
    tidbErrWriteConflict     = 9007  // 与 tools/merge_zone/merge_run.go:1045-1060 同口径
)

var (
    ErrRouteInvalidArgument    = errors.New("player_route: invalid argument")
    ErrRouteMetaMissing        = errors.New("route_meta: row id=1 missing; run `data_service -f <yaml> -migrate`")
    ErrRouteNotWritable        = errors.New("route_meta: writable!=1, route writers are frozen")
    ErrRouteRowVanished        = errors.New("player_route: row vanished between ON DUPLICATE KEY and the locking read")
    ErrRouteFenceShardVanished = errors.New("zone_merge_fence: shard row missing right after ensure")
    ErrRouteSessionMisconfigured = errors.New("route store: session isolation / lock wait / txn mode not as required")
)

type RouteStoreOptions struct {
    FenceShards uint32 // 1..placement.MaxRouteFenceShards
    Writer      string // 写进 updated_by;NewRouteWriterID 生成
}
type RouteStore struct {
    db *sql.DB; fenceShards uint32; writer string; lockRetries atomic.Uint64
}
func NewRouteStore(cfg MySQLConfig, opts RouteStoreOptions) (*RouteStore, error)
func newRouteStoreFromDB(ctx context.Context, db *sql.DB, opts RouteStoreOptions) (*RouteStore, error) // 跑探针;契约测试注入假驱动
func (s *RouteStore) Close() error
func NewRouteWriterID(component, host string) string // 只留 [A-Za-z0-9._@:-],截到 96,host 空 → "unknown"
```

**建池与探针**

1. 建池用 `openMySQLWithDSN(cfg, buildRouteDSN(cfg))`，其中

   `buildRouteDSN = buildDSN(cfg) + "&transaction_isolation=%27READ-COMMITTED%27&innodb_lock_wait_timeout=1"`。

   go-sql-driver 会把不认识的 DSN 参数当成会话级 `SET`。先例有两个：`go/guild/internal/data/guild_manage_repo.go:318-350`（`WithLockWaitTimeout`）和 `go/friend/internal/svc/servicecontext.go:177-193`。`buildDSN` 不带 CLIENT_FOUND_ROWS（`mysql.go:31-34`）。
2. `newRouteStoreFromDB` 依次探测，任一步失败就关池并返回 error（fail-closed）：
   - `SELECT @@SESSION.transaction_isolation, @@SESSION.innodb_lock_wait_timeout` 必须是 `READ-COMMITTED` 和 `1`，否则返回 `ErrRouteSessionMisconfigured`；
   - 用 `serverIsTiDB`（`schema.go:1187-1193`）判断服务端；如果是 TiDB，`SELECT @@tidb_txn_mode` 必须是 `pessimistic`（提案 §3.1，A15）；
   - `SELECT writable FROM route_meta WHERE id = 1`：表不存在时原样返回 1146 错误；`ErrNoRows` 时返回 `ErrRouteMetaMissing`。
3. binlog_format 要求 ROW。RC 在 STATEMENT 格式下会报 1665，见 `cmd/debug_import/main.go:366-367`。本批不探测，与其它 store 同口径。

**`RouteRow`**

```go
type RouteRow struct {
    PlayerID uint64
    HomeZone uint32          // 0 = NULL
    StorageID uint32         // 0 = NULL(与 PlacementVersion 同 NULL)
    PlacementVersion uint64
    FrozenRunID string       // "" = NULL
    Rev uint64
    UpdatedBy string
    CreatedAtMs, UpdatedAtMs uint64 // Inserted 分支不回读,两者为 0(发布不用)
}
func (r RouteRow) HasHome() bool
func (r RouteRow) HasPlacement() bool
func (r RouteRow) IsTombstone() bool                         // 两者都 NULL = "真的没有记录"(提案 §3.2 末)
func (r RouteRow) HomeValue() string                         // "" 或十进制
func (r RouteRow) Placement() (placement.Record, bool, error) // 有落点就 Validate,失败 → ErrMalformed
func (r RouteRow) PlacementValue() (string, error)           // "" 或 Record.Encode()
```

**读写列的类型（m1）**

- **扫描**用 `sql.Null[uint32]`、`sql.Null[uint64]`、`sql.NullString`。不能用 `NullInt64`：bigint unsigned 可能超过 2^63。
- **绑定**一律用 `nullU32(v uint32) any` 和 `nullU64(v uint64) any`：0 返回 `nil`，否则返回 `uint64(v)`。不要把 `sql.Null[uint32]` 直接当参数：它的 `Value()` 返回 `uint32`，go-sql-driver 的转换器只放行 `uint64`，会报 "non-Value type"。这一点读源码不可得，由契约测试钉住。
- `RegisterHome` 内部断言 `(storage 参数 == nil) == (version 参数 == nil)`。

**`RegisterHome`（W1）**

```go
type RouteRegisterRequest struct {
    PlayerID uint64; HomeZone uint32; StorageID uint32
    ExistingRowOnly bool // true = 调用方在缓存里已见过该玩家:只认已有行,绝不新插
}
type RouteRegisterOutcome uint8
const (
    RouteRegisterInserted RouteRegisterOutcome = iota + 1
    RouteRegisterFilledHome
    RouteRegisterFilledHomePlacementKept
    RouteRegisterAlreadySame
    RouteRegisterConflict
    RouteRegisterFenced
    RouteRegisterRowMissing   // 只在 ExistingRowOnly 时出现
)
func (o RouteRegisterOutcome) String() string // inserted|filled_home|filled_home_placement_kept|already_same|conflict|fenced|row_missing
type RouteRegisterResult struct {
    Outcome RouteRegisterOutcome
    Row RouteRow          // Inserted/Filled*/AlreadySame 时 = 提交后的行,用于发布
    ExistingHome uint32   // Conflict
    FenceRunID string     // Fenced
}
func (s *RouteStore) RegisterHome(ctx context.Context, req RouteRegisterRequest) (RouteRegisterResult, error)
```

错误语义：

- 冲突、围栏、缺行这些业务结果通过 Outcome 返回，err 为 nil。
- 只有故障才返回 err：
  - 参数非法：`ErrRouteInvalidArgument`；
  - meta 缺失、不可写、行消失；
  - SQL 错误，包括 1205；
  - 1213/9007 重试用尽。

**导入、审计、只读接口**

```go
type RouteImportRow struct {
    PlayerID uint64
    HomeZone uint32             // 0 = Redis 无 player:zone
    Placement *placement.Record // nil = Redis 无 player:placement
    MapRev uint64               // 0 = Redis 无 player:maprev
}
type RouteImportConflict struct{ PlayerID uint64; DB RouteRow; Redis RouteImportRow }
type RouteImportStats struct {
    Inserted, Matched, Filled, DBAhead int
    RevRegressions []RouteImportConflict // 库 rev < maprev:疑似库被回档,不写
    Conflicts      []RouteImportConflict
}
func (s *RouteStore) ImportRoutes(ctx context.Context, rows []RouteImportRow, dryRun bool) (RouteImportStats, error)
// rows 必须按 player_id 严格升序、无重复、≤ RouteBatchLimit,否则 ErrRouteInvalidArgument

type MergedIntoRow struct{ SrcZone, DstZone uint32; RunID string; MergedAtMs uint64 }
type MergedIntoConflict struct{ DB, Redis MergedIntoRow }
type MergedIntoImportStats struct{ Inserted, Matched int; Conflicts []MergedIntoConflict }
func (s *RouteStore) ImportMergedInto(ctx context.Context, rows []MergedIntoRow, dryRun bool) (MergedIntoImportStats, error)

func (s *RouteStore) ScanRoutes(ctx context.Context, afterPlayerID uint64, limit int) ([]RouteRow, error)
func (s *RouteStore) GetRoutes(ctx context.Context, ids []uint64) (map[uint64]RouteRow, error)
func (s *RouteStore) ListMergedInto(ctx context.Context) ([]MergedIntoRow, error)

func decideImport(db *RouteRow, r RouteImportRow) importDecision // insert(rev)|match|fill(newRow)|dbAhead|revRegression|conflict
```

`decideImport` 按以下顺序判定：

1. `db == nil`：插入，`rev = r.MapRev`（为 0 时取 1）。插入的值取自 Redis，代表的就是 maprev 那一版的状态。
2. `r.MapRev > db.Rev`：判为 revRegression，不写，不再看下面的列。
3. home 和落点两列分别判断（落点按 `Encode()` 后的字符串比较，冻结态和稳定态视为不同）：

   | 库 | Redis | 列结论 |
   |---|---|---|
   | 相等 | 相等 | match |
   | NULL | 有值 | fill |
   | 有值 | 无键 | dbAhead：库领先，例如 db_first 提交后发布失败；不写，不算冲突 |
   | 有值 | 有值且不同 | conflict |

   合并规则：任一列 conflict，整行 conflict；否则任一列 fill，整行 fill，只补 NULL 的列，dbAhead 的列保持库值；否则任一列 dbAhead，整行 dbAhead；否则 match。

### b.3 DDL 全文

四张表都是 `schema.go` 里的 Go 字符串常量，拼接风格同 `:60-68`，加进 `bootstrapTables`。与提案 §3.2 只有一处不同：`zone_merge_fence` 的主键加了 `CLUSTERED`（M5，g.2 第 16 条）。

```sql
CREATE TABLE IF NOT EXISTS `player_route` (
  `player_id`         bigint unsigned NOT NULL COMMENT 'pb:1',
  `home_zone`         int unsigned    NULL     COMMENT 'pb:2',
  `storage_id`        int unsigned    NULL     COMMENT 'pb:3',
  `placement_version` bigint unsigned NULL     COMMENT 'pb:4',
  `frozen_run_id`     varchar(64) CHARACTER SET ascii COLLATE ascii_bin NULL COMMENT 'pb:5',
  `rev`               bigint unsigned NOT NULL COMMENT 'pb:6',
  `updated_by`        varchar(96) CHARACTER SET ascii NOT NULL DEFAULT '' COMMENT 'pb:7',
  `created_at_ms`     bigint unsigned NOT NULL COMMENT 'pb:8',
  `updated_at_ms`     bigint unsigned NOT NULL COMMENT 'pb:9',
  PRIMARY KEY (`player_id`) /*T![clustered_index] NONCLUSTERED */,
  KEY `idx_route_home`    (`home_zone`, `player_id`),
  KEY `idx_route_storage` (`storage_id`, `player_id`),
  KEY `idx_route_updated` (`updated_at_ms`),
  CONSTRAINT `chk_route_home`   CHECK (`home_zone` IS NULL OR `home_zone` BETWEEN 1 AND 999999),
  CONSTRAINT `chk_route_place`  CHECK ((`storage_id` IS NULL) = (`placement_version` IS NULL)),
  CONSTRAINT `chk_route_ver`    CHECK (`placement_version` IS NULL OR `placement_version` >= 1),
  CONSTRAINT `chk_route_frozen` CHECK (`frozen_run_id` IS NULL OR `storage_id` IS NOT NULL),
  CONSTRAINT `chk_route_rev`    CHECK (`rev` >= 1)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin /*T! SHARD_ROW_ID_BITS=4 PRE_SPLIT_REGIONS=4 */ COMMENT='player_route';

CREATE TABLE IF NOT EXISTS `zone_merge_fence` (
  `zone_id`       int unsigned NOT NULL,
  `shard`         tinyint unsigned NOT NULL,
  `run_id`        varchar(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `purpose`       varchar(16) CHARACTER SET ascii NULL,
  `src_zone`      int unsigned NULL,
  `dst_zone`      int unsigned NULL,
  `expires_at_ms` bigint unsigned NULL,
  `holder_json`   varchar(1024) NULL,
  `fence_epoch`   bigint unsigned NOT NULL DEFAULT 0,
  -- CLUSTERED:TiDB 建表后不能改聚簇属性;聚簇才能按 (zone_id, shard) 值 SPLIT 出按 zone 分的 Region(见 g.3 第 1 条)
  PRIMARY KEY (`zone_id`, `shard`) /*T![clustered_index] CLUSTERED */,
  CONSTRAINT `chk_fence_pair` CHECK ((`run_id` IS NULL) = (`expires_at_ms` IS NULL))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='zone_merge_fence';

CREATE TABLE IF NOT EXISTS `zone_merged_into` (
  `src_zone`     int unsigned NOT NULL,
  `dst_zone`     int unsigned NOT NULL,
  `run_id`       varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `merged_at_ms` bigint unsigned NOT NULL,
  PRIMARY KEY (`src_zone`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='zone_merged_into';

CREATE TABLE IF NOT EXISTS `route_meta` (
  `id`                tinyint unsigned NOT NULL,
  `generation`        varchar(32) CHARACTER SET ascii NOT NULL,
  `writable`          tinyint unsigned NOT NULL DEFAULT 1,
  `watermark_ms`      bigint unsigned NOT NULL DEFAULT 0,
  `cache_verified_ms` bigint unsigned NOT NULL DEFAULT 0,
  `leader`            varchar(96) NULL,
  `lease_until_ms`    bigint unsigned NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='route_meta';
```

**形状守卫 `routeTableShapes`**（本批不建 proto 守卫，以此代替）

```go
type routeTableShape struct{ columns, primaryKey []string }
var routeTableShapes = map[string]routeTableShape{
    PlayerRouteTableName: {
        []string{"player_id", "home_zone", "storage_id", "placement_version", "frozen_run_id",
            "rev", "updated_by", "created_at_ms", "updated_at_ms"},
        []string{"player_id"},
    },
    ZoneMergeFenceTableName: {
        []string{"zone_id", "shard", "run_id", "purpose", "src_zone", "dst_zone",
            "expires_at_ms", "holder_json", "fence_epoch"},
        []string{"zone_id", "shard"},
    },
    ZoneMergedIntoTableName: {
        []string{"src_zone", "dst_zone", "run_id", "merged_at_ms"},
        []string{"src_zone"},
    },
    RouteMetaTableName: {
        []string{"id", "generation", "writable", "watermark_ms", "cache_verified_ms",
            "leader", "lease_until_ms"},
        []string{"id"},
    },
}
```

**接入 `migrateSchemaOn`**

放在 bootstrap DDL 循环（`schema.go:1440-1446`）之后、TableMessages 循环（`:1448` 起）之前，按表名排序，对每张表执行：

1. `assertColumnNamesPresent(ctx, db, dbName, tableName, shape.columns)`：从 `assertColumnsPresent` 抽出来的"按列名比对"公共函数，原函数改为从 proto 字段推出列名后再调用它；
2. `primaryKeyColumns`（`:576-598`）的结果必须与 `shape.primaryKey` 逐个相等；
3. 最后执行 `bootstrapRouteMetaRow(ctx, db)`：generation 用 `crypto/rand` 生成 16 字节，再转成十六进制（32 个字符）；日志要区分"新种了一行"和"已存在，未改"。

新表不进 `TableMessages`，所以不会被 proto 同步循环和 `TestGlobalTableDDLContract`（`schema_ddl_test.go:24-35`）误伤。

### b.4 DML 全文

下面用 `<NOW>` 代替 `CAST(UNIX_TIMESTAMP(NOW(3)) * 1000 AS UNSIGNED)`，代码里是常量 `dbNowMsSQL`。所有时间戳都取库时钟。

```sql
-- R1 读可写开关(普通一致读,提案 §4.1-2 第 1 步)
SELECT writable FROM route_meta WHERE id = 1
-- R2 锁围栏分片行(active 为 NULL 也当生效 → fail-closed)
SELECT (run_id IS NOT NULL AND expires_at_ms > <NOW>) AS active, run_id
  FROM zone_merge_fence WHERE zone_id = ? AND shard = ? FOR UPDATE
-- R3 确保分片行存在(autocommit,事务外;只在 R2 命中 ErrNoRows 时跑)
INSERT INTO zone_merge_fence (zone_id, shard) VALUES (?, ?) ON DUPLICATE KEY UPDATE zone_id = zone_id
-- R4 新行(不用 INSERT IGNORE:它把 CHECK 违例降成 warning,提案 §4.1-3);storage/version 用 nullU32/nullU64 绑定
INSERT INTO player_route (player_id, home_zone, storage_id, placement_version, frozen_run_id, rev, updated_by, created_at_ms, updated_at_ms)
  VALUES (?, ?, ?, ?, NULL, 1, ?, <NOW>, <NOW>) ON DUPLICATE KEY UPDATE player_id = player_id
-- R5 加锁读(判定只认它)
SELECT home_zone, storage_id, placement_version, frozen_run_id, rev, updated_by, created_at_ms, updated_at_ms
  FROM player_route WHERE player_id = ? FOR UPDATE
-- R6a 只补 home(落点已在,或请求不带 storage)
UPDATE player_route SET home_zone = ?, rev = rev + 1, updated_by = ?, updated_at_ms = <NOW>
  WHERE player_id = ? AND rev = ? AND home_zone IS NULL
-- R6b 补 home + 落点(落点为 NULL 且请求带 storage)
UPDATE player_route SET home_zone = ?, storage_id = ?, placement_version = 1, rev = rev + 1, updated_by = ?, updated_at_ms = <NOW>
  WHERE player_id = ? AND rev = ? AND home_zone IS NULL AND storage_id IS NULL
-- R7 导入批量插(VALUES 按 player_id 升序;rev 逐行绑定 = maprev 或 1)
INSERT INTO player_route (player_id, home_zone, storage_id, placement_version, frozen_run_id, rev, updated_by, created_at_ms, updated_at_ms)
  VALUES (?, ?, ?, ?, ?, ?, ?, <NOW>, <NOW>)[, ...] ON DUPLICATE KEY UPDATE player_id = player_id
-- R8 导入批量加锁读(dry-run 去掉 FOR UPDATE、不开事务)
SELECT player_id, home_zone, storage_id, placement_version, frozen_run_id, rev, updated_by, created_at_ms, updated_at_ms
  FROM player_route WHERE player_id IN (?, ...) ORDER BY player_id FOR UPDATE
-- R9 导入补列(按读到的 rev 做 CAS;所有可空列按 null* 绑定)
UPDATE player_route SET home_zone = ?, storage_id = ?, placement_version = ?, frozen_run_id = ?, rev = rev + 1, updated_by = ?, updated_at_ms = <NOW>
  WHERE player_id = ? AND rev = ?
-- R10 / R11 merged_into(每行一个事务)
INSERT INTO zone_merged_into (src_zone, dst_zone, run_id, merged_at_ms) VALUES (?, ?, ?, <NOW>) ON DUPLICATE KEY UPDATE src_zone = src_zone
SELECT dst_zone, run_id, merged_at_ms FROM zone_merged_into WHERE src_zone = ? FOR UPDATE
-- R12 键集翻页 / R13 点查 / R14 列 merged_into(全是一致读)
SELECT player_id, <R5 的列> FROM player_route WHERE player_id > ? ORDER BY player_id LIMIT ?
SELECT player_id, <R5 的列> FROM player_route WHERE player_id IN (?, ...)
SELECT src_zone, dst_zone, run_id, merged_at_ms FROM zone_merged_into ORDER BY src_zone
-- R15 迁移种 meta 行(ODKU no-op)
INSERT INTO route_meta (id, generation, writable, watermark_ms, cache_verified_ms) VALUES (1, ?, 1, 0, 0) ON DUPLICATE KEY UPDATE id = id
```

**受影响行数的判定**

- R4：1 = 插入，0 = 撞键（no-op），其它值一律 fail-closed，照 `player_name_store.go:886-900` 的写法。
- R6a、R6b、R9：必须恰好是 1，否则报错，口径同 `id_segment_store.go:222-225`。行已被 R5/R8 锁住，不是 1 就说明出了不可能的状态。
- R7：受影响行数必须等于待插行数。不等说明 R8 之后有 W1 并发插入了同一个 id：回滚整批，最多重试 3 次（重试时那一行会被 R8 读到），之后仍不等就报错。
- R10：1 = 插入，0 = 已存在（再由 R11 判断 matched 还是 conflict）。

**库时钟**：`NOW()` 和 `UNIX_TIMESTAMP()` 都按会话时区计算，两者互相抵消，结果就是真实的 epoch 毫秒。只有夏令时切换的那一小时有歧义；Asia/Shanghai 和 UTC 都没有夏令时。

---

## c) W1 db_first 的逐步顺序，以及与 redis 模式的语义对照

### c.1 router 编排

`RegisterPlayerZone`（`router.go:152`）的第一行：

```go
if r.routeMode == config.RouteStoreModeDBFirst {
    return r.registerPlayerZoneDBFirst(ctx, playerID, homeZoneID, storageID)
}
```

redis 分支之后的代码逐字不动。

`registerPlayerZoneDBFirst` 的步骤：

0. **参数校验。** `playerID == 0`，或 home 不在 `[1, store.RouteHomeZoneMax]` 内，返回 `ErrInvalidRegistration`，不碰 Redis，也不碰库。
1. **store 是否装配。** `r.routeStore == nil` 时返回 `fmt.Errorf("%w: route store not assembled", ErrRouteStore)`。
2. **Redis 预检，共两次往返。**
   - **a. 围栏：** 直接复用 `r.IsMergeInProgress(ctx, home)`，即今天的 EXISTS，判据是"键存在即封锁"（`router.go:85`）。不并进 MGET：MGET 对非 string 类型的键返回 nil，空串值也会被当成缺席。
     - 查询失败：沿用 `router.go:155-158` 的文案，最后是 Unavailable，码 1；
     - 命中：返回 `ErrZoneMergeInProgress`，不进库。这保证"围栏先于冲突"（`router_test.go:257`）。
   - **b. 三键：** 一次 `MgetCtx`，取 `player:zone:{id}`、`player:placement:{id}`、`player:maprev:{id}`。
     - 失败时返回 `"mapping precheck for player %d failed: %w"`，最后是 Unavailable，码 1；
     - zone 值非空且不等于请求值：能解析就返回 `&HomeZoneConflictError{Existing}`，解析不了就用今天的 unparsable 文案（`router.go:208-213`），都不进库；
     - 计算 `known := zone != "" || placement != "" || maprev != ""`，设 `ExistingRowOnly = known`。缓存认得这名玩家，库里就必须已有这一行。
3. **调库：** `res, err := r.routeStore.RegisterHome(ctx, ...)`，store 内部的顺序见 c.2。
   - err：返回 `fmt.Errorf("%w: %w", ErrRouteStore, err)`。
   - Fenced：返回 `ErrZoneMergeInProgress`。
   - Conflict：返回 `&HomeZoneConflictError{PlayerID, Existing: res.ExistingHome, Requested}`。
   - RowMissing：返回 `fmt.Errorf("%w: %w", ErrRouteStore, ErrRouteUnknownToStore)`，同时打 ERROR 日志："缓存里有该玩家但 player_route 无行，先跑 route_tool import-from-redis"。
4. **发布：** `r.publishRoute(ctx, res.Row)`，幂等分支（AlreadySame）同样要发。失败时打 ERROR 日志，带 `player_id`、`rev`、`outcome`、原因。这些只进日志，不做指标 label（AGENTS.md §9）。返回值：
   - 发布脚本回 `{3}`：`fmt.Errorf("%w: zone %d (player %d) fenced at publish; player_route row rev=%d committed but unpublished", ErrZoneMergeInProgress, ...)`，最后是 FailedPrecondition，码 22；
   - 其它失败：发布错误，不包 `ErrRouteStore`，最后是 Unavailable，码 1。
5. **落点保留：** `FilledHomePlacementKept` 打今天那条 ERROR（`router.go:200-201` 的文案），然后返回 nil。
6. 按结局计 `route_register_total{outcome}`。

新增的哨兵错误：`ErrInvalidRegistration`、`ErrRouteStore`、`ErrRouteUnknownToStore`、`ErrRoutePublishConflict`、`ErrRoutePublishNotDurable`。

**`publishRoute` 的细节**

1. **准备参数。**
   - `home = row.HomeValue()`；
   - `plc, err = row.PlacementValue()`：值非法时返回 `fmt.Errorf("%w: player_route row invalid: %w", ErrRouteStore, err)`，不发布垃圾；
   - `rev = FormatRev(row.Rev)`；
   - `keys = placement.PublishRouteKeys(id, row.HomeZone)`，共四把键，第四把是围栏键。
2. **最多循环 `PublishAttempts` 次。** 每次先算 WAITAOF 的等待预算：

   `waitMs, ok := waitAOFBudgetMs(now, deadline, hasDeadline, WaitAOFTimeoutMs, 100ms)`

   规则：先把剩余时间减去 100ms 留量，**截断成整毫秒之后**，再与配置取小；结果小于 1 就是 `ok=false`，直接返回 `ErrRoutePublishNotDurable("budget exhausted")`，不发送任何命令。这是因为 WAITAOF 的 timeout 为 0 表示无限阻塞（m5）。
3. **每次一个 pipeline，在同一条连接上发：**

   ```go
   err := r.mappingRedis.PipelinedCtx(ctx, func(p redis.Pipeliner) error {
       evalCmd = p.Eval(ctx, placement.PublishRouteScript, keys, rev, home, plc)
       if r.publish.waitAOF {
           waitCmd = p.Do(ctx, "WAITAOF", numLocal, numReplicas, waitMs)
       }
       return nil
   })
   ```

   不能用 `p.WaitAOF`，见 d 节末尾。
4. **判定。** 错误分类规则：Redis 的错误回复（`errors.As(err, &goredis.Error)`）都是确定性错误，不重试；只有网络错误和"落盘副本数不足"才在退避 10–50ms 后重试。
   - `evalCmd.Err()`：
     - 是 Redis 错误回复（`ERR route publish: …`、`WRONGTYPE`、`OOM` 等）：计 `result="error"`，直接返回，不重试（m4）；
     - 其它（网络错误、超时）：记为 lastErr，重试。
   - `ClassifyPublishReply` 报错（形状不对）：fail-closed 返回，不重试。
   - 按结局处理：
     - Fenced：见上文第 4 步；
     - Conflict、LegacyConflict：返回 `ErrRoutePublishConflict`，不重试；
     - **Superseded 且 `CurHome != home`**：返回 `ErrRoutePublishConflict`，原因是 `superseded_home_differs`。第 1a 批里一旦写定，home 就不会再被改写；缓存里出现"更新的版本，home 却不同"，只可能是库被回档或有写者绕过了脚本，所以必须 fail-closed（M3）。
   - Applied、Confirmed，以及 home 相同的 Superseded：检查 `waitCmd`。
     - `waitCmd.Slice()` 出错：Redis 错误回复（例如 appendonly 关着时的 ERR）不重试；网络错误重试；
     - `checkWaitAOFReply(vals, numLocal, numReplicas)` 不满足：记为 `ErrRoutePublishNotDurable`，重试；
     - 满足：返回 nil。
   - 重试时整条 pipeline 一起重发。只重发 WAITAOF 不行：新连接的写偏移量可能比上次写入低，会误报已落盘。脚本在返回 0 的分支有 touch，所以重发后 WAITAOF 依然有意义。
   - **叠加的重试：** go-redis 自己也会按 `opt.MaxRetries` 在新连接上把整条 pipeline 重发（vendored `redis.go:1006-1033`）。go-zero 实际配的 MaxRetries 本机无法核实（没有模块缓存）。两层重试叠加之后，总时长仍受 ctx 截止时间约束；脚本是 set-if-newer 加 touch，重发是幂等的。
5. 每次的最终结果计一次 `route_publish_total{result}`。

### c.2 store `RegisterHome` 内部

锁顺序沿用提案 §4.1-2：route_meta（普通读）→ 围栏分片（`FOR UPDATE`）→ player_route（ODKU 或 `FOR UPDATE`）。W1 不锁 merged_into。

1. 先做参数校验，再算 `shard = playerID % fenceShards`。
2. 进入循环，最多 `routeTxAttempts = 4` 次：
   - 开事务 `BeginTx(ctx, nil)`。DSN 已经是 RC，不再额外 SET。
   - **R1：** `ErrNoRows` 返回 `ErrRouteMetaMissing`；`writable != 1` 返回 `ErrRouteNotWritable`。
   - **R2：**
     - `ErrNoRows`：回滚，在事务外跑一次 R3（autocommit），再重开事务。这次重开不消耗重试预算；如果仍是 `ErrNoRows`，返回 `ErrRouteFenceShardVanished`；
     - `active` 为 NULL 或 1：回滚，返回 Fenced，带上 run_id。
   - 如果 `!ExistingRowOnly`，执行 **R4**：
     - 受影响 1 行：COMMIT，返回 `Inserted`，Row 为 `{home, storage?, version=1?, rev=1}`；
     - 受影响 0 行：转下一步。
   - **R5：**
     - `ErrNoRows`：在 `ExistingRowOnly` 下回滚并返回 RowMissing；否则返回 `ErrRouteRowVanished`；
     - `home == req`：回滚（没有写入），返回 AlreadySame 和读到的行；
     - home 非 NULL 且不等于请求值：回滚，返回 Conflict；
     - home 为 NULL：
       - 请求带 storage 且行里落点为 NULL：执行 **R6b**，结局 `FilledHome`；
       - 否则执行 **R6a**：行里已有落点、请求也带 storage 时，结局是 `FilledHomePlacementKept`；请求不带 storage 时，结局是 `FilledHome`；
       - COMMIT 后返回新行，`rev = 旧 rev + 1`。
   - **COMMIT 出错：** 结果不确定，按 err 返回，最后是 Unavailable。调用方重试会落到幂等分支，安全。
   - **1213、9007：** `lockRetries++`，`sleepCtx(jitteredBackoff(10ms, 50ms))`（`snapshot_store.go:514-532`）后重来；ctx 已取消就停。
   - **1205：** 不在原地重试，直接返回 err，最后是 Unavailable。锁等待已经被 `innodb_lock_wait_timeout=1` 封顶，再等一轮只会白白吃掉 2s 的服务端预算，最后还是同一个结论。先例见 guild：`go/guild/internal/data/guild_manage_repo.go:101-103`。
3. **事务里绝不做 Redis 读写。** 预检在开事务之前，发布在提交之后。

**锁等待与预算的分配（M1）**

- 服务端预算：yaml 没配 `Timeout`，用 go-zero 默认的 2000ms（`data_service.yaml:1-39`）；login 侧预算是 3s（`go/login/internal/logic/pkg/homezone/homezone.go:56-58`）。
- 库这一段：锁等待最多 1s。ctx 到期时 go-sql-driver 只会关连接；服务端那个等锁的线程最多再等 1s 就放开已持有的分片锁，不会把同一个 (zone, shard) 上的建角串成最长 50s 的停顿（`go/guild/internal/svc/servicecontext.go:82-87` 记录过同一类问题）。
- 发布这一段：WAITAOF 最多 `min(1500ms, 剩余 − 100ms)`。
- 锁争用和 fsync 都慢的最坏情况，结果是 Unavailable（fail-closed）。正常路径下，库这一段只有个位数毫秒。

**为什么这套锁不会成环**

- W1 只持有一个分片行，再等最多一个玩家行；第一次碰到这个玩家行时，它手里不持有任何玩家行。
- 导入只按升序锁玩家行，不碰分片。
- 同一玩家用两个不同 zone 注册时，两边各锁自己 zone 的分片行，然后在同一个玩家行上单向排队。后到的一方 ODKU 得到 0 行，接着加锁读，得到 Conflict。
- RC 下 ODKU 撞主键只加记录锁，不加间隙锁。R3 在事务外 autocommit 执行，不持锁。

### c.3 与 redis 模式的语义对照（gRPC 码的集合不变）

| 场景 | redis 模式（今天，`router.go:152-245`） | db_first | gRPC 结果（`dataserviceserver.go:95-117` 加两支） |
|---|---|---|---|
| 新玩家 | Lua SETNX zone（以及 placement） | 预检没见到键，R4 插入 rev=1，提交后发布 `{1}`，再等 WAITAOF | OK |
| 同 home 重试 | GET 等于请求值，不写 | 预检见到键，`ExistingRowOnly` 下 R5 读到同 home，然后重新发布（补齐上次失败的发布） | OK |
| 异 home | 回既有值，Conflict | 预检发现异值，或 R5 读到异值，都返回 Conflict | AlreadyExists，码 21，message 带既有 zone |
| 既有值畸形 | unparsable 错误 | 预检用同一文案 | Unavailable，码 1 |
| 合服围栏：预检命中 | 预检 EXISTS | 预检 EXISTS（同一函数） | FailedPrecondition，码 22 |
| **合服围栏：预检之后才立** | 写入脚本里 EXISTS 复核，两键都不写 | 发布脚本 KEYS[4] 复核，三键都不写。**库里已提交的行留下（id 已烧掉）**，见 g.3 第 6 条 | FailedPrecondition，码 22 |
| 合服围栏：库分片行 | 不存在 | R2 `FOR UPDATE`。**第 1a 批里没有任何写者写 run_id，恒不生效**；到第 2 批 merge_zone 改成库先写之后才成为真正的提交点复核 | 码 22 |
| 围栏查询失败 | 当成封锁 | 同 | Unavailable，码 1 |
| home 缺、落点在 | 写 home，保留落点，打 ERROR，返回 nil | 行里 home 为 NULL、storage 在：R6a，打 ERROR，返回 nil | OK |
| home 缺、落点缺 | 写 home 和落点 | R6b（请求带 storage）或 R6a | OK |
| **新增：** 缓存无 maprev 且遗留值不同 | 不存在 | 发布脚本回 `{2}`，零写入，打 ERROR | Unavailable，码 1 |
| **新增：** Redis 认得但库里无行 | 不存在 | RowMissing，拒绝，提示先导入 | Unavailable，码 30 |
| **新增：** 库故障、store 没装配、meta 缺失、writable≠1、1205 | 不存在 | `ErrRouteStore` | Unavailable，码 30 |
| **新增：** 已提交但发布失败或未确认落盘 | 不存在（单个脚本原子完成） | 发布错误 | Unavailable，码 1。login 照常 fail-closed 并释放名字（`createplayerlogic.go:268-273`），重试走幂等分支补发 |
| **新增：** player_id=0 或 home 越界 | 写入垃圾；login 只挡 0（`homezone.go:184`） | `ErrInvalidRegistration`，只有 db_first 可达 | **Unavailable，码 14**（`ErrCodeInvalidRequest`） |

gRPC 码的集合与今天完全相同：OK、AlreadyExists、FailedPrecondition、Unavailable。新增的码 30，以及 Unavailable 下新出现的码 14，都只体现在 message 里。login 只看有没有 err（`homezone.go:180-190`）。`dataserviceserver.go:83-94` 的契约注释要同步补上码 30 和码 14 两种 Unavailable。

---

## d) 配置

`config.Config` 新增字段 `RouteStore RouteStoreConfig \`json:",optional"\``，放在 `Store` 之后（`config.go:52-53`）。

```go
const (
    RouteStoreModeRedis   = "redis"
    RouteStoreModeDBFirst = "db_first"
)
type RouteStoreConfig struct {
    Mode               string `json:",optional"` // ""→redis
    FenceShards        int    `json:",optional"` // 0→8
    MaxOpenConn        int    `json:",optional"` // 0→20
    MaxIdleConn        int    `json:",optional"` // 0→5
    PublishAttempts    int    `json:",optional"` // 0→3
    WaitAOFNumLocal    *int   `json:",optional"` // nil→1;显式 0 = 不要求本地落盘
    WaitAOFNumReplicas *int   `json:",optional"` // nil→0
    WaitAOFTimeoutMs   int    `json:",optional"` // 0→1500
}
func (r RouteStoreConfig) Normalize() RouteStoreConfig
func (r RouteStoreConfig) Validate() error
func (r RouteStoreConfig) DBFirst() bool
func (r RouteStoreConfig) WaitsForAOF() bool // local>0 || replicas>0;两者都显式写 0 = 不发 WAITAOF(miniredis 单测用)
func (r RouteStoreConfig) NumLocal() int; func (r RouteStoreConfig) NumReplicas() int
```

WaitAOF 两项用指针，原因同 `SchemaConfig.AutoMigrate`（`config.go:256-262`）：go-zero 不会下钻一个未出现的 optional 段，而这里的安全默认值不是零值。

**校验规则：** 不论 Mode 是什么，任何一项非法都拒启。拒绝而不纠正，理由同 `PlayerNameConfig.Validate`（`config.go:447`）。

| 键 | 规则 |
|---|---|
| `Mode` | 只能是 `redis` 或 `db_first`，大小写敏感。`DB_FIRST`、`db-first`、`mysql` 一律拒绝，防止拼错后静默降级 |
| `FenceShards` | `[1, 64]`，即 `placement.MaxRouteFenceShards` |
| `MaxOpenConn` / `MaxIdleConn` | `MaxOpenConn > 0`，且 `0 ≤ MaxIdleConn ≤ MaxOpenConn` |
| `PublishAttempts` | `[1, 5]` |
| `WaitAOFNumLocal` | `{0, 1}` |
| `WaitAOFNumReplicas` | `[0, 16]` |
| `WaitAOFTimeoutMs` | `[1, 1500]`（Normalize 之后）。上界由 2000ms 服务端默认超时减去余量得到；另外 go-redis 默认 ReadTimeout 是 3s |

**默认值 1500ms 的理由（M2）。** 在 `appendfsync everysec` 下，WAITAOF 的本地确认最长要等到下一次 fsync，大约 1s，再加上事件循环的延迟。原来的 1000ms 正好压在这条边界上，会系统性地抬高"已写入但未确认"的比例；每一次这样的失败，都会留下一行已提交的库记录加一组 Redis 键，即 login 那段注释说的"幽灵映射"（`createplayerlogic.go:229-236`）。

本批**不**给 RegisterPlayerZone 加 `MethodTimeouts`，原因有两个：

- K8s 的 ConfigMap 不镜像那张表（`data_service.yaml:35`）；
- 本批所有档位都是 redis 模式。

不加 MethodTimeouts 的代价是一道 P2 门禁：切换到 db_first 之前，Codex 必须按 AGENTS.md §6 压测，拿到 W1 的 p99 和"发布未确认率"。不可接受时，二选一：给 mapping 实例配 `appendfsync always`（提案 §10 第 5 条），或者调整服务端的标量 Timeout，并同步更新 `Assert-GrpcClientDeadlineBudget` 的登记（`data_service.yaml:36-39`）。

**在哪里校验、做了什么：**

- **`data_service.go`：** 在 `-migrate` 分支（`:86-88`）之后执行 `c.RouteStore = c.RouteStore.Normalize()` 和 `Validate()`。非法时写 stderr 并 `logx.Errorf`，然后 `os.Exit(1)`。db_first 时：
  - 打 Info，列出各项参数；
  - **再打一条 WARN**：`RouteStore.Mode=db_first: 第 2 批落地前,禁止 merge_zone 任何写模式(merge/unmerge/relocate/pin-placement/backfill);库内合服围栏在本批恒不生效,提交点复核靠发布脚本里的 merge:in_progress(proposal §11.2)`；
  - 调 `metrics.PrimeRoute()`。
- **`NewRouter`：** 内部同样 Normalize 并 Validate，非法就 panic，与 `MustNewRedis` 同口径。
- **`svc.NewServiceContext`：** 执行 `c.RouteStore = c.RouteStore.Normalize()`。只有 `DBFirst() && schemaReady` 时才建 store：
  - 连接参数用 `MySQLConfigOf(c)`，池大小覆盖为 RouteStore 段的值；
  - writer 用 `NewRouteWriterID("data_service", os.Hostname())`；
  - 失败时打 ERROR，store 保持 nil。
- **新接口：** 在 svc 包里声明 `type PlayerRouteStore interface { routing.RouteStore; Close() error }`，与 `servicecontext.go:20-67` 的其它 store 一样用接口。`routing.RouteStore` 由消费方定义，只有 `RegisterHome` 一个方法。`ServiceContext` 新增字段 `PlayerRouteStore PlayerRouteStore`，Close 时关闭（nil 安全）。
- **nil 接口陷阱：** 只有 store 非 nil 时才追加 `routing.WithRouteStore(rs)`，否则路由器会拿到一个"非 nil 接口包着 nil 指针"。
- **`MissingStores()`（`servicecontext.go:125-140`）：** 在 `Config.RouteStore.DBFirst() && PlayerRouteStore == nil` 时追加 `"player_route"`。K8s 各档都是 `Store.Required=true`（`k8s_deploy.ps1:3523-3524`），这时会拒启。

**`etc/data_service.yaml`：** 加在 `Schema:` 段（`:172`）之前，写满所有键。

```yaml
# 玩家归属 / 落点映射的提交点(docs/design/player-route-mapping-durability-proposal.md §4、§11.2)。
# redis(默认):与 2026-10-12 之前逐字相同。db_first:先写全局库 player_route,再按 maprev set-if-newer 发布到 mapping Redis。
# 切 db_first 的前提(缺一不可):
#   1) 已在冻结窗口内:第 2 批落地前禁止 merge_zone 任何写模式(merge/unmerge/relocate/pin-placement/backfill);
#   2) P1 导入完成且 audit 为 0:
#        go run ./cmd/route_tool -f etc/data_service.yaml import-from-redis -dry-run=false
#        go run ./cmd/route_tool -f etc/data_service.yaml audit
#   3) mapping Redis 为 noeviction + appendonly yes、版本 ≥ 7.2(WAITAOF);
#   4) 建角压测已出 W1 p99 与发布未确认率(AGENTS.md §6)。
# 非法值 = 拒启。WAITAOF 两项都显式写 0 = 不等落盘(只给没有 AOF 的环境 / 单测用);redis-ha 档 NumReplicas 要写 1。
RouteStore:
  Mode: redis
  FenceShards: 8
  MaxOpenConn: 20
  MaxIdleConn: 5
  PublishAttempts: 3
  WaitAOFNumLocal: 1        # 本机 compose redis 开着 --appendonly yes(deploy/docker-compose.yml:235)
  WaitAOFNumReplicas: 0
  WaitAOFTimeoutMs: 1500
```

**K8s ConfigMap：** 插在 `k8s_deploy.ps1:3524` 之后。所有档位固定写 `redis`，不镜像服务 yaml。

```
# 映射提交点(player-route-mapping-durability-proposal.md §4、§11.2,第 1a 批):所有档位固定 redis,不镜像服务 yaml ——
# 切 db_first 是 P2 的显式运维动作:先进冻结窗口(第 2 批前禁 merge_zone 一切写模式)、P1 导入 + audit 为 0、建角压测;
# 切换时还要按 redis 拓扑补 WaitAOF*(redis-ha:NumReplicas=1),并确认镜像里有 route_tool(第 1b 批补入口,见 g.2 第 8 条)。
RouteStore:
  Mode: redis
```

**go-redis 的 `WaitAOF` 已核实不可用。** 本机没有模块缓存，但 `robot/vendor` 里有同版本的 go-redis（v9.18.0，与 `go/data_service/go.mod:11` 一致）：

- `cmdable.WaitAOF` 返回 `*IntCmd`（`commands.go:297-302`）；
- `IntCmd.readReply` 调 `ReadInt`（`command.go:885-888`）；
- `ReadInt` 遇到数组回复会报错（`internal/proto/reader.go:393-418`），而 WAITAOF 返回的是 `[numlocal, numreplicas]` 两元素数组。

所以必须用 `Do("WAITAOF", ...)` 加 `Cmd.Slice()`（`command.go:594-604`）。

---

## e) `route_tool` 两个子命令

**调用形式**

```
route_tool -f <data_service.yaml> import-from-redis [-dry-run=true] [-batch=500] [-timeout=30m] [-max-report=200]
route_tool -f <data_service.yaml> audit             [-batch=500] [-timeout=30m] [-max-report=200] [-recheck-delay=2s] [-max-recheck=10000]
```

**装配：**

- 配置加载方式同 `debugutil.MustLoadConfig`（`debugutil.go:37-46`）。
- RouteStore 段做 Normalize 和 Validate，但 Mode 与本工具无关。
- 连 mapping Redis 用 `redis.NewRedis(c.MappingRedis)`，返回 error 而不是 panic（go-zero v1.10.0，`go.mod:14`；签名待 Codex 编译核实）。如果编不过，就在 `MustNewRedis` 外面包 defer-recover，映射成退出码 2。永远是 DB 0，与 data_service 同一个实例（`router_integration_test.go` 的 `TestMappingRedisEffectiveDBIsZero`）。
- 连库用 `store.NewRouteStore(MySQLConfigOf(c) + RouteStore 的池大小, {FenceShards, Writer: NewRouteWriterID("route_tool", host)})`。探针失败（例如没跑过迁移，或会话变量不对）时退出码 2，提示"先跑 `data_service -f <yaml> -migrate`"。
- 输出不打印 DSN 和口令，只打 host 和库名。
- `-batch` 范围 `[1, RouteBatchLimit]`；参数非法或子命令未知，退出码 2，并打印用法。

**内部接口（便于单测）**

```go
type mappingReader interface { // go-zero *redis.Redis 天然满足(router.go:380、:456 已在用)
    ScanCtx(ctx context.Context, cursor uint64, match string, count int64) ([]string, uint64, error)
    MgetCtx(ctx context.Context, keys ...string) ([]string, error)
}
type routeDB interface {
    ImportRoutes(...); ImportMergedInto(...); ScanRoutes(...); GetRoutes(...); ListMergedInto(...)
}
```

**规范形式校验（m6），import 与 audit 共用，放在 `redisread.go`：**

| 对象 | 合法条件 |
|---|---|
| 键名后缀 | `id != 0`，且 `strconv.FormatUint(id, 10) == 后缀`。`player:zone:007` 判畸形 |
| zone 值 | `placement.ParseHomeZone` 能解析，`≤ 999999`，且 `FormatUint(zone) == 原值`。`"0012"` 判畸形 |
| placement 值 | `placement.Parse` 能解析，且 `rec.Encode() == 原值`。`"01:1"` 判畸形 |
| maprev | `placement.ValidRev` |
| merged_into | JSON `{dst, run_id}`，允许未知字段，字段只增不改名（`tools/merge_zone/merged_into.go:38-42`）。src 是规范后缀；`dst ≥ 1` 且 `≠ src`；`placement.ValidRunID(run_id)`（`placement.go:168-180`）。merge_zone 的 run_id 格式 `{src}-{dst}-{UTC}-{hex8}`（`fence.go:316-318`）满足这一条 |

### import-from-redis（提案 P1）

stderr 的第一行固定输出：`NOTE: 只能检测 merge/unmerge 的合服围栏(merge:in_progress:*);relocate / pin-placement / backfill 不立围栏,冻结窗口由人保证(proposal §7 P1)`。

1. **开头的围栏检查：** SCAN `merge:in_progress:*`，只要有一把键就退出码 3，一行都不写。
2. **A 遍：** 逐页 SCAN `player:zone:*`。
   - 键名后缀不规范：记 MALFORMED。
   - 这一页的 id 一次 MGET 三族键（zone、placement、maprev）。
   - 任一值不规范：记 `MALFORMED player=… key=… value=%q reason=…`，**整名玩家跳过**，不导半截。
   - 合法的按 player_id 升序排好，凑成批调 `ImportRoutes`。
   - 把处理过（包括判为畸形）的 id 记进集合 `seenA`。按 100 万玩家估算，内存约几十 MB，对运维工具可以接受。
3. **B 遍：** SCAN `player:placement:*`，**按是否在 `seenA` 里跳过**，而不是按"zone 非空"跳过（m7）。这样在 P1 期间（redis 模式仍在登记），A、B 两遍之间新写入 zone 的玩家也不会被漏掉。其余流程同 A 遍。
4. **merged_into：** SCAN `merge:merged_into:*`，MGET 取值，按上表校验。
   - 不合法的记 MALFORMED；合法的调 `ImportMergedInto`；
   - `merged_at_ms` 取导入时刻的库时钟，原始时刻已无从得知，这一点写进文档；
   - 同 src 而 dst 或 run_id 不同：记冲突，不覆盖。
5. **结尾复查围栏：** 再 SCAN 一次 `merge:in_progress:*`，命中就退出码 3。已写入的部分保留，窗口过后重跑；导入是幂等的。
6. **dry-run（默认为 true）：** 只做 SELECT 判定，不写任何东西，同样输出统计。

**输出**

- stdout 只有一行：`route_import dry_run=… players_scanned=… inserted=… matched=… filled=… db_ahead=… rev_regression=… conflicts=… malformed=… merged_into_inserted=… merged_into_matched=… merged_into_conflicts=… merged_into_malformed=…`
- stderr 输出明细行 `MALFORMED`、`CONFLICT player=%d db_home=… db_placement=%q db_rev=… redis_home=… redis_placement=%q redis_maprev=…`、`REV_REGRESSION player=%d db_rev=… redis_maprev=…`，最多 `-max-report` 条，计数始终精确。

**退出码：**

- 0：完成，且 conflicts、malformed、rev_regression 都是 0；
- 1：完成，但上述任一项非 0，需要人工处理；
- 2：失败（配置、连接、查询或写入出错，包括 1205；或超时）；
- 3：有生效的合服围栏，已中止。

### audit（只读）

1. **方向 A，库对 Redis：** 用 `ScanRoutes` 按键集翻页，每页 MGET 三族键。墓碑行（两列都是 NULL）视为无记录。逐名玩家比对，分类如下：
   - `db_only_unpublished`：行不是墓碑，且三族键**全部**不在。这类行可能是 W1 已提交、未发布的孤儿，也可能是 Redis 丢了已确认的发布，两者本批分不开，都要等第 1b 批的预热推送补齐。单独计数（M2，评审 2 的 minor）；
   - `missing_in_redis`：库里这一列有值，Redis 缺这把键，但三族键不是全部缺席；
   - `missing_in_db`：库里这一列是 NULL，Redis 有值；
   - `value_mismatch`：两边都有，但不同；
   - `redis_malformed`：Redis 值不规范（含 maprev）；
   - `db_invalid`：库里这一行过不了 `Placement()` 的校验；
   - `redis_rev_ahead`：maprev 合法且大于库里的 rev，这是"库被回档"的信号（M3）。
2. **方向 B，Redis 对库：** SCAN `player:zone:*`、`player:placement:*`、`player:maprev:*` 三族键，每页调 `GetRoutes`。库里没有行的 id 记 `missing_in_db`，用集合去重，避免一个 id 在多族键里重复计数。库里有行的已在 A 遍比过，跳过。
3. **merged_into：** 库列表与 Redis 全量对比，计 `merged_into_missing_in_redis`、`merged_into_missing_in_db`、`merged_into_mismatch`。
4. **复核（recheck）：**
   - 差异玩家不超过 `-max-recheck` 时，等 `-recheck-delay` 后重读这些 id 的两边，只有仍然存在的差异才计入；
   - 超过上限时全部计入，并打一行 WARN；
   - `transient_cleared` 记被复核清掉的数量。
5. **生效中的合服围栏：** `redis_fences_active` 只做提示，并打 WARN"合服窗口内审计结果可能包含瞬时差异"。

**输出：** stdout 一行 `route_audit db_rows=… redis_players=… db_only_unpublished=… missing_in_redis=… missing_in_db=… value_mismatch=… redis_malformed=… db_invalid=… redis_rev_ahead=… merged_into_missing_in_redis=… merged_into_missing_in_db=… merged_into_mismatch=… redis_fences_active=… transient_cleared=…`；stderr 输出 `DIFF kind=… player=… db=… redis=…` 明细。

**退出码：**

- 0：所有差异计数都是 0（`redis_fences_active` 和 `transient_cleared` 不算）；
- 1：有差异，**`db_only_unpublished` 也算**；
- 2：查询失败，或参数、配置错误。

"`db_only_unpublished` 在第 1a 批无法自动消除"写进 runbook：它要等第 1b 批的全量预热推送补齐。在 db_first 下，P3 的"audit 为 0"门禁要等第 1b 批落地之后才可达。

---

## f) 测试清单

**DB 替身的选择：**

- 仓库里没有 sqlmock。
- 结局判定和语句、参数用假驱动契约测试钉住，默认运行。先例是 `player_name_store_contract_test.go:1-80`；本批要在它的基础上扩展，支持 `ConnBeginTx`，以及 Tx 的 Commit 和 Rollback，并把 `BEGIN`、`COMMIT`、`ROLLBACK` 记进语句序列。
- 锁、ODKU 行数、CHECK、并发只能靠 storetest 建真库来证：`//go:build integration`。环境变量是 `DATA_SERVICE_IT_MYSQL_HOST`、`_USER`、`_PASSWORD`。连不上或报 1044/1045 时，用例 Skip。
- 编排逻辑在 routing、server、routetool 里，通过接口注入假实现，不连库。

**命名约束（评审 2 第 1 条 major）：** 新单测一律不用 `TestRegisterPlayerZone_` 前缀，以免被复验清单的 `-run` 选中、打乱它"routing 10 个"的计数（`docs/handoff/placement-reverify-checklist-20261012.md:175`、`:182`）。

### 单元测试（默认 `go test`，不需要任何外部依赖）

**`shared/placement/route_test.go`**

- `TestRouteKeys`：`MapRevKey(90002) == "player:maprev:90002"`；`RouteReadyKey == "mapping:route:ready"`；`PublishRouteKeys(7, 0)` 有 3 把键，`PublishRouteKeys(7, 2)` 第 4 把是 `MergeFenceKey(2)`。
- `TestRevVectors`：
  - 合法：`1`、`9`、`10`、`18446744073709551615`；
  - 非法：`""`、`0`、`01`、`-1`、`1a`、`" 1"`、21 位数、`18446744073709551616`。
- `TestCompareRevLengthBeforeLexicographic`：`(9,10)→-1`、`(2,10)→-1`、`(10,9)→1`、`(10,10)→0`、`(MaxUint64, MaxUint64−1)→1`。
- `TestClassifyPublishReply`：
  - 应判成功的结局：Applied、Superseded、Confirmed；
  - Conflict 两种：home 不同、placement 不同；
  - LegacyConflict；Fenced；
  - 应报错的输入：0 回复里 `cur < rev`；2 回复里值相同；cur 是 20 位数但超过 MaxUint64；形状错误（`nil`、`[]any{}`、`[]any{"1","3"}`、`[]any{int64(4)}`、`[]any{int64(3),"x"}`）。
- `TestRouteFenceShard`：`(17,8)→1`；shards 为 0 或 65 时报错。

**`store/route_store_test.go`**

- `TestRouteDSNSetsReadCommittedAndLockWaitWithoutClientFoundRows`
- `TestRouteBootstrapDDLContract`：
  - 4 段 DDL 都以对应表名的 `CREATE TABLE IF NOT EXISTS` 开头；
  - `player_route` 含 NONCLUSTERED、`SHARD_ROW_ID_BITS=4`、`PRE_SPLIT_REGIONS=4`、5 个 `chk_route_*`、3 个 `idx_route_*`；
  - `zone_merge_fence` 含 `CLUSTERED`；
  - `bootstrapTables` 共 6 项；
  - `routeTableShapes` 的键恰好是这 4 张表，每一列都在对应 DDL 里出现。
- `TestRouteSQLNeverUsesInsertIgnore`
- `TestNoPlayerRouteWritesOutsideRouteStore`：用 `go/parser` 解析 `go/data_service` 下所有非 `_test.go`、非 `generated/` 的文件，**只检查字符串字面量**（`*ast.BasicLit`，Kind 为 STRING，先 unquote），不看注释（m9）。正则是 `(?i)(INSERT\s+INTO|UPDATE|DELETE\s+FROM|REPLACE\s+INTO)\s+` + "`?player_route`?"，只允许出现在 `internal/store/route_store.go`。局限：跨字面量拼接出来的语句查不到，在注释里写明。
- `TestDecideImport`：覆盖 b.2 判定表的每一格，加上 rev 规则（无行时 `rev = maprev 或 1`、revRegression）。
- `TestRouteRowPlacementValue`：稳定态、冻结态、version 为 0 时报错。
- `TestNewRouteWriterID`：非 ASCII 被清洗、超长被截断、host 为空时用 `unknown`。

**`store/route_store_contract_test.go`（假驱动，新增）**

- `TestRegisterHomeOutcomeContract`，表驱动，每条都断言 SQL 文本、参数、BEGIN/COMMIT/ROLLBACK 的序列：
  - R4 受影响 1 → Inserted；0 → 进入 R5；2 → 报错；
  - R5 的 `ErrNoRows`：在 `ExistingRowOnly` 下 → RowMissing；否则 → `ErrRouteRowVanished`；
  - R5 同 home → AlreadySame，回滚；异 home → Conflict；
  - R6a、R6b 的分支选择；受影响不为 1 时 fail-closed；
  - R2 读到 `ErrNoRows` → R3 → 重开事务，再读到 `ErrNoRows` → `ErrRouteFenceShardVanished`；
  - active 为 NULL 或 1 → Fenced，带 run_id；
  - R1 的 `ErrNoRows` → `ErrRouteMetaMissing`；writable=0 → `ErrRouteNotWritable`。
- `TestRegisterHomeBindsNullForAbsentPlacement`：storage=0 时，R4 的第 3、4 个参数是 `nil`；storage≠0 时是 `uint64`。
- `TestRegisterHomeRejectsInvalidArgsBeforeAnyStatement`
- `TestRegisterHomeRetriesDeadlockAndWriteConflict`：1213、9007 被重试并计数。
- `TestRegisterHomeDoesNotRetryLockWaitTimeout`：1205 只发一次。
- `TestRegisterHomeRetryStopsOnCanceledContext`
- `TestImportRoutesStatementOrderAndAffectedMismatchRetries`：R8 → R7 → R9 → COMMIT；R7 行数不等时回滚重试，最多 3 次。
- `TestImportRoutesDryRunIssuesNoWriteAndNoLockingRead`
- `TestNewRouteStoreProbeFailClosed`：隔离级别不对、锁等待不是 1、TiDB 加 optimistic、meta 行缺失，四种都返回 error 且关池。

**`config/config_test.go`**

- `TestRouteStoreDefaultsWhenBlockAbsent`：用 `loadYaml(t, minimalYaml)`（`config_test.go:14-28`），Normalize 之后是 redis、8、20/5、3、1/0/1500，并且 Validate 通过。
- `TestRouteStoreValidateRejects`：表驱动，覆盖 d 节的每一条拒绝规则。
- `TestRouteStoreExplicitZeroWaitDisables`
- `TestShippedYamlRouteStoreIsRedisAndValid`

**`routing/route_publish_test.go`（28 个）**

miniredis 一律显式配 `WaitAOFNumLocal=0`、`WaitAOFNumReplicas=0`。miniredis 是否支持 WAITAOF 没有核实（Q4）。

脚本语义（8 个）：

1. `TestPublishRouteScript_AppliesWhenAbsent`
2. `TestPublishRouteScript_LengthBeforeLexicographic`：缓存为 9 时发 10，应写入；缓存为 10 时发 9，应 Superseded
3. `TestPublishRouteScript_EqualSameIsConfirmed`
4. `TestPublishRouteScript_EqualDifferentIsConflict`
5. `TestPublishRouteScript_EmptyArgDeletes`
6. `TestPublishRouteScript_RejectsBadRevAndMalformedMapRev`：三把键都零变更
7. `TestPublishRouteScript_LegacyValuesWithoutMapRev`：表驱动——遗留 zone 不同 → `{2}`，零变更；遗留 placement 存在而 ARGV 为空 → `{2}`，零变更；遗留值相同 → Applied；全部缺席 → Applied；只有遗留 placement 且相同、zone 缺席 → Applied（补 home）
8. `TestPublishRouteScript_FenceKeyBlocksEverything`：KEYS[4] 存在时回 `{3}`，三键零变更；cur 存在时也是 `{3}`，不 touch

db_first 编排（17 个），假 store 记录调用：

9. `TestDBFirstRegister_InsertPublishesThreeKeys`：zone、placement、maprev 为 `"1"`
10. `TestDBFirstRegister_SameHomeRepublishesMissingKeys`
11. `TestDBFirstRegister_ConflictFromStore`
12. `TestDBFirstRegister_RedisPrecheckConflictSkipsStore`
13. `TestDBFirstRegister_RedisFenceSkipsStore`
14. `TestDBFirstRegister_StoreFencedIsMergeInProgress`
15. `TestDBFirstRegister_FenceRaisedAfterPrecheckBlocksPublish`（B1）：在假 store 的回调里 `mr.Set(MergeFenceKey(home), …)`，断言返回 `ErrZoneMergeInProgress`、三键零变更
16. `TestDBFirstRegister_RedisKnownPlayerRequiresExistingRow`：store 收到 `ExistingRowOnly=true`；RowMissing 时返回 `ErrRouteStore`，Redis 零变更
17. `TestDBFirstRegister_StoreErrorWrapsErrRouteStore`
18. `TestDBFirstRegister_NilStoreFailsClosed`
19. `TestDBFirstRegister_PublishFailureAfterCommitThenRetryRepairs`：回调里 `mr.Close()`，之后 `mr.Restart()` 再重试
20. `TestDBFirstRegister_InvalidArgsNeverReachStore`
21. `TestDBFirstRegister_PlacementKeptSucceeds`
22. `TestDBFirstRegister_LegacyPlacementMismatchRefused`（B2）：Redis 预置 zone 和 placement，无 maprev；store 回 AlreadySame，但行里落点不同；断言返回 `ErrRoutePublishConflict`，两键原值不变
23. `TestDBFirstRegister_SupersededWithDifferentHomeFails`（M3）
24. `TestDBFirstRegister_DeterministicScriptErrorNotRetried`（m4）：预置畸形 maprev，断言只发了一次 EVAL
25. `TestDBFirstRegister_RedisModeNeverCallsStore`

纯函数（3 个）：

26. `TestCheckWaitAOFReply`：`[1,0]`、`[0,0]`、本地满足但副本不足、形状错误
27. `TestWaitAOFBudgetMs`（m5）：剩余 100.5ms 加 100ms 留量 → 截断成 0 → `ok=false`；没有截止时间 → 取配置值；配置值小于剩余 → 取配置值
28. `TestNewRouterPanicsOnInvalidRouteStore`

另外删除 `router_test.go:469-528` 的 3 个 remap 用例。删完之后 `router_test.go` 有 26 个，routing 单测合计 54 个。

**`server/dataserviceserver_test.go`**

- 新增辅助函数 `newDBFirstTestServer(t, fake)`：在 `newHomeZoneTestServerWithConfig`（`:40-52`）的基础上，Config **显式写** `RouteStore{Mode: "db_first", WaitAOFNumLocal: ptr(0), WaitAOFNumReplicas: ptr(0)}`，用 `routing.NewRouter(c, routing.WithRouteStore(fake))` 装配。
- `TestDBFirstRegister_ErrorMapping`：表驱动，验证 AlreadyExists+21、FailedPrecondition+22（store 侧围栏和发布侧围栏两条路径）、Unavailable+30、Unavailable+1（发布失败、遗留值冲突）、**Unavailable+14**（参数非法）。断言一律用 `errCodeTag(constants.X)`（`:356-358`）。
- 用 `TestRemapHomeZoneForMerge_AlwaysFailedPrecondition` 替换 `:242-352` 的 5 个用例。覆盖四种情况：token 未配、token 错误、token 正确、token 正确且有围栏标记。全部返回 FailedPrecondition 加 `ErrCodeNotImplemented`，且 Redis 零变更。名字保留前缀，复验清单 `:175` 的 `-run` 仍能选中它。
- `TestDeletePlayerData_DeleteZoneMappingIsFailedPrecondition`：数据键和映射都还在。

**`logic/data_logic_test.go`**

`:313-333` 改写为 `TestDeletePlayerData_ZoneMappingBranchDisabled`。断言：

- `ErrorCode == ErrCodeNotImplemented`；
- `errors.Is(err, ErrZoneMappingDeletionDisabled)`；
- 之后 `GetPlayerField(…, 70, "x")` 仍返回 `"y"`，`GetPlayerHomeZone` 仍返回 2。

**`routetool/routetool_test.go`**（miniredis + 假 routeDB）

- import：
  - 开头有围栏 → 退出码 3，零调用；结尾才出现围栏 → 退出码 3；
  - 畸形值整名玩家跳过且计数；不规范的值（`007`、`0012`、`01:1`）判 MALFORMED；
  - B 遍按 `seenA` 跳过，A、B 两遍之间新出现 zone 的玩家不漏；
  - maprev 传进 `ImportRoutes`；rev_regression → 退出码 1；
  - merged_into 的 JSON 畸形；run_id 非法；
  - `-dry-run` 时向 store 传 `dryRun=true`；
  - stderr 第一行有 NOTE。
- audit：两个方向的各类差异计数（含 `db_only_unpublished`、`redis_rev_ahead`、maprev 族）；复核能清掉瞬时差异；退出码 0、1、2 的映射。

**`cmd/route_tool/main_test.go`**

缺 `-f`、未知子命令、`-batch=0` 都返回退出码 2；Redis 地址不可达时返回 2，不能 panic。

### 集成测试（`-tags=integration`）

**`store/route_store_integration_test.go`**（storetest 一次性真库）

- `TestRouteSchema_MigrationCreatesTablesSeedsMetaAndIsIdempotent`：4 张表都在，主键正确；route_meta 恰好一行，generation 是 32 位十六进制；第二次迁移后 `SHOW CREATE TABLE` 和 generation 都不变。
- `TestRouteSchema_CheckConstraintsEnforced`：MySQL ≥ 8.0.16 时插 home=0，期望 3819；更低版本或 TiDB 时 Skip，并写明原因。
- `TestRouteSchema_ShapeGuardRejectsTamperedTable`：`ALTER TABLE player_route DROP COLUMN updated_by` 之后，`MigrateSchema` 应报错。不能删 `rev`：它被 `chk_route_rev` 引用，MySQL 会以 3959 拒绝删列。
- `TestRouteStore_RegisterHomeBranches`：插入、同值、冲突、补 home（带或不带 storage）、落点保留、冻结行重读。
- `TestRouteStore_NullPlacementBindsNull`（m1）：storage=0 注册后，`storage_id` 和 `placement_version` 都是 NULL。
- `TestRouteStore_ConcurrentSamePlayer`：
  - 32 个 goroutine 用同一 home：恰好 1 个 Inserted，其余都是 AlreadySame；
  - 用不同 home：恰好 1 个胜者，其余都是 Conflict，且 Existing 等于胜者。
- `TestRouteStore_ConcurrentManyPlayersSameZone`：全部成功，打印 lockRetries。
- `TestRouteStore_LockWaitBoundedAfterClientCancel`（M1）：
  - 原始连接开事务，`FOR UPDATE` 锁住玩家行 P 并一直持有；
  - 对 P 发 `RegisterHome`，ctx 300ms，应超时失败；
  - 随即对同一 (zone, shard) 上的玩家 Q 发 `RegisterHome`，应在开始后 2s 内成功（默认 50s 的锁等待下会卡住）；
  - 用例结束时回滚持锁事务。
- `TestRouteStore_FenceBlocksAndExpires`：用原始连接锁住该 zone 的 64 个分片行，写入 run_id 和未来的过期时间后提交，`RegisterHome` 应返回 Fenced；把过期时间改到过去后，可以正常注册。
- `TestRouteStore_LazyFenceShardCreation`
- `TestRouteStore_MetaMissingAndNotWritableFailClosed`：player_route 行数为 0。
- `TestRouteStore_ExistingRowOnlyNeverInserts`
- `TestRouteStore_ImportClassifiesAndIsIdempotent`：第二次导入写入为 0；dry-run 零写入；冲突行不被改；maprev=5 的新行，库里 rev=5；库 rev=2、maprev=5 时判 revRegression，不改。
- `TestRouteStore_ScanRoutesKeysetCoversAll`：2500 行、limit 100，无漏无重。

**`routing/router_integration_test.go`**（真 Redis，`DATA_SERVICE_IT_REDIS_HOST`）

- `TestPublishRoute_RealRedisSameConnection`：经 go-zero 的 `PipelinedCtx` 发 `[CLIENT ID, EVAL, WAITAOF, CLIENT ID]`，两次 CLIENT ID 必须相等（Q3 的实测证据）。
- `TestPublishRoute_RealRedisWaitAOF`：回复是 `[1,0]`。`CONFIG GET appendonly` 为 no，或报 unknown command（Redis < 7.2）时 Skip。
- `TestPublishRouteScript_RealRedisParity`：用与 miniredis 同一组向量。
- `TestDBFirstRegister_RealStores`：storetest 真库加真 Redis，**任一环境变量缺失就 Skip**。
- 原 `:167-209` 改名为 `TestMergeFence_RealRedisBlocksRegister`。

改完之后：routing 集成用例 9 个，带 integration tag 时顶层用例共 63 个。Codex 以 `go test -list .` 的输出核数。

**`routetool/routetool_integration_test.go`**

真库加 miniredis 造数据。跑 import（`-dry-run=false`），之后 audit 应为 0；改一个 Redis 值后 audit 应为 1；库里插一行而 Redis 不写，应计入 `db_only_unpublished`，退出码 1。

### 契约测试（PowerShell）

在 `k8s_deploy_contract.tests.ps1` 的 `$prodRun`（`:926`）之后新增：

```powershell
Test-Case "data-service ConfigMap 显式写 RouteStore.Mode: redis(dev / prod 两档),服务 yaml 也是 redis" {
    foreach ($case in @(@{ Label = 'dev'; Output = $devOut }, @{ Label = 'prod'; Output = $prodRun.Output })) {
        $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $case.Output -Name 'go-svc-data-service-config')
        Assert-Equal -Expected 'redis' -Actual (Get-FlatValue -Flat $flat -KeyPath 'data.data_service.yaml.RouteStore.Mode') `
            -Because "$($case.Label):第 1a 批所有档位固定 redis;切 db_first 是 P2 的显式动作(冻结窗口 + 导入 + audit 为 0)"
    }
    Assert-Equal -Expected 'redis' -Actual (Get-EtcValue -RelativePath 'go/data_service/etc/data_service.yaml' -KeyPath 'RouteStore.Mode') `
        -Because '本地默认必须与今天逐字相同'
}
```

前缀 `data.data_service.yaml.` 与 `:395` 同口径。`Get-EtcValue` 用点分路径的先例见 `k8s_client_entry_contract.tests.ps1:555`。

---

## g) 与 AGENTS.md 的对齐，以及与提案的偏离

### g.1 对齐

- **§4 字段类型：** player_id、rev、placement_version、`*_ms` 都是 bigint unsigned，对应 uint64；zone 和 storage 是 int unsigned，对应 uint32。storage ≥ 1000000 也放得下（`placement.go:52-56`）。本批不新增 proto。DDL 列注释里预留了 `pb:1..9`，第 2 批起补 proto 时必须沿用同样的编号。
- **§7 不变量：**
  - 不碰 SnowFlake 解析，player_id 只当不透明的 uint64 用；
  - 不涉及 Kafka；
  - 不加 tip 码（30 是 data_service 私有码表，见 `data_service.go:163-169`）；
  - 不涉及 ECS；
  - 一致性：提交点是库事务；事务里不做 Redis I/O；发布脚本复核围栏，不覆盖遗留值；发布并确认落盘之后才算成功。
- **§9：**
  - 不 push、不 commit；
  - DSN 和口令不进日志或输出；
  - 不把 player_id 当指标 label。新增两个指标：`data_service_route_register_total{outcome}`，取值 inserted、filled_home、filled_home_placement_kept、already_same、conflict、fenced、refused_unknown_row、precheck_conflict、precheck_fenced、store_error、invalid；`data_service_route_publish_total{result}`，取值 applied、confirmed、superseded、conflict、legacy_conflict、fenced、not_durable、error。两个集合都是封闭的，db_first 时由 `PrimeRoute()` 全部预置为 0，理由同 `metrics.go:240-254`；
  - 不新建 README。route_tool 的运维步骤写进 runbook（第 i 节）。
- **§10.1：** 本规格本身不编译、不测试，交付时写明"未编译，待 Codex 验证"。
- **§11：**
  - 职责：store 只管 SQL，routing 管编排与发布，shared 只放契约，routetool 管运维逻辑；
  - 依赖倒置：router 依赖消费方定义的单方法接口 `RouteStore`；svc 用接口持有 store；
  - 显式依赖：hostname 通过参数注入 writer；时间取库时钟；
  - 失败方向一律 fail-closed；
  - 重试：有次数上限、带抖动、可被 ctx 取消、按截止时间计算；确定性错误不重试；
  - 测试面向接口；没有新造框架；
  - DSN、重试、退避复用 `buildDSN`、`jitteredBackoff`、`sleepCtx`。
- **§12：** 映射存储属于服务端内部，不涉及客户端契约。Java 版状态：**待做**。PARITY.md 在第 1 批（1a + 1b）全部完成时再登记，交付说明要写明这一点（Q14）。

### g.2 与提案的偏离和代价

| # | 偏离 | 理由 | 代价 |
|---|---|---|---|
| 1 | 不新增 proto message（提案 §3.1） | 按任务要求，避免依赖 proto 重新生成和 C++ 重编 | 只守列名和主键，不守类型。第 2 批补 proto 时：删掉 `routeTableShapes`；`RouteRow` 要么改用生成类型，要么明确作为领域类型保留并写清转换（AGENTS.md §3 不许有与 proto 重复的并行 struct） |
| 2 | 用 EVAL，不用 EVALSHA（**任务原文写的是 EVALSHA**） | pipeline 里遇到 NOSCRIPT 无法透明回退；与仓库现状一致（router.go 全部用 `EvalCtx`）；W1 的频率等于建角频率 | 每次多发约 1.5KB。第 2 批 merge_zone 批量发布时可以重新评估 |
| 3 | Lua 加了 rev 校验、可选的围栏键、遗留值守卫、touch、逐字节比较 | B1：提交点复核围栏；B2：不静默覆盖遗留值；touch 让幂等重发也能用 WAITAOF 确认落盘；逐字节比较不依赖 locale | 返回 0 时多一次 SET；每次多两次 GET |
| 4 | `commitAndPublish` 的 I/O 放在 data_service routing，shared 只放脚本、键和回复分类 | `shared/placement` 约定只依赖标准库（`placement.go:19`） | 第 2 批 merge_zone 要自己写 go-redis 的 I/O，并镜像脚本 |
| 5 | **db_first 保留 Redis 预检**（EXISTS 围栏 + MGET 三键），没有删掉 EXISTS 预检 | 第 2 批之前，围栏和清单都在 Redis；库可能缺行（未导入或回档） | 多两次 Redis 往返 |
| 6 | 分片行"确保存在"改成懒执行：R2 读不到才执行 R3 | 避免每次 W1 都在事务外做一次 autocommit 写 | 语义不变 |
| 7 | 新增契约 `MaxRouteFenceShards = 64`：围栏持有者必须锁住并写满全部 64 行 | 各进程 N 配置不同时，大 N 的写者会落到持有者没锁的分片上，绕过围栏 | 获取一次围栏要锁 128 行 |
| 8 | route_tool 是独立的 cmd | 按任务要求 | **K8s 镜像只构建单一 ENTRY**（`deploy/k8s/Dockerfile.go-svc:10`），生产上跑 P1 需要第 1b 批补主程序入口 |
| 9 | W3 改为在 logic 入口零副作用拒绝，映射成 gRPC FailedPrecondition | 不能先删数据键、再拒删映射，留下半截操作 | DeletePlayerData 原本只回 in-band 码，这是唯一的例外；仓库内没有调用方 |
| 10 | 用 `Do("WAITAOF")`，不用 `WaitAOF` | go-redis v9.18.0 会把数组回复错解析成 int | 无 |
| 11 | 新增码 30 `ErrCodeRouteStoreError` | 告警能分清"库坏"和"Redis 坏" | 只出现在 message 里 |
| 12 | W2 在鉴权之前就恒拒，并删除 `Router.RemapHomeZoneForMerge` | 无条件 SET 的危害见 `merge_run.go:836-837`；仓库内只有注释提到它 | 原来的 5 个服务端用例和 3 个路由用例要改写；guild、login、match、trade、merge_zone 里提到它的注释暂不改（记入"已知未改"） |
| 13 | 清库脚本对 `route_meta` 原地复位，不 TRUNCATE | `currency_crash_window.ps1:297` 在服务运行中清库；TRUNCATE 会让 db_first 的 W1 一直 fail-closed，直到下次迁移 | 第 4 张表不是"清空"，而是"复位"；generation 不换 |
| 14 | 参数非法时 db_first 回 **Unavailable + 码 14**，不回 InvalidArgument | 任务要求"与今天的 RPC 契约一致"，gRPC 码集合保持 OK、AlreadyExists、FailedPrecondition、Unavailable | 语义上"参数错"落进了 Unavailable，靠码 14 区分；只有 db_first 可达；redis 模式下仍会写入垃圾，与今天逐字相同 |
| 15 | 会话 `innodb_lock_wait_timeout=1`；**1205 不重试**，只重试 1213 和 9007（提案 §4.1-2 写的是"1213/1205 重试 3 次"） | 默认 50s 的锁等待在 2s 预算里永远等不到 1205，ctx 到期后服务端还会继续占着分片锁；guild 先例 | 锁争用超过 1s 时直接 Unavailable；导入遇到 1205 时整体失败，靠重跑（幂等）恢复 |
| 16 | `zone_merge_fence` 主键加 `/*T![clustered_index] CLUSTERED */` | TiDB 建表后不能改聚簇属性；聚簇才能按 zone 值 SPLIT | MySQL 上没有影响 |
| 17 | 导入写入 `rev = maprev`；库 rev 小于 maprev 时不写 | 防止导入把"库被回档"洗白成 rev 倒退，导致之后的发布被静默判成 Superseded（M3） | 导入多读一族键 |
| 18 | `WaitAOFTimeoutMs` 默认 1500ms；"发布未确认率"列为 P2 门禁 | everysec 下本地确认最长约 1s 以上，1000ms 正压在边界上 | W1 最坏耗时接近 2s；切换之前要先压测 |

**仓库内没有 W2/W3 生产调用方的证据：**

- grep `RemapHomeZoneForMerge`：除 data_service 自身外，只出现在 guild、login、match、trade、merge_zone 的注释里，robot 里只有生成的消息 id，cpp、java、ps1 中都没有；
- `DeleteZoneMapping` 只在 data_service 内部（`data_logic.go:437`、`dataserviceserver.go:266-269`）和测试里出现；
- `rollback_logic.go:1064` 是历史注释。

### g.3 未解决的风险（需要知情）

1. **TiDB 热点**（用户本轮专门强调过；MySQL 上不存在）：
   - `player_route` 的主键唯一索引和 `idx_route_updated` 都是单调递增的。NONCLUSTERED 加 SHARD_ROW_ID_BITS 只打散行数据，打散不了索引尾部（`global-data-layer-tidb-decision.md:119` 已登记）。`idx_route_home(home_zone, player_id)` 在单个 zone 内同样是尾部热点。
   - **`zone_merge_fence` 整张表只有"zone 数 × 8"行，必然落在同一个 Region。** TiDB 悲观事务里的 `FOR UPDATE` 是往 TiKV 写锁记录，所以全服每一次建角，都在同一个 Region Leader 上加锁、解锁。分片只拆开了行锁排队，拆不开 Region 热点。TiDB Phase 1 上线前，必须执行 `SPLIT TABLE zone_merge_fence BY (2,0),(3,0),…`，按 zone 预切，并纳入压测（写进 TiDB 实施清单，见第 i 节）。
   - `route_meta` 是单行，每次 W1 都要读，是读热点。
   - `idx_route_updated` 本批没有读方，保留它的理由见评审处理记录 M5。
2. **TiDB 的 CHECK 约束** 要求 ≥ 7.2，并开启 `tidb_enable_check_constraint`（默认关闭），所以应用层校验不能省。
3. **`writable` 是普通读。** 第 1b 批把它置 0 时，正在进行的事务仍会提交，1b 的流程要先等这些事务结束。
4. **db_first 依赖冻结纪律（M4）。** 第 1a 批里库内围栏恒不生效。在 db_first 下运行 relocate、pin、backfill，会改出一份库不知道的 Redis 值。B2 能挡住"之后的 W1 重发把它改回旧值"，但挡不住"库与缓存长期分叉"，这类分叉要等 audit 发现。导入只能检测合服、撤销合服的围栏。
5. **P2 混跑窗口。** 同一个 player_id 被新旧副本同时、用不同 home 注册：有了 B2，最多只有一边成功，另一边返回 Unavailable 或 AlreadyExists。残留问题是"库是 B、Redis 是 A"的分叉，由 audit 发现。player_id 每次建角都新发，实际几乎不可能发生。
6. **已提交未发布的孤儿行。** 发布失败、WAITAOF 超时、发布时撞上围栏，都会在库里留下 id 已烧掉的行，audit 计为 `db_only_unpublished`，只能等第 1b 批的预热补齐。第 1b 批在推送之前，应先按 audit 清单人工核对：哪些行的 home 指向了已合走的 zone（只有违反冻结纪律时才会出现）。发布未确认的那种情况，还会同时留下 Redis 键，即 `createplayerlogic.go:229-236` 说的幽灵映射。redis 模式下响应丢失时也会出现同类问题，但频率更低。
7. **库被重置后重新种的 `route_meta` 也是 `writable=1`。** 依赖预检的 `known ⇒ ExistingRowOnly`、发布时"Superseded 且 home 不同就失败"、导入的 rev_regression 三处兜底；回档的自动检测要到第 1b 批。
8. **新的启动依赖（m10）。** 新表的 DDL、形状守卫、meta 种行进入了 `MigrateSchema` 的关键路径，**redis 模式也受影响**：任一步失败，快照、流水、号段、名字四个 store 都会置 nil（`servicecontext.go:157-167`），本地会禁止建角；K8s 下因为 `Store.Required=true` 会直接拒启。所以"redis 模式与今天逐字相同"只适用于 W1 的行为。本机 MySQL 是 `mysql:latest`（`deploy/docker-compose.yml:396`），K8s 是 `mysql:8.0`（`deploy/k8s/manifests/infra/mysql.yaml:109`），两边都要跑一次迁移。
9. **WAITAOF 的延迟与 go-zero 的细节没有实测：** `PipelinedCtx` 是否就是 go-redis 的同连接 pipeline、ReadTimeout 和 MaxRetries 的取值、redis 熔断器怎么计数脚本的错误回复。由 h 节第 4 步的 CLIENT ID 夹心用例给出部分证据。本机 compose 的 mapping Redis 是 `redis:latest`（`docker-compose.yml:218`），WAITAOF 要求 ≥ 7.2。

---

## h) 交给 Codex 的验证

**顺序：**

1. 先按 `docs/handoff/placement-reverify-checklist-20261012.md` 复验，**以第 i 节第 10 条补进去的附录为准**：计数与用例名已经因第 0 批和 1a 改变；
2. 再跑第 0 批的验证（提案 `:433-437`）；
3. 最后跑下面的命令。

全部在仓库根目录、用 PowerShell 执行。

1. **shared**

   ```powershell
   cd go/shared
   gofmt -l ./placement           # 无输出
   go vet ./placement/...
   go test ./placement/... -count=1 -v -run "Route|Rev|Publish|MapRev|Keys"
   go test ./... -count=1
   ```

2. **data_service 单元测试**

   ```powershell
   cd go/data_service
   gofmt -l ./internal ./cmd ./data_service.go   # 无输出
   go vet ./...
   go build ./...
   go test ./... -count=1
   go test ./internal/routing/... ./internal/server/... ./internal/logic/... ./internal/config/... ./internal/store/... ./internal/routetool/... ./cmd/route_tool/... -count=1 -v -run "Route|Publish|DBFirst|Remap|DeletePlayerData|ShippedYaml|WaitAOF|NoPlayerRouteWrites|DecideImport|RegisterHome|ImportRoutes|NewRouteStore"
   go test ./internal/routing/... -list . | Measure-Object -Line   # 期望 54 个顶层用例 + 1 行 ok
   ```

   通过标准：全绿。既有失败按复验清单第 4 步归因，并单独列出。

3. **MySQL 集成。** 本机 compose 的 appuser 没有建库权限，用 root；口令取 `deploy/docker-compose.yml` 里的 `MYSQL_ROOT_PASSWORD`，不要写进日志。

   ```powershell
   $env:DATA_SERVICE_IT_MYSQL_HOST = '127.0.0.1:3306'
   $env:DATA_SERVICE_IT_MYSQL_USER = 'root'
   $env:DATA_SERVICE_IT_MYSQL_PASSWORD = '<见 compose>'
   go test -tags=integration ./internal/store/... -count=1 -v -run "Route|MigrateSchema"
   ```

   通过标准：没有 FAIL。出现 SKIP 要如实报告为"未验证"。保留 `TestRouteStore_ConcurrentManyPlayersSameZone` 打印的 lockRetries，以及 `TestRouteStore_LockWaitBoundedAfterClientCancel` 实测的耗时。

4. **Redis 集成。** 必须用一次性 Redis，不要依赖开发库恰好为空。本机 compose 的 Redis 就是服务在用的 mapping Redis，跑过服务就有键；用例在 `itKeyPatterns` 非空时会 Skip（`router_integration_test.go:50`、`:64-72`）。

   ```powershell
   docker run -d --rm --name ds-it-redis -p 127.0.0.1:6390:6379 redis:7.2 redis-server --appendonly yes   # 本地没有该镜像时要拉取,先征得同意
   $env:DATA_SERVICE_IT_REDIS_HOST = '127.0.0.1:6390'
   go test -tags=integration ./internal/routing/... -count=1 -v
   docker rm -f ds-it-redis
   ```

   通过标准：

   - `TestPublishRoute_RealRedisSameConnection` 和 `TestPublishRoute_RealRedisWaitAOF` 都必须 **PASS，不能 Skip**。前者如果 FAIL，按 Q3 切换到独立的 go-redis 客户端；
   - 第 3 步的 MySQL 环境变量还在时，`TestDBFirstRegister_RealStores` 也必须 PASS；
   - 顶层用例共 63 个。

5. **工具与全量集成**

   ```powershell
   go test -tags=integration ./internal/routetool/... -count=1 -v
   go test -tags=integration ./... -count=1
   ```

6. **PowerShell 契约测试**

   ```powershell
   pwsh -NoProfile -NonInteractive -File tools/scripts/tests/k8s_deploy_contract.tests.ps1
   ```

   通过标准：fail=0。

7. **清库脚本：语法检查和计划输出**

   ```powershell
   pwsh -NoProfile -Command "foreach(`$f in 'tools/scripts/stress_round19.ps1','tools/scripts/currency_crash_window.ps1'){`$e=`$null;[void][System.Management.Automation.Language.Parser]::ParseFile((Resolve-Path `$f),[ref]`$null,[ref]`$e);if(`$e){`$e;exit 1}}"
   pwsh -NoProfile -File tools/scripts/stress_round19.ps1 -Phase wipe   # 不带 -Execute,只打 plan
   ```

   通过标准：语法零错误；plan 输出里出现 `testdb` 的三张路由表和 `route_meta` 复位。

8. **可选冒烟（本机全栈）。** 开始之前，先确认这段时间不跑 dev_tools 的任何 merge-zone 命令（冻结窗口）。

   ```powershell
   cd go/data_service
   go run ./cmd/route_tool -f etc/data_service.yaml import-from-redis                  # dry-run,看统计
   go run ./cmd/route_tool -f etc/data_service.yaml import-from-redis -dry-run=false   # 真导入,期望退出码 0
   go run ./cmd/route_tool -f etc/data_service.yaml audit                              # 期望 0
   ```

   然后临时把本地 yaml 改成 `Mode: db_first`，重启 data_service，确认启动日志里有那条 WARN；用 robot 建一个角色，再跑 audit，应为 0。改完要还原 yaml。

   也可以先按 AGENTS.md §6.2 清库再做，这样 dry-run 之后的 audit 自然为 0。注意不要在 dry-run 之后直接期望 audit 为 0：Redis 里有历史玩家时，dry-run 不写库，audit 必然返回 1。

9. **不需要重编 C++。** 本批没有 proto 变更。

---

## i) 文档落地（完成后必做，属于验收项）与待定问题

### 文档清单

1. **提案 §11.2**（`:413-419`）：
   - 第 1 批拆成"1a 已落码（2026-10-12），未编译、未测试"和"1b 未开始"两行；
   - 仿第 0 批（`:421-437`）新增"第 1a 批改动"一段，内容包括：改动清单、g.2 的偏离表、h 节的验证；
   - **P2 切换门禁**：冻结窗口（第 2 批前禁止 merge_zone 一切写模式）、P1 导入且 audit 为 0、Redis 前提、建角压测；
   - "db_first 下库内围栏在本批恒不生效，提交点复核靠发布脚本"；
   - 孤儿行与 `db_only_unpublished` 在本批无法自动消除，P3 的"audit 为 0"要等 1b；
   - "已知未改"：guild、login、match、trade、merge_zone 里提到 `RemapHomeZoneForMerge` 的注释。
2. **`docs/PROGRESS.md`：** 只追加，不改最后一条（union 合并，AGENTS.md §5）。标题："2026-10-12 补充：映射持久化第 1a 批落码（Claude，未编译、未测试）"。正文写明 Java 版本号"无"、Java 状态"待做"（PARITY 推迟到第 1 批完成时登记），以及 EVAL 偏离任务原文这一点。
3. **`docs/README.md:142`：** 状态改为"已拍板，分批落码（第 0、1a 批已落码，未验证）"。
4. **`docs/design/player-storage-placement.md`**（提案 §9 的要求，只做局部修改）：
   - §4.4（`:107-116`）末尾追加一段：映射持久化（D1）已拍板，库为真源，分批落码；第 1a 批只落了存储底座和 W1 的 db_first 开关，默认仍是 redis；链接到提案；
   - §11.4（`:377`）："（提案，待拍板）"改为"已拍板，分批落码中（第 1a 批：存储底座 + W1 开关，默认 redis）"。
5. **`docs/ops/merge-zone-runbook.md`：**
   - `:134`（RegisterPlayerZone 那一行）补一句："`RouteStore.Mode=db_first` 时，提交点复核由发布脚本的 `merge:in_progress:{home}`（KEYS[4]）完成，命中时三键零写入，但库里已提交的行会留下"；
   - `:136`（remap 那一行）改为："已停用（2026-10-12，提案 §4.3）：一律 FailedPrecondition + `ErrCodeNotImplemented`；改映射只走 tools/merge_zone"；
   - §9 新增"§9.3 映射落库：route_tool 导入与审计"，内容包括：P1 的操作步骤、两个子命令的退出码、"只能检测合服围栏"、`db_only_unpublished` 的含义、P2 切换门禁；
   - §12 追加一条："任一 data_service 副本为 db_first 起，到第 2 批落地为止，禁止 merge_zone 任何写模式"；
   - 修订历史追加一行。
6. **`docs/design/server_merge_design.md`：** `:13` 的修订提示和 §3.3（`:173-175`）各补一句"`RemapHomeZoneForMerge` 已停用（2026-10-12，提案 §4.3）"。
7. **`docs/design/enter-scene-zone-routing.md`：** `:122`、`:133` 删掉"或 RemapHomeZoneForMerge"，并注明已停用。
8. **`docs/design/global-data-layer-tidb-decision.md`**（工作区有别人的改动，只追加）：
   - D7（`:181`）注明"`RemapHomeZoneForMerge` 已停用，映射改写走 tools/merge_zone，第 2 批起库先写"；
   - §4 风险清单追加一行："`zone_merge_fence` 单 Region 锁热点、`player_route` 主键与 `idx_route_updated`、`idx_route_home` 的尾部热点"，等级 P1；缓解措施是 SPLIT 加压测；
   - §6 实施清单追加一条："`SPLIT TABLE zone_merge_fence` 按 zone 预切，`player_route` 的热点纳入压测"。
9. **`docs/design/ARCH.md:503`：** 不改这一行历史决策的正文，只在行尾追加"（注：2026-10-12 起 `RemapHomeZoneForMerge` 停用，见 player-route-mapping-durability-proposal.md §4.3）"。
10. **`docs/handoff/placement-reverify-checklist-20261012.md`：** 在第 4 步、4b 之后追加"2026-10-12 第 0 批 / 1a 批后的期望"附录，不改原有段落：
    - routing 单测从 26 变为 54（HEAD 26，第 0 批 +3，删 remap −3，`route_publish_test.go` +28）；
    - 带 integration tag 时顶层用例共 63 个；
    - `TestMergeFence_RealRedisBlocksRegisterAndGatesRemap` 改名为 `TestMergeFence_RealRedisBlocksRegister`；
    - 4b 只配 Redis 时，允许 `TestDBFirstRegister_RealStores` 一个 SKIP；
    - `-run "TestRegisterPlayerZone_"` 仍然恰好命中 routing 的 10 个；
    - `TestRemapHomeZoneForMerge_` 现在匹配的是"恒停用"的新用例，预期 PASS。

### 待定问题（附推荐答案）

| # | 问题 | 推荐 |
|---|---|---|
| Q1 | route_tool 在 K8s 上怎么跑 | 本批按 cmd 实现。第 1b 批在 `data_service.go` 加 `-route-tool <sub>`，复用 `internal/routetool`，与 `-migrate` Job 同形 |
| Q2 | EVAL 还是 EVALSHA | EVAL（g.2 第 2 条），在交付说明里向负责人点明 |
| Q3 | go-zero `PipelinedCtx` 还是独立的 go-redis 客户端 | **go-zero。** 已核实：API 存在，`player_name_cache.go:141`、`:177` 已在用并能编译。未核实：内部委托方式、ReadTimeout、MaxRetries、熔断器。由 h 节第 4 步的 CLIENT ID 夹心用例来定；FAIL 就改用 `goredis.NewClient{Addr: MappingRedis.Host, Username: User, Password: Pass, DB: 0, ReadTimeout: WaitAOFTimeout+1s}` 专门做发布 |
| Q4 | miniredis 是否支持 WAITAOF | **未核实。** 单测一律显式配 0/0；真实的 WAITAOF 只在真 Redis 集成测试里验 |
| Q5 | db_first 是否保留 Redis 预检 | 保留到第 3 批。第 2 批之后，预检里的围栏那一项可以去掉，但发布脚本的 KEYS[4] 要等 merge_zone 改成库先写、库内围栏真正生效之后再评估 |
| Q6 | `DeletePlayerZone` / `DeletePlayerZoneIfLocked` 删不删 | 保留，加注释"生产路径已停用"。等删号设计定下来再处理 |
| Q7 | 清库时 route_meta 怎么处理 | 原地复位：`UPDATE route_meta SET writable=1, watermark_ms=0, cache_verified_ms=0, leader=NULL, lease_until_ms=NULL WHERE id=1`。三张数据表 TRUNCATE。先查 `information_schema` 只处理存在的表；新增参数 `$GlobalDbName = 'testdb'`（`data_service.yaml:96`）。**`$LASTEXITCODE` 非 0 时抛错。这比两个脚本对 zone 表的处理更严格：它们只在找不到客户端时才 throw，SQL 失败被 `2>&1 \| Out-Null` 吞掉（`stress_round19.ps1:122-130`、`currency_crash_window.ps1:133-139`）。** 注释里要写明为什么更严格：残留的路由表行会被 db_first 当成"已存在"，造成 RowMissing 或冲突 |
| Q8 | AGENTS.md §6.2 是否加一行"全局库路由表一并清" | 推荐加一行（提案 §8.2）。**但 AGENTS.md 工作区已有别人没提交的改动，先和负责人确认再动** |
| Q9 | 给 RegisterPlayerZone 加 MethodTimeouts | 本批不加（d 节）。P2 门禁压测之后再定；要加就同步登记到 `Get-GrpcClientMethodsNotCalledFromCpp`（`data_service.yaml:36-39`），并注意 K8s 只镜像标量 Timeout |
| Q10 | 新码 30 | 采用 |
| Q11 | 导入遇到"库非 NULL、与 Redis 不同"怎么办 | 报告并跳过，退出码 1，不覆盖；rev_regression 同样处理 |
| Q12 | 导入时要不要顺带写 maprev 或发布 | 不要，留给第 1b 批的全量预热。B2 的遗留值守卫保证不写 maprev 也安全 |
| Q13 | W1 要不要拒绝注册进已被合走的 zone（检查 `zone_merged_into`） | 本批不做，提案没有要求；记为后续加固项 |
| Q14 | Java 的 PARITY.md 什么时候登记 | 第 1 批（1a + 1b）全部完成时登记一行；本批只在 PROGRESS 里写"Java 待做"，并在交付说明里写明推迟的原因 |
| Q15 | 锁冲突重试次数 | 1213 和 9007：总共 4 次（1 次 + 3 次重试），最多多睡 150ms；1205 不重试 |
| Q16 | P2 门禁的阈值（发布未确认率、W1 p99）怎么定 | 由负责人在看到 Codex 的压测对比表之后拍板，本规格不预设数字 |

---

## 评审处理记录

### 评审 1

| 条目 | 处理 | 说明 |
|---|---|---|
| **B1** 提交点围栏复核丢失 | **采纳** | 发布脚本加可选的 KEYS[4]，围栏存在就回 `{3}`、零写入（b.1）；router 映射成 `ErrZoneMergeInProgress`（c.1 第 4 步）；c.3 拆成三行如实写；新增用例 15；runbook `:134` 补说明；孤儿行写进 g.3 第 6 条 |
| **B2** maprev 缺席时覆盖遗留值 | **采纳** | 脚本在 cur 不存在时检查遗留值，不同就回 `{2}`、零写入；Go 侧判为 `PublishLegacyConflict`，最后是 Unavailable 加 ERROR 日志；补了测试向量（用例 7、22）；g.3 第 5 条随之改写 |
| **M1** 没设锁等待上限 | **采纳** | DSN 加 `innodb_lock_wait_timeout=1`，探针读回校验；1205 不重试（guild 先例 `guild_manage_repo.go:101-103`）；c.2 写明预算分配；新增真库用例 `LockWaitBoundedAfterClientCancel` |
| **M2** WAITAOF 预算与 everysec 撞边界 | **部分采纳** | 采纳：默认改为 1500ms；发布失败打带 player_id 和 rev 的 ERROR（只进日志）；audit 新增 `db_only_unpublished`；"发布未确认率"列为 P2 门禁。**不采纳 MethodTimeouts**：K8s 不镜像那张表（`data_service.yaml:35`），而本批所有档位都是 redis 模式 |
| **M3** 导入把库回档洗白 | **采纳** | 导入读 maprev，新行写 `rev = maprev`；`rev < maprev` 记 rev_regression，退出码 1；audit 新增 `redis_rev_ahead`；运行时另加一道"Superseded 且 home 不同就失败" |
| **M4** db_first 与未改的 merge_zone 写入点不能共存 | **采纳** | 在"开工前六件事"、yaml、ConfigMap、启动 WARN、runbook §12、提案 §11.2 写明约束；导入输出 NOTE；冒烟步骤先确认没有合服命令在跑 |
| **M5** `zone_merge_fence` 单 Region 热点 | **部分采纳** | 采纳：登记进 g.3 和 TiDB 决策文档 §4、§6；DDL 改为 CLUSTERED，为按 zone SPLIT 留出可能。**不采纳"推迟建 `idx_route_updated`"**：bootstrap 只执行 `CREATE TABLE IF NOT EXISTS`，以后补索引要另写收敛代码（同类见 `ensurePlayerNameKeyedByName`）；MySQL 上没有热点；TiDB 尚未部署，Phase 1 也没拍板；这个热点已登记进 TiDB 清单 |
| **M6** 文档清单不完整 | **采纳** | (a) runbook `:134`；(b) runbook 新增 §9.3；(c) player-storage-placement 的 §4.4 和 §11.4；(d) yaml、ConfigMap、data_service.go、config.go 的表数注释；(e) `data_service.go:190-193` 补上码 30 |
| m1 参数绑定 | **采纳** | 用 `nullU32`、`nullU64` 绑定；R6 拆成 R6a、R6b；断言 `(storage==nil)==(version==nil)`；补真库用例和契约用例。另外核实出 `sql.Null[uint32]` 不能直接当参数 |
| m2 "码集合不变"自相矛盾 | **采纳（换了做法）** | 参数非法改回 Unavailable 加码 14，码集合真正不变；登记进 g.2 第 14 条 |
| m3 Q7 引用失实 | **采纳** | Q7 改正，并写明新清库更严格的理由 |
| m4 确定性错误不该重试 | **采纳** | Redis 错误回复一律不重试（c.1 第 4 步，用例 24） |
| m5 WAITAOF timeout 截断成 0 | **采纳** | 截断成整毫秒之后再判断 `< 1`（`waitAOFBudgetMs`，用例 27） |
| m6 键名和值的规范形式 | **采纳** | e 节规范形式表；run_id 复用 `placement.ValidRunID` |
| m7 B 遍会漏人 | **采纳** | 按 `seenA` 集合跳过 |
| m8 OOM 半写的理由不成立 | **采纳** | 脚本注释改为"只为可读，不存在 OOM 半写" |
| m9 grep 守卫误伤注释 | **采纳** | 改用 `go/parser`，只检查字符串字面量 |
| m10 新 DDL 失败会拖垮 redis 模式 | **采纳** | 写进 g.3 第 8 条；h 节在本机和 K8s 两种 MySQL 版本上都跑迁移 |
| m11 9007 | **采纳** | 重试集合为 1213 和 9007（1205 另行处理） |
| m12 locale | **采纳** | Lua 改成逐字节比较 |

### 评审 2

| 条目 | 处理 | 说明 |
|---|---|---|
| **blocking** 提交点不复核围栏，前提没写 | **采纳** | 与评审 1 B1 合并处理（发布脚本 KEYS[4]）；c.3 如实改写；冻结窗口前提写进 yaml、ConfigMap、runbook `:134`、提案 §11.2、启动 WARN；围栏预检单独用 EXISTS（复用 `IsMergeInProgress`），不并进 MGET |
| **major** 复验清单冲突 | **采纳** | 新单测统一用 `TestDBFirstRegister_*`、`TestPublishRoute*` 前缀；集成用例改名，并在清单里追加附录写明新的计数和用例名（第 i 节第 10 条） |
| **major** 缺默认运行的契约单测 | **采纳** | 新增 `route_store_contract_test.go`，假驱动支持事务，覆盖评审列出的全部分支和探针 |
| **major** 1205 重试触发不了、取消后占锁 | **采纳** | 与评审 1 M1 合并处理 |
| **major** InvalidArgument 与"码集合不变"矛盾 | **采纳** | 改为 Unavailable 加码 14，同步改 `dataserviceserver.go:83-94` 的注释，登记进 g.2 第 14 条 |
| 冒烟期望写错 | **采纳** | h 节第 8 步先真导入，或先清库 |
| Redis 集成环境 | **采纳** | h 节第 4 步改用一次性 `redis:7.2 --appendonly yes` |
| Q7 引用失实 | **采纳** | 同评审 1 m3 |
| 发布重试分类 / go-redis MaxRetries 叠加 | **采纳** | c.1 写明分类规则和叠加说明 |
| route_tool 的 MustNewRedis 会 panic | **采纳** | 改用 `redis.NewRedis`，编不过就 recover；补 main_test 用例 |
| server 测试辅助的配置 | **采纳** | `newDBFirstTestServer` 显式写 Mode 和 0/0 |
| svc 字段类型 | **采纳** | 改为 svc 接口 `PlayerRouteStore`（`routing.RouteStore` + `Close`） |
| "五张表"注释 | **采纳** | a 节逐处列出 |
| RemapHomeZoneForMerge 文档没跟着改 | **采纳** | 第 i 节第 6–9 条；ARCH.md 只在行尾加注，不改历史正文 |
| 孤儿行 | **采纳** | `db_only_unpublished` 加 g.3 第 6 条 |
| redis 模式受新表影响 | **采纳** | g.3 第 8 条 |
| `idx_route_updated` 推迟 | **不采纳** | 理由见评审 1 M5；热点已登记 |
| 缓存缺席时顺手修补 | **不采纳** | 修补缓存是第 1b 批对账的职责；在冲突路径上多做一次写会扩大本批的语义面（YAGNI）；对应场景（同一 id 的库有孤儿行、又被异 home 注册）在 id 每次新发的前提下几乎不可达 |
| EVAL 与任务原文不一致 | **维持 EVAL，点明** | 写进"开工前六件事"第 6 条、g.2 第 2 条、PROGRESS 和交付说明 |
| 守卫正则误伤注释 | **采纳** | 同评审 1 m9 |
| RouteRow 与 proto | **采纳** | 写进 g.2 第 1 条的代价 |
| Java 对账本 | **采纳** | g.1 §12 和 Q14 写明状态与推迟原因 |

### 两份评审都确认无误、本规格保留的结论

以下各项沿用原规格，两份评审的 verified_ok 部分已逐条核对：

- W2/W3 仓库内没有生产调用方；
- `Do("WAITAOF")` 加 `Slice()` 的必要性；
- pipeline 单次尝试只占一条连接，以及 touch 的作用；
- RC 的 DSN 写法，不带 CLIENT_FOUND_ROWS；
- 锁顺序没有环；
- W1 在其余格子上与 redis 模式等价；
- 第 1a 批"提交即安全"成立；
- Redis 侧的 noeviction 和 appendonly 前提；
- 字段类型符合 AGENTS.md §4；
- 配置用指针字段、拒绝而不纠正；
- `bootstrapTables` 加表的路径；
- 引用的辅助函数与行号；
- 错误码 30 空闲；
- `NewRouter` 改成可变参数不破坏现有调用方；
- ConfigMap 插入点不破坏 `:175` 的断言；
- 清库脚本今天不碰全局库；
- merged_into 的值格式；
- 本批不需要 proto 重生成，也不需要重编 C++。
