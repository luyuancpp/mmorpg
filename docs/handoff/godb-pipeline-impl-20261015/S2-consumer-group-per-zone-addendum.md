# S2 规格修订稿（优先于原规格与复核意见）

## 逐条裁决
### [accept_modified] R1 [high] F5：回档目标镜像早于 Phase A 时，回档在 PITR / FLUSHDB / 删 legacy 位点之后才因「GroupID is not set」CrashLoop
采用复核修法(2)并推广，不做镜像能力校验（OCI label、-capabilities 子命令、git merge-base 祖先判定都不采用）。核对过的依据：k8s_zone_rollback.ps1 组装的 $upArgs 只传 NodeImage，go-svc 的 tag 跟随它；交接闸之前的二进制把 GroupID 当必填（config.go 的 KafkaConfig 里 `json:"GroupID"`）。新规则是「需要交接闸的形状只在非破坏性路径上渲染」。(1) ConfigMap 收敛为四种形状。两种最终形状都写 `GroupID: "<组>"`：final(L) 写 GroupID=db_rpc_consumer_group，final(Z) 写 GroupID=db_rpc_consumer_group_z<id>。两种交接形状都写 ConsumerGroup 加 GroupHandoffFrom：handoff(Z←L) 与 handoff(L←Z)。所有二进制都能加载最终形状。(2) 回档 Step 6、全新 namespace（keep 读不到 ConfigMap）、交接收尾之后的 keep，一律渲染最终形状，判定规则见修订稿 §2 的决策表。回档 Step 6 显式传 -DbConsumerGroup zone。Step 5 收尾后 zone 组有位点、legacy 组在本 topic 上没有位点，决策表因此落到 final(Z)，渲染 `GroupID: "db_rpc_consumer_group_z<id>"`，不带 GroupHandoffFrom，任何镜像都能起。(3) 只有显式 -DbConsumerGroup zone|legacy 的单 zone zone-up，以及 keep 读到一个尚未收尾的交接形状，才会渲染交接形状。这两条路径都不破坏数据：db 的 Deployment 是 maxUnavailable 0，旧镜像的新 Pod 在 MustLoad 就拒启，rollout 卡住，旧 Pod 继续服务。(4) 回档里剩下唯一一种「破坏性步骤之后才确定失败」的情形：Kafka CLI 不能为不存在的 zone 组建出位点。为此加第 0 步 d 的 Step 5 预演，在停服前失败，见 M5。(5) 契约测试有两条：k8s_deploy_contract 用纯函数断言「Mode=zone、Live=null、ZoneHasOffsets=true、LegacyHasOffsets=false」得到 final(Z)，New-DbConsumerGroupYaml 输出恰好一行 `    GroupID: "db_rpc_consumer_group_z101"`；config_test 新增 TestPreHandoffBinaryLoadsFinalZoneShape，用旧结构加载同一段 YAML，必须成功。

### [accept_modified] R2 [high] inspector 用 NewClusterAdmin 建的 admin 永不刷新 coordinator；seed 每次 Setup 都查 admin，查失败就让 Setup 报错，可能把消费卡到 Pod 重启
证据已核实：sarama v1.43.1 的 admin.go 中 DescribeConsumerGroups 与 ListConsumerGroupOffsets 直接用 ca.client.Coordinator(group)；client.go 的 Coordinator 只读缓存，遇到 NOT_COORDINATOR 不刷新。修法如下。(1) 不做「RefreshCoordinator 加重试一次」，改为「任何查询出错就丢弃 inspector，下次查询用工厂重建」，与 kafkautil.GroupLagProber 注释里「探失败就 Close、下一轮重建」同一做法。新 client 会重新 FindCoordinator，coordinator 迁移、broker 重启、元数据变化都能覆盖。groupHandoff 持有 newInspector 工厂、inspMu 和 insp（懒建）。withInspector(fn) 在 inspMu 内执行：insp 为 nil 时先建，建不出来就返回错误；fn 出错时 Close 掉并置 nil；await、seed、watch 共用它。(2) seed 不再每次 Setup 都查。进程内维护 settled 集合（受 mu 保护），只有不在 settled 里的已分配分区才查 admin。加入 settled 有三条路：闸放行时本组位点 ≥0 的分区；Setup 查到本组 ≥0 的分区；本组与对方都没有位点的分区（no_offsets）。刚播种的分区不进 settled：MarkOffset 之后提交可能丢失，比如崩在 1s 自动提交之前，所以下一次 Setup 再查一次本组位点，查到 ≥0 才进。守望让出时清空 settled。复核建议的「只在交接后的第一个 session 查询」不采用：兄弟副本播种后崩溃、提交丢失的分区会从 Oldest 全量重放。(3) seed 查询失败仍然让 Setup 返回错误（计 aborted、丢弃 inspector），1s 后重新入组时用新 inspector。这只会发生在交接窗口内、且只针对未 settled 的分区；收尾之后 GroupHandoffFrom 会被去掉（见 M4），进程里根本没有 handoff。新增 warning 告警 DbConsumerGroupHandoffSeedAborted。(4) 复用 kafkautil 的位点读取，见 R11。(5) 单测：fake 工厂第一次返回的 inspector 报 NOT_COORDINATOR，断言它被 Close、工厂被再调一次、第二次成功；settled 分区不触发任何 inspector 调用。

### [accept] R3 [high] F2 要求反向交接前、目标组仍有本 topic 位点就由脚本 throw；原规格只靠运行时 R7，滚动中途会停消费
k8s_deploy 在显式 -DbConsumerGroup zone|legacy、非 DryRun 时，于写路径预检块（Assert-ClientEntryDeployPreflight 之后，任何写操作之前）读取 Kafka 事实（kubectl exec 进 kafka-0，见修订稿 §3.3），按 §2 的决策表处理。设目标组为 X、另一组为 Y：X 与 Y 在本 topic 上都有数值位点，且线上 ConfigMap 不是 handoff(X←Y)（不是幂等重跑）时，throw（中文），并给出收尾命令 `kafka_offset_reset.ps1 -BootstrapServer <b> -Topic <t> -Group <当前权威组> -RetireGroup <另一组> -Partitions <N> -FinalizeHandoff`。当前权威组取线上 ConfigMap 的有效组；ConfigMap 不存在时两种命令都打印，并说明按 describe 判定哪组在用。复核里「目标组无订阅者时自动先删再部署」不采用：部署脚本不做删除位点这种破坏性动作，收尾必须显式执行 FinalizeHandoff。运行时 R7 保留，作为兜住手工部署、旧脚本的纵深防御。

### [accept_modified] R4 [medium] 空分区永远没有已提交位点，FinalizeHandoff 前置检查、②、seed、merge_zone 全受影响
重新定义「分区已落定（settled）」：组在该分区有数值已提交位点，或者该分区 earliest==latest（日志起点等于末端；从未写入或已全部过期）。这类分区从 Oldest 起步与从对方位点续接等价，不会跳过消息。落实如下。(a) FinalizeHandoff 前置检查：用 kafka-get-offsets.sh --time -2 / --time -1 取每个分区的 earliest / latest。新组缺数值位点的分区只有在 earliest==latest 时放行，否则报问题；新组有数值、旧组也有数值的分区仍要求新组 ≥ 旧组；get-offsets 缺任一分区就报问题（fail-closed）。(b) evaluate 不再使用「本组完整」，②改为一律等待（见 R5），因此不受空分区影响。(c) seed：两组都没有位点的分区计 no_offsets 并进入 settled，之后不再查询；日志降为 Warn，文案写明从未写入的分区属正常。(d) merge_zone：唯一权威组判定保持「在本 topic 上至少有一行数值 CURRENT-OFFSET」，不需要读日志边界。空分区在 describe 里本来就不出现行，不影响唯一性；原来那种死锁（两组都有位点、一直拒绝）源头是收尾跑不了，(a) 修好之后就消失了。复核修法里「ConsumeClaim 开头 MarkOffset(claim.InitialOffset())」不采用。核对 sarama v1.43.1：组没有已提交位点时，offset_manager.go 的 NextOffset 返回 conf.Offsets.Initial，也就是 -2 哨兵；consumer_group.go 的 newConsumerGroupClaim 把这个值原样作为 InitialOffset()；而 MarkOffset 只在 offset > pom.offset(-1) 时生效，-2 是空操作。V5 的通过标准改为「各 Z 实例 seeded 与 no_offsets 覆盖的不同分区合计为 10」。

### [accept] R5 [medium] ② peer_unreachable_own_complete 排在 R7 之前，对方查询失败时跳过 R7，等于 fail-open
删除 outcome peer_unreachable_own_complete。evaluate 的顺序改为：① 本组位点查询出错，或 inspector 建不出来 → 等待 wait_own_unavailable；② 对方位点或 DescribeGroup 出错 → 等待 wait_peer_unavailable，不管本组是否完整；③ 对方 Stable 且 Held 非空 → wait_peer_holding；④ R7 → wait_own_behind_peer；⑤ 对方全部分区为 -1 且 Held 为空 → 放行 no_peer_offsets；⑥ 对方状态为 Empty / Dead，或 Stable 且 Held 为空 → 放行 peer_released；⑦ 状态为 PreparingRebalance / CompletingRebalance → 等待 wait_peer_rebalancing；⑧ 空串或其它未知状态 → 等待 wait_peer_state_unknown，打 Error 日志。B3 的跨 zone 依赖改由三件事收窄：收尾后去掉 GroupHandoffFrom（M4）、inspector 出错即重建（R2）、只闸一次。

