# Codex 任务单：第 5 批：生命周期、引用与组件类型整理（2026-10-15，第 1 版）

> **谁执行**：Codex。总规则、批次顺序、组件三类初始化口径、协调与回执格式见 [README.md](README.md)，本单不重复。
> **本版性质**：设计稿加挑战者意见的原样整理，还没有细化到「改前/改后代码片段」。Claude 的额度恢复后会出第 2 版（补丁级）。**先读 §8 挑战意见**：标为 high/medium 的问题，实施时必须按其中的修法修正设计。凡与 README §2 三类初始化规则冲突的地方，一律以 README 为准。
> **行号**：设计时取自当时的 HEAD，此后第 1 批和其他会话都改过代码，实施前请按函数名重新定位。

## 1. 目标

第 5 批:收口 entt 使用里「生命周期、引用与组件类型」这一类 low/medium 问题。一共七件事。(1) dispatcher 上绑定 *this 的 sink 改为 RAII 断开;(2) 组件引用不再跨过同步 trigger 使用;(3) 存裸实体句柄的旁路表随实体正确清理,办法按情况三选一:能从组件推导的删掉改查询,只属于一个实体的改成该实体的组件,只写不读的删掉;(4) 基础类型和 std 别名不再当组件用(Guid→PlayerGuidComp;ScenePlayers、DtCrowdPtr 删除;SessionList 移出 registry),生成器模板同步修改;(5) sceneRegistry 改用 EnTT 自定义实体类型 SceneEntity,在类型上与 actor 句柄隔离;(6) TimerTaskComp 改为可搬移,「包含它的组件必须手写 in_place_delete」这条约定不再需要;(7) 清理死组件和死代码,改正错误注释。执行上分两个阶段:A 阶段不碰第 1 批的文件,可以马上并行做;B 阶段必须等第 1 批合入后再做。

## 2. 采用的标准做法

按主题取 EnTT 3.13.2 的官方惯用法,并与本仓库新版 ecs-component-access-rules.md(三类初始化:持久数据、按需状态、派生数据)保持一致。

【sink 生命周期】成员函数 sink 统一用 entt::scoped_connection 成员持有,随对象析构自动断开。不选「析构里手写 disconnect」,因为 RAII 不会漏、也不怕异常。dispatcher.clear() 和 EcsContext::Clear() 都只丢弃排队中的事件,不会断开 sink(dispatcher.hpp:351-360)。static handler 的 Register/UnRegister 绑定的是自由函数,不存在悬垂,不改。

【trigger 与引用】同步 trigger 的监听者可能增删同一个池的组件(swap-and-pop),之前拿到的组件引用或指针会失效。规则:trigger 之前把需要的标量或句柄拷出来,trigger 之后要用就重新 try_get。

【旁路表】逐个评估了 registry.on_destroy<T>()。entt 销毁实体时按池逆序删除组件,on_destroy<Hex> 触发的那一刻 SceneEntityComp 可能已经不在了。AOI 清理恰好要同时读 Hex、场景句柄和别人的兴趣表,所以不用信号,而是按以下方式三选一:
- ScenePlayers:它是 SceneEntityComp 的反向索引,唯一读者是很少执行的 BeginSceneDrain。删掉它,改为扫 view<SceneEntityComp, 玩家身份组件>,事实源只剩一个。
- AnomalyDetector 的 thread_local map:改成玩家实体上的按需状态组件,随实体自动销毁,不再截断 64 位、也不用手工 ClearPlayer。
- 技能上下文镜像:只写不读,直接删除(F043/F045)。
- AOI 格子和兴趣表:这是跨实体关系,保留「离场是唯一清理出口」。DestroyPlayer 的兜底改为调用 DetachFromScene。离场时再全量清扫一遍仍挂着离场者的远端观察者。Unpin 时如果目标已经超出范围,立即驱逐。

【组件类型】组件必须是语义化类型:
- Guid 改为 struct PlayerGuidComp{Guid value;},生成器模板同批修改。
- 加一个可选护栏:对算术类型偏特化 entt::component_traits 并 static_assert,这是 EnTT 官方的定制点。
- SessionList 是单例和私有索引,不进 registry,改为 EcsContext 的普通成员,和 playerList 并列。
- 剩下的 std 容器别名组件(BuffListComp 等)改成派生出来的具名类型。这样调用点基本不用动,类型身份也唯一了;同时补上从基类的隐式转换构造。

【跨 registry 句柄】按 EnTT 官方的自定义标识符做法:enum class SceneEntity : std::uint64_t {},sceneRegistry 用 entt::basic_registry<SceneEntity>。actor 句柄和场景句柄混用会直接编译失败,F029、F005、F079 一起关掉。ECS_GET_OR_* 宏不用改签名:传 SceneEntity 进去本身就编译不过。

【TimerTaskComp】根因是「移动等于取消」。把可变状态放到堆上的 State 里,闭包只持有 weak_ptr<State>,移动时转移定时器。这样任何搬移(swap-and-pop、compact、容器扩容)都安全,不再需要靠约定手写 in_place_delete。

【死代码】按 YAGNI 删除:scene_crowd/DtCrowdPtr(没有创建点,且用 actor 句柄去查 sceneRegistry)、MovementAccelerationSystem(Acceleration 没有创建点、没有设计文档;proto 消息保留,不动契约)、EventOwner、CreateEntityIfNotExists/RecreateEntity、ErrorEntity/OperatorEntity/itemRegistry、SceneRegistryComp 等 scene_node_comp.h 里的死类型。

【保留不动】
- Npc 标签:它是目标判定里「稀疏可选标签」的正当用法,NPC 生成还没实现,标签不算误用。
- C006 的 AOI 策略组件:缺省即默认值也是正当的稀疏用法,本批只改正设计文档和注释。
- C004:要不要这个功能需要用户拍板,本批只在审计文档标注。

## 3. 覆盖的审计编号

F009、F083、F042、F030、F064、F065、F066、F067、C005(同类卫生修正,原条目已推翻)、F069、F072、F076、F045、F043、F032、F054、F055、F068、F099、F029、F005、F079、F026、F016(EventOwner 与 Create*/Recreate* 部分;to_entity 留给外部 ID 批次)、F019/F020(ErrorEntity/OperatorEntity 部分)、F021(itemRegistry 部分)、C004(只改文档并等待拍板)、C006(保留并修正文档)、Npc/Acceleration 无创建点、SceneRegistryComp 死类型

## 4. 实施步骤（预计改动约 105 个文件）

### 4.1 A1 dispatcher 成员函数 sink 改为 scoped_connection(F009/F083)【A 阶段,可与第 1 批并行】

**文件**：
- `cpp/libs/engine/core/node/system/node/node.h`
- `cpp/libs/engine/core/node/system/node/node.cpp`
- `cpp/nodes/gate/handler/rpc/client_message_processor.h`
- `cpp/nodes/gate/handler/rpc/client_message_processor.cpp`

node.h:在私有成员区末尾加 `entt::scoped_connection serverConnectedConnection_;`,并 #include "entt/src/entt/signal/sigh.hpp"。注释写明三点:sink 绑定了 *this,必须随对象断开;dispatcher.clear()/EcsContext::Clear() 不会断开 sink;成员在 ~Node 函数体(含 fallback drain)执行完之后才析构,所以 drain 期间仍能收到事件。
node.cpp RegisterEventHandlers:改为 `serverConnectedConnection_ = tlsEcs.dispatcher.sink<OnConnected2TcpServerEvent>().connect<&Node::OnServerConnected>(*this);`。node.cpp 是 CRLF,保持原行尾。
client_message_processor.h:同样加 `entt::scoped_connection nodeRemoveConnection_;`。构造函数最后一行改为 `nodeRemoveConnection_ = tlsEcs.dispatcher.sink<OnNodeRemoveEvent>().connect<&RpcClientSessionHandler::OnNodeRemoveEventHandler>(*this);`。
两个类的头文件注释写明前提:对象必须和 tlsEcs 在同一线程构造和析构(RunSimpleNodeMainWithOwnedContext 保证这一点)。
不改各 *EventHandler 的 static Register/UnRegister。
验收 grep:`rg "connect<&[^>]*>\(\*this\)" cpp/libs cpp/nodes` 命中的每一处都赋给了 scoped_connection 成员。

### 4.2 A2 TimerTaskComp 改为可搬移,定时器随对象转移(F042)【A 阶段】

**文件**：
- `cpp/libs/engine/core/time/comp/timer_task_comp.h`
- `cpp/libs/engine/core/time/comp/timer_task_comp.cpp`
- `cpp/tests/timer_queue_unit_test/timer_queue_unit_test.cpp`
- `.github/copilot-instructions.md`

