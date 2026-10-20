# 全仓数据问题与覆盖回档审计·第二轮（2026-10-19）

> 只读静态审计，**未编译、未运行**。对象是 2026-10-19 的工作区，包括其他会话尚未提交的改动。
> 本文只登记问题和修复方向，不含代码修改。修复要按 AGENTS.md §10.1 交 Codex 验证，并按 §12 同步 Java 版。
> 第一轮见 [data-overwrite-rollback-audit-20261018.md](data-overwrite-rollback-audit-20261018.md)。本轮按该文 §8 第 3 步执行。
> **状态：进行中。** 第一波（增量对抗评审 4 路）已完成，其余各波等额度窗口重置后继续，见 §6。

## 0. 前置检查（§8 第 1、2 步）

| 步骤 | 状态（截至 2026-10-19） | 依据 |
|---|---|---|
| 第 1 步：验证已落的修复 | **未完成。** §2.1：协议生成、6 个 Go 服务编译、C++ 增量构建已过；player_locator 测试、`routing_identity_test`、`cross_zone_test` 运行和真实环境故障演练没做；`EnterFenceEcs.LoadCallbackBindsNewestAdmitted` 夹具失败等负责人决定。§2.2：7 步验证一步都没跑。§2.3：login 两个包 249 例通过，没跑真实多 gate 环境 | 第一轮 §7；PROGRESS 2026-10-19 各 Codex 条目 |
| 第 2 步：修剩余 medium | **未开始。** §2.4（战斗结算三处）、§2.5（全球匹配、交易三处）都没动手 | 第一轮 §7 |
| 字段保护设计 | [mixed-version-persist-field-protection.md](mixed-version-persist-field-protection.md) v0.2 截至本轮第一波**未落码**（代码里没有 `persist_schema_gen`）。但 2026-10-19 04:15 起，其他会话正在新建 `tools/proto_generator/protogen/internal/persistschema/`，说明已开始落码。本轮不审在途半成品，留给 §8 第 5 步的冻结后增量审计 | 工作区观察 |

由于前置没完成，(a) 增量评审把 §2.1、§2.2、§2.3 的修复都当作"可能有缺陷"来查。

## 1. 结论（第一波）

- 第一波 4 路增量对抗评审，覆盖 §2.1 修复的 Go 侧和 C++/gate 侧、go/db 世代门禁与补投、data_service 读路由与调试导入。**没有新的 high 或 medium**，新增 **9 条 low**。
- 其中 3 条是第一轮修复本身的缺陷：
  - §2.1 的 R5 在"LeaveGame 先于本次登录的 Bind 到达 gate"时不成立；
  - §2.2 的补投把发布失败也计入次数，Kafka 故障约 21 分钟后意图停在 exhausted，不会自愈；
  - §2.1 的 LeaveScene 把"负载表里查不到持有节点"当作释放证据。
- 第一轮的 2 个重点点名项复核成立、无新缺陷：
  - data_service `LoadPlayerData` 字段与版本同读；
  - data-service#1（debug_import 的 uint64 精度）的修复。
- 600s 兜底强删会退回 §2.1 原问题，这一点设计已登记为有意 fail-open（[location-release-binding.md](location-release-binding.md):173-181），本轮不重报。L2、L3 是让它比设计预期更早、更常触发的两个新入口。

## 2. 方法与范围

第一轮按子系统分区，每区一名查找员。本轮换成四种查法：

| 查法 | 内容 | 状态 |
|---|---|---|
| (a) 增量对抗评审 | 基线 `bf3c60632` 到 HEAD，加工作区里涉及存盘、加载、会话、资产的改动，逐个尝试推翻 | 第一波 4 路已完成；login、merge_zone/placement/帮会合服围栏、C++ 收件箱/号段引导/trade/friend 共 3 路待做 |
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

- **只读约束**：每个代理的提示都写明不建、不改、不删文件，`git show` 不重定向，不跑构建和测试。第一波结束后检查了 4 个代理的转录，均没有 Edit 或 Write 调用，也没有重定向。
- **严重度口径**同第一轮。项目尚未上线，只在混版窗口出现的问题降一级并标注。

