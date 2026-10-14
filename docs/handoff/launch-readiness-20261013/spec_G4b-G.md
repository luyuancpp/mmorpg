# G4b-G 实现规格：登录排队在途占位逐个到期（新键 ZSET + 交票后保留 15s）

> 状态：**规格，未落码、未编译、未测试**（AGENTS.md §10.1，待 Codex 验证）；负责人 2026-10-13 拍板走方案 A（`spec_G4b.md` §10 #11）。
> 来由：`spec_G4b.md` §1.5（基线缺陷 Q1 / Q2）、R15、§10 #11、§11、§12；G1b 规格 §4.7 第 2、3 条。
> **行号以 HEAD（`0aa1027bc`）为准**，G1b 已随 `d5cbb8a8e` 入库。工作区是几个会话共用的，评审这一轮期间它一直在变，所以下面的基线只是**快照（2026-10-13 09:07，只读重核）**：
> - 本批要改的 7 个代码 / 配置文件里，6 个（`loginqueue/` 下的 `queue.go`、`capacity.go`、`metrics.go`、`queue_test.go`、`dispatcher_test.go`，以及 `config/config.go`）没有未提交改动，与 HEAD 一致。两份排队文档、拆 Redis 设计、`spec_G4b.md` 同样没有。
> - **`etc/login.yaml` 已被别的会话改了别处的注释**（`Locker`、`NewPlayerSeed`、`PreloadPool` 三段，净增 9 行）。排队那一段内容没变，行号整体 +9：上线清单第 0 步 HEAD `:436-438` → 工作区 `:445-447`，`Queue:` 上方那 6 行注释 HEAD `:445-450` → `:454-459`，`Queue:` 本身 HEAD `:451` → `:460`。**本批对 `login.yaml` 的两处改动按内容锚点定位，叠在别人的改动上，不覆盖、不还原**（§4.6）。
> - **`go/login` 下别的会话的其它未提交改动，不属于本批，不回退、不整理、不并入本批的 diff**：`go.mod` 的 `go` 指令 `1.24.5 → 1.27.2`（仓库里所有 Go 模块的 `go.mod`、三个 Dockerfile、README 是同一批）；`svc/servicecontext.go`（改动在 `:654` 之后，本规格引用的行都在它之前，行号不受影响）；`config/new_player_seed_conf_test.go`；以及 `login.go`、`kafka/`、`clientplayerlogin/`、`callerauth/`、`dataloader/` 下的若干文件（09:07 时共 14 个已跟踪文件、1 个未跟踪文件；04:41 时只有 `go.mod` 一个）。`loginqueue/` 与 `config/config.go` 不在其中。
> - 落码前由主会话重新记一次快照（§8.0），Codex 的判据以那份快照为准；这些改动是谁的，落码前由主会话向用户确认。
> 合入顺序：G1b（已落）→ **G4b-G** → G4b-2。本批不打开排队，不改客户端契约（回包码、proto、tip 都不动）。
> 修订：2026-10-13 三路对抗式评审后修订一轮，逐条处理记录见 §15。

---

## 0. 结论速览

- **做什么**：登录排队的「在途占位」从 `admitted:zone:{z}`（SET，整键滑动 TTL）换成新键 `admitted:zset:zone:{z}`（ZSET，分数 = 占位到期的 Unix 毫秒）。每个成员按自己的分数到期，计数前先清掉过期成员。客户端取走放行槽时不再撤占位，而是把它改成「现在 + 15s」到期。
  - 修 Q1：快速通道的 `fast:<uuid>` 占位以前永远不会被单独删除，持续有人登录时余量被重复扣到 0。
  - 修 Q2：取走即撤占位，而 gate 的人数要等下一次上报才涨，开区时每拍都按「在线 0、在途 0」再放一满批。
- **本规格在提纲之外自己定的四件事**（提纲没写，落码必须有结论；其中前三件列入 §11 #1–#3 待拍板，都给了默认做法；§11 #4、#5 是评审后补的两条）：
  1. **时间来源**：login 进程时钟，经 ARGV 传进脚本，`Queue` 上加一个可注入的 `now`。不用 Redis `TIME`（§4.2）。
  2. **快速通道占位的寿命**：与「取走后」相同，取 15s，不取 `AdmitTTL`（60s）。快速通道是当场交票，等同已取走（§5.1、§11 #2）。
  3. **交票后宽限**：代码常量 15s，不做成配置。C++ gate 实际每 **10 秒**上报一次人数（不是「每几秒」），15s 只留 5s 余量（§4.3、§11 #1）。
  4. **整键 TTL 保留**，但只做垃圾回收：每次写占位都 `PEXPIRE` 到 `max(AdmitTTL, 15s) + 1min`。成员是否有效只看分数（§4.1）。
- **不做**：不动 `Lookup` 的「取走后再轮询回排队中、名次 0」；不把 `PopAdmit` 改成单段 Lua；不新增配置键、指标、proto 字段（§5.8、§9）。
- **规模**：Go 6 个文件（`queue.go` 是唯一的行为改动；`capacity.go`、`metrics.go`、`config.go` 只改注释和帮助文本；两个测试文件，新增 14 个用例）+ `login.yaml` 两段注释；文档 7 处由 Docs 阶段同步。本批不改 `go.mod` / `go.sum`（`go.mod` 上已有别的会话的 `go` 指令改动，本批不碰它），不改 C++、Java、PowerShell。
- **Java 版**：已有同样的两点（Java 仓库 `LoginQueue.java`），本改动做完这两点即对齐，Java 代码不用改；`PARITY.md`「登录排队」行（2026-10-13 是第 **99** 行，不是 `spec_G4b.md` 写的 98）的措辞要改，原文与改法见 §12。本规格以下一律按行名引用这一行，不再写行号。

---

## 1. 现状核对（事实 + 文件:行号）

路径前缀 `go/login/internal/logic/pkg/loginqueue/` 省略。

| # | 事实 | 出处 |
|---|---|---|
| F1 | 在途占位键 `admitted:zone:%d`，类型 SET。全仓只有 `queue.go` 读写它，另有一处测试直接读；Go 以外（脚本、部署清单、C++、Java gateway、robot、合服工具）零引用 | `queue.go:139`、`:355`、`:430-431`、`:444`、`:475`；`dispatcher_test.go:411`；全仓 grep `admitted:` |
| F2 | 放行：每个条目一个 MULTI：`SET admit:{id} EX AdmitTTL` + `SADD` + `EXPIRE 整集合 AdmitTTL`。调用方只有 dispatcher | `queue.go:428-434`；`dispatcher.go:306` |
| F3 | 快速通道：Lua `SCARD >= budget` 返回 0，否则 `SADD fast:<uuid>` + `EXPIRE 整集合`。TTL 以整秒传入。`budget == 0` 不碰 Redis | `queue.go:456-462`、`:468-482`；调用点 `loginpregate/assigngatelogic.go:115-119` |
| F4 | 全仓唯一的 SREM 在 consume，只删 queueId，且只在 meta 里读到 `zone_id != 0` 时执行；错误被忽略。`fast:<uuid>` 从不被单独删除 | `queue.go:339-360` |
| F5 | consume 顺序：`GETDEL admit:{id}`（真正的消费点，只成功一次）→ `signFn` 现签 gate 票 → `HMGET meta` → SREM。`signFn` 失败时在 SREM 之前返回，放行槽已删、占位残留 | `queue.go:300-365`（`:304`、`:321-330`、`:339`、`:355`） |
| F6 | 取走后再轮询：meta 与 token 索引还在、ZRANK 落空，回「排队中、名次 0」，直到 1h 过期（`querystatuslogic.go:18-27` 的注释写的是 EXPIRED，与代码不符） | `queue.go:275-280`；`queue_test.go:154-163` |
| F7 | 计数 `AdmittedCount` = `SCARD`（只读）。调用方两个：`FreeSlots` 和 gauge 快照。dispatcher 一拍调两次，每次非重入的 assign-gate 调一次 | `queue.go:443-449`；`capacity.go:89`；`metrics.go:204`；`dispatcher.go:263`、`:273`；`assigngatelogic.go:95` |
| F8 | `free = capacity − (online + admitted)`，不足为 0；`online = Σ PlayerCount`；候选为空直接报错 | `capacity.go:67-100` |
| F9 | `online` 来自 etcd：login 每次算容量都直接对 etcd 做前缀 Get，**没有缓存** | `svc/servicecontext.go:292-298`；`pkg/node/watcher.go:27-51` |
| F10 | **C++ gate 每 10 秒上报一次 `player_count`**，取的是会话表大小；会话在 TCP 连接建立时就进表（含尚未验票的）。`config.go` 与 `login.yaml` 注释里的 "every few seconds" 不准确 | `cpp/nodes/gate/main.cpp:407-412`；`cpp/nodes/gate/handler/rpc/client_message_processor.cpp:654`；`config/config.go:255-256`；`etc/login.yaml` HEAD `:445-447`（工作区 09:07 快照 `:454-456`，见文件头） |
| F11 | gate 认票是无状态的（HMAC + node_id + zone + 过期时刻），不查 Redis；连上后没有任何回调通知 login。gate 票有效期 10 分钟。所以占位只能靠时间回收 | `client_message_processor.cpp:1029-1074`（摸底给出，本规格未重读）；`assigngatelogic.go:27` |
| F12 | `Queue` 没有时钟字段，时间全部直接 `time.Now()`；包内没有用 Redis `TIME`。同包显式注入时间的先例是 `nowUnix` 参数 | `queue.go:115-120`、`:164`、`:169`、`:357`；`gatedrain.go:40` |
| F13 | 排队在所有环境都是关的：`login.yaml` 的 `Queue.Enabled: false`；K8s 的 login ConfigMap 不写 `Queue` 段（`k8s_deploy.ps1` 里 grep `LoginQueue` / `Queue:` / `DispatcherLockKey` 零命中） | `etc/login.yaml` HEAD `:451-452`（工作区快照 `:460-461`）；`config/config.go:271` |
| F14 | 指标 `login_queue_admitted_inflight` 的帮助文本写死 `SCARD admitted:zone:{zone}` 和「Sustained > 100」。仓库里没有任何告警规则或面板用到它 | `metrics.go:62-73`；全仓 grep `admitted_inflight` |
| F15 | `TryReserveFastPathSlot` 没有任何单测；`loginpregate` 目录没有测试文件。`PROGRESS.md` 2026-08-10 那条曾建议补并发占位单测 | `docs/PROGRESS.md:2472-2474` |
| F16 | login 的 go-redis 客户端用缺省选项（缺省 `MaxRetries` 为 3，超时会重发命令，包括 EVAL 与 GETDEL） | `svc/servicecontext.go:111-116`；go-redis v9.16.0（login 的 `go.mod` 所用版本，本机模块缓存）`options.go:331-336`：`MaxRetries` 为 0 时取 3、为 −1 时取 0 |

**两条缺陷的成因**（与 `spec_G4b.md` §1.5 一致，逐行核过）：

- **Q1**：占位只有「整键 TTL」一种回收方式，而每次 SADD 都把它续上（F2、F3）。已经连上 gate 的快速通道玩家同时算进 `online` 和 `admitted`，余量被重复扣减；扣到 0 后不再有 SADD，要再等最长 60s 整键过期，才放下一批。
- **Q2**：取走即 SREM（F4），而 gate 人数最多滞后 10s（F10）。这段时间这个人既不在 `admitted` 也不在 `online`，下一拍多放一批。

---

## 2. 目标行为

一个占位成员的一生（`G` = 交票后宽限 15s，`now` = login 进程时钟的 Unix 毫秒）：

| 路径 | 成员 | 写入时的分数（到期毫秒） | 之后 | 何时消失 |
|---|---|---|---|---|
| dispatcher 放行 | `queueId` | `now + AdmitTTL` | 客户端轮询取走时改成 `now + G` | 没人来取：放行后 `AdmitTTL` 到点；取走：取走后 `G` 到点 |
| 快速通道占位 | `fast:<uuid>` | `now + G` | 不再改 | 占位后 `G` 到点 |

- 「到点」之后，成员在**下一次计数或占位**时被 `ZREMRANGEBYSCORE -inf now` 清掉（分数 ≤ now 即过期）。没有后台清理任务。
- 计数 = 清掉过期成员之后的 `ZCARD`。`FreeSlots` 的公式不变：`free = max(0, capacity − online − 在途占位数)`。
- 旧键 `admitted:zone:{z}` 新版本不读、不写、不删。

---

## 3. 文件清单

| 类型 | 文件 | 要点 |
|---|---|---|
| 修改（行为） | `go/login/internal/logic/pkg/loginqueue/queue.go` | 键 helper 改名换键；`Queue` 加 `handoffGrace`、`now` 两个未导出字段，`New` 签名不变；两段 Lua（计数、快速通道）；`PopAdmit` 的 MULTI 改 ZADD + PEXPIRE；consume 由 SREM 改为改分数；包头与函数注释 |
| 修改（注释） | `go/login/internal/logic/pkg/loginqueue/capacity.go` | `:14-21`、`:32`、`:49` 三处说明（`SCARD admitted:zone:{zone}`、`admittedSetKey` → 清过期后的在途占位数）；行数不变 |
| 修改（帮助文本 + 注释） | `go/login/internal/logic/pkg/loginqueue/metrics.go` | `:10`、`:62-64`、`:69-71`、`:192-196`。指标名与标签不变 |
| 修改（注释） | `go/login/internal/config/config.go` | 只改 `QueueConf` 类型注释 `:255-259` 共 5 行，行数不变；字段行不动 |
| 修改（注释） | `go/login/etc/login.yaml` | 只改两段注释：上线清单第 0 步（HEAD `:436-438`，3 行换 3 行）与 `Queue:` 上方那段（HEAD `:445-450`，6 行换 6 行）；行数不变，值不动。该文件已有别的会话的未提交改动（别处注释，行号 +9），按内容锚点定位，叠在其上（§4.6） |
| 修改（测试） | `go/login/internal/logic/pkg/loginqueue/queue_test.go` | 新增假时钟与 13 个用例（T1–T11、T13、T14，§6） |
| 修改（测试） | `go/login/internal/logic/pkg/loginqueue/dispatcher_test.go` | fixture 把宽限设 0；`:411` 改读新键；新增 1 个用例（T12，§6.4） |
| 文档阶段同步 | `docs/design/login-queue-2026-05.md`、`docs/ops/login-queue-stress-runbook.md`、`docs/design/shared-redis-split-by-purpose.md`（只改两处）、`docs/handoff/launch-readiness-20261013/spec_G4b.md`（方案 A 落定、R15、PARITY 行号、用例 15、合入门槛）、`docs/design/capacity-model-hok-scale.md`（只核对行号）、`docs/PROGRESS.md`（末尾追加）、Java 仓库 `PARITY.md`「登录排队」行 | 逐条见 §7 |
| 不改 | `dispatcher.go`、`gatetoken.go`、`loginpregate/*.go`、`svc/servicecontext.go`、`svc/login_queue_wiring_test.go` | `AdmittedCount` / `TryReserveFastPathSlot` / `PopAdmit` / `Lookup` / `New` 的签名都不变；`dispatcher.go` 与 `assigngatelogic.go` 里没有写死 SCARD / SADD 的注释 |
| 不改 | `tools/merge_zone/`、`tools/scripts/k8s_zone_rollback.ps1`、三个压测清库脚本、`k8s_deploy.ps1`、`deploy/`、robot、`java/gateway_node`、CI、`docs/README.md` | 逐项理由见 §7.3 |

行为改动只有 `queue.go` 一个文件。没有 proto、生成产物、C++、Java、PowerShell 改动。

---

## 4. 详细设计（照写）

### 4.1 键与数据结构

| 项 | 结论 |
|---|---|
| 键名 | `admitted:zset:zone:{zoneId}`。保留 `admitted:` 前缀，拆 Redis 设计里的通配 `admitted:*`（`shared-redis-split-by-purpose.md:593`、`:718`）继续覆盖它 |
| 类型 | ZSET |
| 成员 | 放行：`queueId`（UUID 字符串，`queue.go:163`）；快速通道：`fast:<uuid>` |
| 分数 | 占位到期时刻，Unix **毫秒**，整数（约 1.8×10¹²，远小于 2⁵³，double 精确表示） |
| 有效判据 | `score > now`。清理是闭区间：`ZREMRANGEBYSCORE key -inf now` 删掉 `score ≤ now` 的成员 |
| 整键 TTL | 保留，但**只做垃圾回收**。每次写占位都 `PEXPIRE key W`，`W = max(AdmitTTL, G) + 1min`（默认 120000 ms） |
| 为什么用新键名 | 新旧版本混跑时不会对同一个键做不同类型的操作（`WRONGTYPE`）。K8s 上排队从没开过，没有存量要迁移（F13） |

**整键 TTL 为什么还要**：成员逐个到期靠「读的时候清」。zone 下线或被合走之后没有 login 再来读，剩下的成员没人清，键永久残留。有了整键 TTL，最后一次写入后 `W` 之内整键消失。

**为什么可以无条件 `PEXPIRE W`**（Java 版是 `pttl < X 才 pexpire X`）：所有写入用同一个 `W`，而任何成员写入时的剩余寿命都 ≤ `max(AdmitTTL, G) < W`。后一次写入把整键到期时刻推到「那一刻 + W」，不会早于此前任何成员的到期时刻，所以不存在「把 TTL 缩短到某个成员之前」的情况。少一次 `PTTL`，脚本里也没有分支。

**多留的 1 分钟**（常量 `admittedKeyGCSlack`）：分数用 login 时钟，整键 TTL 用 Redis 时钟，两者有偏差时不能让整键先于成员消失；同时保证 `PEXPIRE` 的参数恒 ≥ 60000，不会因为配置了亚毫秒的 `AdmitTTL` 变成 `PEXPIRE key 0`（那等于删键）。

**空键**：最后一个成员被清掉后，空的 ZSET 会被自动删除。miniredis v2.35.0 已对源码核实（`db.go:506-515` 的 `ssetRem`：删掉最后一个成员即删键）；真实 Redis 是同样的语义，凭记忆，**未核实**。即使不删，整键 TTL 也会收走。测试不断言「键不存在」，只断言计数。

### 4.2 时间来源：login 进程时钟，经 ARGV 传入

**选择**：分数和「现在」都取 login 进程的墙钟（`time.Now().UnixMilli()`），由 Go 算好后作为 ARGV 传进脚本。`Queue` 加一个未导出字段 `now func() time.Time`，生产是 `time.Now`，同包测试换成可拨的假时钟。

| | login 时钟经 ARGV（**选它**） | Lua 里 `redis.call('TIME')` |
|---|---|---|
| 多副本时钟偏差 δ | 占位寿命误差在 [−δ, +δ]：钟快的副本来读，别人写的占位早 δ 被清；钟快的副本写的占位，别人晚 δ 才清 | 没有，只有一个时钟 |
| 测试怎么控时间 | 直接拨假时钟，不 sleep，不依赖 miniredis 的任何时间行为 | 靠 `mr.SetTime`。miniredis v2.35.0 可行，已对源码核实：`TIME` 取 `m.effectiveNow()`（`cmd_server.go:167-176`），`SetTime` 改的就是它（`miniredis.go:446-450`、`:673-678`）；Lua 的 `redis.call` 把命令原样交给 `srv.Dispatch`，对已注册命令一视同仁（`lua.go:57-60`）。不需要升级 miniredis |
| 脚本里的时间运算 | 没有：Go 传进来的十进制字符串原样交给 `ZREMRANGEBYSCORE` / `ZADD` | 要在 Lua 里算毫秒并 `string.format("%.0f")`（`go/match/internal/team/scripts.go:61-63`） |
| Redis 侧前提 | 无。脚本的写入全部由 ARGV 决定 | 要求 Redis 允许在非确定性命令之后写。Redis 7 默认 effects replication 时允许（`go/match/internal/team/scripts.go:8-10` 的说法；部署的是 `redis:7.2`，`deploy/k8s/manifests/infra/redis.yaml:83`），凭记忆，**未核实** |
| 与 Java 版 | 相同（`LoginQueue.java:159`、`:206-209`、`:238`） | 不同 |
| 与本包既有做法 | 相同：排队令牌过期、入队分数、gate 票过期都用 login 墙钟（`queue.go:164`、`:169`；`gatetoken.go:118`、`:259`） | 本包没有先例 |

**理由**：占位是 15s / 60s 量级的软状态，不是资产数据。同一 zone 的 login 副本跑在做了 NTP 同步的节点上，偏差按毫秒到几十毫秒估（**假设，未测**），相对 15s 可以忽略。login 的墙钟本来就是这条链路的时间基准（gate 票和排队令牌的过期都靠它），没有引入新的依赖种类。换来的是测试完全确定、脚本里没有时间运算、与 Java 版和本包既有做法一致。（两种做法都不用改 `go.mod`，这一点不构成取舍。）

**代价（如实写）**。记某个副本的钟比别人快 `δ`（慢则方向相反），偏差达到宽限量级（≥ 15s）才有可见后果：