数据结构:私有成员改为 `struct State { TimerId timerId; uint32_t generation = 0; bool armed = false; }; std::shared_ptr<State> state_;`,删除 aliveToken/timerId/generation/armed 这几个成员。OnTimer 改为 static,签名 `static void OnTimer(State&, uint32_t firedGeneration, bool repeating, const TimerCallback&)`。
ScheduleTimer:先 Cancel();cb 为空或当前线程没有 loop 就返回;state_ 懒建;取 gen=state_->generation;闭包捕获 `std::weak_ptr<State>`、gen、repeating、cb,不再捕获 this。开火时 lock() 失败就返回,成功则持有这个 shared_ptr 调用 OnTimer,保证 cb() 里销毁宿主时 State 仍然活着。最后 `state_->timerId = schedule(...); state_->armed = true;`。
OnTimer:逻辑不变(generation 比对;one-shot 先清 timerId 和 armed,再调 cb())。
Cancel():state_ 为空直接返回;loop 非空才调 loop->cancel(state_->timerId);然后清 timerId、armed=false、++generation。IsActive():`state_ && state_->armed`。
拷贝构造:新对象为空,与现在一致。移动构造:`state_(std::move(o.state_))`,定时器随对象转移,不再取消。移动赋值:`if(this!=&o){Cancel(); state_=std::move(o.state_);}`。析构:Cancel()。
TimerTaskComp 自身的 `in_place_delete = true` 保留(指针稳定,已有测试按地址比对),注释改为「可选,不再是正确性前提」。skill_comp.h 和 buff_comp.h 不在本步改(第 1 批文件,B8 再改注释)。
测试:
- MoveAndCopyAreSafe 改成新语义:moved.IsActive()==true,source 和 copy 均为 false;
- 新增 MovedArmedTimerFiresOnceIntoNewOwner(移动后跑 loop,回调恰好触发一次);
- 新增 MoveAssignCancelsDestinationOwnTimer(目标原定时器的回调永不触发);
- 新增 SwapAndPopWithoutInPlaceDeleteKeepsSurvivorTimer:测试内定义 `struct NoPinComp{TimerTaskComp timer;};`,两个实体各挂一份并武装,删掉先插入的那个,幸存者 IsActive 且回调恰好触发一次;
- 新增 MovedFromObjectDestructionDoesNotCancel;
- ComponentStaysSmall 上界不变(现在只剩一个 shared_ptr),更新注释。
copilot-instructions 的 TimerTaskComp 一节补一句:移动会转移定时器,ECS 搬移安全;Cancel 遇到空 loop 的规则不变。

### 4.3 A3 AnomalyDetector 旁路表改为玩家组件(F030/F064)【A 阶段】

**文件**：
- `cpp/libs/modules/transaction_log/anomaly_detector.h`
- `cpp/libs/modules/transaction_log/anomaly_detector.cpp`
- `cpp/tests/currency_test/anomaly_detector_test.cpp(新)`
- `cpp/tests/currency_test/currency_test.vcxproj`

组件定义:在 anomaly_detector.h 的类前公开定义 `struct PlayerAnomalyWindowComp { std::unordered_map<uint32_t, PlayerAnomalyBucket> currencies; std::unordered_map<uint32_t, PlayerAnomalyBucket> items; };`,取代私有的 PlayerBuckets。注释写明:按需状态,唯一写入口是 AnomalyDetector::Record*,缺失等于空窗口,随玩家实体销毁,不入库。删除 EntityHash 和 thread_local buckets_;阈值三张表是配置,仍保留 thread_local。
Record*:
- 先 `if (!tlsEcs.actorRegistry.valid(player)) { if (player != entt::null) LOG_WARN(...stale...); return false; }`。原因:对陈旧句柄 emplace 在 Release 下会覆盖同槽位活实体的稀疏映射。
- 再 `auto& window = tlsEcs.actorRegistry.get_or_emplace<PlayerAnomalyWindowComp>(player);`。这是规则 2 允许的按需状态写入口,加注释说明。
- 其余逻辑不变。
其他接口:GetCurrencyGainCount 改用 try_get,拿不到返回 0。ClearPlayer 改为 valid 时 `remove<PlayerAnomalyWindowComp>`。ClearAll 改为 `tlsEcs.actorRegistry.clear<PlayerAnomalyWindowComp>()`。EmitAlert 本步不动。
player_lifecycle.cpp 里 DestroyPlayer 的 ClearPlayer 调用本步不动(变得多余但无害),由 B2 删除。
行为变化(写进注释和 PROGRESS):entt::null 主体(测试或无主体发放)不再记窗口,也不再以 player=0 发告警。
测试:新建 anomaly_detector_test.cpp,避开第 1 批正在改的 currency_test.cpp,用例自行设置阈值:
- WindowLivesOnPlayerEntityAndDiesWithIt:记录后组件存在;destroy 后再 create 复用同一槽位,新实体计数为 0;
- NullOrStaleEntityIsNotRecorded:返回 false,且不产生组件;
- ClearAllDropsAllWindows。
在 currency_test.vcxproj 的 ClCompile 组里加 anomaly_detector_test.cpp。currency_test.cpp 里的旧用例保持通过。

### 4.4 A4 清理 EcsContext 与 registry 死代码,SessionList 移出 registry(F026/F016 部分/F019-F021 部分/F099 的 SessionList)【A 阶段】

**文件**：
- `cpp/libs/engine/thread_context/ecs_context.h`
- `cpp/libs/engine/thread_context/ecs_context.cpp`
- `cpp/libs/engine/core/type_alias/player_session_type_alias.h`
- `cpp/libs/engine/core/utils/registry/game_registry.h`
- `cpp/libs/engine/core/utils/registry/game_registry.cpp`
- `cpp/libs/modules/mission/comp/mission_comp.h`
- `cpp/libs/modules/scene/comp/scene_node_comp.h`

(1) EcsContext:
- 删除 ErrorEntity()、OperatorEntity()、errorEntity_、operatorEntity_、itemRegistry,以及 Clear() 里对应的行;
- 在 playerList 旁新增 `using PlayerSessionMap = std::unordered_map<SessionId, Guid>;` 和成员 `PlayerSessionMap sessionPlayers;`(会话→玩家号索引,进程级私有索引,不是组件),Clear() 里加 `sessionPlayers.clear();`。GlobalEntity/GrpcNodeEntity/nodeGlobalRegistry 属于单例批次,不动。
(2) player_session_type_alias.h:改为 `using SessionList = PlayerSessionMap;`(兼容旧名),`inline SessionList& SessionMap(){ return tlsEcs.sessionPlayers; }`。78 处调用点都不用改。
(3) game_registry.h/.cpp:删除 class EventOwner、CreateEntityIfNotExists、RecreateEntity,以及只为它们引入的 stacktrace include。namespace entt 里的 to_entity 不动,留给外部 ID 批次。
(4) mission_comp.h:`class MissionsComp` 去掉 `: public EventOwner`。`#include "core/utils/registry/game_registry.h"` 保留,防止传递依赖断掉。
(5) scene_node_comp.h:删除 SceneRegistryComp、MainSceneNode、CrossMainSceneNode、SceneSceneNode、CrossSceneSceneNode、NodeStateComp、NodePressureComp、SceneNodePlayerStatsPtrComp。全仓只有不在 game.sln 里、本来就编不过的 cpp/tests/scene_test 还引用它们,该测试工程不动。ScenePlayers 与现有 include 保留,B2 删除整个文件。
协调:如果「单例→ctx()」批次已经删掉 ErrorEntity 等成员,或已移走 SessionList,对应子项跳过,以它的结构为准。

### 4.5 B2 删除 ScenePlayers 反向索引,补齐销毁兜底、trigger 引用卫生与 F069 注释(F065/F066/F067/C005/F069)【B 阶段 L1 第 1 步,第 1 批合入后】

**文件**：
- `cpp/libs/services/scene/player/system/player_lifecycle.cpp`
- `cpp/libs/services/scene/player/system/player_lifecycle.h`
- `cpp/libs/services/scene/player/system/player_scene.cpp`
- `cpp/nodes/scene/handler/rpc/scene_handler.cpp`
- `cpp/nodes/scene/handler/grpc/scene_node_service.cpp`
- `cpp/libs/modules/scene/comp/scene_node_comp.h(删除)`
- `cpp/libs/modules/modules.vcxproj`
- `cpp/libs/modules/modules.vcxproj.filters`
- `cpp/tests/cross_zone_test/cross_zone_test.cpp`

