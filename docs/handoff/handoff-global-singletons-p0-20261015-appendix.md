# 附录:全局单点 P0 部署侧四批的实现报告(2026-10-15)

> 配套 [交接](handoff-global-singletons-p0-20261015.md)。本文是四个实现代理的原始报告,主会话没有逐字改写;报告写成时的行号与 HEAD 以各段自述为准,落码后又被多次快照提交,定位一律按函数名 grep。**这四批都没有经过审查**(审查代理因额度与鉴权失败没跑成),报告里的「给 Codex 的命令」是实现者自己给的,接手者审查修复后以修复后的版本为准。

## P0-B 第二批:K8s 模板 Kafka.GroupID 镜像 bin/etc、DisableLegacyPerNodeTopic

P0-B 第二批已经改完。没有编译,没有跑任何测试,需要 Codex 验证。核实结论是全仓已经没有生产者再写旧的 `<类型>-<node_id>` topic,所以 `DisableLegacyPerNodeTopic: true` 和 GroupID 改空放在同一批落码。开工前看过,本任务没有上次留下的半截改动;别人在 `k8s_deploy.ps1`、契约测试、README、交接文档里的未提交改动都保留没动。

#### 改动文件

- `D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\k8s_deploy.ps1`:只改了 `New-NodeConfigMapYaml`,没碰 param 块和 go/db 会话要改的那几段。
  - :2293 用读分区数的同一个函数 `Get-AuthoritativeScalar` 读 `Kafka.GroupID`。`GroupID: ""` 能读出空串、不抛错,`release_common.ps1` 的解析逻辑我逐行看过。
  - 另加了字符校验:只允许字母、数字和 `.` `_` `-`,含引号、反斜杠等就直接报错,免得把 YAML 写坏。
  - :2434 模板改成 `GroupID: "${kafkaGroupId}"`。
  - :2441 新增 `DisableLegacyPerNodeTopic: true`。这是模板常数,不从 bin/etc 镜像,注释里写明了"取值待实测 / 压测复核"和回退方法。
  - 渲染进 ConfigMap 的注释里没有 `game-consumer-group` 字样,不会碰上新用例的断言。
- `D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\tests\k8s_deploy_contract.tests.ps1`:在 :1636–1652 新增一例。没法直接贴在"命令 topic 世代和分区数"那条后面,因为要等 `$battleInfraRun` 先跑出来,所以放在它定义的下一行。对 node-config 和 battle-node-config 各断言三件事:
  - GroupID 等于 `Get-EtcValue` 读出的 bin/etc 值;
  - 整段不含 `game-consumer-group`;
  - `DisableLegacyPerNodeTopic` 为 `true`。
- `D:\luyuan\xuanming-server-mmo-cpp\deploy\k8s\README.md`:
  - 在迁移窗口那条下面新增「K8s 已关老路径」一条,写了核实依据、为什么必须同批,以及上线影响:改 ConfigMap 不会自动滚 Pod;重启 Deployment 形态的 battle 会作废在打的局;关旧路径前要确认线上 login 已写新 topic、旧 topic 无积压;回退办法。
  - 保留期表里 `gate-<id>` 和 `game-events` 两行按修复后的情况订正。
- `D:\luyuan\xuanming-server-mmo-cpp\bin\etc\base_deploy_config.yaml`:只改注释,值不动。订正了 GroupID 注释里过时的 `:150` 行号引用;补了现状和"K8s 从这里镜像"的说明;迁移开关注释写明 K8s 已翻、本机仍是 false、由运维确认后显式翻。
- `D:\luyuan\xuanming-server-mmo-cpp\deploy\docker-compose.yml`:game-events 那段描述 Init 旧行为的注释已经失实,只改了注释。
- `D:\luyuan\xuanming-server-mmo-cpp\docs\handoff\global-singletons-p0-findings-20261014.md`:在 P0-B「原报告:修复设计」开头加了批 2 落码说明。
- 下面两个是任务清单外的 C++ 文件,只改注释:它们写着 K8s 模板"至今写死",这次改完就不对了。
  - `D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\infra\messaging\kafka\kafka_manager.cpp`(:31–34)
  - `D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\kafka_command_test\kafka_manager_init_test.cpp`(:35)

换行和编码与原来一致:两个 .ps1 和 kafka_manager.cpp 仍是全 CRLF,yaml 和 md 仍是全 LF,测试 .cpp 保留了 BOM。

#### 核实结论:没有生产者再写旧 topic(全仓静态 grep)

**C++**
- battle 只有这几个发送点:
  - `cpp/nodes/battle/logic/battle_room_manager.cpp:94-95`(发 gate)和 `:171-172`(发 scene),都走 `ResolveCommandRoute`,也就是新的 `<类型>-cmd_g<N>`;
  - `:368/:374` 发的是 `match-results`。
- scene 的发送点都是别的 topic:
  - `player_lifecycle.cpp:4132/4165` 发 db_task;
  - `transaction_log_system.cpp:84`、`snapshot_system.cpp:110` 发审计 topic;
  - `anomaly_detector.cpp:99/127` 发 anomaly_alert_topic。
- `KafkaManager::Publish`(`kafka_manager.cpp:101`)没有调用方。
- 拼旧 topic 名的 `LegacyPerNodeTopic()`(`node_command_route.h:57-60`)只有单测在用(`kafka_command_test.cpp:282-285`)。消费端自己拼旧 topic 名,在 `node_kafka_command_handler.h:124`。

**Go**
- 共用的寻址收口:`go/shared/kafkacmd/command_topic.go:209-234`;推送封装 `go/shared/kafkautil/gate_push.go:72-73, :135-136, :190-191, :237-238`。
- 各服务的发送点:

