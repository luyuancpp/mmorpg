# 落码规格:D1 第 1b-2 / 1b-3 / 第 2 / 第 3 批 与最小设计修订 M1–M5(2026-10-18)

> **状态**:落码规格(Claude,2026-10-18)。**未编译、未测试**(AGENTS.md §10.1)。1b-1 批由另一会话「玩家数据存储位置」落码,不在本规格内。
> **以谁为准**:设计结论最终并入 [提案](../../design/player-route-mapping-durability-proposal.md) §13;并入之前以本文为准,并入之后本文只作落码快照。
> **底稿**:1b-2 的大部分细节沿用 [spec_D1_batch1b_scout.md](spec_D1_batch1b_scout.md) 的 1b-2 部分,并**必须**吸收其「# VERDICT」的两条 must 与全部 should(下文 §4.2 逐条列出取舍)。本文只写相对它的增量与第 2、3 批。
> **负责人既有拍板**:库为真源 + 完整副本 + 就绪标记(10-12);1b-2 先用单个就绪标记 + 指标、回档检测容差取 0(10-17)。负责人 10-18 授权「按最标准的做,不一定按任务原文」——本文与任务原文不同的取舍都记在 §10。

> **2026-10-18 评审吸收**:三视角对抗评审(安全并发 / 代码事实 / 上线运维)共 11 条 must、20 余条 should。正文凡被改动之处以「(评审)」标出,逐条取舍见 §13。**§13 与正文冲突时以 §13 为准。**

## 0. 分工与文件边界(同一工作区多会话)

> 2026-10-18 更新:1b-1 已由「玩家数据存储位置」会话落码完毕并把 D1 全部文件移交本规格;下面的边界只剩 `tools/scripts/k8s_zone_rollback.ps1`(另一项改动正在重写它,不再 FLUSHDB,改由 zone_rollback_discard 处置)——本规格不碰,回档相关只在设计 / runbook 登记。

- **1b-1 归「玩家数据存储位置」会话**,它在改:`internal/routetoolcli/`、`cmd/route_tool/`、`data_service.go`、`data_service_entry_test.go`、`internal/store/route_store*.go`、`internal/routetool/`、`shared/placement/route.go` 与 `route_test.go`、`internal/routing/route_publish.go`、提案 §11.2、runbook §9.3、`zone_data_rollback.md`。**它发来最终清单之前,本规格的任何落码都不碰这些文件。**
- 本规格里必须改上述文件的点(AST 守卫允许集合、cli 的 `rewarm` / `audit -require-maprev`、`data_service.go` 启停 leader、unfreeze 读水位键)单列在 §4.6「1b-1 收尾后才能做的接线」,等清单到了再做。

## 1. 结论:已批准方案里会写错库的窗口

已批准方案的读方判据是「`mapping:route:ready` 存在 ⇒ 副本完整可信」(`shared/placement/route.go` 的 RouteReadyKey 注释:读方只看存在),标记只由对账 leader 在下一拍发现身份变化后撤掉。于是「Redis 的数据已经回退或缺失、标记还在」到「leader 下一拍撤标记」之间(至少一拍;leader 卡住、连不上 Redis 或全挂时没有上界),读方会把回退后的值或缺键当成权威:

| 场景 | 标记 | 数据 | 读方后果(第 3 批读方门禁打开之后) |
|---|---|---|---|
| Redis 从旧备份恢复(运维操作) | 备份里的旧标记 | 备份时刻的旧值;之后的登记、搬库、合服全缺 | 缺键当权威缺席 → go/db 走 legacy 写本进程 zone 库;旧落点写旧库 |
| 哨兵把落后的副本提成主(redis-ha,`WAITAOF 1 1` 的确认可能在被提升的副本上不存在) | 复制过来的旧标记 | 丢了一段已确认的写 | 丢了 R5 回到冻结态尚安全;丢到 R1 之前就会写旧库 |
| FLUSHALL / FLUSHDB 发生在全量预热途中 | 预热完成后照样挂上 | 预热前半段推的行被清空 | 缺键当权威缺席 |
| 旧 leader(库里租约已过期、本地还没察觉)迟到挂标记 | 迟到的 SET NX | 新 leader 正在修补 / 预热 | 同上 |
| 映射实例被 SCAN + DUMP/RESTORE 部分拷贝(含标记) | 拷过来的标记 | 漏拷的键 | 同上 |
| 库回档,而 ArmReadyMarker=false(P2→P3) | 不存在 | 库比缓存旧 | leader 没有回档判据(两条判据都依赖标记),水位被 GREATEST 洗掉 |

「每秒对账最终修好」消除不了这些窗口,所以先做最小设计修订(§2)。修订都**不改 DDL、不改已有 Redis 键的格式、不改客户端契约**,只新增两把 Redis 控制键和一段只读 Lua。

## 2. 最小设计修订 M1–M5

### M1 读方把就绪标记绑定到「本次读所在的 Redis 实例」(关键修订)

读方信任副本的条件由「标记存在」收紧为:在**同一段只读 Lua**里取回标记原值、本实例身份与映射键;Go 侧解码后要求:

1. 标记存在、是 string、能被 `placement.DecodeRouteReadyMarker` 解码(v 大于已知版本 → 不可信,不删);
2. 实例 `role == master`;
3. `marker.redis_run_id == INFO server 的 run_id` 且 `marker.master_replid == INFO replication 的 master_replid`。

任一不满足即「副本不可信」(verdict 见 §5.1)。不可信时**值与缺键都不可信**:go/db 写任务延后、不耗重试预算,读任务耗预算;data_service 回源 SQL;guild 当作封锁。**不可信时缺键绝不解释为「没有归属」,也绝不回退到当前进程库写入。**

覆盖面:重启(run_id 必变)、切主(被提升的副本 replid 必变;降级的旧主 role=slave)、旧备份恢复(旧标记身份不符)、标记被拷到别的实例(身份不符)、多套 data_service 各指一个 mapping Redis(另一实例上没有本实例身份的标记)。身份与数据在同一段 Lua 里取,是同一进程、同一时刻的快照,检查与使用之间没有窗口。

代价:每次读多两次 `INFO`(server、replication 两段,在 Lua 内用 `string.match` 解析,不回传整段文本)。服务端开销估计每次几微秒,**未测**,纳入第 3 批压测(go/db 每条存盘一次)。
未核实:Redis 7.2 是否允许脚本调用 INFO。核对依据:仓库 `third_party/redis`(8.4.0)里 INFO 的标志是 `CMD_LOADING|CMD_STALE|CMD_SENTINEL`、不带 NOSCRIPT(`commands.def`),脚本只拒 NOSCRIPT 命令(`script.c`);miniredis v2.37.0 的命令表同样不带。真 Redis 7.2 用例 `TestRealRedisRouteReadyReadScriptCallsInfo` 必须 PASS。**退路**(只在该用例 FAIL 时启用,写进 §10):同一连接 pipeline 发 `[EVAL 读脚本(不含 INFO), INFO server, INFO replication]`,先读数据、后读身份。重启必断连接;切主会改 replid 或 role;所以「先读后验」的顺序仍然安全。

### M2 挂标记用「预热令牌」做 fencing

新键 `mapping:route:warm`(string,随机 32 hex,无 TTL)。leader 每开始一个**可能以挂标记收尾**的任务(全量预热、身份变化后的修补、首次 catch-up)时先 `SET` 新令牌;挂标记用 `RouteReadyArmScript`:令牌等于本任务的令牌、标记不存在、脚本内 INFO 取到的身份等于标记值里的身份,三者都成立才 `SET`;同一连接紧跟 WAITAOF。

覆盖:任务途中 FLUSHALL / FLUSHDB(令牌被清 → 挂不上 → leader 判「数据可能已清空」,回到全量预热);新 leader 接手(覆写令牌 → 旧 leader 迟到挂标记失败);`rewarm` 撤标记时一并 DEL 令牌(在途任务挂不上)。

### M3 不论 ArmReadyMarker 取值都做库回档检测

新键 `mapping:route:watermark`(string JSON `{"v":1,"generation":…,"watermark_ms":…}`,不表示完整,读方不认)。db_first 下 leader 每拍在 R27 提交之后把它单调推进(GET → 判 → `RouteReadyCompareAndSetScript` / SET NX,复用 1b-1 的 CAS 脚本,不新增 Lua);每拍的 AdvanceReconcile 拿它与 `route_meta.generation`、`route_reconcile.watermark_ms` 比较:世代不等,或水位键 > 库水位(容差 0,负责人 10-17 拍板)→ 冻结。P2→P3(没有就绪标记)期间也能发现回档。
连带:**解冻时库水位要抬到 max(可信标记的水位, 世代相同的水位键的水位)**,否则解冻后下一拍会被立刻重新冻结(§4.6 第 4 项)。

### M4 ArmReadyMarker=false 等于撤销信任

