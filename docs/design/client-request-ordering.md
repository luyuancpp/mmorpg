# 客户端请求按会话保序:路由服水平扩展下的乱序修复(gate 路由 lane)

**Created:** 2026-10-14
**状态:** 已定稿,分批实施中(进度见 §16)。2026-10-14 用户确认按完整方案实施(「我不怕改动,我要最标准、可以水平扩展的做法」「记录到文档并开始做吧」),§11 的 Q1、Q2 按推荐项执行,Q3 授权已取得。各批**未编译、未验证**,编译与测试由 Codex 按 §13 执行。
**关联:** [client-rpc-router.md](./client-rpc-router.md)(D29–D34)、[client-access-band-routing.md](./client-access-band-routing.md)(D51–D64,阶段 0b/0d/0g)、[grpc-client-deadline-failure-callback.md](./grpc-client-deadline-failure-callback.md)、[microservice-zone-contract-20260914.md](./microservice-zone-contract-20260914.md)(预算链)、[team-system.md](./team-system.md)(expected_team_id、epoch/version)、[friend-client-spec-20260920.md](./friend-client-spec-20260920.md)(客户端单飞约定)、[scene-enter-session-fence.md](./scene-enter-session-fence.md)(登录代次 login_generation,与 §6.2 互补)、参考项目 `../xuanming-server/docs/design/protocol-ordering-rules.md`、Java 版 `D:\luyuan\xuanming-server-mmo` 的 `game-gate` 与 `PARITY.md`。

**证据标注:**
- 【码】代码或配置事实,写成 `路径:行号`,行号以 2026-10-14 当时的工作区为准(main 上有别的会话的未提交改动)。
- 【文】文档里的说法。
- 【推】静态阅读得出的推断,未运行验证。

**产出方式:** 两轮多代理工作流。第一轮 7 路只读调研(gate、路由服、login/scene_manager/match、社交服务、scene TCP 与推送、Java 版、参考规范);第二轮起草 1 份设计,做了 1 路对抗评审(替代方案视角)。另外三路评审(故障边界、业务栅栏、兼容上线)因账号额度中断,改由主会话逐条读代码复核,结果记在 §15。

---

## 0. 结论速览

1. **乱序是真的,而且与路由服副本数无关。** 这条链上有四处会让同一玩家的请求并发执行:gate 每条请求随机选路由服;路由服每条请求随机选业务实例;grpc-go 服务端每个 stream 起一个 goroutine;go-zero 超时拦截器再起一个 goroutine 跑 handler。所以即使全集群只有 1 个路由服、1 个 match,`JoinQueue → CancelQueue` 照样可能执行成 `Cancel → Join`,玩家以为取消了,实际还在队列里(§1)。
2. **粘性路由、一致性哈希都修不了。** 它们只能保证落到同一处,保证不了串行。按玩家加锁也只能保证互斥,保证不了先发的先拿到锁。
3. **推荐方案:在 gate 上按会话单飞排队。** 每个会话开一条「路由 lane」:经路由服的 gRPC 类请求按 gate 收包顺序逐条发出,上一条完成(成功回包、失败回调、节点摘除、本地截止到期,四者之一)之后才发下一条。不同玩家之间照样并行;路由服保持无状态、继续随机选副本(D29 不变);业务实例也继续随机。只有 gate 看得到客户端的收包顺序,也只有 gate 持有会话生命周期,所以保序放在这里(O1、O2)。
4. **第一阶段每个会话只开一条 lane(全序),不按服务分域。** 分域需要人工标注哪些方法跨域、要做屏障,新功能忘了标就会静默乱序,与仓库「约定拦不住不知情的新服务」的教训相同(AGENTS §7.5)。单 lane 默认全序,自动覆盖 team↔match、帮会历练↔match、登录生命周期↔其他服务这些跨服务因果对,gate 也不需要认识任何 Go 方法号,与号段终态 D51 一致。代价是同一会话内的读请求也被串行:健康时每条多等一个毫秒级往返;某个服务降级时,排队超过等待上限的请求会快速回 1003,不会拖到客户端超时。以后要放宽,也只能凭压测数据、通过生成的方法级 option 按需放开,默认值始终是全序(O3、§4.12)。
5. **gate 排队只能管住「观察到完成」之前的顺序,另外两层必须同批补齐。**
   - **预算链与提交点:** go-zero 超时后 handler 不会停。所以要求目标服务超时 < 路由服转发超时 < gate deadline,业务把请求 ctx 传进存储调用,提交前检查 ctx。满足这些,gate 看到超时的时候,目标已经不会再发出新的写。要修两处:login 的 `Timeout: 10000`、match 的 `Timeout: 5000` 都超出了路由服 5s 的转发预算;match 的票据读写用的是 `context.Background()`(§5、§6.1)。
   - **业务栅栏:** 用来兜跨会话和「最后一条已发出的写晚落地」两类情况。login 加会话栅栏,同时修掉 Disconnect 抢跑 Login,以及 EnterGame 后台链与断线之间的竞态(后者即使顺序正确也会出问题);match 的 JoinQueue 复核会话;guild 写请求补 `expected_guild_id`(§6)。
6. **Java 版已经有这套模型的大半。** Java gate 对每个会话、每个后端域只放一个在途调用【码】`game-gate/.../ClientDispatcher.java:413-439`。要对齐的是:lane 粒度(Java 是按域分,本文建议全序)、排队等待上限、超限行为(Java 是满 64 断连)、login 失败回执带 id、把截止下传给后端,以及跨会话复核(§10)。
7. **需要你拍板的有 4 项**(§11):lane 粒度与 Java 对齐方式;预算链的修法(推荐把 login 和 match 的超时降到 4000);实施授权与批次顺序;客户端行为确认(由客户端负责人回答)。

---

## 1. 问题与根因

### 1.1 现状链路

```
客户端 ──TCP──▶ gate(单 EventLoop,入口对单会话严格 FIFO)
                 │ 每条请求 PickRandomNode(ClientRpcRouter)               ← 随机 ①
                 ▼
            路由服副本 R1 / R2 …(每请求一个 goroutine)                   ← 并发 ②
                 │ 每条请求 pickTarget:login 同 zone 随机,其余全局随机   ← 随机 ③
                 ▼
            业务实例 M1 / M2 …(grpc-go 每 stream 一个 goroutine;
                               go-zero 超时拦截器再起一个 goroutine)      ← 并发 ④
```

- gate 只有一个 EventLoop,同一连接的帧在 `ProtobufCodec::onMessage` 里按到达顺序同步派发【码】`cpp/libs/engine/core/network/codec/codec.cpp:152-197`、`cpp/nodes/gate/handler/rpc/client_message_processor.cpp:542-546`。**乱序全部发生在 gate 出口之后。**
- ① gate 每条请求随机挑路由服副本【码】`client_message_processor.cpp:53-81,140-169`。
- ③ 路由服每条请求单独选实例,不看会话【码】`go/client_rpc_router/internal/logic/forwardlogic.go:148,196-201`、`internal/discovery/node_watcher.go:251-284`。
- ②④ grpc-go 服务端每个 stream 用 `go f()` 起一个 goroutine【码】`google.golang.org/grpc@v1.79.3/server.go:1057-1075`;go-zero 的 `UnaryTimeoutInterceptor` 再起一个 goroutine 跑 handler【码】`go-zero@v1.10.0/zrpc/internal/serverinterceptors/timeoutinterceptor.go:28-74`。
- gate 没有任何在途请求表,发出后不记账,回包到达也不认领【码】`cpp/libs/services/gate/session/comp/session_info_comp.h:12-120`。

### 1.2 为什么粘性 / 单副本 / 加锁都不够

| 手段 | 保证了什么 | 保证不了什么 |
|---|---|---|
| gate 按会话粘到同一路由服 | 落到同一路由服 | 同一 HTTP/2 连接上的两个 unary 是两个 stream,服务端并发执行(④) |
| 路由服按玩家一致性哈希选业务实例 | 落到同一实例 | 同上;而且业务的权威状态本来就在共享 Redis / MySQL,实例亲和不是需求【码】`go/match/internal/logic/queue.go:220-230` |
| 按玩家加分布式锁 | 互斥 | 先发的不一定先拿到锁;login 的 player_locker 拿不到锁就立即回错,不排队【码】`go/login/internal/logic/clientplayerlogin/entergamelogic.go:97-114` |
| 客户端回显 `ClientRequest.id` | 回包对得上号 | 服务端没有任何地方按它排序;它由客户端自定,可能重复或为 0 |

### 1.3 已确认的乱序后果(节选)

| 依赖对 | 乱序后的最终状态 | 现有兜底 | 证据 |
|---|---|---|---|
| JoinQueue → CancelQueue(不等回包、不带票号) | Cancel 找不到票回成功,Join 随后入队,玩家仍在匹配队列,可能被凑单拉进战斗 | 只有带 `queue_ticket` 时才安全,而带票号意味着已经等过回包 | 【码】`go/match/internal/logic/cancelqueuelogic.go:38-57` |
| RespondInvite(accept B) → LeaveTeam(expected=B) | Leave 先执行时未绑定,回成功;随后入队成功,玩家留在 B 队 | 无 | 【码】`go/match/internal/team/service.go:303-307,636-644` |
| AcceptFriend → RemoveFriend;Block → Unblock | 最终仍是好友 / 仍被拉黑,两条都回成功 | 无 | 【码】`go/friend/internal/logic/friend_logic.go:415-424`、`internal/data/block_repo.go:36-50` |
| ApplyJoinGuild → CancelGuildApplication | 撤回回 NotFound,申请随后生效,审批后玩家违背意图进帮 | 申请有过期时间 | 【码】`go/guild/internal/data/guild_manage_repo.go:1937-1955` |
| CancelQueue(match)→ RespondGuildTrialInvite(accept,凑齐开战,guild) | accept 先执行时,开战预检看到 queued 票,整个历练房间以 MEMBER_NOT_READY 结束 | 无 | 【码】`go/guild/internal/logic/activity_trial.go:594-619`、`go/match/internal/logic/activitybattlelogic.go:105-117` |
| CancelQueue(match)→ StartTeamMatch(team) | 预检看到队长自己的 queued 票,回「有成员未准备好」 | 无 | 【码】`go/match/internal/team/service.go:420-462` |
| Login → gate 合成的 Disconnect(客户端登录后立刻断线) | Disconnect 先执行,Login 再写入 login_session 与设备集,残留 30 分钟,可能误触设备数上限 | 只有 TTL | 【码】`go/login/internal/logic/clientplayerlogin/loginlogic.go:150-176`、`disconnectlogic.go:25-38` |
| EnterGame(已受理,后台链未完成)→ 断线 | Disconnect 带 player_id=0 早退;后台链随后写入 ONLINE,没有租约,玩家卡在线。**顺序正确也会发生** | 无 | 【码】`entergamelogic.go:238-241,705-722`、`session_lifecycle.go:117-120` |
| 有副作用的请求超过 5s | 路由服按 5s 回 kServiceUnavailable,目标 handler 继续跑完并提交;客户端接着发下一条或重发 | 部分服务有幂等键 | 【码】`forwardlogic.go:165-184`、`timeoutinterceptor.go:57-72` |

