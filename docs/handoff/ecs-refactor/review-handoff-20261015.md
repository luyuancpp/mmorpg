# 交接：ECS 改造的代码审查（第一波已完成，第二波与复核待做）（2026-10-15）

> **给谁**：接手审查的 AI 会话（负责人会用另一个账号开）。读完本文就能接着做，不需要看原会话。
> **当前状态**：第一波 6 个方向已查完（46 条初步发现，见附录 A，**都未经复核**）。第二波 5 个方向、全部复核、正式报告和给 Codex 的补充任务单都还没做。
> **原会话为什么停下**：账号周额度用满。

---

## 1. 背景

- **负责人的诉求**（原话）：
  - 「我感觉我代码中很多地方滥用了 ecs，哪些滥用了、用得不对或者有 bug 的帮我修复并改动，我不怕大改」；
  - 「ecs 应该用在它该用的地方」「不正常代码了，不可读」；
  - 「已经改完了，你按你的检查，他们改的不一定对，按最标准的检查」。
- **来龙去脉**：
  1. 2026-10-14/15 做了 entt 误用审计，共 110 条（F001–F100、C001–C010），见 [ecs-entt-misuse-audit-20261015.md](../../design/ecs-entt-misuse-audit-20261015.md)。
  2. 负责人确认了组件三类初始化口径，见 [ecs-component-access-rules.md](../../design/ecs-component-access-rules.md)：
     - 持久数据只在加载时建，缺了就拒绝；
     - 按需状态在唯一写入口延迟创建，读不到当空；
     - 派生数据在创建时算好。
  3. 第 1 批由 Claude 的工作流实施，第 2–5 批和补充收尾由 Codex 实施。任务单与回执都在本目录：[README.md](README.md) 的 §7（第 2–5 批合并回执）、§8（收尾补充），各批 `batch*.md` 的 §9/§10，以及 `docs/PROGRESS.md` 里「ECS 第 N 批」各条。
- **本次审查对象**：Codex 交付的全部 ECS 改动，即 `git diff 6281e4d27 -- <文件>`。这个范围包括已快照提交进 `ab0f27907` 及之后几个提交的部分，以及工作区未提交的部分。
- **审查时的注意事项**：
  - 同一批文件里混有其他会话的改动（RPC 通道重构、gRPC→muduo 迁移、好友申请代次、玩家收件箱等），只审 ECS 范围。
  - 原会话审查期间，已有提交 `526780b4e 补充协议重生成结果与场景传输接线`，生成产物可能已经重生。审查时以当前代码为准。

## 2. 审查规则（负责人的偏好，必须遵守）

1. **只读审查**。审查代理不改任何文件，也不在仓库内外新建任何文件。按提交读文件时，只能用 `git show <rev>:<path>` 直接输出到终端，或用 `git grep <rev>`，**禁止重定向成文件**（以前的代理在 %TEMP% 留过副本）。工作流结束后，要核对代理的 Bash 记录里没有 `git show ... > 文件`。
2. **编译由负责人自己做**。不要派代理判断「能不能编过」，也不报纯编译错误。只看语义与运行期：
   - 正确性、数据一致性；
   - 并发与生命周期；
   - 协议与契约兼容；
   - 是否违反规则；
   - 测试是否真能测出问题（修复前会红）；
   - 可读性与 ECS 适用性。
3. **按额度分波**。启动工作流前先查用量：
   - 周额度超过 80% 时，每波 6 个代理以内；
   - 11 个代理同时跑，曾经 14 分钟就撞额度、零结果。
4. **审查基准（「最标准」）**：
   - AGENTS.md：§4 proto 规则、§7 不变量、§11 设计原则（KISS/YAGNI/DRY/显式依赖/信息隐藏）、§11.6 实例/布局分层、§11.7 连接与回调生命周期；
   - 三类初始化口径；
   - EnTT 3.13.2 源码语义（`third_party/entt/src/entt/`，`ENTT_ID_TYPE=uint64_t`）；
   - ECS 适用判据：大量同构实体 + 按组件组合批量处理，才进 registry；单例、请求级数据、网络资源、私有索引、跨实体共享对象不进；dispatcher 只做可有可无的一对多通知，必需步骤直接调用。
5. **严重度定义**：
   - blocker：可达路径上必然崩溃或 UB、资产或存档损坏、安全漏洞、合法业务被静默丢弃；
   - high：可达的逻辑错误、契约破坏、并发或生命周期缺陷；
   - medium：潜在缺陷、违规且有后果、修复缺回归测试或测试无效、任务单或挑战意见要求未落实、明显过度设计；
   - low：可读性问题、注释与实现不符、文档小错。

## 3. 第一波结果摘要（未复核；全文见附录 A）

| 方向 | 条数 | 最重要的 |
|---|---|---|
| 第 1 批 存盘三态 | 4 | W1-01 medium：拒存的退出一律保留实体，没有有界出口，重连会复用一个永远存不了盘的实体（偏离任务单 batch1-verify §1.2） |
| 第 3 批 背包 | 11 | 新增的选择顺序用例至少 5 例必红；旧用例与「先规划再发号」冲突；`Clone` 派生类漏覆盖时被静默切片 |
| 第 4 批 Kafka 白名单 | 6 | Kafka 必需命令仍经 dispatcher，没有监听者时被静默吞掉；白名单单测把上界写死为 49；一次性迁移逻辑被固化进生成器每轮执行。**没有发现漏进白名单的生产者** |
| 第 4 批 句柄与 ActorNetIds | 10 | 必需步骤（沉默、升级重算）仍经 dispatcher 一对一转发；保留了零监听事件，又给它们加了大段重入复核；66 号属性消息的 entity_id 在增量里恒为 0（存量问题） |
| 第 5 批 生命周期 | 8 | 为没有生产订阅者的通知加了重入防护，防护分支会造出半成品状态；离场时全量扫描 AoiListComp，批量退出变成 O(N²)；离场序列写了两份 |
| 补充收尾 | 7 | **唯一的 high**：私有场景的 creators 准入只在 C++ 末端生效，「按场景号加入镜像」「组队跟进镜像」「镜像去重复用」都会被拒，且与 Java 版对账口径相反；停机时先 LeaseRevoke 紧接着 TryCancel，优雅注销退化 |

**共性问题**：Codex 倾向于「保留本该删的东西，再加防护兜住」，例如零监听事件、dispatcher 承担必需步骤、gRPC 状态继续放在 registry 里。这正是负责人说的「不可读」。正式报告要把它作为主线写清楚，补充任务单要给出「删掉」的直接写法，而不是「再加一层防护」。

## 4. 待做一：第二波审查（5 个方向）

每个审查者都要先 `git diff 6281e4d27 -- <相关文件>` 看实际改动，再读最终代码与调用方。返回结构与第一波相同：file、line（当前工作区行号）、batch、category、severity、confidence、title、evidence（贴 1–8 行原文）、reasoning、fix（具体到函数和语句），外加 coverage（核对过哪些、确认无问题的部分）。

1. **b1_runtime｜第 1 批：运行时三类初始化**
   - 范围：spatial（aoi/grid/interest、OnSceneCreated）；actor_attribute_calculator，含新的 `InitializeOnCreate` 及它在 `InitPlayerFromAllData` 里的调用顺序；actor_action_state、player_attribute、player_pet；buff.cpp（C003 no_caster 叠层、BuffListComp 创建点、空列表帧末清理）；skill.cpp（冷却和上下文延迟创建、相位定时器、纯校验前置、延迟落点与引导重验、失败按技能实例清理）；currency_system、player_battle 结算账本、InBattleComp 首写 helper、combat_state、AnomalyDetector 玩家窗口组件。
   - 要求：逐个组件判定它是持久数据、按需状态还是派生数据，并核对四点：创建点是否唯一且位置正确；读路径是否不建；按需组件的写入口是否被误改成 `try_get`，导致永远没人创建；持久数据缺失时是拒绝还是补了默认值。
2. **b2｜第 2 批：单例改为所属对象的显式成员**
   - 范围：`Node::localNodeInfo_` 与 `GetNodeInfo` / `MutableNodeInfo`；node_utils 中 `GetNodeInfo` / `GetZoneId` 这两个自由函数的原子安装指针门面（安装与卸载时机、Node 析构后悬空、测试进程未安装时的默认实例、跨线程读的数据竞争）；ServiceDiscoveryManager 的 ServiceNodeSnapshot；`EcsContext::sessionPlayers` 与 `Clear`；请求级 tip 直接写 response（9 个玩家 handler、`CHECK_PLAYER_REQUEST`、`SendErrorToClient`、`InvokePlayerService`、RpcDoneCallback 延后完成路径、Empty 应答）。
   - 要求：检查全局单例删除后，有没有换个名字的等价物。评估「原子安装指针门面」是否比直接传依赖更难懂、是否真有必要。
3. **b5_types｜第 5 批：组件类型**
   - 范围：`Guid` → `PlayerGuidComp.value`，只看语义：有没有地方仍按 uint64_t 组件池读写，结果读到别的组件或读不到；attribute_sync 模板生成的取值逻辑。四个 std 容器别名改成具名类型。`sceneRegistry` 改成 `entt::basic_registry<SceneEntity>` 后，场景句柄与 actor 句柄在语义上是否真正分开（边界上有没有 static_cast 互转）。`ECS_GET_OR_*` 宏与 sceneRegistry 的配合。SceneEntity 的 hash、null 比较与版本位语义。
4. **cross_sem｜跨批：调用点语义与残留误用**
   - (1) `6281e4d27` 以来语义有变化的函数（void 改 bool 或三态、新增失败分支、删除的函数和宏），全仓 grep 调用点（含 cpp/tests、cpp/nodes/battle、模板），找出忽略新失败结果、把失败当成功、或依赖旧副作用（比如以前会顺手补建组件）的地方。
   - (2) 在非生成代码里扫残留误用：读路径上的 `get_or_emplace`；`to_integral(entity)` 写进 proto、日志冒充业务 id 或下发客户端；entity 截成 uint32；跨 registry 混用；遍历中修改容器；长期持有裸 `TcpConnectionPtr`；dispatcher 承担必需步骤。
   - (3) 规则文档与最终代码是否一致：ecs-component-access-rules.md、审计文档、`.github/copilot-instructions.md`、`cpp/AGENTS.md`。
5. **fit｜跨批：ECS 适用性与可读性**（负责人最关心的一项）
   - (1) 对照 [ecs-fit-overview.md](ecs-fit-overview.md) 的逐子系统结论和 S0–S9 步骤，逐项判断落实情况：做了、部分做了、没做。
   - (2) 全仓找改造后**仍然**不该在 registry 里的东西：NodeContextManager 节点表、RpcSession/RpcClientPtr/CQ/Stub/GrpcClientRuntimePtr 组件、私有索引、shared_ptr 组件、带行为的组件；以及 dispatcher 仍承担必需步骤或链式业务的地方。
   - (3) **可读性**：Codex 新写的代码里有没有过度设计、为了守规则而绕远路、命名晦涩、一个概念多套名字、要读很多文件才能看懂的控制流。重点看原子安装指针门面、EntryOwnershipGuard、TimerTaskComp 的 State、候选 Clone 事务、ActorNetIds、白名单生成器，并给出更直接的写法。
   - (4) 给出一页结论：这个仓库里 ECS 该用在哪、不该用在哪。

原会话的工作流脚本在本机 `C:\Users\luyua\.claude\projects\D--luyuan-xuanming-server-mmo-cpp\0928ea0b-4f45-4a9d-bb03-add801609964\workflows\scripts\ecs-refactor-review-find-v2-wf_bd298a7e-5c0.js`。它支持 `args: {"keys": [...]}` 只跑指定方向，第二波传 `["b1_runtime","b2","b5_types","cross_sem","fit"]` 即可。读不到时，按本节描述自己写。

## 5. 待做二：复核

把第一波与第二波的全部 high/medium（低置信度的 low 不必）按文件分组，每组不超过 5 条，交给**对抗式复核者**：
- 要求复核者亲自读代码，**尽力推翻**每一条；拿不出代码证据证明成立的，默认判为不成立。
- 结论只有三种：成立；降级（给出正确的严重度，或更正描述与修法）；不成立。
- high 及以上，再加一位独立复核者。两人意见冲突时，标为「有争议」，交负责人判断。

有两条复核时要特别注意：
- **W1-40 creators 准入（唯一 high）**：要读 Go scene_manager 的镜像加入、组队跟进与镜像复用路径，以及 Java 仓库 `D:/luyuan/xuanming-server-mmo` 的 PARITY.md 和对应实现，确认这三条合法流程是否真的会被拒。修法需要负责人拍板：撤掉 C++ 末端准入，还是把准入挪到 scene_manager 选场景之前、两版统一。
- **W1-01 拒存退出保留实体**：判断「丢弃实体、不写 A1′、不宣称已落盘」是否满足骨架不变量（I0/I1/A1′），并核对资产重投与结算补偿在玩家重登后能否正确收敛。

## 6. 待做三：交付物

1. **正式审查报告**：`docs/handoff/ecs-refactor/review-report-20261015.md`。按批次列出复核后成立的问题（严重度、位置、证据、修法），单列「有争议」和「需负责人拍板」。另写一页「ECS 该用 / 不该用 + 落实情况」。主线写清第 3 节的共性问题。
2. **给 Codex 的补充任务单**：`docs/handoff/ecs-refactor/batch6-review-fixes.md`。格式照本目录其他任务单（README §3 的通用规则、§6 的回执格式）。每条写到函数和语句级，给出修复前会红的回归测试。偏好「删掉多余的东西」，不要「再加一层防护」。要包含下面这个调试工具：
   - **打印实体全部组件**（负责人问过「entt 能不能看到某个 entity 的所有组件」）。EnTT 没有实体到组件的列表，但可以遍历所有组件池查出来：
     ```cpp
     for (auto [id, storage] : registry.storage()) {
         if (storage.contains(entity)) {
             // storage.type().name() 是类型名(MSVC 下形如 "struct BuffListComp")
             // storage.value(entity) 是 const void*,对 protobuf 组件可按类型登记打印函数输出 ShortDebugString()
         }
     }
     ```
     做成一个 GM 命令，外加一个出错时打日志的函数，例如 `DescribeEntity(registry, entity)`。代价是遍历所有组件池，**不进每帧逻辑**。`registry.orphan(entity)` 可以判断实体是否一个组件都没有。
3. **审计文档 HEAD 状态**：在 [ecs-entt-misuse-audit-20261015.md](../../design/ecs-entt-misuse-audit-20261015.md) 里，按复核结果更新 110 条的状态（已修、部分修、未修、已移交），并注明「源码已改，运行证据待负责人编译与测试」。
4. **`docs/PROGRESS.md` 末尾追加一条**：只追加，不改旧条目。按 AGENTS.md §12 写明 Java 版状态。
5. **不 commit、不 push**。

## 7. 已知的需负责人拍板事项（复核后写进报告的「拍板」节）

- 私有场景 creators 准入的位置与口径（Go 与 Java 两版统一）。
- 拒存退出的处置：保留实体（Codex 的做法）还是丢弃实体但不宣称已落盘（审查者建议）。
- 第 4 批 ActorNetIds 改变了客户端可见 actor 号的取值区间与重启语义：需要按 F080 的三项客户端确认书面记录。客户端仓库 `../mmorpg-client` 未获授权不得读写。
- C004 删除后，proto `item_comp.proto` 里的单人物品封禁消息是否一并删除。

---

## 附录 A：第一波全部发现（未复核，原文）

编号 W1-xx 只在本交接与后续报告中使用。行号是原会话审查时的工作区行号，之后代码可能已变，复核时按函数名重新定位。

### A.b1_save 第 1 批：存盘/快照只读化与存盘三态

#### W1-01 [medium] [design] 置信度 medium：拒存退出一律保留实体，没有有界出口；重连会复用这个永远存不了盘的实体

- 位置：`cpp/libs/services/scene/player/system/player_lifecycle.cpp:3086`
- 证据：

```
	auto saveResult = SavePlayerToRedisImpl(player, /*allowSkipWhenPersisted=*/true);
	if (saveResult == SaveResult::kRejected)
	{
		LOG_ERROR << "HandleExitGameNode: save refused for player " << exitingPlayerId
				  << "; keeping the entity and exit intent, not finishing exit";
		return;
	}
```

- 推理：任务单 batch1-verify §1.2 要求拒存时打出真实原因的 ERROR，然后 FinishExitAfterPersist。Codex 改成在所有退出收尾口都保留实体、UnregisterPlayer 和退出意图（回执“与原任务单不一致之处 1”），并把这条写进了 docs/design/ecs-component-access-rules.md:61「不得……销毁未保存实体」。涉及的行：3086-3091 首次退出、2996-3001 耗尽后重踢、2223-2231 落地回调比对、3325-3332 DBTask 闸门重存、3395-3401/3425-3429 OnDbTaskDispatchSettled、4454-4458 旧 epoch 补存。

保留实体保不住任何东西：持久组件只在加载时创建，缺了不会再回来，所以这个实体的内存态永远存不了盘。保留只带来几个问题：
(1) 出口不存在。重踢 2984-2986 只对 resaveRounds 到上限的实体生效，拒存时 resaveRounds=0；停机谓词按 IsSaveInFlight 一直等，只能靠看门狗；排空、疏散票据也收不了尾。
(2) 实体会被复用。玩家在 30s 租约内重连回本节点时，EnterGameNode 2525-2528 走 EnterScene，CancelExitOnReconnect（2938-2948）复活这个实体。之后每次周期存盘、资产存盘、结算存盘都会被拒，未存盘的内容随在线时长无界增长。Go 资产指令 durable 永远是 false，结算 pending 一直销不了账。
(3) 已发交接标记的退出也被保留。SavePlayerToRedisImpl 4240-4248 把完整性校验挪到了交接跳过之前。但标记发出时 I1 已经成立（Redis ⊇ 冻结内存），此时按 exit-wins 收尾本来不会丢任何数据，现在却要冻结等到 freeze cap。测试 IssuedHandoffCannotHideMissingPersistedComponent 把这个行为钉死了。

反过来，如果销毁实体但不写 A1′：玩家重登会从上次完好的存档重载；资产指令因为盘上账本没有这个 seq 会被 Go 重投并补应用；结算由登录钩子补。语义正确，丢失量也有上界。

- 建议修法：拒存的退出收尾口（HandleExitGameNode 首次退出与重踢、HandlePlayerAsyncSaved 2223 分支、HandlePlayerSaveRejected 4454 分支、FinishExitOrAwaitDbTaskAck/OnDbTaskDispatchSettled 的拒存分支）改成「不宣称已落盘地丢弃」：先 `if (auto* intent = try_get<PlayerExitIntentComp>(e)) intent->releaseMarkSuppressed = true;`（盘上不是最终态，所以不写 A1′），再调 FinishExitAfterPersist(playerId)。这样能复用它的疏散改派、交接标记撤回和摘会话逻辑，又不留僵尸。另一种做法是在 PlayerExitIntentComp 上记「已拒存」，EnterGameNode 复用实体前遇到它就按 DiscardDeposedEntityOnReentry 的方式销毁后重载。同步修改：ecs-component-access-rules.md:61 区分「宣称收敛 / 写 A1′ / 写交接标记」（禁止）与「不存盘丢弃」（允许）；SavePlayerToRedisImpl 对 requestedAtMs!=0 可以继续返回 kHandoffAlreadyRequested；把 MissingPersistedComponentDoesNotFinishInline、ExhaustedRekickWithMissingComponentRetainsEntity、StaleEpochResaveRejectedRetainsExitingEntity、IssuedHandoffCannotHideMissingPersistedComponent 的断言改为：实体已销毁、未写 A1′、落盘快照未被刷新。

#### W1-02 [low] [readability] 置信度 medium：三处用整份 MarshalPlayerForSave 只为拿一个「组件齐不齐」的布尔值，紧接着又整份 Marshal 一次；提交点的 kRejected 分支因此成了死代码

- 位置：`cpp/libs/services/scene/player/system/player_lifecycle.cpp:4717`
- 证据：

```
	PlayerAllData verifiedState;
	if (!MarshalPlayerForSave(player, playerId, verifiedState))
	{
		return genericFailTip;
	}
```

- 推理：StartTravelHandoff 4717-4721、BeginTravelHandoff 5244-5249、B5 RollbackAdopt 5677-5683 都构造一份完整的 PlayerAllData，深拷贝背包、任务和兼容镜像，再丢掉，只用它的返回值。StartTravelHandoff 随后 4763 的 SavePlayerToRedisImpl 又 marshal 一次。预检和提交点之间没有任何代码会摘持久组件，所以 4764-4768 的 kRejected→Abort 分支实际走不到。失败时打出的日志是「[PlayerSave] ... refusing to marshal player_database (load invariant broken; nothing is written)」，在交接预检的语境里会误导排障。同一份「15 个持久组件」清单的权威来源在 PlayerDatabaseMessageFieldsMarshal 里，抽成谓词并不违反 DRY。

