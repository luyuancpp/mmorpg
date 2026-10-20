<!-- 只读分析代理产出（2026-10-16，接手会话工作流 wf_f7177272-747），原样保存供落码时查证；结论未经运行验证。 -->

**item**: 进场链余项 a：login 踢线只按 session_id；失锁（或任何原因导致 SetSession 失败）的登录链会在提交之前就把当前会话踢掉
**current_behavior**: 说明：下列文件与 HEAD（a105c3efd）一致，没有未提交改动（已用 git diff HEAD 核对）。唯一例外是 client_message_processor.cpp：它有别的会话在途的传输层改动，与本题无关，引用的行号取自工作区。

【login 入场链：登录 → 踢旧会话 → SetSession】
1. go/login/internal/logic/clientplayerlogin/entergamelogic.go:79 EnterGame。
   - :113-133：gate 连接会话栅栏，在加锁之前判。
   - :140-152：TryLock `player_locker:{pid}`，只尝试一次。
   - :319-332：起心跳。失锁时 onLost 只做 chainCancel() 并记 lock_lost。
   - :422-432：SubmitPreload，预加载完成后回调 onPreloadComplete（:334），进入 :374 applyLoadedPlayerSession(chainCtx)。
2. :732 GetSession(ctx) 读出 existing。
   - :738 DecideEnterGame（go/login/internal/logic/pkg/sessionmanager/session_manager.go:180-195）：无记录为 FirstLogin；DISCONNECTING 且同账号为 ShortReconnect；其余（含 ONLINE）一律为 ReplaceLogin。
3. :745 铸代次：mintEnterLoginGeneration 调 sessionmanager.MintLoginGeneration（login_generation.go:61），下限取 existing.LoginGeneration。这是 ebccaf04d 引入的。
4. :754 进入 persistEnterGameSession（:900）。ReplaceLogin 分支的顺序是：
   - **先踢线（:909-913）**：kickReplacedSession（:990-999）在以下情况跳过：existing 为 nil、existing.SessionID 等于本会话、GateID 为空。否则调 svcCtx.KickSessionOnGate（go/login/internal/svc/servicecontext.go:611-622）。
     - 这个函数**不接收 ctx**，链 ctx 已被取消也照发。
     - 它经 buildKickPlayerCommand（gate_command.go:134-142）组信封：只填 SessionId、PlayerId（取自 existing.PlayerID）、EventId，**payload 为空**。再经 buildGateCommandMessage（:45-99）同步写入 Kafka。
     - 踢线出错只记日志（:911）。
   - **后提交（:915-927）**：NewOnlineSession 令版本号 = existing+1、带上代次，然后调 SetSession(ctx)。
5. 提交点在 player_locator：go/player_locator/internal/logic/setsessionlogic.go:30-101。
   - :68 版本号必须等于当前记录 +1；
   - :73 代次必须严格递增（loginGenerationAdvances，:110-112，ebccaf04d 引入）；
   - :89 Lua 整字节 CAS（session_cas.go:237 replaceSessionAndCancelLease）。
   - 注意：记录不存在时（:46-47）走 missing 分支，不做任何版本校验。
6. SetSession 成功后才依次执行：:763 bindSessionFence → :771 SendBindSessionToGate（BindSessionEvent 走 payload）→ :792-801 栅栏复查 → :807 EnterScene。

【gate 收到踢线】
7. 生成路由 cpp/nodes/gate/handler/event/gate_kafka_command_router.cpp:25-31：payload 非空时走 DispatchGateCommandEvent；payload 为空时从信封拼事件。Kick 分支（:44-49）**只拼了 `event.set_session_id(...)`**，原因是 KickPlayerEvent 只有 session_id 一个字段（proto/contracts/kafka/gate_event.proto:29-32）。
8. cpp/nodes/gate/handler/event/gate_event_handler.cpp:100-131 KickPlayerEventHandler（守护段内）。处理只有三步：
   - 按 session_id 查会话；
   - lock 连接；
   - 推 kLoginBeKickByAnOtherAccount，然后 shutdown。
   - **不核对玩家**。对比同文件 LeaseExpired（:162-171），它在玩家不一致时跳过。
   - gate 推送路径另有现成的玩家身份栅栏：cpp/libs/services/gate/session/system/session_identity_fence.h 的 ClassifyPush，供 gate_service_handler.cpp:124、:182 使用；踢线没有用它。

【被踢之后】
9. 被踢连接断开（client_message_processor.cpp:455 起，工作区行号）：
   - 向 scene 发 ExitGame，带 player_id（约 :500-504）；scene 侧快照会话相等时会存盘并销毁实体。
   - 向 login 发 Disconnect（disconnectlogic.go），再到 markPlayerSessionDisconnecting（session_lifecycle.go:56），再到 player_locator SetDisconnecting（setdisconnectinglogic.go）。session_id 不同则忽略（:49）；ONLINE 转 DISCONNECTING 并把版本号加 1（:76）。

