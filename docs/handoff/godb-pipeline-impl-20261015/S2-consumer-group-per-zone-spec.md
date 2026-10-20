# S2 实现规格：S2 ③ 消费组按 zone 拆 + Kafka 镜像钉版本 + 回档 Step 2 修正

## 摘要
把 K8s 上全 zone 共用的 go/db 消费组 db_rpc_consumer_group 换成每 zone 一组 db_rpc_consumer_group_z{ZoneId}，不停服迁移。核心有三层：(1) go/db 进程内的交接闸。闸在第一次 Consume 入组之前判定；对方组释放本 topic 之后，在 Setup 里用对方的已提交位点播种（MarkOffset 原值，不加 1，本组已有位点就不动）；守望只在看到对方 Stable、持有本 topic 分区、并且位点在前进时才让出。另有规则 R7：两个组在同一分区都有位点、且对方更大时，拒绝放行并告警，在进程内兜住附录 B2/B4 的陈旧位点重放。(2) ConfigMap 换形状。zone 组与反向交接一律写新键 ConsumerGroup，同时写 GroupHandoffFrom，不再写 GroupID。旧二进制把 GroupID 当必填，缺了直接拒启，所以能力证明来自二进制本身，替代附录 F5 的 OCI label 和注解方案。k8s_deploy 新增 -DbConsumerGroup keep|zone|legacy：keep 读线上 ConfigMap；namespace 已删、ConfigMap 不存在时一律按 zone 形状渲染；DryRun 下渲染 zone 形状的预览，并打印告警。(3) 工具：merge_zone 只认唯一的权威组；回档改为永远落到 zone 组——Step 5 重置 zone 组，再删掉 legacy 组在本 topic 上的位点；Step 2 的等待循环删除，换成停服前按本 topic 过滤的只读快照，只提示、不 throw；kafka_offset_reset 新增 -FinalizeHandoff / -RetireGroup 收尾；stress_summarize 的默认组改为按 -Zone 推导。前置：Kafka 镜像四处钉到 apache/kafka:4.3.1@sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837。2026-10-15 查 Docker Hub，latest 与 4.3.1 是同一个 index digest，latest 推于 2026-06-24，所以钉版不改变现网 broker 版本。login 的 GroupID 本轮只改成 optional，值不动；删除放到收尾 Step 7（下一轮）。按 4 个 PR 拆：PR-A 钉版 5 个文件；PR-B go/db Phase A 13 个；PR-C 脚本与工具 17 个；PR-D 文档 9 个，另加 Java 仓库 PARITY.md 1 个。合计本仓库 44 个、Java 1 个。全部未编译，待 Codex 验证。

## 现状
【按 2026-10-15 工作区；本项涉及的文件 git status 都干净，只有 docs/ops/merge-zone-runbook.md 有他人未提交的 1 行改动】
1) go/db 只用一个组名、新组从 Oldest 开始：go/db/internal/kafka/key_ordered_consumer.go:890-901 用 sarama V3_5_0_0 建配置、Offsets.Initial=OffsetOldest（:893），再以 cfg.ServerConfig.Kafka.GroupID 调 NewConsumerGroup（:895-899）；没设 ClientID，会话、心跳、再平衡超时取 sarama 默认 10s/3s/60s。消费循环 :1040-1051 每轮直接 Consume。Setup :1692-1709 只检查分区契约。Cleanup :1711-1722 只打日志。ConsumeClaim :1732 的 claimCtx 直接派生自 session.Context()。Stop :2274-2283 依次 cancel → wg.Wait → releaseRetryOwnership → consumer.Close。重试 ready / processing / 死信键都按 topic 拼（:907-912），与组无关；游标、排序锁、owner_epoch 同样按 topic 拼。
2) 配置：go/db/internal/config/config.go:227-258 的 KafkaConfig 里 GroupID 是必填（:230，`json:"GroupID"`），没有交接与超时字段；Normalize（:310-379）没有 error 返回。go/db/db.go:57-67 先 MustLoad，再 Normalize，再拼 topic；:121-135 建并启动消费者（Start 不阻塞，readiness 走 gRPC 6000）。go/db/etc/db.yaml:14 是 `GroupID: "db_rpc_consumer_group"`，本机由 tools/scripts/go_services.ps1:425-432 给带引号的 GroupID 行追加 _z{Zone}，所以本地与压测一直是每 zone 一组。
3) K8s：tools/scripts/k8s_deploy.ps1:4100 在 db ConfigMap 里写死这个字面量，每个 zone 相同；:4372 login ConfigMap 也有同名 GroupID。login 只在 go/login/internal/config/config.go:455 声明为必填，代码不读，同名键还在 go/login/etc/login.yaml:489、deploy/login-stack.linux/login.yaml:204。New-GoSvcConfigMapYaml 在 :3652。Apply-OneGoSvc 在 :5511-5584，顺序是先渲染 manifest，再 apply ConfigMap，再 apply Deployment；只改 ConfigMap 不会触发滚动。Add-PodTemplateAnnotation（:5346）遇到已有 annotations: 的 manifest 会 throw，而 deploy/k8s/manifests/go-svc/db.yaml:46-50 的 Pod 模板已有 prometheus 注解。Remove-Zone（:6250-6256）直接删 namespace，ConfigMap 一起没了。Invoke-Kubectl 在 DryRun 下不连集群（:2144-2163）。写路径预检块在 :6756 起，Assert-ClientEntryDeployPreflight 在 :6762。
4) 回档：tools/scripts/k8s_zone_rollback.ps1:122 的 KafkaGroup 缺省是 legacy 组。Step 1（:858-873）删 namespace。Step 2（:876-902）在 zone-down 之后按整组 describe 等 LAG，不按 topic 过滤，超时只 WARN「proceeding anyway」；本 zone 的 db 已停，必然等满 300s。Step 5（:941-958）调 kafka_offset_reset.ps1 重置 legacy 组；只要别的 zone 在线，组就不是 Empty，重置一定被拒。kafka_offset_reset.ps1:106-121 的 Assert-KafkaToolOutputClean（「Error:」行即失败）已在 d5cbb8a8e 落地，所以现在是显式失败而不是静默。git log：该文件最后一次实质改动是 d5cbb8a8e（D1 第 0 批的 FLUSHDB 防护）；e4ff1d165 只把日志改成英文，行数不变；工作区没有未提交改动，可以动。
5) 合服：tools/merge_zone/main.go:118 的 defaultKafkaGroup=db_rpc_consumer_group，:319 的 -kafka-group 缺省取它。preflight.go:43-47 的 kafkaLagSource.TotalLag(ctx, topic, group) 只支持单组；:64-72 跑一次 describe；:83-133 按表头解析，对缺 topic 行和 LAG 为「-」都 fail-closed；:225、:247-257 是 P3。调用点在 merge_run.go:253、unmerge.go:182、integration_test.go:1489。dev_tools.ps1:1001-1005 转发的是 db.yaml 的裸组名，而本地真实组名带 _z，所以本地合服门禁一直在查一个不存在的组，被 fail-closed 拦下。dev_tools.ps1:321 的 RollbackKafkaGroup 缺省也是裸名，:329 / :1587 转发 KafkaDrainTimeoutSec。stress_summarize.ps1:74-75 默认 topic=db_task_zone_1、group=db_rpc_consumer_group（本地查不到）。
6) Kafka 镜像：apache/kafka:latest 出现在 deploy/docker-compose.yml:6 与 :83、deploy/k8s/manifests/infra/kafka.yaml:156（:168 的注释命令也写着 latest，且没设 imagePullPolicy）、kafka-topic-init.yaml:61；deploy/k8s/README.md:1193 与 :1208 登记了浮动 tag；docs/ops/kafka-cluster-production-runbook.md:45、:138 也写着 latest，:138 还在用 Kafka 4.0 已删除的 kafka-mirror-maker.sh。脚本用到的 CLI 只有 kafka-topics / kafka-configs / kafka-consumer-groups / kafka-broker-api-versions / kafka-reassign-partitions（后者只出现在提示文案里），4.x 都有，且都不带 --zookeeper。sarama v1.43.1 的 ClusterAdmin 有 DescribeConsumerGroups / ListConsumerGroupOffsets，GroupMemberDescription 有 GetMemberAssignment；ErrGroupIDNotFound=69、ErrGroupSubscribedToTopic=86（已在模块缓存核对）。

## 复核阻断项 / 修正的处理
- **H-2.3**：交接 §2.3：不能直接改组名（新组没有位点时 OffsetOldest 全量重放；游标缺键时放行旧写）
  - 处理：新组第一次入组前先过交接闸（group_handoff.go 的 await），在 Setup 里用对方组的已提交位点播种（seed）；只有对方也没有位点的分区才落回 Oldest，记 no_offsets 指标并打 Error。ConfigMap 的 zone / legacy 形状不写 GroupID，没有交接闸的旧二进制直接拒启，不会以新组名从 Oldest 起步。Offsets.Initial 保持不变（改它会让真正的全新 topic 丢消息）。
- **H-login-GroupID**：k8s_deploy.ps1 的 login ConfigMap 有同名 GroupID 字段（login 只声明、不用），写清怎么处理
  - 处理：本轮：go/login/internal/config/config.go:455 改成 json:"GroupID,optional" 并注明已废弃；k8s_deploy login 模板（:4372）只加一行注释，说明这是死配置、不是 db 的消费组、值不随切组变化、Step 7 删除；值保持 db_rpc_consumer_group 不动。login.yaml 与 login-stack.linux/login.yaml 本轮不动，避开 S3 在 login.yaml 上的改动。Step 7（下一轮，等全部 login Pod 跑上 optional 版本 ≥7 天）：从 go/login/etc/login.yaml:489、k8s_deploy login 模板、deploy/login-stack.linux/login.yaml:204 三处删除。先放宽再删，新旧 login 混跑期间都能启动。
- **H-kafka-pin**：③ 的前置：Kafka 镜像四处钉到同一个确实存在、发布超过两周的版本，并核对 KRaft 配置项、CLI 路径与参数
  - 处理：钉到 apache/kafka:4.3.1@sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837。依据：Docker Hub API，4.3.1 于 2026-06-23 推送，latest 于 2026-06-24 推送，两者 index digest 相同，所以 K8s（latest 默认 Always 拉取）现在跑的就是 4.3.1。KRaft 用到的环境变量（PROCESS_ROLES、CONTROLLER_QUORUM_VOTERS 静态投票、CLUSTER_ID、LOG_DIRS 等）4.x 都支持；脚本引用的 CLI 4.x 都在、都不带 --zookeeper；入口 /__cacert_entrypoint.sh + /etc/kafka/docker/run 要由 Codex 用 image inspect 复核。README 与 kafka 运行手册同步改；运行手册里 Kafka 4.0 已删除的 MM1 示例加注说明。
- **H-step2**：交接 §4.2 回档 Step 2：zone-down 之后等 LAG 排空必然超时；按 B5/F6 删除，或挪到 zone-down 之前、按本 topic 过滤、只作提示，不要改成 throw
  - 处理：删除 Step 2 的等待循环（:876-902）和 -KafkaDrainTimeoutSec 参数（dev_tools 的 -RollbackKafkaDrainTimeoutSec 一并删）。在第 0 步预检里（停服前，只在 -Apply 下）加一次只读快照 Show-KafkaTopicLagSnapshot：对 zone 组与 legacy 组各 describe 一次，只打印 TOPIC==本 topic 的行和 LAG 合计；任何失败只 Write-Warning，绝不 throw。.DESCRIPTION 第 2 条改为说明已删除及原因（Step 5 会把位点拨回 TargetTime，排空没有意义），3–7 的编号不变。已核对该文件没有在途改动。
- **B1**：反向交接互等约 30-60s：被闸住的成员在 Setup 里也持有分配，守望误判后让出
  - 处理：按 F1：闸挪到 Start() 消费循环里、第一次 Consume 之前（入组前），被闸住的一方根本不入组、不持有分配。守望让出要三条同时成立：对方 Stable、对方成员分配里有本 topic 的分区、对方在本 topic 上的已提交位点在相邻两次观测之间前进过（watchOnce）。两边都配 GroupHandoffFrom 时也不互等。用例 TestGroupHandoffWatch_DoesNotYieldToGatedPeer、TestGroupHandoffWatch_YieldsOnlyWhenPeerHoldsAndAdvances 钉住。
- **B2**：反向之后不删 legacy 位点会从冻结点大段重放；来回切换同理
  - 处理：F2 收尾强制化：每次正向或反向交接完成后，都用 kafka_offset_reset.ps1 -FinalizeHandoff 删除旧组在本 topic 上的位点，并 describe 复核。另加进程内兜底 R7：本组与对方在同一分区都有位点、且对方位点大于本组时，拒绝放行（gate outcome wait_own_behind_peer，打 Error，告警 DbConsumerGroupHandoffOwnBehindPeer），不从陈旧位点续消费。
- **B3**：闸门永久挂在每次再平衡上，又造出跨 zone 的共享故障点（依赖 legacy coordinator）
  - 处理：按 F3：闸只在本进程第一次入组前判定一次（released 标记），之后的再平衡不再看对方；只有守望让出时才复位、重新过闸。快路径：对方在本 topic 上没有位点、且没有成员持有分区时直接放行，对方在再平衡也不等。降级：本组在全部分区都已有位点时，对方查询失败也放行（peer_unreachable_own_complete）。收尾之后各 zone 的稳态重启完全不依赖 legacy 组。等待时长有 gauge 和告警（45s warning、300s critical）。
