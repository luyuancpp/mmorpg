# 调研规格:会话对账按 gate 建索引(player_locator)(2026-10-16)

> **状态**:只读调研产出,**未落码**。由开服线会话 2026-10-16 的调研工作流生成(一名调研者出规格、一名核实者逐条反驳),原样保存,未经人工改写。
> **怎么用**:前半部分是调研者的规格;「# VERDICT」之后是核实者意见。**落码前必须先吸收核实意见里的 must / should**,被判 refuted 的事实以核实者为准。行号与工作区状态是 2026-10-16 03:48~04:35 的快照,工作区有多个会话在并行修改,落码前按函数名重新定位。
> **汇总、拍板项与推荐顺序**:见 [docs/PROGRESS.md](../../PROGRESS.md)「2026-10-16 开服线剩余工作调研」条目,以及交接 [handoff-launch-readiness-and-d1-20261013.md](../handoff-launch-readiness-and-d1-20261013.md) 文首 10-16 段。

## item
player_locator 会话对账去掉全键 SCAN,改为按 gate 实例建索引(容量模型 §1.3 第 3 条 / §A0-4 / §4.2「第 0 批」;全局单点交接 §4.3 / §5 / §7 阶段 3)
## already_done
无。核对依据:docs/PROGRESS.md 里只有 2026-08-10 那条原始对账的落码记录(PROGRESS.md:2292),10-13 之后的条目都没有这一项的落码记录;git diff docs/PROGRESS.md 新增段落中没有「对账 / reconcil / gate_index」相关内容;go/player_locator 整个目录 git status 干净,其中 session_reconciler.go 等文件 mtime 为 2026-10-12 03:53。相关但不等于本项的已有工作:D77 宽松解析 NodeInfo 已落地(session_reconciler.go:166-186 + session_reconciler_unknown_field_test.go);MarkOffline 已改为删会话键(markofflinelogic.go:30-38)。全局单点交接 §4.3 建议的「单 leader + 每页一次 MGET + last_success_unix」也没有落码。在途冲突:docs/handoff/landing-fencing-takeover-20261016/README.md §3.3 选定的 e 方案计划改 setdisconnectinglogic.go、markofflinelogic.go、logic_test.go、player_locator.proto,b 方案计划改 leasemonitor.go:304-307。这些都还没落码(文件仍干净),但会和本项的两处小改动产生文本冲突,详见 plan 与 risks。
## summary
问题:SessionReconciler 由每个 zone 的每个 player_locator 副本各自运行(无选主),每 60s 用 SCAN MATCH player:session:* COUNT 500 把共享 Redis 整个键空间走一遍,命中的键再逐个 GET(没有 pipeline)。按 2000 万在线推算:Z=200~445,副本数 2Z=400~890,会话数 N≈2e7,库内总键数 K≈6.5e9。每拍(60s)全体副本合计约 8e9~1.8e10 次 GET 加 5.2e9~1.2e10 次 SCAN,折合每秒 1.3e8~3.0e8 次 GET 加 8.7e7~1.9e8 次 SCAN。这比核心 Redis 其余全部流量(约 1.7e6/s)高两个数量级以上,而且 GET 项按 Z·N∝PCU² 增长。单副本跑完一轮约需数小时,「连续两轮缺席才清理」随之拖到小时级,对账名存实亡。

根因:会话键按 player_id 散列存放,没有任何按 gate 的二级结构,判定「哪些会话挂在已死的 gate 上」只能扫全库。

做法:
1) 在 player_locator 现有的 3 段会话 Lua(replace / setDisconnecting / deleteIfUnchanged)里同步维护派生索引:player:gate_index:sessions:<gate_instance_uuid>(SET,成员为 player_id)与登记集 player:gate_index:gates(SET,成员为 gate uuid)。不变量:「会话 ONLINE 且 gate_instance_id 非空」当且仅当「player_id 在该 gate 的索引里」。会话写入和索引写入在同一段 Lua 内完成,天然原子。
2) 对账改为全局单 leader(shared/leader,Redis SET NX)。每拍只做三件事:SMEMBERS 登记集、Range 一次 etcd gate 前缀求出死 gate、对连续两拍缺席的 gate 用 SSCAN + MGET 逐个校验后 CAS。CAS 沿用 SetDisconnecting 的同一段 Lua,并原子摘除索引成员。脏成员按「字节未变」守卫删除,自愈。
3) 漏成员(旧版本写者、外部篡改)靠「补建 / 审计扫描」修复。该扫描只补索引、不转换会话:就绪标记 player:gate_index:ready 缺失时连续跑,就绪后每 24h 跑一次,限速且只由 leader 执行。
4) 滚动升级:新版 locator 在 etcd 注册事务里多写一个同租约的能力键 /mmorpg/capability/player_locator/gate_session_index/v1/<uuid>。leader 每拍比对「已注册的 locator」与「有能力键的 locator」;发现旧写者就删就绪标记,回到连续补建。旧进程自带的全键扫描对账在它退出前照常兜底。
5) 不改 proto、不改客户端契约、不改 C++、不改 login,会话值格式不变,读者零影响。

改造后稳态每拍约 3 条 Redis 命令(含在线人数 gauge 时约 N_g=1000~4444 条 SCARD pipeline)外加 2 次 etcd Range。写路径不增加往返,只在既有脚本内多 1~3 次集合操作。日审计约 2.65e7 条命令 / 天,平均约 300/s。总量下降约 6 个数量级。索引常驻内存约 1~1.6GB(推断)。
## est_files: 26
## current_facts
F1. 对账每拍扫的是共享 Redis 的整个键空间:SCAN MATCH player:session:* COUNT 500,游标走到 0 为止;命中键逐个单独 GET,没有 MGET 或 pipeline;默认周期 60s,ticker 会丢拍,一轮超时就退化为背靠背连跑
   证据: 已亲自打开:go/player_locator/internal/logic/session_reconciler.go:41-45(defaultReconcileInterval=60s、reconcileScanBatch=500)、:66-69、:89-103(ticker + safego.Run)、:121-143(Scan 循环,pattern = sessionKeyPrefix+"*")、:196(逐键 Get);keys.go:7 sessionKeyPrefix="player:session:"
F2. 周期配置在 dev yaml 与 K8s 渲染里都没设,实际就是默认 60s;-1 关闭
   证据: 已亲自打开:go/player_locator/internal/config/config.go:51-59(ReconcileIntervalSeconds optional,0=默认,-1=关闭);etc/player_locator.yaml:31-34 的 Lease 段只有 DefaultTTLSeconds / PollInterval / BatchSize;tools/scripts/k8s_deploy.ps1:5991-6017 的 player-locator heredoc 的 Lease 段同样只有这三项
F3. 每个 locator 进程都跑对账,没有选主;每个 zone 2 副本,而且所有 zone 的 locator 连的是同一个 Redis(D12),所以全服是 2Z 个实例在重复扫同一个键空间
   证据: 已亲自打开:go/player_locator/player_locator.go:99-109(每进程 safego.Go 启动 StartSessionReconciler);deploy/k8s/manifests/go-svc/player-locator.yaml:24-30(replicas: 2,注释写明「代价只有共享 Redis 每 60s 多一轮 SCAN」);k8s_deploy.ps1:1773(Scope='zone')、:6000-6003(Redis 统一指向 redis.${InfraNamespace});docs/PROGRESS.md:5618(「各 zone 的 player_locator 必须连同一个 Redis(D12)」,grep 读到)
F4. 失效判定的三个条件:State==ONLINE、GateInstanceId 非空、gate 实例 uuid 不在 etcd 存活集。同一实例连续两轮缺席才动手,去抖表 suspects(playerID→uuid)存在各进程内存里。动手就是整字节 CAS 成 DISCONNECTING,版本号 +1,同时 ZADD player:leases,deadline = now + Lease.DefaultTTLSeconds(30s)。etcd 列举失败则整轮跳过(fail-closed)
   证据: 已亲自打开:session_reconciler.go:85-87、:107-118、:210-226、:227-263(setDisconnectingScript.Run 的 ARGV:data / updated / 0 / deadline / pid)
F5. 存活集取自 etcd GateNodeService.rpc/ 全前缀 Range(超时 5s)。NodeInfo 用 nodeinfo.Unmarshal 宽松解析;解析失败的值(allocated/ 占位键的裸 uuid)原样收进存活集,宁可多收也不误杀
   证据: 已亲自打开:session_reconciler.go:47-51(gateRpcPrefix)、:152-186;session_reconciler_unknown_field_test.go 两条用例
F6. gate 实例 uuid 在每次进程启动时随机生成。gate 在 etcd 有两个键,挂在同一租约上:<Svc>.rpc/zone/<z>/node_type/<t>/node_id/<id>(值为 NodeInfo JSON)和 <Svc>.rpc/allocated/node_type/<t>/node_id/<id>(值为 uuid)。node_id 会在重启后复用,uuid 不会,所以索引必须按实例 uuid 建,不能按 gate_id 建
   证据: 已亲自打开:cpp/libs/engine/core/node/system/node/node.cpp:557(set_node_uuid(uuidGenerator()));cpp/libs/engine/core/node/system/etcd/etcd_manager.cpp:47-75(键形)、:102-144(两阶段写、同租约);proto/player_locator/player_locator.proto:33-51(PlayerSession.gate_id=3、gate_instance_id=4,没有 zone 字段)
F7. player:session:* 只有 Go 的 player_locator 一个写者,C++ 不写(C++ 侧全仓 grep 只在 battle_room_manager.cpp:208 有一处注释)。写入点如下:SetSession(创建或替换为 ONLINE,同时撤销 lease);Reconnect(DISCONNECTING→ONLINE,换 gate);SetDisconnecting(ONLINE→DISCONNECTING,ZADD lease);MarkOffline(CAS 删会话、旧 location 与 lease 簿记,随后调 LeaveScene 或入队);对账(与 SetDisconnecting 同一段 Lua);LeaseMonitor 的 commitLeaseExpiry(删 DISCONNECTING 会话);rearm(PERSIST 会话、ZADD ready);claim / renew / ack / enqueueOfflineCleanup(只动 lease 键,不写会话)
   证据: 已亲自打开:setsessionlogic.go:30-101(:89 调 replaceSessionAndCancelLease)、reconnectlogic.go:30-114(:75-86 原地改 session,:95 提交)、setdisconnectinglogic.go:31-112(:75-76 原地改,:90-99 Lua)、markofflinelogic.go:39-100(:80 删除、:98 LeaveScene)、session_cas.go:29-57 / :59-73 / :75-89 / :94-124 / :129-142 / :144-187 / :203-225;internal/server/playerlocatorserver.go 只注册这 5 个写 RPC(grep 读到),SetLocation 已停用(setlocationlogic.go 已读)
