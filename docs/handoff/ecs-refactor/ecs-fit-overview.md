# Codex 任务单：ECS 适用性总评（2026-10-15，第 1 版）

> **谁执行**：Codex。总规则、批次顺序、组件三类初始化口径、协调与回执格式见 [README.md](README.md)，本单不重复。
> **本版性质**：设计稿加挑战者意见的原样整理，还没有细化到「改前/改后代码片段」。Claude 的额度恢复后会出第 2 版（补丁级）。**先读 §8 挑战意见**：标为 high/medium 的问题，实施时必须按其中的修法修正设计。凡与 README §2 三类初始化规则冲突的地方，一律以 README 为准。
> **行号**：设计时取自当时的 HEAD，此后第 1 批和其他会话都改过代码，实施前请按函数名重新定位。

## 1. 目标

站在架构层面逐个子系统判断用 ECS 是否合适，给出「保留 / 移出 / 部分移出」的结论、理由、替代形态和迁移成本，以及保留部分必须遵守的约束。最后给出在第 1 批（进行中）和假定的第 2–5 批之外仍应做的结构性改动，整理成可以分波交给实施代理执行的规格。全部为服务端内部重构：不改客户端契约，不改 proto 字段与编号，不碰 gRPC 资源组件（已归「C++ gRPC 迁移至 muduo HTTP/2」会话）。

## 2. 采用的标准做法

【判据（EnTT 3.13 惯用法 + .github/copilot-instructions.md）】
(1) 能进 registry 的数据：大量同构实体，并且至少有一个系统按组件组合批量处理它；或者需要「按实体 O(1) 挂载、实体销毁时自动回收」的数据。
(2) 不进 registry 的数据：
- 进程单例，归属到对象；
- 请求级数据，放在栈上或作显式参数；
- 连接和网络资源，归 muduo 或传输对象；
- 跨实体共享的对象和私有索引。
(3) registry 内部的单例只允许是属于这个 registry 的派生数据（例如 scene_id 索引），放 ctx()。
(4) 多个 registry 在类型上区分：用 `entt::basic_registry<自定义 enum 实体>`（entity.hpp:25 对 enum 实体类型做了特化，可直接用），不靠变量名区分。
(5) 派生索引只在一个入口维护。需要兜底 clear() 时用 storage signal（on_destroy）。registry.clear() 会发信号，registry 析构不会发。
(6) dispatcher 只用于「可有可无的一对多通知」。必需步骤、链式业务、传输层回调一律直接调用或在构造时注入。

【逐子系统结论】
1. 玩家与 actor（actorRegistry）：**保留**。
   - 理由：World::Update 每帧有 7 个系统（Afk / Aoi / Movement / MovementAcceleration / Buff / AttributeCalculator / StateAttributeSync），都按 view<Transform,Velocity> 等组合批处理，这正是 ECS 适用的场景。
   - 现状：生产代码里 actorRegistry 只有玩家，唯一的 create 点是 InitPlayerFromAllData。怪物在不用 entt 的回合制 battle 节点。以后场景内加入 NPC，收益会更大。
   - 迁移成本：0，只加约束（见下文「保留部分的约束」）。

2. 玩家子系统组件：**保留为「玩家冷数据组件」，不是 ECS 退化**。
   - 涉及：任务 MissionsContainerComp、货币 CurrencyComp / PlayerCurrencyComp、宠物 PlayerPetComp、背包 PlayerBagsComp、属性 / 账本这类整份 protobuf 组件。
   - 为什么不移出：它们不被 view 批处理，registry 在这里就是一张「实体 → 聚合」的 O(1) 表。EnTT 允许任意可移动类型做组件。改造成独立的 PlayerData 对象要重做生命周期、改约 90 个文件，还会拆掉已经在用的 view<InBattleComp> / view<BattleSettlementLedgerComp>。
   - 真正的问题：
     - 没有唯一创建点（第 1 批在收）；
     - 组件带行为：MissionsComp 继承 EventOwner（S9）；
     - 同一事实存两份：CurrencyComp.debts 与 PlayerCurrencyComp.debts（S8b）；
     - 背包里嵌着私有 registry（第 3 批）。
   - 邮件不在 C++ scene，没有对应组件，不评估。

3. 玩家流程态组件：**保留**。
   - 涉及：PlayerEnterGameStateComp / PlayerTravelHandoffComp / PlayerFrozenComp / PlayerSceneChangeInFlightComp / TeleportSettleComp / BattlePrepareSessionComp 等。
   - 理由：它们是实体状态标签（view<PlayerTravelHandoffComp> 在扫描），不属于请求级数据。
   - 约束：每个状态组件由一个归属模块负责 emplace / remove。

4. 场景（sceneRegistry）：**保留存储，移出部分职责**。
   - 为什么不整体移出：场景数量少，也没有按组件批处理的每帧系统（grid.cpp 里的 view 只用于打日志），ECS 的价值有限。但场景实体销毁时连带清掉 SceneGridListComp / DtCrowdPtr 是有用的。整体移出要改 26 个文件，并且和第 1 批的 AOI / grid 重叠，换来的只是少一个 registry。
   - 现有问题：
     - 场景句柄和 actor 句柄同为 entt::entity（F029 / F005 / F079）；
     - scene_id 查实体靠全表扫描，同一段查找复制了多份（F095）；
     - 创建 / 销毁在 scene_handler.cpp 和 scene_node_service.cpp 各写一份（N6）；
     - ScenePlayers 的反向索引靠手工同步（F066）。
   - 替代：
     - S2a：SceneDirectory（唯一的创建 / 销毁入口 + ctx 索引 + 类型化的 ScenePlayersComp）；
     - S2b：用 `entt::basic_registry<SceneEntity>` 给场景句柄加强类型。

5. 节点与连接（NodeContextManager）：**移出，分两期**。
   - 理由：
     - 节点只有几十个，且彼此异构；
     - 所有查找都是 view<NodeInfo> 线性扫 uuid / node_id；
     - 出站 / 入站角色靠组件组合隐含表达（F011 / F014）；
     - 发现表 ServiceNodeList 另存了一份节点数据；
     - 实体整数外泄到 gate 的 SessionInfo（F033）和 ConnectToNodeEvent（F015 / F097）；
     - 生成的 Send* 签名被绑死成 (registry&, entity)；
     - registry 在这里只充当「map + 动态属性袋」，带来的只有无守卫 get、组件缺失和句柄混用。
   - 替代形态：NodeDirectory。每个 nodeType 一张 `unordered_map<uuid, NodeRecord>`，另加 (zone, node_id) 二级索引。NodeRecord 包含：
     - NodeInfo；
     - 出站传输，用 variant 表示（RpcClientPtr | muduo_grpc 通道）；
     - weak 持有的入站 RpcSession；
     - 重连定时器；
     - gate 的握手状态。
     进程内句柄用 NodeRef。
   - 分期：
     - 第 1 期（S6）：只读门面，加上把场景层调用点收口到门面，不动存储和传输。
     - 第 2 期：替换存储。必须等 gRPC 迁移第 3 批（模板不再生成 registry + entity 签名）和 muduo RPC 第 3 批落地，由那两个会话承担或联合立项。
   - 迁移成本：第 1 期约 13 个文件；第 2 期约 35 个文件，含模板。

6. 背包物品：**移出（第 3 批）**。
   - 理由：每个 Bag 私有一个 registry，只放一种 ItemComp；实体只当 map 的值用；扣哪一堆取决于 pool 的迭代序（F023 / F024）；itemRegistry 是死的。
   - 替代：实例层改成显式有序容器（bag-instance-layout-split.md）。PlayerBagsComp 作为玩家组件保留。

7. 技能上下文 SkillContextCompMap：**部分移出**。
   - 理由：
     - 同一个 shared_ptr 同时挂在施法者表、目标镜像表和 BuffEntry 上（F043）；
     - 目标镜像只写不读（N3）；
     - buff 只读 caster，而 buffPb.caster 里已经有同样的值。
   - 替代（S4）：施法实例只归施法者所有，删除目标镜像，buff 改存值类型 BuffSource。
   - 不推荐把每次施法建成一个实体：会引入更多跨实体引用。

8. 请求级 / 临时数据：**移出 registry**，也不放 ctx()。
   - 涉及：
     - TipInfoMessage（请求输出邮箱，F003 / F004 / F092）；
     - SessionList（进程级索引）；
     - NodeInfo / ServiceNodeList（进程单例，F008）；
     - AOI 的 S2C 缓冲（F075，第 1 批处理）。
   - 目标形态：请求级数据在 CallMethod 的栈上，或由 handler 直接写 response；进程级数据分别归 Node / ServiceDiscoveryManager / scene 会话模块。
   - 归属：假定归第 2 批；本方案只把目标形态写进规范（S0）。

9. dispatcher：**部分移出**。
   - 现状：
     - 除 OnNodeRemoveEvent 外，每个事件都只有 1 个监听者（N2）；
     - 必需初始化（InitializeActorCompsEvent / InitializePlayerCompsEvent / RegisterPlayerEvent）、任务链（AcceptMissionEvent / OnMissionAwardEvent / ConditionEvent 自我重入）、传输层回调（OnConnected2TcpServerEvent → Node，绑 *this）都伪装成了事件；
     - 事件载荷是 proto + uint64 实体整数，而且都能从 Kafka 打进来。
   - 处理：
     - 通知型事件保留：进出场景、战斗状态、击杀、升级；
     - 必需初始化改为直接调用（S3）；
     - 任务链改为「返回待办 + 显式驱动」（S5）；
     - 传输回调改为构造注入，随 muduo RPC 第 3 批一起做，本方案不动；
     - Kafka 入口归第 4 批白名单。

10. globalRegistry / nodeGlobalRegistry / itemRegistry / ErrorEntity / OperatorEntity / GrpcNodeEntity：**删除**（第 2 / 3 批，S9 兜底）。