- **B4**：组归属真相源（线上 ConfigMap）会被删掉；DryRun 读不到线上；能力注解只是声明，且发布顺序自相矛盾
  - 处理：(a) keep 模式下 ConfigMap 不存在时，按 zone 形状渲染（ConsumerGroup=zone 组，GroupHandoffFrom=legacy 组）。这样做安全：交接闸会处理全新 topic（快路径）、legacy 权威（播种）、zone 权威（本组有位点）、两组冲突（R7 拦住）四种情况；旧镜像遇到这个形状直接拒启。所以不需要部署脚本去查 Kafka，组归属由二进制运行时按 Kafka 判定。(b) DryRun 加 keep 时渲染 zone 形状预览，并 Write-Warning 说明这次没读集群、真跑时以线上 ConfigMap 为准，不静默。(c) 能力证明改为必填键：zone / legacy 形状不写 GroupID，交接闸之前的二进制在 conf.MustLoad 就因「field ServerConfig.Kafka.GroupID is not set」拒启；config_test 的 TestPreHandoffBinaryRefusesZoneShapedConfig 钉住 go-zero 的这一行为。发布顺序：PR-B（二进制）先于 PR-C（脚本）。PR-C 之前生成器不变，PR-B 镜像在旧形状 ConfigMap 下零行为变化。
- **B5**：k8s_zone_rollback 的两处改法要么检查失效（删 namespace 之后读 ConfigMap），要么拦死回档（Step 2 改 throw）
  - 处理：(a) 不再需要「线上仍是 legacy 就 throw」：回档一律以 zone 组为权威。Step 5 重置 zone 组到 TargetTime（zone-down 之后 zone 组是 Empty 或不存在，重置能生效），再用 -RetireGroup 删除 legacy 组在本 topic 上的位点。legacy 组即使还有别的 zone 的成员，只要没有成员订阅本 topic，broker 就允许删除。Step 6 显式传 -DbConsumerGroup zone。回档同时把 legacy zone 迁到 zone 组，第 0 步不用判组，也不新增 kubectl 读（原有「预检只读一次集群」的断言不受影响）。(b) Step 2 删除，见 H-step2。
- **B6**：merge_zone「逐分区取各候选组 LAG 最小值」方向是 fail-open
  - 处理：按 F7：候选组为 zone 组 db_rpc_consumer_group_z{zone} 加 -kafka-legacy-group（缺省 db_rpc_consumer_group，传空关闭）。逐个 describe 后，只取在本 topic 上至少有一行数值 CURRENT-OFFSET 的组作为唯一权威组，只对它算 LAG。两组都有位点就 fail-closed，并提示先跑 -FinalizeHandoff；都没有也 fail-closed；CLI 出错（「does not exist」除外）同样 fail-closed。-kafka-group 显式传值时只看这一个组。用例覆盖「非活跃组位点更高时必须拒绝」。
- **F1**：闸挪到入组之前，Setup 只播种；守望要求持有分区且位点前进；补反向不让出、两侧不互等的单测
  - 处理：见 B1。实现落在 Start() 消费循环（await）、Setup（seed）、groupHandoff.watch / watchOnce。单测：TestGroupHandoffWatch_DoesNotYieldToGatedPeer（反向与两侧都开守望）、TestGroupHandoffWatch_YieldsOnlyWhenPeerHoldsAndAdvances。
- **F2**：每次交接都由脚本执行并复核收尾：新组 N 个分区都有数值位点且不小于播种值，然后删旧组本 topic 位点并复核；反向前目标组仍有位点就 throw
  - 处理：kafka_offset_reset.ps1 新增 -FinalizeHandoff 模式（参数 -Group 新权威组、-RetireGroup 旧组、-Topic、-Partitions N）。Test-HandoffFinalizeReady 做前置检查：新组 0..N-1 都要有数值 CURRENT-OFFSET，且凡旧组有数值的分区，新组都不小于它，否则 throw 并列出全部问题。通过后执行 --delete-offsets，再 describe 复核旧组没有本 topic 的数值行；不加 -Apply 时只打印计划。「反向前目标组仍有位点就 throw」由进程内 R7 实现（见 B2），比部署期检查覆盖面更广（旧脚本、手工部署都拦得住）；runbook 写明反向前确认正向已收尾。删除附录里「不删也安全」的说法。
- **F3**：闸只在交接完成前起作用；对方无位点无持有就放行；收尾后滚一次去掉 GroupHandoffFrom；等待告警
  - 处理：前两点与告警按原样实现（见 B3；告警 DbConsumerGroupHandoffWaiting 45s / DbConsumerGroupHandoffStuck 300s）。偏离点：GroupHandoffFrom 保留，不再滚一次去掉。理由：去掉它要新增第三种渲染模式和一次额外滚动，而误用「无交接的 zone 形状」（zone 组还没有位点、legacy 有位点时）会退回 Oldest 全量重放。B3 的根因（每次再平衡都依赖 legacy coordinator）已由「只闸一次」和「本组位点完整则不依赖对方」消除，保留的守望还能继续防旧发布物误部署。代价：每个 Pod 每 5s 对 legacy 组发 DescribeGroups 加 OffsetFetch；开 Kafka ACL 时要给 legacy 组授 Describe。写进设计文档 §10，待负责人确认（见 decisions_needed 第 3 条）。
- **F4**：组归属以 Kafka 为准；两组都有位点 throw；zone / shared 只许单 zone 的 zone-up；写明 DryRun 行为
  - 处理：-DbConsumerGroup zone|legacy 遇到 -Command 不是 zone-up 时，入口直接 throw（中文文案）。all-up、zone-down 等一律拒；k8s_image 的 release 流程不转发该参数，等于 keep。DryRun 行为见 B4(b)。「以 Kafka 为准」与「两组都有位点」改由二进制运行时判定（R7），部署脚本不去 kubectl exec 进 kafka Pod。理由：只有运行时能同时覆盖新脚本、旧脚本、手工部署三条路径，而且正向收尾之前两组本来就合法地都有位点，部署期一律 throw 会挡住正常发布。偏离点写进设计文档 §10。
- **F5**：能力证明来自镜像本身，不来自 manifest 注解；消除发布顺序矛盾
  - 处理：改用「必填键证明」替代 OCI label：带 GroupHandoffFrom 或 zone 组的 ConfigMap 只写 ConsumerGroup。二进制自身的 config 校验（ValidateConsumerGroup）也强制这一点：GroupHandoffFrom 非空时必须用 ConsumerGroup、且不能同时有 GroupID，所以生成器写不出旧二进制能「错误接受」的形状。不依赖部署机有 docker 或 registry 访问，全新 namespace、回档 Step 6、DryRun 都同样生效。Pod 模板注解 mmorpg.io/db-consumer-group / mmorpg.io/db-group-handoff-from 只用来触发滚动，不作能力证明。发布顺序：PR-B 先发，PR-C 后发。
- **F6**：k8s_zone_rollback：组判定和 legacy 检查挪到 Step 0；Step 2 不改 throw；Step 5 只重置权威组再删另一组位点（与 F2 同一函数）；dev_tools 本地回档路径也调用
  - 处理：见 B5 与 H-step2。Step 5 走 kafka_offset_reset.ps1 -Group <zone 组> -RetireGroup <legacy 组> -ToDatetime。-RetireGroup 与 -FinalizeHandoff 共用同一个 Remove-RetiredGroupTopicOffsets。回档是有意回退，所以 -RetireGroup 路径跳过「新组 ≥ 旧组」检查。dev_tools 的 kafka-offset-reset 新增 -KafkaRetireGroup / -KafkaFinalizeHandoff 透传；k8s-zone-rollback 透传 -RollbackKafkaLegacyGroup，-RollbackKafkaGroup 缺省改为空串（由回档脚本推导）。
- **F7**：merge_zone 只认唯一权威组；两组都有位点 fail-closed；保留 -kafka-group 覆盖；补「非活跃组位点更高时必须拒绝」用例
  - 处理：见 B6。接口改为 TotalLag(ctx, topic, groups []string) (lag, group, err)，纯函数 resolveAuthoritativeLag 承担判定，merge_unit_test 覆盖。
- **F8**：改正安全论证并写进 docs/design/db-consumer-group-per-zone.md：(a) 闸 + 守望做不到互斥；(b) 读写不同分区，「无重叠就没有并发写者」不成立；(c) 重试与组无关；(d) 游标缺键默认放行
  - 处理：设计文档 §3 逐条写明：(a) 重叠窗口上限约为守望周期 5s 加一轮再平衡，兜底靠按 topic 的游标、排序锁、owner_epoch；(b) 读任务回写缓存与别的分区上的写本来就会并发（① 落地前），交接只是不放大它；(c) 重试按 topic 拼键，交接闸覆盖不到，正确性仍靠排序锁租约，④ 落地后才收窄；(d) FLUSHDB 或丢键之后游标缺键会放行旧写，K8s 上游标在共享 DB 0，k8s_zone_rollback 的 FLUSHDB 防护拒绝清它。
- **F9**：交 Codex 核实：go-zero 对未知键 / 缺失必填键的行为；钉版之后核对 --reset-offsets 对非 Empty 组的输出与退出码、--delete-offsets 在组 Stable 但无人订阅该 topic 时能否成功
  - 处理：go-zero 两种行为由 config_test 的单测钉住（Codex 跑）：TestPreHandoffBinaryRefusesZoneShapedConfig 证明缺必填键会报错；TestPreHandoffBinaryIgnoresUnknownKeys 证明未知键被忽略。Kafka CLI 在 4.3.1 上的五项行为列进 Codex 步骤 V3：非 Empty 组 reset 的输出与退出码；不存在组的 describe 文本与退出码；不存在组 --reset-offsets --to-datetime 能否建出位点；组 Stable 但无人订阅该 topic 时 --delete-offsets 是否成功；有人订阅时的 GROUP_SUBSCRIBED_TO_TOPIC 输出。外加 sarama DescribeGroups 对不存在组返回的 State / Err。拉镜像属于下载，要负责人同意（decisions_needed 第 2 条）。
- **F10**：测试补充：反向交接空窗 <15s；正向→反向→正向每次 seeded=10、起点等于对方 CURRENT-OFFSET；切组后起 legacy 实例触发守望；守望负向；k8s_deploy 三种情况；回档 Step 0 / Step 2；merge_zone 非活跃组更高时拒绝
  - 处理：单测覆盖守望负向（被闸住、位点不动时不让出）、k8s_deploy 的 DryRun / 全新 namespace / 旧形状解析、回档不被 Step 2 拦死、merge_zone 非活跃组更高时拒绝。集成测试由 Codex 执行（步骤 V5）：正向→收尾→反向→收尾→正向，每次 seeded 合计 10、起点等于对方停后的 CURRENT-OFFSET；最长消费空窗 <15s；切组后起一个 legacy 配置的实例，conflict_total 增加、守望让出；停掉它之后从自身位点续消费、不跳过任务。「镜像无能力标记」一项改为「旧二进制加 zone 形状 ConfigMap 拒启」的配置单测（与 F5 的替代方案对应）。
- **F11**：清单补漏：login-stack.linux/login.yaml:199 的 GroupID；zone_data_rollback_en/zh、data-consistency-stress-testing 的组名口径；行号订正；文件数可能超过 30 要先报告；交付写明未编译
  - 处理：login-stack 文件归入 Step 7（现在在 :204，不是 :199）。PR-D 同步 zone_data_rollback.md / _en / _zh、data-consistency-stress-testing.md:216、merge-zone-runbook.md（只追加或改那几行，该文件有他人未提交的改动）、server_merge_design.md:107/:359、no-single-node-horizontal-scaling-20261001.md §3.4（追加更正行）。附录里的行号（config.go Normalize :265-298、PARITY 第 40 行）已过时：Normalize 现在在 :310-379，PARITY 行号由实现者 grep 定位。文件总数 44+1，超过 30，按 4 个 PR 拆、每个都少于 30，见 decisions_needed 第 1 条。交付说明与 PROGRESS 写明「未编译，待 Codex 验证」。

## 逐文件改动
### [modify] deploy/docker-compose.yml
PR-A。L6（kafka）与 L83（kafka-topic-init）的 image 改为 apache/kafka:4.3.1@sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837。kafka 服务上方加中文注释：为什么钉版（浮动 tag 加命名卷时跨版本读旧日志目录；group coordinator / OffsetDelete 语义会随镜像漂移，③ 的交接闸依赖它们）；2026-10-15 核对 Docker Hub 时 latest 与 4.3.1 是同一 index digest；升级时四处同改（本文件两处、kafka.yaml、kafka-topic-init.yaml），先 docker buildx imagetools inspect 取 Digest 行，再在本地跑 kafka-topic-init 与 F9 的 CLI 核对。

### [modify] deploy/k8s/manifests/infra/kafka.yaml
PR-A。L156 image 改为同一个钉版引用，紧接着加 imagePullPolicy: IfNotPresent（内容寻址，不必 Always）；L168 注释里的 docker image inspect 命令改成钉版引用；加一行注释：改这一行会让下一次 infra-up 滚动重启 kafka StatefulSet。

### [modify] deploy/k8s/manifests/infra/kafka-topic-init.yaml
PR-A。L61 image 改为同一个钉版引用（已有 IfNotPresent）。L60 的注释「与 kafka.yaml 同一个镜像」保留，并补一句「digest 也必须相同」。

### [modify] deploy/k8s/README.md
PR-A。L1193 的 inspect 命令改成钉版引用。L1208「未改、但值得知道」的第一条改写为：已钉 apache/kafka:4.3.1@sha256:77e3…（2026-10-15，与当时 latest 同 digest，钉版不改变运行版本）；四处同改清单与升级步骤；apply 后 kafka StatefulSet 会滚动，单 broker 档等同一次 broker 重启，按本节注意事项在无玩家窗口执行。

