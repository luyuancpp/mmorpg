# 第 1 批运行时复核单

> **在途采样稿，不是放行单。** 负责人随后通知：第 1 批仍有 4 个代理收尾，Codex 只读，等待 `batch1-verify.md` 与收尾通知。本页保留最初读取时的证据；其中“当前”“必红”均指该次采样，不是最终工作区判定。后续读取已看到 `buff.cpp` 的首次插入入口恢复 `get_or_emplace<BuffListComp>`，原 R4 断链正在被修正；相关测试与全部调用链须在文件释放后重核。本页不是原工作流六组清单，也不能作为现在修改这些文件的指令。统一回执见 [codex-readonly-receipt.md](codex-readonly-receipt.md)。

状态：**已实施，Codex 逐条审读 diff，再编译、测试**。本单记录 2026-10-15 对工作区的静态复核；未把在途代码当成已经验收的结果。

基线为 `6281e4d27`。下文 HEAD 行号指这个提交，工作区行号指本次读取时；实施前必须重新搜索函数名。没有找到原来六组工作流的独立产物，因此本单按实际 diff 划成六个运行时复核切片，不能据此声称还原了原来六组分工。持久化、宠物、玩家属性、存盘模板另见第 1 批总任务单。

判定以 [组件访问规则](../../design/ecs-component-access-rules.md) 为准。`get_or_emplace` 出现与否不是验收标准；验收的是组件类别、唯一创建入口、缺失语义，以及失败前是否发生副作用。

## 1. 本次结论

| 切片 | 实施预期 | 当前结论 |
|---|---|---|
| R1 行为状态 | 校验不创建；第一次激活状态由写入口创建；无激活状态是正常空分支 | 活实体检查和非法状态前置可保留；把 `ActorStateComp` 必需化需按类别改回 |
| R2 属性重算 | 脏标记延迟创建、消费摘除；派生组件初始化创建，读缺失记错跳过 | 静态符合；仍缺真实 actor 初始化链的运行证据 |
| R3 技能 | 冷却/上下文按需创建；相位开始创建、结束清理；首发技能不能因空表被拒 | 冷却/上下文仍按旧规则预挂与拒绝；相位生命周期改动可保留并运行验证 |
| R4 Buff | 第一次真正添加时创建列表；叠层/查询不创建；待删标记按需 | **确认断链**：写入口拒绝缺列表，但生产代码没有列表创建点 |
| R5 AOI 格子和消息缓冲 | 首次加入场景格子时创建格子表；查询缺表正常空；消息缓冲不挂 ECS | 查询和消息缓冲改法可保留；格子表仍在场景创建时预挂 |
| R6 兴趣表 | 第一次新增兴趣关系时创建；读取缺表正常空；非法句柄不创建 | 双端存活校验可保留；真正写入口被改成只取不建，创建提前到进格 |

最需要先修的是 R4；测试通过不能替代生产链检查：`buff_test` 的夹具自己 `emplace<BuffListComp>`，恰好掩盖生产没有创建点的问题。

## 2. R1：行为状态（F034）

职责：`ActorActionStateSystem` 根据当前已激活状态决定行为冲突，并在成功时激活目标状态；调用者是 `SkillSystem::CheckState`。组件不入库，缺组件表示没有已激活状态。

| 文件 | 函数 | HEAD → 工作区行号 |
|---|---|---|
| `cpp/libs/services/scene/actor/action_state/system/actor_action_state.cpp` | `TryPerformAction` | 51 → 62 |
| 同上 | `CanExecuteActionWithoutStateChange` | 88 → 109 |
| 同上 | `HasState` | 100 → 129 |
| 同上 | `AddState` | 109 → 138 |
| 同上 | `RemoveState` | 125 → 162 |
| `cpp/nodes/scene/handler/event/actor_event_handler.cpp` | `InitializeActorCompsEventHandler` | 20 → 23；工作区 43 预挂状态 |