### [accept_modified] R6 [medium] F4 未实现：全新 namespace 一律渲染 zone 形状，zone-down/zone-up 会静默撤销反向交接，也不提示收尾
不采用复核建议的「在 zones 配置或 infra 的登记 ConfigMap 里持久化组选择」：登记会与 Kafka 实况漂移，又多出一份真相源。改为 F4 的原意，以 Kafka 为准。keep 读不到 ConfigMap 时（全新 namespace），必须读 Kafka 事实，读失败就 throw（尚未做任何写操作）。两组都有位点 → throw，并给出收尾命令；只有 zone 组有 → final(Z)；只有 legacy 组有 → final(L)，保留运维的反向决定，不会隐式切组；都没有 → final(Z)（新 zone）。全新 namespace 永远只渲染最终形状，不需要收尾，也就没有「隐式交接后忘了收尾」的问题。DryRun 不读集群，按 §2 渲染预览并 Write-Warning，不静默。

### [accept_modified] R7 [medium] 附录 L151：重置 -Apply 之后没有 describe 复核就删 legacy 位点；zone 组不存在时 --reset-offsets --to-datetime 能否建位点未核实
kafka_offset_reset.ps1 的重置模式做四件事。(1) 解析工具输出里的 GROUP/TOPIC/PARTITION/NEW-OFFSET 表（ConvertFrom-KafkaResetPlan）；解析不出就 throw。(2) 给了 -Partitions N>0 时，计划必须覆盖 0..N-1，且 NEW-OFFSET 都是数值；dry-run 和 -Apply 都检查。(3) -Apply 时，--execute 成功之后 describe -Group，逐分区断言 CURRENT-OFFSET 等于 --execute 输出里的 NEW-OFFSET；任一不等、组仍不存在、或解析失败，都 throw（中文：重置后复核失败，未删除 RetireGroup 的位点）。(4) 复核通过之后，才调用 Remove-RetiredGroupTopicOffsets。-RetireGroup 必须同时给 -Partitions>0。zone 组不存在时能否建出位点，采取保守做法：k8s_zone_rollback 新增第 0 步 d（只在 -Apply 下、Step 1 之前执行），先 describe zone 组。zone 组在本 topic 上已有数值位点，说明组存在，跳过预演；否则用 kafka_offset_reset.ps1 的 dry-run（-Group zone 组 -ToDatetime TargetTime -Partitions N，不带 -Apply 和 -RetireGroup）预演 Step 5，计划不完整或退出码非 0 就在停服前 throw。另外交 Codex 在 4.3.1 上实测 V3(c)：对不存在的组 --execute 能建出位点。若实测不能，回档改动（PR-C 的 k8s_zone_rollback 部分）不得发布，交负责人决策；运行期也有第 0 步 d 与 Step 5 复核两道 fail-closed。不改用 --to-offset 或 --from-file：它们与 --to-datetime 走同一条组状态检查和 alterConsumerGroupOffsets 路径，换参数没有用。

### [accept] R8 [medium] dev_tools 的 kafka-offset-reset 分支用数组 splatting，参数按位置绑定，现在就不可用
已核对：dev_tools.ps1 的 kafka-offset-reset 分支是 `$kArgs = @("-BootstrapServer", ...)` 加 `@kArgs`，与 k8s_zone_rollback Step 5 注释里记录的是同一个 bug。PR-C 改成 hashtable splatting：BootstrapServer 与 Topic 必传；Group、ToDatetime、KafkaBin 非空才放；ToEarliest、ToLatest、DeleteAndRecreateTopic、FinalizeHandoff、Apply 这几个开关打开才放 $true；Partitions >0 才放；ReplicationFactor ≠1 才放；RetireGroup 非空才放。新增参数 -KafkaRetireGroup 与 -KafkaFinalizeHandoff。新增测试文件 tools/scripts/tests/dev_tools_kafka_offset_reset.tests.ps1，沿用 dev_tools_proto_gen_exit.tests.ps1 的夹具做法：把 dev_tools.ps1 和 lib/release_common.ps1 拷进临时 tools/scripts 目录，旁边放一个假的 kafka_offset_reset.ps1，把 $PSBoundParameters 按名排序写进记录文件。真实执行一次 dev_tools -Command kafka-offset-reset，断言 BootstrapServer、Topic、Group、RetireGroup、FinalizeHandoff、Partitions、Apply 都按名字绑定正确。

### [accept_modified] R9 [medium] 规格现状与仓库不符、行号漂移、漏报与其它会话的重叠
以当前 HEAD 9f320e485 加工作区为准。原规格写完之后，又有快照提交 0f6690604、676c64789、9b8b902da、41789f474、9f320e485，带走了 k8s_deploy.ps1（+89 行）、lib/k8s_client_entry.ps1、go/login/etc/login.yaml、deploy/k8s/README.md、test_stress_summarize.ps1 等在途改动。现在工作区只剩一个无关的 tmpl 文件未提交。修订稿与实现一律用函数名或锚点文本定位，不用行号；例如 db 模板里的 `GroupID: "db_rpc_consumer_group"` 字面量行、`function New-GoSvcConfigMapYaml`、`function Apply-OneGoSvc`。复核说的「另一个会话」其实是本轮 ①（S3）：ConsumeClaim 解码成功之后新增的 `observeProducerPartition(msg, &dbTask, h.consumer.partitionCount)` 一行，以及 metrics.go 新增的 db_task_producer_partition_total，都已在 0f6690604 提交。S2 必须原样保留这两处，自己的指标追加在它们之后。S2 对 ConsumeClaim 的改动（claimCtx 的父 ctx、yield 分支、派发分支的取消判定）不碰那一行。实现者动手前按 memory「同一工作区多会话并行」再跑一次 git status / git diff。

### [accept_modified] R10 [medium] 本地默认单 zone 的组名是裸名，stress_summarize 改 _z1 会查空组；文档与现状描述有误
本地组名规则不改：go_services.ps1 不加 -Zone 时（start_game / dev-start / go-svc-start 的单 zone 默认启动）组名是裸名 db_rpc_consumer_group，topic 取 db.yaml 的 ZoneId=1，即 db_task_zone_1；-Zone N（dev-start-zones）时组名是 db_rpc_consumer_group_zN、topic 是 db_task_zone_N。本项不碰 go_services.ps1。stress_summarize.ps1 新增 [int]$Zone=0；$KafkaTopic 与 $KafkaConsumerGroup 的缺省值保持 db_task_zone_1 / db_rpc_consumer_group 不变；只有 $Zone -gt 0 且调用方没有显式传这两个参数（用 $PSBoundParameters.ContainsKey 判断）时，才推导为 db_task_zone_$Zone 与 db_rpc_consumer_group_z$Zone。.PARAMETER 帮助同步改。data-consistency-stress-testing.md 的那一行改写为：单 zone 默认启动用 db_rpc_consumer_group；-Zone N 启动用 db_rpc_consumer_group_zN；K8s 切组后用 db_rpc_consumer_group_z<ZoneId>。设计文档 §1 的现状同步更正：「本地合服门禁查不存在的组」只对 dev-start-zones 成立。merge_zone 的候选组方案两种本地情况都能覆盖；两组都有位点时，拒绝文案提示本机混用过两种启动方式时用 -MergeKafkaGroup 显式指定。

### [accept_modified] R11 [low] 重复造轮子：kafkautil 已有基于 sarama 的位点读取
在 go/shared/kafkautil/group_lag.go 抽出导出函数 `CommittedOffsets(r CommittedOffsetsReader, group, topic string, partitions []int32) (map[int32]int64, error)`，加上接口 `CommittedOffsetsReader{ ListConsumerGroupOffsets(group string, topicPartitions map[string][]int32) (*sarama.OffsetFetchResponse, error) }`；groupOffsetReader 内嵌这个接口。错误语义沿用现有逻辑：调用出错、resp 为 nil、resp.Err 非 0、block 缺失、block.Err 非 0，都返回 error，并用 %w 包住 sarama.KError，调用方可以 errors.Is(err, sarama.ErrGroupIDNotFound)。-1 原样返回，表示没有提交过。computeGroupLag 改为调用它，保持「先读 committed 再读 end/start」的顺序和现有行为。group_lag_test 新增 TestCommittedOffsets。go/db 的 saramaGroupInspector.CommittedOffsets 委托给它，ErrGroupIDNotFound 映射为全部 -1。不复用 GroupLagProber 本体：它没有 DescribeConsumerGroups，版本也是 V3_0_0_0。inspector 自建 sarama.Client 加 NewClusterAdminFromClient，网络超时 5s、Metadata.Retry.Max=1，与 GroupLagProber 同口径。

### [accept] R12 [low] evaluate ⑥ 把 State 空串当 peer_released，异常响应时 fail-open
只有 Empty、Dead（包括 desc.Err==ErrGroupIDNotFound 映射来的 Dead），以及「Stable 且 Held 为空」才算释放。PreparingRebalance、CompletingRebalance 归入 wait_peer_rebalancing。空串与任何未知状态归入新 outcome wait_peer_state_unknown，打 Error 日志，并记下原始 State。指标的 outcome 枚举、预建序列与常量同步更新。

