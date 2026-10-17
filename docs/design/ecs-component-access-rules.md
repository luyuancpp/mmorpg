# ECS Component Access Rules

## Overview

entt uses a **sparse-set** storage model. Each component type has its own packed array. Entities that do not have a component use **zero memory** in that pool and are never visited by `view<T>` iteration. This is a core design advantage for instances/dungeons where entities intentionally omit components (AI, bag, buff, skills) to save memory and CPU.

## Three Access Patterns

| API | Behavior | Use When |
|-----|----------|----------|
| `get<T>(entity)` | Asserts existence; **crashes** if absent | Inside `view<T,...>` iteration, or after `any_of<T>` / `all_of<T>` guard |
| `try_get<T>(entity)` | Returns `nullptr` when absent | Cross-entity lookups, optional components, any entity whose archetype is not guaranteed |
| `get_or_emplace<T>(entity)` | Silently **creates default** if absent | 只在两处:实体创建点,以及「按需状态」组件的写入口(见下节) |

## 组件的三种初始化方式(2026-10-15 起,取代旧规则「get_or_emplace 只许在初始化时用」)

**延迟初始化是本仓库有意采用的做法**:功能没被用到时,实体上就没有它的组件——不占内存,也不会被 `view` 遍历到(上面 sparse-set 的优势)。所以规则不是「禁止延迟」,而是**按组件类别决定在哪里创建、读的时候拿不到意味着什么**。

旧规则把延迟初始化整个禁掉,实际效果反而更糟:有人把按需组件的**写入口**也从 `get_or_emplace` 改成 `try_get`,于是再也没人创建它,读路径 `try_get` 拿不到就静默放行——技能冷却和技能上下文因此整条链空转(审计 F044/F050)。与此同时,真正危险的「存盘时补默认值」却被列在旧版的「可接受用法」里(审计 F059)。

| 类别 | 判据 | 典型组件 | 在哪里创建 | 读路径拿不到时 |
|---|---|---|---|---|
| **持久数据** | 会写进 DB / Redis | `CurrencyComp`、`PlayerCurrencyComp`、资产与结算账本、`LevelComp`、`BaseAttributesComp`、`PlayerPetComp`、背包、任务 | **只在加载时**(`*MessageFieldsUnmarshal` / `InitPlayerFromAllData`) | 不变量被破坏:`LOG_ERROR` + fail-closed,**绝不补默认值** |
| **按需状态**(延迟初始化) | 「没有」本身有明确含义:空、未激活、无冷却、不脏 | `CooldownTimeListComp`、`SkillContextCompMap`、`AoiListComp`、`SceneGridListComp`、施法阶段定时器、属性脏标记、`InBattleComp` | **该功能写入口第一次写时**,集中在一个命名函数里;结束时可以 `remove` | 正常分支:当作空 / 未激活处理,不是错误 |
| **派生运行时数据** | 由其他组件算出来,实体存在期间必须有 | `DerivedAttributesComp`、`MoveSpeedComp`、`CombatStateFlagsComp` | 加载 / 创建流程直接调用初始化函数,在首个通知前创建并计算;后续重算只更新 | `LOG_ERROR` + 跳过本次处理;有返回码的请求明确失败 |

硬规则:

1. **读路径不得改变实体的组件集合。** 查询、存盘、快照、面板、日志、每 tick 的只读计算,一律只用 `try_get` / `all_of` / `any_of`。存盘时补默认值,会把零值写进 DB。
2. **`get_or_emplace` 只出现在两处**:创建点,以及按需状态组件的写入口。写入口加一行注释说明「此处延迟创建」。
3. **一个按需组件只有一个延迟创建入口。** 其他写路径要么经过它,要么在它之后执行。**不要把写入口改成 `try_get`**,那样就再也没人创建它,功能会静默失效。
4. **持久数据绝不延迟创建。** 缺了说明加载有问题,补零值等于凭空造账。
5. **校验前置。** 先确认需要的组件齐全,再做副作用,不要做到一半才发现缺组件。

### 第 1 批核对后的具体创建入口

以下按需组件保留延迟初始化,不在 actor / scene 创建时预挂空表。读取缺失都按空或未激活处理,不会借查询创建组件。