- 建议修法：从 player_database_loader.cpp 抽出 `bool HasAllPersistedComps(entt::entity, std::string* missing)`（即当前 FindPersistedComp 那段），PlayerDatabaseMessageFieldsMarshal 和三处预检共用。预检改为调用它，并打出带调用点的 ERROR，例如 "[ZoneTravel] handoff precheck: missing=..."。StartTravelHandoff 提交点的 kRejected 分支保留为防御，注释写明预检之后不可达。

#### W1-03 [low] [doc] 置信度 high：结算 PersistFn 返回 false 的注释和日志仍写「两种来源」，漏了拒存

- 位置：`cpp/libs/services/scene/battle/system/player_battle.cpp:400`
- 证据：

```
		if (!gPersistFn(player))
		{
			// false 有两种来源:脏比较判定「盘上已是这一份」,或交接/退出在途跳过写盘。
			// 两者都不能当成「已落盘」—— 是否 durable 一律由 IsSettlementDurable 按字节判。
			LOG_DEBUG << "[PlayerBattle] Settlement save produced no write(unchanged data or save skipped), player_id="
```

- 推理：现在 SavePlayerToRedis 返回 false 还有第三种来源：缺身份或持久组件被拒存。asset_op_system.cpp 221-245 已同步改了措辞，结算这一侧没改，和回执「资产 bool 返回 false 不再被日志一概解释为脏比较相等」的口径不一致。行为本身没错，durable 仍然只按落盘快照判定。

- 建议修法：注释改为「false 只表示没有排入新写：已落盘相等、交接已发起、或缺身份/持久组件拒存」。DEBUG 文案改为 "no save queued (unchanged, handoff skip or refused)"。

#### W1-04 [low] [doc] 置信度 high：SavePlayerToRedisImpl 头注释仍写「只有三个调用方」传 allowSkipWhenPersisted=false

- 位置：`cpp/libs/services/scene/player/system/player_lifecycle.h:1440`
- 证据：

```
	// SavePlayerToRedis 的本体。allowSkipWhenPersisted = false 时跳过 dirty-save 快路径,
	// 但持久组件缺失仍拒绝,交接已发起时的"不得再写"仍然生效。快路径判"盘上已是同一份",但同 key 还有在途 / 排队中的存盘时,
	// 必须改走一次真实存盘、等落地回调收尾。只有三个调用方:
```

- 推理：本批改了这段注释的前两行，但没更新调用方计数。实际传 false 的有 5 处：M4 3103、交接补写 4782、B5 5698、DBTask 闸门 3320、OnDbTaskDispatchSettled 3420。cpp 4283-4286 的注释已经列了 DBTask 闸门。头文件与实现说法不一致。

- 建议修法：把「只有三个调用方」改为列出全部 5 个调用点，或者删掉计数，改为引用 cpp 里的清单。顺手在 SaveResult 各枚举值上补一句含义（kQueued 只表示已入队、不等于已落盘；kRejected 不是任何落盘凭据）。

<details><summary>该审查者的核对范围（coverage）</summary>

核对范围：git diff 6281e4d27 下各文件的 ECS 第 1 批改动，以及最终代码与调用方。

【确认无问题】

player_database_loader.cpp
- Marshal 先用 FindPersistedComp 检查全部 15 个持久组件，再写字段。组件指针之间没有 emplace/remove，指针稳定；currency 的 CopyFrom→SaveToProto 顺序保持。
- Unmarshal 无条件 emplace 全部组件；背包、任务经各自 Unmarshal 的 get_or_emplace 创建。首登空记录同样建齐 15 个。
- grep 确认生产代码里没有任何 remove 这 15 个组件的路径；唯一的玩家创建点是 InitPlayerFromAllData。
- PlayerCurrencyComp 改为 emplace 后，唯一调用方传入的是新实体，安全。

player_database_1_loader.cpp、player_data_loader.h 及两个模板
- 先拷一份候选消息，成功后 Swap：失败时原输出（内容与 presence）保持不变，成功时整份替换，逻辑上是原子的。
- 所有生产调用方（存盘、两处收敛比对、三处预检、快照）传入的都是新建消息，没有复用输出导致残留的风险。
- h.tmpl 与手写头文件一致；cpp.tmpl 的产物写到 PlayerStorageTempDir，不会覆盖手写版本。

merge_state（字段 10）
- 场景存盘每次构造新消息，确实不带这个字段。
- 但全仓没有任何写入方：tools/merge_zone 改用 Redis flag 键，login 把 flag 放进 EnterGame 应答，不写 blob。所以不存在覆盖造成的数据丢失，不报。

SaveResult 调用点（逐个核对，均正确区分了拒存）
- HandlePlayerAsyncSaved：比对与重存 2223/2264
- 重踢 2995
- 首次退出与 M4 3085/3103
- DBTask 闸门 3320
- OnDbTaskDispatchSettled 3395/3420
- 旧 epoch 补存 4449
- StartTravelHandoff 4718/4763/4782
- BeginTravelHandoff 5245：写标记前复检
- B5 5678/5698
- bool 薄包装 4192：只在 kQueued 时返回 true，与基线语义相同
- 交接拒存不会写 handoff 标记；「converged / already persisted」日志只在 kAlreadyPersisted 时打出；拒存不更新落盘快照。
- B5 预检拒绝后保持冻结，由 freeze cap（6050-6095）最终收口。

InitPlayerFromAllData
- 先查重再 create；首登两处 get_or_emplace 已改为 get；InitializeOnCreate 的失败分支不可达。

snapshot_system
- marshal 在 TryNext 之前，失败不消耗号段。

dirty_save_stats 与 redis.cpp
- save_marshal_refused 独立计数，不进 total/skipped 分母，30 秒汇总在 >0 时打 WARN。

asset_op_system 与 player_battle
- PersistFn 仍是 bool；durable 只按 PlayerLastPersistedSnapshotComp 判定，拒存不会被当作已落盘。
- Decide 在任何副作用之前检查两份货币组件；FindSettlementLedger 只取不建。

测试
- persistence_test：15 个组件逐一拒存、原输出保留、SaveRefuses、SaveWithoutGuid、InitFromAllData 查重。
- snapshot_test：只读断言。
- bag_test SnapshotIsSkipped：夹具已补齐。
- cross_zone_test：MakeExitingPlayer、MakeOnlinePlayer、MakeFrozenHandoffPlayer、EquipInstance 都改用 CreatePersistedPlayerComps；MarshalAsSaved 返回 bool 并加了 ASSERT_TRUE，快路径与收敛用例确实回到了原本要测的分支。
- 新增 ExitPersistEcs×5、TravelPersistEcs×2、TravelOwnershipEcs B5：按修复前代码推演，均会变红。
- equip_attribute_test 1757/2494 已加 ASSERT_TRUE。
- db_task_dispatch_test 覆盖了闸门重踢时的拒存。

【未覆盖、未报】
- OnDbTaskDispatchSettled 拒存分支没有回归用例，save_marshal_refused 计数没有断言（影响低）。
- 同文件中其他会话的改动（DBTask 闸门本体、RPC/gRPC、场景权限、通知重入、InBattleComp helper、背包 AddItems）只在与拒存交互处核对，没有独立审查。

【已报问题】
- 拒存退出保留实体且没有有界出口（medium，偏离任务单 §1.2）
- 三处整份 Marshal 预检（low）
- player_battle.cpp:400 注释漏了拒存（low）
- player_lifecycle.h:1440「只有三个调用方」过时（low）

</details>

### A.b3 第 3 批：背包去私有 entt 与确定顺序（含 C004）

#### W1-05 [medium] [test_invalid] 置信度 high：新增选择顺序用例没有跟上「扣空即释放 / 加载跳过零堆」，至少 5 例必红或已测不到原规则

- 位置：`cpp/tests/bag_test/bag_selection_order_test.cpp:149`
- 证据：

```
73-78:  const auto *item = bag.GetItemCompByGuid(guid);
        EXPECT_NE(nullptr, item);
149:    EXPECT_EQ(0u, StackSize(bag, kGuidB));   // ExpectLowestSlotsDrained
214:    EXPECT_EQ(0u, StackSize(bag, kGuidB));
221:    bag.InsertItemForRestore(kGuidA, kStackConfig, 0, 0, 1);
228:    EXPECT_EQ(0u, StackSize(bag, kGuidA));
254:    bag.InsertItemForRestore(kGuidA, kStackConfig, 0, 0, 1);
259:    ASSERT_EQ(2u, destroyed.size());
```

- 推理：§10 收尾把 Bag::DrainOneConfig 改成扣到 0 当场 DestroyItem（bag_system.cpp:352-355），InsertItemForRestore 对 size==0 直接 return（bag_system.cpp:1368-1371）。但本文件仍按旧语义断言：(1) ClampedDrainTakesLowestSlotFirst(190)、ExactDrainTakesLowestSlotFirst(199) 共用 ExpectLowestSlotsDrained，kGuidB 已被销毁，StackSize 里的 EXPECT_NE(nullptr) 失败；(2) DrainWithoutReceiptUsesTheSameSlotOrder(208) 第 214 行同理；(3) DrainSkipsEmptyStackWithoutReceipt(218) 的零堆根本插不进去，228 行 StackSize(kGuidA) 失败，用例已无法构造它声称要测的「空堆」；(4) MergeNeverPicksAnEmptyStackAsSurvivor(249) 零堆 A 被跳过，合并后只退役 C，destroyed.size()==1，259 行 ASSERT 失败。回执 §10 写「原有 18 项选择顺序测试仍保留」，实际有 5 项会红，且 2 项已测不到原规则。bag_test.cpp 里同类旧用例已被改过（RemoveItemByPos、DrainedNonStackable…），唯独这个新文件漏改。

- 建议修法：ExpectLowestSlotsDrained 和 214 行改为断言 `EXPECT_EQ(nullptr, bag.GetItemCompByGuid(kGuidB)); EXPECT_EQ(kInvalidSlot, bag.GetItemPosByGuid(kGuidB)); EXPECT_TRUE(bag.IsLayerConsistent());`。DrainSkipsEmptyStackWithoutReceipt 和 MergeNeverPicksAnEmptyStackAsSurvivor 下沉到 ItemStore 层：直接 `ItemStore store; store.Insert(MakeInstance(A,0,1))…`，对 DrainStacks 和 MergePartialStacks+CollectEmptyInstances 断言（零堆不进回执、不当幸存者、按实例序回收）。或者删掉这两例，在 Bag 层改测「零数量还原被丢弃」，这一点已由 BagJavaParityTest.RestoreDropsZeroStacks… 覆盖。

#### W1-06 [medium] [test_invalid] 置信度 high：旧用例 VectorBatchNeverEvictsWhenSegmentIsUnavailable ② 与新的「先规划到不动点再发号」语义冲突，会红

- 位置：`cpp/tests/bag_test/bag_test.cpp:4080`
- 证据：

```
4080:    // ② 全部沿用预设 guid、但现状放不下:发号源不可用时**不许靠腾位解决** ——
4082:    InitItemParam preassigned = MakeItem(kNonStack1, 1);
4083:    preassigned.itemPBComp.set_item_id(940003);
4084:    EXPECT_EQ(kBagAddItemInvalidParam,
4085:              bag.AddItems(std::vector<InitItemParam>{preassigned}, &evicted));
4086:    EXPECT_TRUE(evicted.empty());
4088:    EXPECT_NE(nullptr, bag.GetItemCompByGuid(940001));
```

- 推理：新 Bag::AddItems 先用 PlanEviction 规划到不动点（victims=[940001]，newInstanceCount=1）。preassigned.size()==1，所以要铸的号是 0 个，循环 683-692 不调 MintGuid，候选提交成功。返回 kSuccess，evicted=[940001]，940001 被销毁。用例期望 kBagAddItemInvalidParam、不淘汰，所以 4084/4086/4088 都会失败。③(4091-4095) 再次提交同一 940003 时撞包内已有 guid，返回 kBagDeleteItemAlreadyHasGuid，也会失败。新行为本身合理：旧闸门「发号器不可用时连腾位也不做」是为了防止淘汰后重规划才发现要铸号，新实现先规划后铸号，这个风险已经消除。问题在于这个旧用例和它的注释没有同步，只改了同文件的其它用例。

- 建议修法：按新契约重写 ②：发号源不可用、全部沿用预设 guid 的批次在临时格上应该允许按 FIFO 淘汰并成功，断言 evicted==[940001]、940003 已入包、两层一致。③ 改用另一个预设 guid。同时删掉 4080-4081「不许靠腾位解决」的旧理由，并在 4032 行注释里去掉 ReserveBatch。

#### W1-07 [medium] [design] 置信度 medium：提交时用 Clone 出的对象整体替换 layout_，派生类漏覆盖 Clone 时会被永久切片且没有任何检查

- 位置：`cpp/libs/modules/bag/bag_system.cpp:773`
- 证据：

```
bag_system.cpp:753  auto candidateLayout = layout_->Clone();
bag_system.cpp:772  store_ = std::move(candidate);
bag_system.cpp:773  layout_ = std::move(candidateLayout);
container_layout.h:60-62 // 改变布局行为的派生类必须覆盖本接口，禁止把派生类型切片成父类。
    [[nodiscard]] virtual std::unique_ptr<IContainerLayout> Clone() const = 0;
container_layout.h:144 class FlatLayout : public IContainerLayout   // 非 final
container_layout.h:150-153 Clone() → std::make_unique<FlatLayout>(*this)
```

- 推理：FlatLayout 不是 final，而且已经实现了 Clone。今后任何 FlatLayout 子类（测试里已有 LyingLayout、FailSecondPlacementLayout 两个先例；FixedSlotLayout 也是它的子类）只要忘了覆盖 Clone，照样能编译通过。第一次成功入包后，AddItems 就用切片出的 FlatLayout 永久替换背包布局：具名槽语义、格子几何等行为静默丢失，之后的存档和还原都按错误的布局走。这条约束只写在注释里，代码没有任何兜底。另外，每次入包都替换 layout_ 也改变了布局对象的身份，这一点是本批新引入的，旧实现没有。

- 建议修法：发布前加一道 fail-closed 检查：`if (typeid(*candidateLayout) != typeid(*layout_)) { LOG_ERROR…; return PrintStackAndReturnError(kInvalidTableData); }`，Debug 下同时 assert。更彻底的做法有两种：(a) 只在副本上试放并记下每个新 guid 的落位，成功后在原 layout_ 上 Remove(victims) + PlaceAt(guid, slot) 重放，保持对象身份不变；(b) 把 FlatLayout 设为抽象基或 final，由 CRTP 帮助类统一实现 Clone。

#### W1-08 [low] [design] 置信度 medium：候选事务每次入包都整包复制，包括最常见的纯合堆；可加一条无新实例的快路径

- 位置：`cpp/libs/modules/bag/bag_system.cpp:695`
- 证据：

```
695:    ItemStore candidate = store_;
716:        uint32_t remaining = maxStack > 1 ? candidate.PlanStackIntoExistingStacks(proto, maxStack, fills) : proto.size();
753:    auto candidateLayout = layout_->Clone();
bag_system.h:217-218 // 任何成功写入（含纯合堆）都可能整体替换候选仓库/布局，后续必须按 guid 重新获取
```

- 推理：关于「有没有更简单的等价写法」：数据能触发的失败（校验、准入、淘汰不动点、铸号）都在复制之前已经判完。复制之后能失败的只剩内部不变量被破坏（CanReserve 与 Place 不一致、计划数与实际数不一致），所以对一般批量来说，候选副本是最直白的原子写法，可以接受。代价是：每次 AddItem，哪怕只往已有堆里加几瓶药（战斗掉落最常见的形状），都要深拷贝全包 ItemComp（含 equip 段）和布局的两张哈希表，并且让此前所有 ItemComp* 借用失效。旧实现纯合堆时这些借用并不失效。当前没有生产调用方因此悬空（RemoveGem 已改为按 guid 重取），但契约收紧了，成本也不是由数据驱动的。

- 建议修法：在 AddItems 里加快路径：`if (victims.empty() && newInstanceCount == 0)` 时，逐 entry 在 store_ 上直接执行 PlanStackIntoExistingStacks 加 ApplyStackFill 并记录 written，不复制仓库、不 Clone 布局。聚合计划已经证明总空余够用，逐项贪心填充不会失败，所以仍然原子。只有需要新实例或淘汰时才走候选副本。bag_system.h:217 的契约随之改为「产生新实例或淘汰的入包会使借用失效」。

#### W1-09 [low] [readability] 置信度 high：实例层注释仍按「空堆保留占格、等待回填 / 整理回收」描述，与扣空即释放的实现相反；CanMintGuids 已无生产调用方，注释却仍称必须调用

- 位置：`cpp/libs/modules/bag/item_store.h:121`
- 证据：

```
item_store.h:113 // 一条"被抽走"的回执:哪个实例、抽走多少。实例本身仍在(可能 size==0)。
item_store.h:121-123 // 被抽光的堆 size 变 0 但**实例保留**(槽位也就跟着留着),后续并堆可以再填回去
item_store.h:147-150 // 两条来路:①…DrainStacks 刻意保留空堆…②不可叠加物品被 RemoveItemByPos 扣到 0 … 现在由整理统一回收。
item_store.h:163 // size>0 只约束合并;并堆刻意允许往空堆回填。
item_store.h:177-178 // 铸号型写入必须在**改动任何状态之前**用它 fail-closed
bag_system.cpp:517-522 //  * 可叠加物品被 RemoveItems 抽光留下的空堆; * 不可叠加物品被 RemoveItemByPos 扣到 0 的僵尸
```

- 推理：桥层现在由 DrainOneConfig(352-355) 和 RemoveItemByPos(401-404) 在扣到 0 时立即 DestroyItem，加载时也会跳过零堆，Bag 层不会再出现「留格等回填」的空堆。上面这些注释会误导后来的人，以为空堆仍是正常状态，甚至据此设计回填逻辑。另外，AddItems 已经改成直接铸号（683-692），全仓生产代码不再调用 CanMintGuid(s)（只剩定义和测试），item_store.h:177-183 与 .cpp:471-474 却仍写着「必须在改动任何状态之前用它」，实际没有入口在用。

- 建议修法：把这几处改为：「实例层本身不销毁；Bag 在扣到 0 时立即成对释放，空堆只可能是 MergePartialStacks 的瞬时产物或测试直接构造的数据」。CanStack/MeasureFreeRoomPerConfig 里「允许往空堆回填」改为「理论上的兼容分支」。CanMintGuids 二选一：删除（YAGNI），或在 AddItems 铸号循环前调用 `CanMintGuids(newInstanceCount - preassigned.size())`，避免已知不够时白白烧掉号段。

#### W1-10 [low] [readability] 置信度 high：调用方注释与防御分支仍按旧的「入包失败可能已改包 / 空堆留格」写，与新的原子契约矛盾

- 位置：`cpp/libs/services/scene/battle/system/player_battle.cpp:815`
- 证据：

```
player_battle.cpp:791-793 // 抽空的堆会留下 size==0 的实例继续占格(实例层的刻意行为),不回收的话 主背包会被僵尸堆占满
player_battle.cpp:815-817 // 入包前抓一份各 config 的持有量:BagService::AddItems 返回失败**不代表包没变** (bag_service.h 的原子性口径明写:临时格可能已经淘汰了旧物)
player_battle.cpp:851 result = kSuccess;  // 其实全进去了,上面的失败码来自已被淘汰的腾位动作
asset_op_system.cpp:558-564 else { // 已经写进去一部分才失败:不能重投(会重复发放),只能记部分发放。
player_equip.cpp:440-441 // 按 config 扣过数量之后回收被抽空的堆:size == 0 的实例仍占一格,还会以 count = 0 出现在快照里
player_equip.cpp:453 // 没到堆叠上限(被抽空还没回收的空堆也算有余量)
```

- 推理：bag_service.h:51-55 现在写的是「任何拒绝都不修改原包…mutated…失败恒为 false」，Bag 扣空也已当场释放。player_battle 815-817 却声称 bag_service.h「明写」了相反的口径，beforeCounts 补差和 851 分支已不可达。asset_op_system 558-564 的「部分写入」分支，在 ItemCountMap 入口下也已不可达（mutated 只在成功后置 true）。player_equip 的 ReclaimEmptyStacks、HasStackWithRoom 注释同样过时。这些都是本批契约变化的连带面，没有同步。

- 建议修法：player_battle：删掉 beforeCounts 补差和 849-852 分支，主包失败就直接把整批 gained 投给临时格；注释改为引用新的原子契约；791-793 改为「扣空已当场释放，这里只合并零头」。asset_op_system：558-564 改成 LOG_ERROR 加按内部错误处理的不可达兜底，或者删除并注明 mutated 只在成功时为真。player_equip：440-442、453、1452 改为当前语义（扣空即释放；任何成功入包都会让借用失效）。

#### W1-11 [low] [incomplete] 置信度 high：C004 只删了 C++ 类型，单人物品封禁的 proto 消息和设计文档仍宣称该功能存在

- 位置：`proto/common/component/item_comp.proto:6`
- 证据：