### [accept] R13 [low] 「钉版不改变现网版本」只有 Docker Hub 一条依据
新增 V0：上线前由负责人或 Codex 核对，不阻塞落码。(a) `kubectl get pod -n mmorpg-infra -l app=kafka -o jsonpath='{range .items[*]}{.metadata.name}{" "}{.status.containerStatuses[0].imageID}{"\n"}{end}'`，每个 broker 的 imageID 都要含 sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837；(b) 本机有该镜像时跑 `docker image inspect apache/kafka:latest --format '{{json .RepoDigests}}'`，结果要含同一个 digest。任一不一致，PR-A 的发布就按一次 broker 版本升级评估：KRaft metadata.version、日志目录兼容性、能否回退，由负责人决定是否另开窗口。本机命名卷可以在下一次压测前按 AGENTS §6.2 清掉。这一步写进 deploy/k8s/README.md 钉版段落与设计文档 §8 步骤 0。

### [accept] R14 [low] 文档同步遗漏
PR-D 补以下内容，只追加或按锚点改单行。merge-zone-runbook.md 的「`dev_tools.ps1` 已转发 `-kafka-group`」与「-MergeKafkaGroup / -MergeKafkaTopicGeneration 默认从 go/db/etc/db.yaml 现读」两处，改为：组名不再转发，由工具按 zone 推导并认定唯一权威组；-MergeKafkaGroup 只用于显式覆盖；世代号仍现读。tidb-phase1-and-launch-readiness.md：L-09 一行的状态更新为「已落码，未验证（db-consumer-group-per-zone.md）」；§6.3 第 7 步「共享消费组重置单个 topic」那两条之后追加更正行，说明切组之后回档只重置本 zone 组。shared-redis-split-by-purpose.md 的「与 L-09 的关系」段末追加一行，说明 L-09 已落码（未验证）、切组后的 zone 缩扩只让本 zone 再平衡。都只追加，不改原文。

### [accept_modified] R15 [low] 「组不存在」只认一种文案；Setup 总建 yieldCtx 与 TestConsumerSetup_NoHandoffKeepsLegacyBehavior 矛盾
(1) 识别「组不存在」用白名单，PowerShell lib 与 merge_zone 的 Go 两边同一份，互相注释指向对方。三个正则：`(?i)consumer group '[^']*' does not exist`（覆盖 "The consumer group 'g' does not exist." 与 "Error: Consumer group 'g' does not exist."）、`GroupIdNotFoundException`、`GROUP_ID_NOT_FOUND`。另外，「没有表头」且输出含 `(?i)consumer group '[^']*' has no active members` 时，判为「组存在但本 topic 没有位点」（Empty 组 0 行时 CLI 不打印表头）。其余情况：无表头、出现 Error: 行、退出码非 0 且不命中上述白名单，一律报错（fail-closed）。现在就按这些文案写测试数据；Codex 跑完 V3(b)(h)(i) 后，把 4.3.1 的原文追加为 fixture。如果原文一条白名单都不命中，现有代码已经是 fail-closed，只需补正则。(2) 只有 handoff 非空时，Setup 才取消旧 yieldCancel、新建 yieldCtx 并 setSessionYield；handoff 为 nil 时不建，sessionScope 返回 session.Context() 与 nil channel。TestConsumerSetup_NoHandoffKeepsLegacyBehavior 的断言保持不变。

### [accept_modified] M1 漏掉的阻断项 F5：回档 Step 6 与全新 namespace 部署前不校验镜像能力
同 R1：回档 Step 6 与全新 namespace 永远只渲染 GroupID 最终形状，不需要能力校验；交接形状只出现在非破坏性的单 zone 显式 zone-up 上，旧镜像在那里由必填键在 MustLoad 拒启，旧 Pod 继续服务。剩下的 Kafka 工具风险由第 0 步 d 在停服前兜住。设计文档 §10 记录：不采用 F5 的 OCI label，以及不采用的原因。

### [accept] M2 漏掉的阻断项 F2：反向交接前目标组仍有位点不由脚本 throw
同 R3：k8s_deploy 的显式模式读 Kafka 事实后按决策表 throw；R7 保留作纵深防御。runbook 写明：反向（legacy）之前必须先对正向执行 FinalizeHandoff；若脚本拒绝，按它打印的命令收尾后重跑。

### [accept_modified] M3 漏掉的阻断项 F4：ConfigMap 不存在时不以 Kafka 判定组归属
同 R6：keep 遇到全新 namespace 时，用 kubectl exec 读 Kafka 事实，按决策表只渲染最终形状；两组都有位点就 throw。读失败也 throw，此时还没有写操作。不新增登记 ConfigMap。

### [accept] M4 漏掉的阻断项 F3：收尾后不去掉 GroupHandoffFrom；seed 每次查 admin；② 跳过 R7；空分区，导致 B3 共享故障点残留
撤销原规格「保留 GroupHandoffFrom」这一偏离，改为收尾之后去掉。机制是 keep 自动过渡：keep 读到线上交接形状 handoff(X←Y) 时读 Kafka 事实。Y 在本 topic 上没有数值位点（FinalizeHandoff 已删），就渲染 final(X)，即 `GroupID: "X"`，Source=handoff-finalized，Write-Host 提示将去掉 GroupHandoffFrom 并滚一次 db（注解 db-group-handoff-from 由对方组名变为 none，会触发滚动）。Y 仍有位点时保持交接形状，Write-Warning 并打印收尾命令。Kafka 读失败时保持线上形状（原样保留永远安全）并 Write-Warning，不 throw。runbook 的切组步骤改为：切组 → FinalizeHandoff（先 dry-run，再 -Apply）→ 立即以 keep 再跑一次 zone-up。这样收尾之后的进程没有 handoff 对象，不再依赖 legacy coordinator。其余三点：seed 缓存见 R2，② 见 R5，空分区见 R4。代价（写进设计文档 §10）：收尾之后守望不再运行，如果有人误部署旧发布物，运行时发现不了，只能靠 runbook 禁止加上游标与排序锁兜底。

### [accept_modified] M5 漏掉的阻断项：附录 L151，重置 -Apply 后没有 describe 复核
同 R7：kafka_offset_reset 重置后按 --execute 输出的计划逐分区复核，复核通过才删除 RetireGroup 的位点；回档第 0 步 d 在停服前预演 Step 5（只在 zone 组没有位点时）；V3(c) 交 Codex 实测，不行则不发布回档改动，交负责人决策。

### [accept] M6 漏掉的阻断项 F6：dev_tools 本地回档、重置的透传依赖已经坏掉的数组 splatting
同 R8：PR-C 修成 hashtable splatting，并用真实执行 dev_tools 的夹具测试钉住。k8s-zone-rollback 分支照原规格：去掉 KafkaDrainTimeoutSec；KafkaGroup、KafkaLegacyGroup 非空才放进 $rbArgs；-RollbackKafkaGroup 缺省改为空串。

## 修订后的设计
# S2 ③ 修订稿（实现时以本稿为准；本稿没提到的部分按原规格 S2_spec.md 执行）

## 0 读法
实现顺序：原规格 → 复核 → 本稿。冲突时以本稿为准。本稿只写「改了什么、改成什么」；原规格中未被推翻的条目（PR-A 钉版、login GroupID 改 optional、consumerClientID、rebalanceGapTracker、超时键、Pod 注解占位、告警组主体、设计文档章节、PARITY 登记等）照旧。全部未编译，待 Codex 验证。

## 1 现状更正（以 HEAD 9f320e485 + 工作区为准）
- 原规格写完之后，快照提交 0f6690604 / 676c64789 / 9b8b902da / 41789f474 / 9f320e485 又改了 k8s_deploy.ps1、lib/k8s_client_entry.ps1、go/login/etc/login.yaml、deploy/k8s/README.md、test_stress_summarize.ps1、go/db 的 key_ordered_consumer.go 与 metrics.go，行号全部作废。实现时一律按函数名或锚点文本定位。db 模板的锚点是 `    GroupID: "db_rpc_consumer_group"` 字面量行；login 模板的锚点是 `  GroupID: "db_rpc_consumer_group"` 那一行。
- ConsumeClaim 里 `observeProducerPartition(msg, &dbTask, h.consumer.partitionCount)` 这一行，以及 metrics.go 里的 db_task_producer_partition_total，都属于本轮 ①（S3），已提交，必须保留。
- 本地组名：go_services.ps1 只在 -Zone>0 时追加 _zN（`if ($Zone -gt 0)` 块里的 GroupID 正则）。单 zone 默认启动是裸名 db_rpc_consumer_group，topic 是 db_task_zone_1。原规格现状第 2 条「本地与压测一直是每 zone 一组」、第 5 条「本地合服门禁一直在查不存在的组」，都只对 dev-start-zones 成立。
- sarama v1.43.1 核对结论：admin 不刷新 coordinator；组没有已提交位点时，claim.InitialOffset() 返回 OffsetOldest 哨兵 -2；MarkOffset 只往前推；offset manager 在 Setup 之前就已取回本组位点，heartbeat 也在 Setup 之前启动。