- **故障副本还在跑的时候**：它每次来读（它经手的每个 assign-gate、它当 leader 时的每一拍）都用自己的「现在」清理，别人写的、其实还有效的占位（分数 ≤ 它的钟）全部被清掉，退化成 Q2 的旧行为，**多放**。它自己写的占位，由它自己读时按自己的钟清，真实寿命仍是 15s / 60s；但在别的副本看来这些占位要到「真实时间 + δ」才到期。钟慢的副本反过来：它写的占位在别人眼里一写下就过期（不占名额，多放），它读时清不掉别人已到期的占位（由别的副本来清）。
- **故障副本恢复（NTP 校回）或被摘掉之后，不会立刻自愈**：它在最后 15s（快速通道、取走）/ 60s（放行未取）内写下的占位，分数停在「故障钟 + 15s / 60s」，要再过 **δ** 才到期。这段时间里该 zone 的有效容量少掉这些占位的个数，上界是它那一刻能写进去的 `C − O`。旧实现没有「占位按某个副本的钟到期」这回事，这是本改动**新增**的失败模式。
- **整键 TTL 只在一种情况下兜底**：`ZREMRANGEBYSCORE` 只清 `-inf … now` 这一个方向，分数远在未来的成员没人清；而每次写占位都把整键 TTL 续到 `W`。所以只要这个 zone 还有名额在流转（有写入），这些成员就留满 `δ`。只有名额被它们占满、整个 zone 完全停放时才没有任何写入（计数脚本不续 TTL；`free == 0` 时 assign-gate 不调占位脚本，`assigngatelogic.go:109-110`，dispatcher 不放行，`dispatcher.go:278-280`；此前已放行的条目最迟 `AdmitTTL` 内取完），整键在最后一次写入后 `W` 过期，即完全停放最长约 `AdmitTTL + W`（默认 3 分钟）后自愈。
- **止血命令**：`redis-cli DEL admitted:zset:zone:<z>`。代价是在途占位清零，最多多放一批 `C − O`（与「占位键被淘汰或误删」同一行，§5.3）。判断方法：`redis-cli ZRANGE admitted:zset:zone:<z> 0 -1 WITHSCORES` 里有分数比当前毫秒时间戳大出 `max(AdmitTTL, 15s)` 以上的成员，就是这种残留。写进 runbook 模式 2（§7.2）。
- 这种副本签出的 gate 票过期时刻也同样是错的（`gatetoken.go:118`），属于同一个运维故障；但「恢复后还要再拖 δ」是占位独有的。是否在脚本里再加一道上界清理把它压到分钟级，见 §11 #4（默认不加）。
- 放行槽 `admit:{id}` 的 TTL 走 Redis 时钟，占位分数走 login 时钟，两种时钟混用。偏差导致「放行槽还在、占位已被清」时，取走那一步用**普通 ZADD**（不是 `XX`）把占位补回来，见 §4.4 第 5 步。

**退路**：L-14 压测或线上发现时钟偏差不可忽略时，改用 Redis `TIME`。改动面只有 §4.4 的两段 Lua 开头加两行取时间、`holdAdmitted` 改成 Lua、测试改用 `mr.SetTime`；键、成员、分数含义都不变，新旧两种写法可以混跑（分数都是 Unix 毫秒）。miniredis v2.35.0 支持（上表），不用动 `go.mod`；剩下没核实的只有真实 Redis 一侧（「`TIME` 之后允许写」）。

### 4.3 交票后宽限：常量 15s

- **是常量，不是配置**。`defaultHandoffGrace = 15 * time.Second`，写在 `queue.go`。理由：它由「gate 上报周期 + 建连时间」决定，不是运维该调的旋钮；Java 版同样是常量（`LoginQueue.java:37`）；不新增配置键，G4b-2 的 `New-LoginQueueYaml`（`spec_G4b.md` §4.5）与 `login_queue_wiring_test.go:16-26` 的结构体字面量都不用动。
- **`Queue.handoffGrace` 字段只为同包测试而设**：既有的两个集成用例靠「取走即腾出名额」推进（§6.4），要能把宽限设成 0。`New` 不收这个参数，生产路径拿不到别的值。
- **依据**：票交出去之后，这个人要等「连上 gate（`c`）→ gate 下一次上报（周期 `T` = 10s，F10）→ login 读 etcd（无缓存，F9）」才出现在 `online` 里，最坏滞后 `c + T`。15s = 一个上报周期 + 5s 建连与传播余量。Java 版的 15s 对应的是它自己 5s 的发布周期（`GateNode.java:79`），余量 10s；本仓库照搬 15s，余量只有一半。是否调到 20s 见 §11 #1。数值**待压测确认**（L-14 要测「交票 → gate 会话建立」的分布）。
- **与 gate 上报周期没有机械关联**：`cpp/nodes/gate/main.cpp:408` 的 `10.0` 改了，这里不会报错。常量注释里写明出处，`TestNew_DefaultsHandoffGraceAndClock` 把 15s 钉住（改它必须有意识地改测试与文档）。登记为风险 R4。

### 4.4 `loginqueue/queue.go`

**import**：在现有 import 基础上加 `"github.com/zeromicro/go-zero/core/logx"`（go-zero 已是依赖，`dispatcher.go:51` 在用）。`uuid`、`json`、`errors`、`fmt`、`time`、`context` 继续使用。

**1）包头注释**（只改 `:15` 一行和 `:23-24` 两行，其余原样保留）：

```go
//	admitted:zset:zone:{zoneId} ZSET member=queueId | fast:<uuid>, score=占位到期 unix ms
//	                           (在途占位:成员逐个到期,计数前清掉。G4b-G 之前是
//	                           SET admitted:zone:{zoneId} + 整键滑动 TTL,新版本不再读写旧键)
```

```go
//	Admit     → ZPOPMIN N → write admit:{queueId} + 占位(到期 = 现在 + AdmitTTL) (dispatcher)
//	Consume   → GETDEL admit:{queueId}, 签 gate 票, 占位改为「现在 + handoffGrace」到期(不撤)
//	FastPath  → 一段 Lua:清过期 + 计数 < budget + 占位(到期 = 现在 + handoffGrace)
```

**2）常量、结构体、构造函数**（替换 `:113-134`；常量放在 `type Queue` 之前）：

```go
// defaultHandoffGrace:gate 票交到客户端之后,在途占位再保留多久。
//
// 票一交出去(排队路径:客户端轮询取走放行槽;快速通道:assign-gate 当场回票),这个人还要
// 「连上 gate → gate 把人数报进 etcd」才出现在 online 里,这段时间只有占位替他占着名额。
// C++ gate 每 10 秒上报一次 player_count(cpp/nodes/gate/main.cpp 的 playerCountReportTimer),
// login 每次算容量都直接读 etcd(pkg/node/watcher.go FetchAllNodes,无缓存),最坏滞后 =
// 建连耗时 + 10s。15s = 一个上报周期 + 5s 建连与传播余量。改 gate 的上报周期必须同步改这里。
//
// 取小了:占位先于人数到期,空档里多放人(旧实现取走即撤占位,相当于 0s,开区时每拍都按
// 「在线 0」再放一满批)。取大了:人已进 online、占位还在,同一个人算两遍,少放人。
// 与 Java 版 LoginQueue.TAKEN_GRACE 同值(Java gate 每 5s 发布,余量更宽)。待压测确认(L-14)。
const defaultHandoffGrace = 15 * time.Second

// admittedKeyGCSlack:在途占位 ZSET 的整键 TTL 比最长成员寿命多留的时间。
// 成员是否有效只看分数;整键 TTL 只负责在本 zone 没有任何 login 再来清理时(下线 / 合服)
// 把键收走。多留一分钟,免得 login 时钟(分数)与 Redis 时钟(TTL)的偏差让整键先于成员消失。
const admittedKeyGCSlack = time.Minute

// Queue wraps Redis ZSET operations behind a small typed surface. The zero
// value is unusable; construct via New.
type Queue struct {
	rdb        *redis.Client
	entryTTL   time.Duration
	admitTTL   time.Duration
	hmacSecret []byte // signs queue tokens; same secret as gate tokens is fine

	// handoffGrace:gate 票交到客户端之后占位再留多久。生产恒为 defaultHandoffGrace;
	// 只有同包测试会改它(New 不收这个参数)。
	handoffGrace time.Duration
	// now 是在途占位记账用的时钟:占位的到期分数、清过期成员时的「现在」都读它。
	// 生产是 time.Now;同包测试换成可拨的假时钟。排队令牌、入队分数等其它时间不走它。
	now func() time.Time
}

// New constructs a Queue. hmacSecret is used to sign opaque queue tokens —
// reusing the gate token secret is acceptable because the queue token is
// useless to a gate (different message shape) and useless to login without
// a matching queueId entry, so the threat model is just "client tampering",
// which HMAC over (queueId|expire) defeats.
func New(rdb *redis.Client, entryTTL, admitTTL time.Duration, hmacSecret []byte) *Queue {
	return &Queue{
		rdb:          rdb,
		entryTTL:     entryTTL,
		admitTTL:     admitTTL,
		hmacSecret:   hmacSecret,
		handoffGrace: defaultHandoffGrace,
		now:          time.Now,
	}
}
```

**3）键 helper**（替换 `:138-142`；`admittedSetKey` 删掉，残留引用会编译失败，正好逼着改干净）：

```go
func zoneZSetKey(zoneID uint32) string     { return fmt.Sprintf("queue:zone:%d", zoneID) }
func admittedZSetKey(zoneID uint32) string { return fmt.Sprintf("admitted:zset:zone:%d", zoneID) }
func admitKey(queueID string) string       { return "admit:" + queueID }
func metaKey(queueID string) string        { return "queue:meta:" + queueID }
func tokenIndexKey(token string) string    { return "queue:token:" + token }
```

**4）`Lookup` 里的一段注释**（替换 `:247-250` 四行注释，代码不动）：

```go
		// 条目的生命到此为止。meta 与 token 索引留到 TTL(见 consumeAdmitAndSign 末尾);
		// 在途占位不撤,consumeAdmitAndSign 把它改成再留 handoffGrace。
```

**5）`consumeAdmitAndSign` 的收尾**（替换 `:332-360`；`HMGET` 与两段解析原样保留，只换开头的注释和 `if zoneID != 0` 的内容）：

```go
	// 在途占位与等待时长:best-effort,失败不让这次取走作废(票已经签好)。
	//
	// We do HMGET (not HGET) to grab createdAt and zone_id in one RTT so the
	// wait-time histogram observation below pays no extra Redis cost. Missing
	// createdAt is non-fatal — the wait observation is skipped.
	fields, _ := q.rdb.HMGet(ctx, metaKey(queueID), "zone_id", "createdAt").Result()
	var (
		zoneID    uint32
		createdAt int64
	)
	if len(fields) >= 1 {
		if s, ok := fields[0].(string); ok {
			_, _ = fmt.Sscanf(s, "%d", &zoneID)
		}
	}
	if len(fields) >= 2 {
		if s, ok := fields[1].(string); ok {
			_, _ = fmt.Sscanf(s, "%d", &createdAt)
		}
	}
	if zoneID != 0 {
		// 不撤占位,改成再留 handoffGrace(G4b-G):票已交给客户端,但他连上 gate、gate 把人数
		// 报进 etcd 之前不在任何 gate 的 online 里。此刻撤掉,下一拍会按「在线没变、在途为 0」
		// 再放一批(旧实现在这里 SREM,开区时能放进几倍容量)。
		// 用普通 ZADD 而不是 XX:占位若已按分数清掉(login 与 Redis 的时钟差),这个人照样要占名额。
		// zone 读不到(meta 被淘汰)时走不到这里:占位保持放行时的分数,最迟 AdmitTTL 到期。
		pipe := q.rdb.TxPipeline()
		q.holdAdmitted(ctx, pipe, zoneID, queueID, q.handoffGrace)
		if _, err := pipe.Exec(ctx); err != nil {
			logx.Errorf("[loginqueue] hold admitted placeholder after consume zone=%d queueId=%s err=%v",
				zoneID, queueID, err)
		}
		if createdAt > 0 {
			waitMs := time.Now().UnixMilli() - createdAt
			recordWait(zoneID, time.Duration(waitMs)*time.Millisecond)
		}
	}
```

**6）`PopAdmit` 的 MULTI**（替换 `:428-434`）：

```go
		pipe := q.rdb.TxPipeline()
		pipe.Set(ctx, admitKey(queueID), blob, q.admitTTL)
		// 在途占位:没人来取时与放行槽同寿(AdmitTTL),按自己的分数到期;取走时改成再留 handoffGrace。
		q.holdAdmitted(ctx, pipe, zoneID, queueID, q.admitTTL)
		if _, err := pipe.Exec(ctx); err != nil {
			continue
		}
```

**7）计数、占位 helper、快速通道**（替换 `:440-482`）：

```go
// admittedKeyTTL 是在途占位 ZSET 的整键 TTL。只做垃圾回收,见 admittedKeyGCSlack。
func (q *Queue) admittedKeyTTL() time.Duration {
	return max(q.admitTTL, q.handoffGrace) + admittedKeyGCSlack
}

// holdAdmitted 把 member 在 zoneID 的在途占位记到「现在 + hold」到期(成员已在则覆盖分数),
// 并续上整键的垃圾回收 TTL。两条命令排进调用方的 pipeline,由调用方 Exec。
func (q *Queue) holdAdmitted(ctx context.Context, pipe redis.Pipeliner, zoneID uint32, member string, hold time.Duration) {
	key := admittedZSetKey(zoneID)
	pipe.ZAdd(ctx, key, redis.Z{Score: float64(q.now().Add(hold).UnixMilli()), Member: member})
	pipe.PExpire(ctx, key, q.admittedKeyTTL())
}

// admittedCountScript 清掉已到期的占位再计数,两步在同一段脚本里。
//
//	KEYS[1] = admitted:zset:zone:{zone}
//	ARGV[1] = now_ms
//	返回未到期的成员数
var admittedCountScript = redis.NewScript(`
redis.call('ZREMRANGEBYSCORE', KEYS[1], '-inf', ARGV[1])
return redis.call('ZCARD', KEYS[1])
`)

// AdmittedCount 返回 zone 当前的在途占位数:已放行还没来取的,加上票已交出去、还在
// handoffGrace 之内的(排队取走与快速通道)。FreeSlots 用它扣容量,避免上一批还没出现在
// gate 的人数里就再放一批。先清过期成员再数,所以它是一条**写命令**。
func (q *Queue) AdmittedCount(ctx context.Context, zoneID uint32) (uint32, error) {
	n, err := admittedCountScript.Run(ctx, q.rdb,
		[]string{admittedZSetKey(zoneID)}, q.now().UnixMilli()).Int64()
	if err != nil {
		return 0, err
	}
	return uint32(n), nil
}

// reserveFastPathSlotScript 原子占位:先清掉已到期的占位,未到期的不足 budget 才记一枚
// 一次性占位成员。budget = cap - online,即「在当前在线数之上还允许多少在途准入」。
// 清理、判定、占位在同一段脚本里,并发的快速通道调用彼此可见,不会各读同一个 free>0
// 快照后全部旁路队列签票。
//
//	KEYS[1] = admitted:zset:zone:{zone}
//	ARGV[1] = now_ms   ARGV[2] = budget   ARGV[3] = member(fast:<uuid>)
//	ARGV[4] = 占位到期毫秒   ARGV[5] = 整键 TTL 毫秒(只做垃圾回收)
//	返回 1 = 占到,0 = 已满
var reserveFastPathSlotScript = redis.NewScript(`
redis.call('ZREMRANGEBYSCORE', KEYS[1], '-inf', ARGV[1])
if redis.call('ZCARD', KEYS[1]) >= tonumber(ARGV[2]) then return 0 end
redis.call('ZADD', KEYS[1], ARGV[4], ARGV[3])
redis.call('PEXPIRE', KEYS[1], ARGV[5])
return 1
`)

// TryReserveFastPathSlot 尝试为快速通道原子占用一个准入名额。budget 传 cap-online。
// 返回 true 表示占用成功(调用方可直接签发 gate token);false 表示已满(应入队)。
// 快速通道当场交票,等同排队路径的「已取走」,所以占位只留 handoffGrace,不是 AdmitTTL;
// 之后它按自己的分数到期,不需要、也没有回收步骤。
func (q *Queue) TryReserveFastPathSlot(ctx context.Context, zoneID uint32, budget uint32) (bool, error) {
	if budget == 0 {
		return false, nil
	}
	now := q.now()
	member := "fast:" + uuid.NewString()
	res, err := reserveFastPathSlotScript.Run(
		ctx, q.rdb,
		[]string{admittedZSetKey(zoneID)},
		now.UnixMilli(), budget, member,
		now.Add(q.handoffGrace).UnixMilli(), q.admittedKeyTTL().Milliseconds(),
	).Int()
	if err != nil {
		return false, err
	}
	return res == 1, nil
}
```

`QueueLen`（`:484-491`）不动。

### 4.5 四处 Redis 操作的约定（汇总）

| 操作 | 形式 | KEYS / ARGV 或命令序列 | 原子范围 | 返回 | 键不存在 / 出错 |
|---|---|---|---|---|---|
| 计数 | Lua `admittedCountScript` | `KEYS[1]` 占位键；`ARGV[1]` now_ms | 清过期 + 计数 | 未到期成员数 | 键不存在回 0；Redis 出错回 error，调用方按 §5.3 处理 |
| 快速通道占位 | Lua `reserveFastPathSlotScript` | `KEYS[1]` 占位键；`ARGV` = now_ms、budget、member、到期毫秒、整键 TTL 毫秒 | 清过期 + 判定 `< budget` + 占位 + 续整键 TTL | 1 / 0 | 键不存在按 0 个占位处理；出错回 `(false, err)`；`budget == 0` 不碰 Redis |
| dispatcher 放行 | MULTI（`PopAdmit`） | `SET admit:{id} <slot>`（TTL = `AdmitTTL`，与现状同一行代码）；`ZADD 占位键 <now+AdmitTTL> <queueId>`；`PEXPIRE 占位键 W` | 三条一起成功或一起不执行（单条命令类型错误除外） | — | Exec 出错：`continue`，条目已弹出、未计入放行数（既有行为，§5.3） |
| 取走后续期 | MULTI（`consumeAdmitAndSign`） | `ZADD 占位键 <now+G> <queueId>`；`PEXPIRE 占位键 W` | 两条一起 | — | 出错只记一条 ERROR，这次取走照常成功（§5.3） |

放行和取走这两处是「只写、不用先读再决定」，MULTI 就够，与 `PopAdmit` 既有写法一致，不需要 Lua。只有计数和快速通道要先读后判，才用 Lua。

go-redis 把 `int64` / `uint32` 参数编码成十进制字符串，`float64` 分数用 `strconv.AppendFloat(…, 'f', -1, 64)`（go-redis v9.16.0 模块缓存 `internal/proto/writer.go:103`、`:131`、`:232-233`，已对源码核实），毫秒时间戳不会变成科学计数法；`queue.go:172` 的入队分数是同一种写法。脚本里只对 `budget` 做 `tonumber`，时间戳原样透传，没有 Lua 侧的数字格式化问题。

### 4.6 `capacity.go`、`metrics.go`、`config.go`、`login.yaml`（只改注释与帮助文本）

**`capacity.go:14-21`** 改为：

```go
// Free-slots calculation deliberately includes in-flight placeholders
// (`admitted:zset:zone:{zoneId}`, see Queue.AdmittedCount):
//
//	freeSlots = capacity - online - admitted
//
// Without the `admitted` term, a single dispatcher tick would over-issue
// gate tokens until the next PlayerCount refresh propagates from
// gate → etcd → GateWatcher (the C++ gate reports every 10s).
```

（8 行换 8 行，后面的行号不漂移。）

**`capacity.go:32`** 一行里的 `the queue's admittedSetKey count` 改为 `the queue's in-flight placeholder count`（helper 已改名，不能留旧名字）。

**`capacity.go:49`** 一行改为：`//	admitted  = live members of admitted:zset:zone:{zone} (expired ones dropped first)`。

**`metrics.go:10`** 改为：`//   login_queue_admitted_inflight Gauge       per zone     in-flight placeholders (ZCARD after dropping expired)`。

**`metrics.go:62-73`** 改为：

```go
	// admittedInflightGauge 是清掉过期成员之后 admitted:zset:zone:{zone} 的成员数:
	// 已放行还没来取的,加上票已交出去、还在交票后宽限(15s)之内的(排队取走与快速通道)。
	// 容量记账用它(FreeSlots 减去这一项)。
	admittedInflightGauge = metric.NewGaugeVec(&metric.GaugeVecOpts{
		Namespace: metricNamespace,
		Subsystem: "",
		Name:      "admitted_inflight",
		Help: "In-flight admission placeholders per zone: admits not yet picked up, plus gate " +
			"tokens handed out within the last 15s handoff grace (queue consume and fast path). " +
			"ZCARD admitted:zset:zone:{zone} after dropping expired members. Sitting at " +
			"(capacity - online) while login_queue_size > 0 is normal back-pressure; if gate " +
			"PlayerCount does not grow at the same time, clients are not redeeming their admits.",
		Labels: []string{"zone"},
	})
```

**`metrics.go:196`** 一行里的 `ZCARD/SCARD calls` 改为 `ZCARD / EVAL calls`。

**`config/config.go:255-259`**（5 行换 5 行，字段行不动，不触发 gofmt 重排）：

```go
//   - DispatchInterval too low burns Redis QPS for no benefit (gate
//     PlayerCount only refreshes every 10s via etcd anyway).
//   - AdmitTTL 是放行槽等客户端来取的时限,也是没人来取时在途占位的寿命(默认 60s)。
//     取走之后、以及快速通道当场交票之后,占位只再留 15s:那是代码常量
//     loginqueue.defaultHandoffGrace,不是配置,依据见该常量的注释(G4b-G)。
```

