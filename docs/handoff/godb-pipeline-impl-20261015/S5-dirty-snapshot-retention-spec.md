# S5 实现规格：S5 scene 发 DBTask 失败的脏快照 + db_task retention.bytes / Kafka PVC

## 摘要
S5 拆成两个互不依赖的 PR,各自都在 30 个文件以内,原因是 AGENTS §10.2 的 30 文件门槛。
S5a(C++ scene,约 23 个文件):
- 新增一本按玩家记的 DBTask 投递账本(db_task_dispatch_ledger,纯数据结构),加一层粘合(DbTaskDispatchSystem)。
- 在 KafkaProducer 上加逐条回执能力:关联号经 msg_opaque 带回,DeliveryTracker 保证每条已入队消息恰好回执一次,实例重建时合成失败回执。
- 某次存盘的 DBTask 同步被拒或回执失败时,该玩家标记「需要重存」。脏比较快路径因此失效,下一次存盘一定按当前内存整份重发,绝不重放旧载荷。
- 下线收尾要等本玩家所有在途 DBTask 拿到回执才销毁实体(DecideDbTaskGate);投递失败时有界重存,超限后保留实体。
- 实体走别的路径销毁后才到的失败回执,记 orphan 计数并打 WARN;新属主的首存一定写盘,所以 MySQL 会自愈。
- 不再像任务卡片原文那样「作废快照」:PlayerLastPersistedSnapshotComp 的含义是「已进 Redis」,战斗结算和资产通道靠它销账;而且 Redis 落地回调会把它重新写回,作废了也会被覆盖。
S5b(Go + 部署,约 16 个文件):
- kafkautil.TopicSpec 增加 RetentionBytes。db 和 login 启动时对 db_task topic 显式设置 topic 级 retention.bytes。
- k8s_deploy.ps1 新增 Kafka 盘预算 Get-KafkaDiskPlan。公式取自容量模型 A0-3:PVC ≥ 1.3 × RF × Σ(分区数 ×(retention.bytes + segment))÷ broker 数,另加审计 topic 与其它 topic 的预留。
- kafka.yaml 的 PVC 大小改成占位符:新集群按公式算;已有集群沿用线上大小,并把 retention.bytes 按盘收缩,同时打 WARN。显式扩盘时拒绝并给出 runbook。
- 顺手修 db 与 login 两个 ConfigMap 里 RetentionMs 来源不同、保留期来回横跳的既有缺陷。
两个 PR 都不碰 go/db 消费端、markAppliedSeq / markAppliedOwnerEpoch、proto、mysql.yaml 和连接池。全部未编译,构建与测试交 Codex。

## 现状
行号按当前工作区。多个会话正在写同一批文件,行号会漂移,落码时按函数名重新 grep。

【a. 脏快照】
- 存盘入口:cpp/libs/services/scene/player/system/player_lifecycle.cpp `SavePlayerToRedisImpl`(约 L3871)。
  - 另一会话未提交的改动已让它在 `MarshalPlayerForSave` 失败时 return false(约 L3910-3923)。
  - 脏比较快路径约在 L3960-3975:`try_get<PlayerLastPersistedSnapshotComp>` 后 `dirty_save::IsEqual(*message, *snap->snapshot)`,相等就 return false,Redis 和 DBTask 都不写。
  - 发 DBTask 的 lambda `sendSubTableTask` 约在 L4020-4066,每次存盘发两条:player_database、player_database_1。
  - 发送调用 `KafkaProducer::Instance().send(dbTaskTopic, dbTaskBytes, playerIdStr)` 在约 L4059,失败只打 LOG_ERROR(约 L4060-4065);函数在约 L4072 照常 return true。
- 快照更新点:`HandlePlayerAsyncSaved`(约 L2070)末尾,Redis 落地后 `get_or_emplace<PlayerLastPersistedSnapshotComp>(...).Replace(message)`(约 L2285-2290);退出 kResave 分支另有一处(约 L2241)。
  - 所以 Kafka 失败后快照照样前移,下一次存盘被快路径跳过 = 任务卡片说的问题(handoff-launch-readiness-and-d1-20261013.md:138;PROGRESS.md:7118)。
  - Redis 失败路径 `HandlePlayerAsyncSaveFailed`(约 L1957-1990)刻意不动快照,但 Kafka 路径没有对等处理。
- 快照的消费方:
  - player_battle.cpp:330-385、asset_op_system.cpp:182-188 把快照当作「字节确实在 Redis」来销账。
  - last_persisted_snapshot_comp.h:43 的组件只在落地后 Replace。
- 生产者:cpp/libs/engine/infra/messaging/kafka/kafka_producer.cpp。
  - `produceOnce`(L106-123)的 msg_opaque 恒为 nullptr。
  - `dr_cb`(L305-332)只计 `kafka_producer_stats::DeliveryFailed` 并打限流日志,不通知调用方。
  - `rebuildAfterFatal`(L188-283)对来不及派发的 outq_len 只一次性计数(L230-237)。
  - 析构(L96-104)flush 5s。
  - 回执只在本线程的 poll() / flush() 里派发,而 send() 是唯一的驱动点(L171-175;kafka_producer.h 约 L83 的线程模型注释)。空闲节点上回执会一直延迟。
  - 头文件「投递语义」明确要求不重发旧载荷,补救靠重发当前状态。
  - 另一会话未提交的改动在 kafka_producer.h 加了 brokersConfigured()。
- 周期存盘:scene/core/system/redis.cpp:146-196。
  - `SCENE_PLAYER_SAVE_INTERVAL_SECONDS` 缺省 300,按 playerId % 300 每秒一槽,遍历 tlsEcs.playerList。
  - 1s 重试定时器在 L55-74,30s 统计行在 L78-131。
  - `BeginShutdown` 只取消周期存盘(L198-213);`Shutdown` 在 L215-240。
  - scene main.cpp:121 Initialize、:149 BeginShutdown、:367 Shutdown;停机谓词在 :174-260(kafkaDrained = FlushProducer(0) 加 pending==0)。
- 退出收尾:
  - `FinishExitAfterPersist`(约 L3097)有 5 个调用点:HandlePlayerAsyncSaved 的 kFinish(L2228)、kResave 收敛(L2257)、HandleExitGameNode 超限 rekick(L2978)、快路径内联收尾(L3069)、HandlePlayerSaveRejected(L4104)。
  - 唯一的实体销毁出口是 `DestroyPlayer`(约 L2815)。
  - 退出意图 player_exit_intent.h:48-79;`DecideAfterPersist` 在 :193-210,`ShouldRekickExhaustedExit` 在 :216-219,`kMaxExitResaveRounds = 5`(:108)。
- go/db 能挡什么(go/db/internal/kafka/key_ordered_consumer.go):
  - 游标 `orderingForTask` / `markAppliedSeq`(L478-526)比的是同分区 offset,只挡 offset ≤ 游标的重复投递。重新发送会拿到更大的 offset,一律放行。
  - `checkOwnerEpoch` / `guardOwnerEpoch`(L629-697)只在 taskEpoch < 已落库最大 epoch 时拒绝。
  - 同节点重登不铸造新 epoch(go/scene_manager/internal/logic/enterscenelogic.go 约 L660-690:`mint: !samePhysicalNode || sameNodeZeroMint`)。
  - 结论:同 epoch、更大 offset 的旧载荷没有任何守卫能挡,所以绝不能重放旧字节。
- Java:PARITY.md:63 写明 Java「在线存盘失败作废比对基准,下个周期无条件重写」,Java 没有 Kafka 存盘链。

【b. retention / PVC】
- db_task topic 只由 Go 建:
  - go/shared/kafkautil/topic_init.go:19-24 的 `TopicSpec` 只有 RetentionMs。
  - `ensureTopics`(:93-216)建 topic 时只带 retention.ms;每次启动用 IncrementalAlterConfig 只 SET retention.ms(:194-213)。
  - 调用方 go/db/db.go:78-87 和 go/login/login.go:98-107,每次启动都会调。
- C++ 不显式建 topic。broker 开着 auto.create(num.partitions=1),有「scene 先发就被建成 1 分区」的既有风险(Apply-KafkaTopicInitJob 注释)。
- deploy/k8s/manifests/infra/kafka-topic-init.yaml 只建审计 topic 和控制面 topic,镜像在 :61;deploy/docker-compose.yml 同样不建 db_task,本机 broker 缺省 KAFKA_LOG_RETENTION_BYTES=128MiB(:38),镜像在 :6 / :83。
- deploy/k8s/manifests/infra/kafka.yaml 已经是 StatefulSet + PVC,不是 emptyDir:
  - volumeClaimTemplates 写死 `storage: 20Gi`(:356-363);
  - KAFKA_LOG_RETENTION_BYTES / SEGMENT_BYTES 是占位符(:277-280);
  - 镜像 `apache/kafka:latest`(:156)。
- tools/scripts/k8s_deploy.ps1:
  - 参数在 :61-67;`Apply-KafkaProfileDefaults` 在 :1695-1731。db_task 的 retention.ms:dev 1h、prod-like 6h、prod 24h;broker 每分区 retention.bytes:dev 128MiB、其余 512MiB;segment:16MiB / 32MiB。
  - kafka.yaml 的占位替换在 `Apply-Infra` 约 :6660-6676。
  - db ConfigMap 的 Kafka 段(约 :4097-4105)不写 RetentionMs,回落到 Go 缺省 24h(go/db/internal/config/config.go:234);login ConfigMap(约 :4387)写的是 `$dbTaskRetentionMs`。dev / prod-like 档下 topic 的 retention.ms 因此随两个服务的重启顺序在 1h / 6h 与 24h 之间横跳(:1720-1725 的注释只对 prod 成立)。
  - `Get-InfraZoneIds` 在约 :6284。
- 「一节点一 topic」指的是旧的消费路径:
  - 每个 gate / scene 节点一个 `gate-<node_id>` / `scene-<node_id>` topic,外加一个消费组(cpp/libs/engine/core/node/system/node/node_kafka_command_handler.h:113-136;组名取 Kafka.GroupID,:126)。
  - 开关 `Kafka.DisableLegacyPerNodeTopic`:bin/etc/base_deploy_config.yaml:104 为 false;K8s 的 node ConfigMap 不写这个键,等于 false;config.cpp:226-228。
  - 生产者已全部切走(base_deploy_config.yaml:94-103)。
- 容量依据:
  - capacity-model-hok-scale.md:230:实际缓冲 T_buf = 10 × 512MiB ÷ 每 zone 字节率,45k 一区 14 分钟~1.9 小时,100k 一区 6~50 分钟;
  - capacity-model-hok-scale.md:231:Kafka 盘的保留上限是 PVC 的 2.5~5.8 倍;
  - A0-3 的公式在 :405-406;
  - 全局单点交接 :77 要求 db_task 缓冲 ≥ 2h。
- go/db 没有导出消费 lag 指标(metrics.go 里没有 lag)。

