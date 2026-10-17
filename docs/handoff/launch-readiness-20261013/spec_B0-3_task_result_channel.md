# 调研规格:task:result:notify 分频道(go/db → login)(2026-10-16)

> **状态**:只读调研产出,**未落码**。由开服线会话 2026-10-16 的调研工作流生成(一名调研者出规格、一名核实者逐条反驳),原样保存,未经人工改写。
> **怎么用**:前半部分是调研者的规格;「# VERDICT」之后是核实者意见。**落码前必须先吸收核实意见里的 must / should**,被判 refuted 的事实以核实者为准。行号与工作区状态是 2026-10-16 03:48~04:35 的快照,工作区有多个会话在并行修改,落码前按函数名重新定位。
> **汇总、拍板项与推荐顺序**:见 [docs/PROGRESS.md](../../PROGRESS.md)「2026-10-16 开服线剩余工作调研」条目,以及交接 [handoff-launch-readiness-and-d1-20261013.md](../handoff-launch-readiness-and-d1-20261013.md) 文首 10-16 段。

## item
task:result:notify 分频道（容量模型 A0 第 5 条 / §4.2「第 0 批」、G4a 第 0 批、全局单点交接 §4.5 与 §5.1 第 2 条、开服清单 L-06 后半句）
## already_done
代码层面还没有任何会话落地。go/db 仍然只往全局频道发（key_ordered_consumer.go:45、:492）；login 仍然只订全局频道（task_result_dispatcher.go:32、:227）；DBTask 里没有回复目标字段（proto/db/db_task.proto 工作区版本最后一个字段是 save_seq=9）。

目前定下来的只有字段号。负责人在 godb 交接 §5 第 13 项定为「9 给 save_seq，reply_instance 用 10」，见 docs/PROGRESS.md:7745 和 docs/design/db-write-fencing-mysql.md:950。

save_seq=9 已由围栏线写进工作区，尚未提交。围栏接手报告打算在 db_task.proto 里加一句注释，写明 10 号预留给 reply_instance（docs/handoff/landing-fencing-takeover-20261016/fence-inflight.md:135）。

G4a 设计稿里还没有「第 0 批」这一节：launch-readiness 交接 §4 第 5 项「修订拆 Redis 设计」仍未做。
## summary
**问题**：go/db 把每条读回执的到达通知 PUBLISH 到全服唯一的频道 task:result:notify，载荷只有 task_id。所有 login 进程都订这一个频道，收到后在进程内用 take(taskID) 查回调表，把不属于自己的丢掉。结果是：Redis 的投递次数 = 读回执数 × login 实例数 L；每个 login 的入站量 = 全服读回执总量。

**根因**：DBTask 不带回复目标，发布方无法定向投递。

**2000 万在线下的量**（§7.2：E≈11,111/s；Z=200~445，每 zone 2 个 login，所以 L=400~890）：
- **稳态**（父键常驻、新号种子关）：读回执约 139/s，全局投递 5.6 万~12.4 万/s。
- **冷卸载上线或核心 Redis 切主后**（f_miss→1）：读回执约 2.2 万/s，全局投递 889 万~1,976 万/s；每个 1 核的 login 入站约 2.2 万条/s，约 1.9 MB/s。
- **开服且新号种子关**：读回执 2 万~10 万/s，全局投递 800 万~8,890 万/s；每个 login 入站 1.7~8.5 MB/s。
- 冷读这两种情况下 R 和 L 都正比于在线人数，投递量随 PCU² 增长，不能拿 S3 的数乘二。

**做法**：
- DBTask 加 `string reply_instance = 10`（字段号已定）。
- login 每个进程生成一个 UUID 作为回复目标，在同一条 Pub/Sub 连接上同时订旧频道和 `task:result:notify:<uuid>`；读任务带上这个 UUID。
- go/db 校验回复目标合法就只发实例频道；为空或不合法时回落旧频道。只发一次，不双发。
- 频道名和校验规则放进新包 go/shared/taskresult，作为两端唯一的来源。
- 新旧版本任意组合都能送达，滚动顺序不限，两端可以各自回滚。
- 兜底仍用现有机制：TTL 到期时 LPOP（结果 LIST 先于通知写入）。
- 两端各加一个低基数计数器，用来观察：实例频道没人收、回落旧频道、兜底取回、超时。
- 暂时用 PUBLISH，不用 SPUBLISH。理由有三：现在是单主 Redis；miniredis 不支持 S 系命令；容量模型 A6 路线 C 已把 SPUBLISH 列为集群切换批的一项。

**效果**：分频道后投递量就等于读回执数本身，比现在少 L=400~890 倍；每个 login 只收自己发起的回执。
## est_files: 27
## current_facts
F1. 发布方只有一个全局频道。载荷只有 task_id；先写结果 LIST 再发通知；通知失败只打日志、不返回错误。
   证据: go/db/internal/kafka/key_ordered_consumer.go:41-45，常量 taskResultNotifyChannel=task:result:notify；:474-496 publishTaskResult 依次执行 LPUSH task:result:{taskID}、EXPIRE 5 分钟（:40 taskResultExpireDuration）、Publish(taskResultNotifyChannel, taskID)；:492-494 Publish 出错只 logx.Errorf，仍 return nil。已亲自打开工作区版本核对。
F2. 只有读任务发回执。调用点有两处：成功回执和落点失败回执。
   证据: key_ordered_consumer.go:441-472 publishDBReadResult：只在 TaskId 非空时发（:458），调用在 :465。go/db/internal/kafka/placement_route.go:350-357 failRead 发 Success=false 回执，调用在 :353。写路径 publishDBWriteResult（:994-1020）不发回执。已亲自打开。
F3. login 只订全局频道，用一条 PubSub 连接；本地缓冲 8192 条，满了等 5 秒后丢弃。
   证据: go/login/internal/dispatcher/task_result_dispatcher.go:32 常量；:227 d.rc.Subscribe(ctx, TaskResultNotifyChannel)；:231 用 Receive 确认一次；:248-251 WithChannelSize(8192)、WithChannelSendTimeout(5s)。go-redis v9.16.0 pubsub.go:724-738：channel 满且超时后丢弃消息，只打日志。已亲自打开（含本机模块缓存）。
F4. 过滤方式：每个 login 处理全量通知，逐条持锁查回调表，查不到就丢掉。
   证据: task_result_dispatcher.go:280-301 dispatch 调 take(taskID)（:161-173 持 mutex 删 map 项）；cb 为 nil 时直接 return，注释写明 pub/sub 会广播给所有订阅者（:282-286）。已亲自打开。
F5. 通知丢失后的现有兜底：每秒扫一次，到期时做最后一次 LPOP；EnterGame 每条任务的 TTL 是 8 秒。go-redis 断线重连期间的通知会静默丢失。
   证据: task_result_dispatcher.go:203 gcInterval=1s；:347-383 sweepExpired 到期后 LPOP，成功就按成功回调，否则回调超时错误。go/login/internal/logic/clientplayerlogin/entergamelogic.go:421 dispatcherTaskTTL=8s；go/login/internal/svc/servicecontext.go:211 默认 TTL 30s。go-redis pubsub.go:20、:110-125：断线后自动重连并重订，重连窗口内的消息不补发。已亲自打开。
F6. 读任务只有一个生产点：login 的 sendDBReadRequests。DBTask 不带回复目标；每个冷读玩家发 2 条。
   证据: go/login/internal/logic/pkg/dataloader/sync_loader.go:81-109，DBTask 字面量在 :95-102，只有 Key/WhereCase/Op/MsgType/Body/TaskId。调用方是 ensure_player_all_data_async.go:194 和 sync_loader.go:77（TriggerPlayerDataPreload，全仓 grep 无调用方）。task_id=uuid.NewString()（ensure_player_all_data_async.go:87）。new_player_seed.go:23-28 父键有两张子表。全仓生产代码里 Op 为 read 的只有 sync_loader.go:98。C++ 只发写任务（player_lifecycle.cpp:4421-4429 set_op write）；go/db/cmd/data_stress/main.go:214-221 只发写任务。已亲自打开并 grep。
F7. 字段号已定为 10；save_seq=9 已在工作区，未提交。
   证据: proto/db/db_task.proto 工作区 :21 为 uint64 save_seq = 9，git diff 为 2+/1-、未提交，是围栏线加的。docs/PROGRESS.md:7745：「13. DBTask 9 号：9 给 save_seq，reply_instance 用 10」。docs/design/db-write-fencing-mysql.md:950 同口径。go/db 已在用 task.GetSaveSeq()（player_row_fence.go:33）。已亲自打开。
F8. 规模参数：login 每 zone 2 副本，CPU 上限 1 核；在线人数口径以 §7.2 为准。
   证据: deploy/k8s/manifests/go-svc/login.yaml:24 replicas: 2，:86-90 requests 200m / limits 1。capacity-model-hok-scale.md §7.2（:894-910）：E=2000 万/1800≈11111/s；§0.3（:36-60）：C_n=DAU×2%/86400×3、C_z 取 45k 或 100k、K=0.1Z、C_p 是 S3 题设 1~5 万/s；§2.4（:242）是 S3 口径的扇出公式。已亲自打开。
F9. K8s 上是 Redis 7.2 单主，没有覆盖 pubsub 输出缓冲上限；login 和 go/db 都用单机客户端。
   证据: deploy/k8s/manifests/infra/redis.yaml:90 image redis:7.2；:131-133 启动参数只有 --appendonly、--maxclients、--requirepass。go/login/internal/svc/servicecontext.go:119 和 go/db/internal/svc/servicecontext.go:24 都是 redis.NewClient。Redis 默认的 client-output-buffer-limit pubsub 32mb 8mb 60 属外部常识，仓库内未核实。已亲自打开。
