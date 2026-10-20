# 交接:开服线剩余工作(2026-10-19)

> **给新账号**:先按 AGENTS.md「会话启动门禁」读完规定材料,再读本文,然后从 §3 第 1 项开始。开场指令见 §7,可直接复制。
>
> **上游**:[handoff-launch-readiness-and-d1-20261013.md](handoff-launch-readiness-and-d1-20261013.md)(10-13 起的开服线交接,文首按日期追加了 10-15 ~ 10-18 的进度段)。本文接它在 10-18 收尾之后的状态,上游正文里的「剩余工作」以本文为准。

## 0. 一句话

- 开服线的**代码已全部落码**,已随用户的整区快照与 10-19 主干合并入库,**全部未编译、未测试**。
- 剩下的是四类事:
  - 把这批代码交 Codex 验证,红了归因、修复;
  - G4a 拆 Redis 设计稿修订(另一个会话正在做;2026-10-19 更正:已完成,只剩拍板清单汇总,见 §3 第 2 项);
  - 负责人拍板与运维前置;
  - 已登记的后续小项。

## 1. 已落码的内容(全部未编译、未测试)

每项在 [PROGRESS.md](../PROGRESS.md) 都有条目,按标题搜索即可。每个条目都写了改动清单、交 Codex 的命令和通过标准。规格(含核实意见)在 [launch-readiness-20261013/](launch-readiness-20261013/)。

| 项 | 内容 | PROGRESS 条目标题(关键词) | 规格 |
|---|---|---|---|
| B0-2 | 删除 C++ 存盘 Lua 对全服单键 `dirty_keys_set` 的写入;存量清理口径在 release-checklist §6.1 | 「删除 C++ 存盘 Lua 的全服键 dirty_keys_set 写入」 | spec_B0-2_dirty_keys_set.md |
| 注释 | 「共享库是 allkeys-lfu」过时注释,12 处改了 9 处 | 「共享 Redis 淘汰策略过时注释更正」 | spec_G4a_revision_scout.md 第二部分 |
| B0-5 | scene_manager 负载刷新改为领导者按 zone 批量刷新,用登记集合 `scene_manager:zones` 替代全库 SCAN | 「scene_manager 负载刷新:领导者按 zone 批量刷新」 | spec_B0-5_scene_manager_zone_scan.md |
| D1 1b-1 | route_tool 并进 data_service 主程序(`-route-tool`);新增 freeze / reconcile-from-cache / unfreeze;就绪标记的编解码;修正发布在熔断时的分类 | 「D1 映射持久化第 1b-1 批落码」 | spec_D1_batch1b_scout.md(以提案 §11.2 为准) |
| B0-4 | gate / scene / battle 启动时把 nofile 软限抬到硬限,不够就拒启;运维豁免开关为 `NODE_OPEN_FILE_LIMIT_MODE=warn` | 「B0-4:gate / scene / battle 启动时抬 RLIMIT_NOFILE」 | spec_B0-4_nofile.md |
| B0-3 第 1 批 | proto `DBTask.reply_instance = 10`、`go/shared/taskresult`;login 同时订旧频道与实例频道,并填回复目标 | 「B0-3 task:result 读回执通知分频道 第 1 批」 | spec_B0-3_task_result_channel.md(文末两节交付说明) |
| B0-3 第 2 批 | go/db 按 reply_instance 定向发布;指标 `db_task_result_notify_total`;独立告警文件 | 「B0-3 … 第 2 批:go/db 按 reply_instance 定向发布」 | 同上 |
| B0-1 | player_locator 会话对账改为按 gate 实例建索引(在会话 Lua 里原子维护);对账改为全服单 leader;去掉全键 SCAN | 「B0-1:player_locator 会话对账按 gate 实例建索引」 | spec_B0-1_reconcile_by_gate.md;设计 [player-locator-gate-session-index.md](../design/player-locator-gate-session-index.md) |

另有一条收尾汇总:「2026-10-18 开服线收尾:代码全部落完,剩 G4a 修订稿」。**交 Codex 的总顺序**就在那条里。

