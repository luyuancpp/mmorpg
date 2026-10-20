<!-- 只读分析代理产出（2026-10-16，接手会话工作流 wf_f7177272-747），原样保存供落码时查证；结论未经运行验证。 -->

**item**: 进场链余项 c：gRPC ReleasePlayer 带预期 owner_epoch（交接 §3.3；出处 scene-enter-session-fence.md §7 末行、§9 第 6 条；cross-zone-scene-travel.md M6 与 1762 行残余）
**current_behavior**: 【所有调用方】全仓只有一条生产调用链，均为 HEAD 已提交、工作区无在途改动：
1. go/scene_manager/internal/logic/enterscenelogic.go:723-727（第 5b 步）。条件是 `currentLoc != nil && currentLoc.NodeId != "" && !samePhysicalNode`，在可走到的路径上等价于 crossNodeHandoff（:523-528）。它排在换手门（:532-544）和预占人数（:675-697）之后、第 6 步落点（:730-753，铸造 epoch 并 CAS 写 location）之前，也在第 7 步路由（:761-784，Kafka broker ACK 才是提交点）之前。
   - 进入条件一（生产口径）：过了 requireHandoffCommitted。checkHandoffCommitted（owner_epoch.go:557-574）在 `marker.Epoch == observedEpoch` 时放行，落点随后铸造 E→E+1。
   - 进入条件二：dev 旁路 AllowUnsafeCrossNodeHandoff=true。无标记也放行，且不铸造。
   - 跨 zone 重定向不发释放（enterscenelogic.go:1046 注释，handleCrossZoneRedirect）。死节点接管和 zone 下线时会把 currentLoc 置 nil（:313-337），也不发。
2. dispatchReleasePlayer（scene_node_client.go:333-375）：safego 后台协程。首发 1s deadline，失败后按 1s、3s 退避再试两次，每次 1s deadline，最坏约 7s 才送达；全部失败后靠 AFK 兜底。只对传输错误重试，结局计入 metrics.ObserveReleasePlayer（metrics.go:179-192、642-647）。
3. RequestNodeReleasePlayer（scene_node_client.go:150-166）：请求只填 player_id、target_scene_id、target_node_id。
4. 其它端：C++ 只有生成的发送登记 SendSceneNodeGrpcReleasePlayer（cpp/generated/rpc/service_metadata/rpc_event_registry.cpp:266），没有手写调用点。client_rpc_router 路由表里该方法为 ClientProtocol=false（go/client_rpc_router/generated/pb/game/route_table.go:211），客户端不可达。Java 仓只有 proto 副本（game-proto/src/main/proto/proto/scene_manager/scene_node_service.proto），用 git grep 查过，无调用方也无实现。

【调用方能拿到的 epoch】
- observedEpoch：enterscenelogic.go:296，本请求对 owner_epoch 键的唯一一次观察。换手门比对、落点 CAS 的期望值（ARGV[1]）、同落点重连的下发值都用它。
- 凭标记放行时 observedEpoch == 标记 epoch，而标记 epoch 就是源实体写标记那一刻的 PlayerOwnerEpochComp.epoch（player_lifecycle.cpp:5285-5307、5364，BeginTravelHandoff 在 epoch==0 时直接放弃）。
- 第 6 步之后还有 placed.epoch：铸造时为 E+1，dev 旁路不铸造时仍为 E。
- currentLoc.OwnerEpoch 是 location 里记录的值，键被单独淘汰时会与键值不一致，不宜使用。

