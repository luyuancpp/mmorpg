<!-- 只读分析代理产出（2026-10-16，接手会话工作流 wf_f7177272-747），原样保存供落码时查证；结论未经运行验证。 -->

**item**: 进场链余项 e(可选加固):ExitGame 带登录代次 / 断线上报带 gate 实例号;附带评估 scene-enter-session-fence.md §9 残余 7
**current_behavior**: 【状态说明】下面的行号取自读文件那一刻的工作区。gate 的 client_message_processor.cpp 正被另一会话改动(未提交,把路由传输收成只走 muduo_grpc),和断线语义无关,但行号会漂移。player_lifecycle.cpp 也有未提交改动(存盘序号与围栏字段),同样无关。其余文件与 HEAD 一致。

一、gate 断线回调(cpp/nodes/gate/handler/rpc/client_message_processor.cpp,工作区)
1. 入口是 :442 HandleConnectionDisconnection。:458 遇到未建会话的连接直接早退。之后在 erase 之前取出 playerId、loginStarted 等字段。
2. 代发 ExitGame 给 scene,只在会话已提交 scene 节点指向时发(:480 HasNodeEntity(SceneNodeService)):
   - 组 ProcessClientPlayerMessageRequest{session_id, player_id(已绑定才填,:500), message_id=ScenePlayerExitGame(:502)};
   - 消息体 GameNodeExitGameRequest 是空的,proto/scene/player_lifecycle.proto:15-17;
   - :504 用 RpcClient::SendRequest 单向发出。未连接时返回 false 直接丢,不缓存补发。所以同一 gate 进程、同一条 gate→scene TCP 上是 FIFO,没有跨连接的迟到重放。
3. 给 login 发断线通知(:523-558):
   - BuildSessionDetails(:162-175)填 gate_node_id 与 gate_instance_id = node_uuid(:169);:533 再补 session_incarnation;
   - 路由模式(:535)合成 ClientRequest 交路由服,直连模式(:555)调 SendClientPlayerLoginDisconnect;
   - SessionDetails 放在 metadata x-session-detail-bin 里。所以 **gate 已经把实例号带给了 login**,只是 login 没往下传。
4. 随后 OnSessionClosing,再 sessions.erase。此后这个进程不会再为该会话发任何帧。
5. session_id 的构成:[node_id 15 位 | seq 17 位]。见 network_utils.h:12、snow_flake.h:26(kNodeBits=17)、node_id_generator.h:47-53。
   - seq 会回绕,只对活跃会话去重(client_message_processor.cpp:700-709);
   - gate 以同一 node_id 重启后,seq 从 1 重新发;
   - gate_node_id 只在 zone 内唯一(cross-zone-scene-travel.md:181)。
6. 冲突收尾:gate 发现 node_id 已被别的 uuid 持有时,走 SetOnConflictShutdown → disconnectAllClients(gate main.cpp:290-302、:337)。这会对剩下的会话逐个 forceClose,触发上面第 2、3 步。此时新实例已经在同号上服务,收尾最长 15s(node.cpp:873-905)。

二、scene 处理 ExitGame(cpp/nodes/scene/handler/rpc/scene_handler.cpp,HEAD)
入口是 :290 ProcessClientPlayerMessage。原会话所说的"三道守卫":
- 守卫 1「会话映射」(:324-329):SessionMap().find(session_id),找不到就 "session id not found" 返回。
- 守卫 2「player_id 围栏」(:331-335 → CheckRoutedPlayerIdentity :85-112):请求带的 player_id 必须等于映射出的玩家。0 按老 gate 放行,只记一次 INFO(:89-101,R13 兼容位)。
- 守卫 3「快照会话相等」(:357-370):PlayerSessionSnapshotComp.gate_session_id 必须等于请求会话,否则擦掉这条(属于本玩家的)映射(:368)并丢弃。
另有两处相关逻辑:
- 载入中分支(:337-354):实体还没建时,ExitGame 只擦请求会话自己的映射(:347)。
- 守卫通过后进 ScenePlayerHandler::ExitGame(cpp/nodes/scene/handler/rpc/player/player_lifecycle_handler.cpp:25-43),调 HandleExitGameNode(kClientDisconnect)(player_lifecycle.cpp:2964)。:3063 抄下 sessionAtExit。存盘落地后,:2206 IsSupersedingSession(player_exit_intent.h:135-137)只在"当前会话号 ≠ 退出时会话号"时才算被取代。
栅栏侧:
- HandleEnterGameNode 第 0 步 JudgeEnter(player_lifecycle.cpp:2459-2482),第 2 步 RemovePlayerSessionSilently(:2510);
- EnterScene 绑定:复核在 :2619-2631,写映射在 :2702,RecordLoginIdentity 在 :2443-2457;
- EraseSessionMappingIfOwned 在 :2424-2441;
- 判定表 enter_fence.h:140-168。同会话号一律 kAdmitSameSession(:147-149)。

