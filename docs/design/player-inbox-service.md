# 玩家持久收件箱服务（player_inbox）设计

> **状态**：设计 v1.0（2026-10-15）。D1–D4 已由用户拍板（见 §13）；D5–D14 为推荐口径、待第 1 批前确认。未落码、未编译、未测试。第 1 批开始前须向用户报文件清单并获授权（AGENTS §10.2）。
> **版本历史**：v0.1 综合稿；v0.2 对抗评审修订（consistency / repo-fit / ops 三份评审的逐条处置见文末「评审记录」）；v1.0 用户拍板定稿（相对 v0.2 的改动见附录 D）。
>
> **与 [reliable-player-inbox-20261014.md](reliable-player-inbox-20261014.md) 的关系**
> - 该文 §1（`:16`）和 §5.2（`:115`）的结论是「收件箱是语义，不新建通用消息服务」「不再新建第三套通用投递器」。
> - 本设计**不推翻**该文对资产、结算、邮件的分工，只把那条结论**收窄修订**为：没有任何持久渠道、不是资产、改的是 scene 持有状态的玩家待办，可以进入一个按域准入的统一收件箱（§1.3，D1 已拍板）。
> - 第 0 批同时修订该文（文首 `:6` 加修订说明、§1 末尾 `:18` 加收窄注记、§5.2 `:115` 改写、新增 §5.4 入口）和 Java 同名文档（`:108` 新增注记、`:116` 改写）（§11.2）。
>
> **与 [landing-recovery-sync-digest.md](landing-recovery-sync-digest.md) v1.0 的关系**
> - 该文 §3.3 把收件箱列为摘要的待接入域。本稿 v1 **不接摘要**（D4 已拍板），触发方式改为以下三种（另有战斗解冻时的立即重查，共四个入口，见 §7.3）：
>   - 落地时无条件拉一次。无流时，这只是一次不加锁的主键点读。
>   - 在线时靠 Kafka 定向提醒。
>   - 另加有界兜底。
> - 将来若要接摘要，接法届时另行拍板，收件箱不另造摘要。v0.2 比较过的两种接法存档在 §7.14：乙用不下发客户端的内部字段；丙即按该文 §3.3 追加 `SyncDomain`，会泄露待执行的处置，v0.2 判为不推荐。
> - scene 触发挂在已接线的 `PlayerLandingSystem::OnEnteredScene` 上（调用点 `player_lifecycle.cpp:2755`；`player_landing.cpp:206-207` 已经为收件箱预留了一行）。这一行，以及战斗解冻处的一行，要由对方会话确认后再加（协调项 C1）。
>
> **与 [db-write-fencing-mysql.md](db-write-fencing-mysql.md) v0.3 的关系**（用户已于 2026-10-14 授权该设计落码，见 `:3`）
> - **字段号**：该文 §3.2（`:113`）已为 `player_database` 预定 `fence_owner_epoch = 18`、`fence_save_seq = 19`。对方会话已确认 18、19 由围栏设计保留（§11.1），收件箱取 20；加列纪律不允许留空洞，所以收件箱第 1 批必须在 18、19 已声明之后落码（§4.4，协调项 C4）。
> - **owner_epoch 计数器倒退**：以该文 §9.1 的「定真源、抬计数器、不归零」为准。收件箱的围栏高水位登记为 `epoch-raise` 取最大值的来源之一（C4），本服务不提供归零入口。
>
> **基线**：HEAD `6281e4d27`，加上 2026-10-15 工作区里未提交的改动。行号只作参考，以函数名为准。
>
> **标记**：
> - 【事实】可以直接核对代码或文档，附 file:line。
> - 【提议】是本设计的主张。
> - 【推断】是没有直接证据的判断，落码前需要核实。

---

## 0. 结论速览

1. **第一个真实需求是 GM 处置**：封锁某货币的获取、解除封锁、挂补缴欠款。
   - 生产方是收件箱自带的运维接口 `PlayerInboxAdmin.GmSanctionPlayer`。
   - 消费方是玩家当前的 owner scene。
   - v1 只开放一个域 `SCENE_GM_SANCTION`，三种类型含挂欠款（§2，D2 已拍板）。
2. **服务形态（D3 已拍板）**：独立进程 `go/player_inbox`，按 D-14 独占库 `mmorpg_player_inbox`。接入要求见 §1.4，分批清单见 §11。
3. **模型**：每个 (玩家, 域) 是一个有序邮箱。
   - **seq 分配**：在流行的行锁下取 `max(next_seq, 当前毫秒 << 12)`。同一条流内严格递增，全局库按备份恢复之后也不会复用旧号（IB-6）。
   - **执行与确认**：scene 主动拉取（Fetch），按 seq 升序执行。「处理到哪个 seq」和最近 32 条结局写进 `player_database` 的新字段，与货币数据同一次落盘。落盘之后才确认（Ack）。
   - **重复投递**：按 (seq, request_id, 类型) 三项核对一致后，回放原来的结局。不一致就 fail-closed。
4. **围栏**：
   - 流行上记录 owner_epoch 的高水位。判定和写入在同一个事务里完成。
   - 推进围栏之前先读核心 Redis 里当前的 owner_epoch，请求带来的值不得超过它（IB-8）。
   - Redis 计数器倒退时，按围栏文档 §9.1 的 `epoch-raise` 处置。不提供归零接口（§7.8）。
5. **已投递的条目，服务端不单方面终结**。取消只下发作废指令，由消费方裁决（IB-5）。v1 取消人工补发：要改主意，就用新的 request_id 重新下处置（§7.9）。
6. **触发入口有四个**（§7.3）。v1 不读版本摘要（D4）。
   - 实体在本节点新建、或 owner_epoch 变化后的落地：无条件拉一次。
   - 入箱提交后，经 Kafka `scene-cmd` 定向提醒。
   - 在线有界兜底：默认 30 分钟，可配置，0 表示关闭。
   - 战斗解冻时，如果之前因为战斗延后，就立即重查。
7. **安全**：
   - 运维接口用独立的 `PlayerInbox.AdminToken`，由 k8s Secret 常驻，不与回档口令共用。
   - Fetch / Ack 不做调用方签名，靠围栏上界加告警防错。威胁模型见 §1.6，备选见 D6。
8. **上线**：先扩展、后收缩。
   - 所有 scene 先以 `consumer_enabled=false` 上线：认识新字段、原样回写，但不消费。这个版本就是回滚下限。
   - 然后打开消费，最后打开入箱（§11.7）。
9. **改动量**：
   - 独立进程方案：本仓手写约 121 个文件（第 1 至第 4 批分别约 27 / 40 / 30 / 24，第 1 批另有约 7 处放行清单文档）。
   - 生成产物约 110 个，集中在第 1 批，Java 另计。
   - 远超 30 个文件的门槛，分第 0 到第 5 批落地；第 1 批开始前须向用户报文件清单并获授权。
10. **拍板状态**：D1–D4 已于 2026-10-15 由用户拍板（§13.1）：立项并收窄修订、首域 GM 处置含挂欠款、独立进程、v1 不接摘要。D5–D14 为推荐口径，第 1 批前与文件清单一并确认（§13.2）。

---

## 1. 定位与边界

### 1.1 负责与不负责

| 负责【提议】 | 不负责 |
|---|---|
| 按固定的 player_id 和目标域保存待办，等目标消费者就绪后恰好处理一次 | 业务规则（能不能执行、怎么执行），由各域的执行器决定 |
| 入箱分级（只收持久类）、幂等、容量上限、玩家存在校验 | 消费端去重的最终依据：它在消费方账本里，与业务效果同一次落盘 |
| 分配 seq、记录投递、owner_epoch 围栏、登记结局 | 余额和物品的增减，归资产通道 |
| 作废指令、保留期清理、在线提醒与补发、对账、指标 | 玩家能看到的信件归邮件；「需要刷新」的信号归 playersync |

### 1.2 与现有渠道的关系

**资产通道**
- 现状【事实】：发件箱 + (stream, epoch, seq) 窗口账本，durable 之后才终结（`go/shared/assetop/seq.go:172`、`decide.go:53-75`、`asset_op_system.cpp:185-218`）。`SYSTEM_CREDIT` 流归邮件（`mail-system.md:63`、`guild-phase2/04-asset-channel.md:49`）。
- 关系【提议】：
  - 不迁移、不代投、不占流。
  - 复用 `WithTxRetryConfig`，但必须**显式注入**可重试判定，覆盖 1213、1205、9007。`IsRetryable` 为 nil 时一次都不重试（`seq.go:266-267`）；9007 是 TiDB 写冲突，必须在内（`:351-352`）。
  - 复用 `NextAttemptMs`。
  - 「玩家能否被改」「是否已落盘」两个判定沿用资产通道的口径。

**战斗结算发件箱**
- 现状【事实】：Redis 里每个玩家一个槽，进场时由 `PlayerBattleSystem::OnPlayerEnterScene` 补应用；摘要会话已改为不过期（`landing-recovery-sync-digest.md` §0 第 5 条）。
- 关系【提议】：v1 不迁移，列为 v2 候选。

**邮件**
- 现状【事实】：未落码（`mail-system.md:4`）。
- 关系【提议】：两者互不承载。

**playersync（版本摘要）**
- 现状【事实】：
  - 已落码 `go/shared/playersync`，接口是 `NewBumper(eval, service)` 和 `(*Bumper).Bump(ctx, playerID, domains ...SyncDomain)`（`digest.go:111,133`）。
  - Redis 句柄必须是核心实例（`:20-22`）。
  - `SyncDomain` 定义在 `proto/scene/player_scene.proto:136-137`，注释写明「域号是客户端契约」。
  - scene 会把能解析成已知域的字段下发给客户端，未知字段跳过（`player_landing.h:57-58`）。
- 关系【提议】：v1 不接（D4 已拍板）。将来接入的触发条件见 §7.14。

**Kafka `scene-cmd`**
- 现状【事实】：
  - 从末尾开始消费（`node_kafka_command_handler.h:110`）。
  - 玩家不在本节点就丢弃（`player_team.cpp:132-145`）。
  - 先例 `go/match/internal/team/notify.go:226-292`。
- 关系【提议】：
  - v1 用来做在线提醒，**只是提醒**：丢了由落地拉取和兜底补上。
  - 复用 `go/shared/scenenode`（位置读取与节点镜像）和 `go/shared/kafkacmd`（寻址）。

**`SendMessageToPlayer` / `IMPORTANT`**
- 现状【事实】：找不到人就记日志返回。`IMPORTANT` 只把日志级别调高（`reliable-player-inbox-20261014.md:109`）。
- 关系【提议】：不复用。

**owner_epoch**
- 现状【事实】：
  - 只存在 Redis 的 `player:{id}:owner_epoch`，由 scene_manager 铸造。
  - 登出时删除 location（`leavescenelogic.go:33-48`）。之后补种走不到，键丢失时下一次进场从 1 开始（`enterscenelogic.go:623-650`；`db-write-fencing-mysql.md` §9.1 场景表）。
- 关系【提议】：收件箱围栏的上界和倒退处置都以它为准（§7.8）。

### 1.3 现有渠道为什么不够，以及与「不再新建第三套通用投递器」的关系

【事实】那条结论有一个前提：资产、结算、邮件这些重要业务都各有已存在的持久渠道（`reliable-player-inbox-20261014.md:67`）。GM 处置不满足这个前提：

- **生产侧没有可靠路径。** data_service 的批量回收在非 dry-run 时故意失败，注释写明缺的是「持久化的 pending-recall 意图并由 C++ scene 节点在 load/save 时消费」，返回 `ErrCodeNotImplemented`（`go/data_service/internal/logic/recall_logic.go:186-223`）。
- **scene 侧入口不可用。** 补缴相关的 GM 入口全是 `TODO(P1-B)` 空桩（`cpp/nodes/scene/handler/rpc/player/player_rollback_handler.cpp:95,107,119,132`）。这些入口是客户端 RPC，只能作用于发包者自己，生产环境下被拒（`:45-50`）。
- **执行和持久化已经就绪。** `CurrencySystem::AttachDebt / BlockCurrency / UnblockCurrency` 都在（`currency_system.h:76,102,105`）。欠款和封锁随 `CurrencyComp.blocked_types / debts` 落进 `player_database`（`proto/common/component/currency_comp.proto:19-26`）。
- **运维已经按这条路设计。** 运维手册的处置流程就是 `GM → AttachDebt(player, …)`（`docs/design/exploit_loss_prevention_zh.md:36-45`）。全局库还有一张 `player_debt` 表，全仓找不到任何 Go 读写方（`proto/common/database/rollback_database_table.proto:119-140`）。

为什么不能塞进现有渠道：

- **资产通道**：
  - 它的语义是 AssetBundle 的 Debit / Credit / Abort，按流白名单验签（`asset_op_system.cpp:80-91`）。封锁和欠款都不是余额变动。
  - 硬塞进去就要新增操作种类、流、密钥和白名单，等于在资产通道里开一条非资产的子通道。
- **直接改离线存档**：
  - 违反「Go 不写离线玩家 blob」（I7，`04-asset-channel.md:50`）。
  - C++ 版的 MySQL 行内围栏还没落码（db-write-fencing-mysql.md v0.3 状态行）。
  - 回档用的离线栅栏也没接线（`rollback_logic.go:60-70`，栅栏未配置时一律返回 NotImplemented）。
- **为 GM 处置单独做一条通道**：这条通道本身就是第三套投递器，下一个非资产域又会催出第四套。

【提议，D1 已于 2026-10-15 拍板】因此对原结论做**收窄修订**，而不是推翻：
- 资产、结算、邮件保持原来的分工。
- 不做「能投任意消息」的通用投递器。
- 只给「没有渠道、不是资产、改的是 scene 持有状态」的玩家待办提供统一入口。
- 按域准入，第一期只准入一个域（D1）。

### 1.4 服务形态：独立进程的接入要求（D3 已拍板）

D3 已于 2026-10-15 拍板：独立进程 `go/player_inbox`，按 D-14 独占库 `mmorpg_player_inbox`。寄宿 data_service 的备选已否决，理由存档在 §13.3。下面是独立进程必须完成的接入项。

**发现与连接**
- `node.proto` 加 `PlayerInboxNodeService=30`（空闲，见调研评审）。
- `node_util.cpp:11-28` 的 nodeTypeNameMap 加一项。
- scene 白名单（`cpp/nodes/scene/main.cpp:86`）加一项。
- 两份 etcd 前缀表：`bin/etc/base_deploy_config.yaml` 的 `service_discovery_prefixes`（:39-49），以及 `k8s_deploy.ps1` 中 `New-NodeConfigMapYaml`（当前 :2257 起）里的同名列表（当前约 :2395-2413；该脚本工作区有他人在改，以函数名为准）。
- 调用 deadline：在 `base_deploy_config.yaml` 的 `GrpcClient.CallDeadlineMs`（:226 起）加 `PlayerInboxNodeService`；同步把 `Assert-GrpcClientDeadlineBudget`（当前 `k8s_deploy.ps1:951`）的目标表，以及 `base_deploy_config.yaml:222-224` 注释里的「五个目标 / 这五项」改为六个。
- Go 侧照 trade 用 `shared/noderegistry` 注册（`go/trade/trade.go:238-247`）。
- 生成器按「服务名 → 包名 → 目录」推导节点类型（`tools/proto_generator/protogen/internal/model.go:163-183`），`proto/player_inbox/`（`package player_inbox`）下的服务因此挂到 PlayerInboxNodeService 下；第 1 批第 6 步核对 `grpc_init_client.cpp`。

**D-13（Etcd Key）**：**不触发**。
- D-13 只在「首个需要经 go-zero 发现某个全局服务的 Go 调用方」出现时生效（`xuanming-port-decisions-20260910.md:299-311`）。v1 的调用方是 C++ scene（走 etcd 前缀发现）和运维工具（直连）。
- 本服务的 Etcd 段照 D-13 写 `Key: ""`。
- 本服务自己调用 data_service，走的是既有的 `dataservice.rpc` 键（login 先例 `go/login/etc/login.yaml:179-183`），不涉及新全局服务的键。

**端口**：需要新选。推荐 gRPC `50950`、指标 `:9270`（D14）。
- 本地按 zone 位移后是 51950 / 52950 / 53950…，与现有各端口系列都不撞；K8s 上也空闲。
- 已经被占的：50900 归 mail、51100 归 rank；整百尾号的系列都已有人用。
- 落码前按 `mail-system.md:684-698` 的方法复核一遍。

**库**：按 D-14 独占 `mmorpg_player_inbox`（`xuanming-port-decisions-20260910.md:340-345`：「不借 data_service 的 mmorpg_global」）。要登记 MigrateJob、MySQL 连接总账和契约测试。连接池见 §10。

**生产方事务**：`GmSanctionPlayer` 就在本进程里，入箱和审计同库、同一个事务，审计写本服务的 `player_inbox_audit` 表。不需要发件箱和中继。

**玩家存在校验**：调 data_service 的 `GetPlayerHomeZone`（go-zero 客户端）。

**运维鉴权**：本服务自己校验 `AdminToken`，写法照 `authorizeAdmin`（`dataserviceserver.go:189`）。

**核心 Redis、Kafka、scene 节点镜像、data_service 客户端**：在本服务配置里新增。Redis 用于围栏上界和读 location，Kafka 和节点镜像用于在线提醒；`DataServiceRpc`（go-zero 客户端，`Key: dataservice.rpc`，照 `go/trade/etc/trade.yaml:71-75`）用于玩家存在校验和对账读账本。

**故障域与负载**：落地拉取在 S3 约每秒 0.56 万到 2.2 万次（§7.3），压在本服务上，与号段（scene DependencyGate 的条件，`main.cpp:350-352`）隔离。

**职责**：单一职责。

**手写文件数（估算）**：约 121，分批见 §11.3–§11.6。

**选独立进程的理由**：
1. 用户原话是「加一个叫玩家收件箱的服务」。
2. 落地拉取的量级让隔离有实际价值。IB-7 要求收件箱不能拖累进场，独立进程让它不与号段同在一个进程。
3. 原稿估高了独立进程的两项代价（生产方要自建发件箱、要按 D-13 定 Etcd Key），两项都不成立。