**`etc/login.yaml` 的两处改动都按内容定位，不按行号**：别的会话在这份文件前面加了 9 行（文件头的基线说明），落码时行号可能还会再变。两处的锚点分别是 `# Roll-out checklist:` 下面以 `#   0. 多个 zone 共用一个 Redis 时` 开头的 3 行，和紧挨 `Queue:` 之上、以 `# DispatchInterval too low burns Redis QPS without improving fairness;` 开头的 6 行。只替换这 9 行，文件其余部分（包括别人改过的段落）原样保留；任一锚点找不到，或内容与下面引的「现文」不同，停下报告，不猜。

**上线清单第 0 步**（HEAD `:436-438`，工作区 09:07 快照 `:445-447`；3 行换 3 行）。现文只要求「多 zone 共用 Redis 时先升到 G1b」；本批的部署顺序要求（§5.5）对所有拓扑都成立，而手工打开排队的人照着做的正是这份清单，所以必须写在这里。

现文：

```yaml
#   0. 多个 zone 共用一个 Redis 时,先把所有 zone 的 login 升到 2026-10-12(G1b)之后的版本,
#      再打开 Enabled。新旧版本混跑时,同一个 zone 可能短暂出现两个 leader:只会多放行一批,
#      不丢人、不重复放行(docs/design/login-queue-2026-05.md §7.1)。
```

改为：

```yaml
#   0. 先在 Enabled=false 下把所有 zone 的 login 升到 2026-10-13(G4b-G,含 G1b)之后的版本,再打开 Enabled。
#      排队开着混跑时:同一个 zone 可能短暂两个 leader(G1b);新旧两种在途占位键互不可见,旧版副本放行 / 取走的
#      部分仍按旧版超发(G4b-G)。不丢人、不重复放行(docs/design/login-queue-2026-05.md §7.1 与「在途占位的生命周期」)。
```

**`Queue:` 上方的 6 行注释**（HEAD `:445-450`，工作区快照 `:454-459`；6 行换 6 行。连同上面那 3 行，本批不改变文件行数，`Queue:` 的行号不因本批而动）。

现文：

```yaml
# DispatchInterval too low burns Redis QPS without improving fairness;
# 1s is the sweet spot since gate PlayerCount only refreshes every few
# seconds via etcd.
#
# AdmitTTL must outlive the worst-case "client reads admit token →
# connects to gate" round-trip across mobile networks. 60s is generous.
```

改为：

```yaml
# DispatchInterval too low burns Redis QPS without improving fairness;
# 1s is the sweet spot since gate PlayerCount only refreshes every 10s
# via etcd (cpp/nodes/gate/main.cpp playerCountReportTimer).
#
# AdmitTTL:放行槽等客户端来取的时限,也是没人来取时在途占位的寿命。取走之后(以及快速通道当场
# 交票之后)占位只再留 15s —— 代码常量 loginqueue.defaultHandoffGrace,不是配置(G4b-G,2026-10-13)。
```

### 4.7 gofmt 注意（Claude 不跑 gofmt）

- 键 helper 五行是一个对齐段：最长的是 `admittedZSetKey` 那行（`{` 前 1 个空格），其余四行 `{` 前依次是 5、7、8、4 个空格（§4.4 第 3 步已按此写）。
- `Queue` 的两个新字段各自前面有独占一行的注释，注释行会切断对齐段（`dispatcher.go:65-71` 是同样的形状），所以字段名与类型之间各 1 个空格。
- `New` 的复合字面量按最长键 `handoffGrace:` 对齐。
- `metrics.go` 很可能在 HEAD 上就不是 gofmt 干净的：包注释 `:9-14` 的表格行是三空格缩进，gofmt 会把文档注释里的缩进块改成 tab（Go 1.19 起的行为，凭记忆，**未核实**，也没有运行 gofmt）；`PROGRESS.md:5948` 记过 `loginqueue` 下有 HEAD 既有的未格式化文件。§4.6 给的 `metrics.go:10` 新文本特意沿用相邻行的三空格写法，不要为了让 gofmt 安静去改它。
- gofmt 的处理规则写在 §8.2，要点两条：**HEAD 基线里已经被 `gofmt -l` 列出的文件一律不 `gofmt -w`**（仓库先例 `PROGRESS.md:5474`：以不超出基线为通过标准，不顺手整文件格式化）；基线里没有、落码后才被列出的文件，**只有属于本批的那 6 个 Go 文件**才允许 `gofmt -w` 后重跑并在报告里注明。别的会话改过的文件（例如同在 `internal\config` 下的 `new_player_seed_conf_test.go`）即使新被列出，也只记录、不动。

---

## 5. 关键语义

### 5.1 占位的生命周期与几个边角

- **放行未取**：分数 = 放行时刻 + `AdmitTTL`，与放行槽 `admit:{id}` 的 TTL 同长。客户端始终不来取：放行槽 60s 后过期，占位在同一时刻（± 时钟偏差）到点，下一次计数时清掉。
- **取走**：只发生一次（`GETDEL` 只成功一次）。取走时把分数改成「取走时刻 + 15s」。多数情况是缩短（轮询间隔 2s，放行后几秒就取走）；临到 60s 才取则是延长。
- **重复轮询**：第二次 `Lookup` 的 `GETDEL` 回空，在改分数之前就返回，占位不会被反复续期。客户端看到的仍是「排队中、名次 0」（F6，本批不改），按协议它会每 2s 继续轮询直到 1h。所以 **`holdAdmitted` 只能留在 `consumeAdmitAndSign` 里、`GETDEL` 判空与 `signFn` 成功之后**：挪到判空之前或挪进 `Lookup`，每次轮询都会把占位续 15s，占位永不到期，Q1 换个形态复发。由 T13 钉住。
- **取走时占位已被清**（login 钟比 Redis 快）：普通 ZADD 把它补回来，照样占 15s。Java 版这里用 `ZADD XX`、不补（`LoginQueue.java:138`）。本仓库不照抄：这个人确实马上要占一个名额，补回来只会多扣一个名额 15s，不补则会多放一个人；补回来也让新旧版本混跑时新版的计数更准（§5.5）。
- **`signFn` 失败**：在改分数之前返回（与现状的「在 SREM 之前返回」位置相同）。放行槽已删，占位保持放行时的分数，最迟 `AdmitTTL` 到点。不主动回收：签票失败意味着密钥配置错或没有 gate，都不是多一个名额能解决的。由 T14 钉住。
- **meta 读不到 zone**：跳过改分数（现状是跳过 SREM），占位最迟 `AdmitTTL` 到点。
- **快速通道**：`fast:<uuid>` 从写入起 15s 到点。占位成功但随后 `signFastPath` 失败（没有候选 gate 或签名失败，`assigngatelogic.go:191-211`）时不回收，15s 后自己消失（现状是随整键 TTL，不确定多久）。
- **快速通道为什么不取 `AdmitTTL`**：`AdmitTTL` 的含义是「放行槽等客户端来取的时限」。快速通道没有「等人来取」这一段，票在同一个应答里交出去，状态等同排队路径的「已取走」。若取 60s，每个快速通道玩家连上 gate 之后还要被重复扣约 50s：一批人涌入后名额要多停约 50s 才放回来，Q1 的现象（放一批、停一阵）只是从「不定期」变成「约 50s」，没有消除。数字见 §5.2。Java 版取的是 60s（`LoginQueue.java:206-209`），这一点两版不同，列入 §11 #2。

### 5.2 容量计算的变化（算式）

记号：`C` 容量，`O` gate 上报的在线数（滞后），`A` 未到期的占位数，`free = max(0, C − O − A)`；`G` = 15s，`T` = 10s（gate 上报周期），`c` = 交票到 gate 上建立会话的耗时，`φ ∈ [0, T)` = 距下一次上报的相位，`v = c + φ` = 这个人出现在 `O` 里的时刻（相对交票）。

**一个人被算了几遍**（交票之后）：

| 时间段 | 修复后 | 修复前（排队路径） | 修复前（快速通道） |
|---|---|---|---|
| `[0, min(v, G))` | 1 遍（占位） | **0 遍**（已 SREM，还没进 `O`） | 1 遍 |
| `v < G` 时的 `[v, G)` | **2 遍**（占位 + `O`），时长 `G − v` | 1 遍 | 2 遍 |
| `v > G` 时的 `[G, v)` | **0 遍**，时长 `v − G` | 0 遍 | 1 遍（占位不走，人还没进 `O`） |
| `max(v, G)` 之后 | 1 遍（`O`） | 1 遍 | **2 遍，直到整键闲置 60s** |

**双计窗口**（少放的方向）：`G − v = 15 − c − φ` 秒。`c` 取 1s 时范围 (4s, 14s]，平均 9s。

- 瞬时少放的名额数 ≈ 交票速率 `λ`（人/秒）× 平均双计时长。`λ = 3` 约 27 个；`λ = 100` 约 900 个，最多 `15λ`。
- 一次涌入 `B` 人：最多 `B` 个名额多停 `15 − v` 秒（修复前快速通道是「直到整键闲置 60s」，持续有人登录时无上界）。
- 放行后没人来取：1 个名额占 `AdmitTTL`（60s），与现状的设计意图相同。
- 快速通道若取 `AdmitTTL`（§11 #2 的备选）：平均双计约 54s，瞬时少放 ≈ `54λ`，是默认做法的 6 倍。

**漏计窗口**（多放的方向）：只在 `c + φ > 15s` 时出现，时长 `c + φ − 15`，最长 `c − 5` 秒。建连在 5s 以内时为 0。

- 修复前（排队路径）每个人都有 `c + φ` 秒的漏计，平均 `c + 5` 秒。开区时 dispatcher 约每 2～3s 就能再放一满批（客户端轮询间隔 2s + 至多一拍 1s），gate 第一次上报前的 10s 内可放进 4～5 倍容量（推断，未实测）。
- 修复后正常情况（`c ≤ 5s`）开区不超发。
- gate 迟迟不上报或建连普遍超过 5s 时：任一时刻的在途占位仍 ≤ `C − O`（快速通道的判定是原子的，dispatcher 按 `free` 放），占位最快 15s 才放回，所以多放的节奏从「约每 2～3s 一批」降到「约每 17～18s 一批」（15s 宽限 + 轮询与一拍），每批 ≤ `C − O`。这是有界改善，不是消除。
- gate 票有效期 10 分钟（F11），远大于任何宽限：拿了票超过 15s 才连上的人，名额已经放给别人，他连上就是多 1 人。上界 = 宽限之外才连上的人数；gate 侧另有连接上限（`bin/etc/base_deploy_config.yaml:126` 的 `GateMaxConnections`）。

**放行节奏的上界（修复后；判读压测与 runbook 用）**：每次放行都以 `O + A ≤ C` 为界；一个名额被排队放行的人拿走后，最早要到「取走 + 15s」才可能放回，而且前提是这个人没有进 `O`。所以：

- 放进去的人留在线上（正常玩家）：名额从占位转给在线数，不放回。排队的长期放行速率 = 在线玩家的离开速率；`O + A` 顶到 `C` 之后 `q_admitted` 不涨是**正常回压**，不是 dispatcher 卡死。
- 拿了票不连 gate，或连上很快就走：名额每「取走延迟（≤ 轮询 2s + 一拍 1s）+ 15s」约 17～18s 周转一次，任意这样一个窗口内经排队放行的人数 ≤ 窗口起点的 `C − O`。修复前这个周转周期是 2～3s（Q2）。
- runbook 与设计稿里「`q_admitted` 每 30s 涨一个 zone capacity」「增速接近 zone capacity / s」「100s 内涨到 100」这类节奏数字写于 2026-05，没有按这个上界标定过；它们成立的前提是放行的人不长期占着名额（robot 登录后是否保持在线，本规格**未核实**）。照现文判读，修好的行为会被判成「模式 1：dispatcher 卡死」，而 L-14 正是拿这份 runbook 判读，所以 §7.2 把这几处一并登记。

**时钟偏差**：上面所有时长再加 [−δ, +δ]；故障副本恢复之后的残留见 §4.2「代价」。

### 5.3 错误语义（与现状逐条对比，方向都不变）

| 场景 | 现状 | 修复后 | 方向 |
|---|---|---|---|
| assign-gate 里 `FreeSlots` 读占位数失败 | 回 `StatusError`，不放行也不入队（`assigngatelogic.go:102-107`） | 相同（EVAL 出错走同一条包装 `admitted count: …`） | fail-closed，不变 |
| 快速通道占位脚本出错 | 记 ERROR，不放行快速通道，落到入队（`assigngatelogic.go:119-122`） | 相同 | fail-closed，不变 |
| `budget == 0` | 不碰 Redis，回 false | 相同 | 不变 |
| dispatcher 一拍里 `FreeSlots` 出错 | 记 ERROR，这一拍不放行，条目留在队列（`dispatcher.go:273-277`） | 相同 | fail-closed，不变 |
| gauge 快照读占位数出错 | 这一拍不更新 gauge，无日志（`metrics.go:204`） | 相同 | 不变 |
| `PopAdmit` 的 MULTI 出错 | `continue`：条目已弹出、未计入放行数（既有丢条目路径，G1b 规格 §4.5 第 1 条已登记） | 相同，只是 MULTI 里的命令换了 | 不变 |
| 取走时占位记账出错 | SREM 的错误被忽略，无日志；占位留到整键 TTL（少放） | ZADD / PEXPIRE 的错误记一条 ERROR，取走照常成功；占位保持放行时的分数，最迟 `AdmitTTL` 到点（少放） | 对玩家 fail-open、对容量偏保守，不变；多了一条日志 |
| 取走时 meta 读不到 zone | 跳过 SREM | 跳过改分数 | 不变 |
| `signFn` 失败 | 回 error，客户端 500；占位留到整键 TTL | 回 error；占位最迟 `AdmitTTL` 到点 | 不变，残留从不定期变成有界 |
| 占位键被 Redis 淘汰或误删 | 计数归 0，下一批最多多放 `C − O` | 相同 | 不变 |
| `Queue.Enabled=false` | 不构造 `Queue`，不碰任何排队键 | 相同 | 不变 |
| 某副本时钟严重偏差（偏差量 δ ≥ 宽限量级） | 不影响占位（占位不看时钟） | 故障期间：该副本每次读都早清别人的占位（多放）。**恢复或摘掉之后不会立刻自愈**：它最后 15s / 60s 内写的占位要再过 δ 才到期，期间有效容量少掉这些占位（≤ 当时的 `C − O`）；只有 zone 完全停放时整键 TTL 才兜底（最长约 `AdmitTTL + W`，默认 3 分钟）。止血：`redis-cli DEL admitted:zset:zone:<z>`，代价是最多多放一批 `C − O` | **新增的失败模式**，见 §4.2 代价、R2、§11 #4 |

计数从只读的 `SCARD` 变成带清理的写脚本后，「Redis 只读」（主从切换瞬间的 `READONLY`）也会让 `FreeSlots` 报错。此时快速通道占位、入队这些本来就是写的操作同样会失败，结果仍是不放行，没有新增方向。

### 5.4 并发与原子性边界

- `Queue.now` 与 `Queue.handoffGrace` 构造后只读（测试在使用前设置），并发读安全。
- 快速通道：清理、判定、占位在一段 Lua 里，并发调用不会超过 `budget`（`TestTryReserveFastPathSlot_ConcurrentNeverExceedsBudget`）。
- **不原子、且本批不改的两处**（既有）：
  1. assign-gate 里「`QueueLen == 0`」与「占位」是两次往返（`assigngatelogic.go:109-119`）。
  2. dispatcher 里「`FreeSlots` 读占位数 → `ZPOPMIN` → 逐条写占位」不在一段脚本里（`dispatcher.go:273-306`；`queue.go:396-436`）。一批把队列弹空之后、占位逐条写完之前，快速通道看到「队列空、占位还没写全」，可以再占进来。任一时刻在途占位 ≤ 2 ×（`C − O`），与 G1b 规格 §4.5 的双 leader 上界同形。Java 版把弹出、算空位、写占位放在一段 Lua 里（`LoginQueue.java:100-126`），这一点仍是两版的有意差异。
- 取走：`GETDEL` → 签票 → `HMGET` → 改分数，是多次往返（既有）。进程在 `GETDEL` 之后、改分数之前崩溃：占位保持放行时的分数，最迟 `AdmitTTL` 到点。
- go-redis 超时重发（F16）：占位成员名在 Go 侧生成，脚本重跑是同一个成员、同一个分数，幂等；最坏情况是第一次已占到最后一个名额、重跑回 0，多出一个 15s 的空占位。计数脚本重跑无副作用。

### 5.5 新旧版本混跑（滚动发布）与部署顺序

旧 Pod 读写 `admitted:zone:{z}`（SET），新 Pod 读写 `admitted:zset:zone:{z}`（ZSET）。键不同，不会 `WRONGTYPE`；但两边互相看不见对方的占位。

- **排队关着滚（要求的做法）**：`Queue.Enabled=false` 时不构造 `Queue`（`servicecontext.go:439-441`），assign-gate 直接签票、不占位（`assigngatelogic.go:91-93`），新旧两种键都没人碰。滚动对占位记账是空操作。当前所有环境都是这个状态（F13）。
- **部署顺序要求**：先在排队关闭的状态下把所有 zone 的 login 滚到含 G4b-G 的镜像，再由 G4b-2 打开排队。合入顺序 G1b → G4b-G → G4b-2 保证了 K8s 上是这个顺序（K8s 只有 G4b-2 才会生成 `Queue` 段）。这条要求落在三处，缺一处就有人看不到：
  1. `login.yaml` 上线清单第 0 步（§4.6，本批代码同批改）：手工打开排队的人照着做的是它；
  2. `spec_G4b.md` 的契约用例 15 补一条断言「`queue.go` 含 `admitted:zset:zone:%d`、不含 `"admitted:zone:%d"`」（§7.2 第 4 项登记；实现属于 G4b-2，不在本批），把合入顺序变成机械门禁；
  3. 设计稿新小节与 runbook「故障应急」（§7.2 第 1、2 项）。
- **排队开着滚（本机或裸机有人手工打开过时才可能）**。一个条目按谁经手它来记账，逐格列出：
  - **旧 leader 放行、旧 Pod 取走**：与 G4b-G 之前完全相同，取走即 SREM，**Q2 照旧**（开区每拍再放一满批，不以 `C − O` 为界）。
  - **旧 Pod 的快速通道**：照旧写旧键的 `fast:<uuid>`，**Q1 照旧**。
  - 旧 leader 放行、新 Pod 取走：旧 SET 里的成员没人删，按旧版 Q1 的方式残留（旧 leader 少放）；新键里补一个 15s 的占位（新 Pod 的快速通道计数准）。
  - 新 leader 放行、旧 Pod 取走：新键里的占位不被改分数，留到放行后 60s（少放，有界）。
  - 新 leader 放行、新 Pod 取走，新 Pod 的快速通道：G4b-G 的行为。
  - 两种键互不可见带来的**额外**量：新旧两边的快速通道各自按自己的键判 `< budget`，任一时刻在途占位合计最多 2 ×（`C − O`）；每 zone 仍只有一个 leader（锁键本批不动），它只数自己那种键，看不到另一种键上的快速通道占位。
  - 不丢人、不重复放行：`ZPOPMIN` 与 `GETDEL` 的原子性与占位键无关。
  - **结论**：相对各自版本单跑，混跑额外多放 ≤ 一个 `C − O`、少放有界，窗口等于滚动时长；**leader 或经手的副本是旧版期间 Q1 / Q2 仍在，不能把混跑窗口当作 G4b-G 已生效**。G4b-G 的两条修复只在放行方与取走方都是新版时才成立。可以接受，但不推荐。
- **旧键**：新版本不再写。最后一个旧 Pod 停写后，旧键在 `AdmitTTL`（60s）内整键过期，不需要清理步骤。要立即清：`redis-cli DEL admitted:zone:<z>`。
- **新键的止血命令**：`redis-cli DEL admitted:zset:zone:<z>`（时钟故障残留，§4.2；或任何怀疑占位记账卡住的时候）。代价是最多多放一批 `C − O`。
- **回滚（新 → 旧）**：窗口与上面对称。新键在最后一次写入后 `W`（默认 2 分钟）内整键过期。排队开着时不要用 `kubectl rollout undo` 退镜像（`spec_G4b.md` §12 已写）：先用 `-LoginQueue off` 关排队，再退。

### 5.6 zone-down 与合服后旧 zone 的残留

源 zone S 的 login 全部下线后，不再有人写 `admitted:zset:zone:S`，它在最后一次写入后 `W`（默认 2 分钟）内整键过期。设计稿 `login-queue-2026-05.md:251` 写的「`admitted:zone:S` 在 `AdmitTTL`（60s）内过期」要改成这个时长。合服工具与回档脚本的键清单本来就不含排队键，不用登记新键（§7.3）。

### 5.7 指标

- `login_queue_admitted_inflight` **不改名、不改标签**。口径变宽：以前是「已放行还没取走的 + 永不单独回收的快速通道占位」，现在是「已放行还没取走的 + 票已交出去 15s 之内的（排队取走与快速通道）」。稳态约等于 `交票速率 × 15s + 等轮询的放行数`。帮助文本里的 `SCARD` 与「> 100」阈值删掉（§4.6）。
- 读这个 gauge 的那次 `AdmittedCount` 带清理副作用。它只由 leader 每拍调用一次，清理本身幂等，没有问题。
- **不新增指标**。理由：仓库里没有任何告警或面板消费它（F14）；把「没来取」与「已取走等上报」拆成两个 gauge 需要每拍多一次按前缀或分数段的统计；L-14 排查时可以直接 `redis-cli ZRANGE admitted:zset:zone:<z> 0 -1 WITHSCORES` 看成员与到期时刻。YAGNI。
- 仓库里没有引用旧阈值的告警规则；runbook 里的文字阈值由 Docs 阶段同步（§7.2）。

