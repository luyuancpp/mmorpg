# S1 实现规格：S1 ④ 重试只交给分区属主(a)(c) + 重试/死信/毒消息入队补 WAITAOF + 修订 TiDB 决策 D5

## 摘要
在 go/db 引入「claim 租约表」(ConsumeClaim 进出即登记/摘除,属主判据只认 sarama claim 是否存活),把重试 ready 拆成共享收件箱 kafka:retry:queue:{topic} + 按原始分区的 kafka:retry:queue:{topic}:p{N};新增开关 ServerConfig.Kafka.RetryRouting(shared|owner,环境变量 DB_RETRY_ROUTING 可覆盖)。shared = 今天语义 + 兜底排空 p*;owner = 每拍先用单条 Lua 把收件箱按载荷里的原始分区分拣进 p{N}(坏载荷进死信),再只对本进程存活的租约从 p{P} 认领执行,任务带 claimCtx 与 inflight 回执,失败写回 p{origin}。handleTask 入口与 SQL 之前两次复核 claim,撤销则原样搬回 p{origin}、不执行不耗预算;重试 SQL 仍用 w.ctx。ConsumeClaim 排空在同一 10 秒截止内等租约 inflight 归零(互斥锁+计数+归零 channel,不用 WaitGroup)。新增 go/db/cmd/retry_requeue 搬回工具(p*→共享,回滚用)、3 条路由告警、k8s_deploy 写路径入口的 replicas ≤ PartitionCnt 门禁(go/db 运行期不拒启,多出副本为热备)。WAITAOF:三处「Kafka 位点提交前任务唯一副本写进 Redis」的入队点(Kafka 源失败入重试、Kafka 源隔离进死信、毒消息持久化)统一走 durableEnqueue;开启时用专用 go-redis 客户端做组提交(一批 LPUSH + 一次 WAITAOF,同一 pipeline=同一连接),确认不足/超时 fail-closed(不 ack、位点不提交、消息重投,幂等靠 seq 游标);启动探针失败拒启;配置 ServerConfig.RedisClient.WaitAOFNumLocal/NumReplicas/TimeoutMs,默认 1/0/1500,两项写 0 关闭。D5 以追加修订块方式改为「④ 与 WAITAOF 允许改,但必须独立发布、不与 TiDB 迁移同窗口;topic/partition 契约不变;B2 仍需先落条件写并另行修订」。本仓库改 28 个文件(新增 6),外加 Java 仓库 PARITY.md 一行;未编译、未测试,构建与测试全部交 Codex。

## 现状
基线 HEAD 6281e4d27,go/db 工作区无他人未提交改动(git status 只有 docs/PROGRESS.md、docs/README.md、k8s 相关少量别处改动,改这些共享文件时只追加)。
【重试不绑属主】
- go/db/internal/kafka/key_ordered_consumer.go(下称 KOC):构造期为契约内全部分区建 worker(KOC:946-948),Start 全部拉起并无条件启动重试消费(KOC:1030-1038)。consumeRetryQueue(KOC:1155-1178)/consumeOneRetryTask(KOC:1182-1253)从共享 ready kafka:retry:queue:{topic}(KOC:907)认领,按载荷 v2 头里的原始分区派给本地 worker,不检查本进程是否持有该分区;重试 workerTask 不带 claimCtx(KOC:1238-1245)。
- handleTask 入口的 claim 检查只调 failKafkaTask(KOC:1559-1566),而 failKafkaTask 对重试任务是空操作(KOC:315-320):若只给重试挂 claimCtx 不改这里,收据会永远卡在本实例 processing。
- processCtx:Kafka 任务用 claimCtx(KOC:1642-1645);但读写 handler 都把 ctx 丢掉(handleDBReadOp `_ context.Context` KOC:343-353;handleDBWriteOp KOC:814-837,store.Save 不接 ctx),claimCtx 只影响 Redis 步骤(GetExpandStatus KOC:1852、TryLock、route.confirm placement_route.go:243、publishDBOpResult)。
- 成功路径:verify(KOC:1662-1667)→ markAppliedSeq(KOC:1670)→ markAppliedOwnerEpoch(KOC:1674)→ ack 收据(KOC:1677-1683)→ ackKafkaTask(KOC:1685)。
- ConsumeClaim(KOC:1724-1840):claimCtx = WithCancel(session.Context())(KOC:1732);drainAfterStop 与消息通道关闭路径各等 acker 最多 claimDrainTimeout=10s(KOC:289-300、1736-1742、1753-1771)。Setup 只打印 session.Claims()(KOC:1692-1709),无分区时打 ERROR(KOC:1706)。sarama v1.43.1(go/db/go.mod),未设分配策略 = Range + eager;任一 claim 退出即取消整个 session,新属主要等全员退出上一代 claim 才拿到分区。结论:「本实例当前拥有哪些分区」不能用 Setup/Cleanup(Setup 早于 ConsumeClaim 起、Cleanup 晚于排空),要以每次 ConsumeClaim 调用的 claimCtx 存活为准。
- retry_ownership.go:收据按实例 processing + 租约 ZSET(:16-39、:63-70),认领脚本先续租再 RPOPLPUSH(:117-121、claim :182-194),回收/归还/旧共享列表一律搬回共享 ready(:126-148、:220-235、:351-361),回收跳过自己(:271-273)。只解决「收据归谁」,不解决「谁有资格执行」。
- store_failure.go:150-165 sampleRetryQueues 只采 ready/processing/dead 三个列表;placement_route.go:381-392 retryBudgetExempt(排序锁忙/冻结/落点变化/存储类错误对写豁免)不变。
- 部署:deploy/k8s/manifests/go-svc/db.yaml:34 replicas 2、maxSurge 1(:36-41),注释 :29-30;所有 zone 共用组 db_rpc_consumer_group(tools/scripts/k8s_deploy.ps1:4100);Apply-OneGoSvc(k8s_deploy.ps1:5511)只 apply ConfigMap+Deployment,go-svc Pod 模板没有 ConfigMap 内容指纹(该函数内 login 削峰注释自认「go-svc 的 ConfigMap 都没有内容指纹」),只改 ConfigMap 不会滚动。写路径门禁集中在 k8s_deploy.ps1:6756-6778;现成 Get-ManifestDeploymentReplicas(:1190-1202)。
- tools/merge_zone/preflight.go:182-200 dbTaskQueueKeys 只含 ready、旧共享 processing、dead、登记表里每实例 processing;audit_checks.go:159 复用它。merge_zone 没有 miniredis(gap_fixes_test.go 头注释),go-redis v9.7.3。
- deploy/k8s/db-task-retry-alerts.yaml:100-112 DbTaskRetryQueueGrowing 只看 queue="ready"。
【WAITAOF】
- 三个「位点提交前、任务唯一副本写进 Redis」的入队点全部经 pushPayloadDurably(KOC:2247-2264,只 LPUSH,固定 4 次重试,无落盘确认):① deferFailedTask Kafka 分支 saveToRetryQueue(KOC:2152-2166,随后 ackKafkaTask);② quarantineOrderingConflict Kafka 分支进死信(KOC:2199-2215;调用方 handleTask orderingIncomparable KOC:1618-1621、routeTask stale_topic placement_route.go:287-291、missing_required 写 :299-303);③ persistPoisonMessage(KOC:2218-2229;ConsumeClaim 分发前解码失败 KOC:1777-1791、quarantinePoisonTask KOC:2231-2245)。重试源的转移(moveRetryReceipt KOC:1091-1111、deadLetterRetry、moveClaimedRetryToDead、ackRetryReceipt、回收/归还)都是单脚本原子搬移,前一状态本身就在 Redis 里,不是「唯一副本首次落地」。
- go/db 的 Redis 客户端(go/db/internal/svc/servicecontext.go)只设 Addr/Password/DB/DisableIndentity:go-redis 默认 ReadTimeout 3s、PoolSize 10×GOMAXPROCS;db Pod CPU limit 500m(db.yaml manifest resources)→ GOMAXPROCS=1 → 池 10 条,且排序锁、游标、落点 MGET(K8s 上同实例)共用此池。WAITAOF 在 everysec 下每次占连接平均约 0.5s,直接放进主池会在 MySQL 故障(L-04 后每条写都走入队)时饿死整条热路径。
- 现成做法:go/data_service/internal/routing/route_publish.go:298-359(同一 pipeline 发 EVAL + p.Do("WAITAOF"),waitCmd.Slice()),checkWaitAOFReply :448-461,waitAOFBudgetMs :466-478(timeout 0=无限阻塞,<1ms 不发),ProbeWaitAOF :393-423;internal/config/config.go:268-404(*int 区分没写与写 0、Normalize 填默认、Validate 拒启,默认 1/0/1500)。go-redis v9.18.0 的 WaitAOF 封装返回 *IntCmd(commands.go:297),数组回复会解析错;Cmdable 接口无 Do 但有 Pipelined(commands.go:173-178);管道读超时用整条 ReadTimeout,go-redis 对超时/EOF 会整条 pipeline 重发(redis.go:1006-1031、error.go:79-106)。
- Redis 版本与 AOF:K8s redis.yaml:83 redis:7.2、:95/:97 --appendonly yes;redis-ha.yaml:684 redis:7.2、:252 appendonly yes;compose docker-compose.yml:217-235 redis:latest --appendonly yes。miniredis v2.36/2.37 有 WAIT、无 WAITAOF(cmd_generic.go:59),可经 mr.Server().Register 注册假命令。
【D5】docs/design/global-data-layer-tidb-decision.md:152-159「retry/dead 队列……全不动」。

## 复核阻断项 / 修正的处理
- **A4-B1**：复核阻断项 B1:第 3 步在生产金丝雀做「暂停 MySQL 30 秒 + kill -9 db Pod」违反不停服(全服共用一台 MySQL、共享消费组 session 超时后全组 eager 再平衡)
  - 处理：仍有效。rollout 第 3 步改为生产只被动看指标;MySQL 暂停、kill -9、回滚演练全部挪到本机/kind/staging(由 Codex 执行)。生产若需造积压,只允许对合成测试玩家持有带 TTL 的排序锁 distributed:lock:kafka:ordering:db_task_zone_<z>:<测试pid>:<MsgType>(errOrderingLockBusy 属免预算错误,placement_route.go:383,不进死信)。S2 拆组之后 kill -9 只影响本 zone,但 MySQL 暂停仍是全服,故规则不变。写进 docs/design/db-retry-owner-routing.md §10。
- **A4-B2**：复核阻断项 B2:改 db.yaml → k8s_deploy 重新生成 → 不会滚动,第 4 步切换/回滚与调 SubShardCount 都不生效
  - 处理：在 HEAD 仍成立(Apply-OneGoSvc k8s_deploy.ps1:5511 起只 apply,go-svc 无 ConfigMap 指纹)。rollout 第 4 步与所有回滚都写明 `kubectl -n mmorpg-zone-<z> rollout restart deploy/db` + `kubectl -n mmorpg-zone-<z> rollout status deploy/db`;验收以每个 Pod 启动日志 `retry routing: mode=… source=…` 与新指标 db_retry_routing_mode{mode} 为准(本项新增该 gauge 正是为此)。金丝雀用 `kubectl set env` 改 Pod 模板,天然触发滚动。ConfigMap 指纹注解不在本项(另算文件数)。
- **A4-B3**：复核阻断项 B3:第 2 步「等价于今天的多实例」依赖未验证基线(be6b4ab1d 从未编译运行);若线上仍是 10-08 前单副本旧版,新版每拍搬空旧共享 processing 会让旧实例在途收据重复执行
  - 处理：仍有效(placement-reverify-checklist-20261012.md:23、:456 仍写 be6b4ab1d 无运行记录)。rollout 前置门禁:① Codex 先跑通复验清单第 12 步「本机双 db 实例」与第 3 节缺口项;② 发布前查清并记录线上镜像与副本数(`kubectl -n mmorpg-zone-<z> get deploy db -o jsonpath='{.spec.replicas} {.spec.template.spec.containers[0].image}'`);③ 线上低于 be6b4ab1d 时按瓶颈文档 §4.5 走离线流程或先单独发布 HEAD 基线,不与 S1 同批;④ 不得把「副本 1→2」与 S1 合并为同一次发布。