F10. miniredis 不支持 SPUBLISH/SSUBSCRIBE；go-redis 支持在同一 PubSub 上混订普通频道和分片频道。
   证据: 本机模块缓存里 miniredis v2.35.0（login 用）、v2.36.1（db 用）、v2.37.0 grep SPUBLISH 都是 0 命中；cmd_pubsub.go:14-19 只注册了 SUBSCRIBE、PUBLISH、PUBSUB 等。go-redis v9.16.0 universal.go:315 有 SSubscribe；pubsub.go:32、:260 表明同一 PubSub 维护 schannels。已亲自打开。
F11. 容量模型对 SPUBLISH 前后口径不一：A0 写的是现在就用，A6 只把它列为集群路线的一项。
   证据: capacity-model-hok-scale.md:420（A0 第 5 条）：「直接用 SPUBLISH 形态，便于日后上集群」。:556（A6 路线 C）：「Pub/Sub 改成 SPUBLISH」，路线 R/C 尚未拍板（:547-549）。已亲自打开。
F12. Redis 连接总账给每个 login 记 1 条 PubSub 连接。
   证据: tools/scripts/k8s_deploy.ps1:1764-1765，login 行 PubSub = 1，注释指向 Subscribe(TaskResultNotifyChannel)。已亲自打开。
F13. 重试链路会把 DBTask 整份重新序列化，新字段会随重试载荷一起走。
   证据: key_ordered_consumer.go:2513-2516 encodeRetryPayload 用 proto.Marshal(task)。Go protobuf 默认保留未知字段属外部常识。已亲自打开。
F14. key_ordered_consumer.go 正被多个会话在途修改，但没有一处 hunk 落在本项要改的行上。
   证据: git status 为 M，mtime 2026-10-15 23:12:55，git diff --stat 121+/14-。在途 hunk 位置：:25-26（import assetpb/assetop）、:254-286（untrack）、:985-987（savePlayerRow）、:995-1011（资产 SQL 提交证据）、:1031-1034（publishPlayerCache）、:2241-2257 和 :2288（ConsumeClaim 撤拉取）、:2308、:2477-2480（errPlayerRowStale）、:2703-2796（Stop 和 saveToRetryQueue）。本项只动 :41-45、:465、:474-496。docs/handoff/godb-pipeline-impl-handoff-20261015.md:71 要求 R1→R2→R3 在这个文件上串行，R2（A4）尚未开始（:62），它的清单（S1 规格）不含 publishTaskResult。docs/design/db-write-fencing-mysql.md:450 计划在 publishDBReadResult 里加计数，属围栏线。已亲自打开。
F15. Java 版功能上不适用，但 proto 副本需要机械同步。
   证据: D:\luyuan\xuanming-server-mmo\PARITY.md:162 写明不适用：Java 进场按主键读库，没有经 Kafka 的冷读。Java 源码（排除 build/ 和 target/）grep task:result / DBTask 无引用。game-proto/src/main/proto/proto/db/db_task.proto 由 ContractSync 管理（Java AGENTS.md §1），当前工作区为 M，mtime 2026-10-16 02:41，已同步 save_seq。已亲自打开。
## plan
- [go/shared/taskresult/notify.go（新建）] 新建 taskresult 包，作为发布方和订阅方共用的唯一契约来源。内容如下：
- `const LegacyNotifyChannel = "task:result:notify"`。注释写明：已部署的旧版 go/db 只发这条、旧版 login 只订这条，字面值是滚动升级契约，不许改。
- `const MaxReplyInstanceLen = 64`。
- `func ValidReplyInstance(id string) bool`：长度 1..64 字节，逐字节只允许 A-Z、a-z、0-9 和 '-'。手写循环，不用 regexp。注释写明为什么拒绝 ':' 和 '*?[]' 这类 glob 元字符：避免实例频道和旧频道、运维用的 PSUBSCRIBE 模式互相串；长度上限防止异常输入撑大频道名。36 字节的 UUID 满足。
- `func InstanceNotifyChannel(id string) (string, bool)`：id 合法时返回 LegacyNotifyChannel+":"+id 和 true；不合法时返回空串和 false，回落策略由调用方决定。

不收录 task:result:{id} 结果键。两端目前各自用 fmt.Sprintf 拼，本项不顺手重构。
   工作区: 目录不存在（ls 已确认），新建，不与任何人冲突。go/db/go.mod:126 和 go/login/go.mod:119 都已有 replace shared => ../shared，不用改 go.mod。
- [go/shared/taskresult/notify_test.go（新建）] 表驱动测试，覆盖合法与不合法的 id、实例频道的拼法，并钉住旧频道的字面值。具体用例见 tests 字段。
   工作区: 新建。
- [proto/db/db_task.proto] 在 `uint64 save_seq = 9;` 之后追加 `string reply_instance = 10;`。

注释写明以下几点：
- 含义：读任务的回复目标，即发起读的 login 进程实例标识（进程启动时生成的 UUID，见 go/login/internal/dispatcher）。
- go/db 把读回执的到达通知 PUBLISH 到 task:result:notify:<reply_instance>（见 go/shared/taskresult）。
- 为空（旧版 login、写任务、工具）或不合法时，回落全服旧频道 task:result:notify。
- 写任务不填。
- 字段号依据 godb-pipeline-handoff-20261013 §5 第 13 项：9 给 save_seq，10 给 reply_instance。

不动其他字段。改完交 Codex 跑 proto-gen-run，重生成 go/proto/db/db_task.pb.go 和 cpp/generated/proto/db/db_task.pb.{h,cc}。生成物不手改（AGENTS §3）。
   工作区: git status 为 M，mtime 2026-10-15 22:18:31，git diff --stat 2+/1-。这是围栏线未提交的 save_seq=9 加上文件末尾换行，和本项相关：改的是同一个 message 的末尾。必须在它之上追加，不能回退。

围栏接手报告 fence-inflight.md:135 计划在这里加「10 号预留」注释。落码前要通知围栏线，改为直接引用本字段，避免重复编辑。

三个生成物当前也是 M（save_seq 那次重生成的结果），重生成会一并覆盖，属预期。
- [go/db/internal/kafka/key_ordered_consumer.go] 只改三处，其余一律不碰。

1. 删除 :41-45 的 taskResultNotifyChannel 常量和它的注释，改用 taskresult.LegacyNotifyChannel；import 加 "shared/taskresult"。

2. :465 改为 `publishTaskResult(ctx, redisClient, task, result)`，原来传的是 task.TaskId。

3. 重写 :474-496 的 publishTaskResult，新签名为 `func publishTaskResult(ctx context.Context, redisClient redis.Cmdable, task *db_proto.DBTask, result *db_proto.TaskResult) error`：
   - proto.Marshal(result) 不变。
   - resultKey 用 task.GetTaskId()；LPUSH 和 EXPIRE 不变，而且必须在 PUBLISH 之前。注释写明原因：login 一收到通知就 LPOP；TTL 兜底也依赖列表已经存在。
   - `channel, target := taskResultNotifyTarget(task.GetReplyInstance())`。
   - target 为 legacy_invalid 时，经 notifyLogLimiter 限速打一行 ERROR。内容带 taskID、id 长度，以及截断到 64 字节的 %q。
   - `receivers, err := redisClient.Publish(ctx, channel, task.GetTaskId()).Result()`。
   - 按结果计数：err≠nil 计 (target, error)，并限速打 ERROR；receivers==0 计 (target, no_receiver)，不打日志；其他情况计 (target, ok)。
   - 无论通知成败都 return nil。这是原有语义：通知失败只拉长时延，结果已经在 LIST 里，由兜底取回。

函数注释补一句：只发一次、不双发，因为新 login 两个频道都订着。
   工作区: git status 为 M，mtime 2026-10-15 23:12:55，git diff --stat 121+/14-。在途 hunk 是 G6 停机③④修复、围栏和资产 SQL 提交证据，分布在 :25-26、:254-286、:985-987、:995-1011、:1031-1034、:2241-2257、:2288、:2308、:2477-2480、:2703-2796，与本项三处都不重叠。

已排期但还没动的：
- go/db 接手线 R2（A4 WAITAOF），会大改重试与停机代码，但不含 publishTaskResult。
- 围栏线 v0.4，会改 handleDBWriteOp，并在 publishDBReadResult 的缓存 SET 处加 NX 与计数。它和本项 :465 的那一行同在一个函数里，是相邻 hunk。

落码时用精确的 old_string 编辑，不整文件覆盖。
- [go/db/internal/kafka/task_result_notify.go（新建）] 放回复目标的路由判定，以及本项专用的日志限速器。
- `func taskResultNotifyTarget(replyInstance string) (channel, target string)`：
  - replyInstance 为空：返回 taskresult.LegacyNotifyChannel 和 metrics.TaskResultNotifyLegacyUnset；
  - taskresult.InstanceNotifyChannel 返回 ok：返回实例频道和 metrics.TaskResultNotifyInstance；
  - 其他情况：返回旧频道和 metrics.TaskResultNotifyLegacyInvalid。
- 注释写清楚为什么不合法时回落旧频道而不是丢弃：新 login 也订着旧频道，回落照样能送达；丢弃的话，等待方要等满 TTL（EnterGame 8 秒）才能靠兜底取回。
- `var notifyLogLimiter = &logLimiter{last: map[string]time.Time{}, held: map[string]int{}}`，复用 store_failure.go:119-139 已有的 logLimiter 类型和 10 秒间隔，不改那个文件。
   工作区: 新建。store_failure.go 只是被引用、不修改。R2 会改 store_failure.go，但不涉及 logLimiter 类型。
- [go/db/internal/kafka/placement_route.go] :353 改为 `publishTaskResult(w.ctx, w.redisClient, dbTask, failure)`，原来传的是 dbTask.TaskId。只改这一行。
   工作区: git status 干净，mtime 2026-10-15 14:39:10。S1/R2 的文件清单不含这个文件。
- [go/db/internal/metrics/task_result_notify.go（新建）] 新指标单独成文件，参照 pipeline.go:5-6 的先例：用自己的 sync.Once，不碰 register()。

