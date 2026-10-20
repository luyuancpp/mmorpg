# 全仓数据问题与覆盖回档审计·第二轮（2026-10-19）

> 只读静态审计，**未编译、未运行**。对象是 2026-10-19 的工作区，包括其他会话尚未提交的改动。
> 本文只登记问题和修复方向，不含代码修改。修复要按 AGENTS.md §10.1 交 Codex 验证，并按 §12 同步 Java 版。
> 第一轮见 [data-overwrite-rollback-audit-20261018.md](data-overwrite-rollback-audit-20261018.md)。本轮按该文 §8 第 3 步执行。
> **状态：进行中。** 第一波（增量对抗评审 4 路）、第二波（新落码增量评审 6 路，其中 4 路因周额度停线提前收尾）已完成。第三、四波等周额度在 2026-10-24 22:00 UTC 重置后再跑，见 §6。

## 0. 前置检查（§8 第 1、2 步）

| 步骤 | 状态（截至 2026-10-19） | 依据 |
|---|---|---|
| 第 1 步：验证已落的修复 | **未完成。** §2.1：协议生成、6 个 Go 服务编译、C++ 增量构建已过；player_locator 测试、`routing_identity_test`、`cross_zone_test` 运行和真实环境故障演练没做；`EnterFenceEcs.LoadCallbackBindsNewestAdmitted` 夹具失败等负责人决定。§2.2：7 步验证一步都没跑。§2.3：login 两个包 249 例通过，没跑真实多 gate 环境 | 第一轮 §7；PROGRESS 2026-10-19 各 Codex 条目 |
| 第 2 步：修剩余 medium | 第一波时**未开始**。第二波时：§2.4 三处已落码（随 `53361d055` 提交，未编译、未测试），第二波按"可能有缺陷"审了；§2.5 仍未动手 | 第一轮 §7；PROGRESS「审计 §2.4 战斗结算三处：丢数据修复」 |
| 字段保护设计 | 第一波时**未落码**。第二波时：第 1 批（台账、生成器、冻结 option）与第 2 批 go/db 部分随 `53361d055` 提交，设计升到 v0.4；C++ 携带的三个头文件（`unknown_field_carry.h`、`player_persist_carry_comp.h`、`player_persist_marshal.h`）还是工作区未跟踪文件，HEAD 上的 `player_database_loader.cpp` 不含携带逻辑。第二波审了已落部分，见 §3.3 | [mixed-version-persist-field-protection.md](mixed-version-persist-field-protection.md) §9；工作区观察 |

由于前置没完成，(a) 增量评审把 §2.1、§2.2、§2.3 的修复都当作"可能有缺陷"来查。

## 1. 结论

**截至第二波：** 新增 3 条 medium、22 条 low，没有 high。3 条 medium 在缺省配置下都不触发：两条在缺省关闭的功能里（全球匹配、合服 copy 模式），一条只在上线后的滚动发布混版窗口出现。前两条主会话抽查了代码路径，都属实；三条的对抗复核都排在第四波。

**第一波：**

- 第一波 4 路增量对抗评审，覆盖 §2.1 修复的 Go 侧和 C++/gate 侧、go/db 世代门禁与补投、data_service 读路由与调试导入。**没有新的 high 或 medium**，新增 **9 条 low**。
- 其中 3 条是第一轮修复本身的缺陷：
  - §2.1 的 R5 在"LeaveGame 先于本次登录的 Bind 到达 gate"时不成立；
  - §2.2 的补投把发布失败也计入次数，Kafka 故障约 21 分钟后意图停在 exhausted，不会自愈；
  - §2.1 的 LeaveScene 把"负载表里查不到持有节点"当作释放证据。
- 第一轮的 2 个重点点名项复核成立、无新缺陷：
  - data_service `LoadPlayerData` 字段与版本同读；
  - data-service#1（debug_import 的 uint64 精度）的修复。
- 600s 兜底强删会退回 §2.1 原问题，这一点设计已登记为有意 fail-open（[location-release-binding.md](location-release-binding.md):173-181），本轮不重报。L2、L3 是让它比设计预期更早、更常触发的两个新入口。

**第二波**（第一波之后又合入 7 个提交，含字段保护、§2.4 修复、路由自动对账，以及从别的分支合入的神秘商人、邮件清扫、组队匹配）：

- 6 路：字段保护、§2.4 战斗结算修复、神秘商人、路由自动对账、合服工具、login。新增 3 条 medium、13 条 low。
- **r2-ps#1（medium，仅混版窗口）**：字段保护的台账在 bootstrap 期间把新增持久字段一律记为世代 1。上线时若没把台账切到 active，之后第一次带新持久列的滚动发布里，旧 go/db 会把新列静默丢掉或读成旧值，C1 / C1b / C2 都拦不住。查找员的两份输出对严重度有分歧（medium / low），待复核裁决。修复只需上线前切 active 并写进发布清单。
- **r2-battle#1（medium，§2.4 修复缺陷）**：全球对局个人结算的落库重试轮会无条件重写全部成员的 pending。已被 scene 应用并销账的成员，pending 会复活，下次登录重复发奖。门控在 `GlobalMatch.Enabled`（缺省 false）。
- **r2-mz#1（medium）**：合服 copy 模式在拷行后删掉 `save_barrier`、保留 owner_epoch，以前登录过的被拷玩家全部无法登录，直到人工跑屏障恢复；合服手册里没有这一步。fail-closed，不丢数据。门控在 `-player-rows-mode copy`（缺省 pin，但多 data Redis 集群部署只能用 copy）。
- §2.4 的另外两处修复（cpp-economy#0 号段预检、cpp-economy#1 区分封禁与冻结）推翻不成，只发现 1 条预检不催续段的 low。
- 字段保护在缺省配置下 go/db 侧实际不生效：产物世代为 1、台账在 bootstrap，C1/C1b/C2 都走不到，C0 要等人工写引导关闭标记。C++ 携带还只在工作区未跟踪文件里；只提交已跟踪文件会断构建，停在 HEAD 时第一轮 low「存盘不保留未知字段」仍成立。
- 神秘商人：购买三步同线程同步完成、同行同次落盘，没有共享库存，冻结闸与退出闸都覆盖，只有 1 条限购可被绕过的 low（付费购买，不复制资产）。
- login：第一轮 §4 的「账号 blob 有两个写者不走建角锁」在当前代码里已不成立，四个写者同锁加令牌 CAS。loginqueue 与 dataloader 因额度未读。

## 2. 方法与范围

第一轮按子系统分区，每区一名查找员。本轮换成四种查法：

| 查法 | 内容 | 状态 |
|---|---|---|
| (a) 增量对抗评审 | 基线 `bf3c60632` 到 HEAD，加工作区里涉及存盘、加载、会话、资产的改动，逐个尝试推翻 | 第一波 4 路、第二波 6 路已完成（第二波有 4 路提前收尾，未读部分见 §6）；C++ 收件箱 / 号段引导 / trade / friend / 邮件清扫 1 路待做 |
| (b) 补覆盖 | 第一轮 §1 的未读段；各业务系统持久组件写入口是否绕过冻结闸或退出闸 | 待做 |
| (c) 复核 | 第一轮 §4 的 23 条未复核 low | 待做 |
| (d) 换视角连续查找 | 按资产追踪、按故障、按入口，连续两轮无新发现才停 | 待做 |

第一波四路的范围：