代价是多出约 18 个基础设施文件（按 v0.2 的分批估算，第 2 批多约 6 个、第 4 批多约 12 个）和一个库（相对寄宿方案，见 §13.3）。

**错误码规定**：收件箱的所有响应**只用 `TipInfoMessage error_message`**，不带 `uint32 error_code`；拦截器配 `serverbase.TipVerdict`（照 trade）。
- 注：§5.1 的对账辅助 `GetPlayerInboxLedger` 加在 data_service 里，不受本条约束。那里的进程级拦截器用私有的 `ErrorCodeClassifier` 定性（`go/data_service/data_service.go:202-250`）。它的响应形状照 `GetPlayerAssetOpLedger`（`data_service.proto:518-521`，只有 `found` 与 `ledger`，不带 `error_code` 也不带 tip），落码时核对错误语义与之一致。

### 1.5 不变量

| # | 不变量 | 靠什么保证 |
|---|---|---|
| IB-1 | 持久类条目入箱后，在有明确结局之前不会被删除或覆盖 | 清理只删终态行；满箱拒收，不挤掉旧条目 |
| IB-2 | 同一个 (producer_kind, request_id) 最多一行；同键不同内容报冲突 | 表的唯一键，加上 sha256(类型 ‖ 签发时刻 ‖ 载荷) 比对；在流行锁内权威复读 |
| IB-3 | 消费效果和账本是同一条记录、同一次落盘；Ack 只报告已经落盘的结局 | 新字段与 currency 在同一次 Marshal 里写出（与 I3 同构）；durable 判定读最后一次落盘的快照 |
| IB-4 | 结局一旦终结就不再翻转 | Ack 对终态行只做比对，不一致只告警 |
| IB-5 | 服务端不单方面终结已经投递过的条目 | 已投递的行只能挂作废指令，由消费方裁决 |
| IB-6 | 同一条流的提交顺序等于 seq 顺序，seq 严格递增，全局库恢复后也不复用 | seq 分配和 Fetch 都在流行的 `FOR UPDATE` 锁下进行；seq 取 `max(next_seq, now_ms << 12)` |
| IB-7 | 收件箱不可用时，不阻断玩家进场和正常游戏 | 不加进 DependencyGate；失败只退避 |
| IB-8 | 围栏只升，且一次请求能推到的值不超过核心 Redis 里当前的 owner_epoch | FIRST / ADVANCE 之前读 Redis 做上界判定；不提供归零接口 |
| IB-9 | 消费方回放一条重复投递的结局之前，必须确认它与账本里记的是同一条 | 比对 seq、request_id、msg_type；Ack 也带 request_id，服务端再核对一次 |

和 actor 邮箱的类比：每个 (player, domain) 是一个有序邮箱，同一时刻只有 owner scene 一个消费者。投递是「至少一次」，消费方用账本去重，最终效果等同「恰好一次」。

### 1.6 威胁模型

**运维面**：`GmSanctionPlayer` 以及查询、作废接口。
- 凭据用独立的 `PlayerInbox.AdminToken`，**为空就停用**（fail-closed）。
- 它和 data_service 守回档 RPC 的 `AdminToken` 不同：后者按设计只在维护窗口临时下发（`go/data_service/internal/config/config.go:26-43`），而 GM 处置是常态业务，所以本口令经 k8s Secret 常驻注入。
- 审计同时记录调用方自报的 `operator` 和对端地址。注释里写着地址是唯一不可伪造的部分（`dataserviceserver.go:213`）。

**scene 面**：Fetch 和 Ack。v1 按集群内网可信处理。
- 【事实】核心 Redis 不发 AUTH（`shared-redis-split-by-purpose.md:41`），data_service 的 `DeletePlayerData` 也没有鉴权（`dataserviceserver.go:254`）。能伪造 Fetch / Ack 的进程，本来就能直接改玩家存档。
- 所以 v1 的措施防的是「出错」，不是「恶意」：
  - 围栏上界（IB-8）：防止一次错误的大 epoch 把流永久锁死。
  - Ack 要求 request_id 一致、行已经投递过。
  - 异常结果告警。
- **接受的残余风险**：伪造一条 Ack，能把一条处置登记为「已执行」，而它实际没有生效（IB-4 规定不再翻转）。这种情况由每小时对账发现（§9.3）。
- 要做调用方签名的备选方案见 D6。

---

## 2. 第一个真实生产方与消费域

### 2.1 检索结果

| 候选 | 证据【事实】 | 判断 |
|---|---|---|
| GM 处置：封锁获取、解除封锁、挂补缴 | §1.3 列出的全部证据 | **没有生产路径，执行原语已就绪，列入 v1** |
| GM 道具 / 货币回收的执行 | `recall_logic.go:186-223`；Java 计划在 7.2c 做（Java `docs/porting/data-ops-spec.md:51-52,79`） | 这是余额和物品的收回，属于资产语义，应在资产通道开新流、另行立项（D10） |
| 战斗结算在玩家长期离线后丢失 | `battle_room_manager.h:179`；摘要会话已改成不过期 | 已有渠道，v2 再评估 |
| 组队 / 好友 / 帮会的刷新 | 推送只做提示，权威数据可以拉（`proto/common/event/team_event.proto:9-12`） | 归 playersync |
| 合服后的改名标记 | 由 `tools/merge_zone/post_merge_stamp.go:125` 写入，login 尽力消费 | 设计上允许丢，消费方是 login |
| 跨场景的重要消息 | 没有任何业务调用方 | 不是真实需求 |

### 2.2 v1 切片【提议】

**生产方**：`PlayerInboxAdmin.GmSanctionPlayer`，必须带 `x-admin-token`。
- 【事实】现有的 `BatchRecallItems` 入口没有调 `authorizeAdmin`（`dataserviceserver.go:494`），新接口不能沿用这个缺口。

**调用方**：v1 由运维按 runbook 用 grpcurl 调用。GM 后台接入另行立项，Java 网关的 admin API 是候选。

**批量处置**：典型场景是运营失误多发，要对大量玩家挂欠款（`exploit_loss_prevention_zh.md:36-45`）。
- 由调用侧脚本逐个玩家调用，限速每秒不超过 20 次。
- 失败时按**同一个 request_id、同一个 issued_at** 原样重试。
- 脚本规范写进 runbook。v1 不新增批量 RPC；触发条件是单次处置常态超过千人。

**玩家存在校验**：入箱前先查归属区映射，不存在就返回 `kPlayerInboxPlayerNotFound`。这样输错的 player_id 不会留下一条永远 PENDING 的行。

**消费域**：`SCENE_GM_SANCTION`。由 owner scene 在接管完成后执行，严格按 seq 顺序处理，因为封锁和解封的先后不能颠倒。

**三种类型**：

| 类型 | 执行 | 幂等性 |
|---|---|---|
| `GM_BLOCK_CURRENCY_GAIN` | `CurrencySystem::BlockCurrency` | 天然幂等（已封锁时直接返回成功，`currency_system.cpp:560-567`） |
| `GM_UNBLOCK_CURRENCY_GAIN` | `UnblockCurrency` | 天然幂等 |
| `GM_ATTACH_CURRENCY_DEBT` | `AttachDebt`，需要改成返回错误码（§11 第 3 批） | **不幂等**，必须靠账本去重 |

**AttachDebt 算不算资产指令**：它不改余额，只是让以后的 `AddCurrency` 先抵扣欠款（`currency_system.h:74-75`）。本设计把它归为「账户处置状态」，纳入 v1。这条界线已由 D2 于 2026-10-15 拍板。

### 2.3 如实评估

- 这个生产方目前没有线上流量。需求证据来自 fail-closed 分支、TODO 空桩和运维手册，不是现网正在丢消息。
- 仓库里找不到「Go 服务作为消费方」的真实场景。v1 不设计 Go 消费端（YAGNI）。
- D1、D2 已于 2026-10-15 由用户拍板立项（§13.1）。

### 2.4 后续候选（各自满足触发条件时再做）

- 战斗结算迁入收件箱。需要先设计「单一托管」，避免两边同时持有。
- 补缴的免除、调整、冻结（`WaiveDebt / AdjustDebt / FreezeDebt`）：在同一个域里加类型即可。
- 第一个 Go 消费域：届时再定账本与效果同事务的协议。
- **删号流程**：仓库目前没有真正的删号流程。`DeletePlayerData` 只删 data_service 自有的遗留键 `player:{%d}:*`，没有生产调用方（`data_logic.go:377-400`；`shared-redis-split-by-purpose.md:130`）。将来出现删号流程时，同批把该玩家的所有流作废：从未投递的直接改 VOIDED，已投递的挂作废指令。

---

## 3. 消息分级

### 3.1 三级策略与 v1 的取舍【提议】

三级策略来自用户的诉求。

| 分级 | 含义 | v1 怎么落地 |
|---|---|---|
| 持久 | 有明确结局之前一直保留 | 三种 GM 类型都属于这一类，注册表只登记持久类 |
| 合并（需要刷新） | 同一个键只保留最新状态 | 「需要刷新」的信号归 playersync。将来真出现「需要恢复的最新状态」类收件箱消息时，再追加枚举值和合并列（届时补「只取代从未投递的旧行」的规则） |
| 瞬时 | 不入箱 | 注册表不登记任何瞬时类型。生产方传入未登记的类型，一律返回 `kPlayerInboxUnknownType`，应改走尽力推送 |

### 3.2 唯一事实源

**规格写在哪里**：类型和域的规格写在 `PlayerInboxDomain` / `PlayerInboxMessageType` 的枚举值选项上，扩展号 420001、420002。
- 全仓已有的 EnumValueOptions 扩展只有 `proto/etcd/version.proto:25`（50003），420001、420002 都未被占用。
- 字段只保留 v1 用得到的：
  - 域：`max_pending`、`requires_out_of_battle`。
  - 类型：`domain`、`klass`、`payload_message`、`max_payload_bytes`。
  - 消费方种类、回档策略、过期许可、生产方白名单都推迟，出现第二种取值时再追加。

**Go 侧**：启动时用 protoreflect 把规格读成注册表并自检，任何一条不通过就拒绝启动。自检项：
- 每个类型都有所属的域。
- 载荷的消息名能解析。
- 载荷上限不超过 16KiB。
- 分级只能是 DURABLE。

**C++ 侧**：
- 用描述符读出域规格（比如是否要求不在战斗中）。
- 执行器表是一张按 `PlayerInboxMessageType` 平铺的函数表，用 `static_assert(表长 == PlayerInboxMessageType_ARRAYSIZE)` 防止脱节（AGENTS §11.2，不用宏）。
- 一个单测扫描描述符，保证每个类型都有执行器。

### 3.3 开关：每种开关只有一个来源

- **入箱**：服务配置里的 `PlayerInbox.EnqueueTypes` 白名单，默认空，即全部停用。规格里**不再**有 `enabled` 字段。没在白名单里的类型返回 `kPlayerInboxDisabled`。
  - 各实例的开关短时间不一致时，只会让 GM 偶尔收到 Disabled、重试后成功，不会出错。上线时仍按 §11.7 一次性全开。
- **消费**：scene 配置里的 `GameConfig.player_inbox.consumer_enabled`，默认 false。
  - 关闭时不发 Fetch，但已经应用、等待确认的条目照常 Ack，因为 Ack 只报告事实（§7.7）。
- **Fetch / Ack 本身没有开关**：新版本二进制始终提供这两个接口；没有条目时只是返回 NO_STREAM 或空。
  - 旧版本二进制返回 UNIMPLEMENTED，scene 长退避 10 分钟，不计为故障（§7.6）。

### 3.4 容量、满箱与长度上限

- **容量**：每个 (player, domain) 的未终结条目数不超过 `max_pending`，GM 域取 64。挂着作废指令的条目也计入。
- **满箱**：同步拒绝，返回 `kPlayerInboxFull`（业务拒绝），GM 当场就能看到。持久类既不挤掉旧条目，也不静默丢弃。
- **载荷大小**：按类型设上限，GM 类型分别是 64B 和 1KiB；硬上限 16KiB，远低于单列 256KiB 的预算（`global-data-layer-tidb-decision.md:147-148`）。
- **字符串长度**：库里的 string 列渲染成 MEDIUMTEXT，长度没有任何约束（`go/data_service/etc/data_service.yaml:189-190`），所以在入箱校验里显式检查 UTF-8 是否合法以及字节数：
  - `operator` 不超过 64B。
  - `reason` 和 `note` 不超过 256B。
  - 欠款载荷里的 `reason` 不超过 256B。
- **校验顺序**：先用服务端认定的 operator 覆盖 `attach_debt.gm_operator`，再对**最终载荷**做大小和解码校验。覆盖值会随 `CurrencyDebtEntry` 永久存进玩家存档（`currency_comp.proto:9-10`），所以不能绕过上限。

---

## 4. 数据模型

### 4.1 收件箱表

**文件位置**：
- `proto/player_inbox/player_inbox_table.proto`，由本服务的 `data.Tables()` 建表。
- **不放进** `mysql_database_table.proto`：导表器按那个文件生成 zone 库的建表清单（`rollback_database_table.proto:46-53` 的注释）。

```proto
// 每 (玩家, 域) 一行。锁序第 1。
message player_inbox_stream {
  option(OptionTableName)  = "player_inbox_stream";
  option(OptionPrimaryKey) = "shard,player_id,domain";
  option(OptionIndex)      = "shard,notify_due_ms";       // 提醒补发扫描
  option(OptionTiDBNonclusteredPK)  = true;
  option(OptionTiDBShardRowIDBits)  = 4;
  option(OptionTiDBPreSplitRegions) = 4;
  uint32 shard         = 1;  // player_id % 64,只由 inbox.RowShard 计算
  uint64 player_id     = 2;
  uint32 domain        = 3;  // PlayerInboxDomain
  uint64 next_seq      = 4;  // 上次分配的 seq + 1;分配取 max(next_seq, now_ms<<12)(IB-6)
  uint64 fence_epoch   = 5;  // owner_epoch 高水位,只升(IB-8);epoch-raise 的取 max 来源之一
  uint32 open_count    = 6;  // PENDING 条数
  uint64 notify_due_ms = 7;  // 0 = 无待补提醒;>0 = 到点补发。CAS 令牌用 next_seq,不用它(§6.3)
  uint64 created_ms    = 8;
  uint64 updated_ms    = 9;
}

message player_inbox_item {
  option(OptionTableName)  = "player_inbox_item";
  option(OptionPrimaryKey) = "shard,player_id,domain,seq";
  option(OptionUniqueKey)  = "shard,player_id,producer_kind,request_id"; // 全表唯一的 UNIQUE = 幂等键
  option(OptionIndex)      = "shard,state,updated_ms";                    // 清理 / 对账 / 运维列表
  option(OptionTiDBNonclusteredPK)  = true;
  option(OptionTiDBShardRowIDBits)  = 4;
  option(OptionTiDBPreSplitRegions) = 4;
  uint32 shard = 1;  uint64 player_id = 2;  uint32 domain = 3;  uint64 seq = 4;
  uint32 msg_type             = 5;   // PlayerInboxMessageType
  uint32 producer_kind        = 6;   // PlayerInboxProducerKind
  uint64 request_id           = 7;   // 生产方幂等键
  bytes  content_sha256       = 8;   // sha256(msg_type ‖ issued_at_ms ‖ payload)
  bytes  payload              = 9;   // ≤ 16KiB
  uint32 state                = 10;  // PlayerInboxItemState
  uint32 directive            = 11;  // PlayerInboxDirective: NONE / VOID
  uint32 void_reason          = 12;  // PlayerInboxVoidReason
  uint32 delivery_count       = 13;  // 被 Fetch 返回的次数;0 = 从未投递
  uint64 last_delivered_epoch = 14;
  uint64 last_delivered_ms    = 15;
  uint32 outcome_reason       = 16;  // 消费方给的 tip id;APPLIED 时为 0
  uint32 finalized_by         = 17;  // PlayerInboxFinalizer: CONSUMER / ADMIN
  uint64 decided_epoch        = 18;  // 终结时的 owner_epoch
  uint64 issued_at_ms         = 19;  // 生产方签发时刻
  uint64 created_ms           = 20;
  uint64 updated_ms           = 21;
  string operator_name        = 22;  // ≤ 64B,入箱时校验
  string note                 = 23;  // ≤ 256B,入箱时校验
}

// 运维审计,与入箱同库同事务(§6.1)。
message player_inbox_audit {
  option(OptionTableName)  = "player_inbox_audit";
  option(OptionPrimaryKey) = "shard,player_id,domain,seq,action";   // 每条目每种动作一行;重复调用 INSERT IGNORE
  option(OptionTiDBNonclusteredPK)  = true;
  option(OptionTiDBShardRowIDBits)  = 4;
  uint32 shard = 1;  uint64 player_id = 2;  uint32 domain = 3;  uint64 seq = 4;
  uint32 action      = 5;   // PlayerInboxAuditAction: SANCTION / VOID
  string operator    = 6;   // ≤ 64B
  string peer_addr   = 7;   // 对端地址
  string reason      = 8;   // ≤ 256B
  uint64 created_ms  = 9;
}
```

与 v0.1 相比去掉的东西（YAGNI，按 AGENTS §11.2）：
- **`merge_key`、`expires_at_ms`、`replaces_seq`**：v1 没有合并类、不允许过期，人工补发也已取消。
- **`stream_epoch`**：seq 有了时间下界之后，流被清空重建或全局库恢复都不会让新 seq 落到消费方水位以下，就不需要再靠「流纪元」重置游标（§8 #18）。

### 4.2 TiDB 约束对照

- **主键与唯一键**：主键只用整数列（`go/schemamigrate/plan.go:85,160-166`）。每张表最多一个 UNIQUE：stream 表没有，item 表一个。
- **热点打散**：
  - 主键、唯一键和二级索引都以 shard 开头，单调递增的 player_id 因此被拆成 64 个写入尾部（`global-data-layer-tidb-decision.md:115-128`）。
  - 行数据用 NONCLUSTERED + SHARD_ROW_ID_BITS 打散，和 `rollback_audit_log` 用的是同一个处方（`rollback_database_table.proto:97-102`）。
