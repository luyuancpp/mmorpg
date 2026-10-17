# 交接:开服瓶颈负责人四项决定的收尾与验证(2026-10-15)

> **给新会话**:先按 AGENTS.md「会话启动门禁」读完规定材料,再读本文;然后从 §4 第 1 项开始。
>
> **上游交接**:[handoff-launch-readiness-and-d1-20261013.md](handoff-launch-readiness-and-d1-20261013.md)。本文只接它在 10-15 下午之后的这一段:「完成度核对与补缺口」和「负责人四项决定」。那份交接 §4 里的第 5、7~10 项仍归它,不在本文范围。
>
> **状态**:本文列出的改动**全部已被别的会话的整区快照带进 HEAD**(最晚到 `f0dadf5b0` 之前),工作区里没有本批的未提交改动。**全部没有编译、没有跑任何测试**。用户说过「编译我自己做」:编译和测试由用户或 Codex 执行,Claude 只改代码和文档,给出命令。
>
> **(2026-10-16 进度补注)** 接手会话已完成 §4 第 1、2 项(评审 → 对抗核实 → 修复 → 复查,改动在工作区未提交),第 3 项进行中,第 4 项待额度重置后跑;详见 `docs/PROGRESS.md` 的 2026-10-16 条目。与下文不一致处以那条为准,主要是:第 2 项把放行写入从「MULTI 里经 `markAdmittedScript`」改成了单段脚本 `admitEntryScript`(`markAdmittedScript` 已删除),所以 §2.2 第 1 行与 §5 末尾 G4b-2 门槛里「冒烟要包含 `markAdmittedScript`(MULTI 里的 EVAL)」都改读「冒烟要覆盖 `admitEntryScript`(直接 `EVALSHA` / `EVAL`)」。
> 同日第 3、4 项也已完成(改动在工作区未提交)。与下文不一致处:§2.2 第 4 行与 §4 第 4 项的「`probeMintedID`,1s 预算」已改为探测直接用 handler ctx(1s 子预算会截断连接池排队,让 Redis 停顿时单独拒绝建角);§6 的告警建议拆成两条——`result=~"parent_exists|owner_epoch_exists"` 出现即告警,`result="error"` 按速率告警且覆盖不到 Redis 卡顿(看 `stage_seconds{stage="id_probe"}`)。§4 第 1~5 项均已完成,只剩 §4 第 6 项运行验证与 PROGRESS 2026-10-16 两条里列的待定项。
> 待定四项中「gateway JDBC socketTimeout」「`AssignGateService.awaitAdmission` 限时」已于同日补完(PROGRESS 2026-10-16 第三条);「排队超 1h 弹回队尾」「noeviction 写满弹空整队」两项要改排队核心,建议方案写在那一条末尾,等负责人点头。

## 0. 一句话

负责人 10-15 先问「全部做完了对吗」,核对后补了缺口(登录排队合入门槛机械拦截、网关限流器 Redis 超时、约 20 份文档);随后让「按最标准修复 1234」四个待定项。四项都已落码。还差三件事:
- 部分语义评审:被额度和 403 打断,网关与 G3 两项一条评审都没跑;
- 第 1 项契约部分 4 条已核实问题的修复;
- 全部运行验证。

## 1. 负责人决定(本段即拍板记录)

| # | 事项 | 定下的做法(「最标准」) |
|---|---|---|
| — | 登录排队 G4b-2 越过合入门槛 | 加机械门禁:`tools/scripts/k8s_deploy.ps1` 的 `$script:LoginQueueMergeGateCleared = $false`。门槛未清时所有部署环境排队都关,`-LoginQueue on` 在任何 kubectl 前拒绝。三项门槛满足后由负责人改常量 |
| — | 其余缺口 | 全部补(CI 路径、文档过时、评审结论落档、待办登记、Codex 命令补失败留存) |
| 1 | 持有 `queue_token` 被限流时的客户端回退口径 | 状态查询走专用端点:持 token 时一律轮询 `/api/queue-status`。assign-gate 带 token 重入只作为 queue-status 失败后的恢复手段;重入被限流时回到轮询。服务端同时根治「名次 0 卡 1h」:放行窗口过后查询回 EXPIRED |
| 2 | 网关 Hikari 超时与探针端口 | 探针走独立管理端口 8082;`connection-timeout` 3000ms;`/api/server-list` 加 1s 短缓存;`/api/queue-status` **不**加 zone 目录前置校验(开区瞬间会误拒新 zone) |
| 3 | sarama lag 探针自动建 topic | 关闭(`Metadata.AllowAutoTopicCreation=false`):只读监控探针不得对被监控系统产生副作用 |
| 4 | G3 设计 P2 / P7(新铸 id 上已有状态) | fail-closed:采用 P7,在 6c 名字登记之前做只读探测,命中或出错都拒绝建角;P2 升级为拒绝,种子窗口里命中时释放名字。探测总是开,与种子开关无关 |