## 2 ConfigMap 形状与组归属（取代原规格的三形状、Resolve-DbConsumerGroupPlan、keep / fresh-namespace 规则）
记 L=db_rpc_consumer_group（db.yaml ServerConfig.Kafka.GroupID），Z=L+"_z"+ZoneId，T=本 zone 的 db_task topic。
四种形状：
- final(G)：一行 `    GroupID: "G"`，G∈{L,Z}。任何二进制都能加载。最终形状只用它，不写 ConsumerGroup。
- handoff(X←Y)：`    ConsumerGroup: "X"` 和 `    GroupHandoffFrom: "Y"`，{X,Y}={L,Z}，不写 GroupID。旧二进制在 MustLoad 拒启，这就是能力证明。
- 生成器只写这四种；ConvertFrom-LiveDbConsumerGroup 只接受这四种，其它一律 throw（中文）。每个键按行锚定匹配；同一个键出现两次就 throw。
Kafka 事实：Zc / Lc 表示该组在 T 上至少有一行数值 CURRENT-OFFSET。

决策表（纯函数 Resolve-DbConsumerGroupPlan -Mode keep|zone|legacy -LegacyGroup -ZoneGroup -IsDryRun [bool] -Live [AllowNull] -Facts [AllowNull]（@{ZoneHasOffsets;LegacyHasOffsets}）-FactsError [string]；返回 @{Shape='final'|'handoff'; Group; HandoffFrom; Source; Notice; Warning}，或者 throw）：
DryRun（不读集群）：
- keep → final(Z)，Source=dryrun-preview。
- zone → handoff(Z←L)，Source=dryrun-preview。
- legacy → handoff(L←Z)，Source=dryrun-preview。
- 三种都附 Warning：没有读集群；真跑时按线上 ConfigMap 与 Kafka 位点，可能渲染最终形状，也可能拒绝。
非 DryRun，keep：
- Live 为 final(G) → final(G)，Source=live-configmap，不读 Kafka。
- Live 为 handoff(X←Y)：FactsError 非空 → 保持 handoff(X←Y)，并给 Warning（读不到 Kafka，保持线上形状）；Y 没有位点 → final(X)，Source=handoff-finalized，Notice 写「交接已收尾，去掉 GroupHandoffFrom，db 将滚动一次」；Y 仍有位点 → 保持 handoff(X←Y)，并给 Warning 加收尾命令。
- Live 为 null（全新 namespace）：Zc 与 Lc 都为真 → throw（F4），附收尾命令的两种写法；只有 Zc → final(Z)；只有 Lc → final(L)；都没有 → final(Z)。Source=fresh-namespace-kafka。
非 DryRun，显式模式（zone：X=Z、Y=L；legacy：X=L、Y=Z）：
- X 与 Y 都有位点：Live 为 handoff(X←Y) 时是幂等重跑，保持 handoff，并给 Warning 说明未收尾；否则 throw（F2）。throw 的中文文案说明两组在 T 上都有位点、上一次交接没收尾，并给出 `kafka_offset_reset.ps1 -BootstrapServer <b> -Topic <T> -Group <当前权威组> -RetireGroup <另一组> -Partitions <N> -FinalizeHandoff`。当前权威组取 Live 的有效组（final 的 G，或 handoff 的 X）；Live 为 null 时打印两种写法。
- 只有 X 有 → final(X)，Source=explicit-already-authoritative。回档 Step 6 走的就是这一条。
- 只有 Y 有 → handoff(X←Y)，Source=explicit-handoff。
- 都没有 → final(X)，Source=explicit-nothing-to-inherit。
显式模式只允许 -Command zone-up，入口拒绝的文案与原规格相同。

## 3 tools/scripts/k8s_deploy.ps1（覆盖原规格对应的 ④⑤⑦⑧ 条）
3.1 参数：[ValidateSet("keep","zone","legacy")][string]$DbConsumerGroup='keep'。入口拒绝规则同原规格。
3.2 新增纯函数：ConvertFrom-LiveDbConsumerGroup（§2 的四种形状，返回 @{Shape;Group;HandoffFrom}）、Resolve-DbConsumerGroupPlan（§2）、New-DbConsumerGroupYaml -Plan。New-DbConsumerGroupYaml 先输出一行注释 `    # 消费组由 k8s_deploy.ps1 按 -DbConsumerGroup 与 Kafka 位点决定(docs/design/db-consumer-group-per-zone.md)`，final 再输出一行 GroupID，handoff 再输出两行。
3.3 新增 Read-DbGroupTopicFacts -Topic -ZoneGroup -LegacyGroup，只在非 DryRun 时调用。对每个组执行 Invoke-ClientEntryKubectl -KubectlArgs @('exec','-n',$InfraNamespace,'kafka-0','-c','kafka','--','/opt/kafka/bin/kafka-consumer-groups.sh','--bootstrap-server','localhost:9092','--describe','--group',<g>) -KubeContext $KubeContext -KubeConfig $KubeConfig -CaptureOutput -AllowFailure。把 Stdout 与 Stderr 拆行合并，交给 lib 的 Resolve-KafkaGroupTopicOffsets 分类：missing、no-offsets、offsets，或者 throw。返回 @{ZoneHasOffsets; LegacyHasOffsets}。topic 名由 lib 的 Get-DbTaskTopicName -ZoneId -Generation 拼出（generation ≤1 为 db_task_zone_<id>，否则加 _g<gen>，与 go/db config.DbTaskTopicForGeneration 同规则，注释互指）；generation 取 db.yaml 的 ServerConfig.Kafka.TopicGeneration。
3.4 Initialize-DbConsumerGroupPlans：放在写路径预检块、紧跟 Assert-ClientEntryDeployPreflight 之后，只在 Test-ZoneDbDeployed 为真时执行。逐个 zone：非 DryRun 时用 Read-LiveDbConfigMapYaml 读线上 ConfigMap（同原规格⑥：读失败 throw；输出为空返回 $null；有输出但缺 db.yaml 键就 throw），再 ConvertFrom-LiveDbConsumerGroup。需要 Kafka 事实的条件：非 DryRun，且（Mode≠keep，或 Live 为 null，或 Live.Shape='handoff'）。读事实失败时，只有「keep 加 Live 为 handoff」把错误文本作为 FactsError 传入，其余情况 throw（中文：读不到 Kafka 位点，无法判定 db 消费组归属，尚未做任何写操作；检查 kafka-0 是否就绪、是否有 pods/exec 权限）。最后调用 Resolve-DbConsumerGroupPlan，结果存入 $script:DbConsumerGroupPlans[zoneId]；每个 zone 打一行 `DB consumer group: zone=… mode=… shape=… group=… handoff_from=… source=…`；Notice 用 Write-Host，Warning 用 Write-Warning。
3.5 New-GoSvcConfigMapYaml 的 db 分支：用 New-DbConsumerGroupYaml 的输出替换锚点行。login 模板只在 GroupID 上方加一行注释，值不动（同原规格）。Apply-OneGoSvc 的 db 注解块照原规格：db-consumer-group 写 Plan.Group，db-group-handoff-from 写 Plan.HandoffFrom，为空时写 none。