三、login → player_locator(Go,HEAD;只有 go/login/internal/config 有一处无关的未提交改动)
- 断线:
  - clientplayerloginserver.go:46 → disconnectlogic.go:26-57:先 sessionfence.Close(键是 gate_instance_id + incarnation,很精确),再按 sessionPlayersToSettle 逐个调 markPlayerSessionDisconnecting(:52-54);
  - session_lifecycle.go:56-93 带退避重试 4 次;
  - sessionmanager.SetSessionDisconnecting(session_manager.go:116-126)只发 {player_id, session_id, ttl},**不带实例号**。
- 绑定失败补偿:entergamelogic.go:886-894 compensateFenceBind,用 WithoutCancel 加 10s 预算,调同一个函数。迟到窗口可达约 10s。
- 登出:leavegamelogic.go:26-55 → deletePlayerSession(session_lifecycle.go:46-54)→ DeleteSession(session_manager.go:157-175)。它现读会话、只比 session_id(:162),版本号取的是刚读到的值(:169),所以等效于只比 session_id。
- player_locator 的 SetDisconnecting(setdisconnectinglogic.go:31-112):
  - 只比 session_id(:49-53);
  - ONLINE → DISCONNECTING,版本号 +1,整字节 CAS 并 ZADD 租约(session_cas.go:59-73)。
- player_locator 的 MarkOffline(markofflinelogic.go:39-100):比 session_id + version(:65),整字节 CAS 删会话(:79),同步调 LeaveScene(:99)。
- 租约到期:leasemonitor.go:121-216,发给 gate 时带 target_instance_id = 会话记录里的实例(:251)。gate 端 PlayerLeaseExpiredEventHandler(gate_event_handler.cpp:138 起)再核 session 与 player(:166-172)后 forceClose(:179)。这条链靠实例号是精确的。
- 对账:session_reconciler.go:189-262 按 gate_instance_id 是否存活判定,也是精确的。
- 死契约:gate_event.proto:36-41 的 PlayerDisconnectedEvent 虽有 gate_instance_id 字段,但没有生产者,gate 处理器是空的(gate_event_handler.cpp:133-137)。
- 会话键不带 zone,存放在全服共享的 Redis(cross-zone-scene-travel.md:780)。

小结:目前精确的只有 login 会话栅栏、LeaseExpired、对账这三处,它们都带实例号。SetDisconnecting、MarkOffline、scene ExitGame 只靠 session_id(加 player_id)。
**problem_confirmed**: True
**problem_analysis**: 结论先行:「三道守卫在栅栏生效后已经精确」只在一个前提下成立:**同一个 session_id 不会同时出现在两个仍能发消息的 gate 实例上,且指向同一名玩家**。这个前提可能被打破,所以问题成立,但触发概率很低,后果只伤活性,不会造成数据回档或丢失(退出前会先存盘)。下面按任务点名的场景逐一构造反例。

1. 跨 gate 快速重登,旧 gate 的断线上报迟到:守卫成立。
   - P 在 gateA(node 3)上会话 3:5、代次 g1;设备 B 在 gateB(node 4)上会话 4:9、代次 g2。
   - login 判 Replace,踢 3:5,SetSession(4:9)。
   - scene 收到 enter(4:9, g2):判 kAdmitNewer;第 2 步擦掉 3:5→P 的映射;EnterScene 绑定 4:9。
   - gateA 迟到的 ExitGame(3:5) 在守卫 1 就结束了(cross_zone_test 用例 StaleEnterAfterTakeoverIsRejected 断言的正是这一点)。
   - 如果 ExitGame 先到:实体进入退出;B 入场时 EnterScene 第 0 步取消退出(:2648);若实体已销毁则从 Redis 重载。
   - gateA 迟到的 Disconnect(P, 3:5):SetDisconnecting 发现会话号不符,忽略(:49-53)。
   - 不同 gate 的 node 段不同,会话号不可能相撞。没有反例。

2. 同一 gate、同一连接:守卫成立。
   ExitGame 只在 TCP 断开时代发,发完会话立即 erase,同一连接上 FIFO。即使中途重登、换角色,gate 用的也是当前角色的 player_id 和会话。没有反例。

3. gate 重启复用 node_id,别的玩家 Q 拿到 P 的旧号:守卫成立。
   - 守卫 2 拦下 Q 的帧落到 P 僵尸实体上的情况;
   - EraseSessionMappingIfOwned(§5.6)不会擦掉 Q 的映射;
   - Q 自己的 SetDisconnecting 操作的是 Q 的记录。