| 路 | 范围 |
|---|---|
| A1 | go/scene_manager 的 leave_release / leavescenelogic / enter_holder_release / enterscenelogic；go/player_locator 的 markoffline / leasemonitor / session_cas / session_reconciler / gate_session_index |
| A2 | C++ scene 的 release_fence.h、player_lifecycle 改动段、player_scene、scene_manager_response_handler；gate 的 scene_route_helper、scene_entry_dispatch、client_session_handler、client_request_lane、login_admission |
| A3 | go/db 的 saveoutbox（generation / replay / scripts）、kafka（save_outbox_note / route_ready_gate / key_ordered_consumer / task_result_notify）、barrierrestore 与 save_barrier_restore 工具、shared/placement/route_ready；顺带 retry_stranded、durable_enqueue 的主路径 |
| A4（data_service） | data_logic、routing、store/route_reads、route_reconcile、routetool、debug_fetch / debug_import / debugutil |

第二波六路的范围（基线 `8767c5027` 到 HEAD `c63b22dca`，加工作区；合服与 login 从 `bf3c60632` 起）：

| 路 | 范围 | 完成度 |
|---|---|---|
| persist-schema | 字段保护：台账与生成器、go/db 的 persist_schema / player_row_fence / key_ordered_consumer、barrierrestore 的 schema_gen、C++ 携带（工作区未跟踪文件）、debug_import 世代检查、告警 | 提前收尾 |
| battle-settlement | §2.4 三处修复：player_battle、bag_service、result_commit、settlement_outbox、battle_room_manager | 完整 |
| mystery-merchant | 神秘商人：C++ 系统与 handler、proto、scene_manager 的 mystery_merchant.go | 完整 |
| route-reconcile | data_service 路由自动对账 routereconcile（reconciler / cache）、配置接线、shared/placement | 提前收尾 |
| merge-zone | tools/merge_zone（merge_run、preflight、player_rows、unmerge 调用点）、guild merge_fence | 提前收尾，只读完主流程 |
| login | loginsession、sessionfence、账号目录四个写者、kill switch、deprecation | 提前收尾，loginqueue 与 dataloader 未读 |

- **只读约束**：每个代理的提示都写明不建、不改、不删文件，`git show` 不重定向，不跑构建和测试。两波结束后都检查了代理转录：没有 Edit 或 Write 调用，也没有重定向成文件（第二波唯一命中是 `2>&1`）。
- **第二波的两份输出**：周额度触线时，我给 4 路发了"立即收尾"消息。消息是另起一个续跑实例送达的，工作流里原来的实例照样跑完，所以这几路各有一份结构化结果、一份收尾报告，内容互有增减。本文取两份的并集，严重度不一致的地方逐条注明。
- **严重度口径**同第一轮。项目尚未上线，只在混版窗口出现的问题降一级并标注。

## 3. 发现（本审计均未做对抗复核；第一波有 6 条已由修复会话复核并落码，见 §5）

### 3.1 第一波：第一轮修复本身的缺陷

| 编号 | 问题 | 位置 | 触发时序 | 可达性 | 已登记 | 自信度 |
|---|---|---|---|---|---|---|
| r2-gate#1 | **§2.1 的 R5 有缝**：LeaveGame 先于本次登录的 Bind 到达 gate 时，重置记下的是旧代次。之后到达的 Bind(gE) 不被过滤并清掉标记；session_ended 释放把 location 删掉后，gate 仍留着旧路由和 `ownerEpoch=E`。同一连接再次 EnterGame 时，gate 按旧路由把旧 epoch 补发到原节点 X，scene_manager 同时铸造 E+1 并落到 Y | 钩子 `cpp/nodes/gate/handler/rpc/client_session_handler.cpp:1144`；重置 `cpp/nodes/gate/handler/event/scene_route_helper.h:153`；判据 `:176`；补发 `:367`、`scene_entry_dispatch.cpp:359`；login 先 Bind 后复查 `go/login/internal/logic/clientplayerlogin/entergamelogic.go:791`、`:813` | ① EnterGame 已受理，Bind(gE) 还在 Kafka；客户端发 LeaveGame，gate 收包即重置，记下 g0。② 后台链已过 `:813` 复查，EnterScene 照常执行；Bind(gE) 到达，gE>g0，转发到 X 并清标记。③ LeaveScene 派发 session_ended，X 存盘、写 A1′、销毁，location 被删。④ 同一连接 EnterGame(gF)，gate 按旧路由带 E 补发到 X，scene_manager 铸 E+1 落到 Y。⑤ X 若先于铸造完成核对，会建出幽灵实体：gate 改绑到 Y 之前，客户端操作落在 X 上，随后被 CAS 拒掉（毫秒级丢失）；幽灵实体长期挂在 AOI 里。若铸造在前，X 回 3023，登录类型被这次补发消耗，Y 收到 0 型进场 | 默认路由模式可达，无开关。改包客户端可稳定复现；普通客户端需要在加载中点"返回"，并恰好落在 Bind 的 Kafka 消费延迟里 | 未登记；[location-release-binding.md](location-release-binding.md):153-158 声称这种情形已覆盖 | 很可能 |
| r2-db#1 | **§2.2 补投把发布失败也计次**：Lua 先把 attempts+1 再交出意图，Publish 失败不回退。go/db 到 Kafka 的生产持续不可用、且实体已销毁时，约 21 分钟后 attempts 到 8，意图进入 exhausted；Kafka 恢复后没有路径重置计数，玩家登录在父提交屏障处报 not committed，无法产生新版本 | `go/db/internal/saveoutbox/scripts.go:90-92`、`:76-79`；`replay.go:288-293`；MaxAttempts 缺省 8（`replay.go:75-77`）；登录卡点 `go/login/internal/logic/pkg/dataloader/parent_commit_barrier.go:42` | ① Kafka 生产不可用。② 期间场景节点崩溃或排空看门狗销毁实体（`player_lifecycle.cpp:3410-3423`），最后版本只剩 outbox 一份。③ 每拍先计数、后 publish_error，约 30s + 10…600s 退避，合计约 21 分钟到上限。④ 恢复后不再投，人工 HDEL 前该玩家无法登录 | 默认 dual 可达；需要"Kafka 故障 ≥ 约 21 分钟"和"实体已销毁"同时成立。数据不丢（仍在父快照和意图里），但故障后会大面积锁登录 | 部分登记：`db-write-fencing-mysql.md:1628-1631` 和告警 `deploy/k8s/db-task-retry-alerts.yaml:487-501` 写了 exhausted 要人工处理，但归因只写了消费侧 | 很可能 |
| r2-sm#3 | **LeaveScene 把"负载表里查不到持有节点"当作释放证据**：`scene_nodes:zone:{z}:load` 键丢失或被淘汰时，ZCARD=0、ZSCORE 为空、没有 death_at，会判 `owner_dead`。之后每次登出或断线清理都会按 any 模式立即删 location，下次进场铸造 E+1，持有节点未落盘的部分回档 | `go/scene_manager/internal/logic/leavescenelogic.go:314`；`enterscenelogic.go:1048-1068`；`reentry_barrier.go:149-152` | ① load 键丢失。② leader 下一拍 ZADD 补回之前，有玩家登出。③ 判 owner_dead，立即删 location。④ 进场铸造 E+1，持有节点未落盘的进度被拒 | 需要 load 键丢失；leader 每拍 ZADD，窗口约一拍。EnterScene 本来就用同一判据，这次修复把它扩到了每次登出都会走的路径 | 未登记（设计 §3.1 证据 2 只写"同判据"） | 很可能 |

### 3.2 第一波：其他新发现

