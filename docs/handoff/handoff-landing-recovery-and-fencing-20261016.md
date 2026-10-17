# 交接：落地恢复、超窗处置、进场链余项与 MySQL 行内围栏（2026-10-16）

> 交出方：会话「玩家消息投递与位置管理」（Claude）。接手方：用户另开的账号或会话。
> **所有代码都未编译、未测试**（AGENTS.md §10.1）。下文写「已完成」的，只表示代码与测试已写好、已随用户快照入库，不代表验证通过。
> 基线：主仓 `f0dadf5b0`（2026-10-15 22:12）；客户端仓 `24ff81a`；Java 仓 PARITY.md 已入库（`dffc0ed` 及之前）。工作区里这几块目前没有本会话的未提交改动。
> **接手进展（2026-10-16）**：复核结论、负责人新拍板、本线选定做法与落码批次见 [landing-fencing-takeover-20261016/README.md](landing-fencing-takeover-20261016/README.md)。其中 §3.4 的前提已变：工作区出现了来源不明的在途围栏实现，负责人决定由本线合并接管；§3.5 的 253 限流已定为接受默认档。

---

## 0. 一页结论

| 块 | 状态 | 接手第一步 |
|---|---|---|
| 落地恢复（服务端） | 已写完并经过复核与修复 | 交 Codex 按 §4 验证 |
| 落地恢复（客户端） | 主体已入库，**复核没做；消费者绑定疑似未接到 UI 宿主** | §3.1 |
| robot 冒烟 `sync-smoke` | 已写完，**复核没做** | §3.2 |
| 超窗处置与对账（guild / trade / 结算） | 已写完并经过复核与修复 | 交 Codex 验证；需要 DSN 的真库用例必须跑，Skip 不算通过 |
| 进场链余项 a–e | **没开工**（只有清单） | §3.3：先出设计 |
| MySQL 行内围栏 | 设计 v0.3 已完成（复核没跑完）；表字段 18/19 已先行声明；**条件写逻辑没落码** | §3.4：等两个前置，再请用户拍 13 个待定项 |

---

## 1. 背景与用户决定

- 起点是用户问「玩家连续换场景时，别人发给他的重要消息该怎么处理」。调研结论记录在 [reliable-player-inbox-20261014.md](../design/reliable-player-inbox-20261014.md)：收件箱按人存，推送只是提醒，准确数据靠拉。
- 用户随后要求「按最标准的做完，我不怕改动」，并拍板了以下几件事（2026-10-14 至 10-15）：
  - 范围包括四块：落地恢复、进场门禁、超窗处置与对账、MySQL 行内围栏。
  - 授权修改客户端仓 `../mmorpg-client`。
  - **Java 版只登记待做**。这偏离了 AGENTS §12「客户端契约两版同批改」，已在 Java 仓 PARITY.md 写明原因和退化行为。
  - 资产待办超窗：停放 + 落地唤醒 + 日对账，**再加 180 天自动放弃**（终结为 ABANDONED，退回帮会侧限购与帮贡）。
  - 战斗结算待办：**去掉 7 天到期删除**。
  - 进场门禁由另一会话用「登录代次」实现。它留下的 a–e 五项由本会话接手（用户已确认），见 §3.3。
  - MySQL 行内围栏由本会话落码（用户已确认，go/db 会话不做）。
- 设计文档：
  - [landing-recovery-sync-digest.md](../design/landing-recovery-sync-digest.md)：落地恢复与超窗处置，**唯一依据**，已按实现同步；
  - [db-write-fencing-mysql.md](../design/db-write-fencing-mysql.md)：v0.3；
  - [scene-enter-session-fence.md](../design/scene-enter-session-fence.md)：进场门禁，a–e 在它的 §7、§9。

---

## 2. 已完成（代码与测试已写、已入库、未验证）

批次编号见设计 §9。