### 5.8 既有边角（不在本批范围，只登记）

1. 取走后再拿同一令牌轮询，回「排队中、名次 0」直到 1h（F6）；回包丢失时这张 gate 票找不回来。Java 版回 410。这是 `PARITY.md`「登录排队」行 ③ 的另一句，方案 A 的提纲没有包含。
2. `PopAdmit` 不是单段 Lua（§5.4 第 2 条）；`pickFn` 返回 `(nil, nil)`、Marshal 失败、MULTI 失败时条目丢失（G1b 规格 §4.5 第 1 条）。
3. 「账号已有在线会话就绕过排队」的分支直接签票、不占位（`assigngatelogic.go:83-88`），这批人在 gate 上报之前不在任何计数里。Java 版没有这条路径（`PARITY.md`「登录排队」行 ①）。
4. go-redis 超时重发 `GETDEL` 时，第二次回空，客户端看到「排队中、名次 0」（F16、F6 合起来的后果）。Java 版为此给取走加了请求号。
5. `ZoneCapacityOverride` 不配时的回落公式（`capacity.go:76-87`）与本批无关，由 G4b-2 负责写配置。

---

## 6. 测试清单

全部在 `go/login/internal/logic/pkg/loginqueue/`（`package loginqueue`），只依赖 **miniredis（进程内）**，不需要任何外部依赖。

**怎么控制时间**：占位的分数与清理只读 `Queue.now`，用例换成假时钟直接拨，不 sleep，不调用 `mr.SetTime` / `mr.FastForward`。拨假时钟之后放行槽 `admit:{id}` 仍然在，这正好用来模拟「login 钟比 Redis 快」（用例 T6）。T6 不依赖 miniredis 的时间行为：用例真实耗时是毫秒级，60s 的放行槽放到真实 Redis 上也不会过期。（miniredis v2.35.0 里键的 TTL 本来也不随墙钟走，只在 `FastForward` 里递减，`db.go:711-718`，全包没有定时器，已对源码核实。）本规格没有用 Lua 内的 `TIME`。

**唯一有真实等待的新用例是 T10**（Redis 连不上）：三条会走到 Redis 的调用各拨号一次，每次最多等一个 `DialTimeout`（200ms），上界约 3 × 200ms。其余新用例不依赖真实墙钟（AGENTS §11.4）。Windows 回环上被拒绝的连接实际多久返回，**未核实**，所以只写上界。

### 6.1 公共助手（加在 `queue_test.go`，import 增加 `strings`、`sync`、`sync/atomic`）

```go
// ── G4b-G:在途占位逐个到期 ─────────────────────────────────────────────

// legacyAdmittedSetKeyZone1 是 G4b-G 之前的在途占位键(SET)。新实现不再读写它,
// 用例拿它断言「旧键没人写」。新旧版本混跑时两种键并存、互不可见。
const legacyAdmittedSetKeyZone1 = "admitted:zone:1"

// fakeClock 是在途占位记账用的可拨时钟。占位的到期分数与清理都读 Queue.now,
// 用例直接拨它:不 sleep,也不依赖 miniredis 的时间(miniredis 里键的 TTL 不随它走)。
type fakeClock struct{ ms atomic.Int64 }

func newFakeClock() *fakeClock {
	c := &fakeClock{}
	c.ms.Store(1_800_000_000_000)
	return c
}

func (c *fakeClock) Now() time.Time          { return time.UnixMilli(c.ms.Load()) }
func (c *fakeClock) Advance(d time.Duration) { c.ms.Add(d.Milliseconds()) }

// newClockedQueue 在 newTestQueue 的基础上把占位时钟换成假时钟(起点固定,结果可复现)。
func newClockedQueue(t *testing.T) (*Queue, *redis.Client, *fakeClock) {
	t.Helper()
	q, rdb, _, cleanup := newTestQueue(t)
	t.Cleanup(cleanup)
	clk := newFakeClock()
	q.now = clk.Now
	return q, rdb, clk
}

// oneGateProvider:zone 1 一台 gate、固定容量。在线数由用例直接改 cands[1][0].PlayerCount
// (fakeProvider 返回的是同一个切片,不是副本)。
func oneGateProvider(capacity, online uint32) *fakeProvider {
	return &fakeProvider{
		cands: map[uint32][]GateCandidate{
			1: {{NodeID: 1, IP: "10.0.0.1", Port: 9000, PlayerCount: online, ZoneID: 1}},
		},
		caps: map[uint32]uint32{1: capacity},
	}
}

func pickZone1Gate(zoneID uint32, _ string) (*AdmitSlot, error) {
	return &AdmitSlot{NodeID: 1, IP: "10.0.0.1", Port: 9000, ZoneID: zoneID}, nil
}

func mustAdmittedCount(t *testing.T, q *Queue, want uint32, when string) {
	t.Helper()
	got, err := q.AdmittedCount(context.Background(), 1)
	if err != nil {
		t.Fatalf("admitted count (%s): %v", when, err)
	}
	if got != want {
		t.Fatalf("admitted count (%s) = %d, want %d", when, got, want)
	}
}

func mustFree(t *testing.T, prov CapacityProvider, q *Queue, want uint32, when string) {
	t.Helper()
	free, capacity, online, admitted, err := FreeSlots(context.Background(), prov, q, 1, 1.5)
	if err != nil {
		t.Fatalf("freeslots (%s): %v", when, err)
	}
	if free != want {
		t.Fatalf("free (%s) = %d, want %d (cap=%d online=%d admitted=%d)",
			when, free, want, capacity, online, admitted)
	}
}

func mustLegacyKeyUntouched(t *testing.T, rdb *redis.Client) {
	t.Helper()
	n, err := rdb.Exists(context.Background(), legacyAdmittedSetKeyZone1).Result()
	if err != nil || n != 0 {
		t.Fatalf("legacy key %q must not be written any more: exists=%d err=%v", legacyAdmittedSetKeyZone1, n, err)
	}
}
```

### 6.2 用例

「修复前」一栏：这些用例引用了新字段，在 HEAD 上编译不过，红 / 绿的证明由 §6.5 的两个 HEAD 复现用例承担；此栏写的是同一断言放到旧实现上的结果。

| # | 用例名 | 文件 | 断言要点 | 修复前 |
|---|---|---|---|---|
| T1 | `TestNew_DefaultsHandoffGraceAndClock` | queue_test | `New` 出来的 `handoffGrace == 15s`、`now != nil`、`admittedKeyTTL() == 2min` | — |
| T2 | `TestPopAdmit_HoldsPlaceholderInZSetUntilAdmitTTL` | queue_test | 放行后新键类型是 `zset`；成员分数 = 放行时刻 + `AdmitTTL` 的毫秒；整键 PTTL 在 (`AdmitTTL`, `AdmitTTL + 1min`]；不取走，拨到 `AdmitTTL − 1ms` 计数仍是 1，再拨 1ms 计数为 0；旧键不存在 | 红（旧键是 SET，没有逐成员到期） |
| T3 | `TestTryReserveFastPathSlot_BudgetAndPerMemberExpiry` | queue_test | `budget=2`：true、true、false；`budget=0`：false；两个成员都以 `fast:` 开头、分数 = 现在 + 15s；拨到 15s − 1ms 仍占满，再拨 1ms 计数为 0、可以再占；旧键不存在 | 红（占位不会逐个到期） |
| T4 | `TestFastPath_SustainedLoginsDoNotStarveBudget` | queue_test | **Q1 回归**。容量 100，每秒 1 人走快速通道，10s 后进 `PlayerCount`，连续 60s：每一秒 `free > 0` 且占位都成功；结束时占位数 ≤ 15 | **红**：第 55 秒 `free` 归零（§6.5 Q1） |
| T5 | `TestConsume_KeepsPlaceholderForHandoffGraceThenReleases` | queue_test | **Q2 回归**。容量 2、`PlayerCount` 恒 0、入队 6 人：放行 2 人并取走后，两个成员仍在、分数 = 取走时刻 + 15s，`free == 0`；拨到 15s − 1ms 仍是 0；再拨 1ms，占位数 0、`free == 2` | **红**：取走后 `free` 立刻回到 2（§6.5 Q2） |
| T6 | `TestConsume_HoldsPlaceholderEvenIfAlreadyExpired` | queue_test | 放行后把假时钟拨过 `AdmitTTL`（占位被清，放行槽在 miniredis 里还在）：取走仍回 ADMITTED，占位数回到 1，分数 = 取走时刻 + 15s。钉住「普通 ZADD，不是 XX」 | — |
| T7 | `TestAdmittedCount_DropsOnlyExpiredMembers` | queue_test | 键不存在时计数 0、无错误；直接 ZADD 三个成员（分数 now−1、now、now+1）：计数 1，剩下的只有 now+1 那个。钉住闭区间 | 红（旧实现不清） |
| T8 | `TestTryReserveFastPathSlot_ConcurrentNeverExceedsBudget` | queue_test | 64 个 goroutine 同时占位、`budget=8`：恰好 8 个成功，`ZCARD == 8` | 绿（旧 Lua 同样原子；这是 F15 欠的覆盖） |
| T9 | `TestLegacyAdmittedSetKey_NeverWritten` | queue_test | 走一遍快速通道占位、入队、`FreeSlots`、放行、取走、计数之后，`KEYS admitted:*` 只有 `admitted:zset:zone:1` | 红 |
| T10 | `TestAdmittedAccounting_RedisDown_FailsClosed` | queue_test | 关掉 miniredis 后：`AdmittedCount` 回 error；`FreeSlots` 回 error 且 `free == 0`；`TryReserveFastPathSlot(budget=5)` 回 `(false, err)`；`budget=0` 回 `(false, nil)`（不碰 Redis）。客户端关掉命令重试与拨号重试，真实等待 ≤ 3 × `DialTimeout` | 绿（方向没变，钉住它） |
| T11 | `TestConsume_PlaceholderWriteFailureDoesNotFailAdmit` | queue_test | 放行后把占位键换成 STRING：取走仍回 ADMITTED（记账失败不让取走作废）；随后 `AdmittedCount` 回 error、快速通道占位回 `(false, err)`（读不出记账就不放行） | — |
| T12 | `TestDispatchOnce_ConsumedAdmitsHoldCapacityForHandoffGrace` | dispatcher_test | **Q2 回归（dispatcher 层）**。不 Start，直接调 `dispatchOnce`：容量 2、入队 6：第一拍后队列剩 4；取走 2 人；每拍拨 1s 连跑 5 拍，队列仍是 4；拨过 15s 再跑一拍，队列剩 2 | **红**：取走后的每一拍都再放 2 人 |
| T13 | `TestConsume_RepeatPollDoesNotExtendPlaceholder` | queue_test | 取走后 5s 再拿同一令牌轮询：不是 ADMITTED；占位分数仍是「第一次取走时刻 + 15s」，没有被续期；占位数仍是 1；拨到取走后 15s，占位数为 0。钉住「`holdAdmitted` 在 `GETDEL` 判空之后」（§5.1） | —（防落码时放错位置：每次轮询续 15s，占位永不到期） |
| T14 | `TestConsume_SignFailureKeepsAdmitTimeScore` | queue_test | `signFn` 返回 error：`Lookup` 回 error；占位分数仍是「放行时刻 + `AdmitTTL`」；再轮询拿不到票；拨到放行后 `AdmitTTL − 1ms` 占位数仍是 1，再拨 1ms 为 0 | —（钉住 §5.1 的 `signFn` 失败语义） |
| 改 | `TestIntegration_QueueDrainAdmits`、`TestIntegration_DispatcherRespectsCapacity` | dispatcher_test | 断言不变；fixture 把宽限设 0（§6.4） | 不改的话前者必然超时 |
| 改 | `TestIntegration_DualLeadersSameZone_NoLossBoundedOverAdmit` | dispatcher_test | 断言不变；`:411` 改读新键（§6.4） | 不改则编译失败 |
| 不动 | 其余既有用例 | — | `TestPopAdmitWritesAdmitToken`、`TestFreeSlotsAccountsForAdmittedInFlight`（放行后占位数仍是 1）、`TestIntegration_LeaderElectionSerializesDispatch`、`TestIntegration_TwoZonesSharedRedis_BothQueuesDrain`、`TestDispatchOnce_EmptyQueueSkipsGateDirectory`、svc 包的 `TestInitLoginQueue_*` 都不依赖「取走即撤占位」或占位键的类型 | 绿 |

### 6.3 参考实现（`queue_test.go`，接在 §6.1 的助手之后）