## 2. 已完成(全部在 HEAD,未编译、未测试)

### 2.1 完成度核对与补缺口
详见 [PROGRESS.md](../PROGRESS.md)「2026-10-15 补充:开服瓶颈交接完成度核对与补缺口——登录排队合入门槛机械拦截、网关限流器 Redis 超时、文档对齐」。要点:
- 门禁:`Resolve-LoginQueueSwitch`、测试接缝 `MMORPG_TEST_LOGIN_QUEUE_GATE_CLEARED`(只在 `-DryRun` 生效)、`[PeakShaving]LoginQueueGate=closed` 警告、契约用例 19~22。门禁本身做过一轮语义评审,确认的 2 条已修。
- `java/gateway_node/.../ratelimit/RateLimitConfig.java`:
  - 命令超时 300ms;
  - 断线 `REJECT_COMMANDS`;
  - 未完成命令上限 1024;
  - Bucket4j 整次桶操作 500ms;
  - `RateLimitConfigTest` 9 例。修复做过评审,确认的 5 条已修。
- `.github/workflows/deploy-config-tests.yml` 补 `queue.go`;约 20 份文档加了更正注;sarama 与 miniredis 行为的静态核对结论已落档。

### 2.2 四项决定

| 项 | 落点 | 评审状态 |
|---|---|---|
| 1 服务端 | `go/login/internal/logic/pkg/loginqueue/queue.go`:PopAdmit 的 MULTI 里经 `markAdmittedScript`,meta 存在才 HSET `admitted_at`(避免造出无 TTL 的 meta);Lookup 首次 GETDEL 落空后 HMGET `zone_id` 与 `admitted_at`,有 `admitted_at` 就经 `consumeAdmitAndSign` 再取一次,取不到回 EXPIRED,记 `ExpireReasonAdmitWindowPassed`;测试注入点 `afterAdmitMiss`(默认 nil)。`metrics.go` 加原因常量。`queue_test.go` 新增 5 例、改 2 例。文档:`login-queue-2026-05.md` §5 / §14、`spec_G4b-G.md` 的 F6 等处、`login-queue-stress-runbook.md` 原因枚举 | 评审提出 3 条 nit,**未核实、未修**(§4 第 2 项) |
| 1 契约 | `AssignGateRequest.java`、`AssignGateController.java`(只改注释);`open-server-rate-limit-design.md` §11.3 / §11.5 更正注;`spec_G4b.md` §4.10 J0 与 §8 #10 决定注;`robot/http_assign_gate.go` 重写排队状态机(依赖注入、`pollQueueStatus`)、`robot/main.go` 委托、新增 `robot/http_assign_gate_test.go` | 评审 4 条已核实成立,**未修**(§4 第 1 项) |
| 2 | 8082 与 3000 的部分由上一轮被额度打断的修复代理落码(**不是别的会话**;10-15 PROGRESS 条目曾误写为「另一个会话」,本文更正),包括 `gateway.yaml` 探针、`application.yaml`、`GatewayDataSourcePoolConfigTest`、`k8s_deploy.ps1` 的 gateway ConfigMap `management.server.port: 8082` 与超时镜像、`Get-MysqlPoolMirror` 下限 250、契约用例。本轮补:`ServerListService` 1s 缓存(`AtomicReference<CompletableFuture>` single-flight,失败不缓存)与 `ServerListServiceZoneRowsCacheTest`;`deploy/k8s/README.md` 新增「gateway 探针、管理端口与回退口径」;`spec_G2b.md` 已定注 | **语义评审没跑**(§4 第 3 项) |
| 3 | `go/shared/kafkautil/group_lag.go` 抽出 `newGroupLagProberConfig()` 并设 false;`group_lag_test.go` 加 `TestNewGroupLagProberConfig`、`TestComputeGroupLagUnknownTopic`;`spec_G2a.md` 已定注;`audit-topic-generation-bump-runbook.md` §2.7 补注;`data-service-audit-alerts.yaml` 的 LagProbeBlind 说明补「topic 不存在」 | 已评审:确认 1 条(告警说明)已修;另 1 条(接线无用例)被驳回,理由是只能靠 MockBroker 端到端,代价大,登记为可选加固 |
| 4 | `dataloader/new_player_seed.go`:`newPlayerStateKeys`、`MintedIDProbe`、`ProbeMintedPlayerState`,两条 EXISTS 一个 pipeline。`clientplayerlogin/createplayer_seed.go`:`probeMintedID`,1s 预算;`seedNewPlayer` 返回 tip。`createplayerlogic.go`:6b' 探测、种子拒绝时释放名字;`player:zone` 映射留下并打 ERROR,因为 login 没有删除入口,`DeletePlayerData` 会删到旧持有者。`metrics.go`:`login_create_player_id_probe_total{result}` 预置拒绝结局。测试:新增 2 组,改写 `TestCreatePlayer_SeedNeverOverwritesExistingParent` 与 `TestCreatePlayer_SeedRefusedAfterOwnerEpochMinted`,dataloader 新增 3 例。拒绝复用 `kLoginDataSerializeFailed`。`login.yaml` 锁预算注释;`new-player-first-enter-no-cold-read.md` §2 / §4.5 / §6 / §9 / §10 / §13.2;`spec_G3.md` | **语义评审没跑**(§4 第 4 项) |

