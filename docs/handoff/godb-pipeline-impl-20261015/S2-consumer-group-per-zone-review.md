# S2 对抗复核意见（结论：needs_fix）

## [high] F5 改成「旧二进制见到新形状就拒启」之后，回档目标镜像早于 Phase A 时，回档会在破坏性步骤全部做完之后才失败，而且没有脚本化的恢复路径。
- 证据：k8s_zone_rollback.ps1:771-777 的 $upArgs 只传 NodeImage；go-svc 的 tag 跟随 NodeImage 的 tag（k8s_deploy.ps1:398-403 FollowTag）。第 0 步的静态预检 :839 只是 k8s_deploy -DryRun，不检查镜像能力。规格让 Step 5 先重置 zone 组、再删 legacy 组在本 topic 上的位点，Step 6 再强制按 zone 形状部署。于是旧 db 二进制在 Step 6 CrashLoop，而这时已经停服、做完 PITR、FLUSHDB，legacy 位点也删了。附录 L210（F5）要求的是：回档 Step 6 由守卫校验目标镜像带交接能力，没有就 throw。
- 修法：二选一。(1) 第 0 步、停服之前判定能力：release tag 就是 git sha，用 git merge-base --is-ancestor <PR-B 合入 commit> <tag sha> 判断，不满足就 throw。(2) 推荐：回档 Step 6 改为渲染旧形状 `GroupID: "db_rpc_consumer_group_z<id>"`，不写 GroupHandoffFrom。Step 5 已经删掉了 legacy 位点，新旧二进制都能从刚重置的 zone 组位点续消费，这也是附录原方案「写字面量组名」本来就有的回退兼容性。ConvertFrom-LiveDbConsumerGroup 已经接受这种形状。补一条契约测试：回档 Step 6 的 ConfigMap 必须能被旧结构加载。

## [high] inspector 用 NewClusterAdmin 建出的 admin 永远不刷新 coordinator；同时 seed 在每次 Setup 都通过它查本组位点，查失败就让 Setup 报错。一次 broker 重启让 coordinator 迁移之后，这个 zone 的存盘消费会一直停到 Pod 重启。
- 证据：sarama v1.43.1 admin.go:910-947 的 DescribeConsumerGroups 和 :1009-1018 的 ListConsumerGroupOffsets 都直接用 ca.client.Coordinator(group) 的缓存，遇到 NOT_COORDINATOR 不调 RefreshCoordinator。按规格：seed 先查本组位点，出错时计 aborted 并返回 error；Setup 每次都调 seed；Setup 失败后，消费循环（key_ordered_consumer.go:1042-1045）sleep 1s 重新入组，然后又失败。sarama 消费者自己的 client 会刷新 coordinator，但 inspector 用的是另一个 client。多 broker 档每次滚动重启（包括 PR-A、S5 的变更）都会迁移 coordinator。
- 修法：(1) inspector 改为持有 sarama.Client，经 NewClusterAdminFromClient 派生 admin（go/shared/kafkautil/group_lag.go:122-145 的 GroupLagProber 就是这种做法），遇到 ErrNotCoordinatorForConsumer、ErrCoordinatorNotAvailable 或网络错误时先 RefreshCoordinator(group) 重试一次。(2) seed 只在本进程交接后的第一个 session 查询；之后的 Setup 跳过 admin，本组位点由 sarama 的 offset manager 自己负责。(3) 补单测：fake inspector 先返回 NOT_COORDINATOR、刷新后成功。

## [high] 附录 F2 要求反向交接前、目标组仍有位点就由脚本 throw，规格没有实现，只靠运行时 R7。结果是「反向时还没收尾」会在滚动中途让整个 zone 停止消费。
- 证据：附录 L207（F2）。规格 F2 的 how_addressed 只写了「由进程内 R7 实现」。db.yaml:32-38 是 maxSurge 1 / maxUnavailable 0，readiness 不挂在交接闸上，所以滚动照常杀掉 zone Pod。最后一个 zone Pod 退出后，R Pod 的 own（legacy 冻结位点）小于 peer（zone 组），一直处于 wait_own_behind_peer，消费为 0，login 的 TaskWaitTimeout 30s 全部超时。金丝雀刚切完、还没收尾就发现问题需要反向，正是最可能出现这种情况的时候。
- 修法：k8s_deploy 在 -DbConsumerGroup zone|legacy 且非 DryRun 时，在任何写操作之前对目标组做一次 describe（kubectl exec kafka-0 -- kafka-consumer-groups.sh，或本机 CLI，复用 lib/kafka_consumer_groups.ps1）。目标组在本 topic 上有数值位点、且有分区小于对方组时 throw，并给出 kafka_offset_reset -RetireGroup <目标组> 的命令。目标组此时没有订阅者，也可以改为自动先删再部署。R7 保留，作为兜住手工部署的纵深防御。