### [modify] docs/ops/kafka-cluster-production-runbook.md
PR-A。L45、L138 的 apache/kafka:latest 改为钉版引用。Step 4 的 kafka-mirror-maker.sh 示例上方加注：Kafka 4.0 已移除 MirrorMaker 1，4.x 没有该脚本，改用 MM2（connect-mirror-maker.sh <mm2.properties>），示例未验证。流程不重写。

### [modify] go/db/internal/config/config.go
PR-B。KafkaConfig（:227-258）：GroupID 改为 `json:"GroupID,optional"`，注释写明这是旧形状键，本地 db.yaml 与未切组的 K8s ConfigMap 用它；新形状不写它，旧二进制把它当必填会拒启，这是有意的能力证明。新增字段：ConsumerGroup string `json:"ConsumerGroup,optional"`；GroupHandoffFrom string `json:"GroupHandoffFrom,optional"`；SessionTimeoutMs、HeartbeatIntervalMs、RebalanceTimeoutMs 三个 int `json:",optional"`。新增常量：DefaultKafkaSessionTimeoutMs=10000、DefaultKafkaHeartbeatIntervalMs=3000、DefaultKafkaRebalanceTimeoutMs=60000（等于 sarama v1.43.1 默认值）；MinKafkaRebalanceTimeoutMs=15000（= kafka.claimDrainTimeout 10s + 5s 提交余量，注释写明两处要同步）。Normalize 末尾：三个超时 ≤0 时回填默认值。新增 func (k KafkaConfig) EffectiveGroupID() string：ConsumerGroup 非空取它，否则取 GroupID。新增 func (k KafkaConfig) ValidateConsumerGroup() error，调用方须先 Normalize，错误文本用英文：两者都空 → 'Kafka.GroupID or Kafka.ConsumerGroup must be set'；两者都非空 → 'set only one of Kafka.GroupID and Kafka.ConsumerGroup'；GroupHandoffFrom 非空而 ConsumerGroup 为空 → 'Kafka.GroupHandoffFrom requires Kafka.ConsumerGroup (without Kafka.GroupID) so that binaries without the handoff gate refuse to start'；GroupHandoffFrom == EffectiveGroupID → 'Kafka.GroupHandoffFrom must differ from the consumer group'；HeartbeatIntervalMs*3 > SessionTimeoutMs → 报错；RebalanceTimeoutMs < 15000 → 报错。注释写明：不要命名为 Validate——go-zero conf.Load 会在 Normalize 之前自动调用根结构实现的 Validate()。不新增 DbTaskConsumerGroup 之类的拼名函数：go/db 运行时从不自己拼组名，ConfigMap 写字面量。

### [modify] go/db/internal/config/config_test.go
PR-B。新增用例，见 tests。用 conf.LoadFromYamlBytes 配 holder 结构，沿用文件现有做法。

依赖：go/db/internal/config/config.go

### [new] go/db/internal/kafka/group_handoff.go
PR-B，约 300 行。文件头用中文写清职责与不变量（入组前判定一次、Setup 播种不加 1、守望只凭正面证据让出、R7）。常量：handoffPollInterval=1s，handoffWatchInterval=5s，handoffAdminTimeout=5s（sarama 调用不收 ctx，靠 Net.*Timeout），handoffWaitLogInterval=10s，consumerKafkaVersion=sarama.V3_5_0_0（消费者与 inspector 共用，key_ordered_consumer.go:891 改用它）。类型：type groupInspector interface{ DescribeGroup(group, topic string) (peerGroupView, error); CommittedOffsets(group, topic string, partitions []int32) (map[int32]int64, error); Close() error }；type peerGroupView struct{ State string; Held map[int32]struct{} }（Held 只在 State==Stable 时可信）；常量 groupStateEmpty / groupStateDead / groupStateStable 对应 Kafka 的状态字符串。saramaGroupInspector：newSaramaGroupInspector(brokers []string, version sarama.KafkaVersion, clientID string) (groupInspector, error)，配置 Version、ClientID=clientID+'-handoff'、Net.Dial/Read/WriteTimeout=handoffAdminTimeout、Metadata.Retry.Max=1，用 sarama.NewClusterAdmin。DescribeGroup：调 DescribeConsumerGroups([]string{group})；desc.Err==ErrGroupIDNotFound 时按 Dead、Held 为空处理；其它非零 Err 返回错误；成功时用 heldPartitions 解析。纯函数 heldPartitions(desc *sarama.GroupDescription, topic string) (map[int32]struct{}, error)：逐个成员 GetMemberAssignment，解码出错时返回 error（fail-closed），结果为 nil 时跳过，只收集 Topics[topic] 里的分区。CommittedOffsets：调 ListConsumerGroupOffsets(group, {topic: partitions})；resp.Err 为 ErrGroupIDNotFound 时全部按 -1；其它顶层 Err、block 缺失或 block.Err 非零都返回 error；正常返回 block.Offset（-1 表示没有位点）。type groupHandoff struct{ inspector groupInspector; group, peer, topic string; partitions []int32; now func() time.Time; sleep func(ctx context.Context, d time.Duration) bool; mu sync.Mutex; released bool; sessionYield context.CancelFunc; watchPrev map[int32]int64 }，watchPrev 只由守望 goroutine 读写。newGroupHandoff(inspector, group, peer, topic string, partitionCnt int32) (*groupHandoff, error)：peer 或 group 为空、或 peer==group 时报错；partitions 取 [0, partitionCnt)。newGroupHandoffFromConfig(kcfg db_config.KafkaConfig, group string, newInspector func() (groupInspector, error)) (*groupHandoff, error)：GroupHandoffFrom 为空时直接返回 (nil, nil)，不调用 newInspector（T8，Phase A 零行为变化）。evaluate() gateDecision{pass bool; outcome, detail string}，按顺序判定：①本组 CommittedOffsets 出错 → 等待，wait_own_unavailable；②对方 CommittedOffsets 或 DescribeGroup 出错时，本组全部分区 ≥0 则放行 peer_unreachable_own_complete，否则等待 wait_peer_unavailable；③对方 Stable 且 Held 非空 → 等待 wait_peer_holding；④R7：存在分区 p 满足 own[p]≥0、peer[p]≥0 且 peer[p]>own[p] → 等待 wait_own_behind_peer，detail 带上 p、own、peer；⑤对方在全部分区都没有位点、且 Held 为空 → 放行 no_peer_offsets（不管对方处于什么状态）；⑥对方 State 为 Empty、Dead、空串或 Stable（此时 Held 已为空）→ 放行 peer_released；⑦其它状态（PreparingRebalance、CompletingRebalance、未知）→ 等待 wait_peer_rebalancing。await(ctx) error：已 released 立即返回 nil；否则循环——每轮 evaluate 并调 metrics.CountHandoffGate(outcome)；放行时在锁内置 released=true，SetHandoffWaiting(0)，ObserveHandoffWait(等待时长)，打 Info 日志 'group handoff released: group=%s peer=%s topic=%s reason=%s waited=%s'；等待时 SetHandoffWaiting(已等时长)，原因变化或距上次日志 ≥10s 时打日志（wait_own_behind_peer / wait_*_unavailable 用 Errorf，其余 Infof；own_behind_peer 的文本要写清两种处置：删除陈旧一方在本 topic 上的位点，见 tools/scripts/kafka_offset_reset.ps1 与设计文档）；sleep 返回 false 时 SetHandoffWaiting(0) 并返回 ctx.Err()。seed(session sarama.ConsumerGroupSession, claimed []int32) error：先查本组在 claimed 上的位点，出错时计 aborted 并返回 error；只有存在 own<0 的分区才查对方，出错时同样计 aborted 并返回 error；逐个分区处理——own≥0 计 own_present（Debug 日志），peer≥0 则 session.MarkOffset(topic, p, peer, '')（原值，不加 1）、计 seeded 并打 Info 日志 'group handoff seeded partition: group=%s topic=%s partition=%d offset=%d peer=%s'，两者都没有则计 no_offsets 并打 Error 日志，说明该分区从 Offsets.Initial（oldest）开始。watch(ctx, done chan<- struct{})：handoffWatchInterval 定时调 watchOnce，ctx 结束后 close(done)。watchOnce()：未 released 时把 watchPrev 置 nil 后返回；任一查询出错直接返回（只凭正面证据让出）；prev 为 nil 时只记录基线；Stable、Held 非空、且相邻两次观测间有分区位点前进（prev<0 且 cur≥0，或 cur>prev）三者同时成立时：CountHandoffConflict，打 Error 日志 'group handoff conflict: peer %s is consuming topic %s (held=%v, committed advanced); yielding this session'，然后 yield()。yield()：在锁内置 released=false，若 sessionYield 非空就调用它。setSessionYield(cancel) bool：在锁内保存，返回当前 released（Setup 拿到 false 时立即 cancel，堵住守望与 Setup 之间的竞态）。clearSessionYield()。Close() 关闭 inspector。同文件新增：consumerClientID(zoneID uint32, hostname string) string，返回 'db-z<zone>-<host>'，[^A-Za-z0-9._-] 替换成 '-'，去掉首尾的 '-'，总长截到 200，host 为空时只返回 'db-z<zone>'；type rebalanceGapTracker struct{ mu sync.Mutex; lastEnd time.Time }，方法 sessionEnded(at time.Time)、sessionStarted(at time.Time) (time.Duration, bool)，没有过 sessionEnded 时返回 false。

依赖：go/db/internal/config/config.go, go/db/internal/metrics/metrics.go

### [new] go/db/internal/kafka/group_handoff_test.go
PR-B。fakeInspector 按调用顺序回放 Describe / Committed 的脚本化结果，并记录调用次数；fakeClaim 实现 sarama.ConsumerGroupClaim（Topic / Partition / InitialOffset / HighWaterMarkOffset / Messages）；encodeAssignment 测试辅助按大端手工编码成员分配字节（int16 版本号、int32 topic 数、int16 长度加 topic 名、int32 分区数加各分区、int32 -1 表示 nil userData）。具体用例见 tests。

依赖：go/db/internal/kafka/group_handoff.go

### [modify] go/db/internal/kafka/key_ordered_consumer.go
PR-B。只动下列位置，不碰 markAppliedSeq / markAppliedOwnerEpoch 及其调用顺序。(1) 结构体（:67-103）新增 handoff *groupHandoff（GroupHandoffFrom 为空时为 nil）、handoffDone chan struct{}（在 Start 里创建，没 Start 过就是 nil）、gap rebalanceGapTracker、now func() time.Time（缺省 time.Now，单测可注入）。(2) NewKeyOrderedKafkaConsumer（:871-950）：先对 cfg 的本地副本调用 cfg.Normalize()（幂等，防止调用方漏调）；kcfg := cfg.ServerConfig.Kafka；ValidateConsumerGroup 出错时返回 fmt.Errorf('invalid Kafka consumer group config: %w')；groupID := kcfg.EffectiveGroupID()；sarama 配置 Version=consumerKafkaVersion，ClientID=consumerClientID(cfg.ZoneId, hostname)，Consumer.Group.Session.Timeout / Heartbeat.Interval / Rebalance.Timeout 取三个毫秒值；Offsets.Initial 保持 OffsetOldest 不变，注释说明按 zone 拆组靠交接闸播种，不靠改这里；NewConsumerGroup 用 groupID；之后调 newGroupHandoffFromConfig(kcfg, groupID, func(){ return newSaramaGroupInspector(kcfg.Brokers, consumerKafkaVersion, config.ClientID) })，出错时先关 consumerGroup 再返回；c.groupID=groupID，c.handoff=handoff。(3) Start（:1011-1056）的消费 goroutine：handoff 非空时先创建 c.handoffDone 并 go c.handoff.watch(c.ctx, c.handoffDone)；每轮 Consume 之前，handoff 非空就先 await(c.ctx)，返回错误时打 Info 日志 'consumer group stopped while waiting for group handoff: groupID=%s peer=%s topic=%s' 后退出；启动日志加上 handoffFrom=%s。(4) consumerGroupHandler（:1688）新增字段 yieldCtx context.Context、yieldCancel context.CancelFunc；新增 sessionScope(session) (context.Context, <-chan struct{})：yieldCtx 非空时返回它和它的 Done()，否则返回 session.Context() 和 nil channel；新增 yielded() bool。(5) Setup（:1692-1709）在分区契约检查之后：handoff 非空时调 seed(session, claims[topic])，出错返回 fmt.Errorf('group handoff seeding failed: %w')（sarama 会 release 并让 Consume 返回错误，消费循环 1s 后重新入组；此时 released 已为 true，不会再过闸）；若已有 yieldCancel 先调用一次，再建 yieldCtx, yieldCancel = context.WithCancel(session.Context())；handoff 非空且 setSessionYield 返回 false 时立即 yieldCancel()；最后 d, ok := c.gap.sessionStarted(c.now())，ok 时 metrics.ObserveRebalanceGap(d)。(6) Cleanup（:1711-1722）：yieldCancel 非空就调用；handoff 非空时 clearSessionYield()；c.gap.sessionEnded(c.now())；都要容忍 Setup 失败后的 Cleanup（字段为 nil）。(7) ConsumeClaim（:1724-1840）：:1732 改为 parentCtx, yieldDone := h.sessionScope(session); claimCtx, cancelClaim := context.WithCancel(parentCtx)。外层 select（:1744）新增分支 case <-yieldDone：session ctx 已结束时 drainAfterStop('session canceled')，否则 drainAfterStop('handoff peer reclaimed partition')，两种都 return nil。派发 select 里 <-claimCtx.Done() 分支的判定改成 session.Context().Err()!=nil || h.yielded() 时 return nil。drainAfterStop 闭包保持原样，S1 的 inflight 等待挂在里面，让出路径自动继承。(8) Stop（:2274-2283）：在 consumer.Close() 之后加 c.closeHandoff()——handoffDone 非空就等它关闭（ctx 已取消，最多等一次在途 admin 调用），然后调 handoff.Close()，失败只记日志。(9) 其余函数（handleTask / processTaskBatch / runDBOp / deferFailedTask / 重试相关）一律不动。