## 4 go/db 交接闸（覆盖原规格 group_handoff.go 与 key_ordered_consumer.go 的对应部分）
4.1 groupInspector 接口不变：DescribeGroup(group, topic)、CommittedOffsets(group, topic, partitions)、Close。saramaGroupInspector 持有 sarama.Client，经 NewClusterAdminFromClient 得到 admin，配置 Version=consumerKafkaVersion、ClientID=<clientID>-handoff、Net.Dial/Read/WriteTimeout=5s、Metadata.Retry.Max=1。CommittedOffsets 委托给 kafkautil.CommittedOffsets(admin, …)，errors.Is(err, sarama.ErrGroupIDNotFound) 时全部按 -1。DescribeGroup：结果条数不等于 1 → error；desc.Err==ErrGroupIDNotFound → State=Dead；其它非零 Err → error；Held 用 heldPartitions 解析，解码出错返回 error。
4.2 groupHandoff 字段：group, peer, topic, partitions；newInspector func() (groupInspector, error)；inspMu sync.Mutex；insp groupInspector；now、sleep；mu sync.Mutex 保护 released、sessionYield、settled map[int32]struct{}；watchPrev 只由守望 goroutine 读写。withInspector(fn func(groupInspector) error) error 持 inspMu 执行：insp 为 nil 时用工厂建，建失败返回 fmt.Errorf("group handoff inspector: %w")；fn 返回错误时 insp.Close()（Close 的错误只打日志）并置 nil。newGroupHandoffFromConfig 在 GroupHandoffFrom 为空时返回 (nil, nil)，且不调用工厂（T8）；不为空时只保存工厂，懒建。Close() 持 inspMu 关闭 insp。
4.3 evaluate()：在一次 withInspector 里依次查本组位点（全部分区）、对方位点（全部分区）、对方 DescribeGroup，分别记录 ownErr / peerErr。判定顺序：inspector 建不出来或 ownErr 非空 → wait_own_unavailable；peerErr 非空 → wait_peer_unavailable；Stable 且 Held 非空 → wait_peer_holding；R7（存在 p 满足 own≥0、peer≥0、peer>own）→ wait_own_behind_peer，detail 带 p、own、peer；对方全部为 -1 且 Held 为空 → 放行 no_peer_offsets；Empty / Dead，或 Stable 且 Held 为空 → 放行 peer_released；PreparingRebalance / CompletingRebalance → wait_peer_rebalancing；其它（含空串）→ wait_peer_state_unknown（Error 日志，带原始 State）。放行时在 mu 内置 released=true，并把 settled 置为 {p : own[p]≥0}。
4.4 seed(session, claimed)：need 取 claimed 中不在 settled 里的分区（在 mu 内取快照）。need 为空时直接返回 nil，不调 admin，不计指标。否则在一次 withInspector 里：查本组 need 的位点，出错时每个分区计 aborted，返回错误；own≥0 的分区进 settled，计 own_present；剩下的记为 rest，rest 为空就返回。再查对方 rest 的位点，出错时 rest 里每个分区计 aborted，返回错误；对方 ≥0 的分区 MarkOffset(topic, p, peer, "")（原值，不加 1），计 seeded，打 Info 日志，不进 settled；对方也是 -1 的分区计 no_offsets，进 settled，打 Warn 日志（从 Offsets.Initial 即 oldest 起步，从未写入的分区属正常）。
4.5 yield()：在 mu 内置 released=false、清空 settled、调用 sessionYield。watchOnce 走 withInspector，出错只返回（inspector 随之重建），判据不变：Stable、Held 非空、相邻两次观测之间位点前进。
4.6 key_ordered_consumer.go：Setup 先查分区契约。handoff 非空时，依次调 seed（出错返回 fmt.Errorf("group handoff seeding failed: %w")）→ 取消旧 yieldCancel → 建 yieldCtx → setSessionYield，返回 false 就立即 cancel。handoff 为 nil 时不建 yieldCtx。最后记 gap。Cleanup、ConsumeClaim、Stop 按原规格，派发分支的取消判定同原规格。不改 Offsets.Initial；ConsumeClaim 里不加 MarkOffset(InitialOffset)。
4.7 metrics：gate outcome 改成 8 个值：no_peer_offsets、peer_released、wait_peer_holding、wait_peer_rebalancing、wait_peer_state_unknown、wait_own_behind_peer、wait_peer_unavailable、wait_own_unavailable。删除 peer_unreachable_own_complete。预建序列与常量同步。其余 5 个指标不变，全部追加在 S3 的 db_task_producer_partition_total 之后。
4.8 告警：在原规格 4 条之外增加第 5 条 DbConsumerGroupHandoffSeedAborted：`sum by (namespace)(increase(db_consumer_group_handoff_total{result="aborted"}[5m])) > 0`，severity warning。描述写明：播种查询失败会让本 zone 组反复再平衡；检查 Kafka coordinator，或者 legacy 组是否可读。

## 5 go/shared/kafkautil/group_lag.go
新增 CommittedOffsetsReader 接口与 CommittedOffsets 函数（签名与错误语义见 R11 裁决）；groupOffsetReader 内嵌该接口；computeGroupLag 改为调用它，读取顺序与对外行为不变。

## 6 tools/scripts/lib/kafka_consumer_groups.ps1（纯函数库；覆盖原规格同名文件）
- Test-KafkaGroupMissingText -Text：三个白名单正则，见 R15。
- Resolve-KafkaGroupTopicOffsets -Lines -ExitCode -Topic：命中「组不存在」→ @{State='missing'; Committed=@{}}；否则有 ^\s*Error: 行或退出码非 0 → throw；否则找到表头（按列名定位 TOPIC、PARTITION、CURRENT-OFFSET、LOG-END-OFFSET、LAG）→ 只看 TOPIC==Topic 的行，「-」记 $null，Committed 收集数值行，State 为 'offsets' 或 'no-offsets'；否则没有表头、但含 has-no-active-members 行 → 'no-offsets'；其余一律 throw（无法解析 describe 输出）。
- ConvertFrom-KafkaResetPlan -Lines -Topic：按表头列名 TOPIC / PARTITION / NEW-OFFSET 解析，返回 @{[int]p=[long]offset}；找不到表头就 throw。
- ConvertFrom-KafkaGetOffsets -Lines -Topic：解析 `^(?<t>.+):(?<p>\d+):(?<o>-?\d+)$`，只收 t==Topic 的行；一行都没有就 throw。
- Test-HandoffFinalizeReady -NewOffsets -OldOffsets -Earliest -Latest -Partitions：返回问题列表。对每个 p∈0..N-1：Earliest 或 Latest 缺 p → 报问题；新组缺 p 且 earliest≠latest → 报问题；新组与旧组都有、且新组 < 旧组 → 报问题。
- Get-DbTaskTopicName -ZoneId -Generation：规则见 §3.3。

## 7 tools/scripts/kafka_offset_reset.ps1
- 新增参数 -RetireGroup ''、-FinalizeHandoff。模式互斥：ToDatetime / ToEarliest / ToLatest / DeleteAndRecreateTopic / FinalizeHandoff 恰好一个。FinalizeHandoff 必须给 -Group、-RetireGroup、-Topic、-Partitions>0。-RetireGroup 只能配 ToDatetime / ToEarliest / ToLatest 或 FinalizeHandoff，而且必须 -Partitions>0；不能与 DeleteAndRecreateTopic 同用；不能等于 -Group。违反任一条都 throw。
- 重置模式：把工具输出捕获下来，先过 Assert-KafkaToolOutputClean，再用 ConvertFrom-KafkaResetPlan 解析出 plan。-Partitions>0 时，plan 缺少 0..N-1 中任一分区、或 NEW-OFFSET 不是数值，就 throw；dry-run 与 -Apply 都检查。-Apply 时，describe -Group，用 Resolve-KafkaGroupTopicOffsets 分类，逐分区比较：plan 里的每个分区（N>0 时为 0..N-1）的 CURRENT-OFFSET 都必须等于 plan 值，否则 throw（中文：重置后复核失败，没有删除 <RetireGroup> 的位点，并列出不符的分区）。复核通过、且给了 -RetireGroup 时，执行 Remove-RetiredGroupTopicOffsets -Apply（有意回退，所以不做「新 ≥ 旧」检查）。dry-run 只打印计划，以及「复核通过后将删除 <RetireGroup> 在 <T> 上的位点」。
- FinalizeHandoff：依次 describe -Group 与 -RetireGroup，再执行 Resolve-KafkaTool "kafka-get-offsets.sh"，参数 --bootstrap-server B --topic T --time -2 和 --time -1，得到 earliest / latest。Test-HandoffFinalizeReady 有问题就 throw 并列出全部问题；没有问题时，dry-run 打印计划，-Apply 执行 Remove-RetiredGroupTopicOffsets -Apply。成功后打印下一步：对该 zone 以 keep 执行一次 zone-up，去掉 GroupHandoffFrom。
- Remove-RetiredGroupTopicOffsets：describe 旧组，状态为 missing 或 no-offsets 时打印 nothing to retire。dry-run 打印将要执行的 --delete-offsets 命令。-Apply 执行 --bootstrap-server B --delete-offsets --group G --topic T，捕获输出并过 Assert-KafkaToolOutputClean，再 describe 一次；仍是 offsets 就 throw（中文，提示多半仍有成员订阅该 topic，即 GROUP_SUBSCRIBED_TO_TOPIC）。

## 8 tools/scripts/k8s_zone_rollback.ps1（覆盖原规格对应部分）
- 入口处用 Get-YamlScalar 读 $RepoRoot/go/db/etc/db.yaml 的 ServerConfig.Kafka.GroupID（作基名）和 ServerConfig.Kafka.PartitionCnt（必须 >0）。读不到就 throw（中文，fail-closed）。不再设 $DbConsumerGroupBase 常量。-KafkaGroup 缺省为 ''，推导为 "<基名>_z$ZoneId"；新增 -KafkaLegacyGroup ''，推导为基名；两组相等就 throw。删除 -KafkaDrainTimeoutSec 与 Step 2 整块。.DESCRIPTION 按原规格改，另外补上第 0 步 c / d。
- $upArgs.DbConsumerGroup='zone'，注释写明：Step 5 收尾后 zone 组完整、legacy 组已退役，真跑时决策表落到 final(Z)，即 `GroupID: "<zone 组>"`，任何镜像都能起。
- 第 0 步 c：Show-KafkaTopicLagSnapshot，同原规格（只告警，绝不 throw）。
- 第 0 步 d（新增，只在 -Apply 下、Step 1 之前执行；dry-run 只打印命令）：用本机 kafka-consumer-groups.sh describe zone 组，再用 Resolve-KafkaGroupTopicOffsets 分类。结果为 offsets 时打印 `zone group already holds offsets on <T>; Step 5 rehearsal skipped`。结果为 missing 或 no-offsets 时，以 hashtable splatting 调用 kafka_offset_reset.ps1，参数为 BootstrapServer、Topic、Group=zone 组、ToDatetime=$TargetTime、Partitions=N（不带 Apply 与 RetireGroup）；退出码非 0 就 throw（中文：Step 5 预演失败，Kafka 无法为 zone 组生成完整的重置计划，停服前拒绝；见设计文档 §11 V3(c)）。describe 本身出错也 throw。
- Step 5：$kArgs 为 BootstrapServer、Topic、Group=zone 组、ToDatetime、RetireGroup=legacy 组、Partitions=N，-Apply 时再加 Apply。
- （2026-10-18 补，负责人 2026-10-17 裁决「丢弃 TargetTime 之后的写」后已落码，未编译、未测试：第 0 步 d 与 Step 5 的 `ToDatetime` 一律换成 `ToLatest`；不清游标；不再 FLUSHDB，
  本 topic 的重试收据与受影响玩家的 T 之后状态交给回档工具 zone_rollback_discard（尚未落地）；另加第 0 步 f、Step 1b、4a / 4b / 5b。本节其余条目仍有效，现行流程以
  `docs/design/zone_data_rollback.md`「2026-10-18 修订」为准。）