```go
// 15s 与 gate 的上报周期(10s)配套,见 defaultHandoffGrace 的注释;改它要同步改设计稿与 runbook。
func TestNew_DefaultsHandoffGraceAndClock(t *testing.T) {
	q := New(nil, time.Hour, 60*time.Second, nil)
	if q.handoffGrace != 15*time.Second {
		t.Fatalf("handoffGrace = %v, want 15s", q.handoffGrace)
	}
	if q.now == nil {
		t.Fatal("Queue.now must default to time.Now")
	}
	if got, want := q.admittedKeyTTL(), 2*time.Minute; got != want {
		t.Fatalf("admittedKeyTTL = %v, want %v (max(AdmitTTL, handoffGrace) + 1min)", got, want)
	}
}

// 放行写的是新键 ZSET,分数 = 到期毫秒;没人来取时按自己的分数到期,不靠整键 TTL。
func TestPopAdmit_HoldsPlaceholderInZSetUntilAdmitTTL(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	admittedAt := clk.Now()
	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil)", n, err)
	}

	key := admittedZSetKey(1)
	if key != "admitted:zset:zone:1" {
		t.Fatalf("admitted key = %q, want admitted:zset:zone:1", key)
	}
	if typ, err := rdb.Type(ctx, key).Result(); err != nil || typ != "zset" {
		t.Fatalf("type(%s) = %q err=%v, want zset", key, typ, err)
	}
	score, err := rdb.ZScore(ctx, key, res.QueueID).Result()
	if err != nil {
		t.Fatalf("zscore: %v", err)
	}
	if want := float64(admittedAt.Add(q.admitTTL).UnixMilli()); score != want {
		t.Fatalf("placeholder score = %.0f, want %.0f (admit time + AdmitTTL, unix ms)", score, want)
	}
	if ttl, err := rdb.PTTL(ctx, key).Result(); err != nil || ttl <= q.admitTTL || ttl > q.admittedKeyTTL() {
		t.Fatalf("whole-key PTTL = %v err=%v, want (%v, %v]", ttl, err, q.admitTTL, q.admittedKeyTTL())
	}

	clk.Advance(q.admitTTL - time.Millisecond)
	mustAdmittedCount(t, q, 1, "1ms before AdmitTTL")
	clk.Advance(time.Millisecond)
	mustAdmittedCount(t, q, 0, "at AdmitTTL, never picked up")
	mustLegacyKeyUntouched(t, rdb)
}

func TestTryReserveFastPathSlot_BudgetAndPerMemberExpiry(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()
	reserve := func(budget uint32) bool {
		t.Helper()
		ok, err := q.TryReserveFastPathSlot(ctx, 1, budget)
		if err != nil {
			t.Fatalf("reserve(budget=%d): %v", budget, err)
		}
		return ok
	}

	if !reserve(2) || !reserve(2) {
		t.Fatal("first two reservations must fit budget 2")
	}
	if reserve(2) {
		t.Fatal("third reservation must be refused: budget 2 is full")
	}
	if reserve(0) {
		t.Fatal("budget 0 must never reserve")
	}

	wantScore := float64(clk.Now().Add(q.handoffGrace).UnixMilli())
	members, err := rdb.ZRangeWithScores(ctx, admittedZSetKey(1), 0, -1).Result()
	if err != nil || len(members) != 2 {
		t.Fatalf("placeholders = %v err=%v, want 2 members", members, err)
	}
	for _, m := range members {
		name, _ := m.Member.(string)
		if !strings.HasPrefix(name, "fast:") || m.Score != wantScore {
			t.Fatalf("placeholder %q score %.0f, want fast:<uuid> with score %.0f (now + handoff grace)",
				name, m.Score, wantScore)
		}
	}

	clk.Advance(q.handoffGrace - time.Millisecond)
	if reserve(2) {
		t.Fatal("placeholders are still live 1ms before they expire")
	}
	clk.Advance(time.Millisecond)
	mustAdmittedCount(t, q, 0, "handoff grace elapsed")
	if !reserve(2) {
		t.Fatal("expired fast-path placeholders must free their slots")
	}
	mustLegacyKeyUntouched(t, rdb)
}

// Q1 回归:每秒 1 人走快速通道,10 秒后(gate 的上报周期)出现在 PlayerCount 里,连续 60 秒。
// 旧实现的 fast:<uuid> 占位永远不单独回收,整集合 TTL 又被每次占位续上:
// 连上 gate 的人同时算进 online 和 admitted,第 55 秒余量归零。
func TestFastPath_SustainedLoginsDoNotStarveBudget(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()
	const capacity = 100
	prov := oneGateProvider(capacity, 0)

	for sec := 0; sec < 60; sec++ {
		online := uint32(0)
		if sec > 10 {
			online = uint32(sec - 10)
		}
		prov.cands[1][0].PlayerCount = online

		free, _, _, admitted, err := FreeSlots(ctx, prov, q, 1, 1.5)
		if err != nil {
			t.Fatalf("sec=%d freeslots: %v", sec, err)
		}
		if free == 0 {
			t.Fatalf("sec=%d: free hit 0 with %d online of %d (admitted=%d): connected players are double-counted",
				sec, online, capacity, admitted)
		}
		ok, err := q.TryReserveFastPathSlot(ctx, 1, capacity-online)
		if err != nil || !ok {
			t.Fatalf("sec=%d: fast-path reservation refused (ok=%v err=%v) with %d online of %d",
				sec, ok, err, online, capacity)
		}
		clk.Advance(time.Second)
	}

	// 稳态下占位数 = 最近 handoffGrace 内交出去的票数,不随登录总数增长。
	n, err := q.AdmittedCount(ctx, 1)
	if err != nil {
		t.Fatalf("admitted count: %v", err)
	}
	if limit := uint32(q.handoffGrace / time.Second); n > limit {
		t.Fatalf("placeholders = %d after 60 logins, want <= %d (one per second of handoff grace)", n, limit)
	}
	mustLegacyKeyUntouched(t, rdb)
}

// Q2 回归:取走放行槽之后,gate 要到下一次上报才把人数算进 PlayerCount。
// 这段时间占位必须留着;旧实现取走即 SREM,下一拍按「在线 0、在途 0」再放一满批。
func TestConsume_KeepsPlaceholderForHandoffGraceThenReleases(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()
	prov := oneGateProvider(2, 0)

	queueIDs := make(map[string]string) // token → queueId
	for range 6 {
		res, err := q.Enqueue(ctx, 1, "u", "d")
		if err != nil {
			t.Fatalf("enqueue: %v", err)
		}
		queueIDs[res.QueueToken] = res.QueueID
	}
	mustFree(t, prov, q, 2, "before any admit")
	if n, err := q.PopAdmit(ctx, 1, 2, pickZone1Gate); err != nil || n != 2 {
		t.Fatalf("popadmit = (%d, %v), want (2, nil)", n, err)
	}

	clk.Advance(2 * time.Second) // 客户端的下一次轮询
	consumedAt := clk.Now()
	var taken []string
	for token, queueID := range queueIDs {
		st, err := q.Lookup(ctx, token, stubSignFn)
		if err != nil {
			t.Fatalf("lookup: %v", err)
		}
		if st.Status == StatusAdmitted {
			taken = append(taken, queueID)
		}
	}
	if len(taken) != 2 {
		t.Fatalf("consumed %d admits, want 2", len(taken))
	}
	wantScore := float64(consumedAt.Add(q.handoffGrace).UnixMilli())
	for _, queueID := range taken {
		score, err := rdb.ZScore(ctx, admittedZSetKey(1), queueID).Result()
		if err != nil || score != wantScore {
			t.Fatalf("placeholder %s score = %.0f err=%v, want %.0f (consume time + handoff grace)",
				queueID, score, err, wantScore)
		}
	}

	mustFree(t, prov, q, 0, "right after consume, gate still reports 0 online")
	clk.Advance(q.handoffGrace - time.Millisecond)
	mustFree(t, prov, q, 0, "1ms before the handoff grace ends")
	clk.Advance(time.Millisecond)
	mustAdmittedCount(t, q, 0, "handoff grace elapsed")
	mustFree(t, prov, q, 2, "handoff grace elapsed, players never showed up")
}

// login 的钟比 Redis 快时,占位(login 钟)可能先于放行槽(Redis TTL)到期被清掉。
// 这时取走仍要给这个人占 15s:他确实马上要连 gate。所以是普通 ZADD,不是 XX。
func TestConsume_HoldsPlaceholderEvenIfAlreadyExpired(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil)", n, err)
	}
	clk.Advance(q.admitTTL) // miniredis 里放行槽的 TTL 不随假时钟走,槽还在
	mustAdmittedCount(t, q, 0, "placeholder expired by score")

	st, err := q.Lookup(ctx, res.QueueToken, stubSignFn)
	if err != nil || st.Status != StatusAdmitted {
		t.Fatalf("lookup = (%+v, %v), want ADMITTED", st, err)
	}
	mustAdmittedCount(t, q, 1, "consume re-holds the slot")
	score, err := rdb.ZScore(ctx, admittedZSetKey(1), res.QueueID).Result()
	if want := float64(clk.Now().Add(q.handoffGrace).UnixMilli()); err != nil || score != want {
		t.Fatalf("placeholder score = %.0f err=%v, want %.0f", score, err, want)
	}
}

// 清理是闭区间:分数 <= now 的成员算已到期。
func TestAdmittedCount_DropsOnlyExpiredMembers(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()
	mustAdmittedCount(t, q, 0, "key does not exist")

	now := clk.Now().UnixMilli()
	key := admittedZSetKey(1)
	if err := rdb.ZAdd(ctx, key,
		redis.Z{Score: float64(now - 1), Member: "expired"},
		redis.Z{Score: float64(now), Member: "expires-now"},
		redis.Z{Score: float64(now + 1), Member: "live"},
	).Err(); err != nil {
		t.Fatalf("seed: %v", err)
	}
	mustAdmittedCount(t, q, 1, "only the member with score > now is live")
	left, err := rdb.ZRange(ctx, key, 0, -1).Result()
	if err != nil || len(left) != 1 || left[0] != "live" {
		t.Fatalf("members after count = %v err=%v, want [live]", left, err)
	}
}

// 清理、判定、占位在一段 Lua 里:并发调用成功的个数恰好等于 budget。
func TestTryReserveFastPathSlot_ConcurrentNeverExceedsBudget(t *testing.T) {
	q, rdb, _ := newClockedQueue(t)
	ctx := context.Background()
	const budget, callers = 8, 64

	var granted atomic.Int32
	var wg sync.WaitGroup
	errs := make(chan error, callers)
	for range callers {
		wg.Add(1)
		go func() {
			defer wg.Done()
			ok, err := q.TryReserveFastPathSlot(ctx, 1, budget)
			if err != nil {
				errs <- err
				return
			}
			if ok {
				granted.Add(1)
			}
		}()
	}
	wg.Wait()
	close(errs)
	for err := range errs {
		t.Fatalf("reserve: %v", err)
	}
	if got := granted.Load(); got != budget {
		t.Fatalf("granted %d reservations, want exactly %d", got, budget)
	}
	if n, err := rdb.ZCard(ctx, admittedZSetKey(1)).Result(); err != nil || n != budget {
		t.Fatalf("placeholders = %d err=%v, want %d", n, err, budget)
	}
}

// 新版本只读写新键。旧键 admitted:zone:{z} 靠旧版本最后一次写入后的 TTL 自己消失。
func TestLegacyAdmittedSetKey_NeverWritten(t *testing.T) {
	q, rdb, _ := newClockedQueue(t)
	ctx := context.Background()
	prov := oneGateProvider(10, 0)

	if ok, err := q.TryReserveFastPathSlot(ctx, 1, 10); err != nil || !ok {
		t.Fatalf("reserve = (%v, %v), want (true, nil)", ok, err)
	}
	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	mustFree(t, prov, q, 9, "one fast-path placeholder")
	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil)", n, err)
	}
	if st, err := q.Lookup(ctx, res.QueueToken, stubSignFn); err != nil || st.Status != StatusAdmitted {
		t.Fatalf("lookup = (%+v, %v), want ADMITTED", st, err)
	}
	mustAdmittedCount(t, q, 2, "fast-path placeholder + consumed admit")

	keys, err := rdb.Keys(ctx, "admitted:*").Result()
	if err != nil || len(keys) != 1 || keys[0] != "admitted:zset:zone:1" {
		t.Fatalf("keys admitted:* = %v err=%v, want only admitted:zset:zone:1", keys, err)
	}
}

// Redis 出错时的方向不变:读不出在途占位就不放行。
func TestAdmittedAccounting_RedisDown_FailsClosed(t *testing.T) {
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis: %v", err)
	}
	// MaxRetries -1 关命令重试;DialerRetries 1 关拨号重试(缺省拨 5 次、每次失败后固定等 100ms)。
	// 下面三条会走到 Redis 的调用各拨号一次,每次最多等一个 DialTimeout。
	rdb := redis.NewClient(&redis.Options{
		Addr:               mr.Addr(),
		MaxRetries:         -1,
		DialTimeout:        200 * time.Millisecond,
		DialerRetries:      1,
		DialerRetryTimeout: time.Millisecond,
	})
	defer rdb.Close()
	q := New(rdb, time.Hour, 60*time.Second, []byte("test-secret"))
	ctx := context.Background()
	prov := oneGateProvider(10, 0)
	mr.Close()

	if _, err := q.AdmittedCount(ctx, 1); err == nil {
		t.Fatal("AdmittedCount must surface the Redis error")
	}
	if free, _, _, _, err := FreeSlots(ctx, prov, q, 1, 1.5); err == nil || free != 0 {
		t.Fatalf("FreeSlots = (free=%d, err=%v), want (0, error): no accounting, no admission", free, err)
	}
	if ok, err := q.TryReserveFastPathSlot(ctx, 1, 5); err == nil || ok {
		t.Fatalf("TryReserveFastPathSlot = (%v, %v), want (false, error)", ok, err)
	}
	// budget 0 不碰 Redis:Redis 挂着也不报错。
	if ok, err := q.TryReserveFastPathSlot(ctx, 1, 0); err != nil || ok {
		t.Fatalf("TryReserveFastPathSlot(budget=0) = (%v, %v), want (false, nil)", ok, err)
	}
}

// 取走时占位记账失败不让这次取走作废(票已签好);但之后读不出记账,容量判定一律不放行。
func TestConsume_PlaceholderWriteFailureDoesNotFailAdmit(t *testing.T) {
	q, rdb, _ := newClockedQueue(t)
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil)", n, err)
	}
	// 把占位键换成 STRING:取走时那次 ZADD 必然 WRONGTYPE。
	key := admittedZSetKey(1)
	if err := rdb.Del(ctx, key).Err(); err != nil {
		t.Fatalf("del: %v", err)
	}
	if err := rdb.Set(ctx, key, "not-a-zset", 0).Err(); err != nil {
		t.Fatalf("set: %v", err)
	}

	st, err := q.Lookup(ctx, res.QueueToken, stubSignFn)
	if err != nil || st.Status != StatusAdmitted || st.Admit == nil {
		t.Fatalf("lookup = (%+v, %v), want ADMITTED despite the accounting failure", st, err)
	}
	if _, err := q.AdmittedCount(ctx, 1); err == nil {
		t.Fatal("AdmittedCount must fail on a wrong-type key")
	}
	if ok, err := q.TryReserveFastPathSlot(ctx, 1, 5); err == nil || ok {
		t.Fatalf("TryReserveFastPathSlot = (%v, %v), want (false, error)", ok, err)
	}
}

// 取走只发生一次。取走之后客户端按协议还会每 2s 轮询(回「排队中、名次 0」,直到条目 1h 过期),
// 这些轮询不能给占位续期:否则每次续 15s,占位永不到期,名额一直被扣着。
func TestConsume_RepeatPollDoesNotExtendPlaceholder(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil)", n, err)
	}
	if st, err := q.Lookup(ctx, res.QueueToken, stubSignFn); err != nil || st.Status != StatusAdmitted {
		t.Fatalf("first lookup = (%+v, %v), want ADMITTED", st, err)
	}
	want := float64(clk.Now().Add(q.handoffGrace).UnixMilli())

	clk.Advance(5 * time.Second)
	st, err := q.Lookup(ctx, res.QueueToken, stubSignFn)
	if err != nil {
		t.Fatalf("repeat lookup: %v", err)
	}
	if st.Status == StatusAdmitted {
		t.Fatal("admit was consumed twice")
	}
	score, err := rdb.ZScore(ctx, admittedZSetKey(1), res.QueueID).Result()
	if err != nil || score != want {
		t.Fatalf("placeholder score after a repeat poll = %.0f err=%v, want %.0f (first consume + handoff grace, not extended)",
			score, err, want)
	}
	mustAdmittedCount(t, q, 1, "5s after consume")

	clk.Advance(q.handoffGrace - 5*time.Second)
	mustAdmittedCount(t, q, 0, "handoff grace elapsed since the first consume")
}

// signFn 失败:放行槽已被 GETDEL 删掉,这个人拿不到票;占位不改分数,保持「放行时刻 + AdmitTTL」,到点自己消失。
func TestConsume_SignFailureKeepsAdmitTimeScore(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	admittedAt := clk.Now()
	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil)", n, err)
	}
	want := float64(admittedAt.Add(q.admitTTL).UnixMilli())

	clk.Advance(2 * time.Second) // 客户端的下一次轮询
	failSign := func(*AdmitSlot) (*AdmitToken, error) { return nil, context.DeadlineExceeded }
	if st, err := q.Lookup(ctx, res.QueueToken, failSign); err == nil {
		t.Fatalf("lookup = (%+v, nil), want the signing error", st)
	}
	score, err := rdb.ZScore(ctx, admittedZSetKey(1), res.QueueID).Result()
	if err != nil || score != want {
		t.Fatalf("placeholder score after a failed sign = %.0f err=%v, want %.0f (admit time + AdmitTTL, untouched)",
			score, err, want)
	}
	// 放行槽已经没了:再来轮询也拿不到票。
	if st, err := q.Lookup(ctx, res.QueueToken, stubSignFn); err != nil || st.Status == StatusAdmitted {
		t.Fatalf("lookup after a failed sign = (%+v, %v), want not ADMITTED and no error", st, err)
	}

	clk.Advance(q.admitTTL - 2*time.Second - time.Millisecond)
	mustAdmittedCount(t, q, 1, "1ms before AdmitTTL")
	clk.Advance(time.Millisecond)
	mustAdmittedCount(t, q, 0, "at AdmitTTL after a failed sign")
}
```

T14 的失败签名器沿用既有用例的写法（`queue_test.go:313-315` 用 `context.DeadlineExceeded` 当错误值），不用为它新增 `errors` import。

### 6.4 `dispatcher_test.go` 的改动

**1）`startIntegrationFixture`**：在 `q := New(rdb, time.Hour, 60*time.Second, secret)`（`:48`）之后加：

```go
	// 这个 fixture 的两个用例(QueueDrainAdmits / DispatcherRespectsCapacity)里 PlayerCount 是写死的,
	// 靠「取走即腾出名额」推进。交票后宽限(生产 15s)在这里设 0:取走时分数 = 现在,下一次计数就清掉,
	// 与 G4b-G 之前的行为等价。宽限本身由 TestConsume_* 与
	// TestDispatchOnce_ConsumedAdmitsHoldCapacityForHandoffGrace 用假时钟钉住。
	q.handoffGrace = 0
```

为什么不保留生产宽限：`TestIntegration_QueueDrainAdmits`（`:110-160`）容量 2、`PlayerCount` 恒 0、要在 3s 内放完 5 人，取走后占位再留 15s 就必然超时。`TestIntegration_DispatcherRespectsCapacity`（`:209-222`）每轮把计数清零重数，某一轮恰好只看到 1 个已放行时，旧语义靠「取走即释放、再放一批」凑够 2 个；保留 15s 的话这种交错下会偶发超时。设 0 之后这两个用例的时序与今天完全相同，不引入新的偶发性。

**2）`TestIntegration_QueueDrainAdmits` 的注释**（`:135-136`）改为：

```go
	// Dispatcher should drain all 5 within ~1s (capacity 2 + dispatcher
	// PopAdmit each tick; the fixture sets handoffGrace=0, so a consumed
	// admit frees its slot at the next count).
```

**3）`TestIntegration_DualLeadersSameZone_NoLossBoundedOverAdmit` 的 `snapshot`**（`:409-413`）：

```go
	snapshot := func() (queued, admitted []string) {
		queued, _ = rdb.ZRange(ctx, zoneZSetKey(1), 0, -1).Result()
		admitted, _ = rdb.ZRange(ctx, admittedZSetKey(1), 0, -1).Result()
		return queued, admitted
	}
```

这个用例没有人取走，占位分数是「放行时刻 + 60s」，用例时长内不会到期；每个 leader 仍按自己读到的占位数算余量，`≤ 2×cap` 的推导不变。它用 `New` 的生产宽限，不需要改别的。

**4）新增用例**（接在 `TestDispatchOnce_EmptyQueueSkipsGateDirectory` 之后）：

```go
// G4b-G(Q2 回归,dispatcher 层):取走放行槽后,gate 要到下一次上报才把人数算进 PlayerCount。
// 这几拍里占位必须留着,否则每一拍都按「在线 0、在途 0」再放一满批。
func TestDispatchOnce_ConsumedAdmitsHoldCapacityForHandoffGrace(t *testing.T) {
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis: %v", err)
	}
	defer mr.Close()
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	defer rdb.Close()
	secret := []byte("g4bg")
	q := New(rdb, time.Hour, 60*time.Second, secret)
	clk := newFakeClock()
	q.now = clk.Now
	ctx := context.Background()

	prov := &integrationProvider{
		caps:  map[uint32]uint32{1: 2},
		cands: map[uint32][]GateCandidate{1: {{NodeID: 1, IP: "10.0.0.1", Port: 9000, ZoneID: 1}}},
	}
	// 不 Start:直接调 dispatchOnce,没有 goroutine,结果确定。
	d := mustNewDispatcher(t, q, prov, rdb, secret, "dispatcher:lock:g4bg", 1)

	tokens := make([]string, 0, 6)
	for range 6 {
		res, err := q.Enqueue(ctx, 1, "u", "d")
		if err != nil {
			t.Fatalf("enqueue: %v", err)
		}
		tokens = append(tokens, res.QueueToken)
	}
	mustQueueLen := func(want uint32, when string) {
		t.Helper()
		n, err := q.QueueLen(ctx, 1)
		if err != nil || n != want {
			t.Fatalf("queue len (%s) = %d err=%v, want %d", when, n, err, want)
		}
	}

	d.dispatchOnce(ctx)
	mustQueueLen(4, "first tick admits exactly the capacity")

	taken := 0
	for _, token := range tokens {
		st, err := q.Lookup(ctx, token, stubSignFn)
		if err != nil {
			t.Fatalf("lookup: %v", err)
		}
		if st.Status == StatusAdmitted {
			taken++
		}
	}
	if taken != 2 {
		t.Fatalf("consumed %d admits, want 2", taken)
	}

	// gate 还没上报的 5 拍:一个都不能再放。
	for range 5 {
		clk.Advance(time.Second)
		d.dispatchOnce(ctx)
	}
	mustQueueLen(4, "ticks before the gate reports: consumed admits still hold their slots")

	// 宽限过去,这两个人始终没出现在 PlayerCount 里:名额放回,再放一批。
	clk.Advance(q.handoffGrace)
	d.dispatchOnce(ctx)
	mustQueueLen(2, "handoff grace elapsed")
}
```

`fakeClock` / `newFakeClock` 定义在 `queue_test.go`，同包可见；`dispatcher_test.go` 不需要新增 import。

### 6.5 红用例（只在 HEAD 旧 API 上跑，不入库）

只用 HEAD 上已有的导出面与测试助手（`newTestQueue`、`fakeProvider`、`stubSignFn`），证明 Q1、Q2 在修复前确实是红的。放进临时 worktree 跑，跑完删掉。

```go
package loginqueue

import (
	"context"
	"testing"
	"time"
)

// Q1:每秒 1 人走快速通道,10 秒后出现在 PlayerCount 里。旧实现第 55 秒余量归零。
func TestG4bGRepro_Q1_SustainedFastPathStarvesBudget(t *testing.T) {
	q, _, mr, cleanup := newTestQueue(t)
	defer cleanup()
	ctx := context.Background()
	const capacity = 100
	prov := &fakeProvider{
		cands: map[uint32][]GateCandidate{1: {{NodeID: 1, IP: "10.0.0.1", Port: 9000, ZoneID: 1}}},
		caps:  map[uint32]uint32{1: capacity},
	}
	for sec := 0; sec < 60; sec++ {
		online := uint32(0)
		if sec > 10 {
			online = uint32(sec - 10)
		}
		prov.cands[1][0].PlayerCount = online
		free, _, _, admitted, err := FreeSlots(ctx, prov, q, 1, 1.5)
		if err != nil {
			t.Fatalf("sec=%d freeslots: %v", sec, err)
		}
		if free == 0 {
			t.Fatalf("sec=%d: free hit 0 with %d online of %d (admitted=%d)", sec, online, capacity, admitted)
		}
		ok, err := q.TryReserveFastPathSlot(ctx, 1, capacity-online)
		if err != nil || !ok {
			t.Fatalf("sec=%d: fast-path reservation refused (ok=%v err=%v)", sec, ok, err)
		}
		mr.FastForward(time.Second) // 旧实现的整集合 TTL 每次占位都续上,60s 永远走不完
	}
}

// Q2:容量 2,放行 2 人并取走;gate 还没上报(PlayerCount 仍是 0)。旧实现取走即 SREM,余量立刻回到 2。
func TestG4bGRepro_Q2_ConsumeFreesSlotBeforeGateReports(t *testing.T) {
	q, _, _, cleanup := newTestQueue(t)
	defer cleanup()
	ctx := context.Background()
	prov := &fakeProvider{
		cands: map[uint32][]GateCandidate{1: {{NodeID: 1, IP: "10.0.0.1", Port: 9000, ZoneID: 1}}},
		caps:  map[uint32]uint32{1: 2},
	}
	var tokens []string
	for range 6 {
		res, err := q.Enqueue(ctx, 1, "u", "d")
		if err != nil {
			t.Fatalf("enqueue: %v", err)
		}
		tokens = append(tokens, res.QueueToken)
	}
	pick := func(z uint32, _ string) (*AdmitSlot, error) {
		return &AdmitSlot{NodeID: 1, IP: "10.0.0.1", Port: 9000, ZoneID: z}, nil
	}
	if n, err := q.PopAdmit(ctx, 1, 2, pick); err != nil || n != 2 {
		t.Fatalf("popadmit = (%d, %v), want (2, nil)", n, err)
	}
	taken := 0
	for _, token := range tokens {
		st, err := q.Lookup(ctx, token, stubSignFn)
		if err != nil {
			t.Fatalf("lookup: %v", err)
		}
		if st.Status == StatusAdmitted {
			taken++
		}
	}
	if taken != 2 {
		t.Fatalf("consumed %d admits, want 2", taken)
	}
	free, _, online, admitted, err := FreeSlots(ctx, prov, q, 1, 1.5)
	if err != nil {
		t.Fatalf("freeslots: %v", err)
	}
	if free != 0 {
		t.Fatalf("free = %d right after consume (online=%d admitted=%d): the next tick admits another full batch "+
			"while the gate still reports 0 online", free, online, admitted)
	}
}
```

HEAD 上的预期：Q1 在 `sec=55` 以 `free hit 0 with 45 online of 100 (admitted=55)` 失败（第 `k` 秒 `admitted = k`、`online = k − 10`，`100 − (k − 10) − k = 0` 解得 `k = 55`）；Q2 以 `free = 2 right after consume (online=0 admitted=0)` 失败。

---

## 7. 文档改动清单

### 7.1 本批必须改（与代码同批，已在 §4.6 给出原文）

`capacity.go`、`metrics.go` 的注释与帮助文本；`config/config.go:255-259`；`etc/login.yaml` 的上线清单第 0 步与 `Queue:` 上方那段注释（HEAD `:436-438`、`:445-450`）。它们写的是键的类型、指标口径、`AdmitTTL` 的含义和打开排队前的升级顺序，代码改了不同步就是错的。

### 7.2 文档阶段同步（由主会话的 Docs 阶段做）

1. **`docs/design/login-queue-2026-05.md`**
   - 头部 `:7` 之后加一行：`> **修订**: 2026-10-13 G4b-G — 在途占位改为逐个到期的 ZSET,交票后保留 15s(§5「在途占位的生命周期」)`。
   - `:73` 键布局改为 `admitted:zset:zone:{zoneId}  ZSET member=queueId | fast:<uuid>, score=占位到期毫秒(逐个到期;整键 TTL 只做垃圾回收)`。
   - `:162` 改为 `admitted_inflight = 清掉过期成员后的 ZCARD admitted:zset:zone:{z}`；`:166`「关键安全属性」后补一句：取走后占位再留 15s，覆盖「票已交出、gate 还没把人数报上来」这一段。
   - `:180-181` 伪码改为 `ZADD admitted:zset:zone:{zoneId} <now_ms + AdmitTTL> queueId` 与 `PEXPIRE … <整键垃圾回收 TTL>`。
   - 在「FreeSlots 公式」与「Dispatcher Loop」之间新增小节 **「在途占位的生命周期（G4b-G，2026-10-13）」**：内容取本规格 §2 的表、§4.2 的时钟选择与代价（含「故障副本恢复后残留 δ」与止血命令）、§4.3 的 15s 依据、§5.2 的算式与「放行节奏的上界」、§5.5 的混跑与部署顺序。§5.5 的结论句要原样带上后半句「leader 或经手的副本是旧版期间 Q1 / Q2 仍在，不能把混跑窗口当作 G4b-G 已生效」，不能只抄「额外多放 ≤ 一个 `C − O`」。
   - `:244`「每个 leader 都按自己读到的 `SCARD admitted` 算余量，自己上一拍的 SADD…」改成「…读到的在途占位数…自己上一拍写入的占位…」；结论不变。
   - §11「健康范围速查」`:319-321`：「`q_admitted` 增速接近 zone capacity / s」「长时间停滞 → dispatcher 卡死」「1000/s admit 速率下」三处改口径：增速上界是「在线玩家的离开速率，加上拿了票没留在线上的那部分名额每约 17s 周转一次」；`在线 + login_queue_admitted_inflight ≥ capacity` 时 `q_admitted` 不涨是正常回压，停滞要同时满足「`在线 + 在途 < capacity`」才判 dispatcher 卡死。数字标注「待 L-14 标定」。
   - `:251`「`admit:*` 与 `admitted:zone:S` 在 `AdmitTTL`（60s）内过期」改为「`admit:*` 在 `AdmitTTL`（60s）内过期，`admitted:zset:zone:S` 在最后一次写入后 `AdmitTTL + 1min`（默认 2 分钟）内整键过期」。
   - §10 配置段（`:287-300`）给 `AdmitTTL` 的注释补「取走后只再留 15s（代码常量，不是配置）」。
   - §14 已知不足登记本规格 §5.8 的第 1、2 条。
   - `:384`、`:391` 的清理命令把 `admitted:zone:1` / `admitted:zone:<S>` 换成 `admitted:zset:zone:…`，并注明「旧键 `admitted:zone:*` 是 2026-10-13 之前版本的残留，60s 内自行过期」。