当前新增 `FindActorStateComp(entt::entity)` 先查 `valid`，这能防止向已销毁句柄挂组件；`successState` 范围也在打断之前校验，均应保留。问题是把「活实体但没有状态列表」等同「无效实体」，并用初始化事件给所有 actor 预挂空状态。

修法：

1. 初始化事件去掉 `get_or_emplace<ActorStateComp>`；保留派生数据创建。
2. `AddState` 先校验实体存活及状态值，随后作为唯一创建入口 `get_or_emplace<ActorStateComp>` 并插入状态；注释写明第一次状态写入时延迟创建。
3. `TryPerformAction` 先校验实体及目标状态；有列表时检查冲突、执行打断；没有列表时没有冲突；最后统一调用 `AddState`。不得在检查阶段建表。
4. `CanExecuteActionWithoutStateChange` 对失效实体拒绝，对活实体缺列表返回成功；`HasState` 缺列表返回 false；`RemoveState` 对活实体缺列表按幂等空操作处理。非法实体与没有激活状态必须分开判断。
5. 同步头文件契约和测试注释，删除“只在 actor 初始化时挂”这一旧断言。

测试：保留 `ActorActionStateTest.InvalidSuccessStateIsRejectedBeforeAnyStateChange`；将 `EntriesRejectAnActorWithoutStateComp` 拆成 `ReadOfActorWithoutStateIsEmptyAndCreatesNothing`、`FirstStateWriteCreatesStateComp`、`DestroyedActorCannotCreateStateComp`。夹具使用现有 `ActorEntityFixture::NewEntity`；第一例只调用只读接口并断言无组件，第二例调用 `TryPerformAction` 并断言目标状态出现，第三例 destroy 后调用写入口并断言错误。当前代码第二例会返回 `kThisEntityIsInvalid`，第一例的只读校验也会错误拒绝，因此可形成修复前红证据。

## 3. R2：属性重算（F035）

| 文件 | 函数 | HEAD → 工作区行号 |
|---|---|---|
| `cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp` | `UpdateMoveSpeed` | 29 → 32 |
| 同上 | `ResetCombatStateFlags` | 63 → 78 |
| 同上 | `MarkAttributeForUpdate` | 93 → 115 |
| 同上 | `Update` | 98 → 136 |
| `cpp/nodes/scene/handler/event/actor_event_handler.cpp` | 派生组件创建 | 新增工作区 44–45 |

`AttributeDirtyFlagsComp` 属按需状态：`MarkAttributeForUpdate` 校验 bit 和实体后创建，重复标脏合并位；`Update` 排除冻结实体、拷贝脏位后移除当前实体标记再重算。`MoveSpeedComp` 和 `CombatStateFlagsComp` 属派生数据：实体创建时建立，重算缺失时 `LOG_ERROR` 并跳过。`BuffListComp` 不存在按无 Buff 计算，清除上次残留加成。

静态检查点：不得把派生组件的缺失改回默认创建；不得用 `emplace_or_replace` 覆盖整份脏位而丢掉同 tick 的其他标记；移除标记后不能再引用 `dirtyFlags`；冻结期间标记原样保留。

现有 `cpp/tests/bag_test/player_feature_handler_test.cpp` 已有：

- `ActorAttributeCalculatorTest.UpdateRemovesTheDirtyFlagComponentOnceConsumed`
- `FrozenPlayerKeepsItsDirtyFlagsUntilTheFreezeEnds`
- `RecalculationDoesNotCreateMissingRequiredComponents`
- `MoveSpeedFallsBackToZeroWhenNoBuffListIsLeft`
- `CombatStateFlagsMirrorTheActiveCombatStates`
- `MarkIgnoresAnOutOfRangeAttributeBit`

前两例分别守消费摘除和冻结语义；缺派生组件和无 Buff 两例能区分默认补建与空输入；这几例未在本轮执行。建议补一例经过真实 `InitializeActorCompsEventHandler` 的创建测试，断言派生组件存在而冷却、上下文、Buff、行为状态等按需组件不存在；不得在夹具里手挂目标组件来冒充生产初始化测试。

