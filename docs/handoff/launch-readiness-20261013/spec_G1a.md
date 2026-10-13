# G1a 实现规格:D1 路由表 TiDB 热点修复(第 1a 批 DDL 定稿前的最后一次改形)

> 状态:**规格稿,只读代码与文档得出,未编译、未测试**(AGENTS.md §10.1)。行号以工作区当前内容为准(第 0、1a 批未提交改动在内);HEAD = `73b852f32`。
> 范围:只改映射持久化第 1a 批新建的路由表与其读写代码、route_tool、两份清库脚本和相关文档。**不改 proto、不重编 C++、不改 AGENTS.md、本阶段不写 docs/PROGRESS.md**(最后由 Docs 阶段统一追加)。
> 为什么现在做:四张表还没部署,第 1a 批代码未编译(提案 `player-route-mapping-durability-proposal.md:417-418`);TiDB 建表后不能改聚簇属性(`go/data_service/internal/store/schema.go:198-199`)。现在改 DDL 只是改常量,上线后再改等于整表重写 + 双写迁移。

---

## 0. 一句话

`player_route` 的主键从 `player_id NONCLUSTERED + SHARD_ROW_ID_BITS` 改成 **聚簇 `(route_shard, player_id)`,`route_shard = player_id % 16`**(应用计算 + CHECK 钉住 + 建 store 时实测 CHECK 生效),三条二级索引都带 `route_shard`;`route_meta` 拆成「很少写的开关表 `route_meta`」与「经常写的对账表 `route_reconcile`」;新增 `route_tool tidb-prepare` 在 TiDB 上按片 / 按 zone 预切并设 `merge_option=deny`,MySQL 上自动跳过。

---

## 1. 现状与问题(事实)

| # | 事实 | 出处 |
|---|---|---|
| F1 | `player_route` 主键 `PRIMARY KEY (player_id) /*T![clustered_index] NONCLUSTERED */`,加 `SHARD_ROW_ID_BITS=4 PRE_SPLIT_REGIONS=4`;三条二级索引 `idx_route_home(home_zone, player_id)`、`idx_route_storage(storage_id, player_id)`、`idx_route_updated(updated_at_ms)` | `schema.go:172-192` |
| F2 | NONCLUSTERED + SHARD 只打散行数据;主键退化成的唯一索引与时间索引都是单调尾部;`PRE_SPLIT_REGIONS` 只切行数据,每条索引建表时只有 1 个 Region | `tidb-phase1-and-launch-readiness.md:63`、`:76`;提案 `:628-631`(1a 风险第 1 条) |
| F3 | 号段发的 `player_id` 在每个 login 段内严格递增;首日数据量远低于 Region 按大小分裂的阈值,所以新插入始终打同一个尾部 Region;Load Base Split 只看读 | `tidb-phase1-and-launch-readiness.md:74-75`;launch_verdict `:6` |
| F4 | 4 条索引全部单调:PK 1 条、`idx_route_home` 每 zone 1 条、`idx_route_storage` 1 条(新号 storage 同为 NULL 或同为 1000000)、`idx_route_updated` 1 条 | `tidb-phase1-and-launch-readiness.md:683`、`:688` |
| F5 | `zone_merge_fence` 主键已是 `(zone_id, shard) CLUSTERED`,但整表只有「zone 数 × N」行,必然落在一个 Region;W1 每次 `FOR UPDATE` 一个分片行 | `schema.go:194-212`;`route_store.go:129-130`;决策文档 `global-data-layer-tidb-decision.md:210`、`:230` |
| F6 | `route_meta` 单行同时放「每次 W1 都读的 writable」和「1b leader 每 1s 写的 watermark、每 3s 续的租约」 | `schema.go:224-235`;`route_store.go:125`;提案 `:256-275`(§5.4) |
| F7 | W1 事务形状:R1 普通读 `route_meta.writable` → R2 `FOR UPDATE` 围栏分片 → R4 ODKU 插入 / R5 `FOR UPDATE` 玩家行 → R6 CAS | `route_store.go:633-816` |
| F8 | 全部按 `player_id` 的读写:R4 `:138-141`、R5 `:144`、R6a/b `:147-153`、R7 `:156-159`、R8/R13 `:162`+`:200-214`、R9 `:165-167`、R12(键集翻页)`:176`、`ScanRoutes` `:1367-1389`、`loadRoutes` `:1410-1433` | `route_store.go` |
| F9 | `ScanRoutes(ctx, afterPlayerID uint64, limit)` 是 routetool `routeDB` 接口的一员,audit 方向 A 用 `after = rows[len(rows)-1].PlayerID` 翻页 | `routetool/importer.go:63-69`;`routetool/auditor.go:327-356` |
| F10 | 形状守卫只查列名 + 主键列序,不查类型、不查聚簇属性 | `schema.go:253-312` |
| F11 | TiDB 上 CHECK 默认不生效(`tidb_enable_check_constraint` 默认 OFF,且要在建表前打开);MySQL ≥ 8.0.16 才执行 CHECK | `tidb-phase1-and-launch-readiness.md:196`;`route_store_integration_test.go:165-173` |
| F12 | 聚簇表不能再用 `SHARD_ROW_ID_BITS`(NONCLUSTERED 才「要配 SHARD_ROW_ID_BITS」;改聚簇就要「放弃与之冲突的 SHARD_ROW_ID_BITS」) | `schema.go:121-122`、`:529-530`;`tidb-phase1-and-launch-readiness.md:579`(该处引的 `schema.go:352-366` 在工作区已对不上,以前两处为准) |
| F13 | 只写 CHARSET 的 D1 三张表在 MySQL(`utf8mb4_0900_ai_ci`)与 TiDB(`utf8mb4_bin`)上排序规则一定不同;`CHARACTER SET ascii` 不带 COLLATE 的列同理 | `tidb-phase1-and-launch-readiness.md:640-648` |
| F14 | 路由表在 TiDB 上 TRUNCATE 会换表 ID,预切与 table attributes 不跟过去;BR / TiCDC 跳过 `ATTRIBUTES` DDL | `tidb-phase1-and-launch-readiness.md:351`、`:484-487`、`:699` |
| F15 | 两份清库脚本把 `route_meta` 原地复位为 `UPDATE route_meta SET writable=1, watermark_ms=0, cache_verified_ms=0, leader=NULL, lease_until_ms=NULL WHERE id=1;` | `tools/scripts/stress_round19.ps1:111-112`、`:148-155`、`:174`;`tools/scripts/currency_crash_window.ps1:108-109`、`:149-156`;runbook `docs/ops/merge-zone-runbook.md:783-788` |
| F16 | route_tool 只有 `import-from-redis` / `audit` 两个子命令,任何子命令都先连 mapping Redis 再开 RouteStore | `cmd/route_tool/main.go:39-53`、`:113-124` |
| F17 | data_service 的 K8s ConfigMap 里 `Regions: Zones: [1]` 是写死的,不是真实 zone 清单 | `tools/scripts/k8s_deploy.ps1:3484-3486` |
| F18 | 存量 snowflake `player_id`(login 的 bwmarrin 布局:13-bit node / 9-bit step,step 在最低位) | AGENTS.md §7.1;`go/shared/idsegment/client.go:36-51`(号段与存量 snowflake 分界) |

---

## 2. 设计决定

### D-1 `route_shard`:应用计算、CHECK 钉住、建 store 时实测

- `route_shard tinyint unsigned NOT NULL`,值 = `player_id % 16`,由 Go 侧唯一函数 `placement.RouteRowShard` 算出并绑定;DDL 加 `CONSTRAINT chk_route_shard CHECK (route_shard = player_id % 16)`。
- **为什么不用生成列**:TiDB 不支持用 ALTER 加 STORED 生成列、生成列能否进聚簇主键 / Dumpling / Lightning 怎么处理都未核实(`tidb-phase1-and-launch-readiness.md:523-527`);任务也要求「应用计算」。
- **为什么不用哈希混洗**(如 `(id ^ id>>9) & 15`):两种引擎的位运算 / CRC32 等价性都要额外核实,收益只在存量 snowflake 号(见风险 R3)。取最简单、两边 SQL 必然一致的 `% 16`。
- **为什么是 16**:与仓库其它表 `SHARD_ROW_ID_BITS=4`(16 份)同口径(`schema.go:71`、`:154`);每条单调索引从 1 条尾部变 16 条(`idx_route_home` 每 zone 16 条),足够开服尖峰分摊到 16 个 Region leader;再大只增加空 Region 数与按 zone 翻页的段数。
- **一旦有任何环境建过 `player_route` 就不可改**:`route_shard` 是聚簇主键首列,改模数 = 每一行换主键 = 整表重写。常量与测试向量都写死这一点。
- **`player_id` 唯一性现在依赖 CHECK**:主键 `(route_shard, player_id)` 只在 `route_shard` 由 `player_id` 唯一决定时才等价于「`player_id` 唯一」。CHECK 没生效(TiDB 默认 OFF、MySQL < 8.0.16)时,一个绕过 store 或算错片的写者就能给同一个 `player_id` 写出两行,R5 按正确片点查看不到另一行,**静默破坏「每玩家一行」**。所以:
  - 建 store 的探针新增第 4 步 **P4**:在一个必然回滚的事务里插一行「只违反 `chk_route_shard`」的行,必须收到 3819 且报错里点名 `chk_route_shard`,否则拒建 store(fail-closed,AGENTS §11.3)。db_first 的 data_service、debugutil、route_tool 都经过它。
  - **错片行只有全表键集扫描(R12 = `ScanRoutes`)看得见**:R5 / R8 / R13 都按「正确的片 + id」点查,库里一条 `(3, 6)` 这样的错片行对它们**不可见**,`GetRoutes([6])` 会回「缺席」、导入会把 6 当新行再插一条 `(6, 6)` —— 这正是 P4 必须在建池时 fail-closed 的原因,读路径兜不住。所以:R12 扫描时校验 `route_shard == player_id % 16`,不符即 `ErrRouteShardMismatch`(整页作废,audit 方向 A / 1b 全量预热就是检测器);R8 / R13 的扫描也做同样校验,但只是纵深防御(真库上只有 SQL 文本被改错才可能触发),不能写成「点读不会把错片行当缺席」。(评审 M1)
  - 不另加 `UNIQUE(player_id)`:那正是要消掉的单调尾部。
  - P4 只证明「CHECK 生效」,**不证明模数是 16**:模数由 DDL 契约测试钉住,且表只由本迁移建;若有人用别的模数手工建表,W1 会对一部分玩家撞 3819(fail-closed,不写坏数据)。

### D-2 `player_route` 主键与二级索引

| 索引 | 新形状 | 写入尾部(新号) | 读方 |
|---|---|---|---|
| PRIMARY | `(route_shard, player_id)` **CLUSTERED** | 16 条(每片一条) | 1a:W1 R5/R6、导入 R8/R9、`GetRoutes`、audit 方向 A;1b:全量预热;第 3 批:读路径回源 |
| `idx_route_home` | `(home_zone, route_shard, player_id)` | 每 zone 16 条 | 第 2 批 W10 pin-placement;第 3 批 R7 `GetAllPlayerIDsInZone` / `collectPlayerIDsWithHomeZone` / `data_consistency_check`(提案 `:208`、`:244`) |
| `idx_route_storage` | `(storage_id, route_shard, player_id)` | 每个 storage 值 16 条 | 第 2/3 批 `collectStoragePlayers`、relocate 清单 |
| `idx_route_updated` | `(route_shard, updated_at_ms, player_id)` | 16 条 | 1b 对账增量推送(提案 `:263`、`:275`) |

- 三条二级索引都显式写出尾部 `player_id`:InnoDB 本来就会追加主键列、TiDB 非唯一索引也会带上 handle,显式写出是为了让键集翻页的游标 `(route_shard, player_id)` / `(updated_at_ms, player_id)` 在两种引擎上**语义一致、文档可读**,不增加存储。
- `idx_route_updated` 保留(回应 launch_verdict `:24`「评估去掉」与 L-12 `tidb-phase1-and-launch-readiness.md:823`):1b 要它;bootstrap 只执行 `CREATE TABLE IF NOT EXISTS`,以后补索引要另写收敛代码(提案 `:631`)。加了片前缀之后它也不再是单尾。
- 去掉 `/*T! SHARD_ROW_ID_BITS=4 PRE_SPLIT_REGIONS=4 */`(聚簇表不适用,F12);TiDB 上的预切改由 `tidb-prepare` 做(D-5)。
- **只建片前缀、不预切,在 TiDB 上等于没做**:空表只有 1 个 Region,首日数据量到不了按大小分裂的阈值(F3),16 条尾部仍在同一个 Region。所以 D-5 的预切对 `player_route` 同样是必做项(任务原文只点了 `zone_merge_fence`,这里补上)。
- MySQL 上:InnoDB 主键本来就聚簇,`/*T![clustered_index] CLUSTERED */` 被当注释忽略;插入点从 1 个变 16 个,每个点仍是顺序追加,页分裂启发式按页判断,填充率不受影响(推断);性能无可见变化,正确性同前。

### D-3 `route_meta` 拆表

| 表 | 列 | 写频率 | 第 1a 批 |
|---|---|---|---|
| `route_meta`(开关表,沿用原名) | `id`、`generation`、`writable` | 只有回档恢复 / 冻结时写(1b) | **唯一读的那张**:R1 每次 W1 普通读 `writable`;迁移 R15 种 id=1 |
| `route_reconcile`(新) | `id`、`leader`、`lease_until_ms`、`watermark_ms`、`cache_verified_ms` | 1b leader 每 1s 写水位、每 3s 续租 | **不读不写**;迁移 R16 种 id=1(ODKU no-op),形状守卫钉列 |

**为什么必须先拆**(都是 TiDB 才有的问题;MySQL 上 RC 一致读不被行锁挡,拆不拆都行):
1. **读被写锁挡**:TiDB 的读遇到 start_ts 更早、尚未提交的 prewrite 锁要等提交或解锁(Percolator 语义,推断)。leader 每秒写同一行,W1 的 R1 会间歇性地被它拖住 —— 每次建角都读这一行,抖动直接进建角 p99。
2. **缓存表要求「很少写」**:TiDB 的缓存表(`ALTER TABLE … CACHE`)让读走 tidb-server 内存,但**每次写都要等读租约到期**(`tidb_table_cache_lease`,默认 3s),且写会让缓存失效【未核实,本会话无法联网核对官方文档】。leader 每 1s 的水位写若还在这一张表上:写本身最长阻塞约 3s、跟不上 1s 节拍,水位落后会被 1b 的回档检测误判(提案 `:274`);缓存也几乎一直是失效的,开了等于没开。拆开后,开关表一年写不了几次,才具备设为缓存表的前提。
3. **第 1a 批只读开关表**,所以拆表对本批代码的影响只有迁移种行与清库脚本。