F8. login 侧调用点:EnterGame 的 FirstLogin / ReplaceLogin 走 SetSession,ShortReconnect 走 Reconnect;Disconnect 走 SetDisconnecting,失败后带退避重试 4 次,仍失败只打 CRITICAL 日志;LeaveGame 先 GetSession 再带 expected id/version 调 MarkOffline;入场栅栏补偿也会调这两条
   证据: 已亲自打开:go/login/internal/logic/clientplayerlogin/entergamelogic.go:900-945(:925 SetSession、:930 Reconnect);session_lifecycle.go:46-95;pkg/sessionmanager/session_manager.go:106-175;disconnectlogic.go:40-56(:53;:50-51 注释「Redis key TTL expires」已过时)。grep 读到:leavegamelogic.go:52、entergamelogic.go:890 / :893
F9. C++ gate 断线时单向给 login 发 LoginNodeDisconnect:直连模式随机挑一个 login,路由模式经路由服转发,不重试。gate 整机崩溃时这条通知根本不会发出,会话永远停在 ONLINE,唯一的自愈路径就是对账
   证据: 已亲自打开:cpp/nodes/gate/handler/rpc/client_message_processor.cpp:510-557(该文件有他人未提交改动,本项不碰);session_reconciler.go:20-27 头注释
F10. TTL 现状:会话键在任何路径下都没有 TTL。三段写 Lua 虽支持 PX,但所有调用方都传 0,rearm 还会 PERSIST。player:leases / player:leases:processing(score 为 deadline 秒数)、claim_tokens / claim_payloads 哈希都没有键级 TTL。G4a 设计稿 §2.2 写的「断线后 session 带 PX」与代码不符,已过时
   证据: 已亲自打开:session_cas.go:47-51、:63-67、:182(PERSIST);setsessionlogic.go:89、reconnectlogic.go:95、setdisconnectinglogic.go:82-99、session_reconciler.go:248 均传 0;docs/design/shared-redis-split-by-purpose.md:130(grep 读到)
F11. 现有会话 Lua 本来就跨键:会话键加上全局键 player:leases、player:leases:processing、player:leases:claim_tokens、player:leases:claim_payloads,依赖单实例。所以「同一 Lua 维护索引」在当前 D12 单主前提下是原子的,也不会让日后集群化变得更难
   证据: 已亲自打开:session_cas.go:227-235(sessionLifecycleKeys)、keys.go:5-13;G4a 设计稿 :164(grep 读到)
F12. 会话读者只按 player:session:<id> 读,本项不改键也不改值格式,对它们零影响:friend、guild、match、mail、merge_zone,以及 login 身份补齐 Lua(只读)
   证据: grep 读到:go/friend/internal/data/session_reader.go:47、go/guild/internal/logic/online_status_resolver.go:17 与 push.go:160、go/match/internal/playercontract/playercontract.go:44、go/mail/internal/notify/notify.go:57、tools/merge_zone/preflight.go:582-588;已亲自打开 go/login/internal/logic/clientplayerlogin/player_class_backfill.go:89-95
F13. 容量文档给出的对账负载按 S1~S3 推算;全局单点交接在 1000 万在线下的推算为 GET≈3.3e7/s 加 SCAN≈4.3e7/s(K≈6.5e9)。本项按 2000 万重算:GET 项 ∝ Z·N,翻倍到 4 倍;SCAN 项 ∝ Z·K,K 由 DAU 决定(DAU 不变),翻 2 倍
   证据: 已亲自打开:docs/design/capacity-model-hok-scale.md:266(§2.5)、:414(§A0-4)、:661(§4.2)、:894-910(§7.2);docs/handoff/handoff-global-singletons-10m-ccu-20261013.md:178-181、:286(E2)
F14. 可直接复用的现成件:shared/leader(Redis SET NX + 属主续期 + 自我降级,trade / player_inbox / scene_manager 都在用);go-zero core/metric(guild 在用,player_locator 无需改 go.mod);shared/nodeinfo.Unmarshal
   证据: 已亲自打开:go/shared/leader/leader.go:1-25、:57-140(New / Run / Options{TTL, ID, OnStateChange})、store_goredis.go:19 NewGoRedisStore;grep 读到 go/guild/internal/logic/activity_metrics.go:84;go/player_locator/go.mod 中 prometheus 仅为 indirect、golang.org/x/time 仅为 indirect(所以不引入 x/time/rate)
F15. player_locator 在 etcd 的注册由同一租约的 Txn 一次写入 allocated 与 rpc 两个键,涉及两处:首注册 / 重新分配的 tryClaimNodeID,和 reRegister 第 1 步。NodeUuid 在每次进程启动时用 uuid.New 生成
   证据: 已亲自打开:go/player_locator/internal/node/node.go:166-187、:240-275;grep 读到 :85 NodeUuid: uuid.New().String()
F16. K8s 渲染的 player_locator 配置里没有 Prometheus 段,集群内实际不暴露 /metrics(这是既有缺口)。新增的指标与告警要等 scrape 接线之后才在 K8s 生效
   证据: 已亲自打开:k8s_deploy.ps1:5991-6017;grep 9190 在 k8s_deploy.ps1 与 player-locator.yaml 都无命中;etc/player_locator.yaml 的 Prometheus 段 Host 为 127.0.0.1:9190
F17. Java 版没有对应机制:在线目录 game:presence:{player_id} 以 gate 为唯一写者,TTL 60s、20s 续期。gate 崩溃后条目自然过期,不需要全键对账
   证据: 已亲自打开:D:/luyuan/xuanming-server-mmo/PARITY.md:64(「玩家在线目录」行);grep 读到 game-discovery/src/main/java/com/game/discovery/RedisKeys.java:151
## plan
- [go/player_locator/internal/logic/keys.go] 新增常量与函数,不改动已有内容:
gateIndexSessionsKeyPrefix = "player:gate_index:sessions:"
GateIndexRegistryKey = "player:gate_index:gates"
gateIndexReadyKey = "player:gate_index:ready"
gateIndexReadyValuePrefix = "v1:"
reconcilerLeaderKey = "player_locator:session_reconciler:leader"
func gateSessionIndexKey(gateUUID string) string:入参为空串时返回空串,否则返回前缀 + uuid。

命名理由(写进注释):
1) 前缀用 player: 而不用容量文档建议的 gate:{uuid}:sessions。gate:{id}:draining / drained 属于排队族,G4a 第 1 批要整体搬到 redis-queue;索引必须和会话一起留在核心闭包里,用 player: 前缀能让拆分工具与清库守卫按族识别。
2) 现在不加 hash tag。索引只在会话 Lua 内和会话键一起被访问,而这些脚本本来就因全局 player:leases* 跨槽,加 tag 换不来任何原子性;集群化时的键形见设计文档「前瞻」一节。
   工作区: git status 干净;mtime 2026-10-12 03:53;无他人改动
- [go/player_locator/internal/logic/session_cas.go] 一、replaceSessionAndCancelLeaseScript
KEYS 从 5 个扩到 8 个:KEYS[6] 为旧索引键或 "",KEYS[7] 为新索引键或 "",KEYS[8] 为 GateIndexRegistryKey。ARGV 增加 ARGV[6],即新 gate uuid 或 ""。
在原 `HDEL KEYS[5]` 之后、`return 1` 之前插入:
```
if KEYS[6] ~= "" and KEYS[6] ~= KEYS[7] then redis.call("SREM", KEYS[6], ARGV[5]) end
if KEYS[7] ~= "" then
  redis.call("SADD", KEYS[7], ARGV[5])
  redis.call("SADD", KEYS[8], ARGV[6])
end
```
missing 模式在登记任何清理前返回 2、CAS 失败返回 0,这两条早退路径都不碰索引。

二、setDisconnectingScript
KEYS 增加 KEYS[6],即旧索引键或 ""。在 HDEL 之后插入:
```
if KEYS[6] ~= "" then redis.call("SREM", KEYS[6], ARGV[5]) end
```

三、deleteSessionIfUnchangedScript
KEYS 增加 KEYS[7],即旧索引键或 ""。在 HDEL 之后插入:
```
if KEYS[7] ~= "" then redis.call("SREM", KEYS[7], ARGV[3]) end
```

四、Go 函数签名
- replaceSessionAndCancelLease(ctx, svcCtx, playerID, expected, updated []byte, ttl, previous, next *pb.PlayerSession)。previous 在 missing 模式下为 nil。内部用 indexedGate 推导两个索引键与 ARGV[6]。
- deleteSessionIfUnchanged(ctx, svcCtx, playerID, expected []byte, current *pb.PlayerSession)。
- 新增 transitionToDisconnecting(ctx, svcCtx, playerID uint64, expected, updated []byte, deadlineUnix int64, previous *pb.PlayerSession) (bool, error),封装 setDisconnectingScript.Run。SetDisconnecting 与对账共用它,消除 session_reconciler.go:242-251 那份重复。

五、必须写进注释的三点
1) 索引键与成员都由 Go 从「CAS 的 expected 字节解出的旧会话」和「即将写入的新会话」推导。整字节 CAS 保证提交时 Redis 里的就是 expected,所以推导与提交之间没有窗口。
2) 不变量:ONLINE 且 gate_instance_id 非空 ⇔ 成员在该 gate 的索引里。
3) commitLeaseExpiry / rearm / claim / ack / enqueue 都不改。它们只处理 DISCONNECTING 或已删的会话,不在索引里;这一点同样写进注释,防止以后有人往这些脚本里补 SREM。

空串键是 Redis 合法键名,脚本以 ~= "" 守卫,保证永不写入名为 "" 的键,由测试 6 钉住。
   工作区: git status 干净;mtime 2026-10-12 03:53;无他人未提交改动。landing-fencing 的 b / e 方案文本里没有列这个文件,但 b 若改 LeaveScene 的 Lua 会改 scene_manager 侧,与本文件无关
- [go/player_locator/internal/logic/gate_session_index.go(新增)] 本项的纯函数与索引专用 Lua:

1) func indexedGate(s *pb.PlayerSession) string
s 非 nil、State==ONLINE 且 GateInstanceId 非空时返回 uuid,否则返回 ""。

2) pruneGateIndexMemberScript
KEYS 为 session 键与索引键;ARGV 为 mode(present / missing)、expected、pid。会话仍是 expected 字节(missing 模式下为仍不存在)才 SREM,否则返回 0。

3) releaseGateIndexIfEmptyScript
KEYS 为索引键与登记集。SCARD>0 时返回 0;否则 DEL 索引键、SREM 登记集中的 uuid,返回 1。

4) repairGateIndexMemberScript
KEYS 为 session 键、索引键、登记集;ARGV 为 expected、pid、uuid。GET ~= expected 时返回 -1;否则 SADD 成员与登记集,返回成员 SADD 的结果(1 表示原先缺失)。

5) 能力键前缀
GateSessionIndexCapabilityPrefix = "/mmorpg/capability/player_locator/gate_session_index/v1/",导出给 main 传给 node 包。不放在 PlayerLocatorNodeService.rpc/ 下面:C++ 把该类型当作 zone 级发现前缀解析(node_util.cpp:115-131),混进非 NodeInfo 值有解析风险。

