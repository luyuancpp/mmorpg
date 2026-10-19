# player_locator 会话对账:按 gate 实例建会话索引(2026-10-18)

> **状态**:已落码,**未编译、未测试,待 Codex 验证**(AGENTS §10.1)。规格与核实意见:[spec_B0-1_reconcile_by_gate.md](../handoff/launch-readiness-20261013/spec_B0-1_reconcile_by_gate.md);负责人 2026-10-17 拍板「索引在既有三段会话 Lua 里与会话写入原子维护」。核实意见的 3 条 must(单 leader 无法让位、etcd 一抖就作废补建、标记与代次只在进程内)已按 §4 的做法改设计后落码。
>
> 代码:`go/player_locator/internal/logic/{keys.go, session_cas.go, gate_session_index.go, session_reconciler.go, reconcile_view.go, reconcile_metrics.go}`、`internal/node/node.go`(能力键)、`player_locator.go`(装配);告警 `deploy/k8s/player-locator-alerts.yaml`。
>
> **2026-10-18 评审修订**(同日落码后的核实意见,本文已按修订后的代码改写,未编译、未测试):last_success 启动预置 0、Stalled 不再按 is_leader 过滤(§9);长 for 告警用短子查询抹平换主空档(§9);单键 WRONGTYPE 只跳过该成员 / 该 gate、不中止整拍,实例级错误照常整拍失败(§4.2、§8);完整扫描单页失败对同一游标退避重试、扫描期间 Redis 换进程则整遍作废、有候选被跳过的补建不设标记、审计失败设冷却键(§4.3、§4.4、§8);last_audit 超前一个周期以上按坏值处理(§4.3);大面积故障恢复时间更正为约 3.7 小时(§7);回滚后快速再升级的窗口登记为残余风险(§6、§8)。

## 1. 问题与现状

会话对账兜住「gate 整机崩溃 → 断线回调不执行 → SetDisconnecting 永远不来 → 会话永久 ONLINE」。gate 崩溃时断线通知根本不发(`cpp/nodes/gate/handler/rpc/client_message_processor.cpp` 单向通知、不重试),会话键又没有 TTL,对账是这类会话唯一的自愈路径。

旧实现(2026-08-10 起,HEAD 360d56ae5 之前):每个 zone 的每个 locator 副本各自每 60s `SCAN MATCH player:session:* COUNT 500` 走完整个共享 Redis 键空间,命中的键逐个 GET。所有 zone 的 locator 连同一个 Redis(D12),所以全服 2Z 个实例在重复扫同一个键空间。根因:会话键按 player_id 散列,没有按 gate 的二级结构,判定「哪些会话挂在死 gate 上」只能扫全库。负载推算见 §7。

## 2. 键形与不变量

| 键 | 类型 | 内容 | 写者 |
|---|---|---|---|
| `player:gate_index:sessions:<gate 实例 uuid>` | SET | 十进制 player_id | 三段会话 Lua;对账的守卫脚本 |
| `player:gate_index:gates` | SET | gate 实例 uuid(登记集) | replace Lua、补建;对账释放 |
| `player:gate_index:ready` | STRING + PX | `v1:<设置时刻 unix>`(就绪标记,租约) | 只由 leader |
| `player:gate_index:gen` | STRING(整数) | 代次 | 只由 leader(INCR) |
| `player:gate_index:last_audit` | STRING | 上次完整补建 / 审计完成的 unix 秒 | 只由 leader |
| `player:gate_index:audit_cooldown` | STRING + EX | `1`(审计失败后的冷却,默认 1 小时,审计周期更短时取审计周期) | 只由 leader |
| `player_locator:session_reconciler:leader` | STRING + TTL 30s | 选主锁 | shared/leader |
| etcd `/mmorpg/capability/player_locator/gate_session_index/v1/<node_uuid>` | KV,挂节点租约 | `1`(能力键) | 每个新版 locator 的注册事务 |

- **为什么按 gate 实例 uuid 而不是 gate_id**:gate 每次进程启动随机生成 uuid,node_id 会在重启后复用(`node.cpp` 的 `set_node_uuid`),按 gate_id 建会把新旧实例混在一起。
- **前缀用 `player:`**:`gate:{id}:draining / drained` 属于排队族,G4a 第 1 批要整体搬到 redis-queue;索引必须和会话一起留在核心闭包里。
- **不加 hash tag**:索引只在会话 Lua 内和会话键一起被访问,这些脚本本来就因全局 `player:leases*` 跨槽,加 tag 换不来原子性;见 §11。

不变量:

- **I1**:会话 ONLINE 且 `gate_instance_id` 非空 ⇔ player_id 是该 gate 索引的成员。唯一定义在 `indexedGate()`。
- **I2**:索引非空 ⇒ gate uuid 在登记集里。
- **I3**:索引只是提示。任何依据索引动会话或摘成员的动作,都先按会话原始字节校验,再用整字节 CAS 或字节守卫脚本提交。所以脏成员、漏成员、两个 leader 短暂重叠都不会误伤会话。