| 服务 | 位置 |
|---|---|
| login | `gate_command.go:92-93`,经 `servicecontext.go:590/604/626` 的 `SendToTopicPartition` 发出;2026-09-08 的 `11ba15a0f` 起切到新 topic |
| scene_manager | `changesceneutil.go:30/37`,发送在 `enterscenelogic.go:1191/1384` |
| player_locator | `leasemonitor.go:263-268` |
| match | `team/notify.go:289-296`、`playercontract.go:149` |
| friend | `push.go:140` |
| guild | `push.go:144/146` |

- chat、trade、team、client_rpc_router、go/battle 都不引入任何 Kafka 库。
- `go/login/internal/svc/gate_command_test.go:139-141` 已有测试守住 login 不再写 `gate-<id>`。

**Java**
- `java/gateway_node` 里搜 kafka 零命中。
- 另一个 Java 版仓库(`D:\luyuan\xuanming-server-mmo`)里也没有拼 `gate-`/`scene-` topic 的代码,而且它是独立部署。

**C++ 这个键怎么读**
- `config.cpp:226-228` 读 `Kafka.DisableLegacyPerNodeTopic`,按 bool 解析,只有键存在时才赋值;不写这个键时取 proto 默认值 false(`config.proto:39`)。
- 生效点在 `node_kafka_command_handler.h:119-121`:为 true 时注册完新路径就返回。
- 关掉旧路径后,K8s 上的 C++ 节点不再读 `Kafka.GroupID`:只有 `node_kafka_command_handler.h:126` 读它;`kafka_manager.cpp:66` 只在调用方没给组名时才回落到它,而新路径都会给组名。

#### 给 Codex 的命令

- 工作目录:仓库根。
- 环境:PowerShell 7,先清环境变量 `Get-ChildItem Env:MMORPG_* | Remove-Item`。
- 先做语法检查:
  `pwsh -NoProfile -Command "foreach ($f in 'tools/scripts/k8s_deploy.ps1','tools/scripts/tests/k8s_deploy_contract.tests.ps1') { $e=$null; [void][System.Management.Automation.Language.Parser]::ParseFile((Resolve-Path $f), [ref]$null, [ref]$e); if ($e.Count) { $f; $e | Format-List; exit 1 } }"`
  通过标准:没有输出,退出码 0。
- 再跑契约测试:`pwsh -NoProfile -File tools/scripts/tests/k8s_deploy_contract.tests.ps1`
- 通过标准:
  - 以下这条用例在 PASS 列表里:`node ConfigMap 的 Kafka.GroupID 从 bin/etc 镜像、不再写死共享组名,K8s 上关闭旧的一节点一 topic 路径(node-config 与 battle-node-config)`。
  - `node ConfigMap 的 Kafka 命令 topic 世代和分区数必须与预建 Job 同源`、`battle 配置必须满足独立票据密钥与权威连接上限`、`发现前缀:…` 三条仍然通过。
  - fail=0。
  - 静态计数是 128 个 Test-Case:HEAD 上 124,另一会话的总账用例加 3,本项加 1。go/db 会话稍后还会在文件末尾加用例,所以总数以实际运行为准。
- 想看修复前的红态:在没有本次改动的 HEAD 检出上,把新用例单独套上跑,第一条 GroupID 断言会报 期望 `''`、实际 `'game-consumer-group'`。
- HEAD 上已知的无关红用例:找不到当前工作区的运行记录。`docs/PROGRESS.md:7838` 记过的两条(总账断言写旧中文、`$ProdEnv` 缺排队密钥)HEAD 上都已修。另一会话新增的 3 条 zones.10zones 总账用例还没验证过。如果出现与 Kafka 无关的红,按文件归属单独汇报。
- 失败时保留:`[FAIL]` 那一行和断言原文、对应 DryRun 的整段输出(`--- BEGIN MANIFEST ---` 那几块)。

#### 不确定点

1. **旧 topic 现在是否真的空**:结论只来自读代码。线上 login 及其他 Go 服务实际跑的是不是写新 topic 的版本、旧 topic 有没有积压,都没有运行证据。README 里写了关旧路径前要先确认;如果不满足,把模板那一行改回 `false`。
2. **true 是写死的常数**:改它就是改脚本,没有加部署参数(那会动到 go/db 会话正在改的 param 块)。将来如果需要按环境分别翻,要另外加参数。
3. **回退会重读旧消息**:如果回退成 false,因为 GroupID 已经是空串,旧路径会换成新的消费组,从 earliest 重新读旧 topic 里保留期内的消息。
4. **C++ 端到端没验证**:batch 1 加上本批之后,K8s 上的 battle 链路仍是事后补验项;Linux 镜像和真实 K8s 部署也都没验证。
5. **还没写的两份登记**:
   - `docs/PROGRESS.md` 没有追加条目,需要汇总时补;
   - Java 仓库 `PARITY.md` 的 P0-B 行可以在备注追加"node ConfigMap 的 GroupID 改为镜像 bin/etc;Java 没有 K8s 模板,不适用"。我没改 Java 仓库。
6. **顺带发现,没修**:`bin/etc/base_deploy_config.yaml:95` 注释里写的路径 `go/shared/kafkautil/command_topic.go` 已过时,实际是 `go/shared/kafkacmd/command_topic.go`。

## P0-A 第二批:K8s 给 C++ 下发 zone Redis 口令

P0-A 第二批已全部落码，所有改动都没跑过：没有运行任何测试或部署脚本，契约测试待 Codex 执行。我只用 PowerShell 解析器对 4 个 .ps1 做了静态语法检查，0 错误，这不代表测试通过。

**提交状态，先看这条**：你在我改的过程中提交了 0f6690604 / 676c64789 / 9b8b902da / 41789f474 / 526780b4e，大部分改动已经进了 HEAD。中间两个提交 676c64789 和 9b8b902da 的 `k8s_deploy.ps1` 是半截状态：`Apply-BattlePool` 已改成必须传 `-Workload`，但调用处还没传，所以带 battle 的 infra-up 会失败。HEAD 526780b4e 里这个文件已完整。工作区还剩 6 个文件没提交：两份契约测试、`deploy/k8s/README.md`、`deploy/k8s/AGENTS.md`、`docs/design/shared-redis-split-by-purpose.md`、`docs/handoff/global-singletons-p0-findings-20261014.md`。其中 shared-redis 文档里剩下的差异只有我加的几处。