(1) 删除整个 scene_node_comp.h(A4 之后里面只剩 ScenePlayers)。去掉 player_lifecycle.cpp:32、player_scene.cpp:18、scene_node_service.cpp:5、scene_handler.cpp:34、cross_zone_test.cpp:2498 的 include,以及 modules.vcxproj 和 .filters 里的 ClInclude。如果编译暴露出传递依赖(NodeState、GameNodePlayerInfoComp 等),在用到的文件里补直接 include。
(2) 删除 scene_node_service.cpp 和 scene_handler.cpp CreateScene 里的 `emplace<ScenePlayers>` 各一行。
(3) BeginSceneDrain:改为对 `tlsEcs.actorRegistry.view<SceneEntityComp, Guid>()` 收集 sceneEntity==目标的实体,存成 residents 快照,后续逻辑不变。B1 会把 Guid 换成 PlayerGuidComp。注释写明:场景成员的唯一事实源是玩家身上的 SceneEntityComp,DestroyScene 很少执行,O(在线玩家)可以接受。
(4) DetachFromScene:删除 ScenePlayers 那段。trigger(BeforeLeaveScene) 之后不再解引用 sceneComp(F067)。
(5) DestroyPlayer:
- 删除 ScenePlayers 兜底,改为 `if (valid(playerEntity) && any_of<SceneEntityComp>(playerEntity)) { LOG_WARN("...path skipped DetachFromScene"); DetachFromScene(playerEntity); }`,让 AOI 格子、观察者兴趣表和客户端 ActorDestroy 都经离场出口清理(F065);
- 删除 AnomalyDetector::ClearPlayer 调用和那段注释,组件随实体销毁。
(6) player_scene.cpp HandleEnterScene:
- 删除 oldPlayers 的 erase 和 scenePlayers 的 emplace;
- trigger(BeforeLeaveScene) 之前把 oldScene 句柄拷成局部变量(mapChanged 已经在前面算好);
- trigger 之后重新 `sceneInfo = tlsEcs.sceneRegistry.try_get<SceneInfoComp>(scene)`,拿不到就 LOG_ERROR 并返回。不再使用 trigger 之前的组件指针(C005 卫生)。
(7) F069:改正 player_lifecycle.cpp 中 2133、2930、3009 附近(以 HEAD 为准)三处「try_get/any_of 对已销毁实体是未定义行为」的注释。正确说法:entt 3.13.2 的 valid/any_of/all_of/try_get/remove 对陈旧句柄按 version 比对,安全地返回空;先 valid() 是为了保护后面的 emplace*/get/patch/erase,对陈旧句柄 emplace 会在 Release 下覆盖同槽位活实体的稀疏映射。valid() 调用本身保留。
(8) player_lifecycle.h 里 BeginSceneDrain 和 DetachFromScene 的注释,去掉 ScenePlayers 的描述。
(9) cross_zone_test:
- PlaceInScene 去掉 emplace<ScenePlayers>;
- 新增 BeginSceneDrainOnlyCountsResidentsOfThatScene:两个场景各放玩家,只给目标场景的玩家开票;
- 新增 DestroyPlayerWithoutDetachLeavesNoAoiResidue:用 AoiSystem::OnSceneCreatedHandler 建格子表,两名玩家进入 AOI,直接 DestroyPlayer 其中一人;另一人的 AoiListComp 和格子集合里都不再含被销毁的句柄。

### 4.6 B6 删除死系统 scene_crowd 与 MovementAccelerationSystem(F079/Acceleration 无创建点)【B 阶段 L1 第 2 步】

**文件**：
- `cpp/libs/services/scene/spatial/system/scene_crowd.h(删除)`
- `cpp/libs/services/scene/spatial/system/scene_crowd.cpp(删除)`
- `cpp/libs/services/scene/spatial/comp/dt_crowd_comp.h(删除)`
- `cpp/libs/services/scene/spatial/constants/dt_crowd.h(删除)`
- `cpp/libs/services/scene/spatial/system/movement_acceleration.h(删除)`
- `cpp/libs/services/scene/spatial/system/movement_acceleration.cpp(删除)`
- `cpp/nodes/scene/handler/event/scene_event_handler.cpp`
- `cpp/libs/services/scene/world/world.cpp`
- `cpp/libs/services/scene/spatial/system/movement.cpp`
- `cpp/libs/services/scene/player/system/afk.h`
- `cpp/libs/services/scene/player/system/player_lifecycle.cpp`
- `cpp/libs/services/scene/CMakeLists.txt`
- `cpp/libs/services/scene/scene.vcxproj`
- `cpp/libs/services/scene/scene.vcxproj.filters`
- `cpp/tests/cross_zone_test/cross_zone_test.cpp`

(1) scene_crowd:DtCrowdPtr 全仓没有 emplace,AfterEnterSceneHandler 还拿 actor 句柄去查 sceneRegistry。删除这 4 个文件。scene_event_handler.cpp 是带守护段的生成式 handler,只在 ///<<< BEGIN/END 守护段内改,注释只用 ASCII(该文件没有 BOM,见文件内说明):删掉 include "spatial/system/scene_crowd.h",删掉 AfterEnterSceneHandler 和 BeforeLeaveSceneHandler 里的 SceneCrowdSystem 调用,保留 AoiSystem::BeforeLeaveSceneHandler。
(2) MovementAccelerationSystem:Acceleration 在生产代码里没有创建点、没有设计文档,而且这条积分路径缺少 MovementSystem 的 InBattleComp 排除和导航校验。删除这两个文件,删掉 world.cpp 里的 include 和 `MovementAccelerationSystem::Update` 调用,把 movement.cpp:74 exclude 列表里的 Acceleration 去掉,更新 afk.h 注释。player_lifecycle.cpp StopMotionForExit 删掉 Acceleration 分支,并改正上方注释(不再提 MovementAccelerationSystem)。proto 里的 Acceleration 消息保留,不动契约。
(3) CMakeLists.txt 删掉 scene_crowd.cpp 和 movement_acceleration.cpp 两行;scene.vcxproj 和 .filters 删掉对应的 ClCompile/ClInclude(dt_crowd_comp.h、dt_crowd.h、movement_acceleration.h、scene_crowd.h)。
(4) cross_zone_test 约 1026-1040 行:删掉 Acceleration 的 emplace 与断言,保留 Velocity 部分。
Npc 标签保留不动:它在 ResolveSkillTarget 和 ViewSystem 里属于正当的稀疏可选标签。

### 4.7 B7 必需初始化改为显式直接调用,不再依赖 dispatcher sink(F072)【B 阶段 L1 第 3 步】

**文件**：
- `cpp/libs/services/scene/player/system/player_lifecycle.cpp`
- `cpp/nodes/scene/handler/event/player_event_handler.cpp`
- `cpp/tests/bag_test/player_feature_persistence_test.cpp`

在第 1 批修完 F070 之后的 InitPlayerFromAllData 上做:
(1) 新号分支:在 trigger RegisterPlayerEvent 之前直接调用 `PlayerSkillSystem::RegisterPlayer(player)`(同目录,include player_skill.h)。
(2) PlayerAllDataMessageFieldsUnMarshal 之后直接调用 `PlayerMissionSystem::Initialize(player)`,它保证 kPlayerMission scope 存在、幂等。
(3) 两个事件仍然 trigger,但只作为通知。player_event_handler.cpp 守护段内删掉 RegisterPlayerEventHandler 里的 RegisterPlayer 调用和 InitializePlayerCompsEventHandler 里的 Initialize 调用,保留 valid 校验;注释说明「必需初始化在 InitPlayerFromAllData 显式完成,事件只作通知」。这样未注册 sink 的进程(测试)也能得到完整实体;这两个事件可以经 Kafka DispatchProtoEvent 进入(F071),去掉副作用后攻击面也小了。
(4) player_feature_persistence_test 新增用例:不注册 PlayerEventHandler,新号 InitPlayerFromAllData 之后 PlayerSkillListComp 含职业默认技能,MissionsContainerComp 含 kPlayerMission。
InitializeActorCompsEvent(actor_event_handler)有同样的问题,但属于第 1 批的 handler,本批不改,只在审计文档记为后续项。

### 4.8 B1 Guid 组件改为 PlayerGuidComp,同时改生成器模板(F032/F054/F055/F068/F099)【B 阶段 L1 第 4 步,与模板和重新生成原子落地】

**文件**：
- `cpp/libs/engine/core/type_define/player_guid_comp.h(新)`
- `cpp/libs/engine/core/utils/registry/component_guard.h(新)`
- `cpp/libs/engine/thread_context/ecs_context.h`
- `cpp/libs/engine/core/core.vcxproj`
- `cpp/libs/engine/core/core.vcxproj.filters`
- `tools/proto_generator/protogen/internal/template/attribute_sync.cpp.tmpl`
- `cpp/libs/services/scene/generated/attribute/*_attribute_sync.cpp(6 个,由 Codex 重新生成)`
- `cpp/libs/engine/core/network/player_message_utils.cpp`
- `cpp/libs/modules/mission/system/mission.cpp`
- `cpp/libs/modules/snapshot/snapshot_system.cpp`
- `cpp/libs/modules/transaction_log/anomaly_detector.cpp`
- `cpp/libs/modules/transaction_log/transaction_log_system.cpp`
- `cpp/libs/services/scene/battle/system/player_battle.cpp`
- `cpp/libs/services/scene/player/system/bag_marshal.cpp`
- `cpp/libs/services/scene/player/system/player_attribute.cpp`
- `cpp/libs/services/scene/player/system/player_equip.cpp`
- `cpp/libs/services/scene/player/system/player_landing.cpp`
- `cpp/libs/services/scene/player/system/player_lifecycle.cpp`
- `cpp/libs/services/scene/player/system/player_mission.cpp`
- `cpp/libs/services/scene/player/system/player_pet.cpp`
- `cpp/libs/services/scene/player/system/player_scene.cpp`
- `cpp/libs/services/scene/player/system/player_team.cpp`
- `cpp/libs/services/scene/spatial/system/scene_spawn.cpp`
- `cpp/libs/services/scene/spatial/system/view.cpp`
- `cpp/nodes/scene/handler/rpc/player/player_scene_handler.cpp`
- `cpp/nodes/scene/handler/rpc/player/s2s_player_scene_handler.cpp`
- `cpp/nodes/scene/main.cpp`
- `cpp/tests/aoi_delivery_test/aoi_delivery_test.cpp`
- `cpp/tests/aoi_delivery_test/aoi_delivery_test.vcxproj`
- `cpp/tests/bag_test/bag_remove_exact_test.cpp`
- `cpp/tests/bag_test/bag_test.cpp`
- `cpp/tests/bag_test/player_battle_settlement_test.cpp`
- `cpp/tests/bag_test/player_equip_attribute_test.cpp`
- `cpp/tests/bag_test/player_equip_test.cpp`
- `cpp/tests/bag_test/player_feature_handler_test.cpp`
- `cpp/tests/bag_test/player_feature_persistence_test.cpp`
- `cpp/tests/bag_test/player_feature_snapshot_test.cpp`
- `cpp/tests/bag_test/player_mission_system_test.cpp`
- `cpp/tests/cross_zone_test/cross_zone_test.cpp`
- `cpp/tests/currency_test/asset_op_system_test.cpp`
- `cpp/tests/missions_test/missions_test.cpp`
- `cpp/tests/reward_test/reward_test.cpp`