```
proto/common/component/item_comp.proto:4-7
// Player-level item acquisition block list.
// Persisted per player — survives logout/login.
// Runtime check: Bag::IsItemBlocked() reads from this.
message PlayerItemBlockComp
docs/design/exploit_loss_prevention.md:169  8. **Item block persistence** — per-player blocked item IDs persisted via `PlayerItemBlockComp` proto, survives logout
docs/design/bag-service-srp-refactor.md:24  ### PlayerItemBlockList (data) — per-player GM block state:
```

- 推理：负责人选的是「删除，沿用全服封禁，不新增单人 GM 或持久化契约」。cpp 侧 PlayerItemBlockList 已经零引用，但 PlayerItemBlockComp 仍留在 proto 源和各生成树（go/proto、generated/proto/*）里，注释引用的 Bag::IsItemBlocked 并不存在，全仓也没有任何地方 import 这个 proto。exploit_loss_prevention*.md（三语）、bag-service-srp-refactor*.md（三语）、guild-phase2/04-asset-channel.md:483 仍把单人封禁和 blockList 参数当作现状描述。照文档排障或接功能的人会以为「封了的玩家下线后仍生效」。

- 建议修法：proto：开发期未上线时，按 AGENTS §4 删除消息并重生；若已视为上线，就加弃用注释，并改掉「Bag::IsItemBlocked reads from this」这句错误说明（Java 版同批确认）。文档：在 exploit_loss_prevention*.md 第 28、169 行和 bag-service-srp-refactor*.md 第 24-33 行标注「2026-10-15 C004 已删除，仅全服 GainBlockService」；guild-phase2/04-asset-channel.md:483 的签名去掉 blockList。

#### W1-12 [low] [incomplete] 置信度 high：任务单与挑战要求的背包设计文档更新大半未做（§8 行为差异、回滚分析、策略分层文档仍是旧现状）

- 位置：`docs/design/bag-instance-layout-split.md:293`
- 证据：

```
bag-instance-layout-split.md:293-304 「## 8. 行为差异清单」仍只有 6 行拆分期差异，无本批客户端可见变化
bag-rollback-feasibility-analysis.md:121 bag 的内存数据(`Bag` 类、`ItemsMap items_`、`itemRegistry_`)
bag-rollback-feasibility-analysis.md:144  entt::registry itemRegistry_{};     // 独立 ECS registry,每个 item 是一个 entity
bag-rule-policy-layering.md:432 不重算就会拿着悬空的 entity 去 `ApplyStackFill`。
bag-rule-policy-layering.md:555 全部纯预检并进 `ReserveForBatchAdd`(两个重载)…fence 下**连腾位也不做**
```

- 推理：batch3 §4.7 要求新增「§14 实例层去 entt 与选择顺序」规则表，挑战意见（low）要求在 §8 登记灌哪堆、扣哪堆、留哪堆、批量占格、整理平局等客户端可见变化，并在 rollback 分析文档顶部注明 itemRegistry_ 已过时。实际只改了 §3.1/§3.3/§4/§6/§11.2。§14 和 §8 都没有，「扣空即释放、零堆跳过、候选提交、发号不足不淘汰的旧闸门已取消」等行为变化只写在 handoff 和 PROGRESS 里。bag-rule-policy-layering.md 把 ReserveForBatchAdd 和「fence 下连腾位也不做」写成现行做法，已与代码相反。

- 建议修法：在 bag-instance-layout-split.md 的 §8 追加本批行为差异行（或者新增 §14 并在 §8 里指过去）：实例序定义、并堆/合并/回收/淘汰/整理平局按实例序、扣除按槽位号升序、批量按 config 升序、扣空即释放、加载跳过零堆并补零章、候选提交原子性、写盘改为按 bag/slot 排序。bag-rollback-feasibility-analysis.md 顶部加过时说明。bag-rule-policy-layering.md 的 432 行和 555/562 行注明「2026-10-15 起由 Bag::AddItems 候选提交取代」。

#### W1-13 [low] [readability] 置信度 medium：旧档补章靠调用方「先 Raise 再 Insert」两步协议，规则泄漏到 marshal 且写了两遍

- 位置：`cpp/libs/services/scene/player/system/bag_marshal.cpp:204`
- 证据：

```
bag_marshal.cpp:204-211
    for (const auto &entry : in.items())
        if (entry.bag_type() < … && entry.stack_size() > 0)
            bags.bags[entry.bag_type()].RaiseAcquireSeqForRestore(entry.acquire_seq());
bag_marshal.cpp:249-255 (动态包同一段再写一遍)
bag_system.h:505-506 // acquireSeq:快照里的入包序号。0 = 旧存档没盖过章,由 ItemStore 按重放顺序 重新盖
```

- 推理：「零章要排在全包最大既有序号之后」是实例层的不变量，现在由 bag_marshal 在两个循环里各写一遍，Bag 只暴露 RaiseAcquireSeqForRestore。位置参数版 InsertItemForRestore 的注释也没写这个前置条件。以后任何新还原入口（GM 工具、回滚）只要忘了先 Raise，就会静默盖出重复序号，FIFO 和并堆幸存者都会随之漂移，而且没有报错。这违背 §11.1「信息隐藏/接口是完整契约」，挑战意见的 (b) 方案本来就是在 Bag/ItemStore 里收口。

- 建议修法：在 Bag 上提供 `FinishRestore()`：InsertItemForRestore 对零章条目先保留 0 并按插入顺序记下，FinishRestore 扫描最大非零序号后，把零章条目依次补盖到其后，并把水位抬高。bag_marshal 两处只需在重放后调用一次 FinishRestore，删掉 RaiseAcquireSeqForRestore 这个公开接口。至少也要在 bag_system.h:505 的注释里写明「调用前必须先对全包调用 RaiseAcquireSeqForRestore」，并把 marshal 的两段抽成一个函数。

#### W1-14 [low] [readability] 置信度 medium：AddItems 一个函数串起 9 步，内部不变量失败一律报 kInvalidTableData，掩盖真实原因

- 位置：`cpp/libs/modules/bag/bag_system.cpp:678`
- 证据：

```
676-679  if (newInstanceCount < preassigned.size()) { return PrintStackAndReturnError(kInvalidTableData); }
700-703  if (item == nullptr) { return PrintStackAndReturnError(kInvalidTableData); }
725-730  …"reserved guid count disagrees with plan"… return PrintStackAndReturnError(kInvalidTableData);
747-750  if (nextMint != minted.size()) { return PrintStackAndReturnError(kInvalidTableData); }
754-759  layout cannot create a transaction candidate … kInvalidTableData
```

- 推理：Bag::AddItems(vector) 第 621-784 行一口气完成校验、汇总、预设 guid 去重、淘汰不动点、铸号、候选仓库合堆与初始化、候选布局试放、发布，共 160 多行，并且混用 tab 和 4 空格缩进（同文件 RemoveItemByPos 400-404、PlanEviction 903-923、InsertItemForRestore 1367-1371 也是同一函数内混排）。四五处「计划与提交不一致」之类的编程错误都映射成表数据错误 kInvalidTableData：asset_op 会按这个码回 RETRY，排障时会被引去查配表。这与负责人关心的可读性正相反。

- 建议修法：拆成三个私有步骤：ValidateAndAggregate（返回 stackUnits、separateInstances、preassigned）、ReserveIds（铸号）、BuildCandidate（候选仓库与布局）；AddItems 只负责编排。内部不变量失败统一 LOG_ERROR，Debug 下 assert，并返回一个能体现「内部错误」的现有码，不要复用 kInvalidTableData。缩进按文件既有风格统一。

#### W1-15 [low] [test_invalid] 置信度 medium：BagJavaParityTest 的 TearDown 把号段恢复到非基线区间，与同工程其他夹具不一致

- 位置：`cpp/tests/bag_test/bag_java_parity_test.cpp:124`
- 证据：

```
bag_java_parity_test.cpp:124  EXPECT_TRUE(ArmParitySegment(900000, 1000000));
bag_selection_order_test.cpp:123  EXPECT_TRUE(ArmItemSegment(1, uint64_t{1} << 54));
bag_test.cpp:2841-2842 kBaselineLo = 1; kBaselineHi = uint64_t{1} << 54;
```

- 推理：同工程其余夹具（bag_instance_data、player_equip、player_feature_handler、selection_order）都在 TearDown 把 item 号段恢复为进程基线 [1, 2^54)。本文件却恢复成只有 10 万个号的 [900000, 1000000)。按链接顺序，它之后的 player_battle_settlement_test、player_mission_system_test、player_feature_persistence_test 等依赖基线铸号，于是这些用例的发号区间取决于执行顺序。一旦有用例在该区间内预设 guid（如 bag_test 用过 900001/900002、92xxxx-94xxxx 这类号），就可能撞号，违反「不依赖执行顺序」的测试纪律。

- 建议修法：TearDown 改为恢复基线：`ArmParitySegment(1, uint64_t{1} << 54)`。或者把四份重复的 ArmItemSegment 和基线常量提到一个测试共用头文件里。

<details><summary>该审查者的核对范围（coverage）</summary>

实际核对（只读：git diff 6281e4d27、git show 基线、Read/Grep；未写任何文件）：
- cpp/libs/modules/bag/item_store.{h,cpp} 全文：单一 Guid→ItemComp 字典；Find/InAcquireOrder 断言 key==item_id；Insert 查重先于盖章（不抬水位）；AcquiredBefore 为 uint64 无符号比较；PlanStackIntoExistingStacks 按实例序；ApplyStackFill 找不到 guid 时 LOG_ERROR 并跳过；DrainStacks 每个候选只算一次 rank、零堆不进回执；MergePartialStacks/IsMergeCandidate 排除空堆；CollectEmptyInstances 按实例序。未发现语义错误，只有注释过时（finding 5）。
- bag_system.{h,cpp} 全文：SortedByConfigId 已用于 PlanInstances/AddItems(map)/RemoveItems/RemoveItemsClamped/BagService::AddItems；DesiredOrder 平局键为实例序；IsOrderedByConfigThenSize 早退不变；DrainOneConfig 用槽位 rank 并扣空即成对释放；RemoveItemByPos 扣空即释放；AddItems 候选事务：逐项推演了「聚合计划 vs 逐 entry 贪心」的实例数一致性（含同 config 多 entry、has_equip 分组、预设 guid、脏数据超满堆、淘汰排除集）、淘汰不动点、铸号先于任何修改、失败时 store_/layout_/evicted/written 均不变、成功后发布。确认成立，问题只在 Clone 切片隐患、复制成本、可读性（finding 3/4/10）。InsertItemForRestore 零堆跳过；PutInstance/TakeInstance 无回归。
- eviction_policy：显式使用 AcquiredBefore（挑战 medium 已落实）。container_layout.h：布局层不含 config_id/ItemComp/ItemTable/entt（grep 只命中说明注释），三种布局的 Clone 均为值拷贝，无内部指针。
- bag_service.{h,cpp}：C004 参数在 cpp 全仓零引用；AddItems 只在成功后记淘汰/获得流水，mutated 只在成功后为真；Remove* 流水与扣空释放一致。gain_block_service.h 注释已更新。
- 调用方：player_equip（Enhance/Inlay/RemoveGem 均按 guid 重取，Equip/Unequip 不经 AddItem）、player_mission ClaimReward、asset_op_system ApplyCredit、player_battle 结算扣除/掉落、bag_marshal（扫描后先 Raise 再 Insert、零堆跳过、按包/槽位排序写出、动态包按 bag_id）、player_feature_snapshot（自行排序）。生产代码中没有跨成功入包持有 ItemComp* 或 Layout& 的情况。set_size 外部写入点为 0，Bag 层零堆已不可达。
- 测试：bag_selection_order_test 全文（5 例与新语义冲突，finding 1）、bag_java_parity_test 全文（11 例逐一推演可过；TearDown 基线不一致，finding 11）、bag_test.cpp 改动段及 BagSegmentNotReady/LyingLayout/Eviction 相关旧用例（4080-4096 冲突，finding 2）、bag_remove_exact_test 与 bag_instance_data_test 改动段（已同步）。player_equip_test 抽查空堆相关用例，已同步。
- Java 对照：D:/luyuan/xuanming-server-mmo game-scene Bag.java（add/plan/fits/drainClamped/mergeAndCompact/partialStacks/isCompactAndOrdered/raiseAcquireSeq）与 BagItem.ACQUIRE_ORDER。并堆目标、淘汰 FIFO 前缀、新实例按 config 无符号升序首次适配占格、扣除按格子号升序且扣空即回收、合并幸存者与回收序、整理平局键、零章补盖，与 C++ 一致。未发现客户端可见差异。
- 文档：README §2/§7/§8、batch3-bag.md 全文（含挑战 §8 与回执 §9/§10）、bag-instance-layout-split.md 改动、ecs-component-access-rules.md 中的背包条目、bag-rule-policy-layering/rollback 分析的过时处（finding 8），以及 C004 残留 proto/文档（finding 7）。
- 未覆盖或刻意不审：player_equip.cpp 中把裸指针改为 optional<reference_wrapper> 的改动（归属其他会话的指针清理，不属 ECS），player_battle/asset_op 中非背包部分，Java PARITY.md 逐行内容；编译可行性按要求不判断。

</details>

### A.b4_kafka 第 4 批：Kafka 入口白名单与旧事件清理

#### W1-16 [medium] [ecs_fit] 置信度 medium：Kafka 必需命令仍经 entt dispatcher 路由，kDispatched 不代表已处理，无监听者时静默吞掉

- 位置：`tools/proto_generator/protogen/internal/generator/cpp/service_register_info.go:375`
- 证据：

```
service_register_info.go:375-376
			"\ttlsEcs.dispatcher.trigger(event);",
			"\treturn KafkaEventDispatchResult::kDispatched;",
生成结果 cpp/generated/rpc/service_metadata/rpc_event_registry.cpp:1844-1845（BattleConfirmed/BattleSettlement 等 13 个 case 同形）
		tlsEcs.dispatcher.trigger(event);
		return KafkaEventDispatchResult::kDispatched;
信封回退 template/gate_kafka_command_router.cpp.tmpl:43 / 生成 gate_kafka_command_router.cpp:41-91 同样只 trigger
```

- 推理：白名单里的 13 个事件都是跨进程**必需命令**，而且都只有一个消费者：结算入账 ApplySettlement（玩家资产）、绑定会话、踢线、路由进场、租约到期清理。按 ecs-fit-overview.md 第 (6) 条，以及本次审查采用的判据「dispatcher 只做可有可无的一对多通知，必需步骤直接调用」，这类命令不该经 dispatcher 转发。EnTT 的 dispatcher::trigger 遇到没有监听者的 sink 时什么也不做，而生成代码照样返回 kDispatched，路由层因此不打任何日志。只要某个节点、测试夹具或以后的 UnRegister 时序漏装 handler，结算或绑定就会被静默丢掉，返回值却报告成功。白名单这一批只把派发表按入口拆成两份，没有改变「命令走通知总线」这个结构问题。目前生产启动时 RegisterNodeEvents 会先于 Kafka 注册执行，所以这是结构性隐患，不是已经发生的丢失。

- 建议修法：首选：生成器给每个入口事件生成一个直接调用的处理接缝，例如在生成头里声明 `void HandleSceneCommand(const BattleSettlementEvent&)`，由节点实现。DispatchSceneCommandEvent / DispatchGateCommandEvent 解码成功后直接调用它，链接期就能保证有实现；gate 信封回退同样改为直接调用。最小改法：保留 trigger，但在 trigger 之前判断 `tlsEcs.dispatcher.sink<T>().empty()`；为空时返回新增的 `kNoHandler`，路由层打 LOG_ERROR（带 topic 和 event_id）。同时补一条测试：不注册 handler 时调用派发函数，必须返回非 kDispatched。

#### W1-17 [medium] [test_invalid] 置信度 high：白名单单测把上界写死为 49，漏测已进白名单的 49 号 PlayerInboxNotifyEvent 和 [49,kMaxEventCount) 区间

- 位置：`cpp/tests/kafka_command_test/kafka_command_whitelist_test.cpp:25`
- 证据：

```
kafka_command_whitelist_test.cpp:13
constexpr std::array<uint32_t, 3> kSceneEvents{42, 45, 48};
:25  for (uint32_t eventId = 0; eventId < 49; ++eventId)
:32  for (uint32_t eventId = 0; eventId < 49; ++eventId)
生成头 kafka_command_event_whitelist.h:11  case 49u: // PlayerInboxNotifyEvent
rpc_event_registry.h: constexpr uint32_t kMaxEventCount = 50;
```

- 推理：测试名是「SceneAcceptsOnlyItsPublishedIngressEvents」，注释写的是「固定已发布事件号」，本意是把白名单逐号钉死。但 event_id.txt 已新增 49=PlayerInboxNotifyEvent，生成的 IsSceneCommandEvent(49) 为 true。测试循环只跑到 48，也不认识 49，所以照样是绿的：它既没有确认收件箱提醒确实放行，也拦不住以后误把新号（≥49）标进白名单。任务单 4.5 要求的「≥kMaxEventCount 的号」只用 UINT32_MAX 测了一个点。这条测试和当前生成物不一致，换一个上界就会红。

- 建议修法：include `rpc/service_metadata/rpc_event_registry.h` 取 kMaxEventCount（只用其中的 constexpr），两个循环都改为 `eventId <= kMaxEventCount`，并显式断言 `IsSceneCommandEvent(kMaxEventCount)` 与 `IsGateCommandEvent(kMaxEventCount)` 为 false。kSceneEvents 改为 `{42, 45, 48, 49}`，长度改为 4，注释写明 49 是 player_inbox 的在线提醒。

#### W1-18 [medium] [design] 置信度 medium：一次性迁移逻辑被固化为生成器每轮必跑的步骤：正则改写三份手工维护的工程清单，并硬编码 event_ingress_option 和 attribute_sync_option 锚点

- 位置：`tools/proto_generator/protogen/internal/prototools/local_event_cleanup.go:157`
- 证据：

```
local_event_cleanup.go:84  _, optionErr := os.Stat(filepath.Join(protoRoot, "common", "options", "event_ingress_option.proto"))
:157 	if addIngressOption {
:177 			anchors := entryPattern("attribute_sync_option").FindAllString(updated, -1)
:178 			if len(anchors) != 1 {
:179 				return "", fmt.Errorf("%s 需要唯一 attribute_sync_option.pb.%s 登记以推导事件入口 option", ...)
:189 		if len(optionRefs) != len(kinds) {
:190 			return "", fmt.Errorf("%s 的事件入口 option 登记不完整或含未识别条目", path)
cmd/pipeline.go:46 runner.RunTaskWithWG("prototools.PruneOrphanedLocalEventOutputs", ...)
```

- 推理：全仓生成器原本不维护 cpp/generated/proto 的 CMakeLists / vcxproj / filters：新 pb 一律手工登记，AGENTS §4 对表的 pb 也是这样要求的，本轮 player_inbox_event.pb.cc 同样是手工加的。本文件为一次性删除 11 个 proto，新造了第二套局部清单维护机制：只管 common/event 和一个写死名字的 option 文件，用正则增删 XML，并以另一个 option 文件的登记行为锚点复制出新行。迁移完成后，它仍作为 pipeline 的固定步骤每轮执行。另外：(1) event_ingress_option 的特判以后不会再有用，却永久留在通用清理函数里；(2) 引入了新的 Fatal 条件，例如有人在 CMakeLists 里给 option 加一行 .pb.h，或调整 XML 写法，生成就会中止；(3) 清单的事实源从此变成两套（手工 + 本函数的正则）。挑战意见 §8 的低级别项只建议补删 staging 副本，或让生成器在复制前清空目标目录。这里的实现违背 KISS/YAGNI 和「与仓库一致」。

- 建议修法：把迁移和常驻规则分开。删除 addIngressOption 整段（:84-87、:157-191）以及 syncLocalEventProject 的 addIngressOption 参数：event_ingress_option 的清单行已经落盘，由人手维护即可。若要保留常驻清理，只保留「源 proto 不存在时删除 common/event 的生成物和 staging 副本」，并在清单仍引用已删产物时 Fatal 报出路径，让人手修清单，不要正则改写 XML。对应测试同步删减。

#### W1-19 [low] [incomplete] 置信度 high：任务单要求的 DispatchProtoEvent 零命中门禁未达成；已删 handler 仍被文档引用

- 位置：`cpp/nodes/scene/handler/event/battle_event_handler.cpp:21`
- 证据：

```
battle_event_handler.cpp:21  // battle --Kafka SceneCommand{DispatchEvent}--> DispatchProtoEvent 进程内分发到此(loop 线程)。
team_event_handler.cpp:19  // team 服务(match 进程)--Kafka SceneCommand{DispatchEvent}--> DispatchProtoEvent 进程内分发到此
cpp/libs/services/scene/player/system/player_team.cpp:38  // 由 rpc_event_registry.cpp 的 DispatchProtoEvent 解包后进程内分发到 TeamEventHandler。
go/login/internal/svc/gate_command.go:106  // (gate_kafka_command_router.cpp)对非空 payload 走 DispatchProtoEvent,
tools/proto_generator/protogen/internal/generator/cpp/event_id.go:455  // ...落到 DispatchProtoEvent 的 default 分支被丢弃。
cpp/AGENTS.md:69  Scene room counting hooks `SceneEventHandler::OnSceneCreatedHandler` / `OnSceneDestroyedHandler` only.
```

- 推理：任务单 4.4 第 6 条和 §6 的机械门禁要求 `DispatchProtoEvent` 在 cpp、tools、go 中命中为零，注释也算在内，实际还剩上面 5 处，另有 docs/design/team-system.md:1603、:1932 和 docs/design/scene-enter-session-fence.md:126。cpp/AGENTS.md:69 仍要求「只在 SceneEventHandler::OnSceneCreatedHandler 计数」，可这个类已随 22 个旧 handler 一起删除，接线已移到 scene_local_events.cpp:127/:174。后来的人照着这份规范会找不到入口。运行期不受影响。

- 建议修法：把上述注释改成新函数名：scene 侧写 DispatchSceneCommandEvent，gate 侧写 DispatchGateCommandEvent。event_id.go:455 改为「落到 Dispatch*CommandEvent 的白名单拒绝」。cpp/AGENTS.md:69 改为指向 `scene_local_events.cpp` 的 OnSceneCreatedHandler / OnSceneDestroyedHandler。两份设计文档的引用同步改掉。

#### W1-20 [low] [incomplete] 置信度 high：节点模板仍只声明 KafkaCommandType，照抄会撞上新加的 static_assert

- 位置：`cpp/nodes/_template/main.simple.cpp.example:15`
- 证据：

```
main.simple.cpp.example:14-16
struct YourNodeHooks {
    using KafkaCommandType = contracts::kafka::YourNodeCommand;
};
main.with_context.cpp.example:19-21 同形
node_entry.h:87-89 static_assert(std::is_convertible_v<decltype(&THooks::DispatchKafkaCommand), ...>, "Kafka 命令必须显式指定节点白名单派发函数");
```

- 推理：本批删除了默认全量派发，并要求 hook 显式提供 DispatchKafkaCommand（node_entry.h 的注释已经更新），但两份官方节点模板没有同步。按模板新建的节点会直接编译失败，也看不出该接哪个白名单派发函数。

- 建议修法：两份 example 的 YourNodeHooks 里都补上 `static void DispatchKafkaCommand(const std::string& topic, const KafkaCommandType& command) { DispatchYourNodeKafkaCommand(topic, command); }`，再加一行注释：派发函数必须调用生成的白名单派发（Dispatch*CommandEvent），不得直接 trigger。

#### W1-21 [low] [doc] 置信度 high：白名单规则文档与现状不符；事故文档 F001 未按任务单改为指向新方案

- 位置：`docs/design/control-plane-topic-partitioning-20260908.md:312`
- 证据：

```
control-plane-topic-partitioning-20260908.md:312  当前开放 scene 的结算、结算确认、队伍刷新 3 个事件，以及 gate 的...9 个事件。
:314  本条为源码设计记录；用户要求暂缓生成器构建、重生成、编译与测试，生成产物和运行验收尚待执行。
docs/ops/incident-scene-ecs-gameplay-20261014.md:37  | 12 | F001 / F002 | ... | **未处理，信任边界未收口** |
:130  - [ ] 完成 F001 / F002 的合法生产者清单、命令白名单及非法事件拒绝测试
```

- 推理：生成树里 scene 白名单已是 4 个事件（新增 49 号 PlayerInboxNotifyEvent），生成产物也已在 0f6690604 及工作区重生成，文档仍写 3 个、仍写「尚待重生成」。任务单 4.5 第 2 条要求把事故文档里 F001 的待办改为指向本方案，事故文档未改，两份文档对同一入口的状态描述互相矛盾。

- 建议修法：control-plane 文档 :312 改为 scene 4 个事件（结算、结算确认、队伍刷新、收件箱提醒），:314 改为「生成产物已落盘，编译和运行验收待执行」。事故文档 :37、:83、:130 的 F001/F002 改为「源码已按 OptionKafkaIngress 白名单收口（见 control-plane 文档 §9），运行验收待做」，不要直接标关闭。

<details><summary>该审查者的核对范围（coverage）</summary>

实际核对（按 git diff 6281e4d27 看改动，再读最终代码）：
- proto：event_ingress_option.proto（bool、700001，与 700000 及 cfg_options 60001/60002、etcd 50000/50001 无冲突）；battle_event、team_event、gate_event 的 12 处标注，外加其他会话新增的 player_inbox_event（49，已标注）；PlayerDisconnectedEvent、PlayerLifecycleCommand、match_event、player_migration_event 均未标注，符合盘点结论。
- event_id.txt：与基线逐行比对，29 个活号只转为 `reserved:`，没有任何活号改号，只追加了 49 号。12 份 go/*/generated/pb/game/event_id.go 内容一致；C++ 三个事件号头文件的数值与 txt 一致；player_locator 写死的 34 仍是 PlayerLeaseExpired。
- 生成器：event_ingress.go（目录判定、entity 字段守卫、FdSet 查描述符失败即 Fatal）、event_id.go 的接入、service_register_info.go 拆成两份 case、模板 rpc_event_registry.cpp.tmpl / service_header.tmpl / gate_kafka_command_router.cpp.tmpl、gate_kafka_command_router.go（缺契约时 Fatal、零白名单也重写输出）、local_event_cleanup.go 的预检、删除顺序与幂等、pipeline 的执行顺序（清理排在 InitEventId 墓碑门禁之后），以及三份 Go 测试。生成逻辑本身无错：两个 Dispatch 函数先过同一份 constexpr 白名单，gate 的信封回退也先过 IsGateCommandEvent，空白名单时同样重写拒绝路由。
- 生成产物（已重生成）：kafka_command_event_whitelist.h（scene 4 个、gate 9 个）、rpc_event_registry.cpp 的两个 switch、gate_kafka_command_router.cpp（9 个信封 case，不含 PlayerDisconnected）、proto 工程三清单，以及 cpp、go、staging 下的旧事件产物（已清干净，git ls-files 无残留）。
- 节点接线：node_kafka_command_handler.h（默认全量派发已删，新旧 topic 共用 makeCallback，ValidateCommandTarget 仍在派发之前）、node_entry.h 的 static_assert，scene/gate main.cpp 的 hook（gate 原来的 SetKafkaHandlers 覆盖已删；RunSimpleNodeMainWithOwnedContext 先执行 ApplyPostConstructionHooks，再执行 configure）、scene_kafka_command_router.cpp。
- 生产者全仓盘点，无遗漏：
  - Go：login 的 Bind/Kick/PushTip；scene_manager 的 Route/Redirect；player_locator 的 LeaseExpired（34）；friend、guild、match 三份 gate_command_builder 经 kafkautil 发 Push/BroadcastToPlayers/Scene/All；match/team 的 PlayerTeamRefresh；player_inbox 的 Go 服务尚未实现。
  - C++：battle_room_manager 的 BattleSettlement、BattleConfirmed（SceneCommand）和 PushToPlayer（GateCommand）；scene 与 gate 不往对方发命令。
  - 其他：Java 仓 D:/luyuan/xuanming-server-mmo 没有 GateCommand/SceneCommand 生产者（battle 结果走 match-results topic，battle→scene 走 Dubbo）；本仓 java/、robot/、tools/、deploy/ 均无生产者。
