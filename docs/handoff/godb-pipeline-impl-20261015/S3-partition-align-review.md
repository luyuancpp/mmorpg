# S3 对抗复核意见（结论：needs_fix）

## [medium] 上线抽样门槛（TestDBTaskPartitionAuditDump）可能不检查任何写就判通过。门槛只看三件事：写的非 ok 计数、解析失败、有效行数为 0。被分类成 other 的行不设门槛，抽样里一条写都没识别出来也照样放行。key 判定只检查是不是规范十进制，没有核对 Kafka key 等于 DBTask.key，和 go/db observer 的 key_mismatch 口径不一致。op 用整行子串匹配，body 里碰巧带的字节可能导致误判。
- 证据：规格 changes 中 dbtask_partition_test.go：classifyDumpOp 识别不出来就返回 "other"；只有在「写的非 ok 计数 >0 / 有解析失败的行 / 有效行数为 0」时才 t.Fatalf。如果 Kafka 4.x 格式化输出与预期不同，或 BytesDeserializer 转义有偏差，所有行都会落进 other，门槛照过。rollout 第 1 步正是拿这个结果当作「生产 C++ 实际用 CRC32」的实证门槛（附录 L264 第 3 步 (a)）。DBTask.op=3、key=1，见 proto/db/db_task.proto:11-13。
- 修法：1. 写行数为 0，或 other 行数大于 0，都直接 Fatal。
2. 先把 \xNN 反转义回原始字节，再用 proto.Unmarshal 解成 DBTask，从解码结果取 op 和 key。go/shared 已依赖本地 proto 模块，见 go/shared/go.mod:88-93。不再用子串匹配。
3. key 判定改为 kafkaKey == kafkautil.DBTaskKey(task.Key)，与 go/db observer 用同一口径。
4. 解析前先剥掉 UTF-8 BOM 和行尾的 \r。

## [medium] 复核修正 5 要求「删键前确认 W 真的发出了」，规格拿来作证据的 scene 日志证明不了这一点。
- 证据：player_lifecycle.cpp 当前工作区 :4069-4082：send 失败只打 LOG_ERROR "Kafka send failed"，lambda 返回后无条件打 LOG_INFO "[SavePlayerToRedis] Player … saved to Redis, DB write tasks enqueued"。而且 send 返回 ERR_NO_ERROR 只表示消息进了 librdkafka 本地队列，不等于已经写到 broker。
- 修法：冒烟流程第 3 步改为在 Kafka 侧确认：
1. 登出前记下该玩家所在分区 p = DBTaskPartition(key, N) 的 end offset（kafka-get-offsets --time -1）。
2. 登出、看到存盘日志后，确认这个 end offset 增长了。
3. 再用 console-consumer --partition p --offset <旧 end> 抽到 key=<id>、op=write 的那条，可复用 AuditDump 的解析器。
4. 同时确认该玩家没有 "Kafka send failed" 日志。
以上都满足才允许删键。

## [medium] 行号锚点已经过时。player_lifecycle.cpp 被并行会话改过，规格引用的位置整体下移了约 43 行。照行号改会改错位置：可能覆盖掉一行注释，同时留下原来的声明，结果是重复定义编译失败。ARCH.md 的行号也错了。
- 证据：规格写的是 :3989 playerIdStr、:3993 topic、:4010 op、:4026 send。当前工作区分别在 :4032、:4036、:4053、:4069；现在的 :3989 是注释「// ── 归属(CZ-2 / CZ-3)…」（git diff 显示该文件 +53/-10）。ARCH.md 里真正过时的是 :167「2. Kafka `KeyOrderedKafkaProducer`(playerId 一致性哈希到同一分区,FIFO)」，:165 是「两层保证同玩家串行:」。
- 修法：所有改动点都按内容定位，不按行号：
- player_lifecycle.cpp：SavePlayerToRedisImpl 里唯一一处 `const std::string playerIdStr = std::to_string(playerId);`。
- ARCH.md：上面引用的那一行原文。
- 与 S5 是否冲突，按 git diff 的 hunk 内容判断，不按行号。
- 规格里其余 C++ 行号统一注明「以落码时 grep 为准」。