## 4. R3：技能（F044/F050/F051）

| 文件 | 函数 | HEAD → 工作区行号 |
|---|---|---|
| `cpp/libs/services/scene/combat/skill/system/skill.cpp` | `AddTargetSkillContext` | 62 → 62 |
| 同上 | `CheckRequiredSkillComps` | 新增 → 125 |
| 同上 | `StartCooldown` | 118 → 147 |
| 同上 | `AddSkillContext` | 155 → 190 |
| 同上 | `CheckSkillPrerequisites` | 256 → 298 |
| 同上 | `HandleSkillRecovery` | 316 → 362 |
| 同上 | `HandleChannelSkillSpell` | 356 → 413 |
| 同上 | `HandleChannelFinish` | 405 → 463 |
| 同上 | `CheckTimerPhase` | 474 → 538 |
| 同上 | `CheckCooldown` | 498 → 575 |
| 同上 | `SetupCastingTimer` | 548 → 625 |
| `cpp/nodes/scene/handler/event/actor_event_handler.cpp` | 冷却/上下文预挂 | 32–33 → 35–36 |

当前 `CheckRequiredSkillComps` 要求两张空运行时表先存在，违反新规则；直接移除预挂会让所有首发技能返回 `kServiceUnavailable`。必须同时恢复写入口创建和缺失读语义。

补丁指引：

1. 删除初始化事件的冷却/上下文预挂，以及 `skill_comp.h`、`skill.h`、`skill.cpp` 中必需组件的旧注释。
2. `CheckCooldown` 对活实体缺 `CooldownTimeListComp` 返回 `kSuccess`，且不建组件。`StartCooldown` 在已验证实体的第一次冷却写入时创建列表，再写当前冷却；不能只改成 `try_get` 后提前返回。
3. 上下文的 caster 与 target 两条写路径收口一个本文件匿名命名空间的创建入口，完整签名可为 `SkillContextCompMap* FindOrCreateSkillContextsForWrite(entt::entity actor)`：先校验 `valid`，无效返回 nullptr；有效时按需 `get_or_emplace`。`AddSkillContext` 与 `AddTargetSkillContext` 都通过它写入，禁止只修 caster 漏 target。若沿用现有 void 接口，顶层必须先完成失效实体校验，不能把写失败当施法成功。
4. `FindCasterSkillContext`、移除上下文及查询缺表按无上下文处理；`CreateSkillContext` 已用空表兜住 ID 查重，保留只读做法。
5. 删除 `CheckRequiredSkillComps` 对两张按需表的要求；保留真正的实体/持久数据前置校验。不得用“先创建空表再通过校验”代替按需写入口。
6. 保留相位开始的 `emplace_or_replace`、自然结束摘组件和引导打断时同时摘 finish/interval 的修正。`TimerTaskComp::OnTimer` 在 `cb()` 后不再碰自身，支持回调内结束自己的相位；仍须运行相关回归测试确认。

现有 `CheckCooldown_MissingCooldownComp_ReturnsError` 与 `CheckSkillPrerequisites_MissingSkillContextMap_RejectsBeforeSideEffects` 固化旧分类，必须改写，不能为了全绿保留。建议最少补：

| 用例 | 夹具与断言 | 当前为什么红 |
|---|---|---|
| `CheckCooldown_MissingListMeansNoCooldownAndCreatesNothing` | 活 caster + 有效 SkillTable；查冷却成功，仍无列表 | 当前返回服务不可用 |
| `StartCooldown_FirstWriteCreatesListAndBlocksSecondCast` | 活 caster；真实加载的冷却表中选正时长行，调用 StartCooldown；断言列表建立、第二次 CheckCooldown 被拒 | 当前 StartCooldown 只取不建并返回 |
| `SkillContext_FirstCastCreatesCasterAndTargetMaps` | 两个同场景活玩家走真实 ReleaseSkill；断言 caster/target 同一 skill_id 有上下文，不由夹具先挂表 | 当前前置校验拒绝 |
| `RejectedSkillDoesNotCreateRuntimeTables` | 非法目标/冻结/战斗/失效 caster，各自调用顶层；断言两张按需表仍不存在 | 防止修法在校验阶段抢先建表 |