6) type indexedAction 与 classifyIndexedMember(member string, raw []byte, found bool, deadGate string) (pid uint64, s *pb.PlayerSession, act indexedAction),返回值含义:
- member 不是十进制 uint64:PruneBadMember,直接 SREM
- 会话键不存在:PruneMissing
- 无法 Unmarshal,或 payload 的 PlayerId 与 member 不符:PruneMalformed(按字节守卫,打 ERROR 日志)
- ONLINE 且 GateInstanceId==deadGate:Transition
- 其余:PruneStale
   工作区: 新文件(已确认不存在)
- [go/player_locator/internal/logic/session_reconciler.go] 整体重写,头注释保留原「为什么需要对账、fail-closed 原则」,改写「怎么做」。

一、StartSessionReconciler(ctx, svcCtx, p ReconcilerParams)
ReconcilerParams 含 EtcdEndpoints、DialTimeout、IntervalSeconds、Reconcile config.ReconcileConf(已 Normalized)、LeaderID(取 node uuid)。
- IntervalSeconds<0 时照旧禁用。
- 自建 etcd 客户端,连接失败时打 ERROR 并退出(同现状)。
- 选主:leader.New(leader.NewGoRedisStore(svcCtx.RedisClient), reconcilerLeaderKey, leader.Options{TTL:30s, ID:LeaderID, OnStateChange: 设 is_leader gauge})。
- elector.Run(ctx, func(lctx){ r := newSessionReconciler(...); r.run(lctx) })。
- 注释写明:选主只用于削减负载,不是正确性 fence。两个 leader 短暂重叠时,所有动作都走整字节 CAS 或字节守卫 Lua,无害。

二、sessionReconciler 结构
字段:rdb、view clusterView、cfg、now func() time.Time、pace pacer(Wait(ctx) error)、suspects map[string]int(仅索引循环访问)、legacyEpoch atomic.Uint64(两条循环共享;发现旧写者或无法判定时自增,不用墙钟)。

三、run(lctx)
safego.Go 起补建 / 审计循环;本 goroutine 跑索引循环,每拍 safego.Run("player_locator.session_reconciler.round")。

四、indexRound(ctx),顺序固定
a) view.SessionWriters 返回已注册 locator uuid 与有能力键的 uuid。出错或存在差集时:legacyEpoch++,DEL gateIndexReadyKey,设 legacy_writers gauge。
b) SMEMBERS 登记集。必须先于 c:gate 先在 etcd 注册,login 才会分配它,所以登记集快照里的 gate 一定早于 etcd Range 注册过。
c) view.LiveGateInstances。出错则整拍返回,suspects 不推进(fail-closed)。
d) 遍历登记集:存活则 delete(suspects,g);缺席则 suspects[g]++,≥2 时进入 processDeadGate(g)。不在登记集里的 suspects 条目删除。
e) 刷新 gauge:dead_gates;gate_index_members(对存活 gate 的 SCARD 做 pipeline 求和);last_success_unix{loop="index"}(用 now())。

五、processDeadGate
循环 SSCAN idx COUNT 500,对本页成员 MGET 会话键,逐个 classifyIndexedMember,按结果执行:
- Transition:先 pace.Wait;克隆会话,置 DISCONNECTING、Version++、Marshal;调 transitionToDisconnecting,deadline = now()+DefaultTTLSeconds(0 时取 30s,同现状)。返回 false 计 cas_conflict,成员保留,下一拍重判。
- Prune*:按对应模式调 prune 脚本。
全部页处理完后调 releaseGateIndexIfEmptyScript。日志带 gate uuid、各动作计数;单个 player 只在转换时记一行 INFO,同现状。

六、补建 / 审计循环 repairLoop(lctx)
- 就绪标记缺失:立即跑 repairPass;结束后若满足「通过前后 legacyEpoch 未变,且结束时再查一次 SessionWriters 无旧写者」,则 SET gateIndexReadyKey = "v1:<now unix>"。否则间隔 IntervalSeconds 再跑。
- 标记存在:每 AuditIntervalSeconds 跑一次;为 -1 时不跑,只每 IntervalSeconds 检查标记是否被删。

七、repairPass
SCAN MATCH player:session:* COUNT ScanCount,每页前 pace(按 ScanPagesPerSecond);本页 MGET;对 ONLINE 且 gate 非空的会话做 SISMEMBER pipeline;未命中的调 repairGateIndexMemberScript,返回 1 计 repaired。只补索引,不转换会话:补进来的死 gate 由索引循环两拍内处理,转换只有一条路径。标记存在时 repaired>0 打 ERROR,说明不变量被破坏,需要排查。

八、保留与删除
- 保留 collectLiveGateInstances,行为不变,现有测试不动;新增 collectNodeUUIDs 供写者检查复用同一解析规则。
- 删除 reconcileOnce / handleSessionKey / 进程内按玩家的 suspects;逻辑并入上面的流程,或被替代。
   工作区: git status 干净;mtime 2026-10-12 03:53;无他人改动
- [go/player_locator/internal/logic/reconcile_view.go(新增)] 定义 clusterView 接口:
- LiveGateInstances(ctx) (map[string]struct{}, error)
- SessionWriters(ctx) (all, capable map[string]struct{}, err error)

生产实现 etcdClusterView{cli *clientv3.Client}:
- LiveGateInstances:沿用 listLiveGateInstances,GateNodeService.rpc/ 前缀,超时 5s。
- SessionWriters:Range PlayerLocatorNodeService.rpc/ 前缀,用 collectNodeUUIDs 解析,含 allocated 裸 uuid;Range GateSessionIndexCapabilityPrefix 前缀并取键尾 uuid。

再提供 pacer 接口与 intervalPacer(time.Ticker 实现;perSecond≤0 时不限速),单测注入 no-op。

接口存在的理由:隔离 etcd 外部依赖,作为测试接缝(AGENTS §11.5.2)。
   工作区: 新文件(已确认不存在)
- [go/player_locator/internal/logic/reconcile_metrics.go(新增)] 用 go-zero core/metric 定义以下指标(Namespace player_locator,label 全为有限枚举,不用 player_id 也不用 gate uuid 作 label):
- reconcile_round_total{loop=index|repair, result=ok|etcd_error|redis_error}
- reconcile_last_success_unix{loop}
- reconcile_sessions_total{action=to_disconnecting|cas_conflict|pruned_stale|pruned_missing|pruned_malformed|repaired}
- reconcile_dead_gates
- reconcile_legacy_writers
- reconcile_is_leader
- gate_index_ready
- gate_index_members

注释写明 gauge 只由 leader 产出,告警规则须用 max() 聚合。
   工作区: 新文件(已确认不存在)
- [go/player_locator/internal/logic/setsessionlogic.go] 只改 :89 一处调用:replaceSessionAndCancelLease(l.ctx, l.svcCtx, s.PlayerId, expected, data, 0, previous, s)。
- previous 在 err==redis.Nil 时为 nil,否则为 :49 解出的 current。
- 需要把 current 提到 if 外层声明。

其余逻辑(继承 scene 字段、重放判断、代次栅栏)不动。
   工作区: git status 干净;mtime 2026-10-14 23:06(随 ebccaf04d 入库);landing-fencing a 方案选定 B,不改本文件
- [go/player_locator/internal/logic/reconnectlogic.go] 在 :75 原地修改 session 之前,先 previous := proto.Clone(session).(*pb.PlayerSession)。这里必然是 DISCONNECTING,索引推导为 "",但按通用规则仍然传入。

:95 改为 replaceSessionAndCancelLease(..., data, updated, 0, previous, session)。
   工作区: git status 干净;mtime 2026-10-14 23:06
- [go/player_locator/internal/logic/setdisconnectinglogic.go] 在 :75 原地修改之前,先保存 previous(克隆,或只取 indexedGate(session))。

:89-99 的 setDisconnectingScript.Run 换成 transitionToDisconnecting(l.ctx, l.svcCtx, in.PlayerId, data, updated, time.Now().Add(ttl).Unix(), previous)。

日志与返回语义不变。
   工作区: git status 干净;mtime 2026-10-12 03:53。冲突风险中:landing-fencing e 方案计划在 :48-53 之后插入 gate_instance_id 比对(尚未落码),后落者需要做文本合并,语义互不影响
- [go/player_locator/internal/logic/markofflinelogic.go] :80 改为 deleteSessionIfUnchanged(l.ctx, l.svcCtx, playerID, data, current)。其余不动。
   工作区: git status 干净;mtime 2026-10-12 03:53。冲突风险中:landing-fencing e 方案改 :65 的比对条件,b 方案改 :90-98 注释(均未落码),后落者做文本合并
- [go/player_locator/internal/config/config.go] LeaseConf 新增 Reconcile ReconcileConf `json:"Reconcile,optional"`。ReconcileConf 各字段均为 optional,零值即默认:
- AuditIntervalSeconds int64:0 表示 86400,-1 表示索引就绪后不再定期审计(未就绪时的补建不受影响)
- ScanCount int64:0 表示 1000
- ScanPagesPerSecond int:0 表示 200
- MaxTransitionsPerSecond int:0 表示 1000

提供 func (c ReconcileConf) Normalized() ReconcileConf。保留 ReconcileIntervalSeconds 原语义(0=60s,-1=整个对账关闭),注释改写为新机制。

默认值都是推断值([I]),注释写明需要 V0 实测后校准。
   工作区: git status 干净;mtime 2026-10-12 03:53
- [go/player_locator/etc/player_locator.yaml] 在 Lease 段下加 Reconcile 块,显式写出四个默认值,并用中文注释说明:索引对账、补建 / 审计扫描的限速含义、-1 的语义。

只服务于 dev 与文档化。K8s 渲染不需要改:零值取默认,所以不碰 k8s_deploy.ps1。
   工作区: git status 干净;mtime 2026-10-12 03:53
- [go/player_locator/internal/node/node.go] 1) NewNode 增加可变参 opts ...Option,新增 WithLeaseBoundMarkers(prefixes ...string)。node 包把它们当作不透明的「前缀 + NodeUuid」键,值为 "1",不知道索引的业务含义(SRP)。
2) 抽出纯函数 registrationOps(allocKey, rpcKey, uuid, nodeInfoJSON string, markerKeys []string, lease clientv3.LeaseID) []clientv3.Op。tryClaimNodeID(:177-181)与 reRegister 第 1 步(:259-264)两处 Txn 的 Then 都改用它,于是能力键与 NodeInfo 在同一事务、同一租约里出现和消失,不存在「有 NodeInfo 没能力键」的窗口,也就不会误判旧写者。
3) Close / Revoke 的路径不用改。
   工作区: git status 干净;mtime 2026-10-12 03:53;无在途计划涉及