- 删除的 11 个 proto 和 22 个 handler：Go、Java（只在 MissionFact 注释里提到）、message_id.txt、proto 的 import、vcxproj/CMake、robot 均已无引用。Java 的 ContractSync 会同步 options 目录，event_id.txt 只做复制、不解析。PARITY 第 177 行已登记。
- 确认无问题：墓碑与活号；白名单与派发 case 的一致性；gate 的目标过滤顺序；空载荷时 scene 拒绝、gate 走信封回退（现有生产者的载荷都非空，或本来就走信封）；AGENTS §4 的编号规则。
- 未展开：4b/4c/4d 的其余实现（scene_local_events 的搬运、ActorNetIds 等）、player_inbox 的 handler 正文（其他会话，进行中）、不属本批的 kafka_partition_contract_test / kafka_manager_init_test。

</details>

### A.b4_ids 第 4 批：进程内事件结构体化、节点句柄分型、ActorNetIds

#### W1-22 [medium] [ecs_fit] 置信度 high：沉默状态、升级重算这类必需步骤仍经 dispatcher 一对一转发，没有订阅就静默失效

- 位置：`cpp/libs/services/scene/combat/buff/system/buff_impl.cpp:85`
- 证据：

```
buff_impl.cpp:85-91
void BuffImplSystem::AddSilenceState(entt::entity parent, const BuffEntry& buff) {
    CombatStateAddedEvent event;
    event.actor = parent; ...
    tlsEcs.dispatcher.trigger(event);
(RemoveSilenceState :94-100 同形)
scene_local_events.cpp:139-142
void CombatStateAddedEventHandler(const CombatStateAddedEvent& event)
{
    CombatStateSystem::AddCombatState(event);
}
player_attribute.cpp:1142-1146
// 走升级事件(...),事件处理里会 Recalculate + PushPanel
PlayerUpgradeEvent upgrade; ... tlsEcs.dispatcher.trigger(upgrade);
scene_local_events.cpp:72-78 PlayerAttributeSystem::Recalculate / PushPanel / PetSystem::RecalculateAll / PushList
```

- 推理：判据是：dispatcher 只承担可有可无的一对多通知，必需步骤直接调用。这两处都是唯一消费者的必需步骤。(1) 沉默 Buff 必须把 kActorCombatStateSilence 写进 CombatStateCollectionComp，技能前置检查 CheckBuff 和 ValidateSkillUsage 都读这个组件。现在 buff_impl 与 combat_state 同属 services/scene 库，没有分层上的理由走 dispatcher，handler 只是原样转调 CombatStateSystem。(2) GmSetLevel 改了持久的 LevelComp，却把派生属性重算（DerivedAttributesComp 属于派生运行时数据）、面板推送和宠物重算都交给 nodes/scene 组合根里的 handler，等于系统发事件去调用它自己。生产里只有 scene/main.cpp:100 的 RegisterAll 装了这些订阅。任何没装订阅的宿主（buff_test、skill_test、bag_test 都只装 RegisterMission 或什么都不装）里，沉默 Buff 不会生效，GM 调等级后派生属性保持旧值，都没有报错。仓库里也没有任何测试覆盖沉默生效（tests 中 grep silence 只命中编译宏）。第 5 批已把 actor 派生初始化从 InitializeActorCompsEvent 改成直接调用，这两处属于同一类问题却被遗漏。

- 建议修法：① BuffImplSystem::AddSilenceState/RemoveSilenceState 直接调用 CombatStateSystem::AddCombatState(parent, buffId, kActorCombatStateSilence) / RemoveCombatState(...)（把参数改成显式的 entity/sourceBuffId/stateType）；删除 CombatStateAddedEvent/CombatStateRemovedEvent 两个结构体和 scene_local_events.cpp:139-147、176-177、189-190 的接线。② 新增 PlayerAttributeSystem::OnLevelChanged(player, newLevel)，在其中直接执行 Recalculate + PushPanel + PetSystem::RecalculateAll/PushList，由 GmSetLevel 直接调用；之后只保留真正的一对多通知，即 ConditionEvent(kConditionLevelUp)，并决定 PlayerUpgradeEvent 是否还有存在必要。③ 补两条回归测试，都不调用 RegisterAll：加沉默 Buff 后 CombatStateCollectionComp 含沉默且 CheckSkillPrerequisites 拒绝施法；GmSetLevel 后 DerivedAttributesComp 已按新等级重算。修复前两条都应为红。

#### W1-23 [medium] [readability] 置信度 high：保留了多个生产环境零监听或监听为空操作的事件，并为它们写了大段重入复核

- 位置：`cpp/libs/services/scene/player/system/player_scene.cpp:260`
- 证据：

```
player_scene.cpp:254-268
AoiSystem::BeforeLeaveSceneHandler(leaveEvent);
if (!tlsEcs.actorRegistry.valid(player)) { return false; }
tlsEcs.actorRegistry.remove<SceneEntityComp, Hex>(player);
tlsEcs.dispatcher.trigger(leaveEvent);
if (!ownership.IsCurrent() || CheckEnterScene(player, scene) != kSuccess) { return false; }
if (tlsEcs.actorRegistry.all_of<SceneEntityComp>(player)) { return false; }
player_lifecycle.cpp:4148-4156
InitializeActorCompsEvent initActorEvent; ... trigger(initActorEvent);
if (!canContinueInitialization()) return entt::null;
InitializePlayerCompsEvent initPlayerEvent; ... trigger(initPlayerEvent);
mission.cpp:107-110 OnAcceptedMissionEvent ... trigger(acceptedEvent);
```

- 推理：git grep 显示生产代码里没有 sink<BeforeLeaveScene>、sink<InitializeActorCompsEvent>、sink<OnAcceptedMissionEvent>，只有 cross_zone_test 和 player_feature_persistence_test 订阅它们来模拟重入。RegisterPlayerEvent 与 InitializePlayerCompsEvent 的生产 handler（scene_local_events.cpp:52-63、83-94）只做 valid 检查并打日志；PlayerLoginEvent 的 handler（:96-125）只打日志，switch 分支全是“待接”。按任务单挑战意见里的 YAGNI 口径，Codex 已删除零监听的 ConnectToNodeEvent、OnServerStart、BeKillEvent、InterruptCurrentStateEvent，但这几个事件在第 5 批把正文移成直接调用后同样变成零监听，却被保留了。代价是 HandleEnterScene、DetachFromScene（player_lifecycle.cpp:2899-2914）、InitPlayerFromAllData（:4084-4156 的 canContinueInitialization lambda 及每次 trigger 后的复核）和 EnterScene 的 canContinueEntry 都为“不存在的监听者可能销毁或重建玩家”写了防御分支和专项测试。其中 player_scene.cpp:261-268 的复核一旦为真，会让玩家处于“已从旧场景解绑、未绑定新场景”的状态，而这条路径在生产中不可达，无法验证。这正是负责人说的“滥用 ecs/entt、代码不可读”。

- 建议修法：删除 BeforeLeaveScene、InitializeActorCompsEvent、OnAcceptedMissionEvent、RegisterPlayerEvent、InitializePlayerCompsEvent、PlayerLoginEvent 及其 trigger 点和 scene_local_events 中的空 handler。BeforeLeaveSceneHandler 改为 AoiSystem::OnLeaveScene(entt::entity)，不再借用事件结构体当参数。删除 trigger 之后，同步删掉随之不可达的复核：player_scene.cpp:255-258、261-270，player_lifecycle.cpp:2900-2903、4137-4156 中的 canContinueInitialization 调用，以及只为模拟这些监听者而写的测试订阅（cross_zone_test 中 4316/4343/5542/5554/5576/5647，player_feature_persistence_test 中 845-849/898-902/1074/1103-1107/1170-1172）。以后真的出现可选观察者时再加事件。在 ecs-component-access-rules.md 写明“没有消费者不发事件；dispatcher 只做可有可无的一对多通知”。

#### W1-24 [medium] [contract_compat] 置信度 medium：66 号 ActorBaseAttributesS2C 只有标了 entity_id 脏位时才带主体标识，移动和速度增量的 entity_id 恒为 0，客户端无法归属（存量问题）

- 位置：`cpp/libs/services/scene/generated/attribute/actorbaseattributess2c_attribute_sync.cpp:32`
- 证据：

```
actorbaseattributess2c_attribute_sync.cpp:32-38
if (dirtyMaskComp->dirtyMask.test(ActorBaseAttributesS2C::kEntityIdFieldNumber))
{
    auto* fieldValue = actorRegistry.try_get<PlayerGuidComp>(entity);
    if (fieldValue != nullptr) { syncMsg.set_entity_id(fieldValue->value); ...
movement.cpp:110-111
SetActorBaseAttributesS2CAttrDirtyBit(
    entity, ActorBaseAttributesS2C::kTransformFieldNumber);
actor_attribute_calculator.cpp:105-109 (只有战斗状态投影一并置 entity_id 位)
// entity_id 是这条消息里唯一的归属标识(信封层不带主体 actor)
```

- 推理：这条回答了“两套 id 是否对不上”。玩家这一侧本身对得上：ActorCreateS2C 同时带 guid=PlayerGuidComp 和 entity=net id（view.cpp:93-128），客户端可以建立 guid 到 actor 的映射；改造前也是 GUID 和 to_integral 两套编号，第 4 批没有让这一点变坏。真正的缺口在 66 号消息本身：entity_id 只在 kEntityIdFieldNumber 被置位的那一帧写入，而全仓只有 ResetCombatStateFlags 置这个位。movement.cpp:103/111/197/203、player_movement_handler.cpp:70/134、scene_spawn.cpp:139、player_lifecycle.cpp:2931 只置 transform 或 velocity 位，ActorStateAttributeSyncSystem 每 2 帧把这些增量广播给 AOI 内所有观察者，此时 entity_id 为 0，而 BroadcastMessageToPlayers 的信封里也不带主体。NPC 没有 PlayerGuidComp，任何一帧都拿不到 entity_id。改造前同样如此，不是本批引入。但本批交付说明把“66 号 entity_id 仍为玩家 GUID”当成兼容保证，没有指出它在大多数帧根本不出现。

- 建议修法：在生成模板（不手改生成树）里，只要本轮有任何字段要发，就无条件写入主体标识：玩家写 PlayerGuidComp.value。是否让 NPC 改用 actor net id（ActorNetIds::Of）需要和 Java 版同批定口径，并在 player_state_attribute_sync.proto 的 entity_id 注释中写明“玩家=player GUID，与 ActorCreateS2C.entity 的 net id 不同”。补测试：只置 transform 位时，发出的消息 entity_id 也不为 0。因为客户端可见，需要按 AGENTS §12 两版同批修改。

#### W1-25 [low] [readability] 置信度 medium：ActorNetIds 接缝不够收敛：组件公开可写、Of 在 AOI 热路径上反复做反查自检、调用方和被调方重复判 0 且被调方静默半填消息

- 位置：`cpp/libs/services/scene/actor/actor_net_id.cpp:57`
- 证据：

```
actor_net_id.h:8-11
struct ActorNetIdComp
{
    uint64_t value{0};
};
actor_net_id.cpp:57-61
uint64_t actor_net_id::Of(const entt::registry& actors, entt::entity actor)
{ ... return id != nullptr && Resolve(actors, id->value) == actor ? id->value : 0; }
aoi.cpp:186-190 if (actor_net_id::Of(...otherEntity) == 0) {...continue;} ViewSystem::FillActorCreateMessageInfo(entity, otherEntity, *createMsg.add_actor_list());
view.cpp:93-97 const auto netId = actor_net_id::Of(...); if (netId == 0) { LOG_ERROR ...; return; }
```

- 推理：ActorNetIdComp 定义在公共头里且字段可写，任何人都能 emplace、replace 或改 value 而绕过 Assign。为此 Of 每次都要反查索引来自检，AOI 进出视野每个实体会做两次 Of（aoi.cpp:186 与 view.cpp:93），合计 4 次稀疏集查找和 2 次哈希查找；如果信息真正被隐藏，这些都不需要。FillActorCreateMessageInfo 遇到 0 时直接 return，此时调用方已经 add_actor_list() 了一个元素，消息里会留下 entity=0 的空项。目前靠三处调用方各自预检兜住（aoi.cpp:186、player_scene.cpp:294），契约不显式。另外，反查表放在 registry ctx 并靠 on_destroy 信号维护，与同类反向索引 playerList、sessionPlayers 作为 EcsContext 普通成员的写法不一致，且依赖 World::InitializeSystemBeforeConnect 先调 Install，每个测试夹具都得记得调用。不过它在 EnTT 3.13 语义下是正确的：sigh_mixin::pop/pop_all 在擦除前发布 destroy 信号，clear() 不清 ctx，ctx 晚于各组件池析构。

- 建议修法：把 ActorNetIdComp 移到 actor_net_id.cpp 的匿名命名空间（或至少让它不可直接构造），对外只暴露 Assign/Of/Resolve。之后 Of 改为 valid + try_get 直接返回 value，不再反查。FillActorCreateMessageInfo 改为返回 bool，由它负责分配元素（例如改成 TryAppendActorCreate(observer, entrant, ActorListCreateS2C&)），调用方删掉重复预检。可选：把索引改为 EcsContext 的具名成员，与 playerList 并列，并在文档写明维护方式。

#### W1-26 [low] [incomplete] 置信度 high：客户端 target_id 在一次 ReleaseSkill 中被反查最多 6 次，任务单要求的“入口解析一次后传 entt::entity”只做了一半

- 位置：`cpp/libs/services/scene/combat/skill/system/skill.cpp:110`
- 证据：