- **A4-B4**：复核阻断项 B4:回滚到批次 1 之前的镜像会滞留 p*,只靠 runbook 文字兜底不够;搬回工具须作为代码交付并有测试,第 3 步前就绪;加告警「p* 有积压但没有 owner 实例在消费」
  - 处理：新增 go/db/cmd/retry_requeue(-check 只读盘点;默认执行分批 Lua 搬回,可重入、只搬 v2 载荷,非 v2 进死信)+ internal/kafka 的 RequeuePartitionedRetries/RetryQueueDepths 与单测。另有两层自动兜底:shared 模式的新版二进制每拍兜底排空 p*(回滚首选路径);告警 DbRetryPartitionedNoProgress(p* 15 分钟不空且无 executed)。如实记录残余:实例全部回到旧镜像后没人上报 partitioned,告警哑火,所以回滚规程把「先切 shared 并确认 p*=0(retry_requeue -check)」设为回滚镜像的前置,紧急回滚后立刻跑搬回工具。
- **A4-F1**：复核修正:第 3 步生产金丝雀只看指标不注入,注入挪到 staging/kind;生产积压只允许合成测试玩家持锁
  - 处理：同 A4-B1,写入 rollout 与设计文档 §10。
- **A4-F2**：复核修正:第 4、5 步及回滚写明真正触发重建的动作(rollout restart + rollout status),以启动日志与 db_retry_route_total{mode} 验收
  - 处理：同 A4-B2;验收另加 db_retry_routing_mode{mode} 与 db_owned_partitions。第 5 步(B0 调 SubShardCount)本轮不做。
- **A4-F3**：复核修正:把批次 1 前置门禁写进方案:复验清单双 db 实例项跑通、写明线上镜像与副本数、不把副本 1→2 与批次 1 合并
  - 处理：同 A4-B3;另对应交接 §4.1 第 7 行「前置:复验清单的双 db 实例项跑通」,列为 Codex 验证项 rollout 第 0 步。
- **A4-F4**：复核修正:p* 搬回工具作为代码交付并配测试;至少三条告警(p* 积压持续、owner 下 executed_nonowner>0、非再平衡时段 requeued_revoked);runbook 写紧急回滚顺序
  - 处理：工具见 A4-B4。告警加进 deploy/k8s/db-task-retry-alerts.yaml:DbRetryPartitionedNoProgress、DbRetryNonOwnerExecutionInOwnerZone(zone 内已有 owner 实例时仍有人执行非属主重试 = 混跑/配错;按构造 owner 实例自身不会计 executed_nonowner)、DbRetryRevokedOutsideRebalance(requeued_revoked 增长而 db_owned_partitions 10 分钟内无变化);另加 DbDurableEnqueueNotConfirmed(WAITAOF),并把 DbTaskRetryQueueGrowing 改为 ready+partitioned。紧急回滚顺序写进设计文档 §10。
- **A4-F5**：复核修正:更正「附带发现」与执行门禁的理由——SQL 拿不到 ctx,claim 撤销时 SQL 持锁跑完,复核/缓存发布失败进重试并被 Kafka 重放,只是无害重复执行;补真实后果:DSN 无超时,MySQL 卡住时每次再平衡等满 10 秒
  - 处理：前半仍成立:handler 仍是 `_ context.Context`(KOC:344、815),store.Save 不接 ctx。后半部分已过时:L-04 第 1 批已给业务池 DSN 加 Dial/Read/Write 超时(proto_sql/db.go:145-147,默认 5/30/30 秒,会话锁等待 10 秒),但读超时 30s 仍大于 claimDrainTimeout 10s,排空超时仍可能发生。设计里重试的 processCtx 明确保持 w.ctx(不随 claim 撤销取消),并注明 S4 的 G1(handler 接 ctx)落地后该不变量仍需保持;不把它作为新缺口转交条件写。
- **A4-F6**：复核修正:inflight 不能用 sync.WaitGroup(Add 与 Wait 并发违约);改互斥锁+计数+归零 channel;先 inflight+1 再在 handleTask 检查 claimCtx;逐条列出认领后执行前的每个出口,都要原子搬走收据并调 retryDone
  - 处理：claimLease{mu, inflight, closed, idleCh}:enter() 在锁内检查 closed/ctx 后 +1(0→1 时换新 idleCh),exit() 1→0 时关闭 idleCh;排空先 close() 再等 idle()。顺序:consumeOneOwnedRetry 先 enter() 再认领。出口全集及处理:认领为空/认领出错→exit;载荷解析失败→死信+exit;次数用完(写→死信/读→丢弃)+exit;无分区/越界/ensureWorker 失败→死信+exit;misrouted→原样挪 p{origin}+exit;派发时 lease.ctx.Done→原样挪回 p{P}+exit;派发时 c.ctx.Done→收据留 processing(Stop 时 release 归还收件箱)+exit;派发成功后 handleTask 顶部 defer finishRetry(含 panic 恢复后的 deferFailedTask)、processTaskBatch superseded 分支、routeToSubShard 停机丢弃分支、worker 退出时缓冲区残留(discardQueuedRetries)。finishRetry 置 nil 后调用,幂等。
- **A4-F7**：复核修正:端到端改确定性注入(持部分测试玩家排序锁 30 秒再 kill -9),docker pause 造不出积压、docker stop 会耗预算进死信;补真正的回滚演练(owner 下造 p* 积压→两实例重启为 shared→p* 排空、verifier 0 mismatch)
  - 处理：写进 rollout 的 Codex E2E 步骤:锁注入 + taskkill /F、shared 回滚演练、retry_requeue 工具演练、WAITAOF 拒启演练(可选,必须还原 appendonly)。
- **A4-F8**：复核修正:分拣用单条 Lua(RPOP 收件箱→Lua 里解析 magic 与分区头→LPUSH p{N} 或死信),不经过 processing;proto 解析失败交执行端
  - 处理：forwardRetryScript:KEYS=[共享收件箱, 死信, p0..p{N-1}](键全部经 KEYS 声明),ARGV[1]=本次上限(≤100/次、每拍 ≤ min(200, 收件箱长度));返回 {moved, legacy, invalid}。不解析 proto、不看 RetryCount,这些在执行端判。
- **A4-F9**：复核修正:补齐同类入口——stress_summarize/stress_snap 输出新指标;merge_zone SCAN 之外可按 p0..p{N-1} 枚举做第二道防线;tidb-phase1 清空核对清单加 p*;说明热备仍常驻空闲连接;文件数逼近 30 需说明
  - 处理：① stress_snap.ps1 整页抓 :9160,无需改;stress_summarize 的 DB 段新增 db_retry_* 列转给 S4 的测量打点批(该要求的依据是 B0 触发规则,B0 不在 S1;S4 同改 DB 段,避免两个会话改同一段),见 ordering_and_overlap。② merge_zone 用 SCAN MATCH(topic 先做 glob 转义)+ 出错 fail-closed;不做 p0..p{N-1} 枚举:同一 Redis 上完整 SCAN 迭代保证返回期间一直存在的键,且 merge_zone 不知道 PartitionCnt,再写一份常数违反「契约值单一来源」。③ tidb-phase1-and-launch-readiness.md:375 写的是 `kafka:retry:*`,已覆盖 p*,不改。④ 热备连接:本 zone 主池没有 ConnMaxIdleTime(proto_sql/db.go:329-330),但 ConnMaxLifetime 5 分钟会回收空闲连接;从未执行 SQL 的热备只剩启动期打开的少量连接,被降为热备的实例最多 5 分钟内带着 ≤MaxIdleConn 条;(c) 不减少连接总账(总账本就按 副本×MaxOpenConn 计)。⑤ 本仓库 28 个文件 + Java PARITY 1 行,低于 30;实现中若需再加文件导致 ≥30,按 AGENTS §10.2 停下报告。
- **A4-F10**：复核修正(B2 部分):relocate R2 只等一代排序锁,双消费窗口须禁 relocate 或等两代;D5 明文要求改 topic 走停写排空+新 TopicGeneration,B2 须先修订 D5;start_game.ps1 三方相等校验同步放宽;若 ①(login 读分区器对齐 C++)先落地,「login 先切新代读」会打断读后写屏障,先后要写明
  - 处理：B2 本轮不做(负责人拍板)。四条写成交互约束放进设计文档 §9 与 D5 修订块第 3 条:B2 前必须已落条件写;须另行修订 D5「停写 drain + 新 TopicGeneration」;双消费窗口禁止 merge_zone relocate/合服(或 relocate 改为同时等两代锁);start_game.ps1 与 k8s_deploy 三方相等校验需放宽为「当前代/排空代」;① 已在本轮(S3)落地,B2 设计须让 login 读与 scene 写同步切代。
- **H-4.1-7**：交接 §4.1 第 7 行:前置是复验清单的双 db 实例项跑通;与 WAITAOF 同一段代码合并排期
  - 处理：双实例项列为 rollout 第 0 步(Codex);WAITAOF 与 ④ 在本规格同批实现(durable_enqueue.go + KOC 三个入队点)。
- **H-WAITAOF-scope**：全局单点交接 P0(handoff-global-singletons-10m-ccu-20261013.md:71、:222):重试/死信入队、owner_epoch 铸造、account:{a} 同连接补 WAITAOF,确认不足不 ack
  - 处理：本项只做 go/db 部分(交接 §9.2:三个写入点只有重试与死信在 go/db);owner_epoch 铸造(scene_manager)与账号目录(login)仍归全局单点交接,不在 S1。go/db 内逐个列出的入队点见 current_state 与 interfaces;重试源搬移不加 WAITAOF 的理由写进设计文档 §7。
- **H-D5**：负责人拍板:④ 先做并修订 TiDB 决策 D5「重试队列不动」
  - 处理：changes 中 docs/design/global-data-layer-tidb-decision.md 在 D5 列表(:152-159)之后追加 2026-10-15 修订块,不重写正文(该文惯例)。
- **H-c-gate**：(c) replicas ≤ PartitionCnt 门禁在 go/db 启动还是 k8s_deploy 拦
  - 处理：只在 k8s_deploy.ps1 写路径入口(zone-up/all-up/infra-up,紧跟 Write-MysqlConnectionLedger,先于任何集群写)静态比较 manifest 的 spec.replicas 与 db.yaml 的 PartitionCnt,超出即 throw。go/db 启动不拒(复核否决的 C1:滚动 maxSurge 本来就短暂多一副本;共享组下启动时无从判断分配;热备能在属主崩溃后 session 超时内接手);运行期 owner 模式下拿不到分区的实例只分拣与回收孤儿,Setup 打 INFO,db_owned_partitions=0。`kubectl scale` 绕过门禁的风险写进 manifest 注释。
- **H-mixed**：RetryRouting shared→owner 滚动升级期间新旧实例并存怎么办
  - 处理：设计文档 §6 混跑矩阵:旧版+新版shared 安全(新版只写共享列表,兜底排空的 p* 为空);新版shared+新版owner 安全(shared 兜底排空 p*,全部执行仍在排序锁/游标/epoch 守卫之下);旧版+新版owner 禁止(旧版看不到 p*,owner 实例全没了 p* 滞留)——切 owner 的前置是该 zone 每个 Pod 都暴露 db_retry_routing_mode;owner→旧镜像回滚禁止直接做,先切 shared 等 p*=0,紧急时回滚后立刻跑 retry_requeue。