保留并运行已增加的 `CheckChannel_ImmediateInterrupt_EndsWholeChannelPhase`、`HandleChannelFinish_EndsChannelPhase`、`SetupCastingTimer_NoCastPhaseSkill_DoesNotStartPhase`、`GeneralSkill_PhaseComponentsEndWithTheirPhase`。自然到期测试必须有有界 EventLoop 等待，失败保留定时器/相位/上下文状态。

## 5. R4：Buff（F037/C003）

| 文件 | 函数 | HEAD → 工作区行号 |
|---|---|---|
| `cpp/libs/services/scene/combat/buff/system/buff.cpp` | `ProcessedCasterOf` | 新增 → 65 |
| 同上 | `AddOrUpdateBuff`（三参） | 82 → 105 |
| 同上 | `MarkBuffForRemoval` | 197 → 238 |
| 同上 | `RemovePendingBuffs` | 203 → 257 |
| 同上 | `HandleExistingBuff` | 266 → 322 |

确认缺陷：工作区 145 的 `all_of<BuffListComp>` 拒绝缺列表；全仓生产代码中 `BuffListComp` 只有读取，没有任何创建点。注释称 `ActorEventHandler` 创建它，但该事件没有这行。测试 `CreateBuffActor()` 手挂列表，因此不能发现生产断链。

修法：`BuffListComp` 按需，唯一创建入口是三参 `AddOrUpdateBuff`；二参继续委托。先校验实体存活、冻结/战斗、配表、免疫；纯驱散与不存在可叠条目时按空处理，不创建列表。确实要插入新 Buff 时才创建列表。`HandleExistingBuff` 缺列表正常返回 false，不报初始化错误。驱散等同步回调之后重取列表，不能长期持有引用。去掉 `buff.h` 的必需组件前提。

可保留：`BuffPendingRemoveBuffs` 第一次有效标记时创建、处理后 remove；`ProcessedCasterOf` 被新建和叠层共同使用，no_caster 统一 0，首层为 1。C003 在工作区已实施，不能照旧审计表写“未修”。

测试指引：保留 `BuffStackingTest.NoCasterBuff_StacksAcrossCastersUpToMaxLayer`、`CasterScopedBuff_StacksPerCasterFromLayerOne` 与 `BuffIntervalThinkTest` 三例。将 `BuffListRequiredTest.AddOrUpdateBuff_MissingBuffList_RejectedWithoutCreatingIt` 换成 `FirstBuffWriteCreatesList`：裸活实体 + 夹具表 `kPlainSubBuff`，调用真实 AddOrUpdateBuff，断言成功、非 UINT64_MAX、列表恰一项。当前代码因 kServiceUnavailable 必红。另加 `InvalidTargetDoesNotCreateList`、`ReadOfMissingBuffListCreatesNothing`、`PureDispelOnEmptyTargetCreatesNothing`。可把 `CreateBuffActor` 改为只 create，避免以后再次靠测试补建掩盖写入口断链。

## 6. R5：AOI 格子/消息缓冲（F075/F077）

| 文件 | 函数 | HEAD → 工作区行号 |
|---|---|---|
| `cpp/libs/services/scene/spatial/system/aoi.cpp` | `Update` | 40 → 40 |
| 同上 | `OnSceneCreatedHandler` | 新增 → 69 |
| 同上 | `UpdateGridState` | 62 → 86 |
| 同上 | `NotifyEntityVisibilityChanges` | 164 → 198 |
| 同上 | `BeforeLeaveSceneHandler` | 188 → 226 |
| 同上 | `BroadcastEntityLeave` | 317 → 363 |
| `cpp/libs/services/scene/spatial/system/grid.cpp` | `FindSceneGridList` | 新增 → 45 |
| 同上 | 两个 `GetEntities*` | 40/82 → 62/107 |
| `cpp/nodes/scene/handler/event/scene_event_handler.cpp` | `OnSceneCreatedHandler` | 30 → 30；新增工作区 49 |