(1) 新建 player_guid_comp.h:`struct PlayerGuidComp { Guid value{}; };`。默认值 0,与旧 Guid 组件的值初始化一致。注释写明:玩家实体的业务身份,player_id 用 login 的 13-bit 布局(AGENTS §7.1),唯一创建点是 InitPlayerFromAllData,不入库;以前用 Guid=uint64_t 当组件,所有 uint64_t 组件共用一个池。
(2) 机械替换,生产代码约 41 处:`emplace<Guid>(e, id)` → `emplace<PlayerGuidComp>(e, id)`;`try_get<Guid>` → `try_get<PlayerGuidComp>`,解引用 `*p` → `p->value`;`get<Guid>(e)` → `get<PlayerGuidComp>(e).value`;B2 引入的 `view<SceneEntityComp, Guid>` 换成 PlayerGuidComp。只换 ECS 组件用法,Guid 作为 ID 类型别名保留,`std::vector<Guid>` 等不动。清单以 `rg -n "(get|try_get|emplace|any_of|all_of|remove|view|storage)<([^>]*,\s*)?(Guid|uint64_t)\b" cpp` 为准,结果必须只剩 cpp/tests/scene_test(不在 game.sln、本来就编不过,不改)。注释里出现的 get<Guid> 一并改文字。
(3) 模板 attribute_sync.cpp.tmpl:
- 加 #include "engine/core/type_define/player_guid_comp.h" 与 "muduo/base/Logging.h";
- entity_id/player_id 分支改为 `auto* fieldValue = actorRegistry.try_get<PlayerGuidComp>(entity); if (fieldValue) { syncMsg.set_{{ .FieldName }}(fieldValue->value); ... }`。语义不变:下发的仍是玩家 guid,客户端靠 ActorCreateS2C.guid 归属;
- F055:SyncAttributesCore 在循环结束后(包括 !hasDirtyField 的返回路径,但不包括 targetPlayers 为空的早退)`actorRegistry.remove<{{ .MessageName }}DirtyMaskComp>(entity);`,做到「存在即脏」;
- Set*AttrDirtyBit 改为先检查 `bitIdx >= mask.size()`,越界就 LOG_ERROR 并返回;再 `if(!tlsEcs.actorRegistry.valid(entity)) LOG_ERROR 并返回`;最后 `get_or_emplace<...DirtyMaskComp>(entity).dirtyMask.set(bitIdx)`。注释说明这是按需状态的唯一写入口,与第 1 批 MarkAttributeForUpdate 同一口径。
生成的 6 个文件不手改,由 Codex 跑 proto-gen 重新生成。模板、PlayerGuidComp、全部 emplace 点必须同一批落地,否则 entity_id 会静默消失。
(4) 可选护栏:新建 component_guard.h,内容是 `namespace entt { template<typename T> struct component_traits<T, std::enable_if_t<std::is_arithmetic_v<T>>> { static_assert(!std::is_arithmetic_v<T>, "基础类型不能当 ECS 组件:同底层类型别名共用一个池,请定义语义化 struct(如 PlayerGuidComp)"); using type=T; static constexpr bool in_place_delete=false; static constexpr std::size_t page_size=ENTT_PACKED_PAGE; }; }`,在 ecs_context.h 顶部 include。如果编译发现 entt 内部会实例化算术类型的 component_traits,就撤掉这一项,只保留文档规则。
(5) core.vcxproj 和 .filters 加两个新头的 ClInclude。
(6) 测试:
- 14 个测试文件的 emplace<Guid> 一律机械替换,reward_test:15 改为显式传值;
- aoi_delivery_test 的 Spawn 改为 PlayerGuidComp。B4 落地之后再改这个文件,两步改的是同一文件;
- aoi_delivery_test.vcxproj 加 ClCompile ../../libs/services/scene/generated/attribute/actorbaseattributess2c_attribute_sync.cpp;
- 新增用例 BaseAttributeSyncCarriesPlayerGuidAndClearsMask:置 Transform 和 EntityId 脏位,调用 ActorBaseAttributesS2CSyncAttributes 发给观察者,截获的消息 entity_id 等于 PlayerGuidComp.value,之后实体上不再有 DirtyMaskComp;
- 新增用例 DirtyBitOnDestroyedEntityCreatesNothing。

### 4.9 B4 AOI 旁路条目随离场完整清理,Unpin 立即驱逐;修正 C006 文档(F076/C006)【B 阶段 L2,可与 L1 并行】

**文件**：
- `cpp/libs/services/scene/spatial/system/aoi.h`
- `cpp/libs/services/scene/spatial/system/aoi.cpp`
- `cpp/libs/services/scene/spatial/system/interest.h`
- `cpp/libs/services/scene/spatial/system/interest.cpp`
- `cpp/tests/aoi_test/aoi_system_test.cpp`
- `cpp/tests/aoi_test/interest_system_mock.cpp`
- `cpp/tests/aoi_delivery_test/aoi_delivery_test.cpp`
- `docs/design/aoi_priority_design.md`
- `docs/design/aoi_priority_design_zh.md`
- `docs/design/aoi_priority_design_en.md`

在第 1 批改完 aoi/grid/interest 之后,在不同函数里改:
(1) AoiSystem::BeforeLeaveSceneHandler 重新组织结构,保证所有路径(含没有 Hex、没有场景、缺格子表的早退)最后都执行「远端清扫」:遍历 `tlsEcs.actorRegistry.view<AoiListComp>()`,对 watcher!=离场者且 `entries.erase(离场者)>0` 的观察者收集成集合,循环结束后逐个 NotifyEntityVisibilityChanges(watcher, {}, {离场者})。清扫放在邻格的 BroadcastEntityLeave 之后,邻格观察者已经被它摘除,不会重复通知。注释写明复杂度是 O(AOI 参与者) / 次离场、离场频率低,以及为什么不用 on_destroy(组件销毁顺序不保证)。
(2) Unpin 语义修正:
- 删除 InterestSystem::UnpinAoiEntity,新增 `static void AoiSystem::UnpinAoiEntity(entt::entity watcher, entt::entity target)`;
- 条目不是 kPinned 就返回;否则判断目标是否仍在观察者「当前格+邻格」内:同场景、双方都有 Hex、目标 GridId ∈ GetCurrentAndNeighborGridIds(观察者 Hex),且 ViewSystem::CanSee(watcher,target);
- 在范围内就把优先级改成 DetermineAoiPriority(watcher,target)(不再一律 kNormal,队友保持 kTeammate);不在范围内就立即 RemoveAoiEntity 并给观察者客户端发 ActorListDestroy。
设计文档原先说「下一次 Update 会驱逐」与代码不符:Update 只处理跨格变化。目前 Pin/Unpin 在生产代码里没有调用方,这个改动在线上不可见。
(3) C006:interest.cpp 的 GetEffectiveCapacity/GetPriorityPolicy 加注释,说明生产代码不会挂这三个组件、缺省即默认值(稀疏可选配置),是预留的接入口;代码不删。
(4) 测试:
- aoi_system_test 改写 UnpinDowngradesToNormal 为两个用例:远处目标 Unpin 后立即移除;近处目标 Unpin 后保留并回到空间优先级;
- aoi_delivery_test 新增:远端钉住的观察者在目标离场时收到 ActorListDestroy、表里也没有残留;远处 Unpin 立即收到销毁;
- interest_system_mock.cpp 不参与编译,同步删掉 UnpinAoiEntity 以免误导。
(5) 三份 aoi_priority_design*.md:Pin/Unpin 一节改为新语义;「Scenes attach ScenePriorityPolicyComp at creation」改为「生产未接线,缺省 OpenWorld/默认容量」。