| 编号 | 问题 | 位置 | 可达性 | 已登记 | 自信度 |
|---|---|---|---|---|---|
| r2-sm#1 | forced 计数和 ERROR 日志在删除 Lua 的结果出来之前就记了。Lua 返回 -1（GET 之后有并发 EnterScene 改写）或 0（已被别的调用删掉）时没有真正删除，却照样计 forced，触发告警 `SceneManagerLeaveReleaseForced` 误报；released 同样多计 | `go/scene_manager/internal/logic/leavescenelogic.go:241-245`、`:260-268`；告警 `deploy/k8s/scene-manager-alerts.yaml:658` | 默认开启（`LeaveSceneHolderReleaseWaitSeconds` 缺省 600）；需要恰好并发，很少见。只影响观测 | 未登记 | 确定 |
| r2-sm#2 | 600s 等待计时用的是各 scene_manager 副本的墙钟。副本 A 写 first_ms，副本 B 时钟快 Δ 或 NTP 跳变，算出的等待时长就多 Δ，提前 forced；forced 只比对 location 原文，若持有节点仍在重试存盘，就按 §2.1 原路径回档 | `go/scene_manager/internal/logic/leave_release.go:178-192`；`leavescenelogic.go:324` | 默认配置可达，但要持有节点已卡住，且副本间有时钟偏差 | 未登记（设计 §3.2、§3.6 没写时钟偏差语义，AGENTS §11.3） | 确定；影响面小 |
| r2-db#2 | legacy 存活暂停只挡之后的补投。已经发到 Kafka 的旧补投副本 V1，在 dual→legacy 降级后会被 legacy 无条件写入，盖掉 V2 | `go/db/internal/saveoutbox/replay.go:173-182`；`generation.go:69-95`；§8.6 降级步骤 `db-write-fencing-mysql.md:1012-1014` 没要求积压清零 | **仅混版或降级窗口**；缺省 dual（`db.yaml:177`、`config.go:132`） | 未登记；是第一轮 go-db-pipeline#2 的新变种 | 很可能，窗口窄 |
| r2-db#3 | 在途重试戳只取较大值，且用本机时钟。某个 go/db 实例时钟超前 Δ 时，补投会推迟约 Δ+2 分钟且不计次，期间玩家被锁登录；C++ 写新版本时不清这个戳 | `go/db/internal/saveoutbox/scripts.go:134-143`、`:86-89`；`go/db/internal/kafka/save_outbox_note.go:73` | 默认 dual；需要时钟故障加原件丢失 | 未登记 | 代码路径确定，影响取决于偏差幅度 |
| r2-ds#1 | debug_import bundle 模式"目标≠导出来源"的身份闸对 TiDB 等于没有：取不到 `@@server_uuid` 就返回空串，空的一侧直接跳过比较；地址闸只认回环地址。把线上 TiDB port-forward 到 127.0.0.1 并混用配置时，`--dry-run=false` 会把线上两张玩家行 REPLACE 成导出时的旧状态，并把行内围栏抬高，之后在线 scene 的存盘全被判 stale_owner | `go/data_service/cmd/debugutil/connect.go:101-113`、`:153-171`；`cmd/debug_import/bundle_import.go:451-465`、`:381-390`；`go/db/internal/kafka/player_row_fence.go:160-161` | 运维误用才触发；`-dry-run` 缺省 true（`main.go:63`），还需混配 | 未登记；`docs/ops/online-debug-data-fetch.md:57`、`:88` 写"按 server_uuid 拒绝"，没提 TiDB 例外 | 存疑（TiDB 对该变量的实际行为未核实） |
| r2-ds#2 | bundle 导入重铸 epoch 只取 max(线上, 本地 owner)+1，不看本地两张表的行内围栏。本地 Redis 被 FLUSHALL 而 MySQL 和 Kafka 没清时，epoch 更大的残留 DBTask 会冲掉导入的数据，之后本地新登录的存盘被判 stale_owner | `go/data_service/cmd/debug_import/bundle_import.go:381`、`:487-493`（文件头 `:39-41` 的说法在此情形不成立）；`player_row_fence.go:157-173` | 只影响本地调试环境；需要 `-dry-run=false`、`-replace` 和"清了 Redis 没清 MySQL/Kafka" | 未登记 | 很可能 |

**附带观察（非数据问题）**：客户端可以直接发 Disconnect（消息号 58），它在 `IsClientMessageId` 白名单里，和 `cpp/nodes/gate/handler/rpc/login_admission.h` 注释里"gate 合成"的说法不符。影响只限本人掉线，§2.1 修复之后不会回档。

### 3.3 第二波：medium

| 编号 | 问题 | 位置 | 触发时序 | 可达性 | 已登记 | 自信度 |
|---|---|---|---|---|---|---|
| r2-battle#1 | **§2.4 social-match-battle#2 修复缺陷：落库重试轮无条件重写全部成员的 pending。** 一轮里只要有一名成员没拿到确认，下一轮就对全部成员再 SET 一遍；已被 scene 应用并销账的成员，pending 复活，下次登录重复发奖 | 每轮遍历全部成员、不记成员级状态：`cpp/nodes/battle/logic/battle_room_manager.cpp:1475-1503`（成功判定 `:1482`，未发出按失败计 `:1501`）；整轮全成功才置 staged：`cpp/libs/services/battle/system/result_commit.h:118-126`；无条件 SET：`cpp/libs/services/battle/settlement/settlement_outbox.h:429-432`；销账删 pending 与锁：`settlement_outbox.h:461-468`，回调摘账本：`cpp/libs/services/scene/battle/system/player_battle.cpp:329-330`；离线暂存锁不在就不动：`player_battle.cpp:528-538`；登录钩子只看账本不看锁：`:2207`→`:2077`→`:1727` | ① 全球确认模式对局收尾，第 1 轮落库给 A、B 各 EVAL。② A 写成功；B 回 ERROR、空应答（连接中途断开）或命令未发出，整轮判失败，退避 1~30s。③ 退避期间 A 的 scene 读到 pending（reaper 在 deadline 后补读、A 重登的登录钩子或 RunLoginRechecks），应用、登记账本、落盘，销账 Lua 删 pending 与锁，账本条目被摘。④ 第 2 轮对全部成员无条件 SET，A 的 pending 复活，此时锁已不在、账本里也没有这一局。⑤ 结果发出并回执，派发给 A：A 离线或退出中时，离线暂存因锁不在什么都不做，pending 留下；或第 2 轮之后、回执之前节点停机，pending 同样留下。⑥ A 下次登录，登录钩子读到 pending、账本无记录，整笔再发一次（金币、掉落、任务进度翻倍）。A 在线时派发路径会丢弃并条件删除，能自愈 | 门控：只有确认模式（CreateBattle 带 `placement_operation_id`，`battle_room_manager.cpp:655`）会触发，该字段只由全球编队填写（`go/match/internal/logic/gather.go:338-341`），`GlobalMatch.Enabled` 缺省 false（`go/match/etc/match_service.yaml:6`）。开通后需"一轮部分失败"加"该成员在退避窗口内被应用并销账"；deadline 强制平局时 reaper 补读与重试大致同时，更容易撞上。加重变种：积压索引键类型被写坏时每轮都部分执行又判失败，落库无次数上限（r2-battle#4），可反复刷奖 | 未登记。第一轮 §2.4「修复」、`docs/design/cross-zone-matchmaking.md` §13.6 更正（:556-576）、`result_commit.h:64-67` 只论证了"回执后不改写" | 很可能。主会话抽查：落库循环与无条件 SET 属实；scene 侧离线暂存早已因同类缺陷改成"锁匹配才写"（`settlement_outbox.h:434-443` 的注释），battle 侧落库没有跟上 |
| r2-mz#1 | **合服 copy 模式删掉提交屏障、保留 owner_epoch，被拷的老玩家整批无法登录，且恢复工具拒收这批人** | 删键清单含 `player:{pid}:save_barrier`：`tools/merge_zone/player_rows.go:702-705`、`:725`；调用点：`tools/merge_zone/merge_run.go:493`（copy 合服）、`tools/merge_zone/unmerge.go:289`（copy 撤销）；owner_epoch 刻意不删：`tools/merge_zone/scene_hot_state.go:50`；登录卡点：`go/login/internal/logic/pkg/dataloader/parent_commit_barrier.go:110-111`（及 `:153-155`）；恢复工具拒收无落点记录者：`go/db/internal/barrierrestore/restore.go:430-438` | ① 以 `-player-rows-mode copy` 合服，P2~P6 通过。② 步骤 1 按列名逐字节拷两表行（带行内围栏），随后删父缓存、子缓存、资产证据与 `save_barrier`，owner_epoch 保留。③ 按 runbook 直接开服，全程没有重建屏障的步骤。④ 曾进过场景（owner_epoch≠0）的被拷玩家登录，`beginParentLoad` 报"历史玩家父快照与提交屏障缺失，需要停写恢复"。⑤ pending 只在 scene 存盘时写，进不了场就不会自愈。⑥ 按设计跑 `save_barrier_restore`：v1 要求显式落点记录，copy 合服者没有，被拒。copy 撤销对全部清单玩家同样删屏障 | 门控：`-player-rows-mode copy`。缺省 pin（不调 `invalidatePlayerCaches`），但 runbook §5.1 要求多 data Redis 集群部署只能用 copy。一用即必现，与故障无关。fail-closed，不丢资产 | 部分登记：`db-write-fencing-mysql.md:1562`、`shared-redis-split-by-purpose.md:136` 写了"合服 / 撤销必须删屏障、之后重建才能放流、当前无自动补种"；但合服 runbook 全文没有屏障重建步骤（grep 无结果），恢复工具拒收这批人也未登记 | 很可能。主会话抽查：删键清单、调用点、登录报错分支、runbook 无相关步骤均属实 |
| r2-ps#1 | **字段保护台账在 bootstrap 期把新增持久字段一律记为世代 1。** 上线后若台账仍是 bootstrap，滚动发布时旧 go/db 会把新列静默丢掉或读成旧值，C1 / C1b / C2 只比世代号，拦不住 | `tools/proto_generator/protogen/internal/persistschema/apply.go:43-46`；`proto/persist_schema_gen.txt:7`（state=bootstrap）、`:212`（mystery_merchant 已落在世代 1）；`go/db/internal/kafka/persist_schema.go:173`（C1）、`:186`（C1b）、`:195`（C2）；`key_ordered_consumer.go:515-519`；`player_row_fence.go:188`；`deploy/k8s/README.md:2396`（"最后由负责人改为 active"） | ① 版本 R2 新增顶层持久列 F（例如限购或幂等账本），生成器仍记世代 1。② 滚动发布，R1 go/db（不认识 F）与 R2 并存。③ R2 scene 存盘戳为 1，落在 R1 go/db：C1 不拦，`tx.Save` 只写认识的列，F 停在旧值，照常 ACK。④ 或冷加载落到 R1：只 SELECT 认识的列，C2 不拦，父快照缺 F。⑤ scene 加载后再存盘，F 被写成零值或旧值。F 若是限购或幂等账本，就会重复发放 | 仅混版窗口。项目未上线，首次上线整批起步不触发；上线后第一次带新持久列的滚动发布即可触发，前提是台账没切 active。没有任何机器门禁把台账状态与上线绑定，`docs/ops/release-checklist.md` 没有这一步 | 部分登记：`mixed-version-persist-field-protection.md:402`（§8.1 第 2 步）、`:606`（v0.4）写了发号规则；没写"同为世代 1 的两个二进制字段集合可以不同"，也没写上线前要切 active | 很可能。严重度两份输出不一致：结构化结果按"默认滚动发布可达的回档或丢资产，混版降一级"定 medium；收尾报告定 low |