### 1.4 为什么保序点放在 gate

- 只有 gate 看得到客户端的原始收包顺序,也只有 gate 持有会话生命周期(建连、顶号、断线、重定向)。
- gate 是单 EventLoop,会话表是 thread_local,gRPC 完成回调也由主 loop 上的定时器轮询 CQ 执行【码】`cpp/libs/engine/core/node/system/etcd/etcd_service.cpp:33-37`。队列不需要加锁,也不需要新线程。
- 路由服按 D29 无状态,可以随时重启【文】`client-rpc-router.md:27`。要在路由服里排队,必须先把会话粘到固定副本、处理成员变化时的排空,而路由服被黑洞时,粘性会让一个会话的请求全部卡住。
- Java 版已经验证了这个位置:gate 按会话串行发出,后端契约直接写着「同一会话的调用由 gate 串行发出」【码】`game-api/src/main/java/com/game/api/ClientMessageService.java:30`。

---

## 2. 方案总览:三层

```
                ┌──────────────── 第 1 层:gate 路由 lane(O1–O9)────────────────┐
客户端帧 ─FIFO─▶│ 闸门(会话/验签/白名单/体积/限流/GM/battle)                        │
                │   └─ PROTOCOL_GRPC 且路由模式 → 会话 lane 准入                     │
                │        queued: [r3, r4] ← 有界(32)、带入队时刻与身份快照           │
                │        inFlight: r2(gate_call_seq=s2)── 只放 1 条               │
                │   完成 = 成功桥 / 失败桥 / 节点摘除 / 本地截止 ── 恰好一次 → 发下一条│
                └───────────────┬──────────────────────────────────────────────────┘
                                │ x-session-detail-bin{…, incarnation, gate_call_seq}
                                ▼
                     路由服(无状态、随机,原值回写会话头)
                                ▼
                ┌──────── 第 2 层:预算链 + 提交点(O10、O11)────────┐
                │ 目标 Timeout + 1s ≤ ForwardTimeout < 路由服 Timeout   │
                │ 请求 ctx 传进 Redis/MySQL;提交前检查 ctx            │
                └──────────────────────┬──────────────────────────────┘
                                       ▼
                ┌──────── 第 3 层:业务栅栏(O12–O14)────────┐
                │ login 会话栅栏(双边握手)                       │
                │ match JoinQueue 复核会话;guild expected_guild_id │
                │ 现有:team expected_team_id + ver CAS、           │
                │       player_locator session+version CAS、       │
                │       chat request_id、match ticket CAS          │
                └───────────────────────────────────────────────────┘
```

三层各管一段,缺一不可:

- 第 1 层保证:同一会话的请求按收包顺序发出,前一条观察到完成后才发下一条。
- 第 2 层保证:「观察到完成」(包括超时)时,目标已经不会再发出新的写。
- 第 3 层兜住剩下的两类:跨会话(重连、换 gate),以及截止那一刻已经发出、但晚落地的最后一条写。

---

## 3. 决策

| # | 决策 | 理由 |
|---|---|---|
| O1 | 保序点放在 gate;路由服无状态、随机选副本;业务实例随机 | §1.4;D29、D33 不变 |
| O2 | 只在**路由模式**下、只对 **PROTOCOL_GRPC** 消息启用 lane。TCP(scene)路径不进 lane;直连模式不启用;gate 合成的 Disconnect 不进 lane | TCP 路径本身单连接 FIFO,两端单线程,已经有序;而且返回 Empty 的方法(MoveStart/MoveSync/MoveStop)不回包,排队会卡死【码】`proto/scene/player_movement.proto:105-107`。直连模式下 match 不回写会话头,无法认领(O7)。Disconnect 见 O8 |
| O3 | **第一阶段每个会话一条 lane,全序**;放宽只能凭压测数据、通过生成的方法级 option 显式放开,默认值始终是全序 | §0 第 4 条;fail-closed;不需要方法知识,与 D51 兼容 |
| O4 | lane 同时只放 1 条在途;完成来源有 4 个,每个请求恰好得到一个回执,晚到的完成一律丢弃 | 认领恰好一次,与 Dubbo 超时后丢弃应答同语义【文】Java `docs/design/architecture.md:101` |
| O5 | 认领用 gate 自己的令牌:SessionDetails 新增 `session_incarnation = 7`、`gate_call_seq = 8`,在**准入时**按收包顺序分配;路由服原值回写,不改路由服代码 | 不信任客户端给的 id;incarnation 防 session_id 复用串号(F47);准入时分配的号以后也可以给业务做水位栅栏 |
| O6 | 有界:排队 + 在途 ≤ 32,超限回带 id 的 1008,不计非法包;排队等待超过 `kMaxLaneWait`(5s)不发出,回带 id 的 1003;所有 gate 级失败回执都带请求 id | 防内存放大与客户端超时;合并 band-routing D57;完成阶段 0d |
| O7 | 直连模式维持现状,在启动日志里注明「直连模式不保序」,交由 D61 删除直连路径时一并处理 | 直连模式下 match 不回写会话头【码】`go/match/match_service.go:346-350`,成功桥找不到会话就直接返回【码】`cpp/nodes/gate/main.cpp:292-293`,今天直连模式下 match 的回包本来就全部丢失 |
| O8 | 断线:丢弃未发出的排队项,注销在途项(晚到的回包丢弃);gate 合成的 Disconnect 立即发出,由 login 会话栅栏兜底 | EnterGame 回包只表示已受理,后台链还在跑,把 Disconnect 排在后面也挡不住竞态;跨 gate 重连时没有共享的 lane |
| O9 | 重定向(RedirectToGate):会话标记 redirected,丢弃排队项,之后新到的路由请求回带 id 的 1003;在途回包照常下发 | 与 Java 对齐【码】`ClientDispatcher.java:433-434,1058-1062`;防止旧连接的排队项与新 gate 上的请求交错 |
| O10 | 预算链:目标 `Timeout + 1000 ≤ ForwardTimeoutMs`;推荐把 login、match 的 Timeout 降到 4000;新增不变式 `gate deadline(router) + kMaxLaneWait + 1000 ≤ 客户端超时(15000)`;部署门禁核对 | §5 |
| O11 | 客户端请求路径上的存储调用一律传请求 ctx,提交前检查 ctx;补偿与回滚用独立 ctx | §6.1;先例 team【码】`go/match/internal/team/store.go:226-229` |
| O12 | login 会话栅栏:键 `(gate_instance_id, session_incarnation)`,单段 Lua 双边握手,TTL = chainBudget + 1min | §6.2 |
| O13 | match JoinQueue 提交前复核会话仍是当前会话(不原子,缩小跨会话窗口) | §6.3 |
| O14 | guild 的 Leave / Disband / Kick / Transfer / SetRole 补 `expected_guild_id`,0 表示旧客户端 | §6.4;先例 team 的 expected_team_id |

---

## 4. gate 路由 lane 详细设计

### 4.1 挂载点

当前闸门顺序【码】`client_message_processor.cpp:839-969`:

```
会话存在 → verified → IsClientMessageId 白名单 → 体积 + 限流 → GM 闸 → battle 拒绝 → 按 protocol 分派
```

lane 准入放在 `PROTOCOL_GRPC` 分支里、路由模式下,取代现在直接调用 `HandleRouterForward` 的做法(`:956-964`)。

- 限流、体积、白名单、GM、battle 这些拒绝仍然排在 lane 之前,立即回执,不占 lane 名额。它们的回执可能先于更早排队请求的回包到达客户端。这是允许的:本文保证的是**执行顺序**,不保证拒绝回执的到达顺序;拒绝回执带 id,客户端能对上号。
- `loginStarted` 改在**发出时**置位(现在路由模式下就是发出之后置位【码】`client_message_processor.cpp:821-826`)。排队中、断线时作废的 Login 从没到达 login,不需要通知,这样也不会被「发 Login 后立刻断开」用来放大断线 RPC(`:439-451` 的注释)。

### 4.2 数据结构

全部在 gate 主 loop 线程上读写,不加锁、不新增线程。

| 状态 | 位置 | 说明 |
|---|---|---|
| `uint64 incarnation` | `SessionInfo` 新字段 | 建连 emplace 时从 SessionManager 的进程内单调计数器取,从 1 开始【码】`client_message_processor.cpp:634-654`、`session_manager.h:8-23` |
| `RouterLane lane` | `SessionInfo` 新成员 | `{deque<Item> queued; uint64 inFlightSeq = 0; bool redirected = false;}`,`Item = {shared_ptr<ClientRequest> request; SessionDetails snapshot; uint64 callSeq; steady_clock::time_point enqueuedAt;}`。会话 erase 即取消,先例是 `pendingSceneEntry`【码】`session_info_comp.h:73-80`。codec 每帧 `New()` 一个新对象【码】`codec.cpp:211,235`,持有 shared_ptr 是安全的,它不是连接对象 |
| 在途索引 `unordered_map<uint64 seq, InflightCall>` | 新文件 `cpp/nodes/gate/handler/rpc/client_request_lane.{h,cpp}`,thread_local | `{sessionId, incarnation, clientRequestId, messageId, routerEntity, expiresAt}`;用 `extract(seq)` 保证恰好一次 |
| 过期堆 `priority_queue<(expiresAt, seq)>` | 同上 | 惰性删除:弹出时 seq 已不在索引里就跳过 |
| `gate_call_seq` 发号器 | 同上 | 进程内单调 uint64;0 保留,表示「不受 lane 管理的调用」 |