## 9 tools/scripts/dev_tools.ps1
在原规格 ①–④ 的基础上：kafka-offset-reset 分支改为 hashtable splatting（规则见 R8）。-DbConsumerGroup 的 ValidateSet 为 "", keep, zone, legacy。

## 10 tools/merge_zone
按原规格实现候选组与唯一权威组。另外：consumerGroupMissing 改用三个白名单正则；committedOffsetPresent 在没有表头时，若命中 has-no-active-members 就返回 (false, nil)，否则返回 error（不再是「没有表头就返回 false」）。两组都有位点时，拒绝文案在原有的 FinalizeHandoff 提示之外，再加一句：本机混用过单 zone 默认启动（裸名）与 -Zone 启动（_z）时，用 -kafka-group 或 -MergeKafkaGroup 显式指定实际在跑的组。

## 11 tools/scripts/stress_summarize.ps1
见 R10：新增 [int]$Zone=0，缺省值不变，只在 Zone>0 且未显式传参时推导。

## 12 测试（在原规格基础上增删）
- group_handoff_test.go：删掉断言 peer_unreachable_own_complete 的分支；TestGroupHandoffGate_AdminErrors 改为「对方出错时，不论本组是否完整都等待 wait_peer_unavailable」。新增：TestGroupHandoffGate_PeerStateUnknownWaits（State 为 "" 和 "Bogus" 时都是 wait_peer_state_unknown）；TestGroupHandoffInspector_RebuiltAfterError（第一个 inspector 返回包住 ErrNotCoordinatorForConsumer 的错误后被 Close，工厂再被调用一次，下一次判定成功）；TestGroupHandoffSeed_SkipsSettledPartitions（第一次 Setup：{0,1} own≥0、{2} 两边都是 -1、{3} 被播种；第二次 Setup claims {0,1,2} 时 inspector 调用次数为 0；claims {3} 时只查本组一次，own≥0 后进入 settled）；TestGroupHandoffYield_ClearsSettled；TestGroupHandoffGate_ReleaseSeedsSettledFromOwnOffsets。
- go/shared/kafkautil/group_lag_test.go：TestCommittedOffsets，覆盖顶层 ErrGroupIDNotFound 可被 errors.Is 识别、resp 为 nil、block 缺失、block.Err、-1 原样返回。TestComputeGroupLag 保持绿。
- config_test.go：在原规格用例之外，新增 TestPreHandoffBinaryLoadsFinalZoneShape：旧结构加载只含 `GroupID: "db_rpc_consumer_group_z101"` 的 YAML 必须成功。
- metrics_test.go：预建序列断言加上 wait_peer_state_unknown，删除 peer_unreachable_own_complete。
- k8s_deploy_contract.tests.ps1：基线 DryRun（keep）断言 go-svc-db-config 有 GroupID=db_rpc_consumer_group_z101，没有 ConsumerGroup 与 GroupHandoffFrom，输出含 dryrun-preview；注解 db-group-handoff-from 为 "none"。-DbConsumerGroup zone 的 DryRun 断言为 handoff(Z←L)，legacy 的 DryRun 断言为 handoff(L←Z)；负向用例与原规格相同。纯函数（AST 抽取）：Resolve-DbConsumerGroupPlan 覆盖 §2 决策表的每一行，包括回档行（zone、Live=null、Zc=$true、Lc=$false → final(Z)），以及 New-DbConsumerGroupYaml 对它的输出恰好是注释行加 `    GroupID: "db_rpc_consumer_group_z101"`；ConvertFrom-LiveDbConsumerGroup 覆盖四种合法形状加至少四种非法形状。仓库里现有对 db ConfigMap GroupID 字面量的断言按新的基线更新。
- kafka_offset_reset_output_guard.tests.ps1：在原规格用例之外，新增 Resolve-KafkaGroupTopicOffsets 的五种输入：does-not-exist 文案、GroupIdNotFoundException 文案、无表头加 has no active members、Error 行、无表头无关键字（必须 throw）；ConvertFrom-KafkaResetPlan 与 ConvertFrom-KafkaGetOffsets 的正常输入与缺表头输入；Test-HandoffFinalizeReady 的空分区放行（earliest==latest）与非空分区缺位点报问题；源码断言「-RetireGroup 缺 -Partitions 时 throw」，以及「复核失败的 throw 位于 Remove-RetiredGroupTopicOffsets 调用之前」。
- k8s_zone_rollback_gate_router_mode.tests.ps1：沙箱改成 <tmp>/<guid>/tools/scripts（脚本与 lib），并把仓库的 go/db/etc/db.yaml 拷到 <tmp>/<guid>/go/db/etc/db.yaml。假 kafka_offset_reset.ps1 每次调用追加一行记录（具名参数按名排序）。用例：-Apply 全程通过时，Step 6 的 DevToolsBound 含 DbConsumerGroup=zone；最后一次 KafkaResetBound 含 Group=db_rpc_consumer_group_z101、RetireGroup=db_rpc_consumer_group、Partitions=10、Apply。假 describe 报 zone 组不存在时，第 0 步 d 先调用一次 kafka_offset_reset（不带 Apply、不带 RetireGroup）；那次假脚本以非 0 退出时，脚本 throw，且 DevToolsCalls 为 0（Step 1 没有执行）。其余同原规格。
- 新增 tools/scripts/tests/dev_tools_kafka_offset_reset.tests.ps1，见 R8。

## 13 Codex 验证（覆盖原规格的 V1–V5 对应部分）
- V0（上线前，不阻塞落码）：见 R13。
- V1 追加：`cd go/shared; go vet ./kafkautil/; go test ./kafkautil/ -count=1 -run 'CommittedOffsets|ComputeGroupLag' -v`；`cd go/data_service; go build ./...`（它用 GroupLagProber）。go/db 的 -run 正则加上 Inspector|Settled|StateUnknown。
- V3 追加，逐条记录原始输出与退出码，回填到设计文档 §11 与测试 fixture：(f) --reset-offsets --execute 是否打印 NEW-OFFSET 表；(g) kafka-get-offsets.sh --time -2 / -1 的输出格式；(h) Empty 组、没有已提交位点时 describe 的输出；(i) 不存在组的 describe 原文（是 does not exist 还是 GroupIdNotFoundException）；(j) 在 kind 里执行 `kubectl exec -n mmorpg-infra kafka-0 -c kafka -- /opt/kafka/bin/kafka-consumer-groups.sh --bootstrap-server localhost:9092 --describe --group <g>` 能否成功。(c) 若证实不能为不存在的组建出位点，回档改动不得发布，交负责人决策。
- V4 追加：新测试文件 dev_tools_kafka_offset_reset.tests.ps1。
- V5 改动：通过标准改为「Z1/Z2 的 seeded 与 no_offsets 覆盖的不同分区合计 10，每个 seeded 值等于 L 停掉后该分区的 CURRENT-OFFSET」。收尾之后，再起一个 final 形状的 Z3（`GroupID: db_rpc_consumer_group_z1`，不带 GroupHandoffFrom）替换 Z1/Z2，断言它从自身位点续消费，不播种（handoff_total 没有新样本），verifier 通过。反向流程：先确认 legacy 组在本 topic 上已没有位点，起 R1/R2，收尾后换 final(L)。(k) 改为：旧二进制加交接形状必须拒启；旧二进制加 final(Z) 形状必须能起，并从 zone 组位点续消费。

## 14 上线与回滚（覆盖原规格对应段）
- 步骤 0：先做 V0，再合入 PR-A（同原规格）。
- 步骤 1：Phase A 同原规格。
- 步骤 2：合入 PR-C。线上已有 GroupID 形状的 zone 在 keep 下保持不变，但第一次部署会因为新注解滚一次 db。全新 namespace 与回档只渲染最终形状，旧镜像也能起。只有显式 zone|legacy 交接要求该 zone 已在运行 Phase A 及以后的镜像。
- 步骤 3（金丝雀）：冻结其它 zone 的发布，执行 `k8s_deploy.ps1 -Command zone-up -ZoneName <z> -DbConsumerGroup zone -GoSvcTag <与线上相同>`，脚本会先做 F2 预检。观察 waiting → released → seeded / no_offsets 覆盖 10 个分区、conflict 为 0、gap <15s。然后执行 `kafka_offset_reset.ps1 -BootstrapServer <b> -Topic db_task_zone_<id> -Group db_rpc_consumer_group_z<id> -RetireGroup db_rpc_consumer_group -Partitions 10 -FinalizeHandoff`，先 dry-run 再加 -Apply。紧接着以 keep 再跑一次同样的 zone-up，确认输出含 source=handoff-finalized，ConfigMap 变为 `GroupID: "db_rpc_consumer_group_z<id>"`，db 滚动完成。
- 步骤 4–7：同原规格；每个 zone 都重复步骤 3 的全部收尾动作。
- 回滚（单 zone 反向）：确认正向已收尾（脚本也会拦）→ zone-up -DbConsumerGroup legacy → FinalizeHandoff（-Group db_rpc_consumer_group -RetireGroup db_rpc_consumer_group_z<id>）→ keep zone-up 落到 final(L)。退回到 Phase A 之前的镜像：最终形状下可以直接用旧镜像部署；交接形状下必须先收尾、落到最终形状。设计文档 §8 的手工兜底只保留给「收尾之前必须退镜像」这一种情形。

