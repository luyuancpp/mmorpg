# 玩家归属与落点映射持久化:库为真源 + Redis 完整副本 + 就绪标记(已拍板,分批落码:第 0、1a 批已落码,未验证)

> **状态**:**已拍板(2026-10-12,负责人),分批落码中**。2026-10-12 起草;拍板结果与落码进度见文末「§11 拍板与落码记录」。
> **起因**:用户提出「逻辑服 id(home_zone)和 storage_id 应该存到 TiDB,Redis 只做缓存,不然 Redis 丢了映射无法恢复」。这正是 [TiDB 决策](./global-data-layer-tidb-decision.md) D1「映射持久化兜底」与 [player-storage-placement.md](./player-storage-placement.md) §4.4 / §11.4 的遗留项。
> **怎么来的**:多 agent 只读设计。先盘点两份映射的全部读写点与不变量,再并行出三种方案:A 库为真源 + Redis 缓存;B Redis 为真源 + 数据库副本;C 把归属写进玩家主表。之后从「数据一致性」「运维上线」两个视角做对抗评审,最后汇总成本稿。只读代码、grep、git 得出,没有编译、测试或压测。按 AGENTS.md §10.1,实现后交 Codex 验证,在那之前不能说「已验证」。
> **前置**:当前 HEAD 的落点代码先按 [placement-reverify-checklist-20261012.md](../handoff/placement-reverify-checklist-20261012.md) 复验,本方案不叠在未验证的基线上。行号以 2026-10-12 HEAD `73b852f32` 为准。


## 1. 一句话结论

**要落库。但 Redis 不是"未命中就回源"的普通缓存，而是一份带版本号、保证完整、有就绪标记的只读副本。**

对用户提法的四处修正：

1. **现在就落，不等 TiDB。** 放进 data_service 已有的 MySQL 全局库（dev 是 `testdb`，K8s 是 `mmorpg_global`），建表语句按 TiDB 兼容写，TiDB Phase 1 上线时随全局库一起迁过去。
   - 依据：`go/data_service/etc/data_service.yaml:92-98`，`tools/scripts/k8s_deploy.ps1:997`、`:3511-3516`。
   - TiDB Phase 1 本身"是否继续"还没拍板（`docs/design/player-storage-placement.md:374`），映射的风险今天就在。
2. **提交点从 Redis Lua 挪到数据库事务。** 合服围栏、`merged_into`、冻结态要**一起落库**，否则 Redis 一丢，"防回填""围栏互斥"会跟着失效。
3. **能力标记 `db:capability:zone:{z}` 必须留在 Redis、保持临时，不落库。** 落库会替已回退的旧版 go/db 作证，见 `go/db/internal/kafka/placement_route.go:369-379`。
4. **Redis 丢失依然要全服维护。** 同一个实例里还有会话、锁、存盘游标、`PlayerAllData`（`tools/scripts/k8s_deploy.ps1:3460-3465`），映射落库只解决两件事：
   - 映射能自动恢复；
   - 恢复期间不会静默写错库。今天的行为是：scene_manager 把 gate zone 当 home（`go/scene_manager/internal/logic/home_zone.go:144-151`），go/db 走 legacy 分支写进本 zone 库（`go/db/internal/kafka/placement_route.go:171-172`）。

**紧迫度**：今天的实际风险偏低。所有环境都是 `PinOnCreate=false`，从没跑过 pin、relocate，归属与物理库仍一一对应（`player-storage-placement.md:5`）。但两件事必须排在本方案之后：
- 生产上执行 §13 第 4 步（pin 合服、relocate、`PinOnCreate`）；
- 打开 `PinOnCreate` 并指向全局库。这时会出现"无法恢复的新号"缺口（`player-storage-placement.md:116`、`:384`），本方案是硬前置。

---

## 2. 推荐方案总览

以方案 A 为骨架，嫁接 B、C 和两份评审的修补。

| 维度 | 做法 | 来源 |
|---|---|---|
| 真源 | 全局库 4 张表：`player_route`、`zone_merge_fence`、`zone_merged_into`、`route_meta` | A |
| 提交点 | RC 悲观事务，加锁复核（围栏行 → 玩家行），COMMIT 即提交 | A |
| Redis 键格式 | **不改**，仍是 `player:zone:{id}` 和 `player:placement:{id}`；只新增一个版本键 `player:maprev:{id}` | C + 评审 2 |
| 发布 | 提交后用 set-if-newer Lua（按 maprev）写三把键；与 **WAITAOF** 走同一条连接，确认后才推进依赖步骤 | A + 评审 1 修补 |
| "未命中"的判定 | 整份副本级别：全局就绪标记 `mapping:route:ready` 存在 = 副本完整，缺键就是权威缺席；标记不在 = 不可信 | B/C 的标记 + A 的真源 |
| 热路径 go/db | 不连全局库；标记不在时写延后（不耗重试预算）、读照常耗预算 | C 的论证，取代 A 的 RouteDB 只读池 |
| data_service 读 | 标记不在时按主键回源数据库 | A |
| 部分丢失检测 | 对账 leader 每拍比对 Redis 的 `run_id` 和 `master_replid`，变化就先撤标记、再按 `updated_at_ms` 修补、再挂回标记 | B，改成"库为真源"，方向不会反 |
| 库回档检测 | `route_meta.watermark_ms` 回退 → 全体写者只读，从缓存反向修库 | 评审 1 的"库可信闸" |
| 不需要的东西 | 新值格式 `player:route`、MirrorLegacy、P6 停写旧键、负缓存、outbox、guardian、R5a/R5b、行戳 | 见 §8 |

**提交即安全**（全方案的核心纪律，来自 A）：每个数据库事务提交后的状态，被读方提前看到或延后看到都必须安全。依赖步骤（login 落账号 blob、合服开服、搬库 R2/R3）只能在"发布 + WAITAOF 确认"之后开始。

---

## 3. 表结构与存放位置

### 3.1 位置

- **库**：data_service 的 `SnapshotMySQL.DBName`。data_service 已在这里存 `id_segment`、`player_name`（`go/data_service/internal/store/schema.go:20-30`）。
- **建表方式**：
  - 照 `id_segment`、`player_name` 的先例，走手写 DDL，放进 `bootstrapTables`（`schema.go:47-68`、`:153-161`）。原因是需要 VARCHAR 的 run_id、可空列、二级索引和 CHECK，而 proto2mysql 会把 string 渲染成 MEDIUMTEXT（`schema.go:47-49`）。
  - 同时在 `proto/common/database/rollback_database_table.proto` 追加对应 message，加进 `TableMessages()`（`schema.go:1195-1206`），只用来做列漂移守卫。
  - 这份 proto 也编进 C++（`cpp/generated/proto/CMakeLists.txt:81`），所以要按 AGENTS.md §4 重新生成，并重编 C++ 的 proto 库。
- **建表时机**：dev 启动时 `AutoMigrate` 自动建（`data_service.yaml:173`）；prod 由 data-service-migrate Job 建。
- **TiDB 适配**：
  - 沿用 `NONCLUSTERED` 加 `SHARD_ROW_ID_BITS`（`schema.go:65-68`）；（2026-10-12 G1a 起 `player_route` 改为聚簇 `(route_shard, player_id)`、不再用 SHARD_ROW_ID_BITS，见 §11.2「第 1a 批热点修复」）
  - 锁定读以 `tidb_txn_mode=pessimistic` 为前提，同 A15（`player-storage-placement.md:405`、`:428`）；
  - CHECK 只在 MySQL ≥ 8.0.16、TiDB ≥ 7.2 生效（`schema.go` 的 id_segment 注释），应用层校验不能省。TiDB 还要**建表前**打开 `tidb_enable_check_constraint`；且 G1a 之后 `chk_route_shard` 关系到 `player_id` 唯一性，route store 建池探针会拒绝 CHECK 不生效的库。

### 3.2 建表语句（字段类型按 AGENTS.md §4：GUID 与时间戳用 uint64）

> 2026-10-12 G1a(§11.2「第 1a 批热点修复」)起,下面是**五张表**的定稿形状(代码真源是 `go/data_service/internal/store/schema.go` 的 bootstrap DDL;本块只为可读,两者不一致时以代码为准)。改动:`player_route` 主键改聚簇 `(route_shard, player_id)` 并加片前缀索引;`route_meta` 拆出常写的 `route_reconcile`;全部显式排序规则。

```sql
-- 每个玩家一行:home 与落点同行,原「两键要么一起写、要么都不写」(router.go:140-146) 退化成单行原子
CREATE TABLE IF NOT EXISTS `player_route` (
  `route_shard`       tinyint unsigned NOT NULL COMMENT 'pb:10', -- = player_id % 16,应用按 placement.RouteRowShard 算,CHECK 钉住
  `player_id`         bigint unsigned NOT NULL COMMENT 'pb:1',
  `home_zone`         int unsigned    NULL     COMMENT 'pb:2',  -- NULL = 无归属(删号墓碑 / 只有落点)
  `storage_id`        int unsigned    NULL     COMMENT 'pb:3',  -- NULL = 无落点记录
  `placement_version` bigint unsigned NULL     COMMENT 'pb:4',
  `frozen_run_id`     varchar(64) CHARACTER SET ascii COLLATE ascii_bin NULL COMMENT 'pb:5',
  `rev`               bigint unsigned NOT NULL COMMENT 'pb:6',  -- 本行任何列变化 +1;仅供缓存排序
  `updated_by`        varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '' COMMENT 'pb:7',
  `created_at_ms`     bigint unsigned NOT NULL COMMENT 'pb:8',  -- 取库时钟
  `updated_at_ms`     bigint unsigned NOT NULL COMMENT 'pb:9',  -- 取库时钟,对账增量扫描用
  PRIMARY KEY (`route_shard`, `player_id`) /*T![clustered_index] CLUSTERED */,
  KEY `idx_route_home`    (`home_zone`, `route_shard`, `player_id`),
  KEY `idx_route_storage` (`storage_id`, `route_shard`, `player_id`),
  KEY `idx_route_updated` (`route_shard`, `updated_at_ms`, `player_id`),
  CONSTRAINT `chk_route_shard`  CHECK (`route_shard` = `player_id` % 16),  -- player_id 唯一性靠它,见下
  CONSTRAINT `chk_route_home`   CHECK (`home_zone` IS NULL OR `home_zone` BETWEEN 1 AND 999999),
  CONSTRAINT `chk_route_place`  CHECK ((`storage_id` IS NULL) = (`placement_version` IS NULL)),
  CONSTRAINT `chk_route_ver`    CHECK (`placement_version` IS NULL OR `placement_version` >= 1),
  CONSTRAINT `chk_route_frozen` CHECK (`frozen_run_id` IS NULL OR `storage_id` IS NOT NULL),
  CONSTRAINT `chk_route_rev`    CHECK (`rev` >= 1)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='player_route';

-- 合服围栏真源。按 (zone, shard) 存 N 份相同内容:建角只锁一个分片,获取围栏锁全部分片
CREATE TABLE IF NOT EXISTS `zone_merge_fence` (
  `zone_id`       int unsigned NOT NULL,
  `shard`         tinyint unsigned NOT NULL,                  -- 0..N-1,N 由配置给,默认 8
  `run_id`        varchar(64) CHARACTER SET ascii COLLATE ascii_bin NULL,  -- NULL = 未封锁
  `purpose`       varchar(16) CHARACTER SET ascii COLLATE ascii_bin NULL,  -- merge / unmerge
  `src_zone`      int unsigned NULL,
  `dst_zone`      int unsigned NULL,
  `expires_at_ms` bigint unsigned NULL,                       -- 库时钟;生效判据 run_id 非空且未过期
  `holder_json`   varchar(1024) NULL,                         -- 原样存 mergeInProgressValue,排障用
  `fence_epoch`   bigint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`zone_id`, `shard`) /*T![clustered_index] CLUSTERED */,
  CONSTRAINT `chk_fence_pair` CHECK ((`run_id` IS NULL) = (`expires_at_ms` IS NULL))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='zone_merge_fence';

-- 合走标记,永久;只有 unmerge 按 (src, dst, run_id) 删。契约同 tools/merge_zone/merged_into.go:11-20
CREATE TABLE IF NOT EXISTS `zone_merged_into` (
  `src_zone` int unsigned NOT NULL, `dst_zone` int unsigned NOT NULL,
  `run_id` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `merged_at_ms` bigint unsigned NOT NULL,
  PRIMARY KEY (`src_zone`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='zone_merged_into';

-- 开关表,单行(id=1):库世代、可写开关。很少写(1b 的冻结 / 回档恢复),每次 W1 普通读 writable
CREATE TABLE IF NOT EXISTS `route_meta` (
  `id` tinyint unsigned NOT NULL,
  `generation` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,  -- 建表时随机生成;回档恢复流程换新
  `writable` tinyint unsigned NOT NULL DEFAULT 1,         -- 0 = 全体映射写者拒写
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='route_meta';

-- 对账表,单行(id=1):1b 对账 leader 的租约与水位,每拍写;第 1a 批只由迁移种行
CREATE TABLE IF NOT EXISTS `route_reconcile` (
  `id` tinyint unsigned NOT NULL,
  `leader` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NULL,  -- 与 updated_by 同源的 ASCII 标识
  `lease_until_ms` bigint unsigned NULL,
  `watermark_ms` bigint unsigned NOT NULL DEFAULT 0,      -- leader 每拍写库时钟,用来发现回档
  `cache_verified_ms` bigint unsigned NOT NULL DEFAULT 0, -- 最近一次确认 Redis 身份未变的时刻
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin COMMENT='route_reconcile';
```

**为什么 route_shard(G1a)**:号段发的 `player_id` 在每个 login 段内严格递增,只按 `player_id` 排序的主键 / 索引在 TiDB 上开服首日始终只写同一个尾部 Region(首日数据量到不了按大小分裂的阈值)。聚簇 `(route_shard, player_id)` 加片前缀索引,把每条索引拆成 16 条尾部,再由 `route_tool tidb-prepare` 预切到不同 Region。代价:`player_id` 的唯一性改由「主键 + `chk_route_shard`」共同保证,所以 route store 建池时实测这条 CHECK 生效(探针 P4),不生效就拒建 store。细节见 §11.2「第 1a 批热点修复」。

**为什么拆 route_meta(G1a)**:每次 W1 都普通读 `writable`,而 1b leader 每 1s 写水位、每 3s 续租;两者同在一行时,TiDB 的读会被更早的未提交写锁间歇拖住,常写的表也没法设成缓存表。拆开后开关表一年写不了几次。

**排序规则(G1a)**:只写 CHARSET 时 MySQL 默认 `utf8mb4_0900_ai_ci` / `ascii_general_ci`,TiDB 默认 `utf8mb4_bin` / `ascii_bin`,比较语义会分叉;这些列都是标识符,显式写 `utf8mb4_bin` / `ascii_bin`。

**"真的没有记录"在库里的定义**：没有这一行，或者 `home_zone` 与 `storage_id` 都是 NULL。

**为什么拆 rev 和 placement_version**：`placement_version` 仍然只在搬库切换时 +1（P-2，`player-storage-placement.md:60`）。但冻结、解冻、合服改 home 也必须让缓存看出变化，所以另设 `rev`。

**围栏为什么分片**：两份评审都指出，一行围栏会把同一 zone 的建角串行化。TiDB 没有真正的共享锁，所以用分片代替 `FOR SHARE`。N=1 就退化成 A 的单行设计。表结构从第一天就带 `shard` 列，以后调 N 不用改表。TiDB 上另按 zone 预切(`route_tool tidb-prepare`,G1a),否则整表「zone 数 × N」行落在一个 Region 上。

### 3.3 Redis 键（副本，仍在 mapping Redis DB 0）

| 键 | 值 / TTL | 说明 |
|---|---|---|
| `player:zone:{id}` | 格式不变，无 TTL | 由数据库派生；旧读方照常可用 |
| `player:placement:{id}` | `"{s}:{v}[:frozen:{run}]"` 不变，无 TTL | 编解码仍以 `go/shared/placement` 为唯一出处，merge_zone 镜像一份（`player-storage-placement.md:429`） |
| `player:maprev:{id}` 新增 | 十进制 rev，无 TTL | 只用于 set-if-newer 排序和审计，旧读方不认识也不受影响 |
| `merge:in_progress:{zone}` | 数据库围栏的镜像；值是 holder_json（含 run_id、expires_at_ms）；**改为无 TTL** | 读方判据不变，仍是"键存在即封锁"（`router.go:85`）。过期由对账循环按库时钟删。镜像只可能多封锁，不会少封锁 |
| `merge:merged_into:{src}` | 镜像，永久，**不停写** | 评审 1：P6 之后只剩库里一份会被库回档带走 |
| `mapping:route:ready` 新增 | JSON `{generation, redis_run_id, master_replid, set_at_ms, watermark_ms}`，无 TTL | 存在即"副本完整可信"。只由对账 leader 在全量预热或修补完成后写 |
| `db:capability:zone:{z}` | 不变，90s 心跳 | **不落库、不进副本、恢复时不手写**（`placement_route.go:369-389`） |