2. **`docs/ops/login-queue-stress-runbook.md`**
   - **放行节奏的判读（`:35-55` 核心健康指标表、健康样例与「关键观察」）**：标注「这些节奏数字写于 G4b-G（2026-10-13）之前，待 L-14 重新标定」，并写明新的上界：放行的人取走后 15s 内名额不放回，之后他若已在线，名额转给在线数；`在线 + login_queue_admitted_inflight ≥ capacity` 时 `q_admitted` 不涨是**正常回压，不是模式 1**。`:40` 的红线「上升停滞 = dispatcher 卡死」、`:44` 的「`login_ok` 增速接近 zone capacity / login_avg」、`:55` 的「`q_admitted` 每 30s 涨 1000±10（zone capacity）」三处按此改写（依据：本规格 §5.2「放行节奏的上界」）。
   - **模式 1 的排查项（`:66-71`）**：最前面加一条「先看 `login_queue_admitted_inflight` 与 gate `PlayerCount` 之和是否已到 capacity；到了就是正常回压，不往下查」；`:70`「有 leader 但无 admitted 日志 = `FreeSlots` 永远返回 0」补同一句前提。
   - `:78-81`（模式 2）重写：根因句改为「在途占位没有覆盖『票已交出、gate 还没上报』这一段」；命令改为 `redis-cli ZRANGE admitted:zset:zone:1 0 -1 WITHSCORES`（分数是到期毫秒，`fast:` 开头的是快速通道占位）；阈值改为「稳态约等于交票速率 × 15s；长时间顶在 `capacity − online` 而 gate 人数不涨 = 客户端拿了放行没去连 gate」。再加一条**反方向（少放）的排查**：`ZRANGE … WITHSCORES` 里有分数比当前毫秒时间戳大出 `AdmitTTL`（默认 60s）以上的成员 = 某个 login 副本的钟快过（本规格 §4.2），它恢复后这些占位还要留满偏差量才到期；止血 `redis-cli DEL admitted:zset:zone:<z>`，代价是最多多放一批 `capacity − online`。
   - `:118`（模式 6）命令清单里的 `SCARD` 改 `EVAL(清过期 + ZCARD)`，`N×SADD` 改 `N×(ZADD + PEXPIRE)`。
   - `:137`「期望：11 个 PASS」改成不写死个数（G1b 之后已经不准）。
   - **验证清单 #3（`:149`）**：不写死「`q_admitted` 在 100s 内涨到 100」。改为「后 100 个入队；`q_admitted` 的涨幅 ≤ 容量 − 在线：robot 保持在线时不涨属正常（先核对在线 + 在途是否已到 capacity），robot 下线后每腾出一个名额放行一个」。
   - `:177` 指标表改为「在途占位数（清过期后的 ZCARD）：已放行未取 + 交票 15s 之内」。
   - **推荐告警（`:189-190`）**：「队列在涨但 admit 不涨（dispatcher 卡死）」那条注明「zone 满员（在线 + 在途 ≥ capacity）时同样成立，属正常回压；判卡死要再加『在途 + 在线 < capacity』」。仓库里没有落地的告警规则（F14），只改文字。
   - **故障应急（`:217-219`）**：补升级顺序要求「打开 `Queue.Enabled` 之前，所有 zone 的 login 先在排队关闭下升到含 G4b-G 的版本；排队开着混跑时旧版副本经手的部分仍按旧版超发」（与 `login.yaml` 上线清单第 0 步同一句）。
   - `:226` 清理命令同设计稿。
3. **`docs/design/shared-redis-split-by-purpose.md`**：只改两处。
   - `:65` 键族行：`admitted:zone:{z}` SET → `admitted:zset:zone:{z}` ZSET（成员逐个到期）；多键原子一栏改为「放行 MULTI；计数与快速通道是单键 Lua；取走后续期是单键 MULTI」，行号按落码后的实际位置填。
   - `:718`「等 `admitted:*` 过期（AdmitTTL 60s）」改为「（AdmitTTL + 1min，默认 2 分钟）」。
   - `:149`、`:229`、`:763` 的行号与「读 SCARD」「快速通道 1 次 EVAL」措辞，留给交接 §4 第 5 项（拆 Redis 设计整体修订，不在本轮顺序里）一并改；通配 `admitted:*`（`:593`）仍然覆盖新键，不用动。
4. **`docs/handoff/launch-readiness-20261013/spec_G4b.md`**（行号以 2026-10-13 的文件为准，改前重读）
   - `:746`（§10 #11 方案 A）末尾补「2026-10-13 已拍板，规格见 `spec_G4b-G.md`」。
   - `:724`（R15）的「观察 `admitted:zone:*` 的 SCARD」改为新键的 ZCARD（旧通配匹配不到 `admitted:zset:zone:*`）。
   - **`PARITY.md` 行号**：`spec_G4b.md` 是把两行整体写小了 1（它用 98 指「登录排队」、用 99 指「开服限流」；Java 仓库现在这两行是 99 和 100，第 98 行是「区服白名单」，HEAD 与工作区一致）。只把排队那几处 98 改成 99 会让同一份文档里的「99」同时指两行，所以正文 8 处一起改：
     - 指「登录排队」行的 `PARITY.md:98` → `:99`：`:90`、`:764`；
     - 指「开服限流」行的 `PARITY.md:99` → `:100`：`:512`（有意差异 ④⑧②）、`:702`（④）、`:723`（⑨）、`:756`；
     - `PARITY.md:98-99` → `:99-100`：`:755`、`:765`。
     - §13 评审处理记录里的 4 处（`:809`、`:816`、`:818`、`:844`）是历史快照，不改；在 §13 标题下加一句「本节的 `PARITY.md` 行号比该文件现状小 1：登录排队 = 99，开服限流 = 100（2026-10-13 核对）」。
     - Java 仓库的 `PARITY.md` 眼下还有别的会话在追加行（工作区 `git status` 为 ` M`，改动在 `:150` 之后），行号会再漂。**凡是新写的文字**（PROGRESS 条目、设计稿、`PARITY.md` 备注、本规格）一律按行名「登录排队」行 /「开服限流」行引用，不写行号。
   - **契约用例 15（`:620`）**：断言补「`go/login/internal/logic/pkg/loginqueue/queue.go` 含 `admitted:zset:zone:%d`，且不含 `"admitted:zone:%d"`」，用例名补成「G4b-2 前置：G1b 与 G4b-G 已合入」。把「G4b-G → G4b-2」的顺序变成机械门禁（本规格 §5.5、R3 的缓解靠它；不登记到这里，G4b-2 的实现者看不到）。
   - **合入门槛（`:674`）**：注明「2026-10-13 已选方案 A：『或负责人书面接受』这一支作废，只剩 G4b-G 由 Codex 跑绿这一条」；并补一句「打开排队之前，新脚本至少在真实 Redis 上跑过一次（`spec_G4b-G.md` §8.4），是否作为硬门槛见该规格 §11 #5」。
5. **`docs/design/capacity-model-hok-scale.md:267`、`:784`**：引的是 `queue.go:226-284`（`Lookup`）。内容不用改（每次轮询的命令条数不变：SREM 换成一个 MULTI，仍是一次往返）；`queue.go` 前半段加了约 30 行，落码后把行号核对一遍。
6. **`docs/PROGRESS.md`**：末尾**另起**条目（不回头改 `:2456-2464` 的旧条目，可以引用）。写明：未提交、未编译、待 Codex 验证；`runbook` 验证清单第 2–5 步（起栈的 robot 冒烟与故障演练）**本批未执行**，真实 Redis 首跑的归属见 `spec_G4b-G.md` §8.4；Java 版状态取 §12 的结论与版本号；引用 `PARITY.md` 时写行名。
7. **Java 仓库 `PARITY.md`「登录排队」行**：改法见 §12。

新规格文件放在 `docs/handoff/launch-readiness-20261013/`，`docs/README.md` 不用登记：索引生成器只列各目录下一层的 `.md`（`tools/scripts/gen_docs_index.py:118-125`），同目录现有 8 份规格都没有单独登记。

### 7.3 核对后不需要动的（逐项）

- **合服工具** `tools/merge_zone/`：全目录 grep `admit` / `queue:zone` / `login_queue` 零命中，旧键从没登记过。新键靠整键 TTL 自清。
- **zone 回档脚本** `tools/scripts/k8s_zone_rollback.ps1:633-642`（`Get-SharedRedisGuardPatterns`）：六个 pattern 都是丢了就写错库的全局键，不含任何排队键；排队键按设计不属于守卫对象。
- **压测清库脚本**（`stress-linux-tier.sh`、`deploy-staging.sh`、`gateway-mock-stress.sh`）：grep `queue:` / `admit` 零命中。
- **`tools/scripts/k8s_deploy.ps1`、`deploy/`**：没有 `Queue` 段，没有任何 `login_queue_*` 告警或面板。
- **robot、`java/gateway_node`**：只走 HTTP / gRPC，不碰排队键；回包码不变。
- **`.github/workflows`**：会跑 go/login 的测试，配置不用改。
- **历史规格快照** `spec_G1b.md`、`spec_G4a.md`：只读核对产物，不回改。

---

## 8. 交给 Codex 的验证

不需要清 Redis / Kafka / MySQL，不需要起任何服务，不下载任何东西。全部是 Go 单测，Windows PowerShell。本批**不改 `go.mod` / `go.sum`**。C++、Java、proto、PowerShell 都没改，不需要 MSBuild / mvn / build.bat。

执行顺序固定：**8.0 前置（先主会话记落码前快照，后 Codex 建基线）→ 8.1 红 → 8.2 绿 → 8.3 清理**。HEAD worktree 在 8.0 建、8.3 删，中间三步都要用它，不要提前删。

### 8.0 前置（必做：8.1 的红用例与 8.2 的各项基线都依赖它）

**(a) 落码前快照：主会话在动第一个文件之前记录**（三条都是只读 git 命令，Claude 可以跑；输出原文贴进交给 Codex 的说明）。Codex 是在落码之后才开跑的，那时工作区里别人的改动与本批的改动已经混在一起，分不出来；只有这份快照能把两者分开。

```powershell
git -C D:\luyuan\xuanming-server-mmo-cpp rev-parse HEAD
git -C D:\luyuan\xuanming-server-mmo-cpp status --short -- go/login
git -C D:\luyuan\xuanming-server-mmo-cpp diff -- go/login/go.mod go/login/go.sum go/login/etc/login.yaml
```

- 快照里已经在列的文件都是别的会话的。本批的 7 个文件里凡是已经在列的（09:07 时是 `etc/login.yaml`），本批的改动叠在别人的改动上（§4.6 按内容锚点定位），那份文件的 diff 快照就是「别人的部分」。
- `loginqueue/` 下的 5 个文件或 `config/config.go` 若已经在列，说明别的会话也在改排队代码：本规格的行号与照写代码都可能对不上，停下，先向用户确认，不要落码。

**(b) Codex 开跑时：**

```powershell
# 1) 记录环境与工作区现状(原文留进报告)
go version
go env GOTOOLCHAIN GOPROXY GOFLAGS
git -C D:\luyuan\xuanming-server-mmo-cpp rev-parse HEAD
git -C D:\luyuan\xuanming-server-mmo-cpp status --short -- go/login

# 2) 本次 PowerShell 会话内禁止 go 自行下载工具链与模块(只影响当前会话,不改系统设置)
$env:GOTOOLCHAIN = 'local'
$env:GOPROXY = 'off'

# 3) 建 HEAD 基线 worktree:钉规格所依据的提交,不用符号 HEAD(工作区是几个会话共用的,HEAD 可能已经移动)
git -C D:\luyuan\xuanming-server-mmo-cpp worktree add --detach D:\luyuan\g4bg-red 0aa1027bc

# 4) gofmt 基线(输出原文留存;没有输出也要记「基线为空」)
cd D:\luyuan\g4bg-red\go\login
gofmt -l internal\logic\pkg\loginqueue internal\config
# 对上一条列出的、且属于本批要改的文件(预计含 metrics.go),逐个留存差异:
gofmt -d internal\logic\pkg\loginqueue\metrics.go
```

- **工具链**：`go/login/go.mod` 的 `go` 指令已被别的会话改成 `1.27.2`（HEAD 是 `1.24.5`）。本机 `C:\Program Files\Go\VERSION` 写的是 `go1.27.2`（只读文件得到的线索；没有运行 `go version`，Codex 用的是不是这一份、在不在 PATH 上，**未核实**）。`go version` 低于 `go.mod` 的要求、或任何一条 go 命令提示要下载工具链 / 模块（例如 `go.mod requires go >= 1.27.2`、`module lookup disabled by GOPROXY=off`）时：**停下报告，不自行下载、不改 `go.mod`**，归因到别的会话的 `go.mod` 改动或本机模块缓存不全，不属于本批。`GOTOOLCHAIN=local` / `GOPROXY=off` 的语义凭记忆，**未核实**；这两个变量本身导致报错时，去掉后重跑一次并在报告里注明，出现下载提示仍然停下。
- **HEAD 已经不是 `0aa1027bc`**（别的会话提交过）：worktree 仍钉 `0aa1027bc`，报告里写明当前 HEAD；若本批要改的 7 个文件在新 HEAD 上已经变了（`git diff 0aa1027bc HEAD -- go/login/internal/logic/pkg/loginqueue go/login/internal/config/config.go go/login/etc/login.yaml` 有输出），本规格的行号与基线失效，停下报告。
- worktree 是纯 `0aa1027bc`：`go.mod` 是 `go 1.24.5`，也**不含**别的会话在 `go/login` 下的任何未提交改动。8.1 的红与 8.2 的绿用同一个工具链、不同的 `go` 指令，报告里分别写明。

### 8.1 红（必做，证明复现；在 8.0 的 worktree 里）

```powershell
# 把本规格 §6.5 的代码写成 D:\luyuan\g4bg-red\go\login\internal\logic\pkg\loginqueue\g4bg_repro_test.go
cd D:\luyuan\g4bg-red\go\login
go test ./internal/logic/pkg/loginqueue/ -run TestG4bGRepro -count=1 -v
```

**通过标准**：两个用例都 **FAIL**，信息分别是 `sec=55: free hit 0 with 45 online of 100 (admitted=55)` 和 `free = 2 right after consume (online=0 admitted=0)`。任何一个 PASS，说明复现前提不成立，停下来报告，不继续下一步。保留两条 FAIL 的原文。跑完**不要删 worktree**。

### 8.2 绿（工作区，落码之后）

同一个 PowerShell 会话（8.0 设的两个环境变量还在），工作目录 `D:\luyuan\xuanming-server-mmo-cpp\go\login`：

```powershell
git status --short -- .
git diff -- go.mod go.sum etc/login.yaml
gofmt -l internal\logic\pkg\loginqueue internal\config
# 对 8.0 基线里已列出的本批文件(预计含 metrics.go)再留一份差异,与 8.0 的那份对比:
gofmt -d internal\logic\pkg\loginqueue\metrics.go
go vet ./internal/logic/pkg/loginqueue/... ./internal/config/... ./internal/logic/loginpregate/... ./internal/svc/...
go build ./...
go test ./internal/logic/pkg/loginqueue/... -count=1 -v -run "TestNew_|TestPopAdmit_|TestTryReserveFastPathSlot_|TestFastPath_|TestConsume_|TestAdmittedCount_|TestLegacyAdmittedSetKey_|TestAdmittedAccounting_|TestDispatchOnce_"
go test ./internal/logic/pkg/loginqueue/... -count=10 -run "TestTryReserveFastPathSlot_Concurrent|TestIntegration_"
go test ./internal/logic/pkg/loginqueue/... ./internal/svc/... ./internal/config/... ./internal/logic/loginpregate/... -count=1
go test ./... -count=1
```

**通过标准**：

- **本批的改动范围**（对照 8.0(a) 的落码前快照）：
  - `git status --short -- .` 比快照多出来的，只应是本批的文件：`internal/logic/pkg/loginqueue/{queue,capacity,metrics}.go`、`{queue,dispatcher}_test.go`、`internal/config/config.go`，以及 `etc/login.yaml`（若快照里它还不在列）。
  - `etc/login.yaml` 在快照里已经在列时：`git diff -- etc/login.yaml` 比快照里那份 diff 多出来的 hunk，只应是 §4.6 的两处（上线清单第 0 步 3 行、`Queue:` 上方 6 行）；快照里原有的 hunk 一行不少。
  - 快照里就已经在列的文件是别的会话的，**不计入本批、不并入本批的 diff 摘要、不还原**。它们相对快照又有新变化（别的会话在这期间继续改），记录下来报告，不处理。
- **`go.mod` / `go.sum` 没被本批动过**：`git diff -- go.mod go.sum` 与快照里那份相同（09:07 时只有别的会话那一行 `-go 1.24.5` / `+go 1.27.2`，`go.sum` 无差异）。多出任何一行都停下报告。
- **gofmt 不超出基线**（基线 = 8.0(b) 在 worktree 里的那份输出）：
  - 基线里**已经**列出的文件（预计含 `metrics.go`）：一律**不** `gofmt -w`，哪怕本批改过它。对其中属于本批的文件比较两份 `gofmt -d`：差异块应当一一对应（本批在前面加了行时，后面的块行号整体平移属正常；`metrics.go` 包注释那一块里第 10 行的文字是本批改的，也属正常）。出现基线里没有的**新差异块**，说明本批新写的行格式不对：停下报告，由 Claude 按报告改那几行，不整文件格式化。
  - 基线里**没有**、现在被列出的文件，分两种：属于本批的 6 个 Go 文件，允许对它 `gofmt -w` 后重跑，并在报告里注明文件名与改动行；**不属于本批的**（别的会话改过的，例如 `internal\config\new_player_seed_conf_test.go`），只记录、不 `gofmt -w`、不算本批不通过。
- vet 和 build 没有错误。报错位置落在落码前快照里别的会话改过的文件上时，按下面 `go test ./...` 的同一套规则归因（报告原文，不修、不回退）；落在本批的文件上就是本批的问题。
- §6.2 的 T1–T14 全部 PASS；`TestIntegration_*` 与并发用例跑 10 遍稳定，没有偶发失败。
- `-v` 输出里不能出现 `login.queue_dispatcher` 的 panic 记录（safego 会接住 panic，用例本身不会因此失败）。
- `go test ./...`：`loginqueue` 包必须全绿（这个包里只有本批的改动）。`config`、`loginpregate`、`svc` 三个包本批只改了注释或没改，按理不受影响，但 09:07 的快照里 `svc/servicecontext.go` 与 `config/` 下一个测试文件已有别的会话的改动，所以它们与其余包一样按下面的规则归因。有包失败时，**先不要删 worktree**，在 `D:\luyuan\g4bg-red\go\login` 下跑同一条命令对照：
  - worktree 上同样失败：记为既有问题，不修。
  - 只在工作区失败：worktree 不含别的会话的改动，所以先看失败的包里有没有落码前快照中别的会话改过的文件，或报错是否与 `go` 指令（1.24.5 对 1.27.2）有关；是，就归到那批改动，报告原文，不修、不回退。失败的用例是排队相关的（`TestInitLoginQueue_*`、`loginqueue` 包内任何用例），或者说不清归谁，都按本批的失败处理：停下报告。
- 本机有 cgo 工具链时，对 loginqueue 加跑一次 `-race`；没有就省略并在报告里写明。
- 报告里写明：`go version` 原文；红（worktree，`go 1.24.5` 指令）与绿（工作区，`go 1.27.2` 指令）各自的结果；是否设了 `GOTOOLCHAIN` / `GOPROXY`。

**失败时保留**：失败用例的完整 `-v` 输出；`go vet` / `go build` 的报错原文；`git status --short -- go/login` 与 `git diff --stat -- go/login`（落码前快照里已在列的文件不属于本批，摘要里分开写）。命令失败后不连续自动重试（AGENTS §10.2）。