Arm=false 时 leader 每拍对现存标记 compare-and-delete(event `marker_dropped_disarmed`)。回退时改配置即可让读方停止信任,不依赖人记得跑 `rewarm -wait=0`(VERDICT should「关 Arm 不是 fail-closed」)。

### M5 回看修补只对「同一份数据的延续」成立

身份变化后按 `max(0, min(标记水位, cvm) − RepairLookbackMs)` 回看修补,前提是新实例的数据是旧实例在某一时刻的**完整**快照:自身 AOF/RDB 重启、副本提升、replicaof 全量复制都满足;SCAN + DUMP/RESTORE 这类部分拷贝不满足,身份检测也分不出来。规定(写进 runbook 与 G4a 交接):迁移 mapping 实例时**不得拷贝** `mapping:route:ready` / `mapping:route:warm` / `mapping:route:watermark` 三把控制键;用拷贝方式迁移之后必须 `rewarm`(撤标记 + 令牌,leader 全量预热)。M1 保证漏拷时读方不会因为「拷过来的标记」信任新实例;M5 保证 leader 不会在部分拷贝上只做回看修补就挂标记。

## 3. 用户点名的安全性质:逐条论证与残留

| 性质 | 机制 | 证明手段(用例) | 残留 |
|---|---|---|---|
| 版本乱序不会写错库 | 发布脚本按 rev set-if-newer(先比长度再逐字节);库 rev 对同一行严格递增 | 已有 placement / routing 用例;新增第 2 批批量发布乱序用例 | 无 |
| 同版本异值不会写错库 | 发布回 `{0,cur,…}` 且值不同 → conflict:写者失败、leader 不挂标记、audit 报;读方不受影响(缓存值仍是某个已提交状态) | `TestPush*Conflict*BlocksArming`、第 2 批 `TestRouteBatchPublishSameRevDifferentValueFails` | 绕过脚本的写者只能事后发现(提案 §8.3 第 1 条) |
| 提交后发布失败不会写错库 | 「提交即安全」:每个写入点已提交未发布的状态被读方晚看到都安全;依赖步骤只在发布 + WAITAOF 确认后推进;leader 增量推送补齐 | §6.3 逐写入点表;第 2 批每个写入点一个「发布失败后不推进依赖步骤」用例 | 见 §6.3 |
| 发布和 WAITAOF 同连接、检查实际确认 | data_service:`PipelinedCtx` 同一 pipeline(已有,`TestPublishRoute_RealRedisSameConnection`);leader 与 merge_zone:go-redis `Pipelined`(单连接)里 EVAL… + `Do("WAITAOF")`,`checkWaitAOFReply` 判份数 | `TestRealRedisArmWithWaitAOF`、`TestMergeRoutePublishWaitAOFSameConnection`(CLIENT ID 夹心) | WAITAOF 按全局复制偏移覆盖此前全部写是按 Redis 语义推断,未核实 |
| 预热与在线登记 / 冻结 / 切换并发时不会被旧值覆盖 | set-if-newer;在线写者提交后自己发布;预热读到旧 rev 被 superseded 后复读库,库 rev < 缓存 rev 才判 cache_ahead | `PrewarmConvergesWithNewerConcurrentPublish`、真库 + 真 Redis 的预热并发 RegisterHome / relocate CAS 用例 | 无 |
| Redis 切主、旧快照带旧 ready、部分键丢失时不可信数据不指导写入 | M1(身份绑定)+ noeviction 自检 + M5 | §5.1 读脚本用例;真 Redis `RESTART` / `DEBUG RELOAD` 后读方立即判不可信 | 运维手工 DEL 映射键、改淘汰策略后到下一拍之间(只能靠纪律与 leader 每拍自检);部分拷贝靠 M5 纪律 |
| leader 换届、旧 leader 迟到发布 | 库写全带租约谓词;数据推送 set-if-newer(旧 rev 被拒、同 rev 幂等);挂标记 M2 令牌 fencing;标记 / 水位键更新用整值 CAS | `StaleLeaderLateArmRefusedByWarmToken`、`LeaseLostWritesNothing` | 无 |
| 每秒对账修好 ≠ 期间不写错库 | 读方不依赖「leader 已撤标记」:M1 让读方自己判身份;M2 让不完整的数据挂不上标记 | 同上 | 见第 6 行残留 |
| Redis 恢复保留冻结态与迁移任务 | 冻结态 `frozen_run_id` 在库里,预热按库值推送 `S:v:frozen:{run}`;第 2 批起 relocate 的 read / cas 走 SQL,续跑按库里的冻结值认领(Redis 丢了也认得出本次运行冻结的人);relocate 运行清单本来就在本地 JSON 文件 | `TestPrewarmPreservesFrozenPlacement`、`TestRelocateResumeAfterRedisLossClaimsFromSQL` | 无 |
| 映射恢复 ≠ 其他 Redis 状态恢复 | 不在 D1 范围:会话、排序锁、存盘游标 `consumer:applied:*`、重试队列、`PlayerAllData`、能力标记丢失仍按全服维护处理 | 文档 | 写进 runbook「Redis 丢失」一节 |
| SQL 回档检测、停止写入、受控反向修复、审计 | M3 水位键 + 世代 + cache_ahead → `writable=0`(租约谓词);1b-1 的 freeze → reconcile-from-cache → audit -frozen-recovery → unfreeze | 真库 `TestRouteReconcile_RegressionFreezesWriters`、`..._WatermarkKeyDetectsRollbackWithoutMarker` | 回档后到下一拍之间提交的写(W1 / 工具)仍可能落在回档后的库上:W1 的发布会因缓存 maprev 更大而失败(fail-closed);id_segment 同库回档导致的重号不在 D1 范围,登记给号段负责人 |

## 4. 第 1b-2 批:对账 leader(增量规格)

### 4.1 以 scout 规格 1b-2 部分为底

文件、SQL 编号(R23–R30)、API、并发结构、任务、指标、配置、用例名,均按 [scout 规格](spec_D1_batch1b_scout.md)「plan」节 1b-2 各项。下面 §4.2–§4.5 是覆盖它的增量,冲突时以本文为准。

### 4.2 必须吸收的核实意见(VERDICT)

| 编号 | 意见 | 本文取舍 |
|---|---|---|
| must-1 | catch-up 收尾不设 VI、减法不饱和 | 采纳:catch-up 开始前、结束后各读一次身份,相等且无未决结局才置 VI;所有 `X − Lookback / Overlap` 饱和到 0。用例 `CatchUpCompletesSetsVerifiedIdentityAndDoesNotRequeue`、`LookbackSaturatesAtZero` |
| must-2 | 增量游标没持久化 | 采纳:`cache_verified_ms`(cvm)语义改为「`updated_at_ms < cvm − Overlap` 的行都已推过,且期间 Redis 身份未变」;R27 只在一轮增量(或预热 / 修补 / catch-up)从起点 S 完整跑完且身份不变时把 cvm 推进到 `min(本拍 now, S)`;新 leader 的首次增量从 `max(0, cvm − Overlap)` 开始;长任务结束后的首次增量从该任务开始时的库时钟减 Overlap 开始。用例 `FailoverIncrementalStartsFromPersistedCvm`、`IncrementalAfterPrewarmStartsAtPrewarmStart`、`ThrottledIncrementalDoesNotAdvanceCvmPastItsStart` |
| should Arm=false 无回档检测 | 采纳方案 A = 本文 M3 |
| should Arm=false 保留标记 | 采纳 = M4 |
| should 挂标记被拒后无限预热 | 采纳 armingBlocked 闩锁:记原因与未决 id(上限 1000);之后只做增量并复读未决行;未决清空再试挂;淘汰类原因只在身份变化或 `evicted_keys` 回到挂标记时的基线后解除(基线记在 leader 内存,换届时以新 leader 首拍读到的值为基线)。用例 `ArmingRefusedDoesNotRestartFullPrewarm` |
| should gauge 与告警口径 | 采纳:掉主时 leader 观测型 gauge 用 `DeletePartialMatch` / `Reset` 删掉,只有现任 leader 导出;新增 `route_reconcile_assembled`(db_first 每个副本启动预置 1);告警按 leader 过滤(1b-3)。metrics_test 钉住「掉主后 gauge 被清」 |
| should 构造失败只打 ERROR | 采纳:`ReconcileEnabled()` 且 `Store.Required=true` 时 `route_reconciler` 进 `MissingStores` 拒启;抽纯函数 `newRouteReconciler(c, rs, deps)` 给 svc 单测接缝 |
| should leader ID 长度 | 采纳:host 截到 74 字节;用例覆盖 200 字节含非 ASCII 的 host |
| should 关停顺序 | 采纳:reconciler 停止只 cancel、等待 ≤ 500ms,释放租约尽力而为(独立 1s ctx,失败不重试,租约 10s 自然过期);注册在 Kafka defer 之前(按 LIFO 在 Kafka flush 之后执行),总预算写进注释:不超过 go-zero `proc` 的 5.5s |
| should TiDB 缓存表冻结超时 | 采纳:含 R28 的事务单独用 `FreezeTimeoutMs`(缺省 5000,下限 5000,与 1b-1 的 `RouteFreezeTimeoutMin` 同值);冻结前本地剩余租约 < FreezeTimeout + Safety 时先续约 |
| should 反向修库普通读复核 | 1b-1 已做(R18x) |
| should 冻结期 audit 到不了 0 | 1b-1 已做(`-frozen-recovery`) |
| should unfreeze 不防标记缺失 | 1b-1 已做 |
| should 连不上 Redis 占着租约 | 采纳:连续 Redis 失败超过 LeaseMs → `ReleaseReconcileLease` 并降级(event `lease_released_redis_unreachable`)。用例 `SustainedRedisErrorsReleaseLease` |
| should exec 进在役 Pod | 采纳:Job 清单在 1b-3 必做 |
| should 改名丢覆盖 | 1b-1 已做 |
| should I4(全服同一 MappingRedis) | 由 M1 兜住安全(另一实例上的标记身份不符),可用性靠告警;不变量照样写进文档 |
| should 清库脚本与 leader 冲突 | 采纳,放 1b-3:两份清库脚本在 `route_reconcile.lease_until_ms > 库时钟` 时拒绝,`-AllowWithRunningLeader` 放行 |
| nit (a)–(f) | 全部采纳:push 的 phase 加 `catch_up`;Redis 客户端带 `Username=mapping.User`;v 更大只记日志;措辞「Redis 围栏」;每页先去重;Superseded 后复读不到行按 cache_ahead 冻结 |
| nit 运维 (a)–(d) | 采纳:回退表先改 Arm=false;rewarm 的 fail-closed 时长写进 runbook;「按片标记」的截止点是第 3 批读方落码前,本批维持单标记(10-17 拍板);spec 文件以提案为准 |