## [medium] 「屏障覆盖范围与剩余缺口」不完整，触发条件的订正也不够精确：
(a) 附录风险清单里的「读结果回写子表缓存」放大效应漏掉了。
(b) 缺口 4 没写明子表部分命中时也会出问题。
(c) AOF 部分丢失时，父键是回退到旧值，不是消失。
(d) 没写明屏障的前提：读发出时，本玩家的写已经进了 Kafka。
- 证据：(a) key_ordered_consumer.go:370-375 publishDBReadResult 会把读结果 SET 进 {MsgType}:{id}，TTL 是 cacheTTL，默认 24h（:59-65）。缺口 1/2/3/5 里读到的旧值因此会写进子表缓存，之后又经缺口 4 在后续冷登录里被直接复用。附录 L222 风险「读结果回写子表缓存时的跨 zone 竞态」在规格的五个缺口里没有出现。
(b) ensure_player_all_data_async.go:79-90 逐张子表判断：命中的用缓存，缺的才发读，拼出来的是新旧混合档。
(c) redis.yaml:95-97 与 docker-compose.yml:235 是 appendonly（everysec）。部分丢失时父键回到上一次 SET 的值，EXISTS 快路径（:52-65）命中，不会发读，屏障用不上。
(d) 旧会话还在断线租约内、尚未存盘，或者写还滞留在 librdkafka 本地队列时，读会排在写之前。
- 修法：在契约文档那一节和 processTaskBatch 的注释里补四点：
(a) 读结果回写会把一次旧读的影响延续到缓存 TTL 到期，属于缺口放大。
(b) 缺口 4 包含部分命中，结果是新旧混合档。
(c) 触发条件分两类：全量丢失、FLUSH、删键会走冷读，屏障有效；AOF 部分回退时父键是旧值、走快路径，屏障无关，这属于 Redis 持久化的范畴。
(d) 把「写先于读进入 Kafka」明确写成屏障的前提。

## [low] 同一文件里还有注释会过时，规格却写了「plainCfg 不动」。
- 证据：key_ordered_producer.go:124-127 写的是「上面那个 config(db_task 的事务/幂等生产者)一个字都没动,行为不变」和「非命令 topic 在 NewCommandAwarePartitioner 里仍然拿默认哈希分区器」。规格 changes (4) 写「:116-135 的 plainCfg 不动」。
- 修法：同一批改掉这段注释，写成：db_task 主生产者已装契约分区器；plainCfg 对 db_task_zone_ 前缀的 topic 也装契约分区器，SendToTopic 和 SendToTopicPartition 都拒绝这类 topic；其余非命令 topic 仍用默认哈希。

## [low] 规格对行为变化的描述不完整。另外回滚前置条件写成「镜像 ≥ d5cbb8a8e」，这个说法不精确。
- 证据：规格 changes 末尾称「除分区之外，唯一的变化是去掉了快速失败」。但 (6)(7)(8)(9) 还新增了：SendTask/SendTasks 的 key 校验（不符直接 fail-closed）；SendToTopicPartition 拒绝 db_task topic；删掉了 safego 点位 login.kafka_check_unavailable_partitions，以及 stats 日志里的 unavailablePartitions 字段。
- 修法：交付说明、PROGRESS 和 risks 里把这些行为变化列全。回滚前置改为「回滚目标镜像的 commit 必须包含 d5cbb8a8e（用 git merge-base --is-ancestor d5cbb8a8e <commit> 核验）」。

## [low] 集成用例 W 生产者的配置不全，接上 broker 第一次跑就会失败。
- 证据：规格 tests 第 2 项写「W 生产者：sarama.NewConfig，Partitioner 设为 kafkautil.NewDBTaskPartitioner」。sarama.NewConfig 默认 Producer.Return.Successes=false，NewSyncProducer 会直接返回 ConfigurationError（"Producer.Return.Successes must be true to be used in a SyncProducer"）。
- 修法：W 生产者补上 Return.Successes=true 和 RequiredAcks=WaitForAll（Version 与 R 生产者一致）。随机 topic 名不要用 db_task_zone_ 前缀，参照 topic_init_integration_test.go:18 用 mmorpg_*_it_<nanos>，避免指向 staging broker 时被抽样或告警误纳入。

## [low] TestDBTaskPartitionDistribution 作为硬断言进了 L1。按规格自己的说法，这条一旦失败是修不了的，L1 会长期红。
- 证据：规格写「失败说明 CRC 对存量 id 有偏斜，写侧早已如此；回报负责人，不改契约」。go/test.ps1:59-61 规定「Add to this list ONLY when the new package is hermetic … and consistently green」。附录 L235 原本把它标为「信息性」。
- 修法：改成 t.Logf 输出分布表，或者把阈值放宽到只拦明显异常（例如偏离超过 30%），并在注释里写明这条是信息性检查。

