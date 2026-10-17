# 落码阶段报告（2026-10-15，实现代理自述，未经编译与测试）

> 每个阶段由一个实现代理完成，以下是它们交回的结构化报告原文。「留给第 3 批」是该阶段按纪律没改、交给后续阶段的要点。A3（S4 的 go/db 部分）因额度中断没有报告，见交接文档 §2。

## A1：A1 —— S3 ① 读写分区对齐的 Go 部分（状态 done）

### 文件
- `go/shared/kafkautil/dbtask_partition.go`：新增。这是 db_task 分区契约在 Go 侧唯一的实现，导出 DBTaskTopicPrefix、IsDBTaskTopic、DBTaskKey、AppendDBTaskKey、DBTaskPartition（CRC-32/IEEE 按 uint32 无符号取模；n<=0 或 key 为空时报错并返回 -1）、NewDBTaskPartitioner（就是 sarama.NewConsistentCRCHashPartitioner），以及两个错误变量。另有编译期断言，确认 NewDBTaskPartitioner 满足 PartitionerConstructor。
- `go/shared/kafkautil/testdata/dbtask_partition_vectors.json`：新增。Go 与 C++ 共读的 golden 向量，结构是 schema、check、cases。45 条 case 是规格给出的手算值，没有编造，也没有由我运行代码计算；schema 里写明了待 Codex 三方机械复核。
- `go/shared/kafkautil/dbtask_partition_test.go`：新增。测试用例：CheckValue、GoldenVectors（四种算法逐条比对）、1e5 样本下分区器与契约函数一致、KeyEncoding、拒绝空 key 和非正分区数、IsDBTaskTopic、信息性的 Distribution、hermetic 的 ParseConsoleConsumerLine，以及 opt-in 的 AuditDump。抽样复核器按复核意见改成先反转义再 proto.Unmarshal 成 DBTask。
- `go/login/internal/kafka/key_ordered_producer.go`：新增 newDBTaskProducerConfig，在 NewClient 之前装上 kafkautil.NewDBTaskPartitioner。删掉了一致性哈希环和整套不可用分区代码（6 个函数、3 个字段、safego 点位 login.kafka_check_unavailable_partitions）。SendTask/SendTasks 新增 key==DBTaskKey(task.Key) 校验，不再填 Partition，日志改为打印 sarama 回填的分区和 offset。SendToTopic/SendToTopicPartition 拒绝 db_task topic。plainCfg 和类型注释都已订正。
- `go/login/internal/kafka/key_ordered_producer_lock_test.go`：文件名和用例名 TestSendTaskLeaderNotAvailableDoesNotDeadlock 都保留。内部去掉了对 consistent 包和被删符号的依赖，末尾改为在 5s 上限内轮询 TryLock，确认 p.mu 已释放。
- `go/login/internal/kafka/command_partitioner.go`：NewCommandAwarePartitioner 对 db_task_zone_ 前缀的 topic 返回 kafkautil.NewDBTaskPartitioner，作用域注释已订正。
- `go/login/internal/kafka/command_partitioner_test.go`：Scope 用例拆成两组：db_task 组断言落在契约分区；其余 topic（含 "db_task"）仍断言走 FNV。新增 TestDBTaskTopicNamingMatchesContractPrefix，用 config.DbTaskTopicForGeneration 把命名规则与前缀绑在一起；另新增 TestSendToTopicRefusesDBTaskTopics 和 TestSendToTopicPartitionRefusesDBTaskTopics。
- `go/login/internal/kafka/expand_monitor.go`：只改类型注释：分区一旦漂移只 fence，不做任何重映射。
- `go/login/internal/kafka/dbtask_partitioner_test.go`：新增。用例：ProducerConfigValidates、UsesCRCPartitioner、SendTask 与 SendTasks 按契约路由（mocks）、key 不符时拒发、按 broker 门控的读写同分区集成用例。集成用例的 W 生产者已补全配置，topic 名用 mmorpg_dbtask_partition_it_<nanos>。
- `go/login/internal/logic/pkg/consistent/consistent.go`：连同目录一起删除。删除前全仓 grep 过，只有 key_ordered_producer.go 和 lock_test.go 引用，两处都已改掉；历史文档里的提及不改。
- `go/db/internal/metrics/metrics.go`：以追加方式加入 db_task_producer_partition_total{op,verdict}，在 registerOnce 里单独 MustRegister，并把 12 条序列预置为 0。另新增 ProducerPartition* 常量、两个切片和 CountProducerPartition。
- `go/db/internal/kafka/producer_partition_audit.go`：新增。只做观测：observeProducerPartition、producerPartitionOp、classifyProducerPartition、每进程每种 op 限 20 条的日志额度、不符日志。注释写明不拒绝消息、不改 ack/游标/重试，并写明 N 必须取 msg.Topic 的分区数这一不变量。
- `go/db/internal/kafka/producer_partition_audit_test.go`：新增。用例：3 种 op × 5 种情形的 verdict 加计数 +1、正好 12 条序列、nil 入参返回 invalid、日志额度各 op 独立。计数从 DefaultGatherer 读，不依赖 testutil。
- `go/db/internal/kafka/key_ordered_consumer.go`：ConsumeClaim 里只在解码成功之后、ensureWorker 之前插了一行 observeProducerPartition（另带注释）。其余只订正注释：wrapRetryPayload、writeBackDBCache、consumeOneRetryTask 两处、worker.start 的路由循环，以及 processTaskBatch 文档注释，后者补上了屏障前提、五个缺口、放大效应和触发条件。
- `go/db/cmd/data_stress/main.go`：改用 kafkautil.DBTaskKey 和 kafkautil.NewDBTaskPartitioner，删掉 strconv import，头注释改为分区契约的说明。
- `go/test.ps1`：L1 的 shared 组加 ./kafkautil/...，新增 login 组 ./internal/kafka/...，.DESCRIPTION 补了这两组覆盖什么的中文说明。保持 CRLF。

### 偏离规格
- 抽样复核器按复核意见重做，与规格不同：parseConsoleConsumerLine 现在返回 dumpRecord（partition、offset、key、解码后的 *DBTask），不再返回 value 字符串。新增 unescapeKafkaBytes：只接受可打印 ASCII 和 \xNN，其余一律报错。classifyDumpOp 改成接收 *DBTask。新增 auditDumpVerdict，口径与 go/db observer 相同。
- AuditDump 的失败条件在规格基础上加了两条：写行数为 0 时 Fatal，op 为 other 的行数大于 0 时 Fatal。另外文件开头是 UTF-16 BOM 时直接 Fatal 并提示改用 utf8NoBOM 抓取。
- TestDBTaskPartitionDistribution 改为信息性检查（复核意见）：t.Logf 打出分布表，只有偏离超过 30% 才失败，不再是规格写的 10% 硬断言。
- lock_test 末尾的 TryLock 改为在 5s 截止前反复轮询，规格写的是调一次；这样做是为了不误报，探测 goroutine 也不会泄漏。
- 集成用例的 topic 名用 mmorpg_dbtask_partition_it_<nanos>，副本数取 kafkautil.DefaultReplicationFactor()，W 生产者补上 Return.Successes、WaitForAll 和 V3_6_0_0（复核意见）。
- 新增 TestDBTaskTopicNamingMatchesContractPrefix（复核意见），规格里没有这条。
- metrics.go 新指标的注册写成 registerOnce 里一条单独的 prometheus.MustRegister，没有塞进原有的参数列表。语义不变，与 S1、S4 的追加更容易合并。
- key_ordered_producer.go 的 plainCfg 注释也改了。规格写的是 :116-135 不动，复核指出这段注释已过时，所以按复核意见修改。
- processTaskBatch 文档注释在规格的五个缺口之外，补上了复核要求的四点：(a) 读结果回写子表缓存造成的放大；(b) 缺口 4 包含部分命中，结果是新旧混合档；(c) 触发条件分全量丢失和 AOF 部分回退两类；(d) 屏障前提是写先于读进入 Kafka。
- go/test.ps1 新加的说明用中文写，原文件是英文，遵循 AGENTS §2。
- data_stress/main.go 的头部文档注释在改动前就不是 gofmt 格式（HEAD 版本 gofmt -l 也报），属于既有问题。没有执行 gofmt -w，免得夹带任务外格式化；gofmt -l 仍会列出这个文件。
- safego 那段注释里的「三条后台循环」改成「两条」，因为删掉了一个点位。

### 复核 / 修订意见的处理
- [fixed] [medium] AuditDump 门槛可能一条写都没检查就判通过：other 不设门槛、key 判定口径与 observer 不一致、op 靠子串匹配 —— 已改为先把 \xNN 反转义回原始字节，再 proto.Unmarshal 成 DBTask，op 和 key 都取自解码结果。key 判定改为 kafkaKey==DBTaskKey(task.Key)。写行数为 0、other 行数大于 0 都会 Fatal。解析前剥掉 BOM 和行尾 \r，遇到 UTF-16 文件直接 Fatal。go/db 确认只有 read 和 write 两种 op，所以「other>0 判失败」不会误伤。
- [not_in_scope] [medium] 冒烟流程删键前要确认 W 真的到了 broker，scene 日志证明不了 —— 这属于 rollout C4 runbook 和契约文档，是第 3 批文档的范围，本阶段不改 docs。本阶段的 AuditDump 解析器可以直接复用来按 --partition p --offset <旧 end> 核对 W。已写进 open_items。
- [fixed] [medium] 行号锚点已过时（player_lifecycle.cpp、ARCH.md） —— 本阶段负责的文件全部按函数名和原文字符串定位后再改（key_ordered_consumer.go 的实际行号比规格整体下移了约 9 行）。player_lifecycle.cpp 和 ARCH.md 属于 C++ 线和文档批次，不在本阶段。
- [fixed] [medium] 屏障覆盖范围不完整：读结果回写缓存的放大、部分命中、AOF 部分回退、写先于读进 Kafka 的前提 —— 代码侧已在 processTaskBatch 文档注释里补齐 (a)–(d)。契约文档「屏障覆盖范围与剩余缺口」一节属于第 3 批，已写进 open_items。
- [fixed] [low] key_ordered_producer.go 的 plainCfg 注释会过时 —— 已改写：主生产者装契约分区器，plainCfg 对 db_task_zone_ 前缀也装契约分区器，SendToTopic 和 SendToTopicPartition 都拒绝这类 topic，其余非命令 topic 仍用默认哈希。
- [not_in_scope] [low] 行为变化列得不全；回滚前置条件写成「镜像 ≥ d5cbb8a8e」不精确 —— 交付说明、PROGRESS 和 risks 由主会话或文档批次写。完整清单已放进 open_items：key 校验 fail-closed、SendToTopicPartition 拒绝 db_task、删掉 safego 点位、stats 日志去掉 unavailablePartitions 字段、去掉全部分区不可用时的快速失败。回滚前置应写成用 git merge-base --is-ancestor d5cbb8a8e <commit> 核验。
- [fixed] [low] 集成用例 W 生产者配置不全；topic 名不要用 db_task_zone_ 前缀 —— W 已补上 Return.Successes=true、RequiredAcks=WaitForAll、Version=V3_6_0_0。topic 名改为 mmorpg_dbtask_partition_it_<nanos>。metadata 等待用 10s 单调截止、每 100ms 轮询，t.Cleanup 里删 topic。
- [fixed] [low] TestDBTaskPartitionDistribution 作为硬断言进 L1 会长期红 —— 改为信息性检查：t.Logf 输出分布表，只拦偏离超过 30% 的明显异常，注释写明失败时回报负责人，不改契约。
- [fixed] [low] 重叠清单漏了 S1 也改 ConsumeClaim；observer 取 N 的前提没写成不变量 —— observeProducerPartition 的注释写明：N 必须是 msg.Topic 的分区数；以后一个 handler 消费两代 topic 时要按 msg.Topic 取 N，否则写会误判并触发 critical。S1 那一行插入与 lease 入口不在同一个 hunk。重叠清单本身是文档，交主会话。
- [fixed] [low] "db_task" 条目的处理与复核原文不同；前缀成了第三份命名规则 —— 新增 TestDBTaskTopicNamingMatchesContractPrefix，用 config.DbTaskTopicForGeneration(1, 0/1/2) 生成真实 topic 名，断言 IsDBTaskTopic 为真、落点等于契约分区。"db_task" 按规格仍断言走默认哈希，理由写在测试注释里；这条偏离需要负责人确认，已写进 open_items。
- [not_in_scope] [low] 生产抽样和 apply 的执行人与 AGENTS §9 冲突；runbook 没写输出编码 —— 执行人和 runbook 属于文档批次。本阶段在 AuditDump 的注释里写明只捕获 stdout、用 pwsh 7 Out-File -Encoding utf8NoBOM 或在容器内落盘后 kubectl cp；UTF-16 文件和 stderr 摘要混入 dump 都会 fail-closed 报错。
- [not_in_scope] [low] C++ golden 用例名叫 Vendored，实际测的是运行时 DLL，应断言 rd_kafka_version() —— 属于 C++ 线，本阶段不碰 C++。

### markApplied 说明
没有变化。go/db 里 markAppliedSeq 和 markAppliedOwnerEpoch 的调用顺序与位置一行没动，handleTask 和 runDBOp 也没动。key_ordered_consumer.go 里唯一的执行路径改动，是在 ConsumeClaim 解码成功之后、ensureWorker 之前插的一行只读 observeProducerPartition。它不碰 ack、游标、重试，也不访问 Redis 或 MySQL。

### 遗留事项
- 工作区状态：工作期间有别的执行者做了两次提交（a7c510d65、592d7e718），把本阶段的多数文件按当时的工作区收了进去（kafkautil 三个新文件、key_ordered_producer.go、lock_test、command_partitioner{,_test}.go、expand_monitor.go）。核对过，这些文件的工作区内容与 HEAD 一致，就是本阶段的最终版本。仍未提交的有：consistent.go 的删除（工作区显示 D，未暂存）、key_ordered_consumer.go、metrics.go、data_stress/main.go、go/test.ps1，以及 untracked 的 dbtask_partitioner_test.go 和 producer_partition_audit{,_test}.go。我没有 commit。
- golden 向量的 45 条值取自规格（附录作者与复核者各用独立手写 CRC 算出），我没有运行代码去算。我只用心算核对了 "0"、"1"、"123456789" 三行的取模，以及 2147483647 那一条三种取模写法的取值。最终以 Codex 的 Go crc32、sarama、librdkafka 三方机械复核为准，不一致时回报，不改表。
- 第 3 批文档要落的内容：契约文档里写明 AuditDump 的新门槛（写行数为 0、other 大于 0、解析失败都判失败，所以每个分区的抽样量要足够覆盖写）；C4 冒烟改为在 Kafka 侧确认 W 已到 broker（复核 medium 2）；「屏障覆盖范围与剩余缺口」一节补 (a)–(d)；执行人限定为负责人（AGENTS §9）；抽样只捕获 stdout、用 utf8NoBOM。
- 交付说明、PROGRESS、risks 里要列全的行为变化（复核 low）：(1) SendTask/SendTasks 在 key!=DBTaskKey(task.Key) 时直接拒发；(2) SendToTopic 和 SendToTopicPartition 都拒绝 db_task_zone_* topic；(3) 删掉 safego 点位 login.kafka_check_unavailable_partitions；(4) stats 日志去掉 unavailablePartitions 字段；(5) 去掉「所有分区都被标不可用时立即报错」的快速失败；(6) 读改走 CRC 分区，db 积压超过约 8s 时冷读进场失败，客户端收到 tip 3023。回滚前置写成「回滚目标 commit 必须包含 d5cbb8a8e（git merge-base --is-ancestor d5cbb8a8e <commit>）」。
- 请负责人确认一处偏离：command_partitioner_test 里 "db_task" 这个名字仍断言走默认 FNV，没有按复核原文改成断言 CRC。理由是全仓不存在叫 db_task 的 topic，放宽前缀会误伤无关 topic。
- 不在本阶段：deploy/k8s/db-task-partition-alerts.yaml、C++ 批次（kafka_producer.cpp、player.h、player_lifecycle.cpp、kafka_partition_contract_test.cpp、vcxproj）、全部 docs、PROGRESS、Java PARITY。C++ golden 测试要用 yaml-cpp 读本阶段的 JSON，其 schema 字段含中文（UTF-8 无 BOM），yaml-cpp 应能正常解析。
- command_partitioner.go 文档注释里「两边的规则必须逐字相同 —— 命令 topic 走显式分区，其它 topic 保持原行为」一句没改。db_task 在 plainProducer 上现在走契约分区器，严格说这一句已不完全准确，但 plainProducer 本来就拒发 db_task。
- data_stress/main.go 的文件头注释在改动前就不是 gofmt 格式，如果要 gofmt 干净，需要另立一个格式化改动。
- L1 新纳入的 shared/kafkautil 包里，既有用例（topic_init_test、group_lag_test）是第一次进 L1；如果它们红了，要单独归因。另外 go/shared/kafkautil/group_lag.go 有别的会话的在途改动。

### 验证命令（交 Codex / 负责人执行）
- 前置说明：全部 Go 代码未编译、未测试，以下命令按顺序执行，Go 1.27.2。失败时保留 go test -v 全文和编译错误的前 20 行，不改 JSON，不跳用例。
- Set-Location D:\luyuan\xuanming-server-mmo-cpp\go\shared; go vet ./kafkautil/; go test ./kafkautil/ -count=1 -v -run "DBTask|ParseConsoleConsumerLine|IsDBTaskTopic" —— 通过标准：TestDBTaskPartitionCheckValue、TestDBTaskPartitionGoldenVectors、TestNewDBTaskPartitionerMatchesDBTaskPartition、TestDBTaskKeyEncoding、TestDBTaskPartitionRejectsEmptyKeyAndBadCount、TestDBTaskPartitionDistribution（会打印分布表）、TestIsDBTaskTopic、TestParseConsoleConsumerLine 全部 PASS，TestDBTaskPartitionAuditDump 为 SKIP。GoldenVectors 不一致时保留 -v 全文回报，不改表。
- Set-Location D:\luyuan\xuanming-server-mmo-cpp\go\shared; go test ./kafkautil/... -count=1 —— 通过标准：ok。既有的 topic_init_test 和 group_lag_test 是第一次进 L1，如果红了单独回报。
- （可选，第四方独立复核 golden 表，前提是本机有 Python）python -I -c "import json,zlib; v=json.load(open(r'D:\luyuan\xuanming-server-mmo-cpp\go\shared\kafkautil\testdata\dbtask_partition_vectors.json',encoding='utf-8')); bad=[c for c in v['cases'] if (zlib.crc32(c['key'].encode())&0xffffffff)%c['n']!=c['expect_partition']]; print('check', zlib.crc32(b'123456789')==v['check']['crc32']); print('BAD',bad) if bad else print('OK',len(v['cases']))" —— 通过标准：输出 check True 和 OK 45；输出 BAD 时原样回报。
- Set-Location D:\luyuan\xuanming-server-mmo-cpp\go\login; go build ./...; go vet ./internal/kafka/... ./internal/logic/pkg/dataloader/... ./internal/svc/...; go test ./internal/kafka/... ./internal/logic/pkg/dataloader/... -count=1 -v —— 通过标准：全绿；TestDBTaskReadAndWriteShareKafkaPartitionIntegration 为 SKIP；TestSendTaskLeaderNotAvailableDoesNotDeadlock、TestDBTaskProducerConfig*、TestSend*RoutesByDBTaskContract、TestSendTaskRejectsMismatchedKey、TestCommandAwarePartitionerScope、TestDBTaskTopicNamingMatchesContractPrefix、TestSendToTopic*RefusesDBTaskTopics、TestPartitionDriftIrreversiblyFencesDBTaskSends、TestPartitionsMatchContractRequiresExactContiguousSet 都 PASS。
- Get-ChildItem D:\luyuan\xuanming-server-mmo-cpp\go -Recurse -Filter *.go | Select-String 'pkg/consistent' —— 通过标准：没有任何输出。
- Set-Location D:\luyuan\xuanming-server-mmo-cpp\go\db; go build ./...; go vet ./internal/kafka/... ./internal/metrics/... ./cmd/data_stress/...; go test ./internal/kafka/... ./internal/metrics/... -count=1 —— 通过标准：全绿，并且 build 覆盖 cmd/data_stress 和 cmd/verifier。用 go test ./internal/kafka/ -count=1 -v -run 'TC3|TC5|TC7|TC8|ProducerPartition' 对比：原有 TC3/TC5/TC7/TC8 的用例数与改动前相同；新增的 TestObserveProducerPartitionVerdicts（15 个子用例）、TestObserveProducerPartitionPreRegistersAllSeries、TestObserveProducerPartitionNilInputs、TestAllowProducerPartitionLogBudget 全部 PASS。
- pwsh -File D:\luyuan\xuanming-server-mmo-cpp\go\test.ps1 —— 通过标准：退出码 0，最后输出 L1_TESTS_OK，日志里能看到三组都执行了：[db]、[shared] (./placement/... ./kafkautil/...)、[login] (./internal/kafka/...)。
- （可选，本机 docker kafka 已起时）$env:KAFKA_INTEGRATION_BROKER='127.0.0.1:9092'; Set-Location D:\luyuan\xuanming-server-mmo-cpp\go\login; go test ./internal/kafka/ -run Integration -count=1 -v; Remove-Item Env:KAFKA_INTEGRATION_BROKER —— 通过标准：TestDBTaskReadAndWriteShareKafkaPartitionIntegration PASS（pW==pR==契约分区且 oR>oW），用例自己会删掉 mmorpg_dbtask_partition_it_* topic。
- gofmt 核对：gofmt -l D:\luyuan\xuanming-server-mmo-cpp\go\shared\kafkautil D:\luyuan\xuanming-server-mmo-cpp\go\login\internal\kafka D:\luyuan\xuanming-server-mmo-cpp\go\db\internal\kafka D:\luyuan\xuanming-server-mmo-cpp\go\db\internal\metrics —— 通过标准：没有输出。go/db/cmd/data_stress/main.go 的文件头注释在改动前就不是 gofmt 格式，不计入本阶段。