## 3. 写路径逐脚本对照

索引键与成员都由 Go 从「CAS 的 expected 字节解出的旧会话」和「即将写入的新会话」推导,再作为 KEYS 传进同一段 Lua。整字节 CAS 保证提交时 Redis 里的就是 expected,推导与提交之间没有窗口。空串 KEYS 是「这一侧没有索引」的哨兵,脚本以 `~= ""` 守卫,永不写入名为 `""` 的键。

| 路径 | 脚本 | 索引动作 |
|---|---|---|
| SetSession(首登 / 顶号) | replaceSessionAndCancelLease | 旧 gate ≠ 新 gate 时 SREM 旧;新会话可索引时 SADD 新 + SADD 登记集。missing 模式清理未完成(返回 2)、CAS 失败(返回 0)都不碰索引 |
| Reconnect(DISCONNECTING → ONLINE) | 同上 | 旧会话不可索引,只 SADD 新 |
| SetDisconnecting | setDisconnecting(经 `transitionToDisconnecting`) | SREM 旧 |
| 对账转换 | 同上(与 SetDisconnecting 同一个提交点) | SREM 旧 |
| MarkOffline | deleteSessionIfUnchanged | SREM 旧 |
| commitLeaseExpiry / rearm / claim / renew / ack / enqueueOfflineCleanup | 不改 | 它们只处理 DISCONNECTING 或已删的会话,本来就不在索引里(handleLeaseExpiry 在 commit 前已在 Go 侧校验 State==DISCONNECTING)。**不要往这些脚本里补 SREM** |

写路径不增加往返,每次写入只在既有脚本内多 1~3 次集合操作。

## 4. 控制面状态机

### 4.1 选主

- 全服一个 leader(`shared/leader`,Redis SET NX + 属主续期 + 自我降级,TTL 30s)。作用域 = 一个会话 Redis 域 + 它对应的 etcd:今天所有 zone 共用一个 Redis(D12),所以全服一个;将来多 cell 时自然变成每 cell 一个。
- 选主只为削减负载,**不是正确性 fence**:两个 leader 短暂重叠时,所有动作都走整字节 CAS 或字节守卫 Lua,重叠无害。
- **自我让位**(核实 must 1):选主锁只看 Redis,leader 自己看不清 etcd 时锁照样续着。所以索引循环连续 3 拍失败,或距上次成功超过 3 拍,就返回 whileLeader,选主器带属主校验释放锁;本进程再竞选前多等一个 TTL(30s),让健康副本先抢到。每拍失败打 ERROR,带失败阶段 `stage=etcd|redis|panic`。
- 只由 leader 产出的 gauge(含 `last_success`)在进程启动时预置 0,任期开始、卸任(OnStateChange(false))与任期结束时清零;优雅退出经 `proc.AddShutdownListener(elector.Stop)` 主动让位。
- **为什么轮询而不是 watch etcd DELETE**:去抖语义与旧实现一致(连续两拍缺席),实现简单,watch 断流重建复杂;检测延迟 60~180s,与旧实现相同。

### 4.2 索引循环(每拍,顺序固定)

1. **写者检查**:`SessionWriters` 用一个只读 Txn 同时 Range `PlayerLocatorNodeService.rpc/` 与能力键前缀(同一个 revision,避免「新版 locator 恰在两次读之间正常退出」被误判成旧写者)。
   - 出错(核实 must 2):**只跳过检查本身**,不删就绪标记、不动代次;本拍照常处理死 gate,但整拍记为 `etcd_error`、计入让位门槛。
   - 一次成功的读里看到「已注册却没有能力键」的 locator:同一段 Lua 里 `INCR gen` + `DEL ready`。
   - 零旧写者:给**已存在**且前缀为 `v1:` 的标记续租(PEXPIRE),不新建。