## 复核阻断项 / 修正的处理
- **B-01**：任务卡片(launch-readiness §4 第 10 项):scene 发 DBTask 失败时脏比较快照不作废,下一次存盘因为「没变化」被跳过,MySQL 永久落后
  - 处理：新增按玩家记账的 `db_task_dispatch::Ledger`。状态里有 lastFullyQueued 和 maxFailed 两个代号,`NeedsResave = maxFailed != 0 && maxFailed >= lastFullyQueued`。SavePlayerToRedisImpl 的快路径改为调用 `db_task_dispatch::AllowsDirtySaveSkip(allowSkipWhenPersisted, snapshotMatches, NeedsResave(pid))`,有未送达的失败就强制真写。只有之后某次存盘的两条 DBTask 全部同步入队,且没有代号 ≥ 它的失败回执,NeedsResave 才自然解除。用代号比较而不是布尔清零,与回执先到还是 FinishSave 先到无关。
- **B-02**：PROGRESS 10-12 的发现:SavePlayerToRedis 在 Kafka 失败时仍返回 true;投递回执失败不重发;快照在 Redis 写成功后照常更新
  - 处理：返回值语义保持「已发出 Redis 存盘、之后会有 HandlePlayerAsyncSaved 回调」不变。5 个调用方都按这个含义收尾,改动它会牵连退出、交接、资产通道。Kafka 的结果由账本单独记:同步失败走 SendDbTask 返回值;异步失败走带关联号的投递回执(sendTracked + DeliveryTracker)。
- **B-03**：「失败即作废快照」
  - 处理：按字面实现是错的,改用独立标记。理由一:快照的语义是「字节确实已进 Redis」,player_battle.cpp:330-385 和 asset_op_system.cpp:182-188 拿它判 durable 并销账,作废它会推迟或重做销账。理由二:Redis 落地回调 HandlePlayerAsyncSaved 会在 DBTask 失败之后再次 Replace,作废会被覆盖,这正是现缺陷的成因。所以快照语义不变,另用账本标记驱动强制重存。
- **B-04**：处理「同步发送成功、但投递回执失败」
  - 处理：KafkaProducer::sendTracked 把关联号放进 msg_opaque,并在入队后、poll 之前 Accept 进 DeliveryTracker。dr_cb 里 Resolve 后回调监听器,成功和失败都回调。rebuildAfterFatal 在 purge/flush 之后用 TakeAll 为来不及派发的关联号合成 ERR__PURGE_QUEUE 失败。scene 侧有票据超时兜底 kTicketTimeout = 330s(大于 librdkafka 缺省的 message.timeout.ms 300s)。还新增 50ms 的 Pump 定时器调 KafkaProducer::poll(),解决空闲节点上回执只在 send() 里派发的问题。
- **B-05**：实体已销毁的回调(回调里用 try_get;per-tick 路径不用 get_or_emplace)
  - 处理：监听器在 dr_cb 或重建栈里同步执行,只改账本、只把玩家号压进待办队列,不碰 ECS。待办统一由 Pump 在定时器顶层交给 PlayerLifecycleSystem::OnDbTaskDispatchSettled 处理,里面只用 tlsEcs.GetPlayer + valid + try_get,不新增任何 get_or_emplace。实体不在时调 DbTaskDispatchSystem::OnPlayerGone:没有在途票据就回收条目;如果有未解决的失败,计 orphan_unsynced 并打 WARN。DestroyPlayer(唯一销毁出口)也调 OnPlayerGone。下线收尾有闸门,不再产生孤儿;只有被废黜、交接放行这些路径会出孤儿,新属主首存一定写盘,MySQL 会自愈。
- **B-06**：下线最终存盘失败
  - 处理：FinishExitAfterPersist 的 5 个调用点全部改走 FinishExitOrAwaitDbTaskAck,决策见 player_exit::DecideDbTaskGate:
1. NeedsResave 且未到 3 轮 → kResave,强制重存;
2. NeedsResave 且已到 3 轮 → kRetainExhausted,保留实体;之后靠周期存盘(NeedsResave 会强制真写)、HandleExitGameNode 的 dispatch rekick、或停机看门狗收尾;
3. 只有在途回执 → kAwaitAck,等 Pump 把回执处理完后再恢复收尾;
4. 都没有 → kFinish。
SavePlayerToRedisImpl 返回 false(交接已发起,或持久化组件缺失)时没法再重存,直接收尾并计 exit_unsynced,Redis 保留最后一次落地的状态。停机时 DbTaskDispatchSystem::Shutdown 打一行 ERROR,列出未落定的玩家号(最多 50 个)。
- **B-07**：重复发送的幂等:go/db 的游标 / owner_epoch 能否挡住旧写重放
  - 处理：不能。游标按同分区 offset 比较,重发的旧载荷 offset 更大、照样放行;owner_epoch 只拒绝低于已落库最大值的写,同节点重登不铸造新 epoch。所以本设计绝不重放旧字节,强制重存一律重新 marshal 当前内存。重发当前状态是幂等的:整行覆盖,内容相同或更新,游标前进,epoch 判 match。librdkafka 的内部重试由幂等生产者去重。回执失败的消息里有一类是「可能已持久化」(ERR__MSG_TIMED_OUT),即使它其实已写进日志,后面的整份重写按序覆盖,结果一致。条件写落地后,强制重存同样经过 SavePlayerToRedisImpl,拿到新的 save_seq。go/db 不改。
- **B-08**：回调生命周期遵守 AGENTS §11.7(只绑 weak_ptr,不裸绑 this)
  - 处理：不涉及连接对象。监听器是无捕获 lambda,转调静态函数。Pump 定时器的 lambda 也无捕获。两者都不绑 this 或裸指针。DbTaskDispatchSystem::Shutdown 先注销监听器;它在 RedisSystem::Shutdown 的 loop_ 有值的分支里调用,main.cpp:367 会显式调到,析构里第二次调用是空操作。KafkaProducer 析构时第一行先清监听器,再 flush,不依赖 thread_local 的析构顺序。
- **B-09**：新增文件同步 CMakeLists.txt 与 .vcxproj
  - 处理：scene 库新增 db_task_dispatch_ledger.cpp 和 db_task_dispatch_system.cpp:登记进 cpp/libs/services/scene/CMakeLists.txt;两个 .cpp 和两个 .h 登记进 scene.vcxproj 和 .filters。新单测 db_task_dispatch_test.cpp 登记进 cross_zone_test.vcxproj 和 .vcxproj.filters。测试工程只有 MSBuild,没有 CMake。
- **B-10**：容量模型 A0-3 与全局单点 P1:db_task topic 显式设置 topic 级 retention.bytes
  - 处理：TopicSpec.RetentionBytes:0 = 不管理,>0 = 建 topic 时带上并在每次启动时增量 SET,<0 拒绝。db 和 login 的 KafkaConfig 都加 RetentionBytes(optional)。k8s_deploy 把同一个值同时渲染进 db 和 login 两个 ConfigMap。
- **B-11**：A0-3:PVC ≥ Σ(分区数 × retention.bytes × RF) ÷ broker 数 × 1.3,部署脚本加预算门禁
  - 处理：新增纯函数 Get-KafkaDiskPlan,加上 Resolve-KafkaDiskPlan、Write-KafkaDiskLedger(账本行加 WARN)、Assert-KafkaStorageChangeIsSafe。公式在 1.3 余量上另计 segment 超额、审计 topic(按 broker 缺省)和其它 topic 的预留。kafka.yaml 改用 `__KAFKA_STORAGE_SIZE__` 占位符。盘不够时按盘收缩 retention.bytes,保证不会写爆盘(D3)。
- **B-12**：A0-3:lag 告警以秒计,并与 T_buf 对比
  - 处理：本轮不做。go/db 没有导出 lag 指标,换算成秒需要 S4 的测量打点(可复用 go/shared/kafkautil/group_lag.go)。本轮的账本行会打出 implied_db_task_buffer_min,作为以后告警阈值的依据。登记进上线待做。
- **B-13**：launch-readiness §4 第 7 项 / A0-3:关掉旧的「一节点一 topic」路径
  - 处理：已查清是 DisableLegacyPerNodeTopic 控制的旧消费路径(见现状)。负责人这轮的授权只写了「db_task 的 retention.bytes 与 PVC」;它和全局单点 P0-B 的 GroupID 模板改动耦合(global-singletons-p0-findings-20261014.md:1648-1667、:1973「另案」),翻开关前还要逐个环境确认旧 topic 写入与积压为 0。所以本轮不翻(D5)。旧 topic 的残留由 OtherTopicsReserveGi 预留覆盖。
- **B-14**：A0-3:生产端压缩;message.max.bytes 三处对齐
  - 处理：不在负责人这轮的条目里,属于 A2 存盘降量;它会改变载荷和容量公式,需要实测压缩比。本轮不做,登记在风险里。
- **B-15**：k8s_deploy 中 db 与 login 的 RetentionMs 双源漂移(既有缺陷)
  - 处理：db ConfigMap 的 Kafka 段补上 `RetentionMs: ${dbTaskRetentionMs}`,两边再同时加 `RetentionBytes: ${dbTaskRetentionBytes}`,都从同一个脚本变量渲染。契约测试断言两个 ConfigMap 的值相等。同时改正 Apply-KafkaProfileDefaults 里「对齐之后不再漂」的过时注释。
- **B-16**：StatefulSet 的 volumeClaimTemplates 不可变,直接 apply 新大小会被 kubectl 拒绝
  - 处理：自动模式下线上已有 StatefulSet 就沿用线上大小,渲染值等于线上值,apply 不冲突。显式传 -KafkaStorageGi 且与线上不同时,Get-KafkaStorageChangeVerdict 返回 expand-refused 或 shrink-refused,throw 并附扩盘 runbook:patch 各 PVC → `delete statefulset kafka --cascade=orphan` → 带 -KafkaStorageGi 重跑 infra-up。
- **B-17**：在小盘的现网上把 topic 级 retention.bytes 抬到超过 broker 缺省值,可能写爆盘
  - 处理：rb_effective = min(目标值, 按盘可容纳值)。按盘可容纳值保证 Σ ≤ PVC ÷ 1.3,不会超出。连最少 2 个 segment 都放不下时 throw。
- **B-18**：godb 交接 §9.2 写「retention / PVC 按原文归全局单点交接」;§2.3 写「本文只提供回档 / 重放需要的最短缓冲时长」
  - 处理：负责人 2026-10-13 已改判归本交接(godb-pipeline-launch-todo.md §3)。交付说明里要写明,请负责人通知全局单点会话不要重做。交接承诺的最短缓冲时长此前并没写出来,本规格定为 T_buf = max(能容忍的 DB 故障时长 = retention.ms, 回档 Step 5 拨回 TargetTime 需要的窗口),缺省目标 2h(全局单点 :77)。账本行打出按设计速率的实际缓冲,作为回档 TargetTime 的上限依据。
- **B-19**：附录里的复核阻断项与修正
  - 处理：附录没有 S5 专节:④③①⑤② / B / C 分别归 S1、S2、S3 等。与 S5 文件相关的有:
