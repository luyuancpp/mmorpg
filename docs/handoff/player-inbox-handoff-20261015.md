# 玩家持久收件箱：实现交接（2026-10-15）

> **一句话**：设计已定稿。第 1–4 批代码已随用户快照提交 `de4960bee`（2026-10-15 21:57）入库，proto 与 tip 已由 Codex 正规生成（PROGRESS「2026-10-15 续编完成」条目）。但**收件箱代码没有任何编译或测试证据**：`go/player_inbox` 不在 Codex 构建过的 11 个 Go 服务里，也没有 `go.sum`；C++ 侧入库后又被 gRPC→muduo 迁移改过，要在当前工作区重编。Go 服务那一路写到收尾时被额度打断，完整性要先审读（§2.2）。Java 版没有开始。
>
> **2026-10-16 订正**：初稿写的是「全部未生成、未提交」，与 `de4960bee` 及 PROGRESS 记录不符，已按只读核实结果改正。残留未提交项见 §2.5。
>
> **规格**：[docs/design/player-inbox-service.md](../design/player-inbox-service.md) v1.0。D1–D14 均已由用户拍板，第 1 批精确清单见附录 E。本文只记「做到哪、还差什么、怎么验证」，设计细节以设计稿为准。
>
> **起草**：「玩家收件箱服务设计」会话（Claude）。用户要求交接给其他账号继续。

---

## 1. 已拍板的范围（不要再讨论）

| 项 | 结论 |
|---|---|
| 形态 | 独立进程 `go/player_inbox`，独占库 `mmorpg_player_inbox`，节点类型 `PlayerInboxNodeService = 30`，gRPC `:50950`，metrics `:9270` |
| 首域 | `SCENE_GM_SANCTION`：封锁货币获取 / 解除封锁 / 挂补缴欠款（`GM_ATTACH_CURRENCY_DEBT` 认定为账户处置状态） |
| 模型 | 每 (玩家, 域) 一个有序邮箱；scene 落地后主动 Fetch，按 seq 执行；游标与最近 32 条结局写进 `player_database.inbox_ledger = 20`，与货币同一次落盘，落盘后才 Ack；owner_epoch 高水位围栏 + Redis 上界 |
| 触发 | 落地必拉（`PlayerLandingSystem::OnEnteredScene`）、Kafka 在线提醒、30 分钟兜底、战斗解冻立即重查。**v1 不接版本摘要**（D4） |
| 字段号 | 18/19 归 MySQL 围栏（已在 HEAD 声明），收件箱取 20 |
| tip | 段 `//player_inbox_error base=29000 width=1000`，15 个码，只有 `PlayerInboxStoreUnavailable` 是 fault |
| 批次 | config.proto 在第 3 批与 `config.cpp` 同改（P6）；D9 的 `player_debt` 弃用注释随第 1 批 |
| Java | D13：契约随第 1 批 ContractSync；功能在 C++ 第 3 批之后实现，欠款改走收件箱，不走离线编辑 |

## 2. 已入库的改动与残留未提交项

> 工作区同时有多个会话在改。下面只列收件箱的改动；同一文件里别的会话的改动不要动。
> §2.1–§2.4 的内容已随 `de4960bee` 入库；之后仍未提交的收件箱相关改动见 §2.5。以 `git status` 为准，按归属区分。

### 2.1 第 1 批：契约与登记（已完成）

- **新 proto**：`proto/player_inbox/{player_inbox, player_inbox_admin, player_inbox_table}.proto`，`proto/common/component/player_inbox_ledger_comp.proto`，`proto/common/event/player_inbox_event.proto`。package `player_inbox`，go_package `player_inbox/player_inbox`。
  - 名称清单：服务 `PlayerInbox`（FetchItems / AckItems）、`PlayerInboxAdmin`（GmSanctionPlayer / GetSummary / ListItems / LookupByRequest / VoidItem / ReconcilePlayer）。
  - 表：`player_inbox_stream`、`player_inbox_item_row`（SQL 表名 `player_inbox_item`）、`player_inbox_audit`。
  - 事件：`PlayerInboxNotifyEvent`。
- **改动**：
  - `node.proto` 加 30；`mysql_database_table.proto` 加 import 与 `inbox_ledger = 20`。
  - `data_service.proto` 加 `GetPlayerInboxLedger`；`rollback_database_table.proto` 的 `player_debt` 加弃用注释。
  - `proto_gen.yaml` 与 `proto_gen.no-unity.yaml` 两份都加了 domain_meta；`.gitignore`。
  - `node_util.{h,cpp}`。
- **Tip 表**：`data/tip/Tip.xlsx` 已用 `tools/scripts/player_inbox_xlsx_patch.py tip-codes` 追加，脚本幂等。
- **C++ 工程登记**（14 个文件）：`cpp/generated/{proto,grpc_client,table,rpc}` 与 `cpp/nodes/scene` 的 CMakeLists / vcxproj / filters。
- **放行清单**（6 处）：90-consistency G-03 / G-05、03-names §3.15a / §3.20、92-handoff 步骤 4、04-asset-channel §4.42、turn-battle-gap-closure §9.3、merge-zone-runbook §4.4。
- **生成产物**：Codex 于 2026-10-15 用导表器和 no-unity 配置做了正规生成：补齐 15 个 tip，把表行改名为 `player_inbox_item_row`，两次生成的产物一致（PROGRESS「2026-10-15 续编完成」条目）。产物随 `de4960bee` 入库，包括 `go/proto/player_inbox/*`、`cpp/generated/{proto,grpc_client,rpc/service_metadata}` 下的收件箱文件和 tip 产物。HEAD 的 `segments.go` 已有 base 29000 段，`faults.go` 新段只有 StoreUnavailable。22:27 另一会话又全量重生成了一次：Go 绑定没有变化，`cpp/generated/grpc_client/player_inbox/*` 按新模板变了、未提交（§2.5）。**例外**：`go/player_inbox/generated/pb/game` 被 `.gitignore` 忽略，服务代码却引用了它，见 §2.2 第 3 条。

### 2.2 第 2 批：Go 服务 `go/player_inbox`（写到收尾时被打断，需审读）

46 个手写文件，约 8100 行：

| 部分 | 文件 |
|---|---|
| 入口与配置 | `player_inbox.go`、`player_inbox_test.go`、`go.mod`、`etc/player_inbox.yaml`、`internal/config/*` |
| 骨架 | `internal/{svc, data, lifecycle, metrics, constants}/*` |
| 核心包 `internal/inbox` | registry、fence、decide、seq、shard、store（拆成 store / store_enqueue / store_consume / store_admin / store_maintenance）、notify、loops、reconcile。registry、fence、decide、seq、shard、notify、loops 有单测；store_* 与 reconcile 的 I/O 方法只有 `store_integration_test.go` 覆盖 |
| 服务端与逻辑 | `internal/server/*`、`internal/logic/gm_sanction_logic.go` 及测试 |

**接手先做**（2026-10-16 只读核实过；文件结尾、桩、rpc 覆盖、go.mod 都没发现被打断的痕迹，但没编译）：

1. 完整审读一遍，对照设计 §6、§7.8、§9、§10 和 §11.4 的验收表，补齐缺的测试。测试现状：
   - **完全没有测试**：`player_inbox_admin_server.go`（AdminServer）、`Store.ScanGauges`、`metrics.Register` / `StartHTTP`、`svc.NewServiceContext` / `Start` / `Stop`。
   - **只有集成测试**：`store_consume.go`（Fetch / Ack）、`store_enqueue.go`、`store_admin.go`、`store_maintenance.go`、`reconcile.go`、`data.Tables`。集成测试带 `//go:build integration`，`PLAYER_INBOX_TEST_MYSQL_DSN` 为空时整组 SKIP；设 `PLAYER_INBOX_REQUIRE_MYSQL_TESTS=1` 后，DSN 为空直接判红（HEAD 上就有这个开关，本条初稿误写为「没有」，见下方处理结果的订正）。其中的纯逻辑（`reconcileDiff`、`resultForExisting`、`issuedWithinWindow`、`enqueueResultLabel`）在 `decide_test.go` 有单测。
   - `go.sum` 不存在，要跑 `go mod tidy`。
2. **AdminTokenEnv 已核实一致**：`config.go`、`etc/player_inbox.yaml`、`k8s_deploy.ps1` 渲染的 ConfigMap、manifest 与 Secret 注入四处一致。运维批那份「Go 配置不认」的报告已过时，不用改。
3. **干净检出编不过**：`internal/svc/servicecontext.go:13` import 了 `player_inbox/generated/pb/game`（:150 用 `PlayerInboxNotifyEventEventId`），但 `.gitignore:164-165` 忽略了 `go/player_inbox/generated/`，注释还写「与 trade 同口径」。trade / chat 的口径是「服务代码不引用」，收件箱并不符合。friend、match、guild、login、scene_manager、client_rpc_router 都跟踪了自己的 `generated/pb/game`；CI `go-modules-ci.yml` 直接 `go build ./...`，不跑生成。建议与这些服务一致：删掉 `.gitignore:164-165`，跟踪该目录。源头在设计附录 E（`player-inbox-service.md:1605`），要同步改。
4. **Redis 连接总账漏算**：`tools/scripts/k8s_deploy.ps1:1708` 写的是 player-inbox「还没有建 Redis 客户端」，实际 `servicecontext.go:111` 已经建了（没设 PoolSize，启动时 Ping）。要在 `Get-RedisConnectionLedgerInputs` 的 `$clients` 加 player-inbox（1 个池 × 2 副本），删掉 :1708 那条，同步 `k8s_deploy_contract.tests.ps1` 的期望名单。

**处理结果（2026-10-16，「收件箱 Go 服务审读与补齐」会话，Claude；未编译、未测试，待 Codex 按 §4 第 3、5 步验证）**：