- [go/player_locator/player_locator.go] 1) :56 改为 node.NewNode(nodeType, host, port, node.WithLeaseBoundMarkers(logic.GateSessionIndexCapabilityPrefix))。
2) :99-109 改为传 logic.ReconcilerParams{..., Reconcile: config.AppConfig.Lease.Reconcile.Normalized(), LeaderID: n.Info.NodeUuid},注释同步改写。

注册在 s.Start()(:133)之前完成,本进程处理任何写 RPC 之前能力键已经存在。
   工作区: git status 干净;mtime 2026-10-12 03:53
- [go/player_locator/internal/logic/gate_session_index_test.go(新增)] 写路径行为测试(miniredis,只经公开的 Logic RPC 驱动),见 tests 第 1~10 条。
   工作区: 新文件。刻意不改 logic_test.go,避开 landing-fencing e 方案的在途改动
- [go/player_locator/internal/logic/session_reconciler_test.go(新增)] 索引循环、补建 / 审计、标记、旧写者检测、纯函数的测试,见 tests 第 11~26 条。
- 用 fakeClusterView(可在测试中途切换返回值)。
- 用 go-redis Hook(ProcessHook / ProcessPipelineHook)计数命令,并在指定命令之后注入并发写,以确定性地复现竞态。
- now 固定,pacer 为 no-op,不依赖墙钟。
   工作区: 新文件
- [go/player_locator/internal/config/config_test.go] 追加两条用例:
- TestReconcileConfNormalized:零值取默认,-1 保留。
- TestEtcYamlLoadsReconcileBlock:yaml 中的四个值能读出。
   工作区: git status 干净;mtime 2026-10-12 03:53
- [go/player_locator/internal/node/node_registration_test.go(新增)] 对 registrationOps 做单测:
- Op 数为 2 + 标记数;
- 每个 Op 的 KeyBytes 依次为 allocKey、rpcKey、前缀+uuid;
- 无标记时退化为原来的两键。

可选:在 node_integration_test.go(build tag integration)里补一条断言,NewNode 后能力键存在,Close 后消失。
   工作区: 新文件;node_integration_test.go 干净,mtime 2026-10-12
- [deploy/k8s/manifests/go-svc/player-locator.yaml] 只改 :24-29 的注释:SessionReconciler 改为全局单 leader + 按 gate 索引,不再每副本 SCAN。多副本安全性的表述保留:转换仍走整字节 CAS,重叠无害。不改任何字段。
   工作区: git status 干净;mtime 2026-10-12 03:53:02
- [deploy/k8s/player-locator-alerts.yaml(新增)] PrometheusRule,格式照 scene-manager-alerts.yaml 与 player-inbox-alerts.yaml 的约定,gauge 一律用 max() 聚合。规则:
- Stalled:time()-max(reconcile_last_success_unix{loop="index"})>300,持续 5m,critical
- NoLeader:sum(reconcile_is_leader)==0,持续 5m,critical
- LegacyWriters:max(reconcile_legacy_writers)>0,持续 30m,warning
- IndexNotReady:max(gate_index_ready)==0,持续 24h,warning
- RepairedWhileReady:increase(reconcile_sessions_total{action="repaired"}[1d])>0 and on() max(gate_index_ready)==1,warning
- DeadGatesPending:max(reconcile_dead_gates)>0,持续 15m,warning

文件头写明前提:K8s 目前没有暴露 player_locator 的 :9190(既有缺口),scrape 接线之前这些规则不会生效。
   工作区: 新文件(已确认不存在)
- [docs/design/player-locator-gate-session-index.md(新增)] 设计文档,章节:
1. 问题与现状:附 current_facts 的行号。
2. 键形与不变量:I1「ONLINE 且 gate 非空 ⇔ 是索引成员」;I2「索引非空 ⇒ uuid 在登记集」;I3「索引只是提示,用前必须按会话字节校验」。
3. 写路径逐脚本对照表:SetSession、Reconnect、SetDisconnecting、MarkOffline、对账转换、commit、rearm、enqueue。
4. 对账状态机:选主、两拍去抖、死 gate 处理、释放。
5. 自愈:脏成员按字节守卫删除;漏成员靠补建 / 审计。
6. 滚动升级与回滚:能力键、就绪标记、旧写者检测的残余风险、永久回滚后的清理命令 `redis-cli --scan --pattern 'player:gate_index:*'` 逐个 DEL(只在确认不再使用新版本后执行)。
7. 2000 万在线容量推算(本项 summary 的数字,标 [I] / [D])。
8. 集群化前瞻:见 risks / open_decisions,内容如下。
   - 路线 R(单元化单主)不变。
   - 路线 C(集群)时,会话 Lua 本来就因为全局 player:leases* 必须重构。届时按会话 {player_id} 所在 slot 的预计算 slot-tag,把索引与登记集分桶为 player:gate_index:{<slottag>}:sessions:<uuid> 和 player:gate_index:{<slottag>}:gates,与 lease 的 slot 分桶同批设计,保持 CAS 与索引同槽原子;死 gate 按桶遍历。
   - 备选的非原子方案是「先写索引、再写会话」加「按 login_generation 打分的 ZSET 条件删除」。它在乱序删除上有 ABA 与时序复杂度,不推荐。
   - 索引是派生数据,可以靠补建重建,所以迁移只需要「新形状写者(能力键 v2)+ 删就绪标记 + 补建」,不必停写。
9. 指标与告警。
10. 测试与验证。
11. Java 不适用的理由。
   工作区: 新文件(已确认不存在)
- [docs/README.md] 在设计分类下追加一行索引,指向新设计文档。
   工作区: M:他人未提交改动 12 行,经 git diff 确认与本项无关;mtime 2026-10-15 23:08。只追加,不动他人的行
- [docs/PROGRESS.md] 按 AGENTS §5 只追加一条:「2026-10-xx 会话对账改按 gate 索引(Claude 落码,未编译、未测试,待 Codex 验证)」。内容包括:改动文件清单、交给 Codex 的命令、Java 版本号(不适用)、待办(K8s 暴露 /metrics)。
   工作区: M:他人未提交 +228 行,与本项无关;.gitattributes 配了 merge=union;mtime 2026-10-16 03:56。只在文件末尾另起一条
- [docs/design/capacity-model-hok-scale.md] 正文不改,只加带日期的状态注:
- §1.3 第 3 条、§2.5 会话对账行、§A0 第 4 条、§4.2 新增第 0 批:「对账按 gate 索引已落码,待 Codex 验证,见新设计文档;dirty_keys_set 等其余项不在本项」。
- 同时更正 §A0-4 里不存在的 RPC 名 SetOnline:实际是 SetSession / Reconnect。
   工作区: git status 干净;mtime 2026-10-15 14:38
- [docs/design/shared-redis-split-by-purpose.md] 1) §2.2 :130 的会话族行:键列表加入 player:gate_index:*(必须与会话同实例,属于核心闭包),并把「断线后 session 带 PX」更正为「会话无 TTL」(setdisconnectinglogic.go:82-88)。
2) §2.3 :164 注明索引键在同一批 Lua 里。
3) §4.6 守卫模式清单补 player:gate_index:*。
只加注和更正,不重写。
   工作区: git status 干净;mtime 2026-10-15 14:38
- [docs/ops/release-checklist.md] 附录 B.1 :270 那行原写法是「ZCARD player_locator:lease_zset + SCAN player:session:」,其中键名本身就是错的。改为:在线数看 player_locator_gate_index_members 指标,或对 SMEMBERS player:gate_index:gates 得到的每个 gate 执行 SCARD;断线中的人数用 ZCARD player:leases。并去掉生产全键 SCAN 的建议。
   工作区: git status 干净;mtime 2026-10-15 14:08
- [D:/luyuan/xuanming-server-mmo/PARITY.md(Java 仓,仓外)] 只在「玩家在线目录」行(:64)末尾追加一句:「2026-10-xx 基线会话对账改按 gate 实例索引、去全键 SCAN(本仓 commit 待填);Java 不适用:presence 带 TTL、gate 唯一写者,无全键对账」。不碰该仓其他文件。
   工作区: M:该仓 PARITY.md 有他人未提交改动,mtime 2026-10-16 03:32;只追加,不覆盖
## compat_and_rollout
协议与契约:不改任何 proto,不改客户端可见内容(帧、tip、配表),不改 C++,不改 login / friend / guild / match / mail。会话键名、值格式与语义全部不变,所有读者零影响。

新增数据(全部是派生数据,可重建):
- Redis 键 player:gate_index:sessions:<uuid>、player:gate_index:gates、player:gate_index:ready、player_locator:session_reconciler:leader(带 TTL 30s)。
- etcd 键 /mmorpg/capability/player_locator/gate_session_index/v1/<node_uuid>,随节点租约存亡,每个 locator 一个,全服数百个。

Lua 版本并存:新旧脚本 SHA 不同,go-redis 的 EVALSHA 遇到 NOSCRIPT 会回退 EVAL,新旧可以同时存在于脚本缓存。

滚动升级(只涉及 player_locator,任意顺序逐 zone 发布):
1) 新进程的所有会话写入从第一条起就维护索引,同时注册能力键。
2) 先上来的新进程之一当选 leader,发现仍有旧版 locator(etcd 里有 NodeInfo 但没有能力键)后:删除就绪标记,持续跑限速的补建扫描(只补索引)。索引循环照常按已有索引处理死 gate。
3) 旧进程在退出前照常运行它自带的全键扫描对账,旧写者写出的会话因此一直有兜底。
4) 旧版 locator 全部退出后,下一次完整且干净的补建通过设置就绪标记,随后改为每天审计一次。
5) 不需要任何配置开关,也不需要人工切换;K8s 渲染不用改,零值取默认。

新旧混跑的每种组合都安全:
- 旧写者把玩家从 A 换到 B:A 的索引留下脏成员(按字节守卫删除);B 的索引缺成员(由旧写者检测加补建补齐)。
- 旧对账或旧 MarkOffline 不摘索引:只留下脏成员,无害。

回滚:
- 回滚到旧二进制后,旧二进制恢复「每副本全键扫描」,覆盖完整。新版留下的索引键是无害的垃圾。若之后再升级,回滚期间已删掉的就绪标记会触发重新补建。
- 确认永久回滚时,按设计文档执行一次 `--scan --pattern 'player:gate_index:*'` 逐个 DEL,并 DEL leader 键;etcd 能力键会随租约自动消失。

单 leader 失效:换主最迟一个 TTL(30s)完成,去抖表重置,最多推迟一拍。两个 leader 短暂重叠时,所有动作都经整字节 CAS 或字节守卫,无害。

压测纪律:AGENTS §6.2 的 FLUSHALL 会同时清掉索引与就绪标记,下次启动自动补建。deploy-staging.sh / stress-linux-tier.sh 只删 player:session:*,会留下脏成员与幽灵 gate,它们会被对账按「死 gate」处理后释放,无需改脚本。