依赖：go/db/internal/kafka/group_handoff.go, go/db/internal/config/config.go, go/db/internal/metrics/metrics.go

### [modify] go/db/internal/kafka/key_ordered_consumer_test.go
PR-B。recordingSession.MarkOffset（:97）改为在锁内把 (topic, partition, offset) 记进 markOffsetCalls，新增 markOffsetCallsSnapshot()；ResetOffset 仍为空实现。现有用例的断言不变。新增回归用例 TestReplayAfterSeed_WritesAreOrderingStale，见 tests。

依赖：go/db/internal/kafka/key_ordered_consumer.go

### [modify] go/db/internal/metrics/metrics.go
PR-B。新增 6 个指标，subsystem 为 db，都不带 player_id。① db_consumer_group_rebalance_gap_seconds：histogram，无 label，buckets 0.5,1,2,3,5,10,15,20,30,45,60,120；Help 写明从 session Cleanup 到下一次 Setup 完成，含交接闸等待。② db_consumer_group_handoff_total{result}：counter，result 取 seeded | own_present | no_offsets | aborted，每个分区每个 session 记一次。③ db_consumer_group_handoff_gate_total{outcome}：counter，outcome 取 no_peer_offsets | peer_released | peer_unreachable_own_complete | wait_peer_holding | wait_peer_rebalancing | wait_own_behind_peer | wait_peer_unavailable | wait_own_unavailable，每次判定记一次（≤1/s/Pod）。④ db_consumer_group_handoff_waiting_seconds：gauge，当前正在闸前等待的时长，没在等时为 0。⑤ db_consumer_group_handoff_wait_seconds：histogram，buckets 0.5,1,2,5,10,15,30,45,60,120,300,600,1800，放行时记一次。⑥ db_consumer_group_handoff_conflict_total：counter，无 label。register() 的 MustRegister 列表追加这 6 个，并把 handoff_total 的 4 个 result、gate_total 的 8 个 outcome、conflict_total 先以 0 建出序列（同 taskDeferredTotal 的做法）。导出函数与常量：ObserveRebalanceGap(time.Duration)、CountHandoffSeed(result string)、CountHandoffGate(outcome string)、SetHandoffWaiting(time.Duration)、ObserveHandoffWait(time.Duration)、CountHandoffConflict()；常量 HandoffSeedSeeded / HandoffSeedOwnPresent / HandoffSeedNoOffsets / HandoffSeedAborted 与 HandoffGate*（八个，取值同上），Help 与常量一一对应，同 ownerEpochGuardTotal 的注释纪律。

### [modify] go/db/internal/metrics/metrics_test.go
PR-B。新增 TestConsumerGroupHandoffSeriesPrecreated，见 tests。

依赖：go/db/internal/metrics/metrics.go

### [modify] go/db/etc/db.yaml
PR-B。L14 保持 GroupID: "db_rpc_consumer_group" 的格式（带引号，行尾注释可以改，go_services.ps1 的正则依赖这个形状），注释改为：本机由 go_services.ps1 追加 _z{Zone}；K8s 由 k8s_deploy.ps1 渲染 ConsumerGroup: db_rpc_consumer_group_z{ZoneId} 加 GroupHandoffFrom，渲染时不写本键；本值同时是 k8s_deploy / 回档 / merge_zone 推导组名的基名。其下加 5 行被注释掉的键与说明：# ConsumerGroup（与 GroupID 二选一）、# GroupHandoffFrom（只能和 ConsumerGroup 一起用）、# SessionTimeoutMs: 10000、# HeartbeatIntervalMs: 3000、# RebalanceTimeoutMs: 60000（约束 heartbeat×3 ≤ session、rebalance ≥ 15000），并指向 docs/design/db-consumer-group-per-zone.md。

### [modify] go/login/internal/config/config.go
PR-B。KafkaConfig.GroupID（:455）改为 `json:"GroupID,optional"`，注释写明：已废弃，login 只生产不消费、代码不读；保留为可选是为了让带该键的旧 ConfigMap 仍能加载；三处配置的删除顺序见 docs/design/db-consumer-group-per-zone.md §8 Step 7。本轮不改 login.yaml。

### [modify] deploy/k8s/db-task-retry-alerts.yaml
PR-B。文件头的指标来源补上 consumer_group_* 指标。新增 group db-task.consumer-group-handoff，interval 30s，四条规则。① DbConsumerGroupHandoffWaiting：max by (namespace)(db_consumer_group_handoff_waiting_seconds) > 45，warning；描述写明 db 在交接闸前等待，本 zone 存盘消费暂停，查对方组是否仍有本 zone 的旧 Pod、或对方组是否反复再平衡（切组期间冻结其它 zone 的发布）。② DbConsumerGroupHandoffStuck：同一表达式 > 300，critical；描述提醒 db_task 保留期 24h，等太久未消费的存盘会被 Kafka 删除。③ DbConsumerGroupHandoffOwnBehindPeer：sum by (namespace)(increase(db_consumer_group_handoff_gate_total{outcome="wait_own_behind_peer"}[5m])) > 0，critical；描述写明两组都有位点且对方更大，处置是用 kafka_offset_reset.ps1 删除陈旧一方在本 topic 上的位点，不要手工改游标。④ DbConsumerGroupHandoffConflict：sum by (namespace)(increase(db_consumer_group_handoff_conflict_total[10m])) > 0，critical；描述写明有旧组成员在消费本 topic（多半是旧发布物误部署），先处理掉那个 Pod。no_offsets 不设告警（全新 topic 首启时属正常）。

依赖：go/db/internal/metrics/metrics.go

### [new] docs/design/db-consumer-group-per-zone.md
PR-B，§11.5 要求跨模块改动先落设计。标题行为「# go/db 存盘消费组按 zone 拆分与不停服交接」，docs/README 的索引行必须与它逐字一致。章节：§0 结论与拍板（2026-10-13 授权；开服门槛；前置是 Kafka 钉版）。§1 现状（file:line）。§2 方案：命名；两种 ConfigMap 形状与必填键证明；交接闸规则表 ①–⑦ 与 R7；播种；守望；超时键与约束；ClientID；指标与告警。§3 安全论证与 F8 (a)–(d) 的更正。§4 部署脚本：keep / zone / legacy、DryRun、全新 namespace、注解滚动。§5 收尾（FinalizeHandoff 的前置检查与删除复核）。§6 回档：永远落到 zone 组；Step 2 删除；Step 5 重置加 retire。§7 合服：唯一权威组。§8 上线步骤 1–7、单 zone 反向交接、回退到交接闸之前镜像的手工兜底（db 缩到 0 → 用 --reset-offsets --to-offset / --from-file 给 legacy 组灌位点 → 手工把 ConfigMap 改回 GroupID 形状 → 扩回）。§9 与 ①④ 及条件写的关系（不依赖条件写；先于 ④，以缩小滚动影响面）。§10 偏离附录之处与代价：F3 保留 GroupHandoffFrom；F4 部署期查 Kafka 改为运行时 R7；F5 改为必填键证明；F6 回档不再判组。§11 未核实项（F9）。§12 Java 不适用（无 Kafka 存盘链）。

### [modify] docs/README.md
PR-B。在「玩家数据与存储」分类下、db-service-root-credentials.md 之前，按字母序插入一行：- [db-consumer-group-per-zone.md](design/db-consumer-group-per-zone.md) — go/db 存盘消费组按 zone 拆分与不停服交接（必须与 gen_docs_index.py 的生成结果逐字一致，由 Codex 跑 --check）。

依赖：docs/design/db-consumer-group-per-zone.md

### [new] tools/scripts/lib/kafka_consumer_groups.ps1
PR-C。纯函数库：只定义函数、不读脚本级变量、dot-source 无副作用，文件头用中文说明。ConvertFrom-KafkaConsumerGroupDescribe -Lines [string[]] -Topic [string] 返回 [pscustomobject]@{ GroupMissing [bool]; HeaderFound [bool]; Rows }，Rows 每行为 @{Partition [int]; CurrentOffset [Nullable[long]]; LogEndOffset; Lag}，「-」记为 $null。表头按列名定位 TOPIC / PARTITION / CURRENT-OFFSET / LOG-END-OFFSET / LAG，不按下标；只返回 TOPIC==-Topic 的行；任一行匹配 (?i)consumer group '[^']*' does not exist 即 GroupMissing=$true。Get-KafkaCommittedOffsetMap -Parsed：只取 CurrentOffset 非空的行，返回 @{ [int]partition = [long]offset }。注释写明 4.3.1 上的输出文本由 Codex 实测（F9）。

### [modify] tools/scripts/kafka_offset_reset.ps1
PR-C。dot-source lib/kafka_consumer_groups.ps1。新增参数 [string]$RetireGroup=''、[switch]$FinalizeHandoff。模式互斥检查（:126-134）把 FinalizeHandoff 算进去。FinalizeHandoff 必须同时给 -Group、-RetireGroup、-Topic、-Partitions >0；RetireGroup 不能等于 Group；RetireGroup 不能与 -DeleteAndRecreateTopic 同用。新增函数：Invoke-KafkaConsumerGroupDescribe -CgTool -BootstrapServer -Group -Topic，先捕获输出与退出码；输出含「does not exist」时直接返回 GroupMissing，否则先过 Assert-KafkaToolOutputClean 再解析。Test-HandoffFinalizeReady -NewOffsets [hashtable] -OldOffsets [hashtable] -Partitions [int]，纯函数，返回问题列表 [string[]]：新组缺哪个分区的数值位点；旧组有数值的分区上新组小于旧组。Remove-RetiredGroupTopicOffsets -CgTool -BootstrapServer -RetireGroup -Topic [-Apply]：describe 旧组，GroupMissing 或没有数值行时打印 'nothing to retire' 后返回；不加 -Apply 时打印 '[DRY-RUN] would run kafka-consumer-groups.sh --delete-offsets --group G --topic T'；加 -Apply 时执行 --bootstrap-server B --delete-offsets --group G --topic T，捕获输出并过 Assert-KafkaToolOutputClean，再 describe 一次，仍有数值行就 throw（中文：旧组位点没有删干净，多半仍有成员订阅该 topic，即 GROUP_SUBSCRIBED_TO_TOPIC）。主流程：FinalizeHandoff 模式依次 describe 两组 → Test-HandoffFinalizeReady 有问题就 throw 并列出全部 → Remove-RetiredGroupTopicOffsets；重置模式在 -Apply 成功之后，若给了 RetireGroup 就调 Remove-RetiredGroupTopicOffsets（回档是有意回退，不做 ≥ 检查），不加 -Apply 时只打印计划。.DESCRIPTION 与 .EXAMPLE 里的组名改成 db_rpc_consumer_group_z101，新增 -FinalizeHandoff 示例；:101-105 的注释更新为「K8s 切成每 zone 一组之后……」。

依赖：tools/scripts/lib/kafka_consumer_groups.ps1

### [modify] tools/scripts/tests/kafka_offset_reset_output_guard.tests.ps1
PR-C。保留现有 7 例。新增：dot-source lib；从 AST 取出 Test-HandoffFinalizeReady。新增用例见 tests。

依赖：tools/scripts/kafka_offset_reset.ps1

### [modify] tools/scripts/k8s_zone_rollback.ps1
PR-C。dot-source lib/kafka_consumer_groups.ps1。新增脚本常量 $DbConsumerGroupBase='db_rpc_consumer_group'，注释写明它是 go/db/etc/db.yaml ServerConfig.Kafka.GroupID 的镜像，由测试钉住。参数：$KafkaGroup 缺省改为 ''（推导为 "${DbConsumerGroupBase}_z$ZoneId"）；新增 [string]$KafkaLegacyGroup=''（推导为 $DbConsumerGroupBase）；删除 $KafkaDrainTimeoutSec（:218）。入口校验：两组相等就 throw（中文）。摘要行（:751）改为 kafka : bootstrap topic=… group=<zone 组> retire=<legacy 组>。.DESCRIPTION：第 2 条改写为「（已删除，2026-10-15）zone-down 之后本 zone 的 db 已停，LAG 不可能排空，Step 5 会把位点拨回 TargetTime；改为第 0 步 c 的只读快照」；第 5 条改写为「重置 zone 组并删除 legacy 组在本 topic 上的位点（回档后该 zone 一律以 zone 组为权威，legacy zone 借此迁移）」；第 6 条补一句 Step 6 显式传 -DbConsumerGroup zone。第 0 步预检块（:800-856）末尾、Write-Host '' 之前加第 0 步 c：-Apply 时调用 Show-KafkaTopicLagSnapshot -Bootstrap $KafkaBootstrap -Topic $KafkaTopic -Groups @($KafkaGroup, $KafkaLegacyGroup)；dry-run 只打印将要执行的 describe 命令。新函数 Show-KafkaTopicLagSnapshot：对每个组调 kafka-consumer-groups.sh --describe（局部 ErrorAction Continue，try/catch），用 ConvertFrom-KafkaConsumerGroupDescribe 解析后打印本 topic 的分区 / current / end / lag 和合计；GroupMissing 时打印 '(group does not exist)'；任何异常只 Write-Warning 'Kafka LAG snapshot unavailable: …'，绝不 throw；文案英文。删除 Step 2 整块（:876-902）。Step 5 的 $kArgs 增加 RetireGroup = $KafkaLegacyGroup，Group 用 zone 组。Step 6 之前在 $upArgs（:771）里加 DbConsumerGroup = 'zone'，注释写明回档后永远落 zone 组，legacy 位点已在 Step 5 删除，交接闸走快路径。不新增 kubectl 读，Get-ZoneEntryKubectlArgs 不变。