### 4.10 B5 删除只写不读的技能上下文镜像(F043/F045)【B 阶段 L2】

**文件**：
- `cpp/libs/services/scene/combat/skill/system/skill.cpp`
- `cpp/tests/skill_test/skill_test.cpp`

在第 1 批改完 skill.cpp 之后:
(1) 删除 AddTargetSkillContext 和 RemoveTargetSkillContext。AddSkillContext 只写施法者表。HandleSkillFinish 简化为按迭代器删掉施法者那一条(不再需要「先取目标再删」的顺序说明)。
(2) GenerateUniqueSkillId 改为单参数 `(const SkillContextCompMap& casterContexts)`,调用点同步修改。
(3) CheckTimerPhase 里「上下文就永久留在 caster / target 的 SkillContextCompMap」一段改为只提 caster。
依据:目标侧镜像全仓没有读方。施法者在施法中被销毁时,镜像会泄漏到目标下线;删除镜像后这个问题自然消失,也不需要 on_destroy 回收。
测试:
- skill_test 新增 ReleaseSkillDoesNotTouchTargetContexts:对他人施法后,目标的 SkillContextCompMap 为空;
- 新增 CasterDestroyedMidCastLeavesNoResidueOnTarget。

### 4.11 B8 剩余 std 容器别名组件改为具名类型(F099 余项,低优先,可选)【B 阶段 L2,排在 B5 之后】

**文件**：
- `cpp/libs/services/scene/combat/buff/comp/buff_comp.h`
- `cpp/libs/services/scene/combat/skill/comp/skill_comp.h`
- `cpp/libs/services/scene/spatial/comp/grid_comp.h`

把 BuffListComp(unordered_map<uint64_t,BuffEntry>)、BuffPendingRemoveBuffs(unordered_set<uint64_t>,与 UInt64Set 是同一类型,将来有人把 UInt64Set 当组件就会撞池)、SkillContextCompMap、SceneGridListComp 从 using 别名改成具名派生类型。写法:`struct X : Base { using Base::Base; X() = default; X(const Base& b) : Base(b) {} X(Base&& b) : Base(std::move(b)) {} };`。从基类转换的构造函数要显式写,因为继承来的构造函数不能从基类对象构造。调用点(range-for、find、views::values、传 const Base&)都不用改,只让 ECS 池的类型身份唯一;编译报错时按报错修。顺便把 skill_comp.h 包装组件的 in_place_delete 注释从「必须」改为「A2 之后可选,保留以维持指针稳定」。这一步不做也不影响正确性,ecs-component-access-rules.md 里已经写了禁止新增 std 别名组件。

### 4.12 B3 sceneRegistry 改用 EnTT 自定义实体类型 SceneEntity(F029/F005,F079 余项)【B 阶段最后一个代码步,单代理,在 L1 和 L2 都完成之后】

**文件**：
- `cpp/libs/engine/core/utils/registry/scene_entity.h(新)`
- `cpp/libs/engine/thread_context/ecs_context.h`
- `cpp/libs/modules/scene/comp/scene_comp.h`
- `cpp/libs/services/scene/spatial/comp/teleport_settle_comp.h`
- `cpp/libs/services/scene/player/system/player_scene.h`
- `cpp/libs/services/scene/player/system/player_scene.cpp`
- `cpp/libs/services/scene/spatial/system/scene_spawn.h`
- `cpp/libs/services/scene/spatial/system/scene_spawn.cpp`
- `cpp/libs/services/scene/player/system/player_lifecycle.h`
- `cpp/libs/services/scene/player/system/player_lifecycle.cpp`
- `cpp/libs/services/scene/spatial/system/grid.cpp`
- `cpp/libs/services/scene/spatial/system/aoi.cpp`
- `cpp/libs/services/scene/spatial/system/interest.cpp`
- `cpp/libs/services/scene/spatial/system/nav_query.cpp`
- `cpp/libs/services/scene/player/system/player_team.cpp`
- `cpp/libs/services/scene/player/system/player_team_aoi.cpp`
- `cpp/nodes/scene/handler/rpc/scene_handler.cpp`
- `cpp/nodes/scene/handler/grpc/scene_node_service.cpp`
- `cpp/nodes/scene/handler/rpc/player/s2s_player_scene_handler.cpp`
- `cpp/nodes/scene/handler/rpc/player/player_scene_handler.cpp`
- `cpp/tests/aoi_test/aoi_system_test.cpp`
- `cpp/tests/aoi_delivery_test/aoi_delivery_test.cpp`
- `cpp/tests/skill_test/skill_test.cpp`
- `cpp/tests/routing_identity_test/routing_identity_test.cpp`
- `cpp/tests/cross_zone_test/cross_zone_test.cpp`
- `cpp/tests/bag_test/scene_spawn_test.cpp`
- `cpp/libs/engine/core/core.vcxproj`

(1) 新建 scene_entity.h:
```
enum class SceneEntity : std::uint64_t {};
using SceneRegistry = entt::basic_registry<SceneEntity>;
```
注释:sceneRegistry 专用的实体类型,与 actorRegistry 的 entt::entity 是不同类型,混用编译不过;ENTT_ID_TYPE 已是 uint64,entt_traits 按枚举的底层类型推导。
(2) ecs_context.h:`SceneRegistry sceneRegistry;`。SceneEntityComp:`SceneEntity sceneEntity{entt::null};`(scene_comp.h 补 include)。TeleportSettleComp::scene 改为 SceneEntity。
(3) 改签名:PlayerSceneSystem::HandleEnterScene(entt::entity, SceneEntity);SceneSpawnSystem::EnsureValidEnterLocation(entt::entity, SceneEntity, bool);PlayerLifecycleSystem::BeginSceneDrain(SceneEntity);grid.cpp 的 FindSceneGridList(entt::entity, SceneEntity);s2s 的 FindSceneEntityBySceneId 返回值;player_lifecycle.cpp:2643、scene_handler.cpp:931、scene_node_service.cpp:87 的 `entt::entity targetEntity/targetScene` 局部变量。
(4) 事件整数转场景句柄一律写 `SceneEntity{message.entity()}`(例如 AoiSystem::OnSceneCreatedHandler),不用 entt::to_entity。
(5) 其余按编译错误逐个改。gate 里名叫 sceneRegistry 的局部变量是节点 registry,不受影响。
(6) ECS_GET_OR_* 宏不改:传 SceneEntity 会直接编译失败,F005 由类型系统关闭。可以在 return_define.h 宏注释里补一句「只用于 actorRegistry」;如果该文件正被单例批次修改,就跳过。
(7) 测试:aoi_system_test(约 40 处)、aoi_delivery_test(成员 scene)、skill_test、routing_identity_test、cross_zone_test(PlaceInScene 返回类型)、scene_spawn_test 里的 `entt::entity scene = tlsEcs.sceneRegistry.create()` 改成 SceneEntity 或 auto。interest_system_mock.cpp 和 scene_test 不参与编译,不改。
如果「实体当外部 ID」批次正在改场景事件的载荷(OnSceneCreated 等)或 s2s 的 F091,本步排在它之后,沿用它的转换方式。

### 4.13 B9 文档与进度登记【最后一步;各阶段落地时各追加一条 PROGRESS】

**文件**：
- `docs/design/ecs-component-access-rules.md`
- `docs/design/ecs-entt-misuse-audit-20261015.md`
- `.github/copilot-instructions.md`
- `docs/PROGRESS.md`

(1) ecs-component-access-rules.md 只追加新节,不改别人刚写的三类初始化表:
- 「句柄与生命周期」:陈旧句柄上各 API 的安全性表。valid/all_of/any_of/try_get/remove 安全;emplace*/get_or_emplace/get/patch/replace/erase 必须先 valid(),理由是 Release 下会覆盖同槽位的稀疏映射;
- trigger 之后不复用之前拿到的组件引用;
- 旁路表三选一规则,以及 AOI 为什么不用 on_destroy;
- 成员函数 sink 必须用 scoped_connection;
- 组件必须是语义化类型:禁止基础类型和 std 别名,单例和私有索引不进 registry;
- sceneRegistry 使用 SceneEntity;
- TimerTaskComp 可以随意搬移。
(2) 审计文档:更新本批覆盖条目的 HEAD 状态列,新增「第 5 批」小节,列出保留不动的项及理由(Npc、C006,以及等待拍板的 C004),记下后续项:InitializeActorCompsEvent 也是「必需初始化走 sink」。
(3) copilot-instructions:ECS 规则补一行「不得用基础类型或 std 别名当组件」;A2 的 TimerTaskComp 改动如果还没写进去,在这里补。
(4) PROGRESS.md 只追加:起因、各步摘要、验证状态(写明未编译、待 Codex 验证)、Java 版状态。按 §9 不新建 md;本批不新增文档,所以 docs/README.md 不用改。

## 5. 生成器模板与生成产物