**硬前提：mapping 实例必须是 noeviction。** 否则会出现"标记还在、单键被淘汰"，缺键被误当成缺席。对账 leader 用 `CONFIG GET maxmemory-policy`（被禁用时改用显式配置项）和 `INFO stats` 里的 `evicted_keys` 自检，任一不满足就拒绝挂标记（fail-closed）。
- K8s 单实例已经要求 noeviction（`deploy/k8s/manifests/infra/redis.yaml:137-139`）。
- dev 的 compose 是 `allkeys-lfu`（`deploy/docker-compose.yml:229-231`），这正是 mapping Redis `127.0.0.1:6379`（`data_service.yaml:61-63`）。必须先改，见 §9 第 0 批。

---

## 4. 写入点改造：顺序与原子性

### 4.1 公共规则

1. **事务**：route store 用独立连接池，DSN 设 RC，与 A15 同口径（`player-storage-placement.md:428`）。
2. **全局加锁顺序**：
   1. 普通读 `route_meta.writable`，为 0 就拒写；
   2. `zone_merge_fence`（zone 升序，再 shard 升序）；
   3. `zone_merged_into`；
   4. `player_route`（player_id 升序）。

   错误 1213/1205 有界重试 3 次加抖动，必须落在调用方预算内。
3. **判定只用加锁读（`FOR UPDATE`）的结果。** 新行用 `INSERT … ON DUPLICATE KEY UPDATE player_id=player_id`，**不用 `INSERT IGNORE`**：它会把 CHECK 违例降级成 warning（评审 1）。影响 0 行之后再加锁读查不到行，按内部错误处理。
4. **发布**：
   - COMMIT 后，在**同一条连接**上用 pipeline 发 `EVALSHA 发布脚本` 和 `WAITAOF numlocal numreplicas timeout`。
   - 取值：dev、kind 单实例是 `WAITAOF 1 0`；redis-ha 是 `WAITAOF 1 1`。镜像是 `redis:7.2`（`redis.yaml:83`、`redis-ha.yaml:684`），支持 WAITAOF。
   - 为什么是 WAITAOF 而不是 WAIT：评审都指出 WAIT 在 0 副本的单实例上无效；WAITAOF 能确认本地 AOF 已刷盘。
   - 为什么必须同一连接：两份评审都指出，WAIT 类命令只等本连接此前的写。
   - **待核实**：go-zero 的 `redis.Redis` 能否同连接 pipeline 发 WAITAOF。
5. **发布失败**：重试 3 次后，调用方按"未完成"失败。缓存这时只是落后，由对账增量推送补齐；按"提交即安全"，晚看到是安全的。
6. **防绕过**：所有写都收口在一个 `commitAndPublish` helper（Go 侧放 `go/shared`，merge_zone 镜像一份），另加 grep 测试，禁止在 helper 之外出现 `UPDATE player_route` 或直接 SET 映射键。

### 4.2 发布脚本（set-if-newer）

```lua
-- KEYS: 1=player:zone:{id} 2=player:placement:{id} 3=player:maprev:{id}
-- ARGV: 1=rev(十进制,无前导零) 2=home(''=DEL) 3=placement(''=DEL)
local cur = redis.call('GET', KEYS[3])
if cur and (#cur > #ARGV[1] or (#cur == #ARGV[1] and cur >= ARGV[1])) then
  return {0, cur, redis.call('GET', KEYS[1]) or '', redis.call('GET', KEYS[2]) or ''}
end
if ARGV[2] == '' then redis.call('DEL', KEYS[1]) else redis.call('SET', KEYS[1], ARGV[2]) end
if ARGV[3] == '' then redis.call('DEL', KEYS[2]) else redis.call('SET', KEYS[2], ARGV[3]) end
redis.call('SET', KEYS[3], ARGV[1])
return {1, ARGV[1]}
```

- **rev 比较**：先比长度、再比字典序，不经 Lua 的 double，与 `player-storage-placement.md:289` 担心的精度问题同源。
- **返回 0 时**（采纳评审 2 的修补）：
  - 缓存 rev 大于本次：已被更新的值取代，正常；
  - 缓存 rev 等于本次、值也相同：已确认；
  - 缓存 rev 等于本次、值不同：说明有绕过 helper 的写者，或库回档后 rev 被复用。报 `route_publish_total{result="conflict"}`，写者按失败返回。
- **多键脚本成立的前提**：mapping Redis 是单实例（`router.go:311-314`）；三把键都显式写进 KEYS。

### 4.3 逐写入点

| 写入点 | 改造后的顺序 | 与现状相比 |
|---|---|---|
| **W1 RegisterPlayerZone**（`router.go:152-237`），含 debug_import | 1) 确保围栏分片行存在（ODKU no-op）。2) BEGIN。3) 读 writable。4) `SELECT … FROM zone_merge_fence WHERE zone_id=? AND shard=player_id%N FOR UPDATE`，生效就回 3（`ErrZoneMergeInProgress`）。5) 按 ODKU 插入 rev=1；影响 0 行就加锁读后分支：同 home 幂等；异 home 冲突；home 为 NULL 时补 home，已有落点保留并打 ERROR，等价于原返回 2（`router.go:195-201`）。6) COMMIT。7) 发布 + WAITAOF；幂等分支也要重发，用来修补上次失败的发布 | 围栏复核与写入同事务。获取围栏要锁全部分片，必须等登记事务释放分片锁，`router.go:165-172` 防的窗口仍然关着。删掉 Redis 的 EXISTS 预检（`router.go:155-161`）。发布失败回 Unavailable，login 照旧 fail-closed 并释放名字（`createplayerlogic.go:257-275`）。**不新增可用性依赖**：6c 名字登记本来就同步写同一个库（`createplayerlogic.go:231-236`） |
| **W2 RemapHomeZoneForMerge**（`router.go:488-534`） | **停用**，回 FailedPrecondition("use tools/merge_zone") | 仓库内无调用方；无条件 SET 的危害见 `tools/merge_zone/merge_run.go:836-837` |
| **W3 DeletePlayerZoneIfLocked / W4 DeletePlayerZone** | W3 的 `DeleteZoneMapping` 分支在删号设计定下来之前回 FailedPrecondition；W4 降为测试专用 | 锁 token 在 Redis、删除在库里，两者无法原子；生产无调用方（`rollback_logic.go:1064-1071`）；删号与落点的关系本来就待定（`player-storage-placement.md:383`） |
| **W12 合服围栏**（`fence.go:103-210`，unmerge 同样使用） | **获取**：一个事务，按升序锁 src、dst 的全部分片 → 任一生效且 run_id 不是自己就拒绝 → UPDATE 写入 run_id、`expires_at_ms = 库时钟 + max(timeout+30m, 1h)`（`fence.go:111-114`）→ COMMIT → 写镜像（无 TTL）+ WAITAOF → **之后**才收集清单（`merge_run.go:171-174`）。**续期**：`UPDATE … WHERE run_id=?`，影响 0 行就把围栏标成"已丢失"，之后每批事务的围栏校验都失败、中止（原来只打 WARN，`fence.go:176-180`）。**释放**：先清库，再按 run_id 用 Lua 删镜像；删失败由对账删 | 过期只看库时钟，不看工具本机时钟 |
| **W6 markMergedInto**（`merged_into.go:93-119`） | 一个事务：校验围栏属于本次运行 → `SELECT … FROM zone_merged_into WHERE src_zone=? FOR UPDATE`：无行就插入；同一 dst 就更新 run_id；其它 dst 就拒绝 → COMMIT → 写镜像 + WAITAOF。必须先于 W5 | 原来"先 GET 再 SET、只靠围栏兜住"的非原子写由此关闭 |
| **W5 合服步骤 5**（`merge_run.go:586-617`、`:839-951`） | 每 500 人一个事务：锁 src、dst 围栏并要求 run_id 等于清单值（**新增 fencing token**，挡住 TTL 过期后被人接手的僵尸工具；原 Lua 不校验，`merge_run.go:839-848`）→ 玩家行按升序 FOR UPDATE → 按 `classifyMappingValue` 同口径分类 → `UPDATE … SET home_zone=dst, rev=rev+1 WHERE … AND home_zone=src`，影响行数必须等于要改的人数 → COMMIT → 整批发布 + WAITAOF | 完成判据：原 `complete()`（`merge_run.go:894-901`）**加上**全员发布已确认 |
| **W7 pin 合服钉落点**（`placement_ops.go:661-728`） | 每批一个事务：围栏属于本次运行 → 玩家行 FOR UPDATE → `decidePin` → `UPDATE storage_id=src, placement_version=1 … WHERE storage_id IS NULL AND home_zone=src` → COMMIT → 发布 | 原来不传围栏键，现在正向校验"围栏属于我"，更严格 |
| **W8 unmerge 5' / 5'a**（`unmerge.go:391-433`、`merged_into.go:142-170`） | 每批一个事务：围栏属于本次撤销 → 条件 UPDATE，home 从 dst 改回 src → COMMIT → 发布；5'a 在持有围栏的事务里 `DELETE … WHERE src_zone=? AND dst_zone=? AND run_id=?`，再删镜像 | 原来是 pipeline 普通 SET，不是 CAS（`unmerge.go:426`）。"键不存在就跳过"现在是权威缺席，不可能是丢键 |
| **W9 backfill_home_zone**（`backfill_home_zone.go:185-245`） | 每批一个事务：锁任一围栏分片，要求未生效（**新增**，原实现不看围栏，`:229-230`）→ `zone_merged_into` 有该区就拒绝（这个标记现在持久，Redis 丢了也拦得住）→ 只给"无行"的人 ODKU 插入，给"home 为 NULL"的人补 home → COMMIT → 发布 | 定位降级：只给"从未登记过的存量号"补 home，不再是 Redis 丢失后的恢复手段 |
| **W10 -mode pin-placement**（`pin_placement.go:64-121`） | 每批不超过 200 人一个事务：围栏分片未生效 → 无 `merged_into` → `SELECT … WHERE home_zone=N AND storage_id IS NULL … FOR UPDATE`（走 `idx_route_home`）→ UPDATE → COMMIT → 发布 | 钉的值等于 home，有效落点不变，提前或延后可见都安全 |
| **W11 relocate**（R1、R5、解冻、abort） | **只换 `relocatePlacements` 接口的实现**（`relocate.go:38-43` 这条现成接缝），`decideFreeze`、引擎都不改。`cas` 每批一个事务：锁相关 home 的围栏分片（生效回结局 3）→ 玩家行 FOR UPDATE → 比对"编码后的当前值 vs 期望值、当前 home vs 期望 home"→ 写列、rev+1 → **新增断言**：稳定态的新 version 不得小于当前 version（守 P-2）→ COMMIT → 发布 + WAITAOF → 只有确认了的人才回 Written。R1 必须确认之后才进 R2 | **不需要 B 的 R5a/R5b 拆分**：库先提交，缓存只可能落后。R5 的发布丢了，缓存停在 R1 已确认的冻结态，go/db 延后写，安全；对账再把 `T:v+1` 推上去，R4 已比对过，也安全 |
| **W13 能力标记**（`go/db/db.go:143-157`） | 不改 | 见 §1 第 3 条 |

---

## 5. 读路径，以及"缓存未命中 ≠ 无记录"

### 5.1 统一判据（取代"键不存在 = 无记录"）

| 同一次 MGET 或同一段 Lua 读到的 | 含义 | 处理 |
|---|---|---|
| 就绪标记在，映射键在 | 权威值 | 直接用；畸形值照旧 fail-closed（`player-storage-placement.md:77`） |
| 就绪标记在，映射键缺 | **权威缺席** | 沿用今天的缺席语义 |
| **就绪标记不在** | 副本不可信，即"未命中" | **绝不当缺席**：data_service 回源数据库；go/db 和 guild 按故障 fail-closed |

**为什么"标记在 + 键缺"可以当真缺席**：
- 所有写都是"库先提交，再发布并 WAITAOF 确认"，依赖步骤才推进；
- 实例 noeviction，有自检；
- Redis 整体丢失时标记跟着丢；
- 部分丢失由对账在一拍之内撤掉标记；
- 只有全量预热或修补完成后才挂回标记。

**残留窗口**（写进风险）：
- 已提交但还没发布的状态，对读方暂时不可见。对 W1 而言调用方还没成功、玩家进不了场；对工具而言依赖步骤还没开始。
- Redis 重启到 leader 发现之间（不超过一拍），读到的只会是"没确认过的发布被回退"，按"提交即安全"等价于"晚看到"。

### 5.2 逐读方

| 读方 | 改造 | 标记不在时 |
|---|---|---|
| **R1 go/db 选库与复核**（`placement_route.go:89-99`、`:242-254`） | MGET 从 2 键改为 3 键（placement、zone、ready）；`decideWritePlacement`、`decideReadPlacement`、`recheckPasses` 这些纯函数**一格不改**（`:158-226`）；由开关 `Placement.RequireRouteReady` 控制，缺省 false | 新 outcome 为 `route_not_ready`。**写任务**延后，加入 `retryBudgetExempt`（`:357-367`）不耗预算，避免存盘进死信。**读任务**照常耗预算，理由同 `:352-356` 的"落点库打不开只豁免写"。复核遇到不就绪按可重试处理。**不连全局库**：Redis 整体丢失时游标、排序锁、重试队列也在同一实例一起丢，回源换不来可用性（方案 C 的论证）；同时避开 strict 库名白名单（`k8s_deploy.ps1:3452-3455`）和第三份 SQL |
| **R2 scene_manager → GetPlayerHomeZoneAndMergeFence**（`router.go:299-324`） | Lua 开头加 `EXISTS mapping:route:ready`，不在就回 `{'', -2}` | 回源用一条 SQL：`player_route` LEFT JOIN `zone_merge_fence`（shard 0），home 与围栏仍是同一快照（`router.go:286-289` 的要求），然后用发布脚本回填。查询失败回 Unavailable，**绝不回 NotFound**；NotFound 的文案契约不变（`router.go:247-256`）。scene_manager 收到 Unavailable 走既有的拒绝分支（`home_zone.go:152-157`），不会把 gate zone 当 home |
| **R3 login**、**R5 trade / match / friend**：都经 `BatchGetPlayerHomeZone`（`router.go:366-390`） | MGET 里并入 ready；顺带修掉畸形值被静默跳过（`:383-386`），改为整批报错 | 一条 `WHERE player_id IN (…)` 回源；失败整批回错，各调用方照现有故障分支处理 |
| **R4 guild 直读围栏**（`go/guild/internal/logic/merge_fence.go:62-73`，经济、活动也经由它） | 改 MGET `[merge:in_progress:{z}, mapping:route:ready]` | 标记不在就返回错误，调用方按接口契约当封锁处理。这也覆盖了评审 1 指出的"guild 内部调用不经单查"那条路径 |
| **R6 data_service 内部**（`ClientForPlayer`、snapshot、debug_fetch） | 单 GET 改成 MGET zone + ready | 回源；配了 DevRedis 时今天实际 0 次（`player-storage-placement.md:335`） |
| **R7 全量枚举**：`GetAllPlayerIDsInZone`（`router.go:440-472`）、`collectPlayerIDsWithHomeZone`、`collectStoragePlayers`、`data_consistency_check` | **一律读库**，走 `idx_route_home` / `idx_route_storage` 键集翻页；修掉吞 GET 错误（`router.go:452-454`） | 不受缓存影响。完整性：登记事务持有分片锁直到提交，围栏提交之后的扫描一定能看到此前所有登记 |
| **R8 merge_zone 其它读**：清单、`placementsExisting`、`manifestPlayersBackAt`、`redisMappingPresence`（`merge_run.go:741`）、`verify:manifest_mapping`（`audit_checks.go:261`）、`data_consistency_check/main.go:462` | 全部读库（评审 1 补全的清单）。新增 `verify:route_cache`：开服前逐人比对清单玩家的"缓存 rev = 库 rev，且值相等" | 满足 P-6：缓存必须在 dst 开服之前反映新 home |

### 5.3 围栏、merged_into、冻结态：都落库

| 对象 | 真源 | Redis | 理由 |
|---|---|---|---|
| 合服围栏 | `zone_merge_fence`，过期看库时钟 | 镜像，无 TTL，对账负责删 | 围栏与登记必须同一事务域复核（`router.go:77-79`）；Redis 丢失时围栏不能跟着消失 |
| merged_into | `zone_merged_into` | 永久镜像 | Redis 整体丢失时它是唯一能挡住"回填钉回源区"的东西（`merged_into.go:11-20`、runbook `:734`） |
| 冻结态 | `player_route.frozen_run_id` | 落点值里的 `:frozen:{run}` | relocate 崩溃后续跑靠它认领（`relocate.go:253-266`） |
| 能力标记 | **只在 Redis** | 90s 心跳 | 必须保持临时（`placement_route.go:369-379`）；Redis 丢失后 30s 内自动补回 |