【长期目标形态】
- EcsContext 收缩为 SceneWorld，只在 scene 进程存在：actorRegistry、SceneRegistry、玩家索引、通知用 dispatcher。
- gate 不再持有 ECS 世界。
- engine 层不再认识 actorRegistry：S7 删掉死函数后，剩下的 SendMessageToClientViaGate(entity) / BroadcastMessageToPlayers 两个重载迁到 scene 层（会动 attribute_sync 模板的 include）。
- 前置条件：第 2 批、S7、S6 第 2 期。

【保留部分的约束（写进 S0 规范，S1 机械守住）】
- 唯一创建点：组件只在原型工厂或加载时创建；按需创建的可选组件只能由所属模块的写路径显式 try_get + emplace。
- 组件是纯数据、不嵌 registry。TimerTaskComp 可搬移，in_place_delete 只为指针稳定、不是正确性前提；具名容器组件允许继承 std 容器，不提供从基类容器到组件类型的转换（见 ecs-component-access-rules.md）。
- 类型即语义：不用基础类型或 std 别名当组件。
- 实体句柄不出进程（第 4 批）。
- 不在两个实体的组件之间共享所有权。
- 以实体为键的外部侧表改成组件（S8a）。
- 派生索引走单一入口维护，或用 storage signal 兜底。
- 组件引用不跨过 dispatcher.trigger。

【为何优于备选】
- 场景用强类型 basic_registry、不整体移出：编译器会找出所有跨 registry 的误用；改动是机械的，和第 1 批不冲突。
- 节点用目录、不留在 ECS：节点不需要批处理，留在 ECS 只剩缺点。分两期做，是为了不和两个迁移会话同时改同一批文件。
- 任务链改「返回待办 + 显式驱动」、不保留重入事件：顺序显式，深度有上限，也不再依赖监听者是否注册；同时关掉了从 Kafka 注入事件触发接任务 / 发奖的路径。
- buff 存值类型 BuffSource、不把施法建成实体：改动最小，并消除跨实体共享所有权。
- 原型工厂直接调用、不用事件做初始化：必需初始化不再依赖 sink 是否注册（F044 的同类根因）。

【改动按收益 / 风险排序】
1. S0 + S1：规范与机械守卫。收益高，风险极低，约 9 个文件。
2. S7：删除引擎层死函数。收益中，风险低，4 个文件。
3. S9：删除死 ECS 类型。收益低，风险极低，约 9 个文件。
4. S2a：场景目录。收益高，风险低到中，约 10 个文件。
5. S3：原型工厂与显式初始化。收益高，风险中，约 9 个文件。
6. S5：任务链显式化。收益中高，风险中，约 7 个文件。
7. S8：侧表改组件与 debts 单一来源。收益中，风险低（客户端可见部分待拍板），约 6 个文件。
8. S4：技能施法实例所有权收口。收益中，风险中，约 14 个文件。
9. S2b：场景句柄强类型。收益高，风险中（面广但机械），约 25 个文件。
10. S6：节点目录第 1 期。收益中，风险中高（需跨会话协调），约 13 个文件。
11. 节点目录第 2 期：约 35 个文件，归迁移会话。
12. 可选：技能相位 / buff 到期改为「截止时间数据 + 每帧系统」，待拍板。
13. 长期：拆出 SceneWorld。

【批次假定】本方向没有拿到第 2 / 4 / 5 批的定义，按审计的五个根源推断如下：
- 第 2 批：单例、ctx 与请求级数据；
- 第 3 批：背包物品（任务给出）；
- 第 4 批：句柄不出进程（客户端 actor ID、Kafka 白名单、proto 事件里的 uint64、日志冒充 player_id）；
- 第 5 批：组件创建约定，以及 dispatcher sink 与引用的生命周期。
本方案各项若与实际批次重叠，以批次为准、删去重叠部分。可能重叠的地方：S9 与第 2 批，S3 与第 5 批，S2b 与第 3 / 4 批，S5 与第 5 批。

W1 及以后的各步都必须在第 1 批合入之后串行执行，不和它同时改同一处。

## 3. 覆盖的审计编号

F005（类型层：SceneEntity 与 entt::entity 不再互通，S2b）、F007（ResolvePlayerNodeSession 死代码，S7）、F016（EventOwner / CreateEntityIfNotExists / RecreateEntity，S9）、F020 / F021（ErrorEntity / OperatorEntity / itemRegistry；仅当第 2 / 3 批未做时由 S9 兜底）、F026（MissionsComp 继承 EventOwner，S9）、F029（SceneEntityComp 存裸 sceneRegistry 句柄，S2b）、F030（AnomalyDetector 把 64 位实体哈希截成 32 位，S8a）、F043（SkillContextCompMap 跨实体共享 shared_ptr、双重语义，S4）、F045（目标侧镜像随施法者销毁泄漏，S4）、F064（以实体为键的 thread_local 侧表只能手工清理，S8a）、F065（部分：ScenePlayers 由 SceneDirectory 唯一维护，S2a）、F066（ScenePlayers 反向索引靠手工同步，S2a）、F068（ScenePlayers 那一半：std 别名当组件，S2a）、F070（InitPlayerFromAllData 先 create 后查重，留下孤儿实体，S3）、F071（部分：初始化事件不再有副作用，Kafka 注入无效，S3）、F072（必需初始化依赖 dispatcher sink，S3）、F079（跨 registry 查询变为编译错误，S2b）、F095（scene_id 全表扫描、查找复制多份，S2a）、N1（新发现）：CurrencyComp.debts 与 PlayerCurrencyComp.debts 存了两份；运行期只更新后者；GetCurrencyList（player_currency_handler.cpp:82）和背包信息（player_feature_snapshot.cpp:159）把登录时的过期 debts 下发客户端，Java 的 Wallet.toClient() 不下发（S8b）、N2（新发现）：47 个进程内事件中，除 OnNodeRemoveEvent 外都只有 1 个监听者；必需初始化和任务链伪装成事件（S3 / S5）、N3（新发现）：技能上下文的目标侧镜像只写不读；BuffEntry.skillContext 只被读 caster，而 buffPb.caster 已有同样的值（S4）、N4（新发现）：引擎层 SendMessageToPlayerOnSceneNode / SendMessageToPlayerOnNode / CallMethodOnPlayerNode / SendMessageToPlayerViaClientNode / SendMessageToPlayerViaSessionNode 全仓没有调用方，却是 engine 读 actorRegistry 的主要来源（S7）、N5（新发现）：scene_node_comp.h 里的 9 个类型只有 ScenePlayers 在用；tests/scene_test 引用了不存在的 scene/system/scene.h，也不在 game.sln 和 CMake 里（S9）、N6（新发现）：场景创建 / 销毁在 scene_handler.cpp 和 scene_node_service.cpp 各一份，注释要求「手工保持同步」（S2a）、N7（新发现）：节点表只靠线性扫描加组件组合区分角色，场景层直接拿 gate registry 拼 RpcSession + NodeInfo，同一模式重复 6 处以上（S6）

## 4. 实施步骤（预计改动约 90 个文件）

### 4.1 S0【W0】把 ECS 适用性结论与约束写进规范（只改文档）

**文件**：
- `docs/design/ecs-component-access-rules.md`
- `docs/design/ecs_zh.md`
- `docs/design/ecs.md`
- `docs/design/ecs_en.md`
- `.github/copilot-instructions.md`
- `docs/design/ecs-entt-misuse-audit-20261015.md`
- `docs/PROGRESS.md`

1. ecs-component-access-rules.md：
- 文件开头新增「§0 哪些数据进 registry」，写入 standard_approach 里的 6 条判据、10 条子系统结论表（列：子系统 / 结论 / 理由 / 替代形态 / 成本 / 归属批次）和「保留部分的约束」。
- 改写「Acceptable uses of get_or_emplace」：原来的 player_database_loader、TipInfoMessage、ServiceNodeList 三条已被审计判为误用，改为「只允许原型工厂（S3 新增）与加载反序列化；所属模块自己的可选组件用显式 try_get + emplace」。
- 「Problematic uses」表的行号已过期，删表，改为指向审计文档附录。

2. ecs_zh.md / ecs.md / ecs_en.md：
- 把「允许单例 Component」改成：registry 内的单例只允许属于该 registry 的派生数据，放 ctx()；进程单例和请求级数据不进 registry。
- 补一句：必需步骤直接调用，不经 dispatcher。

3. copilot-instructions.md 的 ECS 一节补 6 行要点，链到 §0（AGENTS 规定细则写在这里）。

4. 审计文档只追加，新增两节：「适用性总评」（链到 §0）和「新发现 N1–N7」，每条一行，写清位置和归属的 S 项。

5. PROGRESS.md 追加条目：Java 版本号写「无，不适用」；注明只改文档、未编译。

不新建 markdown；docs/README.md 不需要改。

### 4.2 S1【W0】ECS 用法棘轮检查脚本（机械守卫，只降不升）

**文件**：
- `tools/scripts/check_ecs_usage.ps1`
- `tools/scripts/ecs_usage_baseline.json`

沿用 check_no_raw_pointer_member.ps1 的 pwsh 风格，新建 check_ecs_usage.ps1。

扫描范围：
- 包含 cpp/libs、cpp/nodes、tools/proto_generator/protogen/internal/template 下的 *.h / *.hpp / *.cpp / *.cc / *.tmpl；
- 排除 third_party、cpp/generated、cpp/tests、cpp/libs/engine/muduo_windows。