- tools/proto_generator/protogen/internal/template/attribute_sync.cpp.tmpl:entity_id/player_id 改为 try_get<PlayerGuidComp>(...)->value;SyncAttributesCore 消费后 remove DirtyMaskComp;Set*AttrDirtyBit 增加位越界和 valid() 守卫;新增 include。由 Codex 用 dev_tools.ps1 proto-gen-build/proto-gen-run 重新生成 cpp/libs/services/scene/generated/attribute/ 下 6 个 *_attribute_sync.cpp,生成物不手改。
- cpp/nodes/scene/handler/event/scene_event_handler.cpp(生成式 handler,只改守护段):删除 SceneCrowdSystem 的 include 和两处调用。
- cpp/nodes/scene/handler/event/player_event_handler.cpp(生成式 handler,只改守护段):RegisterPlayerEventHandler 和 InitializePlayerCompsEventHandler 去掉必需初始化的副作用,只保留校验。
- attribute_sync.h.tmpl 不改,DirtyMaskComp 的定义保持不变。

## 6. 测试

- 总纪律(AGENTS §10.1):Claude 不运行任何构建或测试。下面每条都交给 Codex 执行。MSBuild 必须串行:`msbuild <工程> /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64`。失败时保留首个 error 前后 30 行,不自动重试。
- A 阶段验证(A1 到 A4 都落地后):依次构建 cpp\libs\engine\core\core.vcxproj、cpp\libs\engine\thread_context\thread_context.vcxproj、cpp\libs\modules\modules.vcxproj,然后 `msbuild game.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64`。game.sln 已包含 gate、scene、battle 节点和各测试工程。通过标准:0 error。
- A2 验证:运行 timer_queue_unit_test,全绿,新增 4 个用例都在,MoveAndCopyAreSafe 按新语义通过。同时回归 timer_destroy_test、skill_test、buff_test(包装组件在 swap-and-pop 下定时器不丢)。rpc_controller_test 不在 game.sln,需单独 `msbuild cpp\tests\rpc_controller_test\rpc_controller_test.vcxproj ...` 后运行。
- A3 验证:currency_test 全绿,含新文件 anomaly_detector_test.cpp 的 3 个用例,原 AnomalyBucketsSeparateCurrencyAndItemAndClearInConstantTime 仍然通过。
- A1 验证:没有现成的单测工程能链接 Node 或 gate handler。用 `pwsh tools/scripts/dev_tools.ps1 -Command cpp-node-start` 本地起 gate 和 scene,再用 cpp-node-stop 正常停机,确认两侧都没有 assert 或崩溃转储。然后执行 `rg "connect<&[^>]*>\(\*this\)" cpp/libs cpp/nodes`,每处都赋给了 scoped_connection 成员。
- A4 验证:game.sln 0 error;cross_zone_test、missions_test、bag_test 全绿(SessionMap 和 MissionsComp 的行为不变)。
- B1 生成步骤:模板改完后由 Codex 依次执行 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build` 和 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run`。然后 `git status cpp/libs/services/scene/generated` 应只有 6 个 *_attribute_sync.cpp 有变化。如果有其他生成物变化(其他会话改了模板但未提交),停下报告,不要提交那些变化。
- B1 验证:game.sln 0 error;bag_test、cross_zone_test、currency_test、missions_test、reward_test、skill_test、aoi_test 全绿;aoi_delivery_test 单独构建运行,新增的两个属性同步用例通过;`rg -n "(get|try_get|emplace|any_of|all_of|remove|view|storage)<([^>]*,\s*)?(Guid|uint64_t)\b" cpp` 只剩 cpp/tests/scene_test。如果加了护栏,确认 component_guard.h 没有造成 entt 内部的编译错误。
- B2 验证:cross_zone_test 全绿,含 BeginSceneDrainOnlyCountsResidentsOfThatScene 和 DestroyPlayerWithoutDetachLeavesNoAoiResidue;scene 节点本地起停冒烟,并执行一次 DestroyScene 排空(dev 环境用 SceneManager 缩容或 GM 触发均可),日志里没有 ScenePlayers 相关报错。
- B4 验证:aoi_test 全绿(改写后的 Unpin 用例);aoi_delivery_test 全绿(远端清扫与 Unpin 驱逐用例)。
- B5 验证:skill_test 全绿,含两个新用例。
- B6 验证:game.sln 0 error;CMake 侧可选让 Codex 跑 `tools/scripts/build_linux.sh`,确认 scene 库的源文件清单一致;cross_zone_test 全绿。
- B7 验证:bag_test 的 player_feature_persistence_test 新用例通过,不注册 PlayerEventHandler 时新号也有默认技能和主线任务 scope。
- B3 验证:game.sln、aoi_delivery_test、rpc_controller_test 全部 0 error;aoi_test、skill_test、routing_identity_test、cross_zone_test、bag_test(scene_spawn_test)全绿;scene 节点本地起停,并执行一次进场、换场景、离场冒烟。
- 收尾:`python tools/scripts/gen_docs_index.py --check` 通过。本批不涉及压测,不跑压测,也不做任何性能结论。

## 7. 并行拆分、风险与拍板项

**并行拆分**：【A 阶段,现在就能做,不碰第 1 批的独占文件】分 4 个代理并行,文件互不重叠:
- A1:node.h/.cpp、client_message_processor.h/.cpp;
- A2:timer_task_comp.*、timer_queue_unit_test、copilot-instructions 的 TimerTaskComp 节;
- A3:anomaly_detector.*、新测试文件、currency_test.vcxproj;
- A4:ecs_context.*、player_session_type_alias.h、game_registry.*、mission_comp.h、scene_node_comp.h。
4 个都完成后,Codex 统一验证一次。

【B 阶段,第 1 批合入后开始】分两条并行车道:
- L1 串行:B2 → B6 → B7 → B1。四步都要改 player_lifecycle.cpp 和 cross_zone_test.cpp,必须由同一代理按顺序做,或者每步完成后再交给下一个。
- L2 串行:B4 → B5 → B8。分别改 aoi/interest 与 aoi 测试、skill.cpp 与 skill_test、三个 comp 头文件。B5 和 B8 都改 skill_comp.h,所以 B8 必须排在 B5 之后。
- 跨车道同步点只有一个:B1 要改 aoi_delivery_test.cpp 和它的 vcxproj,必须等 B4 改完 aoi_delivery_test.cpp 之后再动。
- L1 和 L2 全部完成后,由单个代理做 B3(typed SceneEntity,几乎覆盖所有场景相关文件);最后做 B9 文档。
- 每个阶段落地后,Codex 按 tests 中对应条目验证一次,再进入下一阶段。

**风险**：
- 并行会话冲突:node.cpp 和 client_message_processor.cpp 也在「gRPC 迁移至 muduo HTTP/2」会话的范围内,A1 只动 RegisterEventHandlers、构造函数和头文件成员,合并时以对方的结构为准;ecs_context.* 可能同时被「单例→ctx()」批次修改,A4 只增删列出的成员,对方已删的就跳过;game_registry.h 的 to_entity 留给「实体当外部 ID」批次,A4 不碰。
- 第 1 批仍在修改 player_lifecycle.cpp、aoi/grid/interest、skill.cpp、scene_handler/scene_node_service、s2s handler、actor_event_handler、snapshot_system、player_battle 以及若干测试。B 阶段必须等它合入后再开始,在它的最终版本上按函数定位修改,不能照搬本规格里的行号。
- PlayerGuidComp、模板和重新生成必须原子落地。只要有一边先合入,生成代码的 try_get 就取不到组件,ActorBaseAttributesS2C.entity_id 会静默消失,客户端无法归属状态更新,而且全程零报错。
- proto-gen-run 会重新生成全部输出,可能覆盖其他会话尚未提交的模板产物。Codex 必须只保留 6 个 attribute_sync 文件的变化,其余变化先停下报告。
- A2 改的是全进程都在用的 TimerTaskComp(gate、battle、etcd、Node)。移动语义从「取消」变成「转移」。全仓只发现 ECS 存储会搬移它(rooms 用 unique_ptr、BuffEntry 存在 node-based 容器里),这次改动属于修复;但它涉及历史上多次 UAF 修过的类,OnTimer 期间持有 State 的 shared_ptr 保活、generation 比对这些既有不变量必须保持,timer_queue_unit_test 的批内销毁、重新武装用例必须全部保持通过。
- F055 改成「脏才挂、同步后摘」之后,移动中的实体大约每 2 帧做一次 emplace/remove(O(1))。这是为了与第 1 批的 AttributeDirtyFlagsComp 口径一致,没有性能数据支撑,也不宣称性能变化。
- B4 的远端清扫在每次离场时是 O(AOI 参与者);B2 的 BeginSceneDrain 改为 O(在线玩家)。两者触发频率都低,但都没有压测数据,PROGRESS 里如实写明。
- A3 的行为变化:entt::null 主体(测试或无主体发放)不再记录异常窗口,也不再以 player=0 发 Kafka 告警。
- B3 由编译错误驱动,范围大,并且与「实体当外部 ID」批次在场景事件和 F091 上有交集,必须排在它之后,否则会两头返工。
- 删除文件(scene_crowd、movement_acceleration、dt_crowd、scene_node_comp.h)后,CMakeLists 和 vcxproj 要同步删除。只在 Windows 编译会漏掉 Linux 侧的清单,建议 Codex 补跑一次 build_linux.sh。
- scene_event_handler.cpp 和 player_event_handler.cpp 是生成式 handler,只能在守护段内修改;scene_event_handler.cpp 没有 BOM,注释只能写 ASCII。node.cpp 是 CRLF 行尾。
- 可选护栏 component_guard.h 依赖「所有实例化算术组件的翻译单元都看得到偏特化」(ODR)。迁移完成后不会再有实例化;如果编译暴露出 entt 内部会实例化,就撤掉护栏,只保留文档规则。
- cpp/tests/scene_test 不在 game.sln 里,本来就编不过,却引用了被删除的类型。本批不修改它,建议另行决定是否删除这个工程。
- 本批预计改动约 105 个文件,超过 AGENTS §10.2 的 30 文件门槛,需要用户授权后才能按 A/B 阶段实施。