## B4：B4 —— S4 的脚本部分(go_services.ps1 优雅停 + stress_summarize 新段与 S1 的 db_retry_* 列)（状态 done）

### 文件
- `tools/scripts/go_services.ps1`：新增 -ForceStop / -StopTimeoutSeconds 参数与帮助文档;目录 db 加 StopTimeoutSeconds=35 并注释可选键;新增 Get-GracefulStopSeconds / Get-ConsoleCtrlCHelperScript / Send-ConsoleCtrlC / Wait-ProcessExit / Stop-ProcessTree / Get-StopClockSeconds;Invoke-Stop 改为三段(先对全部目标投递 Ctrl+C → 按剩余时间逐个等待 → 超时或投递失败用 taskkill /T /F 强杀进程树,[gone] 输出不变),目标选择与 PID 记录写回不变;Start-ServiceInstance 的 Start-Process 处加注释,禁止 -NoNewWindow。
- `tools/scripts/tests/go_services_stop.tests.ps1`：新建,共 13 个用例:优雅停、超时强杀、投递失败立即强杀、-ForceStop、db 35 / chat 25 / -StopTimeoutSeconds 10、旧格式 PID 记录、先投递后等待、剩余时间扣减、剩余耗尽按 0 等待、[gone]、-Services 只停指定服务、辅助脚本文本与可解析性、契约(Start-Process 必须 -WindowStyle Hidden 且不带 -NoNewWindow)。服务目录取脚本 AST 里的真实字面量。
- `tools/scripts/stress_summarize.ps1`：在 2c 之后、3 之前新增 2d 'DB write path'、2e 'DB Redis steps'、2f 'DB retry routing'(S1 的 db_retry_* 列)三段,加 4 个局部辅助函数,缺序列打印 n/a;头部 DESCRIPTION 补三段说明。2c、KafkaConsumerGroup 缺省值、其它会话新增的 2bc RPC0 段都未动。
- `tools/scripts/test_stress_summarize.ps1`：原「全量输出恰好 1 行 t18m_db」的断言改为 Get-SectionRow 按 'DB task stages' 段定位,6 个均值不变;t18m_db 快照补 L-04 版的三个重试列表长度(不含 partitioned);新增 t20m_db 快照,并按段断言 2d / 2e / 2f 的表头、t20m 行数值与 t18m 行的 n/a。

### 偏离规格
- Send-ConsoleCtrlC 没有照规格用 Start-Process -Wait,改为 -PassThru + 先取 Handle + WaitForExit(20s)。辅助进程挂住时(例如 Add-Type 编译被卡)按「没投递出去」处理并强杀目标,避免 stop 无限挂起(AGENTS §11.3 的超时要求)。ExitCode 读取失败同样按没投递处理,结果只会退回旧的强杀行为。
- [gone] 在第一段遇到时就输出,没有单独放到第三段;输出文案与原来一致。-ForceStop 的输出为 '[kill]  key (PID n): -ForceStop; force-killed process tree'(规格只写了 [kill])。
- 测试里的服务目录没有自造,而是对 go_services.ps1 AST 中 $ServiceCatalogue 的字面量右值求值,这样能钉住真实的 db=35、chat 缺省 25。规格外另加了 4 个用例:旧格式 PID 记录推出服务名、剩余时间耗尽时按 0 等待、-Services 只停指定服务且其余记录保留、生成的辅助脚本可被解析。
- stress_summarize 2e 的均值保留两位小数(Redis 往返常在 0.1ms 量级,一位小数分不开)。sum 只加有序列的 step;一个序列都没有打印 n/a,有序列但 count 全为 0 打印 '-'。
- S1 ④ 的 db_retry_* 列没有追加到 2d 行尾,而是单独放在 2f 'DB retry routing' 表(仍在 DB 段内),这样规格钉死的 2d 列不变、各表宽度可读。列为:mode | q_ready q_part q_proc q_dead | exec nonowner fwd revoked misrouted rt_dead。指标名取自 S1 规格「接口」:db_retry_routing_mode{mode}、db_retry_queue_length{queue=ready|partitioned|processing|dead}、db_retry_route_total{action,mode},route 计数跨 mode 求和。
- test_stress_summarize 的 t20m 快照中 stage 与 step 的 count 都取 2(sum 翻倍),期望值与规格一致(2.0ms / 20.0ms / ...),这样「打印 sum 而不是均值」会变红。另外加入:op="read" 的 cache_set、db_redis_pool_wait_seconds_total、executed_nonowner 三条干扰序列,分别防止读写串表、wait_seconds 被并入 rpool_wait、executed 正则吃进 executed_nonowner。
- 规格写「文件头 Synopsis 补两行」,实际是在 .DESCRIPTION 的段落清单里补了三条(含 2f),因为段清单在 DESCRIPTION 里。

### 复核 / 修订意见的处理
- [fixed] S4 没有单独的复核文件;已对照当前代码核实规格对脚本的描述 —— Invoke-Stop 原为 Stop-Process -Force、启动无 -NoNewWindow,均属实;test_stress_summarize 原断言「恰好 1 行 t18m_db」属实,已改成按段定位。stress_summarize DB 段因其它会话插入 2bc RPC0 段,行号从 :490-535 漂移到约 :552-597,已按函数名和字符串重新定位。
- [fixed] S1「顺序与重叠」④:S1 的 db_retry_* 列由 S4 测量批在 stress_summarize 的 DB 段输出 —— 已放进 2f 表并配了用例:新版 db 的数值逐列断言;L-04 版 db 只有 ready/processing/dead,partitioned、路由计数和模式打印 n/a,不折成 0。
- [not_in_scope] S1 ④ 同时提到 B0 的 db_worker_inflight / db_subshard_queue_depth_max 也交给 S4 测量批 —— 本阶段指令只要求 db_retry_* 列。这两个指标在 S1「接口」和 S4 规格里都没有定义名字和 label,go/db 侧也不归本阶段,没有可对齐的契约,所以没有输出,列入 open_items。
- [fixed] BRM-B2 / BRM-F2:共享 client 时 PoolStats 不重复计数,test_stress_summarize 要补用例 —— rpool_wait / rpool_tmo 跨 redis_client 求和,正则锚在 '_wait_total{',不会吃进 wait_seconds_total。用例只给 redis_client="main" 一份 wait_total=5,断言输出 5、不翻倍。
- [fixed] H-42-4 第 ③ 条:改名或合并 cursor_set / epoch_mark 时要同改 RedisSteps、stress_summarize 2e 和 test —— 2e 段注释写明 11 个名字与 pipeline.go 的 RedisSteps 一一对应、改名需与 TestRedisStepNamesPinned 和测试同改;测试按 RedisSteps 顺序断言 11 个互不相同的均值。

### markApplied 说明
未改动。本阶段不碰 go/db 代码,markAppliedSeq / markAppliedOwnerEpoch 的调用顺序与位置不变。

### 遗留事项
- 提交状态:工作期间另有执行方把 go_services.ps1、go_services_stop.tests.ps1、stress_summarize.ps1 的改动连同其它改动提交进了 a7c510d65 和 592d7e718(不是我提交的)。test_stress_summarize.ps1 目前仍是未提交改动。
- Invoke-Stop 沿用旧的目标选择,强杀前不核对进程身份。这在旧行为里就存在,但现在 taskkill /T 会连子进程一起杀:PID 被别的进程复用时,波及面比旧的单 PID Stop-Process 大。建议后续复用 Test-InstanceProcessIdentity(需要 CIM 查询并补桩用例),本阶段未做。
- Ctrl+C 辅助进程方案未在本机实测(规格风险 10)。它依赖每个实例有独立的隐藏控制台,以及辅助进程用 SetConsoleCtrlHandler(NULL,TRUE) 后自己不被 Ctrl+C 打断。实测由 Codex 按规格「本地故障注入」第 1 步做;失败时会退回强杀,不会比旧行为差。
- stress_summarize 的 2d / 2e / 2f 依赖 go/db 侧(另一批)按 S4 / S1 规格「接口」落地指标。在那之前 Round 20 的快照里这三段全是 n/a。go/db 指标名一旦有偏差,对应列会静默变成 n/a,所以合入时要与 pipeline.go 的 RedisSteps 和 S1 的 metrics.go 逐一核对。
- B0 的 db_worker_inflight / db_subshard_queue_depth_max 尚无指标契约,stress_summarize 没有输出,需要主会话决定由谁定义指标并加列。S1 的 db_owned_partitions / db_ordering_lock_busy_total / db_durable_enqueue_total 不属于 db_retry_*,也没有输出,如需要可以后续加进 2f。
- 按指令未改 docs/PROGRESS.md、docs/ops/godb-pipeline-launch-todo.md、docs/design/db-task-pipeline-bottlenecks.md 和 Java 仓库 PARITY.md,由主会话统一写。Java 版不适用:Java 没有 Kafka 存盘链,也没有本地 go_services 启动器。
- 全部未运行,待 Codex 验证。我只对 4 个文件做了 PowerShell 语法解析(Parser::ParseFile),没有执行任何测试。

### 验证命令（交 Codex / 负责人执行）
- 工作目录:仓库根 D:\luyuan\xuanming-server-mmo-cpp(PowerShell 7)。命令:pwsh -NoProfile -File tools/scripts/tests/go_services_stop.tests.ps1 。通过标准:退出码 0,汇总行 total=13 pass=13 fail=0。失败时保留每个 [FAIL] 行及其后的断言信息。
- 工作目录:仓库根。命令:pwsh -NoProfile -File tools/scripts/tests/go_services_ports.tests.ps1 。通过标准:退出码 0,fail=0(回归:go_services.ps1 新增函数不影响端口与实例归属用例)。
- 工作目录:仓库根。前置:本机不要有正在运行的 cpp scene 节点,因为测试会临时挪走 run\logs\cpp_nodes\scene*.log 再放回。命令:pwsh -NoProfile -File tools/scripts/test_stress_summarize.ps1 。通过标准:退出码 0,末行 PASS,且有 'ok: DB write path / t20m_db'、'ok: DB Redis steps / t20m_db'、'ok: DB retry routing / t20m_db' 及三段的 t18m_db 行。失败时保留 FAIL 行和脚本打印的完整 stress_summarize 输出。若在小数点不是 '.' 的区域设置下运行,数值断言会失败,这与既有用例同一前提。
- (可选,需要本机 compose 环境,由 Codex 执行)工作目录:仓库根。先 pwsh -File tools/scripts/go_services.ps1 -Command start -Services db,再 pwsh -File tools/scripts/go_services.ps1 -Command stop -Services db 。通过标准:输出 '[stop]  db (PID n, exited in Xs)' 且没有 [kill];run\logs\go_services\db.stdout.log 中有 'Shutting down gracefully...';-Command status 不再列出 db。再验证一次 -ForceStop:输出 '[kill]  db (PID n): -ForceStop; force-killed process tree',且 go.exe 及其子进程都已退出(Get-CimInstance Win32_Process 查不到)。若第一次输出 'console interrupt not deliverable',记录辅助进程退出码(2 = AttachConsole 失败,3 = Generate 失败)并回报,不要重试。

## C1：C1 —— S3 ① 读写分区对齐的 C++ 部分（状态 done）

### 文件
- `cpp/libs/engine/infra/messaging/kafka/kafka_producer.cpp`：在 createProducer 里 enable.idempotence 那段之后、RdKafka::Producer::create 之前加 conf->set("partitioner", "consistent_random", errstr)。设置失败只打 LOG_ERROR，不 return false。中文注释写明：分区契约是 CRC-32 mod N；值等于 2.14.0 的默认值；换函数等于换 TopicGeneration；这是 topic 级属性，会 fallthru 到默认 topic conf；设置失败也不阻止建实例。kafka_producer.h 没动，别的会话加的 brokersConfigured() 原样保留。
- `cpp/libs/services/scene/player/constants/player.h`：在 GetDbTaskTopic 之后新增 inline std::string DbTaskKafkaKey(uint64_t playerId)，函数体是 return std::to_string(playerId)。中文注释写明它与 Go 的 kafkautil.DBTaskKey 逐字节相同、key 的字节属于分区契约，并给出契约文档与 golden 向量的路径。
- `cpp/libs/services/scene/player/system/player_lifecycle.cpp`：只改了一行。按内容定位到 SavePlayerToRedisImpl 里唯一的 `const std::string playerIdStr = std::to_string(playerId);`（当前在 :4128，规格写的 :3989 已过时），改成 DbTaskKafkaKey(playerId)，行尾加注释。生成的字节不变。改前核对过 git diff，这一行和别的会话的在途 hunk（InitPlayerFromAllData 等，止于约 :3966）不在同一个 hunk。
- `cpp/tests/kafka_command_test/kafka_partition_contract_test.cpp`：新增 gtest 套件 DbTaskPartitionContract，共四例，全部未编译、未运行：
1. VendoredDefaultPartitionerIsConsistentRandom：按复核意见加了 rd_kafka_version()==RD_KAFKA_VERSION 断言，并用 RecordProperty 记下运行时版本串；然后检查 topic conf 的默认 partitioner。
2. ExplicitPartitionerOnGlobalConfReachesDefaultTopicConf。
3. LibrdkafkaConsistentRandomMatchesSharedVectors：读 go/shared/kafkautil/testdata/dbtask_partition_vectors.json，逐条调 rd_kafka_msg_partitioner_consistent_random；另外用 check.crc32 % N 间接核对自检值，并断言用例数 ≥45、覆盖 N∈{7,10,16,20,INT32_MAX}。
4. KafkaKeyMatchesGoFormatUint。
不写 main。
- `cpp/tests/kafka_command_test/kafka_command_test.vcxproj`：在 ClCompile ItemGroup 里按字母序追加一行 <ClCompile Include="kafka_partition_contract_test.cpp" />，位置在 kafka_manager_init_test.cpp 和 kafka_producer_policy_test.cpp 之间。别人的条目和设置没动。这个工程没有 .filters 文件，也不在任何 CMakeLists 里，所以不需要再改别处。

### 偏离规格
- include 顺序：规格写的是先 include gtest、yaml-cpp、STL，最后才 include rdkafka.h。实际改成定义 LIBRDKAFKA_STATICLIB 之后立刻 include "librdkafka/src/rdkafka.h"，再 include 其余头文件。理由：rdkafka.h 在 Windows 上会 include <winsock2.h>，放在最前面可以避免别的头先拉进 windows.h/winsock.h 造成重定义。行为不变。
- FindDbTaskVectorFile 的起点：规格只写了 fs::current_path()。实际在它之外，又加了本源文件目录（fs::path(__FILE__).parent_path()，只在它是绝对路径时才用）作为兜底起点，两个起点都最多上溯 8 层。理由：cwd 不在仓库里时也能找到向量文件。run_cpp_tests.ps1 以仓库根作 cwd 启动，main 找到配置后会把 cwd 切到 bin/，这两种情况都已覆盖。
- 复核意见里属于 C++ 范围的那条已落实：rd_kafka_version()==RD_KAFKA_VERSION 断言和 RecordProperty 都放进了 VendoredDefaultPartitionerIsConsistentRandom，没有单开第五个用例，所以 Codex 的通过标准仍是「DbTaskPartitionContract.* 四例」。断言用的是 EXPECT 而不是 ASSERT，版本不一致时后面的默认分区器检查照样会跑。
- LibrdkafkaConsistentRandomMatchesSharedVectors 在规格之外多了几项断言：
- check.key=="123456789"、check.crc32==0xCBF43926；
- 对 N∈{7,10,16,20,INT32_MAX}，librdkafka 算出的分区必须等于 check.crc32 % N；
- 向量必须覆盖这五个 N；
- 每条 case 先 ASSERT key 非空、n>0，避免空 key 走随机分区器时访问 rkt=nullptr，或 n=0 时除零崩溃。
- KafkaKeyMatchesGoFormatUint 额外断言 DbTaskKafkaKey(INT64_MAX)=="9223372036854775807"。std::stoull 抛异常时记 ADD_FAILURE 后继续下一条，不中断整个用例。