Java 版无需同批(见 java_parity)。
## tests
- 约定:以下全部为面向行为的单测,用 miniredis + 公开的 Logic / RPC 驱动;不读墙钟(now 注入固定值,pacer 用 no-op,lease deadline 只断言成员存在,或用注入的 now 断言);并发通过 go-redis Hook 在指定命令之后同步注入,结果确定。修复前全部为红:写路径的 1~10 在 HEAD 上会因索引键不存在而断言失败;11~26 依赖新函数,在 HEAD 上编译失败或断言失败。
- 1 TestGateIndex_FirstLoginIndexesOnlineSession(gate_session_index_test.go):SetSession(ONLINE、gate=inst-A)之后,player:gate_index:sessions:inst-A 含该 pid,player:gate_index:gates 含 inst-A。
- 2 TestGateIndex_ReplaceLoginMovesMemberAcrossGates:先在 A 上,再以版本 +1、gate=B 调 SetSession → 成员已不在 A,出现在 B。
- 3 TestGateIndex_ReplaceLoginOnSameGateKeepsMember:在同一 gate 上顶号 → 仍是成员,SCARD=1。用来防止「先 SADD 后 SREM」的顺序写错。
- 4 TestGateIndex_SetDisconnectingRemovesMember:成员被移除;会话状态为 DISCONNECTING;player:leases 含该 pid(原行为保留)。
- 5 TestGateIndex_ReconnectIndexesNewGate:DISCONNECTING 状态下 Reconnect 到 B → 成员在 B,不在 A。
- 6 TestGateIndex_EmptyGateInstanceNotIndexed:GateInstanceId 为空串 → 登记集为空;不存在 player:gate_index:sessions: 前缀的键;mr.Exists("") 为 false(钉住空串哨兵从不落键)。
- 7 TestGateIndex_StaleVersionLeavesIndexUntouched:SetSession 以陈旧版本被拒 → 成员仍在 A,B 不存在。
- 8 TestGateIndex_ReplayIsIdempotent:同一请求重放两次 → SCARD=1。
- 9 TestGateIndex_CleanupPendingDoesNotIndex:预置 processing claim,missing 模式的 SetSession 返回 ErrSessionCleanupPending → 没有任何索引键。
- 10 TestGateIndex_MarkOfflineRemovesMember:会话删除后成员被移除;MarkOffline 带陈旧 expected 时成员保留。
- 11 TestIndexRound_DeadGateNeedsTwoConsecutiveRounds(session_reconciler_test.go):fake view 中 A 缺席。第 1 拍会话不变;第 2 拍会话变为 DISCONNECTING、版本 +1,player:leases 含 pid,索引键 A 被 DEL,登记集不含 A。
- 12 TestIndexRound_LiveGateUntouched:A 存活,跑多拍后会话字节与索引都不变。
- 13 TestIndexRound_ReappearanceResetsSuspicion:A 依次为缺席、存活、缺席 → 第 3 拍不转换(必须连续两拍缺席)。
- 14 TestIndexRound_DoesNotScanKeyspace:预置 5000 个无关键、存活 gate 上 10 个会话、死 gate 上 3 个会话,跑两拍。Hook 计数断言:SCAN 次数为 0,MGET 请求的键总数为 3。这条钉住本项的核心性质;在 HEAD 上旧实现必然发出 SCAN,故为红。
- 15 TestIndexRound_PrunesStaleMembers:A 的索引里有三类成员:会话已在 B 上 ONLINE、会话键不存在、成员是非数字串 → 三者都被移除;B 上的会话字节不变。
- 16 TestIndexRound_MalformedSessionPrunedNotModified:会话键里是垃圾字节,或 payload 的 PlayerId 与成员不符 → 成员被移除,会话字节原样保留,不入租约。
- 17 TestIndexRound_EtcdErrorSkipsWithoutProgress:view 持续返回错误,跑 N 拍 → 不发生任何转换;恢复后仍需连续两拍缺席才动手。
- 18 TestIndexRound_ConcurrentReplaceDuringRoundIsNoop:Hook 在处理死 gate A 的 MGET 返回后,同步执行一次 SetSession 把玩家换到 B → 转换 CAS 失败;会话保持在 B 上 ONLINE;成员在 B,不在 A;不入租约。
- 19 TestRepairPass_BackfillsLegacySessionAndSetsReady:直接写入 ONLINE 会话字节,模拟旧写者,不建索引;标记缺失、所有写者都有能力键 → repairPass 之后成员与登记集已补齐,标记被设置;随后两拍索引循环完成转换。
- 20 TestRepairPass_LegacyWriterBlocksReady:view 报告某个 locator uuid 没有能力键 → 补建后标记仍缺失;若标记原本存在,则被 indexRound 删除。
- 21 TestRepairPass_LegacyAppearingMidPassBlocksReady:Hook 在第一页之后把 view 切换为「有旧写者」→ 本次补建结束后不设标记。
- 22 TestRepairPass_SkipsNonOnlineAndChangedSessions:DISCONNECTING 会话不补;Hook 在 MGET 之后改写会话,repair 脚本返回 -1,不补。
- 23 TestRepairPass_CountsRepairedWhenReady:标记存在时人为删掉一个成员 → 审计报告 repaired=1(断言返回的统计值,不依赖指标后端)。
- 24 TestClassifyIndexedMember 表驱动:覆盖 Transition / PruneStale / PruneMissing / PruneMalformed / PruneBadMember 五种结果。
- 25 TestCollectNodeUUIDs 与 TestLegacyWriters 纯函数用例:NodeInfo 与 allocated 裸值混合;能力键差集为空或非空。另外,现有 session_reconciler_unknown_field_test.go 的两条用例必须原样通过。
- 26 TestReconcileConfNormalized 与 TestEtcYamlLoadsReconcileBlock(config_test.go);TestRegistrationOpsIncludeLeaseBoundMarkers(node_registration_test.go);可选:node_integration_test.go 在 integration tag 下断言能力键随 NewNode 出现、随 Close 消失。
## verify_commands
- 所有命令由 Codex 执行(AGENTS §10.1);Claude 不编译也不测试。工作目录为 D:\luyuan\xuanming-server-mmo-cpp\go\player_locator,使用 Git Bash;不需要清理数据,不碰任何共享环境。
- 1) gofmt -l . ;通过标准:无输出。
- 2) GOTOOLCHAIN=local go build ./... ;通过标准:exit 0。
- 3) GOTOOLCHAIN=local go vet ./... ;通过标准:exit 0。
- 4) GOTOOLCHAIN=local go test ./... -count=1 ;通过标准:全部 ok,没有 FAIL / SKIP;原有用例数量不减少。
- 5) GOTOOLCHAIN=local go test ./internal/logic/ -count=1 -v -run 'GateIndex|IndexRound|RepairPass|ClassifyIndexed|CollectNodeUUIDs|LegacyWriters|CollectLiveGate' ;通过标准:tests 中列出的 1~25 全部 PASS,TestIndexRound_DoesNotScanKeyspace 必须 PASS。另跑 go test ./internal/config/ ./internal/node/ -count=1 -v ,第 26 条 PASS。失败时保留 -v 全量输出。
- 6)(可选,用于修复前红的证据)在一次性 worktree 中执行 git worktree add <临时目录> HEAD,只放入 internal/logic/gate_session_index_test.go 运行 go test ./internal/logic/ -run GateIndex -count=1 ;期望:断言失败或编译失败。证据只记录摘要,用完 git worktree remove 删除。
- 7)(可选,需要本机 etcd)在仓库根执行 pwsh -File tools/scripts/dev_tools.ps1 -Command etcd-up ;然后在 go/player_locator 下执行 GOTOOLCHAIN=local go test -tags=integration ./internal/node/... -count=1 ;通过标准:全部 PASS。
- 8)(可选,在真实 Redis 上核对空串哨兵与 Lua)启动本地 dev 栈:docker compose -f deploy/docker-compose.yml up -d redis etcd ……;用 pwsh -File tools/scripts/go_services.ps1 -Command build 产出 exe,启动栈后跑 robot login-test。
检查:redis-cli SMEMBERS player:gate_index:gates 只有当前 gate 的 uuid;redis-cli SCARD player:gate_index:sessions:<uuid> 等于在线机器人数;redis-cli GET player:gate_index:ready 以 v1: 开头;redis-cli EXISTS "" 为 0。
然后结束 gate 进程,等待至少 3 分钟(两拍 60s + 租约 30s + LeaseMonitor)。检查:EXISTS player:gate_index:sessions:<uuid> 为 0,对应 player:session:* 已被删除,player_locator 日志出现 SessionReconciler 的 to_disconnecting 计数行。
只记录上述结果,不作任何性能结论(AGENTS §6)。
- 9) 依赖未改动的护栏:在 go/shared 下执行 GOTOOLCHAIN=local go test ./leader/... ./nodeinfo/... -count=1 ;通过标准:PASS。
## doc_updates
- 新增 docs/design/player-locator-gate-session-index.md,章节见 plan;并在 docs/README.md 设计分类下登记一行。
- docs/PROGRESS.md 末尾追加一条,写明:未编译、未测试、待 Codex 验证;对应 Java 版本「不适用」;遗留项是 K8s 暴露 player_locator /metrics。
- docs/design/capacity-model-hok-scale.md:在 §1.3 第 3 条、§2.5 对账行、§A0-4、§4.2 第 0 批加带日期的状态注;更正 §A0-4 里不存在的 SetOnline(应为 SetSession / Reconnect)。
- docs/design/shared-redis-split-by-purpose.md:§2.2 :130 补 player:gate_index:*,并把「断线后 session 带 PX」更正为无 TTL;§2.3 :164 注明索引在同一批 Lua 中;§4.6 守卫模式补 player:gate_index:*。
- docs/ops/release-checklist.md 附录 B.1 :270:原行的键名错误(player_locator:lease_zset 并不存在),改为基于指标 / 索引计数,去掉生产全键 SCAN。
- deploy/k8s/manifests/go-svc/player-locator.yaml :24-29 注释更新;新增 deploy/k8s/player-locator-alerts.yaml。
- go/player_locator/etc/player_locator.yaml 的 Reconcile 块注释。
- 可选,同批或另起:go/login/internal/logic/clientplayerlogin/disconnectlogic.go:50-51 注释写「When the Redis key TTL expires」与代码不符(会话无 TTL)。该文件不在本项改动范围,登记为文档债,不夹带进本项。
- Java 仓 PARITY.md :64「玩家在线目录」行追加「不适用」说明(只碰该文件)。
## java_parity
不适用,不需要对等改动。Java 版的在线目录是 game:presence:{player_id}:gate 是唯一写者,TTL 60s,每 20s 续期,场景绑定结束时带条件删除(PARITY.md:64,已亲自打开)。gate 崩溃后条目最迟 60s 自然过期,本身就没有「永久 ONLINE 需要全键对账」这个问题,也没有 SCAN 型对账器。本项只改服务端内部的 Redis 结构与后台循环,不触及两版共享的客户端契约(帧格式、客户端可见 proto、tip 码、配置表),按 AGENTS §12 不需要同批改。