- **第 1 条 审读与补测试：已做。**
  - 46 个文件逐个审读。围栏表、入箱顺序（锁内先判重后判满箱）、Ack 七种结果、作废、清理只删终态、提醒 next_seq CAS、只在领导者上跑、关停顺序都与设计一致，未改。`ClassifyStale` 把 `R < req` 也记成 fence_regressed，偏离设计 §7.8 的「R == req」，代码注释已写明理由（R 比在途旧 owner 还小只可能是计数器倒退），保留。
  - 找到并修了 4 处语义缺口：
    - `player_inbox_fetch_scanned_rows` 记的是返回条数（最多 17），设计 §4.2「超出数百行就加索引」的触发条件永远不会成立。现在锁路径提交之后按主键数实际走过的行（`fetchScanBound` + `countScannedRows`，含被过滤掉的终态行），桶加到 1024。
    - `svc` 用 `zrpc.MustNewClient`：配置错误时直接退出进程，绕过「返回错误、关闭已建资源」的契约；data_service 连接也从不关闭。改用 `zrpc.NewClient`，连接随 `Stop` 关闭。外部连接改为可注入（`externalDeps`），后台任务改为列表（`backgroundTask`），生产行为不变。
    - `Registry.Domains()` 无序，`ReconcilePlayer` 的逐域输出顺序随 map 遍历变化。改为按域号升序。
    - `metrics.StartHTTP` 的监听失败只在后台 goroutine 里报。改为先同步监听再 Serve，失败仍只记错误、照常服务。
  - 抽出纯逻辑（都在 `decide.go`）：`decideEnqueueLocked`、`fetchNeedsLock`、`fetchStreamUpdate`、`splitPage`、`fetchScanBound`、`subtractOpenCount`、`ackStreamUpdate`、`ackMetricLabel` / `isAckAnomaly`、`voidOutcomeOf`、`summarizeStreams`、`foldScanGauges`、`reconcileReport`、`ledgerMismatches`。对应的 store_* 与 reconcile 只剩读写和事务。
  - 新增单测：`internal/inbox/store_logic_test.go`（上面的纯逻辑）、`internal/server/player_inbox_admin_server_test.go`（AdminServer 六个方法：故障 in-band、预算、透传、对端地址进审计）、`internal/metrics/metrics_test.go`（Register、gauge 置位与归零、StartHTTP 的开 / 关 / 端口被占）、`internal/svc/servicecontext_test.go`（自检先于拨号、失败回收、接线与生成事件号、Start / Stop 顺序、有界等待、Stop 后 Start 不起任务）。
  - 新增集成用例（`store_integration_test.go`，带 `-tags integration`）：`ScanGauges` 的 SQL 一侧、扫描行计数、Ack 读 R 失败不改状态。
  - 订正上文：集成测试**有**强制不许跳过的开关 `PLAYER_INBOX_REQUIRE_MYSQL_TESTS=1`（`store_integration_test.go:12-20`），设了之后 DSN 为空直接判红。验收时两个变量都要设。
  - HEAD 的 `store_integration_test.go` 本来就不满足 gofmt，会让 `gofmt -l .` 门禁不过，已格式化。现在整个模块 `gofmt -l .` 为空（本会话只跑了 gofmt，没跑任何构建或测试）。
  - `go.sum` 仍要 Codex `go mod tidy`。新测试多用到 `prometheus/client_golang/prometheus/testutil`（同一模块，不加新 require）。
- **第 2 条 AdminTokenEnv：** 维持「已核实一致，不用改」。
- **第 3 条 干净检出编不过：已修。** 删掉 `.gitignore` 里 `go/player_inbox/generated/` 两行，`generated/pb/game/{event_id,message_id}.go` 现在显示为未跟踪，**下一次快照提交必须带上这两个文件**（本会话没有 `git add`）。内容与 friend 的同名文件逐字相同。设计附录 E 的 B 组 `.gitignore` 行与 C 组「不进版本库」行已订正。
- **第 4 条 Redis 总账：已修。** `Get-RedisConnectionLedgerInputs` 的 `$clients` 在 trade 之后加了 player-inbox（global，1 池，无 Pub/Sub），删了「不计入」那条；`k8s_deploy.ps1` 只改了这一段。契约测试期望名单加 `player-inbox`，另钉住这一行的形状（`global/1/0/0`）。文件里没有按客户端个数断言的总数，唯一的总量断言是规划档峰值 ≤ 80% × MAXCLIENTS：手算 1715 ≤ 32000，仍成立。`deploy/k8s/README.md` 的公式与手算值同步：全局 163 → 203，规划峰值 1635 → 1715，zone-up 示例 steady 667 → 707、peak 1131 → 1211。**注意**：mail 会话在同一个 `$clients` 里新加了 mail 一行（Fixed=8），README 的数字还没算进它，由 mail 那边折算。

**死锁审计复查 #23**：2026-10-16 由「数据库死锁审计与修复」会话登记，它没有改 `go/player_inbox` 下的文件，本线也还没改代码。出处 [incident-friend-lock-order-deadlock-2026-09-21.md](../ops/incident-friend-lock-order-deadlock-2026-09-21.md) §9.6.3 #23。

1. **`ensureStream` 无守卫 INSERT IGNORE（机理成立、当前不可达，第 5 批开放外部生产方之前必修）**：`internal/inbox/store.go:356` 在流行缺失时，在 RC 短事务里直接 `INSERT IGNORE`，前面没有普通读探针，也没有守卫行，和审计 #16 修复前同形。
   - **失败方式**：首个插入者在 INSERT 到 COMMIT 之间回滚（ctx 到期、断连、KILL）时，排在它后面的两个及以上后到者会继承间隙锁，互相挡住插入意向锁，报 1213。RC 下同样成环；复核员在真库（RC）上实测，只要两个后到者真排在未提交行后面，18/18 成环。
   - **当前为什么不可达**：v1 唯一的生产方是运维手调的 `GmSanctionPlayer`，逐玩家串行。
   - **修法**：按 trade / guild 的 #16 做法改成「普通读探针 + 守卫行下建行」，或者把建流行前置到建角 / 首登。设计 §6.2 第 6 步（`player-inbox-service.md:690-691`）把「1213 有界重试」当作消环论证，与审计口径 R8 冲突，要同批改掉这段论证。
2. **`ClearNotifyDue` 在 TiDB 下的反向互等（线索，未复核）**：`store.go:371` 走自动提交，TiDB 下是乐观事务，会改索引列 `notify_due_ms`。推演认为它与同流 Enqueue 主事务的提交，会在行 key 与索引 key 上反向互等。处方同 guild `asset_store.go` 的 TiDB 附加规则：改成显式 RC 事务，首句按完整主键 `FOR UPDATE`，再带 CAS 的 UPDATE。已并入 TiDB Phase 1 的待核清单。
3. **HEAD 上缺 `go.sum`**：HEAD 上 `go test ./internal/inbox/...` 报一串 missing go.sum entry，所以这次死锁审计的收件箱真库回归没跑成。与 §2.2 第 1 条是同一件事：Codex `go mod tidy` 之后，`go.sum` 要随下一次快照一起提交。

### 2.3 第 3 批：C++ scene 消费端（已完成）

- **新建**（`cpp/libs/services/scene/player/` 下）：
  - `comp/player_inbox_runtime_comp.h`、`system/player_mutability.h`
  - `system/player_inbox_{rules,registry,system}.{h,cpp}`、`system/gm_sanction_inbox_executor.{h,cpp}`
  - `cpp/nodes/scene/rpc_replies/player_inbox_response_handler.{h,cpp}`
  - `cpp/tests/currency_test/{player_inbox_rules_test, gm_sanction_inbox_executor_test}.cpp`
- **修改**：
  - 业务代码：`player_database_loader.cpp`、`asset_op_system.cpp`（改用 `player_mutability.h`，行为不变）、`currency_system.{h,cpp}`（`AttachDebt` 返回 tip 码，并加币种越界校验）、`main.cpp`。
  - 生成的事件 handler：`player_inbox_event_handler.cpp`，只改守护段。
  - C1 两行调用：`player_landing.cpp`、`player_battle.cpp`，已获「玩家消息投递与位置管理」会话同意。
  - 配置：`config.proto`（`GameConfig.player_inbox` 用字段 8）、`config.cpp`、`bin/etc/game_config.yaml`（`consumer_enabled: false`）。
  - 工程登记：scene 库、`currency_test.vcxproj`；本会话补登了 scene 节点的 `rpc_replies/player_inbox_response_handler`。这处补登**没有随 `de4960bee` 提交**，而 HEAD 的 `main.cpp` 已经依赖它，见 §2.5。
- **依赖 gRPC→muduo 第 3 批**：`0249bee0a` 已把 `GrpcCallFailure` 改成 `code()` / `message()`，并去掉了 context / status。但 `de4960bee` 里的 `player_inbox_response_handler.cpp` 与 `player_inbox_system.{h,cpp}` 仍按旧签名写（`grpc::ClientContext`、`failure.status`、`PlayerInboxStubPtr`、`GRPC_CHANNEL_READY`）。[grpc-muduo-batch3-5/README.md](grpc-muduo-batch3-5/README.md) 已把这三处列为必须迁移的调用点；工作区里已有迁移改动（22:34–22:36，未提交，归 grpc-muduo 那条线）。动这三个文件前先与 grpc-muduo 接手方确认归属，不要回退。
- **偏离设计**（实现代理给出的理由都成立，接手时请复核）：
  - 货币组件缺失时执行器返回 DEFER，不判 REJECTED。
  - 存盘把收件箱账本列为必需组件：缺了就整份拒存，因此改了 5 个手搭玩家的测试，其中 `db_task_dispatch_test.cpp` 是别的会话未提交的新文件，只加了一行。
  - 回包到达时前置闸没过：条目留在本地续做，不重新 Fetch；epoch 变化则丢弃重拉。
  - 遇到未知类型：前面已定的照常确认；MISMATCH 时一条都不确认。
- `currency_test` 没有 CMake 文件，只登记了 vcxproj。

### 2.4 第 4 批：运维、部署与对账（已完成，有遗留）

- **data_service**：`GetPlayerInboxLedger` 的实现与测试；读存档的逻辑抽成 `persisted_player_database.go`，资产账本共用，行为不变。
- **发现与 deadline**：`bin/etc/base_deploy_config.yaml` 已加 `PlayerInboxNodeService.rpc` 前缀与 6000ms deadline；`k8s_deploy.ps1` 加了前缀表、deadline 目标表、`$GoSvcCatalogue`、ConfigMap、连接总账，并新增 `-PlayerInboxConsumer` 参数。
- **部署文件**：`deploy/k8s/manifests/go-svc/player-inbox{,-migrate}.yaml`、`deploy/k8s/player-inbox-alerts.yaml`、`00_init_zone_dbs.sql`、`deploy/k8s/{README,AGENTS}.md`。
- **本地工具**：`go_services.ps1`、`go_svc_image.ps1`、`start_game.ps1`，以及三份 `.tests.ps1`。
- **其他**：`tools/merge_zone/player_inbox_audit.go` 及测试、`docs/ops/player-inbox-runbook.md`、`go/README.md`。
- **偏离**：
  - ConfigMap 只写 `AdminTokenEnv`（变量名），口令走 Secret，不让口令进 ConfigMap。
  - TiDB 索引预切只在 runbook 写了手工 SQL。