- ③ F9:Kafka 镜像钉版本,以及 go-zero conf.MustLoad 遇到未知键是忽略还是报错。钉版本归 S2。未知键会影响 S5b 的回滚(旧二进制读到新键 RetentionBytes),列为 Codex 验证项。
- ③ 的步骤 2 / 3 / 7:都改 k8s_deploy 的 db / login ConfigMap 和 login.yaml,是文件重叠,见 ordering_and_overlap。
- B mysql-fencing 的 B2(save_seq 播种):强制重存经过同一个 SavePlayerToRedisImpl,本项不用额外处理,但要与那个会话协调同一个 lambda。
其余附录条目不涉及 S5。
- **B-20**：AGENTS §10.2:预计改 30 个以上文件要先停下、拿授权
  - 处理：去重后全部约 35 个仓内文件,外加 Java PARITY 1 个。拆成 S5a(约 23 个)和 S5b(约 16 个)两个 PR(D4)。
- **B-21**：AGENTS §12:Java 版同批登记
  - 处理：在 Java 仓库 PARITY.md 末尾追加两行:
- 「DBTask 投递失败强制重存与下线等回执」:不适用。Java 由 scene 同步写 MySQL;在线存盘失败已作废比对基准(PARITY 第 63 行);最终写回失败后放弃重试,仍是 Java 既有的待做项。
- 「db_task retention.bytes 与 Kafka PVC 预算」:不适用,Java 没有 db_task topic;Java 自己 Kafka(battle 结果等)的保留与盘预算,登记为「待自查」。
- **B-22**：C++ 自动建 db_task 的风险:scene 在 db / login 之前发消息,broker auto.create 会把 topic 建成 1 分区
  - 处理：既有问题,与本项正交,本轮不改。风险里注明,建议另案:zone-up 时预建 db_task,或关掉 broker 的 auto.create。

## 逐文件改动
### [modify] cpp/libs/engine/infra/messaging/kafka/kafka_producer_policy.h
新增 header-only 纯类 `kafka_producer_policy::DeliveryTracker`(include <unordered_set> 与 <vector>):
- `void Accept(std::uint64_t id)`:id==0 时忽略;
- `bool Resolve(std::uint64_t id)`:erase,返回 id 之前是否在跟踪;
- `std::vector<std::uint64_t> TakeAll()`:取出并清空;
- `std::size_t Size() const`。
成员 `std::unordered_set<std::uint64_t> ids_`。
中文注释写明契约:每个 Accept 过的 id 恰好一次经 Resolve 或 TakeAll 离开。

### [modify] cpp/libs/engine/infra/messaging/kafka/kafka_producer.h
类内新增(保留另一会话已加的 brokersConfigured()):
- `struct DeliveryReport { std::uint64_t correlationId = 0; RdKafka::ErrorCode err = RdKafka::ERR_NO_ERROR; bool synthesized = false; };`
- `using DeliveryListener = std::function<void(const DeliveryReport&)>;`
- `void setDeliveryListener(DeliveryListener listener);`(传 nullptr 即注销)
- `RdKafka::ErrorCode sendTracked(const std::string& topic, const std::string& message, const std::string& key, std::uint64_t correlationId, int32_t partition = RdKafka::Topic::PARTITION_UA);`
- `std::size_t trackedInFlight() const;`
- private 部分:`RdKafka::ErrorCode sendImpl(const std::string&, const std::string&, const std::string&, int32_t, std::uint64_t correlationId);`;`produceOnce` 增加 `void* msgOpaque` 参数;`void notifyDelivery(const DeliveryReport&);`;成员 `kafka_producer_policy::DeliveryTracker deliveryTracker_; DeliveryListener deliveryListener_;`。
- `static_assert(sizeof(std::uintptr_t) >= sizeof(std::uint64_t))`。
类注释补「逐条回执契约」:
- 监听器只在本线程的 poll() / flush() / send()(重建路径)里同步回调;
- 监听器只许记账,不得再调 send / poll / flush,不得改 ECS 结构;
- ERR_NO_ERROR 表示 acks=all 已确认;非零表示没送到或可能没送到,调用方只能重发当前状态,不能重发旧载荷;
- 每条已入队的跟踪消息恰好一次回执;析构时先注销监听器。

依赖：kafka_producer_policy.h DeliveryTracker

### [modify] cpp/libs/engine/infra/messaging/kafka/kafka_producer.cpp
- `produceOnce` 把 msgOpaque 作为 producer_->produce 的最后一个实参(当前位置是 nullptr)。
- `send()` 改为 `return sendImpl(topic, message, key, partition, 0);`;`sendTracked()` 调 `sendImpl(..., correlationId)`。
- `sendImpl` 承接现 send() 的全部逻辑:每次 produceOnce 都传 `reinterpret_cast<void*>(static_cast<std::uintptr_t>(correlationId))`;最终 resp==ERR_NO_ERROR 且 correlationId!=0 时,先 `deliveryTracker_.Accept(correlationId)`,再 poll()(必须在 poll 之前)。
- `dr_cb` 开头:`const auto id = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(message.msg_opaque())); const bool tracked = id != 0 && deliveryTracker_.Resolve(id);`。成功分支 return 前 `if (tracked) notifyDelivery({id, ERR_NO_ERROR, false})`;失败分支在原有计数和日志之后 `if (tracked) notifyDelivery({id, err, false})`。
- `rebuildAfterFatal`:outq_len 补记之后、`producer_.reset()` 之前,`for (id : deliveryTracker_.TakeAll()) notifyDelivery({id, RdKafka::ERR__PURGE_QUEUE, true})`;非空时打一行 LOG_ERROR "[Kafka] N tracked messages lost their delivery report when the fatal producer instance was purged; reported as synthesized failures."
- 析构第一行 `deliveryListener_ = nullptr;`;flush 之后若 deliveryTracker_.Size()>0,LOG_ERROR "[Kafka] producer destroyed with N tracked messages still awaiting delivery reports"。
- 实现 `setDeliveryListener` / `trackedInFlight` / `notifyDelivery`(监听器非空才调)。
- createProducer 加注释:message.timeout.ms 保持 librdkafka 缺省 300000,scene 的 kTicketTimeout = 330s 依赖它,改一处要同步改另一处。

依赖：kafka_producer.h

### [new] cpp/libs/services/scene/player/system/db_task_dispatch_ledger.h
纯账本,不 include ECS 和 rdkafka。namespace db_task_dispatch:
- 类型:`PlayerId = std::uint64_t`、`SaveGeneration = std::uint64_t`、`Ticket = std::uint64_t`、`Clock = std::chrono::steady_clock`、`inline constexpr Ticket kNoTicket = 0`。
- `enum class DeliveryEffect : std::uint8_t { kUnknownTicket, kDelivered, kFailedSuperseded, kFailedNeedsResave, kCount };`,配名字表 `kDeliveryEffectNames`("unknown_ticket","delivered","failed_superseded","failed_needs_resave")和 `static_assert(std::size(...) == kCount)`。不用 X-macro。
- `struct DeliveryOutcome { PlayerId playerId{0}; DeliveryEffect effect{kUnknownTicket}; std::uint32_t inFlightAfter{0}; bool needsResaveAfter{false}; };`
- `struct ForgetResult { bool forgotten{false}; bool hadUnresolvedFailure{false}; };`
- `constexpr bool AllowsDirtySaveSkip(bool allowSkipWhenPersisted, bool snapshotMatches, bool needsResave) { return allowSkipWhenPersisted && snapshotMatches && !needsResave; }`
- `class Ledger` 的公开接口:
  - `SaveGeneration BeginSave(PlayerId)`:按需建条目,返回单调递增的代号;
  - `Ticket IssueTicket(PlayerId, SaveGeneration, Clock::time_point now)`:inFlight+1,票据登记 {player, gen, issuedAt},push 进 issueOrder_;必须在真正 produce 之前调用;
  - `void OnProduceRejected(Ticket)`:票据不认识就什么都不做;认识则 inFlight-1、maxFailed = max(maxFailed, gen)、销票;
  - `void MarkLocalFailure(PlayerId, SaveGeneration)`:maxFailed = max;
  - `void FinishSave(PlayerId, SaveGeneration, bool allQueued)`:allQueued 时 lastFullyQueued = max(lastFullyQueued, gen);
  - `DeliveryOutcome OnDelivery(Ticket, bool delivered)`:票据不认识,或玩家条目已被回收,返回 kUnknownTicket(后者也要销票);否则 inFlight-1;成功返回 kDelivered;失败时 maxFailed = max,gen >= lastFullyQueued 返回 kFailedNeedsResave,否则 kFailedSuperseded;
  - `std::vector<DeliveryOutcome> ExpireStale(Clock::time_point now, Clock::duration timeout)`:从 issueOrder_ 头部弹出已销的票据;遇到 now - issuedAt >= timeout 的按失败调 OnDelivery;碰到第一张没过期的活票据就停;
  - 查询:`bool NeedsResave(PlayerId) const`、`std::uint32_t InFlight(PlayerId) const`、`bool IsSettled(PlayerId) const`;
  - `ForgetResult ForgetIfNoInFlight(PlayerId)`:没有条目返回 {false,false};inFlight>0 返回 {false, NeedsResave};否则 erase 并返回 {true, NeedsResave};
  - 统计:`std::size_t TrackedPlayers() const`、`OutstandingTickets() const`、`UnsettledPlayerCount() const`;`std::vector<PlayerId> UnsettledPlayers(std::size_t limit) const`(按 id 升序截断);`void Clear()`。
- 私有成员:`struct PlayerState { std::uint32_t inFlight{0}; SaveGeneration lastFullyQueued{0}; SaveGeneration maxFailed{0}; };`;NeedsResave 的规则是 maxFailed != 0 && maxFailed >= lastFullyQueued;`struct TicketInfo { PlayerId playerId; SaveGeneration generation; Clock::time_point issuedAt; };`;`std::unordered_map<PlayerId, PlayerState> players_`、`std::unordered_map<Ticket, TicketInfo> tickets_`、`std::deque<Ticket> issueOrder_`、`nextGeneration_{1}`、`nextTicket_{1}`。
- 中文注释写明:inFlight 在 produce 之前就加,所以回执可能在 send() 内部先到;用代号比较,与回执先于还是后于 FinishSave 到达无关;线程模型为单个 loop 线程。

### [new] cpp/libs/services/scene/player/system/db_task_dispatch_ledger.cpp
实现上述 Ledger 的全部成员。inFlight 递减时不得下溢,下溢就 LOG_ERROR 并钳到 0。

依赖：db_task_dispatch_ledger.h

### [new] cpp/libs/services/scene/player/system/db_task_dispatch_system.h
头文件不 include rdkafka。

namespace db_task_dispatch_stats(写法同 owner_epoch_stats,进程级原子计数,不当 Prometheus label):
- `enum class Counter : std::uint8_t { kProduceRejected, kLocalFailure, kDeliveryFailed, kDeliveryFailedSuperseded, kTicketExpired, kForcedResave, kExitAwaitAck, kExitResave, kExitResaveCapped, kExitRekick, kExitUnsynced, kOrphanUnsynced, kCount };`
- 名字表 `kCounterNames`:produce_rejected / local_failure / delivery_failed / delivery_failed_superseded / ticket_expired / forced_resave / exit_await_ack / exit_resave / exit_resave_capped / exit_rekick / exit_unsynced / orphan_unsynced,配 static_assert。
- `void Inc(Counter)`;`struct Snapshot { std::array<std::uint64_t, kCount> values{}; bool operator==; bool AlarmingIncreaseSince(const Snapshot& prev) const; };`——produce_rejected、local_failure、(delivery_failed − superseded)、ticket_expired、exit_resave_capped、exit_unsynced、orphan_unsynced 任一增长即为告警;`Snapshot Read()`。