## 2. 负责人已拍板(不要再翻案)

| 日期 | 事项 | 结论 |
|---|---|---|
| 10-13 | 容量目标 | 先定为 S3(1000 万在线),10-15 改为**日活 1 亿、峰值同时在线 2000 万**(容量模型 §7) |
| 10-17 | 核心 Redis 路线 | **单元化 + 单元内 Redis Cluster 按 {player_id} 分片**,两者叠加;D12 / CZ-2 改写为「逻辑单一、物理按 {player_id} 分片」 |
| 10-17 | B0-3 | 授权改动超过 30 个文件;先用 PUBLISH,SPUBLISH 推迟到上 Cluster 那批;分两批落 |
| 10-17 | B0-1 | gate 索引在会话 Lua 里原子维护 |
| 10-17 | 其余 | B0-4 加运维豁免开关、缺省拒启;D1 1b-2 先用单个就绪标记、回档检测容差取 0;存量 `dirty_keys_set` 暂不清理 |
| 10-18 | 上线状态 | **项目尚未上线**,没有存量部署。新旧混跑、降级、灰度类的残余风险,上线前不算阻塞,首次上线整批起步(见存盘围栏设计文末「上线状态」) |

## 3. 剩余工作(按顺序)

### 第 1 项:交 Codex 验证本线代码,收结果并归因

- 总顺序在 PROGRESS「2026-10-18 开服线收尾」条目:
  1. proto 生成核对;
  2. Go 各模块;
  3. C++,MSBuild 串行 `/m:1`;
  4. PowerShell 契约测试;
  5. `gen_docs_index.py --check`。
- 每项的细节命令、修复前红的取证方法(基线提交写在各条目里,多为 `360d56ae5` 或 `a105c3efd`)、通过标准,都在各自的 PROGRESS 条目和规格文末。
- 红了先归因:工作区和主干里有大量别的会话的改动,先判断失败是不是落在本线文件上。本线的就修,修完给出新的验证命令;别人的就如实报告,不要顺手改。
- 本线没有编译、也没有运行证据。在 Codex 跑绿之前,任何文档都不得写「已验证」。

### 第 2 项:G4a 拆 Redis 设计稿修订

- 10-19 已经用任务卡片单独开了一个会话在做,标题是「修订拆 Redis 设计稿 G4a(单元化+单元内分片)」。
- **(2026-10-19 状态)已完成,不用续做**:修订已落到 [shared-redis-split-by-purpose.md](../design/shared-redis-split-by-purpose.md),新增 §16~§26,三条 must 都已落实;围栏硬依赖登记在 §18.7。完成条目是 PROGRESS「2026-10-18 G4a 拆 Redis 设计稿修订」。接手者只需把该稿 §25.2 的拍板清单汇总给负责人,其中标「2026-10-18 新增」的 12 处(第 2、3、15、16、17、20、21、22 条,以及第 1、6、7、9 条下各一个子项)是这次新出现的,并入下面第 4 项第 6 条一起问;按 §25.2 原文逐条收,不要只照 PROGRESS 条目里的枚举(那里漏列了第 7、9 条子项,已在 PROGRESS 末尾另起更正)。
  - 规格里还有一批其他文档的指向注没做,该稿 §24 末段列了清单,写的是「归各自的维护线」,但目前没有哪条线认领:`cross-zone-scene-travel.md` CZ-2、`cross-zone-matchmaking.md` D12、`tidb-phase1-and-launch-readiness.md` L-06、`no-single-node-horizontal-scaling-20261001.md` §5、`spec_G4a.md` 文首、`db-write-fencing-mysql.md` §5.1 / §6.4 / §9.1、`db-task-pipeline-bottlenecks.md` §6.5(按该稿 §21.6 的指引)。都是只追加带日期注的纯文档活,谁接都行;动手前先 `git diff` 看对方是否正在改。容量模型的三处(§4.2、A6、§6 第 6 条)已补齐。
    - **(2026-10-19 补)上面这批指向注已在 G4a 修订会话里全部补上**,不用再接;见 PROGRESS「2026-10-19 G4a 收尾」条目。G4a 这一项现在只剩 §25.2 拍板清单的汇总。