## [low] 文件重叠清单漏了 S1 也会改 ConsumeClaim。observer 取分区数 N 的前提也没有写成不变量。
- 证据：附录 ④ L40 写「key_ordered_consumer.go:1692-1808：ConsumeClaim 里 acquire 并 defer release lease」；L54 计划让构造函数接收 topic 和 partitionCount。规格 ordering_and_overlap 只把 ConsumeClaim 列在 S2 名下。observer 用的是 h.consumer.partitionCount（key_ordered_consumer.go:1727），这依赖「一个 handler 只消费一个 topic」。
- 修法：重叠清单在 S1 下补上 ConsumeClaim（入口处加 lease，与 S3 :1791 后插入的那一行不在同一个 hunk）。在 observeProducerPartition 的注释和契约文档里写明：N 必须是 msg.Topic 的分区数；以后如果一个 handler 消费两代 topic，要按 msg.Topic 取 N，否则写会被误判 partition_mismatch 并触发 critical 告警。

## [low] R3 对 "db_task" 条目的处理与复核原文不同。复核要求它也改为断言 CRC 契约分区，规格保留为默认哈希。理由站得住，但应作为明确的偏离交负责人确认。另外 topic 前缀成了第三份命名规则，没有测试把它和真实的命名函数绑在一起。
- 证据：附录 L307 修正 3 写「"db_task_zone_1_g2"/"db_task" 不要删，改成断言它们落在 CRC 契约分区上」，规格 R3 让 "db_task" 继续断言默认哈希。topic 命名规则现在已有两份：go/db/internal/config/config.go:383 和 go/login/internal/config/config.go:529，都是 fmt.Sprintf("db_task_zone_%d")，kafkautil.DBTaskTopicPrefix 是第三份。
- 修法：decisions_needed 增加一条「"db_task" 偏离复核原文」，等负责人确认。command_partitioner_test 增加断言：用 config.DbTaskTopicForGeneration(1, 0/1/2) 生成的 topic 名，kafkautil.IsDBTaskTopic 必须为真，NewCommandAwarePartitioner 必须返回契约分区器。

## [low] 生产环境操作的执行人与 AGENTS §9 冲突。抽样 runbook 也没写输出编码，在 Windows PS 5.1 上会导致解析全部失败。
- 证据：rollout 第 1 步写「负责人或 Codex 执行；生产和预发每个 zone 都做」，第 5 步生产 kubectl apply 告警没写由谁执行。AGENTS §9 写「不要 kubectl apply 到生产…只可操作本地或用户明确指定的 dev 环境」。PS 5.1 的 `>` 默认写 UTF-16LE，pwsh 写 UTF-8（有无 BOM 取决于设置）；console consumer 的 stderr 摘要如果混进 dump 文件，也会造成解析失败。
- 修法：写明生产和预发的抽样、生产 apply 只由负责人执行，Codex 只在本机或负责人明确指定的 dev 环境执行。runbook 写明：只捕获 stdout；用 kubectl exec … -- sh -c '… > /tmp/dump' 在容器内落盘后 kubectl cp 拷出，或者用 Out-File -Encoding utf8NoBOM（pwsh 7）写文件。

## [low] C++ golden 用例名叫 Vendored…，但实际测的是运行时加载的 rdkafka.dll，不一定是 vendored 源码。
- 证据：kafka_command_test.vcxproj:138 链接的是 lib/rdkafka.lib，旁边有 rdkafka.exp，说明它是 bin/rdkafka.dll 的导入库。头文件取自 third_party/librdkafka（2.14.0，rdkafka.h:170）。infra.vcxproj:33-38 的注释提到另有单独构建的 DLL。头文件版本与运行时 DLL 版本不一致时，这个测试证明的是 DLL。
- 修法：在 DbTaskPartitionContract 里加一条断言 rd_kafka_version() == RD_KAFKA_VERSION，至少用 RecordProperty 记下 rd_kafka_version_str()，把「运行时库就是 vendored 版本」变成被测条件。另外，45 条 golden 值已经由复核者用独立的 CRC-32/IEEE 实现逐条算过，全部吻合，含 123456789 → 3421780262 和 n=2147483647 → 1274296615；这只是旁证，仍以 Codex 的三方机械复核为准。

## 漏掉 / 似是而非的阻断项
- 附录 ① 没有「复核阻断项」（原文是「没有发现阻断问题」）。以下是规格声称已处理、但处理得似是而非的复核修正：修正 5 第二条「删键前确认 W 真的发出了」——规格拿来作证据的 scene 日志 "saved to Redis, DB write tasks enqueued" 在 Kafka 发送失败时也会打印（player_lifecycle.cpp 当前 :4069-4082），而且 send 成功只表示进了 librdkafka 本地队列，证明不了 W 已经到 broker