### 4.3 叠加 M2 / M3 / M4 后的每拍顺序(覆盖 scout「每 TickInterval 一拍」第 1–9 步)

1. 续约(非 leader 先 Peek,看起来过期才 Renew)。未持有:删 leader 观测型 gauge、取消任务、结束本拍。
2. 读身份(一个 pipeline:`INFO server`、`INFO replication`、`INFO stats`、`CONFIG GET maxmemory-policy`)。出错:计 `redis_error`,累计时长超过 LeaseMs 则让出租约(§4.2);不撤标记(撤不了)。role≠master:当出错处理。
3. 读三把控制键(一次 MGET:ready、warm、watermark)。ready 非 string(MGET 回 nil 但 TYPE 不是 none):无条件 DEL(只给 WRONGTYPE 用,scout 已有);畸形:CAD;v 更大:只记日志。
4. AdvanceReconcile(R26 → R18 → `decideReconcileDB`):输入增加 `WatermarkKey *placement.RouteWatermark`。冻结判据 = scout 的两条(标记世代 / 标记水位)∪ M3 的两条(水位键世代 ≠ 库世代;水位键水位 > 库水位 + 容差)∪ cache_ahead。提交后:若未冻结,单调推进水位键(CAS)。
5. Frozen / AlreadyFrozen:停止推送;标记身份 ≠ 当前身份或淘汰异常时 CAD 标记。
6. 淘汰策略不是 noeviction 或 `evicted_keys` 比基线大:CAD 标记、VI=nil、禁止挂标记。
7. **Arm=false**:标记存在 → CAD(M4)。VI==nil → 排 catch-up(设新令牌,M2)。
8. **Arm=true**:标记存在且身份等于当前 → VI=当前身份,CAS 更新标记水位。标记存在且身份不等 → CAD,VI=nil,按 M5 排 repair(设新令牌)。标记不在:VI 非空且 ≠ 当前身份 → repair(设新令牌);VI==nil 或上次挂标记因令牌不符失败 → prewarm(设新令牌);armingBlocked → 不排全量,只增量。
9. 距上次增量满 IncrementalInterval 且没有任务:排 incremental(起点按 must-2)。

挂标记(prewarm / repair 收尾,catch-up 不挂):满足 scout D3 条件 → `RouteReadyArmScript(令牌, 标记值, 身份)` + 同连接 WAITAOF → 复读身份,变了就 CAD 自己的值。脚本回「令牌不符」→ VI=nil、下一拍全量预热(数据可能已被清空);回「已存在」→ 下一拍按第 8 步重新判。

### 4.4 Redis 控制键与脚本(新文件 `go/shared/placement/route_ready.go`,不改 route.go)

| 符号 | 内容 |
|---|---|
| `RouteWarmKey = "mapping:route:warm"` | M2 令牌键 |
| `RouteWatermarkKey = "mapping:route:watermark"` | M3 水位键 |
| `RouteWatermark{V int; Generation string; WatermarkMs uint64}`、`EncodeRouteWatermark` / `DecodeRouteWatermark` | 校验口径同 `DecodeRouteReadyMarker`(generation 1..32 hex、watermark_ms > 0、v 更大回 `ErrRouteReadyMarkerNewerVersion` 同类哨兵 `ErrRouteWatermarkNewerVersion`) |
| `RedisInstanceIdentity{RunID, Role, MasterReplID string}`、`ParseRedisInstanceIdentity(infoServer, infoReplication string)` | 与 1b-1 `routetool.ParseRedisIdentity` 同口径(认 CRLF / LF,run_id 与 replid 40 个小写 hex),多取 role。1b-1 收尾后 routetool 改用它(§4.6) |
| `RouteReadyReadScript` + `RouteReadyReadKeys(mappingKeys ...string) []string` + `ParseRouteReadyRead(raw any, nMappingKeys int) (RouteReadyRead, error)` | 只读 Lua,KEYS[1]=ready,KEYS[2..]=映射键;返回 `{marker 或 false, run_id, role, master_replid, v1, v2, …}`;marker 用 `redis.pcall('GET')`,非 string 当缺席;映射键用 `MGET`(非 string 回 nil,与今天 MGET 口径一致);INFO 两段在 Lua 内 `string.match` 取字段,取不到回空串 |
| `RouteReadyVerdict`(Trusted / Absent / Malformed / NewerVersion / IdentityMismatch / NotMaster / IdentityUnavailable)+ `String()`(低基数,直接作指标 label) | — |
| `VerifyRouteReady(r RouteReadyRead) (RouteReadyVerdict, RouteReadyMarker)` | 纯函数;只有 Trusted 允许信任值与缺键 |
| `RouteReadyArmScript` | KEYS: warm、ready;ARGV: 令牌、标记值、run_id、master_replid。回 1 已挂;0 令牌不符;2 标记已存在;3 实例身份与 ARGV 不符(脚本内 INFO)。不带 WAITAOF(调用方同一 pipeline 紧跟) |
| `MergedIntoKeyPrefix`、`MergedIntoKey(src uint32)`、`EncodeMergedIntoValue(dst, runID)` / `DecodeMergedIntoValue` | 与 merge_zone `json.Marshal(struct{Dst uint32 \`json:"dst"\`; RunID string \`json:"run_id"\`})` 逐字节相同;用例钉住 |

用例(新文件 `route_ready_test.go`):`TestRouteReadyReadScriptShape`、`TestParseRouteReadyRead`(各种回复形状)、`TestVerifyRouteReady`(全部 verdict)、`TestParseRedisInstanceIdentity`(CRLF、role:slave、缺字段)、`TestRouteWatermarkRoundTripAndRejects`、`TestRouteReadyArmScriptShape`、`TestMergedIntoValueMatchesMergeZoneEncoding`。脚本执行语义:miniredis 不支持 INFO 的 server / replication 段,只能在真 Redis 集成测试里测(放 `go/data_service/internal/routereconcile/cache_integration_test.go` 与 go/db 的真 Redis 用例)。

### 4.5 配置增量(`RouteStore.Reconcile`)

scout 的字段全部保留,另加:`FreezeTimeoutMs`(5000,[5000, 60000])、`RedisFailureReleaseMs`(缺省 = LeaseMs)、`ArmingBlockedReportLimit`(1000)。`ArmReadyMarker=true` 时要求 Mode=db_first 且 Enabled≠false(scout);**`RouteStore.ReadRequiresReady=true` 时要求 Mode=db_first**(第 3 批,§7.2)。

### 4.6 1b-1 收尾后才能做的接线(等「玩家数据存储位置」的最终清单)

