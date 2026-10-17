# 交接:开服首日瓶颈修复、D1 映射持久化与 S3 容量目标(2026-10-13)

> **给新会话**:这是本轮工作(开服瓶颈 G1~G4、D1 映射持久化)的入口。先按 AGENTS.md「会话启动门禁」读完规定材料,再读本文,然后从 §4「剩余工作」第 1 项开始。
> **并行交接**:同一天另有两个会话在**同一个工作区**留下了交接,与本文有重叠,分工见 §7。动手前先读 §7,不要做第二份。
> **状态**:本轮(2026-10-12 ~ 10-13,Claude)的所有改动都**在工作区里,没有提交,没有编译,没有跑任何测试**(AGENTS.md §10.1)。没有 Codex 的运行结果之前,不得声称「编译通过」「测试绿」「已验证」。
>
> **2026-10-15 进度**(上一行写于 10-13,「没有提交」已过时;正文不改,以本段为准):
> - §4 第 1~4 项与第 6 项已落码,随用户的工作区快照入库(`e4ff1d165`、`ebccaf04d`、`54c9d775d` 等)。**全部未编译、未测试**,仍待 Codex 验证。
> - 各项的说明与交 Codex 的命令在 [PROGRESS.md](../PROGRESS.md) 2026-10-13~15 的补充条目,按标题找:「G2b 连接预算补完」「开服瓶颈三视角评审(G1a / G1b / G2a / G3)与其中三项的修复」「G4b 削峰按发布档位开启 + G4b-G 登录排队在途占位逐个到期」「G1b 评审确认问题的修复」「路由行片 16 → 64 与模数正向探针 P5」,以及 2026-10-15 的几条评审补充(Java 定向验证口径更正、登录排队 G4b-G / G1b 评审补漏、行片 64 与 P5 评审修复、G2b / G4b 部署脚本复审修复)。
> - 新增规格 [spec_G4b-G.md](launch-readiness-20261013/spec_G4b-G.md):登录排队在途占位逐个到期(G4b 评审补出的 Q1 / Q2,负责人 10-13 选方案 A)。
> - 仍未做:§4 第 5 项(拆 Redis 设计按 S3 修订);第 7~10 项(第 7 项归全局单点交接、D1 后续批、TiDB Phase 1 拍板、其他登记问题)。
> - 新增的待负责人拍板项(§6 未列):G4b-G 规格 §11 六条(已按默认落码);G1b 修复里两条英文错误文案;连接总账在 all-up 时并入缺省 `-ZoneId 101` 的口径;出队整批落同一台 gate(打开排队前的第三条排队基线问题)。
>
> **2026-10-15 再更正**(上面一段写于 10-15 上午,以下以本段为准):
> - **上一条列的四个待拍板项已结案**:负责人 10-15「按最标准的去做」,结论见 [PROGRESS.md](../PROGRESS.md)「2026-10-15 补充:按负责人「按最标准的去做」落实开服瓶颈的待拍板项」。同一轮也结案了收尾汇总里另外三项:G2a `RetentionGap` 不改单调计数器,改为 `max_over_time[15m]`、`for: 0m`;契约用例 15 的名称按默认保留;输出语言约定为日志 / 控制台英文,错误文案跟随所在文件。上一条漏列了这三项。
> - **G4b-2 越过了合入门槛,已补机械拦截**:登录排队的 login 段先于 `spec_G4b.md` §7 的三项门槛随快照入库。三项门槛是:G1b 多 zone 用例由 Codex 跑绿、G4b-G 由 Codex 跑绿、真实 Redis 首跑记进 PROGRESS。入库后 staging / prod 缺省就会打开排队。现由 `tools/scripts/k8s_deploy.ps1` 的 `$script:LoginQueueMergeGateCleared = $false` 拦住:门槛清掉前,所有部署环境的排队都关,`-LoginQueue on` 直接拒绝。三项满足后,由负责人在同一提交里改为 `$true` 并注明 PROGRESS 条目。
> - **仍待负责人定**(详见上面那条 PROGRESS 之后的补充条目):
>   - G3 设计 P2 / P7:id 复用信号是否升级为拒绝建角;是否在 6c 之前加只读探测。见 `new-player-first-enter-no-cold-read.md` §10「待拍板」。
>   - J0 之后持有 `queue_token` 被限流时的客户端回退口径。这是客户端契约,打开排队前必须定。
>   - 网关 Hikari `connection-timeout` 与探针端口。前者即 `spec_G2b.md` §10 #5。
>   - sarama lag 探针是否关掉自动建 topic。
> - **登记为后续、尚未立项**:
>   - G2a 批量模式下坏消息「已跳过」计数的时机。两条消费者要一起改。
>   - `go/login/internal/logic/clientplayerlogin/loginlogic.go` 的 `accountLock.Release(l.ctx)` 仍用请求 ctx 释放账号锁,与 G3 已修的建角锁同类。
>   - `RouteStore.FenceShards` 的 TiDB 档 64 没有配置载体。这是实现方登记的延后,不是负责人拍板;切 db_first 前要补进 ConfigMap。
> - **提交状态**:10-15「按最标准」及之后补缺口的改动,一部分已被别的会话的整区快照带进 HEAD,其余仍在工作区。以各 PROGRESS 条目的文件清单为准。**全部未编译、未测试。**
>
> **2026-10-15 晚,续**:上面「仍待负责人定」的前四条,负责人已让「按最标准修复」,均已落码。剩余的评审、修复与验证移交 [handoff-launch-readiness-decisions-20261015.md](handoff-launch-readiness-decisions-20261015.md),以那份为准。本文 §4 第 5、7~10 项仍归本文。
>
> **2026-10-16**:
> - 全局单点交接 10-16 的订正把五项改派回本线:会话对账按 gate 建索引、删 `dirty_keys_set`、task:result 分频道、nofile、scene_manager 只读本 zone。这与本文 §4 第 7 项「归全局单点交接」的说法相反,以那份订正为准。
> - 这五项、D1 第 1b 批、G4a 修订(第 5 项)与过时注释(第 10 项)都已出调研规格,附核实意见,文件名为 `launch-readiness-20261013/spec_B0-*.md`、`spec_D1_batch1b_scout.md`、`spec_G4a_revision_scout.md`。**未落码**。
> - 逐项结论、11 条待拍板与推荐顺序见 [PROGRESS.md](../PROGRESS.md)「2026-10-16 开服线剩余工作调研」。
> - 不依赖拍板、可以先落的四项:B0-2、过时注释、B0-5、D1 1b-1。
> - **同日落码**(全部未编译、未测试;各项条目见 PROGRESS 同日):
>   - B0-2「删除 C++ 存盘 Lua 的全服键 dirty_keys_set 写入」:写入已在代码中删除,全部 scene 换成新镜像前生产仍在写;存量清理口径在 `docs/ops/release-checklist.md` §6.1,执行环境与时间待负责人定(待拍板第 11 项)。
>   - 第 10 项「共享 Redis 淘汰策略过时注释更正」:go 下实为 10 处,另有 k8s_deploy.ps1、deploy/k8s/README.md 各 1 处,共 12 处;已改 9 处,`go/match/etc/match_service.yaml`、k8s_deploy.ps1、README 3 处待对应会话收尾,建议文案在该条目里。
>   - B0-5「scene_manager 负载刷新」:用「领导者按 zone 批量刷新 + 登记集合替代 SCAN」缓解;字面的「每个副本只读本 zone」、按 zone 选主和 watch 收窄仍归 A1。
>   - D1 1b-1 另起一批做。
>
> **会话产物位置**:原会话的 scratchpad 在本机临时目录,新会话读不到。需要的规格都已复制到 [launch-readiness-20261013/](launch-readiness-20261013/)。