4. 真正的反例:同一名玩家在两个实例上拿到同号,而旧实例仍能发消息。
   前提三条同时成立:
   ① 新旧实例同号。旧实例 I1 和新实例 I2 共用 node 3,而且 I1 还能发帧。可能的来源:
     - 进程冻结或分区后恢复的僵尸 gate;
     - 失租后 node_id 被 I2 抢走,I1 进入冲突收尾,disconnectAllClients 在 I2 已经服务的同时逐个代发 ExitGame 和 Disconnect;
     - 跨 zone:两个 zone 各有一台 node 3,而会话记录放在全服共享的 Redis。
   ② P 在 I2(或另一 zone 的 node 3)上碰巧拿到与旧会话相同的 17 位 seq(I2 的 seq 从 1 开始)。
   ③ P 在 I2 上的新登录已提交,I1 的上报才到。

   (a) player_locator 链(三条里最现实的一条,不依赖 scene 侧的连接处理):
   - I1 迟到的 Disconnect(P, 3:k) 到 login,调 SetDisconnecting(P, 3:k);
   - 记录里是 {3:k, I2, ONLINE},会话号相等,于是标成 DISCONNECTING 并挂租约;
   - 30s 后 LeaseMonitor 向实例 I2 发 LeaseExpired;I2 上 session 3:k、玩家 P 都对得上,forceClose P 的活连接;同时 LeaveScene 删掉 location。
   - 结果:活玩家在 30s 后被踢。
   - 迟到的 LeaveGame 走 DeleteSession/MarkOffline,后果更重:立即删掉新会话并 LeaveScene。
   - 迟到窗口的来源:login 4 次重试约 1.4s 加上 RPC 时间;compensateFenceBind 最长 10s;冲突收尾最长 15s;跨 zone 重定向是"先连新、后关旧"(cross-zone-scene-travel.md:20、:431),旧 zone 的断线上报与新 zone 的 Replace/Reconnect 天然并发。

   (b) scene 链:
   - I1 的 ExitGame(3:k, P) 依次通过守卫 1(映射 3:k→P)、守卫 2(P==P)、守卫 3(快照 3:k==3:k),进入 HandleExitGameNode;
   - 落地时 IsSupersedingSession(3:k, 3:k) 为假,实体被销毁,映射被擦。
   - 结果:gate I2 与 locator 都认为 P 在线,scene 上却没有实体,客户端的消息全部 "session id not found",只能重登。这是栅栏原本要修的那类后果,只是触发概率极低。
   - 前提是 scene 仍从 I1 的旧 TCP 读到这些帧。scene 是否在 I1 节点被摘除时主动关闭入站连接,**未核实**。
   - 跨 zone 的同号在 scene 链上不成立:旧 zone 的 ExitGame 落在源节点,那本来就是该退出的源实体。

   (c) 同一碰撞类的其它面:I1 迟到的 PlayerEnterGameNode 在不变量 2 下按同会话放行;I1 上仍连着的旧客户端的消息也会通过三道守卫。所以单在 ExitGame 上补登录代次,只堵住了这一类问题的一个口子。

   (d) 顺带发现(属于 a 项):login 的 kickReplacedSession 在 existing.SessionID == currentSessionID 时直接跳过踢线(entergamelogic.go:994),不比实例号。同号碰撞时旧连接不会被踢,会一直挂着,于是 (a)(b)(c) 的迟到窗口变成无界。

5. 量级估计(未实测):
   - 条件 ② 对单名玩家约为 1/131072。一次同号重启时,所有回连玩家里出现"同号又同人"的期望个数大约是 O(1),随后还要叠加 ③ 的迟到;
   - 跨 zone 的同号概率约为 1/(每 zone 的 gate 数) × 1/131072。
   - 结论:罕见,但不是"实际不会发生"。原会话的判断"旧进程的迟到帧在新进程的入场之后到达,实际不会发生"对**崩溃式重启**成立,因为崩溃的进程发不出帧;对冲突收尾、僵尸恢复、跨 zone 同号不成立。