- **索引预切**【事实】：`PRE_SPLIT_REGIONS` 只预切行数据，建表时主键（非聚簇时就是一个唯一索引）、唯一键和二级索引各自只有 1 个 Region（`global-data-layer-tidb-decision.md:120-122`）。player_route 为此另跑了 `route_tool tidb-prepare`（`go/data_service/cmd/route_tool/main.go:6-11`）。
  - 【提议】第 4 批照它加一步：按 shard 边界执行 `SPLIT TABLE … INDEX …`，MySQL 上自动跳过。
  - 不这样做的备选是手写聚簇 DDL，照 player_route（D8）。
- **只用现有选项**：只用 proto2mysql 已有的 TiDB 选项（`proto_option.proto:97-100`），里面没有 CLUSTERED。
- **seq 来源**：不用 AUTO_RANDOM，也不依赖 AUTO_INCREMENT。seq 由流行显式分配，带时间下界。
- **shard 模数**：64，与 `placement.RouteRowShard` 相同，但**常量独立定义**。两张表的模数各自是建表时定下的契约，不联动修改。
- **没有 CHECK 约束**：TiDB 默认不执行 CHECK。shard 的一致性靠三样东西保证：唯一的计算函数、读出时断言、单测。
- **量级假设**：单条流在保留期内不超过数百行。Fetch 用主键范围扫描，再按 `state=PENDING` 过滤。如果指标 `player_inbox_fetch_scanned_rows` 显示超出，再加普通索引 `(shard,player_id,domain,state,seq)`。

### 4.3 条目状态机

| 起点 | 触发 | 终点 |
|---|---|---|
| PENDING | Ack 报 APPLIED / REJECTED，且 request_id 一致 | APPLIED / REJECTED（`finalized_by=CONSUMER`） |
| PENDING 且挂了 VOID 指令 | Ack 报 SKIPPED | VOIDED（`finalized_by=CONSUMER`，原因取自指令） |
| PENDING | Ack 报 UNKNOWN（消费方水位已过这一条，但原结局已不在环里） | SETTLED_UNKNOWN，同时告警 |
| PENDING 且 `delivery_count=0` | 管理员作废 | VOIDED（`finalized_by=ADMIN`） |
| PENDING 且 `delivery_count>0` | 管理员作废 / 隔离 | 仍是 PENDING，挂上 `directive=VOID`（IB-5） |
| PENDING | Ack 结果为 MISMATCH / NOT_DELIVERED / SKIP_NOT_REQUESTED | **不变**，告警，消费方停用这名玩家的这个域（§7.7） |
| 任一终态 | — | 不再变化；超过保留期后物理删除 |

### 4.4 消费端账本

新文件 `proto/common/component/player_inbox_ledger_comp.proto`：

```proto
syntax = "proto3";
option go_package = "common/component";

message PlayerInboxDecision {
  uint64 seq = 1;
  uint32 outcome = 2;      // PlayerInboxOutcome: APPLIED / REJECTED / SKIPPED
  uint32 reason = 3;       // tip id,或 PlayerInboxVoidReason
  uint64 request_id = 4;   // 回放前核对(IB-9),也供对账
  uint32 msg_type = 5;     // 回放前核对(IB-9)
}
message PlayerInboxDomainCursor {
  uint32 domain = 1;
  uint64 decided_through_seq = 2;            // 已按 seq 升序处理到这里
  repeated PlayerInboxDecision recent = 3;   // 按 seq 升序,≤ 32 条;补确认时回放原结局
}
message PlayerInboxLedgerComp {
  repeated PlayerInboxDomainCursor cursors = 1;  // 按 domain 字段查找,不靠下标(AGENTS §11.6)
}
```

**挂到 player_database**：在 message 末尾追加 `PlayerInboxLedgerComp inbox_ledger = 20;`（前提是 18、19 已由围栏声明，见下）。
- 【事实】HEAD 上最后一个字段是 `settlement_ledger = 17`（`mysql_database_table.proto:154`）。已获授权的 MySQL 围栏设计预定了 18 和 19（`db-write-fencing-mysql.md:113`），并要求「落码前再 grep 一次」（`:116`）。
- 所以字段号按 C4 协调：围栏设计保留 18 和 19（§11.1 已确认），收件箱取 20。加列纪律不允许留空洞（见下一条），收件箱第 1 批必须在 18、19 已声明之后落码；若届时围栏仍未落码，第 1 批开始前与围栏会话重新协调，不先占 18。
- 加列纪律（`mysql_database_table.proto:139-145`）：声明顺序等于字段号顺序，中间不留空洞，否则 `INSERT … SELECT *` 会静默串档。不变量 I3 同样适用（`:146-154`）。
- **放行清单**：`guild-phase2/90-consistency.md:307` 要求 18 号起新增的字段，在同一次改动里追加进三处放行清单：G-03 第 3 条、`03-names.md` §3.15a、`92-handoff.md` 步骤 4。围栏文档 §3.4 另列了 4 处未点名的位置，一并追加。
- **迁移断言**：第 1 批 Codex 验证时，`cmd/migrate plan` 只能出现这一条 ADD COLUMN（以及届时已放行的其他列）。

`drained_digest` 这个提示值已从持久记录里删掉：按 AGENTS §11.6，能重算的提示值不进落库记录。v1 也不再需要它（§7.3）。

**为什么一个游标就够**：
- 收件箱只按 seq 升序投递 PENDING 条目（IB-6）。
- 一个 seq 进入终态后永不复活；改主意就用新的 request_id 重新下处置，它会得到新的、更大的 seq。
- 所以「已处理到的最大 seq」是一个可靠的水位。被跳过的 seq，只可能是从未投给本消费方的终态行。seq 中间有空洞不影响这个论证。

**为什么最近结局保留 32 条就够**：
- 消费方规定，上一批全部确认之前不拉下一批，每批最多 16 条（§7.4）。所以「已处理、未确认」的条目最多 16 条，32 条的环一定装得下。
- 比环更老的重复投递，只可能来自前几次会话已经落盘的状态。这种情况报 UNKNOWN，由 §7.7 的专门规则确认，不会卡住。

---

## 5. API（proto 草案）

### 5.1 `PlayerInbox`（调用方：scene）

文件位置：`proto/player_inbox/player_inbox.proto`，`package player_inbox`。

这是内部服务，不标客户端协议，写法照 `proto/trade/trade_admin.proto:10-18`。

```proto
syntax = "proto3";
package player_inbox;
option go_package = "player_inbox/player_inbox";
option cc_generic_services = false;
import "google/protobuf/descriptor.proto";
import "proto/common/base/tip.proto";

enum PlayerInboxClass { PLAYER_INBOX_CLASS_UNSPECIFIED = 0; PLAYER_INBOX_CLASS_DURABLE = 1; }  // 其余分级触发时追加(§3.1)
enum PlayerInboxProducerKind { PLAYER_INBOX_PRODUCER_UNSPECIFIED = 0; PLAYER_INBOX_PRODUCER_GM = 1; }

message PlayerInboxDomainSpec { uint32 max_pending = 1; bool requires_out_of_battle = 2; }
message PlayerInboxTypeSpec {
  PlayerInboxDomain domain = 1;  PlayerInboxClass klass = 2;  string payload_message = 3;  uint32 max_payload_bytes = 4;
}
extend google.protobuf.EnumValueOptions {
  PlayerInboxDomainSpec OptionPlayerInboxDomain = 420001;
  PlayerInboxTypeSpec   OptionPlayerInboxType   = 420002;
}

enum PlayerInboxDomain {
  PLAYER_INBOX_DOMAIN_UNSPECIFIED = 0;
  PLAYER_INBOX_DOMAIN_SCENE_GM_SANCTION = 1 [(OptionPlayerInboxDomain) = { max_pending: 64 requires_out_of_battle: true }];
}
enum PlayerInboxMessageType {
  PLAYER_INBOX_MSG_UNSPECIFIED = 0;
  PLAYER_INBOX_MSG_GM_BLOCK_CURRENCY_GAIN = 1 [(OptionPlayerInboxType) = {
    domain: PLAYER_INBOX_DOMAIN_SCENE_GM_SANCTION klass: PLAYER_INBOX_CLASS_DURABLE
    payload_message: "player_inbox.GmCurrencyGainPayload" max_payload_bytes: 64 }];
  PLAYER_INBOX_MSG_GM_UNBLOCK_CURRENCY_GAIN = 2 [/* 同上 */];
  PLAYER_INBOX_MSG_GM_ATTACH_CURRENCY_DEBT  = 3 [/* payload GmCurrencyDebtPayload,1024B,其余同上 */];
}

enum PlayerInboxItemState { PLAYER_INBOX_ITEM_UNSPECIFIED = 0; PLAYER_INBOX_ITEM_PENDING = 1; PLAYER_INBOX_ITEM_APPLIED = 2;
  PLAYER_INBOX_ITEM_REJECTED = 3; PLAYER_INBOX_ITEM_VOIDED = 4; PLAYER_INBOX_ITEM_SETTLED_UNKNOWN = 5; }
enum PlayerInboxDirective { PLAYER_INBOX_DIRECTIVE_NONE = 0; PLAYER_INBOX_DIRECTIVE_VOID = 1; }
enum PlayerInboxVoidReason { PLAYER_INBOX_VOID_UNSPECIFIED = 0; PLAYER_INBOX_VOID_CANCELLED = 1; PLAYER_INBOX_VOID_QUARANTINED = 2; }
enum PlayerInboxFinalizer { PLAYER_INBOX_FINALIZER_UNSPECIFIED = 0; PLAYER_INBOX_FINALIZER_CONSUMER = 1; PLAYER_INBOX_FINALIZER_ADMIN = 2; }
enum PlayerInboxOutcome { PLAYER_INBOX_OUTCOME_UNSPECIFIED = 0; PLAYER_INBOX_OUTCOME_APPLIED = 1; PLAYER_INBOX_OUTCOME_REJECTED = 2;
  PLAYER_INBOX_OUTCOME_SKIPPED = 3; PLAYER_INBOX_OUTCOME_UNKNOWN = 4; }
enum PlayerInboxFenceVerdict { PLAYER_INBOX_FENCE_UNSPECIFIED = 0; PLAYER_INBOX_FENCE_NO_STREAM = 1; PLAYER_INBOX_FENCE_FIRST = 2;
  PLAYER_INBOX_FENCE_MATCH = 3; PLAYER_INBOX_FENCE_ADVANCE = 4; PLAYER_INBOX_FENCE_STALE = 5;
  PLAYER_INBOX_FENCE_EPOCH_REQUIRED = 6; PLAYER_INBOX_FENCE_EPOCH_AHEAD = 7; }
enum PlayerInboxAckResult { PLAYER_INBOX_ACK_UNSPECIFIED = 0; PLAYER_INBOX_ACK_FINALIZED = 1; PLAYER_INBOX_ACK_ALREADY_SAME = 2;
  PLAYER_INBOX_ACK_CONFLICT = 3; PLAYER_INBOX_ACK_UNKNOWN_SEQ = 4; PLAYER_INBOX_ACK_NOT_DELIVERED = 5;
  PLAYER_INBOX_ACK_SKIP_NOT_REQUESTED = 6; PLAYER_INBOX_ACK_MISMATCH = 7; }

message GmCurrencyGainPayload { uint32 currency_type = 1; }
message GmCurrencyDebtPayload { uint32 currency_type = 1; uint64 owe_amount = 2;   // 货币用 uint64(AGENTS §4);入箱再校验 ≤ INT64_MAX
  string reason = 3; string gm_operator = 4; uint64 expires_at = 5; }               // 单位同 CurrencyDebtEntry.expires_at

service PlayerInbox {
  rpc FetchItems(FetchPlayerInboxRequest) returns (FetchPlayerInboxResponse) {}
  rpc AckItems(AckPlayerInboxRequest)     returns (AckPlayerInboxResponse)   {}
}
message FetchPlayerInboxRequest {
  uint64 player_id = 1;  PlayerInboxDomain domain = 2;  uint64 owner_epoch = 3;  uint64 correlation_id = 4;
  uint32 max_items = 5;                 // 0 → 16,上限 16
  uint64 consumer_decided_through = 6;  // 账本水位;用于 CursorAhead 检测(§7.8)
  uint32 scene_node_id = 7;             // 只用于日志
}
message PlayerInboxItem {
  uint64 seq = 1;  PlayerInboxMessageType msg_type = 2;
  bytes payload = 3;                    // directive != NONE 时为空:作废路径不解码载荷
  PlayerInboxDirective directive = 4;  PlayerInboxVoidReason void_reason = 5;
  uint64 request_id = 6;  uint32 delivery_count = 7;  uint64 created_ms = 8;
}
message FetchPlayerInboxResponse {
  TipInfoMessage error_message = 1;     // 只用 tip(§1.4)
  uint64 player_id = 2;  PlayerInboxDomain domain = 3;  uint64 correlation_id = 4;  // 原样回显
  PlayerInboxFenceVerdict fence = 5;
  repeated PlayerInboxItem items = 6;   // 按 seq 升序,只含 PENDING
  bool has_more = 7;
}
message PlayerInboxDecisionReport {
  uint64 seq = 1;  PlayerInboxOutcome outcome = 2;  uint32 reason = 3;
  uint64 request_id = 4;                // 服务端核对(IB-9)
}
message AckPlayerInboxRequest {
  uint64 player_id = 1;  PlayerInboxDomain domain = 2;  uint64 owner_epoch = 3;  uint64 correlation_id = 4;
  repeated PlayerInboxDecisionReport decisions = 5;   // ≤ 32 条,按 seq 升序
}
message PlayerInboxAckItem { uint64 seq = 1;  PlayerInboxAckResult result = 2;  PlayerInboxItemState state = 3; }
message AckPlayerInboxResponse {
  TipInfoMessage error_message = 1;  uint64 player_id = 2;  PlayerInboxDomain domain = 3;  uint64 correlation_id = 4;
  PlayerInboxFenceVerdict fence = 5;  repeated PlayerInboxAckItem results = 6;
}
```

**在线提醒事件**：新文件 `proto/common/event/player_inbox_event.proto`，模板照 `team_event.proto:1-15`。

```proto
import "proto/common/options/event_ingress_option.proto";
message PlayerInboxNotifyEvent {
  option (OptionKafkaIngress) = true;
  uint64 player_id = 1;
  uint32 domain = 2;
}  // 只是提醒;scene 收到后安排一次检查
```

**对账辅助**：在 `proto/data_service/data_service.proto` 新增 `GetPlayerInboxLedger`。它读已落盘的 `inbox_ledger`，读法照 `GetPlayerAssetOpLedger`（`dataserviceserver.go:683`、`asset_op_ledger_logic.go`）。

### 5.2 `PlayerInboxAdmin`（调用方：运维）

每个方法都要求 `x-admin-token`，口令取 `PlayerInbox.AdminToken`（§1.6）。写操作都记审计。

```proto
service PlayerInboxAdmin {
  rpc GmSanctionPlayer(GmSanctionPlayerRequest) returns (GmSanctionPlayerResponse) {}       // 入箱(幂等)
  rpc GetSummary(GetPlayerInboxSummaryRequest) returns (GetPlayerInboxSummaryResponse) {}   // 按玩家各域:open_count/next_seq/fence_epoch/已投递未确认数/最老未决时刻;另给 max_fence_epoch 供 epoch-raise
  rpc ListItems(ListPlayerInboxItemsRequest) returns (ListPlayerInboxItemsResponse) {}      // player_id+domain+state 过滤,after_seq 翻页,limit ≤ 200
  rpc LookupByRequest(LookupPlayerInboxRequest) returns (LookupPlayerInboxResponse) {}      // (player_id, producer_kind, request_id) → 条目与结局
  rpc VoidItem(VoidPlayerInboxItemRequest) returns (VoidPlayerInboxItemResponse) {}         // 未投递 → VOIDED;已投递 → 挂 VOID 指令(IB-5)
  rpc ReconcilePlayer(ReconcilePlayerInboxRequest) returns (ReconcilePlayerInboxResponse) {} // 第 4 批:对比已落盘账本(§9.3)
}
message GmSanctionPlayerRequest {
  uint64 player_id = 1;
  uint64 request_id = 2;            // 运维工具按工单生成一次,重试原样带回
  uint64 issued_at_ms = 3;          // 同上;只接受 [now-7d, now+5min];进入内容摘要(IB-2)
  oneof action { GmCurrencyGainPayload block_gain = 4; GmCurrencyGainPayload unblock_gain = 5; GmCurrencyDebtPayload attach_debt = 6; }
  string reason = 7;  string operator = 8;   // 必填且限长(§3.4);attach_debt.gm_operator 由服务端用 operator 覆盖
}
message GmSanctionPlayerResponse { TipInfoMessage error_message = 1;  uint64 seq = 2;  bool created = 3;  PlayerInboxItemState state = 4; }
message VoidPlayerInboxItemRequest { uint64 player_id = 1; PlayerInboxDomain domain = 2; uint64 seq = 3;
  PlayerInboxVoidReason reason = 4; string operator = 5; string note = 6; }
message VoidPlayerInboxItemResponse { TipInfoMessage error_message = 1; PlayerInboxItemState state = 2; bool void_requested = 3; }
// 查询类请求/响应字段从略:按注释写;所有 id、时间、epoch 一律 uint64
```

v0.1 里的三个接口删掉了：
- **`RequeueItem`**：补发到队尾会颠倒封锁和解封的先后（评审 consistency #6）。
- **`ResetFence`**：归零与围栏文档 §9.1 相反。
- **`RebaseAfterRestore`**：seq 有了时间下界之后不再需要。

### 5.3 tip 新段【提议】

在 `data/tip/Tip.xlsx` 加组头 `//player_inbox_error base=29000 width=1000`。

**为什么是 29000**：
- 已发出的段最高到 equip 的 28000（`go/shared/generated/tip/segments.go:50`）。
- 17000 已留给 mail（`mail-system.md:25`），21000 已留给 rank（`leaderboard-system.md:342`）。
- docs 里 grep 不到对 29000 的占用。