## 逐文件改动
### [new] go/db/internal/kafka/retry_routing.go
约 330 行,文件头中文注释写清:属主判据只有 sarama claim 存活;收据位置集合 {共享收件箱, 某个 p{N}, 某实例 processing, 旧共享 processing, 死信},所有搬移单脚本原子完成;owner 模式下重试只在持有原始分区 claim 的进程执行并与该分区新消息同子 shard 串行。内容:
1) claimLease{partition int32; ctx context.Context; mu sync.Mutex; inflight int; closed bool; idleCh chan struct{}}:newClaimLease(partition, ctx)(idleCh 初始为已关闭);enter() bool(锁内:closed 或 ctx.Err()!=nil → false;inflight 0→1 时 idleCh=make;inflight++);exit()(inflight--,降到 0 关 idleCh;<0 时 logx.Errorf("BUG: claim lease inflight underflow: partition=%d") 并钳 0);close()(幂等置 closed);live() bool;idle() <-chan struct{};inflightCount() int。
2) ownedPartitions{mu; leases map[int32]*claimLease}:newOwnedPartitions();acquire(partition, ctx) *claimLease(覆盖同分区旧条目;nil 接收者返回不登记的 lease);release(l)(l.close();仅当 leases[p]==l 才删);live() []*claimLease(存活、按分区升序;nil 接收者返回 nil);lookup(partition) *claimLease(仅存活);每次 acquire/release 后 metrics.SetOwnedPartitions(存活数)。
3) forwardRetryScript(Lua,KEYS=[共享收件箱, 死信, p0..p{N-1}],ARGV[1]=上限):循环 RPOP 收件箱;len>=13 且首字节==2 时取第 10..13 字节大端 uint32,>=2^31 减 2^32;0<=p<N → LPUSH KEYS[3+p] 原字节,否则 LPUSH 死信计 invalid;非 v2 → LPUSH 死信计 legacy;返回 {moved, legacy, invalid}。
4) (c *KeyOrderedKafkaConsumer) forwardSharedRetries(sharedLen int64):上限 min(retryDrainBudgetPerTick, sharedLen)(读失败 -1 时取 200),每次调用最多 retryForwardBatch=100;计 metrics.CountRetryRouteN(owner, forwarded, moved)、CountRetryRouteN(owner, dead, legacy+invalid)、CountTaskDeadLetterN(legacy_payload, legacy)、CountTaskDeadLetterN(invalid_partition, invalid);有坏载荷时 logx.Errorf("retry forward moved malformed payloads to dead queue: topic=%s legacy=%d invalid_partition=%d deadQueue=%s")。
5) consumeRetryQueueOwner(s retryQueueSample):先 forwardSharedRetries(s.shared);homeStoreCoolingDown() 为真则 return(分拣是纯 Redis,冷却期照做);leases:=c.owned.live(),为空即热备直接 return;c.retryTick++ 后按 tick%len 轮转起点;每个租约的剩余额度 = s.partitioned[P](读失败按预算);每拍总预算 200,轮询每个租约各认领一条直到预算/额度用尽或一整圈无进展;用尽时 logx.Infof("retry drain budget reached this tick: topic=%s budget=%d mode=owner")。
6) consumeOneOwnedRetry(l *claimLease) bool:按 F6 的出口表实现(先 l.enter() → claimRetryReceiptFrom(p{P}) → parseClaimedRetry(msg, owner) → origin!=P 则 moveClaimedRetry(msg, p{origin}, misrouted) → task.RetryCount++ → workerTask{dbTask, seq, originPartition:P, hasOriginPartition:true, fromRetry:true, retryReceipt:拷贝, claimCtx:l.ctx, retryDone:l.exit} → select{w.taskCh<-: 计 executed; <-l.ctx.Done(): moveClaimedRetry(msg, p{P}, requeued_revoked)+finishRetry, return false; <-c.ctx.Done(): finishRetry, return false})。
7) (c) moveClaimedRetry(receipt []byte, dest, action string) bool:moveRetryReceipt(processing→dest, receipt, receipt) 原字节;成功计 CountRetryRoute(当前 mode, action);失败 logx.Errorf("retry receipt move failed; receipt stays in this instance's processing list until shutdown release: dest=%s action=%s moved=%v err=%v")。
8) (w *worker) requeueRevokedRetry(task *workerTask, reason string):moveRetryReceipt(w.ctx, rc, w.retryProcessingKey, w.retryReadyKeyFor(task.originPartition), task.retryReceipt, task.retryReceipt);成功计 CountRetryRoute(owner, requeued_revoked) 并 logx.Debugf;失败同上文案 ERROR。
9) (w *worker) discardQueuedRetries():worker 退出时非阻塞排空 w.taskCh 与(已关闭的)各 subShardChans,对每个任务调 finishRetry(收据留在 processing 由 Stop 的 release 归还;Kafka 任务不动,停机排空归 S4)。
10) 搬回工具核心:requeuePartitionScript(Lua,KEYS=[p{N}, 共享收件箱, 死信],ARGV[1]=上限:RPOP p,v2 → LPUSH 收件箱,否则 → LPUSH 死信,返回 {moved, dead});导出 type RequeueReport{Moved map[int32]int64; Stray map[string]int64; Dead int64};func RequeuePartitionedRetries(ctx context.Context, rc redis.Cmdable, topic string, partitionCount int32, batch int) (RequeueReport, error)(先 p0..p{N-1},再 SCAN `kafka:retry:queue:{topic}:p*` 处理越界/多余键;每键循环到返回数 < batch;可重入);type RetryQueueDepthReport{Shared int64; Partitioned map[string]int64; Dead int64; LegacyProcessing int64; Instances map[string]int64};func RetryQueueDepths(ctx, rc redis.Cmdable, topic string, partitionCount int32) (RetryQueueDepthReport, error)(LLEN + ZRANGE 登记表 + SCAN p*)。

依赖：retry_ownership.go(键名与 claimFrom)、metrics.go(新计数函数)、durable_enqueue.go 无依赖

### [new] go/db/internal/kafka/durable_enqueue.go
约 260 行。文件头写清:适用范围只有「Kafka 位点提交之前、任务唯一副本写进 Redis」的三个入队点;为什么重试源的搬移不需要 WAITAOF(前一状态已在 Redis,脚本整体复制,丢了只回到前一状态);为什么用专用客户端 + 组提交(主池 10 条,WAITAOF everysec 下平均占连接 ~0.5s,会饿死游标/锁/落点读);确认不足 fail-closed 与重投幂等依据。
1) type durableEnqueue interface{ push(ctx context.Context, target, queueKey string, payload []byte) error; start(ctx context.Context) error; close() }。push 返回 nil 才表示副本已按配置确认(关闭 WAITAOF 时 = LPUSH 成功)。
2) type directEnqueue struct{ rc redis.Cmdable }:push = 原 pushPayloadDurably 逻辑原样搬来(退避 0/100ms/500ms/1s,ctx 取消即返回);start/close 空操作。结果计 metrics.ObserveDurableEnqueue(target, ok|error, 耗时)。
3) type aofAck struct{ numLocal, numReplicas int; timeout time.Duration; budget time.Duration }:enabled()=numLocal>0||numReplicas>0;budget 为 0 时取 durableEnqueueBudget=5s(只给单测缩短)。
4) newDurableEnqueue(cfg db_config.RedisConfig, main redis.Cmdable) durableEnqueue:!cfg.WaitsForAOF() → directEnqueue{rc: main},并 logx.Errorf("retry enqueue WAITAOF disabled (WaitAOFNumLocal=0 and WaitAOFNumReplicas=0): enqueued copies are not confirmed durable before Kafka ack");否则建专用客户端 redis.NewClient(&redis.Options{Addr: cfg.Hosts, Password: cfg.Password, DB: cfg.DB, DisableIndentity: true, PoolSize: durableClientPoolSize(=2), MaxRetries: -1(关掉 go-redis 整条 pipeline 自动重发,由本层按预算重试), ReadTimeout: WaitAOFTimeoutMs+1s})返回 *aofGroupEnqueue。
5) aofGroupEnqueue{rc redis.Cmdable; ack aofAck; reqs chan *enqueueReq(容量 durableEnqueueQueueSize=1024); ctx/cancel; done chan struct{}; closer func() error}:start(ctx):probeAOFAck(带 WaitAOFTimeoutMs+2s 超时)失败返回 error;成功后起写协程 run()。run:取第一条请求后非阻塞再取至 durableEnqueueMaxBatch=256 条 → flush(batch)。flush:budget 截止内按 0/100ms/500ms/1s 退避重试;每次 waitMs=waitAOFBudgetMs(now, deadline, true, ack.timeout, waitAOFDeadlineMargin=100ms),<1 不发并以 errEnqueueNotDurable 结束;同一 Pipelined 里逐条 LPUSH + p.Do(ctx, "WAITAOF", numLocal, numReplicas, waitMs);逐条判 LPUSH:nil 进入待确认、Redis 错误回复(isRedisErrorReply)只让该请求失败、网络错误则整批(未被拒的)重发;WAITAOF:错误回复 → 全部待确认请求以 errWaitAOFRejected 失败、不重试;回复形状不对(errUnexpectedWaitAOFReply)→ 同上不重试;份数不足 → 包 errEnqueueNotDurable 重试;满足 → 全部成功。重发会重复 LPUSH(接受,见幂等依据)。close():cancel、等 run 退出、把队列里剩余请求以 errDurableEnqueueStopped 回复、关闭专用客户端。push:select 送入 reqs / ctx.Done / done;再 select 等结果(结果 channel 容量 1)/ ctx.Done;计 ObserveDurableEnqueue(target, ok|not_durable|error, 耗时)。
6) probeAOFAck(ctx, rc redis.Cmdable, ack aofAck) error:未开启直接返回 nil、不发命令;Pipelined 里 Do("WAITAOF", numLocal, numReplicas, timeoutMs);错误 → fmt.Errorf("WAITAOF %d %d %d failed (needs appendonly yes and Redis >= 7.2; set ServerConfig.RedisClient.WaitAOFNumLocal/WaitAOFNumReplicas to 0 only where no AOF exists): %w");份数不足 → 返回 error。
7) 纯函数 checkWaitAOFReply(vals []any, numLocal, numReplicas int) error、waitAOFBudgetMs(now, deadline time.Time, hasDeadline bool, configured, margin time.Duration) (int64, bool)、isRedisErrorReply(err) bool —— 语义逐字同 go/data_service/internal/routing/route_publish.go:439-478,注释写明「两处要改一起改」(不为两段小函数新开 go/shared 包、不动 D1 代码)。
8) 错误哨兵:errEnqueueNotDurable、errWaitAOFRejected、errUnexpectedWaitAOFReply、errDurableEnqueueStopped。

依赖：config.go(RedisConfig 新字段与访问器)、metrics.go(ObserveDurableEnqueue)