6. §9 残余 7(邻域,已核实):
   - 外来角色的路由到达时,ApplyRouteIdentity(scene_route_helper.h:165-179)无条件覆盖会话上的 homeZoneId 与 ownerEpoch;
   - OnRouteDecision(scene_entry_dispatch.cpp:349-360)用外来路由整体替换会话的 lastSceneRoute 和**当前角色在途的欠账**,紧接着 AttemptPendingSceneEntry 返回 kPlayerMismatch,欠账被 reset(:296-312)。
   - 实际危害:当前角色在途的进场欠账被静默丢掉。不推 kEnterSceneFailed,也不踢线,客户端只能等超时。
   - 被污染的 epoch 目前是潜伏状态:当前角色在自己的下一条路由覆盖它之前,没有任何转发路径会读到它。原因是欠账已经被清掉,EntryForLateLoginBinding(:296-313)又会因为 lastSceneRoute 属于别人而返回 nullopt。
   - 触发条件:Bind 是同步生产的(servicecontext.go:595-607),而 gate-cmd 按 gate 节点分区,同一 gate 上全序。所以外来路由只可能是旧角色迟到的路由,例如旧角色后台链在 Recheck 之后才发出的 EnterScene,或旧角色由 scene 自发的换图。
   - 现成的观测:[SceneEntry] 汇总里的 player_changed 计数(scene_entry_dispatch.cpp:304、:339)。
## design_options
  - [0]
    **name**: 方案0:不做,只登记残余
    **description**: 在 scene-enter-session-fence.md §9 加一条残余,写明「同一玩家的同号会话跨 gate 实例(重启、冲突收尾、僵尸、跨 zone 同号)」的触发条件与后果,并给出根治方向(方案C)。不改代码。
    **pros**: 零改动零风险。与原会话「可选」的定性一致。
    **cons**: player_locator 链上那个可以复现的活性缺陷(30s 后误踢活玩家;LeaveGame 迟到时立即删会话并 LeaveScene)继续存在。而且它的修法本来就很便宜。
  - [1]
    **name**: 方案A(推荐):只做「断线 / 登出上报带 gate 实例号」,只改 Go
    **description**: proto 改动:SetDisconnectingRequest 加 string gate_instance_id = 4;MarkOfflineRequest 加 string expected_gate_instance_id = 4。
login 改动:把 SessionDetails.gate_instance_id 传下去,涉及 Disconnect、LeaveGame,以及 compensateFenceBind 用的 state.gateInstanceID;DeleteSession 在本地先比一次实例号。
player_locator 改动:在原有 session_id 比对之后加一条:请求与记录两边都非空且不相等,判为迟到的旧实例上报,忽略(Info 日志)。整字节 CAS 不变,所以比对与提交之间没有窗口。
gate、scene 都不改:gate 早已在 SessionDetails 里带了实例号(client_message_processor.cpp:169)。
    **pros**: 堵住 player_locator 这一侧碰撞类的全部入口(断线、登出、绑定补偿),包括跨 zone 同号的情形。规则与 LeaseExpired 的 target_instance_id 和对账口径一致(DRY)。两个字段都可选,空值就退回旧行为,滚动升级与回退都安全。只改 Go,不动 C++。
    **cons**: 多一处 proto 改动,需要重新生成。scene 侧的 ExitGame 碰撞面仍然开着(只伤活性)。同一 gate 进程内 17 位 seq 回绕(R14)挡不住,但迟到窗口是秒级,而回绕要攒满 13 万次连接,实际不可达。
  - [2]
    **name**: 方案B:方案A + ExitGame 带登录代次
    **description**: proto:GameNodeExitGameRequest 加 uint64 login_generation = 1(该消息一直是空的)。
gate 在断线回调里取会话的 SessionInfo.loginGeneration 盖章。
scene 在 ExitGame 守护段里比较:请求代次非 0、实体 PlayerLoginIdentityComp.loginGeneration 非 0、且请求严格小于实体代次时,计数并打 WARN,直接返回;不擦映射,不挂也不摘退出标记。
    **pros**: 能挡住反例 (b):冲突收尾或僵尸 gate 代发的 ExitGame 销毁同号同人的活实体。对真正的同一会话不会误拒:同一 gate 进程内,会话代次只经 Merge 取 max,而 scene 身份代次 ≤ gate 当时盖的章,所以请求代次恒 ≥ 实体代次。
    **cons**: 不成体系:同一碰撞类里迟到的 PlayerEnterGameNode 和旧客户端的消息它都挡不住(不变量 2 下同号一律放行)。等于只在 ExitGame 上把「同号但代次更低」当成别的会话,与入场栅栏的口径不一致。要改 gate、scene 和 proto,收益只是覆盖一个多重故障下的活性问题。
  - [3]
    **name**: 方案C(根治碰撞类,不在本批):scene 按入站连接核对 gate 实例
    **description**: 会话绑定时,把入站连接握手得到的 gate uuid 记进快照;此后凡是来自「不是当前持有该 node_id 的实例」的连接帧(入场、ExitGame、客户端消息)一律丢弃。也就是把「同一会话」重新定义为 (gate 实例, session_id)。
    **pros**: 一次性覆盖 ExitGame、迟到入场、客户端消息三个面,不需要逐条消息加字段。
    **cons**: 要改「同一会话」的定义:不变量 2 的措辞需要用户拍板。入场入口前会多一道连接级判定,与「栅栏是第一步」的字面冲突。scene 是否已经按连接记录了 gate uuid、node 摘除时是否关闭入站连接,都未核实。改动面大。