| 批次 | 内容 | 主要位置 |
|---|---|---|
| A | 摘要包与 friend / team 的 bump | `go/shared/playersync/`；`go/friend/internal/logic/friend_logic.go`；`go/match/internal/team/syncdigest.go`；两处 svc 装配 |
| A2 | friend 被动方 bump 收紧：按事务内的实际效果决定是否给对方 bump，堵住「循环拉黑任意 id 造键」 | `go/friend/internal/data/{block_repo,friend_repo}.go` 及 logic |
| B | guild 四个域的 bump：升级、撤回申请给审批人、本人帮贡变化等都已补上；bump 用专用 go-redis 客户端（`ContextTimeoutEnabled`） | `go/guild/internal/logic/*.go`、`svc/servicecontext.go` |
| C1 | 资产通道共享层：停放档、`WakePlayer`、日对账 `PendingClassCounter`、自动放弃 `Abandoner`（文件头写了 last_send_ms 契约）、`StatusAbandoned`；唤醒订阅包 | `go/shared/assetop/`、`go/shared/assetopwake/`；`go/shared/go.mod` 补了 miniredis |
| C2 | guild 接入：三个可选接口、last_send_ms 全部 SQL（含懒初始化）、Abandon 对侧账同 REJECTED / ABORTED、配置、选主跑维护、唤醒订阅、`WithSyncBumper` 接线、ABANDONED 在客户端显示为 ABORTED | `go/guild/internal/data/asset_store.go`、`config`、`svc/asset_op.go`、`guild.go`、`guild.yaml`、`proto/guild/guild_db.proto`（ABANDONED=6、last_send_ms=29） |
| C3 | trade 接入：同 C2，另加终态清理；**放弃只放开 TRADE_DEBIT**（对侧账是 P3 TODO，fail-closed）；终结写者先主键点锁再 CAS；9 个真库用例 | `go/trade/internal/data/asset_op_repo.go`、`reconcile/pipeline.go`、`svc/assetchannel.go`、`proto/trade/trade_table.proto`（ABANDONED=6、last_send_ms=24） |
| D1 | scene 摘要读取（`GetSyncDigest` 253 / `NotifySyncDigest` 254）与资产唤醒发布 | `proto/scene/player_scene.proto`；`cpp/libs/services/scene/player/system/player_landing.{h,cpp}`、`comp/player_landing_comp.h`；`player_scene_handler.cpp` 守护段；`cpp/tests/player_landing_test/` |
| D2 | 战斗结算待办：去掉 TTL，加积压索引 ZSET，battle 节点每 60 秒报数并清孤儿，用尽日志分 reason，在线玩家冻结到期时补读，解冻时发唤醒（不受合并窗口限制） | `settlement_outbox.h`、`battle_room_manager.{h,cpp}`、`player_battle.{h,cpp}`、`bag_test` |
| D3 | 挂点与工程登记 | `player_lifecycle.cpp` 在 EnterScene 末尾调 `PlayerLandingSystem::OnEnteredScene`；`scene.vcxproj` / `.filters` / `CMakeLists.txt`；`game.sln`（`player_landing_test`） |
| F | robot `sync-smoke`（账号 robot_9911 / 9912） | `robot/sync_smoke_scenario.go`（+测试）、`etc/sync_smoke.yaml`、两个 handler 桩、`main.go` / `config.go` / `README.md` |
| E | 客户端主体：`LandingSyncClient`、`SyncDigestStore`（PlayerPrefs）、`LandingSyncBindings.BindTeam` / `BindFriendRequests`、GameClient 挂接、gen_messageids 白名单、EditMode 测试 | 客户端仓 `24ff81a` |
| G | 文档、告警与登记 | 见下面列表 |
| 围栏 | 表字段先行声明 | `proto/common/database/mysql_database_table.proto`：`player_database` 18 / 19、`player_database_1` 3 / 4；放行清单同步 |

G 批的文档、告警与登记：
- 新建 `deploy/k8s/asset-channel-alerts.yaml`；battle / scene 的 LogQL 写在文件头，作为值班固定查询。
- [04-asset-channel.md](../design/guild-phase2/04-asset-channel.md) 第 12 部分。
- [shared-redis-split-by-purpose.md](../design/shared-redis-split-by-purpose.md) §2.2 登记新键族：摘要键、结算积压索引、唤醒频道与去重键、trade 选主锁。
- reliable-player-inbox §5.3。
- G-03 放行清单：`90-consistency.md`、`03-names.md` §3.15a、`92-handoff.md` 第 4 步。
- Java 仓 PARITY.md 两行，状态为 Java 待做。