**需负责人拍板**（未拍板前不要实施相关步骤，其余步骤照常进行）：
- 本批约 105 个文件(含测试、生成物和文档),超过 AGENTS §10.2 的 30 文件门槛,需要用户授权:是否同意先做 A 阶段(约 20 个文件,可与第 1 批并行),等第 1 批合入后再按 L1/L2 车道做 B 阶段?
- C004 PlayerItemBlockList(GM 单人物品封禁):5 个发放入口只读不写,这个功能实际不存在。二选一:(A,推荐)删除这个结构体、BagService 的参数和相关测试,单人封禁需求由全服 GainBlockService 覆盖;(B)作为功能补齐,加 GM 写入命令,并考虑持久化(会涉及存档 proto,需要两版同批改)。本批在拍板前只在审计文档标注。
- C006 AOI 策略组件(副本/竞技场优先级、客户端容量、服务器压力)目前没有创建点,本批默认保留,只修正文档和注释。如果你希望按 YAGNI 删除,需要连同约 20 个 aoi_test 用例一起删,请确认。

**Java 版（AGENTS §12）**：不适用,对应 Java 版本号:无。Java 版(D:\luyuan\xuanming-server-mmo)不用 entt,本批都是 C++ 内部的 ECS 结构调整,客户端可见契约没有变化:
- ActorBaseAttributesS2C.entity_id 仍是玩家 guid;
- Pin/Unpin 在生产代码里没有调用方,语义修正线上不可见;
- DestroyPlayer 兜底只在异常路径上补发 ActorDestroy,让表现回到正确状态;
- 没有 proto、tip 码或配置表的改动。

因此 PARITY.md 不需要登记功能行。PROGRESS 条目注明「对应 Java 版本号:无,不适用(Java 不用 entt,无客户端契约变化)」。如果 C004 选择方案 B(补齐 GM 单人封禁并持久化),那会是新功能,需要两版同批实现并在 PARITY.md 登记。

## 8. 挑战者意见（实施时必须处理）

结论：**needs_changes**

- **[medium]** A2 swaps one unstated rule for another. Today a move cancels the timer. That is what keeps owners whose callbacks capture the owner's own `this` safe when the owner is moved. After A2 the armed timer moves to the new owner, but the callback still fires on the old, moved-from or destroyed `this`. Owners in this situation, all implicitly movable (no deleted move, no mutex): DependencyGate::WaitAndRun `probeTimer.RunEvery(..., [this, &node, ...])` (node.h:317/347); ServiceDiscoveryManager `serviceNodeSyncTimer_.RunEvery(2.0, [this]{...})` (service_discovery_manager.cpp:16); EtcdService `[this]` at etcd_service.cpp:275/396/417/531/584. No current code moves any of them, so this is latent. Still, the spec says the convention 'is no longer needed' without stating the new rule, and the gRPC migration's batch 2 will rewrite the etcd timers.
  - 修法：Add a header comment in A2 stating the new rule: callbacks capture entity handles or IDs, never the address of the object that owns the TimerTaskComp; if one must, that owner has to be immovable. Explicitly delete move ctor and move assignment on DependencyGate, EtcdService and ServiceDiscoveryManager, or make them muduo::noncopyable, so the hazard fails to compile. Add one line to the risks.
- **[medium]** B1's new aoi_delivery_test case will not link. The test only stubs `BroadcastMessageToPlayers(uint32_t, const Message&, const EntityUnorderedSet&)` and `SendMessageToClientViaGate(..., entt::entity)` (aoi_delivery_test.cpp:71-76). The generated actorbaseattributess2c_attribute_sync.cpp instantiates both the EntityUnorderedSet and the EntityVector overload (lines 76/81 → 73). Adding it to the vcxproj makes the linker pull player_message_utils.obj from core.lib to resolve the EntityVector overload (player_message_utils.cpp:152). That object also defines the two functions the test already stubs, so the link fails with LNK2005.
  - 修法：In B1, also stub `BroadcastMessageToPlayers(uint32_t, const google::protobuf::Message&, const EntityVector&)` in aoi_delivery_test.cpp. Alternatively, put the new attribute-sync test in a project that links the real player_message_utils (e.g. cross_zone_test) and check the result there.
- **[medium]** B2's new test DestroyPlayerWithoutDetachLeavesNoAoiResidue lives in cross_zone_test, which only links scene.lib/core.lib and none of the cpp/nodes/scene event handlers. Nothing in it connects a BeforeLeaveScene sink (the AOI tests call AoiSystem::BeforeLeaveSceneHandler directly: aoi_system_test.cpp:341, aoi_delivery_test.cpp:185). So DetachFromScene's `dispatcher.trigger(leaveEvent)` reaches no listener, and the asserted AOI residue will still be there. More generally, the DestroyPlayer fallback's AOI cleanup still depends on a sink being registered, which is the same 'required work goes through a sink' pattern B7 is removing for initialization.
  - 修法：In the test, connect `tlsEcs.dispatcher.sink<BeforeLeaveScene>().connect<&AoiSystem::BeforeLeaveSceneHandler>()` and disconnect it in TearDown, or call the AOI cleanup explicitly. In the B2 notes, record that the fallback relies on SceneEventHandler being registered.
- **[low]** B7 step (2) is redundant. mission_marshal::Unmarshal ends with `(void)PlayerMissionSystem::Initialize(player);` (mission_marshal.cpp:92). PlayerAllDataMessageFieldsUnMarshal calls it unconditionally (player_database_loader.cpp:132, template player_data_loader.cpp.tmpl:37). Adding another direct call in InitPlayerFromAllData makes three copies of the same initialization; F072 itself already calls this path duplicated.
  - 修法：Only delete the Initialize call in InitializePlayerCompsEventHandler, and add a comment that the mission scope is guaranteed by unmarshal. Keep the explicit call for PlayerSkillSystem::RegisterPlayer. The new test still covers both.