```
skill.cpp:110-112
entt::entity ResolveSkillTarget(entt::entity caster, uint64_t actorNetId) {
    return ValidateSkillTargetEntity(caster, actor_net_id::Resolve(tlsEcs.actorRegistry, actorNetId));
}
:507 const entt::entity target = actor_net_id::Resolve(tlsEcs.actorRegistry, request->target_id());
:529 if (ResolveSkillTarget(casterEntity, request->target_id()) == entt::null) {
:204 / :218 / :243 / :633 同一 request->target_id() 再次 ResolveSkillTarget
```

- 推理：任务单 4d-2 第 3 条要求：ResolveSkillTarget 改为接收 entt::entity，客户端路径先 Resolve 一次再校验。实际保留了 uint64 包装，ValidateTarget（:507-521 手写的 valid/Player/Npc 检查与 ValidateSkillTargetEntity 重复，然后在 :529 再解析一次）、LookAtTargetPosition、BroadcastSkillUsedMessage、CreateSkillContext、ApplySkillHitEffectIfValid 各自重新解析同一个不可信输入。这些调用都是同步的，目前结果一致，所以不是正确性问题。但“不可信外部号只在接缝处解析一次”的边界因此不清楚：后续有人在两次解析之间插入可能改动目标的逻辑，就会出现校验和使用不一致。

- 建议修法：在 SkillSystem::ReleaseSkill 入口只调用一次 actor_net_id::Resolve 并做 ValidateSkillTargetEntity，得到 entt::entity target 后作为参数传给 CheckSkillPrerequisites/ValidateTarget、LookAtTargetPosition、BroadcastSkillUsedMessage、CreateSkillContext、ApplySkillHitEffectIfValid；删除 uint64 版 ResolveSkillTarget，并删掉 ValidateTarget 中 :507-521 与 ValidateSkillTargetEntity 重复的检查。

#### W1-27 [low] [readability] 置信度 high：进程内事件结构体里留有无人写、无人读的字段，以及调用空函数的“事件”

- 位置：`cpp/libs/engine/core/node/system/node/node_events.h:10`
- 证据：

```
node_events.h:8-10
    entt::entity node{entt::null};
    uint32_t nodeType{0};
    uint32_t sceneNodeType{0};
node.cpp:1597-1599 只设置 node / nodeType
skill.cpp:932-935
SkillExecutedEvent skillExecutedEvent;
skillExecutedEvent.caster = casterEntity;
skillExecutedEvent.target = entt::entity{skillContext->target()};
BuffSystem::OnSkillExecuted(skillExecutedEvent);
buff.cpp:588-591 void BuffSystem::OnSkillExecuted(SkillExecutedEvent& event) { // TODO }
```

- 推理：OnNodeRemoveEvent.sceneNodeType 全仓没有写入点也没有读取点，是从旧 proto 照搬的死字段。SkillExecutedEvent 的 skillId/skillTableId 在唯一构造点没有赋值，唯一消费者 OnSkillExecuted 是空的 TODO。将来有人接这个函数时会静默读到 0。这与本批“没有生产者或消费者就删（YAGNI）”的口径不一致。

- 建议修法：删除 OnNodeRemoveEvent::sceneNodeType。删除 SkillExecutedEvent、combat_events.h 和 BuffSystem::OnSkillExecuted 及其调用（skill.cpp:932-935）；将来要接技能后 Buff 反应时，再按真实需要定义参数并在构造点填全。

#### W1-28 [low] [readability] 置信度 high：gate 节点句柄类型化后残留旧整数逻辑：冗余别名和不可达的重绑分支，“scene_node=”日志打的是实体整数

- 位置：`cpp/nodes/gate/handler/rpc/client_message_processor.cpp:288`
- 证据：

```
client_message_processor.cpp:288-302
const auto effectiveNodeEntity = GetEffectiveNodeEntity(session, nodeType);
const auto nodeEntity = effectiveNodeEntity;
if (!registry.valid(nodeEntity)) { ... return std::nullopt; }
if (effectiveNodeEntity != session.GetNodeEntity(nodeType))
{
    session.SetNodeEntity(nodeType, effectiveNodeEntity);
}
scene_entry_dispatch.cpp:152
LOG_DEBUG << "ForwardPlayerToScene: sent PlayerEnterGameNode to scene_node=" << entt::to_integral(sceneNodeEntityId)
```

- 推理：GetEffectiveNodeEntity 现在只会返回 stored 句柄或 entt::null，null 已经在前面的 valid 检查里返回，所以 :299-302 的重绑分支不可达；它是旧版“按 node_id 回退猜继任节点”逻辑的残骸，nodeEntity 这个别名也没有意义。scene_entry_dispatch.cpp:93/98 的参数仍叫 sceneNodeEntityId，:146 和 :152 日志用“scene_node=”打印实体整数。这正是 4c 规则（标识节点要打 NodeInfo.node_id）在 client_message_processor 里修掉的同类问题（F084），在这里漏了。session_info_comp.h 的成员仍叫 entityIds，参数仍叫 entityId。

- 建议修法：ResolveSessionTargetNode 改为：auto node = GetEffectiveNodeEntity(...); if (node == entt::null) { LOG_ERROR...; session.SetNodeEntity(nodeType, entt::null); return std::nullopt; } return node; 并删除 :299-302。ForwardPlayerToScene 参数改名为 sceneNode，日志改为打印 try_get<NodeInfo>(sceneNode)->node_id()（取不到记 0）作为 scene_node_id，必要时另加 entity=<64 位>。SessionInfo 成员改名为 nodeEntities。

#### W1-29 [low] [test_invalid] 置信度 high：新增的“同 index 不同 version 判为换节点”用例在修复前就会通过，测不出任何回归

- 位置：`cpp/tests/routing_identity_test/routing_identity_test.cpp:643`
- 证据：

```
routing_identity_test.cpp:643-657
TEST(SceneRouteEntry, RecycledNodeIndexWithAnotherVersionCountsAsChanged)
{ ... session.SetNodeEntity(eNodeType::SceneNodeService, previous);
    nodes.destroy(previous);
    const auto replacement = nodes.create(); ...
    EXPECT_EQ(SceneNodeChange::kChanged, gate_scene_route::RebindSceneNode(session, replacement));
```

- 推理：改造前 SessionInfo 存的是 uint64_t to_integral(entity)，旧注释明确写着“Using uint64_t avoids truncation that loses entity version”。ResolveSceneNodeEntity 也返回完整的 to_integral，CompareSceneNode 按 64 位整数比较，同 index 不同 version 本来就判 kChanged。所以 F033 的改动只是类型安全重构，这条用例不满足 README §3“修复前红、修复后绿”的要求，交付说明也不能把它当作回归证据。

- 建议修法：在交付说明里把这条用例标注为“类型契约固定用例，非回归测试”。真正需要防的是整数和句柄往返时截断或丢 version，可以改为编译期断言：static_assert(std::is_same_v<decltype(SessionInfo{}.GetNodeEntity(0)), entt::entity>)，并 grep 守住不再出现 entt::entity{<整数>}。

#### W1-30 [low] [doc] 置信度 high：事件结构体化后多处注释或文档仍描述旧机制

- 位置：`cpp/libs/modules/mission/system/mission.cpp:345`
- 证据：

```
mission.cpp:345-347
// 必须保留完整 64 位:高 32 位是 version,截成 uint32 后槽位复用过的玩家
// 还原出来的是 version 0 的句柄,handler 里 valid() 判否,事件被静默丢弃。
const auto playerEntityValue = playerEntity;
cpp/AGENTS.md:69 Scene room counting hooks `SceneEventHandler::OnSceneCreatedHandler` ...
scene_handler.cpp:761-762 监听者按「场景已建成」记过账(Agones 计数、AOI 格子表)
scene_node_service.cpp:68-69 对称撤账(Agones 计数、AOI 格子表)
actor_attribute_calculator.cpp:107 // 序列化器从 Guid(uint64_t)组件取值
```

- 推理：字段已经是 entt::entity，不存在截断，mission.cpp 这段注释和多余的 playerEntityValue 别名已经过时。SceneEventHandler 类已删除，接线改在 scene_local_events.cpp。AoiSystem::OnSceneCreatedHandler 已删除，SceneGridListComp 改为在 UpdateGridState 中首次入格时创建，OnSceneCreated 的监听者已经不再建 AOI 格子表。属性同步现在取 PlayerGuidComp，不再是 Guid(uint64_t) 组件。另外 proto/scene/player_state_attribute_sync.proto:19 的 entity_id 注释仍是“Entity unique ID”，而同批把 ActorCreateS2C.entity 注释改成了“不是玩家 GUID 的 net id”，两处对照容易让客户端把两套编号混用。

- 建议修法：删除 mission.cpp:345-346 的注释和 :347 的别名，直接使用 playerEntity。cpp/AGENTS.md:69 改为指向 scene_local_events.cpp 的 OnSceneCreatedHandler/OnSceneDestroyedHandler。两处 CreateScene 回滚注释中删去“AOI 格子表”。actor_attribute_calculator.cpp:107 改为 PlayerGuidComp。player_state_attribute_sync.proto 的 entity_id 注释改为“玩家 player_id（GUID）；与 ActorCreateS2C.entity 的 actor 网络号不同，客户端通过 ActorCreateS2C.guid 关联”。

#### W1-31 [low] [contract_compat] 置信度 low：客户端可见 actor 号的取值区间和重启语义已变，但 F080 的三项客户端确认没有书面记录，与 Java 版语义也不完全一致

- 位置：`cpp/libs/services/scene/actor/actor_net_id.h:18`
- 证据：

```
actor_net_id.h:18-19
// 号源为 node32 + seq32,本进程内不复用;未安装、node 为0或序号耗尽时返回0。
uint64_t Assign(entt::registry& actors, entt::entity actor);
node_allocator.cpp:73-79 for (uint32_t id = 1; id <= kMaxNodeId; ++id) { if (usedIds.find(id) == usedIds.end()) { nextNodeId = id; ...
batch4-external-id.md:629 请确认三件事:客户端是否把 ActorCreateS2C.entity 和 ReleaseSkillRequest.target_id 当作不透明值使用...换场景、换节点时是否清空旧的 actor 表...
```

- 推理：字段号和类型没有变，但取值从“版本为 0 时小于 2^32 的小整数”变成恒大于等于 2^32（node_id 从 1 起）。任务单把这一项列为“需负责人拍板，未拍板前不要实施”，并要求确认客户端是否只把它当不透明值、是否在换节点时清空 actor 表。README §7 和 PROGRESS:8299 只写“选默认兼容 A”，没有记录这三项确认。语义上，node_id 按最小空洞复用，进程重启后 seq 从 1 重新开始，同一 node_id 的新进程会重新发出与上一个进程相同的号。Java 版用 snowflake::nextId（SceneNode.java:351），跨重启也唯一。按 AGENTS §12，两版共享客户端契约，这一差异应当写明。是否真的会出问题取决于客户端，这里没有读取客户端仓库，所以置信度低。

- 建议修法：在 batch4 文档 §7 或 PROGRESS 补记负责人对 F080 三项问题的答复。若无法确认客户端在 EnterSceneS2C 或换节点时清空 actor 表，可把 seq 的起点改为带启动时间或 epoch 的高位（例如把 32 位 seq 拆成“启动代次 + 计数”），使号跨重启也不复用，与 Java 语义一致。同时在 Java 仓库 PARITY.md 和 mmorpg-client-contract-aoi.md 写明两版各自的唯一性范围。

<details><summary>该审查者的核对范围（coverage）</summary>

只读核对，未改任何文件，未跑构建或生成器。基线为 git diff 6281e4d27。

**核对无问题的部分**
1) ActorNetIds（actor/actor_net_id.{h,cpp}、world.cpp:30 的 Install、id_generator.h、node_id_generator.h TryGenerateWithoutReuse）：
- 按 EnTT 3.13.2 源码核对：sigh_mixin::pop/pop_all 在擦除前发布 on_destroy；registry::clear 逐池 pop_all 但不清 ctx；成员析构顺序为先池后 vars。Install 幂等，Assign 幂等，耗尽后保持饱和，node 为 0 时拒绝。
- node32 取自 NodeAllocator 分配的 node_id，从 1 起，按 (type, id) 全局 CAS 分配。它在 SetAfterStart 中同步设置，早于任何 RPC 处理，所以不会为 0；同时在线的节点之间不会撞号。
- 唯一建号点是 player_lifecycle.cpp:4073。失败时先 DestroyEntity 再走 RefuseInheritedEntry，此时 playerList 还没写，不会留下孤儿。生产代码里没有其他 actorRegistry.create。
- 反查表随组件移除、实体销毁、clear 正确清理；同 index 新 version 的替身拿到新号；换场景时号不变。
2) 出站与入站：
- 出站：view.cpp:93、aoi.cpp:186/202/369、movement.cpp:164/214、skill.cpp:627/634/679、player_scene.cpp:294 全部走 Of。grep 所有 set_entity/add_entity/set_target_entity/add_target_entity，没有残留 to_integral 下发。
- 入站：唯一入口是 ReleaseSkillRequest.target_id，在 skill.cpp:111/507 走 Resolve。SkillContext 和 DamageEvent 内部存的仍是本地 to_integral，用 entt::entity{} 还原（706/800/901/934），前后自洽。
- 离场和销毁路径（DestroyPlayer→DetachFromScene、EnterScene 失败的回滚）都在号仍有效时完成 AOI 通知。
- skill_test 的 ValidateTarget_RejectsLocalEntityInteger 和 actor_net_id_test 的 6 个用例在修复前会红。
- robot 只把 entity 当作 uint64 回传。
3) 66 号消息与两套 id：玩家这一侧对得上，ActorCreateS2C 同时带 guid 和 net id，改造前也是两套编号，第 4 批没有让这一点变坏。剩下的缺口是 66 号增量缺 entity_id，见 findings。
4) gate 节点句柄与 lane：
- SessionInfo 改为 entt::entity 后，entt::null 与旧的 UINT64_MAX 哨兵语义等价。
- 已核对 CompareSceneNode、RebindSceneNode、AttemptPendingSceneEntry、ResolveSceneNodeEntity、gate_service_handler:103、HandleConnectionDisconnection（改用 NodeInfo.node_id，sceneNodeRegistry 已改名）、OnNodeRemoveEventHandler 的完整 64 位比较、lane 中 InflightCall 的 null 缺省与 OnRouterNodeRemoved。
5) game_registry 与门禁：game_registry.{h,cpp} 只剩 DestroyEntity。cpp 与 tools 中 entt::to_entity(、EventOwner、CreateEntityIfNotExists、RecreateEntity、entt_traits::to_entity、EntityHash 均为 0。11 个已删除的 common/event 生成头没有任何 include 残留。
6) ActorLog（core/utils/registry/actor_log.h）先判 valid 再 try_get，输出完整 64 位。mission.cpp、player_scene.cpp、player_lifecycle.cpp、bag_marshal.cpp、player_lifecycle_handler.cpp 中把实体冒充 player_id 的日志已清理，没有剩余 uint32 截断。
7) 事件结构体与接线：已核对 node/mission/scene/actor/combat/player 六个事件头、scene_local_events 接线与 main.cpp 的 Register/Unregister、mission.cpp 的触发与收集顺序、MatchesEventSlots 改为 span、ConditionEvent 的多 id 用例。ConnectToNodeEvent、OnServerStart、BeKillEvent、InterruptCurrentStateEvent 已连同触发点删除。client_message_processor.h 的前置声明已改为 struct，aoi.h 中的 AfterEnterScene 已删除，cross_zone_test 已改用 player_events.h。

**未审范围**
4a Kafka 白名单与生成器（另有轨道负责），以及第 5 批的重入复核、Anomaly、InBattle 等内部实现。顺带确认：InitPlayerFromAllData:4122 中 InitializeOnCreate 返回失败的分支在当前实现下不可达，不构成孤儿实体问题。

</details>

### A.b5_life 第 5 批：生命周期（scoped_connection、TimerTaskComp、旁路索引）

#### W1-32 [medium] [ecs_fit] 置信度 high：为没有生产订阅者的生命周期通知保留了大量重入防护，防护分支会制造半成品状态

- 位置：`cpp/libs/services/scene/player/system/player_lifecycle.cpp:4084`
- 证据：

```
scene_local_events.cpp:51-61/84-95 中 RegisterPlayerEventHandler 与 InitializePlayerCompsEventHandler 只做 valid 检查后注释「本事件只作通知」，PlayerLoginEventHandler 只打日志；InitializeActorCompsEvent 与 BeforeLeaveScene 在生产代码中没有任何 sink（grep sink<BeforeLeaveScene|InitializeActorCompsEvent> 只命中 tests）。
player_lifecycle.cpp:4084 const auto canContinueInitialization = [player = player, playerId, &ctx, &notificationOwnership]() {...}
player_lifecycle.cpp:4517-4523 DetachFromScene(playerEntity); if (!valid(playerEntity) || GetPlayer(playerId) != playerEntity) { LOG_INFO << "... stopping old entity teardown"; return; }
player_lifecycle.cpp:3077-3080 DetachFromScene(player); if (... GetPlayer(exitingPlayerId) != player) return;
player_scene.cpp:259-268 remove<SceneEntityComp, Hex>(player); trigger(leaveEvent); if (!ownership.IsCurrent() || CheckEnterScene(...) != kSuccess) return false;
cross_zone_test.cpp:4350 EXPECT_EQ(!destroyOldPlayer, tlsEcs.actorRegistry.valid(oldPlayer));
```

- 推理：必需步骤已改为直接调用（AOI 离场、默认技能、派生投影），剩下的 5 个 trigger 在生产里只有空处理器或根本没人订阅。为了防御「监听者在通知里销毁、替换玩家或改场景绑定」这种假想情况，代码加了 EntryOwnershipGuard（9 字段快照）、canContinueInitialization、canContinueEntry/stopEntry 和多处通知后提前 return，相关代码约 40 处，并配了一批专门安装人造监听者的测试。这违反 ecs-fit-overview 第 (6) 条「dispatcher 只做可有可无的通知」和 AGENTS §11.2 的 KISS/YAGNI。更糟的是，这些兜底分支本身处理得不对，到达后都会留下比不发通知更坏的状态：(a) DestroyDeposedPlayer 和 HandleExitGameNode 在「旧实体仍有效、playerList 已指向替身」时直接 return，旧实体带着 Player/UnregisterPlayer 永久留在 actorRegistry 里，IsEmergencyRelocateDrained 的 view<Player>().size()==0 永远不成立，测试 4350 行还把这个泄漏当成预期结果断言；(b) canContinueInitialization 在「快照变了但实体仍有效且仍被索引」时不擦 playerList，直接返回 entt::null，留下一个半初始化的在线实体，之后同号登录会被「already exists」拒绝；(c) HandleEnterScene 在解绑旧场景之后才复核，失败就 return false，玩家没有任何场景。挑战意见 §8 的 low 项明确要求「通知前取副本、通知后不再 re-fetch、不提前 return，守卫放在任何修改之前」，这一条没有落实。生产环境目前到不了这些分支，所以定为 medium，不定为 high。

- 建议修法：按 ecs-fit 判据收口。删除没有订阅者或处理器为空的事件：InitPlayerFromAllData 里的 RegisterPlayerEvent、InitializeActorCompsEvent、InitializePlayerCompsEvent 三个 trigger，DetachFromScene 和 HandleEnterScene 里的 BeforeLeaveScene trigger，连同 scene_local_events.cpp 里对应的空 handler 与 connect/disconnect 一并删除。同时删除 notificationOwnership、canContinueInitialization（只保留 Unmarshal 之后的 valid 检查）、HandleEnterScene 第 255-271 行的通知后复核与重新 CopyFrom、DestroyDeposedPlayer 4519-4524 行和 HandleExitGameNode 3077-3080 行的提前 return，以及 cross_zone_test 中 ReplacePlayerDuringLeaveNotification 一类的人造重入用例。如果确实要保留某个通知，就在事件结构体注释里写明监听者契约：不得销毁、替换玩家，不得改 SceneEntityComp 或 playerList；trigger 之后只用 assert 校验（Debug 下 valid(player) && GetPlayer(id)==player），不写恢复分支。EntryOwnershipGuard 只保留给 PlayerTeamSystem::OnEnteredScene 这类真实业务调用之后的复核。

#### W1-33 [medium] [design] 置信度 medium：每次离场都全量扫描所有 AoiListComp，批量退出路径因此变成 O(N²) 同步阻塞

- 位置：`cpp/libs/services/scene/spatial/system/aoi.cpp:238`
- 证据：

```
aoi.cpp:238-246
    EntityUnorderedSet remoteWatchers;
    for (auto&& [watcher, list] : tlsEcs.actorRegistry.view<AoiListComp>().each()) {
        if (watcher != entity && list.entries.erase(entity) > 0) {
            remoteWatchers.insert(watcher);
        }
    }
player_lifecycle.cpp:3493 HandleExitGameNode(playerEntity, cause);  // EnqueueRelocateTicket，BeginEmergencyRelocateAll 和 BeginSceneDrain 会逐个调用
scene/main.cpp:207-213 for (const auto entity : playersNeedingExit) { ... HandleExitGameNode(entity, ExitCause::kNodeShutdown); }
```