2. **登记集快照**(SMEMBERS),必须先于第 3 步:gate 先在 etcd 注册,login 才会把玩家分给它,快照里的每个 gate 都早于那次 Range 注册过。
3. **etcd 存活集**(Range `GateNodeService.rpc/`,宽松解析 NodeInfo,D77)。出错:整拍返回,去抖不推进、不转换(fail-closed)。
4. **gate 级去抖**:存活 → 清嫌疑与游标;缺席 → 计数(封顶 2),≥2 即确认死亡。已确认死亡的 gate 只要还在登记集里就保持确认;gate 在 etcd 重新出现(重注册沿用同一 uuid)即清掉嫌疑。
5. **预算内处理死 gate**(核实 should「每拍工作量无上限」):逐页 SSCAN 索引 → 一次 MGET 本页会话 → 逐个判定:
   - ONLINE 且就在这个 gate 上 → 经 transitionPacer 限速,转成 DISCONNECTING(版本 +1、ZADD 租约,deadline = now + DefaultTTLSeconds);CAS 失败计 `cas_conflict`,成员保留下一拍重判;
   - 会话已在别处 / 断线中 → 按字节守卫摘(`pruned_stale`);会话键不存在 → 按「仍不存在」守卫摘(`pruned_missing`);字节解不开或载荷 player_id 不符 → 只摘成员、会话原样(`pruned_malformed`,打 ERROR);成员不是规范十进制 player_id → 直接摘(`pruned_bad_member`)。
   - 预算 = `DeadGateMembersPerRound`(默认 30000),再被「转换限速 × 半拍」截住,另有半拍的时间上限;用完记下游标,下一拍续扫。所以大面积故障时的**持续**转换速率约为 `MaxTransitionsPerSecond / 2`(§7)。一个 gate 扫完一遍后索引空了就释放(DEL 索引 + SREM 登记集,守卫脚本内 SCARD==0 才动);没空就下一拍从头再扫。
   - **单键错误只跳过那个键**:成员的会话键类型不对(被写成 hash 等,摘除守卫脚本报 WRONGTYPE)只跳过该成员,计 `member_error`,毒成员留在索引里、这个 gate 不释放(DeadGatesPending 暴露);某个 gate 的索引键类型不对(SSCAN / 释放 / SCARD 报 WRONGTYPE)只跳过该 gate,计 `gate_error`。只认 WRONGTYPE:READONLY / LOADING / MASTERDOWN / OOM / BUSY 等同样是服务端回复,但属于实例级故障,当成单键问题跳过会让主从切换期间每拍报 ok、不让位。两条护栏:一拍里单键错误超过 100 个整拍失败;同一页里**所有**转换都报 WRONGTYPE 也整拍失败 —— 转换只碰刚 MGET 到的 string 会话键与全局租约簿记,全部失败说明坏的是 `player:leases*` 这类全局键。摘除路径不做「整页失败」判定:摘不掉的毒成员恰是死 gate 最后剩下的那些。
6. **刷 gauge**:死 gate 数、死 gate 剩余成员(进度)、存活 gate 的成员数(在线数**上界**)。

last_success 按「完成一拍」推进:预算截断、被跳过的单键错误都不算失败。

### 4.3 补建 / 审计循环(与索引循环同一任期,独立 goroutine)

- 任期内由 `run()` 用 WaitGroup 等它退出后才返回:同一进程再次当选时不会有两个补建循环并发,前任在途的写也不会落在新任之后。两个限速器分开:transitionPacer 归索引循环,scanPacer 归补建 / 审计。
- 每一拍读一次就绪标记:
  - **缺失 → 补建一遍**:先读 `gen`,再做一次写者检查(看到旧写者照样补,但这次检查已经 INCR,结束时不会设标记;etcd 读失败就不开始这一遍),然后跑 §4.4 的完整扫描;结束时 Lua「`gen` 仍是开始时的值才 `SET ready v1:<now> PX 3.5 拍` 并写 `last_audit`」。这一遍里有 ONLINE 会话因键类型错误没能核对 / 补进索引时,结果记 `incomplete`,**不设标记**(否则就是在仍有会话缺成员时宣布 I1 成立)。
  - **存在 → 审计到期才跑**(核实 should「审计计时进程内」):冷却键不存在,且 `now − last_audit ≥ AuditIntervalSeconds`(默认 86400;-1 = 就绪后不审计)。时刻在 Redis 里,换主不重置。审计补进成员计 `audit_repaired` 并打 ERROR(I1 被破坏过)。审计失败不写 `last_audit`,改设冷却键 `player:gate_index:audit_cooldown`(Redis 侧 EX,默认 1 小时),不让持续的故障每拍重开一遍全键扫描。
  - **时钟偏差**:`last_audit` 是写入它的 leader 的墙钟,与本 leader 的墙钟比较。偏差 δ 小于审计周期时,审计最多提前或推迟 δ;比本 leader 时钟超前一个审计周期以上的值只可能来自坏时钟(例如 NTP 故障快了几天),按坏值处理(到期并打 ERROR),否则此后所有 leader 都算出负数、审计停摆。冷却键与就绪标记用 Redis 侧过期,不受 Pod 时钟影响。
- **就绪标记是租约 + Redis 侧代次**(核实 must 3):标记只由「本拍 etcd 读成功且零旧写者」的 leader 续期。没有新版 leader 在跑时最迟 3.5 拍过期,下一任新版 leader 从补建开始;代次替代旧规格里进程内的 legacyEpoch,跨 leader、跨进程都守得住。
- 租约取 **3 拍 + 半拍**(偏离:主会话写的是 3×IntervalSeconds):正好 3 拍时,连续两拍失败后第三拍的续期与过期同刻赛跑,约一半概率误过期、触发一遍全量补建;多半拍余量后,单个坏 leader 连续三拍失败让位,接任者也通常在过期前续上。

### 4.4 完整扫描(补建与审计共用)