码号由导表器分配。常量写在 `go/player_inbox/internal/constants/constants.go`，写成 `ErrX = uint32(table.PlayerInboxError_kX)`，配 `TestNoHandWrittenTipCodes`。

| A 列码名 | fault 列 | 用途 |
|---|---|---|
| `kPlayerInboxDisabled` / `kPlayerInboxUnknownType` / `kPlayerInboxPlayerNotFound` | 空 | 入箱前置拒绝 |
| `kPlayerInboxPayloadInvalid` / `kPlayerInboxPayloadTooLarge` / `kPlayerInboxTextTooLong` / `kPlayerInboxIssuedOutOfWindow` / `kPlayerInboxFull` / `kPlayerInboxIdempotencyConflict` | 空 | 入箱校验 |
| `kPlayerInboxStaleOwner` / `kPlayerInboxEpochRequired` / `kPlayerInboxEpochAhead` | 空 | 围栏（后两个同时告警） |
| `kPlayerInboxItemNotFound` / `kPlayerInboxItemStateConflict` | 空 | 运维接口 |
| `kPlayerInboxStoreUnavailable` | **1** | 服务端故障：库或核心 Redis 不可用 |

---

## 6. 产生端

### 6.1 `GmSanctionPlayer` 的处理顺序

1. 校验 `PlayerInbox.AdminToken`，用常数时间比较，照 `authorizeAdmin`；口令为空则接口停用。
2. 从 oneof 得到 msg_type 和载荷，按 §3.4 做长度校验并覆盖 `gm_operator`。
3. 调 `inbox.Store.Enqueue(ctx, params, auditFn)`（§6.2）。`auditFn` 在同一个事务里往 `player_inbox_audit` 写一行审计，内容包括 seq、operator、对端地址和理由。
4. 返回 `seq` / `created` / `state`。

### 6.2 入箱核心 `Enqueue`

这是 internal Go API，第 5 批对外开放时用同一套逻辑。

```go
type EnqueueParams struct {
    PlayerID uint64; Type pb.PlayerInboxMessageType; Producer pb.PlayerInboxProducerKind
    RequestID, IssuedAtMs uint64; Payload []byte; Operator, PeerAddr, Reason, Note string
}
type EnqueueResult struct { Code uint32; Seq uint64; Created bool; State pb.PlayerInboxItemState }
func (s *Store) Enqueue(ctx context.Context, p EnqueueParams, inTx func(*sql.Tx, uint64 /*seq*/) error) (EnqueueResult, error)
```

处理顺序如下。判重先于一切依赖当前时间的校验，这条原则来自 `mail-system.md:120`；**权威判重在流行锁之内**。

1. **注册表校验**：
   - 类型已知、在 `EnqueueTypes` 白名单里、分级是 DURABLE。
   - 载荷大小合规，能按 `payload_message` 解码。
   - 通过该类型的校验函数，例如 `owe_amount ∈ (0, INT64_MAX]`，因为 `AttachDebt` 的参数是 `int64_t`（`currency_system.h:76`）。
2. **玩家存在校验**：查归属区映射。
   - 映射不存在，返回 `kPlayerInboxPlayerNotFound`。
   - 查询失败，返回 `kPlayerInboxStoreUnavailable`。fail-closed，不入箱。
3. **算 shard 和内容摘要**：`content_sha256 = sha256(msg_type ‖ issued_at_ms ‖ payload)`。
4. **判重快路径**（事务外，按唯一键普通读一次）：
   - 已存在且摘要相同：返回 DUPLICATE（原 seq、当前状态）。
   - 已存在但摘要不同：返回 `kPlayerInboxIdempotencyConflict`。
   - 这一步只是为了省一次加锁，结论以第 7.2 步为准。
5. **时间窗**：`issued_at_ms` 不在 [now−7d, now+5min] 之内，返回 `kPlayerInboxIssuedOutOfWindow`。
6. **补建流行**：在事务外 autocommit 执行 `INSERT IGNORE`，`next_seq=1`，遇到 1213 有界重试。
   - 为什么放在事务外：照 `assetop.EnsureSeqRow`（`seq.go:72`）。两个并发请求在事务里对同一个不存在的行做「加锁读、发现没有、插入」时，共享锁升级会造成死锁。
7. **主事务**：经 `assetop.WithTxRetryConfig` 执行，参数是 `DefaultTxRetryConfig()` 再**注入** `IsRetryable`，覆盖 1213、1205、9007。隔离级固定 RC。
   1. `SELECT … FROM player_inbox_stream WHERE <pk> FOR UPDATE`。
   2. **在锁内复读唯一键**（普通读；RC 下看得见前一个持锁者已经提交的行）。命中时，同摘要返回 DUPLICATE，不同摘要返回 CONFLICT。这一步排在满箱判定之前，所以「重试与原请求赛跑」时，重试方拿到的一定是 DUPLICATE，不会是 FULL。
   3. 如果 `open_count ≥ max_pending`，返回 FULL。
   4. `seq = max(next_seq, now_ms << 12)`，插入 item：状态 PENDING，`delivery_count=0`。
   5. 更新流行：`next_seq = seq+1`、`open_count+1`、`notify_due_ms = now`。
   6. 执行 `inTx(tx, seq)`，写审计行。
   7. 提交。
   - 如果第 4 步撞上唯一键 1062，说明有并发同键：回滚后重读，按第 2 步的规则返回。
8. **提交之后发提醒**：尽力而为，最多 200ms，结果不影响返回值。流程见 §6.3。

**生产方契约**（写进 runbook 和接口注释）：
- `request_id` 和 `issued_at_ms` 都按工单只生成一次，重试时两者都原样带回。
- `issued_at` 在内容摘要里，所以在保留期内刷新它重试会得到 CONFLICT，这个错误是看得见的，不会静默重复入箱。
- 终态行保留 30 天，大于时间窗 7 天。遵守契约的重试，要么命中还在的行（返回 DUPLICATE），要么因签发时间超过 7 天被拒，不会重复入箱。

**分配公式的说明**：`now_ms << 12` 约为 2^53，每毫秒可以容纳 4096 个号，远大于 `max_pending`。
- 同一条流内，单调性由 `next_seq` 保证，不受时钟回拨影响。
- 跨库恢复的安全依赖「恢复之后的墙钟晚于备份之后已分配号的时刻」。时钟严重偏慢的极端情况由 IB-9 的核对和 CursorAhead 抬号兜底（§7.8）。

### 6.3 在线提醒与补发

**一次提醒的步骤**：
1. 读 `player:{id}:location`（`go/shared/scenenode` 的读取器），读核心 Redis。
2. **没有 location**：玩家离线，**视为成功**，下次落地会拉。
3. **有 location**：
   - 从 scene 节点镜像查到 node_id 和 uuid。
   - 发 `SceneCommand{DispatchEvent, PlayerInboxNotifyEvent}` 到 `scene-cmd`，写法照 `go/match/internal/team/notify.go:226-292`：经 `kafkacmd.SceneCommandMessage` 寻址，key 是 player_id，`target_instance_id` 是 uuid（AGENTS §7 #2、#3）。
   - 节点查不到、或 uuid 为空：算失败，留给补发循环。
4. **Kafka 写入成功**：执行 `UPDATE stream SET notify_due_ms=0 WHERE <pk> AND next_seq = <本次 seq+1>`。
   - 只要这期间又有新的入箱，`next_seq` 就变了，CAS 失败，`notify_due_ms` 留给补发循环。最多多发一次提醒，不会丢。
   - 不再用毫秒值当 CAS 令牌，消除了 ABA。

**补发循环**：
- 只在领导者上跑，领导者由 `go/shared/leader` 选举。
- 每 30 秒按分片轮转一轮：`WHERE shard=? AND notify_due_ms BETWEEN 1 AND now-5000 LIMIT 100`。5 秒宽限是为了不和请求内的那次提醒抢。
- 对每一行，读出当时的 `next_seq=N`，提醒成功后用 `WHERE next_seq=N` 做 CAS 置 0；失败就保持原值，下一轮再试。

**提醒丢了怎么办**：Kafka 从末尾消费、玩家不在本节点就丢，所以提醒是「至多一次」。兜底有两层：
- **落地拉取**。如果目标节点在入箱提交之前已经落地，那么那时 location 已经指向它，提交之后读 location 必然投到它。如果落地发生在提交之后，落地时的 Fetch 一定能看到这一条。
- **在线有界兜底**（§7.3）。

### 6.4 进程外生产方（推迟到第 5 批）

触发条件：出现第一个不在本服务进程内的生产方。届时新建 `go/shared/inboxoutbox`。

**发件箱表**：生产方在自己的库里建 `<svc>_inbox_outbox`，照 `trade_asset_op`（`proto/trade/trade_table.proto:105-130`）：
- 主键：`(shard, player_id, request_id)`。
- 列：`msg_type`、`payload`、`issued_at_ms`、`status`、`attempts`、`next_attempt_ms`、`lease_token`、`lease_until_ms`、`inbox_seq`、`last_code` 等。

**接口**：
- `Reserve(tx, msg)`：在业务事务里插入 PENDING 行。
- `Relay`：
  - 两段读，新行优先（照 `reconcile.go:82-119`）。
  - 用租约令牌对主键做 CAS 认领，同时在途不超过 8 条。
  - 退避用 `assetop.NextAttemptMs`。

**结局映射**：
- CREATED / DUPLICATE：标 DELIVERED。
- FULL / 不可用 / 超时：退避重试。
- Conflict / UnknownType / PayloadInvalid / PlayerNotFound / 鉴权失败：标 FAILED_PERMANENT 并告警。
- `issued_at` 超过 7 天：标 DEAD，交人工处理。

**同批要做的事**：对外开放 `Enqueue`；生产方 HMAC，签名串结构照 `go/shared/assetop/auth.go:77-162`。

---

## 7. 消费端协议（C++ scene）

### 7.1 为什么选择拉取，而不是推送（D 表不再单列）

| 维度 | 拉取：scene 调 Fetch / Ack（采用） | 推送：收件箱调 SceneNodeGrpc |
|---|---|---|
| 「准备好才处理」 | 消费方自己判断前置条件，过了才去拉（`reliable-player-inbox-20261014.md:10`） | 推过去会被拒，只能退避重投 |
| 落盘后再确认 | scene 自己知道何时落盘（PING 快路径，`player_battle.cpp:366-397`） | Go 侧要轮询 durable（`assetop/caller.go:28-32`） |
| 收件箱复杂度 | 只做存储和围栏；提醒只是一条 Kafka 消息 | 要定位玩家、签名、做重投循环，`reconcile.go:142-167` 的接口还得先泛化 |

### 7.2 组件

**持久组件** `PlayerInboxLedgerComp`：§4.4 的新字段。

**运行时组件** `PlayerInboxRuntimeComp`（`player/comp/player_inbox_runtime_comp.h`），**不落库**，每个域一份：
- `phase`：Idle / Fetching / Applying / AwaitDurable / Acking。
- `correlationId`：取自本节点 thread_local 的 `uint64` 计数器，**按节点单调递增**，不按实体从头计。这样已销毁实体的在途回包，不可能与新实体的请求同号。
- `sentEpoch`、`landedEpoch`。
- `pendingItems`、`ackQueue`。
- `recheckRequested`、`nextCheckMs`、`failStreak`、`deferReason`。
- `lastFetchMs`。
- `halted`、`haltReason`：协议异常时停用这名玩家的这个域，直到实体下次在本节点新建。

**损坏标记** `PlayerInboxLedgerInvalidComp`：账本校验不过时挂上。只停用收件箱，不阻断登录，照 `player_database_loader.cpp` 对资产账本的做法。

### 7.3 触发点与负载

1. **落地**：在 `PlayerLandingSystem::OnEnteredScene` 里加一行 `PlayerInboxSystem::OnPlayerLanded(player)`（C1）。
   - 【事实】这个钩子已经接线，调用点是 `player_lifecycle.cpp:2755`。首登、重登、跨节点换图、跨 zone 落地、同节点复用实体、gate 补发的重复路由都会经过它（`landing-recovery-sync-digest.md` §5.1）。
   - 【提议】由收件箱系统自己判断这是不是一次「新落地」。满足下面任一条，就安排一次**无条件 Fetch**：
     - 实体上还没有运行时组件，即实体是在本节点新建的。
     - `PlayerOwnerEpochComp.epoch` 不等于 `landedEpoch`。
   - 其余情况直接跳过：同节点换图、gate 补发的重复路由。
   - 无条件拉一次，补上了 v0.1 的一个漏洞：提醒或补发都丢了的时候，旧设计要一直等到下一次入箱才会拉。
2. **在线提醒**：`PlayerInboxNotifyEvent` 到达后，把 `nextCheckMs` 改成 now。处理函数由生成器产出，自定义逻辑写在守护段里（§7.13）。
3. **在线有界兜底**：配置项 `GameConfig.player_inbox.periodic_fetch_minutes`，默认 30，±20% 抖动，0 表示关闭。计时只记在运行时，不写进存档。它只用来兜住提醒丢失，对长期在线的玩家才有意义。
4. **战斗解冻**：在 `player_battle.cpp` 的 `RemoveInBattleComp` 里加一行 `PlayerInboxSystem::OnBattleFreezeCleared(player)`（C1）。如果之前是因为战斗延后的，就立即重查。
   - 【事实】这里已经发出资产唤醒 `kBattleFreezeCleared`（`player_battle.cpp:167-181`；`player_landing.h:151`）。不加这一行，战斗结束后最多要再等 60 秒的退避。

**负载估算**（参数取 `capacity-model-hok-scale.md:64-66`，S3 是已确认的目标档）：

| 来源 | S1（100 万在线） | S3（1000 万在线） | 落点 |
|---|---|---|---|
| 落地拉取 ≈ E + λ_x | 556 ~ 2,223 次/秒 | 5,556 ~ 22,223 次/秒 | 收件箱服务。多数玩家没有流，只是一次不加锁的主键点读（§7.8） |
| 在线兜底 ≈ PCU / 1800 | 556 次/秒 | 5,556 次/秒 | 同上 |
| 在线提醒 | = 入箱量 | = 入箱量 | Kafka scene-cmd；相对其现有约 4.4 万条/秒（`:218`）可以忽略 |
| 核心 Redis | 只在 FIRST / ADVANCE / STALE 时读一次 owner_epoch（每次归属变化至多一次），加上提醒时读 location | 同左 | 远小于 1 万次/秒 |

【推断】v0.1 每 60 秒轮询一次摘要，会在核心 Redis 上压出 S1 约 2 万、S3 约 20 万次/秒的 HMGET，而核心 Redis 单主的参照能力只有约 10 万次/秒（`:89`）。v0.2 把这部分负载移到了可以水平扩展的收件箱服务和 TiDB 上。

### 7.4 单飞与批次

- **单飞**：同一玩家、同一域在 `phase != Idle` 时，任何新触发都只置 `recheckRequested`。
- **去抖**：两次检查至少间隔 1 秒。
- **批次**：每批 `max_items=16`。上一批全部 Ack 成功之前，不拉下一批。这是 §4.4「环装得下」的前提。
- **轮次结束**：`has_more` 为真、或 `recheckRequested` 为真时，再拉一轮；否则回到 Idle。
- **tick**：每秒遍历一次 `PlayerInboxRuntimeComp` 视图，只处理到期项，单个 tick 最多发起 200 次检查。

### 7.5 执行前置闸

拉取之前检查一次。回包到达、准备应用之前**再检查一次**。下列条件全部满足才放行：

- 场景配置 `consumer_enabled` 为真。只管 Fetch，Ack 不受它限制。
- 这个域没有被 halted。
- 实体有效，且身份核对一致：按 player_id 重新找到实体，`Guid == player_id`。
- 玩家可被修改，即 `IsPlayerMutable` 为真：
  - 没有 `PlayerFrozenComp`、`PlayerTravelHandoffComp`，也没有处于 `UnregisterPlayer` 状态；
  - `PlayerOwnerEpochComp.epoch != 0`。
  - 【事实】判定在 `asset_op_system.cpp:174`，目前在匿名命名空间里。【提议】抽到 `player/system/player_mutability.h`，与资产通道共用，行为零变化（D12）。
- 没有 `PlayerInboxLedgerInvalidComp`。
- 如果域规格要求非战斗：没有 `InBattleComp`。照 `player_rollback_handler.cpp:71-72` 对 GM 写操作的战斗闸。
- 有 READY 状态的收件箱节点。选法照 `id_segment_bootstrap.cpp:91-108`。

不满足时，记下延后原因，按 1 秒到 60 秒加抖动退避。同 zone 交接是异步收尾的，落地钩子执行时实体可能仍处于冻结状态，同样靠退避解开。

**DependencyGate 不加收件箱**（`main.cpp:350-352`）。

### 7.6 处理 Fetch 回包

1. **关联校验**：按回显的 `player_id` 找到实体；`domain` 和 `correlation_id` 必须与在途请求一致，否则丢弃。
2. **传输与围栏结果**：
   - **gRPC 返回 UNIMPLEMENTED**（收件箱还是旧版本）：长退避 10 分钟，记 INFO，不计为故障。
   - **gRPC 返回 UNAVAILABLE / DEADLINE**：按 1 秒到 60 秒退避。
   - **STALE**：记 ERROR `metric=inbox_stale_owner`，退避 60 秒。
   - **EPOCH_REQUIRED / EPOCH_AHEAD**：属于缺陷或伪造，记 ERROR，halt。
   - **NO_STREAM 或没有条目**：本轮结束。