## 0. 一句话

- 负责人把容量目标定为 **S3:同时在线 1000 万、日活 1 亿**。
- 本轮完成的,按工作项:
  - D1 映射持久化第 0、1a 批代码,以及路由表热点修复(G1a);
  - 登录排队锁 bug(G1b)、快照审计管道(G2a)、新号免冷读(G3)的代码;
  - 拆 Redis 设计稿(G4a);
  - TiDB Phase 1 设计、容量模型等文档。
- **没做完的**:
  - 连接预算(G2b)只落了一半(Go 侧指标与池旋钮已写,部署脚本总账、告警、契约测试没写,见 §4 第 1 项);削峰打开(G4b)一行没写;
  - 上面 6 项代码的统一三视角评审与修复;
  - 按 S3 修正分片数和 Redis 设计;
  - 容量模型 §1.3 的五件止血事项;
  - D1 后续批次、TiDB Phase 1 拍板项。

## 1. 负责人拍板记录(本轮)

| 日期 | 事项 | 结论 | 记录位置 |
|---|---|---|---|
| 10-12 | D1 映射持久化方向 | 采用「库为真源 + Redis 完整副本 + 就绪标记」,分批落码,已授权超过 30 文件 | [提案](../design/player-route-mapping-durability-proposal.md) §11.1 |
| 10-12 | dev Redis 淘汰策略 | 改 noeviction,与 K8s 一致 | 同上 |
| 10-12 | W2 / W3 | 停用(`RemapHomeZoneForMerge` 已删除,删映射分支拒绝) | 同上 |
| 10-12 | 推进路线 | 「两条都做,D1 先行」= 对 handoff-backlog D-02 表态继续 Phase 1 | [tidb-phase1](../design/tidb-phase1-and-launch-readiness.md) |
| 10-12 | 开工范围 | 四组全选:D1 热点 + 排队锁、快照管道 + 连接预算、新号免冷读、拆 Redis(只出设计)+ 削峰(只在 staging / prod 档打开) | 本文 |
| 10-13 | 容量目标 | **S3**(PCU 1000 万、DAU 1 亿) | [容量模型](../design/capacity-model-hok-scale.md) 文首 |