Java 版:`D:\luyuan\xuanming-server-mmo\PARITY.md` 末尾已登记「开服瓶颈四项负责人决定」一行,状态「Java 待核」。

## 3. 硬约束(新会话照此执行)

- AGENTS.md §10.1:Claude 不跑编译、测试、gofmt、mvn、pwsh 测试脚本;用户自己编译,**不要派「能不能编过」的静态评审**,评审只看语义 / 运行期。
- 不 `git add -A`,不提交,等用户发话;不 push。
- Java 版仓库 `D:\luyuan\xuanming-server-mmo` 只动 `PARITY.md`;独立客户端仓库 `../mmorpg-client` 未获授权,不读不改。
- 不新增 tip 码(要改 `Tip.xlsx` 并跑导表器)。
- 同一工作区有多个会话并行,且会不定时整区快照提交。动手前 `git diff -- <文件>` 分清哪些是自己的。**代理失败后,先查它实际改过哪些文件**(见下),不要把自己代理留下的半截改动当成别人的:

  ```bash
  grep -o '"name":"\(Edit\|Write\)"[^}]*"file_path":"[^"]*"' <transcriptDir>/agent-*.jsonl
  ```
- 工作流分批跑、提示带续做说明;启动前先看用量,周额度高时按风险分波。
- 文档更正一律追加带日期的更正注,不删原文;PROGRESS 只追加。

## 4. 剩余工作(按顺序)

### 第 1 项:登录排队契约部分的 4 条已核实问题(口径已由主会话定,直接改)

1. **robot 回到 queue-status 轮询的间隔没有下限,连续失败也不退避**(`robot/http_assign_gate.go` 约 :196、:245-257)。queue-status 回 500 且 zone 桶耗尽时,会形成毫秒级循环,把 login 故障放大。定:
   - 持 token 时轮询间隔 = max(`retry_after_ms`, 2s) 再加随机抖动,例如在 [0, 间隔的 20%] 内均匀取,随机源可注入;
   - queue-status 连续失败(触发恢复重入的那几类)时改为指数退避:2s 起翻倍,封顶 30s(不超过 AdmitTTL 60s 的一半),成功一次即复位;
   - 不持 token 的分支不动。
   - 补用例:持 token → queue-status 回 500 → 重入回 `rateLimitQueued(1ms)` → 断言等待 ≥ 2s;连续 500 时等待 2s、4s、8s…封顶 30s;成功后复位。用注入的睡眠函数与随机源断言,不真睡。