【proto】proto/scene_manager/scene_node_service.proto
- :16-20 是 rpc 与注释，:41-47 是 ReleasePlayerRequest{player_id=1, target_scene_id=2, target_node_id=3}，响应为 Empty，消息号 128。这几行都是 HEAD 已提交。
- 该文件的在途改动只有 :37-38 新增的 `rpc AssetAckReceipt(AssetOpRequest)`（交易托管释放场景收据），与 ReleasePlayer 无关，归属会话未核实。其生成物（cpp/generated/proto/scene_manager/*、go/proto/scene_manager/*）在工作区已重生。

【scene 侧处理链】
1. cpp/nodes/scene/handler/grpc/scene_node_service.cpp:337-362：生成的 gRPC 包装。在 gRPC 线程上 runInLoop 投递到节点 EventLoop，再 future.get() 等待，status 恒为 OK。HEAD 已提交；本文件在途改动只是 :257-262、:499-524 的 AssetAckReceipt 骨架。
2. 同文件 :149-206 的 HandleReleasePlayer（守护段）：
   - playerList 里找不到 → DEBUG 日志后幂等返回（:153-159）；
   - PlayerLifecycleSystem::IsHandoffRequested 为真 → 忽略（:190-198）。该函数在 player_lifecycle.cpp:5147-5155，判据是交接组件在且 requestedAtMs!=0；
   - 否则调 HandleExitGameNode(entity, ExitCause::kReleasedByTransfer)（:204）。
3. HandleExitGameNode（player_lifecycle.cpp:2964 起，工作区行号；该文件有 MySQL 围栏 save_seq 的在途改动，与本项无关）：
   - 未在退出中：挂 UnregisterPlayer 与退出意图，进入退出存盘并销毁。A1′ 对 released_by_transfer 不写释放标记（exit_release_mark.h:79-83 的 kSkipCause）。
   - 已在退出中：只做 MergeExitCause；生产口径不粘性压制（M6）；ShouldRekickExhaustedExit 成立时补存一次。
4. 源 scene 的 epoch 缓存是 PlayerOwnerEpochComp.epoch（player_ownership_comp.h:101-106；工作区 :104-105 的 lastSaveSeq 是 MySQL 围栏在途改动）。只有两个写入口：路由取 max，以及 ResolveTravelOutcome 的 B5 采纳。B5 在 player_lifecycle.cpp:5721 先执行 `epoch = max(epoch, E+2)`，随后 :5725 才 AbortTravelHandoff 解冻并摘交接组件。同一 loop 线程上不存在“交接已摘、epoch 仍是 E”的瞬间。B4（kUnchanged，:5672-5681）解冻时 epoch 不变。
5. 测试替身：go/scene_manager/internal/logic/integration_test.go:90-93 的 fakeSceneNode.ReleasePlayer 只计数、丢弃请求。
**problem_confirmed**: True
**problem_analysis**: 结论：问题存在，未被别处修掉。证据有四条：
- proto 仍只有 3 个字段，scene_node_service.proto:41-47；
- scene_node_service.cpp:185-189 自述为“已登记的残余，根治要让 ReleasePlayerRequest 携带预期 owner_epoch”；
- cross-zone-scene-travel.md:1762 把它列为设计残余；
- 全仓 grep 不到 expected_owner_epoch。

【主时序：交接被单调回滚后迟到释放（B5）】前提：生产口径（旁路关），玩家 P 在节点 X，缓存 epoch=E，要换到节点 Y 上的场景。
1. X 发 EnterScene，被 18 暂拒。X 随后 StartTravelHandoff：冻结 → 存盘落地 → 写标记 "E:t"，requestedAtMs 置位，IsHandoffRequested 从此为真 → 带标记重发 EnterScene。
2. SM 在 :296 观察到 E，换手门放行（marker.Epoch==E），在 :725 异步派发 ReleasePlayer（只带 player_id）。首发失败，原因可能是 X 的 gRPC 池忙、1s 超时或拨号失败，于是进入 1s、3s 退避重试，最晚约 7s 后才送达。
3. SM 第 6 步铸 E+1，location 指向 Y。
4. 第 7 步 Kafka 写超时（KafkaWriteTimeoutSeconds 默认 5s，config.go:154）。SM 执行 rollbackEnterSceneAfterRouteFailure：location 退回 X，epoch 再 INCR 到 E+2，写回执 "E:t" 并转写标记，然后回 ErrKafkaRoute。
5. 约 5-8s 时 X 收到应答，ResolveTravelOutcome 原子取证得到 B5：epoch 采纳为 E+2（:5721）→ AbortTravelHandoff 解冻 → 强制存盘。此刻 IsHandoffRequested 变为假。
6. 迟到的重试 ReleasePlayer 到达 X：playerList 能找到 P，交接守卫已不拦，于是 HandleExitGameNode(kReleasedByTransfer)。实体以 E+2 存盘（CAS 合法，数据不丢）后销毁。
后果：location 是 X@E+2，gate 会话仍绑在 X，但 X 上已没有实体，玩家消息全部落空，会话变哑，只能重登。重登时走同落点重连会重新载入。归属不变量没被破坏，伤的是活性。

【变体 B：落点或路由失败后 epoch 不动（B4）】第 5b 步先派发，第 6 步再落点。落点 Lua 可能返回：
- errHandoffWithdrawn：源端已取证撤回；
- errOwnerEpochConflict；
- Redis 错误。
任一种都会让 SM 回错误，X 取证得到 B4，epoch 仍为 E，随即解冻。迟到（或在 withdrawn 情形下首发即晚于解冻）的 ReleasePlayer 到达后同样退掉实体，location 仍指 X。
关键点：此时 X 的缓存 E 恰好等于 SM 当时观察到的 E，**单靠“带预期 epoch、相等才释放”挡不住这一类**。必须让 SM 只在本请求提交之后才派发。

【变体 C：回流到源节点】E→E+1 换到 Y 成功，X 收到应答后销毁实体；ReleasePlayer 仍在重试。约 6s 内 P 又从 Y 换回 X：铸 E+2，X 新载入一个缓存为 E+2 的实体。迟到的释放会退掉这个新实体，location 指 X，会话变哑。带预期 epoch=E 即可挡住。

【变体 D：dev 旁路】不铸造。路由失败时走 keep 回滚，epoch 不变，迟到释放照样退掉实体。只影响 dev，属旁路自身声明的不安全范围。

补充：生产口径下，“活着且不在交接中的实体收到 ReleasePlayer”只会出现在交接已收口（B4/B5）之后或回流之后。成功路径里，释放到达时源实体要么仍在交接中被忽略，要么已销毁。cross-zone-scene-travel.md:1878 也写明生产可达性未证实。也就是说，生产上这条通知除了造成上述伤害之外几乎总是 no-op。
## design_options
  - [0]
    **name**: A 只加 expected_owner_epoch，scene 相等才释放（出处原文方向）
    **description**: ReleasePlayerRequest 加 uint64 expected_owner_epoch=4，SM 填 observedEpoch，派发位置不动（仍在第 5b 步）。scene 依次判：在交接中则忽略；期望值为 0 时按旧行为释放并计数；缓存为 0 时按旧行为释放并计数；相等才释放；其余忽略。
    **pros**: 改动最小：proto 加 1 个字段，Go 和 C++ 各改几行。能挡住主时序 B5（E+2≠E）和变体 C（回流后缓存为 E+2）。纯判定，不读 Redis，符合 CZ-3。
    **cons**: 挡不住变体 B（B4 后 epoch 仍为 E，与期望值相等），也挡不住 dev 旁路的 keep 回滚。另外，落点或路由失败时仍会白发一次释放 RPC。
  - [1]
    **name**: B（推荐）A + SM 把派发点挪到本请求提交点之后
    **description**: 在 A 的基础上，把 enterscenelogic.go:701-728 的派发整体移到第 7 步 if/else 之后、`if takenOverLoc != nil`（:788）之前。这样带 GateId 的请求要等路由 broker ACK 成功才派发，不带 GateId 的请求要等第 6 步落点成功才派发，两种情形共用这一个插入点。期望值仍取 observedEpoch：落点 Lua 已用它做过 CAS，保证提交那一刻旧节点持有的恰好是这一代。
    **pros**: 落点失败和路由失败（含回滚）都不再发释放，变体 B 整类消失，dev 旁路的 keep 回滚也一并消失。成功之后的迟到释放（变体 C）由 epoch 栅栏挡住。重试因此变成按代际幂等。热路径无新增延迟（本来就是后台协程）。
    **cons**: SM 行为有变化，需要用户确认。dev 旁路下旧节点开始释放存盘的时刻推后约一个 Kafka 写时延，新节点读到旧快照的窗口略增（只影响 dev，旁路本身就声明不安全）。与余项 d 同改 EnterScene 主函数，有文本冲突。
  - [2]
    **name**: C 带新铸 epoch，scene 在 缓存 < 新值 时才释放
    **description**: 字段语义改成“取代者 epoch”（placed.epoch），派发必须挪到铸造之后，scene 判 cached < new 才释放。
    **pros**: 语义上是“已被更新一代取代才退出”。配合派发后移，覆盖面与 B 相同。
    **cons**: dev 旁路不铸造，new==cached，永远不会释放，必须特判。缓存为 0 时同样要特判。与设计文档的“预期 epoch”口径不一致，评审成本更高。不推荐。
  - [3]
    **name**: D 生产口径不再发 ReleasePlayer，只在 dev 旁路发
    **description**: 凭标记放行的跨节点交接直接跳过派发。
    **pros**: 生产上彻底消除迟到释放（按上文分析，生产上释放几乎总是 no-op）。
    **cons**: 放弃了标记在而实体仍在的未证实边角（例如 A1′ 写标记后退出被取消，未核实）上的那次“补一刀”，语义改动更大；dev 旁路仍然需要 A。可作为 B 的后续收紧，不建议本次做。
  - [4]
    **name**: E scene 收到后 GET location，仍指向本节点就忽略（M6 可选缓解）
    **description**: 非交接中的实体先读 Redis 里的 location 再决定。
    **pros**: 不用改 proto。
    **cons**: 现状下释放早于落点 Lua 到达，读到的必然仍指向本节点，dev 旁路的释放会全部失效；与 GO-2 回滚窗口有取舍（M6 原文）；违背“节点不自己读 Redis 取归属”（CZ-3），还多一次 RTT。否决。
  - [5]
    **name**: F（可与 A/B 叠加）响应改成 ReleasePlayerResponse{outcome}
    **description**: Empty 换成新消息，例如 enum ReleasePlayerOutcome{UNSPECIFIED=0, RELEASED, NOT_FOUND, HANDOFF_IN_FLIGHT, STALE_EPOCH, AHEAD_OF_HOLDER}，gRPC status 恒为 OK（同资产通道口径）。
    **pros**: SM 能按结局计数，Go 测试也能直接断言结局。线上编码与 Empty 兼容：旧 scene 回空字节会解成 outcome=0。
    **cons**: 生成签名会变：C++ 的 HandleReleasePlayer 要加 response 参数，Go 客户端返回类型也变，改动面比 A/B 大。SM 对“忽略”与“已释放”没有不同的处置动作，属 YAGNI。scene 侧计数已足够观测。
**recommended**: 推荐方案 B：A 的字段与比对规则，加上 SM 把派发点挪到提交之后。响应保持 Empty，结局只在 scene 侧计数。

理由：
1. 只加字段只能修主时序和回流，修不了 B4 那一类：epoch 不动时“期望 == 缓存”恒真，scene 无法区分。把派发移到提交之后，B4 和路由回滚这两类根本不会发释放。剩下的“提交成功后迟到”由 epoch 栅栏精确挡住。两者合起来才算闭合。
2. 期望值取 observedEpoch，三个值相互一致：它是落点 CAS 的期望值，也是换手门比对的标记 epoch，又是源实体写标记时的缓存值。比对用 scene 本地缓存，不读 Redis，不新增采纳入口，满足 CZ-3 和 player_ownership_comp.h 文件头“唯一的非路由写入口”约束。
3. 响应用 Empty 时，所有结局都回 OK。释放是通知，忽略就是终态，重试也不会改变结论。tip 码是客户端可见数轴，不能用于内部结局。改成非 OK status 又会被 SM 的重试循环当成传输错误，白白重试并计为 error。

scene 判定表：新增纯函数 release_fence::Decide(expected, cached, handoffRequested)，按以下顺序判，全部零副作用：
1. handoffRequested 为真 → kSkipHandoffInFlight。现有守卫，语义与日志不变。
2. expected == 0 → kReleaseUnversioned。旧 SM 或未铸造：照旧释放并计数，SM 滚完后应恒为 0。
3. cached == 0（含组件缺失）→ kReleaseCacheUnknown。按 owner_epoch 的“0 = 未知，跳过校验并计数，不许静默”口径，照旧释放，WARN 并计数。不选 fail-closed 的原因：缓存为 0 的实体存盘不带 guard，留着它反而会用旧状态覆盖 Redis 和 MySQL。
4. cached == expected → kRelease，即现行为。
5. cached > expected → kSkipStaleRelease。期望值小于缓存：实体已被回滚采纳（E+2）或已在更新的一代重新载入，忽略，WARN 并计数。这正是本项要挡的。
6. cached < expected（两者都非 0）→ kSkipAheadOfHolder。期望值大于缓存：生产口径下构造不出来，因为标记 epoch 就是缓存值，而缓存只增不减；只在 dev 旁路或键异常时出现。退出是破坏性动作，所以忽略并 WARN 计数。这样的实体是陈旧实体，下一次带 guard 的存盘会被 CAS 拒掉，进而按废黜流程自毁，或由 DiscardStaleHandoffEntity、AFK 收掉，不会写坏数据。
判为 1、5、6 时不调 HandleExitGameNode，也不做 MergeExitCause 或 rekick。

调用方：
- 期望值取 observedEpoch，首发和重试都带同一个值；
- 重试策略不变（只重试传输错误，1s/3s 退避、每次 1s，预算约 7s），因为迟到已经无害；
- 日志带上 expected_owner_epoch。
**contract_changes**: 1. proto/scene_manager/scene_node_service.proto
   - message ReleasePlayerRequest 新增 `uint64 expected_owner_epoch = 4;`。类型为 uint64：owner_epoch 是 64 位单调计数，与 PlayerLocation.owner_epoch、RoutePlayerEvent 同类型。编号 4 未用过，也不在 reserved 中，只增不改。
   - 字段注释写明：
     - 取值 = SM 本次 EnterScene 观察到并用于落点 CAS 的 owner_epoch（即旧节点在提交时刻应持有的那一代）；
     - 0 = 旧版调用方或尚未铸造，scene 按旧行为释放并计数 release_unversioned；
     - scene 只在本地缓存 PlayerOwnerEpochComp.epoch 与它相等时释放；缓存为 0 时按旧行为释放并计数；其余忽略；
     - 不进存盘，不与 login_generation 混用。
   - 同步改 rpc 注释（:16-19）：交接中的实体不处理，按预期 epoch 比对；SM 只在本次 EnterScene 提交（路由 broker ACK，或无 GateId 时落点成功）之后才发。
   - 响应保持 `google.protobuf.Empty`（proto/common/base/empty.proto），RPC 签名不变，消息号 128 不变。
   - 只动 ReleasePlayerRequest 和那段 rpc 注释，不碰工作区在途的 AssetAckReceipt 两行（:37-38）。
2. Go 函数签名（scene_manager 内部，非跨服务契约）：
   - `RequestNodeReleasePlayer(ctx, svcCtx, zoneId, nodeId, playerId, targetSceneId, targetNodeId, expectedOwnerEpoch uint64) error`
   - `dispatchReleasePlayer(svcCtx, log, oldZoneID, oldNodeId, playerId, targetSceneId, targetNodeId, expectedOwnerEpoch uint64)`
3. Lua 脚本：不改。luaMintEpochAndSetLocation、回滚 Lua、A2′ 跨语言金样都不动。
4. Redis 键值：不新增，也不改格式（owner_epoch、handoff、location 原样）。
5. C++ 库内新增接口：`static void PlayerLifecycleSystem::HandleReleaseFromSceneManager(Guid playerId, uint64_t expectedOwnerEpoch, uint64_t targetSceneId, const std::string& targetNodeId)`（名字可议）。另有纯判定头 release_fence.h：`enum class Verdict : uint8_t {kRelease, kReleaseUnversioned, kReleaseCacheUnknown, kSkipHandoffInFlight, kSkipStaleRelease, kSkipAheadOfHolder, kCount}` + 平铺名表 + static_assert（AGENTS §11.2，不用 X-macro）。
6. tip 错误码：不新增。内部结局不走 tip 数轴。
**code_changes**: （以下为设计，本任务只读，未改任何文件。行号为工作区现状。）

1. proto/scene_manager/scene_node_service.proto:41-47：按上节加字段 4 并改注释。由 Codex 用 no-unity 配置重生（`dev_tools.ps1 -Command proto-gen-run -UseBinary -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml`）。会一并重生在途的 AssetAckReceipt 产物，需与该改动的归属方协调合入顺序。

2. 新文件 cpp/libs/services/scene/player/system/release_fence.h，照 enter_fence.h 的形状：
   - 只含 Verdict 枚举、名表、constexpr Decide(expected, cached, handoffRequested)、Releases(verdict)，以及 release_fence_stats（verdicts[] 原子计数、Snapshot、Read）；
   - 不含 entt、redis、protobuf；
   - 在 cpp/libs/services/scene/scene.vcxproj 与 .filters 登记 ClInclude（这两个文件工作区无在途改动）。

3. cpp/libs/services/scene/player/system/player_lifecycle.h/.cpp：新增 HandleReleaseFromSceneManager，把 scene_node_service.cpp:152-204 的逻辑搬进库，便于 cross_zone_test 覆盖。顺序如下：
   - playerList 查找，找不到则 DEBUG 后返回；
   - 读 IsHandoffRequested；
   - 用 try_get 读 PlayerOwnerEpochComp（只读，组件缺失视为 0）；
   - 调 release_fence::Decide 并计数；
   - 判为 skip 时打 WARN（带 expected、cached、target），直接返回；
   - 判为释放时调 HandleExitGameNode(entity, ExitCause::kReleasedByTransfer)。
   注意：这两个文件都有 MySQL 围栏 save_seq 的在途改动（player_lifecycle.cpp 的 :755-766、:4128-4133、:4367-4381 等），新函数要放在不相交的位置，例如紧挨 IsHandoffRequested（.cpp:5147、.h:1243-1245），不碰 PlayerOwnerEpochComp 的定义。

4. cpp/nodes/scene/handler/grpc/scene_node_service.cpp:149-206（守护段内）：
   - 函数体改为一行委托：`PlayerLifecycleSystem::HandleReleaseFromSceneManager(request->player_id(), request->expected_owner_epoch(), request->target_scene_id(), request->target_node_id());`
   - 注释 :161-189 中“交接中不走退出”的理由保留，并移到库函数旁；:185-189 的“已登记的残余”改写为本次的栅栏说明。
   - 响应仍是 Empty，生成签名不变，不触及守护段外。

5. cpp/libs/services/scene/core/system/redis.cpp:127 之后，紧跟 [EnterFence] 汇总行，加一行 `[ReleaseFence]` 汇总（key=value、累计值、有变化才打）。skip_stale_release 或 skip_ahead_of_holder 有增长时升 WARN；release_unversioned 与 release_cache_unknown 在全链升级后应恒为 0。

6. go/scene_manager/internal/logic/scene_node_client.go：
   - :150-166 加参数并填 `ExpectedOwnerEpoch: expectedOwnerEpoch`；
   - :333-375 加参数，首发和重试都传同一个值；Errorf/Infof 带上 expected_owner_epoch；
   - 文件头注释写明重试按代际幂等。

7. go/scene_manager/internal/logic/enterscenelogic.go：
   - 删掉 :723-728 的派发，把 :701-722 的注释改写后挪到第 7 步之后（:784 的 else 分支结束之后、:788 `if takenOverLoc != nil` 之前）；
   - 条件不变：`currentLoc != nil && currentLoc.NodeId != "" && !samePhysicalNode`，调用 `dispatchReleasePlayer(..., observedEpoch)`；
   - 第 7 步路由失败的 return（:770-779）与第 6 步各失败 return（:733-753）之前都不再派发；
   - EnterSceneStageReleaseDispatch 指标随之移动；
   - 注释说明期望值为何取 observedEpoch（落点 CAS 已证实它是提交时刻的当前代）。
   若要求可确定地测“未派发”，引入包级钩子 `var releaseDispatcher = dispatchReleasePlayer`，与现有的 nodeDialer、nodeEndpointOverride 同一做法。

8. go/scene_manager/internal/logic/integration_test.go:90-93：fakeSceneNode 记录最后一次和全部 ReleasePlayerRequest（加锁或用 atomic.Pointer）。

9. 文档：
   - scene-enter-session-fence.md §7 末行与 §9 第 6 条标注“c 已设计/落地”，或新增一节“余项 c”写判定表、派发时机与残余；
   - cross-zone-scene-travel.md:535（M6）、:1762（残余）加批注；
   - docs/PROGRESS.md 追加条目（注明 Java 状态）；
   - Java 仓 PARITY.md 登记一行：不适用，Java 内部无 ReleasePlayer 实现与调用方，归属靠库行 owner_epoch 与交出原语。
**rollout_compat**: 字段语义与默认值：0 表示旧调用方或未知。proto3 下旧 SM 不填即为 0。新 scene 收到 0 时按旧行为释放并计数 release_unversioned，行为与现状逐字节一致（含交接中忽略）。

升级顺序：先 scene，后 scene_manager。
- scene 先升：旧 SM 发 0，全部走 kReleaseUnversioned，无行为变化。用这一窗口确认 [ReleaseFence] 计数正常。
- SM 滚动期间：新旧副本并存。旧副本仍在第 5b 步早派发、带 0（旧行为）；新副本在提交后派发、带期望值（新行为）。两者针对同一玩家互不干扰，因为每个请求只派发自己的释放。新副本的释放到旧 scene（若 scene 没先升）时，未知字段 4 被忽略，按旧行为释放：缺陷照旧，但不会更糟。
- 无持久化格式、Redis 键、Lua 变更，没有需要迁移的存量数据。不设开关：scene 侧的开关只能在启动时读，而栅栏本身零副作用，开关提供不了额外能力（理由同 scene-enter-session-fence.md §6）。

回滚：
- 只回滚 SM：期望值变回 0，派发回到第 5b 步，scene 按旧行为处理，等于回到现状。
- 只回滚 scene：字段被忽略，等于回到现状。
- 两者都回滚：现状。任意顺序都安全。

观测口径：
- release_unversioned 在 SM 滚完后应恒为 0；
- release_cache_unknown 应恒为 0（非 0 说明有实体缓存 epoch 为 0，与 owner_epoch_unknown 同源）；
- skip_stale_release 非 0 说明确实挡下了迟到释放（预期罕见），可对照 SM 的 release_player_total{outcome=retry_ok|timeout|error}；
- skip_ahead_of_holder 在生产上应恒为 0，非 0 时排查 dev 旁路是否误开、owner_epoch 键是否被淘汰后补种。

dev 旁路（AllowUnsafeCrossNodeHandoff=true）下，派发推后一个 Kafka 写时延，旁路本来就有的“新节点读到旧快照”窗口相应略增。只影响 dev，需在设计文档写明。
**invariants_check**: 逐条对照交接 §3.3 的四条不可改不变量：

1. 入口栅栏必须是第一步，且零副作用：本项不改 PlayerEnterGameNode、HandleEnterGameNode、JudgeEnter 及其绑定复核，入场栅栏的位置与纯度不受影响。ReleasePlayer 新增的 release_fence::Decide 照同一原则执行：查到实体之后、任何副作用之前做纯判定，被忽略的请求不碰会话、pending、票据、待确认条目和退出意图（不调 MergeExitCause，不 rekick）。✔

2. 同一会话一律放行：ReleasePlayer 不携带会话，也不比较会话或 login_generation，不会因会话维度拒绝或放行任何入场；释放与否只看归属代际 owner_epoch，与登录代次的职责分工一致（scene-enter-session-fence.md §3.5）。✔

3. SessionMap 只擦属于本玩家的映射（EraseSessionMappingIfOwned）：本项不新增任何 SessionMap 写入。判为释放时仍走既有 HandleExitGameNode 退出链，退出链对映射的擦除沿用 EraseSessionMappingIfOwned（player_lifecycle.cpp:2424 定义，退出侧调用如 :2858）。本项只减少进入退出链的次数。✔

4. gate 侧 loginGeneration 只经 scene_route_helper.h 的 ApplyLoginBinding / ApplyRouteIdentity 写入，只抬高、不丢路由：本项不改 gate 的任何文件。SM 侧只移动 ReleasePlayer 的派发时机，不改 RoutePlayerEvent 的内容、发送时机与回滚语义，gate 的盖章规则不受影响。✔

另需守住的相关约束：
- CZ-3 与“唯一的非路由写入口”：栅栏只读本地 PlayerOwnerEpochComp.epoch，不读 Redis、不采纳 epoch、不新增写入口。✔
- AGENTS §7 第 4 条：字段编号 4 为新号，只增不复用。✔
- §7 第 5 条（tip）：不新增 tip 码。✔
- ECS 组件访问规则：读路径用 try_get，只读，不懒建组件。✔
- 本项依赖 B5 采纳“先抬 epoch（:5721）、后摘交接组件（:5725）”的顺序。今后重构 ResolveTravelOutcome 必须保持这个顺序，建议在该处加注释并用回归用例钉住。
**tests**: 要求先写能复现的红用例，再改代码。以下用例均未运行，由 Codex 或用户按 AGENTS §10.1 执行。

C++（cpp/tests/cross_zone_test/cross_zone_test.cpp，工作区无在途改动）：
1. ReleaseFence.DecideTableEveryCell：纯函数覆盖 handoff 真/假 × expected{0, E} × cached{0, E-1, E, E+2}，断言六种结论与 Releases() 的取值；外加 NamesCoverEveryValue（名表与 kCount 一致）。
2. ReleaseFenceEcs.LateReleaseAfterRollbackAdoptKeepsEntity（复现主时序）：建实体，epoch=E；构造 B5 之后的状态（epoch=E+2、无交接组件；有现成 B5 取证 harness 时走真实 ResolveTravelOutcome 更好，参考 :2593-2740 的 RolledBackToSelfFacts）。调 HandleReleaseFromSceneManager(pid, E)。断言实体仍在 playerList、无 UnregisterPlayer、无 PlayerExitIntentComp，skip_stale_release 加 1。
   先能复现：先把守护段搬进库函数、但暂不加栅栏（忽略 expected），此用例应为红；加栅栏后转绿。
3. ReleaseFenceEcs.LateReleaseOnReturnedEntityIgnored（变体 C）：新载入实体 epoch=E+2，释放带 E，实体保留。
4. ReleaseFenceEcs.EqualEpochReleases：epoch=E，释放带 E，挂上 UnregisterPlayer，意图原因为 kReleasedByTransfer。
5. ReleaseFenceEcs.UnversionedReleaseKeepsLegacy：期望值为 0 时照旧释放，release_unversioned 加 1。
6. ReleaseFenceEcs.CacheUnknownReleasesAndCounts：缓存为 0、期望值为 E，照旧释放并计数。
7. ReleaseFenceEcs.AheadOfHolderIgnored：缓存 E、期望值 E+1，忽略并计数。
8. ReleaseFenceEcs.HandoffInFlightStillIgnored：交接中，无论期望值为何都忽略（现有守卫的回归）。
9. ReleaseFenceEcs.StaleReleaseOnExitingEntityDoesNotMerge：退出中的实体缓存 E+2、释放带 E，意图的 cause、suppressedBy、resaveRounds 都不变。

Go（go/scene_manager/internal/logic，工作区无在途改动）：
1. TestEnterScene_RouteFailureDoesNotDispatchRelease（复现变体 B/路由回滚，现状为红）：跨节点凭标记放行，fake Kafka 写失败走 bump 回滚，断言 releaseDispatcher 调用次数为 0。用同步钩子保证确定性，不靠 sleep。
2. TestEnterScene_PlacementFailureDoesNotDispatchRelease（现状为红）：分别构造 errHandoffWithdrawn（预检后改写标记）与 errOwnerEpochConflict（预检后 INCR），都不派发。
3. TestEnterScene_ReleaseCarriesObservedOwnerEpoch：成功跨节点交接，钩子收到的期望值等于 observedEpoch（标记 epoch E），而不是 E+1；用 bufconn fake 端到端时，请求的 ExpectedOwnerEpoch 也为 E。
4. TestEnterScene_DevBypassReleaseCarriesUnmintedEpoch：旁路下期望值为 E，且在路由成功后才派发。
5. TestEnterScene_NoGateIdCrossNodeDispatchesAfterPlacement：不带 GateId 时落点成功后派发、带 E；落点失败时不派发。
6. TestDispatchReleasePlayer_RetriesCarrySameExpectedEpoch：fake 前两次返回 Unavailable、第三次成功，三次请求的期望值相同。
7. 回归：logic_test.go:636-661、:818-840、:993-1005 与 owner_epoch_test.go:1068-1127 现有的“不得调用 ReleasePlayer”断言保持通过。

执行步骤（给 Codex 或用户）：
1. 仓库根目录执行 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run -UseBinary -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml`。生成器源码有变化时，先跑 proto-gen-build。
2. `cd go/scene_manager && go test ./internal/logic/... -count=1`。
3. 串行（/m:1）编译 cpp/tests/cross_zone_test/cross_zone_test.vcxproj 和 scene 节点工程，运行 `cross_zone_test --gtest_filter=ReleaseFence*:EnterFence*:*Travel*`。
通过标准：上列新用例全绿，既有用例无回归。失败时保留 gtest 和 go test 的失败段落原文。
**cross_item_dependencies**: - 与 d（落点 Lua 记会话身份）：同改 enterscenelogic.go 的 EnterScene 主函数。c 只移动第 5b 步的派发、不碰 Lua；d 要改落点 Lua 及其调用。语义上互不依赖，但会文本冲突，建议同批落地，或 c 先 d 后，由同一执行者顺序合并。
- 与 b（LeaveScene 条件删除）：无语义依赖，LeaveScene 走另一条链。b 依赖 d，c 不依赖两者。
- 与 a（Kick 带 player_id）、e（ExitGame 带代次、断线带 gate 实例号）：无关。
- 与 MySQL 行内围栏（交接 §3.4）：共享 PlayerOwnerEpochComp。该会话在途给它加了 lastSaveSeq（player_ownership_comp.h:104-105，未提交），并改了 player_lifecycle.cpp 的存盘路径。c 只读 .epoch、不改组件定义；新库函数放在与在途 hunk 不相交的位置，避免合并冲突。
- 与在途 AssetAckReceipt（交易托管，归属会话未核实）：同改 scene_node_service.proto，scene_node_service.cpp/.h 的生成骨架也在途。c 只改 ReleasePlayerRequest 和 HandleReleasePlayer 守护段，重生时会一起带上对方的产物，需确认合入顺序。
- 与 GO-2 / B5 采纳（cross-zone-scene-travel.md §12.8）：c 的正确性依赖 B5 中 epoch 先于解冻抬高（player_lifecycle.cpp:5721 先于 :5725），这是后续重构 ResolveTravelOutcome 不得打破的前提。
- Java 版：ReleasePlayer 属内部 RPC，不在 AGENTS §12 的客户端契约范围内。Java 无实现也无调用方，PARITY 登记为“不适用”。Java 仓的 proto 副本是否同步加字段 4 需确认，加了也无副作用。
**open_questions**: 以下需用户拍板或尚未核实：
1. 【需拍板】是否同意方案 B 的第二部分，即把派发挪到提交点之后？这是 SM 行为变化：落点或路由失败时不再发释放；dev 旁路下释放推后一个 Kafka 写时延。若只做 A，变体 B（B4 后迟到释放）会作为残余留下，需另行登记。
2. 【需拍板】期望值大于缓存时选“忽略”（推荐，破坏性动作要求严格相等）还是“释放”。生产口径下构造不出该情形，只影响 dev 旁路和键异常。
3. 【需拍板】缓存为 0 时选兼容释放（推荐：保持旧行为并计数，因为不带 guard 的实体留着反而可能用旧状态覆盖存档）还是 fail-closed 忽略。
4. 【可选】是否需要 SM 侧按结局计数（方案 F 改响应类型）。当前推荐不改，只在 scene 侧计数。
5. 【未核实】A1′ 写下标记之后，退出是否可能被取消、实体又恢复为活跃且不在交接中？若存在，生产上“活实体只在交接收口后才会收到释放”的论断要收窄；但方案 B 加栅栏后结论不受影响（缓存等于期望值时释放，与现状相同）。
6. 【未核实】生产上 ReleasePlayer 首发失败、走到重试的实际频率：看 scene_manager 的 release_player_total{outcome=retry_ok|timeout|error}，决定告警阈值。
7. 【未核实】工作区 AssetAckReceipt 在途改动的归属会话与合入计划。
8. Java 仓 proto 副本（game-proto/.../scene_node_service.proto）是否随本仓同步。