1. `store/route_store_test.go`:`TestRouteMetaAndReconcileWritesOnlyInStoreFiles` 允许集合加 `internal/store/route_reconcile.go`,命中下限按新语句数更新。
2. `routetoolcli/cli.go` + `routetool/rewarm.go`(新)+ `auditor.go`:子命令 `rewarm [-wait] [-poll]`(撤标记时**一并 DEL 令牌**,M2)与 `audit -require-maprev`(scout)。
3. `data_service.go`:启停 reconciler(§4.2 关停顺序)、横幅一行。
4. `routetool/freeze.go` 的 unfreeze:最低水位取 `max(可信标记水位, 世代相同的水位键水位)`(M3);`-cache-was-not-verified-acknowledged` 时仍取水位键(若可解码且世代相同)。
5. `routetool` 的身份解析改用 `placement.ParseRedisInstanceIdentity`,删私有副本。
6. 提案 §11.2 进度表与新 §13(本文定稿)、runbook §9.3 / §9.4 补 leader 自动恢复与 rewarm、zone_data_rollback 补注。

## 5. 第 3 批:读取方门禁

### 5.1 公共判据(所有读方同一口径)

一律用 `placement.RouteReadyReadScript` 读(M1),`VerifyRouteReady` 判。每个读方一个开关,**缺省 false**(行为与今天逐字相同);打开后:

| verdict | go/db 写任务 | go/db 读任务 | go/db 落库后复核 | data_service | guild |
|---|---|---|---|---|---|
| Trusted | 照今天判定 | 照今天判定 | 照今天判定 | 用缓存;缺键 = 权威缺席(NotFound 契约不变) | 照今天判定 |
| 其余 | 新 outcome `route_not_ready`,延后、**不耗**预算(进 `retryBudgetExempt`) | `route_not_ready`,**耗**预算(满了按既有读失败回执) | 按「路由变了」重试,不耗预算(同 `errPlacementMoved`) | 回源 SQL;SQL 故障回 Unavailable(**绝不 NotFound、绝不空白新号**) | 返回 error → 调用方按既有契约当封锁 |
| 脚本执行出错 | 照今天的查询失败(fail-closed,耗预算) | 同左 | 同左 | 回源 SQL | error → 封锁 |

### 5.2 data_service(`routing/router.go`,不碰 route_publish.go)

- 开关 `RouteStore.ReadRequiresReady`(bool,缺省 false;true 时 Validate 要求 db_first;所有副本同一取值)。
- R2 `GetPlayerHomeZoneAndMergeFence`:开关打开时改用新脚本 `getHomeZoneFenceReadyScript`(在既有脚本基础上加 ready 与两段 INFO,围栏仍按 home 值拼键,`pcall` 失败口径不变)。Trusted → 照旧;否则回源:`SELECT r.home_zone, f.run_id IS NOT NULL AND f.expires_at_ms > <库时钟> FROM player_route r LEFT JOIN zone_merge_fence f ON f.zone_id = r.home_zone AND f.shard = 0 WHERE r.route_shard = ? AND r.player_id = ?`(一条语句、同一快照,满足 `router.go` 对「home 与围栏同一快照」的要求;围栏持有者写满 0..63 片,读 0 片即可)。无行或 home 为 NULL → NotFound(此时是 SQL 的权威缺席);SQL 故障 → Unavailable。**不回填缓存**(leader 负责;读方回填与预热并发虽安全,但没有收益:标记挂上之前读方本来就不信缓存)。
- R3 `BatchGetPlayerHomeZone`:脚本版 MGET + ready;不可信 → `GetRoutes` 分块(≤ `store.RouteBatchLimit`)回源,整批失败回 Unavailable。
- R6 `GetPlayerHomeZone`(含 `ClientForPlayer`):同 R2 去掉围栏。
- R7 `GetAllPlayerIDsInZone`:db_first 下**一律读库**(新只读方法 `ScanRoutesByHome`,走 `idx_route_home`,游标 `(shard, id)`),与开关无关;redis 模式保持 SCAN(那时 Redis 才是真源)。
- 新只读方法放新文件 `store/route_reads.go`(只读不受写入 AST 守卫约束):`GetHomeAndFence(ctx, id)`、`ScanRoutesByHome(ctx, zone, after, limit)`、`ScanRoutesByStorage(ctx, storage, after, limit)`、`ListActiveMergeFences(ctx)`(给 1b-2 围栏镜像对账)。SQL 按提案 §11.2 G1a「后续批次的读方模板」。
- Router 的 RouteStore 消费方接口扩成读写两个小接口(`RouteRegistrar` 现有、`RouteReader` 新增,ISP),svc 装配时同一个 `*store.RouteStore` 同时满足。
- 指标:`data_service_route_read_total{path,result}`,path ∈ home / home_fence / batch / enumerate,result ∈ cache / sql / sql_error / not_found_sql;`data_service_route_ready_verdict_total{verdict}`。启动预置 0(只在开关打开时)。

### 5.3 go/db(`internal/kafka/placement_route.go`)

- 开关 `Placement.RequireRouteReady`(bool,缺省 false)。打开时 `lookup` 用 `RouteReadyReadScript`(KEYS: ready、placement、zone),先判 verdict 再交给 `decideWritePlacement` / `decideReadPlacement`(**纯函数一格不改**);`confirm` 同一读法。
- 新哨兵 `errRouteNotReady`,写任务进 `retryBudgetExempt`、读任务不进;补 `deferReason`、metrics 常量 `PlacementRouteNotReady` 与 Help;`db_route_ready_verdict_total{verdict}`。
- `saveoutbox/replay.go` 读 `player:zone` 的 GET 已是 fail-closed(缺键保留原意图),本批不改,登记。
- 用 `EvalSha`(go-redis `Script.Run` 自带 NOSCRIPT 回退)。

### 5.4 guild(`internal/logic/merge_fence.go`)

- 开关 `MergeMarkerRedis` 段新增 `RequireRouteReady`(bool,缺省 false)。打开时 EXISTS 改为读脚本(KEYS: ready、`merge:in_progress:{z}`);Trusted → 键在即封锁;否则返回 error(调用方既有契约:error = 封锁,回 kGuildZoneMerging)。

### 5.5 tools/data_consistency_check

- `:462` 的 `Exists(...).Val()==0` 吞错误:改为检查 err,出错整体失败(不计孤儿)。`inferLiveZones` 畸形值改为报告并计数,不静默跳过。
- 新参数 `-route-db-dsn`:给了就按全局库枚举(`player_route`),不给保持 Redis 口径并打印「未用库口径」。

## 6. 第 2 批:merge_zone 全部映射写入者改为库先写

### 6.1 接缝决策:merge_zone 自带一份 SQL 写入层,不新建框架

- merge_zone 是独立 module,按 Go 的 internal 规则 import 不了 `go/data_service/internal/store`。可选做法有三:(a) merge_zone 自带 SQL 写入层(提案 §9 第 2 批原定「新增 route_store.go 作 SQL 镜像」);(b) data_service 新增一组管理 RPC,让 merge_zone 远程调用;(c) 把 route store 挪出 internal,让 merge_zone 依赖整个 data_service module。
- **选 (a)**:它是已拍板方案里写明的接缝,仓库里也有先例(`placement_codec.go` 镜像 `shared/placement`,用同一组测试向量钉住)。(b) 要新增 proto 与一套跨进程的围栏协议,截止期、重试、部署转发都要另做,复杂度高出一截;(c) 要挪动另一会话正在改的文件,还会把 go-zero / Kafka 依赖带进运维工具。
- 新文件 `tools/merge_zone/route_store.go` 是 merge_zone module 里**唯一**写 `player_route` / `zone_merge_fence` / `zone_merged_into` 的文件,配同形的 AST 守卫 `TestNoRouteTableWritesOutsideMergeRouteStore`(扫描根 tools/merge_zone)。
- DRY 守卫:不改 merge_zone 的 go.mod(避免 go.sum 变动与 go-redis 版本被 MVS 抬升),发布脚本、rev 比较、行片、WAITAOF 回复判据在 merge_zone 镜像一份;新增用例 `TestMirroredRouteScriptsMatchShared`,用 `go/parser` 读 `../../go/shared/placement/route.go` 与 `route_ready.go` 的字符串常量,逐字节比对。库结构的一致性靠运行期探针(§6.2),不比对源码里的 DDL。

### 6.2 merge_zone 的 route store