2. **503 zone_\* 被当成 queue-status 本身失败去重入**(约 :247 的 `rsp.Code < 500`)。定:只有网络错误、非 200 的 HTTP、或 `code==500` 才触发带 token 的恢复重入;503 zone_\*(zone 不可进,终态)与 404 zone_not_found 交回状态机按错误处理。
   - `http_assign_gate_test.go` 约 :111 那条用 503「login_unavailable」的用例改为 500,网关从不这样回。
   - 新增一例:持 token 时 queue-status 回 503 zone_maintenance ⇒ 直接报错、不发重入。
3. **契约原文同步**:间隔下限 2s 加抖动(同设计稿 §11.5 原第 1 条)、连续失败指数退避封顶 30s、恢复重入只限网络错误 / 非 200 / code=500。
   - 落点:`AssignGateRequest.java` 约 :34-35(本轮新写的注释,可直接改);`open-server-rate-limit-design.md` §11.5 的 2026-10-15 更正(约 :398-414);`spec_G4b.md` §4.10 决定注(约 :540)与 §8 #10 决定注(约 :736)。
   - 后三处用追加补注的方式。
4. **两份文档还写着旧契约、没有更正注**:
   - `spec_G4b.md` §12「客户端」清单(约 :831-835);
   - `login-queue-2026-05.md` §4 回包表(约 :133,429「退避后重试」)与 §9 客户端协议树(约 :369-377)。
   - 各追加 2026-10-15 更正注,指向设计稿 §11.5 的 2026-10-15 更正。

改完对 robot 做一轮语义评审:状态机在所有分支都不忙等、不无限重试、预算耗尽即停。

### 第 2 项:登录排队服务端 3 条 nit 的核实与修复,外加一次交错复查

先核实,成立再修:
- 用例 (i) `TestLookup_AdmitSlotExpiredUnclaimed_ReturnsExpired` 的 meta TTL 断言只能发现 TTL 被抹掉,发现不了被续期。改法:Enqueue 后、PopAdmit 前先 `FastForward(d)`,断言 `metaTTL == entryTTL - d`。
- 用例 (i) 修复前先红在「meta 里有 `admitted_at`」这条结构断言上,没有面向行为。改法:把结构断言挪到独立用例,例如 `TestPopAdmit_MarksAdmittedAtKeepingMetaTTL`。
- `consumeAdmitAndSign` 开头两处注释(约 :382-383、:394-395)仍写「取不到放行槽即仍在排队」。改法:追加带日期的更正。
- 另外独立复查 Lookup / PopAdmit 的所有交错:第一次 GETDEL、HMGET、二次 GETDEL、MULTI、consume 后的 hold、meta 过期或淘汰。确认不会重复签票,也不会把未取的新放行判成 EXPIRED。

### 第 3 项:网关(第 2 项决定)的语义评审 → 对抗核实 → 修复

评审视角:
- `ServerListService` 缓存的并发:成为查库方后再查缓存;finally 摘 future 时不摘别人的;异常与中断;共享脱管实体只读是否成立;陈旧度对开服 / 改状态的影响;探测态仍实时。
- 8082 在 ConfigMap、清单、契约测试三处一致,Service 不暴露 8082,本机 `start_game.ps1` 不受影响,滚动窗口与回退口径。
- readiness 挪到 8082 的代价是否如实记录。
- Hikari 3000 与下限 250。
- 测试面向行为、修复前红、不依赖墙钟。

### 第 4 项:G3 id 复用 fail-closed(第 4 项决定)的语义评审 → 对抗核实 → 修复

评审视角:
- 所有拒绝都在持久副作用之前,或已按约定补偿。名字释放只在确定已登记时做;释放失败怎么处理。
- 所有返回路径释放账号锁与建角锁。
- 探测 1s 预算与请求 ctx 截止的交互。
- 探测与种子 / EnterGame 预加载是同一 Redis、同一组键(共用 `newPlayerStateKeys`)。
- 指标低基数且预置。
- 复用 `kLoginDataSerializeFailed` 与 6c / 6d 故障一致。
- 改写的两条旧用例与新用例语义正确、修复前红。
- 与另一会话的 `createplayer_deadline_test.go` 的交互:它的「种子写之后」子用例挂在单条命令钩子上,探测走 pipeline 钩子。