class DbTaskDispatchSystem(全静态,状态是 .cpp 里的函数内 thread_local):
- `using SettledHandler = std::function<void(Guid)>;`、`struct SendResult { bool queued{false}; std::string error; };`
- `static void Initialize(SettledHandler onSettled)`:登记无捕获监听器 `KafkaProducer::Instance().setDeliveryListener([](const KafkaProducer::DeliveryReport& r){ OnDeliveryReport(r.correlationId, r.err == RdKafka::ERR_NO_ERROR); })`,并保存 onSettled;
- `static void Shutdown()`:幂等;注销监听器;有未落定玩家时打 LOG_ERROR "[DbTaskDispatch] shutting down with N player(s) whose MySQL copy may lag Redis (in-flight or undelivered DB write tasks): ids=[...]"(最多 kShutdownUnsettledLogLimit = 50 个);清空 onSettled;
- `static db_task_dispatch::SaveGeneration BeginSave(Guid)`;
- `static SendResult SendDbTask(Guid playerId, SaveGeneration gen, const std::string& topic, const std::string& payload, const std::string& key)`:IssueTicket(now) → sendTracked;失败时 OnProduceRejected、计 kProduceRejected、error = RdKafka::err2str;
- `static void MarkLocalFailure(Guid, SaveGeneration)`:同时计 kLocalFailure;`static void FinishSave(Guid, SaveGeneration, bool allQueued)`;
- `static bool NeedsResave(Guid)`;`static std::uint32_t InFlight(Guid)`;
- `static void OnPlayerGone(Guid)`:ForgetIfNoInFlight;forgotten 且 hadUnresolvedFailure 时计 kOrphanUnsynced,打 LOG_WARN "[DbTaskDispatch] player X left this node with an undelivered DB write task; MySQL lags Redis until the next owner saves this player (metric=db_task_orphan_unsynced)";
- `static void OnDeliveryReport(db_task_dispatch::Ticket, bool delivered)`:只调 ledger.OnDelivery、计数;inFlightAfter==0 或 kFailedNeedsResave 时把玩家号压进 pending;不碰 ECS、不打逐条日志;
- `static void Pump()`:先 KafkaProducer::Instance().poll();再 ExpireStale(now, kTicketTimeout = 330s),有过期就计 kTicketExpired、每次 Pump 打一行 LOG_ERROR(带条数和首个玩家号,metric=db_task_ticket_expired),并压进 pending;最后把 pending swap 出来、排序去重,逐个调 onSettled;
- `static std::size_t UnsettledPlayerCount()`、`static std::size_t InFlightTickets()`;
- `static db_task_dispatch::Ledger& LedgerForTest()`、`static void ResetForTest()`(清账本和 pending,不动监听器)。

依赖：db_task_dispatch_ledger.h; kafka_producer.h sendTracked/setDeliveryListener

### [new] cpp/libs/services/scene/player/system/db_task_dispatch_system.cpp
实现上述接口。
- 状态放在函数内 thread_local:`struct State { db_task_dispatch::Ledger ledger; std::vector<Guid> pending; SettledHandler onSettled; bool initialized{false}; }`。
- 在 .cpp 里 include messaging/kafka/kafka_producer.h。
- 常量 `kTicketTimeout = std::chrono::seconds(330)`,注释写明「必须大于 message.timeout.ms(librdkafka 缺省 300000,本仓未覆盖)」;`kShutdownUnsettledLogLimit = 50`。
- 计数器用 `std::array<std::atomic<std::uint64_t>, kCount>` 存。
- 日志文本用英文,注释用中文。

依赖：db_task_dispatch_system.h

### [modify] cpp/libs/services/scene/player/system/player_exit_intent.h
PlayerExitIntentComp 新增两个字段:
- `bool awaitingDbTaskAck{false};`:注释写明只由 FinishExitOrAwaitDbTaskAck 置位或清除,OnDbTaskDispatchSettled 据此决定是否恢复收尾;
- `uint8_t dbTaskResaveRounds{0};`:与 resaveRounds 分开计。
文件头的成对约定段补写这两个字段的写入点。

namespace player_exit 新增:
- `inline constexpr uint8_t kMaxExitDbTaskResaveRounds = 3;`
- `enum class DbTaskGateDecision : uint8_t { kFinish, kAwaitAck, kResave, kRetainExhausted, kCount };`,名字表 "finish" / "await_ack" / "resave" / "retain_exhausted",配 static_assert;
- `constexpr DbTaskGateDecision DecideDbTaskGate(uint32_t inFlight, bool needsResave, uint8_t rounds, uint8_t maxRounds)`:needsResave 时 rounds<max 返回 kResave,否则 kRetainExhausted;不需要重存但 inFlight>0 返回 kAwaitAck;其余返回 kFinish;
- `constexpr bool ShouldRekickDbTaskExhausted(bool awaitingDbTaskAck, uint8_t rounds, uint8_t maxRounds, uint32_t inFlight, bool needsResave, bool hasUnsettledSave)`:四个条件同时成立才为真——awaiting && needsResave && rounds >= max && inFlight == 0 && !hasUnsettledSave。

### [modify] cpp/libs/services/scene/player/system/player_lifecycle.h
PlayerLifecycleSystem 新增两个 public static 声明,放在 FinishExitAfterPersist(约 L1372)附近:
- `static bool FinishExitOrAwaitDbTaskAck(Guid playerId);`:返回 true 表示已收尾销毁;返回 false 表示在等回执,或已为投递失败重存,或已保留实体;
- `static void OnDbTaskDispatchSettled(Guid playerId);`:是 DbTaskDispatchSystem::Pump 的回调,只能在 loop 顶层调用。
同时更新 IsSaveInFlight 的注释:在途状态现在也包括「在等 DBTask 回执」。

依赖：player_exit_intent.h

### [modify] cpp/libs/services/scene/player/system/player_lifecycle.cpp
先 include "player/system/db_task_dispatch_system.h"。

① SavePlayerToRedisImpl 的快路径(约 L3960):
- 改成 `snapshotMatches = allowSkipWhenPersisted && snap && snap->HasSnapshot() && dirty_save::IsEqual(...)`(保留短路),`needsResave = DbTaskDispatchSystem::NeedsResave(playerId)`,`if (db_task_dispatch::AllowsDirtySaveSkip(allowSkipWhenPersisted, snapshotMatches, needsResave))` 才走原有的跳过分支;
- snapshotMatches 为真但需要重存时,计 kForcedResave,打 LOG_INFO "[SavePlayerToRedis] forcing a real save for player X: an earlier DB write task was not delivered (metric=db_task_forced_resave)"。

② 发 DBTask 段(约 L4020-4072):
- lambda 之前加 `const auto saveGeneration = DbTaskDispatchSystem::BeginSave(playerId); bool allDbTasksQueued = true;`;
- 两处 Serialize 失败分支加 `DbTaskDispatchSystem::MarkLocalFailure(playerId, saveGeneration); allDbTasksQueued = false;`;
- `KafkaProducer::Instance().send(...)` 换成 `const auto sent = DbTaskDispatchSystem::SendDbTask(playerId, saveGeneration, dbTaskTopic, dbTaskBytes, playerIdStr); if (!sent.queued) { allDbTasksQueued = false; LOG_ERROR(原文保留, err=sent.error); }`;
- 两次 sendSubTableTask 之后调 `DbTaskDispatchSystem::FinishSave(playerId, saveGeneration, allDbTasksQueued)`;没有全部入队时追加一行 LOG_ERROR "...MySQL lags Redis until the next save re-sends the full state (metric=db_task_produce_rejected)";
- 返回值保持 true。

③ HandlePlayerAsyncSaved:
- kFinish(约 L2228)改为 `if (FinishExitOrAwaitDbTaskAck(playerId)) return; break;`,break 后走到函数末尾,快照照常更新;
- kResave 收敛分支(约 L2257)改为 `FinishExitOrAwaitDbTaskAck(playerId); return;`(这里快照在前面已经 Replace 过)。

④ HandleExitGameNode:
- 超限 rekick 内(约 L2978)、快路径内联收尾(约 L3069)都把 FinishExitAfterPersist 换成 FinishExitOrAwaitDbTaskAck;
- 在「已在退出中」分支原 rekick 的 if 后加 else-if:`ShouldRekickDbTaskExhausted(intent->awaitingDbTaskAck, intent->dbTaskResaveRounds, kMaxExitDbTaskResaveRounds, InFlight(*g), NeedsResave(*g), HasUnsettledPlayerSave(*g))` 成立时,`intent->dbTaskResaveRounds = 0`、计 kExitRekick,打 LOG_WARN "...retained with undelivered DB write tasks; re-saving once more (incoming cause=..., metric=exit_db_task_rekick)",然后调 FinishExitOrAwaitDbTaskAck(retainedPlayerId);此后 intent 不再使用。

⑤ HandlePlayerSaveRejected(约 L4104):FinishExitAfterPersist 换成 FinishExitOrAwaitDbTaskAck。

⑥ DestroyPlayer(约 L2815)开头,紧挨 AnomalyDetector::ClearPlayer,调 `DbTaskDispatchSystem::OnPlayerGone(playerId)`。

⑦ 新增 FinishExitOrAwaitDbTaskAck:
- 实体无效时直接 FinishExitAfterPersist 并返回 true;没有 intent 时打 LOG_WARN 后 FinishExitAfterPersist 并返回 true(沿用旧行为);
- 其余按 DecideDbTaskGate(InFlight, NeedsResave, intent->dbTaskResaveRounds, kMax) 分四支:
  - kFinish:清 awaiting → FinishExitAfterPersist → 返回 true;
  - kAwaitAck:首次进入时计 kExitAwaitAck 并打 LOG_INFO "...has N DB write task(s) in flight; finishing exit after Kafka acknowledges them (metric=exit_db_task_await_ack)";置 awaiting;返回 false;
  - kResave:++rounds、置 awaiting、计 kExitResave、打 LOG_WARN(round r/3);`if (SavePlayerToRedisImpl(entity, false)) return false;`;否则计 kExitUnsynced,打 LOG_ERROR "...could not re-save exiting player X; finishing exit with MySQL lagging Redis (metric=exit_db_task_unsynced)",然后 FinishExitAfterPersist,返回 true;
  - kRetainExhausted:只在首次进入时计 kExitResaveCapped 并打 LOG_ERROR(metric=exit_db_task_resave_capped);置 awaiting;返回 false。