- 推理：远端清扫遍历的是整个节点 actorRegistry 上的全部 AOI 参与者（所有场景共用同一个 registry），不只是当前场景。任务单的风险评估写的是「离场频率低」，但漏看了批量路径：BeginEmergencyRelocateAll、BeginSceneDrain、停机 drain 会在一次同步循环里对每个玩家调用 HandleExitGameNode→DetachFromScene，总代价是 O(N²) 次哈希 erase，全部压在主 EventLoop 的一次调用里。改造前离场清理只看 7 个邻格，是 O(N·k)。在 5000 人的高密度节点上大约是 2500 万次查找，会在身份丢失疏散或停机时卡住 loop 数百毫秒到秒级。而这次清扫真正兜住的只有两类条目：Pin 条目（生产中 Pin/Unpin 没有任何调用方）和场景已销毁、缺格子表的异常残留。正常空间条目在邻格里是对称的，BroadcastEntityLeave 已经清理过。代价发生在主路径上，收益只覆盖死功能和罕见异常，违反 AGENTS §11.2 YAGNI 与 §11.3「没有数据不改变热路径复杂度」。

- 建议修法：把清扫限定到需要它的情况。(1) Pin 改用反向索引：InterestSystem::AddAoiEntity 在写入 kPinned 时，给目标实体挂一个按需组件 PinnedByComp{EntityUnorderedSet watchers}（唯一写入口），RemoveAoiEntity 和 Unpin 时擦除；离场时只遍历 PinnedByComp 里的 watchers。(2) 全量 view<AoiListComp> 扫描只在格子路径没能执行时才做，即 hex、scene 缺失、场景已失效或缺 SceneGridListComp 的分支，正常离场不扫。补一个用例：两名玩家在不同场景，其中一人离场，断言另一个场景玩家的 AoiListComp 没有被访问（可以用计数桩），或者至少验证正常离场不依赖全量扫描也能清干净。PROGRESS 风险项补充写明批量退出路径的复杂度。

#### W1-34 [medium] [readability] 置信度 medium：离场序列在 HandleEnterScene 和 DetachFromScene 各写一份，正是这类分叉以前造成过 Hex 泄漏

- 位置：`cpp/libs/services/scene/player/system/player_scene.cpp:250`
- 证据：

```
player_scene.cpp:252-260
		BeforeLeaveScene leaveEvent;
		leaveEvent.actor = player;
		AoiSystem::BeforeLeaveSceneHandler(leaveEvent);
		if (!tlsEcs.actorRegistry.valid(player)) { return false; }
		tlsEcs.actorRegistry.remove<SceneEntityComp, Hex>(player);
		tlsEcs.dispatcher.trigger(leaveEvent);
player_lifecycle.cpp:2896-2914 DetachFromScene 中同样是 AoiSystem::BeforeLeaveSceneHandler → valid → remove<SceneEntityComp> → remove<Hex> → trigger；2906 行的注释写着「Hex 必须和 SceneEntityComp 成对回收。换场景那条路径…显式删了 Hex…退出这条路径漏了」
```

- 推理：「玩家离开当前场景要做哪些清理」是同一条业务知识，现在两处手写同样的 4 步。DetachFromScene 里的注释本身就记录了一次因为两份副本不同步而造成的线上缺陷（退出路径漏摘 Hex，玩家永久不可见）。本批又把 AOI 清理从通知改成直接调用，两处各改了一次。以后在离场时加清理（例如组队 AOI、战斗占位），很容易只改其中一处。违反 AGENTS §11.2 DRY（「只消除会共同变化的重复」，这里正是会共同变化的重复）。

- 建议修法：在 PlayerSceneSystem 新增唯一出口 `static void LeaveCurrentScene(entt::entity player)`（或把 PlayerLifecycleSystem::DetachFromScene 移过来改名），内容是：valid 加 all_of<SceneEntityComp> 检查 → AoiSystem::BeforeLeaveSceneHandler → remove<SceneEntityComp, Hex>（按第一条的处理决定是否保留通知）。HandleEnterScene 的第 1 步和 PlayerLifecycleSystem::DetachFromScene 都只调用它，两边不再出现 remove<Hex> 或 AOI 调用。

#### W1-35 [medium] [incomplete] 置信度 high：句柄安全表漏写 emplace/get_or_emplace/patch/replace/erase 必须先 valid，任务单 B9 的要求只做了一半

- 位置：`docs/design/ecs-component-access-rules.md:87`
- 证据：

```
| 操作 | 句柄与借用约束 |
| `get<T>` | 调用者先确认实体版本有效且有组件，或由 `view<T>` 保证 |
| `try_get<T>` / `all_of<T>` | 完整句柄检查组件归属；缺失按三类规则处理，不创建 |
| `emplace<T>` | 不搬移同池已有组件；仍不能将借用跨同步回调保留 |
| `remove<T>` | swap-and-pop 可能搬移其它实体的该组件，原借用不能继续用 |
| `destroy` | **先 `valid`**；陈旧句柄可能按 index 释放已复用的活实体… |
```

- 推理：任务单 §4.13(1) 要求这张表写明「valid/all_of/any_of/try_get/remove 安全；emplace*/get_or_emplace/get/patch/replace/erase 必须先 valid()」。EnTT 3.13.2 中 registry::emplace 只有 ENTT_ASSERT(valid)，basic_sparse_set::try_emplace 在 Release 下会直接覆盖同 index 的稀疏映射；get_or_emplace 对陈旧句柄 contains 返回 false，随后走同一条 emplace 路径。结果是同槽位活实体的组件被孤立，池数据损坏。这正是 F069 和 AnomalyDetector 改造要防的 UB。现在表里 emplace 那一行只讲「不搬移」，get_or_emplace、patch、replace、erase 完全没出现，只有挑战意见补的 destroy 写了先 valid。AGENTS §7.5 把这份文档定为组件访问的权威规则，后来的人照表写代码就会漏掉 valid。

- 建议修法：把表改成两组。一组可直接用于陈旧句柄：valid、all_of、any_of、try_get、remove（按版本比较，返回空或 0）。另一组必须先 valid()：emplace、emplace_or_replace、get_or_emplace、get、patch、replace、erase、destroy、storage.erase，理由写明「Release 下 try_emplace 或 erase 按 index 覆盖或释放同槽位活实体的映射」，并指向 DestroyEntity 这个带保护的 helper。保留 remove 一行关于 swap-and-pop 搬移的说明。

#### W1-36 [low] [rule_violation] 置信度 medium：DestroyScene 和 CreateScene 回滚都在 trigger 之后直接 destroy 场景，没有重新 valid

- 位置：`cpp/nodes/scene/handler/rpc/scene_handler.cpp:826`
- 证据：

```
scene_handler.cpp:826-831
	OnSceneDestroyed event;
	event.scene = targetEntity;
	tlsEcs.dispatcher.trigger(event);
	tlsEcs.sceneRegistry.destroy(targetEntity);
scene_handler.cpp:767-771 if (valid(sceneEntity)) { ... trigger(rollbackEvent); tlsEcs.sceneRegistry.destroy(sceneEntity); }
scene_node_service.cpp:139-143 同样是 trigger(event); 然后 tlsEcs.sceneRegistry.destroy(targetEntity);
```

- 推理：本批在 ecs-component-access-rules.md 定下两条规则：「同步 trigger 可销毁实体…返回后重验完整句柄」和「destroy 先 valid；陈旧句柄可能按 index 释放已复用的活实体」。这三处 destroy 紧跟在同步通知之后，没有重新 valid；CreateScene 回滚只在 trigger 之前查过一次。如果监听者在通知里销毁场景，紧接着又建了新场景复用同一槽位，这里就会把新场景释放掉。目前生产只有 Agones 计数监听者，不会销毁场景，所以是潜在问题。

- 建议修法：三处都改成 trigger 之后 `if (tlsEcs.sceneRegistry.valid(e)) DestroyEntity(tlsEcs.sceneRegistry, e);`，或者直接调用带 valid 保护的 DestroyEntity helper（SceneRegistry 需要对应的重载）。改动只在生成 handler 的守护段内进行。

#### W1-37 [low] [readability] 置信度 medium：AoiSystem::PinAoiEntity 只是透传，InterestSystem::PinAoiEntity 仍是公开接口，Pin/Unpin 实际没有成对收口

- 位置：`cpp/libs/services/scene/spatial/system/aoi.h:18`
- 证据：

```
aoi.cpp:249-251
bool AoiSystem::PinAoiEntity(entt::entity watcher, entt::entity target) {
    return InterestSystem::PinAoiEntity(watcher, target);
}
interest.h:21-22 // 钉住条目跨格保留;解除钉住必须经 AoiSystem::UnpinAoiEntity 重验可见性。
    static bool PinAoiEntity(entt::entity watcher, entt::entity target);
aoi_system_test.cpp:440/441/483/592/663/711 仍直接调用 InterestSystem::PinAoiEntity
```

- 推理：挑战意见要求 Pin/Unpin 放进同一个类。现在的做法是加了一个没有隔离或转换价值的透传层（AGENTS §11.1 不鼓励这种层），而 InterestSystem::PinAoiEntity 仍然公开，测试也继续用它，于是同一个能力有两个入口，「成对」只靠注释约束。另外 Pin/Unpin 在生产里没有调用方。

- 建议修法：二选一。要么把 InterestSystem::PinAoiEntity 删掉或改为私有，Pin 只保留在 AoiSystem，测试统一改调 AoiSystem::PinAoiEntity；要么按 YAGNI 在生产接入之前不提供 AoiSystem 包装，只在 interest.h 注明 Unpin 的入口。

#### W1-38 [low] [doc] 置信度 high：多处注释在第 5 批改造后已与实现不符

- 位置：`cpp/nodes/gate/main.cpp:148`
- 证据：

```
gate/main.cpp:148 // TimerTaskComp 自带存活令牌(AGENTS.md §11.7)。
skill.cpp:64 // 施法者与目标共用唯一上下文写入口:第一次写入时延迟创建,失效实体绝不创建。
player_lifecycle.cpp:4510-4512 // …显式先摘是为了让下面 DetachFromScene 触发的事件(BeforeLeaveScene …)看到的是一个普通的离场实体,而不是一个会被各业务系统的 Frozen 闸跳过的实体。
anomaly_detector.h:80 // Clear all tracking data (e.g. on server restart or player logout).
docs/handoff/ecs-refactor/ecs-fit-overview.md:121 - 组件是纯数据：不继承、不嵌 registry。TimerTaskComp 是唯一例外，必须写 in_place_delete。
```

- 推理：aliveToken 已删除，现在是内部 State。目标侧上下文镜像已删除，写入口只服务施法者。BeforeLeaveScene 在生产中已没有监听者，AOI 改为直接调用，也不检查 Frozen。AnomalyDetector::ClearPlayer/ClearAll/GetCurrencyGainCount 已没有生产调用方，「player logout」时清理的说法不再成立。ecs-fit-overview 还写着 TimerTaskComp「必须写 in_place_delete」，且「组件不继承」，与 A2 及 B8（BuffListComp 等继承 std 容器）的结论矛盾。

- 建议修法：逐条改正。gate/main.cpp 改成「闭包只弱持内部 State」。skill.cpp:64 改成「施法者上下文的唯一写入口」。player_lifecycle.cpp:4510 删掉关于事件和 Frozen 的理由，只保留「交接标记与冻结成对摘除」。anomaly_detector.h 标明 ClearPlayer/ClearAll/GetCurrencyGainCount 仅供测试和运维使用，或者直接删除没有调用方的 ClearPlayer。ecs-fit-overview.md:121 改为「TimerTaskComp 可搬移，in_place_delete 只为指针稳定；具名容器组件允许继承 std 容器，基类转换必须 explicit」。

#### W1-39 [low] [incomplete] 置信度 high：挑战意见点名的 3 份设计文档仍在描述已删除的 ScenePlayers

- 位置：`docs/design/scene-creation-architecture.md:344`
- 证据：

```
scene-creation-architecture.md:344 3. Emplaces `SceneInfoComp` (config_id, guid) and `ScenePlayers` (empty set).
agones-scene-node-high-density.md:23 `sceneRegistry.create()` + 挂 `SceneInfoComp` / `ScenePlayers`
async-load-disconnect-reconnect-race.md:45 -> 将玩家绑定到场景，加入 ScenePlayers 和 AOI 网格
```

- 推理：batch5 §8「遗漏的调用点」明确列出了这三处，B2 已经删除 ScenePlayers，场景成员现在只以 SceneEntityComp 为准，但文档没有同步。读者会以为仍有一份反向集合需要维护（AGENTS §5「没写文档 = 没说过」）。

- 建议修法：三处改成「玩家只挂 SceneEntityComp；场景成员由扫描 view<SceneEntityComp, PlayerGuidComp> 得到，不维护反向集合」，并链接 ecs-component-access-rules.md 的对应段落。

<details><summary>该审查者的核对范围（coverage）</summary>

只读核对了以下文件（git diff 6281e4d27 加当前工作区最终代码与调用方）。
1）TimerTaskComp：timer_task_comp.h/.cpp、timer_queue_unit_test.cpp（新增 5 例），并对照 EnTT 3.13.2 storage.hpp 的 pop（swap_and_pop 用 std::exchange、in_place_pop）、swap_at、emplace_or_replace→patch 的移动赋值路径，以及 muduo cancel/到期批次的语义。确认以下几点无问题：移动转移 State、移出方 Cancel 为空操作、移动赋值先取消自身再接管、自移动有保护；开火时由闭包强持 State，回调中销毁或移动宿主不会悬空；generation 能挡住批内旧开火；Cancel 只在 armed 时调 loop->cancel，one-shot 开火后清标志，这两处行为等价。新设计比旧的 aliveToken+this 更简单，不算过度设计。逐个核对了所有捕获 this 的宿主：Node（noncopyable）、DependencyGate、EtcdService、ServiceDiscoveryManager、BattleRoomManager（显式 delete）、GuidSegmentClient（删了拷贝，所以不可移动）、BattleClientEdge（noncopyable）、GateRuntimeContext（成员不可移动）；PendingNodeRemoval 由 unique_ptr 持有；skill、buff 回调只捕获句柄或 ID。全部安全。
2）scoped_connection：node.h/.cpp 的 RegisterEventHandlers 先 release 再 connect，顺序正确（EnTT 的 connect 会先 disconnect 同一候选，旧连接后释放会误断新连接）；成员最后声明、最先析构，析构函数体里的 fallback drain 期间仍保持连接。client_message_processor.h/.cpp：成员析构顺序以及 RunSimpleNodeMainWithOwnedContext 里 context、handler、node 的构造与析构顺序都正确。dispatcher.clear 只清队列。生产代码中 connect<&X::Y>(*this) 只有这两处，均已 RAII。
3）AnomalyDetector：.h/.cpp、anomaly_detector_test.cpp、currency_test 旧用例、bag_service 和 currency_system 两处调用点。确认无问题：先 valid 再 get_or_emplace，查询不创建组件，ClearAll 用 clear<T>，组件随实体销毁。
4）ScenePlayers 删除与 BeginSceneDrain：结果与旧反向索引等价，事实源只剩 SceneEntityComp；cross_zone_test 新用例有效。
5）AOI：aoi.cpp 中的 BeforeLeaveSceneHandler、BroadcastEntityLeave、UnpinAoiEntity、ResetEntityVisibility，interest.cpp 的 Add/Remove/Pin，view.cpp 的 CanSee（先降级再判隐身，正确），DetachFromScene，HandleEnterScene，DestroyPlayer 兜底；DestroyPlayerWithoutDetachLeavesNoAoiResidue 用例修复前确实会红。
6）技能上下文只存施法者：skill.cpp 中的 StoreSkillContextForWrite、HandleSkillFinish、CheckTimerPhase、各相位 emplace_or_replace 与回调内 remove，skill_comp.h、buff_comp.h 的 explicit 基类转换，skill_test 两个新用例（修复前会红）。
7）通知重入防护：InitPlayerFromAllData 的 canContinueInitialization 与三个通知、EnterScene 的 stopEntry/canContinueEntry、EntryOwnershipGuard、DestroyDeposedPlayer、HandleExitGameNode、scene_local_events.cpp 的生产 sink 清单，以及 cross_zone_test 中重入相关用例。
8）文档：ecs-component-access-rules.md 的句柄表、copilot-instructions.md 中 TimerTaskComp 一节、ecs-fit-overview.md、PROGRESS 第 5 批条目，以及挑战意见遗漏清单里点名的 3 份设计文档。
未审：gRPC/muduo 传输（PollGrpcNodeShutdown、CompleteNodeRemoval 的计时器复用）、PlayerGuidComp 模板生成、SceneEntity 全量迁移、背包，这些属于其他批次或其他会话。结论均为静态阅读，未编译、未运行。

</details>

### A.extra 补充收尾：gRPC 资源寿命、场景权限、Go scene_manager、scene_test 迁移

#### W1-40 [high] [contract_compat] 置信度 high：新增的 creators 准入只在 C++ 节点末端生效：「按场景号加入镜像」、组队跟进镜像、镜像去重复用都会被拒，且与 Java 版对账口径相反

- 位置：`cpp/libs/services/scene/player/system/player_scene.cpp:196`
- 证据：

```
player_scene.cpp:196-198
  if (!info->creators().empty() && (playerId == 0 || !info->creators().contains(playerId)))
      return kCheckEnterSceneCreator;
player_lifecycle.cpp:2471-2481（HandleEnterGameNode 预检）、2621-2627（EnterScene）、4064-4069（InitPlayerFromAllData）；s2s_player_scene_handler.cpp:36-43
player_scene_handler.cpp:75  // (joining an existing mirror by id) and falls through to the normal path
go/scene_manager/internal/logic/createscenelogic.go:202-212  去重命中时 return {SceneId: existingId, CreatorIds: in.CreatorIds}
基线 6281e4d27 的生产代码里 git grep kCheckEnterSceneCreator / creators().contains 全为空
```

- 推理：基线里 creators 只写不读。Java 仓 PARITY.md 镜像那一行写明两版同基线口径是「creators 不做准入，任何人可按号加入……组队同节点跟进镜像」，B5「接上 creators 准入（3022）」只是挂着的可选项。这次 Codex 只在 C++ 场景节点的三个入场入口把它变成了强制准入，后果有三条：(1) 客户端的「scene_id>0 按号加入已有镜像」（player_scene_handler.cpp:75 写明是支持的流程），以及 PlayerTeamSystem::CheckFollowLeader 带队员进队长所在的镜像，非创建者现在一律收到 3022。Java 版同样的请求会成功，客户端可见的 tip 和行为在两版之间分叉，违反 AGENTS §12。(2) scene_manager 不知道 creators：EnterScene 在路由之前已经提交了 location、场景人数计数，必要时还铸了 owner_epoch，节点到最后才拒绝，Redis 里的归属和人数就和节点实际状态对不上；跨节点加入时，源节点可能已按交接释放了实体，玩家落到哪儿都没有实体。(3) MirrorDedupBySource=true 时，去重命中直接复用已有镜像，回显的是新调用者的 CreatorIds，但 C++ 端那个场景的 creators 仍是首个创建者，所以自动进场必然被拒，去重复用这个功能整体失效（目前默认关闭）。

- 建议修法：先请负责人拍板。若维持两版同基线：CheckEnterScene 只校验场景存在和身份非零，删掉 creators 分支和对应用例（SceneEntryPermissionTest 里依赖 3022 的几条），同时在 PARITY 的 B5 写明「不做准入」。若确实要做准入：(a) 准入判定挪到 scene_manager 的 EnterScene，在 placePlayerLocation/计数之前完成：CreateScene 时把 creators 写进 Redis 场景元数据，拒绝时不改任何状态；(b) 去重命中时经 C++ CreateScene 的幂等分支把新 creator_ids 合并进已有场景；(c) 组队跟进镜像要明确放行规则（例如同队成员）；(d) Java 版同批实现，并登记 PARITY 和 tip 契约变更。C++ 节点末端的检查只作为纵深防御保留。

#### W1-41 [medium] [lifetime] 置信度 medium：停机时同一调用栈内先发 LeaseRevoke，紧接着就 TryCancel 所有在途调用，优雅注销基本退化成等租约过期

- 位置：`cpp/libs/engine/core/node/system/node/node.cpp:1222`
- 证据：

```
node.cpp:1201-1202
  // 释放请求也属于在途调用：关闭时仍由相同完成路径回收，不留下 CQ/tag。
  ReleaseNodeId();
node.cpp:1215-1223
  for (auto &nodeRegistry : tlsNodeContextManager.GetAllRegistries()) { ... BeginShutdownGrpcNode(nodeRegistry, entity); }
grpc_init_total.cpp.tmpl:88
  for (auto* context : runtime->calls) context->TryCancel();
etcd_transport.cpp:33  默认 return std::make_unique<GrpcEtcdTransport>(endpoint, sink);
```

