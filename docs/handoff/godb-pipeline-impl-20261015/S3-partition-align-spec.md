# S3 实现规格：S3 ① 读写分区对齐（partition-hash-unify，附录 L218–311，复核结论 sound-with-fixes）

## 摘要
契约以 C++ 实际在用的 librdkafka consistent_random 为准：key 取玩家 id 的十进制 ASCII，partition = CRC-32/IEEE(key) mod N，按 uint32 无符号取模。全仓 db_task 只有三个生产者：C++ scene（写，player_lifecycle.cpp:4026 是唯一调用点，不传 partition，走 PARTITION_UA）、login（读）、go/db/cmd/data_stress（写，压测工具）。具体改动：
- 新增 go/shared/kafkautil/dbtask_partition.go，作为这个契约在 Go 侧唯一的实现，并配一份 Go 与 C++ 共读的 golden 向量文件。
- login 主生产者在 sarama.NewClient 之前装上 kafkautil.NewDBTaskPartitioner。
- 删除从来没生效过的一致性哈希环、「不可用分区重映射」和 pkg/consistent（其中 G0 已修好的自锁路径随之一起消失）。
- plainProducer 遇到 db_task_zone_ 前缀的 topic 也走契约分区器；SendToTopic 和 SendToTopicPartition 拒绝 db_task topic；SendTask 和 SendTasks 校验 key == DBTaskKey(task.Key)。
- go/db 在 ConsumeClaim 解码之后只读地观测 db_task_producer_partition_total{op,verdict}：不拒绝消息，不改 ack、游标、重试。
- data_stress 改用契约分区器。
- C++ 显式钉住 partitioner=consistent_random，新增 DbTaskKafkaKey，再加一个直接调用 librdkafka 的 golden 测试。
- 新增 PrometheusRule 告警，订正 5 份文档，PROGRESS 与 Java 版 PARITY 各登记一条。

写侧不改一行、不迁移分区、不换代。玩家可见的行为变化（负责人已接受）：db 积压让冷读的排队超过 login 预加载等待（8s，最多再多 1s 扫描粒度）时，以前是读到旧档，现在是进场失败（tip 3023）。共 29 个文件（28 个在本仓库，1 个是 Java 仓库的 PARITY.md），全部未编译，等 Codex 验证。

## 现状
按当前工作区核对：
【login 读，现在走 FNV】
- go/login/internal/kafka/key_ordered_producer.go:58-71 建 db_task 主生产者配置时没设 Producer.Partitioner，用的是 sarama 默认的 NewHashPartitioner（FNV-1a）。
- sarama 的 async_producer.go:536-575（v1.46.2）在 partitionMessage 里一定会算一遍分区并写回 msg.Partition。所以 :84 建环、:111-113 AddPartition、:181/:282 getAvailablePartition、:205/:292 填 Partition 字段，以及 :531-625 整套不可用分区代码，都从来没生效过。
- plainCfg 在 :128 装了 NewCommandAwarePartitioner，那里的注释 :124-127 可以作旁证。
- key 来源：sync_loader.go:42 是 strconv.FormatUint(playerId,10)，:98 Op="read"，:106-109 一张子表走 SendTask，多张走 SendTasks；SendTask 和 SendTasks 都不校验 key 与 task.Key 是否一致。
- G0（d5cbb8a8e）已经把失败分支改成 markPartitionUnavailableLocked（:538-547），并配了回归用例 key_ordered_producer_lock_test.go，自锁已修。
- pkg/consistent 只被 key_ordered_producer.go:12/:37/:84 和 lock_test.go:8/:18/:30 引用。
- SendToTopic（:380-393）在生产代码里没有调用方；SendToTopicPartition（:401-415）只被 svc/servicecontext.go:590/:604/:626 用来发控制面命令。
- command_partitioner.go:28-34 对非命令 topic 返回 NewHashPartitioner。
- expand_monitor.go:15-17 的注释还在说哈希环。

【C++ 写，现在走 CRC32；所有调用方已逐一核对】
- KafkaProducer::send 的 partition 默认值是 PARTITION_UA（kafka_producer.h:116）。produceOnce（kafka_producer.cpp:106-122）原样传给 produce。createProducer（:49-88）只设了 bootstrap.servers、dr_cb、enable.sparse.connections、enable.idempotence 四项（:59-76）。
- 在 cpp/（不含 third_party）里 grep partitioner、default_topic_conf、CONF_TOPIC，零命中。
- vendored librdkafka 是 2.14.0（third_party/librdkafka/src/rdkafka.h:170 RD_KAFKA_VERSION 0x020e00ff，子模块已检出 f1b831e）。默认分区器是 consistent_random（rdkafka_conf.c:1775-1793）。key 非空时 rd_kafka_msg_partitioner_consistent = rd_crc32(key)%partition_cnt（rdkafka_msg.c:1063-1084），按 uint32 取模。sticky 只作用于 NULL 或空 key（:1209-1226）。rd_crc32 是标准 CRC-32/IEEE（rdcrc32.h:39-42、:98-155，带不带 zlib 结果都一样）。
- C++ 侧全部 Kafka 发送点：
  1. player_lifecycle.cpp:4026：send(dbTaskTopic, dbTaskBytes, playerIdStr)。topic 是 GetDbTaskTopic(homeZoneId, gen)（:3993），key 是 std::to_string(playerId)（:3989），不传 partition。这是唯一的 db_task 生产点，op 为 write（:4010）。
  2. battle_room_manager.cpp:95、:172：gate 和 scene 命令 topic，显式传 route.partition。
  3. battle_room_manager.cpp:374：对局结果 topic，key 是 battleId。
  4. snapshot_system.cpp:101 和 transaction_log_system.cpp:83：审计 topic。
  5. anomaly_detector.cpp:130：告警 topic。
  6. kafka_manager.cpp:103-105 的 Publish：cpp 目录下零调用方。
  cpp/tests 里没有调用 send 的地方。所以 C++ 写侧全部是 CRC32 mod N，这点已经静态确认（交接 §4.1 第 5 行那项「未逐一核对」到此闭合）。线上实际的消息分布要等 rollout 第 1 步的抽样来证实。

【其它生产者与版本】
- go/db/cmd/data_stress/main.go:101-115 也没设 Partitioner（走 FNV），:189 用 strconv 生成 key，头注释 :4-6 说它与 C++ 同分区，这不成立。
- 其余 Go 生产者（scene_manager、match 的 kafka-go Writer）、tools/merge_zone、robot、Java 都不发 DBTask。
- sarama 方面：login 用 v1.46.2，db 和 shared 用 v1.43.1。两版的 partitioner.go 逐字节相同（已 diff），NewConsistentCRCHashPartitioner 在 :197-207，unsigned 分支在 :229-232。

【go/db 读后写屏障（TC3 / 不变量 I4）现在怎么实现】
- ConsumeClaim（key_ordered_consumer.go:1724-1840）按 msg.Partition 选 worker（:1795-1813，originPartition=msg.Partition）。
- worker 的 routeToSubShard（:1311-1344）按 spreadKey(dbTask.Key)%SubShardCount 分到固定子 shard（:1354-1365）。runSubShard（:1371-1401）按到达顺序攒批。
- processTaskBatch（:1409-1536）分三遍：反向那一遍里，读遇到同 (key,msgType) 就 delete(liveBestWrite)（:1503-1508），读之前的写不会被读之后的写合并掉；正向那一遍串行 handleTask（:1511-1531）。
- 读不推进游标（orderingForTask :483-485），不拿排序锁（:1591-1600 只对 write），不进 epoch 守卫（:1626）。
- 写比较游标里记录的分区，分区不同就判 orderingIncomparable 隔离进死信（:504-506 → :1618-1621）。
- 回归用例是 key_ordered_consumer_test.go:438 TestProcessTaskBatch_ReadActsAsBarrier。
- 结论：屏障只在同一分区内成立。现在读走 FNV、写走 CRC，约 90% 的玩家读写不在同一分区，读写落在两个 worker 上并行跑，屏障在生产上不起作用。
- 对齐之后，同一玩家的读和写进同一 worker、同一子 shard，按 offset 串行，这套代码一行不用改就生效。

【8 秒从哪来、失败时客户端看到什么】
- entergamelogic.go:421 的 dispatcherTaskTTL=8s，作为 perTaskTTL 传进 EnsurePlayerAllDataInRedisAsync（:422-432），再到 disp.Register（ensure_player_all_data_async.go:160 → task_result_dispatcher.go:146-158，expiresAt=now+8s）。
- dispatcher 每 1s 扫一次（:203 gcInterval），到期前最后 LPOP 一次补救（:366-378），仍然拿不到就回调 "task result wait timed out"（:380）。实际等待约 8–9s 加一次 Redis RTT，计时从注册算起，覆盖 Kafka 发送、在本分区和子 shard 排队、SELECT、发布。
- 失败链路：firstErr → finalize → onPreloadComplete(err)（entergamelogic.go:334-368）。这条链会：
  1. 记 entergame_preload_seconds{result="preload_failed"} 和 entergame_total{result="preload_failed"}；
  2. 释放 player_locker；
  3. 由 notifyEnterGameFailed（:497-515）经 gate 推 tip SceneError_kEnterSceneFailed=3023（generated/code/proto/tip/scene_error_tip.proto:58），同时计 entergame_failure_notify_total{stage="preload",outcome=sent|failed|skipped_no_gate}。
- EnterGame 的 gRPC 应答此前早已返回成功（"已受理"语义）。
- dataloader 侧还记 dataloader_preload_callback_wait_seconds{result="failed"} 和 dataloader_preload_callback_result_total{result="failed"}。
- 超时没有单独的 label：ResultPreloadTimeout（clientplayerlogin/metrics.go:189）声明了但没用；stress_summarize.ps1:238 依赖 preload_failed 这个取值。
- 父键缺失时才会发读：父键 EXISTS 命中就走快路径（ensure_player_all_data_async.go:52-65）；子表缓存命中就直接拼档，也不发读（:79-90）。

【其它现状】
- go/db 指标在 go/db/internal/metrics/metrics.go。register()（:172-192）会把计数器预置为 0。K8s 上 db ConfigMap 已经写了 MetricsListenAddr ":9160"（k8s_deploy.ps1:4131）。
- go/test.ps1:63-79 的 L1 目前只有 db 的 5 个包和 shared/placement。
- kafka_command_test 只有 vcxproj，没有 CMake（cpp/tests 不在 CMake 里），已经链接 rdkafka.lib 和 yaml-cppd.lib。它已有 Go/C++ 共读向量文件的先例：go/shared/kafkacmd/testdata/command_partition_vectors.json + kafka_command_test.cpp:43-110。
- 工作区里另有会话正在改 player_lifecycle.cpp 和 kafka_command_test.vcxproj（git status 显示 M，vcxproj 新增 kafka_manager_init_test.cpp，并删掉了 rdkafka++.lib）。