⑧ 新增 OnDbTaskDispatchSettled:
- 实体无效 → OnPlayerGone 后返回;
- 实体没有 UnregisterPlayer → 返回;intent 缺失或 !awaitingDbTaskAck → 返回;
- HasUnsettledPlayerSave 为真 → 返回(Redis 落地回调会再判);
- InFlight>0 且不需要重存 → 返回;
- 否则 MarshalPlayerForSave 出 current,对快照做 `player_exit::IsPersistedPayloadCurrent(*snap->snapshot, current)`;marshal 成功但不收敛时打 LOG_INFO,并在 `SavePlayerToRedisImpl(entity,false)` 返回 true 时直接返回;
- 其余情况调 FinishExitOrAwaitDbTaskAck。
- 全程只用 GetPlayer / valid / try_get。

依赖：db_task_dispatch_system.h; player_exit_intent.h; player_lifecycle.h

### [modify] cpp/libs/services/scene/core/system/redis.h
新增成员 `muduo::net::TimerId dbTaskPumpTimerId_; bool dbTaskPumpTimerActive_ = false;`,不用裸指针成员。

### [modify] cpp/libs/services/scene/core/system/redis.cpp
- include db_task_dispatch_system.h。
- Initialize 里,在 SetSaveRejectedCallback 之后调 `DbTaskDispatchSystem::Initialize(&PlayerLifecycleSystem::OnDbTaskDispatchSettled)`。
- 在 1s 重试定时器之后加 `static constexpr double kDbTaskDispatchPumpIntervalSec = 0.05; dbTaskPumpTimerId_ = loop->runEvery(kDbTaskDispatchPumpIntervalSec, [] { DbTaskDispatchSystem::Pump(); }); dbTaskPumpTimerActive_ = true;`。注释写明 BeginShutdown 不取消它,停机 drain 期间要靠它推完退出收尾;lambda 无捕获。
- 30s 统计定时器的捕获列表加 `lastDbTaskDispatch = db_task_dispatch_stats::Snapshot{}`。计数有变化或 unsettled>0 时打一行 `[DbTaskDispatch] produce_rejected=.. local_failure=.. delivery_failed=.. delivery_failed_superseded=.. ticket_expired=.. forced_resave=.. exit_await_ack=.. exit_resave=.. exit_resave_capped=.. exit_rekick=.. exit_unsynced=.. orphan_unsynced=.. unsettled_players=N in_flight_tickets=M`(累计值),AlarmingIncreaseSince 为真时升 WARN,否则 INFO。不动 [DirtySave] 行的解析格式。
- Shutdown 里,在 `if (loop_.has_value())` 块内取消 dbTaskPumpTimer,然后调 DbTaskDispatchSystem::Shutdown()。

依赖：db_task_dispatch_system.h; redis.h

### [modify] cpp/libs/services/scene/CMakeLists.txt
在 player/system/player_lifecycle.cpp 附近登记 `player/system/db_task_dispatch_ledger.cpp` 和 `player/system/db_task_dispatch_system.cpp`,附一行中文注释:漏登记会在 scene 静态库里缺定义。

依赖：新 .cpp

### [modify] cpp/libs/services/scene/scene.vcxproj
加 2 个 ClCompile:player\system\db_task_dispatch_ledger.cpp、player\system\db_task_dispatch_system.cpp;加 2 个 ClInclude:对应的两个 .h。

依赖：新文件

### [modify] cpp/libs/services/scene/scene.vcxproj.filters
四个新文件归到 player\system 所在的同一个 filter 下。

依赖：scene.vcxproj

### [modify] cpp/tests/kafka_command_test/kafka_producer_policy_test.cpp
新增 DeliveryTracker 用例,详见 tests。

依赖：kafka_producer_policy.h

### [new] cpp/tests/cross_zone_test/db_task_dispatch_test.cpp
测试固件 `DbTaskDispatchFixture`:
- SetUp:tlsEcs.Clear()、SessionMap().clear()、DbTaskDispatchSystem::ResetForTest()、DbTaskDispatchSystem::Initialize(&PlayerLifecycleSystem::OnDbTaskDispatchSettled);
- TearDown:再调一次 ResetForTest(),保证不污染 cross_zone_test.cpp 里依赖空账本的 ExitPersistEcs 用例。
辅助函数在本文件内复刻 MakeExitingPlayer / MakeOnlinePlayer / MarshalAsSaved,不改动别人的 cross_zone_test.cpp。用例详见 tests。

依赖：db_task_dispatch_system.*; player_lifecycle.*

### [modify] cpp/tests/cross_zone_test/cross_zone_test.vcxproj
加 `<ClCompile Include="db_task_dispatch_test.cpp" />`。

依赖：db_task_dispatch_test.cpp

### [modify] cpp/tests/cross_zone_test/cross_zone_test.vcxproj.filters
登记 db_task_dispatch_test.cpp。

依赖：cross_zone_test.vcxproj

### [modify] go/shared/kafkautil/topic_init.go
TopicSpec 新增字段 `RetentionBytes int64`,注释:0 = 不管理;>0 = 每分区 topic 级 retention.bytes;<0 拒绝,因为 Kafka 里 -1 表示不限,会让磁盘无界。
ensureTopics 改动:
- 循环开头校验 `spec.RetentionBytes < 0` 就返回 `fmt.Errorf("kafka topic %s has invalid retention.bytes %d (0 = unmanaged, > 0 = bytes per partition)")`,不建也不改;
- 建 topic 时 ConfigEntries 按需放入 retention.ms(>0)和 retention.bytes(>0);
- 增量修改改为按需组装一个 entries map,同时 SET retention.ms 和 retention.bytes;map 非空才调一次 IncrementalAlterConfig;日志改为 "kafka topic %s retention updated: retention.ms=%d retention.bytes=%d"。
不改分区契约和副本数逻辑。

### [modify] go/shared/kafkautil/topic_init_test.go
新增 4 个用例,详见 tests;现有断言不改。

依赖：topic_init.go

### [modify] go/db/internal/config/config.go
KafkaConfig 新增 `RetentionBytes int64 `json:"RetentionBytes,optional"``,中文注释:K8s 由 k8s_deploy.ps1 按 Kafka 盘预算渲染,与 login 同值;本机不设 = 沿用 broker 缺省。字段加在 RetentionMs 之后。注意 S2 会在同一个结构体里加 GroupHandoffFrom。

### [modify] go/db/db.go
EnsureTopics 的 TopicSpec 补 `RetentionBytes: config.AppConfig.ServerConfig.Kafka.RetentionBytes,`。

依赖：go/db config.go; topic_init.go

### [modify] go/db/etc/db.yaml
在 ServerConfig.Kafka 段加一行注释示例 `# RetentionBytes: 0  # 0/不写 = 沿用 broker 的 log.retention.bytes;K8s 由 k8s_deploy.ps1 按 Kafka 盘预算渲染(与 login 同值)`。只写注释,不改本机行为。

### [modify] go/login/internal/config/config.go
KafkaConfig 在 RetentionMs 之后新增 `RetentionBytes int64 `json:"RetentionBytes,optional"``,注释与 db 一致。

### [modify] go/login/login.go
EnsureTopics 的 TopicSpec 补 `RetentionBytes: config.AppConfig.Kafka.RetentionBytes,`。

依赖：go/login config.go; topic_init.go

### [modify] go/login/etc/login.yaml
在 RetentionMs 下面加同样的注释示例行,不改本机行为。

### [modify] tools/scripts/k8s_deploy.ps1
① 参数区,紧接 `[long]$KafkaSegmentBytes` 之后:
- `[long]$KafkaDbTaskRetentionBytes = 0`
- `[long]$KafkaDbTaskDesignBytesPerSec = 0`
- `[long]$KafkaDbTaskBufferTargetMs = 0`
- `[ValidateRange(0,65536)][int]$KafkaStorageGi = 0`
- `[ValidateRange(0,4096)][int]$KafkaOtherTopicsReserveGi = 0`
每个参数都带中文注释。

② Apply-KafkaProfileDefaults 每个档位补缺省值:
- dev:DesignBytesPerSec 262144、BufferTargetMs 3600000、OtherTopicsReserveGi 1;
- prod-like:1048576、7200000、2;
- default/prod:3145728、7200000、4。
同时改正 db / login RetentionMs 漂移那段注释。

③ 在 Kafka 拓扑函数附近新增三个纯函数,契约测试会按 AST 抽出:
- `Get-KafkaDbTaskRetentionBytesTarget -DesignBytesPerSec -BufferTargetMs -RetentionMs -Partitions -SegmentBytes`:取 T = min(BufferTargetMs, RetentionMs),返回 roundUp(R × T/1000 × 1.2 / P, 64MiB) + SegmentBytes;
- `Get-KafkaDiskPlan -Brokers -ReplicationFactor -ZoneCount -DbTaskPartitions -DbTaskRetentionBytesTarget -SegmentBytes -BrokerRetentionBytes -AuditPartitions -OtherTopicsReserveGi -LiveStorageGi -ExplicitStorageGi -DesignBytesPerSec -DbTaskRetentionMs`,返回 hashtable {RequiredGi, EffectiveGi, Source(explicit|live|computed), DbTaskRetentionBytes, TargetRetentionBytes, FitLimited, ImpliedBufferSec, ZoneCount, Brokers, ReplicationFactor, DbTaskPartitions, SegmentBytes, DesignBytesPerSec, DbTaskRetentionMs}。规则:
  - required = 1.3 × RF × (Z × P × (target + seg) + Audit × (brokerRb + seg) + reserve × GiB) ÷ Brokers;
  - RequiredGi = max(20, ceil(required ÷ GiB ÷ 10) × 10);
  - EffectiveGi:显式值 > 0 用显式值,否则有线上值用线上值,否则用 RequiredGi;
  - fit = floor64MiB((EffectiveGi × GiB × Brokers ÷ (1.3 × RF) − Audit × (brokerRb + seg) − reserve × GiB) ÷ (Z × P) − seg);
  - fit < 2 × seg 时 throw,错误文本含「放不下」和所需 Gi;
  - DbTaskRetentionBytes = min(target, fit);ImpliedBufferSec = P × (rb − seg) ÷ (R × 1.2);
- `Get-KafkaStorageChangeVerdict -LiveStorageGi -ExplicitStorageGi`:返回 absent | auto | unchanged | expand-refused | shrink-refused。