- 推理：默认的 etcd 传输是官方 gRPC，LeaseRevoke 的 ClientContext 在 Start…Call 里已登记进 Etcd 实体的 runtime->calls。FinalizeShutdownInLoop 在同一个函数里、没有让出 loop，就遍历包括 Etcd 在内的全部 registry 调 BeginShutdownGrpcNode，对刚发出的 LeaseRevoke 执行 TryCancel。请求可能还没写出去；即使写出去了，etcd 端的 handler ctx 也会被 RST_STREAM 取消，raft 提案未必提交。基线不关 CQ，revoke 至少有进程退出前那段时间可以送达；现在它大概率被本地取消。结果是节点的发现键、分配键、端口键都要等 TTL 过期才消失：这段时间 gate/scene_manager 继续把流量导向已停的实例，同端口重启的 PutIfAbsent 也会撞上残留的端口键。README 说「LeaseRevoke 发送策略属于独立任务」，但这里的取消是本次 F010 改动新引入的退化。

- 建议修法：LeaseRevoke 不能和其他在途调用一起被立即取消。可以改用每次调用回调版发出 LeaseRevoke，Etcd 实体的 BeginShutdownGrpcNode 推迟到它完成或到一个有界截止（例如 1–2s，用单调时钟）之后再调，其他节点照常立即取消；或者把 ReleaseNodeId 挪到 before-shutdown 排空阶段，等它完成后再进入 FinalizeShutdownInLoop。补一条回归：真实 gRPC 假 etcd 服务端断言停机后收到了 LeaseRevoke。

#### W1-42 [medium] [ecs_fit] 置信度 medium：gRPC 客户端寿命状态继续塞进节点 registry，又新增 GrpcClientRuntimePtr 组件，靠 in_place_delete 和每次回调后重取组件来维持正确性，难读难维护

- 位置：`tools/proto_generator/protogen/internal/template/grpc_init_total.cpp.tmpl:153`
- 证据：

```
grpc_init_total.cpp.tmpl:153-154
  registry.emplace<GrpcClientRuntimePtr>(nodeEntity, std::make_shared<GrpcClientRuntime>());
  registry.emplace<grpc::CompletionQueue>(nodeEntity);
grpc_async_client.h.tmpl:30-31  static constexpr bool in_place_delete = true;（流式调用对象仍是 ECS 组件）
grpc_init_total.cpp.tmpl:92-101  // 每次轮询都重取组件；业务完成回调可能重入关闭或改变注册表。 ... registry.try_get<grpc::CompletionQueue>(e) ... try_get<GrpcClientRuntimePtr>(e)
grpc_async_client.cpp.tmpl:85-113  ReadBorrow 析构里 registry.try_get<Async…GrpcClient>(entity) 重取
docs/design/ecs-component-access-rules.md:95  官方 gRPC Channel/CQ/Stub 等已有组件属于明确存量边界，不能据此新增同类…组件
```

- 推理：按 ECS 适用判据，网络资源和单例/私有对象不该进 registry；节点 registry 是十几个异构网络资源的容器，不是大量同构实体的批处理场景。审计 F011/C008 建议把缓冲挪进不可移动的调用对象，Codex 的做法是让流式调用对象本身继续当组件，再加一个 shared_ptr 组件记录寿命，于是 Send、轮询、READ 借用、关闭每一步都要防着「registry 在回调里被改」而重新 try_get，状态散在 CQ、Stub、Runtime、调用组件四处。这正是负责人说的「滥用 entt、不可读」。同时它和 Codex 自己写进组件访问规则「不能据此新增同类组件」的口径相矛盾。

- 建议修法：在模板里引入一个非 ECS 的 GrpcNodeClient：独占 CQ、各 Stub、GrpcClientRuntime 和流式调用对象，用 unique_ptr 保证地址稳定。节点实体上最多挂一个 std::unique_ptr<GrpcNodeClient> 组件，或改由 NodeContextManager 按 uuid 持有；Send、Begin、Poll 直接操作这个对象，不再逐组件 try_get。如果本轮不改，至少在 grpc-muduo-transport.md §10.1 和组件访问规则里把 GrpcClientRuntimePtr 登记为临时偏离，写明删除时机（第 3 批删 CQ 时一并删）。

#### W1-43 [low] [test_invalid] 置信度 high：C008 用例里的「swap-and-pop」步骤在修复前后都不可能触发移动，测不出回归

- 位置：`cpp/tests/rpc_controller_test/grpc_client_lifecycle_test.cpp:356`
- 证据：

```
grpc_client_lifecycle_test.cpp:356-359
  // 删除同类型响应组件会发生 swap-and-pop；不得挪动正在异步读取的缓冲。
  const auto other = registry.create();
  registry.emplace<etcdserverpb::WatchResponse>(other);
  registry.destroy(other);
```

- 推理：other 在 InitNode 之后才创建，它的组件排在池尾，销毁时直接弹出，不会移动节点实体的组件；修复后应答缓冲已不是 WatchResponse 组件，这一步更是空操作。剩下的 EXPECT_EQ(&reply, buffer) 断言的是 &client.reply，在结构上恒成立。所以这条「稳定缓冲」回归在修复前也不会以行为层面的失败暴露问题。

- 建议修法：删掉这几行误导性步骤；或者在 InitNode 之前另建一个实体并挂上同类型的流式调用组件，发起 Read 后销毁那个靠前的实体，再断言 reply 地址不变、回调收到的数据正确。也可以接受这条只能做编译期约束，改用 static_assert(entt::component_traits<Async…GrpcClient>::in_place_delete)。

#### W1-44 [low] [missed_site] 置信度 medium：DataService 选点没有跳过关闭中的节点，与 README「选点跳过无传输及关闭中的节点」不一致

- 位置：`cpp/nodes/scene/id_segment_bootstrap.cpp:95`
- 证据：

```
id_segment_bootstrap.cpp:93-100
  for (auto entity : registry.view<NodeInfo, data_service::DataServiceStubPtr, std::shared_ptr<grpc::Channel>>())
  {
      const auto &channel = registry.get<std::shared_ptr<grpc::Channel>>(entity);
      if (channel && channel->GetState(true) == GRPC_CHANNEL_READY) ready.push_back(entity);
```

- 推理：节点摘除的排空窗口里，实体已 BeginShutdownGrpcNode（closing=true），通道仍可能是 READY。这里仍可能选中它，Send 随后走本地 UNAVAILABLE，领段客户端白白多一次失败和退避。GetSceneManagerEntity 和 gate 的 PickRandomNode 都已改用 HasUsableGrpcClient，这一处漏了。

- 建议修法：循环里先 if (!HasUsableGrpcClient(registry, entity)) continue;，或把视图改为 view<NodeInfo, DataServiceStubPtr, std::shared_ptr<grpc::Channel>, GrpcClientRuntimePtr> 并检查 !runtime->closing。

#### W1-45 [low] [doc] 置信度 high：F010 修复后，路由服摘除时在途调用会以 CANCELLED 回调，gate lane 的注释和计数口径已过时

- 位置：`cpp/nodes/gate/handler/rpc/client_request_lane.h:87`
- 证据：

```
client_request_lane.h:87  // 路由服节点摘除(OnNodeRemoveEvent 在 DestroyEntity 之前…):该节点上的在途调用永远不会再有回调,
client_message_processor.cpp:1240-1241  // …实体上的 CompletionQueue 随后析构,其上的在途调用永远不会再有回调(F29)。
node.cpp:1601-1603  BeginShutdownGrpcNode(nodeRegistry, entity); tlsEcs.dispatcher.trigger(nodeRemovedEvent);
```

- 推理：现在摘除时先取消、后排空，这些调用会经 NotifyCallFailure 回到 OnRouterCallFailed。Claim 取不到会返回 true，所以不会出现重复回执，行为是对的，但 tlsCounters.lateDropped 会把每个被摘除的在途调用再计一次，注释也与实际相反，会误导后续维护和运维看指标。

- 建议修法：两处注释改为「取消后会以非 OK 回调，lane 已先行摘出，回调由 Claim 丢弃」；摘除路径产生的迟到完成不计入 lateDropped，或单独计一个 cancelledAfterRemoval。

#### W1-46 [low] [design] 置信度 low：gRPC 排空轮询没有时间预算，也没有长时间未排空的日志

- 位置：`cpp/libs/engine/core/node/system/node/node.cpp:1272`
- 证据：

```
node.cpp:1269-1273
  if (!PollGrpcClientsInLoop())
  {
      grpcClientDrainTimer_.RunAfter(0.005, [this] { DrainGrpcClientsInLoop(); });
      return;
  }
node.cpp:1625-1633  CompleteNodeRemoval 同样每 5ms 无限重排
```

- 推理：不在未排空时释放 CQ 是对的，否则 grpc_completion_queue_destroy 会断言。但 AGENTS §11.3 要求等待有截止时间并且可观测。只要有业务回调持续向已关闭节点发请求（每次都排一个本地完成），或者某个调用取消后迟迟不完成，停机或摘除就会无声地无限轮询，日志里看不出卡在哪个节点、多少在途。

- 建议修法：用单调时钟记录开始时刻，超过阈值（例如 5s）后按节点类型、uuid 节流打印 calls.size()、queuedCompletions、activeCompletions、localCompletions；停机路径可以在超出更长预算后 LOG_ERROR，并保持不释放，同时把这些数字留作排障证据。

<details><summary>该审查者的核对范围（coverage）</summary>

实际核对（git diff 6281e4d27 + 当前工作区）：
1) gRPC 资源寿命：tools/proto_generator/protogen/internal/template/grpc_async_client.{cpp,h}.tmpl（流式 INIT/READ/WRITE/FINISH 状态机、ReadBorrow 借用期、TryFinish/TryWrite 条件、unary Start…Call 的缺 CQ/Stub/runtime/关闭时异步本地失败、Complete 的 release 顺序、tag 用 unique_ptr 回收）；grpc_init_total.{cpp,h}.tmpl 及已生成的 cpp/generated/grpc_client/grpc_init_client.cpp（BeginShutdownGrpcNode 的 TryCancel、PollGrpcNodeShutdown 的「calls 空且回调空闲才 Shutdown」与「AsyncNext 到 SHUTDOWN 才 drained」、HandleCompletedQueueMessage、各 nodeType/messageId 分派表与 InitGrpcNode 一致）；cpp/libs/engine/core/network/grpc_client_runtime.h（计数、CompletionScope、QueueGrpcClientCompletion）；node.cpp 的 FinalizeShutdownInLoop、PollGrpcClientsInLoop、DrainGrpcClientsInLoop、FinalizeTeardownInLoop（重入时 kafka/redis Shutdown 幂等，已核实）、ExecuteNodeRemoval、CompleteNodeRemoval、~Node 兜底；node_connector.cpp（同 uuid 重注册由 2s 周期 Range 自愈）；third_party EnTT component.hpp 的 in_place_delete 语义。以上确认无问题的部分：与 grpc++ 语义一致（先 TryCancel 再处理全部 tag，calls 空后才 Shutdown，Next 到 SHUTDOWN 后才允许销毁）；unary 的 context 保留到业务回调退栈；本地完成计入排空门槛；关闭后不再向 CQ 追加操作。
2) 选点与前置校验：node_utils.cpp GetSceneManagerEntity（muduo_grpc 目前只给路由服装工厂，不冲突）；gate 的 PickRandomNode、SendRawViaRouter、OnNodeRemoveEventHandler；client_request_lane 的 OnRouterNodeRemoved/OnRouterCallFailed（Claim 失败返回 true，不会重复回执）；registration_manager.cpp 的 IsHandshakeNodeType（只有 Gate→Scene 握手，battle 无出站，不误拒）；service_discovery_manager.cpp 的 node_type 一致性校验（Go 各服务注册时都填 NodeType，shared/noderegistry validateValue 强制，不误拒）；id_segment_bootstrap 选点（漏判 closing，已报）。
3) 场景权限：player_scene.{h,cpp} 的 EntryOwnershipGuard、CheckEnterScene、HandleEnterScene；player_lifecycle.cpp 的 HandleEnterGameNode 预检、EnterScene（stopEntry/canContinueEntry/stillOwnsEntry）、InitPlayerFromAllData 的预检与失败撤销；s2s_player_scene_handler.cpp；player_scene_handler.cpp 镜像分支；scene_manager_response_handler.cpp 自动进场；player_team.cpp 跟随；Go createscenelogic.go 去重分支、enterscenelogic.go 提交顺序；Java 仓 PARITY.md 镜像行（creators 不做准入）。所有权快照对比与通知后复核逻辑本身自洽，问题在 creators 准入的契约层面，已报。
4) Go scene_manager：scene_node_client.go 的 sceneId!=0 与应答身份校验；world_rebalance.go 的失败保留映射；C++ scene_node_service.cpp 在基线和当前都回填 scene_info，滚动升级兼容；integration_test.go 的两条 bufconn/miniredis 用例在修复前会失败，有效。无问题。
5) scene_test 删除：旧 scene_test.cpp 引用的 SceneCommon/SceneNodeSelector 等在基线已不存在，是死工程；唯一有效的 creators 断言已迁到 cross_zone_test SceneEntryPermissionTest，并补了通知重入用例；runner、sln 里无残留引用（muduo_grpc_test.vcxproj 只是 git 按相似度识别成重命名）。迁移完整。
6) 测试：grpc_client_lifecycle_test.cpp 全部 9 例、cross_zone_test 的 ScopedFakeSceneManagerNode 与 SceneEntryPermissionTest 已读；除 swap-and-pop 那一步无效外，其余用例在修复前能失败。
未审：GrpcDone/GrpcCallOptions 每次调用回调、muduo_grpc/、etcd 传输重构、RPC 通道重构（属其他会话）；InitPlayerFromAllData 中 actor 初始化部分属第 5 批 §10，未深入。

</details>

---

## 8. 续做进度（2026-10-16，接手会话）

- **已完成**：第二波 5 个方向查找（47 条，编号 W2-<方向>-NN）；第一波 46 条全部对抗式复核（W1-40 由两人独立复核，结论一致）。
- **未完成**：第二波 47 条的复核（W2-b5_types-07 为 low 且低置信度，按 §5 不复核）、high 二审（W2-b1_runtime-01）、审计 110 条 HEAD 状态、正式报告、补充任务单、PROGRESS。原因：5 小时额度已用到 85%，第二波复核约需 15 个代理，等额度重置后续跑。
- **原始结果**（含每条的证据、推理、修法、回归测试、拍板选项）：工作流日志 `C:/Users/luyua/.claude/projects/D--luyuan-xuanming-server-mmo-cpp/c6d8934d-b65b-4690-aba9-4ee7907d82c3/subagents/workflows/wf_e9c87f48-099/journal.jsonl`，每行一个代理的 `result`。同一会话内可用该会话目录下 `workflows/scripts/ecs-review-wave2-and-verify-wf_e9c87f48-099.js` 加 `resumeFromRunId: "wf_e9c87f48-099"`（不带 findOnly）续跑第二波复核，已完成的代理走缓存。
- **只读核对**：代理记录里没有任何 Write/Edit；%TEMP%、/tmp、会话草稿目录里没有仓库文件副本。

### 8.1 第一波复核结论