### 5.4 新增：data_service 对账 leader

leader 由 `route_meta` 租约选出（10s 租期，3s 续一次）。每拍 1s：

1. 读 `INFO server` 的 `run_id` 和 `INFO replication` 的 `master_replid`，再读就绪标记。
2. **标记在，但身份变了**（Redis 重启或切主）：
   - 先 DEL 标记（关门）；
   - 推送 `updated_at_ms >= cache_verified_ms - 60s` 的行（set-if-newer）。60s 余量覆盖 `min-replicas-max-lag 10`（`redis-ha.yaml:252-254`）和时钟偏差；
   - 全量同步围栏和 merged_into 镜像（只有寥寥几个 zone）；
   - 写回标记。
3. **标记不在**：
   - 自检 noeviction；
   - 全量预热：按主键键集翻页、pipeline 写入，set-if-newer 保证与在线写者并发时收敛；
   - 同步围栏和 merged_into；
   - 核对 run_id 在预热期间没变，再写标记。
4. **每拍还做**：
   - 围栏镜像对账：补齐缺失的生效围栏；删除库里已失效的镜像，先查库确认、再按 run_id 删；
   - 写 `watermark_ms`；
   - **回档检测**：库里的 watermark 小于标记里记下的 watermark 减容差，就把 `writable` 置 0、告警，停止库到缓存的推送（缓存才是较新的一边）。
5. **每 30s** 推送最近 N 分钟变化的行，自动补齐发布失败的那部分。

---

## 6. 故障行为与恢复

| 故障 | 行为 | 恢复步骤 |
|---|---|---|
| **Redis 整体丢失**（FLUSHALL、换空实例、PVC 丢失） | 标记随之消失。go/db 写延后、不丢盘；data_service 的读回源，进场判定照常；guild 的建帮、经济、活动按封锁拒绝；建角照常（库先提交，再发布到新实例）。**已提交的映射零丢失** | **自动**：leader 全量预热后挂标记（RTO = 全量预热时长，100 万行级估计几十秒到分钟，未测）。运维只需：照现有流程处理会话、锁、游标的丢失（全服维护）；确认标记已挂、`-route-audit` 为 0。原 runbook §9.2 的手工重放（`merge-zone-runbook.md:732-750`）不再需要。**过渡期例外**：读方开关打开（第 3 批）之前，旧读方仍会把缺键当缺席，必须保留 §9.2 第 0 步"先停 go/db"，等预热完成再起 |
| **Redis 部分丢失**（AOF everysec 丢约 1s，`redis.yaml:95-97`；切主丢尾部，`redis-ha.yaml:24-27`） | 经 WAITAOF 确认的发布不丢；丢的只是没确认的发布，按"提交即安全"无害。leader 在一拍内发现身份变化，撤标记、修补、挂回 | 自动。残留：哨兵提升了落后的副本时，已确认的写也可能丢，由第 2 步按 `updated_at_ms` 修补 |
| **数据库不可用** | 稳态读全在 Redis，热路径（存盘、进场、登录列表）不受影响。写入 fail-closed：建角失败，但 6c 名字登记本来就依赖同一个库；合服、搬库、钉落点工具拒绝启动，或带着围栏中止。标记保持（没有写能绕过库，副本仍然正确）；对账暂停 | 库恢复后自动续上；数据库不可用期间禁止开始合服或搬库（写进 runbook） |
| **数据库回档**（PITR；K8s 下全局库与 zone 库同一个 mysql 实例，`k8s_deploy.ps1:3443`、`:3511-3512`，zone 回档会连带回档全局库） | leader 发现 watermark 回退：`writable=0`，全体写者拒写；**不往缓存推旧值**；缓存与标记保持不动，读照常 | 1) `data_service -route-reconcile-from-cache`：缓存 maprev 大于库 rev 或库里无行时，把缓存值连同 rev 写回库；围栏和 merged_into 从镜像补回。2) `-route-audit` 两个方向都为 0。3) 换新 `generation`，`writable=1`。`k8s_zone_rollback.ps1` 的流程里要加入这一步 |
| **Redis 与数据库同时丢失** | 全服 fail-closed（不会写错库） | 先恢复数据库备份；按 runbook §9.2 的清单重放，补齐备份之后的合服和搬库（重放目标改为写库）；跑 backfill；最后预热。清单"永久归档"的纪律保留（`merge-zone-runbook.md:720`） |

---

## 7. 存量回填、上线顺序与回退

**前置条件**：
1. 先按 `docs/handoff/placement-reverify-checklist-20261012.md` 复验当前 HEAD 的落点代码（`player-storage-placement.md:4`），本方案不能叠在一个未验证的基线上。
2. 完成第 0 批（§9）。

| 阶段 | 内容 | 新旧并存的说明 |
|---|---|---|
| **P0 建表** | migrate 建 4 张表，写入 `route_meta(id=1)` | 只新增，行为不变 |
| **P1 冻结窗口开始，导入存量** | 禁止合服、unmerge、relocate、pin-placement、backfill、W2，代价为零：从未执行过（`player-storage-placement.md:5`）。`data_service -route-import-from-redis`：SCAN `player:zone:*`、`player:placement:*`、`merge:merged_into:*`，逐批 upsert；发现生效的 `merge:in_progress` 就中止；畸形值报告并跳过，交人工。之后跑 `-route-audit -against-redis` | Redis 仍是唯一提交点 |
| **P2 data_service 切到库先写**（滚动） | W1 改为库先写，W2、W3 停用 | 混跑期间旧副本只写 Redis。登记总是新 id，不会出现异值并发；窗口内没有围栏，两种副本的围栏口径不同也无妨 |
| **P3 二次导入，挂标记** | 全部副本切换后，**再跑一遍导入 + 审计，直到为 0**，补上旧副本在滚动期间写的登记（评审 2 的修补，取代 A 的 shadow 方案）。然后启动对账 leader：全量预热 maprev，挂标记 | 读方还不要求标记，行为不变 |
| **P4 新版 merge_zone** | 工具全部改为库先写（新参数 `-route-db-dsn`）；runbook §9.2 改写；解除冻结窗口 | dev_tools 每次从 HEAD 编译（`player-storage-placement.md:427`）；旧二进制禁用，写进 runbook |
| **P5 读方要求标记** | go/db 逐 zone 打开 `RequireRouteReady`；data_service 读路径和回源；guild | 先有标记、后要求；反过来会全服 fail-closed |
| **压测** | P2 之后测建角（注册阶段 p99、1213/1205 计数，以此定 N）；P5 之后测存盘（go/db MGET 多一个键）。按 AGENTS.md §6 出对比表 | 落点 MGET 本身也从没压测过（`player-storage-placement.md:379`），合并成一轮 |

**与 §13 的衔接**（`player-storage-placement.md:431-450`）：
- P0–P5 是生产环境执行 §13 第 4 步（pin 合服、relocate、`PinOnCreate`）的前置；
- 是 `PinOnCreate` 指向全局库的**硬前置**（§4.4 的缺口，`:116`）；
- 第 5 步（`Required=true`）照原顺序，放在其后。

**回退**：
- **顺序固定为先撤读方、后撤写方**（评审 2）：先关 `RequireRouteReady`，再回退写者。
- data_service 退回只写 Redis 的旧版：Redis 键格式没变，一直是完整副本，旧版照常工作；库会落后，重新前进前必须**重新冻结工具，再跑一次 P3**。
- P4 之后旧版 merge_zone 一律禁用；需要回退就同时回到冻结窗口。
- 回退时不删表。
- §13 的 go/db 回退禁令照旧有效，与本方案正交。

**双版本**（AGENTS.md §12）：映射存储属于服务端内部实现，客户端契约不变，两版也不共用 MySQL。Java 版按自己的方式做等价的"库为真源 + 完整副本"，并在 `PARITY.md` 登记；本仓库的 PROGRESS 条目注明 Java 状态（目前待做）。

---

## 8. 被否决的方案、已吸收的修补、仍未解决的风险

### 8.1 被否决或部分否决

| 方案 | 结论 | 理由 |
|---|---|---|
| **B：Redis 仍为真源 + outbox + applier + guardian** | 否决 | 1) 评审 2 指出致命问题：guardian 以"epoch 不一致"为触发条件，整体丢失也命中，只回写尾部行就重新开库，大量玩家被当成真缺席。2) 评审 1：全局库回档时修尾方向反了，会把旧值写回 Redis。3) 单一有序流有队头阻塞：别的 zone 合服产生约 20 万条 outbox，会让建角超时。4) 机制最多，没有回应"Redis 只做缓存"。**吸收**：Redis 身份检测、就绪标记、库租约 leader、FLUSHDB 防护 |
| **C：行戳下界重建** | 否决（包括 A+C） | 证明不了"不存在"：首存之前的玩家、冻结态、围栏、merged_into 都无法重建。要改最热的表 `player_database`，牵动 C++、所有库的 migrate 和 `alignedPlayerColumns` 门禁（`player_rows.go:463-483`），属于 YAGNI。RTO 未知。**吸收**：不改键格式、加 maprev；"go/db 不回源"的论证；"行为什么不能当真源"的分析 |
| **A 原文** | 采用骨架，四处改动 | 1) 新键 `player:route` + MirrorLegacy + P6 → 保持原格式加 maprev，上线和回退矩阵简单得多。2) 每键负缓存 → 整副本就绪标记，稳态没有未命中。3) go/db 的 RouteDB 只读池 → 去掉，与 strict 白名单冲突，回源也换不来可用性。4) shadow 写库 → 库先写，加二次导入 |
| **Kafka outbox** | 否决 | Lua 无法原子地写 Kafka，写者崩溃会永久丢失副本登记（B 自己的分析） |
| **等 TiDB 再做** | 否决 | Phase 1 未定（`player-storage-placement.md:374`）；风险与玩家表放在哪无关；今天的 MySQL 全局库已有同类先例（`player_name`） |

### 8.2 已吸收的评审修补

- **回档闸门**：`route_meta.watermark_ms` / `generation` / `writable` 加从缓存反向修库，对应评审 1 的"库可信闸"。
- **切读前缺口**：二次导入 + 审计为 0 才挂标记。
- **重启和切主**：自检不依赖哨兵告警；`updated_at_ms` 建了索引。
- **围栏镜像**：去掉 TTL，用库时钟加对账清理；noeviction 写进部署契约，删掉 A 里"volatile-* 也可以"的错误说法。
- **WAITAOF**：与发布脚本同连接。
- **W1**：用 ODKU，不用 `INSERT IGNORE`。
- **建角吞吐**：围栏分片，先压测再定 N。
- **merged_into**：Redis 镜像永久保留。
- **读方清单**：补全 `merge_run.go:741`、`audit_checks.go:261`、`data_consistency_check/main.go:462`。
- **发布返回 0**：返回当前值，写者比较。
- **清库脚本**：`stress_round19.ps1:80`、`:117-124` 和 `currency_crash_window.ps1:111-130` 只清 `zone_N_db` 不清全局库，要把新表加进清单，同步改 AGENTS.md §6.2；否则 FLUSHALL 之后，残留的生效围栏会被对账重新镜像回去。
- **预算豁免**：只豁免写，读照常耗预算。
- **回退顺序**：先读方、后写方。

### 8.3 仍未解决的风险（需知情或拍板）

1. **写者纪律是正确性的根基。** 绕过 `commitAndPublish` 的写者（旧版 merge_zone、手工 `redis-cli SET`、将来的新代码）会造成"rev 相同、值不同"。只能靠 helper、grep 测试、审计告警事后发现，无法事前拦截。
2. **WAITAOF 的延迟。** 在 `appendfsync everysec` 下，本地确认的唤醒时机没有核实，最坏按 1s 估算，落在 W1 3s 预算内（`homezone.go:52-58`）。不可接受时，mapping 实例改为 `appendfsync always`，或单独起一个实例。go-zero 能否同连接 pipeline 发 WAITAOF 也待核实。
3. **WAITAOF 不等于强一致。** 哨兵提升了落后的副本时，已确认的写仍可能丢，只能靠身份检测加 60s 回看修补。
4. **noeviction 是硬前提。** dev 改成 noeviction 之后，内存打满就拒写（fail-closed）。这是与当前 OOM 防御（`docker-compose.yml:222-226` 的注释）的取舍，见 §10。
5. **Redis 重启后 go/db 要等 data_service 的 leader 挂回标记。** data_service 全挂期间，go/db 的写会一直延后（不丢盘）。这是一条新的可用性耦合，但只在 Redis 刚重启之后出现。
6. **库回档检测依赖 watermark 容差。** 回退幅度小于容差（秒级）时检测不到，靠 runbook 硬性要求"凡对全局库所在实例做过 PITR，都要跑反向修库"。
7. **同 zone 建角吞吐、分片数 N、TiDB 悲观事务下的表现都没测。**
8. **删号语义仍未定**（W3 停用，`player-storage-placement.md:383`）。
9. **Redis 整体丢失时，会话、锁、游标、`PlayerAllData` 的恢复不在本方案范围。**
10. **改动超过 30 个文件，需要按 §10.2 授权；HEAD 上的落点代码也还没复验。**

---

## 9. 改动面清单与分批

估计合计约 45–55 个手改文件，不含生成产物，超过 AGENTS.md §10.2 的 30 文件门槛。**开工前必须取得授权**，并按 §11.5 先在 `player-storage-placement.md` 新增"映射持久化"一节，改写 §4.4、§11.4。建议分 4 批，每批可以独立审阅、独立回退。

| 批 | 内容 | 主要文件 |
|---|---|---|
| **0 先修现存隐患**（约 5 个文件，可以立即做，与方案无关） | dev 的 mapping Redis 改 noeviction（或单独起一个实例）；zone 回档脚本的 FLUSHDB 加防护：目标实例存在 `player:zone:*` 或 `merge:*` 键就拒绝，除非显式确认（默认 DB 0，`k8s_zone_rollback.ps1:107-111`、`:803-813`）；BatchGet 畸形值改为报错；`GetAllPlayerIDsInZone` 不再吞错误 | `deploy/docker-compose.yml`、`tools/scripts/k8s_zone_rollback.ps1`、`go/data_service/internal/routing/router.go` 及其测试 |
| **1 存储底座 + data_service 库先写**（约 15–18 个文件，覆盖 P0–P3） | 4 个 proto message 并重新生成（Go 与 C++ proto 库）；bootstrap DDL 和 `TableMessages`；`store/route_store.go`；`go/shared/placement` 增加 maprev 键、就绪标记键、发布脚本和测试向量；router 的 W1 库先写、W2/W3 停用；`commitAndPublish`；对账 leader；四个 cmd：`-route-import-from-redis`、`-route-audit`、`-route-rewarm`、`-route-reconcile-from-cache`；config、yaml、K8s ConfigMap 与契约测试（钉住 noeviction、DBName）；两份清库脚本 | `proto/common/database/rollback_database_table.proto`、`go/data_service/internal/{store,routing,config,svc}/…`、`go/data_service/data_service.go`、`go/shared/placement/*`、`tools/scripts/{k8s_deploy.ps1,stress_round19.ps1,currency_crash_window.ps1}`、契约测试 |
| **2 merge_zone 写入点**（约 15 个文件，覆盖 P4） | 新增 `route_store.go` 作 SQL 镜像；`fence.go`、`merged_into.go`、`merge_run.go`、`placement_ops.go`、`pin_placement.go`、`relocate.go`（只换 `relocatePlacements` 的实现）、`unmerge.go`、`backfill_home_zone.go`、`player_blob_migrate.go`、`audit_checks.go`（新增 `verify:route_cache`）、`main.go`、`placement_codec.go`；集成测试；dev_tools 转发与契约测试；runbook §9 重写 | `tools/merge_zone/*`、`tools/scripts/dev_tools.ps1`、`tools/scripts/tests/dev_tools_merge_zone_contract.tests.ps1`、`docs/ops/merge-zone-runbook.md` |
| **3 读方要求标记**（约 10 个文件，覆盖 P5） | go/db 的三键 MGET、`route_not_ready`、预算豁免、开关、指标；data_service 读路径与回源；guild 围栏读；`data_consistency_check`；告警 yaml；文档（落点设计新增一节、TiDB 决策 D1 标为本方案、PROGRESS、`docs/README.md`） | `go/db/internal/kafka/placement_route.go`、`go/db/internal/config/config.go`、`go/guild/internal/logic/merge_fence.go`、`tools/data_consistency_check/main.go`、`deploy/k8s/*-alerts.yaml`、`docs/design/*` |
| **Java 版** | 等价实现，在 `PARITY.md` 登记 | Java 仓库 |

**每批交给 Codex 的验证要点**：
- 受影响 Go 模块的 build、vet、test；
- data_service 和 merge_zone 的 MySQL + Redis 集成测试：发布 set-if-newer 乱序、WAITAOF 同连接、围栏分片互斥、二次导入收敛、预热与并发写收敛、回档检测；
- 至少一组 TiDB 悲观 RC 用例；
- 第 1 批要重编 C++ proto 库（`/m:1` 串行）；
- 第 1 批之后压测建角，第 3 批之后压测存盘，按 §6 出对比表。