纯逻辑核心(准入、取下一条、完成核对、过期判定)放在只依赖 STL 的头文件 `cpp/libs/services/gate/session/comp/router_lane.h`,照 `cpp/nodes/gate/tests/gate_security_test.cpp` 的方式脱离引擎单独编译测试。

### 4.3 准入(Admit)

```
if lane.redirected                         → 回 {id, message_id, error=1003},return
if lane.queued.size() + (inFlightSeq?1:0) ≥ kMaxLaneDepth(32)
                                           → 回 {id, message_id, error=1008},不计非法包,return
item.snapshot  = BuildSessionDetails(sessionId, session.playerId, &session)
                 并把 kInvalidGuid 归一成 0(阶段 0b)
                 再写 session_incarnation = session.incarnation
item.callSeq   = ++gCallSeq;写进 snapshot.gate_call_seq
item.enqueuedAt = steady_clock::now()
lane.queued.push_back(item);Pump(session)
```

- **身份取入队时刻的快照。** 它反映客户端发这条请求时的身份,与 Java 一致【码】`ClientSession.java:212-217`、`ClientDispatcher.java:427`,也符合 D-9「身份只从会话取」。排队期间 BindSession 才到达时,请求按未登录被拒。今天这类请求带的是 UINT64_MAX,会被各服务当成真实玩家【码】`go/match/internal/logic/joinqueuelogic.go:49-51`;归一成 0 之后是 fail-closed,只会更好。

### 4.4 发出(Pump)

```
while lane.inFlightSeq == 0 && !lane.queued.empty():     // 循环 ≤ 32 次
    item = pop_front
    if now - item.enqueuedAt > kMaxLaneWait(5s): 回 1003(带 id),计 expired_in_queue;continue
    router = PickRandomNode(ClientRpcRouterNodeService)
    if !router: 回 1003(带 id);continue
    SendClientRpcRouterForward(…, metadata = item.snapshot)
    lane.inFlightSeq = item.callSeq
    index[item.callSeq] = {…, routerEntity = router,
                           expiresAt = now + grpc_call_deadline::Get(router 类型) + 1000ms}
    heap.push(expiresAt, seq)
    if message_id == Login: session.loginStarted = true
```

- 排队超时回的 1003 明确表示「没有发出」,客户端重试是安全的。
- `grpc_call_deadline::Get` 读实际生效的 deadline【码】`grpc_call_deadline.h:34-35`,不另写常数;本地与 K8s 都是 8000【码】`bin/etc/base_deploy_config.yaml:230`、`tools/scripts/k8s_deploy.ps1:2175-2176`。多出的 1000ms 用来覆盖 5ms 的 CQ 轮询和 loop 积压。

### 4.5 认领(Complete)

| 来源 | 入口 | 拿 seq 的方式 |
|---|---|---|
| 成功回包 | 成功桥 `SetIfEmptyHandler`【码】`main.cpp:290-315` | 解响应 initial metadata 里的 `x-session-detail-bin`(路由服在 Forward 开头按原值回写【码】`forwardlogic.go:103-105,203-212`) |
| gRPC 失败 | 失败桥 `SetIfEmptyFailedHandler`【码】`main.cpp:325-350` | `failure.FindSentMetadata(kSessionBinMetaKey)`,即发出时的原值 |
| 路由服节点摘除 | `OnNodeRemoveEventHandler`【码】`client_message_processor.cpp:1104-1113` | 遍历在途索引里 `routerEntity == 被摘实体` 的条目。事件在 DestroyEntity 之前、同一 loop 线程触发【码】`node.cpp:1469-1494`;此时该实体 CQ 上的在途调用永远不会再有回调(F29) |
| 本地截止 | GateRuntimeContext 上新增 `TimerTaskComp laneExpiryTimer`,`RunEvery(0.25s)`,回调不捕获对象 | 弹出过期堆。**必须主动扫描**:队头卡住时客户端在等回包,不会再发新请求,纯惰性清理永远触发不到。先例是 `sceneEntryRetryTimer`【码】`main.cpp:141-144,396-398` |

统一处理:

```
call = index.extract(seq);  if !call: late_dropped++,丢弃这次回包;return
it = sessions.find(call.sessionId)
if it 不存在 || it.incarnation != call.incarnation: incarnation_mismatch++,丢弃;return
if it.lane.inFlightSeq != seq: lane_mismatch++,丢弃;return      // 防御性,正常不可达
it.lane.inFlightSeq = 0
成功:原样下发 MessageContent(路由服已回显 id);失败 / 摘除 / 过期:下发 {id, message_id, error=1003}
conn = it.conn.lock();有连接才写
Pump(*it)
```

- `gate_call_seq == 0` 的成功回包只可能来自 gate 合成的 Disconnect,而那时会话通常已经 erase,保留旧行为即可(找不到会话就丢弃)。
- 失败桥改为回带 id 的信封,取代现在不带 id 的 `SendTipToClient(kServiceUnavailable)`【码】`main.cpp:350`,也就是阶段 0d。
- 本地截止与 gRPC deadline 的先后:正常情况下失败桥先在约 8s 时触发,本地截止(约 9s)只兜「回调丢失」这一种情况:F29,以及 CQ 轮询遇到 `!ok` 时直接 `return`、跳过本轮剩余事件【码】`cpp/generated/grpc_client/grpc_init_client.cpp:365-368`。

### 4.6 断线、顶号、重定向

- **断线**(包括顶号:KickPlayerEvent 之后 shutdown,走同一断线路径【码】`gate_event_handler.cpp:107-139`):在 `sessions.erase` 之前【码】`client_message_processor.cpp:484`,把 `lane.inFlightSeq` 从在途索引里删掉,晚到的完成一律丢弃;排队项随会话析构,不发出,计 `dropped_on_disconnect`;不等在途调用。gate 合成的 Disconnect 照旧立即发出(O8)。
- **重定向**:在 `RedirectToGateEventHandler` 里置 `lane.redirected = true` 并清空排队项【码】`gate_event_handler.cpp:230-267`(现在只推一条通知)。

### 4.7 回执与码表

| 情形 | 回执 | 计非法包 | 码(导表生成,不新增) |
|---|---|---|---|
| lane 满(排队 + 在途 ≥ 32) | `MessageContent{id, message_id, error}` | 否 | `kRateLimitExceeded = 1008`【码】`generated/code/proto/tip/common_error_tip.proto:28` |
| 排队超时、无路由服、gRPC 失败、节点摘除、本地截止、已重定向 | 同上 | 否 | `kServiceUnavailable = 1003`【码】`common_error_tip.proto:18`(fault=1) |
| 业务回包与路由服拒绝 | 路由服信封原样下发(已带 id) | — | — |

**客户端可见的变化**:gate 级失败从「23 号 tip、不带 id」改成「带请求 id 和原 message_id 的错误信封」;新增 1008 拒绝;同一会话的路由类回包按请求顺序到达。第一项与路由服现有的拒绝信封形状相同,客户端本来就要处理【推】;robot 按 message_id 认领,也兼容【码】`robot/chat_smoke_scenario.go:367-368,402`。按 AGENTS §12,这一批要与 Java 同批对齐(§10)。

### 4.8 SessionDetails 字段

【码】`proto/common/base/session.proto`:已占用 1–6。

```proto
  // gate 进程内单调,会话存续期间不变;0 = 旧版 gate(无栅栏)。与 gate_instance_id 合起来全局唯一。
  uint64 session_incarnation = 7;   // client-access-band-routing.md D60 预订的号
  // gate 进程内单调,准入时按收包顺序分配;0 = 不受 lane 管理(gate 合成的通知)。
  // 业务服务不得把它当幂等键(它按调用编号,不按客户端意图编号);可以作为 (incarnation, seq) 水位栅栏。
  uint64 gate_call_seq = 8;
```

- 这是内部契约,不是客户端可见契约。路由服不解析、按原值回写,无需改代码;login 拦截器重新 Marshal 后回写,protobuf-go 会保留未知字段【码】`go/shared/callerauth/interceptor.go:110-119`【推】。
- 改 proto 后按 AGENTS §4 重新生成,重编受影响的 C++ / Go 服务;Java 侧按其 ContractSync 规则同步。

### 4.9 生命周期合规(AGENTS §11.7)

排队项、在途索引、定时器闭包都不持有 `TcpConnectionPtr`,也不绑定裸 this。每次下发都先 `find(sessionId)`、核对 incarnation,再 `conn.lock()`,先例见 `main.cpp:294-297`。定时器回调不捕获对象,靠 TimerTaskComp 的存活令牌【码】`timer_task_comp.h:70-80`。

### 4.10 观测

gate 没有 Prometheus 出口。仿照 scene_entry 的汇总日志写法【码】`cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:414`,每 30s 打一行 `[lane]` 汇总:`admitted / sent / replied / failed / node_removed / expired / expired_in_queue / rejected_full / rejected_redirected / late_dropped / incarnation_mismatch / dropped_on_disconnect`,外加当前在途数和最大队长。本地截止进 error_reporter。不使用 player_id、session_id 或原始 message_id 作维度(AGENTS §9)。

### 4.11 性能

- 每条请求多一次 deque push/pop、一次哈希表插入与 extract、一次堆操作。在途索引 ≤ 会话数,堆 ≤ 会话数,每会话排队 ≤ 32。
- 健康时,同一会话内的并发请求从「并行 1 个往返」变成「N 个往返之和」,单个往返是毫秒级。打开帮会面板时的并发查询【文】`client-access-band-routing.md:350` 会受影响。按 AGENTS §6,上线前要压测对比 prev-summary;没有对比表之前不宣称任何性能结论。

### 4.12 放宽路径(第三阶段,凭数据)