## 3. 第一波发现（全部 low，未做对抗复核）

### 3.1 第一轮修复本身的缺陷

| 编号 | 问题 | 位置 | 触发时序 | 可达性 | 已登记 | 自信度 |
|---|---|---|---|---|---|---|
| r2-gate#1 | **§2.1 的 R5 有缝**：LeaveGame 先于本次登录的 Bind 到达 gate 时，重置记下的是旧代次。之后到达的 Bind(gE) 不被过滤并清掉标记；session_ended 释放把 location 删掉后，gate 仍留着旧路由和 `ownerEpoch=E`。同一连接再次 EnterGame 时，gate 按旧路由把旧 epoch 补发到原节点 X，scene_manager 同时铸造 E+1 并落到 Y | 钩子 `cpp/nodes/gate/handler/rpc/client_session_handler.cpp:1144`；重置 `cpp/nodes/gate/handler/event/scene_route_helper.h:153`；判据 `:176`；补发 `:367`、`scene_entry_dispatch.cpp:359`；login 先 Bind 后复查 `go/login/internal/logic/clientplayerlogin/entergamelogic.go:791`、`:813` | ① EnterGame 已受理，Bind(gE) 还在 Kafka；客户端发 LeaveGame，gate 收包即重置，记下 g0。② 后台链已过 `:813` 复查，EnterScene 照常执行；Bind(gE) 到达，gE>g0，转发到 X 并清标记。③ LeaveScene 派发 session_ended，X 存盘、写 A1′、销毁，location 被删。④ 同一连接 EnterGame(gF)，gate 按旧路由带 E 补发到 X，scene_manager 铸 E+1 落到 Y。⑤ X 若先于铸造完成核对，会建出幽灵实体：gate 改绑到 Y 之前，客户端操作落在 X 上，随后被 CAS 拒掉（毫秒级丢失）；幽灵实体长期挂在 AOI 里。若铸造在前，X 回 3023，登录类型被这次补发消耗，Y 收到 0 型进场 | 默认路由模式可达，无开关。改包客户端可稳定复现；普通客户端需要在加载中点"返回"，并恰好落在 Bind 的 Kafka 消费延迟里 | 未登记；[location-release-binding.md](location-release-binding.md):153-158 声称这种情形已覆盖 | 很可能 |
| r2-db#1 | **§2.2 补投把发布失败也计次**：Lua 先把 attempts+1 再交出意图，Publish 失败不回退。go/db 到 Kafka 的生产持续不可用、且实体已销毁时，约 21 分钟后 attempts 到 8，意图进入 exhausted；Kafka 恢复后没有路径重置计数，玩家登录在父提交屏障处报 not committed，无法产生新版本 | `go/db/internal/saveoutbox/scripts.go:90-92`、`:76-79`；`replay.go:288-293`；MaxAttempts 缺省 8（`replay.go:75-77`）；登录卡点 `go/login/internal/logic/pkg/dataloader/parent_commit_barrier.go:42` | ① Kafka 生产不可用。② 期间场景节点崩溃或排空看门狗销毁实体（`player_lifecycle.cpp:3410-3423`），最后版本只剩 outbox 一份。③ 每拍先计数、后 publish_error，约 30s + 10…600s 退避，合计约 21 分钟到上限。④ 恢复后不再投，人工 HDEL 前该玩家无法登录 | 默认 dual 可达；需要"Kafka 故障 ≥ 约 21 分钟"和"实体已销毁"同时成立。数据不丢（仍在父快照和意图里），但故障后会大面积锁登录 | 部分登记：`db-write-fencing-mysql.md:1628-1631` 和告警 `deploy/k8s/db-task-retry-alerts.yaml:487-501` 写了 exhausted 要人工处理，但归因只写了消费侧 | 很可能 |
| r2-sm#3 | **LeaveScene 把"负载表里查不到持有节点"当作释放证据**：`scene_nodes:zone:{z}:load` 键丢失或被淘汰时，ZCARD=0、ZSCORE 为空、没有 death_at，会判 `owner_dead`。之后每次登出或断线清理都会按 any 模式立即删 location，下次进场铸造 E+1，持有节点未落盘的部分回档 | `go/scene_manager/internal/logic/leavescenelogic.go:314`；`enterscenelogic.go:1048-1068`；`reentry_barrier.go:149-152` | ① load 键丢失。② leader 下一拍 ZADD 补回之前，有玩家登出。③ 判 owner_dead，立即删 location。④ 进场铸造 E+1，持有节点未落盘的进度被拒 | 需要 load 键丢失；leader 每拍 ZADD，窗口约一拍。EnterScene 本来就用同一判据，这次修复把它扩到了每次登出都会走的路径 | 未登记（设计 §3.1 证据 2 只写"同判据"） | 很可能 |