按 §12 仍要登记:在 Java 仓 PARITY.md 该行追加「基线对账改按 gate 索引,Java 不适用」的说明;本仓 PROGRESS 条目写「Java 版本号:不适用」。

另外提醒:Java friend 的在线目录按 presence 前缀 SCAN(RedisKeys.java:151),与本仓 go/friend 在线目录的 SCAN(容量模型 §A0-4 中 friend 那一条)是同类问题,属于另外一项,不在本项范围。
## risks
- 1 漏成员的残余窗口:旧写者检测是每拍(60s)轮询 etcd。存活时间短于一拍的旧版 locator(例如 crash-loop 的旧 Pod)写出的会话,可能要等最多 24h 的日审计才能补进索引。非 player_locator 的直写者(目前全仓没有)同理。缓解手段是日审计,repaired>0 时出告警;如需更快,可把审计周期调小,代价是 SCAN 负载。
- 2 前提假设:所有共用同一会话 Redis 的 locator 都注册在 reconciler 使用的同一个 etcd(Registry.Etcd.Hosts)里,今天的 D12 加 k8s 渲染满足这一点。
- 多套环境共用一个 etcd:别的环境的旧版 locator 会被误判为旧写者,只带来持续补建的成本,不影响正确性。
- 多套环境共用一个 Redis 但 etcd 不同:别的环境的 gate 会被当成死 gate,导致误转换。这一隐患今天的实现同样存在(并非本项引入),要写进设计文档。
- 3 内存:索引成员约等于在线人数,即 2e7。SET 在 hashtable 编码下每成员约 50~80B,合计约 1~1.6GB,常驻核心 Redis(推断,需实测 MEMORY USAGE)。需要计入 G4a 与容量模型的核心内存账。
- 4 可观测性:K8s 渲染的 player_locator 配置没有 Prometheus 段,集群里实际不暴露 /metrics(既有缺口,连 safego_panic_total 也看不到)。在接线之前,新告警无法生效,leader 卡死也发现不了。接线需要改 k8s_deploy.ps1 的 player-locator heredoc(:5991-6017),该区段目前没有他人在途改动,但整份文件有 694 行未提交改动,所以建议另立一个小项处理,不并入本项。
- 5 大面积 gate 故障(例如丢失一个可用区):转换限速默认 1000/s,数百万会话需要数十分钟;随后 LeaseMonitor 发 LeaveScene 与 Kafka 的下游压力随之上升。限速参数要靠 V0 实测来定。玩家不会因此被卡住:重登时会看到旧的 ONLINE 会话,走 ReplaceLogin,照常可以进入。
- 6 补建 / 审计在目标规模下仍然是全键 SCAN:K≈6.5e9,按 200 页/s 限速约 9 小时一轮。这是单 leader、低频、限速的,但在旧写者存在、就绪标记缺失期间会连续运行;ScanPagesPerSecond 太高会拉高核心 Redis 的延迟。默认值为推断值,需 Codex 实测后校准。
- 7 与在途工作的冲突:landing-fencing-takeover 的 e / b 方案计划修改 setdisconnectinglogic.go、markofflinelogic.go(都只是本项的一两行调用改动)以及 logic_test.go(本项不改它)。谁后落地谁做文本合并,两者语义正交。player_locator.proto 本项不改。
- 8 行为变化:从「每个副本都扫一遍」变为「全局单 leader」。若 leader 选举本身长时间失败(Redis 不可达),对账会停摆;但此时会话写入同样不可用。is_leader 和 last_success 两条告警覆盖这种情况。
- 9 Lua 用空串作为「无索引」哨兵,并在 KEYS 中声明。Redis 允许空串键名,脚本有守卫,单测钉住不会落空串键;但 miniredis 与真实 Redis 7 的细微差异需要靠 verify 第 8 步在真实 Redis 上确认。
- 10 覆盖面与今天相同,没有扩大:gate 存活、但断线通知在 gate→login 或 login→locator 链路上丢失的会话,仍然不会被对账发现(旧实现同样不覆盖)。按 gate 建的索引为后续「gate 定期上报自己持有的会话集合,locator 与索引求差」创造了条件,登记为后续改进,不在本项。
- 11 集群化:本项不预置 hash tag。如果负责人选路线 C,会话 Lua 必须连同 player:leases* 一起改为按 slot 分桶,届时索引按设计文档前瞻一节改形。因为索引是派生数据,改形只需「能力键 v2 + 删就绪标记 + 补建」,不用停写。
- 12 预计改动约 26 个文件(含文档),低于 AGENTS §10.2 的 30 文件提示线;若后续把 K8s /metrics 接线也并进来,会逼近这条线。
## open_decisions
- (不阻塞本项落码)Redis 集群化路线 R / C(容量模型 §A6,早已登记为待负责人拍板,并与 db-task-pipeline-bottlenecks.md:341「不为 Redis Cluster 改 hash tag 键名」存在冲突)。本项按当前 D12 单主前提采用无 hash tag 的单实例键形。若拍板走路线 C,索引要随 player:leases* 的 slot 分桶一起改形,方案见设计文档的前瞻一节。拍板之后无需返工本项的写路径语义。

# VERDICT
## overall
规格的事实部分基本属实。我逐行核对了对账现状、TTL、写者清单、login 调用点、gate 断线链路、etcd 键形与 Java 对等，均与代码一致。全仓 grep 也没有发现规格漏掉的 player:session 写者。写路径设计是对的：在现有三段整字节 CAS Lua 里，用 expected 字节解出的旧会话和待写的新会话推导出 SREM / SADD，提交点与推导之间没有窗口。commit、rearm、claim、enqueue 这几段不碰索引的论证也成立，因为 handleLeaseExpiry 在 commit 前已在 Go 侧校验了 State==DISCONNECTING。

不能照着落码的问题集中在控制面，有三条 must。

1. 单 leader 只按 Redis 续约。etcd 侧持续失败时它不会让位，K8s 又抓不到指标，全服对账会无声停摆，比现在 2Z 个冗余对账器更脆弱。
2. SessionWriters 一出错就删就绪标记。etcd 一抖就作废补建，目标规模下稳态会退化成永久的全键 SCAN，本项「去全键 SCAN」的目标因此落空。
3. 一次性回滚或再升级时，标记会残留；legacyEpoch 只在进程内，跨 leader 守不住；审计计时每次换主都重置。三者叠在一起，漏成员可能永远补不上，规格「回滚安全」的说法不成立。

建议改法：
- 标记改成按拍续期的租约，配合 Redis 侧代次做 CAS。
- etcd 出错只跳过当拍，不当作出现了旧写者。
- 连续失败就主动让位。
- 审计时机持久化到 Redis。

should 级问题包括：存活 gate 的脏成员无人清理，在线数 gauge 偏高；每拍工作量无上限；卸任 Pod 的 gauge 残留会误报或掩盖告警；node.go 逐字副本约定被打破；K8s 配置没有镜像；补建 goroutine 的生命周期没写清；人工补救文档被削弱；测试缺少上述失败模式。

工作区方面：go/player_locator 目录确实干净；docs/PROGRESS.md、docs/README.md、k8s_deploy.ps1 正被并行会话修改，行号和未提交行数都已漂移。landing-fencing 的 e 方案将改 setdisconnectinglogic.go 和 markofflinelogic.go，与本项只有文本冲突，语义正交，规格说法属实。Java 不需要对等改动，判断成立。
## fact_checks
- [confirmed] 对账每拍 SCAN MATCH player:session:* COUNT 500、游标走到 0;命中键逐个 GET,无 MGET/pipeline;默认 60s;ticker 丢拍,一轮超时退化为背靠背连跑
   亲自打开 go/player_locator/internal/logic/session_reconciler.go:41-45(60s / 500)、:89-104(time.Ticker + safego.Run)、:121-143(Scan 循环,pattern=sessionKeyPrefix+"*")、:196(逐键 Get);keys.go:7。time.Ticker 对慢接收者丢拍,慢轮结束后下一拍立即可读,背靠背成立
- [confirmed] 周期在 dev yaml 与 K8s 渲染里都没设,实际默认 60s;-1 关闭
   亲自打开 internal/config/config.go:51-57(ReconcileIntervalSeconds optional)、etc/player_locator.yaml:33-36(Lease 只有三项)、k8s_deploy.ps1 player-locator heredoc 的 Lease 段同样只有三项。注意行号已漂移:工作区现为 k8s_deploy.ps1:6089-6117(规格写 5991-6017,HEAD 为 5396 起),该文件工作区有 797+/28- 未提交改动(规格写 694 行)
- [confirmed] 每进程都跑对账、无选主;每 zone 2 副本;所有 zone 的 locator 连同一 Redis(D12)
   亲自打开 player_locator.go:99-109;deploy/k8s/manifests/go-svc/player-locator.yaml:24-30(replicas: 2,注释「代价只有共享 Redis 每 60s 多一轮 SCAN」);k8s_deploy.ps1:1778(Scope='zone',规格写 1773)与 heredoc RedisClient.Host=redis.${InfraNamespace};docs/PROGRESS.md:5618 grep 读到
- [confirmed] 失效判定:ONLINE、GateInstanceId 非空、uuid 不在 etcd 存活集;连续两轮缺席;进程内 suspects;整字节 CAS 成 DISCONNECTING、版本+1、ZADD player:leases(deadline=now+DefaultTTLSeconds,0 时 30s);etcd 失败整轮跳过
   亲自打开 session_reconciler.go:85-87、:107-118、:210-226、:229-263(setDisconnectingScript.Run 的 ARGV data/updated/0/deadline/pid)
- [confirmed] 存活集取自 GateNodeService.rpc/ 全前缀 Range(5s);nodeinfo.Unmarshal 宽松解析,解析失败的裸 uuid 原样收进存活集
   亲自打开 session_reconciler.go:47-51、:152-186;session_reconciler_unknown_field_test.go:30、:46 两条用例
- [confirmed] gate uuid 每次进程启动随机生成;etcd 两键同租约;node_id 重启复用而 uuid 不复用,所以必须按 uuid 建索引
   亲自打开 cpp/libs/engine/core/node/system/node/node.cpp:557;etcd_manager.cpp:47-75(键形)、:100-144(两阶段、同租约)。补充:重注册(lease 丢失而进程未退)走 PutIfAbsentOrOwned,沿用同一 uuid(:104-112),即「死 gate」可能以同 uuid 复活——与旧实现同口径,非本项引入
