# entt 误用审计（2026-10-15）

> 起因：用户问「我代码里面哪些乱用了 entt，我感觉有些地方很奇怪」。本文是那次只读审计的结论与修复状态，修 entt 相关问题前先看这里。
> 组件访问规则本身见 [ecs-component-access-rules.md](ecs-component-access-rules.md)。
> **2026-10-15 实施补充**：第 2–5 批源码和回归用例已由 Codex 按授权落盘，用户明确暂缓生成器构建、统一生成、编译和测试；当前生成树仍可包含旧接口。实现范围与保留项见 [分批总回执 §7](../handoff/ecs-refactor/README.md)，本表的历史 HEAD 状态等待独立审查后统一更新，不以源码改动直接关闭审计。

## 范围与方法

- **范围**：`cpp/` 下全部用到 entt / `tlsEcs` / `tlsNodeContextManager` 的非测试代码，以及生成这些代码的模板 `tools/proto_generator/protogen/internal/template/**`。entt 版本 3.13.2，`ENTT_ID_TYPE=uint64_t`（低 32 位 index、高 32 位 version）。
- **查找**：12 个按目录分片的审查 + 7 个跨全仓专项（跨 registry、句柄当外部 ID、引用失效、访问规则、句柄生命周期、dispatcher、设计异味），之后又从 4 个补漏视角扫了一遍：同模式扩散、未覆盖文件与生成器模板、`tlsEcs` 线程亲和、细粒度 API 语义。
- **复核**：每条都交给独立的复核者，要求它尽力推翻，拿不出代码证据就判不成立；high 再加一位独立二审。
- **基线**：发现产生于 `2f897bd07`。复核期间 HEAD 前进到 `ebccaf04d`，修复会话的改动进了这个提交，下文的「HEAD 状态」以它为准。
- **结果**：共 110 条，**102 条成立**（1 high / 24 medium / 77 low），8 条被推翻。全部条目见文末附录。
- 全程只读，没有改代码，也没有跑构建或测试。

## 为什么「奇怪」：五个根源习惯

1. **把 `entt::entity` 当对外 ID 用。** 这一类条目最多。
   - 实体整数直接下发给客户端当 actor ID（`view.cpp:90`、`aoi.cpp`、`movement.cpp`），客户端再把它原样传回来当技能目标。
   - 写进 proto 事件，经 Kafka 跨进程派发（`node_kafka_command_handler.h:163` → 生成的 `DispatchProtoEvent`）。
   - 截成 uint32，丢掉 version（`node_event.proto:19` 的 `ConnectToNodeEvent.entity`，`client_message_processor.cpp:405/413` 的日志，`anomaly_detector.h:90` 的哈希）。
   - SnowFlake 的 scene_id 直接当实体用（`s2s_player_scene_handler.cpp:24`）。
   - 实体整数冒充玩家号写进日志（`player_lifecycle_handler.cpp:28`、`mission.cpp:82`）。
   - `game_registry.h:17` 在 `namespace entt` 里自定义了一个 `to_entity(uint64_t)`，和官方的 `entt::to_entity` 同名，语义却相反。
   - 「没有目标 / 没有施法者」有两套哨兵：0 和 `entt::null`。0 号实体问题已修（F048）；`entt::null` 那一套还在（C003）。
2. **不用 `registry.ctx()`，而是造一个全局实体，挂上组件当单例，再用 `get_or_emplace` 去取。** 全仓没有一处用 `ctx()`。
   - `TipInfoMessage` 单例曾把 handler 写好的拒绝码覆盖成「成功」（F003，已修）。
   - `Clear()` 之后再取，会悄悄造出空的 `NodeInfo`（F008，见下表）。
   - `ErrorEntity`、`OperatorEntity`、`itemRegistry` 都没人用；`nodeGlobalRegistry` 只为放一个单例。
3. **组件该由谁创建没有约定。** 于是到处用 `get_or_emplace` 懒建，或者用 `try_get` 拿不到就静默跳过，功能空转了也看不出来。
   - 技能冷却组件和上下文组件曾经没有任何创建点，整条技能链空转（已修）。
   - 存盘 Marshal 用 `get_or_emplace` 取全部持久化组件，组件一旦缺失就会把零值写进 DB（F059，未修）。
   - `PlayerItemBlockList`、AOI 优先级策略组件在生产代码里也没有创建点（C004、C006）。
   - buff、AOI、属性脏标记、宠物 `EnsureComp` 等热路径上都有 `get_or_emplace`。
4. **生命周期与引用。**
   - 节点实体上挂着 gRPC CompletionQueue，销毁前没有先 Shutdown 再排空（F010）。
   - 生成的 `Send*` 直接 `get` CQ 和 Stub，而选点只按 `view<NodeInfo>` 挑（F100）。
   - dispatcher 的 sink 绑了 `*this`，从不断开（F009、F083）。
   - 遍历容器时经调用链改同一个容器（F040、F047，已修）。
   - 组件引用跨过同步的 `dispatcher.trigger` 继续使用（F067、F087、F096）。
5. **registry 的划分和实际用法对不上。**
   - 物品实体其实放在每个 Bag 私有的 registry 里（F024）。
   - `SceneEntityComp` 在 actor 组件里存 sceneRegistry 的裸句柄（F029）；`ECS_GET_OR_*` 宏写死查 `actorRegistry`（F005）。
   - `Guid = uint64_t` 这类基础类型直接当组件，导致所有 uint64 组件共用一个 pool（F032、F099）。
   - 背包扣除选哪一堆，取决于 pool 的内部迭代顺序（F023）。