【会话号可复用】
10. gate 会话号 = node 前缀 + 17 位序号。Generate 会回绕（cpp/libs/engine/core/utils/id/node_id_generator.h:49-53）。建连时只跳过仍在用的号（client_message_processor.cpp 中 Generate 后的 while 循环）。所以同一 gate 实例内，已关闭连接的 session_id 会被新连接复用。另外，gate 在 LeaveGame 后不清 SessionInfo.playerId（disconnectlogic.go 注释写明，gate 侧也无 LeaveGame 处理）。
**problem_confirmed**: True
**problem_analysis**: 结论：竞态在 HEAD 上**仍然成立**。登录代次栅栏（ebccaf04d）对它**没有缓解**，原因有三：
- 铸代次（entergamelogic.go:745）在踢线（:910）之前，代次校验（setsessionlogic.go:73）又在踢线之后。踢线这个副作用夹在两者中间，不受任何栅栏约束。
- 失败的链根本走不到 EnterScene，scene 入场栅栏与本问题无关。
- 版本号 CAS 在登录链之间本来就已互斥，所以 locator 的代次校验对这类竞态基本是冗余的。

**T1（题述场景：失锁的陈旧链踢掉当前会话）**
1. 设备 A（会话 sA）的 EnterGame 拿到锁（:143），预加载在途。
2. A 的锁键丢失：主从切换丢写，或长停顿导致过期。心跳按 TTL/3 节拍检测（player_locker.go:126-235），检测之前 A 还会继续跑。
3. 设备 B（sB）拿锁，B 链完整跑完：GetSession → 铸 gB → SetSession(sB, gB) → Bind → EnterScene，然后释放锁。
4. A 的预加载回调到达。:732 读到 existing = sB（ONLINE），:738 判 ReplaceLogin，:745 铸出 gA ≥ gB+1。
5. A 的心跳到点，Renew 读到 0，onLost 调 chainCancel（:322-331）。
6. A 在 :910 调 kickReplacedSession(sB)。KickSessionOnGate 没有 ctx，**照发**。
7. :925 SetSession 因 ctx 已取消而失败。链按 apply 失败收尾，给 sA 推 3023。
8. gate 按 session_id 找到 sB，推「账号在别处登录」并 shutdown。sB 断线后 scene 退出、存盘、销毁实体；locator 记 DISCONNECTING(sB)。

结果：合法的当前会话 B 被踢，而没有任何新登录提交，两台设备都掉线，都要重登。实体经历一次多余的退出、存盘、再加载。不损坏数据：退出先存盘，scene 栅栏不受影响。

**T1′（不需要失锁）**
A 读到当前会话 sX → 踢 sX → SetSession 因 locator 抖动、超时或 CleanupPending 失败。后果与 T1 相同：没有提交就先踢了人。

**T2（踢线自己制造失败）**
1. A 读到 sX（版本 v），踢 sX。
2. Kafka、gate、断线回调、Disconnect、SetDisconnecting 这一路先于 A 的 SetSession 到达 locator，版本变为 v+1（setdisconnectinglogic.go:76）。
3. A 的 SetSession 在 :68 判 stale 而失败。

结果：X 被踢，A 也失败。只要提交在踢线之后，这条自伤路径就不存在。

**T3（踢到别人）**
gate 只按 session_id 踢，不核对玩家。被踢对象可能已不是本玩家：
- 记录里的 sX 已关闭，序号回绕后被别的玩家 Q 复用。典型条件：Disconnect 重试用尽，留下 ONLINE 僵尸记录（session_lifecycle.go:90 的 CRITICAL）。
- 同一连接 LeaveGame 后换角 Q，而 DeleteSession 不拿玩家锁。

此时 Q 被推「账号在别处登录」并断开，属于跨玩家误伤。

**修复后仍残留的窗口（供选方案）**
- **T4**：只做「提交后再踢」时仍有的窗口。A 已提交，但它的踢线写 Kafka 被拖慢；同时失锁，被替换的连接 sX 上又有一次更新的同连接重登 X2（gX2 > gA）提交，且 Bind 已先到 gate。迟到的踢线会把 X2 踢掉。只核对 player_id 挡不住（同一玩家）；按代次比较能挡住。
- **T5**：换角发生在新 Bind 之前，gate 上的 playerId 仍是 P，player_id 核对也会放行。概率很低，登记为残余。