### 复核 / 修订意见的处理
- [fixed] [medium] player_lifecycle.cpp 的行号锚点已过时（整体下移约 43 行；当前实际下移约 139 行）。照行号改会改错位置，造成重复定义 —— 按内容定位到 SavePlayerToRedisImpl 里唯一的 `const std::string playerIdStr = std::to_string(playerId);`（当前在 :4128），只替换了这一行。改前后都看过 git diff：这个 hunk 与别的会话、与 S5 的发送失败分支都不重叠，下面 send 那几行一个字没动。kafka_producer.cpp 也是按 enable.idempotence 那段的内容定位的（当前行号恰好与规格一致）。
- [fixed] [low] C++ golden 用例名叫 Vendored…，但测的是运行时加载的 rdkafka.dll，未必是 vendored 源码 —— 在 VendoredDefaultPartitionerIsConsistentRandom 里加了 EXPECT_EQ(rd_kafka_version(), RD_KAFKA_VERSION)，失败信息带十六进制版本号和 rd_kafka_version_str()。同时用 RecordProperty("librdkafka_runtime_version", …) 把版本串记进测试报告。这样「运行时库就是 vendored 版本」本身成了被测条件。已确认 lib/rdkafka.lib 导出本测试用到的全部 C 符号。
- [not_in_scope] [medium] 上线抽样门槛 TestDBTaskPartitionAuditDump 可能一条写都没检查就判通过（other 行不设门槛、op 用子串匹配、key 口径与 observer 不一致、没剥 BOM 和 \r） —— 涉及的是 go/shared/kafkautil/dbtask_partition_test.go（A1 等 Go 阶段），不在 C1 的负责文件里。
- [not_in_scope] [medium] 冒烟流程「删键前确认 W 真的发出了」：scene 的日志 "saved to Redis, DB write tasks enqueued" 证明不了这一点 —— 这条改的是 rollout 和契约文档里的冒烟流程（runbook），不在 C1 范围。本阶段没有改那条日志，也没有改发送失败分支（那部分归 S5）。
- [not_in_scope] [medium] 「屏障覆盖范围与剩余缺口」不完整：读结果回写放大效应、部分命中、AOF 回退、写先于读进入 Kafka 的前提 —— 要改的是契约文档和 go/db 的 processTaskBatch 注释，不在 C1 范围。
- [not_in_scope] [low] key_ordered_producer.go:124-127 的 plainCfg 注释会过时 —— Go login 文件，不在 C1 范围。
- [not_in_scope] [low] 行为变化列得不全；回滚前置条件应写成 merge-base --is-ancestor d5cbb8a8e —— 要改的是交付说明、PROGRESS 和 risks，由主会话或 Go 阶段处理。C1 自身的运行时变化为零（显式值等于默认值），回滚 scene/battle 镜像也没有前置条件。
- [not_in_scope] [low] 集成用例里的 W 生产者缺 Return.Successes 和 RequiredAcks，topic 名用了 db_task_zone_ 前缀 —— Go login 测试文件，不在 C1 范围。
- [not_in_scope] [low] TestDBTaskPartitionDistribution 作为硬断言进 L1，失败了也修不了 —— Go 测试文件，不在 C1 范围。
- [not_in_scope] [low] 文件重叠清单漏了 S1 也改 ConsumeClaim；observer 取 N 的前提没有写成不变量 —— go/db 和文档，不在 C1 范围。
- [not_in_scope] [low] "db_task" 条目偏离复核原文，应交负责人确认；topic 前缀没有测试与真实命名函数绑定 —— go/login 的 command_partitioner_test 和 decisions_needed 归 Go 阶段和主会话。C++ 侧的 topic 名仍由已有的 GetDbTaskTopic 生成，本阶段没改它。
- [not_in_scope] [low] 生产操作的执行人与 AGENTS §9 冲突；抽样 runbook 没写输出编码 —— runbook 和文档，不在 C1 范围。

### markApplied 说明
没有变化。本阶段只改 C++ 文件，没有触碰 go/db，markAppliedSeq / markAppliedOwnerEpoch 的调用顺序和位置原样不动。

### 遗留事项
- 全部改动未编译、未运行（AGENTS §10.1），等 Codex 验证。不能宣称编译通过或测试绿。
- 向量文件 go/shared/kafkautil/testdata/dbtask_partition_vectors.json 已由 A1 写好（只读核对过）。格式与 C++ 测试读取的一致：check.key / check.crc32、cases[].key / n / expect_partition，共 45 条，含 n=2147483647。我没有改这个文件。
- rd_kafka_version()==RD_KAFKA_VERSION 这条断言是新的被测条件。bin/rdkafka.dll 是 10-15 04:34 从源码构建的 Debug 版，里面有 telemetry 等新版源文件路径，但没法静态确认它就是 2.14.0。如果 Codex 跑出版本不一致：这条 EXPECT 会红，其余断言照常执行。应回报 DLL 的版本串，由负责人决定统一 DLL 还是调整断言，不要直接删断言。
- 工作区里 kafka_manager_init_test.cpp 在本阶段期间被别的会话改动（git status 显示 M），我没有碰它。如果 kafka_command_test 编译失败，要先区分是哪个 TU 的问题。
- DbTaskKafkaKey 现在只有 player_lifecycle.cpp 一个调用点。规格风险 16 提到：以后如果加「从 yaml 透传 librdkafka 参数」的能力，必须同时禁止覆盖 partitioner。这是给后续的约束，本阶段不涉及。
- docs/PROGRESS.md、契约文档、Java 仓库的 PARITY.md 按要求没改，由主会话统一写。C++ 这部分的登记内容是：kafka_producer.cpp 显式 consistent_random、player.h DbTaskKafkaKey、kafka_partition_contract_test.cpp 四例、vcxproj 一行、player_lifecycle.cpp 一行。Java 版不适用：Java 版存盘没有 Kafka DBTask 通道。

### 验证命令（交 Codex / 负责人执行）
- [前置] 工作目录 D:\luyuan\xuanming-server-mmo-cpp：git -C D:\luyuan\xuanming-server-mmo-cpp submodule status third_party/librdkafka —— 通过标准：行首不是 '-'（当前是 f1b831e，已检出）。如果需要 init，属于联网下载，要先征得负责人同意。
- [C++ 全量串行编译] 工作目录 D:\luyuan\xuanming-server-mmo-cpp：msbuild D:\luyuan\xuanming-server-mmo-cpp\game.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64 —— infra（kafka_producer.cpp）和 scene（player.h、player_lifecycle.cpp）都改了，所以要全量编译。必须 /m:1，并发会报假的 C1041/LNK1104。通过标准：0 错误；kafka_producer.cpp、player_lifecycle.cpp、kafka_partition_contract_test.cpp 没有新增警告。失败时保留编译错误的前 20 行，并注明出错的 TU。
- [C++ 单测] 工作目录 D:\luyuan\xuanming-server-mmo-cpp：pwsh -File D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\run_cpp_tests.ps1 -Build -Filter kafka_command_test —— 通过标准：DbTaskPartitionContract.VendoredDefaultPartitionerIsConsistentRandom、ExplicitPartitionerOnGlobalConfReachesDefaultTopicConf、LibrdkafkaConsistentRandomMatchesSharedVectors、KafkaKeyMatchesGoFormatUint 四例全部 PASS；原有用例（CommandPartitionVectors、CommandTopicNaming、ClassifyProduceResult、DeliveryErrorClassification、RebuildGate、FailureLogThrottle、kafka_manager_init_test、whitelist 等）不新增失败。main 找不到 base_deploy_config.yaml 时只告警，属正常。
- [可选，单独复跑并看版本属性] 工作目录 D:\luyuan\xuanming-server-mmo-cpp：D:\luyuan\xuanming-server-mmo-cpp\build\cpp\tests\kafka_command_test.exe --gtest_filter=DbTaskPartitionContract.* --gtest_output=xml:D:\luyuan\xuanming-server-mmo-cpp\build\cpp\tests\dbtask_partition_contract.xml —— 通过标准：4/4 PASS；XML 里的 librdkafka_runtime_version 属性应为 2.14.0。golden 不一致或版本不一致时，保留完整输出和 XML 回报，不改向量 JSON，不删断言。
- [三方机械复核的 C++ 一方] 同一份向量文件由 Go 侧 go test ./kafkautil/ -run DBTask（工作目录 D:\luyuan\xuanming-server-mmo-cpp\go\shared，属 A1 阶段）用 crc32.ChecksumIEEE 和 sarama 校验，C++ 侧由上面的 LibrdkafkaConsistentRandomMatchesSharedVectors 用 librdkafka 校验。三方都通过才算 golden 表成立；任何一方不一致都以机器结果回报。

## C2：C2 —— S5a scene 发 DBTask 失败后的脏快照(C++)（状态 done）

### 文件
- `cpp/libs/engine/infra/messaging/kafka/kafka_producer_policy.h`：新增 header-only 的 DeliveryTracker(Accept / Resolve / TakeAll / Size,0 不跟踪),注释写明「每个 Accept 过的 id 恰好一次经 Resolve 或 TakeAll 离开」
- `cpp/libs/engine/infra/messaging/kafka/kafka_producer.h`：新增 DeliveryReport、DeliveryListener、sendTracked、setDeliveryListener、trackedInFlight;私有部分加 sendImpl、带 msgOpaque 的 produceOnce、notifyDelivery 和两个成员,加 uintptr_t 宽度的 static_assert;补「逐条回执契约」类注释;保留 brokersConfigured()
- `cpp/libs/engine/infra/messaging/kafka/kafka_producer.cpp`：send/sendTracked 走 sendImpl,关联号经 msg_opaque 传递,入队成功后在 poll 之前 Accept;dr_cb 中 Resolve 后成功、失败都通知;rebuildAfterFatal 在 reset 之前用 TakeAll 合成 ERR__PURGE_QUEUE;析构第一行先注销监听器;createProducer 加 message.timeout.ms 与 330s 的耦合注释。C1 的 partitioner 段未动
- `cpp/libs/services/scene/player/system/db_task_dispatch_ledger.h`：新增纯账本 db_task_dispatch::Ledger(按代号比较 NeedsResave、票据、超时兜底、条目回收)、DeliveryEffect 及名字表、AllowsDirtySaveSkip
- `cpp/libs/services/scene/player/system/db_task_dispatch_ledger.cpp`：新增 Ledger 实现;inFlight 不会下溢(下溢时 LOG_ERROR 并钳到 0);Clear 不重置代号和票据计数器
- `cpp/libs/services/scene/player/system/db_task_dispatch_system.h`：新增粘合层 DbTaskDispatchSystem(全静态)和 db_task_dispatch_stats 的 12 项进程级计数、名字表、Snapshot::AlarmingIncreaseSince;kTicketTimeout=330s,kShutdownUnsettledLogLimit=50
- `cpp/libs/services/scene/player/system/db_task_dispatch_system.cpp`：新增实现,状态放在函数内 thread_local;监听器用无捕获 lambda,只记账和压待办;Pump 依次 poll、超时兜底、排序去重后交给 onSettled;Shutdown 幂等,列出未落定玩家;未登记监听器时 SendDbTask 退化为不跟踪的 send
- `cpp/libs/services/scene/player/system/player_exit_intent.h`：PlayerExitIntentComp 新增 awaitingDbTaskAck、dbTaskResaveRounds,并补写成对约定;player_exit 新增 kMaxExitDbTaskResaveRounds=3、DbTaskGateDecision 及名字表、DecideDbTaskGate、ShouldRekickDbTaskExhausted
- `cpp/libs/services/scene/player/system/player_lifecycle.h`：新增 public 的 FinishExitOrAwaitDbTaskAck 和 OnDbTaskDispatchSettled 声明;更新 IsSaveInFlight 与 FinishExitAfterPersist 的注释
- `cpp/libs/services/scene/player/system/player_lifecycle.cpp`：快路径加 AllowsDirtySaveSkip(欠账时强制真写并计 forced_resave);发 DBTask 段加 BeginSave / MarkLocalFailure / SendDbTask / FinishSave;5 个 FinishExitAfterPersist 收尾口改走闸门;HandleExitGameNode 增加 DBTask rekick 的 else-if;DestroyPlayer 末尾在没有替身实体时调 OnPlayerGone;新增 FinishExitOrAwaitDbTaskAck 与 OnDbTaskDispatchSettled 两个函数
- `cpp/libs/services/scene/core/system/redis.h`：新增 dbTaskPumpTimerId_ 与 dbTaskPumpTimerActive_ 两个成员
- `cpp/libs/services/scene/core/system/redis.cpp`：Initialize 中调 DbTaskDispatchSystem::Initialize;新增 50ms 的 Pump 定时器(无捕获,BeginShutdown 不取消);30s 统计行新增 [DbTaskDispatch],出现告警性增长时升为 WARN;Shutdown 在 loop_ 分支内取消 Pump 并调 DbTaskDispatchSystem::Shutdown
- `cpp/libs/services/scene/CMakeLists.txt`：登记 db_task_dispatch_ledger.cpp 和 db_task_dispatch_system.cpp,附中文注释
- `cpp/libs/services/scene/scene.vcxproj`：新增 2 个 ClCompile 和 2 个 ClInclude
- `cpp/libs/services/scene/scene.vcxproj.filters`：登记 4 个新文件,写法与 player\system 下其它文件一致(该文件原本就没有 Filter 子元素)
- `cpp/tests/kafka_command_test/kafka_producer_policy_test.cpp`：新增 5 个 DeliveryTracker 用例
- `cpp/tests/cross_zone_test/db_task_dispatch_test.cpp`：新增测试文件:DbTaskDispatchLedger 12 例、DbTaskDispatchStats 1 例、ExitDbTaskGate 3 例、DbTaskDispatchEcs 9 例(固件前后清账本,TearDown 时注销监听器)
- `cpp/tests/cross_zone_test/cross_zone_test.vcxproj`：追加 ClCompile db_task_dispatch_test.cpp
- `cpp/tests/cross_zone_test/cross_zone_test.vcxproj.filters`：登记 db_task_dispatch_test.cpp,Filter 为 source

### 偏离规格
- SavePlayerToRedisImpl 现在返回的是 SaveResult 枚举,不是规格写的 bool,已按枚举逐值处理。闸门重存时:kQueued 返回 false,等落地回调;kRejected(持久组件缺失)保留实体并返回 false,不收尾、不计 exit_unsynced,理由是 ecs-component-access-rules.md 规定「缺组件拒存不是已落盘证据,不得据此销毁实体」;kHandoffAlreadyRequested 时若还有未落地的 Redis 存盘就等它落地,否则计 exit_unsynced 后收尾。规格原文是「返回 false 就收尾并计 exit_unsynced」。
- OnDbTaskDispatchSettled 里 MarshalPlayerForSave 失败时保留实体、直接返回,不调闸门(规格写的是「其余情况调 FinishExitOrAwaitDbTaskAck」)。理由同上一条,与 HandlePlayerAsyncSaved 退出分支的处理保持一致。
- OnDbTaskDispatchSettled 发现内存与快照不一致时,不再无条件重存,而是复用退出分支的 DecideAfterPersist 和 resaveRounds / kMaxExitResaveRounds 做上限:到上限就保留实体并计 exit_resave_capped。目的是避免「等回执 → 发现变了 → 重存 → 再等回执」的无界循环。没有快照时按不收敛处理。
- 闸门的 kRetainExhausted 分支每次判定都计 exit_resave_capped 并打 ERROR,不只在首次。规格要求用 awaitingDbTaskAck 判断「首次」,但 kResave 已经把它置位,照规格写的话这个计数在生产上永远不会增长。kAwaitAck 仍按规格只在首次计数。
- DestroyPlayer 不再有 AnomalyDetector::ClearPlayer。OnPlayerGone 放在函数末尾,而且只在 playerList 里已没有该玩家号时才调用:离场通知里可能重建出同号的新实体,不能把新实体要补写的欠账擦掉。
- Ledger::FinishSave(allQueued=false) 也把该代号记为失败(防御写法,结论不变);Ledger::Clear 不重置代号和票据计数器,防止生产者登记簿里的旧关联号撞上新票据。
- DbTaskDispatchSystem::SendDbTask 在监听器未登记时退化为不跟踪的 send:同步失败仍记失败,异步失败无从得知。生产上 RedisSystem::Initialize 一定先于任何存盘,这条只是防御。
- 测试固件的 TearDown 在 ResetForTest 之外还调了 Shutdown,用来注销监听器、恢复未初始化状态;ExpiredTicketDuringPumpCountsAndEnqueues 发了两张票据,所以断言 ticket_expired 增量为 2(规格写的是 1);另外新增 ExhaustedRekickWithMissingComponentRetainsEntity 与 DbTaskDispatchStats.AlarmingIncreaseIgnoresSupersededFailures 两例。
- kafka_producer.h 的类注释没有引用规格里的 docs/design/db-task-delivery-and-retention.md(该文件不在本阶段清单里,还不存在),改为指向 db_task_dispatch_system.h。

### 复核 / 修订意见的处理
- [fixed] S5 没有单独的复核文件,规格声称回执只在本线程的 poll/flush 里派发、send 是唯一驱动点,需要自行核实 —— 已对照 vendored 源码:ProducerImpl.cpp 用 dr_msg_cb_trampoline 登记 dr_msg_cb,rd_kafka_flush 在 DR_MODE_CB 下内部调 rd_kafka_poll;scene 的生产者是 loop 线程的 thread_local,未配 background queue / event_cb。结论成立,因此新增 50ms Pump 定时器,在 loop 顶层 poll 并处理待办,监听器本身不碰 ECS。
- [fixed] B-04:msg_opaque 能否原样带回关联号 —— 已核实 ProducerImpl::produce 的 headers 重载把最后一个参数作为 RD_KAFKA_V_OPAQUE 传入,MessageImpl::msg_opaque() 返回 rkmessage->_private;rd_kafka_destroy 不回调 dr_cb,所以重建时要在 reset 之前 TakeAll 合成失败回执。已照此实现。
- [fixed] B-05:实体销毁之后才到的回执 —— 监听器只改账本、只压待办;OnDbTaskDispatchSettled 只用 GetPlayer / valid / try_get,实体不在时调 OnPlayerGone 回收条目,有欠账时计 orphan_unsynced。没有新增任何 get_or_emplace。
- [fixed] B-06:SavePlayerToRedisImpl 返回 false 时收尾并计 exit_unsynced(行号与签名已过时) —— 签名现为 SaveResult 枚举,已按值处理:kRejected 时 fail-closed 保留实体,只有交接已发起才计 exit_unsynced 后收尾。偏离理由见 deviations。
- [fixed] B-08:回调生命周期(§11.7,只绑 weak_ptr,不裸绑 this) —— 监听器和 Pump 都是无捕获 lambda 转调静态函数;KafkaProducer 析构第一行先注销监听器;DbTaskDispatchSystem::Shutdown 只在 RedisSystem::Shutdown 的 loop_ 分支里调用且幂等。
- [fixed] 规格 ⑧ 的收敛核对在不收敛时无条件重存,可能无界循环 —— 改为复用 DecideAfterPersist 和 resaveRounds 上限,见 deviations。
- [not_in_scope] B-19:与条件写会话(DBTask 上写 save_seq)协调同一个 lambda —— 当前代码里 DBTask 还没有 save_seq;本阶段只替换了发送那一行并在 lambda 前后加记账,save_seq 留给对方加在 SerializeToString 之前。强制重存走同一个 SavePlayerToRedisImpl,会自然拿到新的 save_seq。
- [fixed] 规格行号与「DestroyPlayer 开头紧挨 AnomalyDetector::ClearPlayer」已过时 —— 按函数名重新定位了 5 个 FinishExitAfterPersist 调用点;DestroyPlayer 里已没有 ClearPlayer,改在末尾带替身检查调 OnPlayerGone。
- [rejected] B-07:能否靠 go/db 的游标或 epoch 挡住旧载荷重放 —— 与规格结论一致,不需要改 go/db:本实现从不重放旧字节,强制重存一律按当前内存重新 marshal。

### markApplied 说明
未碰 go/db 的任何代码,markAppliedSeq / markAppliedOwnerEpoch 的调用顺序与位置不变。

### 遗留事项
- 全部未编译、未测试(AGENTS §10.1),需等 Codex 验证。
- 设计文档 docs/design/db-task-delivery-and-retention.md(规格要求 S5a 写 §A)不在本阶段文件清单内,没有写;docs/README.md 登记、docs/PROGRESS.md 条目、docs/ops/godb-pipeline-launch-todo.md 的「S5 验证」小节,以及 Java 仓库 PARITY.md 的两行「不适用」,都需要主会话补。
- S5b(Go 的 RetentionBytes 与 k8s_deploy 的 Kafka 盘预算)不归本阶段,未做。
- 行为变化需要负责人知悉:没登记 broker 或 Kafka 长时间不可用时,每次 DBTask 都失败,账本恒为欠账。结果是在线玩家的周期存盘不再被快路径跳过(每周期整份写一次 Redis),退出实体重存 3 轮后被保留,直到 Kafka 恢复、rekick 或停机看门狗。这符合负责人「Kafka 不可用时保留」的拍板,但 Redis 写量会回到没有 dirty-save 时的水平。
- orphan_unsynced 包含 exit_unsynced 的那部分(交接已发起时收尾,两个计数都会加 1),已在 db_task_dispatch_system.h 注释写明。
- 工作区有外部的定期提交:本阶段的多数改动已被 0f6690604、676c64789、9b8b902da、41789f474、9f320e485、526780b4e 这几次提交带走(不是本会话提交的)。目前只剩 cross_zone_test.vcxproj / .filters 的修改和新文件 db_task_dispatch_test.cpp 还未提交。
- 规格 B-22 的既有风险本轮未改:scene 先于 db/login 发 DBTask 时,broker 的 auto.create 会把 db_task 建成 1 个分区。