协议生成已由别的会话或 Codex 跑过，用的是服务端配置：`message_id.txt` 的 253 / 254 已分配，`go/proto` 有 SyncDomain，guild / trade 的 db 生成物有 ABANDONED。**客户端的 handler 和 C# 协议类还没生成**，见 §4 第 1 步。

---

## 3. 未完成

### 3.1 客户端（E）：复核没做，绑定疑似没接上

- **消费者绑定疑似没接到 UI 宿主**。`LandingSyncBindings.BindTeam` / `BindFriendRequests` 在全仓只出现在定义处和注释里，没找到 TeamUiRoot、Social 的 UI 宿主调用它们。客户端主体的注释写的是「由持有它们的界面宿主经 LandingSyncBindings 接上」。如果确实没人调用，落地后摘要只会被读取、标脏，好友和组队不会因此拉取，Ack 也不会推进。接手后先 grep 核实；确认没接，就在 TeamUiRoot、好友 UI 宿主里调用 Bind，并保存返回的 IDisposable，在宿主销毁时释放。
- **帮会消费者没接**。`Game/Guild/GuildClient.cs`、`UI/Ugui/Guild/GuildUiRoot.cs` 当时有别的会话的未提交改动，所以没动。现在客户端工作区是干净的，可以补：
  - 给 GuildClient 加一个公开的排队刷新入口；
  - 新增 `BindGuild`：GUILD / GUILD_APPLICATION / GUILD_ECONOMY / GUILD_ACTIVITY 标脏时排队拉取，成功后 Ack；
  - GuildUiRoot 在窗口隐藏时也要消费由摘要触发的首次刷新。现状只在窗口可见时消费。
- **复核要点**：
  - 落地判定是 (PlayerId, GateConnectionIdentity) 变化，且 InGame && IsGateReady；
  - 发请求前先置在途标志，防 Tick 重入；
  - 15 秒超时，按 15 / 30 / 60 秒退避；
  - 用序号戳丢弃旧落地的推送；
  - UNAVAILABLE 或 g 变化时全量拉；
  - 先读后拉，拉取成功才 Ack；
  - 隐私：红点只能来自拉回的内容（设计 §3.5 第 4 条）。

### 3.2 robot `sync-smoke`：复核没做

实现者自述的口径偏差：
- 短重连实际在 Close 后约 250ms 才发 Login，原因是共享 helper 固定等 200ms；
- S6 负向探针只记录，不断言；
- 跨 zone 腿默认关闭。

`robot/main.go` 当时有别的会话的未提交改动，本批只加了一个 hunk。复核看 `sync_smoke_scenario.go` 的步骤断言是否与设计 §10 第 6 步一致，以及限流间隔是否 ≥1.1 秒。

### 3.3 进场链余项 a–e：没开工

出处：[scene-enter-session-fence.md](../design/scene-enter-session-fence.md) §7 表三条「本次不改」、§9 残余 2，外加一条可选加固；§9 残余 7 是邻域问题，按需取用。**先出设计，再落码**（AGENTS §11.5 第 1 条），改动会跨 login、player_locator、scene_manager、gate、scene。