如果压测证明全序的队头阻塞代价不可接受,再引入方法级 option(例如 `OptionClientOrdering { SESSION = 0(默认); DOMAIN; READ_CONCURRENT }`,号避开 410000 / 410001 / 400002),由生成器用 protowire 读取【码】`tools/proto_generator/protogen/internal/prototools/option/value.go:10-83`,产出 lane 分类表。默认值永远是 SESSION:新方法不标注就是全序,忘了标只会慢,不会乱。这一步与 band-routing D51「gate 不认识 Go 方法号」冲突,届时要修订 D51(例如把分类表作为 gate 加载的数据文件)。

---

## 5. 预算链

### 5.1 现状

| 环节 | 值 | 证据 |
|---|---|---|
| 客户端超时 | 15000 | 【文】`friend-client-spec-20260920.md:341-346` |
| gate→路由服 deadline | 8000(本地与 K8s 相同) | 【码】`bin/etc/base_deploy_config.yaml:230`、`tools/scripts/k8s_deploy.ps1:2175-2176` |
| 路由服 zrpc Timeout / ForwardTimeoutMs | 6000 / 5000(所有目标同一个值) | 【码】`go/client_rpc_router/etc/client_rpc_router.yaml:5,32` |
| chat / friend / guild / trade | 4000,各自 Validate 强制 | 【码】`go/chat/internal/config/config.go:113`、`go/friend/internal/config/config.go:281`、`go/guild/internal/config/config.go:430` |
| match(含 team) | **5000 ✗**;team 每个方法自设 3500 ✓ | 【码】`go/match/etc/match_service.yaml:3`、`go/match/internal/team/server.go:21-24` |
| login | **10000 ✗**(注释按直连模式推导:CreatePlayer 持锁后串行三次跨进程调用,各 3s) | 【码】`go/login/etc/login.yaml:7-13` |

**路由模式下 login 的 10s 预算实际不生效。** 路由服用 `context.WithTimeout(l.ctx, ForwardTimeout)` 调目标【码】`forwardlogic.go:165`,deadline 经 gRPC 传播,go-zero 超时拦截器在入站 ctx 上再派生【码】`timeoutinterceptor.go:34`。所以 login handler 实际拿到的预算是 min(10000, 约 5000)。生产形态下 CreatePlayer 的 9s 推导本来就不成立【推,依据 gRPC deadline 传播语义】。

### 5.2 不变式

```
I1  目标 zrpc Timeout + 1000 ≤ ForwardTimeoutMs                        (新增部署门禁)
I2  ForwardTimeoutMs < 路由服 Timeout                                  (现有 Validate【码】config.go:79-93)
I3  路由服 Timeout + 2000 ≤ gate deadline(ClientRpcRouter)             (现有部署门禁【码】k8s_deploy.ps1:948-1000)
I4  gate deadline(ClientRpcRouter) + kMaxLaneWait + 1000 ≤ 客户端超时   (新增;8000 + 5000 + 1000 ≤ 15000 ✓)
```

### 5.3 修法(推荐 B)

- **B(推荐):** login、match 的 `Timeout` 降到 4000。生产形态下它们本来就被截到约 5s,这一步只是把实际预算写对。login 的 CreatePlayer 三个子预算要按 4s 重新分配,由 login 负责人决定数值。`deploy/login-stack.linux/login.yaml` 同步修改。`k8s_deploy.ps1` 新增 `Assert-RouterForwardBudget`,对每个路由目标核对 I1,并补 Pester 用例(含反例)。
- **A(不推荐):** 路由服按目标类型配置 ForwardTimeout(login 11000)。这会把 gate deadline 推到 14000 左右,I4 只能靠把 kMaxLaneWait 降到 0、或者调大客户端超时才能满足。

---

## 6. 业务提交点与栅栏

### 6.1 提交点核实

go-zero 超时拦截器用 `context.WithTimeout` 派生 ctx 交给 handler,超时就取消这个 ctx 并立即返回,handler 的 goroutine 继续跑【码】`timeoutinterceptor.go:34-71`。handler 只要把这个 ctx 传进存储调用,截止之后就不会再发出新命令。但截止前已经发出的那一条(一次 EVAL 或一次 COMMIT)撤不回来,这就是 §6.2–6.4 要兜的剩余窗口。

| 服务 | 请求 ctx 传进存储 | 提交前检查 ctx | 要改的地方 |
|---|---|---|---|
| **match** | **否** | **否** | `queue.go:216`(IsBattleLocked 传 `context.Background()`)、`:221`(无 ctx 的 Hgetall)、`:254`(createTicketIfAbsent 传 Background)、`:301,315,427,443,456,472`(无 ctx 的 Eval)、`:542`(deleteTicketIfOwned 传 Background)【码】;调用点在 `joinqueuelogic.go`、`cancelqueuelogic.go`、`challengelogic.go`、`watchbattlelogic.go`。改为传 `l.ctx`,已有 `createTicketIfAbsentCtx` / `deleteTicketIfOwnedCtx` 可用【码】`queue.go:261,554`;在 createTicket 与 enqueue 之前判 `l.ctx.Err()`;补偿与回滚继续用独立 ctx |
| team | 是 | 是 | ✓【码】`go/match/internal/team/store.go:226-229` |
| login Login | 是 | 隐式(go-redis 截止后不再发命令) | Save 与 SAdd 两步不原子,也不检查关闭状态 → §6.2 |
| login CreatePlayer | 是;补偿用 WithoutCancel | 是(围栏 Lua) | ✓ |
| login EnterGame 后台链 | 独立 background ctx,预算 5min | — | 设计如此【码】`entergamelogic.go:252-253`,由 §6.2 的握手兜住 |
| guild | 是(拦截器套 3500 预算) | 是(`BeginTx(ctx)`) | ✓;缺 expected_guild_id → §6.4 |
| friend / chat / trade | 是 | 是或隐式 | ✓ |

这张表的 match 一行与 go-zero 拦截器语义由主会话复核过;其余各行来自第二轮起草代理的逐服务阅读,未逐行复核。

### 6.2 login 会话栅栏(O12)

**要修的三种竞态:**
- Disconnect 抢跑仍在途的 Login,留下 login_session 和设备集残留;
- EnterGame 已受理、后台链未完成时断线,Disconnect 带 player_id=0 早退,后台链随后写入 ONLINE 而没有租约。这一条顺序正确也会发生;
- EnterGame 之后 LeaveGame:LeaveGame 不拿锁,与后台链并发,后台链在 LeaveGame 之后又写回会话和位置。另外还有同一会话连发 EnterGame(p1)、EnterGame(p2),两条后台链都跑完,同一会话绑了两个角色。

**记录:** `login:sessfence:{gate_instance_id}:{session_incarnation}`,Redis hash,字段 `closed`(0/1)、`gen`(LeaveGame 时递增)、`player`(本会话绑定的玩家)。它和 `login_session:*` 在同一个 Redis client 上,可以写多键 Lua【码】`go/login/internal/logic/pkg/loginsession/loginsession.go:48`。

**为什么要带 gate_instance_id 与 incarnation:** session_id 由 node 段加 17 位序号组成,gate 进程重启后序号从 0 开始【码】`cpp/libs/engine/core/utils/id/node_id_generator.h:49-52`;同 zone 的新进程还可能拿到同一个 node_id。只按 session_id 建键,上一个进程留下的 `closed=1` 会误拒新连接。gate_instance_id 是每次进程启动生成的 UUID;incarnation 在进程内单调、不回绕。incarnation 为 0(旧版 gate)时跳过栅栏,沿用旧行为并计数。

**TTL:** `chainBudget + 1min`,从 `entergamelogic.go:252` 的常量派生。它要比所有还可能读这条记录的操作活得久,最长的是 EnterGame 后台链(5min)。不用 SessionExpireMin(30min):那是登录态的寿命,不是在途操作的寿命。

**操作**(都是单段 Lua):

| 场景 | 位置 | 逻辑 |
|---|---|---|
| Login 提交 | 取代 `loginlogic.go:152-176` 的 Save + TxPipelined | `closed==1` → 拒绝(复用既有码,反正回包也送不到);否则 SET login_session EX、SADD 设备集、EXPIRE。后面的 SCard 判断不变 |
| Disconnect | 取代 `disconnectlogic.go:31` | HSET closed 1 并 EXPIRE;GET+DEL login_session;HGET player 得到 P。在 Lua 之外 SREM 设备集,这是安全的:closed 之后不会再有新的 SADD。要标记断线的玩家取会话里的 player_id,为 0 时取 P → `markPlayerSessionDisconnecting` |
| EnterGame 入口 | `entergamelogic.go:90` 之后 | closed → 拒绝;player 非 0 且 ≠ 本次 player_id → 拒绝(复用 kLoginInProgress);记下 g0 = gen |
| 后台链绑定 | 会话持久化成功之后、`SendBindSessionToGate` 之前(`entergamelogic.go:706-715`) | closed → 补 `markPlayerSessionDisconnecting(P, sid)` 后中止;gen ≠ g0 或 player 已是别人 → `deletePlayerSession`(MarkOffline CAS)后中止;否则 HSET player P |
| 后台链进场前 | `SceneManagerClient.EnterScene` 之前(`:739`) | 再查一次 closed / gen,有变化就跳过 EnterScene 并中止 |
| LeaveGame | `leavegamelogic.go:33-36` | HINCRBY gen;HGET+HDEL player;清 login_session;再走原有的 DeleteSession |

**正确性:** Disconnect 是「先写 closed,再读 player」,后台链绑定是「先查 closed,再写 player」,两段都是同一 Redis 上的原子 Lua。关闭在前,绑定必然看到 closed,由后台链补发 SetDisconnecting;绑定在前,关闭必然读到 P,由 Disconnect 补发。两种顺序都收敛到「带租约的 DISCONNECTING」,现有 player_locator 的 session+version CAS 继续做最终仲裁【码】`go/player_locator/internal/logic/setsessionlogic.go:175-201`、`setdisconnectinglogic.go:35-107`。