### 验证命令（交 Codex / 负责人执行）
- 工作目录 D:\luyuan\xuanming-server-mmo-cpp,PowerShell 7 + VS 开发者环境,MSBuild 严格串行 /m:1,前一步红了就停下归因、不连续重试。
- msbuild cpp\libs\engine\infra\infra.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64   # 通过:0 error
- msbuild cpp\libs\services\scene\scene.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64   # 通过:0 error(新增 db_task_dispatch_ledger.cpp / db_task_dispatch_system.cpp 能编译)
- msbuild cpp\tests\kafka_command_test\kafka_command_test.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64   # 通过:0 error
- msbuild cpp\tests\cross_zone_test\cross_zone_test.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64   # 通过:0 error,新文件 db_task_dispatch_test.cpp 能链接
- msbuild cpp\nodes\scene\scene.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64   # 通过:链接无 unresolved external(核对 scene 节点链接)
- pwsh tools/scripts/run_cpp_tests.ps1 -Filter kafka_command   # 先同步 bin/ 下的 DLL 并跑全量;通过:全绿
- pwsh tools/scripts/run_cpp_tests.ps1 -Filter cross_zone   # 通过:全绿,既有 ExitPersist* 用例没有新增失败
- 如需单独跑:.\build\cpp\tests\kafka_command_test.exe --gtest_filter=DeliveryTracker.*  以及  .\build\cpp\tests\cross_zone_test.exe --gtest_filter=DbTaskDispatch*:ExitDbTaskGate*:ExitPersist*   # 通过:新用例全绿
- 回归(同样链接 scene.lib,会走退出 / 存盘路径):msbuild cpp\tests\bag_test\bag_test.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64 ; msbuild cpp\tests\currency_test\currency_test.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64 ; pwsh tools/scripts/run_cpp_tests.ps1 -Filter bag ; pwsh tools/scripts/run_cpp_tests.ps1 -Filter currency   # 通过:与改动前相比无新增失败
- 失败时保留:msbuild 第一条 error 的前后 20 行、gtest 失败用例全文。报错落在其它会话的未提交文件里时(例如 muduo_grpc/client_session.cpp、generated/*)单独记录,不算本阶段的问题。

## A2：A2 —— S2 ③ 消费组按 zone 拆的 Go 部分（原规格 PR-B 代码部分；以 S2_addendum 修订稿为准）。全部未编译、未运行测试，待 Codex 验证。（状态 done）

### 文件
- `go/shared/kafkautil/group_lag.go`：按修订稿 R11 抽出 CommittedOffsetsReader 接口和导出函数 CommittedOffsets(r, group, topic, partitions)。以下情况都 fail-closed 返回 error：调用失败、resp 为 nil、顶层 Err、block 缺失、block.Err。KError 用 %w 包住，可以 errors.Is(ErrGroupIDNotFound)；-1 原样返回。groupOffsetReader 内嵌该接口。computeGroupLag 改为调用它，只在错误文本前补 'kafka lag probe: '。错误文本、读取顺序（先 committed 后 end/start）、对外行为都与抽取前相同。
- `go/shared/kafkautil/group_lag_test.go`：新增 TestCommittedOffsets，覆盖：正常值与 -1 原样返回、只查请求的分区、顶层 ErrGroupIDNotFound 能被 errors.Is 识别、nil 响应、block 缺失、block.Err、调用本身失败。另钉住 computeGroupLag 的错误文本与抽取前逐字一致。TestComputeGroupLag 未改。
- `go/db/internal/config/config.go`：KafkaConfig 的 GroupID 改为 optional，注释写明它是最终形状键（四种形状见注释）。新增 ConsumerGroup、GroupHandoffFrom、SessionTimeoutMs、HeartbeatIntervalMs、RebalanceTimeoutMs。新增常量 DefaultKafkaSessionTimeoutMs/HeartbeatIntervalMs/RebalanceTimeoutMs（10000/3000/60000）和 MinKafkaRebalanceTimeoutMs=15000，注释写明与 claimDrainTimeout 同步。新增 EffectiveGroupID() 和 ValidateConsumerGroup()，后者错误文本按规格写成英文，刻意不叫 Validate，注释说明原因。Normalize 末尾回填三个超时。gofmt 重新对齐了 KafkaConfig 里 Topic/TopicGeneration/PartitionCnt/RetentionMs 四行（只改空白）。
- `go/db/internal/config/config_test.go`：新增用例：TestKafkaTimeoutDefaults、TestValidateConsumerGroup（表驱动 9 例）、TestEffectiveGroupID、TestPreHandoffBinaryRefusesZoneShapedConfig（用旧结构加载交接形状，必须报含 GroupID 的错；新结构能加载并通过校验）、TestPreHandoffBinaryLoadsFinalZoneShape（修订稿 R1）、TestPreHandoffBinaryIgnoresUnknownKeys。额外加了 TestEtcYamlConsumerGroupIsFinalShape：etc/db.yaml 必须是裸名 final 形状，钉住脚本推导组名用的基名。
- `go/db/internal/kafka/group_handoff.go`：新文件，文件头用中文写清职责和不变量。包含：groupInspector 与 peerGroupView；saramaGroupInspector（sarama.Client 加 NewClusterAdminFromClient，5s 网络超时，Metadata.Retry.Max=1，ClientID 加 -handoff 后缀；CommittedOffsets 委托 kafkautil，组不存在按全 -1 处理；DescribeGroup 遇到 GROUP_ID_NOT_FOUND 记为 Dead）；纯函数 peerViewFromDescription 与 heldPartitions（解码失败按 fail-closed 报错）。groupHandoff 持有工厂并懒建 inspector，withInspector 在任一次查询出错后 Close 并重建（R2）。evaluate 先在一次 withInspector 里取齐事实，再交给纯函数 decideGate，按修订稿 4.3 的 8 档顺序判定（删掉 peer_unreachable_own_complete，新增 wait_peer_state_unknown）。await 只在入组前判定一次，放行时把 settled 置为本组位点 ≥0 的分区；等待时打日志，wait_own_behind_peer 的日志写明两种处置。seed 维护 settled 缓存：刚播种的分区不进 settled，no_offsets 的分区进 settled；MarkOffset 用原值、不加 1。watch/watchOnce 只凭正面证据让出。另有 yield、setSessionYield、clearSessionYield、Close，以及 consumerClientID 和 rebalanceGapTracker。consumerKafkaVersion=V3_5_0_0 由消费者与 inspector 共用。
- `go/db/internal/kafka/group_handoff_test.go`：新文件。替身：fakeCluster（内存里的组状态、按组注入错误、调用计数）、fakeInspector、fakeInspectorFactory（第 i 次建出的实例可以注入为坏连接）、fakeClaim、encodeAssignment（按大端手工编码成员分配）。harness 注入假 sleep 与 now，不依赖墙钟，等待超过 100 轮判失败，防止挂死。用例覆盖规格 tests 的全部条目（T1a/T1b/T2/T3/R7/T4/T5/T6 新口径/ctx 取消/快路径/只闸一次/T7/门闸住的对方不触发让出且正反向不互等/竞态登记/T8/T9/T10/ClientID/Setup 播种并挂上让出/播种失败/ConsumeClaim 让出/无 handoff 保持旧行为），以及修订稿新增的 PeerStateUnknownWaits、Inspector_RebuiltAfterError、Seed_SkipsSettledPartitions、Yield_ClearsSettled、Gate_ReleaseSeedsSettledFromOwnOffsets。额外加了 Seed_AbortsOnQueryFailure、PeerViewFromDescription_ErrorMapping、ConsumerSetup_ObservesRebalanceGap、NewKeyOrderedKafkaConsumer_RejectsInvalidConsumerGroupBeforeKafka。
- `go/db/internal/kafka/key_ordered_consumer.go`：按修订稿 4.6 修改：结构体新增 handoff、handoffDone、gap、now（另加 nowTime() 回落）。构造函数改为：先对本地副本 Normalize，再 ValidateConsumerGroup，用 EffectiveGroupID 建组；Version 改用 consumerKafkaVersion，设置 ClientID 与三个超时；Offsets.Initial 仍是 OffsetOldest，加了注释；最后 newGroupHandoffFromConfig，失败时先关 consumerGroup。Start：有 handoff 时起守望，每次 Consume 之前先 await；启动日志加 handoffFrom。consumerGroupHandler 新增 yieldCtx、yieldCancel、setupDone，以及 sessionScope() 与 yielded()。Setup：分区契约检查之后，只在 handoff 非空时 seed，再建 yieldCtx 并 setSessionYield（返回 false 就立即 cancel）；最后记 gap。Cleanup 容忍 Setup 失败后被调用。ConsumeClaim：claimCtx 的父 ctx 改为 sessionScope；外层 select 加 yieldDone 分支（内层排空 select 也加了）；派发 select 的取消分支在 session 结束或已让出时都 return nil。Stop 末尾加 closeHandoff。observeProducerPartition 那一行、handleTask、processTaskBatch、runDBOp、deferFailedTask、重试相关代码一律未动。
- `go/db/internal/kafka/key_ordered_consumer_test.go`：recordingSession.MarkOffset 改为在锁内记录 (topic, partition, offset)，新增 markOffsetCall 类型和 markOffsetCallsSnapshot()；ResetOffset 仍为空实现，现有断言不变。新增回归用例 TestReplayAfterSeed_WritesAreOrderingStale：3 个 key 先应用到 offset S，再从 S−3 到 S 重放，断言不调用 SQL、游标不变、哨兵缓存不被改写、offset 照常 ACK、不进死信也不进重试队列。
- `go/db/internal/metrics/metrics.go`：只追加。新增一个独立的 var 块，含 6 个指标：db_consumer_group_rebalance_gap_seconds、db_consumer_group_handoff_total{result}、db_consumer_group_handoff_gate_total{outcome}、db_consumer_group_handoff_waiting_seconds、db_consumer_group_handoff_wait_seconds、db_consumer_group_handoff_conflict_total。新增常量 HandoffSeed*（4 个）和 HandoffGate*（8 个，修订稿口径），以及导出函数 ObserveRebalanceGap、CountHandoffSeed、CountHandoffGate、SetHandoffWaiting、ObserveHandoffWait、CountHandoffConflict。register() 在 S3 的 producerPartitionTotal 段之后追加一条单独的 MustRegister，并以 0 预建全部 result 与 outcome 序列和 conflict。S3 的指标原样保留。
- `go/db/internal/metrics/metrics_test.go`：新增 TestConsumerGroupHandoffSeriesPrecreated：conflict 恰好一条且为 0；gate 恰好 8 个 outcome 且都为 0，不含 peer_unreachable_own_complete；handoff_total 恰好 4 个 result；两个直方图与一个 gauge 已注册。通过下标索引，不引入 client_model。
- `go/db/etc/db.yaml`：GroupID 行保持 `GroupID: "db_rpc_consumer_group" # ...` 的形状，只改行尾注释，go_services.ps1 与 dev_tools.ps1 的正则仍能匹配。下面新增注释段，说明：基名的用途；单 zone 默认启动用裸名、-Zone N 追加 _zN（修订稿 R10）；K8s 的四种形状（final(G)、handoff(X←Y)）。另加 5 行注释掉的键 ConsumerGroup、GroupHandoffFrom、SessionTimeoutMs、HeartbeatIntervalMs、RebalanceTimeoutMs 及各自约束，并指向设计文档。
- `go/login/internal/config/config.go`：只改 KafkaConfig.GroupID 一行：改为 `json:"GroupID,optional"`，并加注释说明已废弃、login 代码不读、与 go/db 的消费组无关、三处配置按设计文档 §8 Step 7 删除。值和 login.yaml 都未动。该文件另一会话在途的 PasswordAuthConf 改动没有触碰。

### 偏离规格
- logx 没有 WARN 级别。修订稿要求 no_offsets 打 Warn 日志，实际用 logx.Infof，文本以 'WARN ' 开头，与 store_failure.go 说明的仓库惯例一致。
- ConsumeClaim 内层的排空 select（Messages 通道关闭后等在途任务的那段）也加了 `case <-yieldDone`，返回 nil。规格只列了外层 select，这里是为了让出语义一致：不加的话，排空期间让出会经 failureCh 返回 error，虽然同样结束 session。
- rebalanceGapTracker.sessionStarted 消费 lastEnd，每个结束时刻只计一次。handler 新增 setupDone 字段：只有 Setup 成功的 session 才在 Cleanup 时调用 sessionEnded。这样空窗从上一个真实 session 结束算起，Setup 失败后的重试也算进同一个空窗，进程第一次 Setup 失败不会被误记成再平衡空窗。规格只要求容忍 nil 字段，没有写到这一层。
- 按修订稿 4.2，newGroupHandoff 的签名改为接收工厂 func() (groupInspector, error)，不再接收 inspector 实例。gateDecision 多了 own 字段，用来在放行时初始化 settled。判定逻辑抽成纯函数 decideGate，DescribeGroups 结果的转换抽成纯函数 peerViewFromDescription，两者都便于单测。
- await 每轮先检查 ctx.Err()：ctx 已取消时不碰 Kafka，直接返回 ctx.Err()。
- watchOnce 在查询从成功转为失败时打一行 Info（watchFailing 标记），持续失败不刷屏。规格只写了「出错直接返回」。
- ValidateConsumerGroup 允许「只有 ConsumerGroup、没有 GroupHandoffFrom」。规格的拒绝清单里没有这种组合，它等价于最终形状，生成器也不会写出它，所以没有额外收紧。
- newGroupHandoffFromConfig 出错时，构造函数的报错文本是 'group handoff config: %w'，规格没有指定这段文案。
- 测试比规格多了 5 个，见 files 说明。metrics_test 没有 import client_model（它在 go.mod 里是 indirect），改用 families 下标索引。
- 原规格列给 PR-B 的 deploy/k8s/db-task-retry-alerts.yaml、docs/design/db-consumer-group-per-zone.md、docs/README.md 按本批纪律都没有改，要点见 batch3_handoff。

### 复核 / 修订意见的处理
- [fixed] [high] F5：回档目标镜像早于 Phase A 时，要到 PITR、FLUSHDB 之后才因缺 GroupID 拒启 —— Go 侧按修订稿 R1 实现：GroupID 改为 optional，作为最终形状键，任何版本的二进制都能加载；交接形状由 ValidateConsumerGroup 强制只写 ConsumerGroup 加 GroupHandoffFrom。用例 TestPreHandoffBinaryLoadsFinalZoneShape 和 TestPreHandoffBinaryRefusesZoneShapedConfig 钉住两种行为。回档与 k8s_deploy 渲染最终形状属于脚本阶段（PR-C），不在本阶段。
- [fixed] [high] inspector 用 NewClusterAdmin 建出的 admin 永不刷新 coordinator，seed 每次 Setup 都查，失败会把消费卡到 Pod 重启 —— 按修订稿 R2 实现：withInspector 在任一次查询出错后 Close 并置 nil，下次用工厂重建（新 client 会重新 FindCoordinator）；inspector 改为 sarama.Client 加 NewClusterAdminFromClient。seed 用 settled 缓存，已落定的分区不碰 admin；刚播种的分区不进缓存。用例 TestGroupHandoffInspector_RebuiltAfterError 与 TestGroupHandoffSeed_SkipsSettledPartitions 覆盖。没有采纳「只在第一个 session 查」，理由同修订稿：播种后崩在提交之前会从 Oldest 全量重放。
- [not_in_scope] [high] F2：反向交接前目标组仍有位点时，脚本应当 throw —— 属于 k8s_deploy.ps1（PR-C 或第 3 批）。Go 侧保留运行时 R7（wait_own_behind_peer）作纵深防御，并有单测。
- [fixed] [medium] 空分区永远没有已提交位点，影响 FinalizeHandoff 前置检查、判定②、seed 与 merge_zone —— Go 侧按修订稿 R4 处理：evaluate 不再依赖「本组完整」（②改为一律等待）；seed 对两组都没有位点的分区计 no_offsets 并放进 settled，之后不再查询；没有在 ConsumeClaim 里加 MarkOffset(InitialOffset)。FinalizeHandoff 和 merge_zone 部分不在本阶段。
- [fixed] [medium] ② peer_unreachable_own_complete 排在 R7 之前，对方查询失败时 fail-open —— 按修订稿 R5 删除该 outcome，对方查询失败一律记 wait_peer_unavailable。指标枚举、预建序列与常量同步改成 8 个值；TestGroupHandoffGate_AdminErrors 和 metrics_test 都已改为新口径。
- [not_in_scope] [medium] F4：全新 namespace 时应以 Kafka 事实判定组归属 —— 属于 k8s_deploy.ps1 的决策表（修订稿 §2、§3），不在本阶段。
- [not_in_scope] [medium] 回档 Step 5 重置后应 describe 复核，复核通过才 retire —— 属于 kafka_offset_reset.ps1 与 k8s_zone_rollback.ps1（修订稿 R7），不在本阶段。
- [not_in_scope] [medium] dev_tools 的 kafka-offset-reset 分支用数组 splatting，参数按位置绑定 —— 属于 dev_tools.ps1（修订稿 R8），不在本阶段。
- [fixed] [medium] 规格现状与仓库不符、行号漂移、漏报与其它会话的重叠 —— 全部按函数名或锚点文本定位；每改一个文件前先 git diff 并重读目标段。ConsumeClaim 里 S3 的 observeProducerPartition 那一行和 db_task_producer_partition_total 原样保留，S2 的指标与 MustRegister 追加在它们之后。login config.go 里另一会话在途的改动没有触碰。
- [fixed] [medium] 本地单 zone 默认启动用的是裸组名 —— 本阶段只涉及 db.yaml 注释：已写明不带 -Zone 时用裸名、-Zone N 时追加 _zN。stress_summarize 与文档的修改不在本阶段。TestEtcYamlConsumerGroupIsFinalShape 钉住 db.yaml 的裸名基名。
- [fixed] [low] 重复造轮子：kafkautil 已有位点读取 —— 按修订稿 R11 抽出 kafkautil.CommittedOffsets 与 CommittedOffsetsReader，computeGroupLag 改为调用它且行为不变；saramaGroupInspector.CommittedOffsets 委托它，ErrGroupIDNotFound 映射为全部 -1。GroupLagProber 本体没有复用（它没有 DescribeConsumerGroups，版本是 V3_0_0_0），只复用了它的建链口径。
- [fixed] [low] evaluate ⑥ 把 State 空串当成 peer_released —— 按修订稿 R12：只有 Empty、Dead（包括 GROUP_ID_NOT_FOUND 映射来的）和「Stable 且不持有」才放行；PreparingRebalance、CompletingRebalance 记 wait_peer_rebalancing；空串和其它未知状态记 wait_peer_state_unknown，打 Error 日志并带上原始 State。用例 TestGroupHandoffGate_PeerStateUnknownWaits 覆盖。
- [not_in_scope] [low] 钉版不改变现网版本只有一条依据 —— V0 核对与 PR-A 钉版不在本阶段。
- [not_in_scope] [low] 文档同步有遗漏 —— docs 统一留给第 3 批，要点见 batch3_handoff。
- [fixed] [low] 「组不存在」只认一种文案；Setup 总建 yieldCtx 与 NoHandoff 单测矛盾 —— 第二点已修：只有 handoff 非空时 Setup 才取消旧的、新建 yieldCtx 并 setSessionYield；handoff 为 nil 时 sessionScope 返回 session.Context() 和 nil channel，TestConsumerSetup_NoHandoffKeepsLegacyBehavior 钉住。第一点（CLI 文案白名单）在 PowerShell lib 与 merge_zone，不在本阶段；Go 侧 inspector 走 sarama 的错误码，不依赖文本。

### markApplied 说明
markAppliedSeq 与 markAppliedOwnerEpoch 的调用位置、相对顺序以及它们之前的步骤都没有动，包括 ordering 守卫、owner_epoch 守卫、选库、processDBTask、orderingLock.verify。handleTask、processTaskBatch、runDBOp、deferFailedTask 和重试相关函数一律未改；dbOpHandler 签名仍是 func(ctx, store, task, msg) error。唯一相关的变化在 ConsumeClaim：有交接闸（handoff 非 nil）时，claimCtx 的父 ctx 从 session.Context() 改为派生自它的 yieldCtx。claimCtx 会作为 task.claimCtx 传入，在 handleTask 里当 processCtx 用。因此守望让出时，在途任务的处理方式与 session 结束时完全一样：入队前的检查直接 failKafkaTask，执行中的 DB 调用被取消后进 deferFailedTask，都走不到 markAppliedSeq。最终形状（handoff 为 nil）下，claimCtx 的父 ctx 仍是 session.Context()，与改动前逐字等价。

### 遗留事项
- go/db/db.go 不在本阶段的文件清单。目前消费组校验在 NewKeyOrderedKafkaConsumer 里做，启动时仍然会在开始消费前拒启。如果想更早失败，可以在 db.go 的 Normalize 之后立即调 ValidateConsumerGroup，需另行授权。
- go/login/etc/login.yaml、deploy/login-stack.linux/login.yaml 和 k8s_deploy 的 login 模板里的 GroupID 本轮没删（Step 7）；k8s_deploy 的 login 模板注释属于脚本阶段。
- PR-C 落地之前，K8s 的 db ConfigMap 仍由 k8s_deploy 写 `    GroupID: "db_rpc_consumer_group"`，对新二进制就是 final(L)：没有 handoff、不连 admin。相比旧二进制只多了 ClientID=db-z<zone>-<host> 和三个与默认值相同的显式超时。
- V3 项需要在 Kafka 4.3.1 上实测：sarama DescribeGroups 对不存在的组返回 Dead+ErrorCode 0 还是 GROUP_ID_NOT_FOUND（两种代码都按 Dead 处理）；OffsetFetch 对不存在的组返回全 -1 还是顶层 GROUP_ID_NOT_FOUND（两种都按全 -1 处理）。
- go-zero 的两项行为（缺必填键时报错文本含 GroupID、忽略未知键）由 config_test 的 PreHandoff* 用例钉住，要靠 Codex 跑一遍确认。
- 本阶段没有改任何 .ps1，所以没做 PowerShell 语法解析。db.yaml 的 GroupID 行形状不变，go_services.ps1 的正则、dev_tools.ps1 Get-MergeDbKafka 的正则和 release_common 的 Get-YamlScalar 都按源码核对过仍能匹配，建议 Codex 跑脚本契约测试作回归。
- Java 版不适用：Java 版没有 Kafka 存盘链。PARITY.md 由主会话登记。

### 留给第 3 批
- 指标名（go/db，subsystem=db，都不带 player_id）：db_consumer_group_rebalance_gap_seconds（histogram，buckets 0.5,1,2,3,5,10,15,20,30,45,60,120；从 session Cleanup 到下一次 Setup 完成，含交接闸等待和 Setup 失败的重试，进程第一个 session 不记）；db_consumer_group_handoff_total{result=seeded|own_present|no_offsets|aborted}；db_consumer_group_handoff_gate_total{outcome=no_peer_offsets|peer_released|wait_peer_holding|wait_peer_rebalancing|wait_peer_state_unknown|wait_own_behind_peer|wait_peer_unavailable|wait_own_unavailable}（peer_unreachable_own_complete 已删除）；db_consumer_group_handoff_waiting_seconds（gauge）；db_consumer_group_handoff_wait_seconds（histogram，buckets 0.5,1,2,5,10,15,30,45,60,120,300,600,1800）；db_consumer_group_handoff_conflict_total。result、outcome 和 conflict 都以 0 预建。
- deploy/k8s/db-task-retry-alerts.yaml：文件头的指标来源补上 consumer_group_* 指标；新增 group db-task.consumer-group-handoff，interval 30s，5 条规则。① DbConsumerGroupHandoffWaiting：max by (namespace)(db_consumer_group_handoff_waiting_seconds) > 45，warning；描述写明本 zone 存盘消费暂停，查对方组是否还有本 zone 的旧 Pod、是否在反复再平衡，切组期间冻结其它 zone 的发布。② DbConsumerGroupHandoffStuck：同一表达式 > 300，critical；描述提醒 db_task 保留期 24h。③ DbConsumerGroupHandoffOwnBehindPeer：sum by (namespace)(increase(db_consumer_group_handoff_gate_total{outcome="wait_own_behind_peer"}[5m])) > 0，critical；处置是用 kafka_offset_reset.ps1 删除陈旧一方在本 topic 上的位点，不要手改游标。④ DbConsumerGroupHandoffConflict：sum by (namespace)(increase(db_consumer_group_handoff_conflict_total[10m])) > 0，critical；多半是旧发布物被误部署，先处理掉那个 Pod。⑤ DbConsumerGroupHandoffSeedAborted：sum by (namespace)(increase(db_consumer_group_handoff_total{result="aborted"}[5m])) > 0，warning；播种查询失败会让本 zone 组反复再平衡，查 Kafka coordinator 和 legacy 组是否可读。no_offsets 不设告警。
- ConfigMap 键（go-svc-db-config 的 db.yaml，路径 ServerConfig.Kafka.*）：最终形状 final(G) 只写 `    GroupID: "G"`，G 为 db_rpc_consumer_group 或 db_rpc_consumer_group_z<ZoneId>；交接形状 handoff(X←Y) 写 `    ConsumerGroup: "X"` 加 `    GroupHandoffFrom: "Y"`，不写 GroupID。超时键 SessionTimeoutMs、HeartbeatIntervalMs、RebalanceTimeoutMs 不渲染（默认 10000/3000/60000，约束 heartbeat×3 ≤ session、rebalance ≥ 15000）。二进制自身的拒绝文案（英文）：'Kafka.GroupID or Kafka.ConsumerGroup must be set'、'set only one of Kafka.GroupID and Kafka.ConsumerGroup'、'Kafka.GroupHandoffFrom requires Kafka.ConsumerGroup (without Kafka.GroupID) so that binaries without the handoff gate refuse to start'、'Kafka.GroupHandoffFrom must differ from the consumer group'；启动时包成 'invalid Kafka consumer group config: …'。
- docs/design/db-consumer-group-per-zone.md 需要写进运行时语义的要点：交接闸只在本进程第一次入组前判定（released），之后的再平衡不再看对方，守望让出时复位；evaluate 8 档的顺序与 decideGate 注释一致；seed 的 settled 缓存规则（放行时本组 ≥0 的分区、Setup 查到 ≥0 的分区、两组都 -1 的分区进缓存；刚播种的分区不进，下一次 Setup 再查一次本组）；inspector 任何查询出错就 Close，下次重建（不做 RefreshCoordinator 重试）；守望每 5s 一次，要求对方 Stable、持有本 topic 分区、相邻两次观测之间位点前进，三者同时成立才让出；让出是取消 handler 的 yieldCtx（派生自 session ctx），ConsumeClaim 返回 nil，sarama 结束本 session，消费循环下一轮重新过闸；sarama ClientID 为 db-z<zone>-<host>，inspector 再加 -handoff 后缀；Offsets.Initial 保持 OffsetOldest；logx 没有 WARN 级别，no_offsets 用 Info 加 'WARN ' 前缀。
- docs/design/db-consumer-group-per-zone.md §8 Step 7：login 的 KafkaConfig.GroupID 本轮已改为 optional 并注明废弃（login 代码不读）。删除顺序是：全部 login Pod 跑上 optional 版本至少 7 天后，再从 go/login/etc/login.yaml、k8s_deploy.ps1 的 login 模板、deploy/login-stack.linux/login.yaml 三处删除。
- docs/README.md 在「玩家数据与存储」分类下、db-service-root-credentials.md 之前插入一行：`- [db-consumer-group-per-zone.md](design/db-consumer-group-per-zone.md) — go/db 存盘消费组按 zone 拆分与不停服交接`，必须与 gen_docs_index.py 的生成结果逐字一致。
- PROGRESS.md 条目由主会话写，写明：A2（Go 部分）已落码，未编译，待 Codex 验证；Java 版不适用（无 Kafka 存盘链）。

### 验证命令（交 Codex / 负责人执行）
- 前置：无需清理；不连 Kafka、MySQL 或真实 Redis（Redis 用 miniredis）。工作目录 D:\luyuan\xuanming-server-mmo-cpp\go\shared：`go vet ./kafkautil/` 后 `go test ./kafkautil/ -count=1 -run 'CommittedOffsets|ComputeGroupLag' -v`。通过标准：vet 无输出，TestCommittedOffsets 与 TestComputeGroupLag 全部 PASS。
- 工作目录 D:\luyuan\xuanming-server-mmo-cpp\go\data_service：`go build ./...`（它用 kafkautil.GroupLagProber）。通过标准：退出码 0。
- 工作目录 D:\luyuan\xuanming-server-mmo-cpp\go\db：`go vet ./internal/config/ ./internal/kafka/ ./internal/metrics/` 后 `go build ./...`。通过标准：vet 无输出、build 退出码 0（db 主程序与 cmd/migrate、cmd/verifier、cmd/data_stress 一并编译）。
- 工作目录 D:\luyuan\xuanming-server-mmo-cpp\go\db：`go test ./internal/config/ -count=1 -v`。通过标准：新增的 TestKafkaTimeoutDefaults、TestValidateConsumerGroup、TestEffectiveGroupID、TestPreHandoffBinaryRefusesZoneShapedConfig、TestPreHandoffBinaryLoadsFinalZoneShape、TestPreHandoffBinaryIgnoresUnknownKeys、TestEtcYamlConsumerGroupIsFinalShape，以及原有 TestPlacement*、TestEtcYamlPlacementBlockMatchesDefaults 全部 PASS。PreHandoff* 失败时保留 go-zero 的原始错误文本（这是 F9 的核实证据）。
- 工作目录 D:\luyuan\xuanming-server-mmo-cpp\go\db：`go test ./internal/metrics/ -count=1 -v`。通过标准：TestConsumerGroupHandoffSeriesPrecreated 与 TestSQLPoolCollectorDeclaresDbPools 都 PASS。
- 工作目录 D:\luyuan\xuanming-server-mmo-cpp\go\db：`go test ./internal/kafka/ -count=1 -run 'GroupHandoff|HeldPartitions|PeerView|RebalanceGap|ConsumerClientID|ConsumerSetup|ConsumeClaim|NewKeyOrderedKafkaConsumer|Inspector|Settled|StateUnknown|ReplayAfterSeed' -v`。环境有 cgo 时再加 `-race` 跑一遍。通过标准：全部 PASS，单个用例不超过几秒（等待全部走假时钟）。失败时保留失败用例名和断言输出。
- 工作目录 D:\luyuan\xuanming-server-mmo-cpp\go\db：`go test ./internal/kafka/ -count=1` 跑整包回归。通过标准：全部 PASS，必须保持绿的包括 TestProcessTaskBatch_*、TestOrderingGuard_*、TestClaimAcker_LowerHoleBlocksLaterSuccess、TestConsumerSetupRejectsPartitionsOutsideImmutableContract、TestHandleTask_CanceledClaimCannotReachDB、TestObserveProducerPartition*、TestNewKeyOrderedKafkaConsumer_RequiresPlacementDependencies。之后再跑 `go test ./... -count=1`，退出码应为 0。
- 工作目录 D:\luyuan\xuanming-server-mmo-cpp\go\login：`go build ./...` 后 `go test ./internal/config/ -count=1`。通过标准：退出码 0；带 GroupID 键的 login.yaml 仍能加载。
- 工作目录 D:\luyuan\xuanming-server-mmo-cpp\tools\merge_zone：`go vet ./...` 后 `go build ./...`（它 import go/db 的 config 包）。通过标准：退出码 0。
- 仓库根目录 D:\luyuan\xuanming-server-mmo-cpp，作脚本回归（本阶段没改脚本，只改了 db.yaml 的注释）：`pwsh -File tools/scripts/tests/dev_tools_merge_zone_contract.tests.ps1` 和 `pwsh -File tools/scripts/tests/k8s_deploy_contract.tests.ps1`。通过标准：读 go/db/etc/db.yaml 的 GroupID 为 'db_rpc_consumer_group'、TopicGeneration 为 '1' 的断言仍绿。如果失败，先确认是不是别的会话正在改的用例（k8s_deploy_contract 有在途改动），再报告。

## B1：B1 — S2 PR-A:Kafka 镜像钉版本(apache/kafka:4.3.1@sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837)（状态 done）

### 文件
- `deploy/docker-compose.yml`：kafka 与 kafka-topic-init 两处 image 由 apache/kafka:latest 改为钉版引用 apache/kafka:4.3.1@sha256:77e3df90…6532837。kafka 服务上方加了中文注释,写明三点:为什么钉版(命名卷 kafka-data 下的版本漂移;group coordinator / OffsetDelete 语义随镜像漂移,交接闸与 kafka_offset_reset 收尾依赖它们);2026-10-15 核对时与 latest 同一个 index digest;升级时四处同改(先 imagetools inspect 取 Digest,再本地跑 kafka-topic-init 与 CLI 核对,步骤指向 README「镜像钉版本」)。kafka-topic-init 上方加一行注释,说明与 kafka 服务引用相同。其它行未动。
- `deploy/k8s/manifests/infra/kafka.yaml`：只改容器 kafka 的 image 一段,按规格做了四件事:image 改为钉版引用;紧跟着加 imagePullPolicy: IfNotPresent;上方加注释,说明钉版原因、四处同改、下一次 infra-up 会滚动重启 kafka StatefulSet(单 broker 等于一次 broker 重启,要在无玩家窗口做;多 broker 按 PDB 逐个滚);ENTRYPOINT 核对注释里的 docker image inspect 命令改为钉版引用。PVC、保留期、拓扑等 hunk 未碰(留给 S5 / 第 3 批)。注释里没有 __X__ 形态的占位,不会触发 Apply-Infra 的残留占位门禁。
- `deploy/k8s/manifests/infra/kafka-topic-init.yaml`：image 改为同一个钉版引用(原有的 imagePullPolicy: IfNotPresent 保留)。原注释「与 kafka.yaml 同一个镜像」保留,下面补一行:tag 与 digest 都必须与 kafka.yaml 和 docker-compose.yml 两处相同。
- `deploy/k8s/README.md`：只改「Kafka:StatefulSet + PVC」一节。(1)「文件描述符」一条里的 docker image inspect 命令改为钉版引用,注明换版本时同步换引用。(2)原「### 未改、但值得知道的两件事」的第一条(浮动 tag)改写为新小节「### 镜像钉版本(2026-10-15)」,内容包括:四处钉版清单;为什么钉;钉版不改变运行版本的依据及其局限;修订稿 R13 要求的 V0 核对命令(kubectl 读每个 broker 的 imageID,本机 docker image inspect 看 RepoDigests),不一致时按 broker 升级评估;apply 之后 kafka StatefulSet 会滚动,单 broker 档等于一次 broker 重启、在无玩家窗口做、事后跑 infra-kafka-topics -WaitReady;升级五步(imagetools 取 index digest、四处同改、compose 本地验证、kafka-consumer-groups.sh 行为核对、跨大版本先读升级说明)。(3)原小节标题改为「### 未改、但值得知道的一件事」,只剩堆大小那一条。文件里别的会话未提交的「C++ 节点的 zone Redis 口令」一节(+36 行)未碰。

### 偏离规格
- README 结构有一处偏离:规格要求把「未改、但值得知道的两件事」的第一条原地改写。但改写后这条已经不是「未改」,标题会失实,所以拆成独立小节「### 镜像钉版本(2026-10-15)」,原标题改为「…一件事」。已 git grep 核对,全仓没有引用这个小节标题的地方。
- README 升级步骤第 4 步指向 docs/design/db-consumer-group-per-zone.md §11,即 4.3.1 上 CLI 行为原始输出的记录处。该文档目前还不存在,按本批纪律 docs/ 留给第 3 批创建。在那之前这是一个悬空引用,已列进 batch3_handoff。
- 规格 PR-A 原本是 5 个文件,本阶段只改其中 4 个。docs/ops/kafka-cluster-production-runbook.md 按任务说明归第 3 批,未改。
- V0 的 kubectl 命令照修订稿 R13 写死了 -n mmorpg-infra,另加括注「-n 换成实际的 -InfraNamespace」,因为 README 其它段落的 namespace 也是可配的。
- docker-compose.yml 里其它浮动 tag(provectuslabs/kafka-ui:latest、redis:latest、mysql:latest、nacos、bitnamilegacy/etcd:latest)不在规格「四处」之列,没有动。

### 复核 / 修订意见的处理
- [fixed] 复核 [low] R13:「钉版不改变现网版本」只有 Docker Hub tag 对比一条依据,没有核对正在运行的 broker 和本机缓存的镜像 —— 按修订稿 R13 落地:README「镜像钉版本」小节写入 V0 两条核对命令(kubectl 读 imageID、docker image inspect 看 RepoDigests),写明「这只是 registry 上的对比」这一局限,以及不一致时的处置:按 broker 版本升级评估 metadata.version、日志目录兼容、能否回退,由负责人决定是否另开窗口;本机命名卷可按 AGENTS §6.2 清掉。设计文档 §8 步骤 0 的同步留给第 3 批。
- [fixed] 负责人拍板:钉 tag@digest,而不是只钉 tag —— 四处都用 apache/kafka:4.3.1@sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837。已核对仓库现有约定:Dockerfile.* 的基础镜像都用 tag@sha256。tools/scripts/lib/release_common.ps1 的 Get-ImageTagFromRef 把 @digest 判为不可变引用,与 imagePullPolicy: IfNotPresent 的推导一致。
- [not_in_scope] 复核其余条目(F5 回档镜像能力、inspector coordinator 刷新、F2 反向预检、空分区位点、② 跳过 R7、F4 组归属持久化、回档 Step 5 复核、dev_tools splatting、本地裸组名、DRY 复用 kafkautil、空串 State、文档遗漏、组不存在文案) —— 都属于 PR-B(go/db)、PR-C(脚本工具)或 PR-D(文档),不归本阶段 B1(PR-A 钉版)负责,由对应阶段按修订稿处理。

### markApplied 说明
本阶段只改了 deploy/ 下的 YAML 和 README,没碰任何 go/db 代码。markAppliedSeq / markAppliedOwnerEpoch 的调用顺序与位置、它们之前的步骤、dbOpHandler 签名都没有变化。

### 遗留事项
- 全部未编译、未测试,待 Codex 验证。这次没有拉镜像,也没有连集群。V3 的 docker pull 属于下载:负责人已同意先用 imagetools 只读核对,再拉这一个镜像做 CLI 核对,执行前请再确认一次。
- V0 是上线前的核对项(不阻塞落码),由负责人或 Codex 执行。集群 broker 的 imageID 或本机缓存的 latest 只要有一处不等于 sha256:77e3…,PR-A 的发布就按一次 broker 版本升级评估。
- 合入 PR-A 后,下一次 infra-up 会因为镜像字符串变化滚动重启 kafka StatefulSet。单 broker 档要在无玩家窗口执行,完成后跑 infra-kafka-topics -WaitReady。
- deploy/k8s/AGENTS.md:86 提到「改镜像版本要核 entrypoint」,可以补一句「四处钉版同改,见 README『镜像钉版本』」。该文件不在本阶段清单内,而且工作区里有别的会话未提交的改动,所以没碰。
- 工作区里 deploy/k8s/AGENTS.md、deploy/k8s/manifests/java-svc/gateway.yaml,以及 README 的 zone Redis 口令一节,都是别的会话的在途改动,本阶段没有触碰。

### 留给第 3 批
- docs/ops/kafka-cluster-production-runbook.md(原 PR-A 文件):两处 apache/kafka:latest 改为 apache/kafka:4.3.1@sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837,锚点是 compose 片段里的 `image: apache/kafka:latest` 和 docker run 片段里的 `apache/kafka:latest \`。Step 4 的 kafka-mirror-maker.sh 示例上方加注:Kafka 4.0 已移除 MirrorMaker 1,4.x 没有该脚本,改用 MM2(connect-mirror-maker.sh <mm2.properties>),示例未验证;流程不重写。
- docs/design/db-consumer-group-per-zone.md 必须有 §11「Kafka CLI 行为核对(4.3.1)」,因为 deploy/k8s/README.md「镜像钉版本」的升级步骤第 4 步已指向这里。§11 要列出 V3(a)–(j) 的原始输出与退出码:Stable 组 --reset-offsets --execute;describe 不存在的组(does not exist 还是 GroupIdNotFoundException);不存在的组 --reset-offsets --to-datetime 能否建出位点;Stable 组无人订阅该 topic 时 --delete-offsets;有人订阅时的 GROUP_SUBSCRIBED_TO_TOPIC;--execute 是否打印 NEW-OFFSET 表;kafka-get-offsets.sh --time -2/-1 的格式;Empty 组无位点时 describe 的输出;kind 里 kubectl exec kafka-0 能否跑 kafka-consumer-groups.sh。§0 写「前置是 Kafka 钉版(已落)」。§8 步骤 0 写「先做 V0,再合入 PR-A」,并附 V0 的两条命令(与 README 相同)。
- docs/design/no-single-node-horizontal-scaling-20261001.md:74 的浮动 tag 那一条之后追加更正行:2026-10-15 已钉 apache/kafka:4.3.1@sha256:77e3…,见 deploy/k8s/README.md「镜像钉版本」。只追加,不改原文。
- docs/design/db-task-pipeline-bottlenecks.md:198(第 0 批里的「Kafka 镜像钉版本(未做)」)与 §6.3 第 0 批,按 S2 规格改为已做并附 digest。docs/handoff/godb-pipeline-handoff-20261013.md:203 表里的「Kafka 镜像钉版本」一行可以补状态。
- 历史文档 docs/design/id-routing-session-status-20260909.md:94 与 docs/design/node-id-overhaul-changelog-20260908.md:146 仍写着浮动 tag 是风险,可以各追加一行「2026-10-15 已钉版」(可选,只追加)。
- docs/PROGRESS.md(主会话写):追加一条「S2 PR-A Kafka 镜像钉版已落码(compose 两处、kafka.yaml 加 imagePullPolicy: IfNotPresent、kafka-topic-init.yaml、deploy/k8s/README.md 新增『镜像钉版本』小节,含 V0 核对命令),未编译/未验证,待 Codex 执行 V0/V3」。PROGRESS.md:7307 那条「未做」不回头改,另起新条目说明。
- Java 仓库 PARITY.md:PR-A 只是部署侧的基础设施镜像,不是客户端契约。登记时注明 Java 版是否有自己的 Kafka compose / manifest 也在用 apache/kafka:latest,有的话同批钉到同一个 digest,否则写「不适用」。
- PR-A 没有新增指标、告警表达式或 ConfigMap 键。第 3 批的 deploy/k8s/manifests/go-svc/db.yaml 和 *-alerts.yaml 与本阶段无关。S5 改 kafka.yaml 的 PVC / 保留期时,与本阶段的 image / imagePullPolicy 行不重叠。

### 验证命令（交 Codex / 负责人执行）
- 静态核对(工作目录 D:\luyuan\xuanming-server-mmo-cpp):git grep -n "image: apache/kafka:latest" -- deploy/docker-compose.yml deploy/k8s/manifests/ —— 通过标准:没有输出。git grep -c "apache/kafka:4.3.1@sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837" -- deploy/docker-compose.yml deploy/k8s/manifests/infra/kafka.yaml deploy/k8s/manifests/infra/kafka-topic-init.yaml —— 通过标准:image 行合计 4 处(compose 2 行;kafka.yaml 2 行,其中 1 行在注释里;topic-init 1 行)。
- compose 语法(工作目录同上):docker compose -f deploy/docker-compose.yml config --quiet —— 通过标准:退出码 0。
- K8s 渲染(工作目录同上):pwsh -NoProfile -NonInteractive -File tools/scripts/k8s_deploy.ps1 -Command infra-up -DryRun -SkipPreflight -SkipGoSvc -SkipJavaSvc -InfraNamespace contract-kafka-pin -NodeImage registry.invalid/test/mmorpg-node:0123456789ab *> $env:TEMP\kafka-pin-dryrun.txt —— 通过标准:退出码 0;输出里 `image: apache/kafka:4.3.1@sha256:77e3df90…6532837` 至少出现 2 次(StatefulSet kafka 与 Job kafka-topic-init),kafka 容器带 `imagePullPolicy: IfNotPresent`,没有「未替换的占位」报错。失败时保留输出前后 40 行。
- 回归(V4 子集,工作目录同上):pwsh -NoProfile -NonInteractive -File tools/scripts/tests/k8s_deploy_contract.tests.ps1 —— 通过标准:退出码 0、没有 FAIL 行,重点看「生成的所有 image: 引用都不得是 latest 等可变 tag」与两条 imagePullPolicy 用例。
- V0(上线前,负责人或 Codex 执行,只读):kubectl get pod -n mmorpg-infra -l app=kafka -o jsonpath='{range .items[*]}{.metadata.name}{" "}{.status.containerStatuses[0].imageID}{"\n"}{end}' —— 通过标准:每个 broker 的 imageID 都含 sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837。本机:docker image inspect apache/kafka:latest --format '{{json .RepoDigests}}' —— 通过标准:含同一个 digest(本机没有该镜像就跳过并记录)。任一不一致都上报负责人,按 broker 升级评估。
- V3 钉版核对(只读部分):docker buildx imagetools inspect apache/kafka:4.3.1 与 docker buildx imagetools inspect apache/kafka:latest —— 通过标准:两条输出的 Digest: 行都等于 sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837。
- V3 钉版核对(需拉镜像,执行前请负责人确认):docker pull apache/kafka:4.3.1@sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837;docker image inspect apache/kafka:4.3.1@sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837 --format '{{json .Config.Entrypoint}} {{json .Config.Cmd}}' —— 通过标准:输出为 ["/__cacert_entrypoint.sh"] ["/etc/kafka/docker/run"]。然后 docker compose -f deploy/docker-compose.yml up -d kafka kafka-topic-init;docker wait kafka-topic-init —— 通过标准:输出 0。docker exec kafka /opt/kafka/bin/kafka-topics.sh --version —— 通过标准:显示 4.3.1。之后按 S2 规格与修订稿 §13 的 V3(a)–(j) 逐条记录 kafka-consumer-groups.sh / kafka-get-offsets.sh 的原始输出与退出码,交第 3 批回填 docs/design/db-consumer-group-per-zone.md §11。注意:本机命名卷 kafka-data 可能是更旧版本写的,按 AGENTS §6.2 先清再起更稳妥。

## B2：B2 —— S2 PR-C 脚本部分(不含 k8s_deploy.ps1):lib/kafka_consumer_groups.ps1、kafka_offset_reset.ps1、k8s_zone_rollback.ps1、dev_tools.ps1、stress_summarize.ps1 参数块及对应测试（状态 done）

### 文件
- `tools/scripts/lib/kafka_consumer_groups.ps1`：新增纯函数库(只定义函数、不读脚本级变量、不调外部命令,dot-source 无副作用)。函数:Get-KafkaGroupMissingPatterns / Test-KafkaGroupMissingText(「组不存在」三条白名单:consumer group '..' does not exist、GroupIdNotFoundException、GROUP_ID_NOT_FOUND,注释指向 tools/merge_zone/preflight.go consumerGroupMissing)、Test-KafkaGroupNoActiveMembersText、ConvertFrom-KafkaConsumerGroupDescribe(按表头列名定位 TOPIC/PARTITION/CURRENT-OFFSET/LOG-END-OFFSET/LAG,只取本 topic,'-' 记 null,重复分区/非整数分区号 throw)、Get-KafkaCommittedOffsetMap、Resolve-KafkaGroupTopicOffsets(missing / no-offsets / offsets,Error 行、退出码非 0、无表头无关键字一律 throw)、ConvertFrom-KafkaResetPlan(NEW-OFFSET 表,缺表头/非数值/本 topic 无行 throw)、ConvertFrom-KafkaGetOffsets(topic:partition:offset)、Test-HandoffFinalizeReady(空分区 earliest==latest 放行、新组缺非空分区/新组<旧组/缺日志边界报问题)、Get-DbTaskTopicName(与 go/db DbTaskTopicForGeneration 同规则)。
- `tools/scripts/kafka_offset_reset.ps1`：dot-source lib;新增 -RetireGroup、-FinalizeHandoff;模式五选一;FinalizeHandoff 必须 -Group/-RetireGroup/-Topic/-Partitions>0;-RetireGroup 必须 -Partitions>0、不能等于 -Group(-ceq)、不能配 -DeleteAndRecreateTopic。新增 Invoke-KafkaToolCaptured、Invoke-KafkaConsumerGroupDescribe、Invoke-KafkaGetOffsets、Get-ResetPlanProblems、Get-ResetVerificationProblems(纯函数)、Remove-RetiredGroupTopicOffsets(旧组 missing/no-offsets 即 nothing to retire;dry-run 只打印;-Apply 执行 --delete-offsets、过 Assert-KafkaToolOutputClean、遇 GROUP_SUBSCRIBED_TO_TOPIC 立即 throw、删后再 describe 复核仍有位点 throw)。重置模式:解析 --dry-run/--execute 的计划表并做 0..N-1 覆盖检查(dry-run 与 -Apply 都查);-Apply 后 describe 逐分区断言 CURRENT-OFFSET==NEW-OFFSET,复核失败的 throw 在删除 RetireGroup 位点之前;复核通过且给了 -RetireGroup 才退役旧组(有意回退,不做 新≥旧 检查)。FinalizeHandoff:describe 两组 + kafka-get-offsets --time -2/-1 → Test-HandoffFinalizeReady 有问题一次列全 throw → Remove-RetiredGroupTopicOffsets,成功后提示以 keep 再跑一次 zone-up。Assert-KafkaToolOutputClean 增加可选 -Consequence(缺省仍是「位点没有重置」);注释改为「K8s 切成每 zone 一组之后……」;.DESCRIPTION/.EXAMPLE 组名改 db_rpc_consumer_group_z101 并新增 FinalizeHandoff / RetireGroup 示例。原有 `& $cgTool @cmdArgs 2>&1` 与 Assert 调用文本保持不变。
- `tools/scripts/k8s_zone_rollback.ps1`：dot-source lib;-KafkaGroup 缺省改 ''、新增 -KafkaLegacyGroup '',删除 -KafkaDrainTimeoutSec;入口用 Get-YamlScalar 读 go/db/etc/db.yaml 的 ServerConfig.Kafka.GroupID(基名)与 PartitionCnt(须正整数),读不到即 throw(中文,尚未产生任何改动);推导 zone 组 <基名>_z<ZoneId> 与 legacy 组 <基名>,两者相同(-ceq)入口即拒。摘要行改为 group=<zone> retire=<legacy> partitions=N。$upArgs 加 DbConsumerGroup='zone'(静态预检与 Step 6 同一份)。新增 Invoke-KafkaGroupDescribe、Show-KafkaTopicLagSnapshot(第 0 步 c:只读快照,只计本 topic,任何失败只 Write-Warning)、Invoke-Step5Rehearsal(第 0 步 d:zone 组有数值位点则跳过,否则 hashtable splatting 调 kafka_offset_reset.ps1 dry-run 预演 -Partitions N,失败/非 0/describe 判不出都在停服之前 throw)。删除 Step 2 整块(留一行注释说明)。Step 5 的 $kArgs 加 RetireGroup=legacy、Partitions=N,调用前清零 LASTEXITCODE。.DESCRIPTION 第 0 步补 c/d、第 2 条改为已删除说明、第 5/6 条按修订稿改写。
- `tools/scripts/dev_tools.ps1`：① 新增 [ValidateSet('','keep','zone','legacy')]$DbConsumerGroup=''(放在 GateRouterMode 之后),Invoke-K8sDeploy 非空才 $args.DbConsumerGroup。② $RollbackKafkaGroup 缺省改 '',新增 $RollbackKafkaLegacyGroup='',删除 $RollbackKafkaDrainTimeoutSec;k8s-zone-rollback 分支 KafkaGroup / KafkaLegacyGroup 只在非空时放进 $rbArgs,去掉 KafkaDrainTimeoutSec。③ 新增 $KafkaRetireGroup=''、[switch]$KafkaFinalizeHandoff;kafka-offset-reset 分支由数组 splatting 改为 hashtable splatting(字符串非空才放、switch 打开才放 $true、Partitions>0、ReplicationFactor≠1),调用前清零 LASTEXITCODE、非 0 即 throw。④ Get-MergeZoneArgs 只在 $MergeKafkaGroup 非空时转发 -kafka-group(Get-MergeDbKafka 仍返回 GroupID 但不再转发),同步改了参数块与 Get-MergeDbKafka、Get-MergeZoneArgs 的注释。未出现 -ConfirmRedisCoreRestart 相关行,未触碰。
- `tools/scripts/stress_summarize.ps1`：只改参数块:param 末尾新增 [int]$Zone=0;-KafkaTopic / -KafkaConsumerGroup 缺省值不变(db_task_zone_1 / 裸名 db_rpc_consumer_group);仅当 $Zone>0 且调用方未显式传($PSBoundParameters.ContainsKey 判断)时推导为 db_task_zone_<Zone> / db_rpc_consumer_group_z<Zone>;.PARAMETER 帮助同步。DB 段与 Kafka 段解析逻辑未动。
- `tools/scripts/tests/kafka_offset_reset_output_guard.tests.ps1`：保留原 7 例;新增 dot-source lib 与 deploy_capture,按 AST 取出 kafka_offset_reset.ps1 的 6 个新函数。新增用例:describe 按表头解析/列序打乱/只取本 topic/'-' 为 null/GroupMissing;Resolve-KafkaGroupTopicOffsets 的 missing 三种文案、no-offsets(无表头+has no active members、只有 '-'、只在别的 topic)、offsets,以及 Error 行/退出码非 0/无表头无关键字/无输出 一律 throw;ConvertFrom-KafkaResetPlan、ConvertFrom-KafkaGetOffsets 正常与失败输入;Test-HandoffFinalizeReady 四类;Get-DbTaskTopicName;计划覆盖与 -Apply 后复核;源码顺序断言(重置段落里「重置后复核失败」早于 Remove-RetiredGroupTopicOffsets 调用);Remove-RetiredGroupTopicOffsets 用假 CLI 函数覆盖 nothing-to-retire、dry-run、删后复核成功/仍有位点 throw/删除输出 Error 行 throw;子进程参数校验(FinalizeHandoff 互斥与缺参、RetireGroup 缺 Partitions/等于 Group/配 DeleteAndRecreateTopic)。
- `tools/scripts/tests/k8s_zone_rollback_gate_router_mode.tests.ps1`：沙箱改为 <tmp>/<guid>/tools/scripts(拷入 lib/kafka_consumer_groups.ps1)并拷仓库 go/db/etc/db.yaml 到 <tmp>/<guid>/go/db/etc/(可用 -DbYamlContent 替换或 -OmitDbYaml 省略);假 dev_tools 参数加 $DbConsumerGroup;$rollbackArgs 去掉 KafkaDrainTimeoutSec;假 kafka_offset_reset.ps1 改为声明全部参数、关位置绑定、每次调用记一行具名参数(KafkaResetBound),-KafkaResetDryRunExit 控制预演退出码;假 kafka-consumer-groups.sh 支持 -KafkaCliOutput(Lines/ExitCode 或 Throw),缺省对每个组回「组不存在」并记录调用(KafkaCliCalls)。新增 10 例:db.yaml 前提、-Apply 全流程(Step 6/静态预检含 DbConsumerGroup=zone,Step 5 含 zone 组/RetireGroup/Partitions/Apply,无 Step 2)、zone 组不存在时先预演、预演失败停服前拒绝、zone 组有位点跳过预演且快照合计只含本 topic、CLI 不可用时快照告警而预演拒绝、-KafkaGroup 等于 legacy 入口即拒、组名与分区数取自 db.yaml(含读不到/PartitionCnt=0 拒绝)、dry-run 不读 Kafka、子进程 dry-run 打印内容、dev_tools 回滚入口消费组留空推导/显式转发。
- `tools/scripts/tests/dev_tools_merge_zone_contract.tests.ps1`：原「Kafka 世代号取 db.yaml」用例改名并改断言:-kafka-topic-generation 仍为 '3',-kafka-group 缺省不出现;显式 $MergeKafkaGroup='custom_group' 时 merge-zone 与 merge-zone-audit 都转发。新增源码断言:k8s-zone-rollback 分支不含 KafkaDrainTimeoutSec、KafkaGroup/KafkaLegacyGroup 仅非空转发、-RollbackKafkaGroup 缺省 ''、param 块有 RollbackKafkaLegacyGroup 无 RollbackKafkaDrainTimeoutSec;kafka-offset-reset 分支为 hashtable splatting 且转发 RetireGroup/FinalizeHandoff;-DbConsumerGroup 的 ValidateSet/缺省值与 Invoke-K8sDeploy 的非空透传。
- `tools/scripts/tests/dev_tools_kafka_offset_reset.tests.ps1`：新增。夹具同 dev_tools_proto_gen_exit.tests.ps1(只拷 dev_tools.ps1 与 lib/release_common.ps1,显式 -ImageTag/-GoSvcTag/-JavaSvcTag/-NodeImage),旁放假 kafka_offset_reset.ps1(与真脚本同名同类型参数、关位置绑定、逐行记录 $PSBoundParameters,环境变量控制退出码)。子进程真跑 dev_tools -Command kafka-offset-reset 5 例:交接收尾全部参数按名绑定、按时间重置+RetireGroup 不带 Apply、删 topic 重建(ReplicationFactor/KafkaBin)、缺省值不透传、下游退出 7 时入口失败且带 exit code 7。

### 偏离规格
- Test-HandoffFinalizeReady 按修订稿 §6 放进 lib(原规格放在 kafka_offset_reset.ps1);签名按修订稿加 -Earliest/-Latest,测试改为 dot-source lib 直接调用。
- kafka_offset_reset.ps1 额外新增 Invoke-KafkaToolCaptured、Invoke-KafkaGetOffsets、Get-ResetPlanProblems、Get-ResetVerificationProblems 四个函数(规格未点名):把计划覆盖与 -Apply 后复核做成纯函数,便于按 AST 取出单测;原规格的 Invoke-KafkaConsumerGroupDescribe 改为统一走 lib 的 Resolve-KafkaGroupTopicOffsets(修订稿 §6 的分类规则)。
- Assert-KafkaToolOutputClean 增加可选 -Consequence 参数(缺省仍为「位点没有重置」,原用例文本不变),describe/get-offsets/delete-offsets 复用同一道判定时错误文本不再误称「位点没有重置」。
- fail-closed 加严两处:ConvertFrom-KafkaResetPlan 在本 topic 一行都没有时也 throw(不依赖是否给了 -Partitions);Get-ResetPlanProblems 对计划里 ≥N 的多余分区也报问题(分区数与 db.yaml 漂移)。Remove-RetiredGroupTopicOffsets 在 --delete-offsets 输出含 GROUP_SUBSCRIBED_TO_TOPIC 时立即 throw,再加删后复核。
- dev_tools kafka-offset-reset 分支在 hashtable splatting 之外,加了调用前清零 LASTEXITCODE、非 0 即 throw(规格未写):被调脚本 DeleteAndRecreateTopic / FinalizeHandoff 以 exit 结束,不传播退出码时失败会被吞成成功;新测试的「下游失败」用例依赖它。k8s_zone_rollback Step 5 调用前同样清零 LASTEXITCODE。
- 回滚测试里假 kafka-consumer-groups.sh 的缺省回应由原规格的「无输出」改为「组不存在」:修订稿新增的第 0 步 d 对无输出会判为读不到而在停服前拒绝,原有全部 -Apply 放行用例会因此变红;「组不存在」正是从未切过组的 zone 的常态,并会走一次预演。
- 原规格的 $DbConsumerGroupBase 常量及其 AST 用例不再存在(修订稿 §8 改为入口现读 db.yaml);改用自定义 db.yaml(GroupID custom_db_group、PartitionCnt 4)断言 Step 5 参数随之变化,并覆盖 db.yaml 缺失/PartitionCnt=0 时拒绝。
- stress_summarize.ps1 的 -Zone 加在 param 块末尾而不是开头,避免改变既有参数的位置绑定顺序。
- k8s_zone_rollback 第 0 步 c/d 与被删除的 Step 2 一样直接调用 PATH 上的 kafka-consumer-groups.sh(本脚本没有 -KafkaBin);第 0 步 d 的预演通过 kafka_offset_reset.ps1(其自身用 Resolve-KafkaTool)。
- 已知局限(未改,记入 open_items):zone 组在本 topic 上 no-offsets 且仍有活跃成员时(例如 final(Z) 下从未写入过的 zone),第 0 步 d 的 dry-run 重置会被 Kafka 以组非 Empty 拒绝,回档在停服前被拒;属于零写入 zone 的边角情形。

### 复核 / 修订意见的处理
- [fixed] [high] F5:回档目标镜像早于 Phase A 时 Step 6 CrashLoop —— 按修订稿 R1:回档脚本 Step 6 显式 -DbConsumerGroup zone,Step 5 收尾后 zone 组有完整位点、legacy 组无位点,由 k8s_deploy 决策表落到 final(Z)(GroupID 形状,任何镜像可起);本阶段另加第 0 步 d 预演 Step 5,把「Kafka 建不出 zone 组位点」提前到停服前。k8s_deploy 侧的决策表与 final 形状渲染属第 3 批。
- [not_in_scope] [high] inspector 不刷新 coordinator、seed 每次 Setup 查 admin —— go/db PR-B(group_handoff.go),非本阶段文件。
- [not_in_scope] [high] F2:反向交接前目标组仍有位点应由脚本 throw —— 落在 k8s_deploy.ps1 的 Read-DbGroupTopicFacts / 决策表(第 3 批);本阶段已在 lib 提供其依赖的 Resolve-KafkaGroupTopicOffsets 与 Get-DbTaskTopicName。
- [fixed] [medium] 空分区永远没有已提交位点 —— 脚本侧按修订稿 R4(a):FinalizeHandoff 用 kafka-get-offsets --time -2/-1 取日志边界,Test-HandoffFinalizeReady 对 earliest==latest 的空分区放行、get-offsets 缺分区报问题;go/db seed/evaluate 部分不在本阶段。
- [not_in_scope] [medium] ② peer_unreachable_own_complete 排在 R7 之前 —— go/db evaluate 判定顺序,PR-B。
- [not_in_scope] [medium] F4:全新 namespace 不以 Kafka 判定组归属 —— k8s_deploy.ps1 keep 模式与 fresh-namespace 规则,第 3 批。
- [fixed] [medium] 回档 Step 5 重置后未复核就删 legacy 位点;zone 组不存在时能否建位点未核实 —— kafka_offset_reset 重置模式解析计划表并做 0..N-1 覆盖检查,-Apply 后 describe 逐分区比对 NEW-OFFSET,复核失败的 throw 在 Remove-RetiredGroupTopicOffsets 之前(有源码顺序断言);回档第 0 步 d 在停服前预演 Step 5;V3(c) 实测交 Codex。
- [fixed] [medium] dev_tools kafka-offset-reset 数组 splatting 现在就不可用 —— 改为 hashtable splatting,新增 -KafkaRetireGroup / -KafkaFinalizeHandoff;新增 dev_tools_kafka_offset_reset.tests.ps1 真跑入口,用关闭位置绑定的假下游钉住按名绑定与退出码传播。
- [fixed] [medium] 现状与行号漂移、漏报与其它会话的重叠 —— 本阶段一律按函数名/锚点定位;每个文件改前 git diff 确认无在途改动;dev_tools 未见 -ConfirmRedisCoreRestart 行;S3 的两处在 go/db,本阶段未触碰。
- [fixed] [medium] 本地单 zone 默认启动是裸名组,stress_summarize 改 _z1 会查空组 —— 按修订稿 R10:-Zone 缺省 0,缺省 topic/组保持 db_task_zone_1 / 裸名,只有 Zone>0 且未显式传参时推导 _z<Zone>。
- [not_in_scope] [low] 重复造轮子(kafkautil 位点读取) —— Go 侧(go/shared/kafkautil 与 go/db inspector),PR-B。
- [not_in_scope] [low] evaluate ⑥ 把空串当 peer_released —— go/db,PR-B。
- [not_in_scope] [low] 钉版不改变现网版本的依据不足 —— PR-A / V0 核对,与脚本无关。
- [not_in_scope] [low] 文档同步遗漏 —— docs/ 一律留给第 3 批,要点已列入 batch3_handoff。
- [fixed] [low] 「组不存在」只认一种文案;Setup yieldCtx 与单测矛盾 —— PowerShell 侧按修订稿 R15 用三条白名单正则,无表头时仅 has no active members 判 no-offsets,其余 fail-closed,并写成测试 fixture;Go 侧(merge_zone 同步白名单、Setup yieldCtx)不在本阶段。

### markApplied 说明
本阶段只改 PowerShell 脚本与 PowerShell 测试,没有触碰 go/db 的任何文件。markAppliedSeq / markAppliedOwnerEpoch 的调用顺序与位置、它们之前的步骤都没有变化;dbOpHandler 签名仍是 func(ctx, store, task, msg) error。

### 遗留事项
- 跨批依赖(重要):dev_tools::Invoke-K8sDeploy 新增的 splat 键 DbConsumerGroup 与 k8s_zone_rollback 的 $upArgs.DbConsumerGroup='zone',都要求 k8s_deploy.ps1 声明 -DbConsumerGroup(第 3 批)。在第 3 批落地前:(1) tools/scripts/tests/k8s_client_entry_contract.tests.ps1 的「AST 契约:dev_tools.ps1::Invoke-K8sDeploy → k8s_deploy.ps1 每个 splat 键都是下游声明过的参数」会红;(2) 回档 -Apply 会在第 0 步 b 静态预检被拒(停服前、fail-closed)。第 2、3 批同属 PR-C,必须一起发布。
- k8s_client_entry_contract.tests.ps1 不是本阶段文件(有别的会话在改):第 3 批给 k8s_deploy 加参数后,需把 'DbConsumerGroup' 加进该文件 WrapperPairs 里 dev_tools::Invoke-K8sDeploy 的 Forwarded 清单。
- tools/merge_zone(main.go / preflight.go / merge_run.go / unmerge.go / merge_unit_test.go / integration_test.go)不在本阶段:dev_tools 已不再缺省转发 -kafka-group;在 merge_zone 改完之前工具缺省组仍是 db_rpc_consumer_group,与原先从 db.yaml 转发的值相同,行为不变。merge_zone 侧实现 consumerGroupMissing 时请复用 lib 的三条白名单正则并在注释里指回 tools/scripts/lib/kafka_consumer_groups.ps1。
- 第 0 步 d 的已知局限:zone 组在本 topic 上没有已提交位点、却仍有活跃成员时(如 final(Z) 下从未写入过的 zone),dry-run 重置会被 Kafka 以组非 Empty 拒绝,回档在停服前被拒。属零写入 zone 的边角情形,未处理;如需放行,得让 describe 结果带出成员信息,再决定跳过预演。
- Kafka CLI 文本(不存在组的 describe、Empty 组无位点、--reset-offsets --execute 是否打印 NEW-OFFSET 表、kafka-get-offsets 输出格式、--delete-offsets 成功与 GROUP_SUBSCRIBED_TO_TOPIC 的输出)在 apache/kafka:4.3.1 上都没有实测(修订稿 V3 (b)(c)(d)(e)(f)(g)(h)(i)),脚本按推定文案 fail-closed;实测原文要回填 lib 白名单与 kafka_offset_reset_output_guard 的 fixture。若 V3(c) 证实不能为不存在的组建出位点,k8s_zone_rollback 改动不得发布,交负责人决策。
- AGENTS §6.2 写的压测前清理 `dev_tools.ps1 -Command kafka-offset-reset`(不带参数)本来就会在 kafka_offset_reset 的模式校验处失败(改前改后一致),本阶段未处理,建议第 3 批文档里写明需要的参数。
- Java 版(§12):S2 不适用(Java 无 Kafka 存盘链),PARITY.md 登记由主会话写。
- 全部未编译、未运行测试,待 Codex 验证。

### 留给第 3 批
- k8s_deploy.ps1:新增 [ValidateSet('keep','zone','legacy')]$DbConsumerGroup='keep';非 zone-up 时入口 throw「-DbConsumerGroup $DbConsumerGroup 只允许与 -Command zone-up 一起用(逐 zone 切组 / 反向交接);$Command 请用默认的 keep。」(k8s_deploy_contract 负向用例断言片段「只允许与 -Command zone-up 一起用」)。
- k8s_deploy.ps1 要 dot-source tools/scripts/lib/kafka_consumer_groups.ps1;可用接口:Resolve-KafkaGroupTopicOffsets -Lines [string[]] -ExitCode [int] -Topic [string] → [pscustomobject]@{ State = 'missing'|'no-offsets'|'offsets'; Committed = @{[int]=[long]}; Rows },判不出 throw;Get-DbTaskTopicName -ZoneId -Generation(generation 取 db.yaml ServerConfig.Kafka.TopicGeneration);Test-KafkaGroupMissingText -Text。Read-DbGroupTopicFacts 用 Invoke-ClientEntryKubectl -KubectlArgs @('exec','-n',$InfraNamespace,'kafka-0','-c','kafka','--','/opt/kafka/bin/kafka-consumer-groups.sh','--bootstrap-server','localhost:9092','--describe','--group',<g>) -CaptureOutput -AllowFailure,Stdout+Stderr 拆行合并后交给 Resolve-KafkaGroupTopicOffsets;只在非 DryRun 时调用(Invoke-ClientEntryKubectl 不看 $DryRun)。
- k8s_deploy.ps1 其余按修订稿 §2/§3:四种形状(final(G) 只写 `    GroupID: "G"`;handoff(X←Y) 写 ConsumerGroup + GroupHandoffFrom、不写 GroupID)、ConvertFrom-LiveDbConsumerGroup、Resolve-DbConsumerGroupPlan 决策表(含回档行 zone + Live=null + Zc=true + Lc=false → final(Z))、New-DbConsumerGroupYaml(先输出注释行 `    # 消费组由 k8s_deploy.ps1 按 -DbConsumerGroup 与 Kafka 位点决定(docs/design/db-consumer-group-per-zone.md)`)、Read-LiveDbConfigMapYaml、Initialize-DbConsumerGroupPlans(紧跟 Assert-ClientEntryDeployPreflight)、Get-DbConsumerGroupPlan;db 模板锚点 `    GroupID: "db_rpc_consumer_group"`;login 模板锚点 `  GroupID: "db_rpc_consumer_group"` 上方加一行死配置注释、值不动;Apply-OneGoSvc 的 `if ($SvcName -eq 'db')` 块替换 PLACEHOLDER_DB_CONSUMER_GROUP / PLACEHOLDER_DB_GROUP_HANDOFF_FROM(空写 none),残留 PLACEHOLDER_DB_ 即 throw。
- k8s_deploy 的 keep + handoff 线上形状:对方组在本 topic 无位点 → final(X),Source=handoff-finalized,提示去掉 GroupHandoffFrom 并滚一次 db;这是 kafka_offset_reset.ps1 -FinalizeHandoff 成功后打印的「Next: re-run k8s_deploy.ps1 -Command zone-up ... with the default -DbConsumerGroup keep」所依赖的行为。
- 回档脚本 Step 6 传 -DbConsumerGroup zone 给 dev_tools k8s-zone-up(dev_tools 非空才透传给 k8s_deploy);第 0 步 b 的静态预检用同一份参数加 -DryRun,k8s_deploy DryRun 下 zone 模式渲染 handoff(Z←L) 预览即可通过。
- tools/scripts/tests/k8s_deploy_contract.tests.ps1 按修订稿 §12:基线 DryRun(keep)断言 GroupID=db_rpc_consumer_group_z101、无 ConsumerGroup/GroupHandoffFrom、输出含 dryrun-preview、注解 db-group-handoff-from 为 "none";zone/legacy DryRun 断言两种 handoff 形状;决策表与 ConvertFrom-LiveDbConsumerGroup 的纯函数用例;现有对 db ConfigMap GroupID 字面量的断言按新基线更新。并同步 k8s_client_entry_contract 的 Forwarded 清单加 DbConsumerGroup。
- deploy/k8s/manifests/go-svc/db.yaml:Pod 模板 annotations 末尾加两行中文注释与 mmorpg.io/db-consumer-group: "PLACEHOLDER_DB_CONSUMER_GROUP"、mmorpg.io/db-group-handoff-from: "PLACEHOLDER_DB_GROUP_HANDOFF_FROM";HPA 注释补每 zone 一组。
- deploy/k8s/db-task-retry-alerts.yaml 新 group db-task.consumer-group-handoff(interval 30s)五条:DbConsumerGroupHandoffWaiting `max by (namespace)(db_consumer_group_handoff_waiting_seconds) > 45` warning;DbConsumerGroupHandoffStuck 同表达式 > 300 critical;DbConsumerGroupHandoffOwnBehindPeer `sum by (namespace)(increase(db_consumer_group_handoff_gate_total{outcome="wait_own_behind_peer"}[5m])) > 0` critical(处置写 kafka_offset_reset.ps1 删陈旧一方本 topic 位点);DbConsumerGroupHandoffConflict `sum by (namespace)(increase(db_consumer_group_handoff_conflict_total[10m])) > 0` critical;DbConsumerGroupHandoffSeedAborted `sum by (namespace)(increase(db_consumer_group_handoff_total{result="aborted"}[5m])) > 0` warning。指标名以 go/db metrics.go(PR-B)为准逐字核对。
- docs/design/zone_data_rollback.md(+_en/_zh):2026-10-15 修订小节——Step 2 删除及原因;第 0 步 c 只读 LAG 快照(只告警、只计本 topic);第 0 步 d 在 zone 组无位点时以 dry-run 预演 Step 5、失败停服前拒绝;Step 5 重置 zone 组 db_rpc_consumer_group_z<id> 到 TargetTime,-Apply 后逐分区复核,再用 -RetireGroup 删 legacy 组本 topic 位点(仍有订阅者时 Step 5 失败、需清残留实例后单独重跑);Step 6 显式 -DbConsumerGroup zone → final(Z);参数变化:k8s_zone_rollback 删 -KafkaDrainTimeoutSec、-KafkaGroup 缺省推导、新增 -KafkaLegacyGroup;dev_tools 删 -RollbackKafkaDrainTimeoutSec、-RollbackKafkaGroup 缺省 ''、新增 -RollbackKafkaLegacyGroup;组名基名与分区数现读 go/db/etc/db.yaml;示例组名/topic 改 db_rpc_consumer_group_z<id> / db_task_zone_<id>。
- docs/design/db-consumer-group-per-zone.md §5(收尾):kafka_offset_reset.ps1 -FinalizeHandoff 的前置检查(新组 0..N-1 有数值位点,空分区按 kafka-get-offsets earliest==latest 放行,新组≥旧组)、删除后 describe 复核、成功后以 keep 再跑一次 zone-up;dev_tools 透传 -KafkaRetireGroup / -KafkaFinalizeHandoff(需 -KafkaPartitions);§11 列 V3 (b)(c)(f)(g)(h)(i) 的 CLI 文本待实测并回填 lib 白名单。
- docs/ops/merge-zone-runbook.md:dev_tools 不再转发 -kafka-group,组名由工具按 zone 推导并认定唯一权威组,-MergeKafkaGroup 只用于显式覆盖(本机混用裸名与 _zN 启动时);世代号仍现读 db.yaml。
- docs/design/data-consistency-stress-testing.md:三种组名口径(单 zone 默认启动裸名;-Zone N 启动 _zN;K8s 切组后 _z<ZoneId>),并写明 stress_summarize.ps1 新增 -Zone(缺省 0 = 裸名,>0 推导 topic/组,显式 -KafkaTopic/-KafkaConsumerGroup 优先)。

### 验证命令（交 Codex / 负责人执行）
- 工作目录 D:\luyuan\xuanming-server-mmo-cpp;语法解析:pwsh -NoProfile -Command "foreach ($f in 'tools/scripts/lib/kafka_consumer_groups.ps1','tools/scripts/kafka_offset_reset.ps1','tools/scripts/k8s_zone_rollback.ps1','tools/scripts/dev_tools.ps1','tools/scripts/stress_summarize.ps1','tools/scripts/tests/kafka_offset_reset_output_guard.tests.ps1','tools/scripts/tests/k8s_zone_rollback_gate_router_mode.tests.ps1','tools/scripts/tests/dev_tools_merge_zone_contract.tests.ps1','tools/scripts/tests/dev_tools_kafka_offset_reset.tests.ps1') { $e=$null; [void][System.Management.Automation.Language.Parser]::ParseFile((Resolve-Path $f),[ref]$null,[ref]$e); \"$f $($e.Count)\" }"。通过标准:每个文件 0 个解析错误(本会话已静态解析过一次)。
- 工作目录同上:pwsh -NoProfile -NonInteractive -File tools/scripts/tests/kafka_offset_reset_output_guard.tests.ps1。通过标准:退出码 0,无 [FAIL] 行(原 7 例 + 新增用例全部 PASS)。失败时保留完整输出。
- pwsh -NoProfile -NonInteractive -File tools/scripts/tests/dev_tools_kafka_offset_reset.tests.ps1。通过标准:退出码 0,5 例全 PASS。失败时保留输出与 %TEMP%/mmorpg-dev-tools-kafka-reset-* 下的记录文件(用例结束会删,失败需复现时可临时注释 finally)。
- pwsh -NoProfile -NonInteractive -File tools/scripts/tests/dev_tools_merge_zone_contract.tests.ps1。通过标准:退出码 0,无 FAIL。
- pwsh -NoProfile -NonInteractive -File tools/scripts/tests/k8s_zone_rollback_gate_router_mode.tests.ps1。通过标准:退出码 0,无 FAIL;重点看新增「go/db 存盘消费组按 zone 拆分」段 10 例与原有 -Apply 放行用例(DevToolsCalls 序列不变)。
- pwsh -NoProfile -NonInteractive -File tools/scripts/tests/k8s_zone_rollback_flush_guard.tests.ps1(不改文件,回归)。通过标准:退出码 0,无 FAIL。
- pwsh -NoProfile -File tools/scripts/test_stress_summarize.ps1(不改文件,回归;Kafka 段以 -KafkaContainer '' 跳过)。通过标准:退出码 0,全部 PASS。
- Windows 上回归 dev_tools 入口:pwsh -NoProfile -NonInteractive -File tools/scripts/tests/dev_tools_proto_gen_exit.tests.ps1。通过标准:退出码 0。
- 回归(注意跨批依赖):pwsh -NoProfile -NonInteractive -File tools/scripts/tests/k8s_deploy_contract.tests.ps1 与 tools/scripts/tests/k8s_client_entry_contract.tests.ps1。在第 3 批给 k8s_deploy.ps1 加 -DbConsumerGroup 之前,k8s_client_entry_contract 的「dev_tools.ps1::Invoke-K8sDeploy → k8s_deploy.ps1 的每个 splat 键都是下游 param 块声明过的参数」预期因 DbConsumerGroup 变红,这是已知的跨批依赖;k8s_deploy_contract 第 5 节两条回滚用例(拒绝可变 tag / 接受不可变 tag 的 dry-run)应 PASS。第 3 批落地后两套都必须全绿。
- 手工 dry-run 冒烟(不连集群、不改任何东西):pwsh -NoProfile -File tools/scripts/k8s_zone_rollback.ps1 -ZoneName t1 -ZoneId 101 -TargetTime 2026-05-15T14:23:00Z -NodeImage ghcr.io/luyuancpp/mmorpg-node:0123456789ab -SkipMySqlPause。通过标准:退出码 0;摘要含 `group=db_rpc_consumer_group_z101 retire=db_rpc_consumer_group partitions=10`;有一行 `kafka_offset_reset.ps1 -BootstrapServer kafka:9092 -Group db_rpc_consumer_group_z101 -Partitions 10 -RetireGroup db_rpc_consumer_group -Topic db_task_zone_101 -ToDatetime ...`;Step 6 行含 `-DbConsumerGroup zone`;没有 `Step 2: drain`。
- Kafka 实测(需负责人同意拉镜像,见规格 decisions 第 2 条;在 docker compose 的 kafka 容器内执行):对修订稿 V3 (b)(c)(f)(g)(h)(i) 逐条记录原始输出与退出码——不存在组的 describe;对不存在组 --reset-offsets --to-datetime --execute 后 describe 是否有位点;--execute 是否打印 GROUP/TOPIC/PARTITION/NEW-OFFSET 表;kafka-get-offsets.sh --topic db_task_zone_1 --time -2 / -1 输出是否为 topic:partition:offset;Empty 组无位点时的 describe;--delete-offsets 成功与 GROUP_SUBSCRIBED_TO_TOPIC 时的输出。结果回填 lib 白名单与 kafka_offset_reset_output_guard 的 fixture;若 (c) 不能建出位点,k8s_zone_rollback 改动不得发布。

## B3：B3 —— tools/merge_zone（S2 唯一权威组 + S1 的 p* 重试键）（状态 done）

### 文件
- `tools/merge_zone/preflight.go`：S2：文件头 P3 改为「唯一权威组」口径并写明取最小值为何 fail-open；kafkaLagSource.TotalLag 改为 (ctx, topic, groups []string) (lag, group, err)；kafkaCLILagSource 对每个候选组各跑一次 describe（CombinedOutput），结果交给新纯函数 resolveAuthoritativeLag；新增 groupDescribeResult、consumerGroupMissingPatterns（三条白名单正则，注释指向 lib/kafka_consumer_groups.ps1 的 Get-KafkaGroupMissingPatterns）、consumerGroupNoActiveMembersPattern、consumerGroupMissing、committedOffsetPresent（按表头定位 TOPIC/CURRENT-OFFSET；Error: 行报错；无表头时只有 has-no-active-members 返回 (false,nil)，其余报错）、dbConsumerGroupForZone、kafkaGroupCandidates；attestedLagSource 返回 (0,"(attested)",nil)，fakeLagSource 增加 group 字段；preflightParams.kafkaGroup 换成 kafkaGroups []string；P3 调新接口，拒绝 / 通过日志带权威组名（'preflight P3 OK: authoritative group %s lag=0 on %s [%s]'），并在候选组为空时 fail-closed。S1：新增 dbRetryPartitionQueuePrefix / dbRetryPartitionQueuePattern / escapeRedisGlob / dbRetryPartitionScanCount=1000 / queueKeyReader 接缝 / scanAllKeys；dbTaskQueueKeysFor 加第三参数（p 键排序去重后追加，不改调用方切片）；dbTaskQueueKeys 改收 queueKeyReader，ZRANGE 登记表加 SCAN MATCH kafka:retry:queue:<转义后的 topic>:p*，任一页出错整体报错；P4 成功日志改为 '… incl. per-instance processing lists and partitioned ready lists'。
- `tools/merge_zone/main.go`：defaultKafkaGroup 注释改为说明两个身份：legacy 共享组，以及按 zone 推导 db_rpc_consumer_group_z<zone> 的基名；options 新增 kafkaLegacyGroup；-kafka-group 缺省改为 ''，帮助文本改为英文新口径；新增 flag -kafka-legacy-group，缺省为 defaultKafkaGroup，传空值则只看 zone 组。
- `tools/merge_zone/merge_run.go`：runPreflight 的参数改为 kafkaGroups: kafkaGroupCandidates(o.kafkaGroup, o.kafkaLegacyGroup, src)。
- `tools/merge_zone/unmerge.go`：runPreflight 的参数改为 kafkaGroups: kafkaGroupCandidates(o.kafkaGroup, o.kafkaLegacyGroup, dst)。
- `tools/merge_zone/merge_unit_test.go`：imports 增加 errors、fmt、path、go-redis v9；TestDbTaskQueueKeysCoverEveryInstance 补第三参数 nil。新增 S1 用例：TestDbTaskQueueKeysIncludePartitionedReadyLists、TestDbRetryPartitionQueuePattern（用 path.Match 核对 zone_1 不会扫到 zone_11、_g2 和共享收件箱，并验证 escapeRedisGlob 的转义）、TestDbTaskQueueKeysScanPagesAggregated、TestDbTaskQueueKeysScanFailureFailsClosed，两者都用 queueKeyReaderStub 加 redis.NewStringSliceResult / NewScanCmdResult，不引 miniredis。新增 S2 用例：TestResolveAuthoritativeLag_PicksOnlyGroupWithOffsets（4 种 legacy 形态）、_InactiveGroupHigherIsRefused（F7 指定）、_BothEqualIsRefused、_NoGroupHasOffsetsIsRefused、_CLIFailureIsRefused、_DashLagOnAuthoritativeGroupIsRefused、TestCommittedOffsetPresent_ColumnOrderFromHeader、TestConsumerGroupMissing_Whitelist、TestKafkaGroupCandidates。原有 4 个 parseConsumerGroupLag 用例不动。
- `tools/merge_zone/integration_test.go`：itPreflightParams 改为 kafkaGroups: []string{defaultKafkaGroup}，fakeLagSource 字面量无需改动。TestIT_Preflight_BlocksOnEachHazard 新增一例「partitioned retry queue not empty」（RPUSH 到 <topic>:p3，预期 P4 拦截）；新增 TestIT_Preflight_IgnoresOtherTopicsPartitionedRetryLists（别的 zone、别的世代的 p 列表不拦）。均受 build tag merge_integration 约束。

### 偏离规格
- integration_test.go 多加了两条 P4 用例（原规格只要求适配新接口）：一条断言本 topic 的 p 列表非空时被拦，一条断言 zone_X1 和 _g2 的 p 列表不拦。理由：这是唯一能在真实 Redis 上核对 SCAN MATCH 口径（含 glob 边界）的地方。纯函数单测里用的是 path.Match 近似。两条都受 build tag 约束，不影响默认 go test。
- runPreflight 在 P3 新增一道内部守卫：kafkaGroups 为空时直接拒绝（fail-closed）。规格没写。kafkaGroupCandidates 不会返回空，这道守卫只防将来新增的调用点漏传。
- resolveAuthoritativeLag 收到空的 results 时报错 'no candidate consumer group to check'，规格没写这种情况，按 fail-closed 处理。
- committedOffsetPresent 认表头时只要求同一行有 TOPIC 与 CURRENT-OFFSET，照修订稿 §10 / 原规格的写法。lib 的 ConvertFrom-KafkaConsumerGroupDescribe 还要求 PARTITION / LOG-END-OFFSET / LAG。Go 侧随后用 parseConsumerGroupLag 对权威组算积压，那里本来就要求 TOPIC/LAG 列，所以结论口径一致；表头列缺失时两边都会失败，只是报错的位置不同。
- 「两组都有位点」的拒绝文案里直接给了 kafka_offset_reset.ps1 -FinalizeHandoff 的完整参数模板（-Topic <t> -Group <current> -RetireGroup <stale> -Partitions <N>），还按修订稿 §10 / R10 加了一句：本机混用裸名与 _zN 两种启动方式时，用 -kafka-group（dev_tools 用 -MergeKafkaGroup）指定。
- 额外加了 TestConsumerGroupMissing_Whitelist（规格没列），把三条白名单文案和两条反例钉住，方便 V3 拿到 4.3.1 原文后对照补 fixture。
- audit_checks.go 没改：S1 规格写明调用点不用改（*redis.Client 满足 queueKeyReader）。审计的 details 会自动列出 p 键。只是 :158 的注释仍只写了「每实例 processing 列表」，略不完整。

### 复核 / 修订意见的处理
- [fixed] S2 复核 B6 / F7：merge_zone「逐分区取各候选组 LAG 最小值」方向是 fail-open —— 按原规格加修订稿 §10 实现唯一权威组：候选组为 zone 组加 legacy 组，或显式指定的一个组；只有恰好一个组在本 topic 上有数值 CURRENT-OFFSET 时才对它算 LAG；两组都有、都没有、CLI 读不出都拒绝。用例 TestResolveAuthoritativeLag_InactiveGroupHigherIsRefused 覆盖「非活跃组位点更高」。
- [fixed] S2 复核 R15 [low]：「组不存在」只认一种文案 —— 按修订稿改为三条白名单正则（does not exist / GroupIdNotFoundException / GROUP_ID_NOT_FOUND），与 lib/kafka_consumer_groups.ps1 各写一份、注释互指；无表头时只有 has-no-active-members 判为 (false, nil)，其余报错。4.3.1 原文待 V3 回填 fixture。
- [fixed] S2 复核 R4 [medium]：空分区永远没有已提交位点，导致 merge_zone 因「两组都有位点」一直拒绝 —— 按修订稿 R4(d)：merge_zone 的判据保持「本 topic 上至少一行数值 CURRENT-OFFSET」，不读日志边界。空分区在 describe 里不出行或只出「-」，不会把一个组误判为持有位点。死锁的根源（收尾跑不了）由 kafka_offset_reset.ps1 的 FinalizeHandoff 空分区放行解决，那是另一阶段的文件。
- [fixed] S2 复核 R10 [medium]：本地单 zone 默认启动是裸名，-Zone N 启动是 _zN —— 候选组同时含 zone 组与 legacy 裸名，两种本地启动都能覆盖；两组都有位点时，拒绝文案提示用 -kafka-group / -MergeKafkaGroup 显式指定。dev_tools 已由别的会话改为缺省不转发 -kafka-group，与本改动一致。
- [not_in_scope] S2 复核 R1–R3、R5–R9、R11–R14、M1–M6（go/db 交接闸、k8s_deploy 决策表、回档、kafka_offset_reset、dev_tools splatting、文档） —— 不属于 B3（tools/merge_zone）负责的文件，由其它阶段或第 3 批处理。
- [fixed] S1 复核：没有针对 merge_zone 的条目；S1 修订稿把 preflight.go / merge_unit_test.go 列为「完全不变、仍在清单里」 —— 按 S1 原规格实现：dbRetryPartitionQueuePrefix / escapeRedisGlob / dbRetryPartitionQueuePattern、queueKeyReader 接缝、dbTaskQueueKeysFor 第三参数、ZRANGE 加完整 SCAN 并 fail-closed、不枚举 p0..p{N-1}，P4 日志文案照规格改；5 条单测照规格写。
- [rejected] S1 规格 A4-F9：merge_zone 在 SCAN 之外按 p0..p{N-1} 枚举，作为第二道防线 —— 沿用 S1 规格里的处理决定：同一 Redis 上的完整 SCAN 迭代已能保证结果；merge_zone 不知道 PartitionCnt，再写一份常数违反契约值单一来源。SCAN 失败时 fail-closed。

### markApplied 说明
本阶段只改 tools/merge_zone，没有碰 go/db 的任何文件。markAppliedSeq / markAppliedOwnerEpoch 的调用顺序和位置、它们之前的步骤、dbOpHandler 签名都没有变化。

### 遗留事项
- p* 键名按 S1 规格「接口」写成 kafka:retry:queue:{topic}:p{N}（前缀 kafka:retry:queue:<topic>:p）。落码时 A4 的 go/db/internal/kafka/retry_routing.go 还不在工作区（retryPartitionReadyKey 尚未落地），所以没法与 go/db 侧逐字核对。A4 写完后需要复核：前缀必须是 retryReadyKey(topic)+":p"+十进制分区号；若不一致，改 preflight.go 的 dbRetryPartitionQueuePrefix 及对应单测。
- 「组不存在」的三条白名单正则，以及「has no active members」的文案，都是按版本行为推定的。Codex 在 apache/kafka:4.3.1 上做完 V3 (b)(h)(i) 后，要把原文同时追加为 merge_unit_test.go 与 lib/kafka_consumer_groups.ps1 的 fixture；若原文一条都不命中，两边同步补正则。
- 既有行为、未改：权威组仍处于 Stable、有成员持有从未提交过位点的分区时，describe 会给出 LAG「-」，P3 按原逻辑 fail-closed。K8s 上 zone-down 会删 namespace，组变为 Empty，不受影响；本机合服前要先停掉 go/db。
- audit_checks.go 的 auditKafkaQueues 通过 dbTaskQueueKeys 自动带上了 p 列表，但 :158 的注释还只写「每个 go/db 实例各自的 processing 列表」。没改，因为不在规格要求内。需要的话由后续顺手补一句「以及按分区的 ready 列表 p*」。
- 建议（没有做）：在 merge_unit_test.go 加一条跨文件断言，读 ../../go/db/etc/db.yaml 的 ServerConfig.Kafka.GroupID，要求等于 defaultKafkaGroup（它是 _z<zone> 推导的基名）。merge_zone 目前没有读仓库文件的测试先例，所以没加。
- S2 上线步骤 7（下一轮）：全部 login Pod 跑上 optional 版本 ≥7 天后，把 merge_zone 的 -kafka-legacy-group 缺省改为空串（main.go 里那一行 flag 定义）。
- 全部未编译、未运行测试，待 Codex 验证（AGENTS.md §10.1）。只对改动文件跑过 gofmt -l / -w，结果干净。

### 留给第 3 批
- docs/ops/merge-zone-runbook.md（S2，按锚点改）：P3 段（原 :102 附近）改为「消费组按 zone 推导（db_rpc_consumer_group_z{zone}），legacy 组 db_rpc_consumer_group 也是候选；门禁只认在本 topic 上有已提交位点的唯一权威组，两组都有位点就拒绝，先跑 kafka_offset_reset.ps1 -FinalizeHandoff 收尾」；:103 / :104 改为「dev_tools 不再转发组名，覆盖用 -MergeKafkaGroup」；P3 表格行（原 :266）同步；describe 命令里的组名改为 db_rpc_consumer_group_z<src>（原 :487）；「dev_tools.ps1 已转发 -kafka-group」（原 :986）与「-MergeKafkaGroup … 默认从 go/db/etc/db.yaml 现读」（原 :1030）两处改为：组名不再转发，由工具推导；-MergeKafkaGroup 只用于显式覆盖；世代号仍现读。
- merge_zone 新口径（写文档时逐字使用）：-kafka-group 缺省为 ''（非空时只看这一个组）；新增 -kafka-legacy-group，缺省 db_rpc_consumer_group，传空值则只看 zone 组；按 zone 推导的组名为 db_rpc_consumer_group_z<zone>（merge 取 src，unmerge 取 dst）。拒绝文案的关键字：'more than one candidate group holds committed offsets on topic'、'no candidate group holds committed offsets on topic'；通过日志：'preflight P3 OK: authoritative group <g> lag=0 on <topic> [...]'。
- docs/ops/merge-zone-runbook.md（S1）：P4 行（原 :267）与 kafka_db_task_queues 行（原 :405）补「以及按分区的 ready 列表 kafka:retry:queue:db_task_zone_{src}:p*」；§8 Step 2 命令块（原 :489-494）追加 `redis-cli -h <shared-redis> -n 0 --scan --pattern 'kafka:retry:queue:db_task_zone_<src>:p*'`，逐个 LLEN == 0，并注明 merge_zone 的 P4 已经用 SCAN 查到它们；P4 通过日志文案改为 '… incl. per-instance processing lists and partitioned ready lists'。
- docs/design/db-consumer-group-per-zone.md §7（合服）：写明唯一权威组判定规则；为什么不取最小值（冻结旧组 LAG=0 会替新组担保，是 fail-open）；「组不存在」的三条白名单正则在 tools/merge_zone/preflight.go（consumerGroupMissingPatterns）与 tools/scripts/lib/kafka_consumer_groups.ps1（Get-KafkaGroupMissingPatterns）各写一份，V3 拿到 4.3.1 原文后两边同步；无表头时只有 has-no-active-members 判为没有位点，其余 fail-closed。
- docs/design/db-retry-owner-routing.md（S1）：「需同步的同类入口」里登记 merge_zone 的 P4 与 audit kafka_db_task_queues 已经通过 SCAN MATCH（topic 先做 glob 转义）覆盖 p*，不枚举 p0..p{N-1}，SCAN 失败时 fail-closed。
- docs/design/server_merge_design.md :107 / :359 的组名口径改为「zone 组 + legacy 候选，唯一权威组」。
- docs/PROGRESS.md 条目需注明：tools/merge_zone 的 B3 已落码，未编译、待 Codex 验证；Java 版不适用（无 Kafka 存盘链）。

### 验证命令（交 Codex / 负责人执行）
- 工作目录 D:\luyuan\xuanming-server-mmo-cpp\tools\merge_zone：gofmt -l .  —— 通过标准：没有输出。
- 工作目录 D:\luyuan\xuanming-server-mmo-cpp\tools\merge_zone：go vet ./...  —— 通过标准：以 0 退出。失败时保留编译错误的前 20 行。
- 工作目录 D:\luyuan\xuanming-server-mmo-cpp\tools\merge_zone：go vet -tags merge_integration ./...  —— 用来编译集成测试文件（integration_test.go 改了 preflightParams 字段，还新增了用例）。通过标准：以 0 退出。
- 工作目录 D:\luyuan\xuanming-server-mmo-cpp\tools\merge_zone：go test ./... -count=1 -v -run 'Lag|Authoritative|KafkaGroup|CommittedOffset|ConsumerGroupMissing|DbTaskQueueKeys|DbRetryPartition|DbTaskTopic'  —— 通过标准：全部 PASS，-v 输出里出现以下用例：TestResolveAuthoritativeLag_PicksOnlyGroupWithOffsets / _InactiveGroupHigherIsRefused / _BothEqualIsRefused / _NoGroupHasOffsetsIsRefused / _CLIFailureIsRefused / _DashLagOnAuthoritativeGroupIsRefused、TestCommittedOffsetPresent_ColumnOrderFromHeader、TestConsumerGroupMissing_Whitelist、TestKafkaGroupCandidates、TestDbTaskQueueKeysCoverEveryInstance、TestDbTaskQueueKeysIncludePartitionedReadyLists、TestDbRetryPartitionQueuePattern、TestDbTaskQueueKeysScanPagesAggregated、TestDbTaskQueueKeysScanFailureFailsClosed，以及原有 4 个 TestParseConsumerGroupLag_*。失败时保留 -v 全文。
- 工作目录 D:\luyuan\xuanming-server-mmo-cpp\tools\merge_zone：go test ./... -count=1  —— 整包回归。通过标准：ok。
- 可选（需要本机 MySQL 127.0.0.1:3306 和 Redis 127.0.0.1:6379，凭据见 deploy/docker-compose.yml；Redis DB 9/10/11 非空时用例会 skip，不会 flush 他人数据）。工作目录 D:\luyuan\xuanming-server-mmo-cpp\tools\merge_zone：go test -tags merge_integration -count=1 -v -run 'TestIT_Preflight' ./...  —— 通过标准：TestIT_Preflight_PassesOnACleanZone、TestIT_Preflight_BlocksOnEachHazard（含新子用例 partitioned_retry_queue_not_empty 报 P4）、TestIT_Preflight_IgnoresOtherTopicsPartitionedRetryLists、TestIT_Preflight_IgnoresStaleTeamIndexes 全部 PASS。
- A4 的 go/db/internal/kafka/retry_routing.go 落地后，在仓库根执行：git grep -n 'retryPartitionReadyKey\|":p"' -- go/db/internal/kafka tools/merge_zone/preflight.go  —— 通过标准：go/db 侧拼出的键是 "kafka:retry:queue:"+topic+":p"+十进制分区号，与 tools/merge_zone/preflight.go 的 dbRetryPartitionQueuePrefix 一致。