**仍待负责人拍板**,汇总在 §6。

## 2. 工作区改动(未提交)

用 `git status` 看全量。按工作项归类:

| 工作项 | 主要文件 | 设计 / 记录 |
|---|---|---|
| 文档校准(10-12) | `AGENTS.md` §12(Java 本机路径)、多份 docs 状态行 | PROGRESS 10-12 第一条 |
| D1 第 0 批 | `deploy/docker-compose.yml`、`go/data_service/internal/routing/router.go`(批量查 / 枚举 fail-closed)、`tools/scripts/k8s_zone_rollback.ps1`(FLUSHDB 防护)、`tools/scripts/dev_tools.ps1`、`tools/scripts/tests/k8s_zone_rollback_flush_guard.tests.ps1` | 提案 §11.2「第 0 批」 |
| D1 第 1a 批 | `go/shared/placement/route*.go`、`go/data_service/internal/{store,routing,routetool,server,logic,config,svc,metrics,constants}/…`、`go/data_service/cmd/route_tool/`、`data_service.go`、`etc/data_service.yaml`、`k8s_deploy.ps1`、清库脚本 | 提案 §11.2「第 1a 批」与「评审修复」;规格 [spec_D1_batch1a.md](launch-readiness-20261013/spec_D1_batch1a.md) |
| G1a 路由表热点 | 同上目录:`player_route` 主键改聚簇 `(route_shard, player_id)`,`route_meta` 拆出 `route_reconcile`,新增 `route_tidb.go`、`route_tool tidb-prepare` | 提案 §11.2「第 1a 批热点修复」;[spec_G1a.md](launch-readiness-20261013/spec_G1a.md) |
| G1b 排队锁 | `go/login/internal/logic/pkg/loginqueue/{dispatcher,metrics}.go` 及测试、`internal/svc/servicecontext.go`、`login_queue_wiring_test.go`、`login.yaml` 注释 | `docs/design/login-queue-2026-05.md` §7.1;[spec_G1b.md](launch-readiness-20261013/spec_G1b.md) |
| G2a 快照管道 | `go/data_service/internal/kafka/{snapshot_worker,lag_probe,snapshot_consumer,…}.go`、`store/snapshot_store.go`、`go/shared/kafkautil/group_lag.go`、`deploy/k8s/data-service-audit-alerts.yaml`、`k8s_deploy.ps1` | `docs/ops/audit-topic-generation-bump-runbook.md`;[spec_G2a.md](launch-readiness-20261013/spec_G2a.md) |
| G2b 连接预算(**半成品**) | `deploy/k8s/manifests/infra/mysql.yaml`(max_connections 1800、内存 1Gi/5Gi)、`java/gateway_node/src/main/resources/application.yaml`(Hikari 5/2)、`go/shared/sqlpoolstats/`(新)、`go/db/{db.go,etc/db.yaml,internal/config/config.go,internal/metrics/*,internal/logic/pkg/proto_sql/store_registry*.go}`、`go/data_service/{data_service.go,internal/store/dbstats*.go,internal/svc/servicecontext*.go,internal/metrics/*}`、`deploy/k8s/manifests/go-svc/{db,data-service}.yaml` | [spec_G2b.md](launch-readiness-20261013/spec_G2b.md) §3;已落 / 未落逐项见 §4 第 1 项 |
| G3 新号免冷读 | `go/login/internal/logic/pkg/dataloader/new_player_seed.go`、`clientplayerlogin/createplayer_seed.go`、`createplayerlogic.go`、`config`、两份 `login.yaml`(开关 `NewPlayerSeed.Enabled`,**默认关**) | `docs/design/new-player-first-enter-no-cold-read.md` §13;[spec_G3.md](launch-readiness-20261013/spec_G3.md) |
| G4a 拆 Redis | 只有文档 `docs/design/shared-redis-split-by-purpose.md` | [spec_G4a.md](launch-readiness-20261013/spec_G4a.md) |
| 文档 | `tidb-phase1-and-launch-readiness.md`、`capacity-model-hok-scale.md`、`placement-reverify-checklist-20261012.md`、`global-data-layer-tidb-decision.md` §D3 更正、`no-single-node…` 连接预算更正、`zone_data_rollback.md`、`merge-zone-runbook.md` 等;`docs/README.md` 与 `tools/scripts/gen_docs_index.py`(按生成器格式手工登记,生成器未运行) | 各文档 |