## [medium] 从来没有消息的空分区永远不会有已提交位点，规格里依赖「本组 N 个分区都有数值位点」的几处因此全部失效：FinalizeHandoff 的前置检查永远不过；② 永远判定本组不完整（B3 残留）；seed 每次再平衡都要查 legacy 组；merge_zone 因为两组都有位点而一直拒绝合服。
- 证据：sarama 只提交 dirty 的 POM，no_offsets 的分区不会被 MarkOffset，所以一直不提交。规格中的 Test-HandoffFinalizeReady 要求 0..N-1 都有数值 CURRENT-OFFSET；evaluate ② 要求本组全部分区 ≥0；seed 只要存在 own<0 的分区就查对方，出错就返回 error，而 Setup 失败后重新入组会让本 zone 组反复再平衡；F7 规定两组都有位点时 fail-closed。人少的 zone，10 个分区里有空分区很常见。
- 修法：在 ConsumeClaim 开始处（或 seed 之后，针对 no_offsets 的分区）：本组没有位点时调用 session.MarkOffset(topic, p, claim.InitialOffset(), "")，把 sarama 解析出的 Oldest 起点提交为本组位点，这样不会跳过消息。FinalizeHandoff 的前置检查对「两组都没有位点、并且 LOG-END-OFFSET 等于日志起点」的分区放行。rollout 与 V5 里的 seeded=10 改为 seeded 加 no_offsets 等于 10。

## [medium] ② peer_unreachable_own_complete 排在 R7 之前，对方查询失败时会直接跳过 R7，等于在附录 B2 的场景里 fail-open。
- 证据：规格 evaluate 的顺序是 ② 在 ④（R7）之前，risks 第 5 条自己也承认会跳过 R7。反向时还没收尾：own（legacy）在全部分区上都有冻结位点，算作「完整」；只要 zone 组的 coordinator 一时不可达就放行，从冻结点开始大段重放。附录 L199：分区 FIFO 等于停服，读回写缓存与其它分区上的写并发（PROGRESS.md:7155-7157）。
- 修法：② 改为等待（fail-closed）。为了不重新引入 B3，可以把 ② 限定为「对方组在本进程内曾被确认为没有本 topic 位点（Dead 或已收尾）」才放行；或者按 F3 原意，收尾后改为渲染不带 GroupHandoffFrom 的形状，从根上去掉对 legacy 的依赖。

## [medium] F4「ConfigMap 不存在时以 Kafka 为准」没有实现：全新 namespace 一律渲染 zone 形状。zone-down 再 zone-up 会静默撤销运维有意做的反向交接；如果那次反向还没收尾，zone-up 之后会被 R7 停住。隐式交接完成后也不提示需要收尾。
- 证据：规格的 Resolve-DbConsumerGroupPlan 在 keep 且 LiveDbYaml=null 时返回 zone 形状；k8s_deploy.ps1:6325-6331 的 Remove-Zone 直接删 namespace。附录 L209（F4）要求查 Kafka，两组都有位点就 throw。legacy zone 经 zone-down/zone-up 隐式切组后，legacy 冻结位点还在，merge_zone 会因为两组都有位点一直拒绝，而部署输出只有 source=fresh-namespace 一行。
- 修法：把每个 zone 的组选择持久化到 zone namespace 之外：zones 配置加 dbConsumerGroup 字段，或在 infra namespace 放一个登记 ConfigMap，每次 zone-up 写入。keep 读不到 zone ConfigMap 时回落到这份登记，登记为 legacy 时要求显式传 -DbConsumerGroup。source=fresh-namespace 时额外打印需要 -FinalizeHandoff 收尾的命令。

## [medium] 回档 Step 5 先重置、后 retire，但规格没有在重置之后复核位点，就去删 legacy 位点，附录 L151「-Apply 之后再 describe 一次核对位点」被漏掉了。对从未切组的 legacy zone，zone 组还不存在，--reset-offsets --to-datetime 能不能建出位点也还没核实（V3(c)）。
- 证据：kafka_offset_reset.ps1:220-227 在 -Apply 之后只打印 OK 和一条供手工核对的命令。规格的重置模式在 -Apply 成功后直接调 Remove-RetiredGroupTopicOffsets。如果重置静默无效，retire 之后两组都没有位点，Step 6 会从 Oldest 重放整个保留期，而 Step 4 已经 FLUSHDB 清掉了游标（F8(d)）。
- 修法：重置模式加 -Apply 时，先 describe -Group，断言 0..N-1 都有数值 CURRENT-OFFSET（或与 dry-run 计划的 NEW-OFFSET 一致），不满足就在 retire 之前 throw。Remove-RetiredGroupTopicOffsets 只在复核通过后执行。kafka_offset_reset 的测试补「重置后组仍不存在 → throw、且不调 delete-offsets」。