### 3.2 其他新发现

| 编号 | 问题 | 位置 | 可达性 | 已登记 | 自信度 |
|---|---|---|---|---|---|
| r2-sm#1 | forced 计数和 ERROR 日志在删除 Lua 的结果出来之前就记了。Lua 返回 -1（GET 之后有并发 EnterScene 改写）或 0（已被别的调用删掉）时没有真正删除，却照样计 forced，触发告警 `SceneManagerLeaveReleaseForced` 误报；released 同样多计 | `go/scene_manager/internal/logic/leavescenelogic.go:241-245`、`:260-268`；告警 `deploy/k8s/scene-manager-alerts.yaml:658` | 默认开启（`LeaveSceneHolderReleaseWaitSeconds` 缺省 600）；需要恰好并发，很少见。只影响观测 | 未登记 | 确定 |
| r2-sm#2 | 600s 等待计时用的是各 scene_manager 副本的墙钟。副本 A 写 first_ms，副本 B 时钟快 Δ 或 NTP 跳变，算出的等待时长就多 Δ，提前 forced；forced 只比对 location 原文，若持有节点仍在重试存盘，就按 §2.1 原路径回档 | `go/scene_manager/internal/logic/leave_release.go:178-192`；`leavescenelogic.go:324` | 默认配置可达，但要持有节点已卡住，且副本间有时钟偏差 | 未登记（设计 §3.2、§3.6 没写时钟偏差语义，AGENTS §11.3） | 确定；影响面小 |
| r2-db#2 | legacy 存活暂停只挡之后的补投。已经发到 Kafka 的旧补投副本 V1，在 dual→legacy 降级后会被 legacy 无条件写入，盖掉 V2 | `go/db/internal/saveoutbox/replay.go:173-182`；`generation.go:69-95`；§8.6 降级步骤 `db-write-fencing-mysql.md:1012-1014` 没要求积压清零 | **仅混版或降级窗口**；缺省 dual（`db.yaml:177`、`config.go:132`） | 未登记；是第一轮 go-db-pipeline#2 的新变种 | 很可能，窗口窄 |
| r2-db#3 | 在途重试戳只取较大值，且用本机时钟。某个 go/db 实例时钟超前 Δ 时，补投会推迟约 Δ+2 分钟且不计次，期间玩家被锁登录；C++ 写新版本时不清这个戳 | `go/db/internal/saveoutbox/scripts.go:134-143`、`:86-89`；`go/db/internal/kafka/save_outbox_note.go:73` | 默认 dual；需要时钟故障加原件丢失 | 未登记 | 代码路径确定，影响取决于偏差幅度 |
| r2-ds#1 | debug_import bundle 模式"目标≠导出来源"的身份闸对 TiDB 等于没有：取不到 `@@server_uuid` 就返回空串，空的一侧直接跳过比较；地址闸只认回环地址。把线上 TiDB port-forward 到 127.0.0.1 并混用配置时，`--dry-run=false` 会把线上两张玩家行 REPLACE 成导出时的旧状态，并把行内围栏抬高，之后在线 scene 的存盘全被判 stale_owner | `go/data_service/cmd/debugutil/connect.go:101-113`、`:153-171`；`cmd/debug_import/bundle_import.go:451-465`、`:381-390`；`go/db/internal/kafka/player_row_fence.go:160-161` | 运维误用才触发；`-dry-run` 缺省 true（`main.go:63`），还需混配 | 未登记；`docs/ops/online-debug-data-fetch.md:57`、`:88` 写"按 server_uuid 拒绝"，没提 TiDB 例外 | 存疑（TiDB 对该变量的实际行为未核实） |
| r2-ds#2 | bundle 导入重铸 epoch 只取 max(线上, 本地 owner)+1，不看本地两张表的行内围栏。本地 Redis 被 FLUSHALL 而 MySQL 和 Kafka 没清时，epoch 更大的残留 DBTask 会冲掉导入的数据，之后本地新登录的存盘被判 stale_owner | `go/data_service/cmd/debug_import/bundle_import.go:381`、`:487-493`（文件头 `:39-41` 的说法在此情形不成立）；`player_row_fence.go:157-173` | 只影响本地调试环境；需要 `-dry-run=false`、`-replace` 和"清了 Redis 没清 MySQL/Kafka" | 未登记 | 很可能 |