**仓外(同样未提交)**:
- Java 仓库 `D:\luyuan\xuanming-server-mmo\PARITY.md` 末尾追加了一行「玩家存储落点」(不适用)。该仓库工作区里还有**别人在途的改动**,只碰 PARITY.md。
- `D:\luyuan\xuanming-server\docs\design\region-cell-routing-merge-conversation-20260924.md` 文首加了一段补注。

**数据库提醒**:
- 在把 `route_shard` 从 16 改为 64(§4 第 4 项)之前,**任何环境都不要用这份工作区代码跑 data_service `-migrate`**,也不要启动 data_service:MigrateSchema 在 redis 模式下也会建 `player_route`,建出来就改不了主键。
- 若已经跑过,按 G1a 设计的 HR4 人工 DROP 全部路由表后再迁移。

## 3. 已完成(全部未编译、未测试)

- **D1 第 0、1a 批 + G1a**:默认 `RouteStore.Mode=redis`,建角登记行为与以前相同。
  - 新增一个启动依赖:迁移要建五张路由表,建表失败会拒启。
  - db_first 只能在冻结窗口内打开,条件见提案 §11.2 的 P2 门禁。
- **G1b**:锁键改为 `<前缀>:zone:<zoneID>`,每个 zone 一个 leader。leader 指标的 label 变成 `pod, zone`。排队默认仍关。
- **G2a**:按分区并发消费;批量落库的旋钮默认值等于现状(逐条)。新增 lag 探针和 8 条告警。分区扩容只能离线换代,见 runbook。
- **G3**:建角后写一份新号种子,开关默认关。容量模型 §4.4 判定它是「S1 开服前必须打开」。
- **G4a**:设计稿,分四块。容量模型 §4.2 判定「按用途拆只够约 S1」,需要修订(§4 第 5 项)。
- **文档**:
  - TiDB Phase 1(v0.2,§9.3 有 20 项待拍板);
  - 容量模型(S3 基准,§1.3 有五件不依赖拍板的事);
  - 复验清单;
  - D1 提案 §12(全局目录是不是瓶颈)。