1. 限速 SCAN `player:session:*`(`ScanCount` / `ScanPagesPerSecond`),每页一次 MGET,对 ONLINE 且 gate 非空的候选做一次 SISMEMBER pipeline;缺失的经守卫脚本补(会话已不是扫描时那一版返回 -1,不补:新写入自己维护过索引)。已是成员的,确认其 gate 在登记集里(I2;登记集被局部清库删掉时,不补的话这个 gate 死了也没人处理)。
2. 逐个登记过的 gate SSCAN 索引,按字节守卫摘不符的成员(**含存活 gate**,核实 should「存活 gate 的脏成员无人清理」)。真在这个 gate 上 ONLINE 的成员原样保留。
3. 只补索引、清成员,**不转换会话**:补进来的死 gate 成员由索引循环在两拍内处理,转换只有一条路径。
4. **一遍之内的容错**(一遍约 9 小时、约 2e7 条命令,一次没被 go-redis 重试吸收的抖动不该让整遍作废):
   - 单页失败(传输错误、超时、实例级错误)对**同一游标**做有上限的指数退避重试(1s 起翻倍、封顶 30s,最多 5 次,随任期取消);连续 5 次失败才放弃这一遍。页内的写都是幂等的字节守卫脚本 / SADD,重做一页不会重复生效。只有一个 leader 在扫,不加抖动。
   - 单个候选上的 WRONGTYPE(某个 gate 的索引键不是 SET)跳过并计 `member_error`,这一遍照常扫完;补建遍里有候选被跳过就不设标记(见 §4.3)。全键扫描不设单键错误上限:一个坏索引键会让那个 gate 上的每个候选都报错,设上限会让整遍提前中止、其余键永远扫不到。同一页所有补成员都报 WRONGTYPE(坏的多半是全局登记集)按这一页失败处理。
   - **扫描期间 Redis 换了进程就整遍作废**:SCAN / SSCAN「开始时就在的元素都扫到」只在同一个 Redis 进程里成立(字典的 siphash 种子每个进程随机),主从切换或重启后旧游标续扫会漏键且不报错。一遍开始与结束各读一次 `INFO server` 的 `run_id`,变了就不设标记 / 不记审计完成。读不到 run_id(INFO 被禁用、托管 Redis 不返回该字段)按出错处理,fail-closed,代价见 §8。

## 5. 自愈

- 脏成员(旧写者换 gate 漏摘、局部清库):死 gate 上由索引循环摘;存活 gate 上由审计摘。在两者之前,`gate_index_members` 偏高,所以它的 Help 写明是上界。
- 漏成员(旧写者、外部篡改、非 locator 写者):补建 / 审计补。
- 登记集缺 gate:补建时按已有成员补回。

## 6. 滚动升级与回滚

**能力键**:新版 locator 的 etcd 注册事务多写一个同租约的能力键(`node.WithLeaseBoundMarkers`;首注册、换 node_id 重新分配、租约丢失后的重注册三处都带上)。NodeInfo 与能力键同生同灭,不存在「有 NodeInfo 没能力键」的误判窗口;Close 时先删 NodeInfo 再撤租约,窗口是「有能力键没 NodeInfo」,不会被判为旧写者。能力键不放在 `PlayerLocatorNodeService.rpc/` 下:C++ 把该前缀当 zone 级发现前缀解析,混进非 NodeInfo 值有解析风险。

allocator 是 guild / scene_manager / match / client_rpc_router / data_service 的逐字副本(各文件头有约定)。主体保持逐字,只在 player_locator 加一个可选的 `extraOps` 变参钩子(缺省为空),其余副本头注释登记了这一处差异。

**升级**(只涉及 player_locator,任意顺序逐 zone 发布,不需要开关):

1. 新进程的所有会话写入从第一条起就维护索引,同时注册能力键。
2. 新版 leader 发现仍有旧写者:删标记、递增代次,补建持续跑(只补索引);索引循环照常按已有索引处理死 gate。旧进程自带的全键扫描对账在它退出前照常兜底。
3. 旧写者全部退出后,下一遍干净的补建设上标记,随后改为日审计。

新旧混跑都安全:旧写者把玩家从 A 换到 B,A 留下脏成员(按字节守卫摘)、B 缺成员(补建补);旧对账或旧 MarkOffline 不摘索引,只留下脏成员。

**回滚**(核实 must 3 的场景):

- 回滚到旧二进制:旧二进制恢复「每副本全键扫描」,覆盖完整。新版留下的索引键是无害的垃圾;标记**不再被续期,最迟 3.5 拍过期**,再升级时新 leader 从补建开始,不会拿着残留标记只跑日审计。一次性 `rollout undo` 快到新 leader 来不及看到旧写者,只要**再升级发生在回滚 3.5 拍之后**(标记已过期),也一样。例外:回滚后 3.5 拍内又再升级,且期间没有任何新版 leader 在一次成功的 etcd 读里看到旧 Pod,第一任新 leader 读到零旧写者会直接续上残留的标记 —— 回滚窗口内旧写者写的会话要等日审计才补进索引,登记为 §8 残余风险 8。
- 确认永久回滚后的清理(只在确认不再使用新版本之后执行;**不要用全键 SCAN**,键数只有 gate 数 + 6,含冷却键):
  ```bash
  redis-cli SMEMBERS player:gate_index:gates | while read -r g; do redis-cli DEL "player:gate_index:sessions:$g"; done
  redis-cli DEL player:gate_index:gates player:gate_index:ready player:gate_index:gen player:gate_index:last_audit player:gate_index:audit_cooldown player_locator:session_reconciler:leader
  ```
  不在登记集里的残留索引键(只可能来自违反 I2 的历史状态)无害,随下一次清库处理。etcd 能力键随租约自动消失。