### 3.4 第二波：low

| 编号 | 问题 | 位置 | 可达性 | 已登记 | 自信度 |
|---|---|---|---|---|---|
| r2-battle#2 | §2.4 修复缺陷：落库进行到一半时停机或崩溃，同一局只有部分成员的 pending 落地，结果永不发出。A 拿到奖励，B 的消耗与 HP 不结算，评分与帮会历练都不计 | `cpp/nodes/battle/logic/battle_room_manager.cpp:1707`（AbortAllRooms 先停落库再清房）、`:1475`；`result_commit.h:127`（未 staged 不发）；`docs/design/cross-zone-matchmaking.md:569` | `GlobalMatch.Enabled` 门控；需一轮部分失败后在退避窗口内停机或崩溃 | 部分登记：:569 接受"奖励有、评分无"，没写同局成员不对称 | 确定 |
| r2-battle#3 | §2.4 cpp-economy#0 修复缺陷：号段预检只读 `Available()`，不触发续段。"所需新实例数 > 当前剩余 > 预取阈值、且没有下一段"时，结算一直延后，最终可能被下一局覆盖单槽 pending | `cpp/libs/services/scene/battle/system/player_battle.cpp:774`、`:1764`；`cpp/libs/modules/id_segment/guid_segment_client.cpp:155`、`:259`；`guid_segment_client.h:88`；`bin/etc/base_deploy_config.yaml:213` | 缺省 item 号段已启用；step 缩到下限 1000 时阈值为 100。现有掉落表每只怪最多 2 个掉落槽，几乎不可达，取决于配表。资产通道 `ApplyCredit` 预检同根因（`asset_op_system.cpp:675-693`，AssetOp 门控） | 未登记 | 很可能 |
| r2-battle#4 | 个人结算落库轮次没有上限：持续 ERROR（OOM、NOAUTH/ACL、键类型错）时房间永久滞留、结果永不发出、节点无法排空，并放大 r2-battle#1 | `result_commit.h:118-126`（失败只用于算退避）；`battle_room_manager.cpp:1420-1424`、`:1453-1455` | `GlobalMatch.Enabled` 门控 | 未登记（§13.6 只写"Redis 不可用时等恢复"）；违反 AGENTS §11.3 重试须有上限 | 确定（仅收尾报告） |
| r2-ps#2 | 字段保护修复缺陷：欠款守卫遇到"已有欠款带未知字段"返回 `kFeatureUnavailable`，收件箱执行器把非 kSuccess 一律当终态 REJECTED，GM 追缴永久丢失（守卫注释说"由新版节点来挂"，实际不会再投）。变种：将来 `reserved` 退役欠款字段后，残留字节永远是未知字段，同币种每次挂欠款都被拒 | `cpp/libs/modules/currency/system/currency_system.cpp:357-367`；`cpp/libs/services/scene/player/system/gm_sanction_inbox_executor.cpp:31-34`、`:124`；`player_inbox_system.cpp:880-883`；`go/player_inbox/internal/inbox/decide.go:68-69`；物品同源 `cpp/libs/modules/bag/item_store.cpp:325-328` | 收件箱消费缺省关（`bin/etc/game_config.yaml:35`）；变种 A 仅混版，变种 B 要将来退役字段。开收件箱或开始退役字段后应复评为 medium。代码是工作区未提交的字段保护第 3a 批半成品 | 未登记（设计 §4.4 `:243` 只写"GM 操作返回错误"；`:133` 称退役残留"无害"，与守卫判据矛盾） | 很可能 |
| r2-ps#3 | 引导关闭标记的"只关不开"只在单个进程内成立：标记丢失（备份恢复、主从切换、误删）后重启的 go/db 读到 Nil 判为开放，C0 失效，各 Pod 状态不一致且无告警 | `go/db/internal/kafka/persist_schema.go:48-50`、`:59-69`、`:74-85`、`:108-121`；`go/db/db.go:194-205`；`deploy/k8s/README.md:2396`；`deploy/k8s/persist-schema-alerts.yaml:95-105` | 仅混版窗口，需丢键加重启。世代 ≥2 后 C1b 能兜住 | 未登记；注释 `persist_schema.go:49` 与 README:2396"误删也不放宽"与重启后的行为相反 | 确定 |
| r2-ps#4 | C0 / C1b 进死信后，文档给出的恢复路径都走不通：重放必然再被拒；`save_barrier_restore` 因 `outbox_*` 字段或 zone 待补投索引残留而拒绝；单表死信时 `readPair` 拒绝。玩家被父提交屏障永久卡在登录，只能手改 Redis 与 SQL | `go/db/internal/kafka/key_ordered_consumer.go:2324-2333`；`persist_schema.go:169-171`；`go/db/internal/saveoutbox/scripts.go:63-67`；`go/db/internal/barrierrestore/restore.go:329-331`、`scripts.go:35-44`、`:52-56`；`parent_commit_barrier.go:42`；`deploy/k8s/README.md:2399-2402` | 需 C0 或 C1b 真的触发（误部署，或关闭引导期时仍残留未盖戳意图）；缺省未关闭引导期、世代为 1，不触发 | 部分登记：设计 `:346`、`:370`（L-G）给出的处置是重放或恢复工具，没写二者都不可用；同根因见第一轮 §4"DBTask 被永久拒绝后被登录预检锁死" | 很可能（仅结构化结果） |
| r2-ps#5 | 合堆守卫只改了一部分：`IsIndependentInstance` 已用于 3 处，但入包规划与装备预检仍只看 `has_equip`，头注释却声称三处共用。带未知字段的非装备实例入包时规划与提交分叉，Debug 下 `assert(false)`，Release 返回 `kInvalidTableData`，批次零改动 | 已改：`cpp/libs/modules/bag/item_store.cpp:182`、`:322`、`:416`；注释 `item_store.h:159-162`；未改：`bag_system.cpp:647-648`、`cpp/libs/services/scene/player/system/player_equip.cpp:442` | 仅混版窗口，且需有调用方把带实例数据的 ItemComp 交给 `AddItems`（未找到）。不丢资产 | 未登记 | 存疑（仅收尾报告；工作区半成品） |
| r2-mz#2 | runbook 称 copy 合服后源区库是"冷副本、可按备份口径归档"，但落点记录指向源区的玩家不拷行，源区库仍是他们的真源。照 runbook 归档后，这批人存盘无限延后、登录读失败 | `docs/ops/merge-zone-runbook.md:335`、`:1097-1098`；`tools/merge_zone/merge_run.go:472-476`；`placement_ops.go:137-140`；`docs/design/player-storage-placement.md:328`、`:332` | copy 模式加"源区玩家已有指向 src 的落点记录"（PinOnCreate 缺省 false，但设计终态是全部钉完）加运维下线源区库。人为动作，最高 low | 未登记（runbook §5.1 与设计 §10.2 矛盾） | 很可能（仅结构化结果） |
| r2-rr#1 | 路由对账 leader 内存里的未决行在换届时丢失：cvm 已越过这些行，新 leader 标记可信时不预热，增量扫不到，缓存一直缺键或错值，读方仍信任就绪标记，带非 home 落点的玩家会写错库 | `go/data_service/internal/routereconcile/reconciler.go:736-738`、`:1309-1321`、`:1224-1225`、`:1700-1706`、`:1797-1817`、`:984-986`；`cache.go:436-438` | 门控：`RouteStore.Mode=db_first`（缺省 redis，`data_service.yaml:185`）、`ArmReadyMarker=true`、读方开关打开，再加 noeviction 拒写与换届同时出现 | 未登记 | 代码路径确定，受害面推断（仅收尾报告） |
| r2-rr#2 | `ArmReadyMarker=false` 下 mapping Redis 身份变化后，追平起点只取 cvm−60s，不看随旧备份恢复的水位键；从旧备份恢复或提升落后副本时，中间这段变动的映射永不补回 | `reconciler.go:1679-1692`（对照 Arm=true 路径 `:1709` 用 min(标记水位, cvm)）；`store/route_reconcile.go:419` | 门控 `db_first` 且 Arm=false；需从旧备份恢复。P1 冻结窗口内无合服搬库，以缺键 fail-closed 为主 | 部分登记（文件头 `:42-43`） | 很可能 |
| r2-rr#3 | Arm=false 时，Redis 整体丢失后的全量追平一旦中断，就永久退化为只回看 60s：leader 同一拍用 SET NX 重建了水位键，抹掉了"水位键缺席即全量"这个唯一信号，cvm 也没归零；之后的追平只补最近一分钟，却照样记 catch_up completed 并置 VI | `reconciler.go:1188`、`:1528`、`:1571-1597`、`:1679-1692`、`:1409-1431`、`:709-719`、`:1293-1321`、`:616-627`；`cache.go:298-303`；`go/db/internal/kafka/placement_route.go:78`、`:184` | 门控 `db_first` 且 Arm=false；需 mapping Redis 整体丢数据，且分钟级全量追平被打断（换届、单批出错、令牌写失败）。Arm=true 不受影响 | 未登记（提案 `player-route-mapping-durability-proposal.md:302` 要求"预热完成再起 go/db"，退化后完成日志与全量一样，会误导运维） | 确定（仅结构化结果） |
| r2-rr#4 | 合走标记镜像只增、写入前不复读库：unmerge 先删库行、再按值 CAD 镜像，对账 leader 在两步之间读到旧行并 SET NX，会把刚删的 `merge:merged_into:{src}` 永久写回；之后回档 preflight 被拒，`reconcile-from-cache` 还会把过期标记写回库 | `reconciler.go:1059-1085`；`cache.go:488-551`；`tools/merge_zone/merged_into.go:251-287`；`go/data_service/internal/routetool/repair.go:30`、`:323`；`tools/scripts/k8s_zone_rollback.ps1:183-184` | 门控 `db_first` 且执行 unmerge；毫秒级窗口，命中即永久。fail-closed，不直接丢资产 | 未登记（围栏镜像删前会复读库 `reconciler.go:1968`，合走标记镜像没有） | 很可能（仅结构化结果） |
| r2-mm#1 | 神秘商人限购记录只保留当前一轮。运维调大出现间隔且新停留期盖过旧轮结束时，scene_manager 强制覆盖写新轮，已缓存旧轮的节点继续卖旧轮、新起节点采纳新轮，两轮同时在售；玩家两线往返，每次换轮整份清空记录，限购反复清零 | `cpp/libs/services/scene/player/system/mystery_merchant_rules.h:119-133`、`:143-149`、`:178-182`；`player_mystery_merchant.cpp:265-273`、`:581-585`、`:650`；`go/scene_manager/internal/logic/mystery_merchant.go:531-535`、`:542`、`:570`；`proto/common/component/player_mystery_merchant_comp.proto:14-15` | 缺省不触发（Override 缺省 0，`go/scene_manager/internal/config/config.go:455-456`）；需运维调大间隔加窗口内新起 scene 节点。付费购买，不复制资产 | 未登记（`docs/design/mystery-merchant.md:95-101`、`:165-166`；`mystery_merchant.go:291` 与 comp proto:14-15 断言不串轮） | 很可能 |