## 4. 剩余工作(按顺序做)

每项都按「先读规格 → 落码 → 对抗评审 → 修复 → 文档」做。全部遵守 AGENTS.md §10.1:Claude 只改代码与文档,编译 / 测试交给 Codex。

1. **G2b 连接预算:补完剩下一半**。规格 [spec_G2b.md](launch-readiness-20261013/spec_G2b.md),文件编号用规格 §3 的 A/B/C 表。
   - 原工作流在这一步因额度中断,中断前已写了一半。2026-10-13 交接时按文件逐项核过(只看文件里有没有对应改动,没审内容):
     - **已落**:A1(mysql.yaml)、A2(Hikari)、A4 / A5(「151」注释)、A8 / A9(两份 manifest 注释,diff 存在,内容待评审)、B1~B14(Go 侧 `sqlpoolstats` 收集器与两个服务的注册)、B16(db manifest 的 9160 端口与注解)、C1~C6(`ZoneStore*` / `NonZoneStore*` / `ExtraStoreConnMaxIdleSeconds` 新键与按家族选池)。
     - **未落**:A3(Java `GatewayDataSourcePoolConfigTest`)、**A6(`k8s_deploy.ps1` 总账五个函数与 ConfigMap 镜像,一个都没有)**、A7、A10(`deploy/k8s/README.md` 的「MySQL 连接总账」短节;README 现有 diff 是 G2a 的 topic 换代更正)、A11、A12、A13、B15(`deploy/k8s/mysql-connection-alerts.yaml`)、**B17(db ConfigMap 的 `MetricsListenAddr: ":9160"`)**、B18、C7、C8、C9。
   - **B16 与 B17 当前不一致**:manifest 已经声明 9160 端口和 prometheus 注解,但 K8s 上 db 的 ConfigMap 还没写 `MetricsListenAddr`,抓取会失败。B17 必须和 B16 同批补上,否则别部署这份 manifest。
   - go/db 交接(§7)里说 `k8s_deploy.ps1` 已有 `Get-MysqlConnectionPlan` / `Get-MysqlConnectionLedger`,与实际不符:工作区里没有这两个函数。以本条为准。
   - 先评审已落部分再补未落部分;补的时候不要改动已落文件里与规格无关的行(G2a、D1 也改过 `k8s_deploy.ps1`、`data_service.go`、`metrics.go`)。
2. **G4b 削峰在 staging / prod 档打开**:照 [spec_G4b.md](launch-readiness-20261013/spec_G4b.md) 落码。
   - 必须排在 G2b 之后,两者都改 `k8s_deploy.ps1`。
   - 依赖 G1b 已落地(已落)。
   - 阈值标「待压测确认」。
3. **统一三视角评审 + 修复**(原工作流在这一步中断):
   - 范围:G1a、G1b、G2a、G3,加上新落的 G2b、G4b。
   - 三个视角:规格符合度;编译正确性(Go import / 签名、PowerShell 插值、测试辅助函数是否存在、删改名后的残留引用);一致性与并发。
   - 各实现汇报里点名要重点看的(都只读代码核过,没编译):
     - **G1a**:SQL 实参里片号的位置;P4 探针要求 MySQL ≥ 8.0.16;`route_tool audit` 从此需要 INSERT 权限;零游标的 R12 可能退化成全表扫描。
     - **G1b**:gofmt 对齐;队列有人但没有 gate 时每秒打一条 ERROR;T8a 的盲区;leader 指标 label 变化会让旧告警误报。
     - **G2a**:sarama v1.43.1 / kafka-go v0.4.47 的 API 是凭记忆写的(本机没有模块缓存);worker 循环顶部的 ctx 检查;信号量在 panic 时要归还。
     - **G3**:miniredis 对 `SET … PX`、EVAL 报错的行为未核实;开种子后账号锁最坏 21s,超过 20s 的 TTL;PreloadPool 背压。