**关于「生成器会自动映射 GateCommand.player_id」：属实，但有三个前提**
- 生成器源码 tools/proto_generator/protogen/internal/generator/cpp/gate_kafka_command_router.go:102-124 按字段名配对。跳过 repeated（:109-112）；类型不一致时 WARN 并跳过（:113-118）；配上的生成 `event.set_<f>(command.<f>())`。
- 已生成的路由可作佐证：RoutePlayer 分支映射了 player_id，Bind 分支映射了 enter_gs_type。
- GateCommand.player_id 是 uint64（gate_command.proto:12），login 已在 Kick 信封里填它（gate_command.go:139，取值 existing.PlayerID，见 entergamelogic.go:998），gate_command_test.go:182-189 已断言。
- 三个前提：
  1. 新字段必须命名为 `player_id`，类型必须是 `uint64`；
  2. 必须重跑 proto-gen-run。输出文件 gate_kafka_command_router.cpp 是生成物（写在 cpp/nodes/gate/handler/event/），不能手改；
  3. 只覆盖信封路径。若踢线改走 payload，映射就不起作用。
- 历史核对：KickPlayerEvent 从未用过 2 号字段（b3dd4e14e 至今只有 session_id = 1）。
## design_options
  - [0]
    **name**: A：提交后再踢 + gate 按 player_id 核对（交接原方向，最小改动）
    **description**: 1）把 kickReplacedSession 从 persistEnterGameSession 的 SetSession 之前（entergamelogic.go:909-913）挪到 SetSession 成功返回之后（:925-927 之后、return 之前），仍在 bindSessionFence/Bind/EnterScene 之前；SetSession 失败一律不踢。2）KickPlayerEvent 加 `uint64 player_id = 2;`，信封路径由生成器自动映射，login 不用改 payload。3）gate 的 KickPlayerEventHandler 复用 gate_session_fence::ClassifyPush(session.playerId, event.player_id())：kDrop（玩家不符或会话未绑定）不踢并打 WARN；kDeliverUnfenced（旧发送方 player_id=0）按旧行为踢并留一次 INFO；kDeliver 踢。
    **pros**: 直接封住题述窗口（SetSession 不成功就不踢），也消除 T2 自伤；T3 绝大部分由 player_id 核对挡住；不改 payload，旧版 login 本来就填了信封 PlayerId，所以 gate 一升级就生效；复用现成三态栅栏（R13 同口径，未绑定会话按 fail-closed 处理）；改动小，新旧并存双向兼容。
    **cons**: 失锁时仍有 T4（已提交但踢线迟到，踢掉同连接上更新的重登）；T5（换角发生在新 Bind 之前）仍在；SetSession 结果未知（locator 已写入但应答丢失）时改为不踢，旧设备会继续在线，而记录已指向 sA（见待定问题 2）。
  - [1]
    **name**: B：A + 踢线者登录代次栅栏（fencing token）
    **description**: 在 A 的基础上，KickPlayerEvent 再加 `uint64 login_generation = 3;`（发起踢线的这条已提交登录链铸出的代次）。buildKickPlayerCommand 改为照 Bind / PushTip 的做法，把 KickPlayerEvent{session_id, player_id, login_generation} 序列化进 GateCommand.payload，信封字段照填。gate 新增纯函数 ClassifyKick(sessionPlayer, session.loginGeneration, eventPlayer, eventGen)：先过 ClassifyPush；玩家一致、eventGen≠0 且 session.loginGeneration ≥ eventGen 时判 kSkipNewerLogin，表示该连接已承载不旧于踢线者的登录，不踢。
    **pros**: 把「失锁持有者的破坏性副作用」用 fencing token 封死，是该类问题的标准解。铸造键单调，后铸的值必然更大（有记录时还有 record+1 下限），gate 会话上的代次只来自已提交的 Bind 或路由，所以比较不会误拒合法踢线，T4 被精确挡住。只用 ≥ 比较、只读，不触碰 G4 禁止的相等校验。复用 ebccaf04d 已有的代次，不新增状态。
    **cons**: 踢线改走 payload，与交接所说「不用改 payload」不同：player_id 部分仍可走信封，代次必须进 payload。gate 的 session.loginGeneration 多了一个读者，session_info_comp.h:62-70 中「只用于盖章」的注释要改，设计文档要登记。新增一个 proto 字段和一段 gate 判定，测试面略增。T4 本身需要失锁与 Kafka 写入变慢同时发生，概率很低。
  - [2]
    **name**: C：A + 会话记录存 gate 连接化身号（incarnation）精确寻址
    **description**: PlayerSession 加 `uint64 gate_session_incarnation = 15`，ReconnectRequest 同样加字段。login 从 SessionDetails.session_incarnation 写入，踢线带上该值；gate 比对 session.incarnation 必须相等，同时仍做 player_id 核对。
    **pros**: 彻底免疫 session_id 回绕复用：incarnation 在进程内单调，加上实例 uuid 即全局唯一。
    **cons**: 要改 player_locator 契约（PlayerSession、ReconnectRequest、session_reconciler 的未知字段处理），login 两条写路径、Reconnect 回带都要改；换角走同一连接时 incarnation 不变，仍得靠 player_id 核对；T4 不能解决；跨 4 个组件，改动面大，与 b/d 的 locator 改动冲突概率高。
  - [3]
    **name**: D：提交点回带被替换的会话，按它踢
    **description**: 把 player_locator SetSession 的返回类型从 common.Empty 改为 SetSessionResponse{PlayerSession replaced = 1;}，在同一次 CAS 内回带被覆盖的旧记录；login 只踢 replaced（记录原本不存在则不踢）。
    **pros**: 踢的就是提交点真正替换掉的会话，原子精确；T5 中「记录已被 LeaveGame 删掉」的子情形也一并消除。
    **cons**: 改 gRPC 方法签名，所有生成的 Go 客户端、服务端都要重生成；与按版本号 CAS 的语义大部分重合（版本号一致即 session_id 一致），额外收益只在「记录在读与提交之间被删」这一种情形；对 T3 的复用场景无帮助，仍需 player_id 核对。
  - [4]
    **name**: E（否决）：顺序不变，只在踢线前检查 chainCtx 或锁
    **description**: 在 :910 之前加 `if ctx.Err() != nil` 或 Renew 检查。
    **pros**: 改动最小。
    **cons**: 只缩短窗口不消除：检查之后、SetSession 之前仍可能失锁，T1′、T2 完全不受影响；仍是「副作用先于提交」，违背 AGENTS §11.3。否决。