#### 改了什么

**`D:/luyuan/xuanming-server-mmo-cpp/tools/scripts/k8s_deploy.ps1`**
- **口令写进 node-config**：`New-NodeConfigMapYaml` 的 `zoneredis.password` 改为取 `$script:RedisPassword`，经新函数 `Get-CppZoneRedisPassword` 读取。
- **生成期保护**：以下三种情况直接报错，报错只点名 `MMORPG_REDIS_PASSWORD`，不回显值：
  - 口令还没解析；
  - 非 dev 档口令为空；
  - 口令含 `"`、`\` 或控制字符。

  字符检查另外在 `Initialize-InjectedSecrets` 里、任何写操作之前先跑一次（新函数 `Assert-RedisPasswordYamlSafe`）。这一条同时保护了 Go 服务、gateway 的 ConfigMap 和 Secret `redis-auth`：以前这类口令会让 YAML 写坏，现在提前拒绝，属于更严格的行为变化。
- **指纹注解**：口令非空时，C++ 工作负载的 Pod 模板加注解 `mmorpg.io/zone-redis-auth-hash`。指纹只对口令求值；口令为空时不写这条注解。
  - 两个本脚本模板（gate/scene/battle 共用的 Deployment 模板、scene Fleet）改为通过 `New-CppPodAnnotationBlock` 写注解，与 sidecar 指纹写进同一个 `annotations:` 段。
  - lib 的三个模板通过 `New-CppNodePodCommon` 的 `-PodAnnotations` 拿到同样的注解。
- **没复用 `Add-PodTemplateAnnotation`**：它要求只有一处 template→metadata、且用空格缩进；scene Fleet 有两层 template，Deployment 模板用制表符缩进，都不符合。
- **battle 渲染挪了位置**：从 `Assert-ClientEntryDeployPreflight` 挪到 `Apply-Infra` 开头，仍早于任何基础设施写操作；结果显式传给 `Apply-BattlePool -Workload`，删掉了 `$script:BattleWorkload`。原因是入口预检排在密钥解析之前，在那里拿不到口令。
  - 副作用：battle 生成器的参数错误现在排在密钥检查（以及非 DryRun 的集群现状检查）之后才报，但仍在任何写操作之前。
- 注释写明了：改口令会滚动 C++ Pod，这是期望行为。

**`D:/luyuan/xuanming-server-mmo-cpp/tools/scripts/lib/k8s_client_entry.ps1`**
- `New-ClientEntryPodCommon` 新增可选参数 `-PodAnnotations`。
- 新增 `ConvertTo-ClientEntryPodAnnotationBlock` 和 `Get-ClientEntryPodAnnotationBlock`；`Get-ClientEntrySidecarBlocks` 现在只返回 initContainers 片段，三个模板改用新的注解函数。

**契约测试**
- `D:/luyuan/xuanming-server-mmo-cpp/tools/scripts/tests/k8s_deploy_contract.tests.ps1`：新增 3a 节 7 条（放在 prod 段末尾、3b 之前），battle 段新增 1 条（放在「battle 部署保留 POD_IP…」之后）。新增 6 个 DryRun 子进程。
- `D:/luyuan/xuanming-server-mmo-cpp/tools/scripts/tests/k8s_client_entry_contract.tests.ps1`：新增 1 条库测试。
- `$ProdEnv` 里已经有 `MMORPG_LOGIN_QUEUE_TOKEN_SECRET`（有人先修了），我没有改。

**文档**
- `D:/luyuan/xuanming-server-mmo-cpp/deploy/k8s/README.md`：新增一节「C++ 节点的 zone Redis 口令」，含上线四步和通过标准。
- `D:/luyuan/xuanming-server-mmo-cpp/deploy/k8s/AGENTS.md`：第 105 行后补一行口令来源。
- `D:/luyuan/xuanming-server-mmo-cpp/docs/design/shared-redis-split-by-purpose.md`：§2.5 第 3 条、§12 第 4 条、§13 第 7 条写明结论：选 AUTH，已落码。
- `D:/luyuan/xuanming-server-mmo-cpp/docs/handoff/global-singletons-p0-findings-20261014.md`：在 P0-A 修复设计第 6 条下加了第二批落码说明。

#### 给 Codex 的命令与通过标准

工作目录 `D:\luyuan\xuanming-server-mmo-cpp`，用 pwsh 7。先清掉 `MMORPG_*` 环境变量：dev 基线用例假定这些变量都没设。

```powershell
Get-ChildItem Env:MMORPG_* | ForEach-Object { Remove-Item "Env:$($_.Name)" }
pwsh -NoProfile -File tools/scripts/tests/k8s_deploy_contract.tests.ps1 *> "$env:TEMP\p0a2_deploy.txt"
pwsh -NoProfile -File tools/scripts/tests/k8s_client_entry_contract.tests.ps1 *> "$env:TEMP\p0a2_entry.txt"
Get-ChildItem tools/scripts/tests -Filter *.tests.ps1 | ForEach-Object { pwsh -NoProfile -File $_.FullName *> "$env:TEMP\p0a2_$($_.BaseName).txt"; "$($_.Name) exit=$LASTEXITCODE" }
```

按用例名判，不按退出码判（文件里可能有与本项无关的旧红）。下面 9 条必须是 `[PASS]`：
1. `zone Redis 口令生成期保护:未解析 / 非 dev 档为空 / 含引号、反斜杠或控制字符一律 throw 且不回显值;dev 档为空写空串、不写指纹`
2. `zone Redis 口令(prod):node-config 的 zoneredis.password == MMORPG_REDIS_PASSWORD;gate / scene 带只哈希口令的指纹,与 sidecar 注解同在一个 annotations 段`
3. `zone Redis 口令(dev 基线,不注入):node-config 写空串,C++ 工作负载不带指纹,注解段与改动前逐字相同`
4. `zone Redis 口令换了,gate / scene 的指纹跟着变(换口令自动滚动),sidecar 指纹不变,node-config 写新值`
5. `zone Redis 口令(dev 档注入):external gate StatefulSet 与 Agones scene Fleet 同样带指纹,node-config 写注入值`
6. `负向:prod 缺 MMORPG_REDIS_PASSWORD 在任何写操作之前拒绝并点名变量(C++ 不会拿空口令去连开了 requirepass 的 Redis)`
7. `负向:MMORPG_REDIS_PASSWORD 含双引号时在任何写操作之前拒绝,只点变量名不回显值`
8. `battle:带口令的 infra-up 里 battle-node-config 的 zoneredis.password == 注入值,battle(Deployment / Agones Fleet)带口令指纹;不注入时空串且无指纹`
9. `库:-PodAnnotations 与 sidecar 配置指纹写进同一个 annotations 段(sidecar 在前);都不给时不写 annotations,只给 sidecar 时与之前逐字相同`

另外，原来通过的用例不得变红，重点看这几类：sidecar 注解、`infra-up 默认部署一个 battle…`、`端到端 infra-up agones + external…`、`前提:lib 内部调用的必填参数全部具名传入…`、非 DryRun 的假 kubectl 用例。

如果有 FAIL，用基线提交 592d7e718（我改动之前）的 worktree 跑同一个文件，按用例名对比，区分是不是原本就红。失败时保留完整输出。

**dev 档逐字比对（建议做）**：用 `git worktree add --detach $env:TEMP\p0a2-base 592d7e718` 建基线树。对下面 6 种参数各跑一次基线树和当前树的 DryRun：
- zone-up podip；
- zone-up 加 `-SceneOrchestrator agones -ClientEntryMode external -GateServiceType NodePort -GateNodePortBase 30000 -ClientPublicHost 127.0.0.1`；
- zone-up 加 `-NoCppLogSidecar`；
- infra-up battle 默认；
- infra-up 加 agones + external；
- infra-up 加 `-ClientEntryMode external`（hostPort）。

参数要求：
- 统一传 `-NodeImage registry.invalid/test/mmorpg-node:0123456789ab -SkipGoSvc -SkipJavaSvc`；
- 不设 `MMORPG_REDIS_PASSWORD`；
- 设好 `MMORPG_BATTLE_TOKEN_SECRET` / `MMORPG_GATE_TOKEN_SECRET` 的测试值（同 `$BattleTestEnv`）。

比对方法：取 `--- BEGIN/END MANIFEST ---` 之间、`metadata.name` 为 node-config、battle-node-config、gate、scene、battle 的块，去掉 `\r` 后逐字比较。通过标准是每种参数差异为 0、退出码相同。跑完删掉 worktree 和临时输出。

#### dev 档逐字相同的论证

前提是 dev 档没设 `MMORPG_REDIS_PASSWORD`。

- **node-config / battle-node-config**：口令解析为 `""`，模板写出 `password: ""`，与原来的字面量一致。
- **zone Redis 指纹注解**：口令为空时返回空表。
- **注解段**：只剩 sidecar 一条，新函数拼出的文本与原来的手拼字符串逐字相同：
  - k8s_deploy 侧是 `"\n      annotations:\n        mmorpg.io/cpp-log-sidecar-config-hash: \"<hash>\""`；
  - lib 侧原本就用同一个加引号函数；
  - 12 位十六进制值没有需要转义的字符。
- **关 sidecar 时**：两条注解都没有，返回空串，与原来一样。
- **battle 渲染挪位置**：渲染过程不打印任何东西，输入参数不变，所以 DryRun 输出不变。
- **新增的字符检查**：对空串直接放行。
- **`$script:RedisPassword = $null` 初始化**：与原来「变量未定义」时读到的值相同。

#### 不确定点

- 下面两件事没跑过，靠代码静态推断：
  - PowerShell 函数返回的 `[ordered]` 字典不会被管道展开（仓库里 `Get-GatewayRateLimitSettings` 有同样写法的先例）；
  - 测试里通过 AST 抽出的函数，按动态作用域能读到 `$ReleaseProfile` 和 `$script:` 变量（同 3b 节的写法）。
- 负向用例匹配中文报错原文。pwsh 的错误视图会不会折行没实测；我尽量选了不含空格的片段，只有一条和已有用例同样含空格。
- dev 基线用例假定跑测进程没设 `MMORPG_REDIS_PASSWORD`，与全文件 `$devOut` 的既有假设相同。
- README 里的上线步骤没上过集群，已在文档里写明「取值待实测 / 压测复核」。包括：Redis 首次开口令需手动 `rollout restart statefulset/redis`、在 Redis 重启前起来的 C++ Pod 能自行恢复、external gate 的 StatefulSet 是 OnDelete、需要用 `k8s_gate_drain.ps1` 手动排空。
- 没改的地方：
  - `docs/PROGRESS.md` 的条目没写；
  - Java 仓 `PARITY.md` 里 P0-A 那一行备注还写着「第二批未做」，需要更新（Java 版没有 K8s 部署脚本，第二批对 Java 不适用）；
  - `docs/design/session-extractability-mmo-slg.md:249` 仍写着「入口预检渲染 battle」，属于带旧行号的历史注记，已过期。

## P0-C:etcd 压缩 CronJob(阶段 1a + 1b)

P0-C 阶段 1a 与 1b 已落码，没有编译，也没有运行任何测试，全部待 Codex 验证。阶段 2 没做：etcd.yaml 只加了注释，StatefulSet 的字段一个没动。

开工前查过，没有上次中断留下的 P0-C 半截改动。HEAD 是 526780b4e，不是任务单写的 ab0f27907。

#### 改动文件清单

**新增**
- `D:/luyuan/xuanming-server-mmo-cpp/deploy/k8s/manifests/infra/etcd-maintenance.yaml`
  - 内容是 ConfigMap `etcd-maintenance-scripts`（脚本 `compact.sh`）加 CronJob `etcd-compact`。
  - 每 5 分钟用线性一致读取当前 revision，压缩到 current - 1000000；不超过保留量就打印 `nothing to compact` 跳过。
  - 退出码：读不到 revision、压缩失败、查不到告警时退 1；有活动告警时照常压缩、退 2，让 Job 记为失败；其余退 0。
  - 不做 defrag，不解除告警。`concurrencyPolicy: Forbid`、`backoffLimit: 0`，失败记录保留 5 次。
  - 标签一律 `app: etcd-maintenance`。端点是 3 个成员的 FQDN，namespace 用 `__INFRA_NAMESPACE__` 占位。
  - 镜像与 etcd.yaml 相同（`bitnamilegacy/etcd:latest`），注释写明钉 tag@摘要要等 Codex 实测 M0 后与 etcd.yaml 同批改。
- `D:/luyuan/xuanming-server-mmo-cpp/docs/ops/etcd-maintenance-runbook.md`：写了任务要求的全部内容（原理与取值、NOSPACE 处置与 alarm disarm 顺序、逐成员 defrag、compose 手工 compact、阶段 2 前置与步骤），另加单独 apply 的命令和未验证项清单。取值处标明待实测 / 压测复核。
- `D:/luyuan/xuanming-server-mmo-cpp/tools/scripts/tests/etcd_maintenance_contract.tests.ps1`：静态契约测试，9 条用例，CRLF、无 BOM。
  - CronJob 名与形态；标签不能是 `app: etcd`；占位只能是 `__INFRA_NAMESPACE__`，端点数等于 etcd.yaml 的 replicas。
  - 脚本只用线性一致读，不出现 defrag 和 alarm disarm，查告警不经管道。
  - KEEP_REVISIONS 不小于 100000，且等于 compose 的保留量；compose 用 revision 模式；配额 × 2 + 1Gi ≤ PVC。
  - 清单、etcd.yaml、compose 三处镜像逐字相同。
  - etcd.yaml 去掉注释后不得出现压缩 / 配额参数，守住「阶段 2 未经许可不得提前做」。

**修改**
- `deploy/docker-compose.yml`：etcd 服务加了三项 env（revision 模式、保留 1000000、配额 3221225472）。注释写明：只对新建容器生效，既有容器要 `--force-recreate`；这等于重启本机 etcd，应与整栈重启一起做；单独重启会让在跑的 C++ 节点进入约 181s 的重注册循环；M3 未验证。只改了 etcd 段，kafka 段是另一个会话的钉版改动，我没碰。
- `deploy/k8s/manifests/infra/etcd.yaml`：只加注释（镜像行上方与 PVC 注释处），已核对 diff 里没有非注释行。
- `tools/scripts/k8s_deploy.ps1`：
  - `$infraManifests` 在 `"etcd.yaml"` 后加 `"etcd-maintenance.yaml"`，旁边注释写了部署约束。
  - Show-InfraStatus 的资源列表加 `cronjob`。
  - 两处「五个 infra manifest」的注释改成「`$infraManifests` 里的全部 infra manifest」。
  - 文件保持 CRLF，未碰 go/db 会话要改的段落。
- `tools/scripts/tests/k8s_deploy_contract.tests.ps1`：在「负向:-KafkaBrokers 2」用例之后加一条「infra-up 产物里有 etcd-compact CronJob」，复用现有的 `$battleInfraRun`。保持 CRLF。
- `deploy/k8s/README.md`：etcd 一节加「维护」一段（含有存档环境只 apply 这一个清单的命令）；目录说明与 kafka-topic-init 段里的「五个」同步改掉。
- `deploy/k8s/AGENTS.md`：同步更新三处（表格、新增一条 etcd 压缩说明、「五个」措辞）。
- `.github/workflows/deploy-config-tests.yml`：push 与 pull_request 两段 paths 各加 `deploy/docker-compose.yml`。
- `docs/README.md`：按 AGENTS §5 手工加一行 runbook 索引（生成器没跑）。
- `docs/ops/release-checklist.md`：原来的「etcd `Compact + Defrag`」一项改为指向 runbook。

#### 给 Codex 的命令

工作目录都是 `D:\luyuan\xuanming-server-mmo-cpp`，用 PowerShell 7，本项不需要编译。

**V0 记录现场**
```powershell
git rev-parse HEAD; git status --short
```

**V1 新契约测试**
```powershell
pwsh -NoProfile -File tools/scripts/tests/etcd_maintenance_contract.tests.ps1; "exit=$LASTEXITCODE"
```
- 通过标准：退出码 0，total=9 pass=9。失败时保留完整输出。

**V1b 部署契约测试**
```powershell
pwsh -NoProfile -File tools/scripts/tests/k8s_deploy_contract.tests.ps1; "exit=$LASTEXITCODE"
```
- 通过标准：新用例「infra-up 产物里有 etcd-compact CronJob…」为 PASS。
- 这个文件里有其他会话未提交的用例。若有别的 FAIL，逐条列出并判断是否与 etcd 有关，与 etcd 无关的不归本项。

**V2 修复前必红**（建临时 worktree，只拷入测试文件，跑完必须删掉）
```powershell
$wt = Join-Path $env:TEMP 'etcd-p0c-red'
git worktree add --detach $wt HEAD
Copy-Item tools/scripts/tests/etcd_maintenance_contract.tests.ps1 (Join-Path $wt 'tools/scripts/tests')
pwsh -NoProfile -File (Join-Path $wt 'tools/scripts/tests/etcd_maintenance_contract.tests.ps1'); "exit=$LASTEXITCODE"
git worktree remove --force $wt
```
- 期望：退出码非 0，前 8 条 FAIL（找不到 etcd-maintenance.yaml、compose 缺 env），只有「阶段 2 未经许可不得提前做」PASS。

**V3 compose 只解析、不起容器**
```powershell
docker --context desktop-linux compose -f deploy/docker-compose.yml --profile redis-cluster config --quiet; "exit=$LASTEXITCODE"
```
- 期望退出码 0。

**V4 脚本行为（可选，临时容器，需要本地已有 `bitnamilegacy/etcd:latest`）**
1. 把测试文件里的 `Get-BlockScalar` 函数定义复制到会话里。
2. 取出脚本：
   ```powershell
   [IO.File]::WriteAllText("$out/compact.sh", (Get-BlockScalar -Text ([IO.File]::ReadAllText('deploy/k8s/manifests/infra/etcd-maintenance.yaml') -replace "`r`n","`n") -Key 'compact.sh'))
   ```
3. 按设计文档 P0-C「V4」的 a–d、f 情形在临时容器里跑，期望：
   - a：退出 0，打印 `compacted to revision`
   - b：退出 0，打印 `already compacted`
   - c：`KEEP_REVISIONS=100000000` 时退出 0，打印 `nothing to compact`
   - d：`ETCDCTL_ENDPOINTS=http://127.0.0.1:1` 时必须退出 1。若是 0，说明 etcdctl 不认这个环境变量，停下报告。
   - f：不给 `KEEP_REVISIONS` 时退出非 0。