| 项 | 问题 | 方向（出处原文建议） | 主要文件 |
|---|---|---|---|
| a | login 踢线只按 session_id。失锁的陈旧登录链，可能在 SetSession 失败前把当前会话踢掉 | `KickPlayerEvent` 加 `player_id`，由 gate 核对；`kickReplacedSession` 挪到 SetSession 成功之后。Kick 只走信封，生成器会自动映射 `GateCommand.player_id`，不用改 payload | `proto/contracts/kafka/gate_event.proto`、`go/login/.../entergamelogic.go`、`go/login/internal/svc/gate_command.go`、gate 的 `KickPlayerEventHandler` |
| b | MarkOffline 删掉会话后同步调 LeaveScene，中间没有屏障。同落点重登不写 location，迟到的 LeaveScene 会删掉新登录的 location | 给 LeaveScene 带会话身份，在 Lua 里做条件删除；与 d 统一设计 | `go/player_locator/.../markofflinelogic.go`、`go/scene_manager/.../leavescenelogic.go` |
| c | gRPC `ReleasePlayer` 只带 player_id。迟到的释放会把交接回滚后的实体退掉 | 带预期 `owner_epoch`，scene 比对后才释放 | `scene_node_service.cpp`、scene_manager 的调用方 |
| d | scene_manager 的落点 Lua 不认会话。陈旧登录请求或旧会话在途的换图，仍会改写 location 和人数 | 落点 Lua 记录会话身份（session_id + login_generation），同落点重登也要更新身份；b 的条件删除靠它 | `go/scene_manager/internal/logic/enterscenelogic.go` 的落点 Lua |
| e | 可选加固：ExitGame 带登录代次，断线上报带 gate 实例号 | 原会话评估为「三道守卫在栅栏生效后已经精确」，属可选 | gate、scene |

**不可改的不变量**（原会话交代，必须守住）：
- 入口栅栏必须是第一步，且零副作用；
- 同一会话一律放行；
- SessionMap 只擦属于本玩家的映射（`EraseSessionMappingIfOwned`）；
- gate 侧的 `loginGeneration` 只经 `scene_route_helper.h` 的 `ApplyLoginBinding` / `ApplyRouteIdentity` 写入，只抬高、不丢路由。

### 3.4 MySQL 行内围栏：设计已完成，逻辑没落码

- **设计**：v0.3 已把 24 条评审意见并入（§17 处置表）。最后的独立复核因额度中断没跑，建议先补一轮复核。
- **已先行声明**：表字段 18 / 19（`player_database`）与 3 / 4（`player_database_1`）。玩家收件箱随后取 20 号。
- **前置一**：proto2mysql `feat/conditional-save`（`e81a1e7`）的 `SaveConditional` 要经 Codex 验证，并只合入 main（任务单见 `docs/handoff/godb-pipeline-codex-tasksheet-20261013.md`）。截至交接时结果未回。
- **前置二**：go/db 会话还在做一批较大的改动：G1 治标、WAITAOF、重试只交给分区属主（④）、读写分区对齐（①）、消费组按 zone 拆（③）。它承诺做完后给本会话发文件清单。**在那之前不要动 `go/db/internal/kafka` 与 `go/db/internal/logic/pkg/proto_sql`**。接手后请直接问该会话要清单，并让它单独标出 `markAppliedSeq` / `markAppliedOwnerEpoch` 调用顺序有没有变（v0.3 §5.5 的原子推进依赖这个顺序）。
- **go/db 会话也已交接**（2026-10-16），交接文档是 [godb-pipeline-impl-handoff-20261015.md](godb-pipeline-impl-handoff-20261015.md)，其 §4 引用了本节。它告知的现状：
  - `markAppliedSeq` / `markAppliedOwnerEpoch` 的调用顺序到目前为止没有变；
  - 清单里会单独标出 verify 已挪进 `runDBOp`（在复核之后、缓存回写之前）、Stop 改成五步、条件写复用 `luaDecimalGreaterFn`。v0.3 落码前要按这三点重新核对 §5.4、§5.5、§6；
  - go/db 还有两项没落：④（重试只交给分区属主）和保留期。落完之前，「不碰 `go/db/internal/kafka` 与 `proto_sql`」的约定继续有效。