依赖：tools/scripts/lib/kafka_consumer_groups.ps1, tools/scripts/kafka_offset_reset.ps1, tools/scripts/dev_tools.ps1, tools/scripts/k8s_deploy.ps1

### [modify] tools/scripts/tests/k8s_zone_rollback_gate_router_mode.tests.ps1
PR-C。沙箱 Invoke-RollbackInSandbox（:342-419）做三处改动：把 lib/kafka_consumer_groups.ps1 也拷进沙箱的 lib 目录；假 dev_tools.ps1 的参数列表加 $DbConsumerGroup；$rollbackArgs 去掉 KafkaDrainTimeoutSec。假 kafka_offset_reset.ps1 改为把收到的具名参数（按名排序）记进 kafka_offset_reset.bound.txt，返回对象新增 KafkaResetBound。假 kafka-consumer-groups.sh 支持按用例注入输出（新参数 -KafkaCliOutput），缺省仍是无输出。新增用例见 tests。

依赖：tools/scripts/k8s_zone_rollback.ps1

### [modify] tools/scripts/dev_tools.ps1
PR-C。① 新增参数 [ValidateSet("","keep","zone","legacy")][string]$DbConsumerGroup=''，放在 GateRouterMode 那组参数附近，注释写明留空不透传、语义见 k8s_deploy.ps1 同名参数；Invoke-K8sDeploy（:560）里非空时 $args.DbConsumerGroup=$DbConsumerGroup。② :321 的 $RollbackKafkaGroup 缺省改为 ''，注释写明由回档脚本推导 db_rpc_consumer_group_z<ZoneId>；新增 [string]$RollbackKafkaLegacyGroup=''；删除 :329 的 $RollbackKafkaDrainTimeoutSec。k8s-zone-rollback 分支（:1576-1587）：KafkaGroup 与 KafkaLegacyGroup 只在非空时放进 $rbArgs，删除 KafkaDrainTimeoutSec 那一行。③ kafka-offset-reset 相关参数（:295-308）新增 [string]$KafkaRetireGroup=''、[switch]$KafkaFinalizeHandoff；分发分支（:1548-1562）非空时追加 -RetireGroup，开关打开时追加 -FinalizeHandoff。④ Get-MergeZoneArgs（:1001-1005）只在 $MergeKafkaGroup 非空时转发 -kafka-group，否则不转发，由工具按 zone 推导并认定唯一权威组；Get-MergeDbKafka 仍读 TopicGeneration（返回的 GroupID 保留，但不再转发）；同步更新 :244-245、:955-958 与 :998-1000 的注释。

依赖：tools/scripts/k8s_deploy.ps1, tools/scripts/kafka_offset_reset.ps1, tools/merge_zone/main.go

### [modify] tools/scripts/tests/dev_tools_merge_zone_contract.tests.ps1
PR-C。:99-107 的用例改名为「Kafka 世代号取 db.yaml；consumer group 缺省不转发」：断言 -kafka-topic-generation 为 '3'，-kafka-group 不出现；新增 $script:MergeKafkaGroup='custom_group' 时转发 -kafka-group custom_group。另加对 $devToolsSource 的源码断言：k8s-zone-rollback 分支不再出现 KafkaDrainTimeoutSec；KafkaLegacyGroup 只在非空时转发；kafka-offset-reset 分支转发 -RetireGroup 与 -FinalizeHandoff。

依赖：tools/scripts/dev_tools.ps1

### [modify] tools/scripts/k8s_deploy.ps1
PR-C。① 参数块（:258 附近）新增 [ValidateSet("keep","zone","legacy")][string]$DbConsumerGroup='keep'，注释用中文写清三种模式、只许 zone-up 的原因，以及 zone / legacy 不写 GroupID 是能力证明。② 参数规范化之后（:420 附近）加入口拒绝：DbConsumerGroup 不是 keep 且 Command 不是 zone-up 时 throw「-DbConsumerGroup $DbConsumerGroup 只允许与 -Command zone-up 一起用(逐 zone 切组 / 反向交接);$Command 请用默认的 keep。」③ 新增 Test-ZoneDbDeployed，条件与 Test-ZoneLoginDeployed（:1752）相同，注释写明。④ 新增纯函数 ConvertFrom-LiveDbConsumerGroup -DbYaml -LegacyGroup -ZoneGroup：用正则读 ConsumerGroup / GroupID / GroupHandoffFrom 三个键。CG 与 GID 都有 → throw；CG 非空时组名只能是 zone 组或 legacy 组，HF 只能为空或「另一个」组，否则 throw；只有 GID 时带 HF → throw（这种形状二进制会拒），GID 只能是两组之一；三个键都没有 → throw。返回 [pscustomobject]@{Key='ConsumerGroup'|'GroupID'; Group; HandoffFrom; Source='live-configmap'}；throw 文案中文。⑤ 新增纯函数 Resolve-DbConsumerGroupPlan -Mode -LegacyGroup -CurrentZoneId -IsDryRun -LiveRead [bool] -LiveDbYaml [AllowNull]：zone → @{ConsumerGroup; zone 组; HandoffFrom=legacy 组; explicit}；legacy → @{ConsumerGroup; legacy 组; HandoffFrom=zone 组; explicit}；keep 且 DryRun → zone 形状，Source='dryrun-preview'；keep 且 LiveDbYaml 为 null → zone 形状，Source='fresh-namespace'；keep 且有 LiveDbYaml → ConvertFrom-LiveDbConsumerGroup 的结果。zone 组名为 "${LegacyGroup}_z${CurrentZoneId}"。⑥ 新增 Read-LiveDbConfigMapYaml -Namespace：用 Invoke-ClientEntryKubectl 执行 get configmap $GoSvcCatalogue['db'].ConfigMap -n ns --ignore-not-found -o json，带 -CaptureOutput 与 -AllowFailure；退出码非 0 → throw（读不到不等于没有）；输出为空 → $null；有输出但没有 data[$GoSvcCatalogue['db'].ConfigFile] 键 → throw。⑦ 新增 Initialize-DbConsumerGroupPlans：只在 Test-ZoneDbDeployed 为真时执行；zone-up 处理 $ZoneName/$ZoneId，all-up 处理 zones 配置里的每个 zone；LegacyGroup 取 Get-AuthoritativeScalar go/db/etc/db.yaml ServerConfig.Kafka.GroupID；非 DryRun 且 keep 时逐个 zone 调 Read-LiveDbConfigMapYaml；结果存进 $script:DbConsumerGroupPlans[zoneId]；每个 zone 打一行 Write-Host 'DB consumer group: zone=… mode=… key=… group=… handoff_from=… source=…'；DryRun 且 keep 时 Write-Warning 'DbConsumerGroup=keep under -DryRun: cluster not read (dryrun-preview); previewing ConsumerGroup=<z> GroupHandoffFrom=<legacy>. A real run keeps the live go-svc-db-config group when present.'。在写路径预检块里紧跟 Assert-ClientEntryDeployPreflight（:6762）之后调用，保证早于任何写操作，读失败不会留下半截部署。⑧ 新增 Get-DbConsumerGroupPlan -CurrentZoneId：缓存缺失时 throw「内部错误」。新增 New-DbConsumerGroupYaml -Plan：GroupID 形状输出一行 '    GroupID: "g"'；ConsumerGroup 形状输出一行中文注释、'    ConsumerGroup: "g"'，HandoffFrom 非空时再加 '    GroupHandoffFrom: "p"'。⑨ New-GoSvcConfigMapYaml（:3652）：$SvcName -eq 'db' 时 $dbConsumerGroupYaml = New-DbConsumerGroupYaml -Plan (Get-DbConsumerGroupPlan -CurrentZoneId $CurrentZoneId)；db 模板 :4100 的 GroupID 字面量行替换为 ${dbConsumerGroupYaml}。login 模板 :4372 的 GroupID 上方加一行注释（死配置、login 不读、不是 db 的消费组、Step 7 删除），值不动。⑩ Apply-OneGoSvc（:5511）：在镜像占位替换之后、任何集群写操作之前加 if ($SvcName -eq 'db') 块，服务名判断放在最左边，与 k8s_migrate_gate 用例的 AST 抽取约定一致。块内：$plan = Get-DbConsumerGroupPlan；把 manifest 里的 PLACEHOLDER_DB_CONSUMER_GROUP 替换成 $plan.Group、PLACEHOLDER_DB_GROUP_HANDOFF_FROM 替换成 $plan.HandoffFrom（为空时写 'none'）；之后若仍匹配 PLACEHOLDER_DB_ 就 throw「内部错误」。注释写明 S1 的 RetryRouting 注解也放进同一个 db 块。

依赖：deploy/k8s/manifests/go-svc/db.yaml, go/db/etc/db.yaml

### [modify] tools/scripts/tests/k8s_deploy_contract.tests.ps1
PR-C。新增用例见 tests。基线 DryRun（keep）复用 $devOut。新增两次 DryRun 子进程：-DbConsumerGroup legacy，以及 -Command all-up -DbConsumerGroup zone（负向）。纯函数用例从 AST 取出 ConvertFrom-LiveDbConsumerGroup / Resolve-DbConsumerGroupPlan 直接调用。

依赖：tools/scripts/k8s_deploy.ps1

### [modify] deploy/k8s/manifests/go-svc/db.yaml
PR-C。Pod 模板已有的 annotations 段（:46-50）末尾追加两行中文注释（由 k8s_deploy.ps1 Apply-OneGoSvc 按本 zone 的消费组计划替换；组名或交接对方组一变，模板随之变化，只改 ConfigMap 也会触发滚动），再追加 mmorpg.io/db-consumer-group: "PLACEHOLDER_DB_CONSUMER_GROUP" 与 mmorpg.io/db-group-handoff-from: "PLACEHOLDER_DB_GROUP_HANDOFF_FROM"。:29 的 HPA 注释补一句：K8s 上每 zone 一个消费组（db_rpc_consumer_group_z{ZoneId}），再平衡只影响本 zone；切组期间对方组的再平衡见设计文档。

### [modify] tools/merge_zone/main.go
PR-C。:117-118 的注释改为：legacy 共享组名，同时是按 zone 推导组名的基名（db_rpc_consumer_group_z<zone>，与 k8s_deploy / go_services.ps1 同名）。options 新增 kafkaLegacyGroup string。:319 的 -kafka-group 缺省改为 ''，帮助文本英文：explicit go/db consumer group for the lag pre-flight; empty = derive db_rpc_consumer_group_z<zone> plus -kafka-legacy-group and use the single group that holds committed offsets on the topic。新增 flag -kafka-legacy-group，缺省 defaultKafkaGroup，帮助文本说明传空则只看 zone 组。

### [modify] tools/merge_zone/preflight.go
PR-C。文件头 P3 的说明改为「唯一权威组」口径。kafkaLagSource 改为 TotalLag(ctx context.Context, topic string, groups []string) (lag int64, group string, err error)，注释写明只对唯一权威组算 LAG，以及为什么取最小值是 fail-open。kafkaCLILagSource.TotalLag 对每个组跑一次 describe（CombinedOutput），组成 []groupDescribeResult{group, output, runErr}，交给 resolveAuthoritativeLag。新增纯函数：resolveAuthoritativeLag(topic string, results []groupDescribeResult) (int64, string, error)——consumerGroupMissing(output) 为真就当作这个组不存在；否则 runErr 非空或 committedOffsetPresent 报错都直接返回 error；恰好一个组在本 topic 上有数值位点时，用 parseConsumerGroupLag 对它的输出算 LAG（沿用对 '-' 的 fail-closed）；多于一个时报错，文本列出各组并提示先跑 kafka_offset_reset.ps1 -FinalizeHandoff；一个都没有时报错 'no candidate group holds committed offsets on topic …'。consumerGroupMissing(out string) bool 用正则 (?i)consumer group '[^']*' does not exist。committedOffsetPresent(out, topic string) (bool, error)：按表头定位 TOPIC 与 CURRENT-OFFSET；行首是 'Error:' 且不是 does-not-exist 时报错；没有表头时返回 false。dbConsumerGroupForZone(zone uint32) string 返回 defaultKafkaGroup+'_z'+zone。kafkaGroupCandidates(explicit, legacy string, zone uint32) []string：explicit 非空只返回它；否则返回 zone 组，再加上非空且与 zone 组不同的 legacy。attestedLagSource 返回 (0, '(attested)', nil)。fakeLagSource 增加 group 字段。preflightParams 的 kafkaGroup string（:225）换成 kafkaGroups []string。P3（:247-257）调 TotalLag(ctx, topic, p.kafkaGroups)，拒绝与通过的日志都带上权威组名：'preflight P3 OK: authoritative group %s lag=0 on %s [%s]'。dbTaskQueueKeys / P4 不动（S1 会改那里）。

依赖：tools/merge_zone/main.go

### [modify] tools/merge_zone/merge_run.go
PR-C。:253 改为 kafkaGroups: kafkaGroupCandidates(o.kafkaGroup, o.kafkaLegacyGroup, src)。

依赖：tools/merge_zone/preflight.go

### [modify] tools/merge_zone/unmerge.go
PR-C。:182 改为 kafkaGroups: kafkaGroupCandidates(o.kafkaGroup, o.kafkaLegacyGroup, dst)。