**recommended**: 推荐方案A,并与 b 项同批落码。方案B不做,方案C登记为后续待拍板项。

理由:
1. e 不是 a–d 的前置条件。a–d 修的是登录链本身的竞态。e 处理的是另一类问题:「同一玩家的同号会话跨 gate 实例」。
2. 这一类问题在 player_locator 侧有现实的触发路径(冲突收尾、僵尸恢复、跨 zone 同号加迟到上报),后果是 30s 后踢掉活玩家;LeaveGame 迟到时会立即删会话并 LeaveScene。修法只需要两个可选的 string 字段加约 30 行 Go。gate 已经带了实例号,不用碰 C++。成本远低于收益。
3. 和 b 同批,是因为两者改同一组文件:markofflinelogic.go、player_locator.proto、login 的 DeleteSession。这样 player_locator.proto 只需生成一次,b 的条件 LeaveScene 拿到的也是已经核实过实例号的会话。
4. 不做方案B:它只堵碰撞类的 ExitGame 一面,与入场栅栏「同号即同会话」的口径不一致,而且迟到入场、旧客户端消息这两面仍然开着。真要做 scene 侧,应该走方案C,统一 (gate 实例, session_id) 的会话定义,这需要用户拍板不变量 2 的措辞。
5. 本批应同步更新设计文档:§7 表「scene 的 ExitGame / 断线代发」那一行改为「在单实例前提下精确;同号跨实例仍是残余」。§9 新增碰撞类残余(含反例与方案C的方向)。原文「旧进程的迟到帧……实际不会发生」只对崩溃式重启成立,需要加限定。

§9 残余 7 不并入 e。建议作为独立的小项,与 a 同一 gate 批次处理。三者同在 gate_event_handler.cpp 和 routing_identity_test,判据都是「Kafka 命令按 player_id 核对会话当前角色」:a 的 Kick 要加这条判据,7 的 Route 也是它,LeaseExpired(:166-172)已经内联实现了一份,可以收成一个纯函数。

动手前先看 [SceneEntry] player_changed 计数有没有非零。修法有两种,需要用户二选一:
- (7-A)外来角色的路由在 ApplyRouteIdentity / OnRouteDecision 之前直接丢弃并计数。代价是失去代码注释(scene_entry_dispatch.cpp:301-302)里写的「换回原角色时凭 lastSceneRoute 补发」这条恢复路径。
- (7-B)homeZoneId / ownerEpoch 改为随 SceneRouteTarget 走,转发时从欠账或路由目标里取;外来路由不再替换当前角色的欠账。更标准,但要改 gate 的会话数据模型,属于原文说的「归属语义」范围。

两种都不改登录代次的写入路径。7-A 在字面上是「丢路由」,必须在设计 §4 写明:不丢路由指的是「不按代次丢」,外来角色的路由本来就会被玩家围栏撤销。
**contract_changes**: 方案A(推荐):
1. proto/player_locator/player_locator.proto:
   - `message SetDisconnectingRequest` 在现有 1-3 之后追加:`string gate_instance_id = 4;` 注释:发起断线的 gate 实例 uuid(来自 SessionDetails.gate_instance_id)。请求与会话记录的 gate_instance_id 都非空且不等时,判为旧实例迟到的上报,忽略;空表示旧版 login,不校验。
   - `message MarkOfflineRequest` 在现有 1-3 之后追加:`string expected_gate_instance_id = 4;` 语义同上。
   - 两个消息都没有 reserved 记录,4 号是新号。
2. 不改 Lua:session_cas.go:59-73 的 setDisconnectingScript 和 deleteSessionIfUnchangedScript 仍对 GET 到的完整字节做 CAS,新增的比对在同一份字节上做,没有 TOCTOU。
3. 不改任何 Redis 键或值格式。PlayerSession.gate_instance_id(4)已经存在。
4. 不改 RPC 签名(只加字段)。
5. login 内部的 Go 函数签名要加参数,见 code_changes。
6. 不涉及客户端契约,不涉及 tip 码。

方案B(不推荐;若用户坚持再做):
- proto/scene/player_lifecycle.proto `message GameNodeExitGameRequest { uint64 login_generation = 1; }`。按 git 历史看该消息一直是空的,但没有逐个提交核完,落码前用 `git log -S` 再核一次。
- 比对规则:仅当 request.login_generation != 0、实体 PlayerLoginIdentityComp.loginGeneration != 0、且 request < identity 时拒绝。任何情况下都不能做相等校验(G4)。