| 按需组件 | 第一次创建的时机与入口 | 结束 / 消费 |
|---|---|---|
| `CooldownTimeListComp` | `SkillSystem::StartCooldown`,真正启动冷却时 | 冷却查询按时间判定;无表表示无冷却 |
| `SkillContextCompMap` | `StoreSkillContextForWrite`,登记合法施法的施法者上下文时 | `HandleSkillFinish` 删除本次上下文;目标不保存镜像 |
| `ActorStateComp` | `ActorActionStateSystem::AddState`,首次激活合法状态时 | 状态移除;无表表示无激活状态 |
| `BuffListComp` | `BuffSystem::AddOrUpdateBuff`,完成驱散 / 免疫等判断后首次真正插入时 | 帧末摘除空列表 |
| `BuffPendingRemoveBuffs` | `BuffSystem::MarkBuffForRemoval`,首次标记实际存在的 Buff 时 | 帧末处理后摘除 |
| `AoiListComp` | `InterestSystem::AddAoiEntity`,完成实体 / 容量校验后首次加入观察对象时 | 移除观察关系;无表表示无观察对象 |
| `SceneGridListComp` | `AoiSystem::UpdateGridState`,首次写入场景格子时 | 场景销毁时随实体销毁;空场景可无表 |
| `CastingTimerComp` | `SetupCastingTimer`,进入前摇时 | 前摇结束 / 打断时摘除 |
| `RecoveryTimerComp` | `SkillSystem::HandleSkillRecovery`,进入后摇时 | 后摇结束 / 打断时摘除 |
| `ChannelFinishTimerComp`、`ChannelIntervalTimerComp` | `SkillSystem::HandleChannelSkillSpell`,开始引导时 | 引导结束 / 打断时一起摘除 |
| `AttributeDirtyFlagsComp` | `ActorAttributeCalculatorSystem::MarkAttributeForUpdate`,首次标脏时 | 重算消费后摘除;冻结时保留待处理 |
| `PlayerAnomalyWindowComp` | `AnomalyDetector::WindowForWrite`,合法玩家首次记收益时 | 实体销毁自然清理;查询不创建,无效句柄不记录 |
| 生成的各 `*DirtyMaskComp` | `Set*AttrDirtyBit`,先校验位范围与实体再标脏 | 有接收者时消费后摘除;接收者为空保留 |
| `BattleLoginRecheckComp`(`player_battle.cpp` 内部空标记) | `MarkLoginRecheck`,登录钩子读挂起结算 / 读战斗锁没问到时(空应答、读记录 ERROR、命令没发出去、Redis 不可用;2026-10-16) | reaper 的 `RunLoginRechecks` 在 Redis 可用时摘除并重跑登录检查,又没问到就重新挂上;实体销毁自然清理 |

`CombatStateCollectionComp`、`InBattleComp` 也属于按需状态:分别在实际添加战斗状态、进入战斗或从权威战斗锁恢复时创建;查询缺失表示无对应状态。不要把状态存在与派生投影混为一谈:`CombatStateFlagsComp` 是创建 actor 时就应存在的同步投影。

`DerivedAttributesComp` 只由 `PlayerAttributeSystem::InitializeOnLoad` 创建并重算;`MoveSpeedComp`、`CombatStateFlagsComp` 由 `ActorAttributeCalculatorSystem::InitializeOnCreate` 创建并立即投影当前 Buff / 战斗状态。`InitPlayerFromAllData` 在反序列化之后、首个注册通知之前直接调用该入口,不依赖事件订阅;未来新增 NPC / 怪物创建入口也必须直接调用。重复调用重算投影,不清零 `DerivedAttributesComp` 或 HP/MP,不预挂按需状态、不主动标同步脏位,也不清除加载 / 写入口已有的脏标记。

默认技能由玩家加载直接初始化，任务 scope 由任务反序列化保证；`RegisterPlayerEvent` / `InitializeActorCompsEvent` / `InitializePlayerCompsEvent` 只通知。后续重算缺少必需派生组件时跳过,不能临时补一个零值组件。面板 / 列表若缺少实际使用的持久输入,返回域内既有故障码;空的已加载组件才代表“没有数据”。

缺组件拒存不是已落盘证据。退出、异步比较和交接不得据此宣称收敛、销毁未保存实体或发送交接标记。具体实施与未验证项见 [第 1 批 Codex 回执](../handoff/ecs-refactor/batch1-codex-receipt.md)。

每处组件的现状与修复进度见 [ecs-entt-misuse-audit-20261015.md](ecs-entt-misuse-audit-20261015.md)。

## 会话路由索引的所有者

`SessionMap()` 返回 `EcsContext::sessionPlayers` 普通成员，保存 gate 会话号到玩家号的映射，不注册为组件。它与 `playerList` 同属节点主 EventLoop，由 `EcsContext::Clear()` 一并清理；独立清理组件池不影响该索引。其他线程拥有各自的上下文，不能借此跨线程访问节点的在线状态。

节点身份由 `Node::localNodeInfo_` 持有，`GetNodeInfo()` 只读已安装的身份，未安装返回 protobuf 默认实例；地址发布不使 protobuf 变成线程安全对象，内容修改与 owner 寿命仍由节点 EventLoop 保证。服务发现快照由 `ServiceDiscoveryManager` 持有。请求拒绝码直接写本次 response，延期应答写本次 `RpcDoneCallback` 的 response。原 `EcsContext::globalRegistry` / `nodeGlobalRegistry` 及惰性 getter 已从源码删除。

## 实体类型与生命周期边界