## 复核阻断项 / 修正的处理
- **B0**：附录①的「复核阻断项」
  - 处理：复核结论原文是「没有发现阻断问题」，本节没有阻断项。下面 R1–R8 逐条处理「复核要求的修正」。
- **R1**：修正 1：不能写「删死代码，行为完全一样」，要如实写明删掉的代码里有自锁，并补回归测试：mock SyncProducer 先 ExpectSendMessageAndFail(ErrLeaderNotAvailable)，再断言第二次 SendTask 在超时内完成；回滚说明里写清楚。
  - 处理：部分已过时：G0（d5cbb8a8e）已经单独修好这个自锁（markPartitionUnavailableLocked，key_ordered_producer.go:538-547），回归用例 TestSendTaskLeaderNotAvailableDoesNotDeadlock 也已入库。本项要做的：
- 整段删除不可用分区代码。交付说明和 PROGRESS 写成：「删掉的是 G0 已修过的路径，之后失败分支里不再有二次加锁的代码」。
- 保留 lock_test.go 的文件名和用例名（Codex 任务单按名引用），把内部改成不依赖被删的符号。
- 如实写出唯一的非分区行为变化：去掉了「所有分区都被标不可用时立即报 no available partition」的快速失败，见 risks 第 5 条。
- 回滚说明写成：回滚目标镜像必须 ≥ d5cbb8a8e，否则会把自锁带回来。
- **R2**：修正 2：可选 C++ 批次的行号全错；子模块要先检出；golden 测试加一条「默认 topic conf 导出的 partitioner == consistent_random」。
  - 处理：按当前工作区重新定位：
- kafka_producer.cpp 的 createProducer 在 :49-88，conf->set 在 :59-76。新的 set("partitioner") 插在 :76（enable.idempotence 那段）之后、:78 Producer::create 之前。produceOnce 在 :106-122。
- 子模块现在已检出（git submodule status 行首没有 '-'，f1b831e）。rollout B0 仍让 Codex 先核对；如果需要 init，属于联网下载，要先征得负责人同意。
- 新增用例 VendoredDefaultPartitionerIsConsistentRandom（rd_kafka_topic_conf_new + rd_kafka_topic_conf_get）和 ExplicitPartitionerOnGlobalConfReachesDefaultTopicConf，后者用 rd_kafka_conf_set 加 rd_kafka_conf_get_default_topic_conf，与 ConfImpl::set（src-cpp/ConfImpl.cpp:37-54）对 CONF_GLOBAL 的落点一致。
- **R3**：修正 3：防线收口。不能只拦 p.topic；NewCommandAwarePartitioner 对 db_task_zone_ 前缀返回 kafkautil.NewDBTaskPartitioner；SendToTopic 按前缀拒绝；command_partitioner_test.go:24 的 db_task 条目不删，改为断言 CRC 契约分区。
  - 处理：做法：
- 新增 kafkautil.IsDBTaskTopic，判定 HasPrefix("db_task_zone_")。
- NewCommandAwarePartitioner 先判命令 topic，再判 db_task topic，命中后者返回 NewDBTaskPartitioner。
- SendToTopic 和 SendToTopicPartition 都按前缀拒绝。后者也拒，因为 plainProducer 对 db_task 装的是契约分区器，会覆盖显式分区。
- 测试里 "db_task_zone_1_g2" 和新加的 "db_task_zone_1" 改为断言落在 kafkautil.DBTaskPartition 上。
- "db_task" 保留在列表里，但仍断言走默认哈希。理由：全仓不存在叫 db_task 的 topic（历史名是 db_task_topic，现在的命名规则是 DbTaskTopicForGeneration），把前缀放宽到 "db_task" 会误伤无关 topic。覆盖面没有变小。
- **R4**：修正 4：屏障覆盖范围要补第四个缺口（父键缺失但子表缓存还在时不发读），并订正「Redis 驱逐」这个触发条件。
  - 处理：写进契约文档新节「屏障覆盖范围与剩余缺口」，并在 key_ordered_consumer.go processTaskBatch 的文档注释里列出缺口。共五个：
1. 访客或合服后跨 topic（读走登录 zone 的 topic，写走 home_zone 的 topic）。
2. 写已转进 Redis 重试，读不会等它。
3. 重试被非属主实例执行（④ 之前）。
4. 父键缺失、子表缓存还在（ensure_player_all_data_async.go:79-90）。
5. 再平衡排空超时后旧实例仍在执行的写（G1）。

触发条件订正（已核对）：
- C++ 用 Lua 无 TTL 地 SET 父键（redis_client.h:21）。
- 本机是 noeviction（docker-compose.yml:236-237），K8s 不设 maxmemory（redis.yaml:137-139）。
- 所以现网不存在驱逐，真实触发只有 Redis 丢数据、FLUSH、运维删键。
- 文档写明：这是同 zone 内的正确性补洞，现网频率低。
- **R5**：修正 5：I4 冷读冒烟会假阴性。超时要判为不确定；删键前要确认 W 真的发出了；补一条按 broker 门控的确定性集成测试。
  - 处理：rollout C4 用修订后的冒烟流程：
- 预加载超时（日志里出现 task result wait timed out）判为「不确定、重跑」。
- 只有读到的 test_seq 小于 W 的 test_seq 才判失败。
- 删键前必须先看到 scene 日志 "[SavePlayerToRedis] Player <id> saved to Redis, DB write tasks enqueued"（断线租约到期之后才会存盘）。

另外新增 TestDBTaskReadAndWriteShareKafkaPartitionIntegration（放在 go/login/internal/kafka/dbtask_partitioner_test.go，没有 KAFKA_INTEGRATION_BROKER 时 Skip）：
- 用 kafkautil.NewDBTaskPartitioner 发 W，模拟 C++；
- 用 newDBTaskProducerConfig 发 R；
- 断言两条消息分区相同，且都等于 DBTaskPartition；
- 再断言 offset(R) > offset(W)。
- **R6**：修正 6：第 3 步不要为了观测专门滚一次生产 go/db（共享 GroupID 会让全组 eager 再平衡；maxSurge=1 会让 MySQL 连接临时超限），门槛改为只读抽样，observer 随下一次例行 go/db 发版一起上。
  - 处理：rollout 第 1 步改为只读抽样：用 Kafka 镜像自带的 kafka-console-consumer，带 --partition，不加入消费组、不提交位点，值用 BytesDeserializer 打成 \xNN 转义。抽到的输出交给新增的 opt-in 用例 TestDBTaskPartitionAuditDump，用契约函数本身离线算分区。通过标准：写不符为 0。

go/db 的 observer 不单独发版，随本轮第一个 go/db 例行发版上线（S1、S2、S4 都要发 go/db），按交接 §3.5 和瓶颈文档 §4.5 的红线执行。
- **R7**：修正 7：文档订正清单漏了几处：login-simplification-2026-04.md:68、契约文档 :75、key_ordered_consumer.go:847 与 :1279-1281；契约文档要写明「两边 topic 名必须相同」。
  - 处理：全部列进 changes。
- 文档：ARCH.md:165、db-task-kafka-partition-contract.md:9-12/:25-26/:74-75 和四个新节、infra-reconnect-overview.md:26、cross-zone-readiness-audit.md:149（加更正注，不改原文）、login-simplification-2026-04.md:68。
- 代码注释（当前行号）：key_ordered_consumer.go:721-727、:851（即复核说的 :847）、:1212-1213、:1222-1224、:1278-1280（即 :1279-1281）、:1409-1414；key_ordered_producer.go:28/:43-45；expand_monitor.go:15-17；command_partitioner.go:25-27；data_stress/main.go:4-6。
- 契约文档写明：屏障前提是 login 的 DbTaskTopicForGeneration(登录 zone) 等于 C++ 的 GetDbTaskTopic(home_zone)（player.h:19-27），访客天然不满足。
- docs/design/architecture-current-state-vs-gaps-2026-05.md:24 是历史快照，不改。
- **R8**：修正 8：交给 Codex 的命令要按 §10.1 补全：L1 加 login 套件前确认全部 hermetic；C++ 必须串行 /m:1，并以子模块已检出为前置；golden 表要用 Go crc32 和 librdkafka 双向机械复核。
  - 处理：rollout 第 0 步给出完整命令、工作目录、通过标准和失败时要保留的东西。
- login/internal/kafka 的既有用例（command_partitioner_test、partition_contract_test、lock_test）都是 hermetic；新加的集成用例没有环境变量时 Skip。
- C++ 用全量 msbuild /m:1，再跑 run_cpp_tests.ps1 -Build -Filter kafka_command_test。
- golden 表做三方机械复核：Go crc32.ChecksumIEEE、sarama 分区器、librdkafka rd_kafka_msg_partitioner_consistent_random 都对同一份 JSON 断言。不一致时以机器结果为准回报，不直接改表。
- **H1**：交接 §4.1 第 5 行：「C++ 是否所有调用方都不传 partition 未逐一核对」，上线前用 kcat 抽样。
  - 处理：静态已逐一核对，清单见 current_state：db_task 只有 player_lifecycle.cpp:4026 一个生产点，不传 partition；KafkaProducer 不读任何外部 librdkafka 配置。线上实际分布由 rollout 第 1 步的抽样证实（用镜像自带的 console consumer，免下载；装了 kcat 的话可以只用来做分区核对）。
- **H2**：交接 §4.2「过时说法订正」里与分区相关的部分（ARCH.md「playerId 一致性哈希到同一分区」、契约文档第 9 行、go/db 几处注释），与 S4 的「过时注释」重叠。
  - 处理：建议这部分全部归 S3（见 decisions D1）。S4 只做 store_registry.go 的 REPLACE INTO，以及 migrate/plan.go 的 ON DUPLICATE KEY 三处。
- **H3**：交接 §5 第 9 项：行为变化（db 积压超过 8 秒时进场失败）。
  - 处理：负责人已于 2026-10-13 接受。
- 来源：entergamelogic.go:421 的 dispatcherTaskTTL，再加 dispatcher 1s 扫描粒度和一次补救 LPOP。
- 客户端看到的：gate 推 tip 3023 kEnterSceneFailed；gRPC 应答早已是成功。
- 指标：entergame_total、entergame_preload_seconds{result="preload_failed"}、entergame_failure_notify_total{stage="preload"}、dataloader_preload_callback_*{result="failed"}。
- 不新增 timeout label，以免打断 stress_summarize 的对比口径。排障看日志 "task result wait timed out"、按分区的 lag 和 db_task_stage_seconds{op="read"}。
- 写进契约文档和 PROGRESS 的发布说明。