残余 7:没有 proto 改动。7-B 会给 gate 内部结构 SceneRouteTarget 加 homeZoneId / ownerEpoch 字段,不是线上契约。
**code_changes**: 方案A(只改 Go):
1. go/player_locator/internal/logic/setdisconnectinglogic.go:48-53。在 session_id 比对之后加:`if in.GateInstanceId != "" && session.GateInstanceId != "" && in.GateInstanceId != session.GateInstanceId { l.Infof("SetDisconnecting: stale gate instance ... ignoring"); return &common.Empty{}, nil }`。必须放在状态判断(:64-71)之前。
2. go/player_locator/internal/logic/markofflinelogic.go:65。比对条件改为:session_id 或 version 不等,或「两边实例号都非空且不等」,都判为陈旧并忽略。日志带上两边的实例号。
3. go/login/internal/logic/pkg/sessionmanager/session_manager.go:
   - :116-126 SetSessionDisconnecting 增加 gateInstanceID string 参数,写入请求;
   - :157-175 DeleteSession 增加 expectedGateInstanceID 参数,在 :162 的本地预检里同样比较 current.GateInstanceID,并在 MarkOfflineRequest 里填 ExpectedGateInstanceId。
4. go/login/internal/logic/clientplayerlogin/session_lifecycle.go:46-54 deletePlayerSession 与 :56-93 markPlayerSessionDisconnecting 都加 gateInstanceID 参数并往下透传。CRITICAL 日志带上实例号。
5. 调用点(全仓只有这三处,已 grep 核对):
   - disconnectlogic.go:53 传 sessionDetails.GateInstanceId;
   - leavegamelogic.go:52 传 sessionDetails.GateInstanceId;
   - entergamelogic.go:890 / :893(compensateFenceBind)传 state.gateInstanceID。sessionPlayersToSettle 里的 bound 玩家来自同一个栅栏键,键本身就包含 gate_instance_id,所以用同一个实例号是对的。
6. 测试桩 session_fence_wiring_test.go:87 的 fenceLocator 要能记录新字段。
7. gate、scene 不改。设计文档:§7 表那一行、§9 新增碰撞类残余、§10 验收映射加用例名、§11 改动清单,并在 docs/PROGRESS.md 追加一条。

方案B(备选,不推荐):
- gate client_message_processor.cpp:480-504:组 GameNodeExitGameRequest{login_generation = sessionIt->second.loginGeneration},序列化进 exitMsg.message_content.serialized_message。只读会话字段,不写。
- scene player_lifecycle_handler.cpp:28-41 守护段:在 HandleExitGameNode 之前比较 PlayerLoginIdentityComp。纯判定放进 enter_fence.h(例如 `IsStaleExit(identityGen, requestGen)`)。拒绝时只计数加 WARN,不擦映射、不改退出意图。

残余 7(若与 a 同批):
- 7-A:gate_event_handler.cpp:74 前面加外来判定:event.player_id != 0、session.playerId != kInvalidGuid 且两者不等时,计数并采样打 WARN,然后返回。判定收成 scene_route_helper.h 里的纯函数,供 AttemptPendingSceneEntry(:330-335)、EntryForLateLoginBinding(:308-311)、LeaseExpired(gate_event_handler.cpp:166)和 a 的 Kick 共用。
- 7-B:session_info_comp.h:61-62 的两个字段挪进 SceneRouteTarget;ForwardPlayerToScene(scene_entry_dispatch.cpp:131-132)改为从欠账目标取值;OnRouteDecision(:349-360)遇到外来路由时不替换当前角色的欠账。
**rollout_compat**: 方案A:
- 升级顺序:先 player_locator,后 login。
- 并存窗口里四种组合都退回现状,不会比现在更差:
  - 新 locator + 旧 login:字段为空,不校验,与现状一致;
  - 旧 locator + 新 login:未知字段被忽略,与现状一致;
  - 会话记录的 GateInstanceId 为空(早期记录或 dev 环境):不校验;
  - 只有两边都非空且不等才忽略。
- 回退:任意顺序回退都安全,不涉及数据迁移,也不进存盘。
- 字段默认值语义:"" 表示未知或旧版,不能当成「不等」处理。
- 风险评估:有没有合法上报却带着与记录不同的实例号?login 写会话记录用的 gate_instance_id 与 gate 上报用的 SessionDetails.gate_instance_id 是同一个来源(node_uuid)。新实例上的新会话会通过 SetSession/Reconnect 把记录改成新实例,所以合法的断线上报一定与记录一致。只有跨实例的陈旧上报会被忽略,而这正是要挡的。
- Java 版:属于服务端内部协议,不涉及客户端契约。Java 版的断线与登出模型不同(库行 owner_epoch),是否存在同类问题未核实,需要在 Java 仓 PARITY.md 登记「待核实」。