依赖：tools/merge_zone/preflight.go

### [modify] tools/merge_zone/merge_unit_test.go
PR-C。保留 parseConsumerGroupLag 的现有 4 例，新增用例见 tests。

依赖：tools/merge_zone/preflight.go

### [modify] tools/merge_zone/integration_test.go
PR-C。:1489 的 itPreflightParams 改为 kafkaGroups: []string{defaultKafkaGroup}；所有 fakeLagSource 字面量适配新接口（字段名不变，新增 group 字段可省略）；用例逻辑不变（build tag merge_integration）。

依赖：tools/merge_zone/preflight.go

### [modify] tools/scripts/stress_summarize.ps1
PR-C。param 新增 [int]$Zone=1；$KafkaTopic 缺省改为 ''，推导为 "db_task_zone_$Zone"；$KafkaConsumerGroup 缺省改为 ''，推导为 "db_rpc_consumer_group_z$Zone"（与 go_services.ps1 本地组名一致）。.PARAMETER 帮助同步改。Kafka 段的解析逻辑不动。

### [modify] docs/design/zone_data_rollback.md
PR-D。:182-184 的示例组名改为 db_rpc_consumer_group_z<id>，topic 改为 db_task_zone_<id>。新增「2026-10-15 修订」小节：Step 2 删除及原因；第 0 步 c 是只读快照；Step 5 重置 zone 组并 retire legacy 位点；回档之后一律以 zone 组为权威；Step 6 显式传 -DbConsumerGroup zone；:261 的表格行补一句。

依赖：tools/scripts/k8s_zone_rollback.ps1

### [modify] docs/design/zone_data_rollback_en.md
PR-D。:137 示例的组名与 topic 同上，并附一句英文说明，指向 zone_data_rollback.md 的 2026-10-15 修订。

### [modify] docs/design/zone_data_rollback_zh.md
PR-D。:133 同上，中文。

### [modify] docs/ops/merge-zone-runbook.md
PR-D。该文件有他人未提交的 1 行改动，只改下列行，不碰别处。:102 改为：消费组按 zone 推导（db_rpc_consumer_group_z{zone}），legacy 组是候选；门禁只认在本 topic 上有位点的唯一权威组，两组都有位点就拒绝，先跑 -FinalizeHandoff 收尾。:103 改为：dev_tools 不再转发组名，覆盖用 -MergeKafkaGroup。:266 的 P3 行同步。:487 的 describe 命令组名改为 db_rpc_consumer_group_z<src>。

### [modify] docs/design/server_merge_design.md
PR-D。:107 与 :359 的组名口径同上。

### [modify] docs/design/data-consistency-stress-testing.md
PR-D。:216 的 --group 改为 db_rpc_consumer_group_z<zone>（本地 go_services.ps1 一直带 _z）。

### [modify] docs/design/no-single-node-horizontal-scaling-20261001.md
PR-D。§3.4 第一条之后追加一行「2026-10-15 更新：已落码（未验证），见 db-consumer-group-per-zone.md」；:74 的浮动 tag 一条之后同样追加已钉版的更正行。只追加，不改原文。

### [modify] docs/design/db-task-pipeline-bottlenecks.md
PR-D。§5 的「③ 消费组按 zone 拆」段末追加「2026-10-15 落码说明」：三处偏离（保留 GroupHandoffFrom、运行时 R7、必填键证明、回档永远落 zone 组）及其理由，指向新设计文档。§6.3 第 0 批里的「Kafka 镜像钉版本（未做）」改为已做，附 digest。§8 的两条 CLI 未核实项改为「待 Codex 在 4.3.1 上核实（V3）」。

### [modify] docs/PROGRESS.md
PR-D。末尾追加一条（只追加，不改旧条目）：S2 ③ 加 Kafka 钉版加回档 Step 2；授权出处（2026-10-13 负责人拍板，③ 是开服门槛）；4 个 PR 的文件清单；三处偏离附录及其理由；未编译，待 Codex 验证；Java 版不适用（无 Kafka 存盘链），写明 PARITY.md 登记行与 Java 版本号。

### [modify] D:/luyuan/xuanming-server-mmo/PARITY.md
PR-D（Java 仓库）。先 grep「存盘链」定位到 mmorpg 走 Kafka 存盘链的那一行附近，登记一行：「db_task 消费组按 zone 拆分 + 交接闸 + 回档落 zone 组 | <本仓库 commit> | — | 不适用（Java 无 Kafka 存盘链；官方 kafka-clients 如需可直接配 cooperative-sticky / group.instance.id）」。

## 接口（类型 / 配置键 / 指标 / Redis 键 / 组名）
【go/db 配置键（go/db/internal/config KafkaConfig；YAML 路径 ServerConfig.Kafka.*）】GroupID string 改为可选，是旧形状；ConsumerGroup string 可选，是新形状，与 GroupID 二选一；GroupHandoffFrom string 可选，缺省为空，非空时必须配 ConsumerGroup 且不能有 GroupID；SessionTimeoutMs 缺省 10000；HeartbeatIntervalMs 缺省 3000；RebalanceTimeoutMs 缺省 60000。约束：heartbeat×3 ≤ session，rebalance ≥ 15000。方法 EffectiveGroupID() 与 ValidateConsumerGroup() error。
【ConfigMap 形状（K8s go-svc-db-config 的 db.yaml）】legacy 原样（keep 读到旧 ConfigMap 时保持）：GroupID 为 db_rpc_consumer_group，没有交接键。zone：ConsumerGroup 为 db_rpc_consumer_group_z{ZoneId}，GroupHandoffFrom 为 db_rpc_consumer_group。legacy 反向：ConsumerGroup 为 db_rpc_consumer_group，GroupHandoffFrom 为 db_rpc_consumer_group_z{ZoneId}。超时键不渲染，取默认值。
【Pod 模板注解（db Deployment）】mmorpg.io/db-consumer-group 取有效组名；mmorpg.io/db-group-handoff-from 取对方组或 none。manifest 里的占位是 PLACEHOLDER_DB_CONSUMER_GROUP 与 PLACEHOLDER_DB_GROUP_HANDOFF_FROM，由 Apply-OneGoSvc 的 db 块替换。
【sarama 客户端】ClientID 为 db-z{zone}-{host}，inspector 的 ClientID 再加 -handoff 后缀；Version 统一用 consumerKafkaVersion，即 V3_5_0_0；Offsets.Initial 仍为 OffsetOldest。
【go/db 内部接口（kafka 包）】groupInspector{DescribeGroup(group, topic) (peerGroupView, error); CommittedOffsets(group, topic, partitions) (map[int32]int64, error); Close() error}。groupHandoff 的方法：await(ctx) error；evaluate() gateDecision；seed(session, claimed []int32) error；watch(ctx, done)；watchOnce()；setSessionYield(cancel) bool；clearSessionYield()；Close() error。构造：newGroupHandoffFromConfig(kcfg, group, newInspector) 在没有对方组时返回 (nil, nil)。consumerGroupHandler.sessionScope(session) (ctx, <-chan struct{})，以及 yielded()。consumerClientID(zone, host)。rebalanceGapTracker。
【给 S1（④）的挂点】ConsumeClaim 里的 claimCtx 由 sessionScope 派生（让出时随之取消），S1 的 claimLease 就用这个 claimCtx 登记；S1 的 inflight 等待放进 drainAfterStop，让出分支自动继承。Setup 的顺序：分区契约 → handoff.seed → 建 yieldCtx / setSessionYield → 记 gap → S1 的挂点（如果有）。Cleanup 的顺序：S1 的挂点 → 取消 yield / clearSessionYield → 记 sessionEnded。Stop 里 closeHandoff 必须在 consumer.Close() 之后。k8s_deploy 的 Apply-OneGoSvc 里 S2 新增的 if ($SvcName -eq 'db') 块是 db 专属注解替换的唯一位置，S1 的 RetryRouting 注解（如需要）用同样的占位手法放进这个块。
【指标（subsystem db，都不带 player_id）】db_consumer_group_rebalance_gap_seconds（histogram）；db_consumer_group_handoff_total{result=seeded|own_present|no_offsets|aborted}；db_consumer_group_handoff_gate_total{outcome=no_peer_offsets|peer_released|peer_unreachable_own_complete|wait_peer_holding|wait_peer_rebalancing|wait_own_behind_peer|wait_peer_unavailable|wait_own_unavailable}；db_consumer_group_handoff_waiting_seconds（gauge）；db_consumer_group_handoff_wait_seconds（histogram）；db_consumer_group_handoff_conflict_total。告警组 db-task.consumer-group-handoff（4 条）。
【Kafka 组名】按 zone 的组是 db_rpc_consumer_group_z{ZoneId}，与本地 go_services.ps1 同名；legacy 组是 db_rpc_consumer_group。不新增 Redis 键，不改任何 Kafka topic 或 key。
【脚本参数】k8s_deploy.ps1 -DbConsumerGroup keep|zone|legacy，缺省 keep，zone / legacy 只许配 zone-up。dev_tools.ps1 新增 -DbConsumerGroup ''|keep|zone|legacy、-RollbackKafkaLegacyGroup、-KafkaRetireGroup、-KafkaFinalizeHandoff；-RollbackKafkaGroup 缺省改为 ''；删除 -RollbackKafkaDrainTimeoutSec。k8s_zone_rollback.ps1 的 -KafkaGroup 缺省改为 ''（推导 zone 组）；新增 -KafkaLegacyGroup ''；删除 -KafkaDrainTimeoutSec。kafka_offset_reset.ps1 新增 -RetireGroup <g> 与 -FinalizeHandoff（需要 -Group -RetireGroup -Topic -Partitions）。merge_zone 的 -kafka-group 缺省改为 ''；新增 -kafka-legacy-group，缺省 db_rpc_consumer_group。stress_summarize.ps1 新增 -Zone 1，并据此推导 topic 与 group。
【PowerShell 库】tools/scripts/lib/kafka_consumer_groups.ps1：ConvertFrom-KafkaConsumerGroupDescribe -Lines -Topic 返回 {GroupMissing, HeaderFound, Rows}；Get-KafkaCommittedOffsetMap。
【Kafka 镜像】apache/kafka:4.3.1@sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837，compose 两处、kafka.yaml、kafka-topic-init.yaml 四处相同。

## 顺序与重叠
【本项内部顺序】PR-A（钉版）→ PR-B（go/db Phase A、login optional、告警、设计文档）→ Codex 验证 → 发布 Phase A 镜像 → PR-C（脚本工具）→ 金丝雀 → PR-D（文档）。PR-B 必须先于 PR-C 发布：PR-C 之后，全新 namespace 会渲染 zone 形状，镜像早于 Phase A 时 db 会拒启（这是有意的 fail-closed）。PR-A 与 PR-B 可以并行开发，但 PR-A 先合入，Codex 的 F9 核对在 4.3.1 上做。
【不影响条件写会话】本项不改 markAppliedSeq / markAppliedOwnerEpoch 的调用顺序和位置，也不改 handleTask / processTaskBatch / runDBOp / dbOpHandler 的签名（仍是 func(ctx, store, task, msg) error）。ConsumeClaim 只改 claimCtx 的父 ctx，并加一个让出分支。条件写会话把「游标 SET 与 applied_epoch 推进」合成原子脚本时与本项无冲突。
【与 S1（④ 重试只交给分区属主 + WAITAOF）重叠，按文件】go/db/internal/kafka/key_ordered_consumer.go（结构体、NewKeyOrderedKafkaConsumer、ConsumeClaim、Setup / Cleanup、Stop——S1 改 deferFailedTask / 重试消费 / handleTask，S2 不碰那几段）；key_ordered_consumer_test.go（S1 给 newTestWorker 加字段，S2 改 recordingSession.MarkOffset）；config.go（S1 加 RetryRouting，S2 加组与超时键，都在 KafkaConfig 里，合并时都保留）；metrics.go（两边各追加指标与 MustRegister 项）；go/db/etc/db.yaml；deploy/k8s/db-task-retry-alerts.yaml（各加一个 group）；tools/scripts/k8s_deploy.ps1（db ConfigMap 的 Kafka 段：S2 换掉 GroupID 行，S1 加 RetryRouting 行；db 注解块共用，见 interfaces）；tools/scripts/tests/k8s_deploy_contract.tests.ps1；deploy/k8s/manifests/go-svc/db.yaml（S1 改 replicas 注释，S2 加注解）；tools/merge_zone/preflight.go（S2 改 P3 与 lag 接口，S1 改 dbTaskQueueKeys 与 P4，函数互不相交）。推荐先后：S2 的 PR-B 先合入（S1 批 1 还有前置门禁：双 db 实例那几项要先跑通；附录 ③ 也建议先拆组，以缩小 ④ 滚动时的影响面），S1 在其上 rebase，按 interfaces 的挂点接入。两者都改 Setup 时，按 interfaces 里约定的顺序合并。
【与 S3（① 读写分区对齐）】go/login/internal/config/config.go（S2 只改 GroupID 一行的 tag 与注释）；k8s_deploy.ps1 的 login 模板（S2 只在 GroupID 上方加一行注释）；go/login/etc/login.yaml 本轮 S2 不动，留给 Step 7。
【与 S4（G1 治标、停机顺序、go_services 优雅停、测量打点）】key_ordered_consumer.go 的 Stop()：S4 改成「先撤拉取再等 worker」，必须保留 S2 的 closeHandoff 在 consumer.Close() 之后；metrics.go 两边各自追加；stress_summarize.ps1（S2 只改参数块的缺省与推导，S4 改 DB 段输出）；deploy/k8s/manifests/go-svc/db.yaml（S4 加 minReadySeconds，S2 加注解，行不重叠）。S2 不碰 go_services.ps1。
【与 S5（retention.bytes 与 PVC）】deploy/k8s/manifests/infra/kafka.yaml 与 deploy/docker-compose.yml：S2 只改 image 行并加 imagePullPolicy；S5 改保留期环境变量、PVC 或 topic 配置时，行不重叠。
【其它会话】docs/ops/merge-zone-runbook.md 工作区有他人未提交的 1 行改动，PR-D 只改 :102/:103/:266/:487。k8s_zone_rollback.ps1：最后一次实质改动是 d5cbb8a8e（D1 第 0 批 FLUSHDB 防护），e4ff1d165 只是等行数的日志英文化，工作区没有在途改动，launch-readiness 会话目前没在改；条件写会话的 B1（只清 Redis 时围栏归零）将来可能改 Step 4，与本项改的第 0 步 c、Step 2、Step 5、Step 6 不重叠。docs/design/no-single-node-horizontal-scaling-20261001.md 只追加。全局单点会话（handoff-global-singletons）的 P1「db 消费组全 zone 共用」由本项落地，该会话不要再按组名直接改。