**recommended**: 推荐方案 B，即 A 的全部内容加上代次栅栏，作为一批完成。理由按优先级排列：

1. **正确性优先**：问题的本质是「破坏性副作用先于提交点」，再叠加失锁持有者（T1）。
   - 把踢线挪到 SetSession 成功之后，题述窗口和 T2 自伤一并消失，这是核心修复，必做。
   - player_id 核对复用现成的 ClassifyPush（R13 同一口径：未绑定会话 fail-closed），可挡住 T3 的跨玩家误伤，必做。
   - 代次栅栏挡住提交之后仍存在的失锁迟到窗口 T4。这是标准的 fencing token 用法，复用 ebccaf04d 已铸好的代次，只做 ≥ 比较、只读，代价是一个字段加十几行纯函数。
2. **用户偏好**：用户已表态「按最标准的做完，我不怕改动」（交接 §1）。
3. **兼容性**：新旧组件并存时双向安全，任一侧可独立回退（见 rollout_compat）。

如果用户要最小改动，只做 A 也能封住题述问题，T4 登记为残余。C 和 D 不推荐：改动面大，收益与 A/B 重叠。

实施要点：
- login 侧的踢线点放在「SetSession 成功之后、bindSessionFence 之前」。不能更晚：如果等到 EnterScene 成功才踢，EnterScene 被拒（可重试码 18/20）时，记录已指向 sA，旧设备 X 却还在 scene 上照常游戏；之后 sA 断线进租约，到期清理会把 X 正在玩的实体退掉。
- 提交之后踢线写 Kafka 失败，仍然只记日志，不让链失败（提交已经发生）。旧设备会因 scene 栅栏改绑而变哑，与现状一致。
- gate 的判定写成 session_identity_fence.h 里的纯函数，不放进 scene_route_helper.h，使 loginGeneration 的写入点仍只有那两个 helper（不变量 4）。
**contract_changes**: 1. **proto/contracts/kafka/gate_event.proto:29-32 KickPlayerEvent**（只新增字段，2、3 号历史上从未使用）：
```
message KickPlayerEvent {
  option (OptionKafkaIngress) = true;
  uint32 session_id = 1;
  // 被踢会话所属玩家。gate 只踢当前绑定该玩家的会话（gate_session_fence）；0 = 旧版发送方，按 session_id 踢。
  // 信封路径由生成路由从 GateCommand.player_id（同名、同为 uint64）自动映射。
  uint64 player_id = 2;
  // 方案 B：发起踢线、已提交的那条登录链的代次。会话已见过不小于它的代次时不踢；0 = 不比较。
  // 只经 payload 下发（GateCommand 没有同名字段）。
  uint64 login_generation = 3;
}
```
   - 类型必须是 uint64，否则生成器按类型不一致跳过映射（gate_kafka_command_router.go:113）。

2. **GateCommand（gate_command.proto）不改**。

3. **Kafka 报文**：
   - 方案 A：Kick 仍为空 payload，只靠信封。
   - 方案 B：Kick 改为非空 payload，序列化 KickPlayerEvent 的三个字段；信封里的 SessionId、PlayerId、EventId 照填（分区 key 仍取自信封 PlayerId，gate_command.go:96）。topic 和分区算法不变。