## 逐文件改动
### [new] go/shared/kafkautil/dbtask_partition.go
约 70 行，包内唯一的契约实现。

包注释（中文）写清楚：
- key = 玩家 id 的无符号十进制 ASCII，无前导零（Go strconv.FormatUint / C++ std::to_string(uint64_t)）。
- partition = CRC-32/IEEE(key) mod N，按 uint32 无符号取模，就是 librdkafka consistent_random 对非空 key 的 rd_crc32(key)%partition_cnt；sarama.NewConsistentCRCHashPartitioner 是它的 Go 对应物。
- 空 key 不在契约内：librdkafka 对空 key 随机分区，sarama 对非 nil 的空 key 落 0 号分区。
- 换函数 = 换 TopicGeneration（离线排空），否则写换分区后会被判 orderingIncomparable 进死信。

导出：
- const DBTaskTopicPrefix = "db_task_zone_"
- var ErrEmptyDBTaskKey = errors.New("kafkautil: empty db_task key is outside the partition contract")
- var ErrInvalidDBTaskPartitionCount = errors.New("kafkautil: db_task partition count must be positive")
- func IsDBTaskTopic(topic string) bool { return strings.HasPrefix(topic, DBTaskTopicPrefix) }
- func DBTaskKey(playerID uint64) string { return strconv.FormatUint(playerID, 10) }
- func AppendDBTaskKey(dst []byte, playerID uint64) []byte { return strconv.AppendUint(dst, playerID, 10) }
- func DBTaskPartition(key []byte, partitionCount int32) (int32, error)：partitionCount<=0 时返回 -1 和 fmt.Errorf("%w: %d", ErrInvalidDBTaskPartitionCount, partitionCount)；len(key)==0 时返回 -1 和 ErrEmptyDBTaskKey；否则返回 int32(crc32.ChecksumIEEE(key) % uint32(partitionCount))。
- func NewDBTaskPartitioner(topic string) sarama.Partitioner { return sarama.NewConsistentCRCHashPartitioner(topic) }，写成函数而不是可变的包变量。
- 编译期断言 var _ sarama.PartitionerConstructor = NewDBTaskPartitioner。

import：errors、fmt、hash/crc32、strconv、strings、github.com/IBM/sarama（shared 的 go.mod 已有 v1.43.1）。

### [new] go/shared/kafkautil/testdata/dbtask_partition_vectors.json
Go 与 C++ 共读的 golden 向量，格式仿 go/shared/kafkacmd/testdata/command_partition_vectors.json。

结构：{"schema":"<中文说明：契约、两个读取方、文档路径；不一致时以机器复核结果回报，不直接改表>","check":{"key":"123456789","crc32":3421780262},"cases":[{"key":"...","n":N,"expect_partition":P},...]}。

共 45 条 cases。每行依次是 key，然后 N=7、10、16、20 四个分区号：
- "0" → 4, 9, 1, 9
- "1" → 2, 3, 7, 3
- "42" → 3, 8, 8, 8
- "1001" → 3, 3, 1, 13
- "10001" → 4, 5, 3, 15
- "123456789" → 5, 2, 6, 2
- "7311864093319258112" → 5, 7, 3, 7
- "7311864093319258113" → 5, 9, 5, 9
- "7311864094007124480" → 2, 8, 4, 8
- "9223372036854775807" → 5, 2, 14, 2
- "18446744073709551615" → 6, 2, 10, 2
- 外加一条 {"key":"123456789","n":2147483647,"expect_partition":1274296615}。0xCBF43926=3421780262，所以无符号取模得 1274296615，sarama referenceAbs 得 1274296614，有符号取模取反得 873187034，这一条能区分三种取模写法。

数值来源：
- 附录作者与复核者各用手写 CRC 独立算过一次（PowerShell，自检值 CBF43926 / E40C292C 通过）。
- 本规格又做了两项核对："0"、"1"、"123456789" 三行用公开 CRC 值 0xF4DBDF21、0x83DCEFB7、0xCBF43926 手工取模核对过（前两个值凭记忆，待机器复核）；整表做了交叉自洽检查——N=20 的值 mod 10 等于 N=10 的值，N=16 与 N=20 的值 mod 4 逐个相同。
- 最终以 Codex 跑三方机械复核为准。

### [new] go/shared/kafkautil/dbtask_partition_test.go
package kafkautil。从 testdata/dbtask_partition_vectors.json 读向量（结构体加 json tag，仿 kafkacmd 的 command_topic_test.go:11-44）。用例见 tests 第 1 项。

同一文件内还放上线抽样用的离线复核器（它是 opt-in 测试，不是生产代码）：
- parseConsoleConsumerLine(line string) (partition int32, offset int64, key, value string, err error)：按 \t 切分；取 "Partition:" 和 "Offset:" 前缀字段；其后第一个字段是 key，剩余字段用 \t 拼回作 value。
- classifyDumpOp(value string) string：含 `\x1A\x05write` 返回 "write"，含 `\x1A\x04read` 返回 "read"，否则返回 "other"。DBTask.op 是 3 号字段，tag 0x1A，长度 5 或 4；BytesDeserializer 会把反斜杠本身转成 \x5C，所以消息体伪造不出这两个标记。
- TestDBTaskPartitionAuditDump：读环境变量 DBTASK_AUDIT_DUMP（文件路径）和 DBTASK_AUDIT_PARTITIONS（N），任一为空就 t.Skip。
  - key 判定：ParseUint 失败，或 DBTaskKey(id)!=key，判 key_mismatch；DBTaskPartition([]byte(key),N)!=partition，判 partition_mismatch；否则判 ok。
  - 按 op×verdict 计数，用 t.Logf 打汇总表、读不符比例和前 20 条不符（partition/offset/key/op）。
  - 以下任一情况 t.Fatalf：写的非 ok 计数 >0；有解析失败的行；有效行数为 0（空抽样也算失败）。

### [modify] go/login/internal/kafka/key_ordered_producer.go
(1) import：删 "login/internal/logic/pkg/consistent" 和 "strings"，加 "shared/kafkautil"。

(2) :28 的类型注释改为中文：「KeyOrderedKafkaProducer 是 db_task 读请求的生产者（SyncProducer，幂等）。分区由 kafkautil.NewDBTaskPartitioner 决定：CRC-32(key) mod N，与 C++ scene 写存盘用的 librdkafka consistent_random 是同一个函数；同一玩家的读与写落在同一分区，go/db 的读后写屏障才成立（docs/design/db-task-kafka-partition-contract.md）」。

(3) 结构体：删 consistent 字段（:37）和 unavailablePartitions、retryInterval（:48-50）。:43-45 的 routingFenced 注释改为「本进程一旦置位不可逆。broker 分区数漂移会改变 CRC-32(key) mod N 的映射，继续发送就违反 go/db 按分区记录的 applied cursor 顺序」。

(4) 新增 func newDBTaskProducerConfig(cfg config.KafkaConfig) *sarama.Config：
- 把 :58-71 的 13 项原样搬进来。局部变量起名 c 或 saramaCfg，不要用 config，否则会遮蔽 login/internal/config 包。
- 末尾加 c.Producer.Partitioner = kafkautil.NewDBTaskPartitioner。
- 函数注释写：Partitioner 必须在 sarama.NewClient 之前设，因为 NewSyncProducerFromClient 用的是 client 上的配置；sarama 默认的 NewHashPartitioner 是 FNV-1a，与 C++ 不是同一个函数，而且会覆盖 ProducerMessage.Partition，这就是旧哈希环从来没生效的原因。
- NewKeyOrderedKafkaProducer 改为 saramaCfg := newDBTaskProducerConfig(cfg)，再 Validate()，再 sarama.NewClient(cfg.Brokers, saramaCfg)。
- :85-103 的分区契约门禁和 :116-135 的 plainCfg 不动。

(5) 删除：:84 consistentHash；:111-113 AddPartition 循环；:139-140；kp 字面量里 consistent、unavailablePartitions、retryInterval 三项（:149、:151-152）；:164 的 safego.Go("login.kafka_check_unavailable_partitions", …)（全仓没有别处引用这个点位名）。

(6) SendTasks：
- 删 :181-184。
- 在 TaskId 判空之后加 key 契约校验：if want := kafkautil.DBTaskKey(task.Key); key != want { p.recycleMessages(msgs); return fmt.Errorf("db_task kafka key %q does not match task key %d (want %q)", key, task.Key, want) }。
- ProducerMessage 去掉 Partition 字段（:205）。
- ctx.Done 分支删 markPartitionUnavailable（:217）。
- 失败日志改为 logx.Errorf("batch send failed: topic=%s, partition=%d, key=%s, count=%d, err=%v", p.topic, msgs[0].Partition, key, len(msgs), err)，删 :226-228。
- 成功路径在 recycleMessages 之前加 logx.Debugf("batch sent: topic=%s, partition=%d, firstOffset=%d, lastOffset=%d, count=%d", …)，值取 sarama 回填的 msgs[0] 和 msgs[len-1]。

(7) SendTask：
- 在参数校验之后、marshal 之前加同样的 key 校验。
- 删 :282-286；ProducerMessage 去掉 Partition（:292）；ctx.Done 分支删 :303。
- :307 改为 partition, offset, err := p.producer.SendMessage(msg)。
- 失败日志改为 logx.Errorf("send failed: task=%s, topic=%s, partition=%d, err=%v", task.TaskId, p.topic, msg.Partition, err)，删 :312-314。
- :345 改为 logx.Debugf("message sent: task=%s, partition=%d, offset=%d", task.TaskId, partition, offset)。

(8) SendToTopic（:380）：在命令 topic 检查之后加 if kafkautil.IsDBTaskTopic(topic) { return fmt.Errorf("refusing to send db_task topic %s through the plain producer: use SendTask/SendTasks (partition contract)", topic) }。SendToTopicPartition（:401）在负分区检查之前加同样的拒绝。两个方法的注释各补一句理由。

(9) monitorStats（:510-514）：去掉 getUnavailablePartitionCount，日志改为 "message stats: topic=%s, interval=%v, success=%d, errors=%d"。

(10) 删除 :531-625 的六个函数：markPartitionUnavailable、markPartitionUnavailableLocked、getAvailablePartition、checkUnavailablePartitions、isPartitionUnavailableErr、getUnavailablePartitionCount。

行为变化：除分区之外，唯一的变化是去掉了「所有分区都被标不可用时立即报 no available partition」的快速失败。

依赖：go/shared/kafkautil/dbtask_partition.go