- **遗留**：
  - C4（epoch-raise 接入收件箱的 `max_fence_epoch`）**设计已定**：围栏设计 v0.4 的 §9.1 第 c 步、工具来源清单和 §14 第 5 条都写明，独立工具 `go/db/cmd/epoch_raise` 自己经 `PlayerInboxAdmin.GetSummary` 取 `max_fence_epoch` 作为来源之一，任一来源读不到就拒绝执行；`-floor` 只作补充入口；余量 K（§14 第 20 条）定为常数，不要求先排空。工具本身还没落码，由围栏那条线做。收件箱这边只需保证 GetSummary 稳定返回 `max_fence_epoch`。
  - 回档预检自动化、robot GM 处置冒烟、ServiceMonitor 都还没做。

### 2.5 残留未提交项（2026-10-16 核实）

- **scene 节点工程登记**：`cpp/nodes/scene/CMakeLists.txt`、`scene.vcxproj`、`scene.vcxproj.filters` 共 5 行，登记 `rpc_replies/player_inbox_response_handler.{h,cpp}`。HEAD 的 `main.cpp` 已经 include 它并调用 `InitPlayerInboxReply()`，两份工程文件都是显式列表，所以单独检出 HEAD 时 scene 会链接失败。下一次快照提交必须带上这 5 行。
- **`cpp/generated/grpc_client/player_inbox/*` 4 个文件**：22:27 按新模板重生成，未提交。
- **`player_inbox_system.{h,cpp}`、`player_inbox_response_handler.cpp`**：gRPC→muduo 迁移改动，归 grpc-muduo 那条线（§2.3）。
- **文档**：本交接、`docs/README.md` 的索引行、PROGRESS 的收件箱条目。
- **不属于收件箱**：`proto_gen*.yaml`、`data/tip/Tip.xlsx`、`go/shared/generated/tip/segments.go`、`bin/etc/base_deploy_config.yaml`、`deploy/k8s/*.md` 的工作区改动，来自 mail / rank 等其他会话。

## 3. 还没做的事（按顺序）

1. **审读并补齐 Go 服务**（§2.2）。2026-10-16 已做完代码与文档部分，见 §2.2「处理结果」；未编译、未测试，第 2 步的 Codex 验证是它的完成条件。
2. **Codex 验证**（§4），按失败结果修正。
   - **2026-10-18 首轮**（PROGRESS「Codex：player-inbox 续验…」；证据 `build/verification/player-inbox-resume-20261018-023607/verification-summary.json`）：
     - `go/player_inbox` 已通过：tidy、gofmt、vet、build、全量单测（29 个点名用例逐条 PASS）、真库集成 24 个顶层用例（SKIP=0）、`Concurrent|Race -count=20`。
     - tidy 生成了 `go.sum`，`go.mod` 多了 87 行间接依赖。两者都未提交，下一次快照要带上。
     - 首个失败是 `-race`，属于环境问题（本机无 cgo / gcc），不是代码缺陷，跑法见 §4 第 3 步。
     - **还没跑**：`-race`、data_service、merge_zone、go-svc-build、C++、部署脚本与 promtool、迁移、冒烟。
   - 下一轮从 §4 第 3 步的 `-race` 接着跑。
   - **2026-10-18 用户授权**，原话：「同意 Codex 起临时 Linux Go 容器跑 -race（容器内装 gcc，跑完删除），然后继续后面的步骤。」授权只限这一次 `-race` 验证用的临时容器，不能用来改 Windows 工具环境。
   - **2026-10-18 用户再次拍板**：同意 Codex 起临时 `prom/prometheus` 容器跑 `promtool check rules`，跑完删除；360 的信任由用户本人在 360 里给构建输出目录或 `currency_test.exe` 加，加好之后通知 Codex 再跑 C++ 测试。Codex 不改防护、不恢复隔离。
   - **2026-10-18 补充续验**：data_service vet / logic 测试、merge_zone 定向测试、14 个 Go 服务构建通过（按原始 `[ok]` 行核对，之前写成 13 为计数错误）；`game.sln /m:1 /nr:false` Debug|x64 编译通过，0 警告、0 错误，本线未运行 C++ 测试。promtool 18 条规则通过，临时容器删除已核验。原 `-race` 容器由原执行器在模块下载期间中止并清理（exit 143），没有测试结果，本线未重起。
   - **部署脚本修复与完成（最新）**：用户授权继续最小修复新发现及后续同类脚本失败。已修漂移数组聚合、回档四处命令预告的数组/空白参数引用，及测试资源名边界、完整异常采集、时间线事件表达式；真实回档执行与安全门禁不变。九项均 exit 0：deploy 219/219、migrate 34/34、start 20/20、client-entry 109/109、lobby 渲染通过、gate-drain 54/54、discard 27/27、gate-router 57/57、merge-zone 19/19。8 个计数套件合计 539/539；最终 13 个相关 PowerShell 输入前后及交付时哈希均一致，原红/旧摘要保留。最新证据：`build/verification/player-inbox-supplement-20261018T082700Z/verification-summary-after-script-fixes.json`，逐项日志与哈希见同目录 `nine-script-final-summary.json`。
   - **接续分工与输入边界**：已亲核「续验玩家收件箱」聊天中用户通知 360 信任完成并允许协调；该聊天独占 `-race`、C++ 运行测试及独立栈准备，本聊天只收尾上述脚本，不重复起容器或构建。历史 Go 构建后 `go/shared/go.mod` / `go.sum` 被其他会话调整，复用范围由续验线按哈希核验，不能将旧通过记录视为最新输入已验收。
   - **2026-10-18 ~ 19 收尾轮**（证据 `build/verification/player-inbox-completion-20261018T085133Z/completion-status.json`，Claude 2026-10-19 核对）：
     - **`-race` 通过**：临时 Linux 容器（golang:1.27.2-alpine3.24，只读挂载）里 6 个顶层测试各跑 20 次，0 FAIL / 0 SKIP / 0 data race；564 个输入前后无漂移，容器删除已核验（`player-inbox-race-complete-20261018T0852213665213Z/race-result.json`）。
     - **C++ 重编超时，不算通过**：机器 2026-10-18 09:49Z 睡眠，直到 2026-10-19 01:57Z 才唤醒，MSBuild 因此按墙钟超时（exit -1）。这是环境问题，与收件箱代码无关；此前那次 `game.sln` 编译是 0 错误通过的。Codex 已把构建计时器改为只计清醒时长（`QueryUnbiasedInterruptTime`，上限仍为 3h），这次只做了静态检查。
     - **重编在等源码冻结确认**：这次重编由 C++ 共享构建的唯一执行者统一跑，它在等人类确认其他写源码的会话已经停笔（上一次尝试因源码在扫描前后漂移而停下）。
     - 用户已在 360 里加好信任，可以运行 C++ 测试。
     - **仍未跑**：C++ 测试（`currency_test` 中 `PlayerInboxSystemTest` 34 例等）、§4 第 5.5 步的开发库清理、第 6 步迁移、第 7 步七场景冒烟（冒烟驱动只验证过能编译）。到目前为止，收件箱代码本身没有暴露任何失败。
   - **2026-10-19 编译前复核**（Claude；用户改为自己编译）：
     - **做法**：对 C++ 终态做了语义 / 运行期复核，分时序、生命周期、测试三路，不评编译可行性。没找到第一次运行就会失败的测试，也没有 high / medium 问题。
     - **4 条 low 已修**（两个复核代理因 403 失败，这 4 条由 Claude 亲自对代码核实）：
       - **SM-1**：停用的域收到 Ack STALE 时不再排 60 秒重试，也不再打误导性的 `inbox_deferred`。
       - **LC-1**：退出期丢批改为按退出标记判断，冻结 / 交接与退出同时存在时也留 `delivered_undecided`。
       - **LC-2**：退出期闸不过时，账本里已有结局的批首（回放 / UNKNOWN）先入确认队列，只丢需要执行的剩余部分，日志 `result=decided_prefix_queued`。
       - **LC-3**：离开时 Ack 在途，留 `result=ack_inflight corr=` 线索。
     - **改动位置**：`player_inbox_system.cpp`，新增 `UnknownDecisionFor`、`QueueDecidedPrefixOnExit`，修改 `ApplyPending`（闸不过分支与 UNKNOWN 构造）、`OnPlayerLeaving`（kAcking 分支）、`OnAckReply`（STALE 分支）。
     - **新增用例**：`StaleAckOnAHaltedDomainSchedulesNoRetry`、`ExitGateAcknowledgesTheReplayablePrefixAndDropsOnlyNewEntries`、`FrozenExitingPlayerStillLeavesADeliveredUndecidedTrace`；扩充了 `LeavingDoesNotResendWhileAnAckIsInFlight`。`PlayerInboxSystemTest` 现为 37 例。
     - **文档同步**：runbook §6、告警固定查询、设计 §7.7 / §9.4 已同步新的 result 取值。
     - 未编译、未测试，待用户编译。