---

## 10. 待负责人拍板

1. **方向**：采用"库为真源 + 完整副本 + 就绪标记"，即本稿方案，并按 4 批推进（超过 30 个文件，需要授权）。
2. **dev 的 Redis 淘汰策略**：改成 noeviction，内存满即拒写；还是给映射单独起一个 noeviction 实例。
3. **W2 停用、W3 的删映射分支停用**，等删号设计定下来再说。
4. **围栏分片数 N**：先按 8 落码，压测之后定。
5. **对 `appendfsync everysec` 下 WAITAOF 延迟的接受度**：不接受就给 mapping 实例单独配 `appendfsync always`。
6. **时间点**：第 0 批立即做；第 1–3 批放在生产执行 §13 第 4 步之前完成；在落点复验清单完成之前不开工。

---

## 11. 拍板与落码记录

### 11.1 负责人拍板(2026-10-12)

| 第 10 节待拍板项 | 结论 |
|---|---|
| 1. 方向 | 采用:库为真源 + 完整副本 + 就绪标记,按批推进。超过 30 个文件的改动面已获授权 |
| 2. dev Redis 淘汰策略 | 改 noeviction,与 K8s 一致,不另起实例 |
| 3. W2 / W3 | 按本稿默认:停用,等删号设计定下来 |
| 4. 围栏分片数 N | 按本稿默认:先按 8 落码,压测后再定 |
| 5. WAITAOF 延迟 | 未单独表态,按本稿默认:`appendfsync everysec` 下先用 WAITAOF,压测后再定是否改 always |
| 6. 时间点 | 「两条都做,D1 先行」:先写 D1 代码,同时出 TiDB Phase 1 的设计与实施清单 |

前置提醒:当前 HEAD 的落点代码还没按 [placement-reverify-checklist-20261012.md](../handoff/placement-reverify-checklist-20261012.md) 复验。D1 代码可以先写,但交 Codex 验证时要先跑那份清单,再跑本方案各批的验证。

### 11.2 落码进度

| 批 | 状态 | 说明 |
|---|---|---|
| 第 0 批 | 已落码(2026-10-12),**未编译、未测试** | 见下 |
| 第 1a 批 | 已落码(2026-10-12),**未编译、未测试** | 存储底座(四张表 + route store)、W1 的 db_first 开关(所有档位默认 redis)、`route_tool` 导入 / 审计、W2 / W3 停用。见下「第 1a 批」 |
| 第 1a 批热点修复(G1a) | 已落码(2026-10-12),**未编译、未测试** | TiDB 写热点:`player_route` 聚簇 `(route_shard, player_id)` + 片前缀索引、`route_meta` 拆出 `route_reconcile`、建池探针 P4、`route_tool tidb-prepare`。见下「第 1a 批热点修复」 |
| 第 1b 批 | 未开始 | §9 第 1 批的剩余项:对账 leader、全量预热与 `mapping:route:ready`、回档检测与反向修库(`-route-rewarm` / `-route-reconcile-from-cache`)、`route_tool` 并进 data_service 主程序入口(K8s 上可跑)。范围以 1b 开工时的规格为准 |
| 第 2、3 批 | 未开始 | 第 2 批另含:四张表补 proto 守卫(替换 `routeTableShapes`,见第 1a 批偏离第 1 条) |

**第 0 批(2026-10-12)改动**:
- `deploy/docker-compose.yml`:redis 的 `maxmemory-policy` 由 `allkeys-lfu` 改为 `noeviction`(保留 `maxmemory 8gb`),注释写明原因。
- `go/data_service/internal/routing/router.go`:
  - `BatchGetPlayerHomeZone` 遇到畸形值整批报错,与单查 `parseHomeZoneValue` 同口径(此前静默跳过,被当成缺席);
  - `GetAllPlayerIDsInZone` 单键 GET 失败时整体返回错误(此前吞掉,名单会静默缺人)。
  - 新增单测:`TestBatchGetPlayerHomeZone_MalformedValueFailsTheBatch`、`TestGetAllPlayerIDsInZone`、`TestGetAllPlayerIDsInZone_GetErrorFailsTheScan`。
- `go/data_service/internal/server/dataserviceserver.go`:`BatchGetPlayerHomeZone` 注释补上畸形值口径。
- `tools/scripts/k8s_zone_rollback.ps1`:Step 4 FLUSHDB 防护。目标 DB 里有 `player:zone:*` / `player:placement:*` / `merge:*` / `consumer:applied*` / `kafka:retry*` 任一类键就拒绝,查询失败同样拒绝;核对在第 0 步预检(停服之前)与 Step 4 各做一次;新开关 `-ConfirmFlushSharedRedis` 放行;Step 4 打印的命令行遮住口令(此前明文打印)。探测用 `redis-cli SCAN` 逐页、找到第一个命中即停,不用 `--scan`(会把百万级键名全打出来)。
- `tools/scripts/dev_tools.ps1`:新开关 `-RollbackConfirmFlushSharedRedis` 转给回滚脚本。
- `tools/scripts/tests/k8s_zone_rollback_flush_guard.tests.ps1`(新):防护判定、fail-closed、确认开关、dry-run 不读 Redis、口令遮蔽、dev_tools 转发。
- `docs/design/zone_data_rollback.md`:步骤 3 的 FLUSHDB 补警告。

**第 0 批交 Codex 的验证**(仓库根):
1. `cd go/data_service`:`gofmt -l ./internal/routing ./internal/server`(无输出);`go vet ./internal/routing/... ./internal/server/...`;`go test ./internal/routing/... ./internal/server/... -count=1 -v -run "BatchGetPlayerHomeZone|GetAllPlayerIDsInZone"`,上面 3 个新用例与既有 `TestBatchGetPlayerHomeZone` PASS;再 `go test ./... -count=1` 全绿(既有失败按复验清单第 4 步归因)。
2. `pwsh -NoProfile -NonInteractive -File tools/scripts/tests/k8s_zone_rollback_flush_guard.tests.ps1`:fail=0(共 8 个用例)。
3. 回归:`pwsh -NoProfile -NonInteractive -File tools/scripts/tests/k8s_zone_rollback_gate_router_mode.tests.ps1` 与 `dev_tools_merge_zone_contract.tests.ps1` 均 fail=0(dev_tools 参数表新增了一个 switch)。
4. 本机 compose:`docker compose -f deploy/docker-compose.yml up -d redis` 后 `docker exec redis redis-cli CONFIG GET maxmemory-policy` 应为 `noeviction`。重建容器会不会丢本机数据取决于 volume,执行前先确认。

**已知未改**:chat、friend、match、scene_manager 共 9 处代码注释仍写「共享库是 allkeys-lfu」(如 `go/chat/etc/chat.yaml:44`、`go/friend/internal/svc/servicecontext.go:157`、`go/scene_manager/internal/logic/enterscenelogic.go:596`),现已过时。注释本身的结论(staging/prod 要用独立的 noeviction 实例)仍然成立,留作后续统一更正。

---

**第 1a 批(2026-10-12):存储底座 + W1 的 db_first 开关 + 导入 / 审计工具 + W2 / W3 停用**

**未编译、未测试**(AGENTS.md §10.1)。Codex 跑完下面「第 1a 批交 Codex 的验证」之前,不能说「编译通过」「测试绿」「已验证」。

- **改动面**:代码与脚本 38 个(新增 15、修改 23),文档 10 个,超过 §10.2 的 30 文件门槛,已由 §11.1 第 1 行授权。
- **本批不改 proto,不重编 C++**:四张表走手写 DDL,列漂移只守列名与主键(`routeTableShapes`),补 proto 留到第 2 批(偏离第 1 条)。
- **所有档位固定 `RouteStore.Mode: redis`**:本地 `etc/data_service.yaml` 显式写 redis;K8s ConfigMap 所有档位固定 redis、不镜像服务 yaml(契约测试守住)。redis 模式下 W1 与 2026-10-12 之前逐字相同;切 db_first 是 P2 的显式运维动作,门禁见下。例外:新表的 DDL、形状守卫、meta 种行进了 `MigrateSchema` 的关键路径,redis 模式也受影响(风险第 8 条)。
- **代码注释里的「规格 x 节 / 第 N 条」**指第 1a 批定稿实现规格。那是会话工作稿,没有入库。与决策相关的部分收录在本节,编号保持一致:
  - 「规格 g.2 第 N 条」= 下面「相对本提案的偏离」第 N 行;
  - 「规格 g.3 第 N 条」= 「仍未解决的风险」第 N 条;
  - 「规格 h 节第 N 步」= 「第 1a 批交 Codex 的验证」第 N 步;
  - 「规格 c.3」= 「语义对照」表;「规格 Q1–Q16」= 「待定问题」表;
  - 其余各节(b 类型、SQL 与 Lua,c.1 / c.2 的步骤,d 配置,e 工具,f 用例清单,以及评审编号 B1 / B2 / M1–M6 / m1–m12)已落在代码与注释里,以代码为准。

**改动清单**

新增(15 个):

| 文件 | 要点 |
|---|---|
| `go/shared/placement/route.go`、`route_test.go` | 新键 `player:maprev:{id}`、`mapping:route:ready`(本批只定义,不读不写);围栏分片 `DefaultRouteFenceShards=8`、`MaxRouteFenceShards=64` 与 `RouteFenceShard`。64 同时是围栏持有者的契约:持有者必须写满 0..63 全部分片行。rev 的 `FormatRev` / `ValidRev` / `CompareRev`:先比长度,再逐字节比。发布脚本 `PublishRouteScript` 全文、`PublishRouteKeys`;`ClassifyPublishReply` 只认四种回复形状,其余一律 `ErrUnexpectedPublishReply`。仍只依赖标准库 |
| `go/data_service/internal/store/route_store.go` | 独立连接池,DSN 钉会话级 `READ-COMMITTED` 与 `innodb_lock_wait_timeout=1`。建 store 时跑探针:隔离级别、锁等待、TiDB 须为 `pessimistic`、`route_meta(id=1)` 存在,任一不符就关池拒建。W1 事务 `RegisterHome`;导入 `ImportRoutes` / `ImportMergedInto`;只读 `ScanRoutes` / `GetRoutes` / `ListMergedInto`;`decideImport`;`NewRouteWriterID`;`bootstrapRouteMetaRow`。它是四张表**唯一的写入方**,player_route 由 AST 守卫钉住 |
| `store/route_store_test.go`、`route_store_contract_test.go`、`route_store_integration_test.go` | 不连库的单测:DSN、DDL 契约、禁止 INSERT IGNORE、AST 写入守卫、`decideImport`、`RouteRow`、writer 清洗。假驱动契约测试(默认运行,BEGIN / COMMIT / ROLLBACK 记进语句序列):钉住「库应答 → 结局 / 语句 / 参数」和探针 fail-closed。真库集成测试(`integration` tag,storetest 一次性库):迁移幂等、CHECK、形状守卫、各分支、并发、锁等待封顶、围栏生效与过期、导入幂等、键集翻页 |
| `go/data_service/internal/routing/route_publish.go`、`route_publish_test.go` | db_first 的 W1 编排 `registerPlayerZoneDBFirst`;发布 `publishRoute`,同一个 pipeline 里发 EVAL 和 `Do("WAITAOF")`;纯函数 `checkWaitAOFReply`、`waitAOFBudgetMs`;消费方接口 `RouteStore`、选项 `WithRouteStore`、五个哨兵错误。单测 28 个顶层用例(miniredis + 假 store) |
| `go/data_service/internal/routetool/redisread.go`、`importer.go`、`auditor.go`、`routetool_test.go`、`routetool_integration_test.go` | `import-from-redis` 与 `audit` 的逻辑;键名与值的规范形式校验;import 开头 / 结尾各快照一次 `merge:merged_into:*`,有变化即退出码 3(窗口内跑完的合服也抓得到);audit 方向 B 补比方向 A 没比过的 id(含方向 A 之后才提交的行)。单测用 miniredis + 假 routeDB,集成测试用真库 + miniredis |
| `go/data_service/cmd/route_tool/main.go`、`main_test.go` | 薄入口:解析参数、装配、映射退出码(0 / 1 / 2 / 3);yaml 是 db_first 时真导入要显式 `-allow-db-first`,否则退出码 2 |

修改(23 个):

| 文件 | 要点 |
|---|---|
| `store/schema.go`、`store/mysql.go`、`store/export_test.go` | `bootstrapTables` 加四段 DDL,共 6 项;`zone_merge_fence` 主键 `CLUSTERED`(偏离第 16 条)。新增 `routeTableShapes` 与 `assertRouteTableShapes`;从 `assertColumnsPresent` 抽出 `assertColumnNamesPresent`;迁移末尾用 ODKU 种 `route_meta(id=1)`。改了表数注释。`mysql.go` 拆出 `openMySQLWithDSN`,新增 `buildRouteDSN`。`export_test.go` 导出测试钩子 |
| `routing/router.go`、`router_test.go`、`router_integration_test.go` | `NewRouter(c, opts...)` 先 Normalize + Validate RouteStore 段,非法就 panic,在建任何连接之前。`RegisterPlayerZone` 第一行按 Mode 分流,redis 分支逐字不动。删除 `RemapHomeZoneForMerge` 和 `ErrMergeFenceMissing`。`DeletePlayerZone` / `DeletePlayerZoneIfLocked` 注释写明生产路径已停用。单测删掉 3 个 remap 用例。集成测试 `itKeyPatterns` 加 `player:maprev:*`;remap 用例去掉 remap 步骤,改名为 `TestMergeFence_RealRedisBlocksRegister`;新增 4 个用例 |
| `server/dataserviceserver.go`、`dataserviceserver_test.go` | W1 错误映射加两支:`ErrInvalidRegistration` → Unavailable + 14,`ErrRouteStore` → Unavailable + 30,契约注释同步。W2 在鉴权之前恒回 FailedPrecondition + 16(`ErrCodeNotImplemented`),零变更。W3(`DeletePlayerData` 带 `delete_zone_mapping=true`)回 FailedPrecondition + 16。5 个 remap 用例换成 `TestRemapHomeZoneForMerge_AlwaysFailedPrecondition`;新增 `TestDBFirstRegister_ErrorMapping`、`TestDeletePlayerData_DeleteZoneMappingIsFailedPrecondition` 和辅助函数 `newDBFirstTestServer` |
| `logic/data_logic.go`、`data_logic_test.go` | `DeletePlayerData` 入口零副作用拒绝(哨兵 `ErrZoneMappingDeletionDisabled`),删掉不可达的删映射分支;用例改写为 `TestDeletePlayerData_ZoneMappingBranchDisabled` |
| `constants/error_codes.go` | 新码 `ErrCodeRouteStoreError = 30`;24(`ErrCodeMergeFenceMissing`)注释改为「已停用,号不复用」 |
| `config/config.go`、`config_test.go` | `RouteStoreConfig` 及 `Normalize` / `Validate` / `DBFirst` / `WaitsForAOF` / `NumLocal` / `NumReplicas`;AdminToken 与表数注释。用例:缺省值、拒绝表、显式写 0 关掉 WAITAOF、仓库 yaml 为 redis 且合法 |
| `svc/servicecontext.go` | 接口 `PlayerRouteStore`(`routing.RouteStore` 加 `Close`)。只在 db_first、迁移成功、且 mapping Redis 过了 WAITAOF 能力探针(`routing.ProbeWaitAOF`)时装配,失败打 ERROR、保持 nil;只在非 nil 时注入 Router,避开「非 nil 接口包着 nil 指针」。`MissingStores` 在 db_first 时带 `player_route`(`servicecontext_missing_stores_test.go` 表驱动钉住 db_first / redis / 空 Mode 三种);Close 时关闭 |
| `data_service.go` | `-migrate` 之后校验 RouteStore 段,非法就拒启(exit 1)。db_first 时:打参数 Info 和冻结窗口 WARN,横幅加 route 行,调 `metrics.PrimeRoute()`。flag 注释、迁移输出文案、故障码注释(补码 30) |
| `cmd/debugutil/debugutil.go` | db_first 时装 route store(writer `data_service_debug@{host}`),cleanup 时关闭;打不开就报错,不退化成 redis |
| `metrics/metrics.go`、`metrics_test.go` | 两个计数器与 `PrimeRoute`,见下「运行时要点」 |
| `etc/data_service.yaml` | 新增 `RouteStore:` 段:显式 `Mode: redis`,写满全部键,注释里写明切换前提;改 AdminToken 与表数注释 |
| `tools/scripts/k8s_deploy.ps1`、`tests/k8s_deploy_contract.tests.ps1` | data-service ConfigMap 在 `Store:` 之后加 `RouteStore: Mode: redis`;「四张表」注释改成不写数量(只改注释)。契约测试加 1 个用例:dev / prod 两档 ConfigMap 与服务 yaml 都是 redis |
| `tools/scripts/stress_round19.ps1`、`currency_crash_window.ps1` | 新参数 `$GlobalDbName='testdb'`。清库时三张路由数据表 TRUNCATE,`route_meta` 原地复位(不换 generation),只处理已存在的表。SQL 失败就 throw,比 zone 表那段更严格,理由写在脚本注释里(规格 Q7) |