方案B(若做):
- 先 scene,后 gate,反过来也可以:旧 gate 不填,值为 0,不校验;旧 scene 忽略新字段。
- 回退顺序任意。
- 0 一律放行。

残余 7:只改 gate,没有协议变化,可随 gate 单独回退。7-A 会改变外来角色路由的处置方式(从「先替换再撤销」变为「直接丢弃」)。
**invariants_check**: 1. 入口栅栏必须是第一步,且零副作用:
   - 方案A 不改 scene,不受影响。
   - 方案B 的判定在 ExitGame 处理器里,不在入场入口;拒绝分支不擦映射、不挂也不摘 UnregisterPlayer / PlayerExitIntentComp,不违反。
   - 方案C 会在 HandleEnterGameNode 之前多一道连接级判定,与「栅栏是第一步」的字面冲突,所以不放进本批。
   - 残余 7 两种修法都不改 scene,不违反。
2. 同一会话一律放行:
   - 方案A 不涉及。
   - 方案B:同一 gate 进程内,会话代次只经 MergeLoginGeneration 取 max(scene_route_helper.h:132-135)。scene 身份代次来自 gate 盖的章,并且只取 max(player_lifecycle.cpp:2456),所以真正同一会话的 ExitGame 恒满足「请求代次 ≥ 实体代次」,不会被误拒。但它对 ExitGame 把「同号、代次更低」当作不同会话,与入场栅栏「同号即同会话」(enter_fence.h:147-149)的口径不一致。这是不推荐 B 的主要原因。
   - 方案C 需要把「同一会话」改为 (gate 实例, session_id),要用户拍板。
   - 残余 7 不涉及。
3. SessionMap 只擦属于本玩家的映射(EraseSessionMappingIfOwned):
   - 方案A 不涉及;方案B 拒绝时什么都不擦,不违反。
   - 现存两处直接 erase:scene_handler.cpp:347(载入中的 ExitGame)与 :368(快照会话不符)。它们擦的都是刚核对过「映射到本玩家」的那一条,语义上等价于 IfOwned。本批不动;若要统一写法,作为独立的整理项。
4. gate 的 loginGeneration 只经 ApplyLoginBinding / ApplyRouteIdentity 写入,只抬高,不丢路由:
   - 方案A 不改 gate,不违反。
   - 方案B 只在断线回调里读 SessionInfo.loginGeneration,不写,不违反。
   - 残余 7-A「丢弃外来角色的路由」不改代次写入路径,但字面上是丢路由。需要在设计 §4 写明:「不丢路由」指的是「不按代次丢」,而外来角色路由本来就会被 AttemptPendingSceneEntry 的玩家围栏撤销,并请用户确认。
   - 7-B 不丢路由,也不改代次逻辑,不违反。
**tests**: 先写能复现问题的用例(按 AGENTS §10.1,由 Codex 执行):

方案A:
1. go/player_locator/internal/logic/logic_test.go:
   - TestSetDisconnecting_StaleGateInstance_Ignored:先写入 {player=1, session=7, GateInstanceId="gate-I2", ONLINE};再发 SetDisconnecting{1, 7, GateInstanceId:"gate-I1"}。断言:状态仍是 ONLINE,版本号不变,租约 ZSET 里没有该成员。
     - 现状复现:proto 生成之前,用现有用例 TestSetDisconnecting_HappyPath(:247)的形状说明「同号、来源任意」都会把状态改成 DISCONNECTING;
     - proto 生成之后,这条用例在旧逻辑下为红,修复后为绿。
   - TestSetDisconnecting_EmptyGateInstance_Proceeds:请求为空,或记录为空,两种情况都照常进入 DISCONNECTING(兼容)。
   - TestSetDisconnecting_SameGateInstance_Proceeds。
   - TestMarkOffline_StaleGateInstance_Ignored:会话保留,SceneManager 的 LeaveScene 桩没有被调用。
   - TestMarkOffline_EmptyExpectedInstance_Proceeds。
2. go/login/internal/logic/clientplayerlogin/session_fence_wiring_test.go:
   - fenceLocator 记录 SetDisconnectingRequest 和 MarkOfflineRequest;
   - 断言 Disconnect、LeaveGame、compensateFenceBind 三条路径的 GateInstanceId / ExpectedGateInstanceId 都等于 SessionDetails 或 state 里的实例号;
   - 另测一条 sessionPlayersToSettle 的 bound 玩家也带上同一实例号。