3. **按 seq 升序逐条处理**，要求 seq 严格递增：
   - **本二进制不认识的 `msg_type`，或类型不属于本域**：在这一条**停下**，不记账、不 Ack，halt，记 ERROR `metric=inbox_unknown_type`。这通常是版本错位（新类型先于 scene 升级被启用），绝不能当成终态的 REJECTED。
   - **`seq ≤ decided_through`（重复投递）**：在最近结局环里找同一个 seq。
     - 找到，且 `request_id` 与 `msg_type` 都与回包一致：把原结局放进 Ack 队列。
     - 找到，但两者有任何一项不一致：**MISMATCH**，什么都不确认，halt，记 ERROR `metric=inbox_replay_mismatch`（IB-9）。
     - 找不到：报 UNKNOWN，带上回包里的 `request_id`。
   - **`directive == VOID`**：**不解码载荷**，记 SKIPPED，原因取 `void_reason`。
   - **其余条目**：交给执行器，返回三种结果之一：
     - `APPLIED`：记账。
     - `REJECTED(reason)`：记账。
     - `DEFER(reason)`：在这一条**停下**（严格顺序），本地退避后重试，不重新 Fetch。v1 的三个执行器不会返回 DEFER。
   - **认识的类型，但载荷解码失败**：按 `REJECTED(kPlayerInboxPayloadInvalid)` 记账。

   **记账**的含义：在同一个 loop 帧里写效果，推进 `decided_through=seq`，把 (seq, 结局, request_id, msg_type) 压入最近结局环（超过 32 条就挤掉最旧的）。
4. **触发存盘**：本批有新记账时，调 `PlayerLifecycleSystem::SavePlayerToRedis`，500ms 节流（照 `RequestAssetOpPersist`，`asset_op_system.cpp:224`）。紧接着在同一条连接上发 PING 走快路径，照 `player_battle.cpp:366-397`。

**执行器的改动**：
- `GmSanctionInboxExecutor` 调用 Block、Unblock、AttachDebt。
- `AttachDebt` 现在返回 void，非法参数只记日志（`currency_system.cpp:304-330`），要改成返回 tip 码，否则执行器无法判断是否生效。
- 执行成功后写一行 `[PlayerInbox] applied type=… request_id=…` 日志，作为可检索的审计。

### 7.7 落盘之后才确认

**durable 判定** `IsInboxDecisionDurable(player, domain, seq, outcome, request_id)`：读 `PlayerLastPersistedSnapshotComp`（`last_persisted_snapshot_comp.h:43`）里的 `inbox_ledger`。
- **APPLIED / REJECTED / SKIPPED**：快照里这个域的 `decided_through ≥ seq`，并且最近结局环里能找到这个 seq，结局和 request_id 都与内存一致。
  - 如果找到了但结局不同，说明结局固定被破坏，只可能是缺陷：此时**不报** durable，记 ERROR。口径与 `IsAssetOpDurable`（`asset_op_system.cpp:185-218`）相同。
- **UNKNOWN**：只要快照里 `decided_through ≥ seq` 就算 durable，**不要求**环里有这一条。
  - UNKNOWN 只来自之前会话已经落盘的水位。新实体还没有快照时，第一次存盘之后就满足条件，与资产通道「新实体遇到已见 seq 补存一次盘」相同。
  - v0.1 要求 UNKNOWN 也在环里，而这一条按定义就不在环里，于是永远确认不了，流从此卡死。这条规则修正了这个问题。

**发 Ack 的时机**：
- PING 回调里检查一次；之后 tick 每秒检查一次，每 5 秒补请求一次存盘。
- 全部 durable 之后，一次性发 `AckItems`，每条决定都带 request_id。

**确认不受前置闸限制**：Ack 只报告已经落盘的事实，所以玩家冻结、交接、`consumer_enabled=false` 时也照发。如果新的 owner 已经推进了围栏，这条 Ack 会被判 STALE，没有关系：新 owner 加载的存档里已经有这些结局，它会自己补确认。

**处理 Ack 回包**：
- FINALIZED 和 ALREADY_SAME 视为成功。
- CONFLICT、UNKNOWN_SEQ、NOT_DELIVERED、SKIP_NOT_REQUESTED、MISMATCH 都是协议异常：
  - 记 ERROR 并计数；
  - 这名玩家的这个域 halt：本实体不再 Fetch，下次在本节点新建实体时再试；
  - 不重试。
- 这样不会在同一条行上反复空转。行还是 PENDING 时，以后的会话会再拉到它；等它被挤出结局环，会以 UNKNOWN 收尾为 SETTLED_UNKNOWN，并触发告警。

**Ack 请求失败**：保留 `ackQueue`，退避重发。如果实体在这期间被销毁，下次落地时 Fetch 会再次返回这些 PENDING 条目，届时从账本补确认。

### 7.8 owner_epoch 围栏

判定写成纯函数 `DecideFence(req, fence, redisEpoch)`，放在 `internal/inbox/fence.go`，可以单测。规则照 go/db 的 `checkOwnerEpoch`（`go/db/internal/kafka/key_ordered_consumer.go:609-645`），差别有两处：`req==0` 时拒绝；增加 Redis 上界。

| 条件 | 结局 | 动作 |
|---|---|---|
| `req == 0` | EPOCH_REQUIRED | 拒绝，不改任何状态。收件箱没有历史包袱，按 fail-closed 处理，与资产通道一致（`asset_op_system.cpp:150-157`） |
| `req > fence` 且 `req > R` | EPOCH_AHEAD | 拒绝并告警，不改状态（IB-8）。只会来自缺陷或伪造 |
| `fence == 0` 且 `req ≤ R` | FIRST | `fence = req` |
| `req == fence` | MATCH | 放行 |
| `fence < req ≤ R` | ADVANCE | `fence = req`，此后旧 epoch 的请求一律判 STALE |
| `req < fence` | STALE | 拒绝，不投递、不终结。返回前再读一次 R，用来分类：`R == req` 记为 `fence_regressed`（计数器倒退，critical，见 §8 #14）；否则记为 `zombie` |

**R 的读取**：R 是核心 Redis 里 `player:{id}:owner_epoch` 的当前值，解析用 `go/shared/ownerepoch`。
- 只在快照读到的 `req > fence` 时，才在**事务外**读一次 R。
- epoch 只增不减，所以「先读 R、后在锁内写」做上界判定没有竞态：锁内看到的 fence 只会更大，而 R 在读之后只会更大。
- R 读失败，返回 `kPlayerInboxStoreUnavailable`，不改任何状态。
- 判定和写入在**同一个事务、持有流行行锁的期间**完成，不依赖 RowsAffected。

**Fetch**（RC）：
0. **快路径（不加锁）**：普通读流行。
   - 流行不存在：返回 NO_STREAM。这是绝大多数玩家的路径，结束。
   - `open_count == 0`，且没有 CursorAhead（见第 2 步）：按快照计算围栏结局，直接返回空，**不写任何东西**。
   - 不写围栏是安全的。围栏保护的是「投递」和「确认」，而这两者都走下面的加锁路径。僵尸 scene 即使拿到条目，它的存盘也会被 Redis 的 Lua CAS 拒绝（`redis_client.h:42`），效果不会落盘，也就无法确认。
1. 否则开事务，对流行执行 `FOR UPDATE`，调 `DecideFence`。结局是 STALE、EPOCH_REQUIRED 或 EPOCH_AHEAD 时，回滚并返回。
2. **CursorAhead**：如果 `consumer_decided_through ≥ next_seq`，说明全局库被恢复过。处理是：
   - 记 `player_inbox_cursor_ahead_total` 并告警；
   - 在同一个事务里执行 `next_seq = max(next_seq, consumer_decided_through+1)`，只升不降；
   - 然后照常投递。
   - 有了这一步，即使时钟偏慢，此后的新 seq 也一定在消费方水位之上。恢复回来、水位以下的旧行，由 IB-9 的回放核对处理。
3. 普通读出 PENDING 行：主键范围扫描，按 seq 升序，`LIMIT max+1`。
4. 对每一条返回的行，用主键做条件更新：`delivery_count+1`、`last_delivered_epoch`、`last_delivered_ms`，条件是 `state=PENDING`。
5. 如果结局是 FIRST 或 ADVANCE，写回 `fence_epoch`。
6. 提交。

**Ack**（RC）：锁流行，调 `DecideFence`（同样的 R 规则），然后按 seq 升序逐条处理（下表），更新时用主键条件写。最后写回 `open_count`，需要时写回 `fence_epoch`，提交。

| 行的现状 | 报告 | 结果 |
|---|---|---|
| 不存在 | 任意 | UNKNOWN_SEQ（告警，不改） |
| `request_id` 与报告不一致 | 任意 | MISMATCH（告警，不改；IB-9） |
| PENDING 且 `delivery_count=0` | 任意 | NOT_DELIVERED（告警，不改） |
| PENDING，没有指令 | SKIPPED | SKIP_NOT_REQUESTED（告警，不改） |
| PENDING | APPLIED / REJECTED /（有指令时的）SKIPPED / UNKNOWN | 改成对应终态，写 `finalized_by=CONSUMER`、`decided_epoch`、`outcome_reason`，`open_count-1`，结果 FINALIZED |
| 终态，与报告一致 | — | ALREADY_SAME |
| 终态，与报告不一致 | — | CONFLICT（告警，**永不翻转**，IB-4） |

**挂了作废指令、消费方却报 APPLIED**：照收 APPLIED。这表示管理员的取消晚了一步，以消费方已落盘的事实为准。

**计数器倒退的处置**（§8 #14）：
- 收件箱不提供归零。
- 按 `db-write-fencing-mysql.md` §9.1 执行 `epoch-raise`，把收件箱的 `max_fence_epoch` 加进取最大值的来源（C4）。抬升之后，玩家下一次进场拿到 `C'+1`，大于收件箱围栏，判 ADVANCE。
- 归零有两个问题：会与那份文档的处置相反；僵尸 scene 会把围栏重新推回旧的高值（评审 consistency #1、ops #5）。

**备选**（D5）：围栏只告警、不拒绝，正确性完全交给 Redis CAS 加账本。代价是旧 scene 的确认不再受代际约束，偏离已获认可的方向「旧 scene 写入需要归属代际围栏」。

### 7.9 作废指令

处理规则统一（IB-5）：
- **从未投递的行**（`delivery_count=0`）：在流行锁下直接改成 VOIDED，因为载荷从未离开过收件箱。
- **已投递的行**：只挂 `directive=VOID`。消费方收到后：
  - 这条还没处理过：记 SKIPPED，然后确认；
  - 已经处理过：确认原来的结局。

**v1 不提供人工补发。** 补发到队尾，会让一条更早的封锁排到一条更新的解封后面执行，违背 GM 的最新意图（§2.2 要求严格按顺序）。运维看过当前状态之后，用新的 request_id 下一条新的处置；它的 seq 天然排在最后，表达的就是最新意图。

**毒消息**：自动隔离推迟实现（D11）。v1 的做法是 `delivery_count ≥ 5` 时告警，由运维调 `VoidItem(QUARANTINED)`。作废路径不解码载荷，所以即使载荷会让 scene 崩溃，作废也不会再触发崩溃。

### 7.10 迁移窗口

- **源 scene**：冻结或交接一开始，前置闸就拦住它，不再拉取，也不再应用。
- **已经应用、正在等落盘时开始交接**：交接时的那次存盘就是落盘点。源 scene 随后发出的 Ack 有两种结局：
  - 目标 scene 还没拉过：围栏仍是旧 epoch，判 MATCH，照常接受。
  - 目标 scene 已经拉过：判 STALE。目标加载的存档里已经带着账本，它会自己补确认。
- **源 scene 已经应用、但没落盘就失去归属**：新 epoch 铸造之后，旧 scene 的存盘会被 Redis Lua CAS 拒绝（`kSaveIfGuardLuaScript`，`redis_client.h:42-52`）。它的效果随实体一起消失，由新 owner 重做一次，所以只生效一次。
- **目标 scene**：落地钩子判定为新落地、并且通过前置闸之后才开始拉。它带的 epoch 更大，会把围栏推上去（不超过 R）。

保证的边界与资产通道相同：只覆盖 Redis 层，不覆盖 MySQL（`reliable-player-inbox-20261014.md:100`）。

### 7.11 回包关联

- 【事实】生成器不会为 gRPC 服务注册回包处理函数（`main.cpp:104-119`）。
- 【提议】手写 `InitPlayerInboxReply()`，给 Fetch 和 Ack 的 `Async…Handler / …FailedHandler` 赋值，写法照 `InitDataServiceReply`（`id_segment_bootstrap.cpp:264-272`）。在 `main.cpp` 里紧挨 `InitSceneManagerReply()` 调用。
- 回包本身不带请求上下文，所以靠回显的 `player_id + domain + correlation_id` 找回实体和在途状态。
- 失败回调能拿到原请求，从中取出 player_id，清掉在途状态，然后退避。
- 发送用生成的 `SendPlayerInboxFetchItems(registry, entity, req)`。
- 选节点照 `PickReadyDataServiceNode`，换成本服务的 registry 和 Stub 类型。
- 第 1 批生成之后，要核对 `grpc_init_client.cpp` 是否为对应的节点类型挂了这个 Stub。

### 7.12 线程

见 §10。所有回调都按 player_id 重新查找实体，不捕获裸实体句柄。

### 7.13 在线提醒在 scene 侧的接收

- proto-gen 会为 `PlayerInboxNotifyEvent` 生成 `handler/event/player_inbox_event_handler.{h,cpp}`、`event_handler.cpp` 的注册，以及 `rpc_event_registry.cpp` 里的 case。
- 守护段里只写一行：`PlayerInboxSystem::OnNotify(player_id, domain)`，找到实体就把 `nextCheckMs` 改成 now；找不到就丢弃，下次落地会拉。
- 新生成的 handler 文件要手工登记进 `cpp/nodes/scene/{CMakeLists.txt, scene.vcxproj, .filters}`。先例是 team，提交 `615494d92`。

### 7.14 将来接摘要（存档）

v1 不接版本摘要（D4 已拍板）。只有当落地拉取的负载超出预算时（量级见 §7.3 的负载估算，以 `player_inbox_fetch_total` 等指标为准），才重新评估是否接入；接法届时另行拍板，收件箱不另造摘要。v0.2 比较过的两种接法 v1 均未采用（§13.3），重新评估时以下面的结论为起点：

- **乙：内部字段名（v0.2 列为将来的门控候选）**
  - 摘要 hash 里用一个不能解析成十进制域号的字段名，例如 `ib`。scene 的解析函数把它当未知字段跳过，天然不下发客户端（`player_landing.h:57-58`），不改客户端契约。
  - 需要 playersync 新增服务端内部 bump 入口，因为现有 `Bump` 只接受正的 `SyncDomain` 值（`digest.go:192-210`）。收件箱在提醒步骤里顺带 bump；scene 落地用 `HMGET player:%llu:sync_digest g ib` 判断要不要拉。
  - 「上次拉空时的 (g, v)」只放在运行时组件里。实体在本节点新建时仍要无条件拉一次，只能省掉 `owner_epoch` 变化那一类落地拉取。
- **丙：追加 `SyncDomain`（v0.2 判为不推荐）**
  - 即按 `landing-recovery-sync-digest.md` §3.3「追加新域的规则」（`:117`）追加到枚举末尾。这是客户端可见契约变化（`player_scene.proto:136`），需要先取得客户端授权再重新生成（AGENTS §9），并与 Java 同批改（AGENTS §12）。
  - scene 还必须在下发时过滤这个域，否则待执行的 GM 处置会提前泄露给被处置的玩家；C++ 或 Java 任一侧漏写就会泄露（fail-open）。这条理由在将来接入时同样成立。

---

## 8. 失败矩阵

| # | 场景 | 处置 | 结果 |
|---|---|---|---|
| 1 | 生产方提交后崩溃（GM 没收到响应） | 条目已经落库，`notify_due_ms` 由补发循环提醒；GM 用同一个 request_id 和 issued_at 重试，得到 DUPLICATE | 不丢、不重复 |
| 2 | 重试与原请求赛跑 | 锁内复读唯一键先于满箱判定，重试方一定拿到 DUPLICATE（§6.2 第 7.2 步） | 不会误报 FULL |
| 3 | 提醒丢失（Kafka 至多一次、scene 重启、补发失败） | 落地无条件拉；在线兜底（默认 30 分钟）；补发循环用 next_seq 做 CAS，没有 ABA | 在线最多延迟一个兜底周期；离线到下次落地 |
| 4 | Fetch 之后 scene 崩溃 | 条目仍是 PENDING（v1 没有租约），下一个 owner 拉到后按账本去重 | 至少一次投递，加账本去重 |
| 5 | 已执行，但落盘前崩溃 | 内存里的效果和账本一起丢，重新执行 | 只生效一次 |
| 6 | 已落盘，但确认前崩溃 | 下次落地再次拉到，账本显示已处理，补发 Ack；不在环里时按 UNKNOWN 规则确认 | 状态变为终态 |
| 7 | 确认丢失 | 实体还在就退避重发；实体没了就等下次落地补确认 | ALREADY_SAME |
| 8 | 旧 scene 迟到的 Fetch 或 Ack | 判 STALE。旧 scene 的效果被存盘 CAS 挡住，或者早已进入新 owner 加载的存档 | 一致 |
| 9 | 迁移途中入箱 | 源 scene 被前置闸拦住；提醒按提交后的 location 投到目标，或者目标落地时拉到 | 在目标 scene 生效 |
| 10 | 玩家长期离线 | 条目一直 PENDING，GM 类型不许过期；容量上限防止无限增长。离线积压只进 info 看板，不告警（§9.4） | 不丢 |
| 11 | 毒消息 | 入箱时先校验；消费端对确定性错误给 REJECTED；引发崩溃循环时 `delivery_count≥5` 告警，运维 `VoidItem`，作废路径不解码载荷 | 可止损 |
| 12 | 收件箱库或服务不可用 | GM 收到同步失败；scene 的 Fetch 和 Ack 退避；玩法不受影响（IB-7） | 只是延迟 |
| 13 | 滚动升级的版本错位 | 收件箱旧版本：Fetch 返回 UNIMPLEMENTED，scene 长退避。scene 旧版本：`consumer_enabled` 默认关。新类型先于 scene 支持被启用：scene 遇到未知类型停在队头并告警，不记 REJECTED。部署顺序见 §11.7 | 只是延迟，不产生错误终态 |
| 14 | Redis 里的 owner_epoch 倒退（备份恢复、AOF 或主从丢尾、键被删；`db-write-fencing-mysql.md` §9.1 场景表） | 新 owner 的 epoch 小于收件箱围栏，判 STALE，分类为 `fence_regressed`（critical）。按 §9.1 定真源、踢下线、`epoch-raise`，收件箱的 `max_fence_epoch` 在取最大值的来源里（C4）。不归零 | 可恢复，不被僵尸重新毒化 |
| 15 | 已确认之后 Redis 玩家数据丢失（AOF 丢最后 1 秒） | 账本和效果一起回退，服务端行已是终态，不会再投。**每小时对账作业**发现并告警（§9.3），人工补发 | 可发现，需要人工处理 |
| 16 | 玩家回档 | 同上，账本随数据回退。回档预检对比快照账本的 `decided_through` 和收件箱里已终结的 APPLIED 行，列出差异；需要显式接受并写明原因，事后人工补发（D7）。【事实】单人回档的栅栏未接线，目前一律返回 NotImplemented（`rollback_logic.go:60-70`），所以预检与回档接线同批实现 | 不会悄悄发生 |
| 17 | 合服 | 收件箱按全局唯一的 player_id 存，不需要重映射。新字段必须在各 zone 都完成迁移（合服按列名拷贝，`tools/merge_zone/player_rows.go:352`）；`tools/merge_zone/audit_resources.go:338` 加一条只读审计 | 不受影响 |
| 18 | 收件箱库按备份恢复 | 恢复之后的新 seq 带时间下界，一定大于恢复前已分配的号。消费方水位领先 `next_seq` 时，由 CursorAhead 告警并抬号。恢复回来、已被消费方处理过的旧行，按 IB-9 核对后回放或以 UNKNOWN 收尾；核对不一致就 MISMATCH 停用。恢复窗口内丢失的条目，对照运维侧工单记录和 scene 的 `[PlayerInbox] applied` 日志人工核对后重下；不能按原 request_id 直接重放 | 不跳号、不重复；丢失项需人工处理 |
| 19 | 账本损坏 | 加载时校验：各域唯一、最近结局升序且都 ≤ decided_through、条数 ≤ 32。不通过就挂 `PlayerInboxLedgerInvalidComp` | 只停用这名玩家的收件箱 |
| 20 | GM 作废与消费方执行同时发生 | 已投递的行只挂作废指令，以消费方的裁决为准 | 状态与效果一致 |
| 21 | scene 或 go/db 降到不认识新字段的版本 | scene 旧版本按组件重建存档（`player_database_loader.cpp:140-163`），会丢掉新字段；go/db 旧版本按主键只更新已知列，账本列会停在旧值（违反 IB-3）。再升级回来，已应用、未确认的 `ATTACH_DEBT` 会被重复执行。**回滚下限** = 认识新字段的版本（§11.7）。低于下限的降级按 runbook 先关入箱和消费，对已投递、未确认的行挂 `VOID(QUARANTINED)`，升级回来之后逐条按存档核对 | 有回滚下限，破坏性降级有前置步骤 |
| 22 | player_id 输错或玩家不存在 | 入箱前查归属区映射，返回 `kPlayerInboxPlayerNotFound` | 不留永远 PENDING 的行 |
| 23 | 内网伪造 Fetch / Ack | 超大 epoch 判 EPOCH_AHEAD，不改状态；request_id 不一致判 MISMATCH；伪造的「已执行」由每小时对账发现（§1.6） | 不会锁死；残余风险可发现 |