### [modify] go/db/internal/kafka/key_ordered_consumer.go
只做增量,保留 subShardIndex/spreadKey 等既有改动;不改 markAppliedSeq(KOC:1670)/markAppliedOwnerEpoch(KOC:1674)的调用顺序与位置,不改 dbOpHandler 签名(KOC:331-336),不动 processDBTask/expand status(KOC:1842-1873)与 luaMarkAppliedEpoch(KOC:557-563,归 S4)。
1) KeyOrderedKafkaConsumer(KOC:67-103)加字段:retryRouting string("" 视同 shared)、retryPartitionKeys []string(下标=分区号)、owned *ownedPartitions、durable durableEnqueue、retryTick uint64(只在重试协程读写)、drainTimeout time.Duration(0=claimDrainTimeout,单测用);方法 ownerRouting() bool(owner 且 len(retryPartitionKeys)>0)、enqueuer() durableEnqueue(nil → directEnqueue{rc: c.redisClient})。
2) worker(KOC:105-145)加 retryRouting、retryPartitionKeys、durable;方法 retryReadyKeyFor(partition int32) string(owner 且 0<=p<len → p 键,否则共享键)、enqueuer()。
3) workerTask(KOC:152-169)加 retryDone func() 与 finishRetry()(取出置 nil 再调用;注释:任务任一时刻只归一个 goroutine,经 channel 交接,无需锁)。
4) 删除 waitClaimPending(KOC:291-300),新增 claimDrained(acker *claimAcker, lease *claimLease) <-chan struct{}(等 acker.wait() 再等 lease.idle(),lease 为 nil 只等 acker)与 waitClaimDrained(acker, lease, timeout) bool。
5) NewKeyOrderedKafkaConsumer(KOC:871-950):retryQueueKey 改用 retryReadyKey(topic);routing := cfg.ServerConfig.Kafka.RetryRouting("" → shared);owner 且 PartitionCnt<=0 → 返回 error("RetryRouting=owner needs Kafka.PartitionCnt > 0");设 retryRouting、owned=newOwnedPartitions()、retryPartitionKeys=retryPartitionReadyKeys(topic, PartitionCnt)、durable=newDurableEnqueue(cfg.ServerConfig.RedisClient, redisClient);metrics.SetRetryRoutingMode(routing)。
6) buildWorker(KOC:953-978)透传 retryRouting/retryPartitionKeys/durable。
7) Start(KOC:1011-1056):在 retryOwner.start 之前 `if err := c.enqueuer().start(c.ctx); err != nil { return fmt.Errorf("retry enqueue durability: %w", err) }`;结尾日志加 retryRouting=%s。StartRetryConsumer 日志加 mode。
8) claimRetryReceipt(KOC:1080-1086)改为 claimRetryReceiptFrom(readyKey string)(retryOwner nil 时 RPopLPush(readyKey, processing),否则 retryOwner.claimFrom)。
9) consumeRetryQueue(KOC:1155-1178):s := c.sampleRetryQueues();ownerRouting() → consumeRetryQueueOwner(s);否则 consumeRetryQueueShared(s):先查冷却(原语义),来源依次为共享键(额度 s.shared,-1 取预算)与 s.partitioned 非 0 的 p 键,总预算 200,逐源调 consumeOneRetryFrom(key);注释补「shared 模式兜底排空 p*(回滚路径)」。
10) consumeOneRetryTask(KOC:1182-1253)保留为 `return c.consumeOneRetryFrom(c.retryQueueKey)`(placement_route_test 的 claimRetry 在用);新增 consumeOneRetryFrom(readyKey) bool 与抽出的 parseClaimedRetry(msg []byte, mode string) (task *db_proto.DBTask, seq uint64, partition int32, w *worker, ok bool)——原 1194-1236 的分支原样搬入(畸形/次数用完/无分区/负分区/越界 → 死信或读丢弃,另计 CountRetryRoute(mode, dead)),ok=false 表示已终态处理;shared 派发成功时 action := executed,c.owned.lookup(partition)==nil 则 executed_nonowner。
11) worker.start(KOC:1255-1302)两个退出分支在 subWg.Wait() 之后调 w.discardQueuedRetries();routeToSubShard 的 ctx.Done 分支(KOC:1341-1342)补 task.finishRetry()。
12) processTaskBatch superseded 分支(KOC:1518-1528)在 ackRetryReceipt 之后、ackKafkaTask 之前调 pt.wt.finishRetry()。
13) handleTask(KOC:1538-1686):a) 第一条语句 `if task != nil { defer task.finishRetry() }`,先于 recover 的 defer 注册(最后执行,panic 恢复里的 deferFailedTask 跑完才归零 inflight);b) 入口 claim 检查(KOC:1559-1566):task.fromRetry → w.requeueRevokedRetry(task, "claim revoked before execution") 后 return,Kafka 任务照旧 failKafkaTask;c) routeTask 之后、`startTime := time.Now()`(KOC:1641)之前新增:`if task.fromRetry && task.claimCtx != nil && task.claimCtx.Err() != nil { w.requeueRevokedRetry(task, "claim revoked before SQL"); return }`(排序锁由既有 defer 释放);d) processCtx(KOC:1642-1645)条件改为 `task.claimCtx != nil && !task.fromRetry`,注释:重试 SQL 与其后的 Redis 步骤用 w.ctx,已开始的重试持锁跑完比半途撤掉更安全;S4 若让 handler 接 ctx,重试也不得改用 claimCtx;e) 1648-1685 不动。
14) Setup(KOC:1692-1709)只改无分区分支:owner 模式改 logx.Infof("no partitions assigned (hot standby: retries are only forwarded and orphan receipts reclaimed): groupID=%s topic=%s"),shared 照旧 Errorf。
15) ConsumeClaim(KOC:1724-1840):claimCtx 建好后 `lease := h.consumer.owned.acquire(claim.Partition(), claimCtx)`、`defer h.consumer.owned.release(lease)`;drainAfterStop:acker.stop() → lease.close() → waitClaimDrained(acker, lease, timeout)(timeout 取 c.drainTimeout 或常量),超时日志保留前缀 "claim drain timed out;" 并追加 inflightRetries=%d;消息通道关闭路径先 lease.close(),done 改为 claimDrained(acker, lease);毒消息调用改 persistPoisonMessage(h.consumer.ctx, h.consumer.enqueuer(), h.consumer.retryDeadQueueKey, msg, err)。
16) deferFailedTask(KOC:2104-2167):开头在 CountTaskDeferred 之后 `if errors.Is(cause, errOrderingLockBusy) { metrics.CountOrderingLockBusy(origin) }`(origin = retry|kafka);重试分支 moveRetryReceipt 目的地改 w.retryReadyKeyFor(task.originPartition);Kafka 分支 saveToRetryQueue(w.ctx, w.enqueuer(), w.retryReadyKeyFor(partition), dbTask, task.seq, partition)。
17) quarantineOrderingConflict Kafka 分支(KOC:2199-2202)改 w.enqueuer().push(w.ctx, metrics.DurableTargetDead, w.retryDeadQueueKey, payload)。
18) persistPoisonMessage(KOC:2218)签名改 (ctx, q durableEnqueue, deadQueueKey string, msg *sarama.ConsumerMessage, decodeErr error),内部 q.push(ctx, DurableTargetDead, …);quarantinePoisonTask(KOC:2235)传 w.enqueuer()。
19) 删除 pushPayloadDurably(KOC:2247-2264,逻辑移入 directEnqueue);saveToRetryQueue(KOC:2266-2272)签名改 (ctx, q durableEnqueue, retryQueueKey string, task *db_proto.DBTask, seq uint64, partition int32),内部 q.push(ctx, DurableTargetRetry, …)。
20) Stop(KOC:2274-2283)在 consumer.Close() 之后追加 c.enqueuer().close()(其余顺序不动,停机顺序归 S4)。

依赖：retry_routing.go、durable_enqueue.go、retry_ownership.go、store_failure.go、config.go、metrics.go

### [modify] go/db/internal/kafka/retry_ownership.go
1) 键名集中:新增 retryReadyKey(topic) = "kafka:retry:queue:"+topic;retryPartitionReadyKey(topic string, partition int32) = retryReadyKey(topic)+":p"+十进制分区;retryPartitionReadyKeys(topic string, n int32) []string。2) 新增 (o *retryOwnership) claimFrom(ctx, readyKey string) ([]byte, error)(同 retryClaimScript,KEYS[1] 换成 readyKey;先续租再认领的不变量不变);claim() 改为 `return o.claimFrom(ctx, o.readyKey)`。3) 文件头(:16-39)补一段:RetryRouting=owner 时 ready 拆为共享收件箱 + p{N};回收、停机归还、旧共享列表仍一律回共享收件箱,由 owner 实例分拣到 p{origin}(回收与路由解耦);回退到不认识 p* 的旧版前须先切 shared 并等 p* 归零,或执行 go/db/cmd/retry_requeue。回收脚本(:126-148)不改。

### [modify] go/db/internal/kafka/store_failure.go
sampleRetryQueues(:150-165)改为返回 retryQueueSample{shared int64; partitioned []int64}(与 c.retryPartitionKeys 下标对齐,读失败为 -1):一条 Pipelined 发 LLEN 共享键、本实例 processing、死信与每个 p 键;设置 metrics.SetRetryQueueLength 的 ready(共享)、partitioned(p 之和,任一失败则不设)、processing、dead。结构体定义放本文件。注释改为「采样 ready / partitioned / processing / dead」。

依赖：metrics.go(RetryQueuePartitioned)

### [modify] go/db/internal/config/config.go
1) KafkaConfig(:227-258)加 `RetryRouting string json:"RetryRouting,optional"`(注释:shared|owner,缺省 shared,环境变量 DB_RETRY_ROUTING 可覆盖、只用于单 zone 金丝雀,见 docs/design/db-retry-owner-routing.md)。常量 RetryRoutingShared="shared"、RetryRoutingOwner="owner"、RetryRoutingEnvVar="DB_RETRY_ROUTING"。
2) RedisConfig(:219-225)加 `WaitAOFNumLocal *int json:"WaitAOFNumLocal,optional"`、`WaitAOFNumReplicas *int json:"WaitAOFNumReplicas,optional"`、`WaitAOFTimeoutMs int json:"WaitAOFTimeoutMs,optional"`(用 *int 的理由同 data_service RouteStoreConfig:默认值 1 不是零值,显式写 0 又有合法语义);常量 DefaultRedisWaitAOFNumLocal=1、DefaultRedisWaitAOFNumReplicas=0、DefaultRedisWaitAOFTimeoutMs=1500、MaxRedisWaitAOFNumReplicas=16、MaxRedisWaitAOFTimeoutMs=2000(专用客户端读超时 = 本值 + 1s;everysec 下本地确认最长约 1s,1000 正压边界);访问器 NumLocal()/NumReplicas()(nil 取默认)、WaitsForAOF()。
3) Normalize(:310-379)末尾:WaitAOF 两个指针 nil 时填默认,TimeoutMs==0 填默认(负数保留给 Validate);不改 RetryRouting(由 ResolveRetryRouting 处理空值,以便日志区分 default/config)。
4) 新增 func ResolveRetryRouting(configured, env string) (mode, source string, err error):env 去空白非空时必须恰为 shared|owner(大小写敏感),source="env DB_RETRY_ROUTING";否则 configured=="" → shared/"default";configured 合法 → source "config";非法 → error(拼错不得静默回落)。
5) 新增 func (c *Config) Validate() error:RetryRouting ∈ {"", shared, owner};NumLocal ∈ {0,1};NumReplicas ∈ [0,16];WaitAOFTimeoutMs ∈ [1,2000],错误文本英文并点名键。

### [modify] go/db/internal/config/config_test.go
新增用例:TestResolveRetryRouting(表驱动:("","")→shared/default;("owner","")→owner/config;("shared","owner")→owner/env;("","Owner")、("bogus","")→error);TestRedisWaitAOFDefaultsWhenMissing(holder 只含 RedisClient 段、不写 WaitAOF → Normalize 后 1/0/1500,WaitsForAOF=true);TestRedisWaitAOFExplicitZeroDisables(两项写 0 → WaitsForAOF=false 且 Validate 通过);TestConfigValidateRejects(NumLocal=2、NumReplicas=-1/17、TimeoutMs=-1/2001、RetryRouting="Owner" 各自报错);TestEtcYamlRetryKeysMatchDefaults(照 TestEtcYamlPlacementBlockMatchesDefaults 读 go/db/etc/db.yaml:RetryRouting=="shared",WaitAOF 1/0/1500 与默认常量相等)。

依赖：config.go

### [modify] go/db/internal/metrics/metrics.go
新增(label 全为封闭集合,不含 player_id):
- db_retry_route_total{mode,action} Counter,Help "Retry routing decisions. mode: shared | owner. action: executed | executed_nonowner | forwarded | requeued_revoked | misrouted | dead.";
- db_owned_partitions Gauge(本进程存活的 claim 租约数);
- db_retry_routing_mode{mode} Gauge(生效模式为 1、另一个为 0);
- db_ordering_lock_busy_total{origin} Counter(origin: kafka | retry);
- db_durable_enqueue_total{target,result} Counter(target: retry | dead;result: ok | not_durable | error),Help 写明是「Kafka 位点提交前唯一副本」的入队;
- db_durable_enqueue_seconds{target} Histogram(Buckets=stageBuckets);
- db_retry_queue_length 增加 queue="partitioned"(p* 之和),Help 同步改。
常量:RetryModeShared/RetryModeOwner、RetryRouteExecuted/ExecutedNonOwner/Forwarded/RequeuedRevoked/Misrouted/Dead、OriginKafka/OriginRetry、DurableTargetRetry/DurableTargetDead、DurableOK/DurableNotDurable/DurableError、RetryQueuePartitioned。函数:CountRetryRoute(mode, action)、CountRetryRouteN(mode, action string, n int)、SetOwnedPartitions(n int)、SetRetryRoutingMode(mode string)、CountOrderingLockBusy(origin)、ObserveDurableEnqueue(target, result string, d time.Duration)、CountTaskDeadLetterN(reason string, n int)。register()(:172-192)注册全部并以 0 预建:retry_route 2×6、ordering_lock_busy 2、durable_enqueue 2×3、retry_routing_mode 2。