**与 [scene-enter-session-fence.md](./scene-enter-session-fence.md) 的关系:** 那份文档按「登录链」铸造 `login_generation`,裁决的是旧会话的入场请求晚到 scene、覆盖新会话的问题。它回答「哪一次登录是当前合法的」,回答不了「发起这次登录的 gate 连接是否已经关闭」。本节的栅栏按「gate 连接」(gate_instance_id + session_incarnation)记录关闭与离开,两者互补,不是同一份真相的两个副本。实施批次 D 时,后台链里的两个检查点放在铸造代次与会话持久化之后,不改变那份文档定下的铸造顺序;那一批改动同样在改 `entergamelogic.go`,批次 D 要等它落地后再动手。

**这是本方案里最复杂的一块,需要 login 负责人评审。** 测试用 miniredis 覆盖「关闭在前 / 绑定在前」两种顺序、关闭之后的 Login 被拒、LeaveGame 推进 gen、同一会话的第二个角色被拒。

### 6.3 match(O13)

- JoinQueue 在 createTicket 之前用 `playercontract.LoadSession` 复核:`state == ONLINE`,且 `session_id`、`gate_instance_id` 与 SessionDetails 一致;不一致回既有码 `ErrNotInScene`。SharedRedis 与 MatchRedis 是两个库,这一步**不原子**,只能缩小跨会话窗口,剩下的由票据 TTL 和 GetQueueStatus 权威查询兜底。
- 不在 CancelQueue 上加墓碑。§6.1 改完之后,晚落地的 createTicket 只会留下「queued 但不在队列」的孤儿票,下一次 Join、整队开战或活动开战时由 healOrphanTicket 删掉【码】`joinqueuelogic.go:259-269`;晚落地的 enqueue 只留下没有票的残留项,matcher 会摘掉【码】`matcher.go:517-526`。压测证明剩余窗口确实存在时再加墓碑。
- **客户端约定(建议写进客户端规范):** CancelQueue 带上 JoinQueue 回包里的 `queue_ticket`;JoinQueue 结果未知(1003 / 超时)时,先调 GetQueueStatus 拿权威状态和票号,再决定是否取消。这条数据依赖在超时、重连、跨 gate 时都成立,是 lane 之外最便宜的一道防线。

### 6.4 guild expected_guild_id(O14)

`LeaveGuildRequest`、`DisbandGuildRequest`、`KickGuildMemberRequest`、`TransferGuildLeaderRequest`、`SetGuildMemberRoleRequest` 各加一个 `expected_guild_id`,字段号按各消息下一个空闲号【码】`proto/guild/guild.proto:78-128`。0 表示旧客户端,行为不变,并计数;非 0 时作为 guildID 传进 repo 事务,不一致回 `ErrNotInGuild` 加当前快照,不写入。这是客户端可见的 proto 改动,两个版本加客户端同批;它防的是「卡住的 LeaveGuild 恢复后把玩家从新帮会移出」(概率低),所以排在最后一批。

---

## 7. 本方案不管的顺序,以及顺带发现的缺陷

### 7.1 边界

| 顺序 | 现状 | 归属 |
|---|---|---|
| 同会话 scene TCP 消息之间 | 已经有序:单连接 FIFO,gate 与 scene 都单线程同步处理【码】`cpp/nodes/scene/handler/rpc/scene_handler.cpp:386-591` | 不改 |
| TCP(scene)与 gRPC(路由)之间,例如 JoinTeam 之后紧接 TravelToZone | 无顺序 | 各自在提交点复核,例如 scene 的 InBattleComp 复查【码】`player_lifecycle.cpp:3994-4000`;TravelToZone 的队伍缓存过期属于 scene 侧问题 |
| RPC 回包、Kafka 推送、scene 推送三条下行之间 | 无顺序 | 业务版本号(team 的 membership_epoch/version),或推送只提示客户端去拉(guild、friend);参考项目原则 5-A / 5-B |
| chat 消息的显示顺序 | 按处理实例的墙钟排序【码】`go/chat/internal/logic/chat_logic.go:208,302-306` | lane 修不了实例时钟偏差;改用权威存储的单调序(例如按频道 INCR) |

### 7.2 顺带发现的缺陷(建议各自立任务)

1. 直连模式下 match 回包全部丢失(match 不回写会话头)【码】`go/match/match_service.go:346-350`、`cpp/nodes/gate/main.cpp:292-293`。
2. `login_session` 键只用 uint32 session_id,gate 重启后可能与旧进程的会话撞号,存在继承别人登录态的风险【码】`loginsession.go:38-40`。
3. gate 从不在 LeaveGame 之后清 playerId,同一连接换角色时,新角色 BindSession 到达之前的请求仍以旧角色身份执行【码】`gate_event_handler.cpp:78,211`、`leavegamelogic.go:25-39`。Java 靠 login 应答里的 SessionDirective 同步解绑。
4. DonateToGuild / BuyGuildShopGoods 没有客户端幂等键,超时重发会扣两次【码】`go/guild/internal/logic/economy_logic.go:742-746`。这是重试问题,不是顺序问题。
5. RefreshToken 一次性消费,回包丢失后重试必然失败【码】`go/login/internal/logic/pkg/token/token.go:145-159`。
6. CQ 轮询遇到 `!ok` 直接 `return`,跳过本轮剩余的完成事件【码】`grpc_init_client.cpp:365-368`。本文的本地截止能兜住,根治属于生成模板的问题。
7. 镜像 CreateScene 在途时没有登记 PlayerSceneChangeInFlightComp;SortBag 没有判冻结【码】`player_scene.cpp:288-303`、`player_bag_handler.cpp:56-69`。
8. (批次 B 实施时发现)WatchBattle 的 `addObserver` 如果恰好在请求截止那一刻还在途,battle 侧可能已经挂上观众,match 却回了失败、删了观战标记。之后玩家排队开战时,gather 靠观战标记找不到这场观战,也就不会清退,玩家可能同时是 A 场的观众和新一局的参战者。批次 B 把这个窗口从「最长约 9s」缩到「截止那一刻在途的那一次 RPC」,但没有消除它。根治办法:RPC 失败且结果未知时,用独立 ctx 补发一次 `removeObserver`。它会给本玩家推一次 SpectateEnd,所以要先和 battle 侧确认语义再做。

---

## 8. 被否决的方案

| 方案 | 不做的理由 |
|---|---|
| 只靠客户端协议约束(按功能单飞、Cancel 必带票号),服务端不排队 | 写进规范的约束不等于实现,team 客户端的单飞守卫文档里登记为「全部未做」【文】`team-system.md:2229`;按功能单飞覆盖不到跨功能因果。它作为补充保留(§6.3 的客户端约定),不作为唯一防线 |
| 路由服按 hash(session_id) 粘性 + 路由服内排队 | 违背 D29;成员变化时要排空、重排;路由服被黑洞时,粘性让一个会话的请求全部卡住;gate 本来就持有会话状态 |
| 按 player_id 一致性哈希到业务实例 + 进程内 per-player mailbox | 每个服务都要做亲和路由与迁移时的停写/激活交接;路由服不透明转发(D30),拿不到 player_id;业务权威状态已在共享存储里 |
| gate 打会话序号,业务按 (player, 对象) 水位拒绝旧请求,作为主方案 | 每个互逆对都要在业务里落一份水位,还必须与提交写在同一段 Lua 或同一个事务里。作为主方案太重;但 O5 在准入时分配的 `gate_call_seq` 为以后按需落地留好了口子 |
| 按服务分域开 lane(Java 现状)作为第一阶段 | 要靠人工标注跨域屏障方法,忘标就静默乱序;team 和 match 同进程、共用票据却分在两条 lane,丢了因果却换不来隔离;它是 §4.12 放宽路径的目标形态之一,但不是默认 |
| 1003 之后静置一段时间再放行下一条 | 没有数据支撑,而且每次失败后都要多等;等压测有证据再说 |

### 8.1 业界做法对照(选型依据)

业界保证「同一实体的操作按序处理、同时可以水平扩展」的做法,核心都是同一条:**每个 key 只有一个处理者,处理者串行;状态层再用版本号 / 栅栏 / 幂等兜住超时与故障切换。** 常见的落地形态有三种:

| 形态 | 例子 | 适用 | 本仓 |
|---|---|---|---|
| 接入层按会话串行 | 本仓 Java 版 gate、常见游戏网关 | 业务服务无状态,状态在共享 Redis / MySQL | Go 服务正是这种,**本文补的就是这一层** |
| 按实体分片的 Actor(单激活、邮箱串行) | Orleans、Akka Cluster Sharding、BigWorld BaseApp | 实体状态常驻内存 | scene 已经是这种(玩家实体只在一个 scene 线程上),所以 TCP 路径本来有序 |
| 按 key 分区的日志 | Kafka 分区 + 每分区单消费者 | 异步事件 | 推送、存档已经这样用(AGENTS §7.3) |

三种形态都绕不开状态层的版本号 / 栅栏和幂等:超时与故障切换会打破任何传输层顺序。本文的第 3 层就是这一部分,会话代次栅栏与 Kafka 幂等生产者的「producer epoch + 序号」思路相同。

本仓选「接入层按会话串行」,理由:

- 它本身就是按 key 分区、单一所有者的结构。一个玩家只有一条连接、连着一个 gate,gate 就是这个玩家请求流的唯一所有者;加 gate 就是加分区,不需要哈希,也不需要再均衡。gate 挂了,连接随之断开,所有权自然结束。路由服与业务服保持无状态、随机分配。
- 把 Go 服务改成按玩家分片的 Actor,每个服务都要做分片协调和交接(旧节点先停写、新节点再激活),而这些服务的状态本来就在共享存储里。更关键的是,Actor 只保证同一个 Actor 内部的顺序:玩家先取消匹配、再接受帮会历练,两条请求落在两个服务的两个 Actor 上,彼此仍然没有顺序,最后还是要回到接入层保序。

---

## 9. 与既有决策的关系