- 连接:新参数 `-route-db-dsn`(全局库,dev 是 `testdb`,K8s 是 `mmorpg_global`)。DSN 钉会话级 `READ-COMMITTED`、`innodb_lock_wait_timeout=1`(与 `buildRouteDSN` 同口径;TiDB 另要求 `tidb_txn_mode=pessimistic`)。
- 开写前探针,与 data_service `NewRouteStore` 的 P1–P5 同判据(SQL 文本照抄并在注释里注明出处):隔离级别、锁等待、TiDB 悲观、`route_meta(id=1)` 存在、P4(CHECK 生效)、P5(模数 64)。任一不符拒绝启动写模式(退出码非 0)。
- 写者标识 `merge_zone@{host}:{run_id 前缀}`,满足 `validRouteWriter`(ASCII、≤ 96)。
- 公共事务形状(提案 §4.1 的锁序):普通读 `route_meta.writable`(≠1 拒写)→ 围栏行(zone 升序;**批事务只锁每个相关 zone 的 0 号片** `FOR UPDATE`;获取 / 续期 / 释放锁全部 64 片)→ `zone_merged_into` → `player_route`(按聚簇键 `(route_shard, player_id)` 升序 `FOR UPDATE`)。1213 / 9007 有界重试 4 次加抖动,1205 不重试(与 data_service 同口径)。
- **批事务内的围栏校验(fencing token)**:`run_id == 本次运行` 且 `expires_at_ms > 库时钟`,否则 `errFenceLost`。只锁 0 号片就够:围栏的获取 / 续期 / 释放都要改写全部 64 片,必然经过 0 号片的行锁,所以持有 0 号片行锁期间围栏不可能易主。
- 发布:提交后,go-redis `Pipelined`(单连接)里每行一条 `EVAL PublishRouteScript`(三键形态,**fenceZone 一律传 0**——持围栏的工具自己写,传 home 会撞自己的围栏回 `{3}`)+ 末尾一条 `WAITAOF`;逐行用镜像的 `ClassifyPublishReply` 判:Applied / Confirmed 通过;Superseded 且缓存值等于本次值通过,否则冲突;Conflict / LegacyConflict / Fenced / 回复形状不对 → 失败;WAITAOF 份数不足 → 整批重发(幂等),最多 3 次。参数 `-waitaof-local`(1)、`-waitaof-replicas`(0;redis-ha 写 1)、`-waitaof-timeout-ms`(1500)。
- 发布失败 = 这一批「未完成」:清单里不标完成,工具退出非 0,重跑从这一批重新开始(库里已改的行走幂等分支:同值 → 重新发布)。**不伪装成功。**

### 6.3 逐写入点(提案 §4.3 的表落到代码;「提交即安全」逐条复核)

| 写入点 | 新顺序 | 已提交未发布的状态被晚看到为何安全 | 依赖步骤的门槛 |
|---|---|---|---|
| W12 围栏获取(`fence.go`) | 一个事务:每个 zone 的 64 片行 ODKU 确保存在 → 按 (zone, shard) 升序全部 `FOR UPDATE` → 任一片生效且 run_id ≠ 本次 → 拒绝 → 全部 UPDATE 为本次(`expires_at_ms = 库时钟 + max(timeout + 30m, 1h)`,`fence_epoch + 1`,`holder_json` 原样存)→ COMMIT → 写镜像 `merge:in_progress:{z}`(值 = holder_json,**无 TTL**)+ WAITAOF | 库里已封锁、缓存未封锁:W1 在库内复核围栏分片(db_first),封锁已生效;scene_manager / guild 读 Redis 镜像暂时不知道 → **所以收集清单等依赖步骤必须等镜像确认之后** | 镜像 + WAITAOF 确认后才收集清单(`merge_run.go` 现有顺序不变) |
| W12 续期 | `UPDATE … SET expires_at_ms = 库时钟 + ttl WHERE zone_id = ? AND run_id = ? AND expires_at_ms > 库时钟`(全部 64 片);受影响行数 ≠ 64 × zone 数 → 标记**失权**:续期 goroutine 置 `lost`,后续每批事务的围栏校验本来就会失败;每个写步骤(含不碰映射的 player_blob / guild / trade 步骤)每批之前查 `lost`,置位即中止,退出非 0 | — | 失权后**不再写任何东西**(不是只打 WARN) |
| W12 释放 | 先清库(`UPDATE … SET run_id = NULL, expires_at_ms = NULL … WHERE run_id = ?`),再按**完整值**比较删除镜像(不再用子串匹配);删镜像失败由 leader 的围栏镜像对账删 | 库已解封、镜像还在:只会多封锁 | — |
| W6 markMergedInto | 事务:围栏属于本次 → `SELECT … FROM zone_merged_into WHERE src_zone = ? FOR UPDATE` → 无行插入 / 同 dst 更新 run_id / 异 dst 拒绝 → COMMIT → 镜像 SET + WAITAOF | 库已写、镜像未写:回填工具(W9)与 pin-placement(W10)读的是库,已能拦住 | 必须先于 W5(现有顺序) |
| 5'a clearMergedInto | 事务(持撤销运行的围栏):`DELETE … WHERE src_zone = ? AND dst_zone = ? AND run_id = ?` → COMMIT → 镜像按完整值 CAD | 库已删、镜像还在:只会多拦 | — |
| W5 合服改 home(`merge_run.go`) | 每 500 人一个事务:src、dst 围栏属于本次 → 玩家行 FOR UPDATE → 按 `classifyMappingValue` 同口径分类 → `UPDATE … SET home_zone = dst, rev = rev + 1 … WHERE route_shard = ? AND player_id = ? AND home_zone = src`,受影响行数 = 应改人数 → COMMIT → 整批发布 + WAITAOF | 库已改、缓存仍是 src:src 与 dst 都在围栏里、都已关服;玩家进不了场;go/db 的 src topic 已排空 | `complete()` 原判据 **且** 本批全员发布已确认,才标记步骤完成、才可能开 dst |
| W7 pin 合服钉落点(`placement_ops.go`) | 每批事务:围栏属于本次 → 行锁 → `decidePin` → `UPDATE … SET storage_id = src, placement_version = 1, rev = rev + 1 … WHERE … AND storage_id IS NULL AND home_zone = src` → COMMIT → 发布 | 钉的值等于 home(有效落点不变),早看到晚看到都一样 | — |
| W8 unmerge 5'(`unmerge.go`) | 每批事务:围栏属于本次撤销 → 条件 UPDATE home dst→src(**CAS**,原来是无条件 SET)→ COMMIT → 发布 | 同 W5(两区都在围栏里) | 同 W5 |
| W9 backfill_home_zone | 每批事务:该 zone 0 号片围栏**未生效**(新增)→ `zone_merged_into` 有该 zone 作 src 就拒绝(库里的标记,Redis 丢了也拦得住)→ 无行 ODKU 插入 home / home 为 NULL 补 home → COMMIT → 发布。改用 RC 会话的连接,单键读错误不再当缺席 | 只给从未登记的存量号补 home;晚看到等于今天「没有 home」 | — |
| W10 `-mode pin-placement` | 每批 ≤ 200 人一个事务:围栏 0 号片未生效 → 无 `merged_into` → `SELECT … WHERE home_zone = ? AND storage_id IS NULL AND (route_shard, player_id) > (?, ?) ORDER BY route_shard, player_id LIMIT ? FOR UPDATE`(走 `idx_route_home`)→ UPDATE → COMMIT → 发布 | 钉的值等于 home | — |
| W11 relocate(`relocate_run.go`) | **只换 `relocatePlacements` 的实现**为 `sqlRelocatePlacements`;`decideFreeze`、R1–R5 状态机、引擎一格不改。`read` 读库(`storage_id/placement_version/frozen_run_id` 编码成与今天相同的落点值串,home 串同理);`cas` 每批一个事务:op 带 `FenceZone` 时该 zone 0 号片未生效(生效回 Fenced)→ 行锁 → 逐 op 比对「编码后的当前值 vs ExpectRecord / ExpectAbsent」「当前 home vs ExpectHome」→ 写列、rev + 1 → **新增断言**:稳定态新 version 不小于当前 version(P-2)→ COMMIT → 发布 + WAITAOF → **只有确认了的才回 Written**;发布失败按「前缀结果 + error」返回(接口既有约定) | R1 冻结提交未发布:缓存仍是未冻结,go/db 写 S,数据仍在 S,正确;引擎拿不到 Written 不进 R2。R5 切换提交未发布:缓存停在 R1 已确认的冻结态,go/db 延后,安全 | R1 全员确认才进 R2(现有引擎语义) |
| R8 读方 | 清单收集、`placementsExisting`、`manifestPlayersBackAt`、`redisMappingPresence`、`verify:manifest_mapping`、`collectPlayerIDsWithHomeZone`、`collectStoragePlayers`:`-route-source=db` 时**全部读库**;新增审计项 `verify:route_cache`:清单玩家的缓存 rev = 库 rev 且值相同(开 dst 之前跑,满足 P-6) | — | — |
| player_blob_migrate | 不写映射,不改;但每批之前查围栏失权标志 | — | — |

### 6.4 模式与安全缺省

- 新参数 `-route-source=db|redis`,**缺省 db**。db:上表的新路径。redis:今天的纯 Redis 路径原样保留,只给 data_service 仍是 `RouteStore.Mode=redis` 的环境用,启动打 WARN。缺省取 db 是因为它在 redis 模式的环境里会 fail-closed:库里没有行、或 `route_meta` 不可写,事务直接拒绝,不会写出与缓存分叉的值;反过来缺省取 redis,在 db_first 环境里会写出库不知道的缓存值。
- `dev_tools.ps1` 的 merge-zone 包装从 `go/data_service/etc/data_service.yaml` 读 `RouteStore.Mode`,自动传 `-route-source` 与 `-route-db-dsn`(DSN 取同一 yaml 的 SnapshotMySQL);新开关 `-MergeZoneRouteSource` 可显式覆盖。契约测试同步。
- **W2(RemapHomeZoneForMerge)、W3(删映射)保持停用**,本批不碰;不定义删号、墓碑与冷副本规则。