### [modify] go/db/internal/metrics/metrics_test.go
新增 TestRetryRoutingSeriesPrecreated:调 register() 后 prometheus.DefaultGatherer.Gather(),断言 db_retry_route_total 恰有 12 个 (mode,action) 组合、db_ordering_lock_busy_total 2 个、db_durable_enqueue_total 6 个、db_retry_routing_mode 2 个,且取值集合与常量一致(常量改名先在这里红,告警按名字写死)。

依赖：metrics.go

### [new] go/db/internal/kafka/retry_routing_test.go
miniredis + 现有 recordingHarness/newTestWorker;租约用 mr.SetTime 推进,不依赖墙钟;新增 fakeClaim(实现 sarama.ConsumerGroupClaim:Topic/Partition/InitialOffset/HighWaterMarkOffset/Messages)。用例见 tests 字段。

依赖：retry_routing.go、key_ordered_consumer.go

### [new] go/db/internal/kafka/durable_enqueue_test.go
miniredis 注册假 WAITAOF(mr.Server().Register("WAITAOF", …) 回可配置的两元素数组或错误,并计调用次数);真 Redis 用例以环境变量 GODB_REDIS_ITEST_ADDR 门控(未设即 Skip)。用例见 tests 字段。

依赖：durable_enqueue.go、key_ordered_consumer.go

### [modify] go/db/internal/kafka/retry_ownership_test.go
新增 TestRetryOwnership_ClaimFromUsesGivenListAndRenewsLease(从 p 键认领进本实例 processing、登记表租约被续)与 TestRetryOwnership_ClaimDelegatesToReadyKey(claim() 等价于 claimFrom(readyKey))。既有用例不改。

依赖：retry_ownership.go

### [modify] go/db/internal/kafka/placement_route_test.go
仅机械改 4 处 saveToRetryQueue 调用(:523、:559、:1039、:1058)为 saveToRetryQueue(w.ctx, directEnqueue{rc: w.redisClient}, w.retryQueueKey, task, 10, 0);不改断言。

依赖：key_ordered_consumer.go、durable_enqueue.go

### [modify] go/db/db.go
Normalize(:59)之后:mode, source, err := config.ResolveRetryRouting(config.AppConfig.ServerConfig.Kafka.RetryRouting, os.Getenv(config.RetryRoutingEnvVar));err → panic(fmt.Sprintf("retry routing rejected: %v", err));回写 config.AppConfig.ServerConfig.Kafka.RetryRouting = mode;`if err := config.AppConfig.Validate(); err != nil { panic(fmt.Sprintf("config rejected: %v", err)) }`;logx.Infof("retry routing: mode=%s source=%s", mode, source)。启动横幅(:221-243)加一行 fmt.Printf("  retry:       routing=%s (%s) waitaof=%d/%d/%dms\n", …)。不改 EnsureTopics 段(S5 会改 TopicSpec)。

依赖：config.go

### [modify] go/db/etc/db.yaml
Kafka 段 SubShardCount 注释块之后加 `RetryRouting: shared`,中文注释:两种模式含义、环境变量覆盖只用于金丝雀、启动日志核对、切 owner 前该 zone 必须全是新版本、回滚旧镜像前先切 shared 等 p* 归零(指向设计文档)。RedisClient 段(:97-101)DB: 0 之后加 `WaitAOFNumLocal: 1`、`WaitAOFNumReplicas: 0`、`WaitAOFTimeoutMs: 1500`,注释:Kafka 位点提交前这份副本是唯一副本;确认不足不提交位点(fail-closed);启动探针失败拒启;只有确无 AOF 的环境两项写 0;redis-ha 把 NumReplicas 写 1。

依赖：config.go

### [new] go/db/cmd/retry_requeue/main.go
运维工具(回滚用),日志英文。flags:-f(默认 etc/db.yaml,用于推导 topic/PartitionCnt/Redis)、-zone(覆盖 ZoneId)、-redis(host:port 覆盖)、-redis-db、-redis-password-env(从指定环境变量读口令,口令不经命令行、不打印)、-check(只读盘点)、-batch(默认 100,范围 [1,1000])。流程:conf.MustLoad → Normalize → topic = DbTaskTopicForGeneration(zone, TopicGeneration),partitions = PartitionCnt → 建 go-redis 客户端 → -check 时打印 kafka.RetryQueueDepths 的结果后退出;否则先打印盘点,再调 kafka.RequeuePartitionedRetries 并打印每分区搬回数、越界键与进死信数,最后再盘点一次。开头打印前置条件:"run only after every go/db of this zone runs RetryRouting=shared or the pre-owner image; owner instances would forward the items back"。出错非零退出。

依赖：retry_routing.go

### [modify] tools/merge_zone/preflight.go
1) 新增 dbRetryPartitionQueuePrefix(topic) = dbRetryQueueKey(topic)+":p";escapeRedisGlob(s)(转义 * ? [ ] \\);dbRetryPartitionQueuePattern(topic) = escapeRedisGlob(prefix)+"*";注释镜像 go/db retry_ownership.go 的键并说明「只按 SCAN 发现、不枚举 p0..p{N-1}」的理由。2) 新增接缝 type queueKeyReader interface{ ZRange(ctx, key string, start, stop int64) *redis.StringSliceCmd; Scan(ctx, cursor uint64, match string, count int64) *redis.ScanCmd }(*redis.Client 天然满足,audit_checks.go 调用点不用改)。3) dbTaskQueueKeysFor(topic, instanceIDs, partitionedKeys []string)(第三参数排序去重后追加在末尾)。4) dbTaskQueueKeys(ctx, rdb queueKeyReader, topic):ZRANGE 登记表 + scanAllKeys(COUNT 1000 循环到 cursor=0),任一出错返回 error(fail-closed,不能当空)。5) P4 成功日志改为「… incl. per-instance processing lists and partitioned ready lists」。

### [modify] tools/merge_zone/merge_unit_test.go
既有 TestDbTaskQueueKeysCoverEveryInstance 调用补第三参数 nil;新增 TestDbTaskQueueKeysIncludePartitionedReadyLists(纯函数,p 键排序去重追加)、TestDbRetryPartitionQueuePattern(db_task_zone_1 → `kafka:retry:queue:db_task_zone_1:p*`;db_task_zone_1 的前缀不是 db_task_zone_11 / db_task_zone_1_g2 键的前缀;escapeRedisGlob 转义)、TestDbTaskQueueKeysScanFailureFailsClosed 与 TestDbTaskQueueKeysScanPagesAggregated(假 queueKeyReader 用 redis.NewStringSliceResult / redis.NewScanCmdResult 构造回复;不引 miniredis)。

依赖：preflight.go

### [modify] tools/scripts/k8s_deploy.ps1
1) ConfigMap 生成器取值段($dbSubShardCount 附近,约 :3667)加 $dbRetryRouting = Get-AuthoritativeScalar 'go/db/etc/db.yaml' 'ServerConfig.Kafka.RetryRouting',以及 $dbWaitAOFNumLocal / $dbWaitAOFNumReplicas / $dbWaitAOFTimeoutMs(键 ServerConfig.RedisClient.WaitAOF*)。2) db heredoc:Kafka 段 SubShardCount 行(约 :4103)之后加 `RetryRouting: "${dbRetryRouting}"` + 注释(改值后必须 rollout restart);RedisClient 段 DB: 0 之后加三行 WaitAOF 镜像 + 注释(redis-ha 接入时 NumReplicas 要写 1)。不碰 GroupID 行(S2)。3) 在 Assert-MysqlPoolMirror(:1455)之后新增 function Test-DbReplicaPartitionBudget([int]$Replicas, [int]$PartitionCnt)(PartitionCnt<=0 或 Replicas>PartitionCnt 即 throw,文本:"go/db 副本数门禁:deploy/k8s/manifests/go-svc/db.yaml 的 replicas=$Replicas 超过 go/db/etc/db.yaml 的 ServerConfig.Kafka.PartitionCnt=$PartitionCnt(replicas ≤ PartitionCnt)。多出的副本拿不到分区只做热备;滚动发布的 maxSurge 临时副本不计入。要么减副本,要么走离线换代加分区。")与 function Assert-DbReplicasWithinPartitionContract(Get-ManifestDeploymentReplicas + Get-AuthoritativeScalar 后调前者),注释写明为什么不在 go/db 启动期拦。4) 写路径入口(:6776-6777 之后)调用 Assert-DbReplicasWithinPartitionContract。

依赖：go/db/etc/db.yaml 新键

### [modify] tools/scripts/tests/k8s_deploy_contract.tests.ps1
1) 「db ConfigMap 的 Kafka/Database 关键值 == go/db/etc/db.yaml」(:80-97)的 $pairs 加 ServerConfig.Kafka.RetryRouting、ServerConfig.RedisClient.WaitAOFNumLocal/WaitAOFNumReplicas/WaitAOFTimeoutMs。2) 新增一组:按 AST 抽出 Test-DbReplicaPartitionBudget、Assert-DbReplicasWithinPartitionContract 及依赖 Get-AuthoritativeScalar、Get-ManifestDeploymentReplicas(做法同 MySQL 总账段 :1873-1883);Test-Case 「replicas 11 > PartitionCnt 10 即 throw,文本含 replicas ≤ PartitionCnt」;Test-Case 「replicas 10 == PartitionCnt 放行,PartitionCnt 0 throw」;Test-Case 「仓库 db.yaml manifest 副本数 ≤ go/db/etc/db.yaml 的 PartitionCnt(设 $RepoRoot 后调 Assert-DbReplicasWithinPartitionContract 不抛)」。负向用例断错误文本。

依赖：k8s_deploy.ps1

### [modify] deploy/k8s/manifests/go-svc/db.yaml
只改注释(:29-30):有效并行上限 = 分区数;RetryRouting=owner 下拿不到分区的副本是热备(只分拣重试、回收孤儿收据,不执行重试),属主崩溃后在 session 超时内接手;k8s_deploy 写路径入口有 replicas ≤ PartitionCnt 门禁(maxSurge 临时副本不计),`kubectl scale` 会绕过它;继续禁止 HPA。不改 replicas/strategy(S4 加 minReadySeconds)。

### [modify] deploy/k8s/db-task-retry-alerts.yaml
头注释「来源」加 docs/design/db-retry-owner-routing.md。DbTaskRetryQueueGrowing 的 expr 改为 `max by (namespace) (sum by (namespace, pod) (db_retry_queue_length{queue=~"ready|partitioned"})) > 1000`(旧版 Pod 不报 partitioned 也照样被计入),描述补 p* 键。新增(同组):
- DbRetryPartitionedNoProgress(warning,for 0m):`min_over_time((max by (namespace) (db_retry_queue_length{queue="partitioned"}))[15m:1m]) > 0 unless on (namespace) (sum by (namespace) (increase(db_retry_route_total{action="executed"}[15m])) > 0)`;描述:p* 15 分钟不空且无人执行,查 owner 实例是否还在、是否误回滚到旧镜像,处置用 go/db/cmd/retry_requeue。
- DbRetryNonOwnerExecutionInOwnerZone(warning):`sum by (namespace) (increase(db_retry_route_total{action="executed_nonowner"}[10m])) > 0 and on (namespace) (max by (namespace) (db_retry_routing_mode{mode="owner"}) == 1)`;描述:zone 已有 owner 实例,仍有 shared 实例在执行非属主重试(混跑未收口 / env 覆盖残留)。
- DbRetryRevokedOutsideRebalance(info):`sum by (namespace) (increase(db_retry_route_total{action="requeued_revoked"}[10m])) > 0 unless on (namespace) (sum by (namespace) (changes(db_owned_partitions[10m])) > 0)`。
- DbDurableEnqueueNotConfirmed(critical,for 0m):`sum by (namespace, target, result) (increase(db_durable_enqueue_total{result=~"not_durable|error"}[5m])) > 0`;描述:入队未确认落盘,Kafka 位点不提交、claim 停止并重投(存盘不丢但该分区停滞),查 Redis AOF/fsync 与副本;不要关掉 WAITAOF 来止血,除非负责人同意。

依赖：metrics.go