4. e（NOSPACE）：起一个 `-e ETCD_QUOTA_BACKEND_BYTES=16777216` 的容器，循环写入 100KB 的值直到报空间超限，再以 `KEEP_REVISIONS=100` 跑脚本。期望退出 2、打印 `ALARM ACTIVE`。若一直写不满，说明镜像忽略了配额 env，这本身就是 M3 的答案。
5. 结束后 `docker --context desktop-linux rm -f -v` 删掉本步建的临时容器，保留每条命令的完整输出。

**V5 kind 上 apply 并手动触发一次（可选，只在 kind 集群与 infra 已部署时做；context 与 namespace 以实际为准）**
```powershell
$ctx='kind-mmorpg'; $ns='mmorpg-infra'
$before = kubectl --context $ctx -n $ns get sts etcd -o jsonpath='{.status.currentRevision}'
(Get-Content -Raw deploy/k8s/manifests/infra/etcd-maintenance.yaml).Replace('__INFRA_NAMESPACE__',$ns) | kubectl --context $ctx apply -n $ns -f -
kubectl --context $ctx -n $ns create job --from=cronjob/etcd-compact etcd-compact-manual
kubectl --context $ctx -n $ns wait --for=condition=complete job/etcd-compact-manual --timeout=120s
kubectl --context $ctx -n $ns logs job/etcd-compact-manual
$after = kubectl --context $ctx -n $ns get sts etcd -o jsonpath='{.status.currentRevision}'; "$before -> $after"
kubectl --context $ctx -n $ns delete job etcd-compact-manual
```
- 通过标准：Job Complete；日志里有 `nothing to compact` 或 `compacted to revision`，并有 `no active alarm`；StatefulSet 的 currentRevision 前后相同（证明没有滚动）。
- 卡在拉镜像时，保留 `kubectl describe pod` 输出后停下。