### 第 5 项:文档收尾

- 在 `docs/PROGRESS.md` 末尾追加本批完成条目:写明第 1~4 项的评审与修复结果、交 Codex 的命令、Java 状态。
- 若口径有变,同步本文与 PARITY 那一行。

### 第 6 项:交用户 / Codex 运行验证(§5)

## 5. 验证命令(用户或 Codex 执行;串行,红了停下归因,不连续重试)

仓库根 `D:\luyuan\xuanming-server-mmo-cpp`。Go 不写死版本,设 `GOTOOLCHAIN=local`、`GOPROXY=off`,提示下载就停。Maven 用 `-o`,缺依赖不要自行下载,先问用户。

1. **PowerShell 7**:先 `Get-ChildItem Env:MMORPG_* | Remove-Item`,再依次跑 `pwsh -NoProfile -File tools/scripts/tests/<名>.tests.ps1`,`<名>` 依次为:
   - `k8s_deploy_contract`
   - `k8s_client_entry_contract`
   - `k8s_migrate_gate`
   - `k8s_gate_drain`
   - `k8s_zone_rollback_gate_router_mode`
   - `k8s_zone_rollback_flush_guard`

   通过标准:各自 fail=0;「合入门槛…」4 例、「Java gateway 探针走独立管理端口…」、池镜像正负向用例都 PASS。**禁止**带着测试接缝变量、去掉 `-DryRun` 跑 `k8s_deploy.ps1`。
2. **go/login**:
   - `gofmt -l` 以下文件:
     - `internal/logic/pkg/loginqueue`
     - `internal/logic/loginpregate`
     - `internal/logic/pkg/dataloader/new_player_seed.go`
     - `internal/logic/clientplayerlogin/createplayer_seed.go`
     - `internal/logic/clientplayerlogin/createplayerlogic.go`
     - `internal/logic/clientplayerlogin/metrics.go`
     - `internal/svc/servicecontext.go`
   - `go vet ./internal/logic/... ./internal/svc/... ./internal/config/...`
   - `go build ./...`
   - `go test ./internal/logic/pkg/loginqueue/ -count=1 -v -run "TestLookup_|TestPopAdmit_|TestConsume_|TestAdmittedKey|TestDispatchOnce_|TestBatchPickFn_|TestPickGate_"`,再跑整包 `-count=1`
   - `go test ./internal/logic/pkg/dataloader/... -run "ProbeMintedPlayerState|Seed" -count=1 -v`
   - `go test ./internal/logic/clientplayerlogin/... -run "TestCreatePlayer_IDProbe|TestCreatePlayer_Seed|TestCreatePlayer_RequestDoneMidCreateCommitsNothing|TestCreatePlayer_ReleasesLock|TestCreatePlayer_HomeZoneRegistrationFailsClosed" -count=10 -v`
   - `go test ./internal/logic/clientplayerlogin/... ./internal/logic/pkg/dataloader/... ./internal/config/... ./internal/svc/... -count=1`
3. **go/shared**:
   - `gofmt -l kafkautil`
   - `go vet ./kafkautil/`
   - `go test ./kafkautil/ -count=1 -v -run "TestNewGroupLagProberConfig|TestComputeGroupLag|TestCommittedOffsets"`,再跑整包

   之后在 `go/data_service` 跑 `go build ./... && go test ./internal/kafka/ -count=1`,在 `go/db` 跑 `go build ./...`。
4. **robot**:`go vet .`、`go build ./...`、`go test -count=1 -v -run "TestAssignGateWithQueue" .`。
5. **java/gateway_node**:
   - `./mvnw.cmd -o -q test "-Dtest=RateLimitConfigTest,AssignGateRateLimiterTest,AssignGateControllerTest,GatewayDataSourcePoolConfigTest,ServerListServiceZoneRowsCacheTest,ServerListServiceTest"`
   - 再跑全量 `./mvnw.cmd -o test`
6. **可选**:
   - `promtool check rules`:把 `deploy/k8s/data-service-audit-alerts.yaml` 的 `spec` 段取成顶层 `groups:` 的临时文件,期望 8 条无错;本机没有就记「未校验」,不下载。
   - `python tools/scripts/gen_docs_index.py --check`:另一会话有一份未登记的 `handoff-global-singletons-p0-20261015-appendix.md`,可能因此不为 0,如实报告。