- 指标 `db_task_result_notify_total{target,result}`：Subsystem 用 subsystem，Name 为 task_result_notify_total。
- target 取值：instance | legacy_unset | legacy_invalid；result 取值：ok | no_receiver | error。导出常量 TaskResultNotify*。
- Help 写明：no_receiver 在 instance 上表示发起方 login 已经不在或没订上；在 legacy 上表示一个 login 都没有。
- `registerTaskResultNotify()` 里 MustRegister，并把 9 条序列都以 Add(0) 建出来。
- `func CountTaskResultNotify(target, result string)`。
- 不带 player_id 或 task_id 标签（AGENTS §9）。
   工作区: 新建。
- [go/db/internal/metrics/metrics.go] 改两处：
- Start() 里在 registerPipeline() 之后加一行 registerTaskResultNotify()，让告警要用的序列在第一次抓取时就以 0 出现。
- 文件头注释 :19-22 的 result_publish 一段改为：「LPush + Expire + Publish 到发起方 login 的实例频道（缺目标时发全服旧频道）」。
   工作区: git status 干净，mtime 2026-10-15 14:33:09。R2 计划改 metrics.go 的 register() 并加新计数。本项只碰 Start() 里一行和头注释，冲突面很小；如果 R2 先落，就在它的 Start() 上追加。
- [go/db/internal/kafka/task_result_notify_route_test.go 和 task_result_notify_test.go（新建，拆成两份）] route_test 只用签名不会变的入口：publishDBReadResult、failReadTask、encodeRetryPayload/unwrapRetryPayload。另外只依赖 miniredis 和 go-redis 的 ProcessHook 记录器。记录器类型取名 publishRecorder，记下命令名、参数、返回值和顺序。不依赖墙钟、不等待。

这样修复前也能编译，可以拿到断言层面的红证据。另一份 task_result_notify_test.go 放需要用到 metrics 新常量的回落与计数用例。具体用例见 tests 字段。
   工作区: 新建。包内其他在途测试文件（consumer_shutdown_test.go 等为 M，player_row_fence_integration_test.go 等为 ??）不动。已 grep 确认，新的测试名和类型名不重名。
- [go/db/internal/metrics/task_result_notify_test.go（新建）] 钉住 9 条序列在启动时就已建出，且 label 取值和常量逐字一致。告警按字面值写，改名会先让这里变红。
   工作区: 新建。
- [go/login/internal/dispatcher/task_result_dispatcher.go] 1. 删除 :31-32 的导出常量 TaskResultNotifyChannel，改用 taskresult.LegacyNotifyChannel。Go 代码里没有其他引用（grep 确认）。

2. 结构体加字段：`replyInstance string`、`instanceChannel string`、`onDelivery func(outcome string)`。

3. `NewTaskResultDispatcher(rc, defaultTTL)` 签名不变，内部改为 `return newTaskResultDispatcher(rc, defaultTTL, uuid.NewString(), countDelivery)`。注释写明两点：
   - 回复目标以「进程」为粒度：回调表只活在本进程内存里，进程换了旧回调也就没了。
   - 不用主机名：容器原地重启后主机名不变，没必要让新进程接住旧进程的通知。
   保持签名不变，是为了不碰 servicecontext.go（其他会话正在改），也不影响 G4a 规划的 `NewTaskResultDispatcher(s.CacheRedis, …)`。

4. 新增未导出构造函数 newTaskResultDispatcher(rc, ttl, replyInstance, onDelivery)：用 taskresult.InstanceNotifyChannel 校验，不合法就 panic，属于编程错误，启动即失败。

5. 新增 `func (d *TaskResultDispatcher) ReplyInstance() string`。

6. Start() 打一行 INFO，内容是 reply_instance 和两个频道名，供运维把频道对到 Pod。

7. subscribeLoop 的 :227 改为 `d.rc.Subscribe(ctx, taskresult.LegacyNotifyChannel, d.instanceChannel)`。这是一条 SUBSCRIBE 命令订两个频道，用同一个 PubSub 连接，所以连接总账的 PubSub=1 不变。:231 仍只 Receive 一次，注释写明理由：Redis 单线程执行完整条 SUBSCRIBE 才回第一条确认，第二条确认会被 go-redis 的 Channel 忽略。:262 改为 `d.dispatch(ctx, msg.Channel, msg.Payload)`。

8. dispatch 改签名为 `(ctx, channel, taskID string)`：
   - take 返回 nil 时：如果 channel 是本实例频道，记 late_notify（本进程的任务已超时，或已被兜底取走）；如果是旧频道，不记数，因为混跑期间那是别的 login 的任务。然后 return。
   - take 拿到回调时：来自实例频道记 instance_notify，否则记 legacy_notify；之后流程和原来一样。
   - :283-284 的注释同步改写。

9. runDispatchJob 里 LPOP 失败，而且错误是 redis.Nil 时，先记 result_missing，再按原样 cb(nil, err)。这能暴露结果 LIST 和频道不在同一个 Redis 实例上的配置错误。

10. sweepExpired 兜底取回成功记 salvaged，失败记 timed_out。原有日志保留。

其余（worker 池、Stop、TTL 扫描）不变。
   工作区: git status 干净，mtime 2026-10-12 03:53:04，没有他人在途改动。
- [go/login/internal/dispatcher/metrics.go（新建）] - 用 go-zero 的 metric.NewCounterVec，Namespace 为 login_task_result，Name 为 delivery_total，Labels 为 [outcome]。
- outcome 是常量的封闭集合：instance_notify | legacy_notify | late_notify | result_missing | salvaged | timed_out。
- `func countDelivery(outcome string)`。
- 注释写明：go-zero 的 metric 在 prometheus 未启用时 Inc 为空操作（go-zero v1.9.2 core/metric/metric.go:14-20），所以测试通过注入的 onDelivery 来断言，不读这个计数器。
   工作区: 新建。
- [go/login/internal/dispatcher/task_result_dispatcher_test.go（新建）] dispatcher 包此前没有任何测试。用例见 tests 字段。以确定性测试为主：不 Start，直接调 dispatch 和 runDispatchJob；配一条 miniredis 订阅集成用例，等待有上限。
   工作区: 新建。
- [go/login/internal/logic/pkg/dataloader/sync_loader.go] 1. 把 :91-103 循环体里构造 DBTask 的部分抽成 `func newDBReadTask(t *dbReadTask, playerId uint64, playerIdStr, replyInstance string) (*login_proto.DBTask, error)`。原有字段原样保留，另加 `ReplyInstance: replyInstance`。注释写明：replyInstance 必须是登记了这批回调的那个 TaskResultDispatcher 的 ReplyInstance()。

2. sendDBReadRequests 签名末尾加 `replyInstance string`，循环里改调 newDBReadTask。

3. TriggerPlayerDataPreload（全仓没有调用方）签名在 messages 之后加 `replyInstance string`。注释写明：没有人等结果，但仍应传本进程 dispatcher 的 ReplyInstance()，否则空串会让这些回执退回全服广播。:77 的调用同步改。
   工作区: git status 干净，mtime 2026-10-12 03:53:04。
- [go/login/internal/logic/pkg/dataloader/ensure_player_all_data_async.go] :194 改为 `sendDBReadRequests(ctx, producer, tasks, playerId, playerIdStr, disp.ReplyInstance())`，并加一行注释：回复目标必须是 :158-189 登记回调的同一个 disp。
   工作区: git status 干净，mtime 2026-10-13 00:09:01。
- [go/login/internal/logic/pkg/dataloader/sync_loader_test.go（新建）] 测 newDBReadTask：ReplyInstance 写的是传入值，原有字段的取值不变。
   工作区: 新建。同目录已有 new_player_seed_test.go（干净）。
- [deploy/k8s/db-task-retry-alerts.yaml] 文件末尾新增 rule group `db-task.result-notify`，interval 30s，两条规则：

1. DbTaskResultNotifyNoReceiver（warning）：按 namespace 求 db_task_result_notify_total{target=instance,result=no_receiver} 在 instance 总量中的 5 分钟速率占比，超过 5% 且持续 10 分钟就告警。description 写明：最常见的原因是 go/db 和 login 对频道名的认知不一致，或者 login 大面积重启。这时进场要等满 8 秒 TTL、靠兜底取回；处置方式是回滚最近升级的一侧。

2. DbTaskResultNotifyLegacyFallback（info）：sum by namespace rate(db_task_result_notify_total{target!=instance}[30m]) > 0，持续 2 小时。注释写明：login 全量升级之前必然会触发，应在全量之后再 apply；之后如果再触发，说明有新的读生产者没有填 reply_instance。

文件头的「指标定义」一行补上 task_result_notify_total。
   工作区: git status 干净，mtime 2026-10-14 23:14:30。部署方式是手工 kubectl apply（见文件头），不在 k8s_deploy.ps1 里。
- [docs/design/shared-redis-split-by-purpose.md] 见 doc_updates：
- :119 的表行；
- §3.2.4（:307-311）的状态；
- 新增「第 0 批：task:result 按 login 实例分频道」小节；
- :1047 待决第 3 条改为已答复。
   工作区: git status 干净，mtime 2026-10-15 14:38:26。launch-readiness 交接 §4 第 5 项（G4a 修订）也会改这份文件，但那项尚未开工。
- [docs/design/capacity-model-hok-scale.md] 见 doc_updates：A0 第 5 条（:420）、§4.2 第 0 批（:663）、§2.4（:242）或 §7.2 补 2000 万口径。
   工作区: git status 干净，mtime 2026-10-15 14:38:41。
- [docs/design/tidb-phase1-and-launch-readiness.md] :842 的 L-06 行加状态注。
   工作区: git status 干净，mtime 2026-10-15 10:46:36。
- [docs/PROGRESS.md] 在末尾另起一条，不改旧条目（AGENTS §5，merge=union）。
   工作区: git status 为 M（多会话在追加），mtime 2026-10-16 04:02:05。只追加。
- [docs/handoff/handoff-launch-readiness-and-d1-20261013.md] 文首进度段追加一行，正文不改：§4 第 5 项「第 0 批」中的 task:result 分频道已落码，未编译。
   工作区: git status 为 M（他人加了 2 行），mtime 2026-10-15 22:15:56。只在文首段末追加。