---

## 9. 保留期、对账与可观测性

### 9.1 保留期

- 终态行保留 30 天，最少不低于 7 天，必须覆盖生产方的重试窗口。
- 清理循环只在领导者上跑：按分片走 `(shard, state, updated_ms)` 索引普通读，再按主键逐行执行 `DELETE … WHERE <pk> AND state IN (终态) AND updated_ms < cutoff`，每轮最多 500 行。
- **永不自动删除** PENDING 行，也不删除流行和审计行。

### 9.2 过期

v1 的类型都不允许过期，也没有过期列，这里不做任何处理。将来某个类型需要过期时，再追加列，处置同作废指令：从未投递的直接改 VOIDED，已投递的挂指令。

### 9.3 对账

**定时作业**（第 4 批，只在领导者上跑）：
- 每小时一轮，取最近 24 小时内由消费方终结的行（走 `(shard, state, updated_ms)` 索引）。
- 按玩家经 data_service 的 `GetPlayerInboxLedger` 读出已落盘的账本，检查 `decided_through ≥ seq`。
- 不满足就记 `player_inbox_reconcile_mismatch_total`，告警，并写一行带 player_id 的 ERROR 日志（player_id 只进日志）。
- 读不到存档的计为 `unknown`，不告警。
- GM 域的量很小，这个作业的开销可以忽略。

**手工入口** `ReconcilePlayer`：按单个玩家比对，报告两类差异：
- 收件箱已经是终态，但账本没到这个 seq：可能是回档或 Redis 丢了数据。
- 收件箱还是 PENDING，但账本已经处理过：确认丢了，而且玩家一直离线。

### 9.4 指标与告警

前缀用 `player_inbox_`。label 只用有限枚举，**不用 player_id**。

- **计数器**（每个副本各报一份）：
  - `player_inbox_enqueue_total{type,result}`
  - `player_inbox_fetch_total{domain,fence,path}`，path 取 fast / locked
  - `player_inbox_ack_total{domain,result}`
  - `player_inbox_void_total{domain,reason,path}`
  - `player_inbox_notify_total{result}`
  - `player_inbox_cursor_ahead_total`
  - `player_inbox_fence_regressed_total`
  - `player_inbox_epoch_ahead_total`
  - `player_inbox_reconcile_mismatch_total`
  - `player_inbox_sweep_deleted_total`
- **扫描型 gauge**（只由领导者产出，避免多个副本重复上报后被放大；告警和看板一律用 `max()` 聚合）：
  - `player_inbox_delivered_unacked`
  - `player_inbox_delivered_unacked_oldest_seconds`
  - `player_inbox_open_items`
  - `player_inbox_oldest_open_age_seconds`
  - `player_inbox_max_delivery_count`
  - `player_inbox_notify_backlog`
- **连接池**：单独出池指标。
- **直方图**：`player_inbox_fetch_scanned_rows`。
- **scene 侧**：沿用 `metric=` 日志的写法，例如：
  - `[PlayerInbox] metric=inbox_deferred reason=frozen|handoff|exiting|epoch_zero|in_battle|no_node|disabled`
  - `metric=inbox_unknown_type`
  - `metric=inbox_replay_mismatch`
  - `metric=inbox_halted`

**告警**：新建 `deploy/k8s/player-inbox-alerts.yaml`，安装步骤登记进 `deploy/k8s/README.md`，照 `:801-803` 的写法。
- **critical**：
  - `fence_regressed`、`epoch_ahead` 速率 > 0；
  - Ack 结果出现 MISMATCH / CONFLICT / NOT_DELIVERED / SKIP_NOT_REQUESTED / UNKNOWN_SEQ；
  - 对账出现不一致；
  - 出现 CursorAhead。
- **warning**：
  - 已投递未确认最老超过 1 小时（`delivered_unacked_oldest_seconds`），这是真正的「卡住」信号；
  - `max_delivery_count ≥ 5`；
  - 提醒积压超过 5 分钟；
  - 出现 SETTLED_UNKNOWN；
  - scene 出现 `inbox_unknown_type` 或 `inbox_halted`。
- **只进 info 看板，不告警**：未决条目数、最老未决时长。流失玩家的条目永远不会被消费，按它告警会一直亮着，淹没真正的卡住信号。

---

## 10. 并发与线程模型

**Go 侧**

- **连接池**：用独立的连接池 `PlayerInbox.MaxOpenConn / MaxIdleConn`，会话级 RC，锁等待上限 1 秒。照 route store（`go/data_service/internal/config/config.go:300`：「会话级 RC + 锁等待 1s，不能与别的 store 共用池」）。
  - 审计与入箱同库、同事务，共用本服务的这个连接池。
- **隔离级**：所有事务都是 RC，经 `WithTxRetryConfig` 执行，并**显式注入**可重试判定：1213、1205、9007。有一条单测钉住这三个码（`seq.go:351-352`）。
  - 理由同 `seq.go:136-170`：TiDB 是悲观事务、没有间隙锁；普通读必须看得见前一个持锁者已经提交的行。
- **锁顺序固定**：流行（主键 `FOR UPDATE`）→ 条目行（只按主键，seq 升序）→ 审计行插入。条目行一律先普通读、再按主键条件写，**不做**二级索引上的加锁读。
  - 依据是 `seq.go:136-150` 的教训：二级索引加锁读与主键 CAS 的加锁顺序相反，会成环。
- **不会成环**：每个事务只碰一个 (player, domain)，同一条流内部天然串行，不同流之间不存在环。
- **Redis 读**：只在事务外进行，不在持有行锁时等待 Redis。
- **后台循环**：清理、补发、扫描型 gauge、对账，都只在 `go/shared/leader` 选出的领导者上跑。
  - 清理只删终态行，逐行 autocommit，不持流行锁。Ack 不会修改终态行，所以两者不会反向抢锁。
  - 补发的每一步都是 CAS，领导者切换期间重叠执行也无害。
- **关停顺序**：先取消各后台循环和领导者选举，有界等待它们退出，再关闭连接池。

**C++ 侧**

- 落地钩子、tick、Kafka 事件、gRPC 完成队列都运行在 scene 的 loop 线程上。gRPC 完成队列由 loop 上一个 5ms 定时器轮询（`etcd_service.cpp:33-37`）。
- 状态都放在 thread_local 的 ECS 组件里，不加锁。correlation 计数器是 thread_local 的节点级单调计数。
- 定时器和异步回调只捕获 `player_id`，触发时重新查实体并核对 `correlation_id`。
- tick 中跨实体查找一律用 `try_get`（AGENTS §7.5）。

---

## 11. 分批落地计划

**第 1 批开始前，须把该批文件清单报给用户，与 D5–D14 一并确认并获授权（AGENTS §10.2）。** 每一批完成后都停下来，等 Codex 验证。下面的清单都按独立进程形态（D3）。

### 11.1 与其他会话的协调项

**C1：两处一行调用**（第 3 批）
- 落地钩子：`player_landing.cpp` 的 `OnEnteredScene` 里一行，`:206-207` 已预留位置。
- 战斗解冻：`player_battle.cpp` 的 `RemoveInBattleComp` 里一行。这里已有 `PlayerLandingSystem::PublishAssetWake(player, AssetWakeReason::kBattleFreezeCleared)`，这个来源刻意不走 5 秒合并窗口；收件箱的重查同样**不合并、不去抖**，立即置 `nextCheckMs = now`（对方会话 2026-10-15 提醒）。
- 两处都由对方会话确认后添加，不新增依赖。

**C2：摘要入口**：v1 不接摘要，对方会话已确认（2026-10-15）。本项关闭，不影响任何批次；将来接入时见 §7.14。

**C3：进场栅栏会话的 proto 改动**（第 1 批）
- 那个会话正在改 4 个 proto（`gate_event.proto` / `scene.proto` 等），要先落地；否则 proto-gen 会把它们卷进本批的生成产物。
- 那个会话也在改 `cpp/nodes/scene` 的工程文件，第 1 批要在那里登记新生成的事件 handler，需要错开。

**C4：MySQL 围栏会话**（第 0、1、4 批）
- `player_database` 的字段号：18、19 归围栏，收件箱取 20，须在 18、19 声明之后落码（§4.4）。
- 第 1 批的放行清单与它的 §3.4 合并维护。
- 在它的 §9.1 和 `epoch-raise` 入口里，把收件箱的 `max_fence_epoch` 加为取最大值的来源。做法二选一：
  - 工具直接读收件箱库；
  - 工具加一个 `-floor` 参数，由 runbook 先调 `GetSummary` 取值再传入。
- 同步改 runbook。

**对方会话 2026-10-15 的确认**：v1 不接摘要可行（C2 关闭）；`player_database` 18/19 由围栏设计保留（字段号见 §4.4、C4）；C4 中与 epoch-raise 相关的改动，留待围栏会话与其 §14 第 5 条（epoch-raise 入口位置）一起定（§11.2）。

### 11.2 第 0 批：文档（D1–D4 已于 2026-10-15 拍板，用户授权定稿）

本批范围：
- **定稿**：本稿 `docs/design/player-inbox-service.md` 升为 v1.0：改写状态行，删去被否决的备选（存档见 §13.3），§13 拆为已拍板 / 待确认 / 已否决，追加附录 D。
- **收窄修订**：`reliable-player-inbox-20261014.md` 文首 `:6` 加修订说明，§1（`:16`，收窄注记 `:18`）和 §5.2（`:115`）按 §1.3 收窄修订，新增 §5.4 入口（行号为修订后）。
- **Java 仓**：同名设计文档 `:108`（新增注记）、`:116` 同样收窄修订；`PARITY.md` 按 §12 登记一行，状态「Java 待做」。
- **索引与进度**：`docs/README.md`、`docs/PROGRESS.md`。
- **不在本批由本设计改动的项**：
  - `landing-recovery-sync-digest.md` §3.3 注明「收件箱 v1 不接摘要」的注记，由对方会话处理。
  - `db-write-fencing-mysql.md` §3.2 和 §9.1 的 C4 修改仍待围栏会话，与其 §14 第 5 条（epoch-raise 入口位置）一起定。

验收：文档评审通过。

### 11.3 第 1 批：契约与生成（手写约 27 个，生成约 110 个）

**新建（手写）**：
- `proto/player_inbox/player_inbox.proto`
- `proto/player_inbox/player_inbox_admin.proto`
- `proto/player_inbox/player_inbox_table.proto`
- `proto/common/component/player_inbox_ledger_comp.proto`
- `proto/common/event/player_inbox_event.proto`

**修改（手写）**：
- `proto/common/base/node.proto`：加 `PlayerInboxNodeService = 30`。
- `proto/common/database/mysql_database_table.proto`：import，加字段（字段号按 C4）。
- `proto/common/database/rollback_database_table.proto`：给 `player_debt`（:119-140）加弃用注释，不再接读写（D9，待确认；注释会进生成产物，随本批生成）。
- `proto/common/base/config.proto`：`GameConfig` 加 `player_inbox` 段，含 `consumer_enabled`、`periodic_fetch_minutes`。
- `proto/data_service/data_service.proto`：加 `GetPlayerInboxLedger`。
- `tools/proto_generator/protogen/etc/proto_gen.yaml`：`domain_meta` 加 player_inbox 一块，照 trade（`:425`）。
- `data/tip/Tip.xlsx`：组头加 15 行码。
- `cpp/libs/engine/core/node/system/node/node_util.cpp`：nodeTypeNameMap。
- `.gitignore`：加 `go/player_inbox` 的生成目录，照 `:163`。
- C++ 工程登记（生成器不写工程文件）：
  - `cpp/generated/proto/{CMakeLists.txt, proto.vcxproj, proto.vcxproj.filters}`
  - `cpp/generated/grpc_client/{CMakeLists.txt, grpc_client.vcxproj, .filters}`
  - `cpp/generated/table/{CMakeLists.txt, table.vcxproj, .filters}`（新 tip pb）
  - `cpp/generated/rpc/rpc.vcxproj`（service_metadata 头）
  - `cpp/nodes/scene/{CMakeLists.txt, scene.vcxproj, .filters}`（生成的事件 handler，按 C3 错开）
- **放行清单**（文档，按 C4 合并）：`guild-phase2/90-consistency.md` G-03、`03-names.md` §3.15a / §3.20、`92-handoff.md` 步骤 4，以及围栏文档 §3.4 表里另外 4 处。

**生成产物**（不得手改，以生成后的 `git status` 为准）：
- `go/proto/player_inbox/*`、`go/proto/common/{database,component,event,base}/*`
- `cpp/generated/proto/**`、`grpc_client/player_inbox/*`、`grpc_init_client.cpp`
- 注册表和路由表
- `proto/message_id.txt`、event_id 清单，各 Go 服务和 robot 的 `message_id.go` / `event_id.go`，`robot/vendor/proto/**`
- scene 的 `handler/event/player_inbox_event_handler.{h,cpp}`、`event_handler.cpp`、`rpc_event_registry.cpp`
- tip 的 proto 和 `go/shared/generated/tip/{segments,faults}.go`

**Codex 验证**（仓库根目录，按顺序执行）：
1. `pwsh tools/scripts/dev_tools.ps1 -Command proto-gen-run` 连跑两次，第二次之后 `git diff` 应为空。
2. 跑 `dev.bat gen` 导表。确认 `segments.go` 里出现 base=29000 的段，且 `faults.go` 的新段里只有 StoreUnavailable 一个码。
3. `pwsh tools/scripts/dev_tools.ps1 -Command go-svc-build`：所有 Go 服务都要编，因为 message_id 全变了。
4. 串行编译：`msbuild game.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64`。枚举变了，必须全量编。如果遇到已在 PROGRESS 登记的阻塞，单独列出来，不要算作本批失败。
5. 本地每个 zone 先跑 go/db 的 `cmd/migrate plan`：plan 里只能有新账本列（以及届时已放行的列）的 ADD COLUMN。然后 `up`，再用 `SHOW COLUMNS` 确认列在最后。
6. 检查 `grpc_init_client.cpp`：PlayerInboxNodeService 节点上挂了 PlayerInbox 的 Stub。

**Java 同步**（AGENTS §12）：tip 段、消息号、事件号属于两版共享的契约数据。C++ 第 1 批合入后，在 Java 仓执行一次 `java tools/ContractSync.java --mmorpg <本仓>`，然后 `./mvnw -B clean install`（Java `AGENTS.md:79-81`），在 Java 仓提交。这只是契约同步，不是功能实现。

**通过标准**：全部启用的模块都编译通过；Go 的 tip 测试为绿；Java 的 `ContractSync --check` 通过。

### 11.4 第 2 批：Go 服务与 GM 生产方（手写约 40 个）