- staging / 压测清库:`tools/scripts/deploy-staging.sh` 与 `stress-linux-tier.sh` 已把 `player:gate_index:*` 与 `player:session:*` 一起删;AGENTS §6.2 的 FLUSHALL 同样清掉,下次启动自动补建。

## 7. 2000 万在线容量推算

[D] = 有出处的数据,[I] = 推断。Z 随 PCU 翻倍是推断(§7.2 没有给 Z),核心 Redis「其余全部流量约 1.7e6/s」是全局单点交接 1000 万在线口径(:178),这里沿用,2000 万时更高。

| 项 | 旧实现 | 新实现 |
|---|---|---|
| 每拍(60s)命令 | 全体副本合计约 8e9~1.8e10 GET + 5.2e9~1.2e10 SCAN [I](Z=200~445、2Z 副本、N≈2e7、K≈6.5e9) | 每拍约 3 条 Redis 命令 + N_g=1000~4444 条 SCARD(pipeline)+ 2 次 etcd 读 [I] |
| 一轮耗时 | 单副本 Z=1 时 22~108 分钟(交接 E2,[D]);多副本争用下更长,「连续两拍缺席」随之拖到小时级 | 一拍内完成;死 gate 处理受预算与限速约束 |
| 补建 / 审计 | 无 | K/ScanCount ≈ 6.5e6 页,200 页/s 约 9 小时一遍 [I];扫描期间约 200 SCAN + 200 MGET + 约 600 SISMEMBER/s [I];日均约 3.3e7 条命令 ≈ 380/s [I] |
| 大面积 gate 故障 | — | 丢一个可用区约 6.7e6 会话:每拍只用半拍去转换(一拍 60s 最多 30000 个),持续约 500/s,**约 3.7 小时**转换完 [I](2026-10-18 评审更正:本格原写「1000/s 限速约 1.9 小时」,把配置值当成了持续速率;规格原写「数十分钟」更低)。期间每拍照常完成、新死的 gate 照常被识别。要更快就调高 `MaxTransitionsPerSecond`(先评估 LeaseMonitor 的 LeaveScene 与踢人命令的下游压力),不要放宽半拍比例:一拍的超时就是一个拍长,超时的拍计入让位门槛 |
| 索引常驻内存 | 0 | 约 2e7 成员 × 50~80B ≈ 1~1.6 GB,常驻核心 Redis [I],需实测 MEMORY USAGE 后计入 G4a / 容量模型的核心内存账 |

默认参数都是推断值,待 V0 实测后校准。

## 8. 失败模式与残余风险

| 失败 | 行为 |
|---|---|
| leader 到 etcd 的路径坏了(其余副本正常) | 每拍 etcd_error,连续 3 拍让位,多等一个 TTL 再竞选,健康副本接手 |
| etcd 整体抖动 < 3 拍 | 去抖不推进,标记不动(租约 3.5 拍内续上),补建不作废 |
| etcd 整体长时间不可达 | 各副本轮流当选、轮流让位;对账停摆(与旧实现相同,fail-closed);标记过期,恢复后补建一遍。每一任都读到 last_success=0,`PlayerLocatorReconcilerStalled`(critical)持续亮,不受换主空档影响(§9) |
| Redis 不可达 | 选主失败 / 每拍 redis_error;此时会话写入同样不可用 |
| 两个 leader 短暂重叠 | 全部动作走整字节 CAS / 字节守卫,无害;补建结束的代次 CAS 守住「看到过旧写者」 |
| 判定后玩家重连 / 顶号 | 转换 CAS 失败成 no-op(`cas_conflict`) |
| 死 gate 实为分区存活、释放与并发 SetSession 竞态 | 释放脚本 SCARD==0 才动;SetSession 原子地同时 SADD 成员与登记集。gate 只要还在登记集里就保持确认死亡,下一拍处理新成员;真活着的 gate 在 etcd 重新出现即清嫌疑(`TestIndexRound_ReleaseRacesConcurrentSetSessionOnDeadGate`) |
| 毒键:某个会话键被写成别的类型 | 只跳过该成员(`member_error`),本拍照常完成、不让位;毒成员留在索引里,该 gate 不释放,由 DeadGatesPending 暴露,人工处理那个键。旧实现同样是记日志后跳过 |
| 某个 gate 的索引键类型不对 | 索引循环只跳过该 gate(`gate_error`);补建遍跳过该 gate 上的候选、不设标记(`incomplete`),NotReady 会亮 |
| 全局键(`player:leases*`、登记集)类型不对 | 同一页全部转换 / 补成员都报 WRONGTYPE → 整拍 / 这一页失败,照常计入让位门槛(此时 SetDisconnecting 也全部失败) |
| 主从切换、OOM、LOADING 等实例级服务端错误 | 不当成单键问题:整拍失败、连续 3 拍让位;完整扫描内对同一游标退避重试,5 次不成放弃这一遍 |
| 完整扫描期间 Redis 换了进程(主从切换 / 重启) | 一遍结束时 run_id 不符 → 整遍作废(不设标记 / 不记审计完成),补进来的成员照样有效 |