④ 新增四个胶水函数:
- `Get-LiveKafkaStorageGi`:`kubectl get statefulset kafka -n $InfraNamespace --ignore-not-found -o jsonpath={.spec.volumeClaimTemplates[0].spec.resources.requests.storage}`;DryRun 时返回 $null;只接受 `^\d+Gi$`,其它单位或查询失败一律 throw(fail-closed);
- `Resolve-KafkaDiskPlan`:取 Get-KafkaTopology、@(Get-InfraZoneIds).Count、Get-AuthoritativeScalar(db.yaml 的 ServerConfig.Kafka.PartitionCnt;data_service.yaml 的 Kafka.TransactionLogPartitions 与 SnapshotPartitions 相加),再取线上值;`$KafkaDbTaskRetentionBytes` 小于 0 时 throw「-KafkaDbTaskRetentionBytes 必须 ≥ 0」;结果缓存进 `$script:KafkaDiskPlan`;
- `Write-KafkaDiskLedger -Plan`:打一行 `Kafka disk ledger: brokers=.. rf=.. zones=.. db_task_partitions=.. segment_bytes=.. db_task_retention_bytes=<eff> target_retention_bytes=<t> pvc_gib=<eff> pvc_source=<src> required_gib=<req> implied_db_task_buffer_min=<m> db_task_retention_ms_min=<x> design_bytes_per_sec=<R>`;FitLimited 时 Write-Warning(写明扩盘 runbook 和 -KafkaStorageGi <req>);按设计速率算出的缓冲短于 retention.ms 时 Write-Warning;还要注明:先部署的 zone 在下次 zone-up 前仍保留旧的 retention.bytes;
- `Assert-KafkaStorageChangeIsSafe -Plan`:expand-refused 时 throw 并附 runbook;shrink-refused 时 throw「PVC 不能缩」。

⑤ 写操作门禁块(zone-up / all-up / infra-up)在 Write-MysqlConnectionLedger 之后调用 Resolve-KafkaDiskPlan 和 Write-KafkaDiskLedger;只有 infra-up / all-up 再调 Assert-KafkaStorageChangeIsSafe。

⑥ Apply-Infra 的 kafka.yaml 分支加 `.Replace("__KAFKA_STORAGE_SIZE__", ("{0}Gi" -f $plan.EffectiveGi))`。$script:KafkaDiskPlan 为空时现场 Resolve,因为 -SkipPreflight 的 DryRun 也要能渲染。

⑦ New-GoSvcConfigMapYaml 取 `$dbTaskRetentionBytes = (缓存或现场 Resolve).DbTaskRetentionBytes`:
- db heredoc 的 Kafka 段,在 `IsOfflineExpand: false` 之后加两行 `RetentionMs: ${dbTaskRetentionMs}`、`RetentionBytes: ${dbTaskRetentionBytes}`;
- login heredoc 在 `RetentionMs: ${dbTaskRetentionMs}` 后加 `RetentionBytes: ${dbTaskRetentionBytes}`。
放在段尾,是为了少和 S2 的 GroupID 行冲突。

依赖：Go 侧 RetentionBytes 先合入(否则旧二进制读到新键的行为待 F9 核实)

### [modify] tools/scripts/tests/k8s_deploy_contract.tests.ps1
在 Kafka 拓扑用例附近(约 L1758)用 AST 抽出 Get-KafkaDbTaskRetentionBytesTarget、Get-KafkaDiskPlan、Get-KafkaStorageChangeVerdict,新增用例详见 tests。

依赖：k8s_deploy.ps1

### [modify] deploy/k8s/manifests/infra/kafka.yaml
把 volumeClaimTemplates 的 `storage: 20Gi`(:363)改成 `storage: __KAFKA_STORAGE_SIZE__`,并重写 :350-355 的注释:
- 大小由 k8s_deploy.ps1 的 Get-KafkaDiskPlan 按公式算;线上已有时沿用;
- 扩盘不能靠 apply,见 README「Kafka 磁盘预算」;
- db_task 的 retention.bytes 由各 zone 的 db / login 在 topic 级设置。
KAFKA_LOG_RETENTION_BYTES 的注释(:276)补一句:「broker 缺省只兜底没有自己 retention.bytes 的 topic(审计、控制面、旧的一节点一 topic)」。
不碰 :156 的镜像行(归 S2)。

依赖：k8s_deploy.ps1 ⑥

### [modify] deploy/k8s/README.md
在「Kafka 保留期:一条规则」(约 :1244)之后新增「Kafka 磁盘预算与 db_task retention.bytes」一节,内容:
- 公式和缺省值表(dev / prod-like / prod:设计速率、缓冲目标、target retention.bytes = 150994944 / 973078528 / 2785017856,2 个 zone 时 RequiredGi = 20 / 40 / 80);
- 账本行各字段的含义;FitLimited 的含义(盘不够时按盘收缩 retention.bytes);
- 扩盘 runbook:① `kubectl get sc <sc> -o jsonpath={.allowVolumeExpansion}`;② 逐个 `kubectl -n <infra> patch pvc kafka-data-kafka-<i> -p '{"spec":{"resources":{"requests":{"storage":"<N>Gi"}}}}'`,确认容量生效,需要文件系统扩容时逐个重启 broker,并确认 under-replicated = 0;③ `kubectl -n <infra> delete statefulset kafka --cascade=orphan`;④ `k8s_deploy.ps1 -Command infra-up ... -KafkaStorageGi <N>`;
- 已存在 topic 立即改配置:`kubectl -n <infra> exec kafka-0 -- /opt/kafka/bin/kafka-configs.sh --bootstrap-server localhost:9092 --entity-type topics --entity-name db_task_zone_<id>[_g<N>] --alter --add-config retention.bytes=<B>,retention.ms=<T>`,再用 `--describe` 回读;顺序是先扩盘再调大,调小立即生效(retention.check.interval 为 2 分钟);
- 回滚时去掉 topic 级配置:`--alter --delete-config retention.bytes`;
- 加 zone 后要对所有 zone 重跑 zone-up,或用上面的 --alter;
- kind 本机建议传 -KafkaProfile dev。

依赖：k8s_deploy.ps1

### [new] docs/design/db-task-delivery-and-retention.md
设计文档(S5a 写 §A,S5b 追加 §B):
- §A:现状与缺陷;不作废快照的理由;账本状态机(代号比较);生产者逐条回执契约;下线闸门的决策表和出口;孤儿口径;go/db 幂等分析(游标和 epoch 各挡什么、为什么不重放旧字节);计数与日志行;未覆盖的残余(停机看门狗到期、进程崩溃)。
- §B:db_task topic 的建立方;T_buf 的定义;retention.bytes 与 PVC 的公式和缺省值;按盘收缩;扩盘 runbook 指针;「一节点一 topic」的定位与本轮不翻的理由;lag 秒级告警为后续项。
- 文首写明:未编译,待 Codex 验证。

### [modify] docs/README.md
在存盘管线分类下按生成器格式登记新设计文档,需能通过 gen_docs_index.py --check。

依赖：新设计文档

### [modify] docs/PROGRESS.md
末尾另起一条「补充:S5 scene DBTask 投递失败重存与 db_task 保留 / Kafka 盘预算(未编译,待 Codex 验证)」:改动范围、负责人 2026-10-13 的授权摘要、Java 状态(两项都不适用,已登记 PARITY)、验证命令指向 launch-todo。只追加,不改旧条目。

### [modify] docs/ops/godb-pipeline-launch-todo.md
§3 「WAITAOF、脏快照、tonumber、retention」那一行补落码状态;新增小节「S5 验证」:Codex 命令、通过标准,外加两项故障注入:① 一个玩家下线时 docker pause kafka 6 分钟,预期实体被保留,恢复后 MySQL 行与 Redis 一致,exit_await_ack / exit_resave 有计数;② 在 kind 上 infra-up DryRun 核对账本行。lag 秒级告警依赖 S4,登记为待做。

### [modify] D:\luyuan\xuanming-server-mmo\PARITY.md(Java 仓库,仓外)
末尾追加两行「不适用」及理由,见 B-21;不提交。

## 接口（类型 / 配置键 / 指标 / Redis 键 / 组名）
【C++ 生产者】KafkaProducer(thread_local)新增:
- DeliveryReport{correlationId, err, synthesized};
- DeliveryListener = std::function<void(const DeliveryReport&)>;
- setDeliveryListener(DeliveryListener);
- sendTracked(topic, message, key, uint64 correlationId, partition = PARTITION_UA),返回 RdKafka::ErrorCode;
- trackedInFlight()。
send() 的签名与行为不变。

契约:
- correlationId 为 0 等价于 send();
- 返回值不是 ERR_NO_ERROR 时没有入队,也永远不会有回执;返回 ERR_NO_ERROR 时之后恰好一次 DeliveryReport,可能是 librdkafka 的回执、重建时合成的 ERR__PURGE_QUEUE,或 scene 侧 330s 的超时兜底;
- 回执只在本线程的 poll() / flush() / send() 栈里回调,监听器只许记账。

【pure】kafka_producer_policy::DeliveryTracker:Accept / Resolve / TakeAll / Size。

【pure】db_task_dispatch::Ledger:BeginSave / IssueTicket / OnProduceRejected / MarkLocalFailure / FinishSave / OnDelivery / ExpireStale / NeedsResave / InFlight / IsSettled / ForgetIfNoInFlight / UnsettledPlayers / Clear。另有 AllowsDirtySaveSkip、DeliveryEffect 及其名字表。

【scene 粘合】DbTaskDispatchSystem:Initialize(SettledHandler) / Shutdown / BeginSave / SendDbTask → {queued, error} / MarkLocalFailure / FinishSave / NeedsResave / InFlight / OnPlayerGone / OnDeliveryReport / Pump / UnsettledPlayerCount / InFlightTickets / LedgerForTest / ResetForTest。
- 常量:kTicketTimeout = 330s、kShutdownUnsettledLogLimit = 50;redis.cpp 里 kDbTaskDispatchPumpIntervalSec = 0.05。
- db_task_dispatch_stats::Counter 共 12 项,名字见 changes。30s 统计行前缀 `[DbTaskDispatch]`,格式为 key=value 平铺的累计值。没有 Prometheus label,也不用 player_id 当 label。

【玩家生命周期】
- PlayerLifecycleSystem::FinishExitOrAwaitDbTaskAck(Guid) → bool;
- PlayerLifecycleSystem::OnDbTaskDispatchSettled(Guid);
- player_exit 新增:kMaxExitDbTaskResaveRounds = 3、DbTaskGateDecision 及其名字表、DecideDbTaskGate、ShouldRekickDbTaskExhausted;
- PlayerExitIntentComp 新增 awaitingDbTaskAck、dbTaskResaveRounds。
- SavePlayerToRedis / SavePlayerToRedisImpl 的签名和返回值语义都不变。asset_op_system 的 PersistFn 函数指针不受影响。

运行期日志文本(英文)新增:
- `[SavePlayerToRedis] forcing a real save ... (metric=db_task_forced_resave)`;
- `... (metric=db_task_produce_rejected)`;
- `... (metric=exit_db_task_await_ack | exit_db_task_resave | exit_db_task_resave_capped | exit_db_task_unsynced | exit_db_task_rekick)`;
- `[DbTaskDispatch] ... (metric=db_task_orphan_unsynced | db_task_ticket_expired)`;
- `[DbTaskDispatch] shutting down with N player(s) ...`;
- `[Kafka] N tracked messages lost their delivery report ...`。

【Go】kafkautil.TopicSpec.RetentionBytes int64(0 = 不管理,>0 = SET,<0 报错)。
- 配置键:go/db 的 ServerConfig.Kafka.RetentionBytes;go/login 的 Kafka.RetentionBytes。均为 optional,缺省 0。
- 不新增 Redis 键、Kafka 消费组或指标。