### [new] docs/design/db-retry-owner-routing.md
中文设计 + 运维手册:§0 结论与范围(做 (a)(c)+WAITAOF;B0 调参与 B2 不做;D5 已修订);§1 现状(引 current_state 的 file:line);§2 键与载荷表(共享收件箱、p{N}、processing、instances、dead;载荷仍是 v2 magic 0x02);§3 两种模式行为表;§4 不变量、收据位置集合、「认领之后执行之前」出口表;§5 claim 租约与 inflight(为什么不用 WaitGroup、为什么不用 Setup/Cleanup 判属主、eager 交接顺序);§6 混跑矩阵;§7 WAITAOF(入队点清单、组提交、fail-closed 与重投幂等依据、默认值、本机无 AOF、go-redis 用法与 Redis ≥ 7.2、为什么搬移不加);§8 (c) 门禁位置与热备、热备连接的如实说明;§9 与 B2 的交互约束(A4-F10 四条);§10 上线/回滚/紧急搬回步骤(同 rollout);§11 告警与处置;§12 Java 版不适用;§13 Codex 验证命令;§14 修订历史。

依赖：全部代码改动

### [modify] docs/design/global-data-layer-tidb-decision.md
D5 列表(:152-159)之后追加引用块,不改正文:
> **2026-10-15 修订(负责人 2026-10-13 拍板「④ 先做并修订 D5」)**
> 1. 「retry/dead 队列全不动」改为:允许两项改动 —— ④ 重试只交给分区属主(新增键族 `kafka:retry:queue:{topic}:p{N}`、开关 `ServerConfig.Kafka.RetryRouting`,见 [db-retry-owner-routing.md](./db-retry-owner-routing.md))与重试/死信/毒消息入队补 WAITAOF(`ServerConfig.RedisClient.WaitAOF*`)。两项都必须作为**独立发布单元**,不得与 TiDB Phase 1 任何迁移批次同窗口或同一次滚动。
> 2. 迁移窗口内冻结 RetryRouting:L1–L4 与 chaos_test 在迁移前后用同一个值跑;迁移的「停写 drain」核对清单加上 `kafka:retry:queue:{topic}:p*`。
> 3. topic 命名与 partition=10 不可变契约**不变**:④ 不改分区数、不换代;④B2(双消费在线换代)本轮不做。要做 B2 必须先落 MySQL 条件写,并另行修订本节第一条「改 topic 路由必须走停写 drain + 新 TopicGeneration」,不能以本修订为依据;双消费窗口内禁止 relocate/合服,三方相等校验要同步放宽。
> 4. 风险表第 9 条照旧。

### [modify] docs/design/db-task-kafka-partition-contract.md
「离线扩容流程」第 3 步(:40-43)在 processing 说明之后补:以及 `kafka:retry:queue:{topic}:p{N}`(RetryRouting=owner 起按原始分区拆开的 ready,用 `redis-cli --scan --pattern 'kafka:retry:queue:{topic}:p*'` 列出逐个 LLEN)都要为 0。

### [modify] docs/ops/merge-zone-runbook.md
P4 行(:267)与 kafka_db_task_queues 行(:405)补「以及按分区的 ready 列表 kafka:retry:queue:db_task_zone_{src}:p*」;§8 Step 2 命令块(:489-494)追加 `redis-cli -h <shared-redis> -n 0 --scan --pattern 'kafka:retry:queue:db_task_zone_<src>:p*'` 并逐个 LLEN == 0,注明 merge_zone 的 P4 已用 SCAN 查到它们。不改其中的组名(S2 改)。

### [modify] docs/PROGRESS.md
只在末尾追加一条(merge=union):「2026-10-15 存盘管线 S1:④ 重试只交给分区属主 + 重试/死信入队 WAITAOF + D5 修订(未编译、未测试,待 Codex 验证)」,写改动范围、开关与默认值、上线前置、Java 版对应「无(不适用,Java 没有 Kafka 存盘链)」,并注明 db-task-store-unavailable-deferral.md §9 的 WAITAOF / ④ 两行已由 db-retry-owner-routing.md 取代;stress_summarize 的 db_retry_* 段与 B0 两个饱和度指标转 S4。

### [modify] docs/README.md
在 :144(db-write-fencing-mysql.md)之后追加一行:`- [db-retry-owner-routing.md](design/db-retry-owner-routing.md) — go/db 重试只交给分区属主(④)与重试 / 死信入队 WAITAOF:设计、混跑、上线与回滚`。工作区该文件有他人未提交改动,只追加这一行。

依赖：docs/design/db-retry-owner-routing.md

### [modify] D:\luyuan\xuanming-server-mmo\PARITY.md
(Java 仓库,AGENTS §12)追加一行:「db 重试只交给分区属主 / 重试与死信入队 WAITAOF:不适用(Java 没有 Kafka 存盘链与重试队列;对应本仓库 S1 提交)」。不计入本仓库文件数。

## 接口（类型 / 配置键 / 指标 / Redis 键 / 组名）
【配置键】
- ServerConfig.Kafka.RetryRouting:string,optional,取值 shared|owner(大小写敏感),缺省 shared;环境变量 DB_RETRY_ROUTING 优先(只用于金丝雀),非法即拒启。K8s ConfigMap 从 go/db/etc/db.yaml 镜像。
- ServerConfig.RedisClient.WaitAOFNumLocal:*int,缺省 1,∈{0,1};WaitAOFNumReplicas:*int,缺省 0,∈[0,16];WaitAOFTimeoutMs:int,0→1500,∈[1,2000]。两项都显式写 0 = 不发 WAITAOF(退回只 LPUSH,启动打一行 ERROR)。K8s ConfigMap 镜像这三项。
【新函数/签名(go/db)】
- config:ResolveRetryRouting(configured, env string) (mode, source string, err error);(*Config).Validate() error;RedisConfig.NumLocal()/NumReplicas()/WaitsForAOF();常量 RetryRoutingShared/RetryRoutingOwner/RetryRoutingEnvVar、DefaultRedisWaitAOF*、MaxRedisWaitAOF*。
- kafka(未导出):type durableEnqueue interface{push(ctx, target, queueKey string, payload []byte) error; start(ctx) error; close()};directEnqueue;aofGroupEnqueue;aofAck;newDurableEnqueue(cfg db_config.RedisConfig, main redis.Cmdable) durableEnqueue;probeAOFAck(ctx, rc redis.Cmdable, ack aofAck) error;checkWaitAOFReply/waitAOFBudgetMs/isRedisErrorReply;claimLease/ownedPartitions;saveToRetryQueue(ctx, q durableEnqueue, retryQueueKey string, task *db_proto.DBTask, seq uint64, partition int32) error;persistPoisonMessage(ctx, q durableEnqueue, deadQueueKey string, msg *sarama.ConsumerMessage, decodeErr error) error;(o *retryOwnership).claimFrom(ctx, readyKey string) ([]byte, error);retryReadyKey/retryPartitionReadyKey/retryPartitionReadyKeys;(w *worker).retryReadyKeyFor(int32) string;(t *workerTask).finishRetry()。pushPayloadDurably 删除。
- kafka(导出,供 cmd/retry_requeue):RequeuePartitionedRetries(ctx, rc redis.Cmdable, topic string, partitionCount int32, batch int) (RequeueReport, error);RetryQueueDepths(ctx, rc redis.Cmdable, topic string, partitionCount int32) (RetryQueueDepthReport, error)。
- 不变:dbOpHandler 签名 func(ctx context.Context, store *proto2mysql.DB, task *db_proto.DBTask, msg proto.Message) error;markAppliedSeq / markAppliedOwnerEpoch 的调用顺序与位置;retryBudgetExempt;载荷格式(v2 magic 0x02 + seq + origin partition)。
【Redis 键】
- 新:kafka:retry:queue:{topic}:p{N},N∈[0,PartitionCnt),LIST,按原始分区的 ready(owner 模式写入与执行;shared 模式兜底排空)。
- 语义变化:kafka:retry:queue:{topic} = 共享「未分拣收件箱」(回收、停机归还、旧版实例、人工重投都进这里)。processing:{topic}:{instance}、instances、dead、旧共享 processing 不变。排序锁键 distributed:lock:kafka:ordering:{topic}:{key}:{msgType} 不变。
【Lua】forwardRetryScript(KEYS=[收件箱, 死信, p0..p{N-1}],ARGV=[上限],返回 {moved, legacy, invalid});requeuePartitionScript(KEYS=[p{N}, 收件箱, 死信],ARGV=[上限],返回 {moved, dead})。
【WAITAOF 语义】入队点:Kafka 源失败入重试(deferFailedTask)、Kafka 源隔离进死信(quarantineOrderingConflict,含 orderingIncomparable / stale_topic / missing_required 写)、毒消息(persistPoisonMessage,ConsumeClaim 分发前与 handleTask/processTaskBatch 解码失败)。命令:专用客户端同一 Pipelined 内 LPUSH×k + Do("WAITAOF", numlocal, numreplicas, waitMs),Cmd.Slice() 解析两元素数组;waitMs = min(WaitAOFTimeoutMs, 截止前剩余 − 100ms),<1ms 不发;单批总预算 5s;份数不足/网络错误在预算内整批重发,WAITAOF 错误回复与回复形状错误不重试。未确认 → push 返回 error → 调用方不 ack(Kafka 源 failKafkaTask、ConsumeClaim acker.fail),offset 不提交,消息重投。幂等依据:重投与重复 LPUSH 产生的同一任务多份载荷带相同 (seq=offset+1, 原始分区);执行时 applied cursor 判 seq ≤ 已落库即 orderingStale 丢弃并 ack 收据,批内同 seq 视为重复投递(superseded),owner epoch 只升不降;死信重复条目按 taskID /(topic, partition, offset)人工去重。启动探针:开启时 Start() 先发一次 WAITAOF,错误或份数不足即拒启(appendonly no 回 ERR、Redis < 7.2 回 unknown command)。Redis 要求 ≥ 7.2 且 appendonly yes(K8s 7.2、redis-ha 7.2、compose latest 均满足)。
【指标】db_retry_route_total{mode=shared|owner,action=executed|executed_nonowner|forwarded|requeued_revoked|misrouted|dead};db_owned_partitions;db_retry_routing_mode{mode};db_ordering_lock_busy_total{origin=kafka|retry};db_durable_enqueue_total{target=retry|dead,result=ok|not_durable|error};db_durable_enqueue_seconds{target};db_retry_queue_length{queue=ready|partitioned|processing|dead}。
【日志(英文)】"retry routing: mode=%s source=%s";Setup owner 无分区 INFO "no partitions assigned (hot standby: …)";"claim drain timed out; … inflightRetries=%d";"retry receipt move failed; receipt stays in this instance's processing list until shutdown release: …";"requeue revoked retry failed; …";"retry forward moved malformed payloads to dead queue: …";WAITAOF 探针失败文本见 durable_enqueue.go。
【CLI】go run ./cmd/retry_requeue -f etc/db.yaml [-zone N] [-redis host:port] [-redis-db N] [-redis-password-env NAME] [-check] [-batch 100]。
【部署】k8s_deploy.ps1:Test-DbReplicaPartitionBudget -Replicas <int> -PartitionCnt <int>(throw);Assert-DbReplicasWithinPartitionContract(写路径入口调用)。Kafka 组名、topic、分区数均不变。