残余风险(登记,不在本项):

1. **漏成员的残余窗口**:存活时间短于一拍的旧版 locator(crash-loop 的旧 Pod),或恰好只在 etcd 故障期间存活的旧写者,写出的会话要等日审计(≤ AuditIntervalSeconds)才补进索引;非 player_locator 的直写者(今天全仓没有)同理。缓解:日审计 + `PlayerLocatorGateIndexAuditRepaired` 告警;要更快可调小审计周期,代价是 SCAN 负载。
2. **补建开始后、结束前一拍内**才出现的旧写者:标记可能先被设上,下一拍被删。窗口 ≤ 一拍,只推迟下一遍补建,不影响转换。
3. **补建不续点**:换主后新 leader 从头扫(游标不持久化)。目标规模下一遍约 9 小时,发版频率高于这个时,补建可能总被打断,`PlayerLocatorGateIndexNotReady` 会亮(该告警用短子查询抹平换主空档,24 小时里有换主也照样能等满,§9);后续改进是把 SCAN 游标与代次一起存进 Redis。让一遍从头重来的来源不止换主:单页连续 5 次失败(Redis 长时间抖动)、扫描期间 Redis 换了进程(run_id 变化)、补建开始前写者检查失败、补建期间出现旧写者(gen 变化)都会让这一遍作废;审计失败则冷却 1 小时后重来。
   - (2026-10-18 评审增补)读不到 `run_id`(托管 Redis 屏蔽 INFO 或不返回该字段)时每一遍都作废,补建永远不会宣布就绪、只能一直跑限速的全键扫描;NotReady 会亮,届时需改用别的重启探测。
4. **环境隔离前提**:所有共用同一会话 Redis 的 locator 必须注册在 reconciler 读的同一个 etcd。多套环境共用 etcd:别的环境的旧版 locator 被误判为旧写者,只多补建。多套环境共用 Redis 但 etcd 不同:别的环境的 gate 会被当成死 gate、误转换 —— 旧实现同样存在,非本项引入。
5. **覆盖面与旧实现相同**:gate 存活、但断线通知在 gate→login 或 login→locator 上丢失的会话,对账看不到。人工补救见 `docs/ops/release-checklist.md` 附录 F.B-1「定向补救」。按 gate 建的索引为后续「gate 定期上报自己持有的会话,locator 与索引求差」创造了条件。
6. **miniredis 与真实 Redis 7 的差异**:空串 KEYS 哨兵、`string.sub`、SSCAN 游标语义(miniredis 按偏移,删除期间会跳过元素;生产 Redis 不会),需在真实 Redis 上按 §12 末尾的步骤核对。
7. **GateInstanceId 不做字符集校验**:不合法的值不建索引会让那条会话永远不被对账,比照写更糟;gate uuid 由 C++ uuid 生成器产出。
8. **回滚后 3.5 拍内又再升级**(2026-10-18 评审增补):若期间没有任何新版 leader 在一次成功的 etcd 读里看到旧 Pod,残留的就绪标记还在租约内,第一任新 leader 读到零旧写者会直接续上它(§6)。回滚窗口里旧写者写的 ONLINE 会话不在索引里,这些会话所在的 gate 一旦死掉,要等日审计才补进索引后处理。窗口长度不超过 3.5 拍;兜底是日审计与 `PlayerLocatorGateIndexAuditRepaired` 告警。与残余风险 1 同类(「只活在没有新版 leader 的时段」的旧写者),根因是标记只能由看见旧写者的读来作废。
9. **毒成员不自动清理**(2026-10-18 评审增补):会话键被写成别的类型时,该成员摘不掉(守卫脚本报 WRONGTYPE),每拍跳过一次并计 `member_error`,所在死 gate 不释放。刻意不做无条件 SREM:坏键要人工排查来源。一拍里超过 100 个单键错误整拍失败,大面积篡改会让对账停摆并触发 critical 告警。

## 9. 指标与告警

指标(Namespace `player_locator`,label 全为有限枚举,不用 player_id 也不用 gate uuid):