#### 不确定点

1. **外部行为都没实测**：覆盖 command 后 etcdctl 是否在 PATH、是否认 `ETCDCTL_ENDPOINTS`、`-w json` 里 `"revision":` 的写法、「已压缩」的报错原文、NOSPACE 下是否仍接受压缩、默认配额是否 2GiB，以及 bitnamilegacy 镜像是否接受 `ETCD_*` env。compose 的三项 env 可能实际不生效，生效前靠 runbook 的手工压缩兜底。
2. **取值待复核**：KEEP_REVISIONS = 1000000 与配额 3GiB 待 M2 实测和压测复核。K8s 上的配额在阶段 2 之前仍是默认值。
3. **镜像仍是 latest + IfNotPresent**：CronJob 的 Pod 可能被调度到没有该镜像的节点而去 Docker Hub 拉取。钉版要等 M0。
4. **占位的代价**：按决定用了 `__INFRA_NAMESPACE__`。单独 apply 时若忘了替换，Job 每 5 分钟以 1 退出（看得见，但不会自己修好）。runbook 与 README 都给了替换命令。
5. **有意没做**：etcd-alerts.yaml（PrometheusRule）没做，目前「失败可见」只靠 Job 状态与 infra-status。docs/PROGRESS.md 条目、Java 仓库 PARITY.md 登记（本项对 Java 版不适用，Java 版不用 etcd）、交接文档的订正小节，都留给编排方或负责人。
6. **提交时要拆分**：compose、deploy/k8s/README.md、k8s_deploy.ps1、部署契约测试里同时有其他会话未提交的改动（kafka 钉版、P0-A、MySQL 总账等），只提交本项需要 `git add -p`。
7. **阶段 2 的提醒**：契约测试最后一条会在有人给 etcd.yaml 加压缩 env 时变红；真做阶段 2 时要把它改成「StatefulSet 的 env 与 compose 相等」。