4. **Go 内部函数签名（方案 B）**：
   - `svc.(*ServiceContext).KickSessionOnGate(gateID, gateInstanceID string, sessionID uint32, playerID uint64, loginGeneration uint64) error`
   - `buildKickPlayerCommand(gateID, gateInstanceID string, sessionID uint32, playerID uint64, loginGeneration uint64)`
   - login 侧新增测试接缝：接口 `replacedSessionKicker{ KickSessionOnGate(...) error }`，生产实现是 *svc.ServiceContext，做法同 enterFailureTipPusher（entergamelogic.go:467-469）。

5. **C++ 纯函数**：在 cpp/libs/services/gate/session/system/session_identity_fence.h 新增
   - `enum class KickVerdict : uint8_t { kKick, kKickUnfenced, kSkipOtherPlayer, kSkipNewerLogin };`
   - `ClassifyKick(uint64_t sessionBoundPlayerId, uint64_t sessionLoginGeneration, uint64_t targetPlayerId, uint64_t kickerLoginGeneration)`：先调 ClassifyPush，再按代次判定。

6. **其余不改**：Lua 脚本、Redis 键、player_locator 与 scene_manager 的 gRPC 契约、scene 侧都不改。
**code_changes**: 1. **proto/contracts/kafka/gate_event.proto:29-32**：按 contract_changes 加字段 2，方案 B 再加字段 3。

2. **go/login/internal/logic/clientplayerlogin/entergamelogic.go**
   - :908-928：删掉 :909-913 的「先踢」。改成 SetSession 成功后再踢：
     ```
     if err := sessionmanager.SetSession(...); err != nil { return 0, err }
     if decision == ReplaceLogin {
         if err := kickReplacedSession(l.kicker, existing, state.sessionID, loginGeneration); err != nil { logx.Errorf(...) }
     }
     ```
     旁注：踢线是提交的后果，只在提交成功后发；失败的链不得有对外副作用。
   - :990-999 kickReplacedSession：改成接收 kicker 接口与 loginGeneration 的自由函数，保留「nil、同会话号、空 GateID 不踢」这三条判断（:991-996）。
   - :71-77 NewEnterGameLogic：增加 `kicker replacedSessionKicker`，默认值为 svcCtx，测试可替换。
   - :384-387 注释：补一句「踢线只在会话提交之后发出」。

3. **go/login/internal/svc/servicecontext.go:610-622** KickSessionOnGate：方案 B 加 loginGeneration 参数并透传。

4. **go/login/internal/svc/gate_command.go**
   - :134-142 buildKickPlayerCommand：方案 B 照 :112-132 的 Bind 做法，`proto.Marshal(&kafkapb.KickPlayerEvent{SessionId, PlayerId, LoginGeneration})` 放进 Payload，信封照填。
   - :101-111 注释：补上 Kick 改走 payload 的理由。只做方案 A 时本文件不改。

5. **cpp/libs/services/gate/session/system/session_identity_fence.h**：新增 KickVerdict 与 ClassifyKick 纯函数。头文件注释说明两点：
   - 未绑定会话沿用 kDrop（fail-closed）的理由：同一分区上 Bind 先于 Kick，正常时序下被踢会话一定已绑定；
   - 代次只用 ≥ 比较、只读。

6. **cpp/nodes/gate/handler/event/gate_event_handler.cpp**（只改守护段）
   - :4-25 的 include 段加 `<session/system/session_identity_fence.h>`；
   - :100-131 中，在 :112 find 成功之后、:113 lock 连接之前，调 ClassifyKick(it->second.playerId, it->second.loginGeneration, event.player_id(), event.login_generation())；
   - kSkipOtherPlayer / kSkipNewerLogin：LOG_WARN 后 return，日志带会话号、双方玩家、双方代次；
   - kKickUnfenced：首次记一行 INFO，做法同 gate_service_handler.cpp:132-134 的 LogFenceUnfencedOnce；
   - :129-130 的日志补上 event.player_id 与代次；
   - **只读 loginGeneration，不写**。

7. **cpp/libs/services/gate/session/comp/session_info_comp.h:62-70**：方案 B 时把「只用于给 PlayerEnterGameNodeRequest 盖章」改为「盖章；另由踢线身份栅栏只读比较。写入仍只经 ApplyLoginBinding / ApplyRouteIdentity」。

8. **生成物**（由 Codex 重生，不手改）：
   - cpp/nodes/gate/handler/event/gate_kafka_command_router.cpp:44-49 的 Kick 分支应多出 `event.set_player_id(command.player_id());`；
   - cpp/generated/proto/contracts/kafka/gate_event.pb.{h,cc}；
   - go/proto/contracts/kafka/gate_event.pb.go；
   - generated/proto/_unified/... 对应文件。

