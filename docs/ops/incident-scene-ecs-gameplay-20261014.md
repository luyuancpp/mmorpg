# 严重事故记录：任务、实时技能、Buff 与节点生命周期缺陷（2026-10-14）

> 记录日期：2026-10-14（美东）。按负责人要求，本批按严重事故跟踪。
> **当前状态：未关闭。10 组问题已有工作区代码改动（其中 F022 仅部分处理），2 组暂缓；本批修复尚无编译、测试和联机验收结果。**

## 1. 事故范围与证据边界

本批涉及核心任务奖励和任务链失效、技能目标越过场景边界、拒绝应答被改成成功、技能冷却与效果链路缺失，以及可能导致进程崩溃、内存访问错误和存档损坏的生命周期缺陷。不能以“原链路没有真正执行”降低其严重性；补齐初始化后，原本被早退遮住的问题会进入实际执行路径。

记录依据：

- 负责人提供的修复会话摘录，结尾为“已改完 10 条，另外 2 条推迟”“代码还没编译、没跑测试”。
- [进度记录](../PROGRESS.md)中“2026-10-14 entt 误用审计：修复已复核的 high/medium 问题”条目，以及其中列出的源文件和新增测试。
- 本次只读核对当前工作区对应差异、生成器模板和历史进度；登记基线为 `2f897bd0780c1cc78284548284f20d01e61d7ea8` 加未提交改动。该提交号不能当作本批修复已入库的证明。

“严重事故”是本批的跟踪定性，不擅自改写原审计各项等级。原报告中的“线上从不执行”“首次在线上真正执行”属于上一会话的代码分析结论；本次没有部署记录、生产日志、玩家损失统计或修复后的运行结果支撑这些时间和范围判断。首次引入版本、受影响部署、持续时间、实际玩家和资产损失均待追溯。历史本地冒烟已复现的事实另列于 §4。

本次只整理事故文档、索引、进度和 Java 对账，没有修改业务代码、执行修复验收或部署服务。当前 PR / Issue 未能核对：本机没有可用的 `gh`，未为此安装工具或登录账号。

## 2. 缺陷清单与处置状态

序号对应修复报告的业务分组；F 编号沿用原审计，不重新发号。“已有改动”只表示代码存在，均不等于事故关闭。