## P0-E:共享 Redis maxclients(第 1、2 批)

P0-E 的第 1 批和第 2 批都已落码，但没有编译、没有跑测试，全部待 Codex 验证。第 3 批按决定没做。

#### 改动文件
- `D:/luyuan/xuanming-server-mmo-cpp/deploy/k8s/manifests/infra/redis.yaml`
  - 启动薄壳里 `MAXCLIENTS=40000` 只定义一次，两条 `exec redis-server` 都带 `--maxclients "$MAXCLIENTS"`。
  - 启动前打印 `redis fd limit: soft= hard= need= maxclients=`；硬限不够只打 ERROR，不拒启。
  - 不设 timeout，探针没改。注释里写明「取值待实测 / 压测复核」。
  - StatefulSet 自己的 metadata 上加了注解占位 `mmorpg.io/redis-pod-template-digest: "__REDIS_POD_TEMPLATE_DIGEST__"`（不在 Pod 模板里，给重启闸用）。
- `D:/luyuan/xuanming-server-mmo-cpp/deploy/docker-compose.yml`：redis 加 `--maxclients 40000`，注释说明要 `--force-recreate redis` 才生效。
- `D:/luyuan/xuanming-server-mmo-cpp/deploy/k8s/manifests/infra/redis-ha.yaml`：只在文件头加了接入前置注释（要显式 maxclients、重算总账、haproxy 的 maxconn 也要算），行为没改。
- `D:/luyuan/xuanming-server-mmo-cpp/tools/scripts/tests/redis_maxclients_contract.tests.ps1`（新建，CRLF）：T1~T4 按设计，另加 T5（薄壳里没有 exit、不设 timeout、不自己改 nofile）。
- `D:/luyuan/xuanming-server-mmo-cpp/tools/scripts/k8s_deploy.ps1`
  - 参数块在 `-AllowDisruptiveSwitch` 之后加了 `[switch]$ConfirmRedisCoreRestart`。
  - `Get-ManifestDeploymentReplicas` 加了可选参数 `-LedgerName`，缺省仍是「MySQL 连接总账」；只改了参数和两句报错，其他会话的注释没动。
  - 在 `Assert-MysqlPoolMirror` 之后新开一节「Redis 连接总账与重启闸」：规划档、读 MAXCLIENTS、常数表（每行带代码出处，G 按 2 记，注明待 S6）、纯函数总账、写路径打印、模板摘要、三种判定、读线上注解、重启闸、部署后核对。
  - 写路径门禁块里，`Write-MysqlConnectionLedger` 之后加了一行 `Write-RedisConnectionLedger`。
  - `Apply-Infra` 改了三处：开头在任何写操作之前过重启闸；循环里替换摘要占位；`-WaitReady` 时在 redis 就绪后做 maxclients 核对。