【部署】
- k8s_deploy.ps1 新参数:-KafkaDbTaskRetentionBytes / -KafkaDbTaskDesignBytesPerSec / -KafkaDbTaskBufferTargetMs / -KafkaStorageGi / -KafkaOtherTopicsReserveGi,0 都表示按档位或自动。
- 新函数:Get-KafkaDbTaskRetentionBytesTarget、Get-KafkaDiskPlan、Get-KafkaStorageChangeVerdict、Get-LiveKafkaStorageGi、Resolve-KafkaDiskPlan、Write-KafkaDiskLedger、Assert-KafkaStorageChangeIsSafe。
- 账本行前缀 `Kafka disk ledger:`。
- kafka.yaml 新占位符 `__KAFKA_STORAGE_SIZE__`;db 和 login 的 ConfigMap Kafka 段同时带 RetentionMs、RetentionBytes。

缺省值:
| 档位 | target retention.bytes | 2 个 zone 时 PVC |
|---|---|---|
| prod | 2785017856 | 80Gi |
| prod-like | 973078528 | 40Gi |
| dev | 150994944 | 20Gi |

Kafka key 仍是 player_id 字符串(§7.3)。db_task 不是 `{type}-{id}` 形态,不涉及 target_instance_id。

## 顺序与重叠
【顺序】
- S5a(C++)与 S5b(Go + 部署)互不依赖,建议拆成两个 PR,先后随意。
- S5b 内部:Go 侧(TopicSpec 和两个服务的 KafkaConfig)要与 k8s_deploy 渲染新键同批或先合入。
- S5a 不依赖 S1-S4。

【markAppliedSeq / markAppliedOwnerEpoch】S5 不改 go/db 消费端任何代码,不动这两个函数的调用顺序和位置,也不改 dbOpHandler 的签名。

【与 MySQL 条件写会话重叠(必须协调)】
- 同一处代码:player_lifecycle.cpp 的 SavePlayerToRedisImpl → sendSubTableTask lambda。对方要在 DBTask 上写 save_seq;本项把 KafkaProducer::send 换成 DbTaskDispatchSystem::SendDbTask,并在 lambda 前后加 BeginSave / FinishSave / MarkLocalFailure。
- 合并规则:对方的 save_seq 赋值留在 SerializeToString 之前;本项只替换发送那一行,加记账调用。
- 强制重存天然拿到新的 save_seq(NextSaveSeq)。

【与工作区里正在写 MarshalPlayerForSave 的未提交会话重叠】
- 对方已让 SavePlayerToRedisImpl 在 marshal 失败时提前 return false;本项的改动都在这个检查之后。
- FinishExitOrAwaitDbTaskAck 的 kResave 分支把 false 解释为「没法重存」,收尾并计 exit_unsynced,与对方注释里「退出照常收尾」一致。
- OnDbTaskDispatchSettled 按 bool 返回值处理 MarshalPlayerForSave;如果落码时它仍是 void,先照原样调用。
- 对方同时在改 player_database_loader.cpp,本项不碰。

【与全局单点 P0-B 会话(未提交)重叠】
- kafka_producer.h 里对方加的 brokersConfigured() 要保留。
- kafka_manager.{h,cpp}、node.cpp 本项不碰。

【与 S2(③ 消费组按 zone 拆、Kafka 镜像钉版本、回档 Step 2)重叠】
- tools/scripts/k8s_deploy.ps1:
  - 参数区:S2 加 -DbConsumerGroup;
  - db ConfigMap 的 Kafka heredoc:S2 改 GroupID 那一行,并写 GroupHandoffFrom;本项在段尾加 RetentionMs / RetentionBytes;
  - Apply-OneGoSvc 的注解(本项不碰);
  - 写操作门禁块;
  - login ConfigMap:S2 第 7 步会删 GroupID。
- tools/scripts/tests/k8s_deploy_contract.tests.ps1:两边都新增用例。
- deploy/k8s/manifests/infra/kafka.yaml:S2 改 :156 的镜像行,本项改 :363 和 :350-355、:276 的注释;同一文件,不同 hunk。
- deploy/k8s/README.md:都在 Kafka 相关小节。
- go/db/internal/config/config.go 的 KafkaConfig、go/db/etc/db.yaml 的 Kafka 段:S2 加 GroupHandoffFrom 和三个超时键。
- go/login/etc/login.yaml、go/login/internal/config/config.go:S2 第 7 步把 GroupID 改为 optional。
- 建议:S5b 先合入,或 S2 落码时按函数名 rebase。本项新增行都放在段尾,尽量缩小冲突。
- docker-compose.yml 和 kafka-topic-init.yaml 只有 S2 动,本项不碰。

【与 S3(① 读写分区对齐)重叠】
- go/login/internal/config/config.go 与 go/login/etc/login.yaml:S3 加 Partitioner 相关字段。
- 如果 S3 决定在 C++ 的 kafka_producer.cpp createProducer 里显式钉住 partitioner,就和本项的 kafka_producer.cpp 同文件不同函数,需要协调。

【与 S1(④ + WAITAOF)、S4 重叠】
- 代码层面最多是 go/db/internal/config/config.go 的同一个 KafkaConfig 结构体(S1 加 RetryRouting 等字段)。
- 文档共用:docs/PROGRESS.md(union 合并)、docs/README.md、docs/ops/godb-pipeline-launch-todo.md。
- S4 的 go/db 停机顺序、go_services.ps1、测量打点、Lua tonumber 与本项没有交集。B-12 的 lag 秒级告警依赖 S4 的 lag 指标。