| 序号 | 原审计编号 | 缺陷与影响 | 已有改动 / 待处理范围 | 状态 |
|---|---|---|---|---|
| 1 | F027 / F028 | 任务完成事件只入队、生产链没有冲刷点，自动发奖、后续任务接取和“完成任务 X”条件不执行；实体句柄截为 32 位后，复用槽位的事件又会被当成无效实体丢弃。队列还有持续积压风险。 | 本轮全部任务状态落定后同步 `trigger`，保留 64 位句柄；清理 ECS 时清 dispatcher。 | 已有改动，待验证 |
| 2 | F046 / F049 | 客户端目标整数只经 `valid()`，可指向同一节点别的场景或冻结中的实体；注册表有效性被误当成业务许可。 | 统一目标解析：有效、Player/Npc、同场景、未冻结；施法和效果落点重新校验，并保留回合制战斗闸。 | 已有改动，待验证 |
| 3 | F003 | `TRANSFER_ERROR_MESSAGE` 把空 TLS tip 覆盖到 response，抹掉 `ReleaseSkill` 已写入的拒绝码，客户端收到类似成功的结果。 | TLS tip 无 id 且无参数时保留 response 原错误；非空 tip 仍转移并清理。 | 已有改动，待验证 |
| 4 | F044 / F050 | 冷却表、技能上下文在生产初始化中缺失；缺冷却表还返回成功，技能效果阶段缺上下文而早退。 | actor 初始化守护段补挂两组件；冷却表缺失时返回 `kServiceUnavailable`。恢复上下文同时改变伤害、Buff 等路径的可达性。 | 已有改动，玩法变化待验收 |
| 5 | F048 | 全新 actor registry 首个实体整数为 0，与协议“没有目标”的默认值冲突，首个角色可能无法被技能选中。 | `EcsContext` 构造时先 create / destroy 一次，推进首槽位版本。 | 已有改动，待验证 |
| 6 | F040 | 遍历 Buff 表时添加子 Buff 可能 rehash，驱散可删除当前条目；回调返回后继续使用迭代器或 `BuffEntry&`，可能访问失效内存。 | 加成 Buff 先快照 id、逐条重查；生成子 Buff 前复制后续需要的技能上下文。2026-10-15 补同类遗漏入口 `BuffSystem::OnIntervalThink`：三路周期回调之间改为每路前按 id 重查，并在 `buff_test` 补驱散自身与扩容用例（见 §3.3）。 | 已有改动，待验证 |
| 7 | F047 | 对自己施法时施法者与目标共用上下文表，先按 key 删除、再使用失效迭代器删除同一项。 | 自目标不重复镜像；结束时删除施法者条目后，仅对不同目标清理镜像。 | 已有改动，待验证 |
| 8 | F041 | Buff tick 只保证 Buff 表存在，却直接 `get` 三个属性组件；组件不齐时会断言或非法访问。 | 基础 / 派生属性改 `try_get`，缺失时记录并跳过；等级缺失按 1 级计算。 | 已有改动，待验证 |
| 9 | F022 | 出站 TCP 客户端留在 thread_local registry，晚于 EventLoop 析构；客户端析构再次访问 loop，可能 use-after-free。 | 两跳 quit 前清理 `RpcClientPtr`；etcd / gRPC CQ、stub 和 watch 的关闭与排空仍未完成。 | **部分处理，剩余问题未关闭** |
| 10 | F039 | 受伤后回调将攻击者当成受击者，重置攻击者的脱战加成；第 4 项补齐后该错误路径具备执行条件。 | `OnAfterTakeDamage` 按调用契约传递攻击者和受击者。原审计为关联 low 项，纳入本批完整性检查。 | 已有改动，待验证 |
| 11 | F059 / F060 | 存盘 Marshal 使用 `get_or_emplace`，必需组件缺失时会补出默认值并写入快照，存在以零值覆盖有效存档的风险。 | 生成器模板与调用方尚未修；因 `player_lifecycle.cpp` 正在被另一会话修改而暂缓。 | **未处理，仍有数据风险** |
| 12 | F001 / F002 | Kafka 命令直接复用进程内事件分发表，审计记录覆盖 47 个事件，包括携带进程内实体整数的事件；跨进程载荷可进入本不应开放的本地事件入口。 | 待盘清全部合法生产者后制定命令白名单，覆盖正在接入的 `BindSessionEvent`，再改生成源。 | **未处理，信任边界未收口** |

其他未复核审计项不因本表而关闭。原完整审计清单未随本次材料提供，本记录不据分组数量推算其余问题已经解决。

## 3. 根因、修复边界与仍需整改的事项

### 3.1 任务：入队不等于派发，局部测试不等于生产链路

[mission.cpp](../../cpp/libs/modules/mission/system/mission.cpp)原实现将副作用事件 `enqueue`，但实际运行路径没有对应 `update()`。若测试主动冲刷队列，会替生产代码完成缺失步骤，从而掩盖问题。实体值又被暂存到 `uint32_t`，丢失高位版本，导致槽位复用后事件接收方拒绝有效玩家。

现有修法先完成本批任务状态，再按发奖、接取后续任务、推进完成条件的顺序同步派发，避免任务 A 的监听者重入时，把同批尚未置完成位的任务 B 再次接取。自动领奖仍须以 `ClaimReward` 成功后消耗待领取状态为准；满包或其他发奖失败后的重试和不重奖行为需要运行证据。

不能用“帧尾补一个 `dispatcher.update()`”直接结案。上一会话核对的 entt 3.13 队列派发将元素引用交给监听者，同类型事件重入入队并扩容存在引用失效风险。当前同步方案也须验证任务链重入、状态顺序和重复领取，不只验证单条奖励到账。

### 3.2 技能：访问方式替换遗漏初始化，并掩盖了后续缺陷

将 `get_or_emplace` 改为 `try_get` 时，没有在 actor 创建点补齐 `CooldownTimeListComp`、`SkillContextCompMap`。缺冷却组件返回成功，缺上下文时后续效果早退，使“请求成功、玩法未执行”同时存在。初始化规则原已写在 [ECS 组件访问文档](../design/ecs-component-access-rules.md)，但代码和验收没有形成闭环。