| 决策 | 关系 | 需要修订的条文 |
|---|---|---|
| client-rpc-router D29 / D33 | 不变:路由服无状态、随机 | 无 |
| client-rpc-router D30 | 不变:gate 不解析 body | §3 转发契约补一句:「路由服按原值回写 `x-session-detail-bin` 是 gate lane 认领的依赖契约」 |
| client-rpc-router §6 / microservice-zone-contract §3 | 预算链 | 加入 I1–I4,写明 login、match 的 Timeout |
| client-access-band-routing D51 | **兼容**:单 lane 不需要方法知识,只需要知道「这条消息走路由服」 | 第三阶段放宽时再修订(§4.12) |
| client-access-band-routing D57 | 合并 | 「每会话在途 ≤ 32」改为「排队 + 在途 ≤ 32」;按次记账的键由 `(ClientContext*, seq)` 改为 `gate_call_seq`。理由:生成的 Send* 返回 void,gate 拿不到 ctx 指针【码】`cpp/generated/grpc_client/client_rpc_router/client_rpc_router_grpc_client.cpp:67-89` |
| client-access-band-routing D60 | 照用 7 号字段 | 无 |
| client-access-band-routing §12.4 | 「惰性清理、不挂定时器」 | 改为「主动扫描(TimerTaskComp,回调不捕获对象)+ 惰性清理」 |
| client-access-band-routing 阶段 0b / 0d / 0g | 0b(player_id 归一)、0d(失败回执带 id)并入本方案批次 C;0g(节点摘除排空)与本方案兼容:0g 落地后,晚到的真实完成会因 extract 取不到而被丢弃 | 在阶段表里注明 0b、0d 由本文批次 C 承担 |
| grpc-client-deadline-failure-callback | 失败桥行为变化 | 失败回执改为带 id 的 1003 信封 |
| xuanming-port-decisions D-12 | **兼容**:lane 只在路由模式启用,C++ 默认值和现有单测都不改 | 无 |
| AGENTS §4 字段类型清单 | 新增两个 uint64 计数字段 | 是否登记进清单,由用户决定 |

---

## 10. Java 版对齐(AGENTS §12)

| 维度 | Java 现状 | 与本文的差异 | Java 要补 |
|---|---|---|---|
| lane 粒度 | login 与 scene 共用一条 pending 队列和一个 inFlight 位;6 个后端域各一条队列,每条一个 backendInFlight【码】`game-gate/.../ClientSession.java:93-102`、`ClientDispatcher.java:347-359,413-439` | 本文对路由类请求全序;C++ 的 scene 走 TCP、不进 lane | 取决于决策 Q1:推荐把 6 个后端域队列合成一条会话后端 lane(login+scene 队列不变,移动消息不受影响,现有测试「好友调用在途不阻塞 login 与 scene」仍成立) |
| 排队等待上限 | 入队与 drain 都不看入队时刻【码】`ClientDispatcher.java:417-438` | 本文 5s | 补上 |
| 超限 | 后端队列满 64 断连【码】`ClientDispatcher.java:417-426` | 本文回 1008 带 id | 统一为 1008 带 id(决策 Q1 一并定) |
| 失败回执 | 后端失败回带 id 的 1003【码】`:478-484`;**login 失败推 23 号 tip,不带 id**【码】`:533-536` | 本文全部带 id | login 失败改为带 id 的信封 |
| 截止 | gate 不向 `ClientMessageService.handle` 下传截止;match 从受理时刻起算 4500;`Deadline.await` 撤不回已发出的 Redis 命令,Redisson 超时会原样重发 EVAL【码】`game-common/.../Deadline.java:43-47` | 同样的剩余窗口 | handle 下传 gate 的绝对截止,后端从中派生预算、提交前检查;写脚本的重发不得越过截止;修正 `ClientMessageService.java:30` 的注释(超时窗口下「后端不需要并发控制」不成立) |
| 断线 | 丢弃未发出项;不等后端调用;等在途 login 调用完成后再通知 login【码】`:375-379,471-477,523-531` | 本文立即通知 + login 栅栏 | 可以保留等待;Java login 是否存在与 EnterGame 后台链同类的竞态,**待核实** |
| 跨会话 | JoinQueue 要求位置为 ONLINE,不校验会话身份 | 同 §6.3 | 用 SessionContext 的会话号与 gate 实例比对 |
| 重定向 | 丢弃排队项、停止 drain【码】`:433-434,1058-1062` | 本文对齐 Java | 无 |

`PARITY.md`:更新第 31 行「gate 接入与路由」,并新增一行「客户端请求按会话保序」,登记两版各自的批次状态。本仓 `docs/PROGRESS.md` 的条目要注明对应的 Java 版本号。

---

## 11. 需要拍板的决策

| # | 问题 | 选项 | 推荐 |
|---|---|---|---|
| Q1 | lane 粒度与两版对齐 | (a) 两版都对路由类 / 后端类请求全序(Java 把 6 个后端域合成一条);(b) 两版都按域分 lane,外加生成的「跨域屏障」方法类,team 并入 match;(c) C++ 全序、Java 维持按域,PARITY 登记差异 | **(a)**。理由见 §0 第 4 条;(c) 会让同一个客户端在两版上看到不同的顺序语义 |
| Q2 | 预算链 | B:login、match 的 Timeout 降到 4000;A:路由服按类型配置 ForwardTimeout | **B**。生产形态下本来就被截到约 5s;A 与 I4 冲突 |
| Q3 | 实施授权与顺序 | 约 60 个文件,跨两个仓库 | 按 §12 的批次顺序,每批独立上线、独立回滚 |
| Q4 | 客户端行为(问客户端负责人) | 正式客户端会不会不等回包连发有依赖的请求?路由模式下是否按 id 精确配对回包?CancelQueue 是否带 queue_ticket? | 不影响本方案的正确性,但决定严重程度和 1008 的实际出现频率 |

**拍板记录(2026-10-14,用户):**
- 用户要求「最标准、可以水平扩展的做法」,不在意改动量,并要求记录到文档后开始实施。
- Q1 按 (a) 执行:两个版本都对会话内的路由类 / 后端类请求全序,Java 把 6 个后端域队列合成一条会话后端 lane。
- Q2 按 B 执行:login、match 的 zrpc Timeout 降到 4000。
- Q3 已授权,按 §12 的批次顺序实施。
- Q4 仍待客户端负责人回答,不阻塞实施。
- 业界做法的对照与选型理由补在 §8.1。

---

## 12. 分批实施清单

每批都能单独上线、单独回滚。

| 批次 | 内容 | 主要文件(约数) | 生成物 | 客户端契约 | 回滚 |
|---|---|---|---|---|---|
| **A 预算链** | O10:login、match 的 Timeout 改为 4000,重分 CreatePlayer 子预算;部署门禁 I1;文档 | `go/login/etc/login.yaml`、`deploy/login-stack.linux/login.yaml`、`go/match/etc/match_service.yaml`、login 子预算相关配置与注释、`tools/scripts/k8s_deploy.ps1`、`tools/scripts/tests/k8s_deploy_contract.tests.ps1`、`client-rpc-router.md`、`microservice-zone-contract-20260914.md`(约 8) | 无 | 无 | 回退 yaml |
| **B match ctx** | O11 中 match 那一行 | `go/match/internal/logic/{queue.go,joinqueuelogic.go,cancelqueuelogic.go,challengelogic.go,watchbattlelogic.go}` 及测试(约 8) | 无 | 无 | 回退镜像 |
| **C gate lane** | §4 全部:SessionDetails 7/8、incarnation、player_id 归一、失败回执带 id、准入与排队等待上限、主动扫描、节点摘除快速失败、重定向、汇总日志 | `proto/common/base/session.proto`、`cpp/libs/services/gate/session/comp/{router_lane.h(新),session_info_comp.h}`、`session/manager/session_manager.h`、`cpp/nodes/gate/handler/rpc/{client_request_lane.h,client_request_lane.cpp(新),client_message_processor.h,client_message_processor.cpp}`、`cpp/nodes/gate/main.cpp`、`cpp/nodes/gate/handler/event/gate_event_handler.cpp`、`gate.vcxproj` / `.filters` / `CMakeLists.txt`、`cpp/nodes/gate/tests/router_lane_test.cpp(新)`(约 14) | session.pb(C++、Go、generated 副本;Java 按 ContractSync) | **是**(§4.7) | 回退 gate 镜像(换 gate 要踢在线玩家)。新字段对旧二进制只是未知字段 |
| **D login 栅栏** | §6.2;依赖 C 提供 incarnation,incarnation 为 0 时跳过 | `go/login/internal/logic/pkg/sessionfence/{fence.go,fence_test.go}(新)`、`loginlogic.go`、`disconnectlogic.go`、`leavegamelogic.go`、`entergamelogic.go`、`session_lifecycle.go` 及测试(约 9) | 无 | 无(复用既有码) | 回退镜像;栅栏记录 6 分钟后自然过期 |
| **E1 match 会话复核** | §6.3 | 约 3 | 无 | 无 | 回退 |
| **E2 guild expected_guild_id** | §6.4 | 约 5,外加客户端仓 | guild.pb 各语言 | **是**,改客户端仓需 AGENTS §9 授权 | 字段为 0 时兼容 |
| **F Java** | §10 | `ClientDispatcher.java`、`ClientDispatcherTest.java`、`ClientSession.java`、`ClientMessageService.java`、match 预算相关、`application.yaml`、`PARITY.md`(约 8) | 按 Java AGENTS 执行 ContractSync | 与 C、E2 同批 | 按 Java 仓规范 |
| **R 冒烟** | 新 robot 场景:不等回包流水线连发 JoinQueue→CancelQueue、ApplyJoinGuild→CancelGuildApplication、AcceptFriend→RemoveFriend、Login→EnterGame,断言最终状态与发送顺序一致 | `robot/ordering_smoke_scenario.go`、`robot/etc/ordering_smoke.yaml`(约 2) | 无 | 无 | — |
| 文档 | 本文定稿;§9 列出的修订;`docs/PROGRESS.md` | 约 5 | — | — | — |

**推荐顺序:** A、B(改动小、互相独立,先关掉超时窗口)→ C → D → R → E1 → E2;F 与 C 同批。

---

## 13. 给 Codex 的验证命令

工作目录:仓库根 `D:\luyuan\xuanming-server-mmo-cpp`。按顺序执行,任何一步红了就停,保留失败日志摘要。