## 15 重叠与顺序（补充原规格）
- S3（①）：保留 ConsumeClaim 解码之后的 observeProducerPartition 那一行，以及 metrics.go 已有的 db_task_producer_partition_total；S2 的指标、MustRegister 项、预建序列都追加在它们之后。
- S1（④）与 S4：挂点按原规格；Setup 的顺序以本稿 4.6 为准（handoff 为 nil 时不建 yieldCtx）。
- S4：stress_summarize.ps1 的 DB 段输出归 S4，S2 只改参数块与推导。
- go/shared/kafkautil/group_lag.go：S2 只做抽取，不改行为；若与 data_service 的 lag 探针改动并行，合并时保留 computeGroupLag 的读取顺序注释。

## 文件清单变化
相对原规格的文件清单变更（本仓库由 44 个增至 49 个，另有 Java 仓库 1 个；分 4 个 PR，每个都少于 30 个文件，沿用已拍板的「4 PR 继续」决定）：
新增到 PR-B：
- [modify] go/shared/kafkautil/group_lag.go：抽出 CommittedOffsetsReader 与 CommittedOffsets，computeGroupLag 改为调用它，行为不变。
- [modify] go/shared/kafkautil/group_lag_test.go：新增 TestCommittedOffsets。
PR-B 内已有文件，内容有变：
- group_handoff.go / group_handoff_test.go：inspector 出错即重建、settled 缓存、evaluate 新顺序、新增 wait_peer_state_unknown、删除 peer_unreachable_own_complete。
- key_ordered_consumer.go：handoff 为 nil 时不建 yieldCtx；不加 MarkOffset(InitialOffset)。
- metrics.go / metrics_test.go：outcome 枚举变更。
- config_test.go：新增 TestPreHandoffBinaryLoadsFinalZoneShape。
- deploy/k8s/db-task-retry-alerts.yaml：第 5 条告警 DbConsumerGroupHandoffSeedAborted。
- docs/design/db-consumer-group-per-zone.md：§2 四种形状与决策表；§10 改为「F3 已按附录去掉 GroupHandoffFrom（keep 自动过渡）、F4 以 Kafka 事实判定、F5 改为最终形状加必填键证明、R7 保留」；§11 增加 V3(f)–(j)。
新增到 PR-C：
- [new] tools/scripts/tests/dev_tools_kafka_offset_reset.tests.ps1：真实执行 dev_tools 的夹具测试，钉住 hashtable splatting 与新参数透传。
PR-C 内已有文件，内容有变：
- tools/scripts/dev_tools.ps1：kafka-offset-reset 分支由数组 splatting 改为 hashtable splatting，修复 F6；新增 -KafkaRetireGroup 与 -KafkaFinalizeHandoff 透传。
- tools/scripts/k8s_deploy.ps1：四种形状、决策表、Read-DbGroupTopicFacts（kubectl exec kafka-0）、keep 自动去掉 GroupHandoffFrom、F2 / F4 的 throw。
- tools/scripts/lib/kafka_consumer_groups.ps1：新增 Test-KafkaGroupMissingText、Resolve-KafkaGroupTopicOffsets、ConvertFrom-KafkaResetPlan、ConvertFrom-KafkaGetOffsets、Test-HandoffFinalizeReady、Get-DbTaskTopicName。
- tools/scripts/kafka_offset_reset.ps1：计划覆盖检查、-Apply 后复核、复核通过后才 retire、FinalizeHandoff 用 kafka-get-offsets 判定空分区。
- tools/scripts/k8s_zone_rollback.ps1：从 db.yaml 读基名与 PartitionCnt、第 0 步 d 预演 Step 5、Step 5 传 Partitions、Step 6 渲染最终形状。
- tools/scripts/tests/k8s_zone_rollback_gate_router_mode.tests.ps1：沙箱改为 <tmp>/<guid>/tools/scripts，并拷入 go/db/etc/db.yaml；新增第 0 步 d 用例。
- tools/scripts/tests/k8s_deploy_contract.tests.ps1、tools/scripts/tests/kafka_offset_reset_output_guard.tests.ps1：用例按修订稿 §12。
- tools/merge_zone/preflight.go、merge_unit_test.go：白名单识别；无表头时按 has-no-active-members 判定，否则报错。
- tools/scripts/stress_summarize.ps1：-Zone 缺省为 0，缺省组仍是裸名，只在 Zone>0 时推导。
新增到 PR-D：
- [modify] docs/design/tidb-phase1-and-launch-readiness.md：L-09 状态改为「已落码，未验证」；§6.3 第 7 步追加更正行。
- [modify] docs/design/shared-redis-split-by-purpose.md：「与 L-09 的关系」段末追加一行。
PR-D 内已有文件，内容有变：
- docs/ops/merge-zone-runbook.md：在原规格的 4 处之外，再按锚点改「`dev_tools.ps1` 已转发 `-kafka-group`」与「-MergeKafkaGroup … 默认从 go/db/etc/db.yaml 现读」两处。
- docs/design/data-consistency-stress-testing.md：改为单 zone 裸名、-Zone N 用 _zN、K8s 用 _z<ZoneId> 的三种口径，不是原规格写的一律 _z<zone>。
- deploy/k8s/README.md（PR-A）：钉版段落加入 V0 的核对命令。
没有删掉任何原规格里的文件。tools/scripts/test_stress_summarize.ps1、go_services.ps1 仍然不改。

## 给实现者的说明
1. 读法：先读原规格，再读复核，最后读本修订稿，冲突以修订稿为准。所有行号作废，一律按函数名或锚点文本定位。HEAD 已到 9f320e485（之后又有 5 个快照提交）。每改一个文件前先跑 git status 与 git diff，按 memory「同一工作区多会话并行」分清哪些 hunk 不是自己的，不覆盖、不回退别人的改动。
2. 不运行任何构建、测试或压测（AGENTS §10.1）。交付说明写「未编译，待 Codex 验证」，并附修订稿 §13 的 V0–V6 命令。V3 涉及拉镜像（属于下载），需要负责人同意。
3. 必须保留 S3（①）已提交的两处：ConsumeClaim 里的 `observeProducerPartition(msg, &dbTask, h.consumer.partitionCount)`，以及 metrics.go 的 db_task_producer_partition_total。S2 的指标与 MustRegister 项追加在它们之后。
4. 关键语义，不要回退：
(a) 最终形状只写 GroupID，交接形状只写 ConsumerGroup 加 GroupHandoffFrom。回档 Step 6、全新 namespace、收尾之后的 keep，永远不渲染交接形状。
(b) evaluate 的 ② 一律等待，没有 own-complete 例外；State 为空串或未知状态时等待，outcome 为 wait_peer_state_unknown。
(c) inspector 任一调用出错就 Close 并丢弃，下次查询用工厂重建；不做 RefreshCoordinator 加重试。
(d) 刚播种的分区不进 settled；no_offsets 分区进 settled。
(e) 不在 ConsumeClaim 里 MarkOffset(claim.InitialOffset())：组没有已提交位点时 InitialOffset 是 -2 哨兵，MarkOffset(-2) 是空操作。
(f) 重置模式下，复核失败的 throw 必须在 Remove-RetiredGroupTopicOffsets 之前。
(g) k8s_deploy 只在非 DryRun 时 kubectl exec 进 kafka-0；Invoke-ClientEntryKubectl 不看 $DryRun，调用方必须自己判断。
5. PowerShell 约定：
- 调用其它脚本一律用 hashtable splatting。
- 捕获原生命令输出时，局部把 ErrorActionPreference 设为 Continue，把 stdout 和 stderr 合并后再交给分类函数。
- k8s_deploy 与 k8s_zone_rollback 的 throw 文案用中文；k8s_zone_rollback 的 Write-Host 进度行沿用英文（e4ff1d165 起的约定）。
- Go 的错误文本用英文，注释用中文。
6. lib/kafka_consumer_groups.ps1 必须是纯函数库：不读脚本级变量，dot-source 时没有副作用。「组不存在」的三个正则在 lib 与 tools/merge_zone/preflight.go 各写一份，注释互相指向，并说明 V3 拿到 4.3.1 原文后要两边同步。
7. 测试夹具：
- k8s_zone_rollback 的沙箱目录结构要改，否则脚本入口读不到 go/db/etc/db.yaml 就 throw。
- dev_tools 的新测试照抄 dev_tools_proto_gen_exit.tests.ps1 的夹具做法（只拷 dev_tools.ps1 和 lib/release_common.ps1），不运行真实的 Kafka CLI。
8. PR 顺序：A → B →（Codex 验证）→ 发布 Phase A → C → 金丝雀 → D。C 的硬性前提只有一条：对某个 zone 做显式 zone|legacy 交接前，该 zone 已在运行 Phase A 镜像。全新 namespace 和回档已经与镜像版本无关。
9. 交付说明里如实列出偏离附录的地方及代价：保留运行时 R7；收尾之后守望不再运行，误部署旧发布物要靠 runbook 禁止，以及游标和排序锁兜底；k8s_deploy 依赖对 kafka-0 的 pods/exec 权限，读失败时按场景 fail-closed，或保持线上形状。
10. 只在 docs/design/ 写设计文档，不新建其它 markdown。PROGRESS 只追加一条，注明 Java 版不适用以及 PARITY 登记行。