3. **C4**：**2026-10-16 已定**（§2.4 遗留第 1 条）。围栏接手方已把收件箱列为 epoch-raise 的取 max 来源，写进围栏设计 v0.4 的 §9.1 与 §14 第 5、20 条。收件箱侧剩的事：设计 §11.1 C4 与 runbook §7 按 v0.4 的口径订正（第 C 批做）；GetSummary 的 `max_fence_epoch` 语义保持不变。
4. **对抗式评审**：一致性 / 并发、仓库契合、运维 / 安全三个视角，重点看 Fetch / Ack 事务、seq 分配、围栏、账本落盘与 Ack 的时序。按用户约定，评审只看语义与运行期，不做编译可行性的静态评审。**2026-10-16 已做完**，结论见 §6：1 条 high、7 条 medium、14 条 low，其中 5 项待拍板，其余待修。**2026-10-17 ~ 18 已按拍板分 A / B / C 三批修完**，状态见 §6.4；全部未编译、未测试。
5. **Java 版**（AGENTS §12）：Java 仓 `D:\luyuan\xuanming-server-mmo`，按设计 §12 新建 `game-inbox` 模块与 scene 的 `InboxHooks`。PARITY 已登记「Java 待做」一行。Java 仓允许 Claude 自己编译和测试。
6. **文档收尾**：设计稿附录 E 与 §11 按实际落码回填（附录 E 的 `.gitignore` 口径见 §2.2 第 3 条；设计状态行 :3 仍写「未落码」）；PROGRESS 追加实现条目，并注明 Java 版本号。Java 仓 `PARITY.md:168` 的 mmorpg 列与备注已过期（仍写「未提交，设计定稿，未落码」「D13 待确认」），做 Java 版时一并更新。 **2026-10-17 ~ 18 第 C 批已做大半**：设计稿（含附录 E、§11 回填与新增附录 F）、runbook、Java 仓 PARITY.md 第 168 行都已订正。剩下的是 PROGRESS 实现条目，在 Codex 验证之后补上 Java 版本号。
7. **第 5 批（开放外部生产方）的前置**：先修死锁审计 #23 第 1 条（`ensureStream`），并同批订正设计 §6.2 第 6 步的消环论证（§2.2 末尾）；#23 第 2 条（`ClearNotifyDue`）等 TiDB Phase 1 复核的结论。Java 版的入箱实现照修好后的口径写，不要照抄现在的 `ensureStream`。

**2026-10-19 UTC 续验停止记录（Codex）**：用户已确认 360 信任完成，并授权向脚本线与 gRPC 协调线协调、复用结果。本线新临时 Linux Go 容器完成指定 `-race -count=20`：6 项各 20 次 PASS，0 FAIL / SKIP / race，564 项输入前后无漂移；容器删除后精确名称与归属标签查询均为空。证据 `build/verification/player-inbox-race-complete-20261018T0852213665213Z/`。复用的 data_service vet / logic、merge_zone、14 服务构建、九份脚本（539 条计数断言加 lobby 独立 PASS）、promtool 18 条规则均有原始通过回执；709 项旧 Go 输入当前仅 shared 的 go.mod / go.sum 补锁两项变化，不能宣称完整输入零漂移。收件箱自身 tidy 两文件原 SHA 保留。

共享 gRPC 修正后的当前 `game.sln /m:1 /nr:false` 构建在 `2026-10-19T02:05:20Z` 被运行器超时终止：原生退出 -1、`timedOut=true`，外层退出 1；日志未见编译错误，1814 项源码前后不变。Windows 电源事件证实期间休眠，但超时计费因果尚未实证，不能豁免此次失败。**按用户“失败即停、不重试”停止在此，不自动续跑**：独立 RPC、约定的 C++ 测试（包括 34 例三轮）、§4 第 5.5/6 步清理迁移、七项真实冒烟均未执行；修正前的旧构建仅作历史证据。七场景临时驱动只完成一次编译（1900 输入前后无漂移），不计冒烟通过；本线独占开发栈从未启动。首错、末尾、退出回执、电源事件与 git status 封存在 `build/verification/player-inbox-completion-20261018T085133Z/cpp-failure/`，全项汇总为同根 `verification-summary.json`。已核 MSBuild / cl / link 均无残留；未恢复隔离、改防护或 Windows 工具环境，未提交、未部署集群。后续须先由用户决定构建超时的处理方式。

**2026-10-18 纽约时间 22:16 后的授权续验**：用户新授权处理上述超时、必要时调整临时运行器后重跑 C++，同时要求新失败仍停止、旧过项不重跑。只读确认休眠约 16 小时 8 分；扣除观测休眠的窗口墙钟差约 565 秒，但旧回执缺少实际超时入参与等待内部跟踪，不能判定休眠是唯一直接原因。协调方使用现成 v2，保留 game 10800 秒 / RPC 3600 秒上限，以 `QueryUnbiasedInterruptTime` 排除系统休眠，双记清醒与 UTC 耗时，不修改系统电源或防护。

本次协调方 v2 在 `02-snapshot-before` 即退出 1（`timedOut=false`）：**“快照前后源码漂移，拒绝成功快照”**，尚未启动 MSBuild 或测试。本线按“后续仍失败即停”封存，不自动重试或绕过门禁。后续只读扫描当前有 1815 项源，旧超时至当前 9 项差异、后续双扫描无变化；这不是失败 8 秒内的精确变动清单，失败原件未保存当时两份 map，不能据此归责具体文件。新证据位于 `build/verification/player-inbox-completion-20261018T085133Z/timeout-resume-20261019T0223129196883Z/`（原始失败、首错/末尾、清醒时钟 receipt、git status），旧原件不覆盖。

冒烟准备当前复核：driver 的 1900 输入完全一致且无新增/删除，可复用编译 EXE；11 个 Go 角色的 8 个绑定 EXE 哈希仍一致，但最新 login 和 db/saveoutbox 生产源码已超出旧产物覆盖范围，不能拿旧 EXE 代表当前源码。另线补构建当时 db 通过、login 失败后源码又有修订，亦不是当前输入的成功证明。下次需先协调源码稳定、补齐当前 login/db 所需私有产物；不需重编 driver 或重跑已过测试。收件箱 tidy 两文件原 SHA 保留，独占栈与迁移/七场景仍未启动，本线未提交、未部署。

**2026-10-18 纽约时间 23:21 静态修正交付**：用户最新指示“不用等编译，修复完毕给我”，本线据此只交付超时运行器修正，不再等待或启动构建、测试、迁移和临时栈。现有 `build/verification/grpc-muduo-codex-20261018-005135/cpp-prep/run-current-build-window-v2.ps1` 已静态复核：SHA `E2160B8890334475684AE662A9EE6F0D108DE88FE3E32AB24DDD4F47C4C49097`，PowerShell AST 0 错误；保留 game 10800 秒 / RPC 3600 秒原上限，以清醒时长裁决超时，记录 UTC 与清醒耗时，失败即停、源码漂移和共享库检查仍在，旧证据不覆盖。本轮没有调用 P/Invoke 或进行编译运行验证，不能称完整验收通过。交付回执 `build/verification/player-inbox-completion-20261018T085133Z/static-timeout-fix-delivery.json`，原超时、新快照失败及旧通过记录继续保留；收件箱 tidy 两文件原 SHA 未变，未改系统电源或防护，未提交、未部署。后续运行验证保留为未执行项。

## 4. 给 Codex 的验证顺序（仓库根目录）

> 按 AGENTS §10.1，这些命令由 Codex 执行；Claude 不跑。失败时保留输出末尾、第一条错误和 `git status --porcelain`。

**0. 前置**
- 生成积压已于 2026-10-15 由 Codex 处理完：29 个事件墓碑随 `0f6690604` 入库，收件箱的正规生成见 PROGRESS「2026-10-15 续编完成」条目。**不要再设 `PROTOGEN_ALLOW_MASS_EVENT_TOMBSTONE`。**
- 当前工作区还有 mail / rank、gRPC→muduo、RpcChannel 批 7（`proto/message_id.txt` 墓碑行）等其他会话的 proto、模板与生成器改动，重跑生成会把它们一起产出。执行前先确认这些改动已获授权。

**1. 生成**（2026-10-17 起不只是复核：评审修复改了收件箱 proto，必须重新生成。新增内容是 VoidItem 的 `mode = 7` / `force_verdict = 4` 与两个枚举、`AckPlayerInboxRequest.scene_node_id = 6`、审计表字段 10–15 与 `PLAYER_INBOX_AUDIT_FORCE_VOID = 3`，见 §6.4。go/proto/player_inbox 与 cpp/generated/proto/player_inbox 预期会有差异，引用这些新符号的 Go / C++ 代码在重新生成之前编不过）
- `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build`。必须先跑：旧 exe 会把墓碑号当空号回收，并丢掉墓碑行。不要加 `-UseBinary`（[muduo-rpc-channel.md](../design/muduo-rpc-channel.md) §20.1）。
- `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml`
- 不设变量再跑一次，第二次之后 `git diff` 应为空。
- 核对 `proto/message_id.txt` 没有删行，`N=reserved:<KeyName>` 墓碑行（41 / 74 / 78 / 81）都还在。不要设 `PROTOGEN_ALLOW_MASS_MESSAGE_TOMBSTONE`；收件箱的 255–263（PlayerInbox / PlayerInboxAdmin 各方法与 `DataServiceGetPlayerInboxLedger`）已经分配，复核号不变。生成器若因新墓碑报 Fatal，说明有改动删了方法，先查来源。
- 核对 `player_inbox_event_handler.cpp` 与 `s2s_player_scene_handler.cpp` 的守护段内容还在。

**2. 导表**
- 跑 `dev.bat export`。
- `go/shared/generated/tip/segments.go` 里应有 base 29000 的段，`faults.go` 的新段只有 StoreUnavailable 一个码。HEAD 上这两条已经成立，重跑后复核没变即可。
- 再跑 `cd go/shared && go test ./serverbase/...`。

**3. Go**
- `cd go/player_inbox && go mod tidy && gofmt -l . && go vet ./... && go build ./... && go test ./... -count=1`
- 集成测试：`go test -tags integration ./internal/inbox/... -count=1 -v`，必须设 `PLAYER_INBOX_TEST_MYSQL_DSN`（只能指向一次性库，每个用例都会 DROP 重建四张表）**并设 `PLAYER_INBOX_REQUIRE_MYSQL_TESTS=1`**：设了之后 DSN 为空直接判红，不会整组静默 SKIP。SKIP 不算通过。并发用例另跑 `-run 'Concurrent|Race' -count=20`。
- §2.2 第 3 条修好后，在不跑 proto-gen 的干净检出上再 `go build ./...` 一次，模拟 CI。
- 评审修复新增或改动的单测，要逐条在 `-v` 输出里看到 PASS：
  - 强制终结：`TestProveNeverExecuted*`、`TestDecideForceVoidLocked`、`TestVoidOutcomeOfForce`、`TestVoidForceRejectsBeforeTouchingStore`；
  - 对账：`TestFinalizedLedgerGap`、`TestValidLedgerRingCapacity`、`TestLedgerRecentCapacityMatchesConsumer`（会读 C++ 头文件）、`TestCollectFinalized*`、`TestReconcileRotationCoversEveryShard`、`TestReadLedgersBoundedConcurrency`；
  - 提醒：`TestNotifyRetryDueMs`、`TestNotifyGivesUp`、`TestKafkaNotifierOutcomes`；
  - 分片与日志：`TestRowShard*`、`TestMix64MatchesSplitMix64`、`TestAuditRowFor`、`TestFenceRejectLogsCarrySender`、`TestInboxTestDBNamePattern`；
  - 配置与告警：`TestEtcYamlContractValues`、`TestParseEnqueueTypes`、`TestAlertRulesReferenceRegisteredMetrics`（会读 deploy/k8s/player-inbox-alerts.yaml）；
  - 启动：`TestEnsureSchema`、`TestEnsureSchemaRefusesBadRowLayout`（main 包）。
  - 有两个测试会读仓库里的其他文件，所以要在完整检出下跑。