## high / medium 条目与 HEAD 状态

### 仍未修

| ID | 位置 | 问题 |
|---|---|---|
| F001 / F002 / F013 / F098 | `node_kafka_command_handler.h:163`，生成器 `event_id.go:188` | Kafka 入口没有事件白名单。47 个进程内事件都能从外部打进来，其中很多事件的载荷是本进程的实体整数 |
| F059 / F060 | `player_database_loader.cpp:141`，模板 `player_data_loader.cpp.tmpl:25` | 存盘 Marshal 用 `get_or_emplace` 取持久化组件，组件缺失时会把零值货币和零值账本写进 Redis 和 DB |
| F010 | `node.cpp:1491` | 摘除节点时直接 destroy 挂着 CompletionQueue 的实体，在途 gRPC 调用的应答和失败回调永远不会投递 |
| F100 | `grpc_async_client.cpp.tmpl:150` | 生成的 `Send*` 对节点实体无守卫 `get<CompletionQueue>` / `get<StubPtr>`；registry 里混着 TCP 握手建的、缺这两个组件的实体 |
| F008 | `node.cpp:472` | 进程级单例靠 `get_or_emplace` 从全局实体上取。停机时 `Clear()` 之后再取，会造出一个空的 NodeInfo，gate 的断线通知随之丢失 |
| F012 / F022 | `node_entry.h:165`、`node_context_manager.h:56` | **部分修好**：`FinalizeShutdownInLoop` 已清 `RpcClientPtr`；`RunNodeMain` 的兜底路径和 CQ / stub 还没处理 |
| F091 | `s2s_player_scene_handler.cpp:24` | 把 SnowFlake 的 scene_id 直接 `to_entity` 当场景句柄（目前没有发送方，处于潜伏状态） |
| C003 | `buff.cpp:139/278` | no_caster 的 buff 把 `entt::null` 写进 `processed_caster`，叠层时再拿它和施法者整数比较，永远匹配不上。结果每次都新建一条，`max_layer` 失效，和战斗引擎的叠层口径分叉。目前表里没有 no_caster=1 的行 |
| F080 | `view.cpp:90` | 实体整数当客户端 actor ID 下发。**入站已防住**（`ResolveSkillTarget`），根因仍在 |

### 已在 `ebccaf04d` 修好

| ID | 问题 |
|---|---|
| F028（high） | 任务完成事件用 `dispatcher.enqueue` 入队，生产代码却从不 `update`：自动发奖、任务链、「完成任务」条件全部失效。已改为状态落定后同步 `trigger` |
| F027 | 任务事件里的句柄被截成 uint32 |
| F048 | 0 号实体和协议里的「没有目标」撞在一起。现在线程启动时让 0 号句柄退役 |
| F044 / F050 | 技能冷却组件和上下文组件没有创建点。现在 actor 初始化时挂上 |
| F046 / F049 | 客户端 `target_id` 能命中别的场景的玩家。现在统一走 `ResolveSkillTarget`：要求是玩家或 NPC、同场景、未冻结 |
| F003 / F090 | `TRANSFER_ERROR_MESSAGE` 用空 tip 覆盖拒绝码 |
| F040 / F047 | 遍历中修改同一容器；对自己放技能时同一条上下文被删两次 |
| F041 | 每帧 buff tick 里无守卫 `get` |

## 修这些问题时的注意事项

- **`dispatcher.clear()` 不会断开 sink。** entt 3.13.2 的 `clear()` 只丢弃排队中的事件（`dispatcher.hpp:56-61/351-356`）。F009 / F083 要靠成对的 `disconnect`，或者干脆不绑 `*this`。
- **不要用「补一句 `dispatcher.update()`」修 enqueue 问题。** entt 3.13 派发时把队列元素的引用交给监听者，任务链又会往同一个队列里塞事件，队列一扩容这个引用就悬空了。F028 因此改成了同步 `trigger`。
- **修 F010 / F012 / F022 时，先 `TryCancel`、Shutdown CQ 并排空，再销毁或 clear 节点实体。** 生成的流式 gRPC 客户端把 registry 里 protobuf 响应组件的地址交给了异步 `Read`（C008）。这个组件可以移动，按 swap-and-pop 存储；指针目前稳定，只是因为恰好每个 registry 只有一个实体挂它，直接销毁会打破这个前提。修法应改在模板里：把响应缓冲挪进不可移动的 `Async*GrpcClient` 结构。
- **收窄 Kafka 白名单（F001 / F002）之前，先盘清 Go 和 Java 两版里所有往这个入口发事件的地方。** 漏掉一个，合法事件就会被静默丢弃。
- **F059 要改 `player_lifecycle.cpp`。** 2026-10-15 时入场链正在被另一个会话大改，等它落地后再做。存盘侧应改为只读加 fail-closed，同时修改生成器模板。
- `NodeContextManager::GetRegistry` 只有 `assert` 边界检查。几处入口直接拿对端传来的 `node_type` 选 registry（F093、C007、`registration_manager.cpp:151`），Release 下会越界，应加上 Release 也生效的检查。

## 查过、没有问题的

- **线程亲和。** 全进程只有一个 EventLoop，ECS 只在这个线程上访问。具体核对过：
  - gRPC 同步服务的每个方法都用 `runInLoop` 加 promise/future 投回 loop，投递之前不碰 ECS；
  - Kafka poll 线程经 `queueInLoop` 投回；
  - etcd 的 CQ 由 loop 上的定时器轮询；
  - 信号与控制台处理只置标志或 `queueInLoop`；
  - 没有跨线程 connect / trigger 的 sink。