- `D:/luyuan/xuanming-server-mmo-cpp/tools/scripts/dev_tools.ps1`：参数加 `-ConfirmRedisCoreRestart`，`Invoke-K8sDeploy` 和 `Invoke-K8sImage` 都只在给了开关时透传。
- `D:/luyuan/xuanming-server-mmo-cpp/tools/scripts/k8s_image.ps1`：参数块末尾追加同名开关，`Invoke-K8sDeploy` 透传。
- `D:/luyuan/xuanming-server-mmo-cpp/tools/scripts/tests/k8s_deploy_contract.tests.ps1`：在 MySQL 告警用例之后新增一节，共 9 个用例，没放文件末尾。
- `D:/luyuan/xuanming-server-mmo-cpp/deploy/k8s/README.md`：新增「Redis 连接上限与总账」小节，参数说明里加了一条 `-ConfirmRedisCoreRestart`。
- `D:/luyuan/xuanming-server-mmo-cpp/docs/design/shared-redis-split-by-purpose.md`：表格里两行注明「已由 P0-E 按更宽语义实现」，§4.3 加一行（每个拆出的实例显式 `--maxclients`、按用途重算、C++ 每进程一条）。
- `D:/luyuan/xuanming-server-mmo-cpp/docs/design/capacity-model-hok-scale.md`：加一行同样内容。

#### 给 Codex 的命令与通过标准
工作目录 `D:/luyuan/xuanming-server-mmo-cpp`，用 PowerShell 7。

0. 先存基线：跑一遍 `git status --short`，再把 `tools/scripts/tests` 一级下每个不以下划线开头的 `.ps1` 各跑一次，记下退出码。其他会话还在改这些文件，所以要有基线才能区分谁的问题。

1. 新静态测试：
   ```
   pwsh -NoProfile -File tools/scripts/tests/redis_maxclients_contract.tests.ps1
   ```
   - 通过：退出码 0，汇总 `total=5 pass=5 fail=0`。
   - 可选的红检查：`git worktree add --detach "$env:TEMP/p0e-red" 526780b4e`，只把这个测试文件拷进去跑。期望退出码非 0，T1 和 T4 报「应当恰好出现 1 次,实际 0 次」。跑完 `git worktree remove --force`。

2. 全量契约测试：
   ```
   pwsh -NoProfile -File tools/scripts/tests/k8s_deploy_contract.tests.ps1
   pwsh -NoProfile -File tools/scripts/tests/k8s_client_entry_contract.tests.ps1
   ```
   按用例名判。下面 9 个新用例必须 PASS（按名字前缀找）：
   - 「Redis 连接总账:规划档峰值 ≤ 80% × redis.yaml 的 MAXCLIENTS…」
   - 「Redis 总账输入:客户端表的副本数就是各 manifest 的 replicas…」
   - 「自检:Redis 总账公式逐项可复现…」
   - 「部署写路径打印 Redis connection ledger…」
   - 「重启闸纯判定…」
   - 「负向:重启闸要确认时 throw…」
   - 「Redis Pod 模板摘要只跟 Pod 模板走…」
   - 「infra-up DryRun:sts/redis 自己的 metadata 带 16 位模板摘要注解…」
   - 「dev_tools / k8s_image 的 -ConfirmRedisCoreRestart…」

   下面这些原有用例也必须仍然 PASS：
   - MySQL 总账各例。
   - 「-KafkaBrokers 3:…」（它检查整份输出不能有未替换的占位）。
   - 「infra-up 产物里有 etcd-compact…」。
   - GateRouterMode 透传。
   - client_entry 里的三条「AST 契约:…的每个 splat 键都是下游 param 块声明过的参数」，新开关的透传键也由它们覆盖。

   其余文件的退出码要和基线逐个相同。失败时保留完整输出。