- 并发：`go test ./internal/inbox/ -race -count=20 -run 'ReadLedgersBounded|ReconcileRecentRound|EveryAfter|LoopsRunWhileLeader|LoopsNeverRun'`。
  - **`-race` 要 cgo**：这台 Windows 机器 `CGO_ENABLED=0`，`gcc` 也不在 PATH 上，所以直接跑会以 exit 2 退出（`-race requires cgo`）。2026-10-18 首轮验证就停在这里。
  - 跑法照 PROGRESS「schemamigrate … Windows race 首次因缺 gcc 无法编译」那条先例：**经用户同意后**，用临时 Linux Go 容器（与仓库同版本的 golang 镜像，容器内装 gcc / musl-dev），只挂载本仓库，执行上面同一条命令（`-mod=readonly`，必要时 `-p=2`）。跑完删掉容器，并核验它已不存在。不要改 Windows 的工具环境。
  - 用户不同意时，这一步记为「`-race` 未跑」，不阻塞后面的步骤。非 race 的并发用例（`Concurrent|Race -count=20`）已在 2026-10-18 通过。
- 集成测试要补跑新用例：`TestIntegrationForceVoid`、`TestIntegrationRecentlyFinalizedOrder`、`TestIntegrationScanGauges`、`TestIntegrationNotifyResendBackoff`、`TestIntegrationCheckRowShards`。DSN 只许指向 `player_inbox_it` 或 `player_inbox_it_<pid>_<n>`，别的库名测试会直接 Fatal。本机自己起一次性 MySQL，不要借 codex-* 容器。
- `cd go/data_service && go vet ./... && go test ./internal/logic/... -count=1`
- `cd tools/merge_zone && go test -run 'PlayerInbox|RunAuditMode|ZoneDBName' -count=1 .`
- 最后跑 `pwsh -File tools/scripts/dev_tools.ps1 -Command go-svc-build`。

**4. C++**
- 必须在当前工作区编译，要带上 §2.5 的工程登记和 gRPC→muduo 第 3 批的改动（模板、`grpc_call_tag.h`、全部 grpc_client 重生成）。单独检出 HEAD 时，grpc_client 与 `grpc_call_tag.h` 本身就对不上。
- 上一轮 Codex 构建了 `currency_test`，但 Windows 拒绝启动、exe 消失。2026-10-18 已查明原因：本机的 360 安全软件按启发式规则 `HEUR/QVM202.0.BF1D.Malware.Gen` 把旧的 `currency_test.exe`（约 87MB）隔离了。这个标签既不证明它是恶意软件，也不证明是误报。证据在 `build/verification/player-inbox-resume-20261018-diagnostic/`。新编出的 exe 大概率会再被隔离。**是否在 360 里给构建输出目录或测试 exe 加信任，只能由用户本人操作；Codex 和 Claude 都不改防护、不恢复隔离。**用户没处理之前，这一步记为「编出来了、跑不起来」。
- 串行编译：`msbuild game.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64`。
- 测试：
  - `currency_test.exe --gtest_filter=PlayerInbox*:GmSanctionInbox*:AssetOp*:Currency*`，再不带过滤器加 `--gtest_shuffle` 跑一遍。`PlayerInbox*` 同时命中 rules 与 system 两组；`PlayerInboxSystemTest` 应为 37 例（2026-10-19 编译前复核新增 3 例）。另跑 `--gtest_filter=PlayerInboxSystemTest.* --gtest_shuffle --gtest_repeat=3`，确认没有 thread_local 或日志残留。
  - 本批 C++ 改动涉及：`player_inbox_system.{h,cpp}`、`player_inbox_rules.{h,cpp}`、`player_database_loader.cpp`、`player_lifecycle.cpp`（一行 include 与一行调用）、`config.cpp`（只改注释）、`player_inbox_runtime_comp.h`（只改注释），新增测试文件 `player_inbox_system_test.cpp`（已登记 `currency_test.vcxproj`）。
  - `configuration_table_test.exe --gtest_filter=GameConfig*`。
  - `bag_test.exe`（不加 shuffle）、`cross_zone_test.exe`。

**5. 部署脚本**
- `pwsh -NoProfile -File tools/scripts/tests/k8s_deploy_contract.tests.ps1`
- `pwsh -NoProfile -File tools/scripts/tests/k8s_migrate_gate.tests.ps1`
- `pwsh -NoProfile -File tools/scripts/tests/start_game_command_contract.tests.ps1`
- 第 C 批之后再加：`k8s_client_entry_contract`、`lobby_authority_render`、`k8s_gate_drain`、`k8s_zone_rollback_discard`（原 `k8s_zone_rollback_flush_guard` 已被另一会话按 zone_data_rollback.md:518 替换）、`k8s_zone_rollback_gate_router_mode`、`dev_tools_merge_zone_contract` 的 `.tests.ps1`。这些测试都会经过新的写前门禁。
- 全部要求 fail=0。`k8s_deploy_contract` 里要重点看收件箱的几条：门禁行（含 head= / worktree= / live=）、线上交叉核对、`-PlayerInboxConsumer` 只核对、Middlewares 镜像、LogQL 选择器契约。
- 前置：`bin/etc/game_config.yaml` 与 `go/player_inbox/etc/player_inbox.yaml` 不能有未提交改动。staging / prod 的真部署遇到脏文件会拒绝。
- 告警语法：把 `deploy/k8s/player-inbox-alerts.yaml` 的 groups 段取出，跑 `promtool check rules`。本机没有 promtool（2026-10-18 前置核对）。要跑就得经用户同意，起一个临时 `prom/prometheus` 容器（`--entrypoint promtool`，只读挂载规则文件，跑完删除）；用户不同意就记为「promtool 未跑」，不阻塞后面的步骤。

**5.5 上线顺序（硬前置）**
- 新的部署脚本（第 C 批）必须先于或同批于新版 Go（第 A 批）上线。旧脚本渲染的 ConfigMap 没有 `Middlewares.StatConf`，新版 Go 和 player-inbox-migrate Job 都会被 Validate 拒启。
- 本机或开发库里按旧分片公式（`player_id % 64`）写过的收件箱数据要先清掉：DROP 三张表与 schema_migrations，或整库重建。否则新版启动会拒启，`-migrate` 以 exit 4 退出（runbook §2.2）。
- 回滚只回滚镜像（`rollout undo`，或用当前脚本加旧镜像）。不要检出旧 commit 重跑部署：那会把 `consumer_enabled` 一起回退（runbook §2、§11）。

**6. 迁移**（启动任何新版 go/db 或 scene 之前必须做）
- 每个 zone 库与落点库都跑 `cd go/db && go run ./cmd/migrate -f <db.yaml> -command plan`。
- plan 里只允许出现放行清单列的 ADD COLUMN（G-03：`player_database` 6 条、`player_database_1` 2 条）。确认后执行 `up`，再用 `SHOW COLUMNS` 核对 `inbox_ledger` 是最后一列。
- 缺列的后果：所有玩家存盘都会失败。

**7. 冒烟**：按设计 §11.5 的 7 个场景，先清数据（AGENTS §6.2）。

**8. Java 契约同步**：Java 仓工作区里已有 `mmorpg@a105c3efd` 的 ContractSync 结果（未提交），是商务契约会话 22:29 顺带跑的，同批还有 mail / rank 等契约。收件箱契约此后如有变化，就在 Java 仓重跑 `java tools/ContractSync.java --mmorpg <本仓>`；然后 `./mvnw -B clean install`（`target` 里目前没有 PlayerInbox 产物）。提交前先与其他契约的归属方协调，随 Java 仓一起提交。

## 5. 协作与注意

- **「玩家消息投递与位置管理」会话**：负责落地钩子、版本摘要、MySQL 围栏（18/19）和 epoch-raise。C1 两行调用已获它同意；它提醒的三点（落地调用幂等且开销小、冻结期间跳过、调用放在 `PublishAssetWake` 之后）请在评审时核对。
  - 2026-10-16 只读核对了这三点：调用在 `player_landing.cpp:211`，位于 `PublishAssetWake` 之后；冻结期间不是在调用点跳过，而是由前置闸 `CheckApplyGate` 的 kFrozen 分支延后。这三点的出处只在本文，没进设计 §11.1 C1，文档收尾时补上。
  - 该会话已于 2026-10-16 交接，后续协调找它的接手方。它的交接文档 [handoff-landing-recovery-and-fencing-20261016.md](handoff-landing-recovery-and-fencing-20261016.md) §5 记了与本线的约定：两行调用已同意；`player_database` 20 号归收件箱；摘要 §3.3 按「将来要接入，接法届时另行拍板」的口径，且下发时必须过滤这个域，否则会泄露 GM 处置。C4 不在 §5，记在它的 §3.4 末条，与围栏设计 §14 第 5 条（即那份清单的第 6 项）一起定。
  - 对方文档里与本线相关的下列四处问题，**2026-10-16 已由它的接手方全部处理**（围栏设计 v0.4、其交接 §3.4 与 §4 第 2 步、摘要设计文首与 §1.3）。下面保留原文备查：
    - 围栏设计 §9.1 第 c 步与入口段没有列收件箱这个取 max 来源（§3 第 3 条）；
    - 其交接 §3.4 的 13 项清单没有标对应的设计 §14 编号，也漏了 §14 第 20 条（余量 K）；
    - 其交接 §4 第 2 步的迁移放行写的是 `player_database` 5 列，应为 6 列（含 `inbox_ledger`），以 90-consistency G-03 为准，并与本文 §4 第 6 步是同一次迁移；
    - 摘要设计 `landing-recovery-sync-digest.md` 的 :7 和 :47 仍写收件箱「作为本摘要的一个域接入」，与同文 §3.3 的「v1 不接」矛盾。