4. **按 S3 修正路由分片数**(容量模型 §4.1,**必须在任何环境建表前完成**):
   - `RouteRowShards` 16 → **64**(`go/shared/placement/route.go:60` 一带);
   - `schema.go` 的 `chk_route_shard` 改为 `% 64`;
   - 测试里钉住的 16 改为 64;
   - `route_tidb.go` 每 zone 的预切点改为 8 个;
   - 提案 §11.2 与 1b 设计里「按片循环 s=0..15」的描述同步改;
   - 连带:`RouteStore.FenceShards`(TiDB 档 64、MySQL 档 16,只改配置);`route_meta CACHE` 从可选改为 S2 起必选;EVAL → EVALSHA 列为 S2 前置。
5. **修订拆 Redis 设计**(容量模型 §4.2):
   - 新增第 0 批:去掉 SessionReconciler 的全键 SCAN、删除 `dirty_keys_set`、task:result 分频道、scene_manager 负载刷新只读本 zone;
   - 控制面按 zone 组分实例,不并进全服唯一的 mapping;
   - `redis-cache` 从可选改为必做;
   - mapping 三族改用同一个 hash tag(需负责人裁决,和 db-task-pipeline-bottlenecks §6.5 冲突);
   - 账号目录落库;
   - 核心内存上界与冷玩家卸载在 S1 前拍板。
   - 另外,G1b 之后锁已经是每 zone 一把,`shared-redis-split-by-purpose.md` 第 66、236 行附近「锁是全局键」的说法过时,要同步。
   - **与全局单点交接重叠**:那份 §5.1 / §8 第 1 条(D12 / CZ-2 改写为「逻辑单一、物理按 `{player_id}` 分片」)、§7 阶段 4 / 6 和这里是同一件事的两面。先读那几段,在 G4a 文档里合并成一份设计,不要另起一份。
6. **文档收尾**(原工作流 Docs 阶段中断):
   - `docs/PROGRESS.md` 已有一条汇总(「2026-10-13 开服首日瓶颈修复(G1~G4)…」)。补完 G2b、做完 G4b、评审修复之后,在末尾**另起补充条目**,不要回头改那条(AGENTS §5);写明 Java 状态,验证命令写进仓库文档。
   - Java `PARITY.md` 末尾追加:
     - G1b:第 98 行备注补一句「mmorpg 起每 zone 一个出队 leader;Java 全局一个 leader 遍历全部区,不受影响」;
     - G3:「不适用」(Java 建角同步插入玩家行,进场按主键读库);
     - G2a / G2b / G4b:按各规格的 Java 判断登记。
   - `tidb-phase1-and-launch-readiness.md` 的 L-05(新号免冷读)、L-07(快照)标成「已落码,未验证」;`no-single-node-horizontal-scaling-20261001.md` §5 改为指向 G4a 文档。
7. **容量模型 §1.3 的五件止血事项**:**已全部被全局单点交接覆盖,归那份做,本文不再单列**。对应关系与本文多出的要求:

   | 容量模型 §1.3 | 全局单点交接里的位置 | 本文额外要求(接手那份时带上) |
   |---|---|---|
   | etcd 自动压缩、配额、defrag | §3 P0、§7 阶段 0(已开任务卡片) | 钉住 etcd 镜像版本 |
   | RLIMIT_NOFILE | §7 阶段 1「gate nofile」 | 范围是 gate / scene / battle 三类容器,不只是 gate;加启动断言 |
   | SessionReconciler 全键 SCAN、`dirty_keys_set` | §7 阶段 3「对账改按 gate 建索引」「删 dirty_keys_set」 | 容量模型把它列为 S1 前必做,比那份的阶段 3 早 |
   | Kafka retention.bytes 与 PVC | §3 P1、§7 阶段 1 | PVC 按容量模型的公式定;关掉旧的「一节点一 topic」路径 |
   | 测量 V0 | §7 阶段 2「压测基线」 | 采集项以容量模型 §5 为准 |