**开关表设为缓存表(TiDB-only,可选,默认不做)**:
```sql
ALTER TABLE route_meta CACHE;    -- 撤销:ALTER TABLE route_meta NOCACHE;
```
【未核实】项:表上限 64MB;缓存表上不能直接做 DDL(要先 NOCACHE);事务内读是否走缓存;BR / TiCDC / Lightning 是否保留 CACHE 属性(按「不保留」处理,恢复后重放);写入阻塞时长的确切口径。
对 1b 的约束(写进提案 G1a 段):冻结 `UPDATE route_meta SET writable=0` 在缓存表上最长阻塞一个租约,1b 的冻结步骤超时要 ≥ 租约 + 余量;在途事务仍按 1a 风险第 3 条(提案 `:633`)处理。
是否打开由负责人看 TiDB 压测里 `route_meta` 所在 Region 的读热度再定(见 §11 待拍板 #2)—— 按每次建角一次点读估算,单 Region 远未到上限(推断),所以默认不开。

### D-4 `zone_merge_fence`

- 主键保持 `(zone_id, shard) CLUSTERED`(第 1a 批偏离第 16 条,提案 `:590`)。
- TiDB 上按 zone 预切:`SPLIT TABLE zone_merge_fence BY (z1,0),(z2,0),…`,并 `ALTER TABLE zone_merge_fence ATTRIBUTES 'merge_option=deny'`,由 D-5 的子命令执行。每个 zone 的 N 个分片行落在自己的 Region,开服尖峰只压当 zone 那一个 leader。
- 不做 zone 内按分片预切:任务只要求按 zone;压测若显示单 zone 围栏 Region 仍热,把语句生成改成 `(z, s)` 即可(表已聚簇,不需要改 DDL)。记入风险 R6。

### D-5 TiDB-only 预切:新增 `route_tool tidb-prepare`

选「route_tool 子命令」而不是「迁移里自动做」:
- 按 zone 预切需要真实 zone 清单,而 data_service 的 yaml 里没有(K8s ConfigMap 写死 `Zones: [1]`,F17),迁移拿不到;
- 开新 zone、BR / PITR 恢复之后、TRUNCATE 之后都要重放(F14),一个显式、幂等、可 dry-run 的运维命令最容易写进 runbook;
- MySQL 上整步跳过(`serverIsTiDB`,`schema.go:1351-1357`),退出码 0。

子命令做的事(按顺序):
0. 判引擎:MySQL → 打印 `skipped`,退出 0;
1. **前置核对**:`information_schema.TABLES.TIDB_PK_TYPE` 必须对 `player_route`、`zone_merge_fence` 都是 `CLUSTERED`,否则 `ErrRouteTiDBNotClustered`(退出 2,提示「趁表空 DROP 后用 `data_service -migrate` 重建」)—— 非聚簇的表按 `(route_shard, player_id)` 切毫无意义,而且建表后改不了;
2. `ALTER TABLE player_route ATTRIBUTES 'merge_option=deny'`、`ALTER TABLE zone_merge_fence ATTRIBUTES 'merge_option=deny'`(先设防合并,再切,免得刚切出的空 Region 被 PD 合回去);
3. `SPLIT TABLE player_route BY (1,0),(2,0),…,(15,0)`(行数据 16 段);
4. `SPLIT TABLE player_route INDEX idx_route_updated BY (1,0,0),…,(15,0,0)`;
5. 每个 zone 一条:`SPLIT TABLE player_route INDEX idx_route_home BY (z,0,0),(z,1,0),…,(z,15,0)`;
6. 每个 `-storages` 值一条:`SPLIT TABLE player_route INDEX idx_route_storage BY (sid,0,0),…,(sid,15,0)`(Phase 2 `PinOnCreate` 指向 1000000 时传 `-storages 1000000`);
7. `SPLIT TABLE zone_merge_fence BY (z1,0),(z2,0),…`。

不做:`idx_route_storage` 的 NULL 前缀段(`PinOnCreate=false` 时新号 storage 为 NULL)—— `SPLIT … BY (NULL, …)` 的写法【未核实】;这一段的尾部要等 Phase 2 打开 `PinOnCreate` 后才有意义,届时按 6 切 1000000 段。记入风险 R5。

【未核实】:组合聚簇主键 / 多列索引的 `SPLIT … BY` 元组写法;对已存在边界再次 SPLIT 是否 no-op(要求幂等);`ATTRIBUTES` 是否覆盖索引 Region;SPLIT / ATTRIBUTES 需要的权限(应用账号不够时用运维账号的 yaml 副本跑,副本不进 git);SPLIT 结果集列(`TOTAL_SPLIT_REGION`, `SCATTER_FINISH_RATIO`);`TIDB_PK_TYPE` 列名。

### D-6 顺带项:排序规则显式化(建表前改最便宜)

按 `tidb-phase1-and-launch-readiness.md:646-648` 的处理建议、D1 自定取值:四张小表补表级 `COLLATE=utf8mb4_bin`(与 `player_route` 一致);所有 `CHARACTER SET ascii` 列补 `COLLATE ascii_bin`(MySQL 默认 `ascii_general_ci`、TiDB 只有 `ascii_bin`,F13)。这些列都是标识符或排障 JSON,逐字节比较就是正确语义。可拆出单独提交(§11 待拍板 #4)。

另有一处**不只是补排序规则、而是改字符集**:原 `route_meta.leader varchar(96) NULL`(utf8mb4)搬进 `route_reconcile` 后写成 `CHARACTER SET ascii COLLATE ascii_bin`,与 `updated_by` 同口径。理由:leader 标识与写者标识同源(`NewRouteWriterID` 只产 `[A-Za-z0-9._@:-]`)。对第 1b 批的约束:leader 标识必须是同一形态的 ASCII,非 ASCII 字节在严格模式下会被库拒绝。(评审 m-charset)

---

## 3. 文件清单

新增 2 个、修改 19 个(代码 / 脚本),文档 5 个;合计 26 个,在 D1 已获授权范围内(提案 `:404`),也低于 30。

### 3.1 代码与脚本

| 文件 | 新增 / 修改 | 要点 |
|---|---|---|
| `go/shared/placement/route.go` | 改 | 新常量 `RouteRowShards uint32 = 16`、函数 `RouteRowShard(playerID uint64) uint32`;注释写明「一旦上线不可改」、与 `RouteFenceShard`(`:45-50`,可配 N)不是一回事 |
| `go/shared/placement/route_test.go` | 改 | 新增 `TestRouteRowShard`(向量 + 常量冻结) |
| `go/data_service/internal/store/schema.go` | 改 | 五段路由 DDL 全文替换(§4.2);新增 `routeReconcileBootstrapDDL`;`bootstrapTables` 6→7 项(`:244-251`);`routeTableShapes` 改 `player_route` 列与主键、`route_meta` 列,新增 `route_reconcile`(`:265-285`);`migrateSchemaOn` 的 `:1617-1624` 抽成 `ensureRouteTables(ctx, db, dbName)`(形状守卫 + 种 `route_meta` + 种 `route_reconcile`);相关注释把「四张路由表」改成不写数量(`:31`、`:156`、`:165-167`、`:241`、`:259`、`:1361`、`:1583`) |
| `go/data_service/internal/store/route_store.go` | 改 | 全部按 `player_id` 的 SQL 改成带 `route_shard`(§4.3);`scanRouteRow` 支持读键列并校验片;`loadRoutes` 用行构造器 IN、按键排序;`ScanRoutes` 换游标类型;导入的插入 / 补列按键序执行;P4 探针;R16 种行;三个新哨兵;文件头「四张表」改为不写数量,锁顺序注释改为「player_route(按 (route_shard, player_id))」 |
| `go/data_service/internal/store/route_tidb.go` | **新** | `PrepareTiDB` 与纯函数 `routeTiDBPrepareStatements`(§4.8);不含任何 `INSERT/UPDATE/DELETE player_route` 字面量 |
| `go/data_service/internal/store/export_test.go` | 改 | 新导出:`RouteProbeShardCheckSQLForTest`、`RouteReconcileSeedSQLForTest`、`BootstrapRouteTablesOnlyForTest`(TiDB 用例用,见 §6.4)、`RouteSelectRowsSQLForTest(n int, forUpdate bool) string`、`RouteScanSQLForTest`;`RouteMetaSeedSQLForTest` 等保持 |
| `go/data_service/internal/store/route_store_test.go` | 改 | DDL 契约、SQL 清单、ODKU no-op、游标序、语句生成(§6.1) |
| `go/data_service/internal/store/route_store_contract_test.go` | 改 | 假驱动:新标签、实参形状、导入键序、扫描片校验、探针 P4、`PrepareTiDB`(§6.2) |
| `go/data_service/internal/store/route_store_integration_test.go` | 改 | 真 MySQL:迁移五表、CHECK、形状守卫、EXPLAIN、P4、片不一致 fail-closed、键集翻页;TiDB 可选用例(§6.3、§6.4) |
| `go/data_service/internal/routetool/importer.go` | 改 | `routeDB.ScanRoutes` 签名改为 `(ctx, after store.RouteScanCursor, limit int)`(`:66`);注释 |
| `go/data_service/internal/routetool/auditor.go` | 改 | 方向 A 游标改 `store.RouteScanCursor`(`:327-356`);文件头「按主键键集翻页」补「(route_shard, player_id)」 |
| `go/data_service/internal/routetool/tidb_prepare.go` | **新** | `TiDBPrepare` 编排、输出、退出码(§4.9) |
| `go/data_service/internal/routetool/routetool_test.go` | 改 | 假 `ScanRoutes` 按键序与游标过滤(`:88-107`);新增翻页跨片、`TiDBPrepare` 输出用例 |
| `go/data_service/internal/routetool/routetool_integration_test.go` | 改 | 裸 INSERT 补 `route_shard`(`:111-112`,9005 → 13) |
| `go/data_service/cmd/route_tool/main.go` | 改 | 新子命令 `tidb-prepare`;参数集按子命令拆;该子命令不连 mapping Redis(§4.10) |
| `go/data_service/cmd/route_tool/main_test.go` | 改 | 参数用例 + 「tidb-prepare 不连 Redis」 |
| `go/data_service/data_service.go` | 改(文案) | `:44` 注释、`:300` 迁移成功输出补 `route_reconcile`(建议改成「route tables player_route, zone_merge_fence, zone_merged_into, route_meta, route_reconcile in place (route_meta / route_reconcile row id=1 seeded if absent)」,保留现有的种行说明) |
| `go/data_service/internal/config/config.go` | 改(注释) | `:53`、`:412-413` 不写表数量,列出 `route_reconcile` |
| `go/data_service/etc/data_service.yaml` | 改(注释) | `:183`、`:186` 同上;**不新增配置键** |
| `tools/scripts/stress_round19.ps1` | 改 | `$RouteMetaResetSql` 只复位 `writable`;新增 `$RouteReconcileResetSql`;查表名单加 `route_reconcile`;plan 行(`:174`)补一行;注释注明 TiDB 不适用(§4.11) |
| `tools/scripts/currency_crash_window.ps1` | 改 | 同上(两份「改一处两处同改」,`:99`) |

不动:`routing/*`(W1 编排只调 `RegisterHome`,签名不变,`route_publish.go:37-39`)、`svc/*`、`server/*`、`metrics/*`、`config` 的结构体与校验、`k8s_deploy.ps1`(注释 `:3506` 不写数量,已合规)、proto、C++。

### 3.2 文档(§8 详述)

| 文件 | 改法 |
|---|---|
| `docs/design/player-route-mapping-durability-proposal.md` | §3.1 TiDB 适配一条加注;§3.2 DDL 换成新五表;§11.2 第 1a 批记录末尾(`:756` 之后、`:758` 之前)追加「第 1a 批热点修复(G1a)」段;§11.2 进度表(`:415-420`)加一行;§12 更新。**H1 不改**(否则 `docs/README.md` 索引行要同步) |
| `docs/design/tidb-phase1-and-launch-readiness.md` | §1.3、§2.5、§3.2 第 6/7/16 步、§3.5、§5.1、§5.2、§6.2、§7 L-12、§9.3、§9.4 逐处加「(2026-10-12 G1a)」改动;新增「开新 zone 的 TiDB 步骤」 |
| `docs/handoff/placement-reverify-checklist-20261012.md` | 「4 附录」末尾(`:243` 之后)追加 G1a 计数更正 |
| `docs/ops/merge-zone-runbook.md` | §9.3 恢复 SQL(`:783-788`)改为两条 UPDATE;追加 `tidb-prepare` 用法 |
| `docs/design/global-data-layer-tidb-decision.md` | §4 #11(`:210`)、§6 #8(`:230`)、`:123` 各追加一句 G1a 更正 |

`docs/README.md`:无新文档,不改。实现者若改了提案 H1,必须同步索引行并让 Codex 跑 `python tools/scripts/gen_docs_index.py --check`。

---

## 4. 关键类型、函数、SQL 与配置

### 4.1 `go/shared/placement/route.go`

```go
// RouteRowShards 是 player_route 的固定分片数:route_shard = player_id % RouteRowShards,
// 是聚簇主键 (route_shard, player_id) 的首列。目的是把单调递增的 player_id 在 TiDB 上拆成 16 条写入尾部
// (docs/design/player-route-mapping-durability-proposal.md §11.2「热点修复」)。
//
// **一旦任何环境建过 player_route 就不可改**:改模数 = 每一行换主键 = 整表重写;
// schema.go 的 chk_route_shard 里写死的 16 与这里必须一致(route_store_test.go 钉住)。
// 与 RouteFenceShard 无关:那是可配置的围栏分片数,只决定 W1 锁哪一个围栏行。
const RouteRowShards uint32 = 16

// RouteRowShard 返回 player_route.route_shard。写者与读者都只能经它计算(tools/merge_zone 已经 import
// shared/placement,第 2 批写库时直接调它,不得自己再写一份 % 16)。
func RouteRowShard(playerID uint64) uint32 {
	return uint32(playerID % uint64(RouteRowShards))
}
```

### 4.2 DDL 全文(Go 常量按 `schema.go` 现有逐行拼接风格书写)

```sql
CREATE TABLE IF NOT EXISTS `player_route` (
  `route_shard` tinyint unsigned NOT NULL COMMENT 'pb:10',
  `player_id` bigint unsigned NOT NULL COMMENT 'pb:1',
  `home_zone` int unsigned NULL COMMENT 'pb:2',
  `storage_id` int unsigned NULL COMMENT 'pb:3',
  `placement_version` bigint unsigned NULL COMMENT 'pb:4',
  `frozen_run_id` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NULL COMMENT 'pb:5',
  `rev` bigint unsigned NOT NULL COMMENT 'pb:6',
  `updated_by` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '' COMMENT 'pb:7',
  `created_at_ms` bigint unsigned NOT NULL COMMENT 'pb:8',
  `updated_at_ms` bigint unsigned NOT NULL COMMENT 'pb:9',
  PRIMARY KEY (`route_shard`, `player_id`) /*T![clustered_index] CLUSTERED */,
  KEY `idx_route_home` (`home_zone`, `route_shard`, `player_id`),
  KEY `idx_route_storage` (`storage_id`, `route_shard`, `player_id`),
  KEY `idx_route_updated` (`route_shard`, `updated_at_ms`, `player_id`),
  CONSTRAINT `chk_route_shard` CHECK (`route_shard` = `player_id` % 16),
  CONSTRAINT `chk_route_home` CHECK (`home_zone` IS NULL OR `home_zone` BETWEEN 1 AND 999999),
  CONSTRAINT `chk_route_place` CHECK ((`storage_id` IS NULL) = (`placement_version` IS NULL)),
  CONSTRAINT `chk_route_ver` CHECK (`placement_version` IS NULL OR `placement_version` >= 1),
  CONSTRAINT `chk_route_frozen` CHECK (`frozen_run_id` IS NULL OR `storage_id` IS NOT NULL),
  CONSTRAINT `chk_route_rev` CHECK (`rev` >= 1)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='player_route';

CREATE TABLE IF NOT EXISTS `zone_merge_fence` (
  `zone_id` int unsigned NOT NULL,
  `shard` tinyint unsigned NOT NULL,
  `run_id` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `purpose` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `src_zone` int unsigned NULL,
  `dst_zone` int unsigned NULL,
  `expires_at_ms` bigint unsigned NULL,
  `holder_json` varchar(1024) NULL,
  `fence_epoch` bigint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`zone_id`, `shard`) /*T![clustered_index] CLUSTERED */,
  CONSTRAINT `chk_fence_pair` CHECK ((`run_id` IS NULL) = (`expires_at_ms` IS NULL))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='zone_merge_fence';

CREATE TABLE IF NOT EXISTS `zone_merged_into` (
  `src_zone` int unsigned NOT NULL,
  `dst_zone` int unsigned NOT NULL,
  `run_id` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `merged_at_ms` bigint unsigned NOT NULL,
  PRIMARY KEY (`src_zone`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='zone_merged_into';

-- 开关表:很少写(1b 的冻结 / 回档恢复),每次 W1 普通读 writable。TiDB 上可设缓存表(D-3)。
CREATE TABLE IF NOT EXISTS `route_meta` (
  `id` tinyint unsigned NOT NULL,
  `generation` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `writable` tinyint unsigned NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='route_meta';

-- 对账表:1b leader 每拍写;第 1a 批不读不写,只由迁移种 id=1。
CREATE TABLE IF NOT EXISTS `route_reconcile` (
  `id` tinyint unsigned NOT NULL,
  `leader` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `lease_until_ms` bigint unsigned NULL,
  `watermark_ms` bigint unsigned NOT NULL DEFAULT 0,
  `cache_verified_ms` bigint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='route_reconcile';
```

要点:
- 列注释 `pb:10` 给第 2 批补 proto 预留(偏离第 1 条,提案 `:575`):proto message 里 `route_shard` 用字段号 10,`player_id` 仍是 1。
- `player_route` DDL 里**不得**再出现 `SHARD_ROW_ID_BITS` / `PRE_SPLIT_REGIONS`(契约测试钉住)。
- 新常量 `RouteReconcileTableName = "route_reconcile"`(放 `route_store.go:51-55` 那组)。
- `routeTableShapes`:
  - `player_route`:columns `route_shard, player_id, home_zone, storage_id, placement_version, frozen_run_id, rev, updated_by, created_at_ms, updated_at_ms`;primaryKey `route_shard, player_id`;
  - `route_meta`:columns `id, generation, writable`;primaryKey `id`;
  - `route_reconcile`:columns `id, leader, lease_until_ms, watermark_ms, cache_verified_ms`;primaryKey `id`;
  - 另两张不变。
- 形状守卫对 `player_route` **任一步失败**(缺列 **或** 主键不符)都追加一句提示:「若这是 G1a 之前第 1a 批的旧形状(没有 `route_shard` 列 / 主键 = player_id)且路由表里没有要保留的数据,`DROP TABLE player_route, zone_merge_fence, zone_merged_into, route_meta`(`route_reconcile` 若存在一并 DROP)后重跑 -migrate;**绝不自动 DROP**」。实现:`routeTableShape` 加可选字段 `hint string`,`assertRouteTableShapes`(`schema.go:289-312`)对该表的 `assertColumnNamesPresent` 与主键比对两处错误统一包一层 `fmt.Errorf("%w; %s", err, shape.hint)`。(评审 M2)
  - 为什么挂在表级、而不是只挂在主键分支:真实的旧表**没有 `route_shard` 列**,守卫先查列,会在列检查就以「lacks required columns [route_shard]」返回,只挂主键分支的提示永远走不到。
  - 为什么提示要 DROP 全部路由表:守卫只查列名是否**存在**、不查多余列与排序规则 —— 旧形状的 `route_meta`(多出 leader / 水位四列)、未带 `COLLATE` 的旧 `zone_merge_fence` / `zone_merged_into` 都能静默通过;只 DROP `player_route` 会留下一个「守卫全绿、形状却是混合态」的库。
  - 提示文本里不得出现 `UPDATE player_route` / `INSERT INTO player_route` 之类字样(`TestNoPlayerRouteWritesOutsideRouteStore` 按字符串字面量匹配)。

### 4.3 SQL 改写对照(`route_store.go`)

记号:`K` = `route_shard`,绑定值一律 `uint64(placement.RouteRowShard(id))`(与现有 `uint64(req.HomeZone)` 同口径,驱动收到 uint64);`NOW` = `dbNowMsSQL`(`:113`)。

| 编号 | 旧 | 新 | 实参(按顺序) |
|---|---|---|---|
| R1 | `SELECT writable FROM route_meta WHERE id = 1` | **不变** | — |
| R2 / R3 | 围栏 | **不变** | `(zone, fenceShard)` |
| R4 | `INSERT INTO player_route (player_id, …)` `:138-141` | `INSERT INTO player_route (route_shard, player_id, home_zone, storage_id, placement_version, frozen_run_id, rev, updated_by, created_at_ms, updated_at_ms) VALUES (?, ?, ?, ?, ?, NULL, 1, ?, NOW, NOW) ON DUPLICATE KEY UPDATE player_id = player_id` | `K, id, home, storage|nil, version|nil, writer`(6 个) |
| R5 | `… WHERE player_id = ? FOR UPDATE` `:144` | `SELECT <routeRowColumnsSQL> FROM player_route WHERE route_shard = ? AND player_id = ? FOR UPDATE` | `K, id` |
| R6a | `:147-148` | `UPDATE player_route SET home_zone = ?, rev = rev + 1, updated_by = ?, updated_at_ms = NOW WHERE route_shard = ? AND player_id = ? AND rev = ? AND home_zone IS NULL` | `home, writer, K, id, rev` |
| R6b | `:151-153` | `UPDATE player_route SET home_zone = ?, storage_id = ?, placement_version = 1, rev = rev + 1, updated_by = ?, updated_at_ms = NOW WHERE route_shard = ? AND player_id = ? AND rev = ? AND home_zone IS NULL AND storage_id IS NULL` | `home, storage, writer, K, id, rev` |
| R7 | head/tuple/tail `:156-159` | head `INSERT INTO player_route (route_shard, player_id, home_zone, storage_id, placement_version, frozen_run_id, rev, updated_by, created_at_ms, updated_at_ms) VALUES `;tuple `(?, ?, ?, ?, ?, ?, ?, ?, NOW, NOW)`;tail 不变 | 每行 `K, id, home|nil, storage|nil, version|nil, frozen|nil, rev, writer`(8 个);**行按 `(K, id)` 升序** |
| R8 / R8ro / R13 | `… WHERE player_id IN (?,…) ORDER BY player_id [FOR UPDATE]` `:162`、`:200-214` | `SELECT route_shard, player_id, <cols> FROM player_route WHERE (route_shard, player_id) IN ((?, ?), (?, ?), …) ORDER BY route_shard, player_id[ FOR UPDATE]` | 交错的 `K_i, id_i`,按 `(K, id)` 升序 |
| R9 | `… WHERE player_id = ? AND rev = ?` `:165-167` | `… WHERE route_shard = ? AND player_id = ? AND rev = ?` | `home|nil, storage|nil, version|nil, frozen|nil, writer, K, id, dbRev`;**补列按 `(K, id)` 升序** |
| R12 | `… WHERE player_id > ? ORDER BY player_id LIMIT ?` `:176` | `SELECT route_shard, player_id, <cols> FROM player_route WHERE route_shard > ? OR (route_shard = ? AND player_id > ?) ORDER BY route_shard, player_id LIMIT ?` | `uint64(after.Shard), uint64(after.Shard), after.PlayerID, limit`(片与 K 同口径绑 uint64) |
| R15 | `INSERT INTO route_meta (id, generation, writable, watermark_ms, cache_verified_ms) VALUES (1, ?, 1, 0, 0) ODKU id = id` `:180-181` | `INSERT INTO route_meta (id, generation, writable) VALUES (1, ?, 1) ON DUPLICATE KEY UPDATE id = id` | `generation` |
| **R16(新)** | — | `INSERT INTO route_reconcile (id, watermark_ms, cache_verified_ms) VALUES (1, 0, 0) ON DUPLICATE KEY UPDATE id = id` | — |
| **P4(新,探针)** | — | `INSERT INTO player_route (route_shard, player_id, rev, updated_by, created_at_ms, updated_at_ms) VALUES (1, 0, 1, 'route-probe', 0, 0)`,包在 `BEGIN … ROLLBACK` 里 | — |
| **T1(新,tidb-prepare)** | — | `SELECT TABLE_NAME, TIDB_PK_TYPE FROM information_schema.TABLES WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME IN ('player_route', 'zone_merge_fence')` | — |
| **T2 / T3(新)** | — | `ALTER TABLE … ATTRIBUTES 'merge_option=deny'` / `SPLIT TABLE …`(§4.8) | — |

- 行构造器 `(a, b) IN ((…),(…))`:MySQL 8.0 对作为索引前缀的行构造器 IN 走 range(推断,由 §6.3 的 EXPLAIN 用例证实);TiDB 期望 Batch_Point_Get【未核实,§6.4 只记录计划、不硬断言】。1000 对 = 2000 个占位符,远低于 65535。
- R12 写成 `a > x OR (a = x AND b > y)` 而不是 `(a, b) > (x, y)`:前者两种引擎的 range 优化都确定可用,后者在 MySQL 上能否走 range 不确定(推断)。
- 锁顺序(文件头 `:37-38` 同步改):`route_meta`(普通读)→ `zone_merge_fence` 分片(FOR UPDATE)→ `player_route`(按聚簇键 `(route_shard, player_id)`)。W1 只持一个分片行、最多再等一个玩家行;导入只按聚簇键升序锁玩家行、不碰分片;**任何一次锁多行的语句都按聚簇键升序**(R8 FOR UPDATE、R7、R9 循环),两个并发导入之间也不成环。

### 4.4 `RegisterHome`(`route_store.go:649-816`)

- 入参校验之后多算一个 `rowShard := placement.RouteRowShard(req.PlayerID)`;现有代码里围栏片的局部变量 / 形参叫 `shard`(`RegisterHome`、`registerHomeEnsuringShard`、`registerHomeTx`),**统一改名 `fenceShard`**,`rowShard` 作为显式形参传进 `registerHomeTx`(或在其内算),两个「片」在同一函数里不得再有同名变量。(评审 M3)
- **绑错片的后果**:若误把围栏片(`player_id % FenceShards`,默认 8)绑进 R4 / R5 / R6,`player_id % 16 ≥ 8` 的那一半新号 R4 撞 `chk_route_shard` 3819 → 建角失败;R5 / R6 按错片点查读不到行 → `ErrRouteRowVanished`。契约测试必须能区分两者(§6.2 新增用例)。
- `registerHomeTx` 的 R4 / R5 / R6 按 §4.3 绑定;结局、重试、错误语义一格不改。
- `RouteRow` 不加字段:片由 `player_id` 唯一决定,扫描时校验一致后丢弃。

### 4.5 读与扫描

```go
// RouteScanCursor 是 ScanRoutes 的键集游标 = 上一页最后一行的聚簇键 (route_shard, player_id)。
// 零值 = 从头开始(player_id 为 0 的行不合法,所以 {0, 0} 之后就是第一行)。
type RouteScanCursor struct {
	Shard    uint32
	PlayerID uint64
}

// RouteScanCursorAt 返回 row 的聚簇键(片由 player_id 算出;ScanRoutes 已校验库里存的片与之一致)。
func RouteScanCursorAt(row RouteRow) RouteScanCursor

// Less 按聚簇键序比较(先片、后 player_id)。假 routeDB 与排序共用这一个判据。
func (c RouteScanCursor) Less(o RouteScanCursor) bool

// ScanRoutes 按聚簇键键集翻页:返回键 > after 的至多 limit 行,按 (route_shard, player_id) 升序。
// 任一行库里存的 route_shard ≠ player_id % 16 → ErrRouteShardMismatch(整页作废,fail-closed)。
func (s *RouteStore) ScanRoutes(ctx context.Context, after RouteScanCursor, limit int) ([]RouteRow, error)
```

- `ScanRoutes` 入参校验补一条:`after.Shard >= placement.RouteRowShards` → `ErrRouteInvalidArgument`(否则查询静默回空页,audit 会当作扫完)。错误文案里的游标写成 `(shard, player_id)`。
- `scanRouteRow(sc, row, withKey bool)`(原 `withPlayerID`,`:539-564`):`withKey` 时先扫 `route_shard`(到 `uint64`)与 `player_id`,扫完校验 `shard == uint64(placement.RouteRowShard(row.PlayerID))`,不等即 `fmt.Errorf("%w: player_id=%d route_shard=%d want %d", ErrRouteShardMismatch, …)`。R5 仍用不带键的形态。
- `loadRoutes(ctx, q, ids, forUpdate)`(`:1410-1433`):拷贝 `ids`、按键排序(`sortRouteIDsByKey`)、拼 R8 / R13;结果仍是 `map[uint64]RouteRow`。`GetRoutes` 外部契约不变(缺席即不在 map 里、0 跳过、超限拒绝)。**注意**:R8 / R13 按正确片点查,库里的错片行对它们不可见(见 D-1),这里的片校验只是纵深防御;`GetRoutes` 的文档注释要写明「错片行在这里表现为缺席,检测靠 `ScanRoutes`」。
- 导入(`importRoutesTx`,`:950-1014`):`planRouteImport` 不变(dry-run 统计口径不变);执行前对 `plan.inserts` 与 `plan.fills` 按键排序再拼 R7 / 逐条发 R9。`ImportRoutes` 的入参契约**不变**(仍要求按 `player_id` 严格升序、无重复,`validateRouteImportRows` `:1175-1201`),routetool 的 `flush`(`importer.go:323-352`)不用改。
- `RouteBatchLimit`(`:64`)注释补:R8 每批 2×N 个占位符、R7 每批 8×N 个。

### 4.6 建 store 探针 P4(`newRouteStoreFromDB`,`:309-329`)

顺序:P1 会话变量 → P2 TiDB 事务模式 → P3 `route_meta` 行(R1)→ **P4 CHECK 实测**。

```go
// probeRouteShardCheck:player_id 的唯一性现在靠 chk_route_shard(主键是 (route_shard, player_id)),
// CHECK 不生效时一个算错片的写者就能给同一个 player_id 写出两行。所以在一个必然回滚的事务里插一行
// 「只违反 chk_route_shard」的行(player_id=0、route_shard=1,其余列都合法),必须被 3819 拒绝、
// 且报错点名 chk_route_shard;插入成功或被别的约束拒绝都说明我们证不了它,拒建 store。
func probeRouteShardCheck(ctx context.Context, db *sql.DB) error
```

判定:
| 结果 | 返回 |
|---|---|
| `*mysql.MySQLError` 3819 且 `strings.Contains(strings.ToLower(Message), "chk_route_shard")` | nil |
| 执行成功(CHECK 没生效) | `ErrRouteCheckNotEnforced`(文案带「MySQL ≥ 8.0.16;TiDB ≥ 7.2 且建表前 `SET GLOBAL tidb_enable_check_constraint=ON`」) |
| 3819 但点名别的约束 | `fmt.Errorf("%w: %v", ErrRouteCheckNotEnforced, err)` |
| 其它错误(1146 等) | 原样包装返回 |

无论哪种,事务都回滚(`defer tx.Rollback()`),所以 CHECK 没生效时那行也不会留下。多副本同时探针:CHECK 生效时语句在写索引前就被拒,不取锁;不生效时第二个最多等 1s 锁(会话锁等待封顶)后同样回滚。

补充(评审):
- `routeProbeTimeout`(`route_store.go:80-81`)与 `newRouteStoreFromDB` 头注释的探针清单补上第 4 步;P1–P4 共用这 5s。
- 「其它错误」里两种要在文案里点名:1062(库里已有 `(1, 0)` 这一行 = CHECK 曾经不生效时被人写进过错片行,先人工清掉)、1142(账号没有 INSERT 权限)。
- P4 是一次写尝试:`route_tool audit` 这类只读用途从此也需要 `player_route` 的 INSERT 权限。今天 route_tool 与 data_service 同一份 yaml、同一个账号,不受影响;写进 H5 的代价。
- 整个 route store 真库套件(以及 routing 的 `TestDBFirstRegister_RealStores`)从此要求 MySQL ≥ 8.0.16:更老的 MySQL 上建 store 必然失败(不是 SKIP)。本机 compose 的 MySQL 满足;写进 §7 前置。

### 4.7 迁移(`schema.go`)

```go
// ensureRouteTables 在 bootstrapTables 全部执行之后调用(migrateSchemaOn 与 TiDB 用例的测试钩子共用):
// 形状守卫 → 种 route_meta(id=1)→ 种 route_reconcile(id=1)。任一步失败整次迁移失败(与今天同口径,
// 1a 风险第 8 条:redis 模式也受影响)。
func ensureRouteTables(ctx context.Context, db *sql.DB, dbName string) error

// bootstrapRouteReconcileRow 种 route_reconcile(id=1)(R16,ODKU no-op;已存在一个字节都不改),
// 日志区分「新种」与「已存在」,口径同 bootstrapRouteMetaRow(route_store.go:1462-1485)。
func bootstrapRouteReconcileRow(ctx context.Context, db *sql.DB) error
```

`bootstrapRouteMetaRow` 只改 R15 文本。`migrateSchemaOn` 的 `:1617-1624` 换成一行 `ensureRouteTables`。

### 4.8 `store/route_tidb.go`

```go
// RouteTiDBPrepareRequest 是 tidb-prepare 的入参。
type RouteTiDBPrepareRequest struct {
	Zones    []uint32 // 必填,每个在 [1, RouteHomeZoneMax];去重升序后使用;上限 RouteBatchLimit 个
	Storages []uint32 // 可空,每个 ≥ 1;去重升序
	DryRun   bool     // true:只判引擎、核对聚簇、列出语句,一条 ALTER / SPLIT 都不发
}

// RouteTiDBPrepareStep 是一条语句及其执行结果。
type RouteTiDBPrepareStep struct {
	SQL          string
	Executed     bool
	SplitRegions int64 // SPLIT 返回的首列(TOTAL_SPLIT_REGION);ALTER 为 0
}

// RouteTiDBPrepareReport:Engine 为 "mysql" 或 "tidb";MySQL 上 Skipped=true、Steps 为空。
type RouteTiDBPrepareReport struct {
	Engine  string
	Version string
	Skipped bool
	Steps   []RouteTiDBPrepareStep
}

func (r RouteTiDBPrepareReport) SplitRegions() int64

// PrepareTiDB 见规格 D-5:判引擎 → 核对两张表都是聚簇主键 → ATTRIBUTES ×2 → SPLIT ×(3 + zones + storages)。
// 中途失败返回已执行的步骤 + error(带第几步与语句);全部语句幂等,重跑即可。
func (s *RouteStore) PrepareTiDB(ctx context.Context, req RouteTiDBPrepareRequest) (RouteTiDBPrepareReport, error)

// normalizeRouteTiDBPrepareIDs 校验并规范化入参:zones 非空、每个 ∈ [1, RouteHomeZoneMax]、去重后不超过
// RouteBatchLimit 个;storages 每个 ≥ 1;两者都去重升序。非法一律 ErrRouteInvalidArgument(一条语句都不发)。
func normalizeRouteTiDBPrepareIDs(zones, storages []uint32) ([]uint32, []uint32, error)

// routeTiDBPrepareStatements 是纯函数:zones / storages 已规范化(去重升序、范围合法)。
func routeTiDBPrepareStatements(zones, storages []uint32) []string
```

`routeTiDBPrepareStatements([]uint32{1, 2}, []uint32{1000000})` 的期望输出(表名、索引名用反引号;元组之间 `", "`):

```
ALTER TABLE `player_route` ATTRIBUTES 'merge_option=deny'
ALTER TABLE `zone_merge_fence` ATTRIBUTES 'merge_option=deny'
SPLIT TABLE `player_route` BY (1, 0), (2, 0), …, (15, 0)
SPLIT TABLE `player_route` INDEX `idx_route_updated` BY (1, 0, 0), (2, 0, 0), …, (15, 0, 0)
SPLIT TABLE `player_route` INDEX `idx_route_home` BY (1, 0, 0), (1, 1, 0), …, (1, 15, 0)
SPLIT TABLE `player_route` INDEX `idx_route_home` BY (2, 0, 0), (2, 1, 0), …, (2, 15, 0)
SPLIT TABLE `player_route` INDEX `idx_route_storage` BY (1000000, 0, 0), (1000000, 1, 0), …, (1000000, 15, 0)
SPLIT TABLE `zone_merge_fence` BY (1, 0), (2, 0)
```

- 片的上界循环用 `placement.RouteRowShards`,不写字面量。
- SPLIT 用 `QueryContext` 执行,结果集按列数通用扫描(`[]sql.RawBytes`),取首列解析成 int64;列数为 0、**零行**、或首列解析不了 → 报错(fail-closed)。
- import 清单以实际使用为准(`errors` 只在 `ErrRouteTiDBNotClustered` 定义在本文件或用到 `errors.Is` 时需要;`sort` 也可换 `slices`,go.mod 是 1.26),交 Codex 前逐个核对,不留未用 import。
- `ALTER TABLE … ATTRIBUTES` 是 DDL:TiDB 上会推进 schema 版本,**在线重跑**(开新 zone 时)可能让正在途中的 W1 悲观事务以「Information schema is changed」(8028)失败【未核实】,W1 只重试 1213 / 9007,这一次建角会回 Unavailable、由客户端重试。所以 runbook 写明:开新 zone 时在该 zone 对外开放前、低峰执行;记入 HR9 与 §9.4 待核实项。不在本批给 W1 加 8028 重试(改变既有重试口径,另议)。
- T1 结果缺表或任一不是 `CLUSTERED`(大小写不敏感)→ `ErrRouteTiDBNotClustered`,dry-run 同样执行 T1(只读)。
- 本文件不出现任何 `INSERT/UPDATE/DELETE/REPLACE … player_route` 字面量(`TestNoPlayerRouteWritesOutsideRouteStore` 只允许 `route_store.go`,`route_store_test.go:214-279`)。

### 4.9 `routetool/tidb_prepare.go`

```go
// tidbPreparer 是本子命令对全局库的全部依赖,*store.RouteStore 天然满足(编译期断言放 routetool_test.go)。
type tidbPreparer interface {
	PrepareTiDB(ctx context.Context, req store.RouteTiDBPrepareRequest) (store.RouteTiDBPrepareReport, error)
}

type TiDBPrepareOptions struct {
	Zones, Storages []uint32
	DryRun          bool
	Target          string    // 非空时打在 stderr 第一行(只含 host 与库名)
	Stdout, Stderr  io.Writer // stdout 只写一行统计
}

func TiDBPrepare(ctx context.Context, p tidbPreparer, opts TiDBPrepareOptions) (store.RouteTiDBPrepareReport, error)
func TiDBPrepareExitCode(rep store.RouteTiDBPrepareReport, err error) int // err → ExitFailed;否则 ExitClean(含 MySQL 跳过)
```

输出:
- stderr:`Target`;MySQL 时 `SKIP: <version> is not TiDB; nothing to split (MySQL has no Regions)`;TiDB 时每步一行 `PLAN <sql>`(dry-run)或 `DONE <sql> split_regions=<n>`;失败时 `FAILED step <i>/<n>: <sql>: <err>`。
- stdout(只有完整跑完或跳过时才写):`route_tidb_prepare engine=<mysql|tidb> skipped=<bool> dry_run=<bool> zones=<n> storages=<n> statements=<n> executed=<n> split_regions=<n>`。

### 4.10 `cmd/route_tool/main.go`

- 新常量 `subTiDBPrepare = "tidb-prepare"`;用法(`:4-5`、`:48-53`)加一行:
  `route_tool -f <data_service.yaml> tidb-prepare -zones=1,2[,…] [-storages=1000000[,…]] [-dry-run=true] [-timeout=30m]`;退出码说明补「tidb-prepare:0 完成或 MySQL 跳过;2 失败」。
- `parseArgs`(`:160-221`)按子命令登记参数:
  - 全部:`-timeout`;
  - import / audit:`-batch`、`-max-report`(其余不变);
  - import:`-dry-run`、`-allow-db-first`;audit:`-recheck-delay`、`-max-recheck`;
  - tidb-prepare:`-zones`(必填)、`-storages`、`-dry-run`(默认 true)。
  - `-zones` / `-storages` 解析(main.go 新增 import `strconv`、`strings`):逗号分隔、每项 `strconv.ParseUint(s, 10, 32)`、拒空项;`-zones` 每项 ∈ [1, 999999](`store.RouteHomeZoneMax`),`-storages` 每项 ≥ 1。非法 → 退出码 2 并打印用法(与现有口径同)。
  - 未知子命令文案改为列出三个。
- `run`(`:73-157`):`tidb-prepare` 在 `connectMappingRedis`(`:113`)**之前**分流 —— 预切只碰全局库,不连 Redis;`openRouteStore`(`:240-249`)照旧(会跑 P1–P4 探针,TiDB 上同时证明悲观事务与 CHECK 已就绪);`target` 只含 `global_db=host/db`。db_first 闸(`:102`)只对 import。
- 文件头(`:1-17`)补一段:它为什么在 route_tool 里、MySQL 上自动跳过、何时要跑(开新 zone、BR/PITR 恢复后、TRUNCATE 后)、K8s 上要等第 1b 批把 route_tool 并进主程序入口(偏离第 8 条,提案 `:582`)。

### 4.11 清库脚本(两份同改)

```powershell
$RouteDataTables        = @('player_route', 'zone_merge_fence', 'zone_merged_into')
$RouteMetaResetSql      = 'UPDATE route_meta SET writable=1 WHERE id=1;'
$RouteReconcileResetSql = 'UPDATE route_reconcile SET leader=NULL, lease_until_ms=NULL, watermark_ms=0, cache_verified_ms=0 WHERE id=1;'
```
- `Clear-GlobalRouteTables`:查表名单 = 三张数据表 + `route_meta` + `route_reconcile`;存在哪张就对哪张生成语句;两张元数据表都「原地复位,不 TRUNCATE」(理由同现注释 `stress_round19.ps1:104-105`:1b leader 可能在跑,行没了它的租约 UPDATE 会落空)。
- `stress_round19.ps1:174` 的 plan 行改为两行(`route_meta(id=1) writable reset` / `route_reconcile(id=1) reset`)。
- 注释补:两份脚本只连本机 `mysql` 容器;TiDB 环境清库按 `tidb-phase1-and-launch-readiness.md` §3.5,TRUNCATE 之后要重跑 `route_tool tidb-prepare`。
- 用 `"${GlobalDbName}:"` 写法的地方保持现状;新增插值若紧跟冒号必须写成 `${var}:`。

### 4.12 未来批次的读方 SQL 模板(写进提案 G1a 段,供第 1b / 2 / 3 批照抄)

| 用途 | SQL | 游标 / 起点 |
|---|---|---|
| 按 zone 枚举(R7、W10) | `SELECT route_shard, player_id[, …] FROM player_route WHERE home_zone = ? AND (route_shard > ? OR (route_shard = ? AND player_id > ?)) ORDER BY route_shard, player_id LIMIT ?`(W10 再加 `AND storage_id IS NULL` 与 `FOR UPDATE`,锁序分析留给第 2 批规格) | `(shard, id)`,起点 `{0, 0}`;走 `idx_route_home`、无 filesort |
| 按落点枚举 | 同上,`home_zone` 换 `storage_id` | 同上;走 `idx_route_storage` |
| 1b 增量推送 | 按片循环 `s = 0..15`:`SELECT route_shard, player_id, <cols> FROM player_route WHERE route_shard = ? AND (updated_at_ms > ? OR (updated_at_ms = ? AND player_id > ?)) ORDER BY updated_at_ms, player_id LIMIT ?` | 每片一个 `(updated_at_ms, player_id)` 游标;「`>= since`」的起点 = `(since, 0)` 且第一页条件写成 `updated_at_ms > since OR (updated_at_ms = since AND player_id > 0)` |
| 1b 全量预热 | 即 R12 | `RouteScanCursor` |
| 按 id 回源(第 3 批) | 点查 R5 的不加锁形态;批量即 R13 | — |

### 4.13 配置

**无新配置键,无默认值变化**。`RouteStore.*`(提案 `:511-519`)、`FenceShards`(默认 8,上限 64)不动;`RouteRowShards` 是代码常量、刻意不可配。新增的只有 CLI 参数(§4.10):`-zones`(无默认,必填)、`-storages`(默认空)、`-dry-run`(默认 true)、`-timeout`(默认 30m,沿用)。

---

## 5. 错误语义

| 情形 | 哨兵 / 错误 | 上抛到 | 对外表现 |
|---|---|---|---|
| P4:CHECK 不生效(插入成功,或被别的约束拒) | `ErrRouteCheckNotEnforced`(新) | `NewRouteStore` 失败 | data_service db_first:store 不装配、打 ERROR,K8s `Store.Required=true` 拒启(`svc/servicecontext.go:161-162`),本地 W1 → Unavailable + 30;route_tool / debugutil:退出码 2 |
| 扫到库里片与 id 不符的行 | `ErrRouteShardMismatch`(新) | **只有 `ScanRoutes`(R12)在真库上能触发**;`GetRoutes` / `ImportRoutes` 的 R8 / R13 按正确片点查,错片行对它们不可见、表现为缺席(那里的校验只是纵深防御) | audit 方向 A 退出码 2;不吞。点读把错片行当缺席是结构性的,兜底靠 P4 + 定期 audit |
| tidb-prepare:表不是聚簇主键 | `ErrRouteTiDBNotClustered`(新) | `PrepareTiDB` | 退出码 2,零 ALTER / SPLIT |
| tidb-prepare:参数非法 | `ErrRouteInvalidArgument`(既有) | `PrepareTiDB` / CLI 解析 | 退出码 2 |
| tidb-prepare:某条 ALTER / SPLIT 失败 | 包着 MySQLError,带「step i/n」与语句 | `PrepareTiDB` | 退出码 2;已执行的保留,重跑幂等 |
| tidb-prepare 连的是 MySQL | 无错误,`Skipped=true` | — | 退出码 0 |
| 迁移:主键仍是旧形状(`player_id`) | 形状守卫 error(带 G1a 提示) | `MigrateSchema` | 与今天同口径:整次迁移失败(1a 风险第 8 条) |
| W1 / 导入的全部既有结局与错误 | 不变 | — | gRPC 码与 error_code 不变(提案 `:529-547`) |

三个新哨兵放在 `route_store.go:91-106` 那组(`ErrRouteTiDBNotClustered` 放 `route_tidb.go` 也可,二选一,但只能有一处定义)。

---

## 6. 测试清单

### 6.1 不连库单测(默认 `go test ./...` 即跑)

| 包 / 文件 | 用例 | 改 / 新 | 内容 |
|---|---|---|---|
| `shared/placement/route_test.go` | `TestRouteRowShard` | 新 | `RouteRowShards == 16`(失败文案写「改它会让存量行的主键全部失效」);向量:0→0、1→1、15→15、16→0、17→1、4242→2、2^55−1→15、`math.MaxUint64`→15 |
| `store/route_store_test.go` | `TestRouteBootstrapDDLContract` | 改 | `bootstrapTables` 7 项;`player_route` 含 `PRIMARY KEY (\`route_shard\`, \`player_id\`) /*T![clustered_index] CLUSTERED */`、三条新索引全文、`chk_route_shard` 全文且其中模数 = `strconv.FormatUint(uint64(placement.RouteRowShards), 10)`、`chk_route_*` 共 6 条、`idx_route_*` 共 3 条、**不含** `SHARD_ROW_ID_BITS` / `PRE_SPLIT_REGIONS` / `NONCLUSTERED`;`route_meta` DDL **不含** `watermark_ms` / `leader` / `lease_until_ms` / `cache_verified_ms`;`route_reconcile` DDL 含这四列;五张小表都含 `COLLATE=utf8mb4_bin`;所有 `CHARACTER SET ascii` 后紧跟 `COLLATE ascii_bin`;`routeTableShapes` 5 张且与 DDL 互相覆盖(现有循环逻辑扩到 5 张) |
| 同上 | `TestRouteSQLNeverUsesInsertIgnore` | 改 | `allRouteSQLForTest` 加 `R16`、`P4`、`T1`、`ddl_reconcile`,以及 `routeTiDBPrepareStatements([]uint32{1, 2}, []uint32{1000000})` 的每一条 |
| 同上 | `TestRouteODKUClausesAreNoOp` | 改 | 加 `{"R16", routeReconcileSeedSQL, "id = id"}` |
| 同上 | `TestRouteScanCursorOrder` | 新 | `Less` 先比片后比 id;`RouteScanCursorAt` 用 `RouteRowShard`;零值小于任何合法键 |
| 同上 | `TestRouteTiDBPrepareStatements` | 新 | 上面 8 条期望逐字相同;`PrepareTiDB` 的规范化(去重升序)用一个导出给测试的纯函数或在本包直接调:`{3,1,3}` → `{1,3}`;zone 0、1000000、空列表、超过 `RouteBatchLimit` 个 → `ErrRouteInvalidArgument`;storage 0 → 拒绝 |
| 同上 | `TestNoPlayerRouteWritesOutsideRouteStore` | 不改 | 新文件 `route_tidb.go` 必须仍让它通过(只有 `route_store.go` 命中) |
| `routetool/routetool_test.go` | 假 `fakeRouteDB.ScanRoutes` | 改 | 签名 `(ctx, after store.RouteScanCursor, limit int)`;按 `store.RouteScanCursorAt(row)` 过滤 `after.Less(key)` 并按 `Less` 排序 |
| 同上 | `TestRouteAuditPagesAcrossShards` | 新 | `Batch: 2`,库行 id = {1, 17, 33, 2, 18, 15, 31}(片 1,1,1,2,2,15,15),Redis 一致 → `DBRows=7`、差异 0、退出码 0;再删其中一把 Redis 键 → 恰好 1 个差异(证明翻页跨片不漏不重) |
| 同上 | `TestRouteTiDBPrepareOutputAndExitCodes` | 新 | 假 preparer 三种:MySQL 跳过 → 退出 0、stdout 一行含 `engine=mysql skipped=true`、stderr 含 `SKIP`;TiDB dry-run → 每条 `PLAN`、`executed=0`;返回 error → 退出 2、stdout 为空、stderr 含 `FAILED` |
| 同上 | 编译期断言 | 新 | `var _ tidbPreparer = (*store.RouteStore)(nil)` |
| `cmd/route_tool/main_test.go` | `TestRouteToolBadArgsExitTwo` | 改 | 加:`tidb-prepare` 缺 `-zones`(期望含 `-zones`)、`-zones=0`、`-zones=1,a`、`-zones=1,`(空项)、`tidb-prepare -zones=1 -batch=10`(期望含 `batch`)、`audit -zones=1`(期望含 `zones`);未知子命令文案仍含 `unknown subcommand` |
| 同上 | `TestRouteToolTiDBPrepareDoesNotTouchRedis` | 新 | yaml 的 MappingRedis 与 SnapshotMySQL.Host 指向**两个不同的**刚关掉的端口(`closedLocalAddr` 各取一次;`minimalYaml` + `SnapshotMySQL:\n  Host: %s\n`,不写 Host 会回落默认 `127.0.0.1:3306`,本机可能真有 MySQL)→ 退出 2、stdout 空、stderr 含 MySQL 那个地址、**不含** Redis 那个地址、**不含** `unreachable`(那是 Redis 的文案) |

### 6.2 假驱动契约测试(`store/route_store_contract_test.go`,默认即跑)

| 改 / 新 | 内容 |
|---|---|
| `routeStmtLabel`(`:96-142`) | 新精确标签:`R16`、`PROBE_CHECK`、`PK_TYPE`;新前缀标签:`"ALTER TABLE "` → `ALTER_ATTR`、`"SPLIT TABLE "` → `SPLIT`;R8 / R8ro 前缀判定改用新 head |
| `routeRowValues`(`:402-406`) | 前面多一列 `int64(placement.RouteRowShard(player))`;新增 `routeRowValuesWithShard(shard, player, …)` 造不一致行 |
| `wantRegisterArgs`(`:428-452`) | 改:函数里现有的 `shard` 是**围栏片**,改名 `fenceShard`,另算 `rowShard := uint64(placement.RouteRowShard(req.PlayerID))`;R2 / R3 `{home, fenceShard}`;R4 `{rowShard, id, home, storage, version, writer}`;R5 `{rowShard, id}`;R6a `{home, writer, rowShard, id, rowRev}`;R6b `{home, storage, writer, rowShard, id, rowRev}`(`contractPlayer` 常量注释补「4242 % 8 = 4242 % 16 = 2:本常量**区分不了**围栏片与行片,见下一行新用例」) |
| 新 `TestRegisterHomeBindsRowShardNotFenceShard` | (评审 M3)`PlayerID = 4250`(围栏片 4250 % 8 = 2,行片 4250 % 16 = 10)跑三条路径:新插(R1 → R2 → R4=1 → COMMIT)、补 home(R4=0 → R5 → R6a)、补 home + 落点(R6b);逐条断言 R2 / R3 实参里是 2、R4 / R5 / R6a / R6b 实参里是 10;再断言 `wantRegisterArgs` 对 4250 算出的两个片确实不同(防止用例本身退化) |
| `TestRegisterHomeOutcomeContract` | 不改用例表,只靠 `assertRegisterArgs` 新实参 |
| `TestRegisterHomeBindsNullForAbsentPlacement`(`:765-800`) | R4 实参 6 个;storage / version 在下标 3、4 |
| `TestImportRoutesStatementOrderAndAffectedMismatchRetries`(`:949-1037`) | R8 实参改为按键序的交错对 `{2,50, 4,20, 8,40, 10,10, 14,30}`(uint64);R7 实参 `{10, 10, 7, 1000000, 1, nil, 5, writer}`;R9 实参 `{7, 7, 1, nil, writer, 4, 20, 2}`;R8 / R8 重读的应答行带片列 |
| 新 `TestImportRoutesInsertsAndFillsInRouteKeyOrder` | 批次 `{2, 17}`(id 升序合法)两行都新插:R7 实参顺序必须是 17(片 1)在前、2(片 2)在后;两行都补列时 R9 的发出顺序同样按键序 |
| `TestImportRoutesDryRunIssuesNoWriteAndNoLockingRead` | 只改应答行形状 |
| `TestRouteReadQueriesShapes`(`:1132-1184`) | R12 调用改 `ScanRoutes(ctx, RouteScanCursor{Shard: 5, PlayerID: 5}, 2)`,实参 `{uint64(5), uint64(5), uint64(5), int64(2)}`;GetRoutes `[9, 0, 11]` 的 R8ro 实参 `{9, 9, 11, 11}`;应答行带片列;**新子用例**:R12 回一行 `(3, 6, …)`(6 % 16 ≠ 3)→ `errors.Is(err, ErrRouteShardMismatch)`;R8ro 回不一致行 → `GetRoutes` 同样报错(**纵深防御用例**:假驱动才造得出这种应答,真库上 R8 按正确片点查读不到错片行,注释里写明);`ScanRoutes(ctx, RouteScanCursor{Shard: 16}, 1)` → `ErrRouteInvalidArgument` 且一条语句都不发 |
| `TestNewRouteStoreProbeFailClosed`(`:1190-1314`) | 三个成功用例的 `wantLabels` 末尾追加 `"BEGIN", "PROBE_CHECK", "ROLLBACK"`,应答 `PROBE_CHECK: {replyErr(&mysql.MySQLError{Number: 3819, Message: "Check constraint 'chk_route_shard' is violated."})}`;新增失败用例:① `PROBE_CHECK` 回 `replyAffected(1)` → `ErrRouteCheckNotEnforced`、序列以 `ROLLBACK` 结尾、池已关;② 3819 但 Message 点名 `chk_route_rev` → `ErrRouteCheckNotEnforced`;③ 回 1146 → `errors.As` 取到 1146 |
| 新 `TestPrepareTiDBContract` | ① `VERSION` 回 `8.0.36` → `Skipped`、只发 `VERSION`;② TiDB + `PK_TYPE` 回两行 CLUSTERED + `ALTER_ATTR`×2 + `SPLIT`×6(zones `{1,2}`、storages `{1000000}`,每条回 `[[int64(k), float64(1)]]`)→ 语句序列 = `VERSION, PK_TYPE, ALTER_ATTR, ALTER_ATTR, SPLIT×6`,每条 SQL 与 `routeTiDBPrepareStatements` 逐字相同,`SplitRegions()` = 各 k 之和;③ `PK_TYPE` 有一行 `NONCLUSTERED` → `ErrRouteTiDBNotClustered`、无 ALTER / SPLIT;④ dry-run → 只发 `VERSION, PK_TYPE`,`Steps` 全部 `Executed=false`;⑤ 第 4 条 SPLIT 回错 → 报错带「step 6/8」,前 5 步 `Executed=true`、之后不再发 |

### 6.3 真 MySQL 集成(`-tags=integration`,需要 `DATA_SERVICE_IT_MYSQL_*`)

| 用例 | 改 / 新 | 内容 |
|---|---|---|
| `seedRouteRow`(`:80-87`) | 改 | INSERT 补 `route_shard`(`placement.RouteRowShard(player)`) |
| `TestRouteSchema_MigrationCreatesTablesSeedsMetaAndIsIdempotent`(`:127-163`) | 改 | `wantPK`:`player_route` → `{"route_shard","player_id"}`,加 `route_reconcile` → `{"id"}`;`route_reconcile` 恰好 1 行、`watermark_ms=0`;`route_meta` 的列里没有 `watermark_ms`;二次迁移 SHOW CREATE TABLE 五张都不变 |
| `TestRouteSchema_CheckConstraintsEnforced`(`:165-193`) | 改 | 每条违例 INSERT 补**正确的** `route_shard`(否则先撞 1364 / 别的约束,用例就不再证它想证的那条):`(1,1,…home 0)`、`(2,2,…home 1000000)`、`(3,3,…storage 7)`、`(4,4,…version 0)`、`(5,5,…frozen)`、`(6,6,…rev 0)`;新增「route_shard 与 player_id 不符」`INSERT … (route_shard, player_id, home_zone, rev, created_at_ms, updated_at_ms) VALUES (2, 1, 7, 1, 1, 1)` → 3819 且错误文本含 `chk_route_shard` |
| `TestRouteSchema_ShapeGuardRejectsTamperedTable`(`:195-217`) | 改 | 新子用例「player_route 主键退回 player_id」:`ALTER TABLE player_route DROP PRIMARY KEY, ADD PRIMARY KEY (player_id)` → 迁移失败、错误含 `primary key` 与 G1a 提示;新子用例「G1a 之前的真实旧形状」(评审 M2):`DROP TABLE player_route` 后用 G1a 之前第 1a 批的 `player_route` DDL **原文**(在用例里写成字面量,不引用生产常量;无 `route_shard` 列、`PRIMARY KEY (player_id) NONCLUSTERED`)重建 → 迁移失败、错误同时含 `route_shard` 与 G1a 提示里的 `DROP TABLE` 字样;新子用例「丢 route_reconcile 的列」:`ALTER TABLE route_reconcile DROP COLUMN cache_verified_ms` → 迁移失败、错误含该列名 |
| `TestRouteStore_NullPlacementBindsNull`(`:316-325`) | 改(建议) | 裸 SELECT 的 WHERE 改成 `route_shard = ? AND player_id = ?` |
| `TestRouteStore_LockWaitBoundedAfterClientCancel`(`:440-450`) | 改 | 持锁者用 `store.RouteLockRowSQLForTest` 时实参改 `(uint64(placement.RouteRowShard(p)), p)`;p / q 的**围栏**分片关系不变(4001 % 8 == 4009 % 8 == 1),行分片分别是 1 与 9,不影响用例意图 |
| `TestRouteStore_ScanRoutesKeysetCoversAll`(`:692-731`) | 改 | 游标用 `store.RouteScanCursor`,`after = store.RouteScanCursorAt(page[len(page)-1])`;断言:总数 2500、无重复、按 `Less` 有序、16 个片都出现过 |
| `TestRouteStore_ProbeRejectsUnenforcedShardCheck` | 新 | 与 `TestRouteSchema_CheckConstraintsEnforced` 同样的跳过条件(TiDB → Skip;MySQL < 8.0.16 → Skip);迁移后 `ALTER TABLE player_route ALTER CHECK chk_route_shard NOT ENFORCED`(MySQL ≥ 8.0.16 语法)→ `store.NewRouteStore` 返回 `ErrRouteCheckNotEnforced`;`SELECT COUNT(*) FROM player_route WHERE player_id = 0` = 0(探针回滚了) |
| `TestRouteStore_ShardMismatchRowFailsClosed` | 新 | (评审 M1 改写)跳过条件同上。先建 store,再关 CHECK,用裸 SQL 插 `(3, 6, home 7, …)`(之后 `ALTER CHECK … ENFORCED` 会因存量违例失败,所以保持关闭)。断言:① `ScanRoutes(RouteScanCursor{}, 100)` → `ErrRouteShardMismatch`;② `GetRoutes([]uint64{6})` **无错、返回空 map**(点读看不到错片行,这是结构性局限,用例名下注释写明);③ `ImportRoutes([{6, home 7}], false)` 成功且 `Inserted=1` → 此时库里 player 6 有两行,`SELECT COUNT(*) … WHERE player_id = 6` = 2 —— 钉住「CHECK 不生效时唯一性就破了」,也就是 P4 必须 fail-closed 的理由;④ 再 `ScanRoutes` 仍报 `ErrRouteShardMismatch`(错片行 `(3, 6)` 排在 `(6, 6)` 之前) |
| `TestRouteStore_KeyedReadsUsePrimaryRange` | 新 | 先导入 ≥ 2000 行(两批 `ImportRoutes`,让优化器选 range 而不是全表);对下列语句做 `EXPLAIN FORMAT=TRADITIONAL`(占位符按 `player_name_store_integration_test.go:1045-1050` 的办法内联):`RouteSelectRowsSQLForTest(1, false)`(**单个**行构造器,MySQL 的行构造器 range 优化写明要求「多于一个」,单个时是否被改写成等值未核实)、`(3, false)`、`(RouteBatchLimit, true)`(1000 对 + FOR UPDATE:`eq_range_index_dive_limit=200` 之后改用统计估算,`range_optimizer_max_mem_size` 超限会静默退回全表并只给 3170 警告;FOR UPDATE 形态一旦全表扫描,RC 下会逐行加锁再放、与 W1 互等成 1205);`RouteScanSQLForTest` 内联 `(5,5,100001,100)` 与**零游标** `(0,0,0,100)`(覆盖全表,最容易退化成 ALL + filesort)。断言 `key = PRIMARY`、`type ∉ {index, ALL}`,R12 的 `Extra` 不含 `filesort`(否定式判据,同 `player_name_store_integration_test.go:1076-1086` 先例);每份计划 `t.Logf` 原样留档;TiDB 上 Skip |
| `TestRouteToolImportThenAuditOnRealStore`(`routetool_integration_test.go`) | 改 | `:111-112` 的裸 INSERT 补 `route_shard`(9005 → 13);其余断言不变(Batch 2、四个玩家片 9..12,天然跨片) |
| 其它 `TestRouteStore_*` / `TestRouteSchema_*` | 不改 | 只经 store API,形状变化对它们透明 |

### 6.4 真 TiDB 集成(可选,需要本机 `deploy/docker-compose.tidb.yml`)

| 用例 | 内容 |
|---|---|
| `TestRouteStore_TiDBClusteredShardsAndPresplit`(新,放 `route_store_integration_test.go`) | 版本不含 TiDB → Skip;`SELECT @@GLOBAL.tidb_enable_check_constraint` 不是开 → Skip 并提示先 `SET GLOBAL`;用 `storetest.NewEmptyDB` + `store.BootstrapRouteTablesOnlyForTest`(**不能**用 `NewMigratedDB`:全局库迁移在 TiDB 上会因 `player_snapshot` 的可空唯一键失败,`tidb-phase1-and-launch-readiness.md:503`);断言 `TIDB_PK_TYPE` 两表都是 CLUSTERED;`NewRouteStore` 成功(P4 在 TiDB 上拿到 3819 + 约束名);注册 40 名玩家(zone 7)后 `ScanRoutes` 全覆盖;`PrepareTiDB({Zones:[7], Storages:[1000000]})` 成功、`SplitRegions() > 0`;`SHOW TABLE player_route REGIONS` 行数 ≥ 16、`SHOW TABLE player_route INDEX idx_route_home REGIONS` ≥ 16、`SHOW TABLE zone_merge_fence REGIONS` ≥ 2;**再跑一次 `PrepareTiDB` 不报错**(幂等),这一次期间另起 4 个 goroutine 各对 zone 7 的新玩家跑 `RegisterHome`,每个结果(含错误号)只 `t.Logf`、不断言(HR9 取证);`EXPLAIN` R13 的计划只 `t.Logf`,断言不含 `TableFullScan` |

`export_test.go` 新钩子:`BootstrapRouteTablesOnlyForTest(ctx, db *sql.DB, dbName string) error` = 按名排序执行 `routeTableShapes` 那五张的 `bootstrapTables[name]`,再 `ensureRouteTables`。

### 6.5 计数(给复验清单)

- `go/shared/placement` 顶层用例 15 → **16**(新增 `TestRouteRowShard`)。
- `internal/routing` **不变**:54(带 integration 63)—— G1a 不碰 routing。
- `k8s_deploy_contract` 不变:88;`k8s_zone_rollback_flush_guard` 不变:8。
- store / routetool / cmd 的顶层用例数此前没有在文档里钉过,本批也不钉,以「全绿、无意外 SKIP」为准。

---

## 7. 交给 Codex 的验证(全部在仓库根、PowerShell;**插在提案 §11.2「第 1a 批交 Codex 的验证」里执行**,即先复验清单 → 第 0 批 → 第 1a 批第 1–9 步,第 1a 批各步按下面的增量替换)

前置:若本机全局库 `testdb` 曾经跑过 G1a 之前的第 1a 批迁移(`player_route` 主键是 `player_id`),迁移会被形状守卫拒绝。确认这些表里没有要保留的数据后手工 `DROP TABLE player_route, zone_merge_fence, zone_merged_into, route_meta;`(`route_reconcile` 不存在则略)再跑 `-migrate`;**只 DROP `player_route` 不够**(旧 `route_meta` 与旧排序规则守卫查不出来,见 §4.2);不确定时停下来问负责人。一次性库(storetest)不受影响。
另:P4 让所有建 route store 的真库用例要求 MySQL ≥ 8.0.16(更老的版本上是 FAIL 不是 SKIP);跑第 3 步前先 `SELECT VERSION()` 记下版本。

1. **shared**(替换 1a 第 1 步的计数):
   ```powershell
   cd go/shared
   gofmt -l ./placement                      # 无输出
   go vet ./placement/...
   go test ./placement/... -count=1 -v -run "Route|Rev|Publish|MapRev|Keys"
   go test ./placement/... -list . | Measure-Object -Line   # 期望 16 行用例 + 1 行 ok
   go test ./... -count=1
   ```
   通过:全绿;`TestRouteRowShard` PASS。
2. **data_service 单测**(1a 第 2 步,`-run` 正则加 `TiDBPrepare|ScanCursor|RouteRowShard|ShardMismatch`):
   ```powershell
   cd go/data_service
   gofmt -l ./internal ./cmd ./data_service.go   # 无输出
   go vet ./...
   go build ./...
   go test ./... -count=1
   go test ./internal/store/... ./internal/routetool/... ./cmd/route_tool/... -count=1 -v -run "Route|TiDBPrepare|ScanCursor|DecideImport|RegisterHome|ImportRoutes|NewRouteStore|ShardMismatch"
   go test ./internal/routing/... -list . | Measure-Object -Line   # 仍是 54 行用例 + 1 行 ok
   ```
   通过:全绿;§6.1、§6.2 列出的用例全部 PASS。
3. **MySQL 集成**(1a 第 3 步,口令取 `deploy/docker-compose.yml` 的 `MYSQL_ROOT_PASSWORD`,不写进日志):
   ```powershell
   $env:DATA_SERVICE_IT_MYSQL_HOST = '127.0.0.1:3306'
   $env:DATA_SERVICE_IT_MYSQL_USER = 'root'
   $env:DATA_SERVICE_IT_MYSQL_PASSWORD = '<见 compose>'
   go test -tags=integration ./internal/store/... -count=1 -v -run "Route|MigrateSchema"
   go test -tags=integration ./internal/routetool/... -count=1 -v
   ```
   通过:无 FAIL;`TestRouteStore_KeyedReadsUsePrimaryRange` 打印的两份 EXPLAIN 计划**原样保留**(若 `type=ALL`,如实回报,不要改 SQL);`TestRouteStore_TiDBClusteredShardsAndPresplit` 在 MySQL 上 SKIP 属预期;`TestMigrateSchema_UpgradesLegacyHandWrittenTables` 是 09-29 起的既有红,单列。
4. **Redis 集成**:按 1a 第 4 步原样跑(G1a 不改 routing);`TestDBFirstRegister_RealStores` 必须 PASS(它会经过 P4)。
5. **全量集成**:`go test -tags=integration ./... -count=1`(1a 第 5 步)。
6. **PowerShell**:
   ```powershell
   pwsh -NoProfile -Command "foreach(`$f in 'tools/scripts/stress_round19.ps1','tools/scripts/currency_crash_window.ps1'){`$e=`$null;[void][System.Management.Automation.Language.Parser]::ParseFile((Resolve-Path `$f),[ref]`$null,[ref]`$e);if(`$e){`$e;exit 1}}"
   pwsh -NoProfile -File tools/scripts/stress_round19.ps1 -Phase wipe   # 不带 -Execute,只打 plan
   ```
   通过:语法零错误;plan 出现三张数据表的 TRUNCATE、`route_meta(id=1)` 只复位 `writable`、`route_reconcile(id=1)` 复位那一行。`k8s_deploy_contract` 与 `k8s_zone_rollback_flush_guard` 不受影响,可按复验清单照常跑(88 / 8)。
7. **route_tool 冒烟(本机 MySQL)**:
   ```powershell
   cd go/data_service
   # 不用 go run 断言退出码:go run 在子进程非 0 退出时自己以 1 退出(只在 stderr 打 "exit status 2"),判不出 2。
   go build -o "$env:TEMP\route_tool_g1a.exe" ./cmd/route_tool
   & "$env:TEMP\route_tool_g1a.exe" -f etc/data_service.yaml tidb-prepare -zones=1,2; "exit=$LASTEXITCODE"   # 期望 exit=0,stdout 含 engine=mysql skipped=true
   & "$env:TEMP\route_tool_g1a.exe" -f etc/data_service.yaml tidb-prepare; "exit=$LASTEXITCODE"            # 期望 exit=2,stderr 含 -zones
   Remove-Item "$env:TEMP\route_tool_g1a.exe"
   ```
   (前提:本机全局库已 `-migrate`,且 P4 在本机 MySQL 上通过 —— 这本身就是一条证据,结果原样回报。)
8. **可选:真 TiDB**(拉取 `pingcap/*:v8.5.2` 镜像属于下载,先征得用户同意;只在本机 dev TiDB 上做):
   ```powershell
   docker compose -f deploy/docker-compose.tidb.yml up -d
   # 用 root(dev 无口令)建一个测试账号,口令随机生成、不进日志不进 git:
   mysql -h 127.0.0.1 -P 4000 -u root -e "SET GLOBAL tidb_enable_check_constraint=ON; CREATE USER IF NOT EXISTS 'ds_it'@'%' IDENTIFIED BY '<随机>'; GRANT ALL ON *.* TO 'ds_it'@'%';"
   $env:DATA_SERVICE_IT_MYSQL_HOST = '127.0.0.1:4000'
   $env:DATA_SERVICE_IT_MYSQL_USER = 'ds_it'
   $env:DATA_SERVICE_IT_MYSQL_PASSWORD = '<随机>'
   go test -tags=integration ./internal/store/... -count=1 -v -run "TiDB"
   ```
   通过:`TestRouteStore_TiDBClusteredShardsAndPresplit` PASS(不能 SKIP);回报每条 SPLIT 的 `TOTAL_SPLIT_REGION`、三次 `SHOW TABLE … REGIONS` 的行数、二次 `PrepareTiDB` 是否报错、二次期间并发 `RegisterHome` 的结果(有无 8028)、R13 的 EXPLAIN、P4 在 TiDB 上的原始报错文本。**不要**对 TiDB 跑 `-run "Route"` 全集(全局库迁移在 TiDB 上必然失败,见 §6.4)。做完 `docker compose -f deploy/docker-compose.tidb.yml down`(数据卷是否保留由用户定)。
9. **不需要重编 C++**:无 proto 变更。
10. 若实现者改了提案 H1:`python tools/scripts/gen_docs_index.py --check`(工作区里别人的条目可能让它报过期,只看本批涉及的行)。

失败处理:不重试、不改断言;保留首个 FAIL 前后 50 行与完整命令输出,按复验清单第 1 节第 5 条归因(只在当前检出失败的,先归到第 0 批 / 第 1a 批 / G1a)。

---

## 8. 文档改动(实现阶段照写)

### 8.1 提案 `player-route-mapping-durability-proposal.md`

1. §3.1「TiDB 适配」(`:61-64`)第一条后加注:「(2026-10-12 G1a 起 `player_route` 改为聚簇 `(route_shard, player_id)`、不再用 SHARD_ROW_ID_BITS,见 §11.2「第 1a 批热点修复」)」;第三条补「TiDB 还要建表前打开 `tidb_enable_check_constraint`,且 `chk_route_shard` 关系到 `player_id` 唯一性,route store 探针会拒绝 CHECK 不生效的库」(顺带落实 `tidb-phase1-and-launch-readiness.md:1003` 的连带项)。
2. §3.2 DDL 块(`:68-125`)整段换成 §4.2 的五张表;块后补三段短说明:「为什么 route_shard」「为什么拆 route_meta」「排序规则」,各 2–4 句,指向 §11.2 G1a 段。`"真的没有记录"` 与「为什么拆 rev」两段不动;「围栏为什么分片」末尾补「TiDB 上另按 zone 预切(`route_tool tidb-prepare`)」。
3. §11.2 进度表(`:415-420`)第 1a 批行之后加一行:`| 第 1a 批热点修复(G1a) | 已落码(日期),**未编译、未测试** | 见下「第 1a 批热点修复」 |`。
4. §11.2 在「**Java 版(AGENTS.md §12)**:待做…」(`:756`)之后、`---`(`:758`)之前追加 **「第 1a 批热点修复(G1a,2026-10-12)」**,内容:
   - 起因一句(1a 风险第 1 条 `:628-631`、launch_verdict `:24-25`、`tidb-phase1-and-launch-readiness.md` §5.1);
   - 改动清单(本规格 §3.1,精简成表);
   - 新表形状与「读方怎么用、游标怎么改」表(本规格 §2 D-2 表 + §4.12);
   - 对 §5.4 / 第 1b 批的约束:leader 租约、`watermark_ms`、`cache_verified_ms` 读写改在 `route_reconcile`;`writable`、`generation` 留在 `route_meta`;冻结写在缓存表上最长阻塞一个租约;增量推送按片循环;全量预热即 `ScanRoutes`;`route_tool tidb-prepare` 要随第 1b 批并进主程序入口;
     **回档检测跨两表**(评审补):「水位倒退 → 置 `writable=0`」原来可以是同一行上的条件更新,拆表后要在一个事务里读 `route_reconcile`、写 `route_meta`(或先置 `writable=0` 再处理水位),第 1b 批规格要写明原子范围;leader 标识必须是 ASCII(见 D-6);
     **新的 `player_route` 写者**(第 2 批 merge_zone 等):直接调 `placement.RouteRowShard`(merge_zone 已 import `shared/placement`),并在开写前过与 P4 同等的 CHECK 实测(或复用 `RouteStore`)—— CHECK 不生效的库上,任何绕过探针的写者都能给同一 `player_id` 写出两行,点读看不到(D-1);
   - TiDB 运维步骤(建表前全局变量、`tidb-prepare` 的时机:Phase 1 预建表之后、开新 zone 时、BR/PITR 恢复后、TRUNCATE 后;`-storages 1000000` 何时加;可选 `ALTER TABLE route_meta CACHE`);
   - 相对本提案与第 1a 批的偏离(本规格 §9,编号 H1–H9);
   - 风险(本规格 §10,编号 HR1–HR9);
   - 验证增量(本规格 §7);计数更正:「第 1a 批验证第 1 步 placement 15 个 → 16 个;routing 54 / 63 不变」;
   - 「已知未改」:§2 总览表「4 张表」与 `:44`「`route_meta.watermark_ms` 回退」、§4.3 W10 / §5.2 R7 正文、§5.4 正文、§8.2 `:337`「`route_meta.watermark_ms` / `generation` / `writable`」仍按旧口径叙述,以本段为准(水位在 `route_reconcile`);AGENTS.md §6.2(Q8)仍未改;
   - Java 状态(本规格 §12)。
5. §12(`:760-792`):
   - `:775` 那条「TiDB 悲观事务更慢,另有 player_route 主键单调尾部、zone_merge_fence 整表一个 Region、route_meta 单行读热点」改写为:G1a 之后,主键与三条二级索引各 16 条尾部(`idx_route_home` 每 zone 16 条),`tidb-prepare` 预切后落在不同 Region;围栏每 zone 一个 Region;`route_meta` 只剩极少写的开关行,可选设缓存表;剩下的上限是「每 zone 围栏分片行锁」与 TiDB 悲观事务时长;
   - 新增小节 **「万一目录还是成了瓶颈」**:先看哪些信号(`login_create_player_stage_seconds` 的 register 段 p99、`data_service_route_register_total{outcome="store_error"}` 占比、日志里的 lockRetries / 1205、PD 热点里 `player_route` / `zone_merge_fence` 的 Region)→ 按代价从低到高的梯子:① 调大 `RouteStore.FenceShards`(只改配置,上限 64 已是持有者契约,`route.go:37-40`);② `ALTER TABLE route_meta CACHE`;③ 围栏改按 `(zone, shard)` 预切(只改 `tidb-prepare` 的语句生成);④ 加 TiKV / tidb-server(打散之后才线性);⑤ W1 的围栏复核改为不持锁的版本比对(launch_verdict `:176` 的建议,需要单独设计);⑥ 全球多地域时目录单写地域 + 区域薄层(Phase 1 文档 §8)。MySQL 档:目录与 zone 库同一实例(提案 `:286` 引 `k8s_deploy.ps1:3443`、`:3511-3512`),先把全局库拆到独立实例,G1a 在 MySQL 上不改变容量;读路径始终是 Redis 副本,不读库(`:768`)。所有数字仍是推算,以 L-12 / L-14 压测为准;
   - 「开服前要做的」(`:788-792`)第 2 条改为「TiDB 上跑 `route_tool tidb-prepare -zones <全部>`,并把路由表热点纳入压测」。

### 8.2 `tidb-phase1-and-launch-readiness.md`(别人在途的提案,只做带「(2026-10-12 G1a)」标记的增量,不重写原文)

| 位置 | 增量 |
|---|---|
| §0 / §1.3 表 `:63`、`:64` | `:63` 末尾补「G1a 后 D1 四条索引都带 16 片前缀,预切后不再是单尾,见 §5.1」;`:64` 补「`route_meta` 已拆出常写的 `route_reconcile`,开关行可设缓存表」 |
| §2.2 时钟 `:134` | `route_meta.watermark_ms` → `route_reconcile.watermark_ms` |
| §2.5 `:196` | 「D1 `player_route` 的 5 条 CHECK」→ 6 条,并写明 `chk_route_shard` 关系到 `player_id` 唯一性,route store 建池探针 P4 实测它生效,不生效即拒绝 |
| §3.2 第 6 步 `:340` | 「预切与核对」补:`route_tool -f <yaml> tidb-prepare -zones <全部 zone> [-storages 1000000] -dry-run=false`;它会先核对两张表 `TIDB_PK_TYPE=CLUSTERED`,不是就停 |
| §3.2 第 7 步表 `:347` | `route_meta` 行旁加 `route_reconcile`(migrate 种 id=1,同样导入前 DELETE、导入源库的行) |
| §3.2 第 16 步 `:406-409` | `writable` 在 `route_meta`;`watermark_ms` 在 `route_reconcile` |
| §3.5 TRUNCATE 坑 `:484-487` | 补「D1 路由表 TRUNCATE 后重跑 `route_tool tidb-prepare`」 |
| §5.1 表 `:683`、`:685`,`:688` | `player_route` 行改为「G1a:聚簇 (route_shard, player_id),16 片前缀;PK / updated 各 16 尾,home 每 zone 16 尾,storage 每值 16 尾;预切后分散」;单行热点行改为「`zone_merge_fence` 按 zone 预切;`route_meta` 读多写极少,可选缓存表;常写的 leader / 水位在 `route_reconcile`(1 个写者)」;`:688` 后加一句 G1a 更正 |
| §5.2 `:691-702` | 新增第 7 条「D1 路由表:`route_tool tidb-prepare`(做什么、何时跑、幂等、MySQL 自动跳过)」,并在第 5 条「新建表、TRUNCATE、BR/PITR 恢复之后重跑」里点名它 |
| 新增「开新 zone 的 TiDB 步骤」(放 §5.2 第 7 条下) | ① 在 zone 对外开放(第一次建角)之前跑 `route_tool -f <yaml> tidb-prepare -zones <新 zone> -dry-run=false`(也可带上全部 zone,幂等);② 核对 `SHOW TABLE zone_merge_fence REGIONS`、`SHOW TABLE player_route INDEX idx_route_home REGIONS` 里出现以新 zone 开头的 Region;③ MySQL 档跳过(工具打印 skipped、退出 0);④ 第 1b 批之前 K8s 上 route_tool 不在镜像里,TiDB Phase 1 本来就排在 D1 第 1 批之后(`:22`) |
| §6.2「恢复后必做」`:751` | ② 补「含 `route_tool tidb-prepare`;若 `route_meta` 设过 CACHE,一并重放」;⑤ 的 `watermark_ms` 改指 `route_reconcile` |
| §7 L-12 `:823` | 「评估去掉单调的 idx_route_updated」改为「G1a 已改为 (route_shard, updated_at_ms, player_id) 并保留(1b 要用)」;验证列加「PD 热点里路由表 Region」 |
| §9.3 | 新增一项「D1 `route_meta` 是否设为 TiDB 缓存表(默认不设,看压测)」 |
| §9.4「预切」 | 追加:组合聚簇主键与多列索引的 `SPLIT … BY` 元组写法、重复 SPLIT 是否幂等、`ATTRIBUTES` 是否覆盖索引 Region、SPLIT / ATTRIBUTES 所需权限、`TIDB_PK_TYPE` 列、带 NULL 的索引值能否作为切分点、缓存表的写阻塞与事务内读、TiDB 的 3819 报错文案、行构造器 IN 是否走 Batch_Point_Get;(评审补)SPLIT 语句是否自动打散、`tidb_scatter_region`(launch_verdict `:38` 建议 global)对 SPLIT 是否起作用;`ALTER TABLE … ATTRIBUTES` 是否推进 schema 版本、在途悲观事务是否因此 8028;运行期把 `tidb_enable_check_constraint` 改回 OFF 是否停止执行已建表上的约束 |

### 8.3 `docs/handoff/placement-reverify-checklist-20261012.md`

在 `:243`(「本清单跑完之后,按提案…继续」)之后追加一段 **「4 附录补充:第 1a 批热点修复(G1a)之后的期望(2026-10-12,追加;上面的段落不改)」**:
- 第 2 步:`go/shared/placement` 顶层用例 15 → **16**,新增 `TestRouteRowShard`;
- 第 4 步:routing 54 / 63 **不变**;全量 `go test ./...` 多出 G1a 的不连库单测(store 契约、routetool、cmd/route_tool),红了归 G1a;
- 4c:迁移现在建**五**张路由表(多 `route_reconcile`),种两行;`player_route` 主键是 `(route_shard, player_id)`;本机 `testdb` 若跑过 G1a 之前的 1a 迁移会被形状守卫拒绝,处理见提案 §11.2 G1a 段;
- 第 9 步:`k8s_deploy_contract` 仍 88、`k8s_zone_rollback_flush_guard` 仍 8。

### 8.4 `docs/ops/merge-zone-runbook.md` §9.3

- `:783-788` 的恢复 SQL 改为:
  ```sql
  USE <SnapshotMySQL.DBName>;
  TRUNCATE TABLE player_route;
  TRUNCATE TABLE zone_merged_into;
  UPDATE route_meta SET writable=1 WHERE id=1;
  UPDATE route_reconcile SET leader=NULL, lease_until_ms=NULL, watermark_ms=0, cache_verified_ms=0 WHERE id=1;
  ```
  并加一句:TiDB 上 TRUNCATE 会丢预切与 table attributes,清完重跑 `route_tool tidb-prepare`(或改用分批 DELETE)。
- §9.3 末尾加小节「`tidb-prepare`(TiDB-only)」:用法、何时跑、退出码、dry-run 默认、MySQL 自动跳过、权限不足时用运维账号的 yaml 副本(不进 git);**在线重跑(开新 zone)放在该 zone 开放前、低峰**:两条 `ALTER … ATTRIBUTES` 是 DDL,可能让在途建角以 8028 失败一次(HR9);它会经过 P4,CHECK 不生效的库上直接退出码 2。

### 8.5 `global-data-layer-tidb-decision.md`

`:123`、`:210`(§4 #11 处置列)、`:230`(§6 #8)各追加一句:「2026-10-12 G1a:`player_route` 改为聚簇 `(route_shard, player_id)` + 16 片前缀索引,`route_meta` 拆出 `route_reconcile`,`player_route` 与 `zone_merge_fence` 的预切与防合并由 `route_tool tidb-prepare` 执行,见提案 §11.2」。

---

## 9. 与现有设计的偏离与代价(编号 H*,写进提案 G1a 段)

| # | 偏离 | 理由 | 代价 |
|---|---|---|---|
| H1 | `player_route` 主键 `player_id NONCLUSTERED + SHARD` → 聚簇 `(route_shard, player_id)`(提案 `:80`、`:62`) | 消掉主键唯一索引的单调尾部;TiDB 建表后不能再改 | `player_id` 唯一性改由 PK + CHECK 共同保证,多一个建池探针;所有按 id 的 SQL 都要带片;MySQL 上插入点从 1 变 16(无可见影响,推断) |
| H2 | `RouteRowShards = 16` 写死、不可配 | 改它 = 整表重写 | 以后想要 32 片只能新表迁移 |
| H3 | 二级索引全部带片前缀,`idx_route_updated` 加 `player_id` 尾列(提案 `:81-83`) | 每条索引 16 尾;键集游标有确定的平局决胜 | 按 zone / storage 枚举的游标从 `player_id` 变成 `(shard, id)`,结果不再按 `player_id` 排序 |
| H4 | `route_meta` 拆成 `route_meta` + `route_reconcile`(提案 `:116-124`) | TiDB 上读不被 leader 写锁拖住;开关表才可能设缓存表 | 五张表;1b 读写两张;清库脚本与 runbook 要改;`mapping:route:ready` 里的 `watermark_ms` 来自 `route_reconcile` |
| H5 | 新探针 P4:建池时在回滚事务里做一次必失败的 INSERT | `player_id` 唯一性依赖 CHECK,元数据查询在两种引擎上口径不一,实测最可靠 | 每次建池多 3 条语句;探针本身是一次写尝试(必回滚),只读用途(`route_tool audit`)也要 INSERT 权限;真库套件要求 MySQL ≥ 8.0.16 |
| H6 | `ScanRoutes` 的游标与顺序改为聚簇键(`routeDB` 接口签名变) | 键集翻页必须沿聚簇键 | routetool 假实现与 audit 同改;audit 明细不再按 `player_id` 升序出现;**审计窗口内新提交、且缓存三键全无的行**(db_first 下已提交未发布)若落在已扫过的片里,本次方向 A 看不到、方向 B 也不会查它,要到下一次 audit 才报 —— 旧形状下 id 单调、这类行几乎都在游标之后(多 login 号段交错时同样会漏,只是更少) |
| H7 | 新增 `route_tool tidb-prepare`(提案 §7 与 1a 偏离第 8 条之外的新入口) | 预切要真实 zone 清单,迁移拿不到;运维可重放 | 多一个要记得跑的步骤;K8s 上依赖第 1b 批把 route_tool 并进主程序;漏跑只影响性能,不影响正确性 |
| H8 | 对 `player_route` 也预切(任务只点了围栏) | 不预切,片前缀在首日不生效(F3) | Region 数 ≈ 15 + 15 + 16·Z + 16·S + Z,空 Region 靠 `merge_option=deny` 永久留住;Z 上百时 `idx_route_home` 的空 Region 以千计,届时可把语句生成改成每 zone 只切 `(z,0,0)`(只改 `route_tidb.go`,不动 DDL),由压测决定 |
| H9 | 顺带显式排序规则(`utf8mb4_bin` / `ascii_bin`) | 两种引擎默认不同(F13),建表前改最便宜 | 与 MySQL 现状 `utf8mb4_0900_ai_ci` 不同;这些列都是标识符,逐字节比较是正确语义;可拆出(§11 #4) |

---

## 10. 风险(编号 HR*)

1. **HR1 TiDB 细节大量未核实**(本会话不能联网):SPLIT 元组写法、幂等、ATTRIBUTES 覆盖面、权限、`TIDB_PK_TYPE`、缓存表语义、TiDB 的 3819 文案、行构造器 IN 计划。缓解:§6.4 的 TiDB 用例与 §7 第 8 步;任何一项不成立,先改 `route_tidb.go` 的语句生成或探针判据,DDL 不需要动。
2. **HR2 CHECK 是唯一性的一部分**:P4 只在建池时证一次;运行期有人 `ALTER … NOT ENFORCED` 或 DROP CHECK,P4 要到下次重启才发现。缓解:**只有全表键集扫描**(`ScanRoutes` = audit 方向 A / 1b 全量预热)的片校验能发现「已经写坏」的行,点读与导入看不到它(D-1),所以 CHECK 关闭期间写进来的错片行要等下一次 audit 才暴露;runbook 写明不许关这条 CHECK(TiDB 上也不许把 `tidb_enable_check_constraint` 改回 OFF),并把 audit 列为改过 DDL / 全局变量之后的必跑项。
3. **HR3 存量 snowflake 号偏向片 0**:bwmarrin 布局最低 9 位是同毫秒序号,低速铸号时多为 0(AGENTS §7.1),`% 16` 几乎都落片 0。只影响一次性导入与冷读的分布,不影响新号(号段稠密、均匀);导入写片 0 的尾部是批量、一次性的。
4. **HR4 本机旧形状表**:谁在 G1a 之前跑过第 1a 批迁移,`testdb` 里的 `player_route` 会让迁移失败(fail-closed)。处理见 §7 前置;代码不自动 DROP。
5. **HR5 `idx_route_storage` 的 NULL 段没切**:`PinOnCreate=false` 时新号 storage 为 NULL,这 16 条尾部仍在一个 Region。打开 `PinOnCreate` 后按 `-storages 1000000` 切;NULL 段要等确认 `SPLIT … BY (NULL, …)` 的写法。
6. **HR6 围栏仍是每 zone 一个 Region**:单 zone 开服尖峰时,该 zone 的全部 W1 在一个 Region leader 上加锁 / 提交(悲观锁 key 在提交时作为 Lock 写入参与 2PC,推断)。缓解梯子见提案 §12 新小节第 ③ 步。
7. **HR7 TRUNCATE / BR / TiCDC 丢预切与属性**:两份清库脚本只连本机 MySQL,不受影响;TiDB 环境必须在 runbook 的每个恢复 / 清库步骤后重跑 `tidb-prepare`。漏跑 = 回到单尾,只有性能后果。
8. **HR8 叠在未验证的 1a 上**:G1a 改的就是 1a 未编译的代码,两者一起交 Codex,任何红都要区分归属;1a 自己的风险(提案 `:626-639`)全部仍在,G1a 只处理第 1 条里的 TiDB 热点部分,MySQL 单机连接预算(L-03)与 Redis 单主(L-06)不在本批。
9. **HR9 在线跑 `tidb-prepare` 的 DDL 副作用**(评审补):`ALTER TABLE … ATTRIBUTES` 是 DDL,TiDB 上可能让在途的 W1 悲观事务以 8028 失败一次【未核实】,W1 不重试 8028 → 那一次建角 Unavailable。缓解:开新 zone 时在该 zone 开放前、低峰执行;§6.4 TiDB 用例里二次 `PrepareTiDB` 期间并发跑几次 `RegisterHome`,把是否出现 8028 原样记录(不断言);若实测确有,再评估「属性已在就跳过 ALTER」(读 `INFORMATION_SCHEMA.ATTRIBUTES`)或给 W1 加 8028 重试。

---

## 11. 待负责人拍板

1. **片数 16 与 `% 16`**:一旦有任何环境建表就不可改;不做哈希混洗,接受存量 snowflake 号偏向片 0(HR3)。
2. **`route_meta` 设不设 TiDB 缓存表**:本规格默认不设,只在压测显示开关行读热时打开(D-3)。
3. **P4 用「回滚事务里的一次必失败 INSERT」证明 CHECK 生效**(H5),还是改成查元数据(MySQL `information_schema.TABLE_CONSTRAINTS.ENFORCED` / TiDB 全局变量;两种引擎口径不一,未核实)。
4. **排序规则显式化(H9)随本批做,还是拆出**。
5. **预切由 route_tool 显式执行**(H7),还是以后改成 TiDB 上迁移自动做(需要另给迁移一份 zone 清单的来源)。
6. **`idx_route_storage` NULL 段**:等核实写法再切,还是接受到 `PinOnCreate` 之前都单尾(HR5)。
7. **`idx_route_updated` 保留**(1b 要用;以后补索引要写收敛代码)—— 本规格按保留做,回应 L-12 的「评估去掉」。

---

## 12. Java 版(AGENTS.md §12)

- 本项是服务端内部存储的表形状,**不涉及客户端契约**(帧、客户端可见 proto、tip 码、配表都不变),不需要与 Java 同批改。
- Java 版的映射持久化(D1)整体仍是「待做」(提案 `:756`;Java `PARITY.md` 无对应条目,本机 `D:\luyuan\xuanming-server-mmo` grep `player_route` / `route_meta` 零命中)。登记时机按 Q14:第 1 批(1a + 1b)全部完成时在 `PARITY.md` 登记一行。
- 给 Java 的建议(写进提案 G1a 段的 Java 一行):Java 做等价的「库为真源」目录时,若后端是 TiDB,同样避免以单调 `player_id` 作主键 / 唯一索引的唯一前缀(按本仓同口径的 16 片前缀,或 Java 选型的等价方案),并在 `PARITY.md` 写明取舍。

---

## 13. 实现顺序建议

1. `placement`(常量 + 测试)→ 2. `schema.go` DDL / 形状 / `ensureRouteTables` → 3. `route_store.go` SQL、扫描、探针、导入键序 → 4. `route_tidb.go` → 5. store 三份测试 + `export_test.go` → 6. routetool(接口、auditor、tidb_prepare、测试)→ 7. `cmd/route_tool` → 8. 文案注释(`data_service.go`、`config.go`、yaml)→ 9. 两份脚本 → 10. 五份文档。
每改一个 Go 文件都核对:import 齐全无多余(`route_store.go` 新用到 `sort`;`route_tidb.go` 用 `context`、`database/sql`、`fmt`、`sort`(或 `slices`)、`strconv`、`strings`、`shared/placement`,`errors` 视哨兵定义位置而定;`cmd/route_tool/main.go` 新用到 `strconv`、`strings`);被调方签名以本规格 §4 为准;注释讲「为什么」;口令与 DSN 不进日志;`player_id` 不做指标 label(本批不新增指标)。

---

## 评审处理记录(对抗式评审,2026-10-12;只读代码 + 改本规格,未改仓库任何文件)

逐条回到工作区代码核对过的引用:`route_store.go`(`:37`、`:52`、`:64`、`:94-105`、`:113`、`:309`、`:539`、`:649`、`:950`、`:1175`、`:1367`、`:1410`、`:1462`)、`schema.go`(`:156-235`、`:244-312`、`:740`、`:1351`、`:1583-1624`、`:1867`)、`mysql.go` 两个 DSN、`route_store_contract_test.go`(假驱动、`mysqlLikeConvert`、`wantRegisterArgs`、导入 / 读 / 探针用例)、`route_store_test.go`(DDL 契约、写入点守卫正则)、`route_store_integration_test.go`、`routetool/{importer,auditor,routetool_test,routetool_integration_test}.go`、`cmd/route_tool/{main,main_test}.go`、两份清库脚本、`k8s_deploy.ps1:3484-3486`、提案 / Phase 1 文档 / runbook / handoff 的行号。F1–F18 与 §4.3 的 SQL 改写、实参个数、§6.2 的导入实参向量(10..50 的片 10/4/14/8/2)、`9005 % 16 = 13`、`4001/4009` 的两种片、§6.2 ⑤ 的「step 6/8」都核对无误。

### 必须改(已吸收)

| # | 问题 | 依据 | 处理 |
|---|---|---|---|
| M1 | 规格声称错片行在 R8 / R13(`GetRoutes` / `ImportRoutes`)上也报 `ErrRouteShardMismatch`、「不当缺席」—— 不成立。R8 / R13 按「正确片 + id」点查,库里的 `(3, 6)` 对它们不可见,`GetRoutes([6])` 回缺席,导入会再插一条 `(6, 6)`。§6.3 `TestRouteStore_ShardMismatchRowFailsClosed` 断言 `GetRoutes([6]) → ErrRouteShardMismatch`,真库上**必然失败**;§5 错误语义表(对外契约)写错 | `loadRoutes` 只按入参拼 IN;改写后的 R8 头就是 `(route_shard, player_id) IN ((K_i, id_i))` | D-1、§4.5、§5、HR2 改为「只有 R12 能发现,R8 / R13 的校验是纵深防御」;§6.3 用例改为断言 `GetRoutes` 空 map 无错、导入插出第二行(`COUNT=2`)、`ScanRoutes` 报错,把「为什么 P4 必须 fail-closed」钉成可执行证据;§6.2 的 R8ro 不一致子用例保留但注明是纵深防御 |

### 重要(已吸收)

| # | 问题 | 依据 | 处理 |
|---|---|---|---|
| M2 | G1a 提示永远走不到:规格只在「主键不符」分支拼提示,但真实旧表**没有 `route_shard` 列**,`assertRouteTableShapes` 先查列,在列检查就以通用文案返回。另:提示只叫人 DROP `player_route`,而守卫查不出旧 `route_meta`(多四列)与旧排序规则,只 DROP 一张会留下守卫全绿的混合态 | `schema.go:296-311`(先 `assertColumnNamesPresent` 后主键)、`:1867-1900`(只查存在) | §4.2 改为表级 hint、两处错误都包;提示与 §7 前置都要求 DROP 全部路由表;§6.3 新增「真实旧形状」子用例(用旧 DDL 原文重建) |
| M3 | 契约测试区分不了围栏片与行片:`contractPlayer = 4242`,`4242 % 8 == 4242 % 16 == 2`,`wantRegisterArgs` 里现成的变量 `shard` 就是围栏片。实现若把围栏片绑进 R4 / R5 / R6,§6.2 全绿,上线后一半新号 R4 撞 3819 建角失败 | `route_store_contract_test.go:421`、`:430`;`route_store.go:654` 局部变量叫 `shard` | §4.4 要求改名 `fenceShard`、`rowShard` 显式传;§6.2 改 `wantRegisterArgs` 并新增 `TestRegisterHomeBindsRowShardNotFenceShard`(4250:围栏片 2、行片 10) |
| I1 | §7 第 7 步用 `go run` 断言退出码 2:`go run` 对子进程非 0 退出一律自己退 1,Codex 会报假红 | cmd/go `base.RunStdin` → `Errorf` | 改为先 `go build` 再运行、读 `$LASTEXITCODE` |
| I2 | EXPLAIN 用例只测 3 对与游标 (5,5):单个行构造器(MySQL 行构造器 range 优化要求「多于一个」)、1000 对 + FOR UPDATE(dive limit 200、`range_optimizer_max_mem_size` 超限静默退全表)、零游标(覆盖全表)都没测;FOR UPDATE 一旦全表扫描,RC 下逐行加锁再放、与 W1 互等成 1205 | MySQL 行构造器 range 优化与内存上限的手册条件(推断,由用例实测) | §6.3 `TestRouteStore_KeyedReadsUsePrimaryRange` 扩到 5 条语句、计划全部留档 |
| I3 | `ALTER … ATTRIBUTES` 是 DDL,开新 zone 在线重跑可能让在途 W1 以 8028 失败(W1 只重试 1213 / 9007);规格完全没提 | TiDB DDL 推进 schema 版本(未核实) | 新增 HR9;§4.8、runbook 小节写明开放前 / 低峰执行;§6.4 / §7 第 8 步取证;§9.4 待核实项补上 |
| I4 | 第 1b 批约束漏两条:回档检测「水位倒退 → 置 writable=0」拆表后跨两表,原子范围要重定;第 2 批新写者(merge_zone)必须调 `RouteRowShard` 并过 P4 同等探针,否则 CHECK 不生效时照样写出同 id 两行 | D-3 拆表;D-1 唯一性依赖 CHECK;`tools/merge_zone/placement_codec.go` 已 import `shared/placement` | §8.1 第 4 条补两条约束;§4.1 注释改为「直接调,不镜像」 |
| I5 | P4 让整套 route store 真库用例(含 routing 的 `TestDBFirstRegister_RealStores`)在 MySQL < 8.0.16 上 FAIL 而非 SKIP;新增的两条 CHECK 相关用例没写跳过条件 | `route_store_integration_test.go:165-173` 的先例 | §4.6 补充、§6.3 两条用例补跳过条件、§7 前置记版本 |

### 小问题(已吸收)

1. R12 实参写成 `uint64(after.Shard)`,与 §4.3「K 一律绑 uint64」同口径(假驱动反正转成 uint64,真驱动也接受,只为一致)。
2. `ScanRoutes` 对 `after.Shard ≥ 16` 回 `ErrRouteInvalidArgument`,免得静默空页被 audit 当作扫完;§6.2 加一条。
3. SPLIT 结果**零行**也报错(原文只写列数为 0 / 首列解析不了)。
4. import 清单:`route_tidb.go` 的 `errors` 视哨兵位置而定;`main.go` 新增 `strconv`、`strings`。
5. `TestRouteToolTiDBPrepareDoesNotTouchRedis` 用两个不同的关闭端口,否则「stderr 含 MySQL 地址」区分不了 Redis;不写 Host 会回落 `127.0.0.1:3306`(本机可能真有库)。
6. `route_reconcile.leader` 是**改字符集**(utf8mb4 → ascii),不只是补排序规则;D-6 写明并约束 1b 的 leader 标识。
7. 提案「已知未改」补 `:44`(§2)与 `:337`(§8.2)两处 `route_meta.watermark_ms`。
8. `data_service.go:300` 输出保留「row id=1 seeded if absent」并带上 `route_reconcile`。
9. H5 代价补「只读用途也要 INSERT 权限、真库套件要求 MySQL ≥ 8.0.16」;H6 代价补「审计窗口内新提交且未发布的行在已扫过的片里会漏到下一次 audit」;H8 补「Z 上百时空 Region 以千计,可改每 zone 一个切点」。
10. P4 只证明 CHECK 生效、不证明模数是 16(D-1 写明;由 DDL 契约测试 + 表只由本迁移建保证)。
11. P4 的 1062(库里已有 `(1,0)` 错片残行)、1142(无 INSERT 权限)在文案里点名;`routeProbeTimeout` 注释补第 4 步。
12. §9.4 待核实项补:SPLIT 是否自动打散 / `tidb_scatter_region`、运行期把 `tidb_enable_check_constraint` 改回 OFF 的效果;HR2 补「TiDB 上也不许改回 OFF」。

### 核对后不改(附理由)

- **`% 16` 与围栏 `% 8` 相关**:行片 s 只对应围栏片 s % 8,锁序仍是「围栏 → 玩家行」,不成环,也不制造新热点。
- **R12 用 OR 展开而非行比较**:MySQL 对 `(a,b) > (x,y)` 不做 range 优化,原写法正确。
- **R8 FOR UPDATE 的取锁顺序**:range 扫描按区间升序取锁,与 ORDER BY 一致;R9 补列时行已被 R8 锁住,排序只为可读,无害。
- **`TestNoPlayerRouteWritesOutsideRouteStore`**:正则只匹配 `INSERT INTO / UPDATE / DELETE FROM / REPLACE INTO player_route`,`ALTER TABLE` / `SPLIT TABLE` 字面量不命中;P4 在 `route_store.go`,属于允许位置。
- **routetool 现有 audit 用例**:方向 A 的 id 全部 < 16,片 = id,改成按聚簇键排序后明细顺序不变,不需要改断言。
- **探针契约用例的成功分支数**:确是 3 个(MySQL 通过、TiDB 通过、writable=0),规格写对了。
- **MySQL 上 P4 是否取锁**:CHECK 在服务层写行之前求值,违例时不进存储引擎,规格描述正确。
- **Java 版**:纯服务端内部表形状,不涉及客户端契约,§12 的「D1 整体待做」口径正确。