**新建 `go/player_inbox/`**（骨架照 trade）：
- 入口与配置：`player_inbox.go`、`go.mod`、`etc/player_inbox.yaml`（Etcd `Key: ""`，ZoneId 写在顶层，ListenOn 与 MetricsListenAddr 是顶层单行；端口 50950、指标 :9270，按 D14 待确认；显式写 zrpc `Timeout`，供 `CallDeadlineMs.PlayerInboxNodeService ≥ Timeout + 2000` 的部署门禁核对；`DataServiceRpc` 段照 trade）。
- `internal/config/{config.go, config_test.go}`：Validate 拒绝非空 Key，断言库名；`AdminToken` 为空则运维接口停用；`EnqueueTypes`、连接池、核心 Redis、Kafka、scene 节点镜像、`DataServiceRpc`（Validate 要求必配，照 `go/trade/internal/config/config.go:440-441` 的 `HasRpcTarget` 判定）。
- `internal/svc/servicecontext.go`、`internal/data/tables.go`、`internal/lifecycle/{lifecycle.go, lifecycle_unix.go, lifecycle_windows.go}`、`internal/metrics/metrics.go`。
- `internal/constants/{constants.go, constants_test.go}`。
- `internal/inbox/`：`registry.go`、`fence.go`、`seq.go`、`shard.go`、`store.go`、`notify.go`、`loops.go`（清理、补发、gauge、领导者），以及各自的 `_test.go`，另加 `store_integration_test.go`。
- `internal/server/`：`player_inbox_server.go`、`player_inbox_admin_server.go`、`admin_auth.go`，以及测试。
- `internal/logic/`：`gm_sanction_logic.go` 及测试。
- `go.sum` 由 Codex 跑 `go mod tidy` 生成。

**验收**：

| 方面 | 必须覆盖的情形 |
|---|---|
| 围栏 | 七种结局，含 EPOCH_AHEAD；STALE 的两种分类；R 读失败不改状态 |
| seq | `max(next_seq, now<<12)`；时钟回拨时同流仍单调；CursorAhead 抬号 |
| 注册表 | 未知类型和非 DURABLE 被拒；自检失败拒绝启动 |
| 入箱 | DUPLICATE、CONFLICT（含 issued_at 不同）、FULL、签发超窗、玩家不存在、字符串超长；**重试与原请求赛跑时拿到 DUPLICATE 而不是 FULL**；并发入箱时 seq 无空洞 |
| Fetch | 快路径不加锁、不写；有条目时走锁路径 |
| Ack | 七种结果，含 MISMATCH；ADVANCE 之后旧 Ack 被拒 |
| 作废 | 未投递直接 VOIDED，已投递挂指令 |
| 后台循环 | 清理跳过 PENDING；补发以 next_seq 做 CAS，覆盖「同一毫秒两次入箱」；只在领导者上执行 |
| 重试 | 可重试判定覆盖 1213、1205、9007 |
| 鉴权 | 口令为空时停用；口令错误返回 PermissionDenied |

**Codex 命令**（在 `go/player_inbox` 目录下）：
- `gofmt -l ./internal`，应无输出。
- `go vet ./... && go build ./...`
- `go test ./... -count=1`
- 集成测试：按 trade 现有 `*_integration_test.go` 的环境变量约定配好真库后，跑 `go test -tags=integration ./internal/inbox/... -count=1 -v`。**结果不能是 SKIP**；没有真库就如实记「未执行」。并发用例另跑 `-count=20`。

### 11.5 第 3 批：C++ scene 消费端（手写约 30 个）

**新建**（目录前缀 `cpp/libs/services/scene/player/`）：
- `comp/player_inbox_runtime_comp.h`
- `system/player_inbox_rules.{h,cpp}`：纯函数，包括回放核对、最近结局环、durable 判定（含 UNKNOWN 规则）、账本校验、新落地判定
- `system/player_inbox_registry.{h,cpp}`：描述符读取、执行器表
- `system/player_inbox_system.{h,cpp}`
- `system/gm_sanction_inbox_executor.{h,cpp}`
- `system/player_mutability.h`
- `cpp/nodes/scene/rpc_replies/player_inbox_response_handler.{h,cpp}`
- `cpp/tests/currency_test/{player_inbox_rules_test.cpp, gm_sanction_inbox_executor_test.cpp}`

**修改**：
- `player_database_loader.cpp`：新字段的读、校验、写。写出位置必须在 currency 那对 `CopyFrom` / `SaveToProto` 之外，注释里写明了原因（`:154-163`）。
- `currency_system.{h,cpp}`：`AttachDebt` 改为返回 `uint32_t`；同步改调用方 `cpp/tests/currency_test/{asset_op_system_test,client_gm_gate_test,currency_test}.cpp`。
- `asset_op_system.cpp`：改为使用 `player_mutability.h`，行为零变化。
- `cpp/nodes/scene/main.cpp`：白名单加 `PlayerInboxNodeService`，调用 `InitPlayerInboxReply()`，注册 tick。
- `cpp/nodes/scene/handler/event/player_inbox_event_handler.cpp`：只改守护段。
- `player_landing.cpp`、`player_battle.cpp`：各加一行（C1）。
- `bin/etc/base_deploy_config.yaml`：前缀和 deadline。`bin/etc/game_config.yaml`：`player_inbox` 段，`consumer_enabled: false`。这两个文件工作区里已有他人改动，只追加，不动别人的内容。
- 工程文件：`cpp/libs/services/scene/{CMakeLists.txt, scene.vcxproj, .filters}`、`cpp/nodes/scene/{CMakeLists.txt, scene.vcxproj, .filters}`、`currency_test.vcxproj` 及其 CMake。

**Codex 命令**：
- 串行（`/m:1`）用 MSBuild 编 `scene` 和 `currency_test` 两个工程，配置 Debug|x64。
- 运行 `currency_test.exe --gtest_filter=PlayerInbox*:GmSanctionInbox*:AssetOp*:Currency*`，再不带过滤器、加 `--gtest_shuffle` 跑一遍。资产相关的用例必须原样通过。
- 本地栈冒烟，开始前按 AGENTS §6.2 清数据：
  1. `consumer_enabled=false` 时，存档能原样往返新字段，且不发 Fetch。
  2. 打开之后：给离线玩家下封锁，再登录。Redis 存档里 `blocked_types` 已生效，条目变为 APPLIED。
  3. 在线玩家几秒内生效（提醒路径）；停掉 Kafka 写入之后，在兜底周期内生效。
  4. 交接过程中入箱：只在目标 scene 生效。
  5. 重复提交同一个 request_id：得到 DUPLICATE。
  6. 停掉收件箱后进场：进场不受影响，恢复后补上。
  7. 战斗中入箱：解冻后立即生效。
- 失败时保留：编译错误的前 20 行、gtest 的 XML、`[PlayerInbox]` 日志。

### 11.6 第 4 批：运维与部署（约 24 个）

**运维文档、告警与工具**：
- `docs/ops/player-inbox-runbook.md`，覆盖：
  - 计数器倒退时按 §9.1 执行 epoch-raise，含 `max_fence_epoch` 一步；
  - 收件箱库恢复；
  - 回档预检；
  - 毒消息处置；
  - 回滚下限与破坏性降级的前置步骤；
  - 批量处置脚本的限速与重试规范。
- `deploy/k8s/player-inbox-alerts.yaml`，并在 `deploy/k8s/README.md` 里登记安装步骤。
- `tools/scripts/k8s_deploy.ps1`：渲染 `PlayerInbox` 段和 `AdminToken` 的 Secret 引用，加契约断言 `tools/scripts/tests/k8s_deploy_contract.tests.ps1`。dev 环境的 yaml 不带口令，照 `config_test.go:240-251` 用测试钉住。
- `tools/merge_zone/audit_resources.go`：加一条只读审计。
- TiDB 索引预切：照 `route_tool tidb-prepare` 增加一个子命令或独立命令。
- `GetPlayerInboxLedger` 的 data_service 实现（logic、server 两个文件）及测试；对账作业接线。
- 回档预检：随回档栅栏接线同批做。
- robot 的 GM 处置冒烟场景。

**独立进程的部署接线**：
- `$GoSvcCatalogue`：标 Global，加 MigrateJob、ConfigMap case、MySQL 连接总账的输入、两份 etcd 前缀和 deadline 门禁目标表（`k8s_deploy.ps1`）。
- `deploy/k8s/manifests/go-svc/{player-inbox.yaml, player-inbox-migrate.yaml}`。
- `tools/scripts/go_services.ps1`、`go_svc_image.ps1`、`start_game.ps1`（登记为可选服务，加库预检；yaml 形状须满足 `-Zone N` 改写，`DataServiceRpc.Etcd.Key: dataservice.rpc` 会被加 `.z<N>`，照 trade，`go_services.ps1:169`）。
- `deploy/mysql-init/00_init_zone_dbs.sql`。
- `go/README.md`：端口表。
- `deploy/k8s/AGENTS.md:91`。
- 测试：`k8s_migrate_gate.tests.ps1`、`start_game_command_contract.tests.ps1`；`k8s_deploy_contract.tests.ps1` 里「连 MySQL 的 ConfigMap 恰好是 friend 和 trade」的断言也要改。

Codex 跑 `pwsh -File tools/scripts/tests/*.tests.ps1` 里受影响的几个文件，以及对应的 Go 测试。

### 11.7 上线顺序与回滚下限

按顺序执行：
1. 每个 zone 库执行 `migrate`，加入账本列；建好收件箱库和表，TiDB 上预切索引。
2. 升级 go/db 到认识新字段的版本。
3. 部署收件箱服务，`EnqueueTypes` 为空。
4. 全部 scene 升级，`consumer_enabled=false`。**这就是回滚下限**：新字段只是往返，不消费。
5. 全部 scene 打开 `consumer_enabled`。
6. 全部收件箱实例打开 `EnqueueTypes`。

**回滚**：回退到下限以上的任意版本都安全。低于下限的降级是破坏性变更，按 §8 #21 和 runbook 的前置步骤执行。

### 11.8 第 5 批（推迟，满足触发条件才做）

- Fetch / Ack 的 HMAC（如果 D6 没有选在第 1 批做）。
- 对外开放 `Enqueue`、`go/shared/inboxoutbox`、生产方 HMAC。
- 合并类、过期、自动隔离、批量入箱 RPC。
- 摘要门控（v0.2 倾向乙，§7.14；接法届时另行拍板），触发条件是落地拉取的负载指标超出预算。
- 第一个 Go 消费域。

---

## 12. Java 版

【提议】Java 版按 Java 的惯用方式实现同一个切片。

- **宿主**：
  - 对应 D3 的独立进程，Java 新建模块 `game-inbox`：Spring Boot + Dubbo 提供方，group 是 `inbox`。步骤照 game-friend：根 pom、`@EnableDubbo`、Triple 端口、pbmysql 建表、`DubboGroups`、`start-slice.sh` 登记。
  - 表都用同一份 proto 经 pbmysql 建（Java `docs/porting/roadmap.md:59`）。scene 侧调用走 Dubbo，带 `GAME_DUBBO_SECRET` 鉴权附件。
- **生产方**：运维接口 `POST /admin/players/{id}/sanctions`，必须带 `Idempotency-Key`、operator 和 reason，与 Java 写接口的惯例一致；口令独立。
- **消费方**：
  - game-scene 新增 `InboxHooks`，与 `BattleHooks.onEntered` 并列，挂在 `SceneWorld.java` 的进场处（调研定位约 `:1465`）。在线提醒用 Java 自己的推送通道。
  - 账本放在 `player_state`，与 wallet 同一条记录。
  - Java 的「已落盘」是带 `owner_epoch` 条件的 MySQL 提交成功（`PlayerMapper.java:63-80`），比 C++ 只到 Redis 更强。
  - 围栏结局、Redis 上界、作废指令、游标、最近结局环、IB-9 回放核对、UNKNOWN 的确认规则，都与 C++ 逐条一致。
- **行为对齐**：Java 7.2c 原计划用「离线编辑」实现欠款（Java `docs/porting/data-ops-spec.md:52,79`）。建议改走收件箱：不夺取归属、不踢在线玩家，与 C++ 保持一致（D13）。
- **契约**：
  - 收件箱的 RPC proto 是服务端内部契约，Java 不必照抄 C++ 的实现。
  - tip 段、消息号、事件号是两版共享的契约数据，随 C++ 第 1 批**同批**在 Java 仓跑 ContractSync（§11.3）。
  - v1 不接摘要（D4），不涉及 `SyncDomain` 等客户端可见契约的变化。
- **验证**：Java 仓允许 Claude 自己编译和测试，命令是 `./mvnw -B -pl <模块> -am test`；连真库的测试加 `-Dgame.it.*`（Java `AGENTS.md:60-68`）。
- **PARITY 登记**：按列头（`PARITY.md:27`）追加一行：
  `| 玩家持久收件箱 v1（GM 处置域） | docs/design/player-inbox-service.md、go/player_inbox、scene player_inbox_system | <commit> | game-inbox、game-scene | <版本> | Java 待做 | 契约随第 1 批同步（AGENTS §12）；功能跟进方式与时点按 D13 推荐（晚于 C++ 第 3 批、欠款改走收件箱），D13 待确认 |`
  本仓 PROGRESS 的条目同时注明对应的 Java 版本号。

---

## 13. 决策

### 13.1 已拍板（2026-10-15，用户）

| # | 决策 | 结论 | 依据 |
|---|---|---|---|
| D1 | 是否立项，并修订「不再新建第三套通用投递器」 | **立项**。按 §1.3「收窄」修订：只对没有渠道、非资产、改 scene 持有状态的待办开放，按域准入；修订由第 0 批完成（§11.2） | 缺口有代码证据（§1.3）；为 GM 处置单独做一条通道，本身就成了第三套投递器 |
| D2 | 第一个域是否定为 GM 处置；`ATTACH_DEBT` 算不算资产指令 | **首域 `SCENE_GM_SANCTION`**，含 `GM_ATTACH_CURRENCY_DEBT`，认定为「账户处置状态」，纳入 v1（§2.2） | 不改余额，也不走 Debit/Credit |
| D3 | 服务形态 | **独立进程 `go/player_inbox`**，按 D-14 独占库 `mmorpg_player_inbox`；接入要求见 §1.4 | 用户原话；落地拉取量级需要与号段隔离；不触发 D-13、生产方同进程不需要发件箱。代价多约 18 个文件和一个库（§1.4） |
| D4 | 是否接版本摘要 | **甲：v1 不接**。落地无条件拉一次，在线靠 Kafka 提醒加有界兜底（§7.3）；将来接入见 §7.14 | 不动客户端契约、不动 playersync、不给核心 Redis 加负载，也没有「AOF 丢掉 bump」的漏洞 |

### 13.2 待确认（推荐口径，第 1 批前与文件清单一并确认）

按重要性排序。

| # | 决策 | 推荐 | 理由 / 备选 |
|---|---|---|---|
| D5 | 收件箱围栏的形态 | 硬拒绝 + Redis 上界 + 纳入 epoch-raise，不提供归零 | 符合已认可的「旧 scene 写入需要代际围栏」。备选：只告警不拒绝，靠 Redis CAS 加账本保证正确性，永不锁死，但失去对旧 scene 确认的约束 |
| D6 | Fetch / Ack 是否做调用方签名 | v1 不签名，只做围栏上界、request_id 核对和告警，残余风险写明（§1.6） | 集群内网本来就能直接改存档（核心 Redis 无 AUTH）。备选：第 1 批就照 assetop 做 scene→收件箱 HMAC，多约 6 个文件，外加一把 scene 密钥的部署链 |
| D7 | 回档口径 | 账本随回档回退。预检列出差异，需要显式接受；GM 类型人工补发；与回档栅栏同批实现 | 自动重放欠款可能重复处罚 |
| D13 | Java 怎么跟进 | 契约随第 1 批同步；功能也走收件箱，欠款不再走离线编辑；在 C++ 第 3 批之后实现，PARITY 记「Java 待做」 | 保持「不踢人」的行为一致 |
| D14 | 参数 | 每域 64 条；终态保留 30 天；签发窗 7 天；在线兜底 30 分钟（0 表示关闭）；投递告警阈值 5；已投递未确认告警 1 小时；tip 段 base=29000；独立端口 50950 / :9270 | 请确认，或给出运营侧的数据 |
| D8 | 表的形态 | NONCLUSTERED + shard 前缀 + 第 4 批索引预切 | 不改 proto2mysql。备选：照 player_route 手写聚簇 DDL，点查少一次回表，但要走 D-14 的「手写 DDL 例外」申请 |
| D9 | 孤儿表 `player_debt` 怎么处理 | 标注弃用，不再接任何读写 | 避免第二份事实源（DRY） |
| D10 | GM 道具 / 货币回收走哪条路 | 在资产通道新开一条流，另行立项 | 那是余额和物品的收回 |
| D11 | v1 是否做毒消息自动隔离 | 不做。投递次数达到 5 次告警，加人工 `VoidItem` | 唯一的生产方是 GM，载荷已校验；按 epoch 计数在同节点重进时不会铸新 epoch（`enterscenelogic.go:649-650`），可能永远触发不了 |
| D12 | 是否把 `IsPlayerMutable` 抽成公共头文件 | 抽 | 一条业务知识只该有一个来源；代价是资产通道要跑回归 |

### 13.3 已否决 / v1 未采用的备选（存档）

| 备选 | 所属决策 | 否决 / 未采用理由 |
|---|---|---|
| 寄宿 data_service | D3 | 手写文件约少 18 个、不新开库，但与 scene 的启动依赖（号段，DependencyGate，`main.cpp:350-352`）、快照与流水消费同在一个进程，独立连接池只隔离连接、不隔离 CPU 和 gRPC 线程，不利于 IB-7（不拖累进场）；data_service 的职责也要再扩一项 |
| D4 乙：摘要 hash 里用非域号的内部字段名 | D4（v1 未采用） | 不改客户端契约，但要 playersync 新增服务端内部 bump 入口；实体在本节点新建时仍要无条件拉一次，只能省掉 `owner_epoch` 变化那一类落地拉取，省得不多 |
| D4 丙：追加 `SyncDomain` | D4（v1 未采用） | 改客户端可见契约（`player_scene.proto:136`），需要客户端重新生成和 Java 同批；scene 还必须在下发时过滤这个域，C++ 或 Java 任一侧漏写，就会把待执行的 GM 处置提前泄露给被处置的玩家（fail-open） |
| v1 只做封锁和解封，不含挂欠款 | D2 | `AttachDebt` 不改余额、不走 Debit/Credit，可以按「账户处置状态」纳入；而挂补缴正是运维手册的处置流程（`exploit_loss_prevention_zh.md:36-45`），scene 侧补缴相关的 GM 入口至今也只是空桩（§1.3） |