## 测试
### go/db/internal/kafka/group_handoff_test.go
TestGroupHandoffGate_BlocksWhilePeerHoldsTopicThenReleases（T1a）：对方 Stable 且 Held={0,1} 时连续等待；对方释放（Held 为空）后 await 返回 nil，fake sleep 的调用次数正确。TestGroupHandoffSeed_MarksPeerCommittedWithoutPlusOne（T1b）：own=-1、peer=100 时 MarkOffset(topic,p,100,'') 只调一次，结果计 seeded。TestGroupHandoffGate_WaitsWhilePeerRebalancingWithOffsets（T2）：PreparingRebalance、CompletingRebalance 下 Held 为空、对方有位点时等待。TestGroupHandoffSeed_KeepsOwnOffsets（T3）：own=50 时不调 MarkOffset，计 own_present，且不查询对方。TestGroupHandoffGate_RefusesWhenPeerAheadOfOwn（R7，B2）：own{0:10}、peer{0:20}、对方 Stable 且 Held 为空时等待，outcome 为 wait_own_behind_peer；反过来 own{0:20}、peer{0:10} 时放行。TestGroupHandoffSeed_NoOffsetsAnywhere（T4）：两边都是 -1 时不调 MarkOffset，计 no_offsets。TestHeldPartitions_OnlyCountsThisTopic（T5）：分配里只有别的 topic 时 Held 为空；有本 topic {3,4} 时 Held={3,4}；字节损坏时返回 error。TestGroupHandoffGate_AdminErrors（T6，F3）：对方查询出错且本组不完整时等待 wait_peer_unavailable；对方出错但本组完整时放行 peer_unreachable_own_complete；本组查询出错时等待 wait_own_unavailable。TestGroupHandoffAwait_ReturnsOnContextCancel：ctx 取消后返回 ctx.Err()，不调 MarkOffset。TestGroupHandoffGate_FastPathWhenPeerHasNoOffsets（F3）：对方处于 PreparingRebalance 但全部为 -1 且 Held 为空时放行 no_peer_offsets。TestGroupHandoffAwait_OnlyOncePerProcess（F3）：放行之后再次 await 不调用 inspector。TestGroupHandoffWatch_YieldsOnlyWhenPeerHoldsAndAdvances（T7，F1）：第 1 拍建基线；第 2 拍 Stable、Held 非空、位点不变 → 不让出；第 3 拍位点前进 → 调用 sessionYield、released=false、conflict 加 1。TestGroupHandoffWatch_DoesNotYieldToGatedPeer（F1，B1）：对方 Empty 或 Held 为空、哪怕位点前进也不让出；admin 出错时不让出；两侧都配 GroupHandoffFrom 时模拟正向加反向，断言双方都能放行、不互等。TestGroupHandoffSetSessionYield_CancelsWhenAlreadyYielded：守望先让出、Setup 后登记时返回 false。TestNewGroupHandoffFromConfig_NoInspectorWithoutPeer（T8）：GroupHandoffFrom 为空时不调用工厂函数，返回 nil。TestNewGroupHandoff_RejectsPeerEqualGroup（T9）。TestRebalanceGapTracker（T10）：没有 ended 时 started 返回 false；ended 在 t、started 在 t+3s 时返回 3s。TestConsumerClientID_Sanitizes：'db-z7-pod-a'；非法字符被替换；host 为空时为 'db-z7'；超长被截断。TestConsumerSetup_SeedsClaimedPartitionsAndArmsYield：带 handoff 与 fake inspector 的 Setup 只对 claims 里的分区记 MarkOffset；之后守望让出时，handler 的 yieldCtx 被取消。TestConsumerSetup_SeedFailureFailsSession：own=-1 且对方出错时 Setup 返回 error、不调 MarkOffset，随后 Cleanup 不 panic。TestConsumeClaim_YieldReturnsNilAndStopsDispatch：先取消 yield，再用空消息通道调 ConsumeClaim，应及时返回 nil，worker.taskCh 为空。TestConsumerSetup_NoHandoffKeepsLegacyBehavior：handoff 为 nil 时不调 MarkOffset，sessionScope 退回 session ctx。

### go/db/internal/kafka/key_ordered_consumer_test.go
新增 TestReplayAfterSeed_WritesAreOrderingStale（附录要求的回归）：同一分区先应用到 offset S，再从 S−k 重新投递，写全部判为 orderingStale，游标值不变，不写缓存（复用 attachKafkaClaim 与 miniredis 现有设施）。回归：TestProcessTaskBatch_*、TestOrderingGuard_*、TestClaimAcker_LowerHoleBlocksLaterSuccess、TestConsumerSetupRejectsPartitionsOutsideImmutableContract、TestHandleTask_CanceledClaimCannotReachDB 必须保持绿。

### go/db/internal/config/config_test.go
TestKafkaTimeoutDefaults：Normalize 回填 10000/3000/60000，显式值保留。TestValidateConsumerGroup（表驱动）：只有 GroupID 时通过；ConsumerGroup 加 GroupHandoffFrom 时通过；GroupID 与 ConsumerGroup 同时出现时报错；两者都空时报错；GroupID 加 GroupHandoffFrom 时报错；GroupHandoffFrom 等于组名时报错；heartbeat 4000、session 10000 时报错；rebalance 14999 时报错。TestEffectiveGroupID。TestPreHandoffBinaryRefusesZoneShapedConfig（F5 / F9）：在测试里定义与 HEAD 旧版同形的结构（GroupID 为必填的 json:"GroupID"，加上 Brokers / PartitionCnt / IsOfflineExpand），用 conf.LoadFromYamlBytes 加载 zone 形状的 YAML，必须报错且错误文本含 GroupID；同一份 YAML 装进当前 KafkaConfig 的 holder 能加载成功，ConsumerGroup 与 GroupHandoffFrom 取值正确。TestPreHandoffBinaryIgnoresUnknownKeys：旧形状 YAML 多带 SessionTimeoutMs 等未知键时，旧结构仍能加载（记录 go-zero 忽略未知键这一事实）。

### go/db/internal/metrics/metrics_test.go
TestConsumerGroupHandoffSeriesPrecreated：register() 之后从默认 registry Gather，db_consumer_group_handoff_conflict_total=0，db_consumer_group_handoff_gate_total{outcome=wait_own_behind_peer}=0，handoff_total 的 4 个 result 序列都存在（告警依赖从 0 建出的序列）。

### tools/merge_zone/merge_unit_test.go
TestResolveAuthoritativeLag_PicksOnlyGroupWithOffsets：zone 组有行、legacy 组 'does not exist' → 返回 zone 组及其 LAG。TestResolveAuthoritativeLag_InactiveGroupHigherIsRefused（F7 指定）：两组都有数值位点、legacy 更高 → 报错，且提示 FinalizeHandoff。TestResolveAuthoritativeLag_BothEqualIsRefused。TestResolveAuthoritativeLag_NoGroupHasOffsetsIsRefused（不存在，或只有 '-' 行）。TestResolveAuthoritativeLag_CLIFailureIsRefused：runErr 加上非 does-not-exist 文本 → 报错。TestResolveAuthoritativeLag_DashLagOnAuthoritativeGroupIsRefused。TestCommittedOffsetPresent_ColumnOrderFromHeader。TestKafkaGroupCandidates：显式覆盖时只返回它；legacy 传空时只有 zone 组；legacy 与 zone 组相同时去重；dbConsumerGroupForZone(901)=='db_rpc_consumer_group_z901'。现有 4 个 parseConsumerGroupLag 用例保持绿。

### tools/merge_zone/integration_test.go
只适配新接口（kafkaGroups / fakeLagSource），既有 TestIT_Preflight_* 的语义与预期不变（build tag merge_integration，需要真实 MySQL / Redis，由 Codex 按需跑）。

### tools/scripts/tests/k8s_deploy_contract.tests.ps1
基线 DryRun（keep）：go-svc-db-config 有 ConsumerGroup=db_rpc_consumer_group_z101、GroupHandoffFrom=db_rpc_consumer_group，没有 GroupID 键；输出包含 'DbConsumerGroup=keep' 与 'dryrun-preview'（只断不含空格的片段，防 Write-Warning 折行）；db Deployment 有 mmorpg.io/db-consumer-group: "db_rpc_consumer_group_z101" 与 mmorpg.io/db-group-handoff-from: "db_rpc_consumer_group"，且没有 PLACEHOLDER_ 残留。跨文件：ConsumerGroup 的基名等于 go/db/etc/db.yaml 的 ServerConfig.Kafka.GroupID 加 _z101。-DbConsumerGroup legacy 的 DryRun：ConsumerGroup=db_rpc_consumer_group，GroupHandoffFrom=db_rpc_consumer_group_z101。负向：-Command all-up -DbConsumerGroup zone 时退出码非 0，并断言中文错误文本「只允许与 -Command zone-up 一起用」。纯函数（AST 抽取）：ConvertFrom-LiveDbConsumerGroup 的五种形状（旧 GroupID、zone 形状、反向形状、GroupID 加 Handoff 会 throw、未知组名会 throw、空会 throw）；Resolve-DbConsumerGroupPlan 在 keep 加 LiveDbYaml=null 时得到 fresh-namespace 的 zone 形状，在 keep 加 DryRun 时得到 dryrun-preview。login ConfigMap 仍有 GroupID（死配置不变）。原有用例全部保持绿（db 指标注解那一条，按行首锚定的断言与新注解共存）。

### tools/scripts/tests/k8s_migrate_gate.tests.ps1
不改文件，作为回归跑：Apply-OneGoSvc 新增的 db 块不影响 trade / friend / data-service 的抽取用例。

### tools/scripts/tests/k8s_gate_drain.tests.ps1
不改文件，作为回归跑：login 模板多一行注释后仍能解析出同一组定位参数。

### tools/scripts/tests/k8s_zone_rollback_gate_router_mode.tests.ps1
新增：-Apply 全程通过的用例里，Step 6 的 DevToolsBound 含 DbConsumerGroup=zone，第 0 步静态预检的 dry-run 调用同样含有；KafkaResetBound 含 Group=db_rpc_consumer_group_z101、RetireGroup=db_rpc_consumer_group、ToDatetime=…。输出里不再有 'Step 2: drain' 和 'waiting for LAG'。假 kafka-consumer-groups.sh 返回本 topic LAG>0 的行时，脚本仍走到 Step 6、不 throw，并打印快照合计。快照只统计本 topic：注入两个 topic 的行，断言合计只含本 topic。-KafkaGroup 显式等于 db_rpc_consumer_group 时，入口就 throw，且 DevToolsCalls 为 0。$DbConsumerGroupBase（AST 取值）等于仓库 go/db/etc/db.yaml 的 GroupID。原有「预检只读一次集群」等全部用例保持绿（本项不新增 kubectl 调用）。

### tools/scripts/tests/k8s_zone_rollback_flush_guard.tests.ps1
不改文件，作为回归跑（子进程 dry-run 走真脚本，会 dot-source 新的 lib）。

### tools/scripts/tests/kafka_offset_reset_output_guard.tests.ps1
新增：ConvertFrom-KafkaConsumerGroupDescribe 按表头解析、列顺序打乱也能识别、只取本 topic、'-' 记为 null、识别 "Error: Consumer group 'x' does not exist." 为 GroupMissing、'has no active members' 那一行不影响解析。Test-HandoffFinalizeReady：新组缺分区 → 有问题；新组某分区小于旧组 → 有问题；新组都不小于旧组、且旧组缺行的分区不检查 → 通过。源码断言：-FinalizeHandoff 与其它模式互斥，缺 -RetireGroup 或 -Partitions 时 throw；删除之后会再 describe 复核。

### tools/scripts/tests/dev_tools_merge_zone_contract.tests.ps1
-kafka-group 缺省不转发、显式时转发；-kafka-topic-generation 仍取 db.yaml；回档分支不再转发 KafkaDrainTimeoutSec；kafka-offset-reset 分支转发 -RetireGroup 与 -FinalizeHandoff。

### tools/scripts/test_stress_summarize.ps1
不改文件，作为回归跑（Kafka 段被跳过，只验证新增 -Zone 参数不破坏解析）。