| 指标 | 类型 | 说明 |
|---|---|---|
| `player_locator_reconcile_round_total{loop,result}` | counter | loop = index / repair / audit;result = ok / etcd_error / redis_error / gen_changed / incomplete(补建有候选因键类型错误被跳过,不设标记) / panic;启动时全部组合预置 0 |
| `player_locator_reconcile_last_success_unix{loop}` | gauge(leader) | 本任期最近一次完成。进程启动预置 0(三个 loop 都预置,按 `reconcileLoops` 一张表),任期开始与结束都清零:每个副本都导出这条序列,新任期在第一拍成功之前读数为 0 |
| `player_locator_reconcile_sessions_total{action}` | counter | to_disconnecting / cas_conflict / pruned_stale / pruned_missing / pruned_malformed / pruned_bad_member / repaired / audit_repaired / member_error(会话键类型不对,跳过该成员) / gate_error(索引键类型不对,跳过该 gate);预置 0 |
| `player_locator_reconcile_dead_gates` | gauge(leader) | 已确认死亡、尚未释放的 gate 数 |
| `player_locator_reconcile_dead_gate_members` | gauge(leader) | 死 gate 剩余成员(进度) |
| `player_locator_reconcile_legacy_writers` | gauge(leader) | 最近一次成功读到的旧写者数 |
| `player_locator_reconcile_is_leader` | gauge | 1 = 本副本是 leader;启动预置 0,每拍重申 |
| `player_locator_gate_index_ready` | gauge(leader) | 标记存在 |
| `player_locator_gate_index_members` | gauge(leader) | 存活 gate 的索引成员数之和,**在线数上界** |

规则 `deploy/k8s/player-locator-alerts.yaml`:NoLeader、Stalled(critical,都带 absent 分支)、RoundsFailing、LegacySessionWriters、GateIndexNotReady、GateIndexAuditRepaired、DeadGatesPending(warning)。计数器 sum();gauge 大多乘 `(player_locator_reconcile_is_leader == 1)` 过滤后 max(),两处例外与一处平滑(2026-10-18 评审修订):

- **Stalled 不按 is_leader 过滤**:`(time() - max(last_success{loop="index"})) > 300 or absent(...)`。前任卸任已清零,卡死的僵尸 leader 的值只会变旧、不会变新,掩盖不了现任,所以对全体副本取 max 就是现任的值。原写法按 is_leader 关联:从未成功过的 Pod 当选后没有这条序列(当时也没有预置),结果为空集,卸任过的 Pod 都挂着 0,absent 也不触发;「每一任都失败」(etcd 整体不可达、毒键)时一任约 3 拍、短于 `for: 5m`,换主空档的那次空求值又会重置 pending,critical 告警恰好无声。现在全员失败时所有序列都是 0,表达式持续为真。吸收正常换主的是 `for: 5m`(新任期读数为 0,time()−0 恒大于 300)。代价:没有 leader 时与 NoLeader 一起亮。
- **长 for 的 leader 过滤告警用短子查询抹平换主空档**:Prometheus 只要有一次求值没有返回某条 alert 的序列就删掉 pending;换主时旧 leader 的序列变 stale 或变 0,新 leader 的 1 要等下一次抓取才可见。GateIndexNotReady 改为 `max_over_time((…)[10m:1m]) == 0` 配 `for: 24h`,LegacySessionWriters 改为 `min_over_time((…)[5m:1m]) > 0` 配 `for: 30m`。不用「长子查询 + for 0m」:首次上线没有历史,补建本来就要约 9 小时,会正落在预期未就绪的窗口里误报。不用「24h 内 repair ok 的 increase」:就绪后补建循环只跑审计,健康稳态下它恒为 0。
- **DeadGatesPending 不平滑**:新 leader 的去抖表是空的,接任后头一两拍读数本来就是真 0,子查询帮不上;换主会让它的 15 分钟从头计。

`TestAlertRulesReferenceRegisteredMetrics` 守住规则里的指标名都已注册;`TestReconcileMetricLabelsArePrimed` 守住预置覆盖全部 label 取值与全部 loop。

go-zero 在 Prometheus agent 启动(`zrpc.MustNewServer`)之前的指标写入一律丢弃,所以 `player_locator.go` 把对账的启动挪到了 MustNewServer 之后。

**生效前置**:K8s 上 player_locator 原本不暴露 /metrics(ConfigMap 无 Prometheus 段,manifest 无端口)。本项已给 `deploy/k8s/manifests/go-svc/player-locator.yaml` 加注解与 9190 端口;`tools/scripts/k8s_deploy.ps1` player-locator heredoc 的 Prometheus 段与契约用例是共享文件,由主会话合并(见 PROGRESS 同日条目)。合并之前告警全部是哑的。manifest 的注解 / 端口与 heredoc 的 Prometheus 段**必须同批发布,或先发后者**:只有注解、进程没在 9190 监听时,按注解抓取的目标是 down,可能误触通用的 TargetDown 类告警。

## 10. 配置