文档(10 个):本节;`docs/PROGRESS.md`;`docs/README.md`;[player-storage-placement.md](./player-storage-placement.md) §4.4 / §11.4;[merge-zone-runbook.md](../ops/merge-zone-runbook.md) §3 / §9.3 / §12;[server_merge_design.md](./server_merge_design.md);[enter-scene-zone-routing.md](./enter-scene-zone-routing.md);[global-data-layer-tidb-decision.md](./global-data-layer-tidb-decision.md) D7 / §4 / §6;`ARCH.md` 决策 21 的行尾注;[复验清单](../handoff/placement-reverify-checklist-20261012.md)第 4 步附录。

**运行时要点(db_first)**

- **W1 的顺序**(`route_publish.go`):
  1. 参数校验:player_id ≠ 0,home ∈ [1, 999999];
  2. store 是否装配;
  3. Redis 预检,两次往返:先 EXISTS `merge:in_progress:{home}`(与 redis 模式同一个 `IsMergeInProgress`);再一次 MGET 取 zone / placement / maprev。zone 是别的值就判冲突、不进库;三键任一在,就设 `ExistingRowOnly`,库里必须已有这一行;
  4. `RegisterHome`:普通读 `route_meta.writable` → 围栏分片行 `FOR UPDATE`(分片行不存在时在事务外懒建)→ ODKU 插入 rev=1,或 `FOR UPDATE` 加锁读之后补 home → COMMIT;
  5. 发布:脚本 + WAITAOF,同一条连接。

  事务里不做任何 Redis I/O。
- **`RegisterHome` 的结局**:inserted / filled_home / filled_home_placement_kept / already_same / conflict / fenced / row_missing。只有故障才返回 err。1213 / 9007 有界重试(共 4 次,10–50ms 抖动);1205 不重试(偏离第 15 条)。
- **发布脚本**(KEYS 第 4 把是围栏键):
  - `{1, rev}`:三键已写入;
  - `{0, cur, h, p}`:缓存已是这一版或更新,把 maprev 原值写回一次(touch),让随后的 WAITAOF 覆盖缓存现状;
  - `{2, '', h, p}`:缓存没有 maprev,遗留值又与本次不同,零写入;
  - `{3}`:围栏存在,零写入。

  Redis 错误回复、回复形状不对,都不重试。网络错误和 WAITAOF 份数不足,退避后整条 pipeline 重发,最多 `PublishAttempts` 次。WAITAOF 的等待上限 = min(`WaitAOFTimeoutMs`, ctx 剩余 − 100ms),截断成整毫秒后不足 1ms 就不发(timeout 为 0 会无限阻塞)。
- **配置 `RouteStore`**:

  | 键 | 默认 | 合法范围 |
  |---|---|---|
  | `Mode` | redis | `redis` 或 `db_first`,大小写敏感 |
  | `FenceShards` | 8 | [1, 64] |
  | `MaxOpenConn` / `MaxIdleConn` | 20 / 5 | `MaxOpenConn > 0`,`0 ≤ MaxIdleConn ≤ MaxOpenConn` |
  | `PublishAttempts` | 3 | [1, 5] |
  | `WaitAOFNumLocal` | 1 | {0, 1} |
  | `WaitAOFNumReplicas` | 0 | [0, 16] |
  | `WaitAOFTimeoutMs` | 1500 | [1, 1500] |

  任一非法就拒启,与 Mode 无关。WAITAOF 两项都显式写 0 = 不发 WAITAOF,只给单测和没有 AOF 的环境用。
- **指标**:只在 db_first 时有数,启动时预置为 0。
  - `data_service_route_register_total{outcome}`,12 个取值:inserted、filled_home、filled_home_placement_kept、already_same、conflict、fenced、refused_unknown_row、precheck_conflict、precheck_fenced、precheck_error、store_error、invalid。每次 db_first 的 W1 恰好计一次,合计 = 请求数;precheck_error 是 Redis 预检本身出错(EXISTS / MGET 失败、MGET 长度不对、缓存 zone 解析不了),评审后补上(规格原为 11 个,不含它);
  - `data_service_route_publish_total{result}`,8 个取值:applied、confirmed、superseded、conflict、legacy_conflict、fenced、not_durable、error。WAITAOF 回复形状不对(不是两个非负整数)是确定性错误、不重试,计 `error` 而不是 `not_durable`。

  player_id 和 rev 只进 ERROR 日志,不做 label。
- **错误码**:新增 30 `ErrCodeRouteStoreError`;24 `ErrCodeMergeFenceMissing` 不再产生,号不复用。

**语义对照(规格 c.3)**

gRPC code 的集合与今天相同:OK、AlreadyExists、FailedPrecondition、Unavailable。新码 30 和 Unavailable 下的码 14 只出现在 message 里;login 只看有没有 err。

| 场景 | redis 模式(今天) | db_first | gRPC 结果 |
|---|---|---|---|
| 新玩家 | Lua SETNX | 预检无键 → 插入 rev=1 → 发布 `{1}` + WAITAOF | OK |
| 同 home 重试 | 不写 | 预检见到键 → 加锁读同 home → 重新发布(补齐上次失败的发布) | OK |
| 异 home | Conflict | 预检或加锁读发现异值 | AlreadyExists + 21 |
| 既有值畸形 | unparsable | 预检用同一文案 | Unavailable + 1 |
| 围栏:预检命中 | EXISTS | 同一个函数 | FailedPrecondition + 22 |
| 围栏:预检之后才立 | 写入脚本复核,两键都不写 | 发布脚本 KEYS[4] 复核,三键都不写;**库里已提交的行留下(id 已烧掉)** | FailedPrecondition + 22 |
| 围栏:库分片行 | 不存在 | `FOR UPDATE`;**本批恒不生效**,见下 | FailedPrecondition + 22 |
| home 缺、落点在 | 写 home,保留落点,打 ERROR | 补 home(R6a),打同一条 ERROR | OK |
| 缓存无 maprev 且遗留值不同 | 不存在 | 发布脚本回 `{2}`,零写入,打 ERROR | Unavailable + 1 |
| Redis 认得、库里无行 | 不存在 | 拒绝(row_missing),提示先导入 | Unavailable + 30 |
| 库故障、store 没装配、meta 缺失、writable≠1、1205 | 不存在 | `ErrRouteStore` | Unavailable + 30 |
| 已提交,但发布失败或未确认落盘 | 不存在 | 发布错误。login 照常 fail-closed 并释放名字,重试走幂等分支补发 | Unavailable + 1 |
| player_id = 0 或 home 越界 | 照写 | `ErrInvalidRegistration` | Unavailable + 14 |

**P2 切换门禁**(切 `RouteStore.Mode=db_first` 之前,缺一不可)

1. **冻结窗口**:从任一 data_service 副本切到 db_first 起,到第 2 批落地为止,禁止 merge_zone 的一切写模式:merge、unmerge、relocate、pin-placement、backfill。这与 §7 P1 是同一个窗口,原因见下一段。
2. **P1 导入完成且审计为 0**:`route_tool import-from-redis -dry-run=false` 退出码 0,随后 `route_tool audit` 退出码 0。步骤见 runbook §9.3。K8s 镜像只构建单一入口,生产上要等第 1b 批把 route_tool 并进 data_service 主程序才能跑(偏离第 8 条)。
3. **mapping Redis**:`noeviction` + `appendonly yes`,版本 ≥ 7.2(WAITAOF)。按拓扑填 `WaitAOFNumLocal` / `WaitAOFNumReplicas`,redis-ha 档 NumReplicas 写 1。db_first 装配 route store 之前会按这两项发一次 WAITAOF 做能力探针(`routing.ProbeWaitAOF`,timeout = `WaitAOFTimeoutMs`):报错或份数不足就不装配,K8s(`Store.Required=true`)拒启。理由:这类配置错误是确定性的,留到请求期就是每次 W1 都失败,而 go-zero 给 mapping Redis 套的熔断器把 Redis 错误回复都记成失败,熔断一开,同一客户端上的 GetPlayerHomeZone、BatchGet、玩家锁也被按概率丢弃。
4. **建角压测**:按 AGENTS.md §6 出 W1 p99 与「发布未确认率」的对比表,阈值由负责人看表拍板(规格 Q16)。发布未确认率 = `route_publish_total{result=~"not_durable|error"}` 占总数的比例:WAITAOF 回复形状不对计在 `error`(确定性、不重试),只看 `not_durable` 会漏掉它们。不可接受时二选一:给 mapping 实例配 `appendfsync always`(§10 第 5 条),或调整服务端标量 Timeout,并同步更新 `Assert-GrpcClientDeadlineBudget` 的登记。
5. **切换本身**:改 yaml(K8s 改 ConfigMap 生成器里那一行),滚动重启,启动日志里应能看到那条 `WARN RouteStore.Mode=db_first: ...`。全部副本切完之后,按 §7 P3 再导入一次(yaml 已是 db_first,真导入要显式 `-allow-db-first`,建议小批量)、再审计一次。除 P3 这一次之外,切 db_first 之后不再跑真导入:那时库是真源,从缓存回填库方向是反的。

**db_first 下,库内围栏在本批恒不生效**

`zone_merge_fence` 已经建表,W1 每次也都 `FOR UPDATE` 锁一个分片行。但第 1a 批没有任何写者写 `run_id`:merge_zone 仍只写 Redis 的 `merge:in_progress:*`,从不写这张表。所以库内复核的结果恒为「未封锁」。

提交点的合服围栏复核,靠的是发布脚本的第 4 把键 `merge:in_progress:{home}`:命中时三键零写入、回 22,但**库里已提交的行会留下**(见下一段)。第 2 批 merge_zone 改成库先写之后,库内复核才成为真正的提交点复核。在那之前,db_first 的安全前提就是上面第 1 条的冻结窗口:在 db_first 下跑 relocate、pin、backfill,会改出一份库不知道的 Redis 值。遗留值守卫能挡住「之后的 W1 重发把它改回旧值」,挡不住「库与缓存长期分叉」,这类分叉只能等 audit 发现。导入工具也只能检测 merge / unmerge 立的围栏。

**已提交未发布的孤儿行**

发布失败、WAITAOF 超时、发布时撞上围栏,都会在库里留下一行 id 已烧掉的映射,而 W1 对调用方返回失败。`route_tool audit` 把三族键全缺的计为 `db_only_unpublished`,部分在的计为 `missing_in_redis` / `value_mismatch`,退出码都是 1。WAITAOF 超时那种情况,缓存其实已经写入、只是没确认落盘,audit 看不出差异,直到 Redis 丢了这次写才变成 `db_only_unpublished`。本批**没有**自动补发,要等第 1b 批对账 leader 的全量预热推送。因此:

- db_first 下,P3 的「audit 为 0」门禁要等第 1b 批落地之后才可达;
- 第 1b 批推送之前,先按 audit 清单人工核对:哪些行的 home 指向了已合走的 zone(只有违反冻结纪律时才会出现);
- 发布未确认的那种情况,还会同时留下 Redis 键,即 login `createplayerlogic.go:229-236` 说的「幽灵映射」。redis 模式下响应丢失也会出现同类问题,只是频率更低。

**相对本提案的偏离(规格 g.2,编号一致)**

| # | 偏离 | 理由 | 代价 |
|---|---|---|---|
| 1 | 不新增 proto message(§3.1) | 避免依赖 proto 重新生成和 C++ 重编 | 只守列名和主键,不守类型。第 2 批补 proto 时删掉 `routeTableShapes`;`RouteRow` 要么改用生成类型,要么明确作为领域类型保留并写清转换(AGENTS.md §3 不许有与 proto 重复的并行 struct)。DDL 列注释已预留 `pb:1..9`,补 proto 时沿用同样的编号 |
| 2 | 用 EVAL,不用 EVALSHA(§4.1 第 4 条;**任务原文写的是 EVALSHA**) | pipeline 里遇到 NOSCRIPT 无法透明回退;与仓库现状一致(router.go 全部用 `EvalCtx`);W1 的频率等于建角频率 | 每次多发约 1.5KB。第 2 批 merge_zone 批量发布时重新评估 |
| 3 | 发布脚本(§4.2)加了 rev 校验、可选的围栏键、遗留值守卫、touch、逐字节比较 | 提交点复核围栏;不静默覆盖遗留值;touch 让幂等重发也能用 WAITAOF 确认落盘;逐字节比较不依赖 locale | 返回 0 时多一次 SET;每次多两次 GET |
| 4 | 发布的 I/O 放在 data_service routing,`go/shared/placement` 只放脚本、键和回复分类;没有 `commitAndPublish` helper(§4.1 第 6 条) | `shared/placement` 约定只依赖标准库 | 第 2 批 merge_zone 要自己写 go-redis 的 I/O,并镜像脚本 |
| 5 | **db_first 保留 Redis 预检**(EXISTS 围栏 + MGET 三键),没有删掉 EXISTS 预检(§4.3 W1) | 第 2 批之前,围栏和清单都在 Redis;库可能缺行(未导入或回档) | 多两次 Redis 往返 |
| 6 | 分片行「确保存在」改成懒执行:加锁读读不到才建 | 避免每次 W1 都在事务外做一次 autocommit 写 | 语义不变 |
| 7 | 新增契约 `MaxRouteFenceShards = 64`:围栏持有者必须锁住并写满全部 64 行 | 各进程 N 配置不同时,N 大的写者会落到持有者没锁的分片上,绕过围栏 | 获取一次围栏要锁 128 行 |
| 8 | route_tool 是独立的 cmd(§7 P1 写的是 `data_service -route-import-from-redis`) | 按任务要求 | **K8s 镜像只构建单一入口**(`deploy/k8s/Dockerfile.go-svc:10`),生产上跑 P1 要等第 1b 批补主程序入口 |
| 9 | W3 改为在 logic 入口零副作用拒绝,映射成 gRPC FailedPrecondition | 不能先删数据键、再拒删映射,留下半截操作 | DeletePlayerData 原本只回 in-band 码,这是唯一的例外;仓库内没有调用方 |
| 10 | 用 `Do("WAITAOF")`,不用 go-redis 的 `WaitAOF` | go-redis v9.18.0 会把数组回复错解析成 int | 无 |
| 11 | 新增码 30 `ErrCodeRouteStoreError` | 告警能分清「库坏」和「Redis 坏」 | 只出现在 message 里 |
| 12 | W2 在鉴权之前就恒拒,并删除 `Router.RemapHomeZoneForMerge` | 无条件 SET 的危害见 `merge_run.go:836-837`;仓库内只有注释提到它 | 原来的 5 个服务端用例和 3 个路由用例要改写;guild、login、match、trade、merge_zone 里提到它的注释暂不改(记入「已知未改」) |
| 13 | 清库脚本对 `route_meta` 原地复位,不 TRUNCATE | `currency_crash_window.ps1` 在服务运行中清库;TRUNCATE 会让 db_first 的 W1 一直 fail-closed,直到下次迁移 | 第 4 张表不是「清空」而是「复位」;generation 不换 |
| 14 | 参数非法时 db_first 回 **Unavailable + 码 14**,不回 InvalidArgument | 与今天的 RPC 契约一致:gRPC 码集合保持 OK、AlreadyExists、FailedPrecondition、Unavailable | 语义上「参数错」落进了 Unavailable,靠码 14 区分;只有 db_first 可达;redis 模式下仍会写入垃圾,与今天逐字相同 |
| 15 | 会话 `innodb_lock_wait_timeout=1`;**1205 不重试**,只重试 1213 和 9007(§4.1 第 2 条写的是「1213/1205 重试 3 次」) | 默认 50s 的锁等待在 2s 预算里永远等不到 1205,ctx 到期后服务端还会继续占着分片锁;guild 先例 | 锁争用超过 1s 时直接 Unavailable;导入遇到 1205 时整体失败,靠重跑(幂等)恢复 |
| 16 | `zone_merge_fence` 主键加 `/*T![clustered_index] CLUSTERED */`(§3.2) | TiDB 建表后不能改聚簇属性;聚簇才能按 zone 值 SPLIT | MySQL 上没有影响 |
| 17 | 导入写入 `rev = maprev`;库 rev 小于 maprev 时不写 | 防止导入把「库被回档」洗白成 rev 倒退,导致之后的发布被静默判成 Superseded | 导入多读一族键 |
| 18 | `WaitAOFTimeoutMs` 默认 1500ms;「发布未确认率」列为 P2 门禁 | `everysec` 下本地确认最长约 1s 以上,1000ms 正压在边界上 | W1 最坏耗时接近 2s;切换之前要先压测 |