当前 [actor 初始化改动](../../cpp/nodes/scene/handler/event/actor_event_handler.cpp)会让此前早退的实时效果路径具备执行条件。这是客户端可感知的行为变化，必须连同伤害、Buff、冻结、回合战斗隔离、死亡与复活边界一起确认，不能仅凭“设计要求初始化”认定整条玩法已经可以交付。Java 的旧对账明确将实时伤害 / Buff 保持未启用，见 §6。

[skill.cpp](../../cpp/libs/services/scene/combat/skill/system/skill.cpp)的目标解析同时修补另一条信任边界：同一个 actor registry 被本节点多个场景共用，`valid()` 只证明句柄当前存在。施法瞬间的场景和冻结状态也不能代替延迟落点时的复核。本批没有把对外实体整数升级为 GUID 或另设 net-id；不把协议未变误写成客户端行为未变。

[错误码转移宏](../../cpp/libs/engine/core/macros/return_define.h)是跨 handler 的公共出口，不能只回归 `ReleaseSkill`。既有直接写 response 的入口与 TLS 写错入口都应保持原拒绝语义，并验证调用后 TLS 已清空，避免错误串到下一次请求。

### 3.3 Buff 与上下文：回调可能删除当前对象

[buff_impl.cpp](../../cpp/libs/services/scene/combat/buff/system/buff_impl.cpp)存在两种不同失效：向 `unordered_map` 插入造成遍历迭代器失效，以及驱散删除条目造成该对象引用悬空。只预留容量不能解决后者；只做 id 快照也不能允许跨回调继续读旧引用。每次有副作用的回调之后，都要按稳定 id 重取仍需要的数据，或在回调前复制需要保留的值。

2026-10-15 补登一处同类遗漏：[buff.cpp](../../cpp/libs/services/scene/combat/buff/system/buff.cpp) 的 `OnIntervalThink` 在第一路回调后仍用原迭代器调用另外两路。子 Buff 的驱散标签命中父 Buff 时，父 Buff 会在第一路回调中被删除，MSVC Debug 下随后的解引用直接断言。现表没有授予唯一 43 型 Buff 的入口，属潜伏缺陷。现已改为每一路之前按 id 重查，并在 [buff_test.cpp](../../cpp/tests/buff_test/buff_test.cpp) 新增 3 条用例；未编译、未运行，详见 PROGRESS 2026-10-15 条目。

技能自目标删除还须满足“一份上下文只清理一次”；受伤回调须验证攻击者和受击者角色，不能靠含糊参数名推断。以上路径会在技能上下文恢复后共同变得可达，必须一起回归。

### 3.4 停机：只处理了 TCP 客户端，不能整体清空节点 registry

[Node::FinalizeShutdownInLoop](../../cpp/libs/engine/core/node/system/node/node.cpp)目前在 loop 仍存活时释放出站 `RpcClientPtr`，使关闭动作进入两跳 `queueInLoop` 排空阶段。该改动遵循 [AGENTS.md §11.7](../../AGENTS.md) 和 [Scene 线程模型](../design/scene-node-threading-model.md)的生命周期约束。

同批 registry 中的 etcd / gRPC CompletionQueue、stub 和 watch 流需要先关闭、终止在途操作并排空，再销毁。不能用整体 `Clear()` 代替该流程，也不能把 TCP 处理完成写成整个 F022 已修复。节点摘除和进程关机两条路径都需单独核对。

### 3.5 暂缓不等于风险解除