- **[low]** B2(6): after the trigger, the spec re-runs try_get on SceneInfoComp and returns if it is missing. Returning at that point leaves a half-finished state: BeforeLeaveScene has already pulled the player out of the old scene's AOI and grids and Hex has been removed, but SceneEntityComp still points to the old scene because the rebinding comes later.
  - 修法：Before the trigger, copy the SceneInfoComp values needed later (step 4's EnterSceneS2C and the log line) into a local. After the trigger, use that copy, with no re-fetch and no early return. If a guard is wanted, put it before any mutation.
- **[low]** The non-explicit `X(const Base&)` / `X(Base&&)` constructors in B8 allow silent temporary copies. A plain std container passed to a `const BuffListComp&` parameter (buff.cpp:26 GenerateUniqueBuffId, :35 IsTargetImmune) would be copied, and the TimerTaskComp copy constructor drops timers. Today the alias is the same type, so these calls bind by reference.
  - 修法：Mark the base-conversion constructors explicit, or drop them and keep only `using Base::Base;`. Fix whatever then fails to compile.
- **[low]** B9's handle-safety table omits `registry.destroy`. In entt 3.13.2, registry.hpp:521-527 removes from the pools with a version check, but `entities.erase(entt)` looks the slot up by index only. In Release, destroying a stale handle releases the live entity that now occupies that slot.
  - 修法：List destroy (and storage.erase) with emplace*/get/patch/erase as calls that need valid() first, and point to DestroyEntity as the guarded helper.
- **[low]** Conflicts with other sessions are missing from the risks. scene_node_service.cpp, a generated gRPC service handler edited in guarded regions by both B2 and B3, will be rewritten by batch 4 of the gRPC migration ('scene / battle handler 模板改为 loop 内异步完成', grpc-muduo-transport.md §10). core.vcxproj, node.h and node.cpp already carry uncommitted changes from another session (the rpc_channel rename and FinalizeTeardownInLoop), and A1/B1/B3 edit them. The spec also flags guarded regions only for scene_event_handler and player_event_handler, but scene_handler.cpp, s2s_player_scene_handler.cpp, player_scene_handler.cpp and scene_node_service.cpp are generated guarded handlers too.
  - 修法：Add these files to the risks, state that all edits to them stay inside the guarded regions, and sequence B2/B3 relative to gRPC batch 4.
- **[low]** B4 moves Unpin to AoiSystem but leaves Pin in InterestSystem, so a paired API is split across two classes. The ResetEntityVisibility comment in aoi.cpp (~line 283, 'entries left in distant watchers' tables after Unpin downgrades them are not covered here') becomes wrong after B4.
  - 修法：Move Pin to AoiSystem as well, or add a matching AoiSystem::PinAoiEntity wrapper, and update that comment as part of B4.
- **[low]** Several std-type or shared_ptr components are neither converted nor explicitly deferred: ServiceNodeList = std::array<...> on nodeGlobalRegistry (node_utils.h:8; node.cpp:410 and others); RpcClientPtr; MuduoGrpcChannelPtr; std::shared_ptr<grpc::Channel> (node_connector.cpp:84/96/153, node_utils.cpp:251, id_segment_bootstrap.cpp:97, scene_entry_dispatch.cpp:104). B9's new rule banning basic types and std aliases as components would be broken the day it is written.
  - 修法：In B9 and in the audit doc, list these as known exceptions handed to the singleton batch and the gRPC migration, so the rule does not contradict the code.

**遗漏的调用点**：
- cpp/libs/services/scene/player/system/player_database_loader.cpp:190 (try_get<Guid>; missing from B1's file list, a batch 1 file)
- cpp/libs/services/scene/spatial/system/movement.cpp:202,230 (settle.scene and `const entt::entity currentScene = sceneComp->sceneEntity`; B3 will fail to compile here, not in B3's list)
- cpp/libs/services/scene/spatial/system/aoi.h:30 (UpdateGridState(entt::entity, entt::entity sceneEntity); B3 lists only aoi.cpp)
- cpp/libs/engine/core/node/system/node/node.h:317-347 DependencyGate, cpp/libs/engine/core/node/system/discovery/service_discovery_manager.h, cpp/libs/engine/core/node/system/etcd/etcd_service.h (implicitly movable owners whose callbacks capture [this]; need their moves deleted alongside A2)
- cpp/tests/aoi_delivery_test/aoi_delivery_test.cpp:74 (needs an EntityVector overload stub of BroadcastMessageToPlayers before B1 adds the generated sync cpp)
- cpp/libs/services/scene/player/system/player_lifecycle.h:1012 (Velocity / Acceleration comment; B6 only mentions the .cpp)
- cpp/libs/services/scene/spatial/system/aoi.cpp ~283 (ResetEntityVisibility comment about entries left behind after Unpin)
- docs/design/scene-creation-architecture.md:344, docs/design/agones-scene-node-high-density.md:23, docs/design/async-load-disconnect-reconnect-race.md:45 (still describe ScenePlayers)
- docs/design/cross-zone-scene-travel.md:642 (StopMotionForExit clearing Acceleration)
- cpp/libs/modules/modules.vcxproj.filters (in B2's list but does not exist)
- cpp/libs/engine/core/network/node_utils.h:8 ServiceNodeList, cpp/libs/engine/core/node/system/node/node_connector.cpp:84/96/153 (shared_ptr/std::array components to list as deferred in B9)

## 9. 回执

按 [README.md](README.md) §6 的格式交回。另外逐条说明 §8 每个 high/medium 问题是怎么处理的。

## 9. 2026-10-15 Codex 源码回执

A1–A4、B1–B8源码及回归用例已落盘：this事件订阅RAII、可搬移Timer内部State、Anomaly玩家按需组件、ScenePlayers反向索引删除、AOI显式离场与Pin/Unpin、只存施法者上下文、默认技能显式初始化、PlayerGuidComp、具名容器及SceneEntity。删除死crowd/acceleration系统并同步CMake/工程项。InBattleComp三处合法创建/恢复也集中到唯一首写helper，锁与账本顺序保留。

§8挑战修法：必要AOI清理直接调用，不依赖测试或生产sink；换场景先解绑再通知且使用场景信息副本，bool结果由两caller消费；初始化、退出、废黜在通知后复核旧句柄与索引归属，保留通知创建的同号替身。补真实无sink初始化、AOI离场/排空、初始化3阶段×2形态、废黜两形态回归。Timer回调保活State且不捕获组件地址，捕获owner this的owner禁止移动；容器基类转换explicit。生成属性同步消费后摘dirtymask，空target保留，测试补EntityVector广播stub，避免真实网络对象重复链接。

静态交叉复核未见活跃入口actor/scene混传，工程XML可解析，句柄安全表补destroy需先valid。未生成、编译、运行新测试或本地冒烟。InitializeActorCompsEvent派生初始化仍沿原入口、C004待业务决策、C006保留默认、gRPC资源组件由迁移会话验收、内部0号哨兵兼容及废弃scene_test未迁移均明确保留；详见README §7与组件访问规则。Java现有0.1.0-SNAPSHOT无需复制EnTT机制，本轮无Java业务改动/ContractSync/构建测试，不宣称已有玩法差异已对齐。

## 10. actor 必需派生初始化补充回执（2026-10-15）

本节继续处理 §9 当时保留的 actor 初始化订阅依赖。负责人要求直接完成源码、暂不编译，按需状态仍保留延迟初始化。

- `ActorAttributeCalculatorSystem::InitializeOnCreate(entt::entity)` 为显式创建接口，返回 bool，调用方必须处理失败。只在有效 actor 上建立 `MoveSpeedComp` 和 `CombatStateFlagsComp`，立即重算；移速沿用 Buff 加减聚合与非负夹取，战斗状态全量投影当前状态键。初始化和后续重算共用投影规则。
- `InitPlayerFromAllData` 在 Unmarshal 后、首个 RegisterPlayerEvent 前直接调用，首登、重登及迁移落地共用这条加载链。`DerivedAttributesComp` 继续由 `PlayerAttributeSystem::InitializeOnLoad` 直接创建并重算，不重复清零二级属性、HP/MP。
- 删除 `scene_local_events.cpp` 的 actor 初始化业务 handler 及 connect/disconnect。`InitializeActorCompsEvent` 保留为通知；原三种通知后的实体版本和 playerList 归属校验均保留。初始化失败不得继续发布后续通知、入场或快照。
- 冷却、施法者上下文、Buff、行为/战斗状态、AOI、相位定时器和脏标记仍由各自实际写入口创建。新初始化函数不预挂这些状态、不主动标同步脏位、不清除加载或合法写入已经产生的脏标记。后续每帧重算仍只取必需组件，缺失报错跳过，不借此修补创建缺口。
- 当前活跃生产 actor 创建入口只有玩家加载；EcsContext 的 create/destroy 用于退役 0 号内部哨兵。未来新增 NPC/怪物创建入口必须显式调用本初始化接口，不重新引入订阅依赖。本轮没有新增生成文件、工程项、协议字段或客户端改动。

本轮未生成、编译、运行测试或联机验证；现有生成树与第 2–5 批运行验收仍按 README §7 等待统一处理。Java 现有 0.1.0-SNAPSHOT 使用领域对象构造/加载和直接属性计算，不复制 EnTT 机制；本轮无 Java 业务代码或契约变化，未运行 Java 构建与测试，不借本项关闭既有玩法差异。

本次回归集中在 `cpp/tests/bag_test/player_feature_persistence_test.cpp`，没有新工程项：

- 强化 `NewPlayerInitializesDefaultSkillsAndMissionScopeWithoutPlayerEventSinks` 和 `InitializationStopsAfterEachNotificationDestroysOrReplacesPlayer`：最早 Register 通知以及 Actor/Player 通知到达时必需投影已齐，原 3×2 销毁/替换覆盖保留；观察者只读，不代替被测创建。
- 新增 `RegisteredPlayerInitializesActorProjectionsWithoutBusinessEventSinks`：已有注册记录同样直接初始化，不因跳过首登分支漏掉投影。
- 新增 `ActorCreationBuildsOnlyRequiredProjectionsWithoutDirtying`：裸 actor 只获得两种必需投影，不另建空 Derived、按需状态或脏标记。
- 新增 `ActorCreationProjectsExistingSourcesAndPreservesRuntimeAndDirtyData`：使用真实配置的加速 Buff 和正式战斗状态写入口，不等待 tick 就得到投影；重复调用保留来源状态、上下文、HP/MP、Derived 和已有脏位，移除来源后清掉旧投影且不补来源空表。
- 新增 `ActorCreationRejectsNullAndStaleWithoutChangingPoolsOrReplacement`：null 与失效版本句柄返回失败，组件池和新实体不被修改。

修复前失败的预期为：无初始化业务 sink 的真实加载在最早通知时缺少两种投影；只创建不计算会使非空来源投影断言失败；误用同步重算入口会制造或修改脏掩码。以上是静态推演，未实际运行红/绿。恢复运行验证时，在 README §7 的统一生成及串行重编前置完成后执行 `pwsh -File tools/scripts/run_cpp_tests.ps1 -Build -Filter bag_test`，先确认这六条及原缺组件重算用例通过，再按受影响范围验收 buff_test/skill_test；保留日志及 JUnit XML，失败不连续盲重试。