- [confirmed] player:session:* 只有 Go player_locator 一个写者;写入点为 SetSession / Reconnect / SetDisconnecting / MarkOffline / 对账 / commitLeaseExpiry / rearm(PERSIST);claim/renew/ack/enqueue 不写会话
   亲自 grep go/、tools/、cpp/:写者只有 player_locator(其余为读者、注释或 merge_zone 集成测试夹具);亲自打开 session_cas.go 全文与 setsessionlogic.go:89、reconnectlogic.go:95、setdisconnectinglogic.go:90-99、markofflinelogic.go:80、leasemonitor.go:121-216(commit 前 Go 侧已校验 State==DISCONNECTING,畸形载荷分支只能删不可解析的会话,不可能是已索引会话)。player_locator 目录无 Kafka 消费者或其他写点(find 列全)
- [confirmed] login 调用点:FirstLogin/ReplaceLogin→SetSession,ShortReconnect→Reconnect;Disconnect→SetDisconnecting 退避重试 4 次后 CRITICAL;LeaveGame→MarkOffline
   亲自打开 entergamelogic.go:907-945(:925 SetSession、:930 Reconnect)、session_lifecycle.go:46-95、disconnectlogic.go:40-56(过时注释现位于 :50-51 附近);session_manager.go:106-175 grep 读到
- [confirmed] C++ gate 断线单向通知 login,不重试;gate 崩溃时通知不发,唯一自愈是对账
   亲自打开 cpp/nodes/gate/handler/rpc/client_message_processor.cpp:510-557(该文件 git status 为 M,他人改动)
- [confirmed] 会话键在任何路径都无 TTL(调用方都传 0、rearm PERSIST);G4a §2.2 「断线后 session 带 PX」已过时
   亲自打开 session_cas.go:47-51、:63-67、:182;setsessionlogic.go:89、reconnectlogic.go:95、setdisconnectinglogic.go:96、session_reconciler.go:248 均传 0;docs/design/shared-redis-split-by-purpose.md:130 亲自打开,确为「断线后 session 带 PX」
- [confirmed] 现有会话 Lua 已跨键(会话键 + 全局 player:leases*),依赖单实例;同一 Lua 维护索引在 D12 单主下原子
   亲自打开 session_cas.go:227-235、keys.go:5-13;另 claimExpiredLeasesScript 用 ARGV 拼会话键名(session_cas.go:111),本来就不兼容集群;G4a :164 亲自打开
- [confirmed] 会话读者只按 player:session:<id> 读,本项对其零影响(friend/guild/match/mail/merge_zone/login 补齐 Lua 只读)
   grep 读到 friend session_reader.go:47、guild online_status_resolver.go:17、match playercontract.go:44、mail notify.go:57、merge_zone preflight.go:582-588;亲自打开 player_class_backfill.go:89-95、:179-191(EXISTS 只读)。另核:所有 SCAN 模式(player:*:location、player:zone:*)都不会匹配 player:gate_index:*
- [unverifiable] 2000 万在线下 GET≈1.3e8~3.0e8/s、SCAN≈8.7e7~1.9e8/s;比核心 Redis 其余全部流量(约 1.7e6/s)高两个数量级;单副本一轮约数小时
   亲自打开 capacity-model §2.5(:266,GET=2Z·N/60,S3 为 3,333万~7,430万,反推 2Z=200~446)与全局单点交接 :181、:286。算术与「Z∝PCU 翻倍」假设一致,但 Z 翻倍是推断,§7.2 未给 Z。「约 1.7e6/s」是交接 :178 的 1000 万口径,不是 2000 万;「单副本一轮约数小时」与交接 E2(:286)「Z=1 时单副本 22~108 分钟」不一致(多副本争用下更长才说得通)
- [confirmed] 可复用 shared/leader(SET NX + 属主续期 + 自我降级,trade/player_inbox/scene_manager 在用)、go-zero core/metric、nodeinfo.Unmarshal;go.mod 中 prometheus 与 x/time 仅 indirect
   亲自打开 go/shared/leader/leader.go:1-200、store_goredis.go:19;grep 读到使用方 guild/login/player_inbox/scene_manager/trade;player_locator/go.mod:58-61、:86 indirect。注意:leader 的心跳是独立 goroutine,只看 Redis,whileLeader 卡住或反复失败时领导权不会自动交出(leader.go Run)
- [confirmed] player_locator etcd 注册由同租约 Txn 一次写 allocated 与 rpc 两键(tryClaimNodeID 与 reRegister 第 1 步);NodeUuid 启动时 uuid.New
   亲自打开 go/player_locator/internal/node/node.go:166-187、:258-265、:85。规格漏看文件头 :3-8:该 allocator 是 guild/friend/player_locator/scene_manager 的逐字副本,修改须同步(client_rpc_router registry.go:6-7 记为六处副本)
- [confirmed] K8s 渲染的 player_locator 配置没有 Prometheus 段,集群内不暴露 /metrics
   亲自 grep:k8s_deploy.ps1 与 player-locator.yaml 无 9190、heredoc 无 Prometheus 段;对照 player-inbox 需 heredoc MetricsListenAddr + manifest prometheus.io 注解 + containerPort(player-inbox.yaml:66-68、:105-107)才可抓取
- [confirmed] Java 版无对应机制:game:presence:{player_id} 以 gate 为唯一写者、TTL 60s、20s 续期
   亲自打开 D:/luyuan/xuanming-server-mmo/PARITY.md:64;RedisKeys.java:80-84、:151-153 grep 读到(PARITY.md 工作区为 M,mtime 2026-10-16 04:20)
- [refuted] 工作区状态:go/player_locator 全目录干净;docs/README.md 他人改动 12 行;PROGRESS +228;k8s_deploy.ps1 694 行
   git status:go/player_locator 确实干净(setsessionlogic/reconnectlogic/logic_test mtime 10-14 23:06,其余 10-12 03:53)。但数字已过时:docs/README.md 现 +13/-2(mtime 10-16 04:19)、PROGRESS.md 现 +329(mtime 04:23,仍在被追加)、k8s_deploy.ps1 现 797+/28-(mtime 04:18)。并行会话正在改,落码时须按内容重定位
- [confirmed] capacity-model §A0-4 写的 SetOnline 不存在,应为 SetSession / Reconnect
   亲自打开 capacity-model-hok-scale.md:414;grep go/player_locator 与 proto/player_locator 无 SetOnline;playerlocatorserver.go 只注册 SetLocation/GetLocation/MarkOffline/SetSession/GetSession/SetDisconnecting/Reconnect
## plan_problems
- [must] 单 leader 失去冗余且无法被发现、无法自动交出。旧实现是 2Z 个独立对账器,坏一个无所谓。新实现只有 1 个,且领导权只看 Redis:shared/leader 的心跳是独立 goroutine。leader 所在 Pod 若到 etcd 的路径坏了,而其他副本正常(DNS 缓存、网络策略、单边分区),indexRound 第 c 步会每拍「整拍返回」,但领导权一直续着,全服对账停摆,别的副本也接不了手。规格风险 8 只考虑了「选举本身失败」。它新增的 Stalled / NoLeader 告警在 K8s 根本抓不到(player_locator 没有 /metrics,规格风险 4 自己也承认),而这正是全局单点交接 :138 明确要求防的「leader 卡死而 is_leader 之和仍为 1」。
   修法: 在 run(lctx) 里加自我让位:连续 K 拍失败(建议 3 拍),或距上次成功超过 3×IntervalSeconds,就返回 whileLeader,让 elector 释放锁;本进程下一次竞选前多等一个 TTL,让健康副本先抢到。每拍失败打 ERROR,带失败阶段 etcd/redis。补测:fake view 连续返回 K 次错误后 run 返回,且 leader 键被释放(miniredis)。同时把「K8s 暴露 player_locator /metrics」列为本项告警生效的前置。具体改动:heredoc 加 Prometheus 段,manifest 加 prometheus.io 注解与 containerPort,照 player-inbox 加 k8s_deploy_contract 用例。要么同批做,要么在设计文档与 PROGRESS 写成阻塞项并指定归属,不能只写「另立小项」。
- [must] SessionWriters 一出错就删就绪标记并 legacyEpoch++(plan session_reconciler.go 第四节 a)。这样每次 etcd 抖动都会作废正在进行的补建,并触发一次全键补建扫描。按规格风险 6,K≈6.5e9、200 页/s,一轮约 9 小时。只要 etcd 抖动比「一轮补建」更频繁(目标规模下合理),标记就永远设不上,稳态退化为永久的全键 SCAN(200 SCAN/s + MGET + SISMEMBER),IndexNotReady 常亮。规格「稳态每拍约 3 条命令、日审计约 300/s」的结论因此不成立,本项「去全键 SCAN」的目标也落空。fail-closed 的对象应是「是否转换会话」,不是「出错即认定有旧写者」。
   修法: SessionWriters 出错时本拍只跳过写者检查,不删标记、不动 epoch,打 ERROR 并计 reconcile_round_total{result=etcd_error}。只有在一次成功的 Range 里确实看到「已注册但无能力键」的 uuid,才删标记并递增代次。残余风险是旧写者恰好只在 etcd 故障期间存活,写进风险 1,由审计兜底。补测:view.SessionWriters 连续出错 N 拍,标记保持不变,也不启动补建。
- [must] 「回滚 / 再升级安全」的说法不成立。就绪标记只有在新版 leader 亲眼看到旧写者时才会被删。若一次性回滚(kubectl rollout undo 全 zone,或回滚快到新 leader 来不及跑一拍),所有新 Pod 都在观察到旧写者之前退出,标记就留在 Redis 里。回滚期间旧写者写的会话缺成员,换 gate 还会留下脏成员。再升级时新 leader 看到标记存在,只走日审计;而日审计计时是进程内的,每次换主都会重置(见 should 第 1 条),可能永远不跑。之后这些 gate 一死,会话就永久 ONLINE,正是对账要兜的缺口。另外,legacyEpoch 只在进程内:两个 leader 短暂重叠时,前任可以在新 leader DEL 之后再 SET 标记,跨进程的竞态守不住。
   修法: 把就绪标记改成租约:SET player:gate_index:ready v1:<unix> PX 3×IntervalSeconds,只由「本拍成功 Range 且零旧写者」的 leader 续期。只要连续若干拍没有新版 leader,标记就自然过期,下任 leader 自动补建。另用一个 Redis 侧代次键 player:gate_index:gen:看到旧写者就 INCR;补建开始时读 gen,结束时用 Lua 做「gen 未变才 SET 标记」,替代进程内的 legacyEpoch。补测:不续期时标记过期,下任 leader 启动补建;补建期间 gen 变化,结束时不设标记。设计文档第 6 节的回滚描述同步改写。
- [should] 审计周期是进程内计时器(「标记存在:每 AuditIntervalSeconds 跑一次」)。每次换主都会重置 24h,发版频率高于每天时审计可能永远不跑。风险 1 与 RepairedWhileReady 告警都以「日审计」为兜底,这个前提随之落空。
   修法: 审计时机以 Redis 里持久化的时间为准:复用标记值 v1:<unix>,或单设 player:gate_index:last_audit。当 now - last ≥ AuditIntervalSeconds 时才跑,跑完更新时间戳。补测:注入的 now 跨过阈值后才触发,换一个新 sessionReconciler 实例不会重置。