- **待用户拍板的 13 项**，原文见 v0.3 §14。下面是**本交接的序号**，括号里是设计 §14 的条号（2026-10-16 补注；两套编号不同，引用时写清是哪套）。2026-10-16 已全部定稿，见设计 v0.4 §14：
  1. equal 的缓存处理（§14 第 12 条）；
  2. Observe 超上界的处置与容差（§14 第 13 条）；
  3. 失租是否豁免预算（§14 第 14 条）；
  4. Stage 1b 依赖 L-04 第 2 批死信工具（§14 第 9 条）；
  5. 冷登录与在途写竞态的修法（§14 第 15 条）；
  6. epoch-raise 入口放在哪（§14 第 5 条；与第 20 条「余量 K 还是先排空」、收件箱 C4 一起定）；
  7. proto 生成怎么跑才不写客户端，以及陈旧 sql 是否刷新（§14 第 6 条）；
  8. 新旧 critical 告警重复（§14 第 16 条）；
  9. 进程换代是否必铸 epoch（§14 第 8 条）；
  10. G4a 拆分后 go/db 的 Redis 客户端（§14 第 19 条）；
  11. 铸造的持久下限（§14 第 7 条）；
  12. 两个可选项（§14 第 17、18 条）；
  13. 两项另开任务的待核项（§14 第 10、11 条）。
  原清单漏了 §14 第 20 条（余量 K），已并入第 6 项。
- **与收件箱的联动**：收件箱会话希望把它的高水位纳入 §9.1 epoch-raise 的取最大值来源（它的 C4），与本交接第 6 项（§14 第 5、20 条）一起定。**v0.4 已定**：`epoch_raise` 工具经 `PlayerInboxAdmin.GetSummary` 取收件箱 `max_fence_epoch`，列为取最大值来源之一（设计 §9.1 第 c 步、§14 第 5 条）。

### 3.5 其他小项

- **MessageLimiter 限流行**：`data/MessageLimiter.xlsx` 还没有 253 号（GetSyncDigest）这一行。设计要求档位同 `SceneInfoC2S`，但 43 号自己也不在表里，等于默认档。请用户确认是否接受默认档，或追加一行；robot 冒烟按默认档设计。
- **A2 可选**：按方向精确判断好友边是否被删。现在只剩单向残留边时会多 bump 一次，后果只是多拉一次。
- **C2 证据项**：在 MySQL 与 TiDB 上 EXPLAIN 日对账的 `sqlListPendingFactsPage` 与 `sqlListAbandonCandidates`，确认走 PRIMARY；没走再加 `USE INDEX(PRIMARY)`。
- **PROGRESS.md**：已在本交接同批追加一条（见 docs/PROGRESS.md 2026-10-16 条目）。
- **提交**：由用户决定。用户习惯整个工作区一次打快照提交。

---

## 4. 交给 Codex 的验证顺序（落地恢复与超窗处置）

C++ 的 MSBuild 一律串行 `/m:1`。每步都要保留失败日志。

1. **proto 生成（含客户端）**：在仓库根执行 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build`，再执行 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run`。
   - 本任务已获客户端授权，用默认配置，会把 253 / 254 的 handler 写进客户端仓。
   - 不要用 `dev.bat proto`。
   - 期望：
     - `message_id.txt` 原有号不变；
     - `player_scene_handler.{h,cpp}` 守护段原样保留、方法不重复；
     - 客户端 `Net/Generated/Handlers/` 出现 SyncDigest 的 handler。
2. **迁移，必须在任何新版 go/db / scene 启动前做**：对每个 zone 库和落点库跑 `go/db` 的 `cmd/migrate plan → up → SHOW COLUMNS`。
   - 放行清单按 G-03 最新一条（`90-consistency.md` G-03 节）：`player_database` **6 列**（`profile_component` / `asset_op_ledger` / `settlement_ledger` / `fence_owner_epoch` / `fence_save_seq` / `inbox_ledger`=20）加 `player_database_1` 2 列（2026-10-16 订正，原写 5 列已过时）。
   - 这与收件箱交接（`player-inbox-handoff-20261015.md`）§4 第 6 步是**同一次迁移**，不要分两次跑。
   - guild / trade 的 `last_send_ms` 列由各自的 schemamigrate 或 migrate 补齐，按两服务现有机制执行。
   - 缺列的话，全部玩家存盘都会因 unknown column 失败。