- 接手前先看 PROGRESS 有没有它的完成条目:
  - 有:只需把它产出的拍板清单汇总给负责人;
  - 没有,而且那个会话已经停了:按 `docs/handoff/launch-readiness-20261013/spec_G4a_revision_scout.md` 第一部分及核实意见续做。
- 硬要求(核实意见的三条 must):
  - 新写的文字里不要再出现「共享库是 allkeys-lfu」;
  - chat 的聊天历史带 TTL(缺省 7 天、每频道 200 条);
  - 登记 db-write-fencing 的「排序锁 / 子表缓存 / 资产证据三键同实例或同 slot」依赖。
- 纯文档,不改代码。

### 第 3 项:开服前复核「登录取逻辑服 / 物理服是否瓶颈」(L-18)

- 负责人 10-19 要求:上线前重新查一遍账号登录时取 home_zone(逻辑服)和 storage_id(物理服)这两步是不是瓶颈、能不能水平扩展。10-18 那次的结论全是推算,不能直接沿用。
- 按 [player-route-mapping-durability-proposal.md](../design/player-route-mapping-durability-proposal.md) §12.2 的六步做,登记为 `tidb-phase1-and-launch-readiness.md` §7 L-18 和 `docs/ops/release-checklist.md` A.1。
- 结论写回提案 §12.1 和 PROGRESS;压测与汇总交 Codex,按 AGENTS §6。

### 第 4 项:需要负责人拍板或运维执行的事项(整理好后问负责人)

1. **TiDB Phase 1**:[tidb-phase1-and-launch-readiness.md](../design/tidb-phase1-and-launch-readiness.md) §9.3 列了约 20 项待拍板(部署方式、版本、迁移方式、发号打散、Codex 可操作的环境、Operator 安装授权等),不拍板就开不了工。另外,容量模型 §4.5「OLTP 与审计至少分两套集群、单元化后每单元一套」要回写进 Phase 1 文档。
2. **存量 `dirty_keys_set` 清理**:在哪个环境执行、什么时候执行;步骤在 `docs/ops/release-checklist.md` §6.1(门禁是版本盘点,用 UNLINK,不用 DEL)。
3. **B0-4 上线前置**:在各集群、各节点池上读一次容器的 nofile 硬限,记进 PROGRESS;读法在 `docs/ops/gate-kernel-tuning-runbook.md` §3.1。单副本的 battle hostPort 一旦拒启,战斗服会整体停服。
4. **B0-1 上线前置**:k8s_deploy.ps1 的 player-locator ConfigMap 已补上 Prometheus 段(9190),要和 manifest 的注解、端口同批发布;告警 `deploy/k8s/player-locator-alerts.yaml` 手工 apply。
5. **B0-3 上线顺序**:login 已经是第 1 批的 zone,改为按 zone 金丝雀发 go/db,不要一次升全部 zone;告警等两侧全量之后再 apply。
6. **G4a 修订稿产出的拍板清单**:mapping 三族 hash tag 的时机与键名、控制面 zone 组大小、redis-cache 形态、账号目录落库位置、核心内存上界与冷玩家卸载阈值(S1 前必须定)、三键同实例与值封装二选一等。

### 第 5 项:已登记的后续小项(不阻塞,按需另开任务)

- **过时注释还剩 3 处**:`go/match/etc/match_service.yaml`、`tools/scripts/k8s_deploy.ps1` 的 friend here-string、`deploy/k8s/README.md` 的 friend 段。建议文案在「共享 Redis 淘汰策略过时注释更正」条目里。k8s_deploy.ps1 那行在可展开的 here-string 里,替换文案不得含 `$` 和反引号。
  - **(2026-10-19 已完成)** 3 处都已改完,12 处过时注释全部收尾(只改注释,未编译);见 PROGRESS「2026-10-19 G4a 收尾」条目。