- **gRPC→muduo 第 3–5 批**（[grpc-muduo-batch3-5/README.md](grpc-muduo-batch3-5/README.md)）：收件箱 scene 侧的应答适配、系统与选点都在它的待迁调用点里（§2.3）。C++ 编译要和它的改动放在同一工作区状态下进行（§4 第 4 步）。2026-10-16 它确认：第 3 批对这三个文件的迁移已定稿，第 4、5 批不碰它们，收件箱可以直接改。它唯一的后续是 `docs/handoff/grpc-muduo-batch3-5/global-handler-removal.md` 步骤 3（交 Codex，需负责人分批授权）：会把 Fetch / Ack 改成每次调用带回调，并删掉 `player_inbox_response_handler.cpp` 与全局处理器装配，届时会改发送点和 `OnFetch*` / `OnAck*` 入口。所以收件箱改完 C++ 状态机后，要在 PROGRESS 条目里写清改了哪些函数，供它重新定位。
- **生成积压**：如果有人在收件箱改动落盘后跑了生成器，收件箱的生成产物会和积压一起出现。这在预期之内。
- **本会话没有执行**任何编译、测试、生成或提交；Tip 表补丁脚本由实现代理按授权运行过一次。

## 6. 实现评审（2026-10-16，§3 第 4 条；Claude，只读，未编译、未测试）

**做法**：
- 四路评审并行：一致性 / 并发分 Go 侧与跨进程时序两路，另有仓库契合、运维 / 安全。
- 每路的问题交给一个独立复核者，任务是尽力推翻；之后一个查漏者补漏，它新报的问题再逐条复核。
- 评审对象是当前工作区，含 §2.2「处理结果」的未提交改动。只看语义与运行期，不评能否编过。
- 结果：复核后保留 22 条（1 high、7 medium、14 low），另有 2 条被推翻、列为可选加固。行号按 2026-10-16 工作区。

**已核实没问题的点**（下一位不必重查）：
- **子会话重构**：§2.2 处理结果里抽出的纯逻辑，与 HEAD 逐个对比都等价。唯一有意的行为变化是 `fetch_scanned_rows` 的口径。
- **Go 侧事务与锁**：锁序是流行 FOR UPDATE → 主键条件写，单事务只碰一条流，不成环。open_count、next_seq、fence 都在锁内按快照绝对值写回，重试安全。
- **Go 侧围栏与竞态**：R 读失败时，Fetch 与 Ack 一致 fail-closed。`ClassifyStale` 把 R < req 也记成 fence_regressed，这条偏离的理由成立。Void 与 Fetch 互斥。保留期（≥ 8 天）大于签发窗，不会因行被删而重复入箱。提醒 CAS 以 next_seq 为令牌。
- **C++ 侧落盘与确认**：效果与账本是同一次 Marshal、同一个 DBTask。Ack 只在 Redis 落地回调更新快照之后发出。关联号节点级单调，回调只捕获 player_id / domain。落地钩子排在 epoch 采纳之后。
- **§2.3 的四条偏离**：第 1 条（缺货币组件判 DEFER）、第 2 条（账本必需组件）理由成立；第 3 条不成立，见 L1；第 4 条引出 M1。

### 6.1 high：上线开消费之前必须修

- **H1 消费开关不粘滞，例行发布会静默关掉消费**（`tools/scripts/k8s_deploy.ps1:47-52`、`:3814`、`:8609`；`k8s_image.ps1:470`）
  - **失败方式**：`-PlayerInboxConsumer` 默认 off，包装入口不透传，zone-up 每次都重写 node-config。按 §11.7 打开消费之后，任何一次 `k8s-release-zone` / `zone-up` / `zone-rollback` 都会把 `consumer_enabled` 写回 false。之后 GM 处置照样回 created=true，但永远不执行。
  - **为什么没人发现**：条目 delivery_count=0，进不了任何告警。
  - **修法（待拍板）**：
    - 把开关放进 git 跟踪的 yaml，用 `Get-AuthoritativeYamlBlock` 复制（同 EnqueueTypes）。不要从集群现状读，zone-rollback 会删 namespace。
    - 加写前门禁：渲染值是 off，而 EnqueueTypes 非空时拒绝部署，除非显式传 `-AllowPlayerInboxConsumerOff`。
    - 加告警：`increase(player_inbox_enqueue_total{result="created"}[6h]) > 0` 且 `increase(player_inbox_fetch_total{path="locked"}[6h]) == 0`。
    - scene 关闭消费时周期性打 WARN。
    - 同步改契约测试 `k8s_deploy_contract.tests.ps1:2159` 和 runbook §2。

### 6.2 medium

- **M1 VoidItem 解不开 MISMATCH / 未知类型造成的停用，runbook 的处置无效**（`player_inbox_system.cpp:582-642`、`player_inbox_rules.cpp:166-187`；runbook :156、:195）
  - **失败方式**：判定顺序是先查执行器、再做回放核对，最后才看 directive。已投递的行只能挂 VOID 指令，所以每次登录都会再次停用，该域后续的解封、欠款都不执行。v1 也没有终结已投递行的运维接口（IB-5）。
  - **修法（待拍板，复核者纠正了原建议）**：**不要**把 VOID 判定整体提到回放核对之前，否则 kReplay 会把已执行的行报成 SKIPPED，违反 IB-4 / IB-9。正确做法分两类：
    - kNew 且 directive==VOID：不查执行器，直接记 SKIPPED。这能解掉未知类型的停用。
    - kMismatch：在服务端加一个带审计的强制终结（VoidItem 的强制模式），要求先经 `GetPlayerInboxLedger` 证明账本环里同 seq 是另一个 request_id，再终结为 VOIDED。这需要改 proto。
    - 同步改设计 §7.6 / §7.9、runbook §5 / §8 第 3 步，以及 :621 的注释。
- **M2 AttachDebt 合并同币种欠款，并覆盖 expiresAt / reason / gmOperator**（`currency_system.cpp:357-369`、`:176-197`）
  - **失败方式**：一张小单就能让大额欠款提前失效，过期欠款的剩余部分也会被后来的单子复活。合并逻辑是收件箱之前就有的，但收件箱是它在 v1 唯一的生产入口。
  - **修法（待拍板）**：v1 最小修法分四条：
    - 已过期的条目先清掉剩余，再追加新欠款；
    - 现有欠款仍有效、而到期时间不同时，回一个专用的可见 REJECTED tip。要在 Tip.xlsx 加一行；Tip.xlsx 工作区里有 mail / rank 的未提交改动，要协调；
    - expires_at 非 0 且执行时已过期，判 REJECTED(kPlayerInboxPayloadInvalid)；
    - 不覆盖 reason / gmOperator。
    - 长期修法是一单一条（以 request_id 为键），要改持久 proto、做迁移，Java 版也要对齐。
- **M3 挂错的欠款在 v1 没有撤销路径**（`registry.go:90-92`；`player_rollback_handler.cpp` 的 Waive / Adjust / Query 都是 TODO 空桩）
  - **失败方式**：误输金额后，该玩家这个币种的收入会一直被全额抵扣，没有任何工具能纠正。
  - **修法（待拍板）**：
    - 同域的撤销类型上线之前，`GM_ATTACH_CURRENCY_DEBT` 先不进 EnqueueTypes。撤销类型要走 AdjustDebt 按金额减少；不要用 WaiveDebt，它会连带清掉合并进来的其他单（M2）。
    - 入箱时按币种设单笔上限，放在判重之后。
    - 补一个只读欠款查询。
    - runbook 写明 v1 无撤销，并写明 VoidItem 之后要用 LookupByRequest 确认终态。
- **M4 「已投递未确认」告警会被不再上线的玩家永久顶住**（`store_maintenance.go:111-118`、`player-inbox-alerts.yaml:146`；PoisonSuspect 同理）
  - **失败方式**：典型场景是 GM 先处置、紧接着封号踢下线：实体销毁时 ackQueue 跟着丢，这一行就永远已投递未确认，告警常亮，掩盖之后真正的卡住。
  - **修法**：
    - 告警用的 gauge 只算 `last_delivered_ms ≥ now−24h` 的行，或者排除没有 `player:{id}:location` 的 owner；更老的行另出一个 info gauge。
    - scene 在登出 / 最终存盘回调里，对已 durable 的 ackQueue 尽力补发一次 Ack，不等回包。
    - 同步改设计 §9.4 与 runbook §6。
- **M5 定时对账每轮都从 shard 0 开始，满 2000 名玩家就截断，而且不留痕迹**（`reconcile.go:94-116`、`store_maintenance.go:142-145`）
  - **失败方式**：设计 §2.2 自己预期会有批量挂欠款；量一大，高号分片在 24h 窗口内一次都对不上。
  - **修法**：
    - 起始分片用 `(nowMs/interval)%64` 轮转，不依赖内存状态；
    - 截断时计 `player_inbox_reconcile_truncated_total` 并打 ERROR；
    - 单片查询加 ORDER BY，按 keyset 分页；
    - 账本读改为有界并发，避免撞上 2 分钟的轮预算。
- **M6 对账只比水位，丢数据会被之后的条目掩盖**（`decide.go:465-473`、`:207-221`）
  - **失败方式**：seq=a 已 APPLIED，随后 Redis 丢尾，玩家重登时水位回到 a-1；之后 b(>a) 一执行，水位越过 a，定时对账和 ReconcilePlayer 都看不出来。设计 §8 #15 说这种丢失「可发现」，这时并不成立。
  - **修法（复核者订正了下界条件）**：
    - 增加空洞判定，两个对账入口共用。条件：消费方终结；状态是 APPLIED / REJECTED / VOIDED（排除 SETTLED_UNKNOWN）；seq ≤ decided_through；并且 `len(recent) < 32 || seq ≥ recent[0].seq`。环不满 32 条说明从没裁剪过，这样「首次处置就丢」的情况也能抓到。
    - 环里同 seq 而 request_id 或结局不同，也要报。
    - Go 侧常量 32 要与 C++ 的 `kRecentCapacity` 对齐。
    - 测试数据改成真实的环。