**附带观察（非数据问题）**：客户端可以直接发 Disconnect（消息号 58），它在 `IsClientMessageId` 白名单里，和 `cpp/nodes/gate/handler/rpc/login_admission.h` 注释里"gate 合成"的说法不符。影响只限本人掉线，§2.1 修复之后不会回档。

## 4. 第一波已排除（尝试推翻成功）

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

## 5. 修复方向（第一波）

| 编号 | 修复方向 |
|---|---|
| r2-gate#1 | LeaveGame 回包成功时再执行一次 `ResetSceneRoute`，`loginGenerationAtReset` 取回包携带的 login 侧当前代次；或在 lane 里有在途或排队的 EnterGame 时延后钩子或拒绝这次 LeaveGame |
| r2-db#1 | broker 确认之后才计数（失败回退计数），或发布失败时整拍暂停认领 |
| r2-sm#3 | LeaveScene 只认带 etcd 正面死亡证据的 `playerLocationOwnerDead` |
| r2-sm#1 | forced / released 指标与 ERROR 只在 `outcome==deleted` 之后记 |
| r2-sm#2 | 等待计时改用 Redis 侧时间，例如带过期的计时键 |
| r2-db#2 | 降级步骤加"该 zone 的 topic 积压为 0"；或 legacy 收到带 (owner_epoch, save_seq) 的两表写时按冲突停机 |
| r2-db#3 | 戳超过 now+窗口视为非法并截断；或改用 Redis TIME 打戳 |
| r2-ds#1 | 任一侧身份取不到时 fail-closed，除非显式加 flag；TiDB 改用可取到的集群身份，或要求目标库带本地环境标记 |
| r2-ds#2 | E 取 max(线上, 本地 owner, 本地两行围栏, 本地屏障令牌)+1；或本地行围栏大于 owner 时拒绝导入 |

## 6. 后续各波（待额度）

| 波 | 内容 |
|---|---|
| 二 | (a) login（loginsession 新键、account_self_heal、remove_players_from_accounts、legacy_gate_killswitch、loginqueue）；(a) merge_zone route_store / route_publish / player_blob_migrate、shared/placement、guild merge_fence；(a) C++ 收件箱、id_segment_bootstrap、trade asset_op_repo、friend sweep_repo；(b) player_lifecycle 未读段（交接取证、改派待确认表、EnterScene 应答分派）；(b) 各业务持久组件写入口 vs 冻结闸 / 退出闸；(b) 帮会逻辑正文与 `k8s_zone_rollback.ps1` |
| 三 | (c) 23 条 low 复核；(d) 第一轮换视角：资产追踪、按故障、按入口 |
| 四 | (d) 第二轮换视角；high / medium 对抗复核，以及 r2-gate#1、r2-db#1、r2-sm#3 三条修复缺陷的复核 |

**待办（不在本会话范围）**：Java 版（`D:\luyuan\xuanming-server-mmo`）同类审计，按该仓库 `AGENTS.md` 执行。

## 7. 证据与额度

- 第一波 4 个代理约 137.9 万子代理 token。开跑前 5 小时窗口 75%、周额度 74%；结束时 5 小时窗口 93%、周额度 78%（含同时在跑的其他会话）。
- 代理完整返回保存在本会话转录里；中间结论同步记在会话组共享笔记 `audit-r2-progress.md`。