| 数据 | 权威存储与身份 | 访问边界 |
|---|---|---|
| 场景中的 actor | `actorRegistry`，`entt::entity` | 玩家业务身份用 `PlayerGuidComp.value`，日志用 `ActorLog` |
| 场景实例 | `SceneRegistry = entt::basic_registry<SceneEntity>` | `SceneEntityComp`、瞬移等待与场景事件均用 `SceneEntity`，不与 actor 混传 |
| 客户端 actor 编号 | `ActorNetIdComp` + registry ctx 内的私有反查 | 创建时 `Assign`，出站 `Of`，入站 `Resolve`；不透明、非 0、不落库，本进程不复用 |
| 背包物品实例 | `ItemStore` 的 `Guid -> ItemComp` 字典 | 不再使用私有 registry，不暴露物品实体句柄 |
| 节点路由句柄 | 对应节点类型 registry 的 `entt::entity` | `SessionInfo::SetNodeEntity/GetNodeEntity` 保留版本位，缺失用 `entt::null` |

进程内 dispatcher 事件是模块内的普通结构体，字段明确所属 registry。Kafka 只能进入显式标注的 proto 事件，详见 [控制面事件入口](control-plane-topic-partitioning-20260908.md#2026-10-15-事件入口边界第-4-批源码方案)。同一业务同时需要两种传播时，在网络入口把业务 ID 解析成本地句柄再通知。

同步 `trigger` 可以销毁实体、移动组件或重绑玩家索引：通知前取值副本，返回后重验完整句柄和当前 `playerList` 归属，不跨通知保留组件借用。必要的 AOI 离场清理由离场入口直接调用，解绑之后才发通知；销毁兜底与解除 Pin 也清远端观察关系。场景成员只以 actor 上的 `SceneEntityComp` 为准，排空时扫描玩家，不另维护反向集合。

| 操作 | 句柄与借用约束 |
|---|---|
| `get<T>` | 调用者先确认实体版本有效且有组件，或由 `view<T>` 保证 |
| `try_get<T>` / `all_of<T>` | 完整句柄检查组件归属；缺失按三类规则处理，不创建 |
| `emplace<T>` | 不搬移同池已有组件；仍不能将借用跨同步回调保留 |
| `remove<T>` | swap-and-pop 可能搬移其它实体的该组件，原借用不能继续用 |
| `destroy` | **先 `valid`**；陈旧句柄可能按 index 释放已复用的活实体，不能将其当作幂等空操作 |

`TimerTaskComp` 的调度闭包只弱持有内部 State，移动转移定时器，复制得到空定时器；回调开火时保活 State。外层 owner 若有业务回调捕获 `this`，该 owner 必须禁止移动。包装组件可保留 `in_place_delete` 维持指针稳定，但不再靠它保证定时器正确性。

新增组件使用有业务含义的独立类型，不能用基础类型或通用 std 容器别名共享池。已有 `BuffListComp`、`BuffPendingRemoveBuffs`、`SkillContextCompMap`、`SceneGridListComp` 已改为具名类型，基类容器转换必须显式。传输侧 `RpcClientPtr`、`MuduoGrpcChannelPtr`、官方 gRPC Channel/CQ/Stub 等已有组件属于明确存量边界，不能据此新增同类业务组件。ECS 收尾已补取消、稳定缓冲与非阻塞排空源码，排空须涵盖正在执行及排队的回调；完整替换按 [gRPC 迁移验收](grpc-muduo-transport.md#101-来自-entt-审计的验收条件) 处理，源码/生成/运行状态以任务总索引为准。

内部 SkillContext/Buff 暂仍以 uint64 本地句柄及 0 表示空来源，因此保留 actor 的 0 号句柄退役兼容。该历史约束不再承担客户端编号职责。AOI 容量、压力和策略组件缺失时采用既定默认值，`Npc` 标签保留给未来生成入口。负责人已选择删除无生产写入口的 `PlayerItemBlockList` 及死参数，物品封禁沿用全服机制，不新增单人 GM 或持久化契约。

上述为第 2–5 批源码边界；生成器构建、统一重生成、编译及测试依用户要求暂缓，当前生成产物仍可能使用旧接口。实施和验收状态以 [任务总索引](../handoff/ecs-refactor/README.md) 为准。

## `std::optional` Return Pattern

Functions that depend on optional components should return `std::optional<T>`:

```cpp
// GOOD: caller decides how to handle missing Transform
std::optional<double> GetDistanceBetweenEntities(entt::entity e1, entt::entity e2)
{
    const auto *t1 = registry.try_get<Transform>(e1);
    const auto *t2 = registry.try_get<Transform>(e2);
    if (!t1 || !t2) return std::nullopt;
    return ComputeDistance(*t1, *t2);
}

// BAD: crashes if either entity lacks Transform
double GetDistanceBetweenEntities(entt::entity e1, entt::entity e2)
{
    const auto &t1 = registry.get<Transform>(e1); // assert crash
    const auto &t2 = registry.get<Transform>(e2); // assert crash
    return ComputeDistance(t1, t2);
}
```

## Dungeon/Instance Memory Savings

Entities that intentionally omit components:
- Use **zero memory** in those component pools (sparse-set design)
- Are **never iterated** by `view<T>` — zero CPU cost
- Keep **packed arrays dense** — better cache line utilization for entities that do have the component

Example: 200 NPC桩体 in a dungeon with only `Transform` + `Npc` tag, no AI/bag/buff/skills → each saves ~10 components × dozens to hundreds of bytes each.