`Lease.ReconcileIntervalSeconds`(默认 60,-1 关闭)与 `Lease.Reconcile.{AuditIntervalSeconds, ScanCount, ScanPagesPerSecond, MaxTransitionsPerSecond, DeadGateMembersPerRound}`。`MaxTransitionsPerSecond` 是瞬时上限,大面积故障时的持续速率约为它的一半(§4.2、§7),估恢复时间按一半算。**事实源是代码默认值**(`config.ReconcileConf.Normalized`):dev yaml 只写注释,K8s ConfigMap 不写,`TestEtcYamlLeavesReconcileToCodeDefaults` 守住。leader 是全服一个,**这些项必须全服一致**,否则行为取决于哪个 Pod 当选;要改就改代码默认值。

## 11. Cluster 前瞻

- 路线 R(单元化单主):不变。负责人 2026-10-17 已拍板「单元化 + 单元内 Cluster 分片」,单元内上 Cluster 时按下面改形。
- 上 Cluster 时会话 Lua 本来就要因全局 `player:leases*` 重构。届时按会话 `{player_id}` 所在 slot 的预计算 slot-tag,把索引与登记集分桶为 `player:gate_index:{<slottag>}:sessions:<uuid>` 与 `player:gate_index:{<slottag>}:gates`,与 lease 的 slot 分桶同批设计,保持 CAS 与索引同槽原子;死 gate 按桶遍历。
- 不推荐的非原子备选:「先写索引、再写会话」+「按 login_generation 打分的 ZSET 条件删除」,乱序删除有 ABA 与时序复杂度。
- 索引是派生数据,可以靠补建重建,所以迁移只需要「新形状写者(能力键 v2 前缀)+ 删就绪标记 + 补建」,不必停写。

## 12. 测试与验证

单测(miniredis + 注入的固定时钟 + 不限速 pacer + 受控的拍,不读墙钟):

- 写路径(`gate_session_index_test.go`):首登、跨 gate 顶号、同 gate 顶号、断线、重连、空实例号(空串哨兵不落键)、陈旧版本、重放幂等、清理未完成、MarkOffline。本文件只用修复前就有的符号,索引键名写字面量,单独拷到基线 360d56ae5 上能编译,修复前红是断言红(索引键不存在),不只是编译红。
- 控制面(`session_reconciler_test.go`):两拍去抖、存活不动、复现清零、**不 SCAN 全键空间**、三类脏成员、坏会话只摘成员、etcd 出错不推进、判定后顶号 CAS no-op、每拍预算分拍、**一拍内多页续扫**、**死 gate 释放与并发 SetSession 竞态**(释放之前注入;单独的 SCARD / DEL 出现时在两步之间注入)、写者读失败不动标记与代次、旧写者删标记加代次、只续已存在的标记、补建补齐并设标记、旧写者阻止就绪、写者读失败不开始补建、补建中代次变化不设标记、标记过期后下一任重新补建、不补非 ONLINE 与已改写的会话、审计计 audit_repaired、审计时机跨 leader 持久、审计清存活 gate 脏成员、连续失败让位、3 拍无成功让位、让位释放选主锁并退避(真实选主器)、run 等补建循环退出、两个限速器与预算、纯函数;固定种子 600 步的确定性序列测试(每步断言 I1 / I2,从写路径文件挪来)。
- 2026-10-18 评审补充:毒会话键只跳过该成员(连续 3 拍不失败)、坏索引键只跳过该 gate、单键错误超过上限整拍失败、全局租约键类型不对整拍失败、READONLY 照常让位;补建单页失败同一游标重试后设上标记、审计第二页 SSCAN 失败按同一非零游标重试、持续失败按指数退避 5 次后放弃、审计失败设冷却键、扫描期间 run_id 变化整遍作废、有候选被跳过不设标记(incomplete)、last_audit 超前一个周期按到期处理;`isKeyTypeError` 与 `parseRedisRunID` 纯函数。
- 配置 / 节点 / 告警:`config_test.go`、`node_registration_test.go`(+ integration tag 的能力键随租约存亡)、`reconcile_metrics_test.go`。

验证命令见 PROGRESS 同日条目。真实 Redis 上的核对(交 Codex,只记录结果,不下性能结论):本地栈跑 robot login-test 后,`SMEMBERS player:gate_index:gates` 只有当前 gate,`SCARD player:gate_index:sessions:<uuid>` 等于在线机器人数,`GET player:gate_index:ready` 以 `v1:` 开头,`EXISTS ""` 为 0;结束 gate 进程等约 3 分钟,`EXISTS player:gate_index:sessions:<uuid>` 为 0、对应会话已删除,leader 日志有 `orphaned ONLINE session -> DISCONNECTING`。

## 13. Java 版

不适用。Java 的在线目录 `game:presence:{player_id}` 以 gate 为唯一写者、TTL 60s、20s 续期(Java 仓 `PARITY.md`「玩家在线目录」行),gate 崩溃后条目自然过期,没有「永久 ONLINE 需要全键对账」这个问题,也没有 SCAN 型对账器。本项只改服务端内部的 Redis 结构与后台循环,不触及两版共享的客户端契约。PARITY 行的登记由主会话合并。