### [modify] go/login/internal/kafka/key_ordered_producer_lock_test.go
文件名和用例名 TestSendTaskLeaderNotAvailableDoesNotDeadlock 都保留（Codex 任务单按名引用）。
- newLockTestProducer(t, sp, partitions)：去掉 consistent 环、unavailablePartitions、retryInterval；import 去掉 pkg/consistent。
- 用例里删掉对 isPartitionUnavailableErr 的前置断言和 getUnavailablePartitionCount 的断言。
- 保留「第一次 SendTask 因 ErrLeaderNotAvailable 失败，第二次在 5s 内成功」。
- 末尾改为在 goroutine 里调 p.mu.TryLock()：成功就 Unlock 并通过；5s 超时则 t.Fatal("p.mu still held after SendTask returned")。
- 注释改为：「回归（G0，d5cbb8a8e）：失败分支曾在持有 p.mu 时二次加锁。本项删掉了整套不可用分区代码，这条用例防止以后有人在失败分支里再引入二次加锁」。

依赖：go/login/internal/kafka/key_ordered_producer.go

### [delete] go/login/internal/logic/pkg/consistent/consistent.go
连同目录整个删掉。删之前 grep 确认引用只剩 key_ordered_producer.go 和 lock_test.go（两处本项都会改掉）。

依赖：go/login/internal/kafka/key_ordered_producer.go

### [modify] go/login/internal/kafka/command_partitioner.go
- NewCommandAwarePartitioner（:28-34）：在 IsCommandTopic 分支之后、默认分支之前加 if kafkautil.IsDBTaskTopic(topic) { return kafkautil.NewDBTaskPartitioner(topic) }，配中文注释：db_task 的分区函数只有一个权威来源；plainProducer 本来就不该发 db_task（SendToTopic 和 SendToTopicPartition 都会拒绝），这里仍然装契约分区器，保证万一有绕过的路径也落在同一分区上。
- import 加 "shared/kafkautil"。
- :25-27 的注释订正为：「db_task 不经 plainProducer 发送；主生产者用 kafkautil.NewDBTaskPartitioner；plainProducer 遇到 db_task_zone_ 前缀同样装契约分区器；其它非命令 topic 保持 sarama.NewHashPartitioner」。

依赖：go/shared/kafkautil/dbtask_partition.go

### [modify] go/login/internal/kafka/command_partitioner_test.go
- TestCommandAwarePartitionerScope：把 :24 的列表拆成两组。
  - dbTaskTopics {"db_task_zone_1","db_task_zone_1_g2"}：断言不是 commandPartitioner；对 key "1001" 和 "7311864093319258112"、N=8 和 N=10，分区都等于 kafkautil.DBTaskPartition。
  - otherTopics {"db_task","gate-5","scene-77","player_snapshot_topic_g1"}：保持原断言（与 NewHashPartitioner 同分区）。
  - 更新顶部注释。
- 新增 TestSendToTopicRefusesDBTaskTopics：p := &KeyOrderedKafkaProducer{}，对 "db_task_zone_1" 和 "db_task_zone_1_g2" 都必须返回 error。plainProducer 为 nil，所以也证明了拒绝发生在触碰生产者之前。
- 新增 TestSendToTopicPartitionRefusesDBTaskTopics，写法同上。

### [modify] go/login/internal/kafka/expand_monitor.go
只改 :15-17 的注释，换成中文：「ExpandMonitor 只探测外部分区漂移并 fence 生产者。CRC-32(key) mod N 的映射由分区契约固定，在线扩分区保不住 go/db 按分区记录的 applied cursor，所以一旦漂移只 fence，不做任何重映射」。

### [new] go/login/internal/kafka/dbtask_partitioner_test.go
package kafka。
- 辅助函数 testKafkaConfig() config.KafkaConfig：PartitionCnt=10，InitialPartition=10，三个超时各 1s，RetryMax=3，RetryBackoff=100ms，ChannelBuffer=256，Idempotent=true。
- 辅助函数 newLockTestProducer 复用 lock_test 里那个。
- 用例见 tests 第 2 项，其中包含按 broker 门控的集成用例。

### [modify] go/db/internal/metrics/metrics.go
- var 块加 producerPartitionTotal = prometheus.NewCounterVec(CounterOpts{Subsystem: subsystem, Name: "task_producer_partition_total", Help: "Kafka db_task messages checked against the producer partition contract (CRC-32(key) mod N, docs/design/db-task-kafka-partition-contract.md). op: read | write | other. verdict: ok | partition_mismatch | key_mismatch | invalid. Observation only: no message is rejected."}, []string{"op","verdict"})，导出名为 db_task_producer_partition_total。
- register()（:172-192）的 MustRegister 加上它；在预置循环里对 producerPartitionOps × producerPartitionVerdicts 共 12 条执行 .Add(0)，让 increase 能抓到第一次。
- 新增常量 ProducerPartitionOpRead="read"、ProducerPartitionOpWrite="write"、ProducerPartitionOpOther="other"，ProducerPartitionOK="ok"、ProducerPartitionMismatch="partition_mismatch"、ProducerPartitionKeyMismatch="key_mismatch"、ProducerPartitionInvalid="invalid"，以及两个切片。
- 新增 func CountProducerPartition(op, verdict string) { register(); producerPartitionTotal.WithLabelValues(op, verdict).Inc() }。
- label 是封闭集合，不含 player_id。按追加方式写，方便与 S1、S4 的新增合并。

### [new] go/db/internal/kafka/producer_partition_audit.go
package kafka，只观测。

- func observeProducerPartition(msg *sarama.ConsumerMessage, task *db_proto.DBTask, partitionCount int32) string：算出 op 和 verdict，调 metrics.CountProducerPartition；verdict 不是 ok 时调 logProducerPartitionMismatch；返回 verdict。
- producerPartitionOp(task)：task 为 nil 返回 other；"read" 和 "write" 原样返回；其余返回 other。
- classifyProducerPartition(msg, task, n) (verdict string, expected int32)：
  1. msg 为 nil、task 为 nil 或 n<=0 → (invalid, -1)。
  2. var buf [20]byte; want := kafkautil.AppendDBTaskKey(buf[:0], task.Key)；!bytes.Equal(msg.Key, want) → key_mismatch，expected 用 want 算出来，供日志使用。nil 或空 key 也落在这一支。
  3. exp, err := kafkautil.DBTaskPartition(msg.Key, n)；err → (invalid, -1)。
  4. exp != msg.Partition → (partition_mismatch, exp)；否则 (ok, exp)。
- 限量日志：const producerPartitionLogBudget = 20；var producerPartitionLogged [3]atomic.Int64，下标 0=read、1=write、2=other；allowProducerPartitionLog(op) bool 判断 Add(1) 之后是否 ≤ 20。
  - read 用 logx.Infof("db_task read outside the partition contract (old login?): verdict=%s topic=%s partition=%d expectedPartition=%d offset=%d kafkaKey=%q taskKey=%d", …)。
  - write 和 other 用 logx.Errorf("DATA-ORDERING: db_task producer violated the partition contract: verdict=%s op=%s topic=%s partition=%d expectedPartition=%d offset=%d kafkaKey=%q taskKey=%d msgType=%s", …)。
- 中文注释写清：不拒绝，因为拒写会丢存盘、拒读会打断滚动窗口里的旧 login；遗留重试载荷照旧按原规则 fail-closed，observer 不替它补分区；重试任务不经 ConsumeClaim，不在观测范围内；计数按每次投递算，重投递会重复计。

依赖：go/db/internal/metrics/metrics.go

### [new] go/db/internal/kafka/producer_partition_audit_test.go
用例见 tests 第 5 项。计数从 prometheus.DefaultGatherer.Gather() 读：写一个小 helper 按 op 和 verdict 找 db_task_producer_partition_total 的那条序列，不依赖 testutil。

依赖：go/db/internal/kafka/producer_partition_audit.go

### [modify] go/db/internal/kafka/key_ordered_consumer.go
唯一的执行路径改动：在 ConsumeClaim 里解码失败分支收尾的 `}`（当前 :1791）之后、:1793 注释之前、ensureWorker 之前，插入：
// 分区契约观测（db-task-kafka-partition-contract.md「分区函数契约」）：只计数、限量打日志，不拒绝消息，不改 ack、游标、重试。写不符说明有生产者破坏契约（critical 告警）；读不符说明还有旧版 login 在跑。
observeProducerPartition(msg, &dbTask, h.consumer.partitionCount)

以下只改注释，给出替换文字。S1 如果重写了所在函数，就把这些文字带过去：
- :724-727 wrapRetryPayload：「生产端分区按 kafkautil.DBTaskPartition（CRC-32(key) mod N）决定，消费端拿 Key%N 重算得到的通常是另一个分区……原始分区一律以载荷里记的为准，不用契约函数重算：载荷记的是消息实际所在的分区，契约被破坏时两者会不一样」。
- :851 writeBackDBCache：「写回 Redis 缓存。同一 key 的写在同一分区、同一子 shard 内串行，跨实例由排序锁兜底（见 handleTask）」。
- :1212-1213：「Key%N 不是生产端的分区函数；按契约重算也只能得到应在的分区，证明不了原消息实际在的分区」。
- :1222-1224：「同 key 的新消息在生产端按分区契约（kafkautil.DBTaskPartition）落到的分区上串行」。
- :1278-1280：「按 spreadKey(task.Key) % subShardCount 分发到子 shard（见 subShardIndex）」。
- :1409-1414 processTaskBatch 文档：在原注释后追加读屏障的前提——两侧生产者都守分区契约、topic 名相同——并列出五个不覆盖的情形（同 blocking R4）。

不动 handleTask、runDBOp，也不动 markAppliedSeq 和 markAppliedOwnerEpoch（:1670、:1674）。

依赖：go/db/internal/kafka/producer_partition_audit.go

### [modify] go/db/cmd/data_stress/main.go
- import 加 "shared/kafkautil"。
- :189 改成 playerKey := kafkautil.DBTaskKey(playerID)。改完之后 strconv 在这个文件里不再有用处，删掉这个 import。
- :108 之后加 cfg.Producer.Partitioner = kafkautil.NewDBTaskPartitioner，配中文注释：与 C++ scene 写存盘用的是同一个分区函数，不设就是 sarama 默认的 FNV，同一玩家会落到别的分区，go/db 会把它判为 orderingIncomparable。
- 头注释 :4-6 改成中文：key 用 kafkautil.DBTaskKey，分区器用 kafkautil.NewDBTaskPartitioner，所以与生产 scene 的写落在同一分区。

依赖：go/shared/kafkautil/dbtask_partition.go

### [modify] go/test.ps1
- $L1Suites（:63-79）的 shared 那一组 Packages 加 './kafkautil/...'。集成用例没有 KAFKA_INTEGRATION_BROKER 时 Skip，见 topic_init_integration_test.go:16；opt-in 的 AuditDump 用例没有环境变量时也 Skip。
- 新增一组 @{ Dir = Join-Path $GoRoot 'login'; Packages = @('./internal/kafka/...') }。
- 顶部 .DESCRIPTION 补两行说明这两组覆盖什么。