保留只读空间查询 `try_get`、sceneRegistry.valid、发送缓冲从 GlobalEntity 移出。当前问题是权威规则已把 `SceneGridListComp` 列为按需，但实现和测试仍要求场景创建即挂。

修法：删除或取消接线专门预挂空表的 `AoiSystem::OnSceneCreatedHandler`，保留 `SceneEventHandler` 的 Agones 等其他功能。新增唯一写入口 `SceneGridListComp* FindOrCreateSceneGridListForWrite(entt::entity scene)`（本文件匿名命名空间）：valid 后按需创建。在 `AoiSystem::Update` 即将把首个实体登记进格子前调用它；这里是状态写入而非只读 tick。其他查询/离场继续 `try_get`；活场景没有格子表表示尚无 AOI 实体，返回空不应报初始化错误。失效 scene 仍记错并跳过。

现有 `OnSceneCreatedAttachesGridList`、`UpdateDoesNotCreateGridListForSceneWithoutOne` 断言相反，需要替换：

- `EmptySceneHasNoGridListUntilFirstActorJoins`：仅创建 scene 时无表，首次 AOI Update 后有表且实体在正确格子；当前实现没有预挂表时 Update 跳过，必红。
- `SpatialQueryOnEmptySceneCreatesNothing`：活 scene + actor 场景引用，调用两种查询，返回空且 scene 无表。
- 保留 `SpatialQueryOnDestroyedSceneDoesNotCreateGridList`，继续守失效句柄。
- 继续运行邻格往返、同场景瞬移、离场广播、可见性双向通知测试，防止改创建时机漏掉行为链。

## 7. R6：兴趣表（F078）

`cpp/libs/services/scene/spatial/system/interest.cpp::AddAoiEntity`：HEAD 80 → 工作区 81。当前双端 valid 校验正确，但缺列表拒绝并要求观察者必须先入格；`aoi.cpp::UpdateGridState` 又为所有初次进格 actor 预挂空兴趣表。`PinAoiEntity` 同样被新增前提限制。生产 Add 调用在 `AoiSystem::HandleEntityVisibility` 两个方向，Pin 当前只有封装入口，没有生产调用者。

修法：把唯一创建收口 `InterestSystem::AddAoiEntity`，在 watcher/target 存活以及必要的接受条件校验后第一次真正插入关系时创建。`UpdateGridState` 不再预挂 AoiList；`PinAoiEntity` 继续委托同一入口。`Remove/Unpin/Upgrade/Downgrade` 缺表正常 no-op，不创建。保留双端失效拒绝。

`AoiPriorityTest.AddAoiEntityRejectsWatcherNotJoinedAoi` 固化旧口径，应换成 `FirstInterestWriteCreatesList` 和 `PinBeforeGridEntryCreatesPinnedRelation`；夹具仍用 SpawnAt，但不手调 JoinAoi，断言添加成功、目标在表中、优先级正确。当前代码两例都返回 false，能给红证据。保留失效 watcher、失效 target 用例，并断言拒绝没有新建表。测试辅助 JoinAoi 不得成为所有新写用例的前提。

## 8. 额外已实施项：F091

`cpp/nodes/scene/handler/rpc/player/s2s_player_scene_handler.cpp` 中 `EnterScene` 的 HEAD 24 曾直接 `entt::to_entity(request->scene_id())`。工作区新增 `FindSceneEntityBySceneId(uint64_t sceneId)`（17），按 `SceneInfoComp.scene_id` 查本地 sceneRegistry；`EnterScene`（37）找不到时在 `HandleEnterScene` 和队伍刷新前返回。静态已消除 GUID 当句柄，不能再标为“未修”。