### 3.5 第二波附带观察

- **字段保护在缺省配置下 go/db 侧实际不生效**：两份生成产物都是世代 1（`go/shared/generated/persistschema/persistschema.go:9`、`cpp/generated/persist_schema/persist_schema_gen.h:12`），C1 / C1b / C2 走不到；C0 要等人工写引导关闭标记；C3 / C4 未落码，login 没有 `schema_gen` 的读者。
- **C++ 携带只在工作区**：`unknown_field_carry.h`、`player_persist_carry_comp.h`、`player_persist_marshal.h` 是未跟踪文件。只提交已跟踪文件会断构建；停在 HEAD 时第一轮 low「存盘不保留未知字段」仍成立。
- **路由对账 catch_up 收尾不查未决就置 VI**（`reconciler.go:742-744`），偏离规格 must-1（`spec_D1_batch1b2_2_3.md:94`）。
- **login 待查疑点**：`go/data_service/cmd/debugutil/bundle.go:399` 把 `account:{account}` 列为 bundle 条目，debug_import 可能不拿账号锁就整块写回账号目录，盖掉并发建出的角色。运维工具、缺省 dry-run，最高 low，建议与 r2-ds#1、r2-ds#2 一起查。
- **神秘商人字段号文档与代码不一致**：设计 §2、§9.1 写 `player_database.mystery_merchant = 21`，代码是 22（`proto/common/database/mysql_database_table.proto:184`）。文档问题。