- [should] 存活 gate 上的脏成员没有任何清理路径。审计只补不删,processDeadGate 只处理死 gate。脏成员来源有三:升级窗口里的旧写者换 gate、deploy-staging.sh:101 与 stress-linux-tier.sh:55 只删 player:session:*、局部清库。gate 可能连续存活数周,索引常驻内存一直膨胀;gate_index_members 也会系统性偏高。规格却把 release-checklist B.1 的「在线数」改为看这个 gauge,口径是错的。
   修法: 审计时对存活 gate 的索引也做 SSCAN + MGET,用现成的字节守卫 prune 脚本删掉不符的成员。若不做,就在指标 Help 和 B.1 写明它是上界,不能当在线数。staging / 压测清库清单补上 'player:gate_index:*'(两行脚本,无冲突)。补测:存活 gate 索引里预置三类脏成员,审计后被删,真实成员不动。
- [should] processDeadGate 在一拍之内要处理完。按 1000/s 限速,丢一个可用区(约 1/3、6.7e6 会话)要处理约 1.9 小时,不是规格风险 5 写的「数十分钟」。这期间新死的 gate 不会被发现,etcd 快照陈旧数小时,last_success 不推进:Stalled 告警要么误报,要么把真正的停摆淹掉。
   修法: 给每拍设预算(时间或成员数),超出就留到下一拍继续;已确认死亡的 gate 在 suspects 里保持 ≥2,不必重新去抖。last_success 按「完成一拍」推进,另加进度 gauge reconcile_dead_gate_members(死 gate 索引 SCARD 之和)。补测:单个死 gate 成员数超过一拍预算,分两拍处理完,第二个死 gate 在第一拍之后仍能被识别。
- [should] 「只由 leader 产出」的 gauge 在卸任 Pod 上停在最后的值。规则用 max() 聚合后:前任残留的 gate_index_ready=1 会掩盖现任的 0;残留的 dead_gates>0 或 legacy_writers>0 会误报。NoLeader 用 sum(...)==0,在没有序列时不触发。RepairedWhileReady 用 increase(),第一次从无到 1 会漏掉;仓库约定见 deploy/k8s/player-inbox-alerts.yaml 文件头,稀有 label 要预置为 0。
   修法: 在 OnStateChange(false) 与任期结束时把只由 leader 产出的 gauge 清零。告警表达式乘上 is_leader 过滤,例如 max(gate_index_ready * on(pod) group_left reconcile_is_leader)。NoLeader / Stalled 加 absent()。reconcile_sessions_total 的各 action 在启动时预置为 0。
- [should] internal/node/node.go 头注释 :3-8 写明:该 allocator 在 guild / friend / player_locator / scene_manager 中逐字相同,修改须同步另外三处(client_rpc_router registry.go:6-7 记为六处,计划收进 go/shared)。规格只改 player_locator 的 tryClaimNodeID / reRegister,抽出 registrationOps 加了 marker,没提这条约定,会悄悄打破副本一致性,也给后续收口埋坑。
   修法: 二选一。一是 allocator 主体保持逐字,只加一个可选的 extraOps 钩子参数(默认空),并在所有副本头注释里登记「player_locator 传入能力键」。二是至少在 player_locator 头注释写明有意分叉及原因,并在其余副本头注释同步一句,指向这里。不要默默改。
- [should] 配置镜像缺失。dev yaml 显式写了 Reconcile 的四个值,K8s heredoc 却不写、全靠代码默认值。仓库惯例是 Get-AuthoritativeScalar 加契约测试(k8s_deploy_contract.tests.ps1:178-188 已对 player-locator 的 LeaseTTL / DefaultTTLSeconds 这样做)。有人照 yaml 调参,K8s 不会跟着变。另外 leader 是全局的,各 zone 配置一旦不同,行为就取决于哪个 Pod 当选。
   修法: 二选一。一是 dev yaml 里只写注释、不写值,事实源留在代码默认值。二是在 heredoc 镜像这四项,并在契约用例的 key 列表里追加。设计文档写明 Reconcile 各项必须全服一致。
- [should] 补建 goroutine 的生命周期不清。run(lctx) 用 safego.Go 起了 repairLoop,但没说 lctx 取消后要等它退出。同一进程再次当选时,可能有两个补建循环并发;前任在途的 SET ready 可能落在新任的 DEL 之后。sessionReconciler 结构里只有一个 pace 字段,却要承载两种限速(ScanPagesPerSecond 与 MaxTransitionsPerSecond)。
   修法: run 用 WaitGroup 等补建 goroutine 退出后再返回。结构里拆成 transitionPacer 与 scanPacer 两个字段。补测:lctx 取消后 run 返回时补建循环已停止(用 Hook 计数,取消后不再有 SCAN)。
- [should] 人工补救文档被削弱。login 的 session_lifecycle.go:62-67 注释和 :93 的 CRITICAL 日志都指向「release-checklist #B-1 SCAN 巡检」,处理的是「gate 存活但 SetDisconnecting 重试耗尽」的永久 ONLINE。规格风险 10 承认新旧实现都不覆盖这种情况,却又把 B.1 改成去掉全键 SCAN、只看指标,值班手里就没有任何补救步骤了。
   修法: 在 release-checklist 附录 B 或 F 补一条定向补救步骤:从 CRITICAL 日志取 player / session id,对该玩家 GetSession,再调 SetDisconnecting(grpcurl 示例)。不再建议生产全键 SCAN。把 session_lifecycle.go 的注释与日志文案登记为文档债,和 disconnectlogic.go 那条并列,不夹带改动。
- [should] 测试没覆盖新引入的失败模式。tests 1-26 只验证主路径,缺少:leader 连续失败后让位、SessionWriters 出错不动标记、标记租约过期与代次 CAS、审计时机持久化、存活 gate 脏成员清理、每拍预算、成员数超过 COUNT 的多页 SSCAN,以及「死 gate(实为分区存活)索引释放与并发 SetSession 的 SADD 竞态」。也没有对不变量 I1 的序列测试。
   修法: 按上面的 must / should 逐条补面向行为的用例,全部用注入的 now 与 no-op pacer。另加一条确定性序列测试:用固定种子生成 SetSession / Reconnect / SetDisconnecting / MarkOffline / 转换在多个 gate 间的操作序列,每步之后断言「ONLINE 且 gate 非空 ⇔ 是成员」。
- [nit] 容量叙述有几处口径不一。「核心 Redis 其余全部流量约 1.7e6/s」是 1000 万口径(交接 :178);「单副本一轮约需数小时」与交接 E2 的 22~108 分钟(Z=1)不一致;大面积故障「数十分钟」低估了(见上面每拍预算那条);Z 随 PCU 翻倍是推断。
   修法: 在设计文档第 7 节逐项标 [I] / [D],改正数字,并注明 Z∝PCU 的假设出处。
- [nit] 领导范围表述有误。所谓「全局单 leader」实际是「每个会话 Redis + etcd 域一个」,在全局单点交接 :202 的多 cell 前景下,会自然变成每 cell 一个,应写清楚。交接 :181 建议监听 etcd DELETE 事件驱动,规格改用 60s 轮询,没有说明取舍。
   修法: 设计文档第 4 节补一段:leader 作用域等于会话 Redis 域;选轮询的理由(去抖语义与旧实现一致、实现简单、watch 断流重建复杂),以及检测延迟 60~180s 与旧实现相同。
- [nit] 引用的行号与工作区状态已漂移:k8s_deploy.ps1 heredoc 现约在 :6089-6117,Scope 在 :1778;README、PROGRESS、k8s_deploy 的未提交行数都已变化(见 fact_checks)。
   修法: 落码时按内容重新定位,并在交付说明里写明是按哪个时刻的工作区核对的。
- [nit] 对账器另建了一个 etcd 客户端。仓库已有复用 node 客户端的约定(node.go:304-318 注释)。空串 KEYS 作哨兵可以工作,但可读性一般,而且 GateInstanceId 不经校验就直接拼进键名。
   修法: 可改为传入 n.EtcdClient()(生命周期归 Node)。可选:写索引前限制 GateInstanceId 的长度与字符集;不合法时不建索引并打 ERROR(会话仍照写,与旧对账口径一致)。
## missed
- go/login/internal/logic/clientplayerlogin/session_lifecycle.go:62-67 注释与 :93 的 CRITICAL 日志文案仍指向「release-checklist #B-1 SCAN 巡检」;B.1 改写后要给出定向补救步骤,并把这两处登记为文档债
- tools/scripts/deploy-staging.sh:101、tools/scripts/stress-linux-tier.sh:55 的清库模式清单只删 player:session:*;存活 gate 上的脏成员没有清理路径,应补 'player:gate_index:*'
- K8s 暴露 player_locator /metrics 的接线:k8s_deploy.ps1 player-locator heredoc 加 Prometheus 段、manifests/go-svc/player-locator.yaml 加 prometheus.io 注解与 containerPort、k8s_deploy_contract.tests.ps1 加用例。这是新告警与「防 leader 卡死」的前置,规格只登记为「另立小项」
- Reconcile 配置的 K8s 镜像与契约测试(照 k8s_deploy_contract.tests.ps1:178-188 的 LeaseTTL 用例),以及「全服必须一致」的说明
- go/guild/internal/node/node.go、go/friend/internal/node/node.go、go/scene_manager/internal/noderegistry/registry.go、go/client_rpc_router/internal/noderegistry/registry.go 的「逐字副本」头注释(player_locator/internal/node/node.go:3-8)
- tools/scripts/k8s_zone_rollback.ps1:653-661 的 Get-SharedRedisGuardPatterns 是 FLUSHDB 守卫的代码事实源;规格只改 G4a 文档 §4.6,应注明 G4a 守卫落码时要带上 player:gate_index:*
- deploy/k8s/README.md 的告警安装说明(其他 *-alerts.yaml 都登记了 kubectl apply 命令与抓取前提)
- leader 键 player_locator:session_reconciler:leader 与 player:gate_index:ready / gen 键,应登记进 G4a 文档的键族表
- docs/design/no-single-node-horizontal-scaling-20261001.md §1 单点盘点表:全局单点交接 :261 已指出漏了 player_locator 对账,本项又新增一个 leader 单例,应登记
- docs/handoff/handoff-launch-readiness-and-d1-20261013.md(§4 第 7 项)与全局单点交接文首的状态注:本项落码后应加带日期的状态行(AGENTS §5)
- OnStateChange(false) 时清零只由 leader 产出的 gauge;reconcile_sessions_total 各 action 在启动时预置为 0(仓库告警约定)
- run() 等待补建 goroutine 退出的生命周期,以及两个独立的限速器
- 存活 gate 的索引脏成员清理(审计只补不删)