规则（按文件计数）：
- R1：`\b(globalRegistry|nodeGlobalRegistry|itemRegistry)\b|\b(GlobalEntity|GrpcNodeEntity|ErrorEntity|OperatorEntity)\(\)`。
- R2：`\bget_or_emplace\s*<`。
- R3：`\.(emplace|emplace_or_replace|get|try_get|get_or_emplace|any_of|all_of|remove|view|storage)\s*<\s*(Guid|u?int(8|16|32|64)_t|bool|float|double|std::\w+)\s*[,>]`。
- R4：路径含 /comp/ 的头文件里 `^\s*using\s+\w+\s*=\s*(std::|EntityUnordered)`。
- R5：`\bdispatcher\.(enqueue|update)\b`。
- R6：`\.connect<[^>]+>\(\s*\*this\s*\)`。
- R7：`tlsNodeContextManager\.(GetRegistry|TryGetRegistry|GetAllRegistries)`，只统计 cpp/libs/engine/core/node 与 cpp/libs/engine/core/network 以外的文件。

基线：ecs_usage_baseline.json，格式为 {rule: {relpath: count}}。
- 某文件计数超过基线，或出现新的 (rule, file) 组合，就失败，并用中文打印规则含义和应改用的写法；
- 计数低于基线时提示可以下调；
- `-UpdateBaseline` 只允许下调，加 `-AllowIncrease` 才能上调（须在 PR 里说明）；
- `-SelfTest` 用内嵌样例校验每条正则。

首次基线由 Codex 生成（见 tests）。本步不接入 Directory.Build.targets，避免影响构建；要不要接入留待后续。

### 4.3 S7【W0】删除引擎层读 actorRegistry 的死函数

**文件**：
- `cpp/libs/engine/core/network/player_message_utils.h`
- `cpp/libs/engine/core/network/player_message_utils.cpp`
- `cpp/libs/engine/core/network/node_message_utils.h`
- `cpp/libs/engine/core/network/node_message_utils.cpp`

删除以下函数（全仓无外部调用方，已用 git grep 在 cpp/libs、cpp/nodes、cpp/tests、cpp/generated、tools 中核对）：
- SendMessageToPlayerOnSceneNode（2 个重载）
- SendMessageToPlayerOnNode（2 个）
- CallMethodOnPlayerNode（2 个）
- ResolvePlayerNodeSession（F007）
- SendMessageToPlayerViaClientNode（2 个）
- SendMessageToPlayerViaSessionNode（2 个）
- 只被上述函数使用的 static / 匿名命名空间辅助函数（ResolvePlayerSession、ResolveNodeClient、ResolveNodeSession、ResolvePlayerSessionSnapshot 等）。

删除前逐个 git grep 确认只命中定义。保留 SendMessageToClientViaGate 和 BroadcastMessageToPlayers 的全部重载，它们还有 28 处调用。

执行前先 git diff 这 4 个文件。若 muduo RPC 会话（docs/design/muduo-rpc-channel.md 第 6 批「玩家回包死链」）已经在改，就只删它没动的函数，并在交付说明里注明。

### 4.4 S9【W0】删除死 ECS 类型和孤儿测试

**文件**：
- `cpp/libs/modules/scene/comp/scene_node_comp.h`
- `cpp/libs/engine/core/utils/registry/game_registry.h`
- `cpp/libs/engine/core/utils/registry/game_registry.cpp`
- `cpp/libs/modules/mission/comp/mission_comp.h`
- `cpp/tests/scene_test/scene_test.cpp`
- `cpp/tests/scene_test/game_server_load_balance.vcxproj`
- `cpp/tests/scene_test/game_server_load_balance.vcxproj.filters`
- `cpp/tests/scene_test/game_server_load_balance.sln`
- `cpp/libs/engine/thread_context/ecs_context.h`
- `cpp/libs/engine/thread_context/ecs_context.cpp`

1. scene_node_comp.h：
- 删除 MainSceneNode、CrossMainSceneNode、SceneSceneNode、CrossSceneSceneNode、SceneRegistryComp、NodeStateComp、NodePressureComp、SceneNodePlayerStatsPtrComp；生产代码各只命中定义处，SceneRegistryComp 和 SceneNodePlayerStatsPtrComp 只被孤儿测试使用。
- 只保留 ScenePlayers，S2a 再改名。
- 删掉因此不再需要的 include。删 node_constants.h 前先 grep 确认 NodeState / NodePressureState 在别处还有没有用。

2. game_registry.h / .cpp：
- 删除 EventOwner、CreateEntityIfNotExists、RecreateEntity（F016）；
- 保留 DestroyEntity；
- `namespace entt { to_entity }` 暂时保留，它的调用点改造属于第 4 批（F017），在它上方加注释说明。

3. mission_comp.h：MissionsComp 去掉 `: public EventOwner`（F026），删除不再需要的 game_registry.h include。

4. 删除整个 cpp/tests/scene_test 目录：它引用已不存在的 scene/system/scene.h、scene_node_selector.h，也不在 game.sln 和任何 CMakeLists 里。删除前再 grep 一次 game.sln 和 CMake，确认没有引用。

5. 条件执行（避免和第 2 批重叠）：仅当第 2 批的范围不含 ecs_context 时，才删除 itemRegistry、ErrorEntity / OperatorEntity 及对应成员（F020 / F021），container_layout.h:18 的注释也一并改掉；否则跳过 ecs_context.*。

### 4.5 S5【W0】任务链从「重入事件」改为「返回待办 + 显式驱动」

**文件**：
- `cpp/libs/modules/mission/system/mission.h`
- `cpp/libs/modules/mission/system/mission.cpp`
- `cpp/libs/services/scene/player/system/player_mission.h`
- `cpp/libs/services/scene/player/system/player_mission.cpp`
- `cpp/nodes/scene/handler/event/mission_event_handler.cpp`
- `cpp/tests/missions_test/missions_test.cpp`
- `cpp/tests/bag_test/player_mission_system_test.cpp`

1. mission.h 新增：
```cpp
struct MissionFollowUps {
    std::vector<uint32_t> autoGrant;    // 按配置要自动发奖的任务
    std::vector<uint32_t> chainAccept;  // 要自动接取的后续任务
    std::vector<uint32_t> completed;    // 本次完成的任务，用于推进「完成任务 X」条件
    bool Empty() const;
};
```

2. 签名改为：
- `MissionSystem::HandleConditionEvent(const ConditionEvent&, MissionsComp&, const IMissionConfig&, MissionFollowUps& out, uint32_t onlyMissionId = 0)`
- `OnMissionCompletion(..., MissionFollowUps& out)`
删掉 mission.cpp 末尾 3 段 `tlsEcs.dispatcher.trigger` 循环，改为填写 out。libs/modules 层从此不再借 dispatcher 反向调用 services 层。OnAcceptedMissionEvent 属于通知型事件，保持不动。

3. PlayerMissionSystem 新增两个函数：
- `void ApplyFollowUps(entt::entity player, uint32_t scope, const MissionFollowUps&, uint32_t depth = 0)`
  - 处理顺序与现状一致：先逐个 ClaimReward（失败时保留可领位，LOG_WARN），再逐个 Accept，最后对 completed 构造 kConditionCompleteMission 的条件并走 OnCondition。
  - Accept 或 OnCondition 新产生的 follow-ups 立即递归处理（深度优先，与原来同步 trigger 的顺序等价）。
  - 新常量 `kMaxMissionFollowUpDepth = 32`。超过时 LOG_ERROR，写明 player Guid、missionId 和深度，然后停止。原先遇到配置环会无界重入。
- `void OnCondition(entt::entity player, const ConditionEvent&)`
  - 做 valid 校验、跨 zone 冻结检查、取 MissionsContainerComp → HandleConditionEvent → ApplyFollowUps。

4. 已有的 Accept 内两处 HandleConditionEvent 调用（player_mission.cpp 约 152 / 179 行）改为收集 follow-ups，再 ApplyFollowUps。

5. mission_event_handler.cpp（只改守护段）：
- ConditionEventHandler 改为调用 PlayerMissionSystem::OnCondition。player_battle 的击杀事件和 player_event_handler 的升级事件仍然 trigger ConditionEvent，这两处不改。
- AcceptMissionEventHandler / OnMissionAwardEventHandler 改为只 LOG_WARN 并忽略，注释写明：生产代码已不再触发，仅 Kafka 入口可达，删除留给第 4 批白名单。

6. 不改 proto，不删事件定义。

### 4.6 S8a【W0】AnomalyDetector 侧表改为玩家组件

**文件**：
- `cpp/libs/modules/transaction_log/anomaly_detector.h`
- `cpp/libs/modules/transaction_log/anomaly_detector.cpp`
- `cpp/tests/currency_test/anomaly_detector_test.cpp`
- `cpp/tests/currency_test/currency_test.vcxproj`
- `cpp/tests/currency_test/CMakeLists.txt`

1. 把私有的 PlayerBuckets 提升为组件：
```cpp
struct AnomalyBucketsComp {
    std::unordered_map<uint32_t, PlayerAnomalyBucket> currencies;
    std::unordered_map<uint32_t, PlayerAnomalyBucket> items;
};
```
删除 `thread_local buckets_` 和自定义的 EntityHash（F030）。

2. RecordCurrencyGain / RecordItemGain 先做 valid 校验，再用显式的 try_get + emplace 取组件（模块自有的可选组件，按需创建）。只读查询 GetCurrencyGainCount 用 try_get，取不到返回 0。

3. 清理接口：
- ClearPlayer 改为 remove<AnomalyBucketsComp>，签名不变，player_lifecycle 的调用照旧有效，S3 再删除这处调用；
- ClearAll 改为 actorRegistry.clear<AnomalyBucketsComp>()。
- 实体销毁时数据自动回收（F064）。

4. 阈值表（currencyThresholds_ / itemThresholds_ / defaultThreshold_）是进程级配置，保持 thread_local。