## 4. 已排除（尝试推翻成功）

**第二波：**

- **§2.4 cpp-economy#0 预检到铸号之间被别的系统耗号**：从 `HasItemGuidsForDrops`（`player_battle.cpp:1764`）到 `AddItems`（`:877`）在 scene 单逻辑线程上同步执行，期间只有 AddCurrency（用 txlog 号段）、HP 与宠物回写、扣药，都不铸 item 号；收件箱、资产通道、商店、神秘商人插不进来。
- **`MaxNewInstancesFor` 上界不够**：主背包放不下时在 PlanEviction 就拒，早于铸号（`bag_system.cpp:665-689`）；不可堆叠按件计，多 config 求和，同 config 多行在 uint32 上饱和。
- **cpp-economy#1**：封禁判断在动账之前（`currency_system.cpp:135-150`）；解封后不补发是设计取舍。冻结期间 `PrepareBattle` 拒绝带 `PlayerFrozenComp` 的玩家（`player_battle.cpp:1187`），开不了下一局，不会覆盖 pending。
- **正常路径重复派发**：发件箱去重键 `{battle_id, player_id}`（`battle_room_manager.cpp:2190`），scene 侧账本判重，回执后 `DispatchStagedSettlement` 不再写 pending（`:2119-2125`）。
- **结算部分应用后崩溃重投**：账本与货币、背包、任务、宠物在同一次 Marshal（`player_database_loader.cpp:252-274`），同生共死。
- **神秘商人购买三步原子性与共享库存**：发物、扣钱、记名额同线程同步完成，三者同在 player_database 一行同次落盘；scene_manager 只写 Redis 读模型，没有跨服务扣减，不存在超卖或"扣了钱库存失败"。冻结闸（`IsCrossZoneFrozen`）与退出闸（`scene_handler.cpp:273`）都覆盖；时钟回拨与 Redis 键丢失都不会重置限购。
- **合服时玩家在线或有在途 DBTask**：P2~P6 在围栏之下检查场景节点、Kafka LAG、retry / processing / dead 队列、`player:save_pending` 64 桶、`lock:player`、`player:session`，任一不过即拒（`merge_run.go:279-292`、`preflight.go:832-924`）。先拷数据后改映射的顺序成立；copy 拷行对目标库已有行拒绝写入，撤销只删与源库逐字节相同的行。
- **login 账号目录写者不一致**：建角、入场自愈、外观修复、运维摘孤儿四个写者都先拿 `account_lock:create:{account}`，提交点用 Lua 核对令牌（`createplayerlogic.go:122`、`account_self_heal.go:43`、`player_class_backfill.go:114`、`admin/remove_players_from_accounts.go:100`）。**第一轮 §4「账号 blob 有两个写者不走建角锁」在当前代码里已不成立**，第三波 low 复核时据此关闭。
- **login kill switch 先产生副作用再拒绝**：拒绝点在 `resolveAccount`、账号锁和全部 Redis 写之前（`loginlogic.go:69-76`）。
- **login_session 新键跨账号继承**：带代次的连接只读 `{gate_instance_id}:{incarnation}` 键，不回落旧键（`loginsession.go:169-174`）；旧版 gate 兼容键属已登记的混版残余。
- **路由对账用旧映射盖新映射、双 leader**：leader 只写 Redis 副本，从不写库映射；发布按 rev set-if-newer（`shared/placement/route.go:350-359`）；库侧写全部带租约谓词。只按库补 Redis，不从 Redis 补库。
- **字段保护的 C1 延后造成旧盖新**：dual 下行内围栏判 writeOlder；C1b 不误拒合法存盘（两表盖同一戳、只升不降）；两表同 Kafka key 同分区；merge_zone 用 INSERT…SELECT 并校验两库列集合相等。`save_barrier_restore` 接受 `schema_gen` 后仍不会恢复旧屏障。

**第一波：**

- **data_service `LoadPlayerData` 同读**：指定字段时版本和字段在同一条 MGET 里读（`data_logic.go:144-166`）；已建索引时全量读在一段 Lua 里（`:186-212`）；SCAN 回退时版本变了就重读（`:362-366`）；一次 RPC 只解析一次 client。默认部署配了 DevRedis，`ClientForPlayer` 第一行直接返回（`router.go:495-498`），路由切换类问题今天走不到。
- **data-service#1 修复**：uint64 字段按整型解析，行值走 `json.Number`，`-player` 换号时一并改写匹配列，bundle 模式禁止 `-player`。修得对。
- **route_reconcile 的 leader 原语**（AdvanceReconcile / FreezeByLeader / ScanRoutesUpdatedSince）在仓库里没有调用方；unfreeze 与 leader 再冻结的竞态方向是 fail-closed，且未接线。freeze / unfreeze / rewarm 都是单步提交或原子 DEL，中途取消最多留下"冻结"。
- **go/db 世代门禁**：dual 只写标记、运行中不复查，Redis 丢键、首启、新建 zone 都不会让 dual 拒启或停机；legacy 拒启是有意的 fail-closed。
- **补投旧快照**：dual 行内围栏判 writeOlder 后只确认，不发布缓存、证据和 ACK；ACK 只升不降（`player_cache_publish.go:46`）。补投与在线新存盘并发时新版本赢。
- **终态误标**：MarkTerminal 要求 pending、outbox_version、任务版本三者相等，合法的当前版本标不上终态。
- **route_ready_gate**：缺省 `RequireRouteReady: false`；关闸时不取新消息，位点只按连续前缀提交。
- **barrierrestore**：只从 SQL 围栏取版本；Lua 原子校验父子缓存、证据、handoff 不存在、64 桶全空、屏障缺失或三字段相等、owner 未变且无 TTL；缺省 plan 只读，apply 要计划 SHA。不会用旧值盖新屏障。
- **task_result_notify**：只用于读回执的通知，写入是否提交只看屏障 ACK。
- **session_reconciler**：只改会话（整字节 CAS 转 DISCONNECTING，`session_reconciler.go:705`）和 gate 索引（字节守卫，SCARD==0 才 DEL），不碰 location、owner_epoch、handoff。
- **"持有者永不释放、只能等 600s"**：旧会话的清理与更新的会话不能并存（`session_cas.go:39-62`、`markofflinelogic.go:166-170`），不可达；代次 0 只在混版窗口。
- **C++ 收到释放时实体在加载、交接或退出中**：加载中和改派待确认判 `kSkipLocalHolder`（`player_lifecycle.cpp:5408`），交接中判 `kSkipHandoffInFlight`，退出中只合并原因；A1′ 只在落地后写，与 DestroyPlayer 在同一个函数里。
- **CAS 口径**：存盘 Lua 用 `owner ~= ARGV[2]` 严格相等（`redis_client.h:153`），与 `release_fence::Decide` 一致。
- **EnterScene / CreateScene 应答**：改成每次调用按值捕获 playerId 和关联号，回显值逐字段核对，实体重建后应答会被丢弃。
- **登录准入**：`loginSucceeded` 按会话代次核对，断线重连是新会话，没找到绕过方式。
- **dev yaml 的 `AllowUnsafeCrossNodeHandoff: true`**（`scene_manager_service.yaml:62`）会关掉整个修复；K8s ConfigMap 模板不写这个键，取代码缺省 false；已登记（事故报告 §7.2）。