### [new] deploy/k8s/db-task-partition-alerts.yaml
PrometheusRule，写法同 owner-epoch-alerts.yaml。metadata.name 为 db-task-partition-alerts，labels 为 app: db、prometheus: kube-prometheus、role: alert-rules。

头注释写：来源文档；指标是 db_task_producer_partition_total，进程启动即建出 12 条 0 值序列；安装命令 kubectl apply -n observability -f …；读规则必须等 login 全部滚完再 apply；抓取前提是 go/db 的 :9160 加 ServiceMonitor。

规则组 db-task.partition-contract，interval 30s：
- DbTaskWritePartitionContractBroken：expr sum by (namespace) (increase(db_task_producer_partition_total{op="write",verdict!="ok"}[10m])) > 0，for 0m，severity critical。description 写处置：按 go/db 日志 "DATA-ORDERING: db_task producer violated the partition contract" 里的 topic、partition、key 找出写入方（误跑了旧版 data_stress、scene 构建改了 partitioner、librdkafka 升级改了默认值），并同时看 db_task_dead_letter_total{reason="ordering_incomparable"}。
- DbTaskReadPartitionMismatch：expr sum by (namespace) (increase(db_task_producer_partition_total{op="read",verdict!="ok"}[30m])) > 0，for 30m，severity warning。description：还有旧版 login 在跑，或者回滚到了旧镜像；读本身不写数据，但这些读没有读后写屏障。

### [modify] cpp/libs/engine/infra/messaging/kafka/kafka_producer.cpp
在 createProducer 的 :76（enable.idempotence 那段结束）之后、:78 RdKafka::Producer::create 之前插入 conf->set("partitioner", "consistent_random", errstr)。返回不是 CONF_OK 时只打 LOG_ERROR << "Kafka producer: failed to pin partitioner=consistent_random (db_task partition contract): " << errstr;，不 return false。

中文注释写清：
- 这是玩家存盘 DBTask 的分区契约（CRC-32(key) mod N），login 的读用同一个函数，见 go/shared/kafkautil/dbtask_partition.go。
- 值等于 vendored 2.14.0 的默认值，显式写出来是为了把契约钉在代码里：librdkafka 升级改了默认值、或者有人在这里改了 partitioner，同一玩家的写就会在新旧 scene 之间换分区，被 go/db 判为 orderingIncomparable 送进死信。
- 换函数 = 换 TopicGeneration。
- 设置失败不阻止建实例：拒绝建实例会让本节点所有 Kafka 发送（包括存盘和命令）全部失败。

这个 topic 级属性设在 CONF_GLOBAL 上，会落到默认 topic conf（rd_kafka_conf_set 的 fallthru 逻辑，rdkafka_conf.c:2567-2591）。battle 命令显式传分区，审计 topic 本来就用默认分区器，运行时零变化。

### [modify] cpp/libs/services/scene/player/constants/player.h
在 :27 之后新增 inline std::string DbTaskKafkaKey(uint64_t playerId) { return std::to_string(playerId); }。中文注释写：玩家存盘 DBTask 的 Kafka key 是玩家 id 的无符号十进制 ASCII，与 Go 的 kafkautil.DBTaskKey（strconv.FormatUint(id,10)）逐字节相同；分区 = CRC-32(key) mod N，key 字节一变，同一玩家的写就会换分区，被判为 orderingIncomparable 进死信；契约见 docs/design/db-task-kafka-partition-contract.md。头文件已经 include <cstdint> 和 <string>。

### [modify] cpp/libs/services/scene/player/system/player_lifecycle.cpp
只改一行：SavePlayerToRedis 里的 :3989 改成 const std::string playerIdStr = DbTaskKafkaKey(playerId); // Kafka key 与 Go 侧 kafkautil.DBTaskKey 逐字节相同（分区契约），task_id 前缀沿用同一串。player.h 已在 :44 include，字节不变。

该文件现在正被别的会话修改（git status 显示 M），S5 也要动同一函数里发送失败的分支（:4026-4032）。落码前先 git diff 确认不在同一 hunk，只改这一行。

依赖：cpp/libs/services/scene/player/constants/player.h

### [new] cpp/tests/kafka_command_test/kafka_partition_contract_test.cpp
- 文件开头先 #ifndef LIBRDKAFKA_STATICLIB / #define LIBRDKAFKA_STATICLIB / #endif，再依次 include <gtest/gtest.h>、<yaml-cpp/yaml.h>、<filesystem>、<cstdint>、<limits>、<string>、"librdkafka/src/rdkafka.h"、"player/constants/player.h"。
- 匿名命名空间里写 FindDbTaskVectorFile()：从 fs::current_path() 逐级上溯最多 8 层，找 "go/shared/kafkautil/testdata/dbtask_partition_vectors.json"。main() 可能已经把 cwd 换到 bin/（仓库根的下一级），上溯就能找到。不复用 kafka_command_test.cpp 的 FindVectorFile，因为它写死了另一份向量的路径，改它属于无关重构。
- 四个用例见 tests 第 6 项。本文件不写 main，main 在 kafka_command_test.cpp。

依赖：go/shared/kafkautil/testdata/dbtask_partition_vectors.json

### [modify] cpp/tests/kafka_command_test/kafka_command_test.vcxproj
在 ClCompile 的 ItemGroup（当前 :161-163）里按字母序追加一行 <ClCompile Include="kafka_partition_contract_test.cpp" />。该文件正被别的会话修改（新增了 kafka_manager_init_test.cpp、去掉了 rdkafka++.lib），只追加这一行，不回退、不调整别人的改动。cpp/tests 不在 CMake 里，不需要改 CMakeLists。本工程没有 .filters 文件。rdkafka.lib 仍在 AdditionalDependencies 里，C API 能链上。

依赖：cpp/tests/kafka_command_test/kafka_partition_contract_test.cpp

### [modify] docs/design/db-task-kafka-partition-contract.md
订正三处：
- :9-12 的「原因」改为：同一玩家的读（login）与写（C++ scene）都按 CRC-32(player_id 十进制) mod N 选 partition，见下文「分区函数契约」；原文「login 生产者按 player_id 一致性哈希」从来没生效过。
- :25-26 门禁 3 的「不动态修改 hash ring」改为「CRC-32 mod N 的映射随 N 变化，漂移只 fence，不做任何重映射」。
- :74-75 改为「不等于保留同一分区映射与 applied cursor 的因果关系」。

代码门禁加第 6 条：Go 侧唯一来源是 kafkautil，C++ 显式钉 consistent_random，两侧共读同一份向量，go/db 观测计数，写不符 critical。

新增四节：
- 「分区函数契约」：key、算法参数、空 key 例外、两侧实现、生产者清单（2026-10-15 静态核对）、换函数 = 换代。
- 「golden 向量」：文件路径、数值来源、三方复核、那条区分取模写法的校验向量。
- 「滚动对齐步骤与观测」：同 rollout 第 1–6 步，并附抽样命令。
- 「屏障覆盖范围与剩余缺口」：覆盖范围；前提是两侧 topic 名相同（DbTaskTopicForGeneration ↔ GetDbTaskTopic）；五个缺口；触发条件订正；行为变化（8s → tip 3023）；与条件写的分工（条件写不校验读，两者互补，不能互相替代）。

### [modify] docs/design/ARCH.md
:165 改为：「2. Kafka db_task 分区契约：key = playerId 十进制，partition = CRC-32(key) mod N（login 的读与 C++ scene 的写用同一个函数，见 db-task-kafka-partition-contract.md「分区函数契约」），同一 topic 内同一玩家的读写落同一分区、按 offset 串行；访客和合服后跨 zone 的读写分属两个 topic，不在此保证之内。」

### [modify] docs/design/infra-reconnect-overview.md
:26 Kafka (login) 那一行的 Notes 改为：「db_task 主生产者按分区契约 CRC-32 mod N 分区（kafkautil.NewDBTaskPartitioner，与 C++ librdkafka consistent_random 同一个函数）；分区漂移时永久 fence；重连和重试靠 sarama（Retry.Max）；控制面命令走 plainProducer 的显式分区。（2026-10-15 订正：原文写的 Partition unavailability tracking / consistent hash remap 从来没生效，已删除）」

### [modify] docs/design/cross-zone-readiness-audit.md
在 :149 之后插一条更正注（历史审计不改原文）：「> 更正（2026-10-15）：这一条当时不成立——login 的读走 sarama 默认 FNV，C++ 的写走 CRC32，读写不在同一分区。分区契约对齐之后，同一 topic 内成立；跨 topic（访客、合服）仍然不成立。见 db-task-kafka-partition-contract.md。」

### [modify] docs/design/login-simplification-2026-04.md
:68 改为：「   - 分区 = CRC-32(key) mod N（kafkautil.NewDBTaskPartitioner，与 C++ scene 写存盘是同一个函数，见 db-task-kafka-partition-contract.md），同一 topic 内同 key 同分区。（2026-10-15 订正：原文的 Consistent hash 从来没生效）」

### [modify] docs/PROGRESS.md
只在末尾追加，不改旧条目。标题：「## 2026-10-15 存盘管线 ① 读写分区对齐落码（Claude，未编译、未测试，待 Codex 验证）」。内容：
- 契约一句话；改动摘要。
- 顺带说明：G0 已修过的自锁代码整段删除。
- 行为变化：负责人 2026-10-13 已接受，db 积压超过 8s 时进场失败，tip 3023。
- 验证命令：指向契约文档「滚动对齐步骤与观测」和本规格 rollout。
- 文件清单。
- Java 版（AGENTS §12）：对应 Java 版本号 N/A，理由是 Java 存盘同步直写 MySQL（带 owner_epoch 条件），没有 Kafka DBTask 通道；PARITY.md 已登记。

### [modify] D:\luyuan\xuanming-server-mmo\PARITY.md
Java 仓库。先读最近几行，按现有表格列格式登记一行：「本仓库 <commit>：DBTask 读写分区对齐（login 读改用 CRC-32 mod N，与 C++ 写同分区；go/db 分区契约观测；C++ 显式 consistent_random）↔ Java 不适用（Java 存盘同步直写 MySQL，没有 Kafka DBTask 通道；不涉及客户端契约）」。