## [medium] dev_tools 的 kafka-offset-reset 分支用的是数组 splatting，按位置绑定，现在就不可用。规格在这个数组上追加 -RetireGroup / -FinalizeHandoff，F6 要求的「dev_tools 本地回档路径也调用」实际上跑不通；契约测试只做源码断言，测不出来。
- 证据：dev_tools.ps1:1548-1562：`$kArgs = @("-BootstrapServer", ...)` 然后 `& ... @kArgs`。k8s_zone_rollback.ps1 Step 5 的注释（:944-948 附近）记录的正是同一个 bug。本地用 pwsh 核对过：数组 splat 时 "-Topic","t1" 里的 t1 被绑到 [int]$Partitions，报类型转换错误。
- 修法：在 PR-C 里把该分支改成 hashtable splatting（与 k8s_zone_rollback Step 5 相同）。dev_tools_merge_zone_contract（或新增用例）用假的 kafka_offset_reset.ps1 记录 $PSBoundParameters，实际执行一次 dev_tools -Command kafka-offset-reset，断言 Group / RetireGroup / FinalizeHandoff / Partitions 都按名字绑定正确。

## [medium] 规格的 current_state 与仓库现状不符，与别的会话的冲突面也漏报：复核期间又落了两个提交，另有一个不在 S1–S5 之列的会话正在改 go/db 的 ConsumeClaim 和 metrics。
- 证据：复核开始时，k8s_deploy.ps1、k8s_deploy_contract.tests.ps1、go/login/internal/config/config.go、go/login/etc/login.yaml、deploy/k8s/README.md、docs/PROGRESS.md 都有未提交改动，与「git status 都干净」不符；merge-zone-runbook.md 那 1 行已在 ab0f27907 提交。复核期间 a7c510d65、592d7e718 改了 k8s_deploy.ps1、contract 测试、go_services.ps1、stress_summarize.ps1、go/login 配置、README。当前工作区里 go/db/internal/metrics/metrics.go 有未提交的 +44 行（db_task_producer_partition_total），还有未跟踪的 go/db/internal/kafka/producer_partition_audit.go（在 ConsumeClaim 解码后做观测）。ordering_and_overlap 没有列出这个会话。行号全部漂移：db 的 GroupID 在 :4173，不是 :4100；login 在 :4445；New-GoSvcConfigMapYaml 在 :3723；Apply-OneGoSvc 在 :5586；Remove-Zone 在 :6325；写路径预检在 :6842；stress_summarize 在 :84-85；README 在 :1223/:1238；login config 在 :459；login.yaml 在 :496。
- 修法：动手前重新执行 git status / git diff，按 memory「同一工作区多会话并行」逐个区分他人的 hunk。ordering_and_overlap 补上「db_task 分区契约观测」会话：ConsumeClaim 的解码分支、metrics.go 的 register 都与 S2 重叠，约定合并顺序和挂点。规格里的位置一律改用函数名或锚点文本，不用行号。

## [medium] 本地默认单 zone 启动时组名仍是裸的 db_rpc_consumer_group：go_services 只在 -Zone>0 时加 _z。规格据此做的几项判断和改动有误：stress_summarize 把默认组改成 _z1，会让 1-zone 压测的 Kafka 段查一个空组；data-consistency 文档改成 _z<zone> 也不对。
- 证据：go_services.ps1:71 的 `[int]$Zone = 0`；:425 起的 `if ($Zone -gt 0) {` 块里才在 :452 替换 GroupID 后缀。dev_tools.ps1:194 的 Zone 缺省 0；start_game.ps1:341 的 go-svc-start-exe 不带 -Zone。历史 1-zone 压测（stress-1zone-25k-2026-05-28-deep-dive.md:16）记录的是 GROUP=db_rpc_consumer_group。规格 current_state 第 2、5 条的说法「本地与压测一直是每 zone 一组」「本地合服门禁一直在查不存在的组」只对 dev-start-zones 成立。
- 修法：stress_summarize 不传 -Zone 时保持裸名缺省，传了 -Zone 才推导 _z$Zone，或者两个组都 describe、取有本 topic 行的那个。data-consistency-stress-testing.md:216 写成「单 zone 默认启动为裸名，-Zone N 启动为 _zN」。更正规格与设计文档 §1 的现状描述。merge_zone 的候选组方案本身能覆盖这两种情况，不用改。