8. **D1 后续批**:
   - 1b:对账 leader、全量预热与 `mapping:route:ready`、回档检测与反向修库、`route_tool` 并进 data_service 主程序(K8s 上可跑);
   - 2:merge_zone 写入点改为库先写,补 proto 守卫;
   - 3:读方认就绪标记。
   - 范围见提案 §9 与 §11.2。
9. **TiDB Phase 1**:等 §9.3 的 20 项拍板。另外容量模型 §4.5 要求「OLTP 与审计至少分两套集群,单元化后每单元一套」,需要回写进 Phase 1 文档。
10. **其他已登记的问题**:
    - **任务卡片「Kafka 存盘失败后 MySQL 永久落后」**:scene 发 DBTask 失败时,脏比较快照不作废;
    - **PROGRESS 10-12「补充」条目里的同玩家串行缺口**:MySQL 条件写 `(owner_epoch, save_seq)`、login 读写分区对齐、ARCH.md「一致性哈希」订正,**都归 go/db 交接**(那份 §待办 1、3 与「过时文档订正」行),本文不做。Lua `tonumber` 比较:go/db 交接附录(第 545、564、710 行附近)已处理 epoch 段的 nil / 类型判断,接手时核对是不是 PROGRESS 10-12「补充」里说的同一处,不是的话并进那份;
    - **chat / friend / match / scene_manager 里 9 处「共享库是 allkeys-lfu」的过时注释**。

## 5. 交给 Codex 的验证顺序

按这个顺序,前一步红了就停下归因,不要连续重试(AGENTS §10.2):

1. [复验清单](placement-reverify-checklist-20261012.md):落点链,含附录里第 0 / 1a 批之后的用例数更正。
2. D1 第 0 批、第 1a 批、1a 评审修复、G1a 热点修复:命令都在[提案](../design/player-route-mapping-durability-proposal.md) §11.2 各段。
3. G1b:[spec_G1b.md](launch-readiness-20261013/spec_G1b.md) §7(先在 HEAD 的临时 worktree 跑红用例,再跑绿用例;T3 / T4 / T7 / T8 各跑 10 遍)。
4. G2a:[spec_G2a.md](launch-readiness-20261013/spec_G2a.md) 的验证段,另加 `audit-topic-generation-bump-runbook.md` 里的检查。
5. G3:`docs/design/new-player-first-enter-no-cold-read.md` §13。
6. G2b、G4b:落码后按各自规格。
7. 仓库根:`python tools/scripts/gen_docs_index.py --check`,退出码应为 0。docs/README.md 是按生成器格式手工登记的。
8. 全程不改 go.mod / go.sum;要拉镜像(redis:7.2、TiDB 等)属于下载,先征得用户同意;C++ 本轮没改。

## 6. 待负责人拍板(汇总)

- **TiDB Phase 1**:[tidb-phase1](../design/tidb-phase1-and-launch-readiness.md) §9.3 共 20 项,含部署方式、版本、迁移方式、发号打散、Codex 可操作的环境、安装 helm / Operator 的授权。
- **容量模型** §6:目标数字细化,包括地域数、开服建角峰值、单元规模。
- **拆 Redis**:G4a 文档的拍板项;mapping 三族 hash tag;核心内存上界与冷玩家卸载(S1 前)。
- **G2a**:开服期快照策略、批量打开前要求的压测对比表与内存护栏(找回档负责人)。
- **G4b**:削峰最终阈值(压测后定)。
- **AGENTS.md §6.2** 要不要加一行「压测前清库时,一并清全局库的路由表」(提案 Q8,这是改规范文档,本轮没动)。
- **提交**:本轮所有改动都没提交,包括两个仓外仓库;等用户明确发话。