5. 新测试放进 currency_test 工程：新建测试文件，并登记到 vcxproj 和 CMakeLists。如果第 1 批此时也在改 currency_test 的工程文件，本步整体推迟到 W1。

### 4.7 S3【W1：第 1 批与入场链会话落地后】玩家 / 战斗 actor 原型工厂，必需初始化改为直接调用

**文件**：
- `cpp/libs/services/scene/actor/archetype/combat_actor_archetype.h`
- `cpp/libs/services/scene/actor/archetype/combat_actor_archetype.cpp`
- `cpp/libs/services/scene/player/system/player_archetype.h`
- `cpp/libs/services/scene/player/system/player_archetype.cpp`
- `cpp/libs/services/scene/player/system/player_lifecycle.cpp`
- `cpp/nodes/scene/handler/event/actor_event_handler.cpp`
- `cpp/nodes/scene/handler/event/player_event_handler.cpp`
- `cpp/libs/services/scene/CMakeLists.txt`
- `cpp/libs/services/scene/scene.vcxproj`
- `cpp/libs/services/scene/scene.vcxproj.filters`
- `cpp/tests/cross_zone_test/cross_zone_test.cpp`

1. combat_actor_archetype：
- `void EmplaceCombatComps(entt::registry&, entt::entity actor)`：以第 1 批合入后 InitializeActorCompsEventHandler 和各系统「try_get + LOG_ERROR」所依赖的必需组件为准，原样搬进来，不增删语义。当前至少有 CooldownTimeListComp、SkillContextCompMap。
- `bool HasCombatComps(const entt::registry&, entt::entity)`：用 all_of 判断。

2. player_archetype：
- `void EmplaceRuntimeComps(entt::registry&, entt::entity, Guid playerId, uint32_t currentFrame)`：集中放 InitPlayerFromAllData 里现有的 emplace，即 Player、Guid（改名属第 4 批）、LastActiveFrameComp、PlayerHomeZoneComp、PlayerOwnerEpochComp、ViewRadius(10)。
- `bool Validate(const entt::registry&, entt::entity)`：必需持久化组件的集合与第 1 批 FindPersistedComp 的清单一致；再加上运行时组件和 HasCombatComps。失败时 LOG_ERROR，逐个列出缺了哪个组件。

3. InitPlayerFromAllData 改为：
- (a) 先查 playerList 是否已有该 playerId，再 create 实体，修 F070 的孤儿实体；
- (b) EmplaceRuntimeComps → Unmarshal → EmplaceCombatComps → PlayerMissionSystem::Initialize(player) → 首次注册时直接调 PlayerSkillSystem::RegisterPlayer(player)；删除 RegisterPlayerEvent、InitializeActorCompsEvent、InitializePlayerCompsEvent 三处 trigger；
- (c) Validate 失败则 fail-closed：DestroyEntity，erase playerList，返回 entt::null，和第 1 批「存盘缺组件拒绝」的口径一致；
- (d) 删掉 AnomalyDetector::ClearPlayer 调用（S8a 之后已自动回收）。

4. 事件 handler（只改守护段）：InitializeActorCompsEventHandler、InitializePlayerCompsEventHandler、RegisterPlayerEventHandler 的函数体改为 LOG_WARN 并忽略，注释写明：初始化已改由原型工厂直接调用，本事件只有 Kafka 入口可达。npc_event_handler 本来就是空操作，不动。

5. 修改 player_lifecycle.cpp 前先 git diff，不覆盖入场链会话的改动。

### 4.8 S8b【W1：第 1 批落地后，与 S3 并行】货币 debts 单一事实来源（与 Java 对齐）

**文件**：
- `cpp/libs/services/scene/player/system/player_database_loader.cpp`
- `cpp/libs/modules/currency/comp/player_currency_comp.h`
- `cpp/tests/currency_test/currency_test.cpp`

1. 在 PlayerDatabaseMessageFieldsUnmarshal 里，紧跟 `PlayerCurrencyComp.LoadFromProto(message.currency())` 之后，对刚 emplace 的 CurrencyComp 调 `clear_debts()`。
- 内存中 CurrencyComp 只保留 values 和 blocked_types；
- debts 的唯一权威是 PlayerCurrencyComp；
- 存盘时 SaveToProto 会先清空再写回，往返不丢数据。

2. grep 所有写入 CurrencyComp 的路径：emplace / replace / CopyFrom 到组件，包括回滚、快照恢复。凡是从 proto 整份恢复 CurrencyComp 的地方，都必须同样做「LoadFromProto + clear_debts」。

3. player_currency_comp.h 的注释改为：CurrencyComp 在内存中永不持有 debts。

4. 效果：GetCurrencyList（player_currency_handler.cpp:82）和背包信息（player_feature_snapshot.cpp:159）的应答不再带登录时的过期 debts，与 Java 的 Wallet.toClient() 一致。这是客户端可见变化，开工前须先等 decisions_needed 第 2 条拍板。

### 4.9 S4【W2：S3 之后】技能施法实例只归施法者，buff 不再共享上下文

**文件**：
- `cpp/libs/services/scene/combat/skill/comp/skill_comp.h`
- `cpp/libs/services/scene/combat/skill/system/skill.cpp`
- `cpp/libs/services/scene/combat/buff/comp/buff_comp.h`
- `cpp/libs/services/scene/combat/buff/system/buff.h`
- `cpp/libs/services/scene/combat/buff/system/buff.cpp`
- `cpp/libs/services/scene/combat/buff/system/buff_impl.cpp`
- `cpp/libs/services/scene/combat/buff/system/modifier_buff_impl.h`
- `cpp/libs/services/scene/combat/buff/system/modifier_buff_impl.cpp`
- `cpp/libs/services/scene/combat/buff/system/motion_modifier_impl.h`
- `cpp/libs/services/scene/combat/buff/system/motion_modifier_impl.cpp`
- `cpp/libs/services/scene/actor/archetype/combat_actor_archetype.cpp`
- `cpp/tests/skill_test/skill_test.cpp`
- `cpp/tests/buff_test/buff_test.cpp`

1. skill_comp.h：用 `struct SkillCastsComp { std::unordered_map<uint64_t, std::shared_ptr<SkillContextComp>> bySkillId; };` 替换 `using SkillContextCompMap = ...`（F043，也符合「类型即语义」）。注释写明：shared_ptr 只用于在一条回调链里拿到不随 erase 失效的快照，禁止存进任何其他实体的组件。

2. skill.cpp：
- 删除 AddTargetSkillContext / RemoveTargetSkillContext 及其调用。目标镜像全仓没有读者，删掉后 F045 的泄漏自然消失。
- GenerateUniqueSkillId 改为只查施法者自己的表。
- 其余 try_get<SkillContextCompMap> 改为 SkillCastsComp。

3. buff_comp.h：
- 新增 `struct BuffSource { uint64_t caster{0}; uint64_t skillId{0}; uint32_t skillTableId{0}; };`。caster 的哨兵口径以第 1 批修 C003 后的约定为准。
- BuffEntry 删除 `SkillContextPtrComp skillContext`，改为 `BuffSource source`。

4. buff.h / buff.cpp / buff_impl.cpp / modifier_buff_impl.* / motion_modifier_impl.*：
- 所有 `const SkillContextPtrComp& abilityContext` 参数改为 `const BuffSource& source`，`BuffSource{}` 代替原来的 nullptr；
- 子 buff 继承父 BuffEntry 的 source；
- skill.cpp 里 TriggerSkillEffect 从上下文构造 BuffSource。

5. combat_actor_archetype.cpp 里的 emplace 改为 SkillCastsComp。

6. 前置检查：确认 player_battle.cpp 的战斗快照不依赖 BuffEntry.skillContext（当前 grep 无命中）。

### 4.10 S2a【W2：S3 之后，与 S4 并行】场景目录：唯一创建 / 销毁入口 + scene_id 索引 + ScenePlayersComp

**文件**：
- `cpp/libs/services/scene/world/scene_directory.h`
- `cpp/libs/services/scene/world/scene_directory.cpp`
- `cpp/libs/modules/scene/comp/scene_comp.h`
- `cpp/libs/modules/scene/comp/scene_node_comp.h`
- `cpp/nodes/scene/handler/rpc/scene_handler.cpp`
- `cpp/nodes/scene/handler/grpc/scene_node_service.cpp`
- `cpp/libs/services/scene/player/system/player_scene.cpp`
- `cpp/libs/services/scene/player/system/player_lifecycle.cpp`
- `cpp/libs/services/scene/CMakeLists.txt`
- `cpp/libs/services/scene/scene.vcxproj`
- `cpp/libs/services/scene/scene.vcxproj.filters`
- `cpp/tests/aoi_test/scene_directory_test.cpp`
- `cpp/tests/aoi_test/aoi_test.vcxproj`
- `cpp/tests/aoi_test/CMakeLists.txt`

1. scene_comp.h：新增 `struct ScenePlayersComp { EntityUnorderedSet players; };`，删除 scene_node_comp.h 里的 `using ScenePlayers`。S9 之后这个头文件已经空了，直接删除，并改掉 include。

2. scene_directory.h（全部 static，状态放在 `tlsEcs.sceneRegistry.ctx()` 里的 `struct SceneIndex { std::unordered_map<uint64_t, entt::entity> byId; }`）：
- `entt::entity Create(const SceneInfoComp& info)`
  - scene_id 为 0 或重复时返回 entt::null，并 LOG_ERROR；
  - 用完整值 emplace SceneInfoComp，然后 emplace ScenePlayersComp，写索引；
  - 最后 trigger OnSceneCreated，trigger 前后不持有组件引用；
  - 约定：SceneInfoComp.scene_id 创建后不可改。