- **battle 节点与 `libs/services/battle`**：不用 entt。
- **API 细节。**
  - `view.get` 取的类型都在 view 内；
  - `exclude` 用法正确，遍历中改当前实体的组件属于 entt 允许的范围；
  - 没有用 `size_hint` 当精确数量；
  - `in_place_delete` 的定制与指针假设一致；
  - `ENTT_ID_TYPE` 在所有 x64 配置里一致。

## 与 Java 版的关系

Java 版不用 entt，本审计的 ECS 部分对它不适用。不过有两条是客户端可见的行为，Java 版是否等价**尚未核对**：
- 技能目标必须同场景，冷却真正生效；
- no_caster buff 应跨施法者叠层（C003）。

## 附录：全部条目

F 开头的是查找阶段的发现，C 开头的是补漏阶段的发现。「结论」列含义：确认、降级（问题真实但严重度或描述有更正）、推翻；带「二审」的是 high 级的独立复核。行号以 `2f897bd07` / `ebccaf04d` 为准。「HEAD 状态」只填了已核实的条目。

### 成立(确认/降级)(102 条)

| ID | 严重度 | 结论 | 位置 | 问题 | HEAD 状态 |
|---|---|---|---|---|---|
| F028 | high | 确认(二审) | `cpp/libs/modules/mission/system/mission.cpp:358` | 任务完成后用 dispatcher.enqueue 入队，但生产代码从不调用 dispatcher.update()，事件永远不派发，还会无限堆积 | 已修(改同步 trigger) |
| F001 | medium | 降级 | `cpp/generated/rpc/service_metadata/rpc_event_registry.cpp:1721` | DispatchProtoEvent(rpc_event_registry.cpp:1783)把全部 47 个事件都开放给 Kafka 命令入口，其中多数事件的载荷是进程内 entt 句柄整数(生成器问题；与 F002 同一问题) |  |
| F002 | medium | 确认 | `cpp/generated/rpc/service_metadata/rpc_event_registry.cpp:1783` | Kafka 入口能分派载荷为本进程 entity 整数的进程内事件，外进程句柄被直接拿来索引本地 registry |  |
| F003 | medium | 确认 | `cpp/libs/engine/core/macros/return_define.h:67` | 挂在 GlobalEntity 上的 TipInfoMessage 单例被 TRANSFER_ERROR_MESSAGE 整体覆盖进应答，ReleaseSkill 的拒绝码被抹成成功 | 已修(空 tip 不覆盖) |
| F010 | medium | 确认 | `cpp/libs/engine/core/node/system/node/node.cpp:1491` | 节点摘除时直接 destroy 挂着 grpc::CompletionQueue 组件的节点实体,没有先 Shutdown 再排空;在途 gRPC 调用的应答和失败回调永远不会投递(生成器模板问题) |  |
| F008 | medium | 确认 | `cpp/libs/engine/core/node/system/node/node.cpp:472` | 进程级单例挂在 thread_local registry 的懒创建实体上，到处用 get_or_emplace 取：线程不对或 Clear 之后会静默造出空的 NodeInfo/ServiceNodeList |  |
| F012 | medium | 确认 | `cpp/libs/engine/core/node/system/node/node_entry.h:165` | 退出时只清了 tlsEcs，从没清 tlsNodeContextManager：节点 registry 里的 RpcClient/TcpClient 要等 EventLoop 析构之后才销毁 | 部分修(只清了 RpcClientPtr) |
| F013 | medium | 确认 | `cpp/libs/engine/core/node/system/node/node_kafka_command_handler.h:163` | scene 的 Kafka 入口会把所有 proto 事件原样 trigger，包括携带进程内 entity 句柄的事件，跨进程整数被直接 to_entity 去索引 actorRegistry |  |
| F022 | medium | 确认 | `cpp/libs/engine/thread_context/node_context_manager.h:56` | tlsNodeContextManager 停机时从不 Clear，里面的 RpcClient/gRPC/protobuf 组件要等 thread_local 析构才释放，此时 EventLoop 和 protobuf 都已关闭 | 部分修(同 F012) |
| F027 | medium | 确认 | `cpp/libs/modules/mission/system/mission.cpp:341` | OnMissionCompletion 把 64 位实体句柄截成 uint32，丢掉 version，槽位复用过的玩家收不到任务事件 | 已修(保留 64 位) |
| C003 | medium | 确认 | `cpp/libs/services/scene/combat/buff/system/buff.cpp:278` | processed_caster 用 entt::null 表示「不区分施法者」，HandleExistingBuff 却拿它和原始 caster 整数比较，no_caster 的 buff 永远叠不上、每次都新建条目 |  |
| F040 | medium | 确认 | `cpp/libs/services/scene/combat/buff/system/buff_impl.cpp:35` | 遍历 caster 的 BuffListComp 时,经 AddSubBuffs -> AddOrUpdateBuff 往同一个容器插入或删除元素 | 已修(先收集再处理) |
| F041 | medium | 确认 | `cpp/libs/services/scene/combat/buff/system/modifier_buff_impl.cpp:39` | 每帧 buff tick 路径里对 parent 无守卫地 get 三个组件 | 已修(改 try_get) |
| F046 | medium | 降级(二审) | `cpp/libs/services/scene/combat/skill/system/skill.cpp:139` | 客户端上报的 target_id 直接当 entt::entity 用,只做 valid() 校验,可以跨场景、对冻结或战斗中的玩家生效 | 已修(ResolveSkillTarget) |
| F047 | medium | 确认 | `cpp/libs/services/scene/combat/skill/system/skill.cpp:308` | HandleSkillFinish 在 caster==target 时,两次 try_get 拿到同一个 SkillContextCompMap,先按 key 删掉节点,再用已失效的迭代器 erase,构成重复删除 | 已修(调换删除顺序) |
| F048 | medium | 确认 | `cpp/libs/services/scene/combat/skill/system/skill.cpp:377` | 用 0 当「没有目标/没有施法者」的哨兵,但 entt 第一个创建的实体就是整数 0(本节点第一个登录的玩家) | 已修(0 号句柄退役) |
| F049 | medium | 降级 | `cpp/libs/services/scene/combat/skill/system/skill.cpp:395` | ReleaseSkill 把客户端 target_id 直接当 entt 句柄，只做 valid() 检查、不校验同场景；当前可跨场景对任意玩家触发 OnSkillHit(剥离脱战加成 buff)和 LookAt，伤害链因 SkillContextCompMap 未挂载而休眠，修 F050 后会升级为跨场景伤害 | 已修(同 F046) |
| F050 | medium | 降级 | `cpp/libs/services/scene/combat/skill/system/skill.cpp:449` | SkillContextCompMap 和 CooldownTimeListComp 生产代码里没人挂,技能路径用 try_get 宏静默放行:冷却不生效,施法链全部空转 | 已修(同 F044) |
| F044 | medium | 确认 | `cpp/libs/services/scene/combat/skill/system/skill.cpp:85` | 技能链路依赖的 CooldownTimeListComp / SkillContextCompMap 在生产代码里没有任何创建点，所有读写都用 try_get/ECS_GET_OR_* 静默早退，导致冷却从不生效、技能上下文从不入表 | 已修(actor 初始化挂组件) |
| F059 | medium | 确认 | `cpp/libs/services/scene/player/system/player_database_loader.cpp:141` | 存盘 Marshal 对所有持久化组件用 get_or_emplace：组件一旦缺失就会造出零值账本和货币并写进 Redis 与 DB |  |
| F060 | medium | 确认 | `cpp/libs/services/scene/player/system/player_database_loader.cpp:156` | 存盘 Marshal 对全部持久化组件 get_or_emplace：组件缺失时会把零值或空账本当真实数据写进 Redis/DB（规则源自生成器模板） |  |
| F080 | medium | 确认 | `cpp/libs/services/scene/spatial/system/view.cpp:90` | 把 entt::entity 原样当成客户端可见的 actor ID 下发,客户端再原样回传当目标 | 入站已防住,下发仍用实体整数 |
| F090 | medium | 确认 | `cpp/nodes/scene/handler/rpc/player/player_skill_handler.cpp:25` | ReleaseSkill 直接写 response->error_message,被生成层用 globalRegistry 上的空 TipInfoMessage 覆盖 | 已修(同 F003) |
| F091 | medium | 确认 | `cpp/nodes/scene/handler/rpc/player/s2s_player_scene_handler.cpp:24` | 把 snowflake 的 scene_id 直接 entt::to_entity 当成 sceneRegistry 句柄用 |  |
| F100 | medium | 确认 | `tools/proto_generator/protogen/internal/template/grpc_async_client.cpp.tmpl:150` | 生成的 gRPC Send* 对 nodeEntity 直接 get<CompletionQueue>/get<StubPtr>，而选点函数只按 view<NodeInfo> 挑，registry 里还混着 TCP 握手建的、缺这两个组件的实体 |  |
| F005 | low | 确认 | `cpp/libs/engine/core/macros/return_define.h:119` | 5 个 registry 的句柄同为 entt::entity，ECS_GET_OR_* 写死查 actorRegistry；scene_crowd 已出现拿 actor 句柄查 sceneRegistry |  |
| F004 | low | 降级 | `cpp/libs/engine/core/macros/return_define.h:71` | TipInfoMessage 以 get_or_emplace 挂在 GlobalEntity 上，充当进程级隐式输出邮箱，正确性依赖每条写路径都同步走非 Empty 的 CallMethod 并由 TRANSFER 清空；目前没有可达的残留路径，但约定很脆弱，本应使用 ctx() |  |
| F006 | low | 确认 | `cpp/libs/engine/core/network/node_utils.cpp:248` | GetSceneManagerEntity 明知会选出没挂 CompletionQueue/Stub 的实体，交给生成的发送函数做未守卫的 registry.get |  |
| F007 | low | 降级 | `cpp/libs/engine/core/network/player_message_utils.cpp:300` | ResolvePlayerNodeSession 把业务 node_id 直接构造成 entt::entity 查节点注册表（死代码中的 R03 残留） |  |
| F009 | low | 确认 | `cpp/libs/engine/core/node/system/node/node.cpp:1239` | thread_local dispatcher 绑定了 Node / RpcClientSessionHandler 的实例，从不断开，Clear() 也不清 sink | 未修(dispatcher.clear 不断开 sink) |
| F011 | low | 降级 | `cpp/libs/engine/core/node/system/node/node_connector.cpp:47` | 节点 registry 把出站 gRPC 客户端拆成多个组件，又和 TCP 入站会话混放在同一 registry；生成的 Send* 不加守卫直接 get。只有内网对端握手时谎报 node_type=SceneManager 才会触发，选择器分级已把这种实体压到最后 |  |
| F014 | low | 降级 | `cpp/libs/engine/core/node/system/registration/registration_manager.cpp:120` | 节点类型 registry 里出站实体和入站握手实体只靠组件组合区分，gRPC 的去重和已连接判断只看 view<NodeInfo>；当前拓扑下触发不到 |  |
| F015 | low | 确认 | `cpp/libs/engine/core/node/system/registration/registration_manager.cpp:204` | ConnectToNodeEvent.entity 定义成 uint32，64 位节点句柄写进去会丢 version |  |
| F017 | low | 降级 | `cpp/libs/engine/core/utils/registry/game_registry.h:17` | namespace entt 里注入了与官方 entt::to_entity 同名、语义相反的 to_entity(uint64_t)；唯一把 GUID 当句柄的 s2s EnterScene 没有任何发送方，查不到时也会安全失败 |  |
| F016 | low | 确认 | `cpp/libs/engine/core/utils/registry/game_registry.h:6` | EventOwner 带隐式实体转换，CreateEntityIfNotExists/RecreateEntity 用外部整数 create(hint)，两者都是无人使用的地雷 |  |
| F019 | low | 确认 | `cpp/libs/engine/thread_context/ecs_context.cpp:13` | 单例一律挂在 GlobalEntity/GrpcNodeEntity 上并用 get_or_emplace 访问（全仓没有用 ctx()）；ErrorEntity/OperatorEntity 是死代码；AOI 每次可见性变化都对 GlobalEntity 做 get_or_emplace 取临时消息 |  |
| F018 | low | 降级 | `cpp/libs/engine/thread_context/ecs_context.cpp:5` | globalRegistry / nodeGlobalRegistry 实际各只装一个实体，当「类型→单例」容器用（全仓没有 ctx()），单例访问一律走 get_or_emplace，包括每条客户端消息和 AOI 路径 |  |
| F020 | low | 降级 | `cpp/libs/engine/thread_context/ecs_context.h:18` | 单例统一用「懒建实体 + get_or_emplace」实现；itemRegistry、ErrorEntity、OperatorEntity 闲置，nodeGlobalRegistry 只为放一个单例（设计异味，目前没有实际后果） |  |
| F021 | low | 确认 | `cpp/libs/engine/thread_context/ecs_context.h:21` | registry 的划分和实际用法对不上:itemRegistry 是死的，物品实体其实在每个 Bag 私有的 registry 里，句柄却对外暴露 |  |
| F023 | low | 确认 | `cpp/libs/modules/bag/item_store.cpp:241` | 扣除、并堆、合并选哪一堆完全取决于 entt pool 的内部迭代序(逆序 + swap-and-pop 重排) |  |
| F024 | low | 确认 | `cpp/libs/modules/bag/item_store.h:188` | 每个 Bag 私有一个完整 entt::registry 只放一种 ItemComp,实体仅当 map 值;registry 嵌在组件里,公开 API 还外泄私有 registry 的句柄 |  |
| F025 | low | 确认 | `cpp/libs/modules/currency/system/currency_system.cpp:149` | 货币增减路径里 try_get 之后再 get_or_emplace PlayerCurrencyComp:非初始化路径悄悄补组件,掩盖缺失初始化 |  |
| F026 | low | 确认 | `cpp/libs/modules/mission/comp/mission_comp.h:19` | MissionsComp 继承 EventOwner:从不设置的实体字段 + 非 explicit 的 entity/id_type 隐式转换 |  |
| C001 | low | 确认 | `cpp/libs/modules/mission/system/mission.cpp:82` | 任务系统日志把 entt 句柄整数标成 "playerId" 输出（F089 的同类，而且标签更直接地冒充了玩家号） |  |
| F029 | low | 确认 | `cpp/libs/modules/scene/comp/scene_comp.h:5` | SceneEntityComp 在 actorRegistry 组件里存 sceneRegistry 的裸 entt::entity,类型上无法区分属于哪个 registry |  |
| F030 | low | 确认 | `cpp/libs/modules/transaction_log/anomaly_detector.h:90` | AnomalyDetector 用 thread_local map 按 entt::entity 建键做旁路表,自定义哈希把 64 位实体截成 32 位 |  |
| F032 | low | 确认 | `cpp/libs/modules/transaction_log/transaction_log_system.cpp:19` | 用基础类型别名 Guid(=uint64_t)当 ECS 组件类型:所有 uint64_t 组件共享同一个 pool |  |
| F033 | low | 确认 | `cpp/libs/services/gate/session/comp/session_info_comp.h:24` | 会话用裸 uint64 存 entt 句柄,靠 UINT64_MAX 碰巧等于 entt::null,各处反复 entt::entity{int} 再自己做 valid 校验 |  |
| F034 | low | 确认 | `cpp/libs/services/scene/actor/action_state/system/actor_action_state.cpp:110` | 施法校验路径上 AddState 用 get_or_emplace<ActorStateComp>,且不校验实体 |  |
| F035 | low | 确认 | `cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:94` | 属性脏标记组件只加不删,每 tick 的 view 实际遍历全部 actor;循环体内还有多处 get_or_emplace |  |
| F036 | low | 降级 | `cpp/libs/services/scene/battle/system/player_battle.cpp:331` | 结算幂等账本的读取入口用 get_or_emplace,与资产账本的 fail-closed 约定不一致(目前不可达) |  |
| F037 | low | 确认 | `cpp/libs/services/scene/combat/buff/system/buff.cpp:123` | 战斗路径(加 buff、叠层判断)里用 get_or_emplace,违反「只在初始化用」的规则 |  |
| F039 | low | 降级 | `cpp/libs/services/scene/combat/buff/system/buff.cpp:410` | OnAfterTakeDamage 把攻击者当受击者传给 ResetCombatIdleBuff;另外从 DamageEventComp.attacker_id 走 uint64 -> to_entity 往返取实体是多余的(该参数未使用,整条链目前不可达) | 已修(参数改正) |
| F042 | low | 确认 | `cpp/libs/services/scene/combat/skill/comp/skill_comp.h:11` | TimerTaskComp 的移动语义会取消定时器,“包含它的组件必须手写 in_place_delete = true”只靠约定维持,漏写一次就会悄悄取消别的实体的定时器 |  |
| F045 | low | 确认 | `cpp/libs/services/scene/combat/skill/system/skill.cpp:126` | 技能上下文被镜像到目标实体的 SkillContextCompMap,但施法者在施法或后摇期间被销毁时没有人回收,目标侧条目泄漏到目标下线为止 |  |
| F051 | low | 确认 | `cpp/libs/services/scene/combat/skill/system/skill.cpp:491` | 施法路径里用 get_or_emplace 懒建四种定时器组件,CheckTimerPhase 每次施法又把它们 remove 掉 |  |
| F043 | low | 确认 | `cpp/libs/services/scene/combat/skill/system/skill.cpp:61` | SkillContextCompMap 同时承担「我施放的技能」和「指向我的技能镜像」两种语义，shared_ptr 在两个实体的组件之间共享；并且生产代码从未 emplace 这个组件 |  |
| F052 | low | 确认 | `cpp/libs/services/scene/combat/skill/system/skill.cpp:648` | 进程内事件都用 protobuf 携带 uint64 形式的实体句柄,两种转换写法混用 |  |
| F053 | low | 降级 | `cpp/libs/services/scene/combat_state/system/combat_state.cpp:15` | Kafka SceneCommand 路由对 DispatchProtoEvent 不设白名单，带进程内 entt 句柄的事件(CombatStateAdded/BeforeLeaveScene 等)理论上可被跨进程派发;目前没有生产者 |  |
| F054 | low | 降级 | `cpp/libs/services/scene/generated/attribute/actorbaseattributess2c_attribute_sync.cpp:31` | 生成器 attribute_sync 模板用原始 uint64_t 作为 entt 组件类型取 entity_id/player_id(与 Guid 同一个 pool,可读性差，日后新增 uint64 别名组件会静默撞 pool) |  |
| F055 | low | 确认 | `cpp/libs/services/scene/generated/attribute/actorbaseattributess2c_attribute_sync.cpp:88` | 生成的 Set*AttrDirtyBit 用 get_or_emplace,在 MovementSystem 等 per-tick 视图循环里调用 |  |
| C004 | low | 确认 | `cpp/libs/services/scene/player/system/asset_op_system.cpp:527` | PlayerItemBlockList（单玩家 GM 物品封禁）只有读的地方、没有写的地方：5 个发放入口都 try_get 后回落到空表，封禁功能实际不存在 |  |
| F058 | low | 确认 | `cpp/libs/services/scene/player/system/player_attribute.cpp:1070` | GmSetLevel 用 to_integral 把进程内的 entt::entity 塞进 protobuf 事件，而这个事件类型也能从 Kafka 反序列化后派发 |  |
| F057 | low | 确认 | `cpp/libs/services/scene/player/system/player_attribute.cpp:573` | EnsureComp 用 get_or_emplace，注释说只用于初始化，实际是所有读写路径的通用访问器；Recalculate 和 GmSetLevel 里也有 get_or_emplace |  |
| F061 | low | 确认 | `cpp/libs/services/scene/player/system/player_lifecycle.cpp:2285` | EnterScene 是公开入口,却不做 valid() 校验,上来就无守卫 get<Guid>,随后连续 get_or_emplace |  |
| F063 | low | 确认 | `cpp/libs/services/scene/player/system/player_lifecycle.cpp:2457` | 进程内 dispatcher 事件用 protobuf 定义,实体被压成 uint64 再靠 namespace entt 里自定义的 to_entity 还原 |  |
| F064 | low | 确认 | `cpp/libs/services/scene/player/system/player_lifecycle.cpp:2533` | 玩家维度的异常检测状态放在 ECS 外部、以 entt::entity 为键的 thread_local map 里,只能在 DestroyPlayer 手工清理 |  |
| F065 | low | 降级 | `cpp/libs/services/scene/player/system/player_lifecycle.cpp:2547` | DestroyPlayer 的兜底只摘了 ScenePlayers,没清 AOI 格子和观察者兴趣表;现有销毁路径都会先走 DetachFromScene,这是一处没做完的纵深防御 |  |
| F066 | low | 确认 | `cpp/libs/services/scene/player/system/player_lifecycle.cpp:2549` | ScenePlayers 是存放在 sceneRegistry 里的 actorRegistry 实体集合,反向索引全靠各处手工同步 |  |
| F067 | low | 确认 | `cpp/libs/services/scene/player/system/player_lifecycle.cpp:2580` | try_get 拿到的组件指针跨越同步 dispatcher.trigger 和外部系统调用后继续解引用,目前安全只是因为监听者恰好不删组件 |  |
| F068 | low | 确认 | `cpp/libs/services/scene/player/system/player_lifecycle.cpp:2647` | 把基础类型或类型别名直接当组件类型用(Guid = uint64_t、ScenePlayers = unordered_set<entity>) |  |
| F069 | low | 确认 | `cpp/libs/services/scene/player/system/player_lifecycle.cpp:2716` | 注释对 entt 3.13.2 语义的理解与源码相反:安全的读操作被当成 UB 加了防护,真正危险的写操作反而没有防护 |  |
| F070 | low | 确认 | `cpp/libs/services/scene/player/system/player_lifecycle.cpp:3507` | InitPlayerFromAllData 先 create 实体再查 playerList,重复时把刚建的实体留成孤儿 |  |
| F071 | low | 确认 | `cpp/libs/services/scene/player/system/player_lifecycle.cpp:3535` | 初始化事件用 proto 的 uint64 字段携带 entt::entity 整数,这些事件在生成的 DispatchProtoEvent 里也能被 Kafka SceneCommand 触发 |  |
| F072 | low | 降级 | `cpp/libs/services/scene/player/system/player_lifecycle.cpp:3536` | 新号默认技能只经 dispatcher 的 RegisterPlayerEvent 下发,必需的初始化依赖 sink 已注册;InitializePlayerCompsEvent 的任务初始化与反序列化路径重复 |  |
| F073 | low | 确认 | `cpp/libs/services/scene/player/system/player_lifecycle.cpp:3573` | SavePlayerToRedisImpl 只做了 valid() 就直接 get<Guid>,和调用方 HandleExitGameNode 对缺 Guid 的容错不一致 |  |
| F074 | low | 降级 | `cpp/libs/services/scene/player/system/player_pet.cpp:211` | PetSystem::EnsureComp 用 get_or_emplace 包住全部 14 个读写入口(含战斗快照/结算、只读面板),与『初始化路径』注释及项目 get_or_emplace 规则不符;当前加载路径保证组件先挂上,emplace 分支走不到,无实害 |  |
| F075 | low | 确认 | `cpp/libs/services/scene/spatial/system/aoi.cpp:169` | 每 tick 把 S2C 消息缓冲当成 global entity 上的组件,用 get_or_emplace 取用 |  |
| F076 | low | 确认 | `cpp/libs/services/scene/spatial/system/aoi.cpp:317` | 格子集合和兴趣表里存的是裸 entity 句柄,没有 on_destroy 钩子,只靠 BeforeLeaveScene 事件清理;Pin 后又 Unpin 降级、留在远处观察者表里的条目永远清不掉 |  |
| F077 | low | 降级 | `cpp/libs/services/scene/spatial/system/grid.cpp:57` | grid.cpp 空间查询对场景句柄做 get_or_emplace<SceneGridListComp>,且不先做 sceneRegistry.valid();SceneGridListComp 只在热路径上懒创建 |  |
| C006 | low | 降级 | `cpp/libs/services/scene/spatial/system/interest.cpp:71` | ScenePriorityPolicyComp / AoiClientCapacityComp / ScenePressureComp 在生产代码里没有创建点，try_get 恒回落到默认值（死代码，与设计文档不符）；由于生产只产生 kNormal/kTeammate，即便挂上策略组件，淘汰结果目前也完全相同 |  |
| F078 | low | 确认 | `cpp/libs/services/scene/spatial/system/interest.cpp:84` | AddAoiEntity 在每 tick 的 AOI 路径上 get_or_emplace<AoiListComp>,且不校验 watcher 是否存活 |  |
| F079 | low | 降级 | `cpp/libs/services/scene/spatial/system/scene_crowd.cpp:23` | SceneCrowdSystem::AfterEnterSceneHandler 拿 actorRegistry 的玩家句柄去 sceneRegistry 查 DtCrowdPtr(跨 registry 查询,目前是死代码) |  |
| F081 | low | 降级 | `cpp/nodes/gate/handler/event/gate_kafka_command_router.cpp:21` | gate Kafka GateCommand 的 payload 分支走全局 DispatchProtoEvent,没有白名单;携带进程内 entt 句柄的 OnNodeRemoveEvent 也能被跨进程投递 |  |
| F083 | low | 确认 | `cpp/nodes/gate/handler/rpc/client_message_processor.cpp:182` | RpcClientSessionHandler 把带 *this 的成员监听器挂到 thread_local dispatcher 上,但从不 disconnect | 未修(同 F009) |
| F084 | low | 确认 | `cpp/nodes/gate/handler/rpc/client_message_processor.cpp:405` | 把 entt 实体整数当成 scene_node_id 打日志,而且截断到 uint32,只剩 index |  |
| F085 | low | 确认 | `cpp/nodes/gate/handler/rpc/client_message_processor.cpp:413` | gate 把 SceneNodeService 节点 registry 命名为 sceneRegistry,并把节点 entity 整数截成 uint32 当 scene_node_id 打日志 |  |
| F082 | low | 确认 | `cpp/nodes/gate/handler/rpc/client_message_processor.cpp:57` | PickRandomNode 只按 view<NodeInfo> 选节点,生成的 sender 随后无守卫地 get<CompletionQueue>/get<StubPtr> |  |
| C007 | low | 确认 | `cpp/nodes/gate/handler/rpc/gate_service_handler.cpp:193` | 另外几处入口把对端或跨进程消息里的 node_type 未经校验直接交给 NodeContextManager::GetRegistry(只有 assert),Release 下数组越界（F093 的同类） |  |
| F086 | low | 确认 | `cpp/nodes/scene/handler/event/player_event_handler.cpp:32` | 进程内 entt 事件用 protobuf + uint64 entity 字段传句柄，类型安全和所属 registry 信息都丢了 |  |
| F087 | low | 确认 | `cpp/nodes/scene/handler/grpc/scene_node_service.cpp:41` | HandleCreateScene 持有 SceneInfoComp& 跨过 dispatcher.trigger,靠「监听者不删场景」才不悬空 |  |
| F088 | low | 降级 | `cpp/nodes/scene/handler/grpc/scene_node_service.cpp:56` | 组件引用/指针跨越同步 dispatcher.trigger 继续使用，只有订阅者删除同 pool 组件(swap_and_pop)时才会悬空;emplace 不会使其失效 |  |
| F089 | low | 确认 | `cpp/nodes/scene/handler/rpc/player/player_lifecycle_handler.cpp:28` | 日志用 entt::to_integral(player) 冒充玩家身份 |  |
| F092 | low | 确认 | `cpp/nodes/scene/handler/rpc/scene_handler.cpp:556` | 把请求级单例（TipInfoMessage、SessionList）挂在 GlobalEntity 上，每条消息都 get_or_emplace |  |
| F093 | low | 降级 | `cpp/nodes/scene/handler/rpc/scene_handler.cpp:808` | NodeContextManager::GetRegistry 只有 assert 边界检查，RoutePlayerStringMsg 用对端传来的 node_type 选 registry（Release 下可越界；当前仓内没有发起方） |  |
| F094 | low | 降级 | `cpp/nodes/scene/handler/rpc/scene_handler.cpp:856` | UpdateSessionDetail 对可缺失的 PlayerSessionSnapshotComp 无守卫 get（当前无发送方）；472 行 LastActiveFrameComp 的 get 无实害 |  |
| F095 | low | 确认 | `cpp/nodes/scene/handler/rpc/scene_handler.cpp:893` | scene_id→场景实体靠全表扫描 view<SceneInfoComp>，同一段查找复制了多份 |  |
| F096 | low | 降级 | `cpp/nodes/scene/handler/rpc/scene_handler.cpp:912` | CreateScene 持有 SceneInfoComp& 跨越同步 dispatcher.trigger（只有监听者删除 SceneInfoComp 时才会失效） |  |
| F097 | low | 确认 | `proto/common/event/node_event.proto:19` | ConnectToNodeEvent.entity 是 uint32,发送端塞进的是 64 位 to_integral(entity),version 被截掉(潜在问题) |  |
| F098 | low | 降级 | `tools/proto_generator/protogen/internal/generator/cpp/event_id.go:188` | DispatchProtoEvent 只在 Kafka 路由里调用,却把携带 entity 整数的进程内事件也收进了分发表,Kafka 入口没有事件白名单(纵深防御缺口) |  |
| F099 | low | 确认 | `tools/proto_generator/protogen/internal/template/attribute_sync.cpp.tmpl:33` | 把基础类型或 std 别名直接当组件类型(Guid=uint64_t、SessionList、ScenePlayers 等),生成器还靠 try_get<uint64_t> 恰好取到 Guid 池 |  |
| C008 | low | 确认 | `tools/proto_generator/protogen/internal/template/grpc_async_client.cpp.tmpl:92` | 生成的流式 gRPC 客户端把 registry 里 protobuf 响应组件的地址交给异步 Read，而这个组件的存储是 swap-and-pop，指针稳定只靠每个 registry 恰好只有一个实体挂它 |  |