- **M7 收件箱仍以「Redis 已落盘」作为确认依据，而资产通道已改为 SQL 提交证据**（plausible；`player_inbox_system.cpp:427-455`、`player_inbox_ledger_logic.go`）
  - **现状**：设计 §7.10 的理由是「与资产通道相同」，这个前提已经不成立。go/db 现有的 `PlayerAssetSQLCommit` 证据里不含 inbox_ledger，没法直接复用。资产 SQL 提交那条线本身也还在工作区实施中。
  - **修法**：先在设计 §7.10 / §8 把它列为显式残余风险；再与资产 SQL 提交那条线的负责人对齐：扩展证据，或另发一份 PlayerInboxSQLCommit。

### 6.3 low

- **L1 偏离第 3 条的理由不成立**（`player_inbox_system.cpp:564`）：回包到达时前置闸不过，条目就留在本地续做；这期间挂上的 VOID 看不见，解冻后仍按旧副本执行并报 APPLIED。修法：前置闸不过时清掉本地条目、回 kIdle，下次重新拉取（只多 +1 次 delivery_count）。只有执行器 DEFER 时才保留本地续做。同步改设计 §7.5 与 §2.3 偏离第 3 条。
- **L2 新落地重置 kApplying 时不清 ackQueue**（`player_inbox_system.cpp:871-875`）：与 DEFER 前缀叠加时，Ack 里会出现重复 seq，服务端回 InvalidParameter，scene 当作故障无限重发。今天还是潜伏问题。修法：带非空 ackQueue 时先转 kAwaitDurable，并置 recheck；Ack 回 InvalidParameter 时按协议异常 Halt。
- **L3 PlayerInboxSystem 状态机没有任何单测**（`player_inbox_system.h:71`）：测试注入点都有，但没人调用。按 `asset_op_system_test.cpp` 的写法补 `player_inbox_system_test.cpp`，要覆盖 M1 / L1 / L2 的回归。
- **L4 对账配置与易主**（`config.go:334`）：Validate 没要求 `window×60 ≥ 2×interval`；当选后第一轮对账要等满一个间隔；也没有对账的存活指标。补 `player_inbox_reconcile_rounds_total{result}` 和对应告警。
- **L5 `RowShard = player_id % 64` 对存量 bwmarrin ID 会退化到 shard 0**（`shard.go:14`）：新角色走号段、分布均匀，存量玩家会集中到一片。分片公式是建表契约，**首次建表之前**改成 `mix64(player_id) % 64` 成本最低，上线后再改等于迁移全部主键（待拍板）。所有重算 shard 的地方要走同一函数。`placement.RouteRowShard` 的同类问题登记给 data_service 的负责人。
- **L6 在线提醒 NoNode / Error 永不落定**（`notify.go:139`）：失败行永远占着队头，积压告警常亮。修法：失败时在同一条 CAS 里把 `notify_due_ms` 推到 now + 指数退避；不可恢复的结局直接落定，交给落地拉取。
- **L7 核心 Redis 客户端没开 `ContextTimeoutEnabled`**（`servicecontext.go:131`）：500ms / 200ms 的预算管不住 socket 读，in-band 回故障的契约会被击穿。修法：开启它，并显式设 ReadTimeout / WriteTimeout / PoolSize（PoolSize 与 Redis 总账一致）；同步核对 leader 的 FenceAfter 门槛。trade 有同样问题，登记给对应负责人。
- **L8 审计无法归因到人**（`admin_auth.go:67`）：按 runbook 经 port-forward 调用时，peer_addr 恒为回环；口令又是共享的。短期在文档里写明靠 kube-apiserver 的 portforward 审计日志对人；中期由凭据推出 operator。
- **L9 审计行不记「做了什么」**（`player_inbox_table.proto:85`）：条目行 30 天后被清理，之后就看不出处置类型、金额和 request_id。按加列纪律给审计表加 msg_type / request_id / issued_at_ms / content_sha256 / 载荷字节，VOID 审计再加 void_reason。
- **L10 围栏 / Ack 异常的 ERROR 日志缺发送方身份**（`store_consume.go:85`）：告警描述让值班按 scene_node_id 定位，但日志里没有这个字段。日志加 peer 与 scene_node_id；`AckPlayerInboxRequest` 追加只用于日志的 scene_node_id（新字段号）。
- **L11 同一处对账不一致在 24h 内被重复计数**（`reconcile.go:136`）：critical 告警会持续约 25h，处置后也消不掉。修法：只首次计数，或改成「当前不一致数」gauge，再配一张豁免表。
- **L12 没配 go-zero Stat 的 `IgnoreContentMethods`**（`etc/player_inbox.yaml`）：Fetch / Ack 的全部请求体都按 INFO 进 Loki。把 Fetch / Ack 列进去；先补 L9 / L10，以免丢掉追溯线索。
- **L13 scene 本地停用没有可执行的告警**（`player_database_loader.cpp:157`、`player_inbox_system.cpp:815-823`）：账本损坏、注册表自检失败时，在线玩家的处置一直不执行，看起来和离线一样。修法：日志统一加 `metric=inbox_ledger_invalid` / `metric=inbox_registry_broken` 并列进告警文件；不会自愈的 Defer 原因周期性 WARN；runbook 补「账本损坏」一节（不要清空账本重建，那会重复执行已执行过的处置）。
- **L14 集成测试会 DROP 任意库**（`store_integration_test.go:129-142`）：照 guild 的做法，DROP 之前用 `SELECT DATABASE()` 的结果匹配 `^(player_inbox_it|player_inbox_it_\d+_\d+)$`，并补一条不带 integration 标签的单测。`go/trade/internal/data/listing_repo_integration_test.go` 有同样的缺口，登记给 trade。

**被推翻、只作可选加固**：
- **GAP-1**：入箱判重排在白名单 / 载荷 / 玩家存在三项校验之后。这与设计 §6.2 一致，按 runbook 续跑会拿到 DUPLICATE。
- **GAP-4**：消费方水位没有结构上界。仓库里不存在能写出这种值的写入方；可选加固是 `≥ 2^63` 时 fail-closed。

**哪些要拍板**：H1（开关来源）、M1（是否加强制终结接口，改 proto）、M2（新 tip 码与合并语义）、M3（ATTACH_DEBT 是否暂不进 EnqueueTypes）、L5（分片公式）。M7 要与资产 SQL 提交那条线协调。其余各条有明确修法，可以直接做。

**用户拍板（2026-10-16）**：
- **H1**：开关写进 git 跟踪的 yaml，用 `Get-AuthoritativeYamlBlock` 复制；同时加写前门禁与「入箱在涨、Fetch 不涨」告警。
- **M1**：两处都修：scene 侧 kNew+VOID 不查执行器、直接 SKIPPED；服务端 VoidItem 加强制模式，经账本证明后带审计终结为 VOIDED。要改 proto，Java 契约同批跟进。
- **M2+M3**：v1 先不开 `GM_ATTACH_CURRENCY_DEBT`，EnqueueTypes 只放封锁 / 解封。合并语义、撤销类型和单笔上限后续一起做。
- **其余**：分三批修（Go 服务 → C++ scene → 部署 / 运维 / 文档），每批交 Codex 验证。L5 趁首次建表前把分片改成 `mix64(player_id) % 64`。M7 本次只登记为残余风险，并与资产 SQL 提交那条线协调。
- **不在本轮**：死锁审计 #23（第 5 批前必修，§2.2 末尾）、GAP-1 / GAP-4 的可选加固。

### 6.4 修复状态（2026-10-16 ~ 17，Claude 分三批派代理落码；**全部未编译、未测试，待 Codex 按 §4 验证**）

**做法**：每批里的实现代理按顺序执行，互不并行写同一文件；之后由两名复核者对抗式复核，再由一个代理修掉复核确认的问题。第 A 批的 A1–A3 大部分改动已被用户快照提交（e76f6f747…360d56ae5，luyuan-cpp；VoidItem 强制模式的 proto 与 force_void_test.go 在 e76f6f747）带进 main，其余仍在工作区。

| 条目 | 状态 | 落在哪里 |
|---|---|---|
| H1 消费开关 | 已做 | 权威值只认 git 跟踪的 `bin/etc/game_config.yaml`，部署时整段复制 `player_inbox` 段；`-PlayerInboxConsumer` 只做核对，不改渲染值。写前门禁有三层：两份 yaml 互相核对；非 DryRun 时与线上 ConfigMap 交叉核对；staging / prod 拒绝未提交的配置。另有 `-AllowPlayerInboxConsumerOff` 豁免（C1 + 修复）。告警 `PlayerInboxConsumerMaybeDisabled`（C2）。scene 消费关闭时启动打 WARN，之后每 5 分钟补打 `metric=inbox_consumer_disabled`（C3） |
| M1 VoidItem 解卡 | 已做 | 服务端：VoidItem `mode=FORCE_MISMATCH`，先读已落盘账本证明同 seq 是另一个 request_id 才终结为 VOIDED，带审计 FORCE_VOID（A1）。scene：先 ClassifyDelivery，kNew+VOID 直接 SKIPPED、不查执行器（B1） |
| M2 / M3 ATTACH_DEBT | v1 不开 | `config.ParseEnqueueTypes` 启动即拒 ATTACH_DEBT；EnqueueTypes 示例只留封锁 / 解封（A3）。合并语义、撤销类型与单笔上限后续一起做 |
| M4 已投递未确认告警 | 已做，有残余 | 服务端：告警 gauge 只算近 24h 投递的行，更老的进 `player_inbox_delivered_unacked_stale`（A3）。scene：退出收尾 `FinishExitAfterPersist` 里尽力补发一次 Ack（B2）。**残余**：已投递但退出时还没定结局的行（Fetch 在途、退出期被前置闸丢掉、DEFER 剩下的）补不了，只留日志 `metric=inbox_leave_ack result=delivered_undecided|fetch_inflight`，等下次落地 |
| M5 对账截断 | 已做 | 起始分片按步长 4 轮转，16 轮扫遍 64 片；截断计 `player_inbox_reconcile_truncated_total` 并打 ERROR；单片 keyset 分页；账本 8 路并发读（A2 + 修复 R1） |
| M6 环内空洞 | 已做 | `finalizedLedgerGap` 谓词两处共用；Go 常量 32 由测试从 C++ 头文件抽数比对（A2 + 修复 R4） |
| M7 SQL 提交证据 | 只登记 | 设计 §7.10 / §8 写为残余风险（C5）；要与资产 SQL 提交那条线协调 |
| L1 前置闸在回包时不过 | 已做（扩展） | 丢本地副本重拉；执行器 DEFER 后续做时闸不过也丢副本、先确认前缀（B1） |
| L2 新落地重置 | 已做 | ackQueue 非空先转 kAwaitDurable 确认前缀；Ack 回 InvalidParameter 停用；StartFetch 加不变量守卫（B1） |
| L3 状态机单测 | 已做 | 新建 `cpp/tests/currency_test/player_inbox_system_test.cpp`（34 例：第 B 批 31 例 + C3 补 3 例，含退出收尾端到端一例），已登记 vcxproj（B3 + 修复 F1/F2） |
| L4 对账配置与易主 | 已做 | Validate 要求窗口 ≥ 2×间隔；当选后首轮 1–5 分钟；`player_inbox_reconcile_rounds_total{result}`（A2） |
| L5 分片公式 | 已做 | `RowShard = mix64(player_id) % 64`；启动与 `-migrate` 在表结构对齐后跑 `inbox.CheckRowShards`，不一致拒启 / exit 4（A4 + 本会话接线 `player_inbox.go`） |
| L6 提醒退避 | 已做 | 失败按 next_seq CAS 退避 30s 起封顶 30min；`node_missing` 悬满 1 小时才落定；`no_node` 只剩位置记录残缺（A3 + 修复 R3） |
| L7 Redis 超时 | 已做 | `ContextTimeoutEnabled`、读写超时 1s、PoolSize 20（A3）；trade 同类问题登记给 trade |
| L8 审计归因 | 只写文档 | runbook（C4） |
| L9 审计列 | 已做 | 审计表追加字段 10–15，内容一律从条目行复制（A4） |
| L10 发送方身份 | 已做 | 围栏 / Ack 异常日志带 peer 与 scene_node_id；Ack 请求追加 `scene_node_id = 6`，scene 已填（A4 + B1 + 修复 R5） |
| L11 重复计数 | 已做 | 改为「当前不一致数」gauge `player_inbox_reconcile_inconsistent_rows`；告警表达式由第 C 批改（C2） |
| L12 请求体日志 | 已做 | 服务 yaml 的 `IgnoreContentMethods` 列 Fetch / Ack，Validate 钉住（A3）；ConfigMap 由第 C 批镜像（C1，**硬前置**） |
| L13 scene 本地停用告警 | 已做 | `metric=inbox_ledger_invalid` / `inbox_registry_broken`，补打节拍钉在 10 分钟告警窗口内（B1 + 修复 F3）；告警固定查询由第 C 批加（C2） |
| L14 测试库守卫 | 已做 | DROP 前白名单 `^(player_inbox_it|player_inbox_it_\d+_\d+)$`（A4）；trade 同类缺口登记给 trade |