- `bool Destroy(uint64_t sceneId)`：沿用现有两个入口的前置校验和事件顺序。
- `entt::entity Find(uint64_t sceneId)`：O(1)；命中但 `!valid` 时视为不存在。
- `void AttachActor(entt::entity actor, entt::entity scene)` / `void DetachActor(entt::entity actor)`：SceneEntityComp 和 ScenePlayersComp 的唯一写入方。
- 内部 `EnsureInstalled()`：ctx 里没有 SceneIndex 时 emplace，并连接 `on_destroy<SceneInfoComp>` 到一个自由函数，用来删除索引条目。这样即使 registry.clear()（测试里的 tlsEcs.Clear()）也不会留下陈旧索引。registry 析构不发信号，没有退出期风险。

3. scene_handler.cpp 的 CreateScene / DestroyScene 与 scene_node_service.cpp 的 HandleCreateScene / 销毁：
- 删掉各自复制的创建和 view<SceneInfoComp> 全表扫描（F095），只调用 SceneDirectory；
- 删除「Keep this block in sync」注释；
- 请求字段到 SceneInfoComp 的组装留在各自入口；
- 若 gRPC 迁移第 4 批已改写 scene_node_service，就在新代码上做同样的收口。

4. player_scene.cpp（约 183 / 194 行）和 player_lifecycle.cpp（约 2640 / 2836 / 2867 / 3271 行，以执行时的实际位置为准）：ScenePlayers 的读写和场景查找改走 SceneDirectory。DestroyPlayer 的兜底保持语义不变，改为调用 DetachActor。

### 4.11 S2b【W3：S2a、S4 之后】场景句柄强类型：entt::basic_registry<SceneEntity>

**文件**：
- `cpp/libs/engine/core/type_define/scene_entity.h`
- `cpp/libs/engine/thread_context/ecs_context.h`
- `cpp/libs/modules/scene/comp/scene_comp.h`
- `cpp/libs/services/scene/world/scene_directory.h`
- `cpp/libs/services/scene/world/scene_directory.cpp`
- `cpp/libs/services/scene/spatial/comp/teleport_settle_comp.h`
- `cpp/libs/services/scene/player/system/player_exit_intent.h`
- `cpp/libs/services/scene/spatial/system/aoi.h`
- `cpp/libs/services/scene/spatial/system/aoi.cpp`
- `cpp/libs/services/scene/spatial/system/grid.cpp`
- `cpp/libs/services/scene/spatial/system/interest.cpp`
- `cpp/libs/services/scene/spatial/system/movement.cpp`
- `cpp/libs/services/scene/spatial/system/scene_spawn.h`
- `cpp/libs/services/scene/spatial/system/scene_spawn.cpp`
- `cpp/libs/services/scene/spatial/system/nav_query.h`
- `cpp/libs/services/scene/spatial/system/nav_query.cpp`
- `cpp/libs/services/scene/spatial/system/scene_crowd.cpp`
- `cpp/libs/services/scene/player/system/player_scene.cpp`
- `cpp/libs/services/scene/player/system/player_lifecycle.h`
- `cpp/libs/services/scene/player/system/player_lifecycle.cpp`
- `cpp/libs/services/scene/player/system/player_team.cpp`
- `cpp/libs/services/scene/player/system/player_team_aoi.cpp`
- `cpp/libs/services/scene/combat/skill/system/skill.cpp`
- `cpp/nodes/scene/handler/rpc/scene_handler.cpp`
- `cpp/nodes/scene/handler/grpc/scene_node_service.cpp`
- `cpp/nodes/scene/handler/rpc/player/s2s_player_scene_handler.cpp`
- `cpp/nodes/scene/handler/rpc/player/player_scene_handler.cpp`
- `cpp/nodes/scene/handler/event/scene_event_handler.cpp`
- `cpp/tests/aoi_test/aoi_system_test.cpp`
- `cpp/tests/aoi_delivery_test/aoi_delivery_test.cpp`
- `cpp/tests/cross_zone_test/cross_zone_test.cpp`

1. 新建 scene_entity.h（engine 层只认识类型，不带场景语义）：
```cpp
enum class SceneEntity : std::uint64_t {};
using SceneRegistry = entt::basic_registry<SceneEntity>;
static_assert(!std::is_same_v<SceneEntity, entt::entity>);
```
EnTT 3.13 对 enum 实体类型做了特化（third_party/entt/src/entt/entity/entity.hpp:25）。ENTT_ID_TYPE = uint64_t，位布局与 entt::entity 相同。

2. ecs_context.h：`SceneRegistry sceneRegistry;`。

3. 组件字段改类型：
- `SceneEntityComp { SceneEntity scene{entt::null}; }`，字段 sceneEntity 改名为 scene，让编译器找出全部使用点；
- TeleportSettleComp、PlayerExitIntent 里的场景句柄同样改成 SceneEntity。

4. 由编译器驱动：所有接收或保存场景句柄的函数签名、局部变量改为 SceneEntity。例如 aoi 的 OnSceneCreated、FindSceneGridList，skill.cpp 的 IsInSameScene，nav_query、scene_spawn、scene_crowd 等。
- 场景事件 proto 里的 uint64 改为显式 `static_cast<SceneEntity>(msg.entity())`，事件载荷类型的改造属于第 4 批；
- 拿 actor 句柄查 sceneRegistry 的地方（F079）、把 scene_id 当句柄的地方（F091 若仍在）变成编译错误，一律改为 SceneDirectory::Find(sceneId) 或 SceneEntityComp.scene。

5. ECS_GET_OR_* 宏只查 actorRegistry，传入 SceneEntity 会编译失败，这正是期望行为（F005），宏本身不改。

6. 不改任何生成模板。attribute_sync 不涉及场景句柄，已 grep 核对。

### 4.12 S6【W3：S2b 之后，须先拍板 decisions 第 1 条】节点目录第 1 期：只读门面 + 场景层调用点收口

**文件**：
- `cpp/libs/engine/core/node/system/node/node_directory.h`
- `cpp/libs/engine/core/node/system/node/node_directory.cpp`
- `cpp/libs/engine/core/CMakeLists.txt`
- `cpp/libs/engine/core/core.vcxproj`
- `cpp/libs/engine/core/core.vcxproj.filters`
- `cpp/libs/engine/core/network/network_utils.h`
- `cpp/libs/engine/core/network/network_utils.cpp`
- `cpp/libs/engine/core/network/player_message_utils.cpp`
- `cpp/libs/services/scene/player/system/player_lifecycle.cpp`
- `cpp/libs/services/scene/player/system/player_team.cpp`
- `cpp/libs/services/scene/player/system/player_scene.cpp`
- `cpp/nodes/scene/handler/rpc/player/player_scene_handler.cpp`
- `cpp/nodes/scene/rpc_replies/scene_manager_response_handler.cpp`
- `cpp/tests/cross_zone_test/cross_zone_test.cpp`
- `tools/scripts/ecs_usage_baseline.json`

1. node_directory.h：
```cpp
struct NodeRef { uint32_t nodeType{0}; entt::entity entity{entt::null}; };  // 带 version；只在本进程、本线程有效，不序列化
struct NodeView {
    NodeRef ref;
    const NodeInfo* info;      // 必非空
    RpcSession* inbound;       // 可空
    RpcClient* outbound;       // 可空
    bool IsInboundConnected() const;
};
namespace node_directory {
    [[nodiscard]] std::optional<NodeView> Resolve(NodeRef);
    [[nodiscard]] std::optional<NodeView> FindByUuid(uint32_t nodeType, std::string_view uuid);
    [[nodiscard]] std::optional<NodeView> FindByZoneAndNodeId(uint32_t nodeType, uint32_t zoneId, uint32_t nodeId);
    [[nodiscard]] std::optional<NodeView> ResolveLocalZoneGateForSession(SessionId);  // 吸收 ResolveLocalZoneGateEntity 的逻辑，包括 zone 滞后时的兜底
}
```
- 实现仍在 tlsNodeContextManager 的 registry 上做线性扫描（节点只有几十个，KISS）；
- nodeType 一律走 TryGetRegistry，越界返回 nullopt；
- NodeView 的指针只在当前调用栈内有效，禁止跨 dispatcher.trigger 或异步回调持有。

2. 场景层「先解析 gate 实体，再 try_get<RpcSession>，再 try_get<NodeInfo>」的重复模式全部改用 ResolveLocalZoneGateForSession：
- player_lifecycle.cpp：PushToSessionViaGate 及另外两处；
- player_team.cpp 约 474 行；
- player_scene.cpp 约 289 行；
- player_scene_handler.cpp 约 133 行；
- scene_manager_response_handler.cpp 约 155 行；
- player_message_utils.cpp 里的 SendMessageToClientViaGate(entity) 和 BroadcastMessageToPlayers。

3. ResolveLocalZoneGateEntity 保留为薄包装，标注 deprecated。

4. 不碰以下文件：node.cpp、node_connector.cpp、registration_manager.cpp、node_util.cpp、gate 的 client_message_processor / gate_service_handler / router_muduo_forward、etcd_*、id_segment_bootstrap（归 gRPC / muduo 两个迁移会话），以及 player_battle.cpp:888（等第 1 批合入后再顺手改）。

5. 请 Codex 用 `-UpdateBaseline` 把 S1 的 R7 基线下调。

6. 第 2 期只写设计，不在本步实施，写进 S0 的 §0：
- 存储换成 `unordered_map<uuid, NodeRecord>`；
- 生成的 Send* 改为接收通道或 NodeRecord；
- gate 的 SessionInfo 改存 NodeRef（F033）；
- ServiceNodeList 并入，成为「已发现」状态。

## 5. 生成器模板与生成产物

- 没有生成器模板改动，也不需要重新生成。
- S3 / S5 只改生成的事件 handler 文件守护段（///<<< BEGIN WRITING YOUR CODE）内的手写代码，守护段外不动。涉及：actor_event_handler.cpp、player_event_handler.cpp、mission_event_handler.cpp。
- S2b 如果需要修改 scene_event_handler.cpp 里场景句柄的转换，也只改守护段内。attribute_sync 生成物和模板不涉及场景句柄，已 grep 核对。