---

## 附录 A：v0.1 方案评分表（历史记录，未随 v0.2 重评）

每项 1 到 5 分。v0.2 在丙的骨架上做了修订，主要改动见附录 C。本附录与附录 B 的甲 / 乙 / 丙是 v0.1 的三份候选方案（游标 / 租约逐条 / YAGNI 切片），不是 D4 的选项。

| 维度 | 甲（游标） | 乙（租约逐条） | 丙（YAGNI 切片） |
|---|---|---|---|
| 正确性与数据一致性 | **4**：游标与效果同落盘，「提交序等于 seq 序」论证扎实；但 advance 时允许游标回退并把行改回 PENDING，合并类物理删行，路径多、容易错 | **5**：完整照搬资产通道已经验证过的租约、窗口、durable 后终结、结局不翻转，证明最完整 | **4**：游标加最近结局环够用；但人工终结和补发会与已投递的行竞态，按 epoch 计数的毒消息隔离可能永远不触发，库恢复后轮换纪元会导致重复执行 |
| 简单与仓库一致性 | **3**：无租约，但回档下限、队头阻塞、停滞计数等字段很多 | **2**：租约、窗口、fence 表、端到端签名、泛型循环，还要抽模板动资产账本 | **4**：复用现有发现、鉴权和 proto2mysql，只新增一个逻辑模块 |
| YAGNI 与第一批可落地性 | **2**：独立服务加 HMAC，首域是带资产例外的回收 | **2**：手写约 110 个文件，v1 就做 Kafka，还夹带对资产代码的重构 | **5**：首域证据充分且不碰资产，推迟项各有触发条件 |
| TiDB / 性能 / 运维 | **4**：shard 前缀、保留期回收；回收要扫描游标 | **3**：三张表加每玩家 fence 行，每次进场领取都要写租约 | **4**：多数玩家只有一次主键点查 |
| 可测试性 | **4**：DecideAck 是纯函数，集成用例完整 | **4**：有窗口证明的性质测试 | **4**：纯规则头文件，单测清单完整 |
| 与资产 / 邮件 / playersync 的边界 | **2**：回收是资产扣减，与「v1 不承载资产指令」冲突 | **2**：同样以货币回收为首域，需要放宽资产边界 | **5**：GM 处置不改余额，不占 SYSTEM_CREDIT，刷新类明确归 playersync |
| **合计** | **19** | **18** | **26** |

## 附录 B：v0.1 采纳与舍弃的点（历史记录）

**采纳**

| 来源 | 采纳的点 | v0.2 状态 |
|---|---|---|
| 丙（骨架） | GM 处置作为首域；scene 主动 Fetch/Ack；游标加最近结局环；只用 `TipInfoMessage`；运维接口集合；`AttachDebt` 改为返回码；Java 同切片 | 保留；宿主推荐改为独立进程（D3）；摘要门控改为不接（D4） |
| 甲 | 「提交序等于 seq 序」的论证（IB-6） | 保留，并加上时间下界 |
| 甲 | 全局库恢复后 `next_seq` 跳号，加 CursorAhead 时 fail-closed | 被时间下界和 CursorAhead 抬号取代 |
| 甲 | 类型级 `on_rollback` 字段 | 推迟：v1 只有 MANUAL 一种取值 |
| 甲 | `DecideFence` 写成纯函数 | 保留，增加 Redis 上界 |
| 甲 | Fetch 携带 `deferred_seq` | 删除（YAGNI） |
| 甲 | 「保留期大于生产方重试期」的推导 | 保留，并把 issued_at 纳入内容摘要 |
| 乙 | 已投递的行只能下发作废，由消费方裁决（IB-5） | 保留 |
| 乙 | 结局不翻转，冲突时告警 | 保留 |
| 乙 | 入箱时判重先于依赖时间的校验 | 保留，并把权威判重移到锁内 |
| 乙 | 域级的非战斗闸 | 保留，加战斗解冻重查 |
| 乙 | 载荷用 sha256 做摘要 | 改为对 (类型, 签发时刻, 载荷) 做摘要 |
| 乙 | 发件箱与中继的形状 | 留给第 5 批 |

**舍弃或推迟**：甲的资产窄例外、游标回退、合并类物理删行、入箱 HMAC、`stall_count` 自动隔离；乙的租约加 CLAIMED、1024 位窗口、抽 seq_window 模板、端到端签名、每玩家 fence 表；丙的人工直接终结已投递行、按不同 epoch 数自动转 DEAD、恢复后轮换流纪元、单值 `drained_digest`、键 `player:sync:{id}`。v0.1 里给出的舍弃理由在 v0.2 中仍然成立。

## 附录 C：v0.2 相对 v0.1 的主要改动

| 改动 | 原因（评审条目） |
|---|---|
| 宿主推荐改为独立进程，补全对比表和两套分批清单 | repo-fit #3；落地拉取的负载 |
| v1 不接版本摘要；落地无条件拉一次，在线用 Kafka 提醒加 30 分钟兜底，去掉 60 秒轮询 | ops #4、#9；repo-fit #2、#10；consistency #4 |
| seq 加时间下界；删除 `stream_epoch` 和 `RebaseAfterRestore`；CursorAhead 改为告警并抬号 | consistency #3 |
| 回放核对 request_id 和类型；Ack 也带 request_id，新增 MISMATCH | consistency #3 |
| UNKNOWN 的 durable 规则不再要求结局在环里；协议异常时 halt，不空转 | consistency #2 |
| 删除 `ResetFence`；围栏加 Redis 上界；STALE 分类；纳入 epoch-raise | consistency #1；ops #1、#5；repo-fit #9 |
| 提醒补发的 CAS 改用 next_seq | consistency #4；repo-fit #6；ops #11 |
| 锁内复读唯一键；issued_at 纳入摘要 | consistency #5 |
| 删除 `RequeueItem` | consistency #6 |
| 未知类型停在队头，不再记 REJECTED | consistency #7 |
| correlation 改为节点级单调计数 | consistency #8 |
| 字段号不再写死 18；补放行清单和迁移断言 | repo-fit #1 |
| 显式注入可重试判定 1213 / 1205 / 9007 | repo-fit #5 |
| 每小时对账作业 | repo-fit #7 |
| 玩家存在校验 | repo-fit #8；ops #10 |
| 删除 v1 没有调用方的字段和枚举；开关各只有一个来源 | repo-fit #10 |
| 审计事务和鉴权的落点按形态分别写清 | repo-fit #11 |
| 战斗解冻重查 | repo-fit #12 |
| 独立的运维口令 | ops #2 |
| 先扩展后收缩、回滚下限 | ops #3 |
| Fetch 不加锁的快路径；索引预切 | ops #6 |
| 独立连接池和锁等待上限 | ops #7 |
| `consumer_enabled`；UNIMPLEMENTED 长退避 | ops #8 |
| 告警改为「已投递未确认」；gauge 只由领导者产出 | ops #10 |
| 字符串限长和校验顺序 | ops #12 |

## 附录 D：v1.0 相对 v0.2 的改动

| 改动 | 原因 |
|---|---|
| 状态行改为 v1.0，加版本历史；与其他文档关系的说明改为已定陈述 | 用户 2026-10-15 拍板 D1–D4 |
| §0 第 1、2、9、10 条，§1.2 的 playersync 一行，§1.3 的收窄提议改为已定陈述；改动量只保留独立进程方案 | 同上 |
| §1.4 改写为「独立进程的接入要求」；删去寄宿 data_service 的对比和拆分接缝，以及 §4.1、§5.1、§5.3、§6.1、§7.5、§9.4、§10、§11.3–§11.6、§12 中的寄宿差异 | D3 选独立进程 |
| §7.14 改为「将来接摘要（存档）」，保留 v0.2 对乙、丙的技术评估，两者 v1 均未采用、接法将来另行拍板；§11.1 C2 关闭；§11.8、§12 同步 | D4 选甲 |
| §2.2、§2.3 删去「v1 只做封锁和解封」备选和「D2 否决则本期不建服务」 | D2 选 GM 处置，含挂欠款 |
| §11.2 第 0 批改为本批范围；§11 开头写明第 1 批前报文件清单 | 用户授权第 0 批定稿 |
| §13 拆为 13.1 已拍板、13.2 待确认、13.3 已否决 | D5–D14 尚未拍板，保持推荐口径 |
| 定稿评审（decisions / no-loss）修正：§1.4 补 `DataServiceRpc`、deadline 门禁与前缀表的具体位置、生成器挂载规则、独立进程代价；手写文件总数与分批对齐为约 121（第 1 批补 D9 的 `player_debt` 弃用注释），生成约 110；字段号定为围栏 18/19、收件箱 20；reliable 文档引用改为修订后行号与 §5.4；PARITY 模板标 D13 待确认 | 两份评审核实成立 |
| 附录 A、B、C 与评审记录不改 | 历史记录。附录 A、B 的甲 / 乙 / 丙指 v0.1 候选方案，与 D4 的甲 / 乙 / 丙无关；v1.0 的骨架来自 v0.1 的丙。评审记录中「乙、丙的代价写在 §7.14」「乙方案需要新增内部入口（C2）」「如果要接，推荐乙」，以及寄宿形态、「D4 推荐」等表述，均为 v0.2 时的状态，v1.0 起以 §7.14、§13.1、§13.3 为准 |

---

## 评审记录

### consistency 评审

| # | 意见 | 处置 | 理由 |
|---|---|---|---|
| 1 | Redis owner_epoch 倒退会永久锁死收件箱，ResetFence 归零与围栏文档相反 | **采纳** | 核实 `db-write-fencing-mysql.md` §9.1（v0.3）确为「不归零、epoch-raise」，`:494-511` 是 save_seq 节。已删除 ResetFence，收件箱高水位纳入 epoch-raise 取最大值（C4），加 STALE 的 `fence_regressed` 分类告警；「只告警不拒绝」列为 D5 备选 |
| 2 | UNKNOWN 永远判不了 durable，流会永久卡住 | **采纳** | 逻辑成立。UNKNOWN 改为只要求快照 `decided_through ≥ seq`；协议异常结果改为 halt，避免空转（§7.7） |
| 3 | 回放只比 seq，恢复后 CursorAhead 可被绕过 | **采纳问题，换一种修法** | 回放核对 request_id 和类型，Ack 带 request_id（IB-9）已采纳。技术闸没有采用 RebaseAfterRestore 闸门，改为 seq 带时间下界，加 CursorAhead 时抬号，不依赖人工步骤，也删掉了 `stream_epoch` |
| 4 | 毫秒 CAS 有 ABA；30 分钟兜底只在单个会话内计时 | **采纳** | CAS 改用 next_seq；实体在本节点新建或 epoch 变化时无条件拉一次 |
| 5 | 判重可能被 FULL 或时间窗抢先；issued_at 不在幂等摘要里 | **采纳（部分）** | 锁内复读唯一键，排在满箱判定之前；issued_at 纳入内容摘要，并写成生产方契约。「从 request_id 解出时间」未采用：它要求运维工具按 SnowFlake 发号，牵涉节点隔离不变量 |
| 6 | RequeueItem 打乱严格顺序 | **采纳** | v1 删除 RequeueItem，改主意就用新的 request_id 重新下处置 |
| 7 | 未知类型被记成终态 REJECTED | **采纳** | 未知类型停在队头、halt、告警；只有认识的类型解码失败才记 REJECTED |
| 8 | correlationId 按实体计数，挡不住前一个实体的回包 | **采纳** | 改为节点级 thread_local 单调计数器 |

### repo-fit 评审

| # | 意见 | 处置 | 理由 |
|---|---|---|---|
| 1 | 字段 18 与已授权的围栏设计撞号，漏了放行清单 | **采纳** | 核实 `db-write-fencing-mysql.md:3,113,116`。字段号改为落码时的下一个空闲号（C4）；第 1 批补放行清单和 `migrate plan` 断言 |
| 2 | 追加 SyncDomain 就是改客户端契约，没有客户端和 Java 同批 | **采纳** | 核实 `player_scene.proto:136` 和 `player_landing.h:57-58`。D4 推荐不接摘要；乙（内部字段）和丙（SyncDomain）的代价写在 §7.14；Java 的 ContractSync 写进第 1 批 |
| 3 | 独立进程的代价被高估，且没有分批清单 | **采纳（部分）** | 「不需要发件箱」「不触发 D-13」成立，已修正对比表并补两套清单，推荐也改为独立进程。「可以沿用全局库」不成立：D-14 要求新服务独占库、不借 `mmorpg_global`（`xuanming-port-decisions-20260910.md:340-345`） |
| 4 | playersync 已落码，接口和 Redis 句柄口径都不符 | **采纳** | 已按 `digest.go:20-22,111,133` 更正事实。D4 甲方案不用 Bumper；乙方案需要新增内部入口（C2）。核心 Redis 句柄在两种形态下都要新增配置 |
| 5 | `DefaultTxRetryConfig` 不注入判定就一次都不重试 | **采纳** | 核实 `seq.go:266-267`，以及 data_service 现有判定不含 9007（`id_segment_store.go:243-249`）。改为显式注入并加单测 |
| 6 | `notify_due_ms` 做 CAS 令牌有 ABA | **采纳** | 同 consistency #4 |
| 7 | 「不会悄悄发生」没有机制撑住 | **采纳** | 新增每小时对账作业（领导者执行）和告警，失败矩阵 #15 的结论改为「可发现」 |
| 8 | 没有覆盖玩家不存在和删号 | **采纳（部分）** | 入箱前做存在校验，新增 `kPlayerInboxPlayerNotFound`。删号部分未落码：核实 `DeletePlayerData` 只删 data_service 的遗留键、没有生产调用方，仓库目前没有删号流程；已写入后续候选，届时同批作废 |
| 9 | ADVANCE 没有上界，Fetch / Ack 不鉴权 | **采纳** | 加 Redis 上界和 EPOCH_AHEAD；威胁模型写进 §1.6；是否签名列为 D6 |
| 10 | 提前写入了 v1 没有调用方的字段和枚举，提示值进了持久记录 | **采纳** | 删除 merge_key、expires_at_ms、replaces_seq、GO_SERVICE、ROLLBACK_REPLAY/DROP、deferred_*、spec.enabled、drained_digest；开关各只有一个来源。分级枚举只保留 DURABLE，三级策略写在 §3.1 |
| 11 | 审计同事务和复用 authorizeAdmin 都没有现成接口 | **采纳** | 核实 `snapshot_store.go:839` 和 `dataserviceserver.go:189`。寄宿形态新增 `InsertAuditLogTx`，并抽出参数化的鉴权函数；独立形态用自己的审计表和鉴权 |
| 12 | 战斗中只能靠退避等待 | **采纳** | 核实 `player_battle.cpp:167-181` 已有解冻挂点；C1 加一行重查 |

### ops 评审

| # | 意见 | 处置 | 理由 |
|---|---|---|---|
| 1 | Fetch / Ack 不认证调用方，ADVANCE 没有上界 | **采纳（部分）** | Redis 上界、request_id 核对、威胁模型和告警已采纳。「第 1 批就做 HMAC」改为 D6 交用户拍板，推荐不做：核心 Redis 无 AUTH、`DeletePlayerData` 也无鉴权，签名不能抬高集群内的实际防线 |
| 2 | GM 复用维护窗口口令，operator 无法归责 | **采纳（部分）** | 改为独立的 `PlayerInbox.AdminToken`，经 Secret 常驻，由 k8s_deploy 渲染并加契约测试；审计记录对端地址。「按凭据映射出 operator」推迟到 GM 后台接入：单一共享口令无法区分具体的人 |
| 3 | 降级会丢新字段，导致欠款重复 | **采纳** | 先扩展后收缩：`consumer_enabled=false` 的版本就是回滚下限；破坏性降级的前置步骤写进 runbook（§8 #21、§11.7） |
| 4 | 负载按 1 万在线估算，60 秒轮询压在核心 Redis 上 | **采纳（部分）** | 去掉 60 秒轮询，Kafka 提醒提前到 v1，负载按 S1 / S3 重算。兜底默认保留 30 分钟而不是默认关闭：Kafka 是至多一次，长期在线的玩家需要兜底；而它落在不加锁的点读上，不压核心 Redis |
| 5 | ResetFence 归零与已定稿的处置相反 | **采纳** | 同 consistency #1 |
| 6 | Fetch 对不存在的行也加锁，索引没有预切，摘要读失败会放大成风暴 | **采纳** | 加不加锁的快路径；第 4 批做索引预切。v1 不读摘要，「摘要读失败」这一项不再存在 |
| 7 | 「共用现有连接池」不成立 | **采纳** | 核实 `metrics.go:467` 和 `config.go:300`。改为独立池加 1 秒锁等待，并补池指标、总账和契约断言 |
| 8 | 灰度单位和路由对不上，scene 没有开关，Disabled 回包没有定义 | **采纳（部分）** | 加 `consumer_enabled`；Fetch / Ack 不设开关；UNIMPLEMENTED 长退避，不计故障。入箱开关不放全局库或 etcd：它短时不一致只会让 GM 重试，上线时按 §11.7 一次性全开 |
| 9 | 把收件箱域放进 SyncDomain 是 fail-open | **采纳** | D4 推荐不接摘要；如果要接，推荐乙（内部字段名，天然不下发） |
| 10 | 30 天告警常亮，gauge 重复上报，没有存在校验，没有批量接口 | **采纳（部分）** | 告警改为「已投递未确认」，离线积压只进看板；gauge 由领导者产出、按 max 聚合；加存在校验；告警的安装步骤登记进 README。批量接口没有新增，改为写明调用侧限速和重试规范，触发条件写进第 5 批 |
| 11 | 毫秒 CAS 有 ABA | **采纳** | 同 consistency #4 |
| 12 | 字符串长度只写在注释里，覆盖 gm_operator 与大小校验的先后没有定义 | **采纳** | 显式限长；先覆盖 gm_operator，再校验最终载荷；审计字段同样限长（§3.4） |