## 5. 修复方向

| 编号 | 修复方向 |
|---|---|
| r2-gate#1 | LeaveGame 回包成功时再执行一次 `ResetSceneRoute`，`loginGenerationAtReset` 取回包携带的 login 侧当前代次；或在 lane 里有在途或排队的 EnterGame 时延后钩子或拒绝这次 LeaveGame。**已修（2026-10-19，Claude，复核成立，未编译，待 Codex）**：两种建议都没用。回包是客户端可见的 `LoginEmptyResponse`，带代次就要改客户端契约；只看 lane 盖不住 login 后台链，因为 EnterGame 的回包早于 Bind 发出。改为只改 gate：login 只在处理 EnterGame 时发 Bind，所以重置之后、本连接下一次 EnterGame 收包之前到达的 Bind / 路由一律丢弃（Bind 入口新增过滤）。之后按被离开的那条链认旧链：角色加代次，代次随丢弃抬高，另加「比会话已见过的更新登录旧」一条。残余：旧链的 Bind 与路由都晚于下一次 EnterGame 收包、又早于新登录任何事件时认不出。回归用例 `cpp/tests/routing_identity_test/routing_identity_test.cpp` `SceneRouteEntry.OldChainBindArrivingAfterLeaveGameIsDroppedAndNeverReplayed` 等 5 条；说明见 [location-release-binding.md](location-release-binding.md) §3.5「旧链的 Bind 晚于 LeaveGame 到达」与 §14。Java 版：不适用，结构上不可达（2026-10-19 只读核对 Java HEAD `0e4a613`，未改代码）。Java 的绑定只随同一会话 EnterGame 的 login 同步应答下发，gate 按会话串行调 login，LeaveGame 排在 EnterGame 应答之后，没有异步 Bind 或路由事件；超时的迟到应答由 Dubbo 丢弃。迟到的 scene 帧按 (节点, 链路代次, 玩家, epoch) 被丢弃，加载中离开会取消进场。Java 没有待做项，`PARITY.md` 不登记；推导见 [location-release-binding.md](location-release-binding.md) §14.4 |
| r2-db#1 | broker 确认之后才计数（失败回退计数），或发布失败时整拍暂停认领。**已修（2026-10-19，Claude，复核成立，未编译，待 Codex）**：两种都做。认领不再计次，两条任务都经 broker 确认后才由 `confirmAttemptScript` 按版本加计数 CAS 计一次；发布失败不计，并让补投器暂停 `RetryAfter`；意图本身不合法仍计次。回归用例 `go/db/internal/saveoutbox/replay_r2_regression_test.go`；说明见 [db-write-fencing-mysql.md](db-write-fencing-mysql.md) 文末「2026-10-19 第二轮审计修复」。Java 版不适用：不经 Kafka / go/db 落库，没有补投器 |
| r2-sm#3 | LeaveScene 只认带 etcd 正面死亡证据的 `playerLocationOwnerDead`。**已修（2026-10-19，Claude，复核成立，未编译，待 Codex）**：照此修。`judgeHolderRelease` 只用 `playerLocationOwnerDead`，真正下线的 zone 在 etcd 里已注销，照样立刻删；拿不准的落到 600s 上限。EnterScene 陈旧位置判定里的 `playerLocationOwnerGone` 不在本批，登记备查；**同日另批复核成立并已修（未编译、未测试，待 Codex）**：改为先要 etcd 注册表里该 zone 已无任何节点的正面证据，合服后源区照常放行，见 [location-release-binding.md](location-release-binding.md) §14.5。回归用例 `go/scene_manager/internal/logic/leave_release_test.go` `TestLeaveScene_MissingLoadSetIsNotOwnerDead`、`TestLeaveScene_OwnerGoneFromRegistryDeletes`；说明见 [location-release-binding.md](location-release-binding.md) §3.1、§14。Java 版不适用：没有 scene_manager LeaveScene 释放判定 |
| r2-sm#1 | forced / released 指标与 ERROR 只在 `outcome==deleted` 之后记。**已修（2026-10-19，Claude，复核成立，未编译，待 Codex）**：照此修。返回 0 / -1 时只打 INFO，不计判定、不打 forced ERROR；等待类照旧每次回拒都计。回归用例 `TestLeaveScene_ReleaseVerdictCountedOnlyWhenDeleted`（forced→superseded、released→already_gone）；说明见 [location-release-binding.md](location-release-binding.md) §3.2、§14。Java 版不适用 |
| r2-sm#2 | 等待计时改用 Redis 侧时间，例如带过期的计时键。**已修（2026-10-19，Claude，复核成立，未编译，待 Codex）**：读值、比 scope、登记和求差合进一段 Lua（`luaNoteLeaveReleaseWait`），时钟取 Redis `TIME`，不再读任何副本的墙钟；键名、值格式与 TTL 不变。Redis 时钟回拨时钳 0，只会多等。回归用例 `TestLeaveScene_WaitTimerUsesRedisClockNotReplicaClock`、`TestNoteLeaveReleaseWait_UsesRedisClock`；说明见 [location-release-binding.md](location-release-binding.md) §3.6「时钟语义」、§14。Java 版不适用 |
| r2-db#2 | 降级步骤加"该 zone 的 topic 积压为 0"；或 legacy 收到带 (owner_epoch, save_seq) 的两表写时按冲突停机。**延后（2026-10-19）**：只在 dual→legacy 降级或混跑窗口出现。按 [db-write-fencing-mysql.md](db-write-fencing-mysql.md) 文末「上线状态」，项目尚未上线，没有可降级或混跑的旧部署，上线前不阻塞，本批不改代码。上线后若要降级，先按本行补 §8.6 的「topic 积压为 0」步骤。Java 版不适用 |
| r2-db#3 | 戳超过 now+窗口视为非法并截断；或改用 Redis TIME 打戳。**已修（2026-10-19，Claude，复核成立，未编译，待 Codex）**：取前者。在途戳改为最后一次写入为准，认领时超过 now+`InflightWindow` 或格式不对的戳截掉，单个未来戳的推迟上限为两个窗口。没用 Redis TIME，理由是调度分数用 go/db 时钟，换钟只会把偏差转移到 Redis 与 go/db 之间。「C++ 写新版本不清戳」维持现状：戳按玩家表示有重试链在途，截断后影响有上界。回归用例与说明同 r2-db#1。Java 版不适用 |
| r2-ds#1 | 任一侧身份取不到时 fail-closed，除非显式加 flag；TiDB 改用可取到的集群身份，或要求目标库带本地环境标记 |
| r2-ds#2 | E 取 max(线上, 本地 owner, 本地两行围栏, 本地屏障令牌)+1；或本地行围栏大于 owner 时拒绝导入 |
| r2-battle#1 | ResultCommit 按成员记落库结果，后续轮次只重写没确认的成员；落库脚本改成与 `kStorePendingSettlementIfLockMatchScript` 同一纪律，`battle:lock:{player}==battle_id` 才写，锁不在或属于别的局按"该成员已收尾"计成功（销账与删锁在同一条 Lua 里，销账之后的重写必然看到锁不在）。补单测：第 1 轮部分失败、期间对 A 销账，第 2 轮之后 A 的 pending 必须不存在。全球匹配开通前必修 |
| r2-mz#1 | 前向 copy 合服不删 `save_barrier`（两表逐字节拷贝、带行内围栏，原 pending / ACK 对 dst 行仍成立），只删父缓存、子缓存与资产证据；撤销保留删除，但 runbook §10.1 补"保持维护闸 → `save_barrier_restore` 重建 → 再开服"，并让恢复工具接受"无落点记录且 home==zone"的玩家；`-verify-merged` 断言 owner≠0 的清单玩家屏障存在且与两表 ACK 同版。首次 copy 合服前必修 |
| r2-ps#1 | 首次上线前把 `proto/persist_schema_gen.txt` 切到 `state=active`，写进 `docs/ops/release-checklist.md`；或让生成器在 bootstrap 状态下遇到新增字段时失败。纵深防御：在世代号之外再带持久字段集合指纹，世代相同、指纹不同时延后或失败 |
| r2-battle#2 | 停机路径对没落完库的房间补发一轮落库并有界等待；或在 §13.6 明确接受同局成员不对称，并按 battle_id 给出对账或告警口径 |
| r2-battle#3 | 预检判出不足时调用号段客户端的续段入口（例如新增 `RequestTopUp(needed)`，没有下一段、没有在途或退避时立即取段）；资产通道 `ApplyCredit` 预检同改 |
| r2-battle#4 | 落库轮次设上限，到上限按"终局未知"保留房间并打告警指标或转人工；不要"放弃落库直接发结果"，那会退回原缺陷 |
| r2-ps#2 | 收件箱执行器在调用 `AttachDebt` 前先查同币种已有欠款是否带未知字段，带则 Deferred；或 `AttachDebt` 返回专用码、由 `FromTip` 映射成 kDefer。另需区分"新版本字段"与"已退役字段残留"，否则字段退役后守卫永久误拒 |
| r2-ps#3 | 把引导关闭事实持久化到不随 zone Redis 丢失的地方（etcd、zone 配置或 SQL 元数据表），任一来源为 closed 即 closed；告警加各 Pod 不一致规则；更正注释与 README:2396 |
| r2-ps#4 | 提供单玩家恢复工具：确认写方已下线后，按 task_id 豁免 C0 或丢弃该版本，并原子地把屏障 pending 回退到 SQL 已提交版本、清理 `outbox_*` 与该玩家的索引条目；关闭引导期前扫描待补投索引、重试队列与死信里的未盖戳任务；更正设计 :346、:370 |
| r2-ps#5 | `bag_system.cpp:647-648` 与 `player_equip.cpp:442` 改用 `IsIndependentInstance`，让头注释所说的"三处共用"成立 |
| r2-mz#2 | runbook §5.1、§11 改为：copy 模式下只有 `merge-zone-storage-audit -MergeAuditStorage <src>` 判可退役，源区库才能归档；合服结束时 `placed_skipped>0` 且有记录指向 src 时打印显式警告并写进清单 |
| r2-rr#1 | cvm 只推进到本轮最早未决行的 `updated_at_ms` 之前，或把未决 id 落库；换届后有未决记录时先复读再信任标记；增量出现未决时按策略撤标记 |
| r2-rr#2 | Arm=false 的追平起点改为 `satSub(min(水位键水位, cvm), lookback)`，或身份变化时一律从 0 全量 |
| r2-rr#3 | 检测到三键清空或水位键缺席的那一拍，先经 `AdvanceReconcile` 把 `cache_verified_ms` 写成 0，再 SET NX 水位键；或 Arm=false 下 VI 为空时暂不 SET NX，等全量追平完成再写。补用例：清空 → 追平中途换届或推送报错 → 下一拍起点仍为 0 |
| r2-rr#4 | `MirrorMergedInto` 对 SET NX 成功的键再复读一次库，库里已无此行就按刚写的完整值 CAD 撤掉；或由 leader 对"库里没有、值为本方案编码"的镜像做受控删除 |
| r2-mm#1 | 间隔不一致且旧轮仍在停留期内时，scene_manager 不强制覆盖，等旧轮结束再写新轮；或玩家记录按轮次保留最近 2~3 轮，`RecordPurchase` 不再整份清空。Java 版实现时一并规避 |