## 5a. proto 改动

- 无：不新增、删除任何 proto 字段与事件，也不改编号。S3 / S5 停止触发的进程内事件（InitializeActorCompsEvent、InitializePlayerCompsEvent、RegisterPlayerEvent、AcceptMissionEvent、OnMissionAwardEvent）保留定义，删除和 reserved 留给 Kafka 白名单批，按生成器的事件 ID Reserved 机制处理。

## 6. 测试

- 【所有波次，由 Codex 执行；Claude 未编译、未运行】在仓库根目录跑 `msbuild game.sln /m:1 /p:Configuration=Debug /p:Platform=x64`。必须串行 /m:1，否则会报假的 C1041 / LNK1104。新增 .cpp 的那几波（S2a / S3 / S6 / S8a），再跑一次 `tools/scripts/build_linux.sh`，确认 CMake 的 SOURCE_FILES 已登记。
- S1：`pwsh -File tools/scripts/check_ecs_usage.ps1 -SelfTest` 必须通过。首次执行 `-UpdateBaseline -AllowIncrease` 生成 ecs_usage_baseline.json，把 diff 贴进交付说明供审。之后每波跑一次 `pwsh -File tools/scripts/check_ecs_usage.ps1`，退出码 0 才算通过；计数下降时用 `-UpdateBaseline` 下调基线。
- S7 / S9：编译通过即可。另外用 git grep 核对以下名字在 cpp/ 与 tools/ 里零命中：被删函数名、MainSceneNode / SceneRegistryComp / EventOwner / CreateEntityIfNotExists / RecreateEntity。另确认 cpp/tests/scene_test 已不存在，且 game.sln 不引用它。
- S5：跑 `pwsh tools/scripts/run_cpp_tests.ps1 -Filter missions` 和 bag_test 里的 player_mission_system_test。新增用例：
(1) 完成任务后，处理顺序（自动发奖 → 接后续任务 → 推进「完成任务 X」条件）和最终任务状态与改造前一致；
(2) 不调用 MissionEventHandler::Register 时，任务链照样推进；
(3) 配置环 A→B→A 在 kMaxMissionFollowUpDepth 处停止，打 ERROR，不栈溢出；
(4) 直接 trigger AcceptMissionEvent / OnMissionAwardEvent，玩家任务状态不变。
- S8a：currency_test 里新增 anomaly_detector_test。用例：两个玩家的桶互不串；实体 destroy 后组件随之消失，不调用 ClearPlayer 也不残留；槽位复用（同 index、version+1）的新实体从空窗口开始；ClearAll 之后计数归 0。
- S3：在 cross_zone_test / bag_test 的夹具里新增用例：
(1) InitPlayerFromAllData 之后 player_archetype::Validate 为真；
(2) 同一个 player_id 第二次初始化返回 entt::null，并且 actorRegistry 的存活实体数不变（F070）；
(3) UnRegister 全部 event handler 之后，冷却和任务初始化仍然生效；
(4) 构造缺一个必需持久化组件的数据时拒绝入场，实体和 playerList 条目都被回收。
回归：跑 cross_zone_test 全套。
- S8b：在 currency_test 中验证：Unmarshal 之后 CurrencyComp.debts 为空，PlayerCurrencyComp 持有原 debts；Marshal 往返 debts 不丢；GetCurrencyList 和背包信息的应答 currency 里没有 debts（与 Java Wallet.toClient 一致）；回滚 / 快照恢复路径同样不残留 debts。
- S4：skill_test / buff_test。用例：
- 对自己施法并结束，不会重复删除；
- 施法期间施法者被销毁，目标实体上不残留任何施法记录（已经没有镜像）；
- 技能挂上的 buff 在技能结束之后 caster 仍然正确；
- 子 buff 继承父 buff 的 BuffSource；
- 第 1 批的 C003 叠层用例继续通过。
- S2a：在 aoi_test 中新增 scene_directory_test。用例：
- 重复 scene_id 或 scene_id=0 时 Create 返回 entt::null；
- Find 在 Destroy 之后返回 entt::null；
- tlsEcs.Clear() 之后索引没有残留（on_destroy 钩子生效），并且可以用同一个 scene_id 重建；
- AttachActor / DetachActor 正确维护 ScenePlayersComp 和 SceneEntityComp；
- RPC 和 gRPC 两个 CreateScene 入口建出的场景都能被 Find 查到。
回归：aoi_test、aoi_delivery_test 全套。
- S2b：编译通过本身就是主要验证（static_assert 确保 SceneEntity 与 entt::entity 不同）。之后跑 aoi_test、aoi_delivery_test、cross_zone_test、skill_test 全套，必须全绿。
- S6：在 cross_zone_test（已有 gate registry 夹具）中新增用例：
- ResolveLocalZoneGateForSession 返回 info 与 inbound；
- NodeRef 的 version 不匹配时 Resolve 返回 nullopt；
- 节点实体销毁后查不到；
- 非法 nodeType 返回 nullopt，不越界。
同时检查 lint 的 R7 计数确实下降。
- 每一波结束都跑全量 `pwsh tools/scripts/run_cpp_tests.ps1`。失败时保留 gtest 输出摘要和失败用例名，不重试，不跳过测试。本方案不涉及压测，不得宣称任何性能结论。

## 7. 并行拆分、风险与拍板项

**并行拆分**：四波执行。同一波内各子任务的文件互不重叠；各波之间串行。\n\nW0：现在就可以做，不依赖第 1 批，也不碰第 1 批、gRPC 迁移和 muduo RPC 会话的文件。下面 6 个任务可以并行：\n- A = S0，只改 docs/ 和 .github/copilot-instructions.md；\n- B = S1，tools/scripts/check_ecs_usage.ps1 与基线；\n- C = S7，player_message_utils.* 与 node_message_utils.*；\n- D = S9，scene_node_comp.h、game_registry.*、mission_comp.h、tests/scene_test，ecs_context.* 按条件执行；\n- E = S5，mission.h/.cpp、player_mission.h/.cpp、mission_event_handler.cpp 和任务测试。E 不得改 mission_comp.h，那个文件归 D；\n- F = S8a，anomaly_detector.* 与 currency_test 新测试文件。如果第 1 批还在改 currency_test 的工程文件，F 顺延到 W1。\n\nW1：第 1 批合入、入场链会话落地之后开始。下面 2 个任务并行：\n- G = S3，独占 player_lifecycle.cpp、actor_event_handler.cpp、player_event_handler.cpp 和新增的 archetype 文件；\n- H = S8b，player_database_loader.cpp、player_currency_comp.h、currency_test.cpp。\n\nW2：S3 之后开始。下面 2 个任务并行：\n- I = S4，skill_comp.h、skill.cpp、buff_comp.h、buff*.cpp/h、modifier / motion_modifier、combat_actor_archetype.cpp、skill_test、buff_test；\n- J = S2a，新增 scene_directory、scene_comp.h、scene_handler.cpp、scene_node_service.cpp、player_scene.cpp、player_lifecycle.cpp、aoi_test。\nI 与 J 文件不重叠；J 这时是 player_lifecycle.cpp 唯一的写入方。\n\nW3：严格串行。先做 K = S2b（需要 S2a 和 S4 都完成，因为它要改 skill.cpp 和 scene_directory），再做 L = S6（需要 S2b 完成，并且用户已拍板 decisions 第 1 条，因为它要改 player_lifecycle、player_team、player_scene）。\n\n热点文件 player_lifecycle.cpp 依次只归 G → J → K → L 中的一个任务。每个实施代理动手前先 git diff 自己要改的文件，不覆盖其他会话尚未提交的改动；交接中写的提交状态可能已过时，以实际 diff 为准。

**风险**：
- 第 2 / 4 / 5 批的实际定义本方向没有拿到，是按审计的五个根源推断的。可能重叠的地方：S9 与第 2 批（ecs_context 死成员）、S3 与第 5 批（组件创建约定）、S2b 与第 3 / 4 批（registry 划分、句柄类型）、S5 与第 5 批（dispatcher）。需要编排方去重，以批次为准。
- player_lifecycle.cpp 超过 5000 行，入场链会话正在大改它，S3 / S2a / S2b / S6 也都要改它，必须严格串行。另一个会话的未提交改动可能被覆盖：每步动手前先 git diff，只做最小改动。
- S3 的 Validate 采用 fail-closed，必需组件的集合一旦定宽了，正常玩家会被拒绝入场。集合必须和第 1 批 FindPersistedComp 的清单严格一致，并用 cross_zone_test 夹具覆盖新号、老号和跨 zone 落地三条路径。
- S5 把无界重入改为深度上限 32。若某些合法任务链本来就超过 32 层，会被截断并打 ERROR。上线前要用配置表统计最长链路深度。处理顺序保持深度优先，与原来的同步 trigger 等价，但每一步都必须由测试比对。
- S4 修改 BuffSystem 的公开签名，而第 1 批正在改 buff.cpp（C003）。必须等第 1 批合入后执行。BuffSource 的 caster 哨兵要沿用第 1 批定下的口径，否则会重新引入叠层分叉。
- S2a 依赖 ctx 里的 SceneIndex 和 on_destroy 钩子，并要求「SceneInfoComp.scene_id 创建后不可改」。任何直接 mutate scene_id 的代码都会让索引失真，需要 grep 确认没有这种写法，并写进注释。
- S2b 是由编译器驱动的大面积签名修改，约 25 个文件。如果 gRPC 迁移第 4 批（服务端 handler 改写）正在改 scene_node_service.cpp，会和它冲突，要和那个会话约定先后。
- S6 和 gRPC 迁移、muduo RPC 两个会话改的是同一片节点代码，没拍板就实施会三方冲突。另外，NodeView 里的裸指针只能在当前调用栈内使用，误用会悬空，需要在 review 时重点检查。
- S8b 是客户端可见变化：GetCurrencyList 和背包信息不再下发 debts。目前没有授权读客户端仓库，无法确认客户端有没有读这个字段。
- S1 是基于正则的检查，会有误报（例如注释里出现 dispatcher.enqueue），靠基线吸收。它不接入构建，约束力依赖 Codex 和 CI 每波都去跑。
- 删除 tests/scene_test 只是清理不参与构建的孤儿代码。如果负责人想把它留作历史参考，可以改为移到 docs/archive，属于低风险。
- S3 / S5 停止触发的那些进程内事件（InitializeActorCompsEvent 等），在 Kafka 白名单批落地之前仍然可以从 Kafka 投递进来。本方案把它们的 handler 改成了忽略，这比现状更安全，但事件定义还在；如果第 4 批要删除它们，需要按生成器的 Reserved 机制保留 ID。