- [docs/handoff/godb-pipeline-handoff-20261013.md] :86 的表行和 §5 第 13 项（:228）各加注：reply_instance=10 已落码，见 PROGRESS 对应条目。
   工作区: git status 干净，mtime 2026-10-13 04:31:05。
- [D:\luyuan\xuanming-server-mmo\game-proto\src\main\proto\proto\db\db_task.proto 与 PARITY.md（Java 仓）] proto 由 Java 仓的 tools/ContractSync.java 从 mmorpg 机械同步，不手改。PARITY.md 末尾追加一行：「task:result 读回执通知按 login 实例分频道」，Java 状态为「不适用」，对应 Java 版本号 0.1.0-SNAPSHOT。
   工作区: 两个文件在 Java 仓都是 M：db_task.proto mtime 2026-10-16 02:41:32（save_seq 同步），PARITY.md mtime 2026-10-16 03:32:52（他人在追加）。与本项同批提交，只追加，不改他人行。
## compat_and_rollout
**兼容矩阵**：四种组合都能送达，而且都只发一次。
- 旧 login + 旧 go/db：就是现状。
- 旧 login + 新 go/db：读任务没有 10 号字段，reply_instance 为空。新 go/db 发旧频道，计 target=legacy_unset，旧 login 能收到。
- 新 login + 旧 go/db：旧 go/db 不认识 10 号字段，按 proto3 未知字段处理，解码时保留、不影响逻辑，照旧发旧频道。新 login 同时订着旧频道，能收到，计 outcome=legacy_notify。
- 新 login + 新 go/db：只发实例频道，只有发起方收到。

所以既不需要双发，也不需要配置开关。新 login 永久保留旧频道订阅，这是全局单点交接 §4.5 已经定下的。这条订阅是 go/db 随时回滚的保险；稳态下旧频道几乎没有流量，代价只是一个空闲订阅。

**滚动顺序**：任意顺序都安全。建议先全部 zone 的 go/db，再 login：
- go/db 先上时行为完全不变，所有读回执都是 legacy_unset，可以先确认新指标确实在采集。
- login 按 zone 做金丝雀。观察三项：db_task_result_notify_total{target=instance} 里 no_receiver 占比约为 0；login_task_result_delivery_total 中 salvaged、timed_out、result_missing 没有上升；entergame 的 preload 失败率不变。三项都正常再全量。

混跑期间，旧 login 发起的读仍然广播给所有 login，和今天一样，不会更差；新 login 发起的读，只要落到新 go/db 上，就只发实例频道。

**在途任务与重试**：读任务可能在重试队列里停留几分钟。重试载荷是 DBTask 整份重新序列化（key_ordered_consumer.go:2514），新旧 go/db 都会把 10 号字段原样带上。即使旧 go/db 丢了这个字段，也只是退回旧频道，新 login 照样能收到。

如果发起方 login 已经重启，实例频道上没有订阅者，通知落空，计 no_receiver。这和今天「广播出去没人认领」是等价的，结果 LIST 5 分钟后过期。

**回滚**：两端可以独立回滚，没有数据迁移，键名也不变（结果 LIST 仍是 task:result:{taskID}）。
- 回滚 go/db：全部退回旧频道，新 login 仍订着旧频道。
- 回滚 login：读任务不再带回复目标，新 go/db 走旧频道。

**收口**：本项没有需要以后删除的「过渡代码」。
- go/db 对空或不合法回复目标的旧频道回落永久保留，用来兼容旧 login 和工具，并有计数可查。
- login 的旧频道订阅永久保留，作为回滚保险。

「广播已经消失」的验收口径：全部 zone 升级后 7 天内，sum(rate(db_task_result_notify_total{target!=instance}[1h])) 约为 0，login 的 legacy_notify 也约为 0。之后如果出现非零，说明有新的生产者没填 reply_instance，由 DbTaskResultNotifyLegacyFallback 告警暴露。如果将来要删掉旧频道订阅，前提是回滚窗口内所有 go/db 版本都已包含本项，并且另开一个变更。

**与在途工作的接缝**：
- G4a 第 2/3 批要把 go/db 的结果发布和 login 的订阅、LPOP 挪到 CacheRedis。实例频道跟着 publishTaskResult 的 Redis 句柄走，必须和结果 LIST、login 的 Subscribe/LPOP 在同一个实例上。shared-redis-split-by-purpose.md §3.2.4 的「全服同一拍切」约束不变。
- NewTaskResultDispatcher 的签名不变，G4a 规划里的写法照用。
- 将来若选 A6 路线 C 上集群，再把实例频道切成 SPUBLISH/SSUBSCRIBE。那一步本来就是 go/db 和 login 全服同一拍切换（G4a §5.6 口径），不需要额外过渡。如果届时不是同拍切，就沿用本项「新 login 两边都订、发布方只发一次」的办法。