### 6.5 测试 harness 修复(用户要求区分 PASS / FAIL / SKIP)

`integration_test.go` 的 TestMain 在依赖不可达时 `os.Exit(0)`,SKIP 与 PASS 退出码相同。改为:依赖地址取环境变量(`MERGE_ZONE_IT_MYSQL_DSN`、`MERGE_ZONE_IT_REDIS_ADDR`、新增 `MERGE_ZONE_IT_ROUTE_DB_DSN`),缺省值保持今天的硬编码地址(兼容既有命令);不可达时各用例 `t.Skip` 并打印原因,`go test -v` 输出里能逐条看到 SKIP;另加环境变量 `MERGE_ZONE_IT_REQUIRE=1`,设了就把「依赖不可达」从 SKIP 变成 FAIL(给 Codex 的真库验证用,杜绝把 SKIP 报成通过)。

## 7. 第 1b-3 批:部署载体、告警、运维防护

1. `k8s_deploy.ps1` 的 data-service ConfigMap `RouteStore:` 段补 `FenceShards`(MySQL 档 16;TiDB 档等 TiDB Phase 1 部署方式定了再补,注释写明)、`WaitAOFNumLocal: 1`、`WaitAOFNumReplicas`(单实例 0,redis-ha 档 1)、`WaitAOFTimeoutMs`、`ReadRequiresReady: false`、`Reconcile:` 段(全部缺省、`ArmReadyMarker: false`);db ConfigMap `Placement.RequireRouteReady: false`;guild ConfigMap `MergeMarkerRedis.RequireRouteReady: false`。契约用例钉住:三处开关缺省 false、FenceShards ∈ [1, 64] 且等于档位值、Mode 仍是 redis。**该脚本与契约测试有他人大量在途改动,只用 Edit 精确插入,动手前重新定位行号。**
2. 新告警 `deploy/k8s/data-service-route-alerts.yaml`(PrometheusRule,格式照 `db-task-retry-alerts.yaml`):NoLeader(`sum(route_reconcile_leader) == 0 and sum(route_reconcile_assembled) > 0` for 1m)、MultiLeader(> 1 for 1m)、ReadyMissing(arm 开且现任 leader 报标记不在 for 10m)、WritesFrozen(现任 leader 报 writable=0)、RollbackOrCacheAhead(冻结事件 increase > 0)、ArmingRefused、PushConflicts、CacheVerifyStale(> 30s for 2m)、RouteNotReadyDeferrals(go/db `route_not_ready` 速率 > 0 for 5m,只在开关打开后有数)、RouteReadSQLFallbackErrors(data_service `sql_error` increase > 0)。告警按 leader 过滤(VERDICT should)。
3. route-tool 一次性 Job 清单 `deploy/k8s/manifests/go-svc/data-service-route-tool.yaml`(照 `data-service-migrate.yaml`:同镜像、同 ConfigMap,args `-f … -route-tool <子命令>`,资源单独设,backoffLimit 0,restartPolicy Never),runbook 写用法;exec 进在役 Pod 只限 dry-run 或小库。
4. 两份清库脚本:在 `route_reconcile.lease_until_ms > 库时钟` 时拒绝(有在任 leader),`-AllowWithRunningLeader` 放行;plan 输出里打印探测结果。
5. `k8s_zone_rollback.ps1`:第 3 步(手工 PITR)之后新增只读核对:读 `route_meta.writable` 与 `-route-tool audit`;若全局库所在实例也被回档(K8s 下全局库与 zone 库同一 MySQL 实例),提示按 runbook §9.4 的 freeze → reconcile-from-cache → audit → unfreeze 恢复,脚本本身不自动执行修库。
6. 文档:`deploy/k8s/README.md` 补 route-tool 入口与 Redis 连接总账(leader 独立 go-redis 池);AGENTS.md §6.2 的 Q8(清库加全局库路由表)仍待负责人确认,本批不改 AGENTS.md。

## 8. 上线顺序与回退(提案 §7 的 P0–P5,补门禁)

| 阶段 | 动作 | 进入门禁 | 回退 |
|---|---|---|---|
| P0 | 迁移建表(已在 1a) | — | 不删表 |
| P1 | 冻结窗口开始(禁止一切 merge_zone 写模式);`import-from-redis` → `audit` | — | 窗口内重跑导入见 runbook §9.3 |
| P2 | data_service 滚动切 `Mode=db_first`;leader 随之运行(Enabled 缺省 true,Arm=false),补发孤儿行、做回档检测(M3) | 1b-3 的 ConfigMap 载体就位;W1 建角压测出对比表(§6 门禁) | 先确认读方开关都是 false,再改回 redis(先撤读方、后撤写方) |
| P3 | 二次导入(`-allow-db-first`)→ `audit -require-maprev` 为 0 → 全部副本 `ArmReadyMarker=true` 滚动 → leader 预热挂标记 → 再 audit 一次 | P2 全部副本完成 | Arm 改回 false(M4 自动撤标记) |
| P4 | 新版 merge_zone(`-route-source=db`)上线,runbook §9 改写,解除冻结窗口;旧二进制禁用 | P3 完成;第 2 批验证通过 | 回到冻结窗口,不跑合服 |
| P5 | 读方逐个打开:go/db 逐 zone `Placement.RequireRouteReady=true` → data_service `ReadRequiresReady=true` → guild | 标记已挂、`route_ready_verdict_total{verdict!="trusted"}` 稳定为 0;存盘压测出对比表(go/db 多一次 INFO) | 逐个关读方开关(先读方、后写方) |

开关全部保持安全缺省(false / redis),**代码完成不等于启用**:任何环境的切换都由负责人按上表逐阶段决定,本批不改任何部署档位的取值。

## 9. Java 版(AGENTS.md §12)

按摸底(Java 仓 HEAD `80be462`,版本 `0.1.0-SNAPSHOT`):归属区的唯一真源是 `game.player.zone_id`(`game-player-schema.sql:15`,NOT NULL),建角同事务写入(`PlayerStore.java:216-255`),读方 `PlayerHomeZones` 直接读库、把故障(`DependencyException`)与缺项分开;没有任何承载归属区的 Redis 副本,也没有 storage_id / 多库搬迁。因此就绪标记、对账 leader、身份检测、回档检测与反向修库在 Java **不适用**;等价契约(SQL 真源、无可丢副本、故障 ≠ 缺席)**已满足**。
**未满足、必须登记为 Java 待做(7.3)**:合服围栏(`ZoneMergeFence` 只有恒放行的 OPEN,`RedisZoneMergeFence` 未实现)、合服作业与清单(代替 merged_into)、撤销合服、`RedirectOnEnter`、F4 改判 `home_zone_id`、归属区变更审计;库里没有 `zone_id > 0` 的 CHECK;没有真 MySQL / TiDB 运行证据。PARITY.md 只在末尾追加一行(该文件有他人未提交改动),**不能写两版全部完成**。

## 10. 偏离任务原文与提案的取舍(决策记录)

| # | 取舍 | 理由 | 代价 |
|---|---|---|---|
| X1 | 读方信任条件加身份绑定(M1),不再只看标记存在 | §1 的窗口;任务原文要求「不可信数据不能指导写入」 | 每次读多两次 INFO;读脚本取代 MGET |
| X2 | 挂标记加预热令牌(M2)、新增水位键(M3)、关 Arm 撤标记(M4)、部分拷贝纪律(M5) | §1 | 两把新控制键 |
| X3 | **回档恢复不换 generation**(沿用 1b-1 的 B1;任务原文写了「新 generation」) | M1 + 水位检测已能认出旧快照与再次回档;换代会让现有标记失效,S3 规模下读方要 fail-closed 到全量预热完成(数小时),或者要多一套「换代同时改写标记」的协调 | 恢复前后没有世代边界可供审计;审计改由 freeze / repair / unfreeze 的 stderr 明细与 `route_reconcile_event_total` 承担 |
| X4 | merge_zone 自带 SQL 写入层 + 源码比对守卫,不 import shared、不走 RPC | §6.1 | 一份镜像代码;由 `TestMirroredRouteScriptsMatchShared` 与运行期探针守住 |
| X5 | merge_zone 批事务只锁围栏 0 号片 | §6.2 的论证 | 无 |
| X6 | data_service 回源不回填缓存 | §5.2 | 无 |
| X7 | 第 3 批读方开关分服务各一个,缺省 false | 逐个打开、逐个回退 | 多三个配置键 |

## 11. 验证计划(交 Codex;Claude 不运行)