1. **proto 重生成(批次 C、E2):** 按 AGENTS §4 执行 `cd go && build.bat`(脚本结尾有 `pause`,非交互执行时用等价的 `go/build.ps1`)。期望:`go/proto/common/base/session.pb.go` 与 C++ 的 `session.pb.*` 更新,其余生成物无差异。
2. **C++(批次 C):** `msbuild game.sln /m:1 /p:Configuration=Debug /p:Platform=x64`,**必须串行 `/m:1`**,通过标准是 0 error。SessionDetails 变了,所有节点都要重编。`router_lane_test` 照 `cpp/nodes/gate/tests/gate_security_test.cpp` 的方式独立编译运行,全绿。覆盖:满额拒绝;lane 内 FIFO;同时只有 1 条在途;Complete 恰好一次、旧 seq 被忽略;incarnation 不符被丢弃;排队超时不发出;同步失败时 Pump 循环会终止;断线返回在途 seq;过期堆惰性删除;重定向之后拒绝新请求。
3. **Go:** 分别在 `go/match`、`go/login`(以及 E2 时的 `go/guild`)目录执行 `go build ./... && go vet ./... && go test ./... -count=1`,全绿。login 栅栏用 miniredis 跑 §6.2 列出的用例;match 用例覆盖 ctx 取消后不再建票。
4. **部署门禁(批次 A):** `pwsh -NoProfile -Command "Invoke-Pester -Path tools/scripts/tests/k8s_deploy_contract.tests.ps1 -Output Detailed"`。期望新增的 I1 用例通过,并且反例(login Timeout 配成 10000)被拒。
5. **冒烟(批次 C、D、R):** 先按 AGENTS §6.2 清空 Redis / MySQL / etcd 与 Kafka offset,确认路由模式开启(`GATE_CLIENT_RPC_ROUTER=1`)。在 `robot/` 下依次跑 team / friend / guild / chat / battle / trade 现有冒烟,再跑 `ordering_smoke`。失败时保留:robot 输出、gate 的 `[lane]` 汇总行、路由服日志、login 与 match 日志。
6. **性能(批次 C 上线前):** 按 AGENTS §6 先保存上一轮的 `prev-summary.txt`,再压测,用 `stress_summarize.ps1` 出对比表。没有对比表之前,不宣称延迟变化的结论。
7. **Java(批次 F):** 按 Java 仓 AGENTS 的构建与测试口径执行,ContractSync 之后 clean install。

---

## 14. 开放问题

1. 正式客户端的行为(Q4)。
2. 在 pending 状态下析构 grpc::CompletionQueue 会不会崩溃(F29),本轮没有验证,属于阶段 0g。
3. Java login 是否存在与 C++ EnterGame 后台链同类的「已受理、未完成即断线」竞态。
4. §6.1 表中 match 以外各行是起草代理逐服务读代码的结论,主会话没有逐行复核;实施时对每个服务补一条「ctx 取消后不再提交」的单测作为证据。
5. 工作区里 `client_message_processor.cpp`、`main.cpp` 等文件有别的会话的未提交改动,实施前要先分清归属。登录代次那一批(scene-enter-session-fence.md)正在改 login 的 EnterGame 链与 gate 的会话转发,批次 C、D 要排在它之后,并在它的基础上改。

---

## 15. 评审裁决记录

第二轮只有「替代方案」视角的代理评审完成;「故障边界」「业务栅栏」「兼容上线」三个视角因账号额度中断,由主会话按同样的问题清单读代码复核。

### 15.1 替代方案视角(代理评审,verdict = sound-with-fixes)

| 级别 | 意见 | 裁决 |
|---|---|---|
| major | team 与 match 同进程、共用 MatchRedis 票据,按节点类型分两条 lane 会丢因果(CancelQueue→StartTeamMatch 假拒绝;JoinQueue 与 StartTeamMatch 并发时 healOrphanTicket 误删) | **采纳**。第一阶段改为每会话一条 lane(O3),自然覆盖 |
| major | 帮会历练凑齐开战会在同一请求里调 match,与 CancelQueue 跨 lane 乱序会让整个房间失败 | **采纳**。单 lane 覆盖;屏障分类推迟到 §4.12 放宽阶段 |
| major | 排队等待时间没有计入预算,队尾可能在客户端 15s 超时之后才发出 | **采纳**。新增 `kMaxLaneWait` 与不变式 I4(O6、§5.2) |
| major | 否决 (d)、(e) 的理由不成立;序号应在准入时分配 | **采纳**。(e) 升为第一阶段方案;`gate_call_seq` 改在准入时分配,允许以后做水位栅栏,只禁止当幂等键(§4.8、§8) |
| major | lane 与客户端「按功能单飞」高度重叠,而批次 C 改动面最大、还改变客户端可见行为;建议先做服务端栅栏,并采用 Cancel 带票号的数据依赖 | **部分采纳**。批次顺序改为 A、B 先行;客户端约定写进 §6.3。lane 仍然要做:规范约束不等于实现,而且单 lane 能覆盖按功能单飞覆盖不到的跨功能因果(§8 第一行) |
| minor | 重定向时没有处理 lane,与 Java 不一致 | **采纳**(O9) |
| minor | gate 在 LeaveGame 之后不清 playerId | **采纳为相关缺陷**(§7.2 第 3 条),不在 lane 范围内 |
| minor | 「extent 对齐 lane(message_id >> 8)」把语义藏进号段布局 | **采纳**。单 lane 方案不再需要它,已删除 |
| minor | 域内读写全部串行的代价被低估 | **部分采纳**。用排队等待上限限制降级时的放大;放宽要凭压测数据(§4.11、§4.12) |

### 15.2 主会话复核(代替未完成的三个视角)

| 视角 | 复核项 | 结论 |
|---|---|---|
| 业务栅栏 | go-zero 超时拦截器取消的是不是 handler 拿到的那个 ctx | 是:`context.WithTimeout(ctx, t)` 派生后传给 handler,`ctx.Done()` 时返回错误,handler goroutine 继续跑【码】`timeoutinterceptor.go:34-71`。§6.1 的论断成立,剩余窗口限于截止前已发出的那一条命令 |
| 业务栅栏 | match 票据读写是否用了 `context.Background()` | 是【码】`queue.go:216,221,254,301,315,427,443,456,472,542` |
| 业务栅栏 | login 的超时预算 | `Timeout: 10000`,注释按直连模式推导【码】`login.yaml:5-13`;路由模式下被转发 deadline 截断【推】 |
| 业务栅栏 | guild 历练开战是否在同一请求里调 match | 是【码】`activity_trial.go:613-618` |
| 故障边界 | 失败桥能拿到什么 | 发出的 metadata 与请求副本;今天回的是不带 id 的 tip【码】`main.cpp:325-350` |
| 故障边界 | 直连模式能否按回显认领 | 不能:match 不回写会话头【码】`match_service.go:346-350`。因此 O7 只在路由模式启用 |
| 故障边界 | session_id 复用、晚到回包、5ms 批内多个完成 | 由 incarnation 核对与 `extract(seq)` 恰好一次处理;每个完成只 Pump 自己的会话,批内不需要跨会话排序(§4.5) |
| 故障边界 | 排队期间 BindSession 才到达 | 取入队快照,请求按未登录被拒;今天带的是 UINT64_MAX,归一之后更安全(§4.3) |
| 兼容上线 | 与 D-12 冲突吗 | 不冲突:只在路由模式启用,C++ 默认值和单测不改 |
| 兼容上线 | 与 D51 冲突吗 | 第一阶段不冲突(单 lane 不需要方法知识);§4.12 放宽时才需要修订 |
| 兼容上线 | 是否加运行时开关 | 不加。lane 关闭态就是要修的缺陷态,两条路径都得测;回滚靠回退 gate 镜像 |
| 兼容上线 | 1003 / 1008 是否存在、需不需要新 tip | 都存在【码】`common_error_tip.proto:18,28`,不新增 tip |

---

## 16. 实施进度

只追加,不改旧行。每批的「验证」一栏只写 Codex 实际跑出的结果;没跑就写「待 Codex」。