## 顺序与重叠
【markApplied* 调用顺序】S1 不动 markAppliedSeq(KOC:1670)与 markAppliedOwnerEpoch(KOC:1674)的调用顺序与位置,也不动 verify(KOC:1662-1667)与成功分支其余代码。S1 在 handleTask 里只改:顶部新增 defer finishRetry;入口 claim 分支(KOC:1559-1566)给重试加 requeue;routeTask 之后、startTime(KOC:1641)之前新增重试 claim 复核;processCtx 条件(KOC:1642-1645)加 !task.fromRetry。条件写会话在这些改动上 rebase 即可;它若把 stale_owner 等结果经 quarantineOrderingConflict 送死信,Kafka 源分支会自动走 WAITAOF。dbOpHandler 签名不变。
【与 S4(G1 治标 + 停机顺序 + 打点 + 过时注释 + Lua tonumber)重叠】同改 KOC 与 metrics.go。① processCtx 同 4 行:S4 若让 handler 接 ctx(store.WithContext)并随排序锁失租取消,必须保留「重试任务不从 claimCtx 派生 processCtx」;失租取消对重试与 Kafka 一视同仁可以。② Stop():S1 只在 consumer.Close() 之后加 c.enqueuer().close();S4 改「先撤拉取再等 worker」时要保留这行在最后,并保留 S1 加的 worker 退出 discardQueuedRetries 与 routeToSubShard 丢弃分支的 finishRetry(否则每次停机都等满 drain 超时)。③ luaMarkAppliedEpoch tonumber(KOC:557-563)与 wrapRetryPayload 等过时注释归 S4,S1 不碰。④ 打点:B0 的 db_worker_inflight / db_subshard_queue_depth_max 与 stress_summarize DB 段的 db_retry_* 列交给 S4 的测量批(S4 本就改 DB 段与 metrics.go),S1 不改 stress_summarize.ps1 / test_stress_summarize.ps1;stress_snap.ps1 整页抓 :9160 无需改。⑤ metrics.go register() 列表两边都追加,合并时冲突只在列表行。
【与 S2(③ 消费组按 zone + 镜像钉版本 + 回档 Step 2)重叠】KOC 的 NewKeyOrderedKafkaConsumer、Start(S2 在入组前加交接闸)、Setup(S2 播种位点;S1 只改无分区分支的日志)、ConsumeClaim(S1 租约与排空);config.go KafkaConfig(S2 加 GroupHandoffFrom 等,S1 加 RetryRouting);go/db/etc/db.yaml Kafka 段(S2 改 GroupID);k8s_deploy.ps1 db heredoc(S2 改 :4100 与 login :4372 的 GroupID,S1 加 RetryRouting 与 RedisClient WaitAOF 三行)与契约测试;tools/merge_zone/preflight.go(S2 改 lag 来源/唯一权威组,S1 改 dbTaskQueueKeys);merge-zone-runbook §8 Step 2(S2 改组名,S1 加 p* 命令)。全部是同文件不同函数/行。生产上 owner 模式的切换(rollout 第 3/4 步)建议排在该 zone 完成 S2 切组之后,否则任一 zone 的 Pod 进出都会让全服出现 requeued_revoked 尖峰(瓶颈文档 §5.2 第 3 条)。
【与 S3(① 读写分区对齐)重叠】S3 在 ConsumeClaim 解码后插写不符计数一行、新增 producer_partition_audit.go、改 metrics.go;与 S1 的租约代码相邻,合并注意。
【与 S5(脏快照 + retention/PVC)重叠】db.go(S5 若改 EnsureTopics 的 TopicSpec 加 retention.bytes,S1 只在 Normalize 之后加路由解析与 Validate)、k8s_deploy.ps1 与契约测试(topic 配置段 vs db ConfigMap);无 KOC 重叠。
【建议合入顺序(KOC)】S3 → S2 → S4 → S1(与瓶颈文档「① → ③ → ④」一致);若 S1 先合,后来者按上面列出的不变量 rebase。D5 修订与设计文档可随 S1 一起合。
【不归本轮/保留】kafka:expand:status 死代码保留(S1 不碰 processDBTask);④B2 不做;② 批 B 作废,不改连接池与 mysql.yaml;MySQL 条件写归另一会话。
【工作区他人改动】docs/PROGRESS.md、docs/README.md 有他人未提交改动,只追加;k8s_deploy.ps1、merge-zone-runbook.md 动手前重新 git status / git diff,只改上面列出的块。

## 测试
### go/db/internal/kafka/retry_routing_test.go
TestClaimLease_EnterExitIdle(初始 idle 已关闭;enter 后 idle 未关闭、exit 后关闭;close 后 enter=false;ctx 取消后 enter=false;exit 下溢钳 0 不 panic)。TestOwnedPartitions_ReleaseOnlyRemovesOwnLease(同分区先 a 后 b,release(a) 后 lookup==b)。TestOwnedPartitions_LiveExcludesDeadLeases(取消/关闭的不在 live,按分区升序)。TestOwnedPartitions_ConcurrentAcquireRelease(16 goroutine × 1000 次,-race 干净)。TestRetryRouting_ForwardSortsByOriginPartition(a:owner、无租约,收件箱 origin=3/1 → 分别进 p3/p1,字节不变、RetryCount 不变、无 handler 调用、forwarded+2)。TestRetryRouting_ForwardDeadLettersLegacyAndOutOfRange(h:v1、raw、v2 origin=9(N=4)、v2 0xFFFFFFFF → 死信 4 条、p* 空,dead_letter legacy_payload×2 / invalid_partition×2;shared 模式同组载荷沿用原路径进死信)。TestRetryRouting_OwnerExecutesOnlyOwnedPartitions(b:A 持 {0}、B 持 {1} 共用 miniredis;先跑 A 只派出 P0 任务、p1 原样,再跑 B 派出 P1)。TestRetryRouting_HotStandbyOnlyForwards(c:无租约时 p0 不被认领、taskCh 为空)。TestRetryRouting_RevokedWhileQueuedRequeuesUntouched(d:派发后撤销租约再 handleTask → 不调 handler,p{P} 内为原字节(RetryCount 未加),processing 空,租约 idle)。TestRetryRouting_RevokedDuringHandlerCompletes(e:afterCall 里撤销租约 → handler 恰一次,游标为 v2:P:seq,收据已 ack,租约 idle)。TestRetryRouting_KafkaFailureDestinationByMode(f:owner 下 Kafka 源失败进 p{origin}、shared 下进共享键;owner 下重试再失败搬回 p{origin} 且 RetryCount 持久化)。TestRetryRouting_SharedModeDrainsPartitionedLists(g:shared 消费者把 p2 的收据派出,计 executed_nonowner,p2 清空)。TestRetryRouting_MisroutedMovesToOriginPartition(i:origin=2 的载荷放在 p0,A 持 {0} → 原字节进 p2,misrouted+1,不执行)。TestRetryRouting_OrphanReclaimForwardExecute(j:A 认领后不处理模拟崩溃,mr.SetTime 推过租约,B 一拍内回收进收件箱→分拣→执行)。TestRetryRouting_MixedModesConverge(k:shared 与 owner 两个消费者混跑,过程中翻转租约,programFailure 随机注入;循环到所有列表为空,每个 key 游标 = 最大 seq,沿用 TestConcurrentWorkers_FinalStateIsGlobalMax 写法)。TestRetryRouting_ReceiptUniqueness(l:无 Kafka 重投的场景下每一步后 {收件箱, p*, processing:*, 死信} 合计每条载荷最多一份)。TestRetryRouting_RetryDoneOnEveryExit(表驱动:superseded、handler panic、派发时租约取消(taskCh 已满)、畸形载荷、次数用完、misrouted、worker 停机残留 → 每种之后 lease.inflightCount()==0)。TestConsumeClaim_DrainWaitsForInflightRetries(fakeClaim + recordingSession;lease.enter() 模拟在途重试,取消 session ctx 后 100ms 内 ConsumeClaim 不返回,exit 后很快返回)。TestConsumeClaim_DrainBoundedWhenRetryStuck(drainTimeout=200ms,在途不归零也在约 200ms 返回)。TestRequeuePartitionedRetries_MovesV2AndIsIdempotent(p0 两条 v2、p3 一条 v2 + 一条 legacy、p12 越界键一条 → 收件箱 4 条字节不变、死信 1 条;再跑一次全为 0)。TestRetryQueueDepths_Report。

### go/db/internal/kafka/durable_enqueue_test.go
TestCheckWaitAOFReply(形状错、负数、份数不足、满足)。TestWaitAOFBudgetMs(无截止取配置值;先截断成整毫秒再判 <1;margin 生效)。TestDirectEnqueue_LPushOnly(miniredis 未注册 WAITAOF,直写成功即证明没发 WAITAOF;ctx 取消立即返回)。TestAOFGroupEnqueue_ConfirmedBatch(假 WAITAOF 回 [1,0],50 个并发 push 全部 nil,列表 50 条,durable_enqueue_total{ok}+50)。TestAOFGroupEnqueue_NotDurableWithinBudget(回 [0,0],budget 300ms、timeout 50ms → errors.Is(err, errEnqueueNotDurable),耗时 <1s,result=not_durable)。TestAOFGroupEnqueue_WaitAOFErrorReplyNotRetried(回 ERR … appendonly is disabled → errors.Is(err, errWaitAOFRejected),假命令只被调一次)。TestAOFGroupEnqueue_StoppedReturnsError(close 后 push → errDurableEnqueueStopped)。TestAOFGroupEnqueue_CanceledCallerCtx(已取消的 ctx → ctx 错误)。TestProbeAOFAck(关闭时不发命令返回 nil;[1,0] → nil;[0,0] → error;未注册 → error 文本含 appendonly)。TestDeferFailedTask_KafkaNotAckedWhenEnqueueNotDurable(worker.durable=已启动的组提交、假命令 [0,0]、attachKafkaClaim + programFailure → session.marked() 为空、claimCtx 被取消)。TestQuarantine_NotAckedWhenEnqueueNotDurable(复用 orderingIncomparable 场景)。TestPoison_NotAckedWhenEnqueueNotDurable(复用 TestPoisonKafkaPayloadIsPersistedBeforeAck 的消息)。TestDurableEnqueue_RealRedis(GODB_REDIS_ITEST_ADDR 门控:一条 pipeline 发 [CLIENT ID, LPUSH, WAITAOF 0 0 100, CLIENT ID] 两次 CLIENT ID 相等;CONFIG GET appendonly 为 yes 时 WAITAOF 1 0 1500 回 [1,0],为 no 时 t.Fatalf 提示而不是 Skip;组提交 push 成功且列表含载荷;用随机前缀键并清理)。

### go/db/internal/kafka/retry_ownership_test.go
TestRetryOwnership_ClaimFromUsesGivenListAndRenewsLease;TestRetryOwnership_ClaimDelegatesToReadyKey。既有用例全部不改、须保持通过。

### go/db/internal/kafka/key_ordered_consumer_test.go + placement_route_test.go
key_ordered_consumer_test.go 不改,既有用例(含 TestRetryConsumer_*、TestHandleTask_CanceledClaimCannotReachDB、TestPoisonKafkaPayloadIsPersistedBeforeAck、TestConcurrentWorkers_FinalStateIsGlobalMax)必须无新增失败——零值字段 = 旧语义(retryRouting ""=shared、owned nil、durable nil=directEnqueue)。placement_route_test.go 只改 4 处 saveToRetryQueue 调用,断言不变。

### go/db/internal/config/config_test.go
TestResolveRetryRouting;TestRedisWaitAOFDefaultsWhenMissing;TestRedisWaitAOFExplicitZeroDisables;TestConfigValidateRejects;TestEtcYamlRetryKeysMatchDefaults。

### go/db/internal/metrics/metrics_test.go
TestRetryRoutingSeriesPrecreated(四个新计数/仪表的 label 组合预建且取值集合固定)。

### tools/merge_zone/merge_unit_test.go
TestDbTaskQueueKeysCoverEveryInstance(补 nil 参数);TestDbTaskQueueKeysIncludePartitionedReadyLists;TestDbRetryPartitionQueuePattern(含 zone_1 不串 zone_11 与 _g2、glob 转义);TestDbTaskQueueKeysScanFailureFailsClosed;TestDbTaskQueueKeysScanPagesAggregated。

### tools/scripts/tests/k8s_deploy_contract.tests.ps1
db ConfigMap 的 RetryRouting 与三项 WaitAOF == go/db/etc/db.yaml;Test-DbReplicaPartitionBudget 11>10 throw 且文本含「replicas ≤ PartitionCnt」;10==10 放行、PartitionCnt=0 throw;仓库 manifest 副本数 ≤ db.yaml PartitionCnt(Assert-DbReplicasWithinPartitionContract 不抛)。

### 本机端到端(Codex,非文件)
双 db owner 模式 + 测试玩家排序锁注入 30 秒 + taskkill /F 一个实例 + verifier strict;shared 回滚演练(owner 造 p* 积压 → 两实例改 shared 重启 → p* 排空);retry_requeue 工具演练;WAITAOF 拒启演练(可选,必须还原 appendonly);shared 模式重跑一遍(允许 executed_nonowner>0)。命令与通过标准见 rollout。