3. go/login/internal/logic/pkg/sessionmanager:DeleteSession 在 current.GateInstanceID="gate-I2"、expected="gate-I1" 时不调用 MarkOffline。
4. 执行命令(Codex):在 go/player_locator 与 go/login 下分别跑 `GOTOOLCHAIN=local go test ./internal/...` 和 `go vet ./...`。通过标准:原有用例数不减少,并且全部通过。proto 生成先按 AGENTS §4,用 proto-gen-build 再 proto-gen-run,加 `-ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml`。

方案B(若做):
- cross_zone_test 新增 EnterFenceEcs.ExitGameFromOlderLoginOnSameSessionIdIgnored:实体以 (s, g2) 绑定后,派发 ExitGame(s, g1<g2)。断言没有 UnregisterPlayer、SessionMap[s] 仍指向本玩家。
- 同一用例补三种对照:g=0 时照常退出(兼容);g=g2 时退出;g>g2 时退出。
- 纯函数用例覆盖判定表。

残余 7(若做):
- routing_identity_test 新增 ForeignRouteKeepsOwnPendingEntry:会话已绑 Y,且有 Y 的欠账;来一条 X 的路由后,断言 Y 的欠账、homeZoneId、ownerEpoch 都不变。
- 按选定修法改写 ForeignRouteLeavesGenerationAlone(:1502)对 zone 与 epoch 的期望。
- 补一条 7-A 的「换回 X 后等 X 自己的路由」用例。
**cross_item_dependencies**: - a(Kick 带 player_id):
  - 顺带发现:login 的 kickReplacedSession 在 existing.SessionID == currentSessionID 时直接跳过踢线(entergamelogic.go:994),不比实例号。跨实例同号时旧连接不会被踢,会一直挂着,于是本项反例 (a)(b)(c) 的迟到窗口变成无界。建议 a 把这条判断改成比较 (GateInstanceID, SessionID)。
  - a 给 Kick 加的 player_id 核对、LeaseExpired 现有的内联核对(gate_event_handler.cpp:166-172)、残余 7 的外来路由判定是同一条判据,建议收成一个纯函数。残余 7 若要做,与 a 同批。
- b(LeaveScene 带会话身份):方案A 与 b 改同一组文件(markofflinelogic.go、player_locator.proto、login 的 DeleteSession / deletePlayerSession),建议同批,只生成一次 proto。b 的条件 LeaveScene 应该在 MarkOffline 核实实例号之后才执行。
- d(落点 Lua 记会话身份):d 用 (session_id, login_generation) 作身份。同号跨实例时新登录的代次一定更大,所以 d 的身份不受碰撞类影响,不需要再记实例号。b 与 d 从 PlayerSession.login_generation(14)取值时要注意:旧会话记录可能为 0。
- c(ReleasePlayer 带 owner_epoch):无依赖。
- a、b、d 与方案A 都要改 proto,建议合并成一次 proto-gen,一次让 Codex 全量编译。
- 设计文档:scene-enter-session-fence.md §7、§9、§10、§11 会被 a–e 一起改,要按项分段追加,不要回退栅栏批次的原文。
**open_questions**: 1. 需要用户拍板:e 是否要做。我的建议是只做方案A,并入 b 的批次;方案B 不做。
2. 需要用户拍板:碰撞类的根治(方案C,按 (gate 实例, session_id) 定义会话)是否立项。它要改不变量 2 的措辞,并在入场入口前加一道连接级判定。
3. 未核实:scene 在 gate 节点从 etcd 摘除(ExecuteNodeRemoval,node.cpp:1500 起)时会不会关闭来自该 gate 的入站 TCP。会关,则 scene 链反例 (b) 只剩冲突收尾那十几秒;不会关,则僵尸 gate 恢复后仍能发帧。
4. 未核实:路由模式下 gate → 路由服 → login 的 Disconnect 调用有多长的截止时间,它决定 login 侧迟到窗口的上界。
5. 未核实:客户端跨区重定向「先连新、后关旧」的具体时序(只依据设计文档 cross-zone-scene-travel.md:20、:431 和 :821,没有读客户端仓)。
6. 需要用户拍板:R13 兼容位(scene_handler.cpp:89-101,player_id=0 放行)。审计文档要求「灰度一版后改为丢弃」,目前仍是放行,守卫 2 对老 gate 是失效的。是否另立一项收口?
7. 需要用户拍板:残余 7 选 7-A(提前丢弃外来路由,失去换回原角色时的补发恢复路径)还是 7-B(homeZoneId / ownerEpoch 随路由目标走,改动大一些)。也可以先看 [SceneEntry] player_changed 计数再决定做不做。
8. 未核实:Java 版断线与登出路径是否有同类「只比 session_id」的问题。PARITY.md 需要登记。
9. 未核实:GameNodeExitGameRequest 的 1 号字段在全部历史提交里是否用过。现有证据显示它一直是空消息,方案B 落码前要用 `git log -S` 再核一次。