**需负责人拍板**（未拍板前不要实施相关步骤，其余步骤照常进行）：
- 节点表（NodeContextManager）移出 ECS、改为 NodeDirectory：要不要立项？第 1 期（S6：只读门面 + 场景层收口）现在做，还是等 gRPC 迁移第 3 批和 muduo RPC 第 3 批之后再做？第 2 期（替换存储、生成的 Send* 不再接收 registry + entity、gate SessionInfo 存 NodeRef）由哪个会话承担，还是联合立项？
- S8b：C++ 目前在 GetCurrencyList 和背包信息里，把登录时的过期 debts 随 CurrencyComp 下发给客户端，Java 版不下发。方案是对齐 Java、不再下发。这是客户端可见变化，需要确认客户端没有读这个字段（未获授权，不能读 ../mmorpg-client）。
- 技能相位和 buff 到期要不要从 muduo 定时器组件（回调 + in_place_delete）改为「截止时间数据 + 每帧系统」？这会把时间精度从毫秒变为一帧，影响玩法手感，Java 版也需要同批对齐。本方案不做，拍板后再另立项。
- 本方案约 90 个文件，超过 AGENTS §10.2 规定的 30 个文件门槛。请按 W0 / W1 / W2 / W3 逐波授权；W1 及以后还要等第 1 批和入场链会话落地。

**Java 版（AGENTS §12）**：Java 版（D:/luyuan/xuanming-server-mmo）不用 entt。本方案除 S8b 外，都是 C++ 服务端内部的结构调整，不改客户端契约、proto 和 tip 码，Java 不需要对应改动。实施时在 Java 仓库的 PARITY.md 登记一行「ECS 适用性重构：不适用」（跨仓写入需用户同意）。

S8b 例外：C++ 停止随 CurrencyComp 下发登录时的过期 debts，结果与 Java 的 Wallet.toClient()（game-scene/.../player/Wallet.java:250，只发 values 和 blocked_types）一致，属于 C++ 向 Java 对齐，应在 PARITY.md 登记为「C++ 对齐 Java」。

可选的「定时器改每帧」若立项，属于客户端可见的时间行为，两版要同批改。审计遗留的两条客户端可见行为（技能目标必须同场景、no_caster buff 跨施法者叠层）不在本方案范围内，Java 是否等价仍待核对。

## 8. 挑战者意见（实施时必须处理）

结论：**needs_changes**

- **[high]** S0 / S1 / S8a 依据的是旧版规范文档。工作区里 docs/design/ecs-component-access-rules.md、ecs*.md、.github/copilot-instructions.md 已被另一会话改写，尚未提交。新版本删掉了「Acceptable uses」和「Problematic uses」两节，所以 S0 第 1 点要改写或删除的内容已经不存在。新版规则 2 明确允许在「按需状态组件的唯一写入口」用 get_or_emplace，并把 SkillContextCompMap 列为按需状态；新版派生数据一行还写着创建点是 InitializeActorCompsEvent。这和方案冲突：S1 的 R2 把所有 get_or_emplace 算作只降不升的违规；S8a 要求写成 try_get + emplace；S3 / S4 把 SkillContextCompMap 定为原型必需组件；S3 又要删除 InitializeActorCompsEvent。ecs_zh.md 也已经去掉了「允许单例 Component」。另外，S0 在 W0「现在就做」，会直接覆盖这些未提交的改动。
  - 修法：S0 改为在工作区新版文档上增量修改，先和改写这些文档的会话（很可能是第 1 批）对齐，并调整 W0 的先后顺序。§0 的结论要和新版「三种初始化方式」统一：明确 SkillContextCompMap / CooldownTimeListComp 归哪一类，同步更新表中「InitializeActorCompsEvent」的表述。S1 的 R2 要么去掉，要么只统计读路径 / 每帧路径；否则基线会和已成文的「按需组件写入口用 get_or_emplace」规则冲突。S8a 的写法按新规则统一。
- **[high]** S7 的工作已由 muduo RPC 会话（muduo-rpc-channel.md 第 1 批「清理」）在工作区完成，只是还没提交：node_message_utils.h / .cpp 已经 git rm 暂存删除，player_message_utils.h / .cpp 净删 221 行，对 SendMessageToPlayerOnSceneNode / CallMethodOnPlayerNode / ResolvePlayerNodeSession 等名字 grep 全仓零命中。S7 的文件清单里有两个已不存在的文件。W0 的任务 C 还会去改另一会话未提交的 player_message_utils.*。
  - 修法：删掉 S7（W0 的任务 C）。F007 / N4 在 findings_covered 和审计追记中改记为「由 muduo RPC 第 1 批覆盖」。S6 里涉及 player_message_utils.cpp 的部分，也要等 muduo RPC 会话提交后再确定。
- **[medium]** S5 的深度上限实际不起作用。按方案，PlayerMissionSystem::Accept 内部会自己调 ApplyFollowUps，而 depth 默认为 0；ApplyFollowUps 处理 chainAccept 时又会调 Accept。这样每经过一次 Accept，深度就归零，kMaxMissionFollowUpDepth=32 拦不住经由接取形成的链。另外，「原先遇到配置环会无界重入」的说法不成立：CheckAccept（player_mission.cpp 约 101 行）会以 kMissionAlreadyCompleted 拒绝已完成的任务，A→B→A 在第 2 层就被拒绝了。测试用例 (3) 因此根本到不了深度上限，测不到要测的东西。
  - 修法：拆成内部的 AcceptImpl(player, scope, missionId, nowMs, MissionFollowUps& out)，只收集 follow-ups，不在内部驱动。由公开的 Accept 或 OnCondition 作为唯一驱动入口调用 ApplyFollowUps，depth 一路往下传。改正关于「无界」的说法。测试 (3) 改为构造一条每一步都能立即完成的长链（超过 32 层）来验证截断。
- **[medium]** S5 改了 MissionSystem::HandleConditionEvent 的签名（在默认参数前插入 MissionFollowUps& out），但漏掉了一个直接调用方：cpp/tests/bag_test/player_feature_handler_test.cpp:159 的 MissionSystem::HandleConditionEvent(kill, Missions(), MissionConfig::GetSingleton())。bag_test 会编译失败，而 S5 的文件清单里没有这个文件。
  - 修法：把 player_feature_handler_test.cpp 加进 S5 的文件清单，改为传入 follow-ups 后调用 ApplyFollowUps，或改走 PlayerMissionSystem::OnCondition。也可以在 MissionSystem 上保留一个不带 out 的薄重载，只供测试使用，但要注明它不驱动后续任务。
- **[medium]** S3 让 Validate 失败时 fail-closed 返回 entt::null，但唯一的生产调用方 HandlePlayerAsyncLoaded（player_lifecycle.cpp:2067）不看返回值。此时 tlsPendingEnterMap 的条目已经 erase，玩家会一直卡在入场中：没有拒绝回包，gate / 客户端 / Go 侧的入场锁也不会释放。同一函数里的 RefuseInheritedEntry 已有现成的拒绝路径可以参照。另外，F070（先查重再 create）在工作区已经修好（player_lifecycle.cpp:3809-3816），回归测试在 player_feature_persistence_test.cpp:577，S3 的 (a) 和测试 (2) 是重复工作。
  - 修法：S3 写明失败要通知：复用 RefuseInheritedEntry 或现有的入场失败回执，向 gate / 客户端返回明确的错误码，并释放入场状态，同时补一条「Validate 失败 → 客户端收到拒绝、会话不悬挂」的用例。删掉 (a) 和测试 (2)，F070 改记为已修。
- **[medium]** S2a 的 SceneDirectory::Create 会把现有 CreateScene 的幂等语义改坏。scene_handler.cpp:860-872 和 scene_node_service.cpp:22-36 遇到重复 scene_id 时都返回已有的 scene_info，按成功处理（调用方会重试）。方案改成「重复时返回 entt::null 并 LOG_ERROR」，没有要求入口先 Find 再原样回包。Create 也没有写明要保留两处现有逻辑：监听者删掉 SceneInfoComp 时补发 OnSceneDestroyed 并销毁实体的回滚（scene_handler.cpp:899-920），以及 Agones 的 permit 校验。另外，Destroy 要「沿用前置校验」，就得调用 PlayerLifecycleSystem::BeginSceneDrain；而 player_lifecycle 又要调用 SceneDirectory::AttachActor / DetachActor，world 和 player 两层会互相依赖。
  - 修法：入口先调 SceneDirectory::Find，命中就回已有 info，保持幂等；Create 里的重复检查只作防御，日志降为 WARN。把回滚分支写进 Create 的契约。Destroy 只负责「发 OnSceneDestroyed + 销毁 + 维护索引」，排空判断（BeginSceneDrain）留在两个入口或另一个编排函数，避免模块循环依赖。改动只限守护段内。