**复核中新发现并已修的**：
- 第 A 批：对账轮转覆盖不足（R1）、强制终结接受超长环（R2）、node_missing 被当成已落定（R3）、环容量测试没有真比对 C++（R4）、日志键 `scene_node=` 与 `scene_node_id=` 不统一（R5）、旧分片公式的行是「静默不可见」而不是「读出被拒」（R3 注释与行布局核对）。
- 第 B 批：退出补发缺口（M4，已补日志）、退出收尾端到端用例缺失（F1）、旧快照下的 IB-3 用例缺失（F2）、周期 WARN 节拍与 10 分钟告警窗口不匹配（F3，节拍改为 8 次）、MISMATCH 日志里账本字段恒为 0（F4）。

**部署前置**：新版 Go 的 Validate 要求 ConfigMap 带 `Middlewares.StatConf`，缺了就拒启。对账 critical 告警原先引用的指标已经删掉。第 C 批已补齐这两项（C1 镜像 Middlewares，C2 改写告警），所以部署脚本必须先于或同批于这版 Go 上线（§4 第 5.5 步）。

**第 C 批（2026-10-17 ~ 18）**：
- **C1 部署脚本**：H1，见上表；Middlewares 段从服务 yaml 镜像进 ConfigMap（player-inbox-migrate Job 用同一份）；核对过 Redis 总账与 PoolSize 20 一致。
- **C2 告警**：
  - 对账告警改用 gauge `player_inbox_reconcile_inconsistent_rows`。
  - 新增 `ReconcileTruncated`、`ReconcileStalled`、`RoundsFailing`、`ReconcileLedgerInvalid`、`DeliveredUnackedStale`、`ForceVoided`、`ConsumerMaybeDisabled`，共四组 18 条。
  - scene 侧 LogQL 固定查询补齐 5 条，选择器改为 `{job="cpp_nodes", service="scene"}`：原来的 `{app=~"scene.*"}` 在仓库的日志管线里永远匹配不到。
  - 新增两个守卫：`TestAlertRulesReferenceRegisteredMetrics`（告警引用的指标都已注册）、LogQL 选择器契约测试。
- **C3 scene**：消费关闭时周期 WARN；新增 3 个系统级用例。
- **C4 / C5 / C6**：runbook、设计稿（新增附录 F，逐节列出订正）、Java 仓 PARITY.md 第 168 行（含 12 条「Java 落码须对齐」）都已按代码订正。
- **复核中新发现并已修的**：
  - 门禁只看当前检出（high）：补线上交叉核对；回滚口径改为只回滚镜像。
  - LogQL 选择器不匹配（high）。
  - 包装入口没有豁免参数：报错里写明恢复路径。
  - 部署读的是工作区而不是 git：staging / prod 拒绝脏文件。
  - 文档里有 6 处说法超出代码能力，或与代码不一致。
  - `config.cpp:289` 的过时注释由本会话改掉。

**第 C 批留下的待定或待登记**：
1. 消费开关的权威文件是 `bin/etc/game_config.yaml`，它同时是本机 scene 的运行配置。如果要把 K8s 和本机分开，得另建一份 K8s 专用的 git yaml。
2. 写前门禁同时拦 zone-up、all-up、infra-up；包装入口（dev_tools / k8s_image / k8s_zone_rollback）不透传豁免开关。
3. 线上交叉核对需要部署身份能在集群范围 list ConfigMap。RBAC 以后收紧的话，改为只列 zones.json 里的 namespace。
4. `k8s_zone_rollback` 第 0 步是 DryRun，看不到线上层，也拦不住未提交的配置，所以第 6 步可能在 zone 已停之后才拒绝。是否加第 0 步预读，归回滚脚本那条线。
5. 不合法账本的口径：Go 的 validLedger 另外检查 `request_id == 0`，C++ 的 ValidateLedger 不查。文档已如实写明差异；要两边一致，就在 C++ 侧补上这一条。
6. 告警守卫只核对指标名，不核对 label 取值（`result="created"`、`path="locked"` 等）。另外还没加 notify 失败率告警。
7. 登记给其他负责人：
   - `docs/ops/release-checklist.md` 的回滚段仍写「改用旧 commit 重跑 zone-up」；
   - `asset-channel-alerts.yaml` 与 `data-service-audit-alerts.yaml` 的 `{app=...}` 选择器同样永远匹配不到；
   - Java 仓 `docs/porting/data-ops-spec.md` 仍把欠款放在离线编辑，与 D13 不符。

**按推荐口径先定、可改的小取舍**（不阻塞，用户要改就说）：
1. VoidItem 回包新增 `force_verdict = 4` 与枚举 `PlayerInboxForceVoidVerdict`，用来区分强制终结被拒的原因。这是约定之外新增的契约面，建议保留。
2. ATTACH_DEBT 在 `ParseEnqueueTypes` 里硬拒，而不是只从默认配置里拿掉。
3. `delivered_unacked` 计数也只算 24h 窗口；24h 是代码常量，不是配置键。
4. Redis PoolSize 取 20（沿用 yaml 已有值）。
5. 提醒时节点身份不明确算重试，不算落定。
6. `ReconcilePlayer` 遇到不合法账本回 StoreUnavailable，没有在回包加 `ledger_invalid` 字段（加字段是 proto 变更）。
7. 对账窗口没有要求 ≥ 16 个间隔：默认 24 轮满足覆盖保证，更小的配置只在截断时打 ERROR。
8. 审计表没有加 `producer_kind` 列：v1 只有 GM 一种生产方。
9. 同 seq、同 request_id、只有 msg_type 不同的 MISMATCH，强制终结按 fail-closed 拒绝（只可能来自数据损坏，需人工）。
10. 退出收尾的一个快照时点缺口：`HandlePlayerAsyncSaved` 的 kFinish 分支在替换快照前就销毁实体，此时少补发一次 Ack（安全，只是少减一次残留）。
11. Fetch 回包 tip=InvalidParameter 仍按故障退避，没有改成停用。
12. L1 扩展到执行器 DEFER 后续做时的闸检查（见上表）。
13. `DeliveryClassName` 在生产代码里已无调用方，保留为日志辅助。
14. registry_broken 的周期补打没有单测（SelfCheck 没有注入点）。

**登记给其他负责人**：
- **trade**：L7 同类问题，SharedRedis 没开 `ContextTimeoutEnabled`；L14 同类缺口，`go/trade/internal/data/listing_repo_integration_test.go` 会 DROP 任意库。
- **data_service**：`placement.RouteRowShard` 也是 `player_id % 64`，对存量 bwmarrin ID 会退化。

**给 gRPC→muduo 那条线（global-handler-removal 步骤 3 重新定位用）**：
- `player_inbox_system.cpp` 里，第 B 批改过的函数：
  - 新增：`DropPendingForRefetch`、`EnterAwaitDurable`、`LogDeliveredUndecided`、`LogRegistryBroken`、`IsStuckDeferReason`、`PlayerInboxSystem::OnPlayerLeaving`。
  - 修改：`ApplyPending`、`StartFetch`、`SendAck`、`ScheduleRetry`、`AfterBatchDecided`、`Initialize`、`OnPlayerLanded`、`OnAckReply`、`Tick`。
  - 第 C 批的 C3 还会再动 `Initialize` / `Tick`。
- `player_inbox_rules` 新增：`ItemAction`、`PlanItem`、`IsStuckWarnDue` 及相关常量；`ClassifyDelivery` 判 kMismatch 时也会拷出环里那一条，只供日志用。
- `player_lifecycle.cpp`：`FinishExitAfterPersist` 加了一行 `OnPlayerLeaving` 调用，文件头加了一行 include。
- `OnPlayerLeaving` 经 `SendAck` 发出的那次 Ack，回调不捕获实体，回包按 player_id 加关联号找实体。改成每次调用带回调之后，这一点要保持。