总原则:先验证已有基线(复验清单 → 第 0 / 1a / G1a / P5 各段 → 1b-1 段,命令都在提案 §11.2),再按本文各批。每步串行,红了就停下归因,不连续重试、不改用例迁就、不 `gofmt -w`;`GOTOOLCHAIN=local`、`GOPROXY=off`;真库用例必须分别报 PASS / FAIL / SKIP,**MySQL 通过不能代替 TiDB**;口令不进回报;失败时按提案 §11.2「2026-10-15 补登」那张表保留证据。各批的具体命令在落码完成后写进提案 §13 与 PROGRESS 条目。

## 12. 风险

1. INFO 在 Lua 里的可用性与开销(M1)未实测;不可用时走 §2 M1 的退路。
2. WAITAOF 按全局偏移覆盖此前全部写是推断(scout 已登记)。
3. S3 规模下全量预热数小时,期间读方开关打开的服务 fail-closed(10-17 拍板先单标记,压测后再定是否按片)。
4. merge_zone 镜像代码与 data_service 两份事务实现可能漂移:靠源码比对守卫(脚本)、运行期探针(库结构)与两边各自的真库用例。
5. 手工 `redis-cli SET` / 旧版 merge_zone 等绕过脚本的写者(提案 §8.3 第 1 条)。
6. 本批叠在未编译、未验证的 1a / G1a / P5 / 1b-1 之上。

## 13. 评审吸收(2026-10-18,三视角对抗评审;与正文冲突时以本节为准)

### 13.1 对账 leader(覆盖 §4.3、§4.4、§4.5)

1. **(must)「标记不在」一律全量预热**:标记不在(不论 VI 是否为空、是否等于当前身份)→ VI=nil、排 prewarm(armingBlocked 时除外,见第 6 条)。回看 repair **只用于**「标记在、身份不符」(自身 AOF/RDB 重启、副本提升:标记随数据一起延续,M5 前提成立)。空实例重启、PVC 丢失、换空实例、rewarm、人工 DEL、稳态 FLUSHALL 都走全量预热。删掉 §4.3 第 8 步里「标记不在:VI 非空且 ≠ 当前身份 → repair」一支。
2. **(should)标记的 `watermark_ms` 改记「缓存已验证到」的点**:leader 每拍 CAS 更新标记时写入当时的 cvm(`route_reconcile.cache_verified_ms`,§4.2 must-2 的新语义),而不是库水位。身份变化后的回看起点 = `max(0, min(marker.watermark_ms, 库 cvm) − RepairLookbackMs)`:标记跟着数据走(旧备份里的标记带着备份时刻的 cvm),比库 cvm 更贴近这份数据实际推到了哪。**库回档判据只用水位键(M3)、世代与 cache_ahead,不再用标记水位**(scout D4 第一条判据作废)。1b-1 unfreeze 用标记水位作库水位下限的逻辑仍然成立(标记水位 ≤ 库水位),另按第 4 条取 max。
3. **(must)围栏镜像由 leader 双向对账,列为挂标记前置**(取代 scout B2「1b 不碰 merge:in_progress」与用例 `NeverTouchesMergeFenceKeys`,该用例改写为 `FenceMirrorFollowsDB`):
   - 镜像值改为 merge_zone 写的 holder JSON **加字段 `fence_epoch`**(读方只看 EXISTS,值格式不是契约);写镜像一律用新脚本 `placement.MergeFenceMirrorSetScript`:当前值能解出 `fence_epoch` 且 ≥ 本次 epoch 时零写入(挡住失权旧工具迟到的 SET 覆盖新持有者),否则 SET 并 `PEXPIREAT = 库 expires_at_ms + 10 分钟`(TTL 来自库时钟、只会比库围栏晚过期:多封锁不少封锁;merge_zone 崩溃又没有 leader 时也能自己消失)。删镜像一律按完整值 CAD。
   - leader 每拍(在 AdvanceReconcile 之后、未冻结时)读 `ListActiveMergeFences`(库时钟判生效)与 `SCAN merge:in_progress:*`:库里生效的 → 用 set-if-newer 脚本补 / 续镜像;Redis 里有、库里对应 run_id 已不生效的 → 复读库确认后按完整值 CAD;**值解不出 `fence_epoch` 的(旧版 merge_zone 写的)不删**,计 `route_reconcile_event_total{event="foreign_fence"}` 并告警。
   - 挂标记的前置增加「本轮围栏镜像对账成功且无 foreign_fence」。
4. **(must)水位键的出口**:unfreeze(`-confirm-audited`)在置 writable=1 的同一流程里,把水位键 CAS 成 `{库世代, 解冻后的库水位}`(水位键不在就 SET NX;CAS 输了退出码 1 重跑);`rewarm` 与「回退 redis 模式」一律**单条** `DEL mapping:route:ready mapping:route:warm mapping:route:watermark`。M3 的判据与这一条**同批**上线(§4.6 第 4 项随 1b-2 一起改,不能先上判据后上出口)。
5. **(should)DEBUG 进程内整库替换**:`DEBUG RELOAD NOSAVE` / `DEBUG LOADAOF` 在同一进程里替换数据、不换 run_id 与 replid,M1 识别不了。leader 每拍 `CONFIG GET enable-debug-command`,不是 `no`(Redis 7 缺省即 no)就拒绝挂标记并 CAD 现有标记(event `arming_refused_debug_enabled`);K8s / compose 的 mapping Redis 启动参数显式写 `--enable-debug-command no`(1b-3)。§3 表中「真 Redis `RESTART` / `DEBUG RELOAD` 后读方立即判不可信」改为「真 Redis 重启后、`DEBUG CHANGE-REPL-ID` 后读方立即判不可信」。淘汰策略被改到下一拍之间被淘汰的键:读脚本不能调 CONFIG(NOSCRIPT),登记为残留。
6. **(should)untrusted 必须有上界**:armingBlocked 由 `rewarm` 一并清除(rewarm 撤标记 + 令牌 + 水位键后,新 leader 首拍重新取 evicted_keys 基线);Arm 取值是**全服单值**——K8s 契约测试钉住所有 zone 的 data-service ConfigMap 取值相同,metrics 导出 `route_reconcile_arm_enabled`(每个装配了 reconciler 的副本恒导出 0/1),告警「出现两种取值」;回退顺序写死为「先关全部读方开关 → 再 Arm=false」。
7. **(nit)M2 的保证如实描述**:令牌是最后写者胜,不是单调 fencing;它保证的是「挂标记成功 ⇒ 本任务设令牌之后没有 FLUSH / rewarm,且本任务完整扫过库」,与谁是 leader 无关;挂标记前再 Peek 一次库租约(必须仍是自己)。残留:Peek 与挂标记之间整机挂起(VM suspend 时单调时钟不走)的旧 leader 仍可能挂上——它挂的是一次完整预热的结果,且受 M1 约束。arm 回 3(身份变了)→ VI=nil、下一拍按第 1 条判。
8. **(should)leader 自检导出 verdict**:leader 每拍用同一段 `RouteReadyReadScript`(不带映射键)+ `VerifyRouteReady` 自检,导出 `data_service_route_ready_selfcheck{verdict}` gauge(只有现任 leader 导出)。P5 门禁看它,不看读方计数(读方开关打开前读方没有数据)。
9. **(nit)指标名**一律带 `data_service_` 前缀;leader gauge(`route_reconcile_leader`)在每个装配了 reconciler 的副本上**恒导出 0/1**(不删),只有「leader 观测型」gauge(writable、ready_marker、selfcheck、cache_verified_age)掉主时删;所有 event / result 取值进 Prime 预置。

### 13.2 第 3 批读方(覆盖 §5)

1. **(must)go/db 不可信时暂停消费,不进重试队列**:`Placement.RequireRouteReady=true` 时,消费循环在取下一批之前看进程级「就绪闸」(每 `RouteReadyGateIntervalMs`(缺省 1000)用 `RouteReadyReadScript` 不带映射键刷新一次 verdict);闸不是 Trusted → 不取新消息、不 ack、不往 Redis 重试队列推(Kafka 保留,lag 增长由既有告警覆盖),每秒复查;导出 `db_route_ready_gate{state}`(0/1)与 `db_route_ready_verdict_total{verdict}`。逐条任务的读仍走脚本、仍判 verdict(权威判据,原子);闸与逐条判之间的竞态(闸刚放行、逐条判不可信)才走 `errRouteNotReady` 的延后(写不耗预算、读耗预算),量级限于闸刷新间隔内的在途任务。日志与 frozen 同口径(DEBUG,另每分钟一条 INFO 汇总)。
2. **(nit)启动探针**:任一读方开关为 true 时,启动期试跑一次读脚本(不带映射键);脚本执行出错(例如 INFO 在脚本里不可用)→ 拒启。
3. **(nit)INFO 次数**:go/db 每条写任务 lookup + confirm 各一次脚本 = 4 次 INFO;压测按这个口径估。
4. **(nit)go/db 读任务预算用尽**是既有的「静默丢弃」(store_failure.go),不是回执;本批不改这一既有语义,登记。
5. **(should)data_service 接口**:现有消费方接口叫 `routing.RouteStore`(route_publish.go),不改名;在 router.go 新增 `RouteReader` 与 `WithRouteReader`。
6. **(nit)协议形状守卫**:第 3 批补用例钉住「`ReadRequiresReady=true` 且缓存不可信时:SQL 故障 → gRPC Unavailable(绝不 NotFound);SQL 无行 → NotFound;批量任一 SQL 故障 → 整批 Unavailable」(server 层表驱动)。
7. **(nit)残留登记**:redis-ha 网络分区时,被隔离的旧主在哨兵切主后仍可能自报 role:master、身份与标记相符,读方会信任它上面的旧值【推断】;HAProxy 只把客户端导向多数哨兵认可的主,风险限于 HAProxy 自身也被隔离的情形。