9. **文档**：
   - docs/design/scene-enter-session-fence.md：§7 表 :284 改为「已修」，§9 残余登记 T4/T5（只做 A 时）或 T5（做了 B 时）；
   - 交接 §3.3 第 a 行状态；
   - docs/PROGRESS.md 追加一条，并注明 Java 版状态；
   - routing-identity-audit 文档补一行「踢线也过 R13 栅栏」（可选）。
**rollout_compat**: **默认值语义**
- event.player_id == 0（发送方不带玩家）→ kKickUnfenced：按 session_id 踢，保持旧行为，并记一次 INFO 用于观测迁移进度。
- event.login_generation == 0 → 不做代次比较。
- session.loginGeneration == 0（旧版 login 写入的会话、或会话未见过代次）→ `0 ≥ gen` 不成立，照踢（回到旧行为）。
- 会话未绑定（playerId 为 kInvalidGuid）且事件带了 player_id → 不踢。与 ClassifyPush 的 R13 口径一致：同一 gate 分区上，Bind 的生产先于踢线（被替换链在 Bind、EnterScene 完成后才释放玩家锁，entergamelogic.go:350），所以未绑定只会出现在会话号复用或失败路径上。

**升级顺序**：没有硬依赖，建议先 gate、后 login。
- 新 gate 配旧 login：旧 login 一直在 Kick 信封里填 PlayerId（gate_command.go:139），新 gate 重生成后的路由就能拿到 player_id，玩家核对立即生效。旧 login 不带代次，不比较。踢线顺序仍是先踢后提交，T1 仍在，等 login 升级后才消除。
- 旧 gate 配新 login：
  - 方案 A：信封不变，旧 gate 只按 session_id 踢；顺序修复由 login 单方生效。
  - 方案 B：payload 非空，旧 gate 走 DispatchGateCommandEvent（rpc_event_registry.cpp:2196）解码 KickPlayerEvent，未知字段 2、3 被忽略，按 session_id 踢。行为与现状一致，不会丢踢线。
- Kafka 里在途的老格式踢线（空 payload）由新 gate 的信封路径处理，可以正常拿到 player_id。

**回退**：gate、login 可以各自独立回退，任一侧退回旧版都只是退回到对应的旧行为，不产生不一致。proto 只新增字段，回退不需要删字段；字段一经上线不再复用（AGENTS §4.3）。

**行为变化**
- 改动前：ReplaceLogin 在 SetSession 失败或结果未知时也已经踢掉旧会话。
- 改动后：只有提交成功才踢。
- 「SetSession 应答丢失、但 locator 实际已写入」时不再踢：旧设备继续在线，记录却已指向 sA。之后 Kafka 推送按记录寻址会投到 sA，旧设备收不到，直到下次登录收敛。这种情形是否用复读来消歧，见待定问题 2。

**升级窗口观测**
- gate 日志 `KickPlayer: skip` 的计数，以及 kKickUnfenced 首次 INFO；
- 不加以 player_id 为 label 的指标。如需指标，只用 `result` 低基数 label。
**invariants_check**: 1. **入口栅栏第一步且零副作用**（scene PlayerLifecycleSystem::HandleEnterGameNode 第 0 步，scene-enter-session-fence.md §5.3）：本方案不改 scene 任何代码，enter_fence 判定表和入口顺序都不动，满足。同一精神也落到了 login 和 gate：
   - login 把对外副作用（踢线）挪到提交点之后，即先完成判定与提交、再产生副作用；
   - gate 的 ClassifyKick 是纯函数，在 shutdown 之前判完，判为跳过时零副作用。

2. **同一会话一律放行**：scene 规则不受影响。
   - login 保留 kickReplacedSession 的「existing.SessionID == 本会话号不踢」（entergamelogic.go:994），同连接重登永远不踢自己。
   - 方案 B 的 `≥` 中，等号对应「该连接承载的正是踢线者本次的登录」。这只会在同一会话上出现，而同一会话本来就不发踢线，所以等号判为不踢是安全方向。
   - 方案 B 的比较不会作用于路由，也不会拒绝任何入场请求。满足。

3. **SessionMap 只擦属于本玩家的映射（EraseSessionMappingIfOwned）**：scene 的擦除路径一处不碰。gate 新增的是同一原则的对应物「只踢当前绑定本玩家的会话」：复用 ClassifyPush，未绑定或绑定的是别的玩家一律不踢。满足，且口径一致。

4. **gate 侧 loginGeneration 只经 scene_route_helper.h 的 ApplyLoginBinding / ApplyRouteIdentity 写入，只抬高、不丢路由**：
   - 方案 A 不读也不写 loginGeneration；
   - 方案 B 只在 ClassifyKick 里读（按值传入纯函数），踢线处理器与新纯函数都不写这个字段；判定放在 session_identity_fence.h，不放进 scene_route_helper.h，写入点仍只有那两个 helper；
   - 踢线不是路由，跳过踢线不会丢掉任何路由，ForwardPlayerToScene 的盖章逻辑不变。
   - 需要同步修改的只是 session_info_comp.h:62-70 的注释：读取方多了一个。满足。