仍缺实际回归：造一个数值恰好等于另一合法 entity 的 scene_id，断言按业务 ID 命中正确 scene；不存在/0 ID 不触发入场与队伍刷新；失效玩家不触发副作用。此 handler 未在本次所读测试工程中单独接入；实施测试时先确认工程纳入真实 handler 或通过现有 seam 验证，不以重写一份查找算法充作测试。

## 9. 构建与测试证据、后续执行口径

本轮主执行者实际启动 scene 构建，**失败**：`cpp/libs/engine/core/network/rpc_status.h:69` 的 `HANDLER_FAILED` 与 `:71` 的 `REPLY_DROPPED` 未声明，属于另一会话的 RPC 在途链路。日志：`build/ecs-refactor/scene-build.log`。已停止后续构建/测试，没有为 ECS 任务修 RPC，也没有自动重试。静态结论不等于编译通过。

恢复前置：RPC 会话完成生成物与协议常量同步，并明确释放公共构建链；ECS 新分类修正与回归测试先完成，不能带着已知断链直接宣称第 1 批验收通过。不要安装工具或替换依赖来绕过错误。

后续在仓库根依次执行，每步失败即停并保留完整日志：

```powershell
msbuild cpp/libs/services/scene/scene.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64
pwsh -File tools/scripts/run_cpp_tests.ps1 -Build -Filter buff
pwsh -File tools/scripts/run_cpp_tests.ps1 -Build -Filter skill
pwsh -File tools/scripts/run_cpp_tests.ps1 -Build -Filter aoi
pwsh -File tools/scripts/run_cpp_tests.ps1 -Build -Filter bag
```

工程静态链接 `lib/scene.lib`，测试脚本不负责重建上游库，必须先编 scene；受共享库变更影响时由主执行者按依赖顺序先编它们。MSBuild 一律 `/m:1`，禁止多个会话同时写 lib/PDB 输出。脚本的 `-Filter aoi` 会同时运行 aoi_test 和 aoi_delivery_test。需要只运行某个新 gtest 时先构建对应工程、由脚本同步 DLL，再运行 `build/cpp/tests/<test>.exe --gtest_filter=<完整用例名>`。

通过标准：新分类正向首写用例与拒绝无副作用用例全部通过，现有相位/叠层/可见性/瞬移/属性冻结测试全绿，进程退出 0 且有完整 `[ PASSED ]` 汇总。失败保留首个编译错误、完整 gtest 失败段、异常退出码与相关状态；禁止只看“没有 FAILED 行”。本轮尚无任何这些通过证据。

红证据只能在隔离快照运行；不得回退共享工作区。新分类回归应先在“本单记录的当前工作区实现”上证明失败，再用修正版本验证，不要把 HEAD 与工作区混为同一个修复前基线。

## 10. 协作与另一版状态

公共写点由一个 Codex 顺序处理：先写技能/Buff/行为状态/兴趣/格子的唯一创建入口与相应测试，再删除 actor/scene 初始化预挂，最后重审所有调用点。R2 与 C003 可独立只读复核，构建统一串行。

`actor_event_handler.cpp`、`scene_event_handler.cpp` 是半生成文件，只改守护段；RPC/入场两个会话的源文件和公共传输头不在本单修正范围。`s2s_player_scene_handler.cpp` 同时引用正在退场的传输 helper，等 RPC 会话合并后再安排 F091 实测。

Java 不使用 entt，组件存储与 sparse-set 分类无需照抄。但技能冷却真正生效、首发技能可用、Buff no_caster 跨施法者叠层、同场景技能约束是客户端可观察行为，需核对 Java 等价实现并在 `PARITY.md` 登记版本；本单只读复核，没有跨仓修改，也没有 Java 运行证据，状态为待核对。