**不需要做的**：不需要停服，不需要迁移，不需要改 C++ 业务代码。C++ 只是随 proto 重生成后，重新编译依赖 db_task.pb.h 的工程。
## tests
- go/shared/taskresult/notify_test.go::TestValidReplyInstance（表驱动）。合法：36 字节 UUID、单字符、正好 64 字节。不合法：空串、65 字节、含 ':'、'*'、'?'、'['、空格、换行、非 ASCII（如「登录」）。修复前这个包不存在，属于编译红。
- go/shared/taskresult/notify_test.go::TestInstanceNotifyChannel。合法 id 返回 task:result:notify:<id> 和 true；不合法返回空串和 false。同时钉住 LegacyNotifyChannel 等于 task:result:notify，这是和已部署旧版二进制之间的线上契约。
- go/db/internal/kafka/task_result_notify_route_test.go::TestPublishDBReadResult_NotifiesReplyInstanceChannel。环境：miniredis.RunT 加 go-redis 客户端，客户端挂 publishRecorder（一个 ProcessHook）。构造 DBTask{Op:read, TaskId:t-1, Key:42, MsgType:合法子表名, ReplyInstance:固定 UUID}，经 publishDBReadResult（签名不变）发出。断言：恰好一条 PUBLISH，频道为 task:result:notify:<UUID>，载荷为 t-1；LPUSH 和 EXPIRE 的记录序号都在 PUBLISH 之前；task:result:t-1 列表里是 Success=true 的 TaskResult。修复前红：实际频道是 task:result:notify。
- go/db/internal/kafka/task_result_notify_route_test.go::TestFailReadTask_NotifiesReplyInstanceChannel。构造最小的 worker{ctx, redisClient}，调 failReadTask(&workerTask{}, dbTask 带 ReplyInstance, 原因串)。断言 PUBLISH 发到实例频道，列表里是 Success=false 且 Error 为该原因串的回执。修复前红。
- go/db/internal/kafka/task_result_notify_route_test.go::TestEncodeRetryPayload_PreservesReplyInstance。encodeRetryPayload(带 ReplyInstance 的任务, seq=7, partition=3, true)，再 unwrapRetryPayload，再 proto.Unmarshal，断言 ReplyInstance 不变。这是兼容守卫：从重试队列出来的读回执也要定向。
- go/db/internal/kafka/task_result_notify_test.go::TestPublishTaskResult_FallsBackToLegacyChannel（表驱动）。ReplyInstance 分别取空串、a:b、*、65 个 a，断言都发到旧频道，target 计数分别加在 legacy_unset、legacy_invalid、legacy_invalid、legacy_invalid 上。计数用 prometheus testutil 比较前后差值。空串这一例修复前就是绿的，作为兼容守卫。
- go/db/internal/kafka/task_result_notify_test.go::TestPublishTaskResult_NotifyFailureKeepsResult。让 hook 对 publish 返回错误。断言 publishTaskResult 返回 nil，列表里有结果，计 result=error。钉住的语义是：通知失败只拉长时延，结果由 TTL 兜底取回。
- go/db/internal/kafka/task_result_notify_test.go::TestPublishTaskResult_NoReceiverCounted。没有任何订阅者时，miniredis 的 PUBLISH 返回 0，断言计 target=instance、result=no_receiver。
- go/db/internal/metrics/task_result_notify_test.go::TestTaskResultNotifySeriesPreCreated。调 registerTaskResultNotify 后 Gather，断言 db_task_result_notify_total 恰好有 3×3=9 条序列且值都为 0，label 取值和常量逐字一致。
- go/login/internal/dispatcher/task_result_dispatcher_test.go::TestNewTaskResultDispatcher_ReplyInstanceValidAndUnique。两次构造得到的 ReplyInstance() 都能通过 taskresult.ValidReplyInstance，且彼此不同。修复前编译红。
- go/login/internal/dispatcher/task_result_dispatcher_test.go::TestNewTaskResultDispatcher_PanicsOnInvalidInstance。newTaskResultDispatcher(rc, ttl, "bad:id", rec) 应当 panic。
- go/login/internal/dispatcher/task_result_dispatcher_test.go::TestDispatch_InstanceChannelDeliversOwnTask 和 TestDispatch_LegacyChannelDeliversOwnTask。不 Start，即不起任何 goroutine。用注入的记录器作 onDelivery。先 Register(t1, 1h, cb)，再直接调 d.dispatch(ctx, 实例频道或旧频道, t1)。断言：pending 里没有 t1 了，dispatchJobs 里有 1 个 job，记录器得到 instance_notify 或 legacy_notify。然后在 miniredis 里预先 LPUSH task:result:t1，同步调 d.runDispatchJob(ctx, job)，断言回调同步拿到 Success=true。全程确定，没有等待。
- go/login/internal/dispatcher/task_result_dispatcher_test.go::TestDispatch_LegacyForeignTaskIgnored。旧频道上来一个不认识的 task，断言 pending 不变、没有 job、记录器没有记录。对应混跑期间别的 login 的任务。
- go/login/internal/dispatcher/task_result_dispatcher_test.go::TestDispatch_InstanceLateNotifyCounted。实例频道上来一个不认识的 task，断言记录器得到 late_notify。
- go/login/internal/dispatcher/task_result_dispatcher_test.go::TestRunDispatchJob_ResultMissingCounted。列表为空时调 runDispatchJob，断言回调收到包含 redis.Nil 的错误，记录器得到 result_missing。
- go/login/internal/dispatcher/task_result_dispatcher_test.go::TestStart_SubscribesLegacyAndInstanceChannels（miniredis 集成）。Start 之后在限定时间内（不超过 5 秒，每 10 毫秒轮询一次），断言 mr.PubSubNumSub(旧频道, 实例频道) 两个都等于 1。再 Register(t2) 并 LPUSH 结果，然后 mr.Publish(实例频道, t2)，断言回调在期限内到达。最后 Stop。这里的等待只用来等异步订阅建立，不拿时间判断对错。修复前红：没有订实例频道。
- go/login/internal/logic/pkg/dataloader/sync_loader_test.go::TestNewDBReadTask_StampsReplyInstance。断言 ReplyInstance 等于传入值，并钉住其他字段不变：Key=id、WhereCase=player_id='<id>'、Op=read、MsgType=子表全名、TaskId、Body 能反序列化回原消息。修复前编译红。
## verify_commands
- 【顺序 1｜proto 重生成】工作目录：D:\luyuan\xuanming-server-mmo-cpp（仓库根）。命令：pwsh -NoProfile -File tools/scripts/dev_tools.ps1 -Command proto-gen-run -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml。通过标准：退出码 0；Select-String -Path go/proto/db/db_task.pb.go -Pattern 'func \(x \*DBTask\) GetReplyInstance' 命中 1 次；cpp/generated/proto/db/db_task.pb.h 中含 reply_instance。生成器会顺带重生成其他会话在途的 proto 产物，不要回退。生成器 exe 落后于源码时，脚本会告警并改走 go run（见 PROGRESS 2026-10-16 proto-gen-run 条目），因此需要本机 Go。失败时保留完整控制台输出。
- 【顺序 2｜go/db 红证据，在写实现之前】实现方此时只落了这些：proto 新字段与重生成产物、go/shared/taskresult、go/db/internal/kafka/task_result_notify_route_test.go。工作目录：go/db。命令：$env:GOPROXY='off'; go test -count=1 -run 'TestPublishDBReadResult_NotifiesReplyInstanceChannel|TestFailReadTask_NotifiesReplyInstanceChannel' ./internal/kafka/。期望：FAIL，失败信息里实际频道为 task:result:notify。把输出保留为红证据。login 侧的新用例引用了新符号，修复前就是编译失败，记为红即可。
- 【顺序 3｜go/shared】工作目录：go/shared。命令：gofmt -l taskresult；go vet ./taskresult/；go test -count=1 ./taskresult/。通过标准：gofmt 无输出，vet 退出码 0，测试 ok。
- 【顺序 4｜go/db 绿】工作目录：go/db。命令：$env:GOPROXY='off'; gofmt -l internal/kafka/task_result_notify.go internal/kafka/task_result_notify_route_test.go internal/kafka/task_result_notify_test.go internal/kafka/placement_route.go internal/metrics/task_result_notify.go internal/metrics/task_result_notify_test.go internal/metrics/metrics.go；go vet ./internal/kafka/ ./internal/metrics/；go test -count=1 -run 'TaskResult|ReplyInstance' ./internal/kafka/ ./internal/metrics/；go test -count=1 ./internal/kafka/ ./internal/metrics/；go build ./...。通过标准：列出的文件 gofmt 无输出，vet 退出码 0，定向用例全部 PASS，build 退出码 0。整包测试如果有失败，逐条注明失败用例属于本项还是他人的在途改动（围栏、停机、R2），不要混在一起下结论。key_ordered_consumer.go 单独跑一次 gofmt -l，若因他人改动不整齐，只报告，不顺手格式化。缺 Go 模块时停下请示，不要改 GOPROXY。
- 【顺序 5｜go/login】工作目录：go/login。命令：$env:GOPROXY='off'; gofmt -l internal/dispatcher internal/logic/pkg/dataloader；go vet ./internal/dispatcher/ ./internal/logic/pkg/dataloader/；go test -count=1 ./internal/dispatcher/ ./internal/logic/pkg/dataloader/；go build ./...。通过标准：gofmt 无输出，vet 退出码 0，两个包 ok，build 退出码 0。可选：本机有 cgo/gcc 时再跑 go test -race -count=1 ./internal/dispatcher/。
- 【顺序 6｜C++，和围栏线 save_seq 的重生成合成一批，不单独跑】工作目录：仓库根。按仓库惯用的 Configuration/Platform 串行执行 msbuild cpp/generated/proto/proto.vcxproj /m:1，然后 cpp/libs/services/scene/scene.vcxproj /m:1，最后 cpp/nodes/scene/scene.vcxproj /m:1。通过标准：0 error。必须串行，并发会报假的 C1041/LNK1104。C++ 不设置 reply_instance，这一步只是重建依赖这个头文件的工程。
- 【顺序 7｜Java 契约同步】工作目录：D:\luyuan\xuanming-server-mmo。命令：java tools/ContractSync.java --mmorpg D:\luyuan\xuanming-server-mmo-cpp；java tools/ContractSync.java --mmorpg D:\luyuan\xuanming-server-mmo-cpp --check；./mvnw -pl game-proto -am clean install。通过标准：--check 退出码 0，mvn 构建成功。Java AGENTS §1 规定同步契约之后必须 clean install。
- 【可选｜本机 dev 真实 Redis 冒烟，不碰生产数据】起 login 和 go/db 后执行：redis-cli PUBSUB NUMSUB task:result:notify，结果应等于 login 进程数；redis-cli PUBSUB CHANNELS 'task:result:notify:*'，每个 login 一条，且和 login 启动日志里打印的 reply_instance 一致。对一个测试玩家删除父键 PlayerAllData:<id>，然后进场：go/db /metrics 上 db_task_result_notify_total{target="instance",result="ok"} 应增加 2（两张子表），进场成功。这一步只验证功能，不据此宣称任何性能结论（AGENTS §6）。
## doc_updates
- docs/design/shared-redis-split-by-purpose.md：:119 的表行改为「旧频道 task:result:notify（全服，兼容用）+ 实例频道 task:result:notify:<login 实例>（定向）」；§3.2.4（:307-311）改为「分频道已落码，未验证；实例频道跟随 publishTaskResult 的 Redis 句柄，必须与结果 LIST、login 的订阅和 LPOP 在同一实例」；新增「第 0 批：task:result 按 login 实例分频道」小节，写契约、兼容矩阵、指标、告警、回滚、收口，以及「先用 PUBLISH、SPUBLISH 随 A6 路线 C 切」的理由与代价；:1047 待决第 3 条标为已答复。
- docs/design/capacity-model-hok-scale.md：A0 第 5 条（:420）加注「已落码，未验证；当前用 PUBLISH，SPUBLISH 随 A6 路线 C（:556）一起切。理由：单主下两者等价；miniredis 不支持 S 系命令；集群切换本来就是全服同一拍」；§4.2「第 0 批」（:663）的 task:result 一项加状态；在 §7.2 表中加一行「task:result 全局广播」：投递量 = R×L，冷读场景下 R 与 L 都正比于 PCU，因此随 PCU² 增长，并附 2000 万口径的三个场景数字。
- docs/design/tidb-phase1-and-launch-readiness.md:842 的 L-06 行加状态注：「task:result 按 login 实例分频道已落码（未编译、未验证），见 PROGRESS 对应条目」。
- docs/PROGRESS.md 末尾另起一条，不改旧条目，标题注明「Claude，未编译、未测试，待 Codex 验证」。内容写：范围；文件清单；Java 状态（功能不适用，proto 已经 ContractSync 同步，对应 Java 版本号 0.1.0-SNAPSHOT）；交 Codex 的命令（即 verify_commands）；与围栏线、go/db 接手线的分工约定。
- docs/handoff/handoff-launch-readiness-and-d1-20261013.md 文首进度段追加一行，说明 §4 第 5 项「第 0 批」中的 task:result 分频道已落码，未验证。
- docs/handoff/godb-pipeline-handoff-20261013.md：:86 的表行和 §5 第 13 项（:228）加注「reply_instance=10 已落码」。
- Java 仓 PARITY.md 末尾追加一行：「task:result 读回执通知按 login 实例分频道」，Java 状态「不适用（进场按主键直接读库，没有 Kafka 读任务和 task:result；PARITY:162 同口径）」，对应版本 0.1.0-SNAPSHOT，并注明 proto 已经 ContractSync 机械同步。
- 可选，不计入本项必做：docs/handoff/handoff-global-singletons-10m-ccu-20261013.md §4.5（:151-157）加一行「字段号最终为 10，已落码」，该文件当前为 M；docs/design/new-player-first-enter-no-cold-read.md:82 的流程描述改为「PUBLISH 到发起方实例频道」；tools/scripts/k8s_deploy.ps1:1764 的注释改成「同一连接订旧频道和实例频道」，PubSub=1 不变，但该文件多个会话正在改，等空档再动。
## java_parity
**功能上不适用。** Java 版进场时按主键直接读库，没有经 Kafka 的读任务，也没有 task:result 回执。依据：PARITY.md:162 G3 那一行是同一口径；Java 源码（排除 build/ 和 target/）grep task:result 和 DBTask 都没有引用，已亲自核对。

**机械同步必须做。** 这次改的 proto/db/db_task.proto 在 Java 仓有一份 ContractSync 副本：game-proto/src/main/proto/proto/db/db_task.proto。Java AGENTS.md §1 规定这份副本不许手改，并且要两边同批提交。所以本项加了 10 号字段后，要在 Java 仓跑 ContractSync 并做 clean install。

这份副本在 Java 工作区已经是 M（save_seq 那次同步，mtime 2026-10-16 02:41），本项的改动和它同批提交。

**登记。** 在 PARITY.md 登记一行「不适用」，对应 Java 版本号 0.1.0-SNAPSHOT。