- **B0-2**:提交号要回填到 release-checklist §6.1;`cross-zone-matchmaking.md:298`、`player-inbox-service.md:965` 的行号引用要更新;`MessageSyncRedisClient` 与死工程 `cpp/tests/redis_test` 可以一起删。
- **B0-4**:
  - k8s_deploy.ps1 给 `NODE_OPEN_FILE_LIMIT_MODE` 接线;
  - C++ Deployment 显式写 `maxUnavailable: 0`;
  - `-WaitReady` 失败诊断时 grep 拒启行;
  - `deploy/k8s/AGENTS.md` 加一条「启动壳不加 ulimit」;
  - 用实测重新核 `kNonClientReserve`;
  - **Java 版待做**:启动时打印 `UnixOperatingSystemMXBean.getMaxFileDescriptorCount()` 并与需求比较,`start-slice.sh` 启动前打印 `ulimit -Sn` / `-Hn`(PARITY 已登记)。
- **B0-5**:A1 还没做:scene_manager 按 zone 选主、watch 与 knownNodes 收窄到本 zone,以及 rebalance 等领导者循环限定到本 zone。`handleWatchEvent` 的 PUT 换 zone 分支没有按领导者门控。
- **B0-3**:stress_summarize 补 login 侧的回执送达小表;dispatch 线程池饱和时的回退路径仍不可观测;`TriggerPlayerDataPreload` 是死代码。
- **B0-1**:etcd 成功返回空的或残缺的存活集时没有合理性闸门(这是既有行为,需要另做设计);接线 /metrics 之后用压测校准告警阈值。
- **D1**:1b-2、1b-3、第 2 批、第 3 批归「D1 玩家归属与存储持久化」会话,见 §5。

### 第 6 项:至今没有归属的小项(来自全局单点交接 10-16 订正,做之前先核对当时的代码)

- refresh token:每个账号最多 32 枚,而且不作废;
- Java gateway 单 zone 准入容量默认 5000;
- scene_manager 的 `reassignSceneNode` 没有 CAS(`go/scene_manager/internal/logic/world_init.go`);
- match 的 `migrateLegacyQueues` 仍在;
- 每次存盘都打一条 LOG_INFO;
- 任务卡片「修复 Kafka 存盘失败后 MySQL 永久落后」(scene 发 DBTask 失败后,脏比较快照没有作废):10-15 由 go/db 存盘管线会话认领,该会话 10-18 说代码已全部落完,但没提这一项。接手前先在 PROGRESS 里搜索,确认是否已修。

## 4. 硬约束

- **AGENTS §10.1**:Claude 只改代码与文档,不跑编译、测试、gofmt、proto 生成或压测;也不要派「能不能编过」的静态评审,评审只看语义和运行期。每项交付都要附 Codex 可执行的命令。
- **不 `git add -A`,不提交,不 push**;提交等用户发话。
- **多会话并行**:工作区随时会被整区快照提交。改文件前先 `git diff` 分清改动归属;一律用 Edit 精确替换,不回退、不覆盖别人的改动。
- **Java 仓**(`D:\luyuan\xuanming-server-mmo`)**只记待做项**:核对下来「不适用 / 已有等价」的,不往 PARITY.md 加行,结论写回本仓文档。10-19 已撤掉本线之前加的 4 行纯「不适用」(B0-2、B0-5、B0-3 第 2 批、B0-1)。保留的 3 行:B0-4(Java 待做)、B0-3 第 1 批(db_task.proto 的 ContractSync 同步待做)、D1(由 D1 会话维护)。
- **文档**:PROGRESS 只追加;更正一律追加带日期的更正注,不删原文;新增文档要在 `docs/README.md` 登记,格式与 `tools/scripts/gen_docs_index.py` 一致。
- **额度**:开多代理工作流前先看用量。落码类代理按「代理数 × 5%」估算 5 小时窗口;同一账号上的多个会话共用额度,不够就分批。每个实现代理的提示里都带续做说明(先看现状、在半截改动上续做)。