| 编号 | 原严重度 | 复核 | 复核后 | 更正后的问题 |
|---|---|---|---|---|
| W1-01 | medium | 降级 | low | 退出流程遇到拒存一律保留实体:拒存原因全是永久的,保留永远存不了盘,只会让退出收不了尾、重连复用僵尸并扩大丢失量;但当前生产代码到不了拒存分支,属防御分支的处置口径问题,原修法(releaseMarkSuppressed + FinishExitAfterPersist)还会经改派写出交接标记 |
| W1-02 | low | 降级 | low | 三处交接预检各构造一份整份 PlayerAllData 后丢弃。StartTravelHandoff 的预检拒绝没有调用点日志；BeginTravelHandoff 的预检保护不了任何东西。提交点 kRejected 分支是负责人要求保留的复核，不是缺陷 |
| W1-03 | low | 降级 | low | 结算 RequestSettlementPersist 中 PersistFn 返回 false 的注释列了一个在这里走不到的来源（交接/退出跳过），又漏了实际可达的拒存；原修法把「交接已发起」写进新注释同样不准 |
| W1-04 | low | 成立 | low | SavePlayerToRedisImpl 头注释仍写「只有三个调用方」传 allowSkipWhenPersisted=false，实际 5 处，与 cpp 内清单不一致 |
| W1-05 | medium | 成立 | medium | bag_selection_order_test 有 5 例没跟上「扣空即释放 / 还原跳过零堆」的新语义，当前必红，其中 2 例已构造不出它要测的空堆 |
| W1-06 | medium | 成立 | medium | VectorBatchNeverEvictsWhenSegmentIsUnavailable 的 ② ③ 仍按旧闸门「号源不可用不许腾位」断言，与「先规划到不动点、按最终规划铸号」的新契约冲突，当前必红 |
| W1-07 | medium | 降级 | low | FlatLayout 是唯一非 final 的具体布局，今后它的派生类漏写 Clone 会在首次入包后被切片；目前所有派生类都已覆盖，属于潜在的维护陷阱 |
| W1-08 | low | 不成立 | none | 每次入包都复制候选仓库和布局，这是文档化并已被接受的原子性代价；没有性能数据，不应为它另开一条快路径 |
| W1-09 | low | 成立 | low | 实例层和桥层的注释仍写着「空堆保留占格、等后续并堆回填 / 由整理统一回收」，与「扣空当场两层成对释放、加载跳过零堆」的现行实现相反；CanMintGuid(s) 已无生产调用方，注释却仍说铸号型写入必须先调用它 |
| W1-10 | low | 成立 | low | 调用方仍按旧口径写：「入包失败可能已改包」「扣空留僵尸堆」。beforeCounts 补差、items 部分发放分支、mutated 出参、扣除后的 ReclaimEmptyStacks/MergeAndCompact 在原子提交和扣空即释放之后都已不可达或不再需要，属于该删没删的防护 |
| W1-11 | low | 成立 | low | C004 只删了 C++ 的 PlayerItemBlockList，proto 的 PlayerItemBlockComp（注释还引用不存在的 Bag::IsItemBlocked）和 exploit_loss_prevention / bag-service-srp-refactor / 04-asset-channel 文档仍把单人物品封禁和 blockList 参数写成现状 |
| W1-12 | low | 降级 | medium | batch3 §4.7 任务单与挑战意见要求的背包设计文档更新大半没落实：没有 §14 规则表，§8「全部已知差异」没登记本批客户端可见的变化，bag-rule-policy-layering.md 一字未改、仍把已删除的 ReserveOrEvict/ReserveForBatchAdd 写成现行形状 |
| W1-13 | low | 成立 | low | 旧档零章补盖依赖调用方「先对全包 Raise 再逐条 Insert」两步协议：bag_marshal 写了两遍，Bag 只暴露半个不变量 |
| W1-14 | low | 降级 | low | AddItems 有五处内部不变量失败复用表数据错误码 kInvalidTableData，其中三处没有专门日志；同文件多处函数内 tab 与空格缩进混排 |
| W1-15 | low | 降级 | low | BagJavaParityTest::TearDown 把 item 号段恢复成 bag_instance_data 的 [900000,1000000) 而不是基线 [1,2^54)，默认顺序下被后续夹具掩盖，乱序或换序运行时会与硬编码 guid 撞号 |
| W1-16 | medium | 降级 | medium | Kafka 入口的 13 个必需命令（4 个 scene、9 个 gate，外加 gate 信封回退）仍经 tlsEcs.dispatcher.trigger 转发；没有监听者时静默丢弃，却照样返回 kDispatched。这是任务单 4.4 定下的写法，生产路径目前到不了丢消息的情形 |
| W1-17 | medium | 成立 | medium | 白名单单测把上界写死为 49，没有覆盖已进入白名单的 49 号 PlayerInboxNotifyEvent，也拦不住以后把新事件号（≥49）误标进白名单 |
| W1-18 | medium | 成立 | medium | 一次性的事件迁移逻辑被固化成生成器每轮必跑的步骤：用正则改写三份手工维护的构建清单，并以 attribute_sync_option 为锚点硬编码补登 event_ingress_option。迁移早已完成，这段逻辑现在只剩新增 Fatal 的副作用 |
| W1-19 | low | 成立 | low | 已删除的 DispatchProtoEvent 和 SceneEventHandler 仍被注释与规范引用，任务单 4.4 第 6 条的零命中门禁没有达成 |
| W1-20 | low | 成立 | low | 两份节点模板和模板 README 没有跟上新 hook 契约：只声明 KafkaCommandType 而缺 DispatchKafkaCommand，照抄就会撞上 static_assert，也看不出该接哪个白名单 |
| W1-21 | low | 成立 | low | 控制面 topic 文档里的 scene 白名单数量和生成状态都已过时；事故文档 F001/F002 没有按任务单 4.5 第 2 条改为指向新方案 |
| W1-22 | medium | 降级 | medium | 沉默状态和 GM 调级后的重算都是只有一个消费者的必需步骤，现在仍经 dispatcher 转发：不装订阅的宿主里会静默失效，skill_test 只能手工复刻接线，Buff 系统还为任意监听者的重入写了防御代码和专项测试 |
| W1-23 | medium | 成立 | medium | 生产中零监听或只有空监听的 6 个事件仍在 trigger，还配了通知后重入复核和人造重入测试；复核分支一旦走到会留下半成品状态 |
| W1-24 | medium | 降级 | low | 66 号 ActorBaseAttributesS2C 只在战斗状态那一路带 entity_id，移动和速度增量的 entity_id 为 0。这是改造前就有的缺口，Java 已修，C++ 待跟；真客户端没有 66 的处理器，ECS 改造也没有让它变坏 |
| W1-25 | low | 降级 | low | ActorNetIdComp 定义在公共头里、可以随意写，导致 Of、Resolve、Assign 各自再做一遍反查自检；FillActorCreateMessageInfo 与两个调用方重复判 0，契约也没写明 |
| W1-26 | low | 成立 | low | 同一个不可信的 target_id 在一次 ReleaseSkill 里被反查最多 6 次；uint64 版的 ResolveSkillTarget 没有按任务单删掉，ValidateTarget 还手写了一遍与 ValidateSkillTargetEntity 重复的检查 |
| W1-27 | low | 成立 | low | OnNodeRemoveEvent::sceneNodeType 是无人写、无人读的死字段；SkillExecutedEvent 的 skillId/skillTableId 从不赋值，唯一消费者 BuffSystem::OnSkillExecuted 是空 TODO |
| W1-28 | low | 成立 | low | gate 节点句柄类型化后残留旧整数逻辑：GetEffectiveNodeEntity 成了多余包装，ResolveSessionTargetNode 里有无意义别名和不可达的重绑分支，ForwardPlayerToScene 日志用“scene_node=”打印实体整数 |
| W1-29 | low | 降级 | low | “同 index 不同 version 判为换节点”用例是类型契约固定用例，不是修复前会红的回归测试，回执把它称作“回归”是误标 |
| W1-30 | low | 降级 | low | 事件结构体化后仍有注释描述旧机制：mission.cpp 的“截成 uint32”注释和多余别名、CreateScene 回滚注释里的“AOI 格子表”、属性同步注释里的“Guid(uint64_t)组件”；回滚分支本身在生产中也已不可达 |
| W1-31 | low | 降级 | low | actor net id 的唯一性范围（同时在线的节点之间唯一，进程重启后同一 node_id 会重新发同样的号）与 Java 雪花号不同，但没有写进两版的契约文档；F080 的三项客户端确认也没有记录。原文“取值区间已变”的担忧不成立 |
| W1-32 | medium | 重复 | medium | 为没有生产订阅者的通知保留重入防护，防护分支自身留下半成品状态（与 W1-23 同根，已并入）（并入 W1-23） |
| W1-33 | medium | 成立 | medium | 每次离场都全量扫描全节点 AoiListComp，疏散、排空、停机这几条批量退出路径因此变成 O(N²) 的主循环同步阻塞，而这次扫描只兜住了生产中没有调用方的远端 Pin |
| W1-34 | medium | 成立 | medium | 玩家离场序列（AOI 清理→摘 SceneEntityComp 与 Hex）在 HandleEnterScene 和 DetachFromScene 各写了一份，同类分叉以前造成过 Hex 泄漏 |
| W1-35 | medium | 成立 | medium | 组件访问规则的句柄安全表漏写 emplace/emplace_or_replace/get_or_emplace/patch/replace/erase/storage.erase 必须先 valid，任务单 B9 §4.13(1) 和挑战意见都只落实了一半 |
| W1-36 | low | 降级 | low | 场景创建和销毁通知之后的复核与回滚，是为不存在的监听者写的防护，并且与规则文档的「trigger 后重验」互相矛盾；应改用监听者契约收口，删掉 CreateScene 的回滚，而不是在 4 处再补 valid |
| W1-37 | low | 成立 | low | Pin 有 AoiSystem::PinAoiEntity（纯透传）和公开的 InterestSystem::PinAoiEntity 两个入口，10 处测试与三份设计文档仍指向 InterestSystem，Pin/Unpin 成对只靠注释约束 |
| W1-38 | low | 降级 | low | 第 5 批后有 4 处注释或文档与实现不符(skill.cpp、player_lifecycle.cpp、anomaly_detector.h、ecs-fit-overview.md);gate/main.cpp 那处并没有过时 |
| W1-39 | low | 成立 | low | 3 份设计文档仍描述已删除的 ScenePlayers 反向集合 |
| W1-40 | high | 成立 | high | Codex 把废弃 scene_test 里的 creators 断言做成了生产准入，而且只在 C++ 节点末端生效。非创建者按号加入镜像、组队跟进镜像、开启去重后复用镜像这三条合法流程都会被拒，scene_manager 已提交的 location 和人数随之与节点状态不一致，C++ 和 Java 两版对客户端的行为也分叉了 |
| W1-41 | medium | 已被改写 | none | 原问题（F010 在 LeaseRevoke 后立即 TryCancel 全部在途调用）已随 CQ 和 GrpcEtcdTransport 一起删除；现在 MuduoEtcdTransport 对 LeaseRevoke 只做「尽量送达」，这是迁移设计 §13.7 记录在案的取舍，与 ECS 改动无关 |
| W1-42 | medium | 已被改写 | none | GrpcClientRuntimePtr、CQ 组件、in_place_delete 流式调用组件和逐回调 try_get 都已删除；节点实体上只剩一个 MuduoGrpcChannelPtr，调用状态随回调走，不进 ECS |
| W1-43 | low | 已被改写 | none | 被测对象(注册表里的流式调用组件及其 reply 缓冲)已随 muduo 迁移删除,swap-and-pop 步骤和整组流测试一起作废 |
| W1-44 | low | 已被改写 | none | DataService 选点改为 view<NodeInfo, MuduoGrpcChannelPtr> 并要求 kReady;摘节点先同步移除通道组件,不再有「关闭中」窗口 |
| W1-45 | low | 已被改写 | low | 路由服摘除时 ~Channel 把在途调用以 UNAVAILABLE 回调,OnRouterNodeRemoved 已经多余;lane 注释仍写「永远不会再有回调」,lateDropped 会把每个被摘调用再计一次 |
| W1-46 | low | 已被改写 | none | 停机和摘节点的 5ms gRPC 排空轮询已删除；剩下的 FinalizeTeardownInLoop 本地完成让轮靠 FIFO 有界收敛，当前没有任何回调会同步重发，不构成可达缺陷 |

「已被改写」：所在代码已被 gRPC→muduo 迁移会话重写（CQ、Stub、GrpcEtcdTransport 已删除），原问题不再以原形式存在。

### 8.2 第二波查找结果（未复核）

| 编号 | 严重度 | 置信度 | 位置 | 问题 |
|---|---|---|---|---|
| W2-b1_runtime-01 | high | medium | `cpp/libs/services/scene/combat/skill/system/skill.cpp:926` | 落点时目标已死被当成「缺数据」中止整次施法:效果和后摇都跳过 |
| W2-b1_runtime-02 | medium | high | `cpp/libs/services/scene/combat/skill/system/skill.cpp:661` | 没有前摇相位的技能每次施放都留下一条永不回收的 SkillContext |
| W2-b1_runtime-03 | medium | high | `cpp/libs/services/scene/combat/skill/system/skill.cpp:192` | cooldown_id=0(无冷却)的技能也建冷却表并写 0 号条目,之后每次施放都在热路径打 LOG_ERROR |
| W2-b1_runtime-04 | medium | medium | `cpp/libs/services/scene/combat/skill/system/skill.cpp:376` | CanContinueSkillCast 在同一同步调用链里背靠背重复校验多达 6+N 次,另有预检/提交两份相同检查 |
| W2-b1_runtime-05 | medium | high | `cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:131` | MoveSpeedComp 全仓零读者,却被定成每个 actor 的必需派生组件(创建、重算、缺失报错、测试一应俱全) |
| W2-b1_runtime-06 | medium | medium | `cpp/libs/services/scene/combat/buff/system/buff.cpp:284` | BuffPendingRemoveBuffs 实际只是 RemoveBuff(set) 一次同步调用内的重入标记,却作为 registry 组件反复 emplace/remove |
| W2-b1_runtime-07 | medium | medium | `cpp/libs/services/scene/player/system/player_data_loader.h:35` | 旧快照的背包在派生属性算完之后才恢复,DerivedAttributesComp 漏算装备加成 |
| W2-b1_runtime-08 | medium | medium | `cpp/libs/services/scene/spatial/system/scene_spawn.cpp:101` | 入场写路径仍对持久组件 Transform 用 get_or_emplace(同类还有 RegisterPlayer 对 PlayerSkillListComp) |
| W2-b1_runtime-09 | low | medium | `cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:119` | InitializeOnCreate 为不存在的场景设计成「可重入、投影已有状态、保留脏位」并返回不可达的 bool |
| W2-b1_runtime-10 | low | high | `cpp/libs/services/scene/battle/system/player_battle.cpp:143` | StoreBattleFreezeForWrite 是一行透传,还把三处已确认不存在的 emplace 换成静默复用的 get_or_emplace |
| W2-b1_runtime-11 | low | high | `cpp/libs/services/scene/player/system/player_pet.cpp:965` | PetSystem::BuildBattleSnapshot 的契约写的是「缺组件照常进战斗」,与唯一调用方的 fail-closed 前置检查相反 |
| W2-b2-01 | medium | high | `cpp/libs/engine/thread_context/node_context_manager.h:66` | 节点 registry 里的「全局实体」设施仍在：GetOrCreateGlobalEntity/GetGlobalEntity/SetGlobalEntity 生产零调用，等于换个名字的 GrpcNodeEntity |
| W2-b2-02 | medium | high | `cpp/libs/engine/core/macros/return_define.h:66` | 过渡用的空 TRANSFER_ERROR_MESSAGE 宏零调用，注释却写「生成头仍有调用」，C2 没做 |
| W2-b2-03 | medium | medium | `cpp/libs/engine/core/network/node_utils.cpp:17` | 身份门面：atomic 加返回旧值的 exchange 只服务测试，SameInstanceOnEveryThread 等用例把契约禁止的跨线程读和换主固化下来 |
| W2-b2-04 | medium | high | `cpp/tests/ecs_context_test/ecs_context_test.cpp:36` | ecs_context_test 的会话索引用例对基线实现也全绿，回执里「2 条修复前为红」的证据对当前代码不成立 |
| W2-b2-05 | low | high | `cpp/libs/services/player/rpc/player_service_interface.h:19` | 玩家接口契约仍写「RpcDoneCallback 延后完成同样遵守」，engine 的 rpc_done_callback.h 反向指向 services 规则，挑战意见 [low] 没落实 |
| W2-b2-06 | low | high | `docs/design/player-features-ui.md:22` | 多处文档和注释仍按全局 tip、TLS NodeInfo 描述，C4.3 指名的 player-features-ui.md 没改 |
| W2-b2-07 | low | high | `cpp/tests/aoi_delivery_test/aoi_delivery_test.cpp:185` | C3 没按挑战意见收尾：用例名和注释仍指向已删的 GlobalEntity，末尾断言改成了对旧实现也必过的 actor/scene 检查 |
| W2-b2-08 | low | medium | `proto/common/base/common.proto:38` | NodeInfoListComp 只为 ServiceNodeList 单例而存在，单例删除后成了死 proto 消息 |
| W2-b2-09 | low | medium | `cpp/libs/engine/core/network/node_utils.cpp:45` | SceneManager 选点头注释仍写「只在注册表为空时返回 null、不新增 null 窗口」，与过滤无传输实体后可返回 null 的新实现矛盾 |
| W2-b5_types-01 | low | medium | `cpp/libs/services/scene/player/system/player_scene.cpp:48` | 读 PlayerGuidComp 的逻辑在 6 个文件里各有一份私有 GuidForLog,名为日志却被当身份校验和业务字段用,失效哨兵 0 与 kInvalidGuid 各处不一 |
| W2-b5_types-02 | low | high | `cpp/nodes/scene/handler/rpc/scene_handler.cpp:599` | 已有 PlayerSceneSystem::FindSceneById,scene_id→SceneEntity 的全表扫描仍在两个入口手写 4 份(F095 未收口) |
| W2-b5_types-03 | low | high | `cpp/libs/services/scene/spatial/system/grid.cpp:44` | SceneEntity 类型化后新增的注释声称 try_get 前必须先 valid(),与 EnTT 语义和组件访问规则表矛盾;多处冗余 valid 与复述类型的注释 |
| W2-b5_types-04 | low | high | `cpp/libs/services/scene/combat/buff/comp/buff_comp.h:24` | 四个具名容器组件的 explicit 基类转换构造全仓零调用,只是为它们才不得不写 X() = default |
| W2-b5_types-05 | low | high | `tools/proto_generator/protogen/internal/template/attribute_sync.cpp.tmpl:49` | 模板改成“消费后整体 remove DirtyMaskComp”后,逐字段 dirtyMask.reset 成了死代码,6 个生成文件都带着 |
| W2-b5_types-06 | low | high | `cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:97` | gate 把场景节点 registry 叫 sceneRegistry、节点句柄叫 sceneEntity,与新类型 SceneRegistry/SceneEntity 同名异义(F085 同类漏点) |
| W2-b5_types-07 | low | low | `cpp/libs/engine/thread_context/ecs_context.h:3` | B1 可选的算术类型组件护栏未落地,也没有记录取舍,F032/F099 没有任何机械防复发手段 |
| W2-cross_sem-01 | medium | high | `cpp/nodes/scene/handler/rpc/scene_handler.cpp:549` | Agones 单元计数这个必需步骤仍经 dispatcher 一对一转发,还为此在两个入口各留了一套“监听者删了场景”的回滚分支 |
| W2-cross_sem-02 | medium | medium | `cpp/libs/modules/mission/system/mission.cpp:403` | 任务链自动发奖、自动接后续、完成条件和战斗击杀进度这些必需步骤仍靠 dispatcher 的唯一监听者完成;漏注册时静默丢失,战斗结算还会把这一局永久记成“已应用” |
| W2-cross_sem-03 | low | high | `tools/proto_generator/protogen/internal/generator/cpp/event.go:246` | 白名单落地后,生成器仍为非 Kafka 入口的事件生成并注册 dispatcher handler 空壳,这些订阅永远不会触发 |
| W2-cross_sem-04 | low | high | `cpp/libs/services/scene/spatial/system/scene_spawn.cpp:101` | 入场落位对持久组件 Transform 用 get_or_emplace,违反“持久数据绝不延迟创建” |
| W2-cross_sem-05 | low | high | `cpp/libs/services/scene/battle/system/player_battle.cpp:947` | BuildBattleSnapshot 新加了 all_of 前置校验,后面却原样保留“缺组件就给默认值”的分支,注释也改成了“已确认存在” |
| W2-cross_sem-06 | low | high | `cpp/libs/engine/core/macros/return_define.h:66` | 空的 TRANSFER_ERROR_MESSAGE 过渡宏已无任何调用方,该删没删,注释还说“生成头仍有调用” |
| W2-cross_sem-07 | low | high | `robot/travel_smoke_scenario.go:255` | robot 冒烟场景的注释和失败信息仍在教旧规则“拒绝码必须写 TLS tip”,与第 2 批后“handler 直接写 response”相反 |
| W2-cross_sem-08 | low | high | `cpp/nodes/scene/handler/rpc/scene_handler.cpp:514` | 新增了 PlayerSceneSystem::FindSceneById,但 scene_id 全表扫描仍在 4 个场景入口各手写一份(F095 未收口);s2s 入场还把 CheckEnterScene 做了两遍 |
| W2-cross_sem-09 | low | medium | `cpp/libs/services/scene/player/system/player_lifecycle.cpp:4137` | InitializeOnCreate 返回 [[nodiscard]] bool 只是为了拒绝无效句柄;唯一的生产调用点刚确认过有效,而失败分支既不清 playerList 也不撤会话 |
| W2-cross_sem-10 | low | medium | `cpp/libs/services/scene/player/system/player_feature_snapshot.cpp:191` | 任务列表读路径在持久组件 MissionsContainerComp 缺失时仍回成功,并按配置表下发一整份默认状态,与规则“面板 / 列表缺持久输入返回故障码”冲突 |
| W2-cross_sem-11 | low | medium | `docs/design/ecs-entt-misuse-audit-20261015.md:70` | 审计文档的“已修”和“注意事项”与最终代码矛盾:F044/F050 写的是“actor 初始化时挂上”,而现行规则正好相反 |
| W2-fit-01 | medium | high | `cpp/libs/modules/mission/system/mission.cpp:403` | 任务链（自动发奖、接续任务、推进完成任务条件）仍经 dispatcher 一对一转发，S5 没做；modules 层借事件反调 services 层，模块单测要编译 nodes 文件才能跑通 |
| W2-fit-02 | medium | high | `cpp/nodes/scene/handler/rpc/scene_handler.cpp:512` | S2a 场景目录没做：scene_id→场景实体仍是 5 份全表扫描，其中 FindSceneById 是本次新加的一份；创建与销毁仍在两个入口各写一份，注释要求「手工保持同步」 |
| W2-fit-03 | medium | high | `cpp/nodes/scene/handler/local_event/scene_local_events.cpp:127` | 场景的 Agones 单元计数是必需记账，却经 OnSceneCreated / OnSceneDestroyed 一对一转发；两个 CreateScene 入口为「监听者删掉 SceneInfoComp」写了回滚，而这种监听者并不存在，注释还说监听者包括早已不监听的 AOI 格子表 |
| W2-fit-04 | medium | high | `cpp/libs/services/scene/combat/buff/comp/buff_comp.h:15` | S4 只做了一半：目标侧镜像删了，但目标身上的 BuffEntry 仍持有施法者 SkillContextCompMap 里同一个 shared_ptr，跨实体共享所有权仍在；BuffSource 值类型没做 |
| W2-fit-05 | medium | high | `docs/design/ecs-component-access-rules.md:83` | S0 / S1 都没做：规范里没有「哪些数据进 registry」和「dispatcher 只做可选通知」，反而把「通知后重验句柄与 playerList 归属」写成了通用做法；ECS 用法棘轮脚本不存在 |
| W2-fit-06 | medium | medium | `cpp/libs/engine/core/node/system/node/node_util.cpp:52` | 节点表仍是 registry（S6 第 1 期、第 2 期都没做）：网络资源、握手定时器和 gate 握手状态都挂在节点实体上，所有查找都是对 view 的线性扫描，角色靠组件组合区分 |
| W2-fit-07 | medium | high | `cpp/libs/engine/core/network/rpc_client.h:258` | 传输层的连接回调仍经 ECS dispatcher 转给 Node（唯一监听者），engine 网络层因此依赖场景的线程局部 ECS 上下文；总评说要改成构造注入，后来没有任何会话接手 |
| W2-fit-08 | low | high | `cpp/libs/engine/thread_context/node_context_manager.h:55` | 节点 registry 的「全局实体」三件套已无生产调用方，仍被保留，还加了 Release 下的 FATAL 守卫和三条死亡测试；头部注释仍说 etcd 的 gRPC 状态挂在这个实体上 |
| W2-fit-09 | low | high | `cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:97` | 场景句柄分型之后，gate 仍把 scene 节点的 registry 叫 sceneRegistry、把节点实体叫 sceneEntity，和 scene 进程里的 SceneRegistry / SceneEntity 同名不同义；SceneEntityComp 的字段仍叫 sceneEntity |

第二波内部已知重叠（复核时合并）：cross_sem-06 = b2-02（空 TRANSFER 宏）；cross_sem-04 = b1_runtime-08（Transform 的 get_or_emplace）；cross_sem-08 = b5_types-02，并与 fit-02 相近（scene_id 全表扫描、场景目录）；cross_sem-01 = fit-03，并与 W1-30、W1-36 相关（OnSceneCreated 计数与回滚分支）；cross_sem-02 = fit-01（任务链经 dispatcher）；fit-08 = b2-01（节点 registry 的全局实体）；fit-09 = b5_types-06（gate 的 sceneRegistry 同名异义）。


### 8.3 完成情况（2026-10-16）

本交接列出的工作已全部完成，后续以下列文件为准，本文 §3–§8.2 只作历史记录：

- 正式审查报告：[review-report-20261015.md](review-report-20261015.md)。第二波 47 条已全部复核（W2-b5_types-07 按规则跳过）；第二波唯一的 high（W2-b1_runtime-01）首审与独立二审都降为 medium。逐条复核记录见报告附录 B。
- 给 Codex 的补充任务单：[batch6-review-fixes.md](batch6-review-fixes.md)，含打印实体全部组件的调试工具（T10）。
- 审计文档的 HEAD 状态已更新：[ecs-entt-misuse-audit-20261015.md](../../design/ecs-entt-misuse-audit-20261015.md)。
- 第 7 节的 4 项拍板写进了报告 §4.1；复核另外提出的默认决定与修法冲突，分别见任务单 §2.2 与 §2.3。