## 接口（类型 / 配置键 / 指标 / Redis 键 / 组名）
【Go：go/shared/kafkautil，新增导出】
- const DBTaskTopicPrefix = "db_task_zone_"
- var ErrEmptyDBTaskKey, ErrInvalidDBTaskPartitionCount error
- func IsDBTaskTopic(topic string) bool
- func DBTaskKey(playerID uint64) string
- func AppendDBTaskKey(dst []byte, playerID uint64) []byte
- func DBTaskPartition(key []byte, partitionCount int32) (int32, error)：n<=0 或空 key 时报错并返回 -1；否则返回 int32(crc32.ChecksumIEEE(key) % uint32(n))
- func NewDBTaskPartitioner(topic string) sarama.Partitioner：等于 sarama.NewConsistentCRCHashPartitioner，类型满足 sarama.PartitionerConstructor

【Go：login，包内】
- func newDBTaskProducerConfig(cfg config.KafkaConfig) *sarama.Config
- KeyOrderedKafkaProducer 公开方法的签名都不变，语义有变化：
  - SendTask 和 SendTasks 要求 key == kafkautil.DBTaskKey(task.Key)，不符直接返回 error，不发送。
  - SendToTopic 和 SendToTopicPartition 拒绝 IsDBTaskTopic 为真的 topic。
- NewCommandAwarePartitioner(topic)：db_task_zone_ 前缀返回 kafkautil.NewDBTaskPartitioner。
- 删除的符号：markPartitionUnavailable、markPartitionUnavailableLocked、getAvailablePartition、checkUnavailablePartitions、isPartitionUnavailableErr、getUnavailablePartitionCount、包 login/internal/logic/pkg/consistent、safego 点位 login.kafka_check_unavailable_partitions。

【Go：go/db，包内】
- func observeProducerPartition(msg *sarama.ConsumerMessage, task *db_proto.DBTask, partitionCount int32) string，返回 verdict。
- metrics 包新增：func CountProducerPartition(op, verdict string)；常量 ProducerPartitionOpRead/Write/Other（"read"/"write"/"other"）和 ProducerPartitionOK/Mismatch/KeyMismatch/Invalid（"ok"/"partition_mismatch"/"key_mismatch"/"invalid"）。
- dbOpHandler 签名不变。

【指标】
- 新增 db_task_producer_partition_total{op="read|write|other", verdict="ok|partition_mismatch|key_mismatch|invalid"}，共 12 条序列，进程启动时预置为 0。
- login 侧不新增指标。进场失败沿用 entergame_total{result="preload_failed"}、entergame_preload_seconds、entergame_failure_notify_total{stage="preload"}、dataloader_preload_callback_wait_seconds{result="failed"}、dataloader_preload_callback_result_total{result="failed"}。

【告警】DbTaskWritePartitionContractBroken（critical）、DbTaskReadPartitionMismatch（warning）。

【日志（英文）】
- go/db："DATA-ORDERING: db_task producer violated the partition contract: …"（ERROR）；"db_task read outside the partition contract (old login?): …"（INFO）。各 op 每进程只打前 20 条。
- login："batch send failed: topic=… partition=… key=… count=…"、"batch sent: …"、"send failed: task=… topic=… partition=…"、"message sent: task=… partition=… offset=…"，以及两条 refusing … db_task topic … 拒绝信息。

【C++】
- inline std::string DbTaskKafkaKey(uint64_t playerId)（player.h）。
- KafkaProducer::createProducer 显式设 partitioner=consistent_random（等于默认值）。
- 新增 gtest 套件 DbTaskPartitionContract.*。

【测试数据】go/shared/kafkautil/testdata/dbtask_partition_vectors.json，Go 与 C++ 共读。

【opt-in 测试的环境变量】DBTASK_AUDIT_DUMP、DBTASK_AUDIT_PARTITIONS（上线抽样复核用）、KAFKA_INTEGRATION_BROKER（沿用现有）。

【没有变化的】
- 不新增配置键；TopicGeneration、PartitionCnt、InitialPartition 不变。
- 不新增 Redis 键；不改 Kafka 组名；Kafka key 字节不变。
- 不改 proto，不动 generated/。
- db_task topic 不是 {type}-{id} 形式，target_instance_id 不适用。

## 顺序与重叠
一、与条件写会话（MySQL 行内围栏、SaveConditional、fence、save_seq）的关系：两项正交。
- S3 不改 handleTask、runDBOp、handleDBWriteOp，也不改 dbOpHandler 签名。
- **S3 不移动 markAppliedSeq / markAppliedOwnerEpoch 的调用顺序和位置**（key_ordered_consumer.go:1670、:1674 原样不动）。
- S3 在 KOC 里唯一的执行路径改动，是在 ConsumeClaim 解码成功后（:1791 之后、ensureWorker 之前、早于任何 handleTask）插一行只读的 observeProducerPartition。它不碰 ack、游标、重试。
- 文档分工：契约文档写明条件写不校验读，本项不覆盖跨 topic，两者互补。条件写会话的设计稿如果提到读到旧档，应链接到契约文档的「屏障覆盖范围与剩余缺口」。

二、本轮各条目的文件重叠：
- S1（④ 重试只交给分区属主 + WAITAOF）：
  - 都动 key_ordered_consumer.go：consumeOneRetryTask（:1182-1253）、wrapRetryPayload 注释（:721-727）、deferFailedTask、saveToRetryQueue。S3 在这些位置只改注释，替换文字已在 changes 给出；如果 S1 重写了函数，由 S1 把这些文字带过去。
  - 都动 metrics.go 的 register()，各自追加即可合并。
  - 都动 go/test.ps1 的 L1 列表。
  - 都动 db-task-kafka-partition-contract.md：S1 改离线扩容第 3 步里的重试列表名（:40-43）；S3 改 :9-12、:25-26、:74-75，并在文末追加新节。hunk 不同。
- S2（③ 消费组按 zone 拆）：
  - 交接闸和位点播种可能改 ConsumeClaim / Setup，与 S3 的一行插入在同一个函数。
  - 契约文档里可能加消费组的段落。
  - S2 改 kafka.yaml、kafka-topic-init.yaml、docker-compose.yml（镜像钉版本），S3 不碰这些文件。rollout 第 1 步 console consumer 的参数以 S2 钉定的 Kafka 版本为准。
- S4（G1 治标、停机顺序、测量打点、过时注释、Lua tonumber）：
  - key_ordered_consumer.go 里的 handleTask、Stop、stage 打点、luaMarkAppliedEpoch，S3 都不碰。
  - metrics.go 的 register() 里 S4 会新增大量指标，与 S3 同一个函数，按追加合并。
  - 「过时注释」与 S3 重叠：与分区函数相关的文字归 S3（见 D1），S4 只做 store_registry.go 的 REPLACE INTO 和 migrate/plan.go 的 ON DUPLICATE KEY。
- S5（scene 发 DBTask 失败后的脏快照 + db_task retention/PVC）：
  - S5 改 player_lifecycle.cpp SavePlayerToRedis 里发送失败的分支（:4026-4032 一带），S3 只改 :3989 一行，hunk 不同。
  - 工作区里该文件和 kafka_command_test.vcxproj 正被别的会话改着（git status 显示 M）。落码前先 git diff，只追加自己的那一行，不回退别人的改动。
  - S5 的 retention/PVC 改 deploy/k8s/manifests/infra/kafka*.yaml，S3 不碰。

三、合入顺序：
- 按瓶颈文档 §5.2「子 shard → ②A → ① 的观测 → ③ → ④」，建议 S3 在 KOC 上最先合入（改动只有一行插入加注释，最小），然后 S2、S1、S4 依次 rebase。metrics.go 和 go/test.ps1 都是追加式冲突，手工合并即可。
- S3 内部的依赖：shared/kafkautil 与向量文件先落；login、go/db、data_stress 依赖它；C++ 测试依赖向量文件和 player.h。

四、发布顺序（各自独立，没有硬依赖）：
1. 只读抽样门槛。
2. login 逐副本滚动。
3. go/db 观测随本轮第一个 go/db 例行发版上。
4. 写告警在 go/db 观测上线后 apply，读告警在 login 全部滚完后 apply。
5. C++ 随下次 scene/battle 例行发版。

login 与 go/db 谁先上都安全。条件写可以任意先后上线。

## 测试
### go/shared/kafkautil/dbtask_partition_test.go
- TestDBTaskPartitionCheckValue：crc32.ChecksumIEEE("123456789") 必须等于向量里的 check.crc32，也就是 0xCBF43926。
- TestDBTaskPartitionGoldenVectors：对向量的每一条，以下四种算法都必须等于 expect_partition：
  1. DBTaskPartition；
  2. 独立重算 int32(crc32.ChecksumIEEE(key)%uint32(n))；
  3. sarama.NewConsistentCRCHashPartitioner("db_task_zone_1").Partition(&ProducerMessage{Key: StringEncoder(key)}, n)；
  4. NewDBTaskPartitioner 的结果。
  另外断言用例数 ≥45，并且覆盖 N∈{7,10,16,20} 和 2147483647。
- TestNewDBTaskPartitionerMatchesDBTaskPartition：key 集合 ≥1e5 个，包括 0、MaxUint64、1..50000 顺序 id，以及用确定性循环生成的 bwmarrin 形状 id（毫秒<<22 | node<<9 | step）。N∈{1,2,3,5,7,8,10,16,20,32,64}，sarama 分区器与 DBTaskPartition 逐个相等。
- TestDBTaskKeyEncoding：0→"0"、1→"1"、1<<63-1→"9223372036854775807"、MaxUint64→"18446744073709551615"；AppendDBTaskKey(buf[:0],id) 与 DBTaskKey(id) 相等；向量里每个 key 经 ParseUint 再 DBTaskKey 能还原成原串（canonical）。
- TestDBTaskPartitionRejectsEmptyKeyAndBadCount：nil 和 []byte{} 满足 errors.Is ErrEmptyDBTaskKey；n=0 和 n=-1 满足 errors.Is ErrInvalidDBTaskPartitionCount；返回值都是 -1。
- TestIsDBTaskTopic：db_task_zone_1、db_task_zone_1_g2 为真；db_task、gate-cmd_g1、player_snapshot_topic_g1 为假。
- TestDBTaskPartitionDistribution：用 1e5 个确定性的 bwmarrin 形状 id、N=10，每个分区占比偏离 0.1 不超过 10%。注释写明：失败说明 CRC 对存量 id 有偏斜，写侧早已如此；回报负责人，不改契约。
- TestParseConsoleConsumerLine（hermetic）：输入 Go raw string `Partition:3\tOffset:42\t1001\t\x08\xE9\x07\x1A\x05write"\x0Fplayer_database`（分隔符用真实的 tab），解析得 p=3、o=42、key=1001，classifyDumpOp 得 write；把 op 部分换成 `\x1A\x04read` 得 read；缺少 Offset 字段的行必须报错。
- TestDBTaskPartitionAuditDump：opt-in，没有环境变量时 Skip；有环境变量时的判定规则见 changes。