## 2026-10-16 更正

（追加于 R7 语义复核 scripts 区域修复之后；代码注释里这批修复记为 2026-10-17。上文原文不改，以下逐条说明哪里已过时。现行口径以代码和 `docs/design/db-consumer-group-per-zone.md` 为准。状态：已落码，未编译、未测试，待 Codex 验证。）

1. **R1 (3)（第 5 行）与 M1（第 50 行）「旧镜像的新 Pod 在 MustLoad 就拒启，rollout 卡住，旧 Pod 继续服务」**：只在线上 db 本身带交接闸时成立。`go-svc-db-config` 整目录挂到 `/app/etc`（`deploy/k8s/manifests/go-svc/db.yaml`，没有 subPath），apply 之后已在运行的 Pod 的挂载内容也会变成交接形状，而进程只在启动时读一次；旧 ReplicaSet 的 Pod 一旦重启，交接闸之前的二进制同样在 MustLoad 拒启并 CrashLoop，两个副本都重启后本 zone 存盘消费全停。代码因此在显式交接渲染交接形状之前加了交接闸能力校验：`tools/scripts/k8s_deploy.ps1` 的 `Assert-LiveDbPodsHaveHandoffGate`，由 `Initialize-DbConsumerGroupPlans` 在非 DryRun、计划 Source=`explicit-handoff` 时调用；它经 API server 代理（`kubectl get --raw /api/v1/namespaces/<ns>/pods/<pod>:9160/proxy/metrics`）抓每个运行中的 db Pod 的 `/metrics`，要求都有 `db_consumer_group_handoff_gate_total`，部署账号需要 `pods/proxy` 权限。这与本稿 R1「不做镜像能力校验」不一致：本稿不采用的是 OCI label、`-capabilities` 子命令与 `git merge-base` 祖先判定，这项校验取证于二进制自己暴露的指标，不在其列。设计文档见 §2.2、§4.3、§10.3。
2. **R1 (4)（第 5 行）「回档里剩下唯一一种『破坏性步骤之后才确定失败』的情形：Kafka CLI 不能为不存在的 zone 组建出位点」**：不止一种。Step 6 的 k8s_deploy 读不到 kafka-0 的位点（缺 `pods/exec`、kafka-0 未就绪）同样只在停服之后暴露。代码新增第 0 步 e：`tools/scripts/k8s_zone_rollback.ps1` 的 `Assert-Step6KafkaFactsReadable`，只在 -Apply 下、Step 1 之前，按 Step 6 的同一条路径（lib 的 `Get-DbGroupDescribeExecArgs` / `Resolve-DbGroupDescribeExecResult`、同一组 `-KubeContext` / `-KubeConfig`、`dev_tools.ps1` 的 `-InfraNamespace` 默认值）读两组位点，并与本机 CLI（`-KafkaBootstrap`）比对有无位点，读不到或不一致都在停服前拒绝。§8 的第 0 步因此是 a–e。
3. **给实现者的说明第 8 条与 §14 步骤 2「C 的硬性前提只有一条：对某个 zone 做显式 zone|legacy 交接前，该 zone 已在运行 Phase A 镜像」**：现由脚本检查（上面第 1 条）。它证明的是线上 Pod，不证明本次 `-GoSvcTag` 指向的镜像。交接 rollout 卡住、旧 Pod CrashLoop 时的收回方法（反方向显式模式落到最终形状）见设计文档 §8.3。
4. **R4 (d)「merge_zone 的唯一权威组判定……不需要读日志边界。空分区在 describe 里本来就不出现行，不影响唯一性」**：论证不完整。合服 T-0 时源区 db 已停、权威组是 Empty，describe 只列出有已提交位点的分区；从没提交过位点、却已有消息的分区同样不出现，其余分区 LAG 都是 0 时合计为 0，P3 会放行。代码在 LAG 合计为 0 之后加了全覆盖检查：`tools/merge_zone/preflight.go` 的 `kafkaCLILagSource.TotalLag` 用 `kafka-get-offsets --time -2 / -1` 取日志边界，交给 `verifyAuthoritativeCoverage` 逐分区核对（earliest 与 latest 分区集合相同；describe 里出现的分区 get-offsets 都有；每个非空分区都有数值 CURRENT-OFFSET 且该分区 LAG = 0），规则与 lib 的 `Test-HandoffFinalizeReady` 相同。`kafka-get-offsets` 由 `kafkaGetOffsetsCmdFor` 从 `-kafka-consumer-groups-cmd` 按同目录、同扩展名推导，执行机必须两者齐备。唯一性判定本身不变。
5. **R4 (a) 与 §6 `Test-HandoffFinalizeReady`「对每个 p∈0..N-1」**：现在还会对分区号 ≥ N 的分区（出现在 get-offsets、新组或旧组位点里）报问题：`--delete-offsets --topic` 会删掉旧组在全部分区上的位点，-Partitions 小于实际分区数时超出部分没被检查。口径与 `Get-ResetPlanProblems` 相同。
6. **R15 (1) 与 §6 `Resolve-KafkaGroupTopicOffsets`「命中『组不存在』→ missing」、§10 merge_zone 的白名单识别**：现在白名单只在 describe **没有表头**时才认定组不存在；命中白名单又列出本 topic 的数值位点，按矛盾输出 throw / 报错；命中白名单、有表头但没有数值位点，交给常规判定。PowerShell 侧在 `Resolve-KafkaGroupTopicOffsets`，Go 侧新增 `groupMissingInDescribe`，`committedOffsetPresent` 与 `resolveAuthoritativeLag` 都经它判定。
7. **R8 与 §7 的 `-DeleteAndRecreateTopic`（「ReplicationFactor ≠1 才放」）**：`kafka_offset_reset.ps1` 现对 db_task topic（lib 新增 `Test-DbTaskTopicName`，`^db_task_zone_\d+(_g\d+)?$`）一律拒绝同名重建，改走 `TopicGeneration` 换代；`-ReplicationFactor` 改为必填（0 = 没给）且须等于现有副本数，`dev_tools.ps1` 的 `-KafkaReplicationFactor` 缺省 0、大于 0 才透传；删除前先 describe（`ConvertFrom-KafkaTopicDescribe`），读不到或分区数 / 副本数不一致不删；删除后 `Wait-KafkaTopicDeleted` 按单调时钟最多等 30s，`--list` 失败或超时即 throw、不 create。
8. **§7 重置模式与 §8 Step 5 的退出码**：重置模式的成功路径现以 `exit 0` 结束，`Invoke-KafkaToolCaptured` 在 finally 里把 `LASTEXITCODE` 复位为 0。本稿没有规定这一点；不这样做时，「组不存在」的 describe 以非 0 退出会让回档 Step 5 在 PITR / FLUSHDB 之后把成功判成失败。
9. **§8「入口处用 Get-YamlScalar 读 … GroupID … 和 PartitionCnt」**：入口现在还读 `ServerConfig.Kafka.TopicGeneration`（非负整数），用 `Get-DbTaskTopicName` 推导 topic；显式 `-KafkaTopic` 与推导值不一致即拒。本稿没有提 topic 世代号。
10. **回档的 Kafka 语义**：`kafka_offset_reset.ps1` 原注释「Step 4 已 FLUSHDB 清掉游标」的前提在 K8s 上不成立（游标在共享 DB0，Step 4 的防护必拒），已更正注释；`k8s_zone_rollback.ps1` 文件头 Step 5 写明了已知问题。「从 TargetTime 起重放」在 K8s 上并不成立，「丢弃 TargetTime 之后的写」也不完整。两个互斥方案（丢弃：Step 5 改 `-ToLatest` 并拦截本 topic 的重试收据；灾难恢复重放：Step 5 之前只清本 topic 的游标，并在第 0 步校验 TargetTime 不早于各分区日志起点）**待负责人裁决**，行为没有改。见设计文档 §6 末尾与 `docs/design/zone_data_rollback.md`「2026-10-17 修订」。（2026-10-18 补：负责人 2026-10-17 已裁决「丢弃 TargetTime 之后的写」，脚本已按丢弃方案落码，未编译、未测试；见设计文档 §6 末尾「2026-10-18：已裁决并落码」与 `docs/design/zone_data_rollback.md`「2026-10-18 修订」。）
11. **§3.3 exec 命令**：设计文档 §4.3 已记录改成 `/bin/sh -c` 加小堆；现在命令与结果分类搬进 lib（`Get-DbGroupDescribeExecArgs` / `Resolve-DbGroupDescribeExecResult`），`Read-DbGroupTopicFacts` 与回档第 0 步 e 共用。
12. **§13 V3 追加**：(k) 执行机上 `kafka-get-offsets(.sh/.bat)` 的位置与输出格式；(l) 部署账号的 `pods/proxy` RBAC 与 API 代理抓 `/metrics` 的原始输出；(m) 「组不存在」文案与表格同时出现的矛盾输出在 4.3.1 上是否真会出现。见设计文档 §11。