仓库内没有 W2 / W3 生产调用方的证据:grep `RemapHomeZoneForMerge`,除 data_service 自身外只出现在 guild、login、match、trade、merge_zone 的注释里,robot 里只有生成的消息 id,cpp、java、ps1 中都没有;`DeleteZoneMapping` 只在 data_service 内部和测试里出现。

**落码时相对定稿规格的取舍**(都朝 fail-closed 或更可测的方向,逐条可在代码注释里找到理由)

- **store**:
  - 导入的两个事务也先读 `route_meta.writable`(§4.1 第 2 条:所有映射写者先读它)。
  - 入参校验比规格严:`ImportRoutes` 拒绝 player_id 为 0、home 大于 999999、home 与落点都缺、落点不合法;`ImportMergedInto` 要求 src / dst 非零且不同、run_id 合法;`RouteStoreOptions` 要求分片数在 [1, 64]、writer 非空、不超过 96 字节、只含允许的字符。
  - `GetRoutes` 复用 dry-run 的批量读语句,多一个 `ORDER BY player_id`,结果相同。
- **routing / server**:
  - 发布前拒绝 PlayerID、HomeZone 为 0 或 rev 非法的行;编排层核对 `RegisterHome` 回来的行与请求是同一名玩家、同一个 home,对不上返回 `ErrRouteStore`。
  - WAITAOF 回复形状不对不重试,只有份数不足才重试(规格 c.1 第 4 步原写「不满足就记 not_durable 并重试」;形状错误是确定性的,重试无益,改为 fail-closed 返回,计 `result="error"` —— P2 门禁的「发布未确认率」因此要把 `error` 一并算上,见上)。
  - 指标口径:Superseded 且 home 不同计 `result="conflict"`;Redis 预检本身出错(EXISTS / MGET 失败、MGET 长度不对、缓存值解析不了)计 `route_register_total{outcome="precheck_error"}`(评审后新增的第 12 个取值;起初不计,但 W1 的 error_code 只在 gRPC message 里,grpcstats 分不出码 1 与码 30,不计这类拒绝在指标上就看不见)。
  - AdminToken 注释写成「守三个 Rollback* 与 ReleasePlayerName」,因为代码里 ReleasePlayerName 也用这个 token。
  - debugutil 在 db_first 下打不开 route store 就报错,不退化成 redis。W2、W3 被拒时打带调用方身份的 ERROR。
  - 测试手法:命令计数用 miniredis 的 `SetPreHook`;真 Redis 对照向量放在集成测试文件里,为的是让 routing 单测数保持 54;`TestDBFirstRegister_RealStores` 遇到 appendonly 关闭时关掉 WAITAOF、只记日志,真实的 WAITAOF 路径由 `TestPublishRoute_RealRedisWaitAOF` 负责。
- **route_tool / 脚本**:
  - 配置用 `conf.Load` 而不是 `MustLoad`:后者出错时直接退出码 1,破坏「失败一律 2」。logx 改写到 stderr、只留 Error 级别,stdout 只有一行统计。装配时显式 PING mapping Redis,上限 5s。
  - import 的 merged_into 冲突与畸形也算退出码 1;结尾命中围栏(退出码 3)时仍输出统计行。
  - import 开头(围栏检查之后)与结尾各原样快照一次 `merge:merged_into:*`,有新增 / 消失 / 改值即退出码 3(规格 e 节只要求结尾复查围栏;一次在两次围栏检查之间开始并跑完的合服 / 撤销合服会删掉围栏,只查围栏抓不到)。结尾退出码 3 之后**不能原样重跑**:窗口内被合服改写的玩家,库里是旧 home、缓存里是新 home,导入从不覆盖非 NULL 值,重跑只会判 conflict、永远收敛不了。恢复办法写在 runbook §9.3「重跑与恢复」:只限 P1(库还不是真源),窗口外清空 `player_route` / `zone_merged_into`、`route_meta` 原地复位,再从头导入。
  - yaml 是 db_first 时,`import-from-redis -dry-run=false` 默认拒绝(退出码 2),要显式 `-allow-db-first`:那时库是真源,从缓存回填库方向是反的,导入事务的 `FOR UPDATE` 还会让并发 W1 吃 1205。唯一合法的用法是 §7 P3 那一次。
  - audit 方向 B 对方向 A 没比过的 id 都补比一次(规格原写「库里有行的已在 A 遍比过,跳过」):方向 A 翻页扫过之后才提交的行(db_first 下审计期间新登记的玩家)否则在本次审计里完全没比对过。
  - 只有 maprev、没有 zone 和落点的玩家判 MALFORMED。audit 里 merged_into 畸形和键名不规范计入 `redis_malformed`;maprev 缺席不算差异(第 1a 批 redis 模式下没有写者写它);按「玩家 × 差异种类」计数,复核取两次读取的交集。
  - 清库 SQL 连接时不指定默认库:先查 information_schema,写入前 `USE <db>`;`GlobalDbName` 只接受普通标识符。两份脚本各有一份同样的函数,注释写明「改一处两处同改」。
  - 规格里可选的 `tools/scripts/tests/wipe_global_route_tables.tests.ps1` 没有做。

**评审修复(2026-10-12,同日;同样未编译、未测试)**:除上面几条已并入正文的(precheck_error、WAITAOF 启动探针、merged_into 快照、`-allow-db-first`、audit 方向 B),还有:

- 测试:`svc/servicecontext_missing_stores_test.go` 补 db_first 缺 store 必报 `player_route`、redis / 空 Mode 不报的表驱动用例,以及 WAITAOF 探针失败不装配的用例(放在 svc 包,routing 单测数仍是 54);`route_store_contract_test.go` 删掉「`sql.Null[uint32]` 当参数必被驱动拒绝」的反向断言 —— Go 1.24 起 `Null[T].Value()` 先过 `driver.DefaultParameterConverter`,uint32 会被转成 int64 放行,那条断言在 go 1.26 上会红;绑定继续用 `nullU32` / `nullU64`,理由改写为「`Null[T]` 的转换随 Go 版本变化,统一走 0 = NULL 的领域约定」;`route_publish_test.go` 的「发布失败后重试补齐」改为在新地址的 miniredis + 新 Router 上重试(go-zero 按地址缓存连接池,同地址 Restart 会拿到旧连接、在熔断器上攒失败,结果取决于墙钟与平台);真库用例 `LockWaitBoundedAfterClientCancel` 把 p 的客户端超时从 300ms 放宽到 700ms(余量 ≈ p 的超时 − p 走到 R4 前的往返,超时越长余量越大;仍 < 1s),q 遇 1205 在用例里重试一次以吸收 InnoDB 锁等待超时的检查节拍。
- 脚本:两份清库脚本的 `Invoke-GlobalDbSql` 在函数内局部 `$ErrorActionPreference='Continue'`,成败只看 `$LASTEXITCODE`(否则 Windows PowerShell 5.1 / pwsh < 7.2 会把 mysql 的口令警告当 NativeCommandError 抛出);`k8s_zone_rollback.ps1` 的 FLUSHDB 共享键清单(第 0 批)补 `player:maprev:*`,`k8s_zone_rollback_flush_guard.tests.ps1` 期望同步。
- 注释:`constants/error_codes.go` 码 23 与分节标题、`config/config_test.go` 的 AdminToken 分节与用例注释,写明 remap 已停用、token 现在只守三个 Rollback* 与 ReleasePlayerName 的运维路径;`mysqlLikeConvert` 注释写明只复刻本 store 用得到的子集。
- 文档:本文 H1 带上状态,与 `docs/README.md` 索引行逐字一致;`tools/scripts/gen_docs_index.py` 的「玩家数据与存储」规则补 `player-route-mapping-*`,索引行按字母序挪到 `player-storage-placement.md` 之前。`gen_docs_index.py --check` 仍可能因工作区里别人新增 / 改动的其它条目报过期(例如 `tidb-phase1-and-launch-readiness.md` 没登记分类、是手工插的行),那些不归本批。runbook §9.3 同步上述行为与恢复步骤。

**仍未解决的风险(规格 g.3,编号一致)**

1. **TiDB 热点**(MySQL 上不存在):
   - `player_route` 的主键唯一索引和 `idx_route_updated` 都单调递增,NONCLUSTERED + SHARD_ROW_ID_BITS 只打散行数据、打散不了索引尾部;`idx_route_home(home_zone, player_id)` 在单个 zone 内同样是尾部热点。
   - **`zone_merge_fence` 整张表只有「zone 数 × N」行,必然落在同一个 Region。** TiDB 悲观事务的 `FOR UPDATE` 要往 TiKV 写锁记录,全服每次建角都在同一个 Region Leader 上加锁、解锁;分片只拆开了行锁排队,拆不开 Region 热点。TiDB Phase 1 上线前必须 `SPLIT TABLE zone_merge_fence BY (2,0),(3,0),…` 按 zone 预切并纳入压测(已登记进 TiDB 决策 §4 第 11 行、§6 第 8 条)。
   - `route_meta` 是单行,每次 W1 都要读,是读热点。`idx_route_updated` 本批没有读方,保留它是因为 bootstrap 只执行 `CREATE TABLE IF NOT EXISTS`,以后补索引要另写收敛代码。
2. **TiDB 的 CHECK 约束**要求 ≥ 7.2 并开启 `tidb_enable_check_constraint`(默认关),所以应用层校验不能省。
3. **`writable` 是普通读**:第 1b 批把它置 0 时,正在进行的事务仍会提交,1b 的流程要先等这些事务结束。
4. **db_first 依赖冻结纪律**:见上面「库内围栏在本批恒不生效」。
5. **P2 混跑窗口**:同一个 player_id 被新旧副本同时用不同 home 注册时,有遗留值守卫兜着,最多一边成功,另一边回 Unavailable 或 AlreadyExists;残留问题是「库是 B、Redis 是 A」的分叉,由 audit 发现。player_id 每次建角都新发,实际几乎不可能发生。
6. **已提交未发布的孤儿行**:见上面同名一段。
7. **库被重置后重新种的 `route_meta` 也是 `writable=1`**:靠预检的「缓存认得 ⇒ 只认已有行」、发布时「Superseded 且 home 不同就失败」、导入的 rev_regression 三处兜底;回档的自动检测要到第 1b 批。
8. **新的启动依赖**:新表的 DDL、形状守卫、meta 种行进了 `MigrateSchema` 的关键路径,**redis 模式也受影响**。任一步失败,快照、流水、号段、名字四个 store 都置 nil,本地禁止建角;K8s 下 `Store.Required=true` 直接拒启。所以「redis 模式与今天逐字相同」只适用于 W1 的行为。本机 MySQL 是 `mysql:latest`、K8s 是 `mysql:8.0`,两边都要跑一次迁移。
9. **WAITAOF 的延迟与 go-zero 的细节没有实测**:`PipelinedCtx` 是否就是 go-redis 的同连接 pipeline、ReadTimeout 与 MaxRetries 的取值、redis 熔断器怎么计数脚本的错误回复。由验证第 4 步的 CLIENT ID 夹心用例给出部分证据;本机 compose 的 mapping Redis 是 `redis:latest`,WAITAOF 要求 ≥ 7.2。熔断器把 pipeline 回的 Redis 错误回复都记成失败(go-zero 的 acceptable 只放过 nil / redis.Nil / context.Canceled):AOF 配错这类**确定性**错误已由启动期 WAITAOF 探针挡住;运行期仍会计入熔断的只剩个别玩家的畸形 maprev(脚本拒绝)与 WRONGTYPE,影响面取决于它们的频率。彻底隔离要按 Q3 的备选给发布单独建一个不经 go-zero 熔断器的 go-redis 客户端,本批不做。

**待定问题(规格 Q;本批按推荐答案做)**

| # | 问题 | 本批做法 |
|---|---|---|
| Q1 | route_tool 在 K8s 上怎么跑 | 本批是独立 cmd;第 1b 批在 `data_service.go` 加 `-route-tool <sub>`,复用 `internal/routetool`,与 `-migrate` Job 同形 |
| Q2 | EVAL 还是 EVALSHA | EVAL(偏离第 2 条),向负责人点明 |
| Q3 | go-zero `PipelinedCtx` 还是独立 go-redis 客户端 | go-zero。由 `TestPublishRoute_RealRedisSameConnection` 定;FAIL 就改用独立 `goredis.NewClient`(DB 0,ReadTimeout = WAITAOF 上限 + 1s)专做发布 |
| Q4 | miniredis 是否支持 WAITAOF | 未核实;单测一律显式配 0 / 0,真实 WAITAOF 只在真 Redis 集成测试里验 |
| Q5 | db_first 是否保留 Redis 预检 | 保留到第 3 批;发布脚本的 KEYS[4] 要等库内围栏真正生效之后再评估 |
| Q6 | `DeletePlayerZone` / `DeletePlayerZoneIfLocked` 删不删 | 保留,注释写明「生产路径已停用」,等删号设计 |
| Q7 | 清库时 route_meta 怎么处理 | 原地复位;三张数据表 TRUNCATE;只处理存在的表;SQL 失败即抛错 |
| Q8 | AGENTS.md §6.2 是否加「全局库路由表一并清」 | 推荐加(§8.2),但 AGENTS.md 工作区有别人未提交的改动,**本批没改,待负责人确认** |
| Q9 | 给 RegisterPlayerZone 加 MethodTimeouts | 本批不加;P2 压测之后再定,要加就同步登记 `Get-GrpcClientMethodsNotCalledFromCpp`,并注意 K8s 只镜像标量 Timeout |
| Q10 | 新码 30 | 采用 |
| Q11 | 导入遇到「库非 NULL、与 Redis 不同」 | 报告并跳过,退出码 1,不覆盖;rev_regression 同样处理 |
| Q12 | 导入时顺带写 maprev 或发布 | 不做,留给第 1b 批全量预热;遗留值守卫保证不写 maprev 也安全 |
| Q13 | W1 拒绝注册进已被合走的 zone(查 `zone_merged_into`) | 本批不做,记为后续加固项 |
| Q14 | Java 的 PARITY.md 何时登记 | 第 1 批(1a + 1b)全部完成时登记一行;本批只在 PROGRESS 写「Java 待做」 |
| Q15 | 锁冲突重试次数 | 1213 / 9007 共 4 次(1 + 3),最多多睡 150ms;1205 不重试 |
| Q16 | P2 门禁的阈值 | 负责人看 Codex 的压测对比表之后拍板,本批不预设数字 |

**第 1a 批交 Codex 的验证(规格 h 节,步骤编号一致)**

先后顺序:

- 先按 [placement-reverify-checklist-20261012.md](../handoff/placement-reverify-checklist-20261012.md) 复验,**以第 4 步之后追加的附录为准**:计数与用例名已因第 0 批和第 1a 批改变;
- 再跑上面「第 0 批交 Codex 的验证」;
- 最后跑下面第 1–9 步。全部在仓库根目录、用 PowerShell 执行。

第 1–9 步:

1. **shared**:

   ```powershell
   cd go/shared
   gofmt -l ./placement           # 无输出
   go vet ./placement/...
   go test ./placement/... -count=1 -v -run "Route|Rev|Publish|MapRev|Keys"
   go test ./... -count=1
   ```

   placement 包顶层用例由 9 个变为 15 个(新增 `route_test.go` 6 个)。
2. **data_service 单元测试**:

   ```powershell
   cd go/data_service
   gofmt -l ./internal ./cmd ./data_service.go   # 无输出
   go vet ./...
   go build ./...
   go test ./... -count=1
   go test ./internal/routing/... ./internal/server/... ./internal/logic/... ./internal/config/... ./internal/store/... ./internal/routetool/... ./cmd/route_tool/... -count=1 -v -run "Route|Publish|DBFirst|Remap|DeletePlayerData|ShippedYaml|WaitAOF|NoPlayerRouteWrites|DecideImport|RegisterHome|ImportRoutes|NewRouteStore"
   go test ./internal/svc/... ./internal/metrics/... -count=1 -v -run "MissingStores|PlayerRouteStore|PrimeRoute"   # 评审修复补的 svc 用例与 12 个 outcome
   go test ./internal/routing/... -list . | Measure-Object -Line   # 期望 54 个顶层用例 + 1 行 ok
   ```

   通过标准:全绿。既有失败按复验清单第 4 步归因,单独列出。`TestNewPlayerRouteStoreRefusedWhenWaitAOFProbeFails` 连一个刚关掉的本机端口,要几秒(go-zero PING 超时),属正常。
3. **MySQL 集成**。本机 compose 的 appuser 没有建库权限,用 root;口令取 `deploy/docker-compose.yml` 里的 `MYSQL_ROOT_PASSWORD`,不要写进日志。

   ```powershell
   $env:DATA_SERVICE_IT_MYSQL_HOST = '127.0.0.1:3306'
   $env:DATA_SERVICE_IT_MYSQL_USER = 'root'
   $env:DATA_SERVICE_IT_MYSQL_PASSWORD = '<见 compose>'
   go test -tags=integration ./internal/store/... -count=1 -v -run "Route|MigrateSchema"
   ```

   通过标准:没有 FAIL。`TestMigrateSchema_UpgradesLegacyHandWrittenTables` 从 09-29 起就红(复验清单 4c),它会被 `MigrateSchema` 选中,单独列出,不计入本批结论。出现 SKIP 如实报成「未验证」。保留 `TestRouteStore_ConcurrentManyPlayersSameZone` 打印的 lockRetries,以及 `TestRouteStore_LockWaitBoundedAfterClientCancel` 实测的耗时。