- **F059 / F060**：[Marshal 生成模板](../../tools/proto_generator/protogen/internal/template/player_data_loader.cpp.tmpl)仍直接使用 `get_or_emplace`；[player_lifecycle.cpp](../../cpp/libs/services/scene/player/system/player_lifecycle.cpp)调用该生成链构造存盘快照。整改应明确必需组件缺失的错误语义，在任何持久化副作用前拒绝不完整快照，并覆盖周期存盘、退出、停机、迁移和失败重试。必须修生成源、正式重生成并检查调用方，不能手改生成产物。
- **文档遗留冲突**：ECS 访问文档仍把 Marshal 的“缺失时默认值回退”列为可接受用途。这与本批识别出的存档风险及 `AGENTS.md §11.3` 数据完整性要求不一致。该历史例外不能充当 F059 放行依据，后续修复必须同时修正文档。本次只记录冲突，没有擅自改写该会话正在处理的实现。
- **F001 / F002**：[Kafka 默认分发入口](../../cpp/libs/engine/core/node/system/node/node_kafka_command_handler.h)检查事件号非零和 payload 非空后进入通用 `DispatchProtoEvent`；[生成模板](../../tools/proto_generator/protogen/internal/template/rpc_event_registry.cpp.tmpl)没有区分跨进程命令与本地事件。应先列出发送方、接收节点、身份字段、白名单和非法事件拒绝行为，再在生成源收口；保留实例 UUID 防僵尸检查。不能漏掉合法命令后静默丢弃，也不能因生产者尚未盘清而把入口记为安全。

## 4. 历史关联：已经出现过的信号未形成完整闭环

以下均来自 [PROGRESS.md](../PROGRESS.md)历史条目；旧运行结果只证明当时对应范围。

| 历史信号 | 对本次事故的意义 |
|---|---|
| 2026-08-05，战斗 / 空间 ECS 审计修复已处理 `ProcessBuffs` 遍历时增删、自身驱散后继续使用引用，以及 `AddSubBuffsWithoutCheck` 的同类问题。 | 当前 F040 属同类遗漏入口，后续要沿回调和所有调用点检查完整范围，不能只修最先报错的循环。 |
| 2026-08-10 曾基于“没有入口”证伪任务系统问题；9 月随后接上任务入口、真实发奖与持久化。 | 旧的不可达结论会随接线失效，不能沿用为当前任务副作用无需验收的理由。 |
| 2026-09-15，本地 `SkillCast` 冒烟因目标实体 0 失败；09-17 虽然 23/23 通过，记录明确目标为 1、实体 0 缺陷仍未修。 | F048 是长期登记问题本轮补码；普通目标通过不能代替新 registry 首实体边界的验收。 |
| 2026-09-18，已记录 `EnterScene` 七处同步拒绝码被 `TRANSFER_ERROR_MESSAGE` 抹空。 | F003 暴露公共出口仍有同族遗漏，需回归所有调用者。 |
| 2026-09-14，连接生命周期改动曾有 596/596 C++ 用例通过，但明确未做 Node 析构运行时验收。 | 历史测试绿不能证明本次出站 TCP 客户端和 gRPC 资源的拆除顺序已安全。 |

反复暴露的流程问题是：只改访问语句而未补初始化、只验证局部函数而未走生产事件链、只修单个崩溃点而未检查同类入口，以及把“未触发”误当成“没有缺陷”。后续评审须对照创建、读取、更新、迁移、销毁与恢复路径检查，遵循 `AGENTS.md §11.4`，并保留明确的运行证据边界。

## 5. 验证现状与事故关闭条件

现有新增测试共 9 条，**仅确认源码存在，均未在本次运行**：

| 测试文件 | 新增用例 |
|---|---|
| [skill_test.cpp](../../cpp/tests/skill_test/skill_test.cpp) | `ValidateTarget_TargetInAnotherScene_ReturnsError`、`ValidateTarget_FrozenTarget_ReturnsError`、`HandleSkillFinish_SelfTargetedContextIsErasedOnce`、`CheckCooldown_MissingCooldownComp_ReturnsError`、`TransferErrorMessageTest.KeepsDirectResponseErrorWhenTlsTipIsEmpty`、`TransferErrorMessageTest.MovesTlsTipIntoResponseAndClearsIt`、`EcsContextTest.FirstActorEntityIntegralIsNotZero` |
| [player_mission_system_test.cpp](../../cpp/tests/bag_test/player_mission_system_test.cpp) | `AutoRewardIsGrantedWithoutDispatcherUpdate`、`AutoRewardReachesPlayerOnRecycledEntitySlot` |

原修复会话给出的 Codex 验证入口如下，工作目录为仓库根，Debug / x64，MSBuild 严格串行。本次没有执行这些命令：