### go/login/internal/kafka/dbtask_partitioner_test.go
- TestDBTaskProducerConfigValidates：newDBTaskProducerConfig(testKafkaConfig()).Validate() 为 nil，并且 Idempotent=true、MaxOpenRequests=1、RequiredAcks=WaitForAll、Partitioner 不为 nil。
- TestDBTaskProducerConfigUsesCRCPartitioner：对 11 个 golden key 和 N∈{7,10,16,20}，cfg.Producer.Partitioner("db_task_zone_1") 的落点等于 kafkautil.DBTaskPartition；运行时统计与 sarama.NewHashPartitioner 落点不同的 key，至少 1 个，证明分区器确实换了。
- TestSendTaskRoutesByDBTaskContract：sp := mocks.NewSyncProducer(t, newDBTaskProducerConfig(testKafkaConfig()))；sp.TopicConfig.SetDefaultPartitions(10)；用 ExpectSendMessageWithMessageCheckerFunctionAndSucceed 断言 msg.Partition == DBTaskPartition("1001",10)，且 key 等于 "1001"。SendTask(Key=1001, key "1001") 返回 nil。
- TestSendTasksRoutesByDBTaskContract：同上，两条任务对应两个 expectation，两条消息的分区都正确。
- TestSendTaskRejectsMismatchedKey：mock 不设任何 expectation；SendTask(Key=42, key "43") 和 SendTasks([Key=42], "43") 都返回 error；sp.Close() 不报未满足的 expectation。
- TestDBTaskReadAndWriteShareKafkaPartitionIntegration：没有 KAFKA_INTEGRATION_BROKER 时 Skip。
  1. 用 ClusterAdmin 建一个 10 分区的随机 topic，t.Cleanup 里删掉。
  2. 等 metadata 可见，用 10s 单调截止，每 100ms 轮询一次。
  3. W 生产者：sarama.NewConfig，Partitioner 设为 kafkautil.NewDBTaskPartitioner，模拟 C++。R 生产者：sarama.NewClient(newDBTaskProducerConfig(cfg 加 Brokers)) 再 NewSyncProducerFromClient。
  4. 对同一个 key "7311864093319258112" 先发 W、再发 R。
  5. 断言 pW==pR==DBTaskPartition，且 oR>oW。
- 回归：原有的 TestPartitionDriftIrreversiblyFencesDBTaskSends、TestPartitionsMatchContractRequiresExactContiguousSet、TestCommandPartitioner* 照旧通过。

### go/login/internal/kafka/key_ordered_producer_lock_test.go
TestSendTaskLeaderNotAvailableDoesNotDeadlock（用例名保留，内部改写）：
- mock 先 ExpectSendMessageAndFail(sarama.ErrLeaderNotAvailable)，再 ExpectSendMessageAndSucceed。
- 第一次 SendTask 必须返回 error，第二次在 5s 内返回 nil。
- 最后在 goroutine 里调 p.mu.TryLock()，必须在 5s 内成功。

### go/login/internal/kafka/command_partitioner_test.go
- TestCommandAwarePartitionerScope 改写：db_task_zone_1 和 db_task_zone_1_g2 的落点等于 DBTaskPartition（key 1001 和 7311864093319258112，N=8 和 10）；db_task、gate-5、scene-77、player_snapshot_topic_g1 照旧与 NewHashPartitioner 同分区；命令 topic 照旧拿到 commandPartitioner。
- 新增 TestSendToTopicRefusesDBTaskTopics。
- 新增 TestSendToTopicPartitionRefusesDBTaskTopics。
- 其余原有用例不变。

### go/db/internal/kafka/producer_partition_audit_test.go
- TestObserveProducerPartitionVerdicts：表驱动，op 取 read/write/other，每种 op 跑下面五种情形：
  1. ok：Key "1001"，Partition = DBTaskPartition("1001",10)；
  2. partition_mismatch：分区为 (exp+1)%10；
  3. key_mismatch：msg.Key 为 "1002"，task.Key 为 1001；
  4. key_mismatch：msg.Key 为 nil；
  5. invalid：partitionCount 为 0。
  断言返回的 verdict，以及对应序列的计数正好 +1（计数从 DefaultGatherer 读）。
- TestObserveProducerPartitionPreRegistersAllSeries：观测一次之后，db_task_producer_partition_total 正好有 12 条序列。
- TestObserveProducerPartitionNilInputs：msg 为 nil 或 task 为 nil 时返回 invalid，不 panic。
- TestAllowProducerPartitionLogBudget：先把 producerPartitionLogged 置零；对 write 连续调用 25 次，前 20 次返回 true、后 5 次返回 false；read 有独立的额度。
- 回归：原有的 key_ordered_consumer_test（TC3/TC5a/5b/5c/TC7/TC8）、subshard_route_test、placement_route_test、retry_* 用例数不变，全部通过。

### cpp/tests/kafka_command_test/kafka_partition_contract_test.cpp
- DbTaskPartitionContract.VendoredDefaultPartitionerIsConsistentRandom：rd_kafka_topic_conf_new()，再 rd_kafka_topic_conf_get("partitioner") 必须等于 "consistent_random"，最后 destroy。
- DbTaskPartitionContract.ExplicitPartitionerOnGlobalConfReachesDefaultTopicConf：rd_kafka_conf_new()；rd_kafka_conf_set(conf,"partitioner","consistent_random") 返回 RD_KAFKA_CONF_OK；rd_kafka_conf_get_default_topic_conf(conf) 不为 nullptr，再 get 得到 "consistent_random"；最后 rd_kafka_conf_destroy。这条用例与 createProducer 的设置路径一致。
- DbTaskPartitionContract.LibrdkafkaConsistentRandomMatchesSharedVectors：用 FindDbTaskVectorFile 定位文件，找不到就 ASSERT 失败，消息写明「缺了就等于没测」。YAML::LoadFile 读入后，对每条 case 断言 rd_kafka_msg_partitioner_consistent_random(nullptr, key.data(), key.size(), n, nullptr, nullptr) == expect_partition。key 非空时不会访问 rkt。断言 case 数 ≥45。
- DbTaskPartitionContract.KafkaKeyMatchesGoFormatUint：DbTaskKafkaKey(0)=="0"、DbTaskKafkaKey(1)=="1"、DbTaskKafkaKey(UINT64_MAX)=="18446744073709551615"；向量里每个 key 经 std::stoull 再 DbTaskKafkaKey 能还原成原串。
- 原有用例照旧通过：CommandPartitionVectors、CommandTopicNaming、ClassifyProduceResult、DeliveryErrorClassification、RebuildGate、FailureLogThrottle 等，以及另一会话新增的 kafka_manager_init_test。

## 上线与回滚
0）Codex 本机验证，合入前做，Go 1.27.2。所有命令用全路径，按顺序执行。

A. Go
1. Set-Location D:\luyuan\xuanming-server-mmo-cpp\go\shared
   - go vet ./kafkautil/
   - go test ./kafkautil/ -count=1 -v -run "DBTask|ParseConsoleConsumerLine|IsDBTaskTopic"
   - 通过标准：CheckValue、GoldenVectors、Matches、KeyEncoding、Rejects、Distribution、IsDBTaskTopic、ParseConsoleConsumerLine 全部 PASS；AuditDump 为 SKIP。
   - golden 不一致时：保留 -v 全文，不改 JSON，回报。
2. Set-Location D:\luyuan\xuanming-server-mmo-cpp\go\login
   - go build ./...
   - go vet ./internal/kafka/... ./internal/logic/pkg/dataloader/... ./internal/svc/...
   - go test ./internal/kafka/... ./internal/logic/pkg/dataloader/... -count=1 -v
   - 通过标准：全绿，Integration 为 SKIP；并且 Get-ChildItem D:\luyuan\xuanming-server-mmo-cpp\go -Recurse -Filter *.go | Select-String 'pkg/consistent' 没有输出。
3. Set-Location D:\luyuan\xuanming-server-mmo-cpp\go\db
   - go build ./...（包括 cmd/data_stress 和 cmd/verifier）
   - go vet ./internal/kafka/... ./internal/metrics/... ./cmd/data_stress/...
   - go test ./internal/kafka/... ./internal/metrics/... -count=1
   - 通过标准：全绿，TC3/TC5/TC7 的用例数与改动前相同。
4. pwsh -File D:\luyuan\xuanming-server-mmo-cpp\go\test.ps1
   - 通过标准：输出 L1_TESTS_OK，日志里能看到 shared（./placement/... ./kafkautil/...）和 login（./internal/kafka/...）两组都执行了。
   - shared/kafkautil 里的既有用例此前不在 L1；如果它们红了，单独回报。
5. 可选（本机 docker kafka 已起）：
   - $env:KAFKA_INTEGRATION_BROKER='127.0.0.1:9092'
   - 在 go\login 下执行 go test ./internal/kafka/ -run Integration -count=1 -v，应 PASS
   - 跑完 Remove-Item Env:KAFKA_INTEGRATION_BROKER

B. C++（串行）
0. git -C D:\luyuan\xuanming-server-mmo-cpp submodule status third_party/librdkafka，行首不能是 '-'。现在已检出；如果需要 init，属于联网下载，先征得负责人同意。
1. msbuild D:\luyuan\xuanming-server-mmo-cpp\game.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
   - infra 和 scene 都改了，所以全量串行编译；没有新增警告算通过。
2. pwsh -File D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\run_cpp_tests.ps1 -Build -Filter kafka_command_test
   - 通过标准：DbTaskPartitionContract.* 四例全部通过，原有用例不新增失败。
   - main 找不到 base_deploy_config.yaml 时只告警，属正常。

C. 本机一区联调
- 前置：按 AGENTS §6.2.6 清空本机 redis、mysql、etcd，删掉 tools/scripts/.run 下的 pid 和 log。
1. 用 start_game.ps1 起服，顺序 db → login → scene。
2. 跑 robot login-test，通过率与基线一致（22–23/23）。
3. curl.exe -s http://127.0.0.1:9160/metrics | Select-String db_task_producer_partition_total
   - 通过标准：12 条序列都在；op="write" 只有 verdict="ok" 在增长；op="read" 只有 ok 在增长；key_mismatch 和 invalid 为 0。
   - 可选对照：换回旧 login 二进制跑一轮，读的 partition_mismatch 约占 90%，只记录，不判失败。