7. **失败时保留**:
   - 失败用例全文与断言原文;
   - 契约测试的 `[FAIL]` 行与对应 DryRun 子进程整段输出(含 `--- BEGIN MANIFEST ---`);
   - surefire 报告;
   - gofmt -d、vet / build 第一条错误;
   - `git diff --stat`。

**G4b-2 门槛**:要清掉门槛、把 `$script:LoginQueueMergeGateCleared` 改为 `$true`,必须同时满足三项:
- G1b 多 zone 用例由 Codex 跑绿;
- G4b-G 由 Codex 跑绿;
- `spec_G4b-G.md` §8.4 的真实 Redis 冒烟跑通并记进 PROGRESS。冒烟要包含本批新增的 `markAdmittedScript`(MULTI 里的 EVAL)。

清门槛前,第 1 项契约的客户端改动也要排进同一批。

## 6. 已登记、不在本文范围的后续

- **独立客户端**(需客户端任务授权):
  - 持 token 时轮询 queue-status;
  - 网络错误 / 非 200 / code=500 时带 token 重入一次;
  - 重入被限流时回到轮询,间隔 ≥ 2s 加抖动、连续失败退避封顶 30s;
  - 410 丢 token 重来;
  - 不持 token 的 429 仍按 30/60/120s 退避。
- **Java 版**:四项按 PARITY 那一行逐项核对。
- **告警**:`login_create_player_id_probe_total{result=~"parent_exists|owner_epoch_exists|error"}` 出现即告警,尚未加规则。
- **压测汇总**:`stress_summarize.ps1` 的 CreatePlayer 阶段表还没有 `id_probe` 列。
- **登录排队遗留**:
  - PopAdmit 在写放行槽之前就丢掉的条目(`pickFn` 回 `(nil, nil)`、Marshal 或 MULTI 失败)仍停在名次 0,直到 meta 过期;
  - 取走回包丢失时票找不回,客户端从队尾重排(Java 有取走请求号,本仓库没有);
  - 放行很快时,410 之后不带 token 的重调可能撞上 5s 账号冷却,回 429 走外层重试。
- **网关限流器**:没有熔断(Java 版有 5s fail-open hold);启动期懒连接没做。
- **自动建 topic**:kafka-go 消费者与 scene(librdkafka)仍有自动建 topic 的风险,目前靠预建。
- **上游交接已登记的**:
  - G2a 批量模式坏消息计数时机;
  - `loginlogic.go` 的 `accountLock.Release(l.ctx)`;
  - `RouteStore.FenceShards` 的 TiDB 档 64 没有配置载体;
  - `spec_G4a.md` 仍写 `admitted:zone:{z}` SET 与 SCARD。

## 7. 给新账号的开场指令(可直接复制)

```
请接手 docs/handoff/handoff-launch-readiness-decisions-20261015.md 的工作。
1. 先按 AGENTS.md「会话启动门禁」读规定材料,再读这份交接全文(尤其 §3 硬约束)。
2. 工作区有多个会话并行且会被整区快照提交。不要回退、覆盖或顺手整理不属于你当前任务的改动;不要 git add -A,不要提交,等我发话。
3. 顺序:交接 §4 第 1 项(登录排队契约 4 条修复,口径已定)→ 第 2 项(排队服务端 3 条 nit 核实 + 交错复查)→ 第 3 项(网关语义评审与修复)→ 第 4 项(G3 id 复用语义评审与修复)→ 第 5 项(PROGRESS 收尾)。
4. 编译和测试我自己做:你只改代码和文档,不跑任何编译、测试、gofmt,也不要派「能不能编过」的静态评审;评审只看语义 / 运行期。每做完一项给出验证命令(工作目录、命令、通过标准、失败时要留的日志)。没有运行结果前,不要说「编译通过」「已验证」。
5. 每完成一项在 docs/PROGRESS.md 末尾追加条目(只追加),写明 Java 版状态(AGENTS.md §12);Java 仓库 D:\luyuan\xuanming-server-mmo 只动 PARITY.md;独立客户端仓库不碰。
6. 遇到新的拍板项、要超过 30 个文件、要下载镜像或装工具,先停下来问我。
```