4. **Redis 集成**。必须用一次性 Redis,不能依赖开发库恰好为空:本机 compose 的 Redis 就是服务在用的 mapping Redis,用例在 `itKeyPatterns` 非空时会 Skip。

   ```powershell
   docker run -d --rm --name ds-it-redis -p 127.0.0.1:6390:6379 redis:7.2 redis-server --appendonly yes   # 本地没有该镜像时要拉取,先征得同意
   $env:DATA_SERVICE_IT_REDIS_HOST = '127.0.0.1:6390'
   go test -tags=integration ./internal/routing/... -count=1 -v
   docker rm -f ds-it-redis
   ```

   通过标准:
   - `TestPublishRoute_RealRedisSameConnection` 和 `TestPublishRoute_RealRedisWaitAOF` 都必须 **PASS,不能 Skip**。前者 FAIL 就按 Q3 改用独立 go-redis 客户端;
   - 第 3 步的 MySQL 环境变量还在时,`TestDBFirstRegister_RealStores` 也必须 PASS;
   - 带 integration tag 时顶层用例共 63 个(单测 54 + 集成 9)。
5. **工具与全量集成**:

   ```powershell
   go test -tags=integration ./internal/routetool/... -count=1 -v
   go test -tags=integration ./... -count=1
   ```