### 被复核推翻(8 条)

| ID | 严重度 | 结论 | 位置 | 问题 | HEAD 状态 |
|---|---|---|---|---|---|
| F031 | low | 推翻 | `cpp/libs/modules/transaction_log/anomaly_detector.h:107` | 异常检测的滑动窗口放在以 entt::entity 为键的 thread_local 外部表里,需要手动清理,而且窗口随实体重建而重置 |  |
| C002 | low | 推翻 | `cpp/libs/services/scene/battle/system/player_battle.cpp:1032` | BuffComp 施法者哨兵混用 0 与 entt::null，第一个玩家实体为 0 导致快照 caster 丢失 |  |
| F038 | low | 推翻 | `cpp/libs/services/scene/combat/buff/system/buff.cpp:382` | OnIntervalThink 把 buffIt->second 依次传给三个回调,第一个回调可能已经通过驱散把该 buff 从表里删掉 |  |
| F056 | low | 推翻 | `cpp/libs/services/scene/player/system/asset_op_system.cpp:226` | 资产 RPC 路径上用 get_or_emplace 懒建运行时限频组件 PlayerAssetOpPersistRequestComp |  |
| F062 | low | 推翻 | `cpp/libs/services/scene/player/system/player_lifecycle.cpp:2316` | get_or_emplace 用在非初始化路径(存盘回调、进场),其中两处与 Init 重复,会掩盖漏初始化 |  |
| C005 | low | 推翻 | `cpp/libs/services/scene/player/system/player_scene.cpp:183` | HandleEnterScene 用 try_get 拿到 SceneEntityComp* 和 SceneInfoComp* 后，跨过同步的 dispatcher.trigger(BeforeLeaveScene) 和外部系统调用继续解引用 |  |
| C009 | low | 推翻 | `tools/proto_generator/protogen/internal/template/grpc_async_client.cpp.tmpl:265` | 生成的流式 gRPC 客户端把收发缓冲拆成一组松散组件挂在节点实体上(连裸 protobuf 类型也直接当组件)，异步 Read 的写入目标就是 entt 池里组件的地址 |  |
| C010 | low | 推翻 | `tools/proto_generator/protogen/internal/template/grpc_init_total.cpp.tmpl:80` | CQ 泵在 view<grpc::CompletionQueue>().each() 里同步调用业务应答/失败处理器，同时握着当前节点实体 CQ 组件的引用；安全完全依赖“节点摘除一律 queueInLoop 延后”这条没写进契约的约定 |  |