**附带观察（只读，频率未核实，不属于本项对齐范围）。** Java 的 game-login/src/main/java/com/game/login/ownership/RedisOwnerTakeovers.java:11-14、:27-39 会把归属接管请求发到一个全局 topic，所有 scene 节点都订阅它，由持有该归属的节点处理。这和本项是同一种「全局广播、接收端过滤」形状，但功能不同。建议 Java 侧另立项，按接管频率和 scene 节点数估算投递量，再决定是否改成定向投递。
## risks
- **在途冲突与分工。** 涉及四个文件：
- key_ordered_consumer.go：已有 121+/14- 的在途改动，来自停机③④修复、围栏、资产 SQL 提交证据。go/db 接手线的 R2（A4，S1 规格）还会大改这个文件，按 godb 实施交接 §3 要和 R1、R3 串行。本项只动 :41-45、:465、:474-496，和现有 hunk 以及 S1 规格清单都不重叠。:465 和围栏线 v0.4 计划在 publishDBReadResult 里改的缓存 NX 是同一函数里的相邻 hunk。
- metrics.go：R2 会改 register()。本项只在 Start() 里加一行。
- placement_route.go：没有在途改动。
- db_task.proto：围栏线的 save_seq=9 还没提交；它计划加的「10 号预留」注释要改成直接引用本字段。

建议：本线现在落码，用精确的 Edit 只改上述几处，落码前把文件和函数清单发给 go/db 接手线和围栏接手线；proto 重生成和 C++ 编译跟 save_seq 合成一批。如果负责人要求在该文件上严格串行，就把 go/db 部分排到 R2 之后，shared 和 login 部分不依赖它，可以先落。
- **两端频道名不一致。** 后果是所有实例频道通知都落空，每次冷读都要等满 8 秒 TTL，再靠兜底取回：进场变慢，player_locker 持锁时间变长。缓解措施：频道名只从 go/shared/taskresult 一处生成；用 no_receiver 计数和 DbTaskResultNotifyNoReceiver 告警盯着；上线顺序是先 go/db、再 login 按 zone 金丝雀；出问题时任意一侧回滚都能立即恢复。
- **通知丢失仍然存在。** 剩下的几种原因：go/db 在 LPUSH 之后、PUBLISH 之前崩溃；PUBLISH 报错；订阅连接自动重连的窗口；Redis 切主。兜底仍是 TTL 到期时 LPOP（EnterGame 8 秒）。本项把「本地通道溢出（8192 条，满 5 秒后丢）」和「Redis 输出缓冲把订阅端踢掉」这一主因降了 L 倍，但其他原因没有消除。

要不要加更快的兜底（例如注册时间过半时探测一次，或重新订阅后补扫一遍），等新指标 salvaged、timed_out 的实测再定。每次探测都是一次额外的 LPOP，积压时会放大 Redis 压力，所以不预先做。

没有采纳的替代方案：给每个 login 实例建一个 LIST 收件箱、用 BRPOP 取。这样不会丢通知，但要改结果存储契约，混跑期还需要双写，超出本项范围。
- **和容量模型 A0-5 的偏离。** A0-5 写的是「直接用 SPUBLISH」，本项用 PUBLISH。如果负责人坚持现在就用 SPUBLISH，需要的改动有：
- Redis 不低于 7.0：K8s 是 7.2，满足；compose 用 latest。
- login 用 SSubscribe 订实例频道，和旧频道的 Subscribe 混在同一个 PubSub 上。
- go/db 的测试改用 hook 拦截，不能在 miniredis 上真发，因为三个版本的 miniredis 都不支持 S 系命令。
- login 的订阅集成测试需要真 Redis。
- **跨 Redis 拓扑。** 回复目标只标识 login 进程，不标识 Redis 实例。今天全服共用一套核心 Redis，所以没问题。G4a 拆实例或单元化之后，发布方写结果的 Redis 必须和发起方 login 订阅的是同一个（CZ-2），否则通知和结果都会落空。这是 G4a §3.2.4「全服同一拍切」已有的约束，实例频道既没有放宽它，也没有加重它。result_missing 计数可以暴露 LIST 和频道不在同一实例的配置错误。
- **启动竞态。** dispatcher.Start 是异步订阅，订阅确认之前发出的读任务可能收不到通知。今天对旧频道也存在同样的问题，靠 TTL 兜底。本项不改。
- **proto 重生成是全量的。** 会把其他会话在途的 proto 改动一起生成出来，Codex 不能回退。db_task.pb.h 变了，依赖它的 C++ 工程要重新编译，和 save_seq 合成一批。
- **TriggerPlayerDataPreload 签名变化。** 它目前没有调用方，但签名随本项改了。将来如果有人传空串调用，这类回执会退回全服广播；legacy_unset 计数和 info 级告警能看到。建议另开小任务删掉这段死代码，不在本项里夹带。
- **告警的时机。** DbTaskResultNotifyLegacyFallback 在 login 全量升级之前一定会触发，要等全量后再 apply，或者先作为 info 级观察。
- **容量数字都是推算（[I]）。** 每次 Pub/Sub 投递在 Redis 和 login 上的 CPU 成本没测过。开服场景的 C_p 沿用 S3 题设，§7.2 还没按 2000 万重估。按 AGENTS §6，没有压测对比表之前，不能宣称性能有提升。
- **Redis 默认的 pubsub 输出缓冲上限**（32mb/8mb/60s，外部常识；redis.yaml 未覆盖）。分频道之前，冷读高峰时每个 login 入站 1.9~8.5 MB/s，订阅端卡 4~5 秒就可能被 Redis 断开，导致成批通知丢失。分频道之后，开新区时热点 zone 的单个 login 入站也只有约 0.3 MB/s。这也是本项必须排在冷玩家卸载上线之前的理由，全局单点交接 §4.5 要求两者同批排期。
## open_decisions
- go/db 部分由哪条线落码、什么时候落码，需要负责人确认，并通知 go/db 接手线和围栏接手线。背景：godb 实施交接 §3 要求 R1→R2→R3 在 key_ordered_consumer.go 和 metrics.go 上串行，R2（A4）还没开始。本项在该文件只动 :41-45、:465、:474-496，和全部在途 hunk 以及 S1 规格清单都不重叠；新指标放独立文件，metrics.go 只在 Start() 里加一行。推荐由本线现在落码，并把清单发给两条线。备选是严格串行：go/db 部分排到 R2 之后，由 go/db 接手线顺手做。两种方式下，go/shared 和 login 部分都可以先落。

# VERDICT
## overall
结论：规格的事实底座基本可靠，设计方向也没有发现会导致数据错误或送达失败的缺陷，可以照做；但落码之前要先补上上面的 must 和若干 should。

**事实核对**
- 共 17 条，16 条 confirmed。我亲自打开了 key_ordered_consumer.go:41-45/:441-496、placement_route.go:350-357、task_result_dispatcher.go 全文、sync_loader.go 全文、ensure_player_all_data_async.go:39-215、db_task.proto 及其 diff、redis.yaml、login.yaml、go-redis、miniredis、go-zero 的模块源码，以及 Java 仓的 PARITY 和 proto。
- 唯一 refuted 的是「在途 hunk 不重叠」这条的证据快照。key_ordered_consumer.go 在规格写完后又被改过（04:17:53，现在 152+/15-）。此刻仍没有 hunk 落在本项要改的行上，但结论已不能直接沿用。
- 多处行号已经漂移：db-write-fencing-mysql.md:950/:450、k8s_deploy.ps1:1764、encodeRetryPayload :2513、uuid :87。
- 容量数字已复算，与 §0.3 / §2.4 / §7.2 自洽：稳态 5.6万~12.4万/s，冷卸载上线后 889万~1976万/s，开服 800万~8890万/s。这些都是推算，没有压测支撑。

**设计层面**
- 「新 login 双订、go/db 单发、空串或不合法回落旧频道」在四种新旧组合下都能送达，两端可以各自独立回滚。
- 重试载荷保留未知字段，全仓也没有 DiscardUnknown；跨 zone 读（CZ-2）在共享 Redis 下成立；SUBSCRIBE 一条命令订两个频道，加上 go-redis 重连时整体重订，语义也成立。
- 与 G4a 的 CacheRedis 接缝相容。Java 判为功能不适用、只需 ContractSync 同步，这个判断成立。

**主要问题**
1. must：生成物算进去后改动约 35 个文件，越过 AGENTS §10.2 的 30 文件门槛。需要负责人授权或正式拆批。
2. 冲突分析过期，而且没交代 R1/G6 正在同一文件上改。
3. 新告警混进待 apply 的 L-04 告警文件，会提前误报，也会和 S4 / 第 3 批在文件末尾冲突。
4. go/db 计数测试方案违反仓库约定（「不用 testutil」），且计数器不会自注册，测试读不到值。
5. 偏离 A0-5 SPUBLISH 的决定没有列进 open_decisions。
6. 围栏设计文档里「10 号不得占用」的定稿注释没有纳入同步。
7. 新测试没有进 go/test.ps1 的 L1，新指标没有进 stress_summarize。
8. 兜底计数缺少不依赖墙钟的测试。

**建议落码顺序**
- 先拿到负责人对文件数与 SPUBLISH 偏离的确认。
- 第 1 批：proto、go/shared、login（与 go/db 无文件重叠）。
- 第 2 批：go/db 加独立告警文件。等 R1/G6 当前持有者给出空档后，按函数名精确编辑落码。
- 交付说明如实写「未编译、未测试，待 Codex 验证」。
## fact_checks
- [confirmed] 发布方只有一个全局频道 task:result:notify，载荷只有 task_id；先 LPUSH task:result:{id}、再 EXPIRE 5 分钟、最后 PUBLISH；PUBLISH 失败只打日志并 return nil
   亲自打开工作区 go/db/internal/kafka/key_ordered_consumer.go:40（taskResultExpireDuration=5m）、:41-45（常量）、:476-496（publishTaskResult）、:492-494（Publish 出错只 logx.Errorf，仍 return nil）。补充：LPUSH、EXPIRE 失败会返回 error（:482-487），调用方因此延后重试并重复 LPUSH，属既有语义。