6. **PowerShell 契约测试**:`pwsh -NoProfile -NonInteractive -File tools/scripts/tests/k8s_deploy_contract.tests.ps1`,fail=0,total 由 87 变为 88。评审修复动了第 0 批的 FLUSHDB 共享键清单(补 `player:maprev:*`),再跑一次 `pwsh -NoProfile -NonInteractive -File tools/scripts/tests/k8s_zone_rollback_flush_guard.tests.ps1`,fail=0(用例数不变,仍是 8)。
7. **清库脚本:语法检查与计划输出**:

   ```powershell
   pwsh -NoProfile -Command "foreach(`$f in 'tools/scripts/stress_round19.ps1','tools/scripts/currency_crash_window.ps1'){`$e=`$null;[void][System.Management.Automation.Language.Parser]::ParseFile((Resolve-Path `$f),[ref]`$null,[ref]`$e);if(`$e){`$e;exit 1}}"
   pwsh -NoProfile -File tools/scripts/stress_round19.ps1 -Phase wipe   # 不带 -Execute,只打 plan
   ```

   通过标准:语法零错误;plan 输出里出现 `testdb` 的三张路由表与 `route_meta` 复位。(plan 不连库;`Invoke-GlobalDbSql` 在函数内局部改回 `Continue`、只认 `$LASTEXITCODE` 这一点,只有真清库时才走到。)
8. **可选冒烟(本机全栈)**。开始之前先确认这段时间不跑 dev_tools 的任何 merge-zone 命令(冻结窗口)。

   ```powershell
   cd go/data_service
   go run ./cmd/route_tool -f etc/data_service.yaml import-from-redis                  # dry-run,看统计
   go run ./cmd/route_tool -f etc/data_service.yaml import-from-redis -dry-run=false   # 真导入,期望退出码 0
   go run ./cmd/route_tool -f etc/data_service.yaml audit                              # 期望 0
   ```

   然后临时把本地 yaml 改成 `Mode: db_first`,重启 data_service,确认启动日志里有那条 WARN;用 robot 建一个角色,再跑 audit,应为 0;做完还原 yaml。也可以先按 AGENTS.md §6.2 清库再做。注意不要在 dry-run 之后直接期望 audit 为 0:Redis 里有历史玩家时,dry-run 不写库,audit 必然返回 1。
9. **不需要重编 C++**:本批没有 proto 变更。

**已知未改**:

- guild、login、match、trade、merge_zone 里提到 `RemapHomeZoneForMerge` 的代码注释(`go/guild/internal/logic/home_zone.go`、`go/login/internal/logic/pkg/homezone/homezone.go`、`go/match/internal/team/homezone.go`、`go/trade/internal/logic/home_zone.go`、`tools/merge_zone/{backfill_home_zone,merge_run,player_rows}.go`),留作后续统一更正。
- AGENTS.md §6.2 没有加「全局库路由表一并清」(Q8,待负责人确认)。
- 本提案 §4.1 / §4.2 / §4.3 / §7 的正文没有改写:EVALSHA、1213/1205 重试、删 EXISTS 预检、`data_service -route-import-from-redis` 等说法以上面的偏离表为准。
- [global-data-layer-tidb-decision.md](./global-data-layer-tidb-decision.md) 第 35、82 行(「结论」第 2 条与 D1)、[server_merge_design.md](./server_merge_design.md) §5.5 与 v2 Changelog 的历史叙述仍提到 `RemapHomeZoneForMerge`。它们是历史决策,不改写;前者只在 D7 加注,后者在文首修订提示、§3.5、§3.3 加注。
- [tidb-phase1-and-launch-readiness.md](./tidb-phase1-and-launch-readiness.md)(工作区里别人在途的提案)没有同步 `zone_merge_fence` 的 SPLIT 项,TiDB 决策 §6 第 8 条已登记。

**Java 版(AGENTS.md §12)**:待做。映射存储是服务端内部实现,不涉及客户端契约;Java `PARITY.md` 推迟到第 1 批(1a + 1b)全部完成时登记一行。

---

**第 1a 批热点修复(G1a,2026-10-12):`player_route` 聚簇片前缀 + `route_meta` 拆表 + 建池探针 P4 + `route_tool tidb-prepare`**

> 状态:**已落码,未编译、未测试**(AGENTS.md §10.1,待 Codex 按下面「验证增量」执行)。与第 1a 批一起交验证,任何红都要区分归属(第 0 批 / 第 1a 批 / G1a)。

**起因**。上面「仍未解决的风险」第 1 条:`player_route` 主键 `player_id NONCLUSTERED + SHARD_ROW_ID_BITS` 只打散行数据,主键退化成的唯一索引、`idx_route_updated`、单 zone 内的 `idx_route_home` 都是单调尾部;`PRE_SPLIT_REGIONS` 只切行数据,每条索引建表时只有 1 个 Region;号段发的 `player_id` 在每个 login 段内严格递增,首日数据量到不了按大小分裂的阈值,新插入始终打同一个尾部 Region(Load Base Split 只看读)。`zone_merge_fence` 整表一个 Region;`route_meta` 单行同时放「每次 W1 都读的 writable」和「1b leader 常写的水位 / 租约」。开服首日瓶颈核对把它列为 TiDB 档的首要问题之一([tidb-phase1-and-launch-readiness.md](./tidb-phase1-and-launch-readiness.md) §5.1)。**为什么现在做**:这几张表还没在任何环境部署,第 1a 批代码也未编译;TiDB 建表后不能改聚簇属性 —— 现在改只是改常量,上线后再改等于整表重写 + 双写迁移。

**改动清单**

| 位置 | 要点 |
|---|---|
| `go/shared/placement/route.go` | `RouteRowShards uint32 = 16`(写死、不可配)、`RouteRowShard(id) = id % 16`;写者与读者都只能经它算 |
| `store/schema.go` | `player_route` 聚簇 `(route_shard, player_id)` + `chk_route_shard` + 三条带片前缀的二级索引,去掉 `SHARD_ROW_ID_BITS / PRE_SPLIT_REGIONS`;`route_meta` 只留 `id / generation / writable`,新表 `route_reconcile`;五张表显式 `COLLATE=utf8mb4_bin`、ascii 列 `COLLATE ascii_bin`;`bootstrapTables` 7 项;形状守卫对 `player_route` 的任一失败附 G1a 处理提示;`ensureRouteTables` = 形状守卫 + 种 `route_meta` + 种 `route_reconcile` |
| `store/route_store.go` | 按 `player_id` 的全部 SQL 带 `route_shard`(R4–R9、R12、R13);R8 / R13 用行构造器 `(route_shard, player_id) IN ((?, ?), …)`,按聚簇键升序;R12 键集游标改 `RouteScanCursor{Shard, PlayerID}`;导入的 R7 / R9 按聚簇键升序执行;扫描读到错片行 → `ErrRouteShardMismatch`;建池探针 P4;R16 种对账行;W1 里围栏片改名 `fenceShard`、行片 `rowShard` 在事务函数内算 |
| `store/route_tidb.go`(新) | `PrepareTiDB` 与纯函数 `routeTiDBPrepareStatements`:判引擎 → 核对 `TIDB_PK_TYPE` → `ATTRIBUTES 'merge_option=deny'` ×2 → `SPLIT` ×(3 + zones + storages) |
| `routetool/{importer,auditor}.go` | `routeDB.ScanRoutes` 签名改游标类型;audit 方向 A 沿聚簇键翻页 |
| `routetool/tidb_prepare.go`(新)、`cmd/route_tool/main.go` | 子命令 `tidb-prepare -zones=… [-storages=…] [-dry-run=true]`;不连 mapping Redis;参数按子命令登记 |
| 两份清库脚本 | `route_meta` 只复位 `writable`;`route_reconcile` 存在时复位 leader / 租约 / 水位;注释写明 TiDB 清库后要重跑 `tidb-prepare` |
| 测试 | placement `TestRouteRowShard`;store 的 DDL 契约 / SQL 清单 / 游标序 / 语句生成;假驱动的行片 ≠ 围栏片(4250)、导入键序、错片行、P4、`PrepareTiDB`;真库的 CHECK、旧形状守卫、P4、错片行、EXPLAIN;可选真 TiDB 用例;routetool 跨片翻页与 `tidb-prepare` 输出;cmd 参数与「不连 Redis」 |

**新表形状与读方怎么用**

| 索引 | 形状 | 写入尾部(新号) | 读方 |
|---|---|---|---|
| PRIMARY | `(route_shard, player_id)` CLUSTERED | 16 条 | 1a:W1、导入、`GetRoutes`、audit;1b:全量预热;第 3 批:回源 |
| `idx_route_home` | `(home_zone, route_shard, player_id)` | 每 zone 16 条 | 第 2 批 W10 pin-placement;第 3 批 R7 `GetAllPlayerIDsInZone` 等 |
| `idx_route_storage` | `(storage_id, route_shard, player_id)` | 每个落点值 16 条 | 第 2 / 3 批 relocate 清单等 |
| `idx_route_updated` | `(route_shard, updated_at_ms, player_id)` | 16 条 | 1b 增量推送 |

后续批次的读方照抄下面的模板(**结果不再按 `player_id` 排序**,游标从 `player_id` 变成 `(shard, id)`):

| 用途 | SQL | 游标 / 起点 |
|---|---|---|
| 按 zone 枚举(R7、W10) | `SELECT route_shard, player_id[, …] FROM player_route WHERE home_zone = ? AND (route_shard > ? OR (route_shard = ? AND player_id > ?)) ORDER BY route_shard, player_id LIMIT ?`(W10 再加 `AND storage_id IS NULL` 与 `FOR UPDATE`,锁序分析留给第 2 批规格) | `(shard, id)`,起点 `{0, 0}`;走 `idx_route_home`、无 filesort |
| 按落点枚举 | 同上,`home_zone` 换 `storage_id` | 同上;走 `idx_route_storage` |
| 1b 增量推送 | 按片循环 `s = 0..15`:`SELECT route_shard, player_id, <cols> FROM player_route WHERE route_shard = ? AND (updated_at_ms > ? OR (updated_at_ms = ? AND player_id > ?)) ORDER BY updated_at_ms, player_id LIMIT ?` | 每片一个 `(updated_at_ms, player_id)` 游标;「`>= since`」的起点写成 `updated_at_ms > since OR (updated_at_ms = since AND player_id > 0)` |
| 1b 全量预热 | 即 R12(`ScanRoutes`) | `RouteScanCursor` |
| 按 id 回源(第 3 批) | R5 的不加锁形态;批量即 R13 | — |

**`player_id` 唯一性与探针 P4**。主键 `(route_shard, player_id)` 只在 `route_shard` 由 `player_id` 唯一决定时才等价于「`player_id` 唯一」,所以唯一性现在靠 `chk_route_shard`。CHECK 不生效(TiDB 默认 `tidb_enable_check_constraint=OFF`、MySQL < 8.0.16)时,一个绕过 store 或算错片的写者能给同一 `player_id` 写出两行,而 R5 / R8 / R13 都按「正确片 + id」点查,**错片行对它们不可见、表现为缺席**,导入还会再插一条正确片的行。读路径兜不住,所以:建池探针第 4 步 P4 在必然回滚的事务里插一行只违反 `chk_route_shard` 的行,必须收到 3819 且报错点名该约束,否则 `ErrRouteCheckNotEnforced` 拒建 store(db_first 的 data_service、debugutil、route_tool 都经过它);全表键集扫描(`ScanRoutes`,即 audit 方向 A / 1b 全量预热)校验每行的片,错片即 `ErrRouteShardMismatch`;不另加 `UNIQUE(player_id)`(那正是要消掉的单调尾部)。P4 只证明 CHECK 生效,不证明模数是 16(由 DDL 契约测试钉住)。

**对 §5.4 / 第 1b 批的约束**

- leader 租约、`watermark_ms`、`cache_verified_ms` 的读写改在 `route_reconcile`;`writable`、`generation` 留在 `route_meta`。`mapping:route:ready` 里的水位来自 `route_reconcile`。
- **回档检测跨两表**:「水位倒退 → 置 `writable=0`」原来可以是同一行上的条件更新,拆表后要在一个事务里读 `route_reconcile`、写 `route_meta`(或先置 `writable=0` 再处理水位),1b 规格要写明原子范围。
- leader 标识必须是 ASCII(`route_reconcile.leader` 是 `ascii`,与 `NewRouteWriterID` 同源),非 ASCII 字节在严格模式下会被库拒绝。
- 若 `route_meta` 设成 TiDB 缓存表,冻结写 `UPDATE route_meta SET writable=0` 最长阻塞一个读租约(`tidb_table_cache_lease`,默认 3s【未核实】),1b 的冻结步骤超时要 ≥ 租约 + 余量;在途事务仍按「仍未解决的风险」第 3 条处理。
- 增量推送按片循环(见上表);全量预热即 `ScanRoutes`。
- `route_tool tidb-prepare` 要随第 1b 批并进主程序入口(同偏离第 8 条)。
- **新的 `player_route` 写者**(第 2 批 merge_zone 等):直接调 `placement.RouteRowShard`(merge_zone 已 import `shared/placement`),并在开写前过与 P4 同等的 CHECK 实测(或复用 `RouteStore`)。CHECK 不生效的库上,任何绕过探针的写者都能给同一 `player_id` 写出两行,点读看不到。

**TiDB 运维步骤**

1. 建表**之前**:`SET GLOBAL tidb_enable_check_constraint=ON`(TiDB ≥ 7.2),否则 P4 拒建 store。
2. 建表之后(Phase 1 预建表之后)跑 `route_tool -f <yaml> tidb-prepare -zones=<全部 zone> -dry-run=false`;它先核对两张表 `TIDB_PK_TYPE=CLUSTERED`,不是就退出码 2(趁表空 DROP 后用 `-migrate` 重建)。默认 `-dry-run=true` 只列语句。
3. 开新 zone:在该 zone 对外开放(第一次建角)之前、低峰跑 `tidb-prepare -zones=<新 zone>`(带上全部 zone 也行,幂等)。两条 `ALTER … ATTRIBUTES` 是 DDL,在线重跑可能让在途建角以 8028 失败一次(HR9)。
4. BR / PITR 恢复之后、TRUNCATE 之后重跑(换表 ID,预切与 table attributes 不跟过去;BR / TiCDC 也跳过 `ATTRIBUTES` DDL)。
5. Phase 2 打开 `PinOnCreate` 指向 1000000 时加 `-storages=1000000`。
6. 可选:`ALTER TABLE route_meta CACHE`(撤销 `NOCACHE`)。默认不开,看 TiDB 压测里 `route_meta` 所在 Region 的读热度再定。缓存表上不能直接做 DDL、BR / TiCDC 是否保留 CACHE 属性都【未核实】,按「不保留、恢复后重放」处理。
7. MySQL 档:`tidb-prepare` 打印 `SKIP` 并退出 0;G1a 在 MySQL 上不改变容量(InnoDB 主键本来就聚簇,插入点从 1 个变 16 个,推断无可见影响)。

**相对本提案与第 1a 批的偏离(H)**

| # | 偏离 | 理由 | 代价 |
|---|---|---|---|
| H1 | `player_route` 主键 `player_id NONCLUSTERED + SHARD` → 聚簇 `(route_shard, player_id)`(§3.2、§3.1) | 消掉主键唯一索引的单调尾部;TiDB 建表后不能再改 | `player_id` 唯一性改由 PK + CHECK 共同保证,多一个建池探针;所有按 id 的 SQL 都要带片 |
| H2 | `RouteRowShards = 16` 写死、不可配 | 改它 = 整表重写 | 以后想要 32 片只能新表迁移 |
| H3 | 二级索引全部带片前缀,`idx_route_updated` 加 `player_id` 尾列 | 每条索引 16 尾;键集游标有确定的平局决胜 | 按 zone / storage 枚举的游标从 `player_id` 变成 `(shard, id)`,结果不再按 `player_id` 排序 |
| H4 | `route_meta` 拆成 `route_meta` + `route_reconcile` | TiDB 上读不被 leader 写锁拖住;开关表才可能设缓存表 | 五张表;1b 读写两张;清库脚本与 runbook 要改 |
| H5 | 建池探针 P4(回滚事务里一次必失败的 INSERT) | 唯一性依赖 CHECK,元数据查询在两种引擎上口径不一,实测最可靠 | 每次建池多 3 条语句;只读用途(`route_tool audit`)也要 `player_route` 的 INSERT 权限;真库套件(含 routing 的 `TestDBFirstRegister_RealStores`)要求 MySQL ≥ 8.0.16(更老是 FAIL 不是 SKIP) |
| H6 | `ScanRoutes` 的游标与顺序改为聚簇键(`routeDB` 接口签名变) | 键集翻页必须沿聚簇键 | audit 明细不再按 `player_id` 升序;审计窗口内新提交、且缓存三键全无的行若落在已扫过的片里,本次方向 A 看不到、方向 B 也不查它,要到下一次 audit 才报 |
| H7 | 新增 `route_tool tidb-prepare`(§7 与偏离第 8 条之外的新入口) | 预切要真实 zone 清单,迁移拿不到;运维可重放 | 多一个要记得跑的步骤;K8s 上依赖第 1b 批并进主程序;漏跑只影响性能 |
| H8 | 对 `player_route` 也预切(原任务只点了围栏) | 不预切,片前缀在首日不生效 | Region 数 ≈ 15 + 15 + 16·Z + 16·S + Z,空 Region 靠 `merge_option=deny` 永久留住;Z 上百时 `idx_route_home` 的空 Region 以千计,届时可把语句生成改成每 zone 只切 `(z,0,0)`(只改 `route_tidb.go`) |
| H9 | 显式排序规则(`utf8mb4_bin` / `ascii_bin`;`route_reconcile.leader` 由 utf8mb4 改 ascii) | 两种引擎默认不同,建表前改最便宜 | 与 MySQL 现状默认的 `utf8mb4_0900_ai_ci` 不同;这些列都是标识符,逐字节比较是正确语义 |

**风险(HR)**

1. **HR1 TiDB 细节大量未核实**(本批无法联网对照官方文档):SPLIT 元组写法、重复 SPLIT 是否幂等、`ATTRIBUTES` 是否覆盖索引 Region、所需权限、`TIDB_PK_TYPE` 列、缓存表语义、TiDB 的 3819 文案、行构造器 IN 的计划。缓解:真 TiDB 用例 `TestRouteStore_TiDBClusteredShardsAndPresplit` 与验证增量第 8 步;任何一项不成立,改 `route_tidb.go` 的语句生成或探针判据,DDL 不动。
2. **HR2 CHECK 是唯一性的一部分**:P4 只在建池时证一次;运行期有人 `ALTER … NOT ENFORCED`、DROP CHECK 或把 `tidb_enable_check_constraint` 改回 OFF,要到下次重启才发现;其间写进来的错片行只有 `ScanRoutes`(audit 方向 A)看得见。runbook 写明不许关这条 CHECK,改过 DDL / 全局变量之后必跑 audit。
3. **HR3 存量 snowflake 号偏向片 0**:bwmarrin 布局最低 9 位是同毫秒序号,低速铸号时多为 0(AGENTS §7.1),`% 16` 几乎都落片 0。只影响一次性导入与冷读的分布,不影响新号(号段稠密、均匀)。
4. **HR4 本机旧形状表**:G1a 之前跑过第 1a 批迁移的库,`player_route` 会让迁移失败(fail-closed,带处理提示)。确认路由表里没有要保留的数据后手工 `DROP TABLE player_route, zone_merge_fence, zone_merged_into, route_meta;`(`route_reconcile` 若存在一并 DROP)再 `-migrate`;**只 DROP `player_route` 不够**(旧 `route_meta` 与旧排序规则守卫查不出来)。代码不自动 DROP。
5. **HR5 `idx_route_storage` 的 NULL 段没切**:`PinOnCreate=false` 时新号 storage 为 NULL,这 16 条尾部仍在一个 Region;`SPLIT … BY (NULL, …)` 的写法未核实。打开 `PinOnCreate` 后按 `-storages=1000000` 切。
6. **HR6 围栏仍是每 zone 一个 Region**:单 zone 开服尖峰时,该 zone 的全部 W1 在一个 Region leader 上加锁 / 提交。缓解梯子见 §12「万一目录还是成了瓶颈」第 ③ 步。
7. **HR7 TRUNCATE / BR / TiCDC 丢预切与属性**:两份清库脚本只连本机 MySQL;TiDB 环境每个恢复 / 清库步骤后重跑 `tidb-prepare`。漏跑 = 回到单尾,只有性能后果。
8. **HR8 叠在未验证的 1a 上**:G1a 改的就是 1a 未编译的代码,两者一起交 Codex。1a 自己的风险(上面「仍未解决的风险」)全部仍在,G1a 只处理第 1 条里的 TiDB 热点部分;MySQL 单机连接预算(L-03)与 Redis 单主(L-06)不在本批。
9. **HR9 在线跑 `tidb-prepare` 的 DDL 副作用**:`ALTER TABLE … ATTRIBUTES` 可能让在途 W1 悲观事务以 8028 失败一次【未核实】,W1 只重试 1213 / 9007 → 那一次建角 Unavailable,由客户端重试。缓解:开新 zone 时在开放前、低峰执行;真 TiDB 用例在二次预切期间并发建角并原样记录。若实测确有,再评估「属性已在就跳过 ALTER」或给 W1 加 8028 重试(改变既有重试口径,另议)。

**验证增量**(插在上面「第 1a 批交 Codex 的验证」里执行:先复验清单 → 第 0 批 → 第 1a 批第 1–9 步,各步按下面替换 / 追加)

- 前置:本机 `testdb` 若跑过 G1a 之前的第 1a 批迁移,按 HR4 处理;不确定时停下来问负责人。P4 让所有建 route store 的真库用例要求 MySQL ≥ 8.0.16,跑第 3 步前先 `SELECT VERSION()` 记下版本。
- 第 1 步:placement 顶层用例 15 → **16**(新增 `TestRouteRowShard`);另跑 `go test ./placement/... -list . | Measure-Object -Line`(期望 16 行用例 + 1 行 ok)。
- 第 2 步:`-run` 正则追加 `TiDBPrepare|ScanCursor|RouteRowShard|ShardMismatch`,并跑 `go test ./internal/store/... ./internal/routetool/... ./cmd/route_tool/... -count=1 -v -run "Route|TiDBPrepare|ScanCursor|DecideImport|RegisterHome|ImportRoutes|NewRouteStore|ShardMismatch"`;routing 顶层用例**不变**(54)。
- 第 3 步:`go test -tags=integration ./internal/store/... -count=1 -v -run "Route|MigrateSchema"` 与 `go test -tags=integration ./internal/routetool/... -count=1 -v`;`TestRouteStore_KeyedReadsUsePrimaryRange` 打印的 EXPLAIN 计划**原样保留**(若 `type=ALL` 如实回报,不改 SQL);`TestRouteStore_TiDBClusteredShardsAndPresplit` 在 MySQL 上 SKIP 属预期。
- 第 4 步:按原样跑;`TestDBFirstRegister_RealStores` 必须 PASS(它会经过 P4)。routing 带 integration 仍是 63。
- 第 7 步:plan 里应出现三张数据表的 TRUNCATE、`route_meta(id=1) writable reset`、`route_reconcile(id=1) reset`。
- 新增 route_tool 冒烟(本机 MySQL;**不要用 `go run` 断言退出码**,它对子进程非 0 退出一律自己退 1):

  ```powershell
  cd go/data_service
  go build -o "$env:TEMP\route_tool_g1a.exe" ./cmd/route_tool
  & "$env:TEMP\route_tool_g1a.exe" -f etc/data_service.yaml tidb-prepare -zones=1,2; "exit=$LASTEXITCODE"   # 期望 exit=0,stdout 含 engine=mysql skipped=true
  & "$env:TEMP\route_tool_g1a.exe" -f etc/data_service.yaml tidb-prepare; "exit=$LASTEXITCODE"            # 期望 exit=2,stderr 含 -zones
  Remove-Item "$env:TEMP\route_tool_g1a.exe"
  ```

- 可选真 TiDB(拉取 `pingcap/*` 镜像属于下载,先征得用户同意;只在本机 dev TiDB 上做):`docker compose -f deploy/docker-compose.tidb.yml up -d`,用 root 先 `SET GLOBAL tidb_enable_check_constraint=ON` 并建一个随机口令的测试账号(口令不进日志不进 git),`DATA_SERVICE_IT_MYSQL_HOST=127.0.0.1:4000` 后只跑 `go test -tags=integration ./internal/store/... -count=1 -v -run "TiDB"`(**不要**对 TiDB 跑 `-run "Route"` 全集:全局库迁移在 TiDB 上会因 `player_snapshot` 的可空唯一键失败)。回报每条 SPLIT 的 `TOTAL_SPLIT_REGION`、三次 `SHOW TABLE … REGIONS` 的行数、二次 `PrepareTiDB` 是否报错、期间并发 `RegisterHome` 的结果(有无 8028)、R13 的计划、P4 在 TiDB 上的原始报错文本。
- 不需要重编 C++:无 proto 变更。

**已知未改**:§2 总览表「4 张表」与「`route_meta.watermark_ms` 回退」、§4.3 W10 / §5.2 R7 正文、§5.4 正文、§8.2「`route_meta.watermark_ms` / `generation` / `writable`」仍按旧口径叙述,以本段为准(水位在 `route_reconcile`,枚举游标是 `(shard, id)`);AGENTS.md §6.2(Q8)仍未改。

**Java 版(AGENTS.md §12)**:本项是服务端内部存储的表形状,不涉及客户端契约(帧、客户端可见 proto、tip 码、配表都不变),不需要与 Java 同批改。Java 版的映射持久化(D1)整体仍是「待做」,登记时机按 Q14。给 Java 的建议:做等价的「库为真源」目录时,若后端是 TiDB,同样避免以单调 `player_id` 作主键 / 唯一索引的唯一前缀(按本仓同口径的 16 片前缀,或 Java 选型的等价方案),并在 `PARITY.md` 写明取舍。

---

## 12. 全局目录本身是不是瓶颈(2026-10-12 补,回应负责人追问)

「玩家属于哪个逻辑服、数据在哪个库」这份目录全服只有一份,本质上是全局单点。要分清它和 Phase 2 全局玩家库是两类负载。数字都是推算,没有压测:

| | 玩家主数据全局库(Phase 2) | 归属 / 落点目录(本方案) |
|---|---|---|
| 每玩家数据量 | 整份存档,KB~MB 级 | 一行约 100 字节(`player_route`) |
| 写频率 | 每次存盘:在线人数 / 300 每秒,再加事件存盘 | **一生一次**(建角),加上极少的合服 / 搬库运维 |
| 读路径 | 冷读 / 预加载,走 go/db | 热路径全部读 **Redis 副本**:go/db 每条存盘一次 MGET、scene_manager 每次进场、login 每次登录。**不读库**(go/db 设计上不连全局库,§5.2) |

**写:只在建角时发生。**
- 每次建角在全局库做一个短事务:读 `route_meta`,`FOR UPDATE` 一个围栏分片行,插入一行,提交。
- 发布到 Redis 和 WAITAOF 都在**提交之后**,不持锁。所以 WAITAOF 只拉长单次建角的延迟,不降吞吐。
- 吞吐上限约为「每 zone 分片数 N ÷ 事务时长」。
  - MySQL 上事务几毫秒:N=8 时约每 zone 千级建角/秒(推算)。
  - TiDB 悲观事务更慢。G1a(§11.2「第 1a 批热点修复」)之后:`player_route` 主键与三条二级索引各 16 条写入尾部(`idx_route_home` 每 zone 16 条),`route_tool tidb-prepare` 预切后落在不同 Region;围栏每 zone 一个 Region;`route_meta` 只剩极少写的开关行,可选设成缓存表。剩下的上限是「每 zone 围栏分片行锁」与 TiDB 悲观事务时长。
- 它和今天已经在建角同步路径上的「名字登记」(同一个全局库、每次建角一条写)是同一类负载。所以本方案没有引入新的瓶颈种类,只是把这一类加倍。

**读:真正的压力在 mapping Redis,不在库。**
- 这个 Redis 现在和会话、锁、`PlayerAllData`、存盘游标共用一个单主实例。开服首日它是比库更早的单点(tidb-phase1-and-launch-readiness.md §7)。
- 缓解:mapping 独立成实例,保持 noeviction;压测里单独看它的 ops 与 CPU。
- 不建议在 go/db 进程内再缓存一层映射:搬库冻结要求「按当前记录选库」(player-storage-placement.md P-3),本地缓存会读到旧落点。

**能不能不要这份全局目录?**
- 只要 player_id 全局唯一、不编码 zone,而且合服只改归属、不重新发号(AGENTS §7、合服设计 §2),就必须有一处能回答「这个 id 属于哪个区、数据在哪个库」。
- 业界通常的做法也是这样一个「低写入、可缓存读」的全局目录。账号、名字唯一性、号段本来就是同类的全局薄层。
- 全球多地域时,目录留在单一写入地域,远端玩家建角多付 1~2 个跨洋往返。这是一次性的,见 Phase 1 文档 §8。

**万一目录还是成了瓶颈(2026-10-12 G1a 补)**

先看信号,再按代价从低到高往上爬。下面的数字都是推算,以 [tidb-phase1-and-launch-readiness.md](./tidb-phase1-and-launch-readiness.md) §7 的 L-12 / L-14 压测为准。

- **信号**(任一持续抬头就该看):
  - `login_create_player_stage_seconds{stage="register"}` 的 p99(建角注册段,`stress_summarize.ps1` 的 CreatePlayer stages 表);
  - `data_service_route_register_total{outcome="store_error"}` 占 db_first W1 总数的比例;
  - data_service 日志里 `[RouteStore] … retry … after lock conflict`(1213 / 9007 重来)与 1205 的频率;
  - TiDB 档:PD / Dashboard 的热点视图里 `player_route`、`zone_merge_fence` 所在 Region 的写热度与 leader 分布。
- **梯子**(代价从低到高):
  1. 调大 `RouteStore.FenceShards`(只改配置;上限 64 已是围栏持有者的契约,`placement.MaxRouteFenceShards`):同一 zone 的建角分摊到更多围栏行锁上。
  2. TiDB:`ALTER TABLE route_meta CACHE`(开关行读热时;冻结写会多等一个读租约,见 §11.2 G1a 段)。
  3. TiDB:围栏改按 `(zone, shard)` 预切 —— 只改 `route_tidb.go` 的语句生成,表已聚簇,不动 DDL(G1a HR6)。
  4. TiDB:加 TiKV / tidb-server。热点已打散,扩容才近似线性;打散之前扩容没用。
  5. W1 的围栏复核改为不持锁的版本比对(提交点只比 `fence_epoch` 而不 `FOR UPDATE` 围栏行),去掉每次建角都要的行锁。改变提交点语义,需要单独设计与评审,不在任何已排批次里。
  6. 全球多地域:目录留在单一写入地域、各地域放只读薄层(Phase 1 文档 §8),远端玩家建角多付跨地域往返,是一次性的。
- **MySQL 档**:目录与 zone 库目前在同一个 MySQL 实例上(§6 引的 `k8s_deploy.ps1:3443`、`:3511-3512`),这时目录的瓶颈其实是实例本身(连接预算、IO),先把全局库拆到独立实例;G1a 在 MySQL 上不改变容量。
- **读路径始终是 Redis 副本,不读库**(上面「读」一节):目录库成为瓶颈只会发生在建角写入上,不会拖慢登录、进场、存盘。

**开服前要做的(都已在别处登记):**
1. 压测建角:P2 切 db_first 的门禁本来就要求,出注册阶段 p99 与 1213 / 1205 计数,据此定 N。
2. TiDB 上跑 `route_tool tidb-prepare -zones=<全部>`(G1a;预切 `player_route` 与 `zone_merge_fence`),并把路由表热点纳入压测。
3. mapping Redis 独立成实例。
4. 号段开服档 Step=1000。