- **[medium]** S2a 漏改了几处 scene_id 查找和 ScenePlayers 的使用者，删头文件会导致编译失败。(1) 第 1 批在 s2s_player_scene_handler.cpp:18-32 新加了一份 FindSceneEntityBySceneId 全表扫描（F095 的又一份拷贝），S2a 没收口它。(2) cross_zone_test.cpp:2498 include 了 scene_node_comp.h，3766-3769 的 PlaceInScene 直接 emplace<ScenePlayers> 并给 sceneEntity 赋值；S2a 删掉这个头并改名后，cross_zone_test 编不过，而它不在 S2a 的文件清单里（W2 时也没有别的任务持有它）。(3) modules.vcxproj:62 有 ClInclude scene_node_comp.h，删头文件不改工程文件会导致构建报错。
  - 修法：把 s2s_player_scene_handler.cpp（改用 SceneDirectory::Find，只改守护段）、cross_zone_test.cpp（PlaceInScene 改走 SceneDirectory::Create / AttachActor，或改成 ScenePlayersComp）、modules.vcxproj 加进 S2a。同时写明：直接 create 场景实体的测试夹具不会进 SceneIndex，凡是会走 Find 的用例必须改用 SceneDirectory::Create。
- **[medium]** S2b 说「PlayerExitIntent 里的场景句柄同样改成 SceneEntity」，这是错的。player_exit_intent.h:73 存的是 uint64_t sceneIdAtExit，即 SceneManager 发的 SnowFlake scene_id，不是 sceneRegistry 句柄。按方案机械地改，会把持久 ID 和进程内句柄混为一谈，也就是 F091 那类错误。S2b 还漏列了几处使用场景句柄的文件：player_scene.h:10（HandleEnterScene(entt::entity player, entt::entity scene)）、skill_test.cpp:46-53 / 103-117、bag_test/scene_spawn_test.cpp:26-34、aoi_test/interest_system_mock.cpp:22-49（读 sc->sceneEntity）、ecs_context.cpp。
  - 修法：从 S2b 删掉 player_exit_intent.h，并注明 sceneIdAtExit 是 scene_id，保持 uint64。把 player_scene.h 和上述测试文件补进清单，W3 时它们都不归其他任务。
- **[medium]** S6 第 1 期要收口的调用点，和 muduo RPC 第 5 批（§8「全部调用点」，要求「目标绑定到具体 RpcClient / RpcSession 实例」，RpcSession 改为持 weak_ptr<RpcChannel>）是同一批代码：PushToSessionViaGate、SendMessageToClientViaGate、scene_manager_response_handler 等。player_message_utils.cpp 现在正被该会话改动，还没提交。NodeView 暴露 RpcClient* / RpcSession* 裸指针，而这两个类型会被那一批改造。另外 scene_handler.cpp:790 也调用了 ResolveLocalZoneGateEntity，S6 没列。
  - 修法：把 S6 的前置条件写成「muduo RPC 第 5 批合入之后」，而不只是「等拍板」。NodeView 的出站 / 入站字段按那时的通道类型（RpcChannel 的 weak_ptr 或 lock 后的引用）定义。scene_handler.cpp:790 一并纳入，或注明它只做存在性检查、不改。
- **[low]** S4 用 BuffSource{} 替代 nullptr 时，没有写清「没有施法上下文」怎么判断。buff.cpp:330 有 `buffRow->no_caster()==0 && !abilityContext → return false`：按施法者隔离、又没有上下文的 buff，每次都新建一条。kNoBuffCaster 也等于 0（buff.cpp:56）。如果实施者只按 caster 字段判断，或者把 BuffSource{} 当成普通施法者，叠层语义就会变。此外 skill.h:15 的注释也提到 SkillContextCompMap，S4 的文件清单没列。
  - 修法：给 BuffSource 加一个显式谓词，例如 bool HasCaster() const { return caster != 0; }（依据 F048：0 号句柄已退役），把 `!abilityContext` 一一对应地改成 `!source.HasCaster()`，并补用例「无上下文、按施法者隔离的 buff 不叠层」。skill.h 加入清单。
- **[low]** S3 把创建点从 ActorEventHandler 挪到 combat_actor_archetype，但没有同步改那些写明「唯一创建点是 ActorEventHandler::InitializeActorCompsEventHandler」的注释：actor_action_state.cpp:50、actor_action_state.h:6、actor_attribute_calculator.cpp:30/76、buff.cpp:143、skill_comp.h:14、skill.cpp:120/576、skill.h:15，以及 buff_test.cpp:72、player_feature_handler_test.cpp:275。其中 actor_action_state.* 和 actor_attribute_calculator.cpp 不在任何 S 步骤里，过时的注释会一直留下，「单一权威来源」被破坏。
  - 修法：S3 的文件清单补上这些注释所在的文件：第 1 批合入后由 S3 统一改，或由 S4 顺带改它持有的 skill / buff 文件。也可以在 S3 里加一条要求：grep「InitializeActorCompsEventHandler」，确认零命中。
- **[low]** 测试工程的文件清单与实际不符。cpp/tests/currency_test/ 和 cpp/tests/aoi_test/ 下都没有 CMakeLists.txt，测试只靠 vcxproj 构建，S8a 和 S2a 却要求「登记到 CMakeLists」。aoi_test.vcxproj.filters 存在但没列。S2a 的用例「RPC 和 gRPC 两个 CreateScene 入口建出的场景都能被 Find 查到」放在 aoi_test 里跑不起来：aoi_test 只编 aoi_system_test.cpp 并链接 scene.lib，没有编译 scene_handler.cpp / scene_node_service.cpp，这两个文件还依赖 Agones 和 gRPC 服务类。
  - 修法：清单改为 vcxproj（aoi_test 再加 .filters），去掉 CMakeLists。两个入口的对比用例要么删掉，要么换到会编译节点 handler 的测试工程；也可以只测 SceneDirectory，入口一致性靠 code review 保证。
- **[low]** S9 删除 cpp/tests/scene_test 之后，tools/scripts/run_cpp_tests.ps1:39 的注释还在把 scene_test 列为「死工程」，会变成过时说明。
  - 修法：把 run_cpp_tests.ps1 的这行注释加入 S9 的文件清单，删掉 scene_test 那一项。
- **[low]** S8b 的「内存中 CurrencyComp 永不持有 debts」要靠今后每条写路径都记得 clear_debts 来维持。客户端契约真正生效的地方只有两处出口：player_currency_handler.cpp:85 和 player_feature_snapshot.cpp:160 的 CopyFrom。
  - 修法：在 Unmarshal 里 clear_debts 的同时，在这两个出口 CopyFrom 之后也调用 clear_debts()（或改成只拷 values 和 blocked_types，与 Java 的 Wallet.toClient 一致），客户端契约就不再依赖内存不变量。再加一条用例覆盖。

**遗漏的调用点**：
- cpp/tests/bag_test/player_feature_handler_test.cpp:159 —— 直接调用 MissionSystem::HandleConditionEvent，S5 改签名后编译失败
- cpp/nodes/scene/handler/rpc/player/s2s_player_scene_handler.cpp:18-32 —— FindSceneEntityBySceneId，第 1 批新增的又一份 scene_id 全表扫描，S2a 未收口
- cpp/tests/cross_zone_test/cross_zone_test.cpp:2498,3766-3769 —— include scene_node_comp.h、emplace<ScenePlayers>、给 sceneEntity 赋值，S2a 删头文件并改名后编译失败
- cpp/libs/modules/modules.vcxproj:62 —— ClInclude scene_node_comp.h，S2a 删头文件后须同步删除
- tools/scripts/run_cpp_tests.ps1:39 —— 注释把 scene_test 列为死工程，S9 删目录后过时
- cpp/libs/services/scene/player/system/player_scene.h:10 —— HandleEnterScene(entt::entity player, entt::entity scene)，S2b 未列
- cpp/tests/skill_test/skill_test.cpp:46-53,103-117 —— 直接 create sceneRegistry 实体并构造 SceneEntityComp{scene}，S2b 未列
- cpp/tests/bag_test/scene_spawn_test.cpp:26-34 —— 直接创建场景实体，S2b 未列
- cpp/tests/aoi_test/interest_system_mock.cpp:22-49 —— 读 sc->sceneEntity 并查 sceneRegistry，S2b 未列
- cpp/libs/services/scene/player/system/player_exit_intent.h:73 —— sceneIdAtExit 是 SnowFlake scene_id，不是句柄，S2b 不应改它
- cpp/libs/services/scene/player/system/player_lifecycle.cpp:2067 —— 不检查 InitPlayerFromAllData 的返回值，S3 fail-closed 后入场会悬挂
- cpp/libs/services/scene/combat/buff/system/buff.cpp:330 —— 依赖「无上下文」语义（!abilityContext），S4 改成 BuffSource 后需显式谓词
- cpp/libs/services/scene/actor/action_state/system/actor_action_state.cpp:50 / actor_action_state.h:6 / actor/attribute/system/actor_attribute_calculator.cpp:30,76 / combat/skill/system/skill.h:15 —— 注释写着唯一创建点是 InitializeActorCompsEventHandler，S3 后过时
- cpp/nodes/scene/handler/rpc/scene_handler.cpp:790 —— 调用 ResolveLocalZoneGateEntity，S6 未列
- docs/design/ecs-component-access-rules.md（工作区未提交的改写）—— S0 / S1 / S8a 需基于新版「三种初始化方式」修订
- cpp/libs/engine/core/network/node_message_utils.h/.cpp —— 已在工作区被 muduo RPC 第 1 批删除（暂存），S7 清单失效

## 9. 回执

按 [README.md](README.md) §6 的格式交回。另外逐条说明 §8 每个 high/medium 问题是怎么处理的。