4. I4 冷读冒烟（修订版，scene 开 STRESS_TEST_PROBE，10 个玩家）：
   1. 优雅停掉 go/db。
   2. robot 登录、改状态、下线。
   3. 等 scene 日志出现 "[SavePlayerToRedis] Player <id> saved to Redis, DB write tasks enqueued"（断线租约到期之后才会存盘），再从父键解出 W 的 test_seq。
   4. 删掉父键 <PlayerAllData 全名>:<id> 和两个子表键（键名规则见 dataloader.buildParentKey / buildCacheKey）。不要删 consumer:applied* 键。
   5. robot 再次登录（login 日志出现 "LoadSync: sending … DB read tasks"）。
   6. 2 秒内拉起 go/db。
   7. 判定：
      - 预加载超时（entergame_total{result="preload_failed"} 加 1，日志出现 task result wait timed out）判为不确定，重跑。
      - 进场成功后，解码父键里的 player_database.stress_test_probe.test_seq：等于 W 判通过，小于 W 判失败。
   - 通过标准：至少 5 次有结论，并且全部通过。
5. 把第 1 步的抽样流程对本机 kafka 容器演练一次，确认 runbook 可用。

- 失败时保留：go test -v 全文、编译错误前 20 行、kafka_command_test 的输出、bin/logs/cpp_nodes/、go/db 与 login 的日志、:9160 和 :9101 的 /metrics 快照。
- 不得据此宣称任何性能结论。

1）只读抽样门槛（负责人或 Codex 执行；生产和预发每个 zone 都做；在 login 切换之前）
- 本机用 docker exec kafka …；K8s 换成 kubectl -n <kafka 所在 ns> exec kafka-0 -- …，bootstrap 用集群内地址。以 zone 1、N=10、每分区取最近 M=500 条为例：
  - end 偏移：/opt/kafka/bin/kafka-get-offsets.sh --bootstrap-server <B> --topic db_task_zone_1 --time -1
  - begin 偏移：同一命令把 --time 换成 -2
  - 输出每行是 topic:partition:offset。对每个 p，from = max(begin, end-M)，count = end-from；count 为 0 就跳过。
- 每个分区执行一次，输出追加到临时文件 <dump>：/opt/kafka/bin/kafka-console-consumer.sh --bootstrap-server <B> --topic db_task_zone_1 --partition <p> --offset <from> --max-messages <count> --timeout-ms 15000 --property print.partition=true --property print.offset=true --property print.key=true --property print.value=true --value-deserializer org.apache.kafka.common.serialization.BytesDeserializer --consumer-property enable.auto.commit=false
  - 带 --partition 就是 assign 模式：不加入 db_rpc_consumer_group，不提交位点，也不会触发再平衡。
  - Kafka 4.1 起 --property 可能提示已弃用，改用 --formatter-property。具体以 S2 钉定的镜像版本为准。
  - kcat 的 %s 输出原始字节，会被载荷里的换行打断，不能用来判定 op。只有 kcat 可用时，只打 %p、%o、%k 做全量分区核对，不符的行逐条查看载荷。
- 离线复核：
  - $env:DBTASK_AUDIT_DUMP='<dump>'; $env:DBTASK_AUDIT_PARTITIONS='10'
  - 在 D:\luyuan\xuanming-server-mmo-cpp\go\shared 下执行 go test ./kafkautil/ -run TestDBTaskPartitionAuditDump -count=1 -v
  - 用完删掉临时文件和这两个环境变量。
- 通过标准：
  - 写的非 ok 计数为 0，解析错误为 0。
  - 读不符比例记作基线，旧 login 下约 0.9。
- 不满足时停在这一步：按输出的前 20 条不符（topic、partition、offset、key）找出不守契约的写入方。login 改动本身仍然安全，但先不要推进。

2）staging（预发）
- 按生产顺序走一遍：如果有 go/db 发版就先发 go/db，然后滚 login。
- 重复 C4 的 I4 冒烟，只在 staging 删 Redis 键。
- 可选：用 KAFKA_INTEGRATION_BROKER 指向 staging broker，跑一次集成用例。

3）生产 login 逐副本滚动（核心切换）
- 前置：这次滚动的回滚目标镜像必须 ≥ d5cbb8a8e（G0）。
- 观察指标：
  - 读不符比例单调下降到 0（两副本时大约 0.9 → 0.45 → 0），read ok 计数上升。前提是 go/db 观测已经上线；没上线就用第 1 步的抽样在滚动后复跑一次。
  - EnterGame 成功率、entergame_total{result="preload_failed"} 的速率、dataloader_preload_callback_wait_seconds、db_task_stage_seconds{op="read"} 都与基线持平。
  - 按分区看 Kafka lag，没有单个分区持续堆积。
  - login 日志里没有 "producer routing fenced"，也没有新的发送错误。
- 发布说明写明行为变化：db 积压超过约 8s 时，冷读进场会失败，客户端收到 tip 3023（以前是读到旧档后静默回档）。runbook 写明：看 preload_failed 和按分区的 lag。
- 回滚：回滚 login 镜像，读回到 FNV（即现状），没有数据需要清理，混跑任意时长都安全。

4）go/db 观测
- 不单独安排发版，随本轮第一个 go/db 例行发版一起上（S1、S2、S4 都要发 go/db）。
- 执行时遵守交接 §3.5 和瓶颈文档 §4.5 的红线：低峰；多 zone 依次滚，一次一个 Pod；先核对 MySQL 连接余量，不够就用 maxSurge=0；共享 GroupID 期间每次 Pod 进出都会让全组再平衡。
- 上线后观察：写的 verdict!="ok" 恒为 0，write ok 随在线人数增长，至少看 2 个存盘周期（≥10 分钟）。

5）告警
- go/db 观测上线之后：kubectl apply -n observability -f deploy/k8s/db-task-partition-alerts.yaml。
- 读规则 DbTaskReadPartitionMismatch 要等第 3 步全部滚完才算生效。为避免误报，可以先把这条规则注释掉，或者等第 3 步完成后再 apply。
- 回滚：kubectl delete -f 同一个文件。

6）C++ 改动随下次 scene/battle 例行发版，不单独安排窗口
- 观察：写不符恒为 0；scene 启动日志里没有 "failed to pin partitioner"；kafka_producer_stats 与发版前一致。
- 回滚：回滚 scene/battle 镜像，运行时零变化。

7）文档、PROGRESS、PARITY 随代码一起合入。

## 待决项（负责人已按推荐默认定）
- 「过时说法订正」里与分区函数相关的文字（ARCH.md:165、契约文档 :9-12/:25-26/:74-75、infra-reconnect-overview.md:26、cross-zone-readiness-audit.md:149、login-simplification-2026-04.md:68，以及 go/db、login、data_stress 的分区注释）归 S3 还是 S4？
  - 采用：全部归 S3。S4 只做 proto_sql/store_registry.go 的 REPLACE INTO 注释，以及 go/db/internal/migrate/plan.go 的 ON DUPLICATE KEY 三处。
- C++ 批次（kafka_producer.cpp 显式 partitioner、player.h DbTaskKafkaKey、golden 测试、vcxproj 一行、player_lifecycle.cpp:3989 一行）是否本轮落码？player_lifecycle.cpp 那一行归谁改？
  - 采用：本轮落码，随下次 scene/battle 例行发版上线。player_lifecycle.cpp:3989 那一行由 S3 改：落码前先 git diff，确认与 S5 以及正在改这个文件的会话不在同一个 hunk；如果冲突，就交给 S5 顺手带上这一行（字节不变）。

## 风险
1. 前提风险：生产 C++ 如果在某处偏离了契约（构建改了配置、误跑旧版 data_stress），写的分区就和契约对不上。rollout 第 1 步的抽样和 critical 告警会拦下来。login 改动本身在任何情况下都安全，只是收益会落空。

2. 将来动写侧分区函数是灾难性操作：librdkafka 升级改了默认值、有人加了 partitioner 配置、Java 化之后想换 murmur2，都会让同一玩家的写在新旧 scene 之间换分区，被判 orderingIncomparable 送进死信。缓解：C++ 显式钉值；两侧 golden 测试共读一份向量；写不符设 critical 告警；文档写明「换函数 = 换代」。

3. 有意的 fail-closed 行为变化：db 积压让冷读排队超过约 8–9s 时进场失败（tip 3023），player_locker 在等待期间一直持有。要写进发布说明和 runbook，并观察 preload_failed。

4. 屏障只覆盖一部分，五个缺口依然存在：访客跨 topic、写已转进重试、非属主执行重试、子表缓存命中时不发读、再平衡排空超时（G1）。文档必须如实写，不能写成「读后写一致」。

5. 去掉了「所有分区都被标不可用时立即报错」的快速失败。Kafka 全挂时，每次发送都要等 sarama 跑完 Retry.Max 重试和网络超时，而且发送在 p.mu 下串行，预加载池更容易打满（entergame_total{result="pool_saturated"} → 客户端收到 kLoginInProgress）。旧代码对每个分区的第一次失败本来也是这样，影响限于 Kafka 整体不可用的时段。

6. 回滚：回滚目标镜像如果早于 d5cbb8a8e，会把 G0 的自锁带回来。

7. 两侧对空 key 的处理不一致（librdkafka 随机分区，sarama 落 0 号分区）。两侧生产者都拒绝空 key，契约函数也对空 key 报错。

8. 版本漂移：已比对 sarama 1.43.1 与 1.46.2 的 partitioner.go，逐字节相同；以后升级 sarama 由 golden 测试兜底。

9. golden 值的证据强度：附录里是手写 PowerShell CRC 算的；本规格只用公开 CRC 值手工核对了 3 行，另做了整表的交叉自洽检查；必须等 Codex 用 Go、sarama、librdkafka 三方机械复核。代码全部未编译、未测试。

10. 负载分布：读从 FNV 分布改成 CRC 分布，写本来就在 CRC 分布上。如果有偏斜，写侧早已存在，由分布测试和按分区的 lag 观察兜底。

11. L1 新加了整个 shared/kafkautil 包，其中的既有用例是第一次进 L1，可能暴露旧问题，要单独归因。

12. 并行会话：player_lifecycle.cpp 和 kafka_command_test.vcxproj 正被别的会话修改；key_ordered_consumer.go 和 metrics.go 与 S1、S2、S4 是同一批文件。落码时只动自己的 hunk，不回退别人的改动。

13. 抽样 runbook 依赖 Kafka 自带工具的参数，以及 BytesDeserializer 的 \xNN 转义格式。如果格式与预期不符，AuditDump 用例会以解析失败或空抽样报错，属于 fail-closed，不会误判放行。参数以 S2 钉定的 Kafka 版本为准。

14. observer 的开销：每条消息一次约 20 字节的 CRC 加一次计数器自增，可以忽略。计数按投递算，重投递会重复计入，比例类指标不受影响。

15. 历史文档 architecture-current-state-vs-gaps-2026-05.md:24 仍指向 pkg/consistent。它是历史快照，不改。

16. 静态核对的范围：KafkaProducer 不读任何外部 librdkafka 配置，所以源码核对就覆盖了全部 C++ 发送路径。以后如果加了从 yaml 透传 librdkafka 参数的能力，必须同时禁止覆盖 partitioner。