## 7. 三份并行交接的分工(同一工作区,2026-10-13)

| 交接 | 管什么 | 和本文的边界 |
|---|---|---|
| **本文** | 开服瓶颈 G1~G4、D1 映射持久化、S3 容量模型 | — |
| [go/db 存盘管线](godb-pipeline-handoff-20261013.md)(+ [附录](godb-pipeline-handoff-20261013-appendix.md)) | 存盘条件写(`SaveConditional`)、读写分区对齐、连接错误延后不耗重试预算、db 消费组按 zone 拆、重试归分区属主、guild 阶段 0 | 它的 ② 批 B「连接预算」由本文 G2b 承担;它说 `k8s_deploy.ps1` 已有总账函数,实际没有(§4 第 1 项)。DBStats 指标由 G2b 导出,它的待办 2 / 4 不要再注册一遍 |
| [全局单点与 1000 万在线](handoff-global-singletons-10m-ccu-20261013.md) | 规模无关缺陷(C++ Redis AUTH、battle Kafka、etcd、C++ etcd 流重建等)、D12 / CZ-2 改写、cell 划分、账号落库 | 容量模型 §1.3 五件事归它(§4 第 7 项);拆 Redis 设计与它的 §5.1 合并(§4 第 5 项)。它 §9 把本文的 G1b / G4a / D1 / G2a 列为「他人在途改动,不要碰」,指的就是本文这些文件 |

**两份容量核算**:本文的 [capacity-model-hok-scale.md](../design/capacity-model-hok-scale.md)(负责人已确认 S3 档)和全局单点交接 §5 是两次独立推算,结论方向一致,都认为共享 Redis、单 MySQL、etcd、nofile 先撞墙。数字以容量模型文档为准;那份 §10「终审修正」还空着,补的时候和容量模型对一下数,有冲突就在两边都注明。

**建议的总顺序**(三份交接共用一个工作区、都没提交,先把已写的验掉再叠新改动):

1. Codex 先验三份已落码的部分:本文 §5,go/db 交接 §3,全局单点交接无代码;
2. 本文 §4 第 1~3 项(补完 G2b、做 G4b、三视角评审修复)和第 4 项(分片数 16 → 64,必须在任何环境建表前);
3. 全局单点交接 §7 阶段 0(止血),与 go/db 交接待办 4(连接错误不耗预算)一起做;
4. 本文 §4 第 6 项(文档收尾),之后再按各交接剩下的顺序推进;
5. **提交**:三份的文件在工作区里混在一起,用户发话提交时按各交接 §2 的文件清单分别 `git add`,不要 `git add -A`。

## 8. 给新账号的开场指令(可直接复制)

```
请接手 docs/handoff/handoff-launch-readiness-and-d1-20261013.md 的工作。
1. 先按 AGENTS.md「会话启动门禁」读规定材料,再读这份交接全文,尤其是 §7「三份并行交接的分工」。
2. 工作区里有三个会话的未提交改动混在一起。不要回退、覆盖或顺手整理任何不属于你当前任务的改动;不要 git add -A,不要提交,等我发话。
3. 顺序:交接 §4 第 1 项(补完 G2b 未落部分,先修 B16/B17 不一致)→ 第 2 项 G4b → 第 3 项三视角评审 + 修复 → 第 4 项路由分片 16→64 → 第 6 项文档收尾。规格在 docs/handoff/launch-readiness-20261013/。
4. AGENTS.md §10.1:你只改代码和文档,不跑任何编译、测试、gofmt;每做完一项,按交接 §5 的格式写出给 Codex 的命令(工作目录、命令、通过标准、失败时要留的日志)。没有 Codex 结果前,不要说「编译通过」「已验证」。
5. 每完成一项在 docs/PROGRESS.md 末尾追加条目(只追加),写明 Java 版状态(AGENTS.md §12);Java 仓库 D:\luyuan\xuanming-server-mmo 只动 PARITY.md。
6. 遇到交接 §6 的拍板项、要超过 30 个文件、要下载镜像或装工具,先停下来问我。