```powershell
msbuild game.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter skill
pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter missions
pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter bag
pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter buff
pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter aoi
```

共享工作区还有其他会话改动；运行失败应按文件归因，保留编译关键错误、gtest 断言和脚本汇总，不能覆盖他人代码或通过跳测结案。若需要修复前红证据，应在保留当前修改的隔离快照中执行，不直接回退共享工作区文件。

后续关闭条件仍全部待完成：

- [ ] 记录实际受影响版本、首次引入点和部署范围；查明是否发生漏奖、非法跨场景作用或存档覆盖，再据证据决定补偿与数据修复。没有证据时保留“未知”。
- [ ] 完成受影响构建和上述回归，9 条新增用例均通过；旧用例不得依赖测试额外冲刷生产不会冲刷的队列。
- [ ] 走真实 actor 初始化和完整任务链：首次进场、复用槽位、自动奖励、满包重试、重复领取、任务链重入；验证效果与状态一致。
- [ ] 专项验收实时技能变化：冷却拒绝、目标跨场景 / 冻结 / 下线后的延迟落点、自目标收尾、伤害与 Buff、受击方语义，以及死亡 / 复活边界。不能用冷却组件缺失单测代替完整玩法验收。
- [ ] 覆盖 Buff 添加触发 rehash、子 Buff 驱散当前条目、属性组件不齐，以及节点带真实出站连接关机的场景；现有 9 条新增测试没有覆盖全部这些风险。
- [ ] 完成 F022 的 gRPC / etcd 资源关闭与排空，并分别验证节点摘除、进程退出；TCP 改动不能代替此项。
- [ ] 完成 F059 / F060 的生成源和调用方修复，证明缺必需组件时不写入零值快照，并修正文档历史例外。
- [ ] 完成 F001 / F002 的合法生产者清单、命令白名单及非法事件拒绝测试，验证 `BindSessionEvent` 等合法入口无遗漏。
- [ ] 更新 Java 对账并完成相关行为验证；保留有意差异及其依据，不把本次文档登记计作双版本功能完成。

## 6. Java 版对账

已只读核对 `D:/luyuan/xuanming-server-mmo`，Java HEAD 为 `52cccf2`，版本仍为 **`0.1.0-SNAPSHOT`**。本次只在该仓 `PARITY.md` 追加事故登记，没有修改功能或版本，未执行 Java 构建、测试或联机验收。

| 事项 | Java 当前状态 |
|---|---|
| 任务完成、自动领奖、后续任务链 | 已有同步 `Cascade` 实现和对应测试源码；本次未复跑，不能套用 C++ entt 队列与句柄截断缺陷。 |
| 技能目标隔离与冻结 | **待同步**。`SkillRules` 只判本节点玩家存在性和回合战斗状态，`SkillService` 使用任意场景 `world.playerByEntity`；没有同场景 / 目标冻结过滤。`SkillFeature` 还保留冻结施法者 `ALLOW` 策略，需结合实时效果边界重新核对。 |
| 拒绝码、冷却与施法阶段 | 拒绝码直接写应答；玩家必有 `PlayerSkillState`，冷却、引导、后摇已有实现。`PARITY.md` 记录 2026-10-03 的 15 项 robot 历史通过；本次没有复跑。 |
| 实时伤害与实时 Buff | **尚未启用**。旧 `PARITY.md` 和 `SkillService` 注释以死亡状态、死亡通知、复活点未形成完整流程为由保留关闭；C++ 本批补齐上下文后产生新的效果可达性差异。旧“两版都未生效”不能继续充当当前已对齐证据，需随功能处置修正。 |
| entt / muduo、ECS Marshal、Kafka 全本地事件分发 | Java 没有这些具体实现路径；本批实现缺陷不直接适用，不代表 Java 同类数据和生命周期风险已经全面审计。 |

Java 代码定位：`game-scene/src/main/java/com/game/scene/skill/{SkillRules,SkillService,SkillFeature}.java`，以及 `world/{SceneWorld,ScenePlayer}.java`。Java 目标隔离、冻结策略和实时效果边界继续以该仓 `PARITY.md` 的本次事故登记行跟踪；本批没有新增功能版本号，也没有完成双版本验收。