## 6. 后续各波（待周额度重置）

**节奏**：第二波结束时周额度 93%，要到 **2026-10-24 22:00 UTC** 才重置。按会话组约定，周额度超过 80% 时只跑风险高的一波，第三、四波等重置后再跑；每波不超过 6 个代理，开跑前和每个代理返回后都查用量。

| 波 | 内容 |
|---|---|
| 三（补覆盖与余项增量） | (a) C++ 收件箱、`id_segment_bootstrap`、`asset_op_system`、trade `asset_op_repo` 与 engine、friend 清扫、邮件清扫与 `mailadmin`（原第二波计划，未跑）。(a) 第二波提前收尾留下的未读部分：merge_zone 的 `route_store.go`、`route_publish.go`、`fence.go`、`relocate_run.go`、`unmerge.go` 正文、帮会合服围栏、合服后 ID 撞号与号段续接、`data_consistency_check`；login 的 loginqueue、dataloader、`node_cache` / `watcher`、`entergamelogic.go` :420 之后；字段保护的生成器其余部分、C++ 运行期整体替换持久组件的路径、`bag_marshal`；路由对账的 W1 发布失败语义、`route_read` / `route_reads`、`RegisterHome` 等正文。(b) player_lifecycle 未读段（交接取证、改派待确认表、EnterScene 应答分派、`HandlePlayerAsyncSaved` 等）与 `redis_manager` 改动；各业务持久组件写入口 vs 冻结闸 / 退出闸；帮会逻辑正文与 `k8s_zone_rollback.ps1`；组队匹配、评分排行与 go/db 的 `durable_enqueue` / `retry_stranded` 余项 |
| 四（复核与换视角） | (c) 第一轮 §4 的 low 复核（SanitizeSkillList 两区重复报告算一条，共 22 条；login 第 1 条已由第二波证实不成立，见 §4）；(d) 换视角连续查找（资产追踪、按故障、按入口，连续两轮无新发现才停）；对抗复核 r2-battle#1、r2-mz#1、r2-ps#1 及新出现的 high / medium |

- **从第四波撤下**：原计划对 r2-gate#1、r2-db#1、r2-sm#3 三条修复缺陷的复核。这三条与 r2-sm#1、r2-sm#2、r2-db#3 已由两个修复会话各自复核并落码（见 §5，未编译），改为在代码冻结后的增量审计（第一轮 §8 第 5 步）里查。
- **避开在途文件**：上述两个修复会话正在改 scene_manager、gate、go/db saveoutbox；第三、四波在它们交付前不审这些文件。
- **续跑方法**：见交接文档 [handoff/data-overwrite-audit-r2-20261019](../handoff/data-overwrite-audit-r2-20261019/README.md)。工作流脚本副本在同一目录（`audit-workflow.js`）。换账号或换会话后不能用 `resumeFromRunId`，要全新运行，`args` 的填法见交接文档 §5。
- **待办（不在本会话范围）**：Java 版（`D:\luyuan\xuanming-server-mmo`）同类审计，按该仓库 `AGENTS.md` 执行。

## 7. 证据与额度

- 第一波 4 个代理约 137.9 万子代理 token。开跑前 5 小时窗口 75%、周额度 74%；结束时 5 小时窗口 93%、周额度 78%（含同时在跑的其他会话）。
- 第二波：工作流 `wf_12bc0724-9b9`，6 个代理（只查找），工作流计 225.6 万子代理 token；另有 5 个收尾续跑实例约 136 万（见 §2"第二波的两份输出"），合计约 362 万。开跑前 5 小时窗口 21%、周额度 86%；结束时 5 小时窗口 45%、周额度 93%（含其他会话）。
- **教训**：给工作流里正在跑的代理发消息，不会让它提前收尾，而是另起一个续跑实例并行跑，费用翻倍。以后要压缩规模，应在开跑前就限定代理数，或直接停掉整条工作流。
- 代理完整返回保存在本会话转录里（工作流结果见该 run 的 `journal.jsonl`）；中间结论同步记在会话组共享笔记 `audit-r2-progress.md`。