**兜底说明**（不要自行绕过）：两段脚本用到的 miniredis / go-redis 行为已对模块缓存里的源码逐条核过（§13 U1–U4），按理不会因为测试替身的差异而红。若仍出现 Lua 桥不认某条命令，或 `WRONGTYPE`、`PTTL`、`TYPE` 的行为与 §13 写的不符（例如实际编译用的不是缓存里这一份），保留报错原文并停下，不升级 miniredis（那是改 `go.mod`），等负责人决定。

### 8.3 清理（8.2 的全部命令与对照做完之后）

```powershell
cd D:\luyuan\xuanming-server-mmo-cpp
git worktree remove --force D:\luyuan\g4bg-red
Remove-Item Env:GOTOOLCHAIN, Env:GOPROXY -ErrorAction SilentlyContinue
```

`--force` 是因为 worktree 里有 8.1 写的未跟踪文件 `g4bg_repro_test.go`。删之前确认 8.2 里需要对照的命令都已经在 worktree 里跑过。

### 8.4 不属于本批的验证，与真实 Redis 首跑的归属

- 双副本时钟偏差、L-14 压测（交票到 gate 会话建立的分布、`redis-queue` 的写 ops、runbook 节奏数字的重新标定）都不在本批。没有压测对比，不宣称任何性能结论（AGENTS §6）。
- **本批的全部运行证据都在 miniredis 上**。runbook 写着「验证清单（每次改 loginqueue 都跑一遍）」（`login-queue-stress-runbook.md:132`），其中第 1 步是单测（8.2 覆盖），第 2–5 步要起整套服务（robot 冒烟、杀 leader、重启 Redis）。**第 2–5 步本批不执行**，登记为对这条既有门禁的偏离（§9 #11），PROGRESS 条目里如实写明（§7.2 第 6 项）。
- **真实 Redis 首跑归 G4b-2**：排队在所有环境都关着（F13），不登记的话，新脚本第一次碰真实 Redis 就是 G4b-2 在 staging / prod 打开排队的那一刻。所以在 `spec_G4b.md` 合入门槛处登记「打开排队之前至少跑过一次下面的冒烟」（§7.2 第 4 项）；是否作为硬门槛、要不要提前到本批跑，见 §11 #5。
- **最小冒烟步骤**（可选，口径同 G1b 规格 §7.3：由负责人决定是否跑，不属于压测；需要起本地栈，命令由 Codex 或负责人执行；需要拉镜像或装东西时先问用户）：
  1. 本机已有的 Redis 7.x + etcd + gate + login；login 的配置按 G1b 规格 §7.3 的办法用 scratchpad 里的副本（`Queue.Enabled=true`，`ZoneCapacityOverride` 取小值，例如 `"1": 100`），**不改 `go/login/etc/login.yaml`**。
  2. 跑 runbook 验证清单第 2、3 步（`RobotCount=5` 与 `RobotCount=200`）。
  3. 期间用 `redis-cli` 核对五件事：`TYPE admitted:zset:zone:1` 是 `zset`；`ZRANGE admitted:zset:zone:1 0 -1 WITHSCORES` 的分数是 13 位的到期毫秒，快速通道成员以 `fast:` 开头；成员在取走（或快速通道占位）后约 15s 消失；`PTTL admitted:zset:zone:1` 不超过 120000；`EXISTS admitted:zone:1` 为 0。
  4. 判读第 3 步的 `q_admitted` 时按 §5.2「放行节奏的上界」，不按 runbook 现文的「100s 内涨到 100」。

---

## 9. 与现有设计的偏离与代价

| # | 偏离 | 原设计 / Java 版 | 理由 | 代价 |
|---|---|---|---|---|
| 1 | 在途占位换键换类型 | `admitted:zone:{z}` SET + 整键滑动 TTL（设计稿 `:73`、`:180-181`） | 负责人已拍板的方案 A | 新旧版本混跑时两边计数互不可见（§5.5）；运维手册里的排查命令要换 |
| 2 | 取走不撤占位、再留 15s | 取走即 SREM（设计稿 `:166` 的安全属性只覆盖到「取走之前」） | 方案 A | 每个取走的人被重复计平均约 9s（§5.2） |
| 3 | 计数变成写脚本 | `SCARD`，只读 | 清理与计数必须原子，否则读到已到期的成员 | 每次非重入的 assign-gate 里那次 `SCARD` 变成一次 EVAL 写；dispatcher 每拍两次。没有成员可清时是否产生复制 / AOF 流量**未核实**，L-14 看 `redis-queue` 的写 ops |
| 4 | 快速通道占位寿命取 15s | 现状：整键 `AdmitTTL`；Java：`now + admitTtl`（60s） | 快速通道当场交票，等同已取走；取 60s 时 Q1 的现象只是变短、没有消除（§5.1） | 与 Java 不同；建连超过 5s 的快速通道玩家与排队玩家一样会漏计（§5.2）。列入 §11 #2 |
| 5 | 取走时用普通 ZADD | Java 用 `ZADD XX`（成员不在就不补） | 两种时钟混用时不漏人；混跑时新版计数更准 | 占位已被清的边角下，多扣一个名额 15s |
| 6 | 整键 TTL 无条件 `PEXPIRE W`，`W` 多留 1 分钟 | Java：`pttl < X 才 pexpire X`，`X` = `admit_ttl` 或 15s | 同一个 `W` 下无条件续期不会缩短任何成员的寿命，少一次 `PTTL` 与一个分支 | zone 下线后占位键多留 1 分钟（共 2 分钟）才消失 |
| 7 | 时间取 login 墙钟 | 仓库里 `go/match`、`go/db` 的新代码取 Redis `TIME`；Java 排队取客户端时钟 | §4.2 | 多副本时钟偏差直接进占位寿命（± δ）；钟快的副本恢复或摘掉之后，它留下的占位还要再拖 δ 才到期（§4.2 代价）。列入 §11 #3、#4 |
| 8 | `Queue` 新增两个未导出字段，只有测试会改 | — | 既有集成用例需要宽限为 0；占位到期需要可控时钟（AGENTS §11.2 显式依赖） | 包内多一处「测试可改」的状态；`New` 不暴露它，生产路径改不到 |
| 9 | `AdmitTTL` 的含义收窄 | 既是放行槽 TTL，也是占位整键 TTL（间接决定快速通道占位的寿命） | 取走后与快速通道改由常量决定 | 运维调 `AdmitTTL` 不再影响快速通道占位；注释与设计稿同步写明 |
| 10 | 不做方案 A 提纲之外的 Java 对齐（取走后再轮询回 410、单段 Lua 放行、请求号防重发） | Java 已做 | 提纲没有，回归面大 | 仍是有意差异（§5.8、§12） |
| 11 | runbook「验证清单（每次改 loginqueue 都跑一遍）」的第 2–5 步本批不执行 | `login-queue-stress-runbook.md:132-166`：单测 + 单 / 多 robot 冒烟 + 杀 leader + 重启 Redis | 第 2–5 步要起整套服务，Claude 不跑（AGENTS §10.1）；排队在所有环境都关着，本批也不打开 | 新脚本在真实 Redis 上没有任何运行证据，直到 G4b-2 打开排队之前的那次冒烟（§8.4、§11 #5） |

---

## 10. 风险

| # | 风险 | 依据 | 缓解 |
|---|---|---|---|
| R1 | 15s 对 C++ gate 只留 5s 余量。建连普遍超过 5s 时（开服洪峰、gate 过载）漏计重新出现，多放的节奏约每 17s 一批 | F10；`assigngatelogic.go:19-27` 记着 Round 18 在 7.8 万并发下「签票到 gate 验票」平均 66s、最大 403s（那是极端过载，任何宽限都盖不住） | §11 #1；L-14 测「交票 → gate 会话建立」的分布后定值；上游的 gateway 限流（G4b-1）先把洪峰压住 |
| R2 | 某个 login 副本时钟偏差 δ 达到宽限量级：故障期间它早清别人的占位（多放）；**它恢复或被摘掉之后不立刻自愈**，它最后 15s / 60s 内写的占位要再过 δ 才到期，期间有效容量少掉这些占位（≤ 当时的 `C − O`）。只要 zone 还有写入，整键 TTL 一直被续上、不兜底；完全停放时最长约 `AdmitTTL + W`（3 分钟）后自愈。旧实现没有这种模式 | §4.2 代价；清理只有 `-inf … now` 一个方向；每次写都无条件 `PEXPIRE W` | 节点 NTP（偏差按毫秒级估是假设，未测）；**止血 `redis-cli DEL admitted:zset:zone:<z>`**，代价是最多多放一批 `C − O`，写进 runbook 模式 2；脚本里加上界清理见 §11 #4（默认不加）；彻底的退路是改用 Redis `TIME`（§4.2） |
| R3 | 新旧版本在排队开着时混跑：两种键互不可见，在途占位合计最多 2 ×（`C − O`）；**leader 或经手的副本是旧版期间 Q1 / Q2 原样存在**，G4b-G 只在放行方与取走方都是新版时才生效 | §5.5 | 部署顺序：排队关着滚完再开（现状就是关的），写进 `login.yaml` 上线清单第 0 步与 runbook；G4b-2 契约用例 15 的机械门禁已登记到 `spec_G4b.md`（§7.2 第 4 项）；不能把混跑窗口当作 G4b-G 已生效 |
| R4 | gate 的上报周期（`main.cpp:408`）与常量 15s 没有机械关联，改一边不会报错 | §4.3 | 常量注释写明出处；T1 钉住 15s；设计稿新小节写明两者的关系 |
| R5 | 双计让有效容量在高交票速率下打折：瞬时少放 ≈ `9λ`，最多 `15λ` | §5.2 | 有界、自愈；`ZoneCapacityOverride` 定值时把它算进去（G4b-2 / L-14） |
| R6 | 计数变成写命令后 `redis-queue` 的写 ops 上升 | §9 #3 | L-14 测；必要时把 dispatcher 每拍的两次计数并成一次（G1b 评审 N2 当时未采纳的微优化） |
| R7 | 测试替身与真实 Redis 的差异。miniredis v2.35.0 / go-redis v9.16.0 一侧已对模块缓存源码核实（§13 U1–U4），用例因替身行为而红的可能性很低；剩下的风险在另一头：**真实 Redis 上没有任何运行证据**（§13 U5） | §13；§9 #11 | §8.2 的兜底说明（出现不符就停下报告，不自行升级依赖）；真实 Redis 首跑归 G4b-2 打开排队之前（§8.4、§11 #5） |
| R8 | gate 票有效期 10 分钟远大于宽限：拿了票很晚才连上的人不在任何占位里 | F11 | 既有性质，不是本批引入；gate 侧有连接上限 |
| R9 | 指标口径变了但名字没变，看历史曲线时 2026-10-13 前后不可比 | §5.7 | 帮助文本与 runbook 写明；仓库里没有依赖旧阈值的告警 |
| R10 | runbook 与设计稿的放行节奏判读（「`q_admitted` 不涨 = dispatcher 卡死」「每 30s 涨一个 capacity」）没有按修复后的上界标定；照现文，正常回压会被判成模式 1，L-14 拿它判读会误报 | §5.2「放行节奏的上界」；`login-queue-stress-runbook.md:39-44`、`:55`、`:70`、`:149`、`:189-190`；`login-queue-2026-05.md:319-321` | §7.2 第 1、2 项逐处登记；数字待 L-14 重新标定 |
| R11 | 工作区里 `go/login` 有别的会话的未提交改动，而且评审这一轮期间还在增加（04:41 只有 `go.mod` 的 `go` 指令；09:07 已有 14 个已跟踪文件，含本批要改的 `etc/login.yaml`）：红（HEAD worktree）与绿（工作区）不在同一基线上；其它包的失败容易被错记到本批头上；落码时可能覆盖别人对 `login.yaml` 的改动；工具链不匹配时 go 可能去下载 | 文件头的基线说明；`git status --short -- go/login`；`.github/workflows/login-path-tests.yml:81`（CI 的 Go 版本跟 `go/login/go.mod` 走） | §8.0(a) 落码前快照（排队代码已被别人动过就停）；`login.yaml` 按内容锚点改（§4.6）；§8.0(b) 禁止自动下载；§8.2 的范围判据与归因规则；这些改动的归属由主会话向用户确认 |

---

## 11. 待负责人拍板

只列会改变线上行为或验证口径、且本规格无法靠读代码定下来的五条（#1–#3 是原有的，#4、#5 是评审后补的）。每条都给了默认做法，本规格的代码与用例按默认写；落码前要不要先等负责人回复，由主会话按用户的约定判断。#1–#4 都只影响常量或一两行代码，落码后再改的成本很低；#5 不改代码。

1. **交票后宽限取多少**。
   - **默认：15s**（方案 A 提纲写的值，与 Java 同值）。
   - 备选：20s（两个上报周期，建连余量 10s，与 Java 的余量相同）。代价：平均双计从约 9s 升到约 14s，瞬时少放 ≈ `14λ`。
   - 提纲写「覆盖 gate 每几秒发布一次人数的滞后」，而代码里 C++ gate 是每 10s（F10），15s 的余量只有 Java 版的一半，所以单独提出来。最终值待 L-14。改法：`defaultHandoffGrace` 一个常量 + T1 的期望值 + 文档。
2. **快速通道占位的寿命**。
   - **默认：与取走后相同（15s）**。理由见 §5.1。
   - 备选：照 Java 取 `AdmitTTL`（60s）。代价：每个快速通道玩家平均重复计约 54s；T4 的场景（容量 100、每秒 1 人）在第 55 秒余量仍会归零，等于 Q1 在这个形状下没修掉，T4 的参数要跟着放宽。
   - 改法：`TryReserveFastPathSlot` 里 `q.handoffGrace` 换成 `q.admitTTL`，T3、T4 的期望值跟着改。
3. **时间来源**。
   - **默认：login 进程时钟经 ARGV 传入**（§4.2，与 Java 排队相同）。
   - 备选：Redis `TIME`。消除多副本时钟偏差，也就没有 R2「故障副本恢复后残留 δ」这回事。代价：两段 Lua 里要做毫秒运算与格式化，`holdAdmitted` 也要改成 Lua（放行与取走各多一段脚本）；与 Java 版、本包既有做法不一致；Redis 一侧要求「`TIME` 之后允许写」（Redis 7.2，凭记忆，**未核实**）。**不需要升级 miniredis、不用改 `go.mod`**：v2.35.0 的 `TIME` 跟随 `SetTime`，已对源码核实（§4.2 表、§13 U7）。
4. **（评审后补）要不要在两段 Lua 里加一道上界清理**，把 R2 的残留从「δ」压到分钟级。
   - **默认：不加**。只按 §4.2 / R2 如实写明持续时间，并把止血命令写进 runbook。
   - 备选：两段脚本在第一条 `ZREMRANGEBYSCORE` 之后各加一行 `redis.call('ZREMRANGEBYSCORE', KEYS[1], '(' .. ARGV[n], '+inf')`，`ARGV[n]` 由 Go 算好传入，等于 `q.now().Add(q.admittedKeyTTL()).UnixMilli()`（计数脚本是 `ARGV[2]`，快速通道脚本是 `ARGV[6]`）；T7 加两个成员，分数 `now + W` 的保留、`now + W + 1` 的被清。合法分数 ≤ 写入方的「现在」+ `max(AdmitTTL, 15s)`，偏差小于 1 分钟时不会被误删；分数超出「读方的现在 + `W`」的成员在下一次读时就被清掉，所以任何残留最长留约 `W`（2 分钟），与 δ 无关。
   - 不默认加的理由：① 它引入一条新的耦合：排队开着时把 `AdmitTTL` 滚动上调超过 1 分钟（例如 60s → 300s），旧配置的副本会把新配置副本写的占位当成越界清掉，滚动期间多放；② 完全停放的情形整键 TTL 已经兜底（约 3 分钟），其余情形是有效容量打折而不是停服，且有一条命令的止血办法；③ 钟偏到分钟级的副本同时在签错过期时刻的 gate 票，本来就要靠 NTP 告警发现，脚本里悄悄删掉「不该存在」的成员反而少了一个现场；④ 旧实现的 Q1 是无故障时就恒在的同类残留，本改动把它收窄到只有时钟故障时才出现，已经是改善。
   - 改法（选备选时）：`queue.go` 两段脚本各 1 行 + 两处 `Run` 各多传 1 个参数 + 脚本注释；T7 的期望；本规格 §4.5 的 ARGV 表。
5. **（评审后补）真实 Redis 上的首跑放在哪一步、算不算硬门槛**。
   - **默认：登记为 G4b-2 打开排队之前要做的一步**（写进 `spec_G4b.md` 合入门槛处，§7.2 第 4 项），本批不跑；步骤见 §8.4。本批的运行证据只有 miniredis，runbook 验证清单第 2–5 步未执行（§9 #11）。
   - 备选 A：本批就按 §8.4 跑一次最小冒烟（要起本地栈；需要拉镜像或装东西时先问用户）。
   - 备选 B：只当建议，不作为 G4b-2 的门槛。代价：新脚本第一次碰真实 Redis 可能就是 staging / prod 打开排队的那一刻。

不需要拍板、已按 YAGNI 定下的：不新增配置键；不新增指标；不动 `Lookup` 的重复轮询行为；`PopAdmit` 不改单段 Lua；`shared-redis-split-by-purpose.md` 本轮只改两处。

---

## 12. Java 版状态（AGENTS §12）

- **Java 已做，本改动落码并验证后这两点与之相同，Java 代码不用改**。Java 的排队在 game-gateway 里（`D:\luyuan\xuanming-server-mmo\game-gateway\src\main\java\com\game\gateway\queue\LoginQueue.java`，批次 3.4b 提交 `554405f` 引入，版本号 `0.1.0-SNAPSHOT`）：
  - 占位是 ZSET、分数 = 到期毫秒、计数前清掉（`:76-79`、`:82-92`、`:100-126`）：对应本规格的主体。
  - 取走后占位改为再留 15s（`TAKEN_GRACE`，`:37`、`:134-141`）：对应 Q2。
- **`PARITY.md` 的行号**：「登录排队」现在是第 **99** 行，「开服限流」是第 100 行，第 98 行是「区服白名单」（2026-10-13 只读核对，HEAD 与工作区一致）。`spec_G4b.md`、G1b 规格 §4.7 / §10、交接 §4 第 6 项写的行号都比现状小 1：它们用 98 指排队、用 99 指限流。`spec_G4b.md` 的订正见 §7.2 第 4 项；G1b 规格是历史快照，不回改。Java 仓库的 `PARITY.md` 工作区里还有别的会话未提交的一行追加（在 `:150` 之后，不影响这两行），行号以后还会漂，所以下文与所有新写的文字都按行名引用。
- **③ 不能整条写成已对齐**：「登录排队」行的 ③ 是个复合条目，只有「取走后占位不撤、再留 15 s」一句对应 Q2。其余各句（放行槽超时没取或取走后再轮询回 410、弹出与放行同一段 Lua、元数据过期不放行、请求号防重发、按段挑 gate）方案 A 没有包含，G4b-G 之后仍是有意差异。`spec_G4b.md` §11 写的「②③ 做完即对齐」对 ③ 说宽了。
- **两版仍然不同、且是本规格有意为之的**（服务端内部实现，不涉及客户端契约，AGENTS §12 允许）：
  1. 快速通道占位寿命：Java 60s，本仓库 15s（§11 #2）。Java 版在「一批人从快速通道涌入」后同样会多停约 50s，是否跟进由 Java 负责人定，本规格不要求。
  2. 取走时占位已被清：本仓库补回（普通 ZADD），Java 不补（`ZADD XX`）。
  3. 整键 TTL：本仓库无条件续到 `max(AdmitTTL, 15s) + 1min`，Java 是「不足才续」。
  4. 键名：本仓库 `admitted:zset:zone:{z}`，Java `<ns>:login-queue-admitted:{zone}`。两版不共用 Redis 键。
  5. gate 上报周期：本仓库 10s，Java 5s，所以同样的 15s 余量不同。
- **`PARITY.md`「登录排队」行要改成什么**（文档阶段由主会话改；Java 仓库只动这一个文件、只动这一行的备注列；改之前重读，不覆盖别的会话在该文件里的未提交改动）。文档阶段时本仓库的 G4b-G **还没提交、没编译、没测试**（AGENTS §9：commit 只在用户明确要求时执行，本轮不提交；§10.1：没有 Codex 结果前不得写成已验证），所以措辞不写「已对齐」，也不留填不出来的 commit 占位：
  - ② 原文：`② 已放行占位用 ZSET（分数 = 到期毫秒）逐个过期，计数前清掉——基线 SET + 整体 TTL，每次 SADD 都续上，持续有人登录时占位只增不减、空位归零；`
    改为：`② 已放行占位逐个过期：mmorpg 自 G4b-G 起同样是 ZSET（分数 = 到期毫秒）、计数前清掉过期成员，键 `admitted:zset:zone:{z}`（2026-10-13 落码，未提交、未编译，待 Codex 验证；提交后回填 commit）。仍不同：快速通道占位寿命 Java = `admit-ttl`（60 s）、mmorpg = 15 s（快速通道当场交票，等同已取走）；整键 TTL Java 不足才续、mmorpg 每次写入续到 max(AdmitTTL, 15 s) + 1 min；`
  - ③ 里的那一句，原文：`取走后占位不撤、再留 15 s（客户端连上 gate、gate 每 5 s 发布人数之前它不在任何在线数里——基线取走即撤占位，开区时每轮都按「在线 0」再放一满批，能放进几倍容量）；`
    改为：`取走后占位不撤、再留 15 s（客户端连上 gate、gate 每 5 s 发布人数之前它不在任何在线数里）——mmorpg 自 G4b-G 起相同（2026-10-13 落码，未提交、待 Codex 验证；它的 gate 每 10 s 上报，余量 5 s；取走时占位已被清掉的话 mmorpg 补回、Java `ZADD XX` 不补）；`
  - ③ 的其余各句、①、④–⑧ 都不动。**「mmorpg commit」列（现为 `26ceb70ca`）与状态列「已对齐（行为有意不同）」也不动**；等 Codex 跑绿、用户发话提交之后，再把两处括号里的「未提交、…待 Codex 验证」换成 commit 号，那时才更新「mmorpg commit」列。
  - §11 #2 若选备选（快速通道也取 60s），② 的「仍不同」里去掉快速通道那一句。