3. **限流表**：见 §3.5，需用户确认。
4. **Go**（各模块目录，`GOTOOLCHAIN=local`）：
   - `go/shared`：`go test ./playersync/... ./assetop/... ./assetopwake/... ./leader/...`。
   - `go/friend`：`./internal/...`。真库部分设 `FRIEND_TEST_MYSQL_DSN` 与 `FRIEND_REQUIRE_MYSQL_TESTS=1`。
   - `go/match`：`./internal/team/...`。
   - `go/guild`：`./internal/...`。真库部分设 `GUILD_TEST_MYSQL_DSN`，库名只许 `guild_test` 或 `guild_it_*`。
   - `go/trade`：`./internal/...`。真库部分用 `-tags integration` 并设 `TRADE_TEST_MYSQL_DSN`。
   - 以上各模块都跑 `go vet ./...`。
   - **真库用例显示 SKIP 不算通过。**
   - 通过标准：原有用例数不减，且全绿。
5. **C++**：
   - `msbuild game.sln /m:1 /p:Configuration=Debug /p:Platform=x64`；
   - 跑 `player_landing_test.exe`（摘要解析，13 例）；
   - 跑 `bag_test.exe` 全量（含结算新用例）、`routing_identity_test.exe` 全量。
6. **robot**：在 `robot/` 下执行 `go test -mod=vendor ./...`，再执行 `go build -mod=vendor`。
7. **客户端**（`D:\luyuan\mmorpg-client`）：
   - `tools/gen_proto.ps1 -ProtoRoot D:\luyuan\xuanming-server-mmo-cpp`；
   - `tools/gen_messageids.ps1 -ProtoRoot D:\luyuan\xuanming-server-mmo-cpp`；
   - `tools/client_compile_check.ps1`；
   - Unity `-batchmode -runTests -testPlatform EditMode -testFilter LandingSyncClient`；
   - 一并提交 Unity 生成的 `.meta`，不要夹带他人的资源与 ProjectSettings 改动。
8. **联机**：`dev-start` 之后：
   - `robot.exe -c etc/sync_smoke.yaml`，期望输出 `SYNC_SMOKE_OK`；
   - 再回归 `friend_smoke`、`travel_smoke`；
   - 资产唤醒：先开启 guild 资产通道，机器人离线时发起一笔兑换，登录后 5 秒内终结（`assetop_wake_total{result="woke"}`）。

各批更细的命令与通过标准，见设计 §10 和各批代码头注释。

---

## 5. 协作现状（2026-10-16）

- **「go/db 玩家存盘管线交接」**：负责 L-04 及后续 go/db 改动。约定本会话不碰 `go/db/internal/kafka`、`proto_sql`，等它发文件清单。
- **「玩家收件箱服务设计」**：
  - 已获授权落码独立服务 `go/player_inbox`；v1 不接摘要（用户拍板 D4）；
  - 在 scene 侧加了两处一行调用，本会话已同意：`player_landing.cpp` 的 `OnEnteredScene` 预留位，以及 `player_battle.cpp` 的 `RemoveInBattleComp`；
  - `player_database` 取 20 号（`inbox_ledger`）；
  - 摘要的 §3.3 注明：收件箱接入「届时另行拍板」，下发时必须过滤，否则会泄露 GM 处置。
- **「旧入场请求覆盖新会话问题」**：登录代次栅栏已落地（`ebccaf04d` 及之后），a–e 已移交本方（§3.3）。
- **「C++ RPC 异步回调链式调用设计」**：已删掉零调用的玩家消息发送函数，Route 一族改为显式拒绝。`SendMessageToClientViaGate` 不变（D1 依赖它）。
- **「给帮会捐献与商店购买加客户端幂等键」**：在给 guild.proto 加 `client_request_id`，没有碰客户端。

---

## 6. 残余风险

见设计 §12 第 1–11 条。最要紧的三条：
- 在线期间推送丢失，要等下次落地或打开面板才恢复；
- 自动放弃不可逆，审计行和 `assetop_abandoned_total` 是人工补偿的依据；
- 结算积压的 `oldest_age_s` 在玩家永久流失后会单调增长，这是预期，告警看 `reason=not_acked` 与积压数的增速。