### 13.3 第 2 批 merge_zone(覆盖 §6)

1. **(must)批事务锁围栏 63 号片,并先确保行存在**:围栏片行由 W1 懒建,事先不一定存在,RC 下对不存在的行 `FOR UPDATE` 不加锁。所有「校验围栏」的事务前,先在事务外 `INSERT … ON DUPLICATE KEY UPDATE zone_id = zone_id` 确保 `(zone, 63)` 存在;事务内锁它,**读不到行一律当错误**(不当未生效)。用 63 号片而不是 0 号片:W1 只锁 `player_id % FenceShards` 号片,FenceShards < 64 时永远碰不到 63 号片,工具的批事务不拖累建角。获取 / 续期 / 释放仍写满 64 片,fencing 论证不变(必经 63 号片的行锁)。data_service R2 回源读围栏也读 63 号片(不加锁;行不在 = 从未封锁过)。
2. **(must)「库 rev 领先缓存」的行先发布确认再用**:任何把库读结果交给依赖步骤的读(relocate 的 `read`、W5 / W8 / W7 / W10 的「已是目标状态」分支、续跑时的清单复核),对「库 rev > 缓存 maprev(或缓存没有 maprev / 值不同)」的行,先按库值发布 + WAITAOF,确认了才返回;确认不了返回 error,依赖步骤不推进。批处理规则统一为:**每批里处于目标状态的行(本批刚改的与早已是目标的)全部发布**,`complete()` 要求全员确认。这关掉了「R1 已提交未发布 → 续跑认领冻结 → 直接进 R2」的窗口。
3. **(must)`-route-source` 必填、无缺省,并核对库与 Redis 是否成对**:`db` 时 `-route-db-dsn` 必填,且开写前要求 `mapping:route:watermark` 存在、可解码、世代等于该库 `route_meta.generation`(证明有 db_first 的 leader 正在用这份库维护这个 Redis);`redis` 时见到水位键就拒绝(说明这个 Redis 已归 db_first 管)。dev_tools **不推断**,新增必填开关 `-MergeZoneRouteSource db|redis` 与 `-MergeZoneRouteDbDsn`(写模式缺了就报错);`itRun` 显式传 `-route-source=redis`。runbook 的 13 处 `go run .` 同步补参数。
4. **(should)失权判据**:续期 goroutine 维护本地单调截止时间 `lastRenewOK + ttl − 60s`;每批(含 player_blob / guild / trade 等不碰映射的步骤)之前查「已失权 或 超过本地截止」→ 中止;续期返回 1205 / 网络错误 → 退避重试并 WARN,不当失权;只有「受影响行数 < 64 × zone 数」才是确定失权。批事务内 `run_id == 本次 ∧ expires_at_ms > 库时钟` 的复核仍是权威判据。
5. **(should)W10 键集游标**用提案模板 `route_shard > ? OR (route_shard = ? AND player_id > ?)`,不用行比较(MySQL 不对行比较做 range 优化)。
6. **(should)R8 漏项补齐**:`checkMergedIntoBeforeMerge`(merge_run.go)、relocate R0 的 `readPlacements`(relocate_run.go,绕过了 relocatePlacements 接口)、`storage_audit.go`(决定库能否退役,必须读库)、`audit_checks.go` 的 mainRowCheckFor、`unmerge.go` 的 MGET,`-route-source=db` 时全部读库。
7. **(should)测试 harness**:TestMain 只探测依赖、记下原因;每个集成用例开头调 `requireIT(t)`(不可达时 `t.Skip(原因)`,设了 `MERGE_ZONE_IT_REQUIRE=1` 则 `t.Fatal`);AST 守卫用例检查集成测试文件里每个 `Test*` 都调了它。路由表 DDL 不在 merge_zone 里复制:真库用例要求 `MERGE_ZONE_IT_ROUTE_DB_DSN` 指向一个已由 `data_service -migrate` 建好表的一次性库(Codex 步骤里先跑 migrate);未设时这些用例 SKIP(REQUIRE 时 FAIL)。既有「基址 + 库名」的拼接方式保留,新环境变量只给路由库用。

### 13.4 第 1b-3 批(覆盖 §7)

1. **(must)部署转发与防翻转**:`k8s_deploy.ps1` 新增参数 `-RouteStoreMode redis|db_first`、`-RouteReconcileArm`、`-RouteReadRequiresReady`、`-DbPlacementRequireRouteReady`、`-GuildRequireRouteReady`,缺省都是今天的值(redis / false);**粘滞**:照 GateRouterMode 的先例(`k8s_deploy.ps1` 的防翻转核对),部署前读集群现有 ConfigMap 的取值,与本次参数不一致且没带显式确认开关时拒绝(防止 zone 回档按缺省值把 db_first 静默降回 redis);preflight 拒绝「Arm 或任一读方开关为 true 而 Mode=redis」的组合(否则 data_service 拒启 CrashLoop);契约测试钉住缺省值、粘滞核对与非法组合拒绝、全部 zone 的 Arm 取值相同。
2. **(should)告警修正**:按 §13.1 第 9 条的指标口径重写表达式,NoLeader 写成 `(sum(data_service_route_reconcile_leader) or vector(0)) == 0 and sum(data_service_route_reconcile_assembled) > 0`;ReadyMissing 用 `max(data_service_route_reconcile_arm_enabled) == 1`;文件头写明抓取前提:仓库里 data-service 只有 Prometheus 注解、没有 ServiceMonitor,接入哪种抓取方式是 P2 门禁的一项。
3. **(should)route-tool 入口**:新脚本 `tools/scripts/k8s_route_tool.ps1`(不往 k8s_deploy.ps1 里加):渲染 Job 清单(同镜像、同 ConfigMap、args `-f … -route-tool <子命令与参数>`)→ apply → 等完成 → 把 stdout / stderr 存到报告目录 → 按 Job 容器退出码判定并原样返回;dev_tools 加 `k8s-route-tool` 转发。
4. **(should)zone 回档**:`k8s_zone_rollback.ps1` 本批不改(另一项改动在重写它)。设计 / runbook 登记:db_first 下,在「选择性清本 zone 玩家缓存、保留映射键」的载体(zone_rollback_discard)落地前,K8s 的 zone 回档不可执行;回档后的 audit 留在 runbook 人工做;不自动修库(相对 1b-1 规格的偏离)。
5. **(nit)清库脚本防护**:探测放在 FLUSHALL 之前;`route_reconcile` 不存在按「没有 leader」;租约未到期时先等到期(最长 LeaseMs + 1s),等不到再拒绝,`-AllowWithRunningLeader` 放行。
6. **(nit)Redis 连接总账**是 k8s_deploy 里的代码算出来的:leader 的独立 go-redis 池(`Reconcile.RedisPoolSize`)要进那段计算,不能只改 README。
7. **(nit)mapping Redis 启动参数**加 `--enable-debug-command no`(redis.yaml、redis-ha.yaml、docker-compose)。

### 13.5 上线、回退与 Java(覆盖 §8、§9)

1. 回退列写全:**先关全部读方开关 → Arm=false,确认 leader 计了 `marker_dropped_disarmed` → Mode=redis → `rewarm -wait=0`(一并删令牌与水位键)**。紧急回退时同样不能跳过前两步,否则 Validate 拒启或读方全服 fail-closed。
2. PARITY.md 已有 D1 行(:242 附近)与 7.3 合服待做行(:141 附近):**改 D1 那一行**(补本仓提交与「合服围栏 / 撤销 / 审计仍随 7.3 待做」的引用),不在末尾另加一行。
3. 旧版 merge_zone 没有机械防线(它直写 Redis、不碰 maprev,leader 按 updated_at_ms 扫库看不见):靠 §13.1 第 3 条的 `foreign_fence` 告警、§13.3 第 3 条新二进制的成对核对,以及 runbook「P4 之后禁用旧二进制」。