| 批次 | 状态 | 改动文件 | 验证 |
|---|---|---|---|
| A 预算链 | 进行中(2026-10-14) | — | 待 Codex |
| B match ctx | 未开始 | — | — |
| C gate lane | 等待:登录代次那一批正在改 `session_info_comp.h`、`gate_event_handler.cpp`,对方落地后通知 | — | — |
| D login 栅栏 | 等待:登录代次那一批正在改 `entergamelogic.go` | — | — |
| E1 / E2 / F / R | 未开始 | — | — |
| A 预算链 | 已改完(2026-10-14) | `go/login/etc/login.yaml`、`deploy/login-stack.linux/login.yaml`(Timeout 10000→4000)、`go/match/etc/match_service.yaml`(5000→4000)、`tools/scripts/k8s_deploy.ps1`(新增 `Get-RouterForwardBudgetViolations` / `Assert-RouterForwardBudget`,挂在写路径入口)、`tools/scripts/tests/k8s_deploy_contract.tests.ps1`(3 条用例)、`docs/design/team-system.md`(§A.3 第 7 条更正注)、`microservice-zone-contract-20260914.md`、`client-rpc-router.md` | 待 Codex(§13 第 4 步) |
| B match ctx | 已改完(2026-10-14) | `go/match/internal/logic/{queue.go,joinqueuelogic.go,cancelqueuelogic.go,challengelogic.go,watchbattlelogic.go,spectate.go,spectate_test.go}`、新增 `request_deadline_fence_test.go`。要点:客户端路径的提交点(建票 / 入队 / 删票 / 出队 / 挑战占坑与写记录 / 应战决定点 / 观战标记 / addObserver)用请求 ctx 或先检查 ctx;JoinQueue 入队失败的补偿由无条件 DEL 改为按 ticket id CAS 删;Hmset 之后的 Expire、各类回滚仍用独立 ctx。已核实 go-redis v9.17.3 在取连接前检查 ctx(`internal/pool/pool.go:502,682-688`),ctx 已取消时命令不会发出 | 待 Codex(§13 第 3 步,`go/match`) |
| E1 match 会话复核 | 已改完(2026-10-14) | `go/match/internal/logic/joinqueuelogic.go`(`callerSessionStale`)、`request_deadline_fence_test.go`(4 个子用例)。已核实 login 写进 player_locator 的 session_id / gate_instance_id 取自 gate 的 SessionDetails(`go/login/internal/logic/clientplayerlogin/entergamelogic.go:556-558`) | 待 Codex |
| C gate lane(第 1 部分) | 已写好、**未接线**(2026-10-14) | 新文件(都不在 gate.vcxproj / CMakeLists 里,不参与编译):`cpp/libs/services/gate/session/comp/router_lane.h`(纯逻辑:RouterLane / InflightIndex / QueueWaitExceeded)、`router_lane_item.h`(排队项)、`cpp/nodes/gate/tests/router_lane_test.cpp`(独立单测 12 例)、`cpp/nodes/gate/handler/rpc/client_request_lane.{h,cpp}`(装配:Admit / OnRouterReply / OnRouterCallFailed / OnRouterNodeRemoved / ExpireDue / OnSessionClosing / OnSessionRedirected / NextIncarnation / SetRouterSender)。`proto/common/base/session.proto` 加 `session_incarnation = 7`、`gate_call_seq = 8`(纯追加,对旧二进制只是未知字段) | 待 Codex:router_lane_test 可先跑(§13 第 2 步的独立编译命令) |
| C gate lane(第 2 部分:接线) | **等待**:登录代次那一批正在改 `session_info_comp.h`、`gate_event_handler.cpp`,对方落地后通知 | 必须一次做完(只做一半会让 gate 编不过):① `session_info_comp.h` include `router_lane_item.h`,加 `gate_lane::SessionRouterLane routerLane;`、`uint64_t incarnation{0};`;② `client_message_processor.cpp`:include `client_request_lane.h`;`HandleConnectionEstablished` 置 `session.incarnation = gate_lane::NextIncarnation()`;`HandleRouterForward` 改为 `gate_lane::Admit(sessionId, session, request, BuildSessionDetails(...))`,删掉那里的 `loginStarted` 置位(已移进 Pump 的发出时刻);新增 `SendRawViaRouter(request, bin) -> optional<uint64_t>`(SendViaRouter 改为调用它,断线通知照旧走 SendViaRouter,seq = 0)并在构造函数里 `gate_lane::SetRouterSender`;`HandleConnectionDisconnection` 在 `sessions.erase` 之前 `gate_lane::OnSessionClosing`;`OnNodeRemoveEventHandler` 对 `ClientRpcRouterNodeService` 调 `gate_lane::OnRouterNodeRemoved(pb.entity())`;③ `main.cpp`:成功桥在拿到 sd 之后先 `if (gate_lane::OnRouterReply(*sd, reply)) return;`,失败桥解析出 sessionDetails 之后先 `if (gate_lane::OnRouterCallFailed(sessionDetails)) return;`;GateRuntimeContext 加 `TimerTaskComp routerLaneExpiryTimer`,与 sceneEntryRetryTimer 同处 `RunEvery(gate_lane::kSweepIntervalSeconds, []{ gate_lane::ExpireDue(std::chrono::steady_clock::now()); })`;④ `gate_event_handler.cpp` 的 `RedirectToGateEventHandler` 调 `gate_lane::OnSessionRedirected`;⑤ `gate.vcxproj` / `gate.vcxproj.filters` / `CMakeLists.txt` 登记 `handler/rpc/client_request_lane.{h,cpp}` | — |
| R 冒烟 | 已写好(2026-10-14) | `robot/ordering_smoke_scenario.go`(新)、`robot/etc/ordering_smoke.yaml`(新)、`robot/main.go`(分派)、`robot/config/config.go`(模式校验)。批次 C 接线之前**预期会间歇失败**,之后必须每轮通过 | 待 Codex(§13 第 5 步加 `.\robot.exe -c etc/ordering_smoke.yaml`) |
| F Java(第 1 部分) | 已改完(2026-10-14,Java 仓未提交) | `game-gate/.../ClientSession.java`、`ClientDispatcher.java`(6 个后端域队列合成一条会话后端 lane)、`ClientDispatcherTest.java`(两条「跨域并发」用例改写为新语义、两处字段引用)、`PARITY.md`(新增一行)、`docs/design/architecture.md`(§4.13 更正注)。线上形状不变 | 待 Java 仓构建测试(`mvn -pl game-gate -am test`,以 Java AGENTS 为准) |
| F Java(第 2 部分) | 未开始,与批次 C 接线同批 | 排队等待上限 5s、lane 满回 1008 不断连、login 失败改带 id 的信封、handle 下传截止、JoinQueue 复核会话 | — |
| C gate lane(第 2 部分:接线) | 已改完(2026-10-15) | `session_info_comp.h`(追加 `incarnation`、`routerLane`,不碰 loginGeneration)、`client_message_processor.cpp`(`SendRawViaRouter` + 注入、`Admit`、建连分配代次、断线 `OnSessionClosing`、节点摘除 `OnRouterNodeRemoved`)、`main.cpp`(成功桥 / 失败桥先交给 lane;`routerLaneExpiryTimer`)、`gate_event_handler.cpp`(重定向 `OnSessionRedirected`,写在守护段内)、`cpp/nodes/gate/{gate.vcxproj,gate.vcxproj.filters,CMakeLists.txt}`、`cpp/libs/services/gate/{gate.vcxproj,gate.vcxproj.filters}`(只登记头文件) | 待 Codex:proto 重生成后 `msbuild game.sln /m:1` |
| C 与 muduo 传输的协调 | 进行中(2026-10-15) | 「C++ gRPC 迁移至 muduo HTTP/2」会话同时在改 gate 到路由服的发送与回包桥(`router_muduo_forward.*`、`rpc_reply_bridge.*`)。已发消息建议:lane 的两个钩子挪进它的 `gate_rpc_reply_bridge`,让两种传输都保序;muduo 分支接进 `SendRawViaRouter` 内部,签名与返回值不变 | — |
| D login 会话栅栏 | 已改完(2026-10-15) | 新包 `go/login/internal/logic/pkg/sessionfence/{fence.go,fence_test.go}`(CommitLogin / Close / CheckEnterGame / BindPlayer / Recheck / Leave,单测 11 例);`loginsession.go` 导出 `Key` / `TTL`;`loginlogic.go`(有代次时原子提交)、`disconnectlogic.go`(Close + 用绑定玩家补挂租约)、`leavegamelogic.go`(Leave + 用绑定玩家删会话)、`entergamelogic.go`(入口 CheckEnterGame;persist 之后 `bindSessionFence`;EnterScene 前 Recheck;`chainBudget` 提为包级常量 `enterGameChainBudget`,登录代次铸造顺序不变)、新测试 `session_fence_wiring_test.go`(5 例)。旧版 gate(incarnation = 0)全部跳过,行为不变 | 待 Codex:`cd go/login && go build ./... && go vet ./... && go test ./... -count=1` |
| F Java(第 2 部分:gate 侧) | 已改完(2026-10-15,Java 仓未提交) | `ClientDispatcher.java`(`MAX_BACKEND_LANE_DEPTH = 32` 满回 1008 不断连;`MAX_BACKEND_LANE_WAIT_NANOS` 5s 排队超时回 1003;login 失败改带 id 的信封)、`ClientSession.java`(BackendPending 带入队时刻)、`GateMetrics.java`(新增 `LANE_FULL`、`QUEUE_EXPIRED`)、`ClientDispatcherTest.java`(改写 1 例、新增 2 例)、`PARITY.md` 行状态 | 待 Java 仓构建测试 |
| F Java(第 3 部分) | 未开始 | `ClientMessageService.handle` 下传 gate 截止、后端提交前检查;game-match JoinQueue 复核会话身份 | — |
| 文档修订 | 已改(2026-10-15) | `client-access-band-routing.md` §12.4 修订注、`grpc-client-deadline-failure-callback.md` #8 修订注;此前已改 `client-rpc-router.md`、`microservice-zone-contract-20260914.md`、`team-system.md` | — |
| E2 guild expected_guild_id | 未开始 | 客户端可见的 proto 改动,两版加客户端仓同批,需要客户端仓授权(AGENTS §9) | — |
| C 回包桥 | 已合(2026-10-15,muduo 会话合入) | 成功 / 失败回包统一由 `cpp/nodes/gate/handler/rpc/rpc_reply_bridge.{h,cpp}` 认领:`DeliverReply` 第一行 `gate_lane::OnRouterReply`,`NotifyCallFailure` 采样日志之后 `gate_lane::OnRouterCallFailed`;官方 gRPC 与 muduo_h2 两种传输都走它。`SendRawViaRouter` 签名不变,内部多了 muduo 分支 | 待 Codex |
| F Java(第 3 部分,部分) | 已改(2026-10-15,Java 仓未提交) | game-match `QueueService.join(SessionContext, …)` 跨会话栅栏:在线目录条目存在、且会话号或 gate 实例不同就拒(16020);条目缺席放行,因为 gate 进场后才异步写入。`QueueHandlers` 改调新重载;`FakePlayerStatus` 导出 `GATE_INSTANCE` / `sessionIdOf`,`QueueHandlersTest`、`MatchClientMessageServiceTest` 的会话按它构造。**还没加**「旧会话被拒」的专门用例;`handle` 下传截止、Redisson 超时重发 EVAL 仍待评估 | 待 Java 仓构建测试 |
| proto 重生成 | 合并到登录代次那一批的同一次重生成 | 由 docs/PROGRESS.md 里登录代次批次的 Codex 步骤一并产出 SessionDetails 7、8 号字段,不另跑一遍 | — |
| F Java(第 3 部分) | 已改完(2026-10-15,Java 仓未提交) | game-match:`QueueHandlersTest` 新增 3 例(旧会话号被拒、换 gate 实例被拒、目录未写放行);`RedissonTicketStore` 新增 `CLIENT_WRITE_SCRIPT`(`OptionalOptions.defaults().codec(StringCodec).retryAttempts(0)`),157 的 enqueue / createMatched 与 148 的 cancel 改用它,截止前发出的写不再被 Redisson 在 gate 放行下一条之后原样重发(原 retry-attempts = 1,单条最坏约 4.2 s)。`handle` 下传截止不做:各后端自身预算已小于 gate 的 5 s | 待 Java 仓构建测试 |