**tests**: **先复现，再修复**

HEAD 上 kickReplacedSession 直接调 svcCtx.KickSessionOnGate。测试 harness 没有 KafkaClient，记录里若填了 GateID，会对 nil 生产者解引用，所以现有测试刻意把 GateID 留空来避开踢线（entergame_login_generation_test.go:30）。步骤如下：
1. 先单独提交「引入 kicker 接缝」这个纯重构，不改变行为；
2. 写下面的 L1，它在旧顺序下为红（踢了 1 次）；
3. 再调整踢线顺序，L1 转绿。

**Go，go/login/internal/logic/clientplayerlogin**（复用 newEnterWiringHarness，player_identity_backfill_test.go:389；它的 fake locator 的 SetSession 恒返回错误，:366-370）

- **L1 `TestEnterGame_ReplaceLoginSetSessionFailureDoesNotKick`**（复现用例）
  - 场景：existing 是 ONLINE 记录，GateID="7"、GateInstanceID="uuid-7"、SessionId=1（不等于 enterWiringSessionID）；SetSession 失败；注入 fake kicker。
  - 断言：kick 次数为 0。旧顺序下为 1。
- **L2 `TestEnterGame_ReplaceLoginKicksOnlyAfterSetSessionCommits`**
  - 新增一个 SetSession 成功的 fake locator 变体，与 kicker 共用一份事件序列记录。
  - 断言：序列依次为 GetSession、SetSession、Kick；踢线参数为 ("7", "uuid-7", 1, identityPlayerID, 铸出的代次)。
  - 链之后在 Bind 处因 state.gateInstanceID 为空而被 buildGateCommandMessage 拒绝，不会碰 Kafka。
- **L3 `TestEnterGame_SameSessionReplaceDoesNotKick`**：existing.SessionId 等于本会话号时，kick 次数为 0。这是 :994 的回归用例。
- **L4（失锁）**：SetSession 返回 context.Canceled，用来模拟心跳 onLost 之后的提交失败。断言 kick 次数为 0。
- **L5**（若采纳待定问题 2 的复读消歧）：SetSession 返回错误，但复读到的记录 SessionId 与 LoginGeneration 都等于本链写入值。断言踢 1 次、链仍按失败收尾。

**Go，go/login/internal/svc/gate_command_test.go**
- 扩展 :170-190 表驱动用例的 kick 行。
- 方案 B 新增 `TestKickPlayerCommandPayloadCarriesEvent`，仿照 Bind 那条：解码 payload 得到 KickPlayerEvent，session_id、player_id、login_generation 三项都对；信封的 SessionId、PlayerId、EventId 不变；分区 key 等于 player id。

**protogen，tools/proto_generator/protogen/internal/generator/cpp/gate_kafka_command_router_test.go**
- 新增 `TestGateKafkaRouterAutoMapsSameNameSameTypeFields`。
- 构造描述符：GateCommand 含 player_id(uint64)、session_id(uint32)；事件含 session_id(uint32)、player_id(uint64)。
- 断言 Assignments 同时包含 set_session_id 与 set_player_id。
- 再构造一个事件 player_id 为 uint32 的版本，断言被跳过。这条用来固化「必须同型」这个前提。
- 注意：现有 installGateRouterDescriptors（:16-25）里 GateCommand 没有字段，需要扩一个带字段的版本。

**C++，cpp/tests/routing_identity_test/routing_identity_test.cpp**（已经 include session_identity_fence.h，:22）
- `KickFence.MatchingPlayerKicks`
- `KickFence.OtherPlayerSkips`
- `KickFence.UnboundSessionSkips`
- `KickFence.LegacyKickWithoutPlayerIsUnfenced`
- 方案 B 追加：
  - `KickFence.SessionHostingNewerOrEqualLoginSkips`
  - `KickFence.OlderSessionGenerationKicks`
  - `KickFence.ZeroKickerGenerationSkipsComparison`
  - `KickFence.LegacySessionGenerationZeroKicks`
  - `KickFence.ClassifyDoesNotTouchSessionGeneration`：纯函数按值传参，断言调用前后 SessionInfo.loginGeneration 不变。

**交给 Codex 的执行步骤**（Claude 不执行）
1. 在仓库根目录执行 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build`（仅在生成器有变化或还没有工具时需要），再执行 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml`。
   - 期望：gate_kafka_command_router.cpp 的 Kick 分支多出 set_player_id；message_id 和 event_id 不变；守护段原样保留。