【与其它在途会话重叠】
- launch-readiness 的 G2b / G4b 评审修复和 P0 各批都在改 k8s_deploy.ps1 和契约测试(有 +126 行的在途改动)。行号以函数名为准。
- 多个会话在写 cpp/libs/services/scene/** 和 cpp/tests/**:新单测放进独立的新文件,不改 cross_zone_test.cpp。
- Java 仓库的 PARITY.md 由所有条目共用,只在末尾追加。

## 测试
### cpp/tests/kafka_command_test/kafka_producer_policy_test.cpp
TEST(DeliveryTracker, ...):
- AcceptThenResolveIsTrueOnceThenFalse;
- ZeroIdIsNeverTracked:Accept(0) 之后 Size()==0,Resolve(0)==false;
- ResolveUnknownIdIsFalse;
- TakeAllReturnsEveryOutstandingIdAndEmpties:Accept 3 个、Resolve 1 个,TakeAll 得到其余 2 个,与顺序无关;之后 Size()==0,再次 TakeAll 为空;
- ResolveAfterTakeAllIsFalse:重建合成回执之后,原回执迟到也不会二次通知。

### cpp/tests/cross_zone_test/db_task_dispatch_test.cpp
纯账本 DbTaskDispatchLedger:
- BothTicketsDeliveredSettles:两次 IssueTicket,FinishSave(all),两次 OnDelivery(ok);第二次 inFlightAfter==0,IsSettled,!NeedsResave;
- SyncRejectionNeedsResaveUntilNextFullyQueuedSave:gen1 一张被拒、一张入队,NeedsResave;gen2 全部入队后 !NeedsResave;
- DeliveryFailureOfLatestSaveNeedsResave:effect==kFailedNeedsResave;
- FailureSupersededByLaterFullyQueuedSave:gen1 入队;gen2 FinishSave(all);gen1 失败 → kFailedSuperseded,!NeedsResave;
- FailureBeforeFinishSaveIsNotCleared:gen2 发票后、FinishSave(gen2, all) 之前 gen2 的票据失败,之后仍 NeedsResave;
- LocalFailureNeedsResave;
- DuplicateOrUnknownReportIsIgnored:重复回执,以及 Forget 之后到达的回执,都返回 kUnknownTicket,inFlight 不下溢;
- ForgetOnlyWhenNoInFlight:在途时 forgotten=false;回执之后 forgotten=true,hadUnresolvedFailure 如实;
- ExpireStaleTreatsTimedOutTicketsAsFailedInIssueOrder:注入 time_point,已销的票据被跳过,碰到第一张未过期的停止;
- UnsettledPlayersSortedAndBounded;
- AllowsDirtySaveSkipTable:8 种组合。

纯决策 ExitDbTaskGate:
- DecideDbTaskGate 表:(0,false,0)=kFinish、(2,false,0)=kAwaitAck、(0,true,0)=kResave、(1,true,2)=kResave、(0,true,3)=kRetainExhausted、(5,true,3)=kRetainExhausted;
- ShouldRekickDbTaskExhausted:只在 awaiting、needsResave、rounds>=max、inFlight==0、!unsettled 全部成立时为真,逐项置反各为假;
- 名字表覆盖每个枚举值。

ECS 级 DbTaskDispatchEcs(无 Redis,不走真重存;固件清账本):
- LandedExitWaitsForInFlightAck:退出中的玩家 IssueTicket 1 张,HandlePlayerAsyncSaved(landed) 后实体仍在,intent->awaitingDbTaskAck,快照等于 landed;OnDeliveryReport(t,true) 加 Pump() 后实体销毁,playerList 与 SessionMap 清空,TrackedPlayers==0;
- FastPathExitWaitsForInFlightAck:在线玩家 MarkPersisted,加 1 张在途票据,HandleExitGameNode 后实体仍在;回执成功加 Pump 后销毁;
- FailedAckOnExitingPlayerRetainsWhenCapped:dbTaskResaveRounds 预置为 3,IssueTicket 后回执失败加 Pump,实体保留、awaiting 为真、计数 exit_resave_capped 增加;不触发重存,因此不碰 Redis;
- OrphanFailureIsCountedAndForgotten:没有实体时 IssueTicket,回执失败加 Pump,orphan_unsynced 增量为 1,TrackedPlayers==0;
- DeliveredOrphanIsForgottenSilently:没有实体时回执成功,orphan 计数不变,条目回收;
- EmptyLedgerKeepsExistingFinishBehavior:账本为空时 HandlePlayerAsyncSaved 直接销毁,回归原行为;
- SendDbTaskWithoutBrokersMarksResave:本测试线程的生产者没登记 broker,所以 SendDbTask 一定被同步拒绝;queued==false,NeedsResave 为真,InFlight==0,produce_rejected 增量为 1;
- ExpiredTicketDuringPumpCountsAndEnqueues:通过 LedgerForTest 注入过去的 issuedAt,Pump 后 ticket_expired 增量为 1。

先跑 `cross_zone_test.exe --gtest_filter=DbTaskDispatch*:ExitDbTaskGate*:ExitPersist*`,再跑全量。

### go/shared/kafkautil/topic_init_test.go
- TestEnsureTopicsSetsRetentionBytesOnCreateAndAlter:spec 为 RetentionMs=86400000、RetentionBytes=2785017856;新建的 ConfigEntries 同时带 retention.ms 与 retention.bytes;IncrementalAlterConfig 恰好一次,含两个 SET 条目,值逐字相等;
- TestEnsureTopicsLeavesRetentionBytesUnmanagedWhenZero:entries 里没有 retention.bytes,只有 retention.ms,原有行为不变;
- TestEnsureTopicsRejectsNegativeRetentionBytes:RetentionBytes=-1 时报错,错误含 retention.bytes;不 create、不 alter;
- TestEnsureTopicsRetentionBytesWithoutRetentionMs:RetentionMs=0、RetentionBytes>0 时 alter 只含 retention.bytes。
命令:`cd go/shared && go test ./kafkautil/ -run 'TestEnsureTopics|TestPartitionContract|TestParseReplicationFactor' -count=1 -v`。

### tools/scripts/tests/k8s_deploy_contract.tests.ps1
纯函数用例:
- prod 缺省、2 个 zone、3 broker、RF3:Target 2785017856,RequiredGi 80;
- 同样配置但 1 broker、RF1:也是 80;
- dev 缺省:150994944,20(取下限 20);
- prod-like:973078528,40;
- prod 下线上 20Gi:EffectiveGi 20,Source live,DbTaskRetentionBytes 268435456,FitLimited 为真,ImpliedBufferSec 在 622±1;
- 线上 5Gi、prod、3 个 zone:throw,错误文本含「放不下」;
- 显式 retention.bytes 大于可容纳值时仍按盘收缩;
- Get-KafkaStorageChangeVerdict 表:(null,0)=absent、(null,80)=absent、(20,0)=auto、(20,20)=unchanged、(20,80)=expand-refused、(80,20)=shrink-refused。

DryRun 用例:
- 复用 $devOut(zone-up):db 与 login 两个 ConfigMap 都含 `RetentionMs: 86400000` 和 `RetentionBytes: 2785017856`,且两边相等;输出含 `Kafka disk ledger:` 一行,并且 pvc_source=computed、required_gib=80;
- 复用 $battleInfraRun(infra-up):kafka 清单渲染出 `storage: 80Gi`,且没有残留的 `__KAFKA_STORAGE_SIZE__`;
- 新增一次 `-KafkaProfile dev` 的 infra-up DryRun:`storage: 20Gi`;
- 负向:`-KafkaDbTaskRetentionBytes -1` 退出非零,错误文本含「必须 ≥ 0」;`-KafkaStorageGi -1` 被参数校验拒绝,断言错误文本含 KafkaStorageGi。
命令:`pwsh -File tools/scripts/tests/k8s_deploy_contract.tests.ps1`。

## 上线与回滚
【S5a 部署】
- scene 节点滚动升级即可。不改 proto、配置、库表,不改与 go/db 之间的消息格式,新旧版本可以并存。
- 行为变化(需 D1 确认):
  - 下线销毁实体要等本玩家 DBTask 的 acks=all 回执,正常多 ≤50ms 的 Pump 间隔;
  - Kafka 不可用时,下线实体与 Redis 失败时一样被保留,直到 Kafka 恢复(由周期存盘强制真写)、再次收到退出请求触发 rekick,或停机看门狗到期。
- 观察 30s 一行的 `[DbTaskDispatch]`:
  - exit_await_ack 随下线增长属正常;
  - 稳态下 produce_rejected、delivery_failed(扣除 superseded)、ticket_expired、exit_resave_capped、exit_unsynced、orphan_unsynced 都应为 0;
  - 停机 drain 的耗时应基本不变。
- 回滚:换回旧的 scene 镜像。账本只在内存里,没有持久化状态要处理。

【S5b 部署】
1. 合入 Go 改动,构建 db / login 镜像。
2. 逐 zone 跑 zone-up:新 ConfigMap 带 RetentionMs / RetentionBytes,db / login 重启时经 IncrementalAlterConfigs 生效。不想为改保留期滚动 go/db(瓶颈文档 §4.5 的发版红线)时,用 README 里的 kafka-configs.sh --alter 立即生效,ConfigMap 等下次正常发布再收敛。
3. infra-up:
   - 已有集群沿用线上 PVC;盘低于预算时 retention.bytes 按盘收缩,并打 WARN。例如 prod 档、20Gi、2 个 zone 时,从 broker 缺省的 512MiB 降到 256MiB,按设计速率约能缓冲 10 分钟。
   - 要扩盘时按 runbook 先 patch PVC,再带 -KafkaStorageGi N 重跑 infra-up。
   - 新集群直接按公式建盘。
4. 回滚:topic 级配置在回滚后仍留在 topic 上,本身无害;要撤掉用 `--delete-config retention.bytes`。旧二进制遇到 ConfigMap 里的未知键 RetentionBytes 会怎样还没核实(附录 F9),要是会报错,回滚时必须连同旧的 ConfigMap 生成器一起回退。

【Codex 验证命令】仓库根 D:\luyuan\xuanming-server-mmo-cpp,PowerShell 7,VS 开发者环境。前一步红了就停下归因,不连续重试。
- C++:MSBuild 必须串行 `/m:1`;不以 game.sln 为通过标准,因为主干整体编不过,见 global-singletons-p0-findings-20261014.md:1938。
  1. `msbuild cpp\libs\engine\infra\infra.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64`
  2. `msbuild cpp\libs\services\scene\scene.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64`
  3. `msbuild cpp\tests\kafka_command_test\kafka_command_test.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64`
  4. `msbuild cpp\tests\cross_zone_test\cross_zone_test.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64`
  5. `msbuild cpp\nodes\scene\scene.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64`(核对链接)
  6. `pwsh tools/scripts/run_cpp_tests.ps1 -Filter kafka_command`
  7. `.\build\cpp\tests\kafka_command_test.exe --gtest_filter=DeliveryTracker.*`
  8. `.\build\cpp\tests\cross_zone_test.exe --gtest_filter=DbTaskDispatch*:ExitDbTaskGate*:ExitPersist*`,通过后再跑一遍全量。
  - 通过标准:各工程 0 error;新用例全绿;既有 ExitPersist* 用例没有新增失败。
  - 失败归因:报错落在其它会话的未提交文件里(scene 库下大量 M 状态的文件、player_database_loader.cpp 等)时,分开记录。
  - 失败时保留:msbuild 第一条 error 的前后 20 行、gtest 失败用例全文。
- Go:环境变量 GOTOOLCHAIN=local、GOFLAGS=-mod=readonly、GOPROXY=https://goproxy.cn,direct,Go 1.27.2,不改 go.mod / go.sum。
  1. `cd go/shared && gofmt -l kafkautil && go vet ./kafkautil/ && go test ./kafkautil/ -run 'TestEnsureTopics|TestPartitionContract|TestParseReplicationFactor' -count=1 -v`
  2. `cd go/db && gofmt -l internal/config && go vet ./... && go build ./...`
  3. `cd go/login && gofmt -l internal/config && go vet ./... && go build ./...`
  4. 用一个最小用例核实 go-zero conf.MustLoad 对未知键的行为(附录 F9),写进 launch-todo。
  - 通过标准:gofmt 无输出;vet / build / test 退出码为 0。
- PowerShell:`pwsh -File tools/scripts/tests/k8s_deploy_contract.tests.ps1` 全部 PASS。
- 文档:`python tools/scripts/gen_docs_index.py --check` 退出码 0。
- 故障注入登记进 launch-todo,不在本轮执行。

## 待决项（负责人已按推荐默认定）
- 是否接受下线收尾等 DBTask 回执:正常情况多 ≤50ms;Kafka 不可用时下线实体被保留,直到 Kafka 恢复或停机看门狗到期
  - 采用：接受。所有退出原因统一走闸门,不为 kReleasedByTransfer 或有改派票据的情况开例外。
- db_task 的缓冲目标与每 zone 设计字节率(决定缺省的 retention.bytes 和 PVC)
  - 采用：prod 2h、3 MiB/s;prod-like 2h、1 MiB/s;dev 1h、256 KiB/s;其它 topic 预留 4 / 2 / 1 GiB。2 个 zone 时 PVC 分别为 80Gi / 40Gi / 20Gi。
- PVC 低于预算时:按盘收缩 retention.bytes 并打 WARN,还是直接阻断部署
  - 采用：按盘收缩加 WARN,不阻断。只在连 2 个 segment 都放不下时 throw。
- S5 是否拆成 S5a(C++)和 S5b(Go + 部署)两个 PR
  - 采用：拆。
- 「关掉旧的一节点一 topic」(DisableLegacyPerNodeTopic 置 true)是否放进本轮
  - 采用：不放,按全局单点 P0-B 第 13 项另案处理;本轮只在盘预算里给残留留出预留。

## 风险
1. 全部未编译、未测试;没有 Codex 结果之前,不得声称已验证。
2. 多会话同时改同一批文件:
   - player_lifecycle.cpp 里同时有条件写会话、MarshalPlayerForSave 会话的未提交改动;
   - k8s_deploy.ps1 和契约测试里有 S2、G2b、P0 的在途改动。
   合并时容易漏改或误覆盖,必须按函数名重新定位,不回退别人的改动。
3. 回执靠新增的 50ms Pump 定时器驱动;票据超时 330s 依赖 librdkafka 缺省的 message.timeout.ms(300s)。以后有人改这个值却不同步改 kTicketTimeout,会把迟到的成功回执误判为失败(结果只是多一次重存,不丢数据)。
4. Kafka 长时间不可用时,下线实体在节点内存里累积,上限是在线人数。停机看门狗到期时,这些玩家的 MySQL 会落后于 Redis:Redis 里有最新数据,下次登录自愈;但合服、回档这类以 MySQL 为准的操作会读到旧档。Shutdown 那行 ERROR 列出的玩家号就是证据。
5. 被废黜、交接放行这些路径产生的孤儿,要等新属主的首存来补;新节点在首存之前崩溃时,只剩 Redis 一份。
6. 按盘收缩会降低现网小盘集群的 db_task 缓冲。WARN 只是提示,真正要靠运维扩盘。
7. 加 zone 之后,新部署的 zone 拿到更小的 retention.bytes,老 zone 在重新 zone-up 之前保留旧值,期间有短暂的超量承诺。runbook 要求对全部 zone 重跑。
8. 缺省 KafkaProfile 是 prod。kind 本机的新集群会申请 80Gi PVC,local-path 不强制容量,retention 上限也比原来高约 5 倍,高写入压测可能写满宿主盘。README 建议本机用 -KafkaProfile dev。
9. 公式只给控制面 topic、match-results、__consumer_offsets、旧 per-node topic 留了固定预留,没有按 broker 缺省 retention.bytes 取最坏值。到 S1 规模时控制面约 4.8 GB/h 每 broker,预留必须调大。审计 topic 仍沿用 broker 缺省,保留策略待 G2a Q-1 拍板;G2a 换代期间新旧审计 topic 并存,会临时占双倍。
10. lag 秒级告警本轮没做(依赖 S4 的 lag 指标)。积压逼近 T_buf 时仍然没有主动告警,删除是静默的。
11. 附录 F9 没核实:旧版 go-zero 遇到未知键 RetentionBytes 是忽略还是报错,影响 S5b 的回滚路径。
12. 既有风险,本轮未修:
    - scene 先于 db / login 发 DBTask 时,broker 会把 db_task 自动建成 1 分区,导致 db 启动 panic;
    - 生产端没开压缩,message.max.bytes 三处没对齐。
13. 设计速率是推算值。账本行打出的 implied buffer 只是估计,不能当成承诺的缓冲时长;回档 TargetTime 超过实际缓冲时会重放不全。