## 上线与回滚
【0. 前置(Codex,线上不动)】
a) 按 docs/handoff/placement-reverify-checklist-20261012.md 跑通第 12 步「本机双 db 实例」及第 3 节缺口项,结果写回 PROGRESS(对应交接 §4.1 第 7 行)。
b) 查清并记录线上各 zone 的 go/db 镜像与副本数;低于 be6b4ab1d 的 zone 先按瓶颈文档 §4.5 处理,S1 不与「副本 1→2」同批。不在 TiDB Phase 1 迁移窗口内。
c) 确认 Redis:K8s `redis-0` 的 redis_version ≥ 7.2、appendonly=yes(按 redis.yaml 的口令注入方式在容器内执行 INFO server / CONFIG GET appendonly,口令不进命令行与日志)。

【1. Codex 构建与测试(按序执行,失败即停,不连续重试)】
前置:Go 1.27.2(`C:\Program Files\Go\bin\go.exe`,`go version` 应为 go1.27.2 windows/amd64);PowerShell 7;docker compose 的 redis(redis:latest,--appendonly yes;`docker exec redis redis-server --version` ≥ 7.2)。
(1) 工作目录 D:\luyuan\xuanming-server-mmo-cpp\go\db:
  gofmt -l ./internal ./cmd ./db.go            # 期望无输出
  go vet ./...
  go build ./...
  go test -count=1 ./internal/kafka/... ./internal/config/... ./internal/metrics/... ./cmd/...
  go test -count=1 -v -run 'TestClaimLease|TestOwnedPartitions|TestRetryRouting|TestRequeuePartitioned|TestRetryQueueDepths|TestConsumeClaim|TestRetryOwnership|TestAOFGroupEnqueue|TestDirectEnqueue|TestProbeAOFAck|TestCheckWaitAOFReply|TestWaitAOFBudgetMs|TestDeferFailedTask_|TestQuarantine_|TestPoison_|TestConcurrentWorkers|TestRetryConsumer' ./internal/kafka/
  $env:CGO_ENABLED='1'; go test -count=1 -race -run 'TestOwnedPartitions|TestClaimLease|TestRetryRouting|TestConsumeClaim|TestAOFGroupEnqueue|TestConcurrentWorkers' ./internal/kafka/   # 无 gcc 时在 Linux/WSL 跑并注明平台
  $env:GODB_REDIS_ITEST_ADDR='127.0.0.1:6379'; go test -count=1 -v -run 'TestDurableEnqueue_RealRedis' ./internal/kafka/   # 必须 PASS,不得 SKIP
(2) 工作目录 tools\merge_zone:go vet ./... ; go test -count=1 ./...
(3) 仓库根:pwsh -NoProfile -File tools/scripts/tests/k8s_deploy_contract.tests.ps1
通过标准:gofmt 无输出;全部 PASS;新增用例名都出现在 -v 输出且不是 SKIP;契约测试用例数 = 原有 + 新增,0 失败。失败时保留:失败用例的 `go test -v` 全文、编译错误前 20 行、契约测试完整输出;归因时区分 S1 与他人改动。
(4) 本机端到端(先按 AGENTS §6.2:redis-cli FLUSHALL、清空测试库、清 etcd、删 tools/scripts/.run 的 pid/log):
  $env:DB_RETRY_ROUTING='owner'; pwsh -NoProfile -File tools/scripts/go_services.ps1 -Command start-exe -Services db -Counts db=2
  # 两个实例日志各有 "retry routing: mode=owner source=env DB_RETRY_ROUTING";/metrics 有 db_retry_routing_mode{mode="owner"} 1
  cd go/db; go run ./cmd/data_stress -zone 1 -players 200 -writes 50 -concurrency 32
  # 压测中对 10 个测试玩家持锁 30 秒(MsgType 取 go/db 日志 task processed 行或缓存键 {MsgType}:{Key} 前缀):
  docker exec redis redis-cli SET "distributed:lock:kafka:ordering:db_task_zone_1:<pid>:<MsgType>" e2e-hold PX 30000
  # 持锁期间 taskkill /F /PID <其中一个 db 的 PID>
  go run ./cmd/verifier -config etc/db.yaml -mode strict -wait 180s
  # 抓两个实例 /metrics 过滤 db_retry_ / db_owned_partitions / db_durable_enqueue_;键:docker exec redis redis-cli --scan --pattern 'kafka:retry:queue:db_task_zone_1*' 逐个 LLEN,及 kafka:retry:processing:db_task_zone_1:*、kafka:dead:queue:db_task_zone_1
  通过:verifier 0 mismatch;两实例 executed_nonowner=0;所有 ready/p*/processing/dead LLEN=0;被杀实例有在途收据时存活实例打出 "reclaimed abandoned retry receipts";无 "processing receipt not found";db_durable_enqueue_total{result!="ok"}=0。
  回滚演练:owner 下持锁造出 p* 积压 → 停两实例 → $env:DB_RETRY_ROUTING='shared' 重启 → p* 排空、verifier 0 mismatch。工具演练:owner 造积压 → 全停 → go run ./cmd/retry_requeue -f etc/db.yaml -check,再不带 -check 执行 → p*=0 → shared 启动 → verifier 通过。
  WAITAOF 拒启演练(可选):docker exec redis redis-cli CONFIG SET appendonly no → 起一个 db 必须拒启且日志含 WAITAOF 提示 → 必须执行 docker exec redis redis-cli CONFIG SET appendonly yes 还原。
  再用 RetryRouting=shared 整跑一遍第 (4) 步,标准相同(executed_nonowner 允许 >0)。失败保留:两实例 stdout/stderr、各时刻 metrics 快照、verifier 输出、Redis 键清单。

【2. 生产发布(逐 zone、低峰、一次一个 Pod,遵守瓶颈文档 §4.5 与交接 §3.5)】
第 1 步 kind/staging 演练:上面 (4) 的锁注入、kill -9、MySQL 暂停、回滚与工具演练全部在这里做;生产不做任何注入。
第 2 步 发布 S1 镜像,db.yaml RetryRouting=shared、WaitAOF 1/0/1500(k8s_deploy zone-up 生成 ConfigMap;镜像变化会触发滚动)。每换一个 Pod 核对:db_retry_routing_mode{mode="shared"}=1、db_retry_queue_length{queue="partitioned"}=0、db_durable_enqueue_total{result!="ok"} 不增、启动未拒(探针通过);记录基线 executed_nonowner/executed 比例、db_ordering_lock_busy_total、死信增速、db_task_result_total{result="error"}、Kafka lag。回滚:回旧镜像(不会残留 p*)。
第 3 步 金丝雀(建议在该 zone 完成 S2 切组之后):前提是第 2 步完成且该 zone 每个 Pod 都暴露 db_retry_routing_mode;冻结其它 zone 发布;`kubectl -n mmorpg-zone-<z> set env deploy/db DB_RETRY_ROUTING=owner` + `kubectl -n mmorpg-zone-<z> rollout status deploy/db`。只被动观察:稳定后各 Pod db_owned_partitions 之和 = PartitionCnt;executed_nonowner=0;forwarded 后共享收件箱 1–2 拍归零;requeued_revoked 只在再平衡时刻出现;ordering_lock_busy{origin="retry"} 接近 0;死信速率不高于基线;无 "processing receipt not found"。需要积压时只对合成测试玩家持带 TTL 的排序锁。回滚:`kubectl -n mmorpg-zone-<z> set env deploy/db DB_RETRY_ROUTING-`(触发滚动),shared 实例自动排空 p*。
第 4 步 全量:db.yaml 改 RetryRouting: owner → k8s_deploy zone-up → `kubectl -n mmorpg-zone-<z> rollout restart deploy/db` + `rollout status`(只改 ConfigMap 不会滚动)→ 去掉金丝雀 env(`set env … DB_RETRY_ROUTING-`)。验收:每个 Pod 启动日志 "retry routing: mode=owner source=config",24 小时内 executed_nonowner=0、死信速率不升、p* 无长期积压、lag=0、DbDurableEnqueueNotConfirmed 不响。回滚:db.yaml 改回 shared → zone-up → rollout restart + status。
回滚到 S1 之前的镜像:必须先完成上面的「切回 shared」,确认 db_retry_queue_length{queue="partitioned"}=0 且 `go run ./cmd/retry_requeue -check`(经 kubectl port-forward 到 Redis,口令用 -redis-password-env)显示 p* 全 0,再回滚镜像。紧急情况(新镜像 shared 模式本身有缺陷):回滚镜像后立刻执行 retry_requeue 搬回(可重入),并在搬回完成前不清理任何 Redis 键。
告警:apply deploy/k8s/db-task-retry-alerts.yaml(PrometheusRule)。
其它:上线后 Java 仓库 PARITY.md 登记「不适用」;S1 第 2 步起 WAITAOF 即生效,redis-ha 接入时把 ConfigMap 的 WaitAOFNumReplicas 改 1 并重新评估(见 risks)。

## 待决项（负责人已按推荐默认定）

## 风险
1) 文件数:本仓库 28 个(新增 6)+ Java 仓库 PARITY.md 1 行,低于 AGENTS §10.2 的 30 门槛但余量很小;实现中若需再加文件(例如给 go-svc 加 ConfigMap 指纹、改 L-04 设计稿 §9、stress_summarize)导致 ≥30,须停下报告。
2) WAITAOF fail-closed 的可用性代价:确认不足时 claim 失败,在 S2 落地前会触发全服再平衡;Redis fsync 卡顿或(将来 redis-ha 且 NumReplicas=1 时)副本不可用,会让失败路径上的分区反复停滞。K8s 现为单 Redis、NumReplicas=0,只需本地 fsync,概率低;DbDurableEnqueueNotConfirmed 为 critical。
3) 重复载荷:组提交在网络错误/份数不足时整批重发 LPUSH,Kafka 重投也会再入队一次,重试队列与死信可能出现同一任务多份;执行侧由 seq 游标/批内合并/epoch 收敛,死信重复需人工按 taskID 或 (topic,partition,offset) 去重。
4) 本机没开 AOF 的开发环境默认会拒启(需显式写 0/0);compose 与 K8s 均已开 AOF。appendfsync 非 everysec 的部署(如 no)WAITAOF 本地确认行为未核实,探针会拦。
5) 专用 Redis 客户端每个 go/db 实例多 2 条连接,计入共享 Redis 的 maxclients 账(该实例未设 maxclients,见全局单点交接)。
6) 重试集中到属主后,MySQL 恢复期的重试排空并发下降(非属主实例不再额外执行);staging 演练对比 p* 回落时间,必要时按 B0 调 SubShardCount(本轮不做)。
7) 共享消费组下(S2 之前)任一 zone 的 Pod 进出都会让 owner 模式出现 requeued_revoked 尖峰并暂停重试排空;建议切 owner 排在 S2 之后。
8) 收据搬移失败(Redis 错误)时收据留在本实例 processing,直到停机 release 才归还(本实例持续续租,别人不会回收)——与现有 deferFailedTask 的失败语义相同,未新增自扫描。
9) 实例全部回到 S1 之前的镜像后没人上报 partitioned,DbRetryPartitionedNoProgress 会哑火;只能靠回滚规程(先切 shared 并 -check 归零)与紧急搬回工具兜底。
10) claim 撤销后的残余重叠窗口(session 超时 + 10 秒排空,MySQL 读超时 30 秒仍可能使排空超时)仍只靠排序锁租约,彻底封死要靠 MySQL 条件写。
11) inflight 漏调 finishRetry 会让每次再平衡等满排空超时、全服停顿变长;已用 defer 收口并有逐出口用例,但 S4 改停机流程时可能破坏这些调用点(见 ordering_and_overlap)。
12) merge_zone 的 p* 检查依赖完整 SCAN;SCAN 失败 fail-closed,不做 p0..p{N-1} 枚举。
13) 与 S2/S3/S4 同改 KOC、metrics.go、config.go、db.yaml、k8s_deploy.ps1、契约测试、preflight.go,合并冲突概率高,须按建议顺序或列出的不变量 rebase。
14) 以上全部是静态阅读代码得出的设计;未编译、未测试、未压测,须等 Codex 结果,不得声称「编译通过」「测试绿」。