3. dev_tools 的 DryRun 透传检查（不连集群；两个变量用的是契约测试里同一份夹具值）：
   ```
   $env:MMORPG_BATTLE_TOKEN_SECRET='contract-battle-ticket-0123456789abcdef012345'
   $env:MMORPG_GATE_TOKEN_SECRET='contract-gate-ticket-0123456789abcdef012345'
   $common = @('-Command','k8s-infra-up','-DryRun','-WaitReady','-SkipGoSvc','-SkipJavaSvc','-InfraNamespace','p0e-dryrun','-KubeContext','p0e-never-exists','-NodeImage','registry.invalid/test/mmorpg-node:0123456789ab')
   pwsh -NoProfile -File tools/scripts/dev_tools.ps1 @common
   pwsh -NoProfile -File tools/scripts/dev_tools.ps1 @common -ConfirmRedisCoreRestart
   ```
   - 两次都是退出码 0。
   - 输出里都有 `Redis connection ledger: zones=2 steady=667 peak=1131 maxclients=40000 warn_at=32000 headroom=38869; plan zones=4 peak=1635 headroom=38365`（这是手算值，以实跑为准），以及 `[dry-run] redis maxclients check skipped (manifest MAXCLIENTS=40000)`。
   - `Redis restart gate: … confirm_redis_core_restart=` 后面，第一次是 `False`，第二次是 `True`。
   - k8s_image 不做 DryRun 检查：它的 release-all DryRun 也要先有 bin/gate 等运行时产物，本机没有。它的透传由第 2 步的假下游用例和 AST 契约核对。

4. 修复设计第八节第 4 步的薄壳实测（三种 nofile × 有无口令），本批没有替代它，仍需 Codex 跑。

#### 重启闸第一次上线怎么做
1. **合入后第一次** `infra-up`、不带 `-SkipInfra` 的 `all-up` 和 `k8s-release-all` 都会被拒。原因有两个：线上 `sts/redis` 还没有摘要注解，这次 maxclients 改动本身也改了 Pod 模板。这是预期行为。维护窗口之前，zone 级发布用 `zone-up` 或 `all-up -SkipInfra`，不受影响。
2. 已有环境要**在到线之前**先在线调大（不持久，重启 redis-0 后回到清单值）：
   ```
   kubectl -n <infra> exec redis-0 -c redis -- sh -c 'if [ -n "$REDIS_PASSWORD" ]; then REDISCLI_AUTH=$REDIS_PASSWORD; export REDISCLI_AUTH; fi; exec redis-cli config set maxclients 40000'
   ```
3. 维护窗口内：
   - 先停写，确认 db_task 积压、重试队列、死信都为 0。
   - 再跑 `pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-infra-up -WaitReady -ConfirmRedisCoreRestart <照常的参数>`。
   - 期望先看到 `[RedisCoreRestart]Confirmed: … <none> -> <摘要>`，然后 redis-0 重启（全服登录中断并重放 AOF），最后打出 `Redis maxclients check OK`。
   - infra-up 会连带 apply `mysql.yaml`，建议和 G2b 那次重启并进同一个窗口。
4. 核对：
   - `kubectl -n <infra> logs redis-0 -c redis` 里有 `redis fd limit:` 一行，没有 ERROR，也没有 Redis 自己下调 maxclients 的日志。
   - `kubectl -n <infra> get sts redis -o jsonpath='{.metadata.annotations.mmorpg\.io/redis-pod-template-digest}'` 有值。
   - 之后模板不变的 infra-up 会打 `pod template unchanged`，不再需要开关。

#### 不确定点
- **外部常识，待实测**：
  - dash 下 `ulimit -Sn` / `-Hn` 的写法能不能用。
  - Redis 是否会自己把软限抬到硬限、硬限不够时如何下调 maxclients、日志原文是什么。
  - `CONFIG SET maxclients` 能否运行期生效；到线后 redis-cli 是否也连不进。
  - GOMAXPROCS 是否真是 2（S6）；go-zero 是否确实不设 PoolSize（S2b）。
  - `kubectl get … --ignore-not-found -o json` 资源不存在时是否输出为空；`kubectl exec pod/redis-0 -c redis` 和 `redis-cli --raw config get` 的输出形态。
- **重启闸的盲区**：
  - 只比较「上次经本脚本 apply 的模板」，`kubectl edit` 改过的看不到。
  - 极老集群从 Deployment 迁到 StatefulSet 时，线上没有 sts，会判为「不存在」直接放行。这本来就是已文档化的停机变更。
  - 行尾注释会多要一次确认（偏保守）。
- **总账口径**：
  - trade 按资产通道打开计（K8s 现在固定关闭，实际 0 条），gateway 按限流打开记 2 条，两项都偏保守。
  - player-inbox（代码还没建 Redis 客户端）和 guild（没有 K8s 清单）没计入。
  - zone 集合的取法和 MySQL 那份是两份代码，用「两行 `zones=` 必须相等」的用例钉住。
  - CI 触发路径不含 `docker-compose.yml`，只改 compose 时不会重跑 T4。
  - 40000 条连接最坏约 0.8GB 内存（外部常识），而 redis.yaml 只有 512Mi 的 requests、没有 limits。
- **多会话并发**：参数块和写路径门禁块是 go/db 会话也要改的段，我各只插入了一处。它下次 Edit 前必须重读。
- **顺带看到、没碰**：`k8s_deploy.ps1` 里 player-inbox 的 ConfigMap 生成器去服务 yaml 里查 `SharedRedis.Host`，但 `go/player_inbox/etc/player_inbox.yaml` 里那段叫 `CoreRedis`。这是其他会话的在途改动，可能导致生成期 throw，需要那边确认。

#### 未做
- 第 3 批：告警（依赖 redis_exporter，待拍板）、go-redis 显式 PoolSize、C++ 断连日志带原因。
- `docs/PROGRESS.md` 追加条目，和 Java 仓库 `PARITY.md` 登记（Java 版状态：待做）。任务清单里没有这两项，也避免和并行会话撞车。
- `deploy/k8s/AGENTS.md:24` 仍写着「redis 是单副本 Deployment」，已过时，没改。