## 上线与回滚
【给 Codex 的验证（Claude 不跑；全部在 D:\luyuan\xuanming-server-mmo-cpp 下执行，Go 1.27.2，pwsh 7）】
V1 go/db：cd go/db；go vet ./...；go build ./...；go test ./internal/kafka/... ./internal/config/... ./internal/metrics/... -count=1；再跑 go test ./internal/kafka/ -run 'GroupHandoff|HeldPartitions|ConsumerSetup|ConsumeClaim|RebalanceGap|ConsumerClientID|ReplayAfterSeed|ClaimAcker|OrderingGuard|ProcessTaskBatch' -count=1 -v（Linux / WSL 加 -race）。通过标准：全部 PASS，-v 输出里 T1–T10 对应用例都出现。失败时保留 -v 全文与编译错误前 20 行。
V2 其它 Go 模块：cd go/login；go vet ./internal/config/；go build ./...；go test ./internal/... -count=1。cd tools/merge_zone；go vet ./...；go test ./... -count=1 -v -run 'Lag|Authoritative|KafkaGroup|CommittedOffset|DbTaskTopic'；再整包跑 go test ./... -count=1。
V3 Kafka 钉版（docker pull 属于下载，先征得负责人同意，见 decisions 第 2 条）：docker buildx imagetools inspect apache/kafka:4.3.1 与 apache/kafka:latest，两条命令的 Digest 行都必须等于 sha256:77e3df90…6532837；docker image inspect <钉版引用> --format '{{json .Config.Entrypoint}} {{json .Config.Cmd}}' 必须是 [\"/__cacert_entrypoint.sh\"] 与 [\"/etc/kafka/docker/run\"]；docker compose -f deploy/docker-compose.yml up -d kafka kafka-topic-init，kafka-topic-init 以 0 退出；docker exec kafka /opt/kafka/bin/kafka-topics.sh --version 显示 4.3.1。F9 的 CLI 行为逐条记录原始输出与退出码：(a) 对 Stable 组执行 --reset-offsets --execute；(b) describe 一个不存在的组；(c) 对不存在的组执行 --reset-offsets --to-datetime --execute 能否建出位点；(d) 组 Stable、但没有成员订阅该 topic 时执行 --delete-offsets；(e) 有成员订阅时的输出（预期含 GROUP_SUBSCRIBED_TO_TOPIC）。结果写回设计文档 §11。
V4 PowerShell：逐个执行 pwsh -NoProfile -NonInteractive -File tools/scripts/tests/{k8s_deploy_contract, k8s_migrate_gate, k8s_gate_drain, k8s_client_entry_contract, dev_tools_merge_zone_contract, k8s_zone_rollback_gate_router_mode, k8s_zone_rollback_flush_guard, kafka_offset_reset_output_guard}.tests.ps1，以及 pwsh -File tools/scripts/test_stress_summarize.ps1。每个都要以 0 退出、没有 FAIL 行。另跑 python tools/scripts/gen_docs_index.py --check。
V5 本地交接集成（功能验证，不出性能结论）：前置按 AGENTS §6.2 清 redis / mysql / etcd，停掉 go 服务，执行 dev_tools kafka-offset-reset。(a) compose 起 kafka redis mysql etcd；(b) cd go/db; go run ./cmd/migrate -f etc/db.yaml -command up；(c) 在 scratchpad 里从 etc/db.yaml 生成四份配置：L1 / L2 用 GroupID db_rpc_consumer_group，端口 6000/6001，指标 :9160/:9161，RetryInstanceId 分别为 L1 / L2；Z1 / Z2 删掉 GroupID 行，改写 ConsumerGroup db_rpc_consumer_group_z1 和 GroupHandoffFrom db_rpc_consumer_group，端口 6002/6003，指标 :9162/:9163，RetryInstanceId 分别为 Z1 / Z2；四份都是 ZoneId 1、AllowlistEnforcement warn；(d) 启动 L1、L2，后台跑 data_stress（-players 2000 -writes 200 -concurrency 64 -burst-sleep-ms 5）；(e) 30s 后启动 Z1、Z2，确认日志出现 'group handoff waiting … reason=wait_peer_holding'；(f) 依次 Ctrl+C 停掉 L1、L2；(g) data_stress 结束后，对两个组各 describe 一次，再跑 verifier。通过标准：verifier 退出码为 0；Z 侧每个分区 'seeded … offset=N' 的 N 等于 L 停掉后 legacy 组该分区的 CURRENT-OFFSET；Z 侧第一条 ConsumeClaim 的时间晚于最后一个 L 的 'consumer group closed'；:9162 与 :9163 的 handoff_total{result=seeded} 合计为 10、conflict_total 为 0；zone 组 LAG 为 0；死信长度不增加。(h) 收尾：用 docker exec 跑 kafka-consumer-groups.sh --delete-offsets --group db_rpc_consumer_group --topic db_task_zone_1，再 describe 复核（Windows 宿主没有 CLI 时，-FinalizeHandoff 的判定改由 V4 的单测覆盖）。(i) 反向：起 legacy 反向配置 R1 / R2（ConsumerGroup db_rpc_consumer_group，GroupHandoffFrom db_rpc_consumer_group_z1），停掉 Z1 / Z2，按同样判据检查，并断言 topic 的最长消费空窗 <15s；收尾删除 zone 组位点，再正向切一次，每次 seeded 合计都是 10。(j) 守望：切组之后起一个旧形状 L3（GroupID legacy、无交接），断言 conflict_total 增加且 Z 侧让出；停掉 L3 之后 Z 侧从自身位点续消费，verifier 通过。(k) 必填键证明：用 HEAD 之前的旧 db 二进制（或 git worktree 里 73b852f32 的构建）配 Z1 配置启动，必须以「field … GroupID is not set」退出。保留：所有实例的 stdout、每次 describe 的输出、verifier 输出。
V6 kind 演练（只在 dev 集群，可选）：all-up 两个 zone 并完成 Phase A；robot 持续负载期间执行 k8s_deploy.ps1 -Command zone-up -ZoneName z1 -GoSvcTag <与线上相同> -DbConsumerGroup zone；z1 切组与删 Pod 期间，z2 的 db 不应出现 rebalance_gap 样本；收尾后执行 kafka_offset_reset.ps1 -FinalizeHandoff。
Claude 交付说明必须写明「未编译，待 Codex 验证」。

【上线（不停服，逐 zone）】
步骤 0：合入 PR-A。下一次 infra-up 会因为镜像字符串变化滚动重启 kafka StatefulSet：单 broker 档等同一次 broker 重启，在无玩家窗口执行，之后按 README 重跑 infra-kafka-topics。多 broker 档按 PDB 逐个滚动。
步骤 1（Phase A）：发布 PR-B 镜像，生成器不变（仍是 legacy GroupID 形状），各 zone 照常 zone-up。观察 rebalance_gap 基线（任一 zone 滚动时，所有 zone 都会出现样本，这正是共享组耦合的实测证据），describe 的 CLIENT-ID 列出现 db-z{zone}-*。通过条件：除新增指标外没有任何差异。回滚：用旧 tag 重跑。
步骤 2：合入 PR-C。各 zone 下一次部署时 keep 读到线上 legacy，组不变；但 db Pod 模板新增注解，会触发一次 db 滚动，与普通发布同性质。PR-C 合入之后，全新 namespace 默认就是 zone 形状，必须使用 Phase A 及以后的镜像。
步骤 3（金丝雀）：选人数最少的 zone；冻结其它 zone 的发布，确认 legacy 组处于 Stable；执行 k8s_deploy.ps1 -Command zone-up -ZoneName <z> -DbConsumerGroup zone -GoSvcTag <与线上相同>（runbook 规定切组只做「只改配置」的发布）。观察日志依次出现 handoff waiting → released → seeded ×10（跨副本合计），handoff_total{seeded}=10，conflict 为 0，本 zone 的 gap <15s，播种值等于切换前 legacy 组的 CURRENT-OFFSET。然后立刻收尾：kafka_offset_reset.ps1 -BootstrapServer <b> -Topic db_task_zone_<id> -Group db_rpc_consumer_group_z<id> -RetireGroup db_rpc_consumer_group -Partitions 10 -FinalizeHandoff，先 dry-run，再加 -Apply；复核 legacy 组已没有本 topic 的行。
步骤 4：金丝雀浸泡 24h。生产环境只观察指标、经历一次例行发布；delete pod 演练放到 staging 或 kind 做。
步骤 5：其余 zone 每天切 1–2 个，每个 zone 重复步骤 3（含收尾）。全部切完后 legacy 组变为 Empty，位点按保留期过期。
步骤 6（以后另立项）：按 AGENTS §6 跑压测出对比表之后，再调 HeartbeatIntervalMs=1000、RebalanceTimeoutMs=25000，没有对比表不调。
步骤 7（下一轮）：全部 login Pod 跑上 optional 版本 ≥7 天后，删除三处 login GroupID；merge_zone 的 -kafka-legacy-group 缺省改为空。
【回滚】单个 zone 用同一镜像执行 -DbConsumerGroup legacy 做反向交接，再收尾（-Group db_rpc_consumer_group -RetireGroup db_rpc_consumer_group_z<id>），不影响已切完的 zone。切组之后要回退到 Phase A 之前的镜像，只能走设计文档 §8 的手工兜底，并且禁止用旧发布物重新部署 db。

## 待决项（负责人已按推荐默认定）
- 本项合计约 44 个本仓库文件加 1 个 Java 仓库文件，超过授权时估计的 28–33 个，也超过 AGENTS §10.2 的 30 文件线。是否按 4 个 PR（A 钉版 5 个、B go/db 13 个、C 脚本工具 17 个、D 文档 9 个加 Java 1 个，每个都少于 30）在现有授权下继续？
  - 采用：按 4 个 PR 拆分并继续，视为在 2026-10-13「③ 授权」的范围内；PR-D 里的 server_merge_design.md、data-consistency-stress-testing.md 可以延后，以压缩数量。
- Kafka 镜像是只钉 tag（4.3.1），还是钉 tag@digest？Codex 核对时需要拉 apache/kafka:4.3.1，这属于下载，是否同意？
  - 采用：钉 apache/kafka:4.3.1@sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837（与仓库基础镜像钉 digest 的惯例一致）；同意 Codex 先用 imagetools inspect 只读核对 digest，再拉这一个镜像做 F9 的 CLI 核对。
- 是否接受对附录复核修正的三处偏离：F3 不再滚一次去掉 GroupHandoffFrom（改为只在入组前闸一次，本组位点完整时不依赖对方）；F4 不在部署期查 Kafka 判组（改为二进制运行时的 R7 加 fresh-namespace 渲染 zone 形状）；F5 不用 OCI label（改为 zone / legacy 形状不写 GroupID 的必填键证明）。
  - 采用：接受。三处都写进设计文档 §10，交给下一轮对抗复核。

## 风险
1) 发布顺序：PR-C 合入之后，全新 namespace 与回档 Step 6 都渲染 zone 形状。如果 go-svc 镜像早于 Phase A，db 会以「field … GroupID is not set」进入 CrashLoop。这是有意的 fail-closed，但会停该 zone 的存盘消费，所以 PR-B 镜像必须先发。2) PR-C 之后各 zone 的第一次部署会因为新增 Pod 注解滚一次 db；PR-A 的 kafka.yaml 改动会滚 kafka StatefulSet，单 broker 档在窗口期等同 broker 重启。3) 切组空窗取决于 legacy 组多久能稳定：其它 zone 正在滚动时，legacy 组会反复再平衡，最坏可达 Rebalance.Timeout 60s，超过 login 的 TaskWaitTimeout 30s，所以切组时必须冻结其它 zone 的发布。告警阈值 45s / 300s，等待接近 24h 保留期时未消费的存盘会被删除。4) R7 会在两组都有位点、且对方更大时停住该 zone 的消费（例如漏了收尾、或 PITR 之后没删对方位点），必须人工收尾，靠 OwnBehindPeer 告警发现。5) peer_unreachable_own_complete 放行时会跳过 R7 和重叠检测，要等对方可达后由守望补救。守望只凭正面证据让出，看不见的重叠窗口约为 5s 加一轮再平衡；兜底仍是按 topic 的游标、排序锁、owner_epoch（F8，其中游标缺键时会放行旧写）。6) 收尾之后，如果有人用旧发布物（旧生成器加旧镜像）把 db 部署回 legacy 组，它会从 Oldest 重放整个保留期：写由游标吸收（前提是游标键还在），读全部重算，负载很高，runbook 必须禁止。7) Kafka CLI 的文本与退出码（does not exist、delete-offsets 的输出、GROUP_SUBSCRIBED_TO_TOPIC）在 4.3.1 上没有实测，脚本按「出现 Error: 就失败、删完再 describe 复核」做 fail-closed；sarama DescribeGroups 在新 coordinator 上对不存在组的 State / Err 映射也没有实测，已按 Dead 与 GROUP_ID_NOT_FOUND 两种都处理。8) 回档的行为变化：legacy zone 回档之后会自动迁到 zone 组。Step 5 删除 legacy 位点时，如果还有成员订阅本 topic（namespace 没删干净），脚本会在 Step 4 之后 throw；此时 zone 已停、数据已回档，需要手工重跑 Step 5；遗漏时 R7 会拦住，不会重放。9) merge_zone 现在要求合服前先收尾，两组都有位点就拒绝，多了一个运维步骤。10) 每个 db Pod 每 5s 对 legacy 组发一次 DescribeGroups 加 OffsetFetch，开销可以忽略；将来开 Kafka ACL 时，缺 legacy 组的 Describe 权限会让闸 fail-closed（本组位点完整时不受影响）。11) 与 S1 / S4 / S5 在多个文件上并行修改，合并时按 ordering_and_overlap 里的挂点处理，否则可能覆盖对方的改动。12) 以上全部是静态阅读得出的结论，没有编译、测试或压测；暂停时长与 rebalance_gap 都没有实测基线。