- [confirmed] 只有读任务发回执，调用点两处：成功回执 publishDBReadResult、落点失败回执 failRead；写路径不发
   亲自打开 key_ordered_consumer.go:441-472（:458 TaskId 非空才发，:465 调用）、placement_route.go:350-357（:353 调用）、key_ordered_consumer.go:994-1021 publishDBWriteResult 不发回执。全仓 grep publishTaskResult( 只有这两处，测试里只有 placement_route_test.go:152 直接读 task:result 列表。
- [confirmed] login 用一条 PubSub 连接只订全局频道；本地缓冲 8192 条，满了等 5 秒后丢弃
   亲自打开 go/login/internal/dispatcher/task_result_dispatcher.go:32、:227、:231、:248-251。另核对模块缓存里的 go-redis v9.16.0 pubsub.go:724-741：channel 满且超时后 Logger.Printf「message is dropped」。
- [confirmed] 过滤方式：每个 login 处理全量通知，持锁 take(taskID)，查不到就丢掉
   亲自打开 task_result_dispatcher.go:161-173（take 持 mutex 删除 map 项）、:280-286（cb==nil 直接 return，注释写明 pub/sub 会广播给所有订阅者）。
- [confirmed] 兜底：每秒扫一次，到期时做最后一次 LPOP；EnterGame TTL 8s、默认 30s；断线重连窗口内的通知会丢
   亲自打开 task_result_dispatcher.go:203（gcInterval=1s）、:347-383（sweepExpired 中的 LPOP 兜底）；go/login/internal/logic/clientplayerlogin/entergamelogic.go:421；go/login/internal/svc/servicecontext.go:211（30*time.Second）；go-redis v9.16.0 pubsub.go:20-21（注释写自动重连并重订）、:110-130 resubscribe，会把 channels/patterns/schannels 全部重订。补充一处细节：sweep 和消息读取在同一个 select 里（:252-263）；subscribeLoop 出错后退避的那 1 秒内（:215-219）不会扫描，属既有行为。
- [confirmed] 读任务只有一个生产点 sendDBReadRequests；DBTask 不带回复目标；每个冷读玩家发 2 条
   亲自打开 sync_loader.go:81-110（DBTask 字面量在 :95-102）、:77（TriggerPlayerDataPreload，全仓 grep 没有调用方）；ensure_player_all_data_async.go:194。uuid.NewString 实际在 :86，规格写成了 :87。new_player_seed.go:25-28 是两张子表；player_lifecycle.cpp:4422 是 set_op("write")；data_stress/main.go:217 是 Op:"write"。git grep 'DBTask{' 的生产代码里，只有 sync_loader.go 发 read。
- [confirmed] 字段号已定为 10；save_seq=9 在工作区、未提交；PROGRESS:7745 与 db-write-fencing-mysql.md:950 同口径
   proto/db/db_task.proto:21 为 save_seq=9，git diff 3 行（+2/-1，含文件末尾换行），未提交；docs/PROGRESS.md:7745 第 13 项已亲自核对。规格引用的 db-write-fencing-mysql.md:950 已失效：该文件 2026-10-16 04:16 又被改过，950 行现在是回滚表。字段号结论实际在 :54、:155、:158（「注释定稿……10 号预留给 reply_instance，不得占用」）和 :1264。player_row_fence.go 是 untracked（??），fence-inflight.md:135-139 计划放弃这份实现。
- [confirmed] login 每 zone 2 副本、CPU 上限 1 核；按 §7.2 口径
   deploy/k8s/manifests/go-svc/login.yaml:24 replicas: 2，:86-90 requests 200m、limits "1"；k8s_deploy.ps1:1770 login 为 Scope='zone'。capacity-model-hok-scale.md §7.2 从 :894 起，E=2000万/1800≈11111/s；§0.3 在 :34-60，按 C_n=DAU×2%/86400×3≈69/s 算出 2C_n≈139/s；§2.4（:242）的 S3 列是 2.8万~6.2万，与规格按 L=400~890 推出的 5.6万~12.4万相互印证。以上数字已复算，均为推算（[I]）。
- [confirmed] K8s 上是 Redis 7.2 单主，未覆盖 pubsub 输出缓冲；login 与 go/db 都用单机客户端
   redis.yaml:90 image redis:7.2；:131、:133 两条 exec 只带 --appendonly、--maxclients（和 --requirepass）；文件头 :1-20 写明全服所有 zone 连这一个实例。go/login/internal/svc/servicecontext.go:119 和 go/db/internal/svc/servicecontext.go:24 都是 redis.NewClient。Redis 默认 client-output-buffer-limit pubsub 32mb 8mb 60 在仓库内无法核实。
- [confirmed] miniredis 不支持 SPUBLISH/SSUBSCRIBE；go-redis 支持 SSubscribe
   模块缓存里 miniredis v2.35.0 grep SPUBLISH/SSUBSCRIBE 零命中；v2.35.0 有 direct.go:737 Publish 与 :756 PubSubNumSub，规格测试要用的 API 存在。go-redis v9.16.0 universal.go:315、redis.go:1186 有 SSubscribe。
- [confirmed] 容量模型对 SPUBLISH 口径前后不一（A0-5 现在就用，A6 只列为路线 C 一项）
   capacity-model-hok-scale.md:420「直接用 SPUBLISH 形态」；:543-556 A6 写「二选一」，路线 C 下列有「Pub/Sub 改成 SPUBLISH」。文中没有记录 R/C 的拍板结论。
- [confirmed] Redis 连接总账给每个 login 记 1 条 PubSub 连接（k8s_deploy.ps1:1764-1765）
   内容属实，行号已漂到 tools/scripts/k8s_deploy.ps1:1769-1770，注释引用的是 Subscribe(TaskResultNotifyChannel)。该文件 git status 为 M（+825 行），mtime 2026-10-16 04:18:12，有其他会话正在改。
- [confirmed] 重试链路把 DBTask 整份重新序列化
   encodeRetryPayload 现在位于 key_ordered_consumer.go:2527-2531（规格写 :2513-2516，已漂移），proto.Marshal(task)。go/db、go/login、go/shared 都没有对 DBTask 用 DiscardUnknown（grep 只命中 nodeinfo 的 protojson）。
- [refuted] key_ordered_consumer.go 在途改动 121+/14-，mtime 2026-10-15 23:12:55，没有一处 hunk 落在本项要改的行上
   规格引用的快照已经过时。现在 mtime 是 2026-10-16 04:17:53，git diff --stat 为 152+/15-。新增的 hunk 在 ConsumeClaim :2162-2190 一带（撤拉取时转 drainClaimForShutdown，属 G6 停机/R1 一线），Stop 一带在 :2717-2817。结论层面，此刻仍没有 hunk 落在 :38-53 或 :441-496，但这个文件正被实时修改，规格列出的 hunk 表与 diff 统计都不能再用。godb-pipeline-impl-handoff-20261015.md（untracked）确认 A4 未开始，但没有交代 R1（核实补完 A3）的当前状态。
- [confirmed] Java 版功能不适用，只需机械同步 proto 副本
   D:\luyuan\xuanming-server-mmo\PARITY.md:162（新号免冷读）写的是「不适用……没有经 Kafka 的冷读」。Java 源码（排除 build/ 与 target/）grep task:result、DBTask 零命中。Java AGENTS.md:16-20 规定 ContractSync、不许手改，:79 规定同步后 clean install。game-proto/.../db/db_task.proto 为 M（save_seq 已同步），mtime 02:41:32。PARITY.md 现在 mtime 04:20:15，晚于规格，有他人在追加。RedisOwnerTakeovers.java:11-14、:27-39 那条附带观察也属实。
- [confirmed] placement_route.go 无在途改动，S1/R2 清单不含它
   git status 干净，mtime 2026-10-15 14:39:10。S4 规格 :415 也改过这个文件（ObserveRedisStep），但已入库（placement_route.go:247、:277 已有该调用），所以没有在途冲突。
- [confirmed] go/login/internal/dispatcher 包此前没有任何测试；go/shared/taskresult 目录不存在；go.mod 已有 replace shared
   ls 显示 dispatcher/ 下只有 task_result_dispatcher.go；go/shared/taskresult 不存在；go/db/go.mod:126 和 go/login/go.mod:119 都有 replace shared => ../shared；go/shared/go.mod 的 module 名是 shared。
- [confirmed] 已有工作：代码层面没有任何会话落地 reply_instance
   在主工作区和 .claude/worktrees/* 下 grep reply_instance、ReplyInstance、InstanceNotifyChannel，.go/.proto 里零命中；PROGRESS 中 7745 行之后也没有 task:result 分频道的落码条目。
## plan_problems
- [must] 改动规模越过 AGENTS.md §10.2 的「预计改动 30+ 文件立即停止并报告」门槛，规格却报 est_files=27。规格手工改的文件约 26 个（含 Java 仓 2 个），proto 重生成还会再改至少 7 个：go/proto/db/db_task.pb.go、cpp/generated/proto/db/db_task.pb.{h,cc}、generated/proto/{_unified,db,login}/proto/db/db_task.proto、generated/robot/proto/db/db_task.proto。再加上下文建议补的 go/test.ps1、launch-todo、stress_summarize、围栏设计文档，总数约 35。
   修法: 在 open_decisions 里写明需要负责人授权超过 30 个文件；或者正式拆批：第 1 批是 proto、go/shared/taskresult、login 侧（与 go/db 无文件重叠），第 2 批是 go/db 侧加告警，在 R1/R2 空档落。est_files 按真实数（含生成物）重算。
- [should] 工作区冲突分析基于过期快照。key_ordered_consumer.go 在规格写完后又被改了（04:17:53，152+/15-，新增 ConsumeClaim 撤拉取 hunk）；db-write-fencing-mysql.md（04:16）和 k8s_deploy.ps1（04:18）也在实时变化，规格里的行号 :950、:450、:1764、:2513 都已失效。规格只说 R2 未开始，没有交代 R1（核实补完 A3、停机 G6）正在同一文件上动。
   修法: 落码前立刻重跑 git status --short、git diff -U0 和 stat，按函数名重新定位（publishTaskResult、publishDBReadResult、常量块 taskResultNotifyChannel），不按行号定位。先向 R1/G6 当前持有者和围栏接手线确认空档，用精确 old_string 编辑。规格里的 hunk 表改成「落码时现查」，不再写死。
- [should] 新告警组追加进 deploy/k8s/db-task-retry-alerts.yaml，和 L-04 的 5 条告警是同一个 PrometheusRule。按 docs/ops/godb-pipeline-launch-todo.md 2.5.2，这个文件「待做 apply」；只要为 L-04 apply，DbTaskResultNotifyLegacyFallback 就会在 login 全量升级之前触发，和规格「全量后再 apply」的要求做不到分开。另外，S4 规格 :482-489 计划给同一文件追加 db-task.ordering-lease 组（目前还没出现在文件里），两边都在文件末尾追加，会冲突。
   修法: 新建独立的 deploy/k8s/task-result-notify-alerts.yaml（PrometheusRule 名独立），文件头写明 apply 时机。或者 LegacyFallback 只在 instance 流量已存在时才判，例如 `... > 0 and on(namespace) sum by(namespace)(rate(db_task_result_notify_total{target="instance"}[30m])) > 0`。同时把冲突登记给 godb 第 3 批 D1。
- [should] go/db 计数测试的方案和仓库约定冲突，而且读不到值。规格写「用 prometheus testutil 比较前后差值」，但 go/db/internal/kafka/instrumentation_test.go:6 明确规定经 prometheus.DefaultGatherer 读、不 import testutil（避免改 go.sum）；kafka 包的测试也碰不到 metrics 包里未导出的 vec。规格只在 metrics.Start() 里调 registerTaskResultNotify()，而 Start 在 addr 为空时提前返回（metrics.go:627-631），kafka 测试也不调 Start，所以 Gather 拿不到这个序列，断言恒为 0，与行为无关。
   修法: 仿照 pipeline.go:154-205：CountTaskResultNotify 内部先调 registerTaskResultNotify()（sync.Once）。kafka 测试复用 instrumentation_test.go:27-68 的 metricSample/metricCount（经 DefaultGatherer）比较前后差值。去掉对 testutil 的依赖。
- [should] 偏离容量模型 A0-5「直接用 SPUBLISH」这一已写入设计的决定，只放在 risks 里，没有列进 open_decisions。按 AGENTS §11 和 §5，偏离已文档化的决定要由负责人确认并记录原因与代价。
   修法: 在 open_decisions 增加一条：「A0-5 的 SPUBLISH 推迟到 A6 路线 C，先用 PUBLISH」，附理由（单主下两者等价；miniredis 不支持 S 系命令；切集群本来就要全服同一拍）和代价（切集群时 go/db 与 login 要再同拍改一次；混跑期须沿用双订单发）。负责人确认后再改 capacity 文档 :420。
- [should] 围栏线的「注释定稿」要求落码时照抄「10 号预留给……reply_instance，不得占用」（db-write-fencing-mysql.md:158），同文件 :54、:155、:1264 也写着 10 号预留。本项一旦占用 10 号，围栏线照抄就会在 proto 里留下自相矛盾的注释。规格只写了「通知围栏线」，doc_updates 没有改这份设计文档。
   修法: doc_updates 加入 docs/design/db-write-fencing-mysql.md §3.1（:155-158）和 §… :1264：改为「10 号已由 reply_instance 使用（见 PROGRESS 对应条目）」。该文件为 M、由围栏线持有，改之前先和围栏接手线约定由谁改。两条线谁先落码，谁就在 db_task.proto 写最终注释。
- [should] 新增的包和测试没有接入 L1 绿集。go/test.ps1 的 $L1Suites 只含 shared 的 ./placement、./kafkautil，login 的 ./internal/kafka，以及 db 的 ./internal/kafka 等；go/shared/taskresult、go/login/internal/dispatcher、go/login/internal/logic/pkg/dataloader、go/db/internal/metrics 都不在里面，新测试不会进常规回归。
   修法: 在 go/test.ps1 的 shared 套件加 './taskresult/...'，login 套件加 './internal/dispatcher/...'、'./internal/logic/pkg/dataloader/...'（只用进程内 miniredis，属于 hermetic）；db 是否加 './internal/metrics/...' 交 Codex 先确认是否稳定为绿。同步更新文件头注释里的清单。
- [should] 可观测性没有接到压测复盘入口。AGENTS §6.1 规定压测复盘只读 stress_summarize.ps1 的输出。本项新增的 salvaged、timed_out、no_receiver 正是 Round 13「notify dropped, data present」的直接信号，但规格没有改 tools/scripts/stress_summarize.ps1（:302 起已有 dataloader 段），复盘时看不到。
   修法: 在 stress_summarize.ps1 的 dataloader 段或 DB 段追加两张小表：login_task_result_delivery_total 按 outcome 汇总（读 :9101 快照），db_task_result_notify_total 按 target×result 汇总（读 :9160 快照）。test_stress_summarize.ps1 同步加用例。若认为超出范围，就在 PROGRESS 中明确登记为后续项。
- [should] sweepExpired 的 salvaged 和 timed_out 计数没有测试。tests 只覆盖 dispatch、runDispatchJob 和订阅；兜底路径是本项「通知丢失仍存在」风险的唯一缓解手段，却没有面向行为的用例。
   修法: 在同包测试里直接往 d.pending 写入 expiresAt 已过的条目（不 sleep、不依赖墙钟），同步调用 d.sweepExpired(ctx)。一例预先 LPUSH 结果，断言回调拿到结果并记 salvaged；另一例列表为空，断言回调收到超时错误并记 timed_out。回调在 safego.Go 里执行，用带上限的 channel 等待；记录器要加互斥。
- [nit] DbTaskResultNotifyNoReceiver 按 namespace 求比例，没有设最低流量门槛。稳态下每个 zone 读回执约 0.3/s 甚至更少，一次 login 重启留下的几个 no_receiver 就可能让比例超过 5%，再持续 10 分钟就是误报。
   修法: 表达式加上 `and on(namespace) sum by(namespace)(increase(db_task_result_notify_total{target="instance"}[10m])) > 50` 之类的最低量门槛（取值待定）。
- [nit] notifyLogLimiter 复用 logLimiter，它的 last、held 两个 map 没有上界（store_failure.go:118-138）。规格没写限速 key 取什么；如果实现时用了 taskID 或 reply_instance 作 key，map 会无限增长。
   修法: 规格写死：key 只能是 target+"/"+result 这类常量组合。
- [nit] 顺序 2 要拿断言层面的红证据，但 go test -run 仍要求整个 internal/kafka 包能编译。包里有其他会话在途的测试文件（consumer_shutdown_test.go 为 M，player_row_fence_integration_test.go、player_cache_publish_test.go 为 ??），它们编不过时就拿不到红证据，失败也会被误判为本项问题。
   修法: verify_commands 顺序 2 补一句：包编译失败时先确认失败源属于哪条线，记为「红证据未取得」，不能当作本项的红。
- [nit] login 侧多数「修复前红」只是编译红，真正验证行为的只有 TestStart_SubscribesLegacyAndInstanceChannels；TestStart 的 onDelivery 记录器跨 goroutine 调用，规格没要求线程安全。
   修法: 注明记录器要用 mutex 或 channel；能在不引用新符号的前提下取得断言红的用例，单独放一个文件（与 go/db route_test 同法）。
- [nit] 重生成说明只列了 go/proto 和 cpp/generated 两类产物，漏掉 generated/proto/**/db_task.proto 四份副本和 robot/vendor/proto/db/db_task.pb.go（vendor 副本连 save_seq 都还没有）。
   修法: verify_commands 顺序 1 写明这些副本都应变化。robot 不用 DBTask，vendor 不同步只需在交付说明里注明。
## missed
- go/test.ps1 的 $L1Suites 没有加入 shared ./taskresult/...、login ./internal/dispatcher/... 和 ./internal/logic/pkg/dataloader/...（以及视情况加 db ./internal/metrics/...）
- tools/scripts/stress_summarize.ps1（及 test_stress_summarize.ps1）没有汇总 db_task_result_notify_total 与 login_task_result_delivery_total（AGENTS §6.1）
- docs/ops/godb-pipeline-launch-todo.md 没有登记新告警的 apply 时机与金丝雀观察项（现有 2.5.2 只覆盖 L-04 的告警文件）
- docs/design/db-write-fencing-mysql.md:54、:155-158、:1264 的「10 号预留、不得占用」口径未纳入 doc_updates（文件为 M，归围栏线）
- deploy/k8s/db-task-retry-alerts.yaml 上还有待落的 S4 / 第 3 批改动（S4 规格 :482-489 的 db-task.ordering-lease 组），两边都在文件末尾追加，会冲突，规格写成了「干净、无冲突」
- docs/handoff/handoff-global-singletons-10m-ccu-20261013.md:152 原文仍写 `reply_instance = 9`，规格把订正列为可选；它是本项的原始设计出处，应改为必做（加一行注）
- 生成物副本没有列出：generated/proto/{_unified,db,login}/proto/db/db_task.proto、generated/robot/proto/db/db_task.proto、robot/vendor/proto/db/db_task.pb.go
- sweepExpired 兜底路径（salvaged / timed_out）缺少不依赖墙钟的行为测试
- dispatch 的饱和回退路径（task_result_dispatcher.go:292-300）仍不可观测；:81 注释引用的 pubsub_dispatch_overflow_total 在仓库里不存在。可考虑在 outcome 集合中加 pool_overflow（可选）
- 容量场景漏了「合服回流」冷读高峰（全局单点交接 §4.5 提到 tools/merge_zone/player_rows.go 会 DEL 父键）
- tools/scripts/k8s_deploy.ps1:1769 注释引用的符号 TaskResultNotifyChannel 将被删除，注释会失效（规格列为可选，行号已从 :1764 漂到 :1769）