## [low] 重复造轮子：go/shared/kafkautil 已经有基于 sarama 的位点读取，带严格的错误语义；规格的 saramaGroupInspector 另写了一份。
- 证据：go/shared/kafkautil/group_lag.go:31-33 的 groupOffsetReader（ListConsumerGroupOffsets）、:47-75 的 computeGroupLag（顶层 Err / block 缺失 / block.Err 都 fail-closed）、:122-145 的 GroupLagProber（带网络超时的 Client 加 NewClusterAdminFromClient）。go/db 已经依赖 kafkautil（db.go 的 EnsureTopics）。AGENTS §11.2 DRY，§11.5 第 2 条要求先找现有实现。
- 修法：在 kafkautil 抽出 CommittedOffsets(admin, group, topic, partitions)，供 group_lag 与 handoff 共用；inspector 复用 GroupLagProber 的建链方式，这也正好修掉上面 coordinator 不刷新的问题。

## [low] evaluate ⑥ 把 State 为空串当作 peer_released，遇到异常响应时等于 fail-open。
- 证据：规格 evaluate ⑥ 写的是「Empty、Dead、空串或 Stable（Held 已为空）→ 放行」。空串只会来自不正常的响应，这与「只凭正面证据」的原则矛盾。
- 修法：空串和未知状态都归入 ⑦ 等待（wait_peer_rebalancing，或单列 wait_peer_unknown）；只有 GROUP_ID_NOT_FOUND 映射为 Dead。

## [low] 「钉版不改变现网版本」只有 Docker Hub 上 tag 对比这一条依据，没有核对正在运行的 broker 和本机缓存的镜像。
- 证据：K8s 的 broker Pod 拉到的是它创建时的 latest；本机 compose 用的是本地缓存的 latest（PROGRESS.md:8236 记录过某次 pull），可能早于 4.3.1，而命名卷里是旧版本写的日志目录。
- 修法：V3 增加两项：kubectl get pod -n <infra> -l app=kafka -o jsonpath='{..imageID}'，以及 docker image inspect apache/kafka:latest --format '{{json .RepoDigests}}'。两者都应等于 sha256:77e3…，不一致就按一次 broker 升级来评估（KRaft metadata.version 与回滚）。

## [low] 文档同步有遗漏。
- 证据：merge-zone-runbook.md:984「dev_tools.ps1 已转发 -kafka-group」与 :1028「-MergeKafkaGroup 默认从 db.yaml 现读」改动后不再成立，PR-D 只改了 :102/:103/:266/:487。tidb-phase1-and-launch-readiness.md:844 的 L-09（本项就是 L-09）、:374、:801，以及 shared-redis-split-by-purpose.md:686 仍按共享组描述。
- 修法：PR-D 补上这几处：只追加更正行，或更新 L-09 的状态列（已落码，未验证）。

## [low] 「组不存在」的识别只认一种文案，Kafka 4.x 可能输出别的文本，到时 merge_zone 会在 legacy 组不存在时一律拒绝。另外规格自身有一处矛盾：Setup 总是创建 yieldCtx，又有单测断言没有 handoff 时 sessionScope 退回 session ctx。
- 证据：规格的 consumerGroupMissing 与 ConvertFrom-KafkaConsumerGroupDescribe 都只匹配 `consumer group '...' does not exist`；新 coordinator 可能输出 GroupIdNotFoundException 一类文本（V3(b) 尚未实测）。规格 changes 第 (5) 项说先 cancel 再建 yieldCtx，并没有写「仅在 handoff 非空时」；而 TestConsumerSetup_NoHandoffKeepsLegacyBehavior 断言 sessionScope 退回 session ctx。
- 修法：等 V3(b) 拿到 4.3.1 的原文后再定识别规则，并把原文写进测试数据。明确 Phase A 下 handoff 为 nil 时不创建 yieldCtx，或者改那条单测的断言，两者取一。

## 漏掉 / 似是而非的阻断项
- F5：回档 Step 6 与全新 namespace 部署前没有校验镜像能力，改成了运行时拒启；回档到 Phase A 之前的镜像时，要等 PITR、FLUSHDB、删除 legacy 位点都做完才失败
- F2：反向交接前「目标组仍有本 topic 位点就由脚本 throw」没有实现，只靠运行时 R7，滚动中途会让该 zone 停止消费
- F4：ConfigMap 不存在时以 Kafka 判定组归属、两组都有位点就 throw，没有实现；全新 namespace 一律渲染 zone 形状
- F3：收尾后去掉 GroupHandoffFrom 没有做；seed 每次 Setup 都查 admin，coordinator 不刷新，再加上 ② 跳过 R7、空分区永远没有位点，B3 跨 zone 共享故障点仍然存在
- 附录 L151：重置模式在 -Apply 之后 describe 复核位点没有纳入，回档 Step 5 在没复核的情况下就删除 legacy 位点
- F6：dev_tools 本地回档 / 重置路径的透传依赖已经坏掉的数组 splatting，实际不可用