2. `cd tools/proto_generator/protogen && go test ./internal/generator/cpp/...`
3. `cd go/login`，设 `GOTOOLCHAIN=local`，执行 `go test ./internal/svc/... ./internal/logic/clientplayerlogin/...` 与 `go vet ./...`。
4. MSBuild 串行（`/m:1`）构建 gate 与 routing_identity_test，然后执行 `routing_identity_test.exe --gtest_filter=KickFence.*:LoginGeneration.*`。

通过标准：原有用例数不减少，全部绿。失败时保留首个失败用例的输出，以及生成器的 diff。
**cross_item_dependencies**: **与 a–e 其他项的关系**
- 与 b（LeaveScene 带会话身份做条件删除）、d（scene_manager 落点 Lua 记会话身份）**没有代码依赖**，a 可以独立先行。但三者对「会话身份」应使用同一口径：(session_id, login_generation)，比较只用 > 或 ≥，0 表示不比较，不在会话记录与下游之间做相等校验（G4）。方案 B 的 ClassifyKick 就是按这个口径写的，建议在 b、d 的设计里直接引用。
- 与 c（ReleasePlayer 带 owner_epoch）无关。
- 与 e（ExitGame 带登录代次、断线上报带 gate 实例号）有**文件重叠**：e 同样会动 gate 的 gate_event_handler.cpp 与 client_message_processor.cpp，也可能新增对 session.loginGeneration 的读取。
  - 建议 e 复用同一个 session_identity_fence.h，并遵守同一约束：只读、写入点不变（不变量 4）。
  - a 先合入，e 在其结构上追加。

**协作冲突与范围外问题**
- 并发会话冲突面：client_message_processor.cpp 当前有别的会话在途的传输层改动（工作区 diff），本项原则上不改它。go/login 的 login.go、config 有在途改动，与本项文件不重叠。
- 邻接问题（同类，不在本项范围内，建议另立）：
  - gate 的 PushToPlayerEventHandler（gate_event_handler.cpp:278-296）同样只按 session_id 投递，没有玩家栅栏。login 的 3023 失败通知（gate_command.go:157-180，走 payload）可能投到复用了该会话号的陌生连接。修法要给 PushToPlayerEvent 加 player_id 并写进 payload。
  - 设计 §9 残余 7（外来路由覆盖 epoch）与本项无关。
- Java 版（D:\luyuan\xuanming-server-mmo）：踢线由 scene 在归属被接管之后发出（PlayerKicked，见 game-gate ClientDispatcher.java:1193 与 SceneEventRouter.java:64），从结构上看属于「提交后才踢」，大概率不存在本缺陷。**未逐行核实**。按 AGENTS §12，需要在 PARITY.md 登记「不适用（结构上不存在）」或「待核」，并补上本仓 commit 号。
**open_questions**: 1. 采用方案 B（加踢线者代次栅栏，踢线改走 payload），还是只做方案 A（只用信封里的 player_id，把 T4 登记为残余）？推荐 B。

2. SetSession 结果未知（locator 已写入但应答丢失或 ctx 被取消）时怎么办？推荐用一个独立短超时 ctx 复读一次 GetSession：记录的 SessionId 等于本会话号、且 LoginGeneration 等于本链铸出值（由 G2 保证唯一），就视为已提交，照常踢旧会话，然后链仍按失败收尾。否则保持「不踢」。是否纳入本批？不纳入的话，新顺序会把「旧设备被踢」换成「旧设备继续在线、记录却指向 sA」：推送会投到 sA，直到下次登录才收敛。

3. 未绑定会话（playerId 为 kInvalidGuid）收到带 player_id 的踢线，推荐沿用 ClassifyPush 的 fail-closed（不踢）。另一种选择是像 LeaseExpired 那样 fail-open。需要确认。fail-closed 的代价：只有在失锁导致 Bind 被拖到踢线之后时，旧连接才会暂时不被踢，之后由 scene 栅栏拒掉陈旧入场、推 3023 收口。

4. T5 残余：gate 在 LeaveGame 后不清 SessionInfo.playerId，换角在新 Bind 之前，player_id 核对会放行。接受为残余，还是另立一项让 gate 在 LeaveGame 时清绑定？后者会影响 disconnectlogic.go 的 sessionPlayersToSettle，原文依赖「不清」这一点，所以需要单独设计。

5. 提交之后踢线写 Kafka 失败，目前只记 ERROR。是否加低基数计数 `login_kick_total{result}`？

6. 未核实项：
   - Java 版踢线时序未逐行核实；
   - 本分析中关于 Kafka 同一分区 Bind 先于 Kick 的保序，依赖「被替换链在 Bind 完成后才释放锁」（entergamelogic.go:350 的 defer 顺序）和同步生产者已确认才返回，没有运行验证。