- **本仓库 `PROGRESS.md` 条目要写的 Java 状态**：对应 Java 版本号 `0.1.0-SNAPSHOT`；状态「已做（Java 批次 3.4b 已实现占位逐个到期与取走后保留 15s）；本仓库 G4b-G 落码后这两点与之相同，未提交、未编译、待 Codex 验证；快速通道占位寿命等五处内部实现差异见 `spec_G4b-G.md` §12」。
- 客户端契约（assign-gate / queue-status 的回包码、queue token、proto、tip）不变，两版都不需要同批改协议。

---

## 13. 外部事实：哪些已对源码核实，哪些仍未核实

2026-10-13 评审后重核：本机模块缓存里有与 `go/login/go.mod` 同版本的 miniredis v2.35.0 与 go-redis v9.16.0（`C:\Users\luyua\go\pkg\mod\github.com\alicebob\miniredis\v2@v2.35.0`、`…\github.com\redis\go-redis\v9@v9.16.0`），U1–U4 与 U7 的 miniredis 一半已逐条读源码核过，下表「依据」栏的路径都相对这两个目录。**「已对源码核实」只说明读过实现，不是运行证据**：没有运行任何 go 命令，测试是否通过仍待 Codex（§8）。其余各条仍是凭记忆或假设。

| # | 事实 | 用在哪 | 状态与依据 | 不成立时 |
|---|---|---|---|---|
| U1 | miniredis v2.35.0 的 Lua 里可以调 `ZREMRANGEBYSCORE`、`ZCARD`、`ZADD`、`PEXPIRE`；被调命令出错时脚本整体回 error | 两段脚本；T11 | **已对源码核实**。命令注册：`cmd_sorted_set.go:18-19`、`:32`，`cmd_generic.go:46`；Lua 的 `redis.call` 把参数原样交给 `srv.Dispatch`，对已注册命令一视同仁（`lua.go:57-60`），错误回复转成 Lua error（`lua.go:63-72`），脚本出错回 error（`cmd_scripting.go:106-109`） | 兜底：停下报告（§8.2） |
| U2 | miniredis 里键的 TTL 不随墙钟走，只随 `FastForward` | §6.5 的 Q1 红用例（`mr.FastForward`）；`TestIntegration_DualLeadersSameZone_…` 的既有注释 | **已对源码核实**：递减 TTL 的只有 `db.go:711-718` 的 `fastForward`，全包没有定时器。**T6 不依赖它**：用例真实耗时毫秒级，60s 的放行槽在真实 Redis 上也不会过期 | 不影响 T6；只影响 §6.5 Q1 红用例的写法 |
| U3 | miniredis：`ZREMRANGEBYSCORE` 对不存在的键回 0、对非 ZSET 键回 `WRONGTYPE`，`-inf` 可解析、上界是闭区间；`ZADD` 打到非 ZSET 键回 `WRONGTYPE`，`EXEC` 逐条执行；删掉最后一个成员即删键；`TYPE` 对 ZSET 回 `zset`；`PTTL` 回存下的毫秒数；`KEYS` 支持通配 | T2、T7、T9、T11 | **已对源码核实**：`cmd_sorted_set.go:1065-1073`、`:1168-1186`、`:1207-1217`、`:145-148`；`cmd_transactions.go:90-93`；`db.go:506-515`；`redis.go:21` 与 `cmd_generic.go:396-402`；`cmd_generic.go:302-308`；`cmd_generic.go:41`、`:490` | 对应用例红，报告，不改实现 |
| U4 | go-redis v9.16.0：`TxPipeline()` 返回 `Pipeliner`、`PExpire`、`Cmd.Int64`、`Script.Run`（先 EVALSHA，`NOSCRIPT` 再 EVAL；miniredis 的文案以 `NOSCRIPT` 开头，能命中）、`MaxRetries: -1` 只关命令重试、拨号另有 `DialerRetries`（缺省 5 次）/ `DialerRetryTimeout`（缺省 100ms）、`float64` 用 `'f'` 格式编码 | §4.4、§4.5、T10 | **已对源码核实**：`redis.go:1098`、`generic_commands.go:195`、`command.go:331`、`script.go:68-74`（miniredis `redis.go:58`）、`options.go:331-336`、`options.go:115-123` 与 `:286-291`、`internal/pool/pool.go:335-360`、`internal/proto/writer.go:103`、`:131`、`:232-233` | 编译报错，按报错改调用 |
| U5 | **真实 Redis**：`ZREMRANGEBYSCORE key -inf x` 是闭区间；最后一个成员删掉后空 ZSET 自动消失；`PEXPIRE` 覆盖已有 TTL；没有成员可删的写脚本不产生复制 / AOF 流量 | §4.1、§9 #3 | **未核实**（凭记忆）。miniredis 上的同名行为已核（U3），但那不是真实 Redis 的证据 | 前三条是 Redis 的基本语义，由 §8.4 的冒烟在真实 Redis 上看一次；最后一条只影响 R6 的估算 |
| U6 | 同一 zone 的 login 副本之间时钟偏差在毫秒到几十毫秒 | §4.2 的取舍 | **未核实**（假设，无旁证） | 走 §4.2 的退路；偏差大时的后果见 §4.2 代价、R2 |
| U7 | (a) miniredis v2.35.0 的 Lua 内 `TIME` 可用并跟随 `SetTime`；(b) Redis 7.2 允许脚本在 `TIME` 之后写 | 只有 §11 #3 选备选时才用到 | (a) **已对源码核实**：`cmd_server.go:167-176`、`miniredis.go:446-450`、`:673-678`、`lua.go:57-60`。(b) **未核实**（凭记忆；仓库旁证 `go/match/internal/team/scripts.go:8-10`、`go/db/internal/kafka/retry_ownership.go:108-112`） | (b) 不成立则备选不可行 |
| U8 | etcd 写入到 login 读到的延迟是毫秒级 | §4.3、§5.2 的 `v = c + φ` | 无 | 宽限的余量再减去这一段 |
| U9 | gate 认票的细节（HMAC、无状态、行号 `:1029-1074`） | F11 | 摸底给出，本规格只重读了 `main.cpp:407-412` 与 `client_message_processor.cpp:654` | 不影响设计，只影响 R8 的表述 |
| U10 | `GOTOOLCHAIN=local` 让 go 不自动下载工具链；`GOPROXY=off` 让 go 不下载模块，缺了直接报错 | §8.0 | **未核实**（凭记忆） | 变量本身报错时去掉重跑；出现下载提示仍然停下（§8.0） |
| U11 | Codex 用的 Go 版本不低于 `go.mod` 要求的 1.27.2 | §8 全部命令 | **未核实**。只读线索：本机 `C:\Program Files\Go\VERSION` 写的是 `go1.27.2`；没有运行 `go version` | §8.0：停下报告，不下载 |
| U12 | gofmt 会把文档注释里三空格缩进的块改成 tab；`metrics.go` 在 HEAD 上因此不是 gofmt 干净的；Go 1.27 的 gofmt / vet 相对 1.24 有没有行为变化 | §4.7、§8.2 的 gofmt 规则 | **未核实**（没有运行 gofmt；旁证 `PROGRESS.md:5948`） | §8.0 的基线会给出实际情况；规则本身（基线内的文件不 `gofmt -w`）不依赖这个猜测 |
| U13 | Windows 回环上被拒绝的 TCP 连接多久返回 | T10 的真实等待 | **未核实**；所以 §6 只写上界 3 × `DialTimeout` | 不影响通过与否 |
| U14 | robot 登录后保持在线（不主动下线） | §5.2「放行节奏的上界」对 runbook 样例的判读 | **未核实** | 只影响 runbook 节奏数字怎么重写，不影响代码；L-14 标定 |

---

## 14. 对三路摸底的核对结果

逐条回到代码核过，下面是与摸底不一致、以代码为准的地方：

1. **`PARITY.md` 行号**：surface 摸底写「登录排队」在第 98 行；实际是第 99 行（go、java 两路摸底是对的）。`spec_G4b.md`、G1b 规格、交接文档里的行号都比现状小 1（它们的 99 指的是「开服限流」，现在在第 100 行），订正办法见 §7.2 第 4 项。
2. **`TestIntegration_DispatcherRespectsCapacity`**：surface 摸底说它「新语义下更稳」；实际相反，保留 15s 宽限时它在「测试的轮询落在两次 MULTI 之间」的交错下会超时（go 摸底的推断是对的）。本规格用 fixture 宽限设 0 处理（§6.4）。
3. **`queue_test.go:322`**：java 摸底说它「也断言占位数」；那一行断言的是 `PopAdmit` 的返回个数，与占位无关，不用改。
4. **miniredis Lua 里的命令支持**：go、java 两路摸底都列为未核实。`PEXPIRE` 有既有的运行证据（`pkg/locker/player_locker.go:102-103` 的续期脚本，`PROGRESS.md:4827` 记着锁包与排队包用例全过）；`ZADD` / `ZREMRANGEBYSCORE` / `ZCARD` 评审后已对 v2.35.0 源码核实（§13 U1）。摸底与初稿说的「本机没有模块缓存」不成立。
5. **快速通道占位寿命**：三路摸底都只把它列为开放问题。本规格用 T4 的算例给出了结论：照 Java 取 60s 时，容量 100、每秒 1 人的场景在第 55 秒余量仍然归零（§11 #2）。

其余摸底结论（键的全部读写点、`New` 的 7 个调用点、gate 10s 上报、login 读 etcd 无缓存、合服 / 回档 / 压测脚本不含排队键、`k8s_deploy.ps1` 没有 `Queue` 段、两处必改的既有测试、Java 版的四段 Lua 与常量）与代码一致。

---

## 15. 评审处理记录（2026-10-13，对抗式评审，只读核对，未运行任何命令）

三路评审（correctness：R1–R6；implementability：A1–A4；completeness：OPS-1–OPS-8）共 18 条，全部回到代码、文档与本机模块缓存逐条核对。核对只用了读文件、grep 与只读 git 命令（`status` / `diff` / `show` / `log`）；没有运行 go、gofmt、测试、部署命令，没有改本规格以外的任何文件，Java 仓库只读。18 条的问题陈述都成立，全部吸收进正文；其中 7 处具体建议没有照原样采纳，单列在第二张表。

### 15.1 已吸收

| # | 问题 | 核对到的依据 | 落在正文哪里 |
|---|---|---|---|
| R1 | 文档同步清单漏了「放行节奏」的判读文字，修好的行为会被 runbook 判成模式 1 | `login-queue-stress-runbook.md:39-44`、`:55`、`:70`、`:149`；`login-queue-2026-05.md:319-321`；同类还有 `:189-190` 的推荐告警 | §5.2 新增「放行节奏的上界」；§7.2 第 1 项补设计稿 `:319-321`，第 2 项补健康表 / 样例、模式 1、验证清单 #3、推荐告警；新增 R10、U14 |
| R2 | 「重复轮询不续期」「`signFn` 失败保持放行分数」两条语义没有用例钉住 | `queue.go:304-307`（`GETDEL` 回空即返回）、`:275-280`；T5、T12 对每个令牌只 `Lookup` 一次 | 新增 T13、T14（§6.2 表与 §6.3 代码）；§5.1 两条各写明由哪个用例钉住，并写明 `holdAdmitted` 只能放在哪里 |
| R3 | 混跑结论漏了「旧 leader + 旧 Pod 取走」这一格：Q1 / Q2 原样存在 | `queue.go:355`（SREM）、`:443-449`（SCARD） | §5.5 的列举改成逐格、结论句补后半句；R3 的风险与缓解同步；§7.2 第 1 项要求设计稿原样带上后半句 |
| R4 / A2 / OPS-3 | 基线声明失效：`go/login/go.mod` 被别的会话改了 `go` 指令，红绿不在同一基线，可能触发下载，`git diff --stat` 会带出不属于本批的文件 | `git status --short`、`git diff -- go/login/go.mod`（`-go 1.24.5` / `+go 1.27.2`）；`.github/workflows/login-path-tests.yml:81`；本机 `C:\Program Files\Go\VERSION` = `go1.27.2` | 文件头基线重写（09:07 快照：`go.mod` 之外又多出 13 个文件，含本批要改的 `etc/login.yaml`）；§0 规模；新增 §8.0（(a) 主会话落码前快照，(b) 记录环境、禁止自动下载、worktree 钉提交）；§8.2 的改动范围、`go.mod` / `go.sum` 判据、失败归因规则；`login.yaml` 行号全部改成「HEAD 行号 + 工作区快照行号 + 内容锚点」；新增 R11、U10、U11 |
| R5 | 时钟故障的持续时间写轻了：恢复后还要再留 δ，整键 TTL 多数情况不兜底，没有新键的止血命令 | 规格自己的脚本（清理只有 `-inf … now`、每次写都 `PEXPIRE W`）；`assigngatelogic.go:110`、`dispatcher.go:278-280`（完全停放时没有写入） | §4.2「代价」重写；§5.3 末行；§5.5 新键止血命令；R2；§7.2 第 2 项 runbook 模式 2 加反方向排查与止血；代码加固列 §11 #4 |
| R6 / OPS-4 | `spec_G4b.md` 的 `PARITY.md` 行号是整体小 1，只改排队那几处会留下新的错引用 | Java `PARITY.md`：98 区服白名单、99 登录排队、100 开服限流（HEAD 与工作区一致；第 100 行含 ④ `queue_token`、⑨ /64）；`spec_G4b.md:90`、`:512`、`:702`、`:723`、`:755`、`:756`、`:764`、`:765` | §7.2 第 4 项改为正文 8 处一起改、§13 历史快照加说明；本规格与所有新写文字按行名引用（§0、§3、§5.8、§12、§14） |
| A1 / OPS-1 | §8.1 把 worktree 删了而 §8.2 还要用；§8.1 标「推荐」可跳过；没有 gofmt 基线命令；worktree 钉的是符号 HEAD | 初稿 §8.1 / §8.2 原文 | §8 改成 8.0 前置（必做）→ 8.1 红（必做）→ 8.2 绿 → 8.3 清理；8.0 含 gofmt 基线命令；worktree 钉 `0aa1027bc`；其它包失败时在 worktree 对照后再删 |
| A3 | §13 说「本机没有模块缓存」不成立；§11 #3 备选的代价（要升级 miniredis）是错的；T6 不依赖 U2 | 模块缓存 miniredis v2.35.0 / go-redis v9.16.0 源码，逐条重读（行号见 §13） | §13 重写（U1–U4、U7(a) 改为已对源码核实）；§4.1 空键、§4.2 表与退路、§4.5、F16、§11 #3、R7、§8.2 兜底说明、§6 开头、§14 第 4 条同步 |
| A4 | T10 的注释不对：`MaxRetries: -1` 不关拨号重试，用例里有没登记的真实等待 | go-redis v9.16.0 `options.go:115-123`、`:286-291`、`:331-336`；`internal/pool/pool.go:335-360` | T10 的 `Options` 加 `DialerRetries: 1`、`DialerRetryTimeout: time.Millisecond` 并改注释；§6 开头登记唯一的真实等待与上界；§6.2 表；U13 |
| OPS-2 | gofmt 规则自相矛盾：对 HEAD 上就不干净的文件既允许 `gofmt -w` 又说不要并进本批 | `metrics.go:9-14`（三空格缩进的包注释）；`PROGRESS.md:5474`、`:5948` | §4.7 与 §8.2 统一为「基线内的文件一律不 `gofmt -w`，只比对差异块；基线外新列出的才允许」；U12 |
| OPS-5 | 给 `PARITY.md` 的改写文本里有填不出的 commit 占位，并把未验证的改动写成「已对齐」 | Java `PARITY.md` 表头（`:25`）与「登录排队」行的 commit 列 `26ceb70ca`；`LoginQueue.java:88-90`、`:122-124`、`:139-141`（`pttl` 不足才 `pexpire`） | §12 的 ②③ 改写文本换成「未提交、未编译，待 Codex 验证；提交后回填」，补整键 TTL 差异；写明 commit 列与状态列不动；PROGRESS 措辞同步 |
| OPS-6 | 部署顺序要求没写进运维实际照着做的 `login.yaml` 上线清单第 0 步 | `go/login/etc/login.yaml` HEAD `:436-438`（工作区 09:07 快照 `:445-447`） | §3、§4.6 新增第 0 步三行换三行（附现文，按内容锚点定位）；§7.1；§5.5；§7.2 第 2 项 runbook 故障应急 |
| OPS-7 | G4b-2 的机械门禁与合入门槛只写在本规格，没登记到 G4b-2 会读的 `spec_G4b.md` | `spec_G4b.md:620`（用例 15）、`:674`（合入门槛） | §7.2 第 4 项补这两处；§5.5、R3 的「建议」改为「已登记」 |
| OPS-8 | 全部证据在 miniredis 上，真实 Redis 首跑没有归属；runbook 的既有门禁「每次改 loginqueue 都跑一遍」没有提 | `login-queue-stress-runbook.md:132-166`；`spec_G1b.md:843-855`（§7.3 的先例） | §8.4 写明归属与最小冒烟步骤；§9 #11 登记偏离；§11 #5；§7.2 第 4、6 项；R7 |

### 15.2 不采纳及理由

| # | 没有照原样采纳的建议 | 理由 | 实际做法 |
|---|---|---|---|
| R5（代码部分） | 两段 Lua 各加一行上界清理（`ZREMRANGEBYSCORE key (now+W +inf`），作为本批代码 | 评审自己标的是「可选，由主会话定」。它引入新的耦合：排队开着时把 `AdmitTTL` 滚动上调超过 1 分钟，旧配置的副本会清掉新配置副本的占位（多放）。而它要防的情形有界且有一条命令的止血办法；完全停放时整键 TTL 已经兜底 | 不作默认；改法与取舍完整写在 §11 #4，等负责人拍板 |
| R5（表述） | 「钟快 1 小时的副本写满 `C − O` 个占位，这个 zone 就停放 1 小时」「旧实现没有这种模式」 | 核对后要分两种情形：完全停放时没有任何写入，整键 TTL 在约 `AdmitTTL + W`（3 分钟）后兜底；持续 δ 的是「有效容量打折」。旧实现的 Q1 是无故障时就恒在的同类残留，新增的只是「按某个副本的钟到期」这一点 | §4.2、R2 按核对后的两种情形分别写 |
| R4 / A2 / OPS-3（措辞与判据） | 第 5 行写成「`go/login` 下除 `go.mod` 外无未提交改动」；用 Codex 开跑时的 `git status` 当基线 | 与重核时的工作区不符，而且它还在变：05:31 多出 6 个源文件，09:00 前后又多出 `etc/login.yaml`（本批要改的文件，别处注释 +9 行）、`svc/servicecontext.go` 等，09:07 共 14 个已跟踪文件。Codex 是落码之后才开跑的，那时的 `git status` 已经分不出哪些是本批的 | 文件头按 09:07 的实际状态写并标明是快照；基线改为主会话在落码前记的快照（§8.0(a)）；`login.yaml` 改按内容锚点定位并附现文（§4.6） |
| OPS-2（判据细节） | 「`gofmt -d` 差异块的行范围应与 HEAD 版本相同」 | 本批会在 `metrics.go` 里加行（帮助文本多 2 行），后面的差异块行号必然平移；包注释那一块里第 10 行的文字本来就是本批要改的 | 判据改为「差异块一一对应，行号平移与本批改的那一行属正常，出现新差异块才停」 |
| R6 / OPS-4（备选） | 把 `spec_G4b.md` 里的引用统一改写成按行名，或只在头部加一句说明、不改数字 | 前者要改写 12 处的句子，那份文档别的会话也可能在动；后者正文里的错行号还在，读到 `:723` 的人看不到头部说明 | 正文 8 处只改数字，§13 历史快照 4 处不动、标题下加说明；新写的文字才按行名 |
| OPS-6（文案） | `login.yaml` 第 0 步写「最多多放行一批」 | 与 R3 冲突：leader 或经手副本是旧版时 Q2 照旧，不以一批为界 | 改写为「旧版副本放行 / 取走的部分仍按旧版超发」，保留「不丢人、不重复放行」 |
| OPS-8（门槛） / OPS-1（备选） | 把真实 Redis 首跑直接定为 G4b-2 的合入前置；§8.1 允许跳过并给替代取基线办法 | 前者是给另一批次加硬门槛，不该由本规格单方面定；后者保留「可跳过」就还要维护两条路径，红用例本身也是缺陷修复该有的证据（AGENTS §11.4） | 首跑登记到 `spec_G4b.md` 并列入 §11 #5 由负责人定是否硬门槛；§8.0 / §8.1 改为必做，不留跳过分支 |