## 5. 与其他会话的分工(10-18 时的状态)

| 会话 | 负责 | 本线与它的边界 |
|---|---|---|
| D1 玩家归属与存储持久化 | D1 的 1b-2 / 1b-3 / 第 2 批 / 第 3 批;10-18 起 D1 全部文件归它 | 1b-1 的最终文件清单已移交。不要再改 `go/data_service/internal/{routereconcile,routing,routetool,routetoolcli,store,config,metrics,svc}`、`go/shared/placement`、`tools/merge_zone`、提案 §11 |
| go/db pipeline 交接文档 | 存盘管线 R1–R8 | 10-18 说 go/db 代码已全部落完,之后不再改;B0-3 第 2 批的文件清单已记进它的交接 `godb-pipeline-impl-handoff-20261015.md` §7.7 |
| G4a 修订(任务卡片会话) | `docs/design/shared-redis-split-by-purpose.md` | 它做完之前,别的会话不要改这份文件(2026-10-19:已完成,此限制解除;之后改这份文件先 `git diff`,只用 Edit 追加带日期注) |
| redis-ha 部署路径 | 见 `redis-ha-deploy-path-handoff-20261019.md` | 与本线不重叠 |

## 6. 关键文件索引

- 规格与核实意见:`docs/handoff/launch-readiness-20261013/spec_*.md`(调研规格的前半是规格,「# VERDICT」之后是核实意见;D1 的以提案 §11.2 为准)。
- 设计:
  - [capacity-model-hok-scale.md](../design/capacity-model-hok-scale.md) §7(2000 万口径);
  - [shared-redis-split-by-purpose.md](../design/shared-redis-split-by-purpose.md)(G4a);
  - [player-locator-gate-session-index.md](../design/player-locator-gate-session-index.md)(B0-1);
  - [player-route-mapping-durability-proposal.md](../design/player-route-mapping-durability-proposal.md)(D1);
  - [tidb-phase1-and-launch-readiness.md](../design/tidb-phase1-and-launch-readiness.md)。
- 运维:
  - `docs/ops/release-checklist.md`:§6.1 dirty_keys_set 清理,附录 F.B-1 会话定向补救;
  - `docs/ops/gate-kernel-tuning-runbook.md` §3.1(nofile);
  - `docs/ops/scene-node-role-split.md` §6.1(scene_manager 登记集合补登记);
  - `docs/ops/merge-zone-runbook.md` §9.4(route-tool);
  - `docs/ops/godb-pipeline-launch-todo.md` §10(task:result 上线)。
- 告警(都是手工 apply):
  - `deploy/k8s/scene-manager-alerts.yaml`
  - `deploy/k8s/task-result-notify-alerts.yaml`
  - `deploy/k8s/player-locator-alerts.yaml`

## 7. 给新账号的开场指令(可直接复制)

```
请接手 docs/handoff/handoff-launch-line-remaining-20261019.md 的工作。
1. 先按 AGENTS.md「会话启动门禁」读规定材料,再读这份交接全文(尤其 §2 已拍板、§4 硬约束、§5 分工)。
2. 从 §3 第 1 项开始:把本线代码交 Codex 验证,按 PROGRESS「2026-10-18 开服线收尾」条目的总顺序给出命令;拿到结果后归因,本线的问题修复,别人的如实报告。
3. 再看 §3 第 2 项 G4a 是否已由另一个会话完成;没完成且那个会话已停,按规格续做。然后按 §3 第 3 项做 L-18 复核,第 4 项整理成拍板清单问我。
4. 编译、测试、proto 生成、冒烟都交给 Codex:你只改代码和文档,不派「能不能编过」的评审。工作区多会话并行,不 git add -A、不提交。
5. Java 仓只登记待做项。每完成一项在 docs/PROGRESS.md 末尾追加条目(只追加),写明 Java 状态。
6. 遇到新的拍板项、改动超过 30 个文件、要下载镜像或装工具时,先停下来问我。
```
