# 第 1 批存盘验证补充与必要修正

> **在途采样稿，不是实施任务单。** 负责人已通知第 1 批只读，等待收尾代理释放文件及正式 `batch1-verify.md`。以下补丁、类型名和测试均为独立审查建议，尚未与最终任务单合并，不得据此同步修改生产文件。输出失败时保留原字节是本审查建议明确的接口契约，并非用户另行指定的实现方式；最终契约由正式任务单收口。统一回执见 [codex-readonly-receipt.md](codex-readonly-receipt.md)。

状态：**已有实现，Codex 逐条审读 diff，再编译、测试；本页列出的补丁尚未实施。** 本页是第 1 批的存盘附录，不是第 2–5 批原方案。代码基线为 `6281e4d27` 及 2026-10-15 工作区在途 diff。下列 `HEAD:` 行号均从 `git grep ... HEAD -- ...` 提取，实施时以函数名重定位，不能把工作区行号当 HEAD 行号。

职责：装卸器将已加载玩家的完整持久状态复制为数据库记录；存盘、退出、跨区交接和回滚快照是调用者。修改原因是缺组件时旧代码会凭空补零并覆盖资产。不变量是**读不创建，缺持久状态则拒绝；拒绝不能被当成已落盘，不能放行跨区或销毁尚未收敛的实体**。

本页只做静态审读和任务细化，没有修改生产代码或执行构建、测试。主会话已尝试串行构建 scene，因其他在途 RPC 变更的 `rpc_status.h:69 HANDLER_FAILED`、`:71 REPLY_DROPPED` 未声明而失败；日志在 `build/ecs-refactor/scene-build.log`。按 AGENTS.md §10.2，不自动重试，不顺手改 RPC，不把本页测试标成通过。

## 一、三类组件在存盘接缝中的判定

| 类别 | 本页涉及的组件 | 创建时机 | 读不到的处理 |
|---|---|---|---|
| 持久数据 | `Transform`、`PlayerUint64Comp`、`PlayerSkillListComp`、`PlayerUint32Comp`、`BaseAttributesComp`、`LevelComp`、`PlayerAttributeComp`、`PlayerPetComp`、`PlayerProfileComp`、`PlayerBagsComp`、`MissionsContainerComp`、`PlayerAssetOpLedgerComp`、`BattleSettlementLedgerComp`、`CurrencyComp`、`PlayerCurrencyComp` | 加载新实体时由 `PlayerDatabaseMessageFieldsUnmarshal` 创建；背包和任务经其专用 Unmarshal 创建 | ERROR + 拒绝完整存盘；输出保持原字节；不补零、不发 DBTask、不发回滚快照、不据此推进交接或销毁 |
| 按需状态 | `PlayerLastPersistedSnapshotComp`、`InBattleComp`、退出意图、交接意图、冻结标记、脏标记 | 对应功能写入口首次写时创建；落盘快照在真实加载/成功保存证据出现时创建；战斗锁恢复属于该功能恢复写入口 | 无落盘快照表示尚无相等证据，必须真实写；无 InBattle 表示未冻结；无退出/交接意图表示没有该流程，不应全员预挂 |
| 派生运行时数据 | `DerivedAttributesComp` | 加载链的 `PlayerAttributeSystem::InitializeOnLoad`，完成后玩家才能交给业务入口 | ERROR + 跳过本次计算/拒绝依赖它的快照，不写入持久化字段 |

注意两个易误判项：`BaseAttributesComp` 虽然字段名叫 `derived_attributes_component`，当前 HP/MP 等会落库，属于持久数据；`PlayerCurrencyComp` 同时含 dirty 和 debts，但 debts 由 `CurrencyComp.debts` 装卸，不能因它是 C++ 结构便把整组件当可随手补建的脏标记。权威分类仍为 `docs/design/ecs-component-access-rules.md`。

## 二、现有 diff 的预期、核对点与缺口

| 文件/函数（HEAD 行） | 工作区已做 | 核对结论/发现问题的修法 |
|---|---|---|
| `player_database_loader.cpp:140` `PlayerDatabaseMessageFieldsMarshal` | 改 bool，先 `FindPersistedComp` 检查 15 个必需组件再 CopyFrom；缺失不改数据库子消息 | 方向正确；测试应对预填非空输出断言完全不变，而非只检查新空对象；加 invalid entity 拒绝再 try_get |
| 同文件 `PlayerDatabaseMessageFieldsUnmarshal`（HEAD:95 左右，函数名定位） | `PlayerCurrencyComp` 由 get_or_emplace 改 emplace | 符合加载所有权；保留 LoadFromProto、技能 sanitize、等级上限、账本校验、属性和宠物初始化等手工增强 |
| `player_database_1_loader.cpp:8` | 返回 bool/true；此表当前没有组件字段 | 保留，未来增加组件时模板必须生成校验 |
| `player_data_loader.h:12` `PlayerAllDataMessageFieldsMarshal` | 两子表返回 bool，失败直接 return false | **未满足保留旧输出**：先 `mutable_*` 已改变 presence，未来第二表失败还会半写第一表；用候选对象成功后 Swap |
| `bag_marshal.h:54`、`mission_marshal.h:7` | 说明缺容器输出“权威空记录”，落盘前由总入口检查 | 现实现不创建，但它们的 void API 不能表达故障；本批保留直接 marshal 语义，禁止绕过完整存盘入口写盘 |
| `player_lifecycle.cpp:743/3890` | `MarshalPlayerForSave` 返回 bool；Save 遇缺组件在 Redis/Kafka 前 return false | **未完整 fail-closed**：false 原来代表已相等/已交接；退出和交接沿 false 成功分支继续。必须区分拒绝与已落盘，见下节 |
| 同文件 HEAD:2209 | Marshal 改 bool 后调用者未检查 | 必须检查，失败保留退出实体与意图，不比较半写 current、不重存后误销毁 |
| `snapshot_system.cpp:34` | marshal 失败返回 0，位于领取 snapshot ID 之前 | 顺序正确；补号段未消耗/未发送的证据，不能只凭 return 0 判断，因为号段本来未就绪也返回 0 |
| 两份 `player_data_loader.*.tmpl` | bool/try_get/总入口返回值同步 | `.h` 仍半写；模板只覆盖 descriptor 字段，不覆盖 `PlayerCurrencyComp` 额外装卸。不要用模板生成物覆盖现行 loader 手工增强 |

## 三、拟议补丁：总输出事务化

涉及 `cpp/libs/services/scene/player/system/player_data_loader.h` 与 `tools/proto_generator/protogen/internal/template/player_data_loader.h.tmpl`。签名不再变化：

```cpp
[[nodiscard]] inline bool PlayerAllDataMessageFieldsMarshal(entt::entity player, PlayerAllData& message);
[[nodiscard]] bool PlayerDatabaseMessageFieldsMarshal(entt::entity player, player_database& message);
[[nodiscard]] bool PlayerDatabase1MessageFieldsMarshal(entt::entity player, player_database_1& message);
```

工作区改前：

```cpp
if (!PlayerDatabaseMessageFieldsMarshal(player, *message.mutable_player_database_data()))
    return false;
if (!PlayerDatabase1MessageFieldsMarshal(player, *message.mutable_player_database_1_data()))
    return false;
message.mutable_bag_data()->CopyFrom(message.player_database_data().bag_component());
message.mutable_quest_data()->CopyFrom(message.player_database_data().mission_component());
return true;
```

应改为（候选由旧对象复制以保留总消息上其他字段和未知字段，成功时只替换本函数负责字段）：

```cpp
PlayerAllData candidate = message;
if (!PlayerDatabaseMessageFieldsMarshal(player, *candidate.mutable_player_database_data()))
    return false;
if (!PlayerDatabase1MessageFieldsMarshal(player, *candidate.mutable_player_database_1_data()))
    return false;
candidate.mutable_bag_data()->CopyFrom(candidate.player_database_data().bag_component());
candidate.mutable_quest_data()->CopyFrom(candidate.player_database_data().mission_component());
message.Swap(&candidate);
return true;
```

模板 `.Entries` 循环必须改为写 `candidate`，最后只做一次 Swap。删除“false 时 message 只写一半，内容无意义”的旧注释，改成“false 时 message 保持原样”。子表继续先收集完整组件，再写任何字段；invalid player 先返回 false，不能先 try_get。不需要给 descriptor 本身新增 proto 字段。

## 四、拟议补丁：保存结果不能再由一个 bool 表达

以下枚举和签名是**建议实现，当前不存在**。为避免破坏 `asset_op_system.cpp`、`player_battle.cpp` 已有 `bool (*)(entt::entity)` 注入点，保留公开 bool facade，只把生命周期内部改成有判别力的结果。放在 `PlayerLifecycleSystem` 私有区（`player_lifecycle.h:1410` 附近）：

```cpp
enum class SaveResult : uint8_t {
    kQueued,
    kAlreadyPersisted,
    kHandoffAlreadyRequested,
    kRejected,
    kCount,
};
static SaveResult SavePlayerToRedisImpl(entt::entity player, bool allowSkipWhenPersisted);
```

公开签名保持：

```cpp
static bool SavePlayerToRedis(entt::entity player);
```

facade 改前/后：

```cpp
// 前
return SavePlayerToRedisImpl(player, true);
// 后：仅回答“是否压入一次写”；不能拿本接口的 false 当成收尾凭据。
return SavePlayerToRedisImpl(player, true) == SaveResult::kQueued;
```

`SavePlayerToRedisImpl` 逐出口替换：invalid entity、无 Guid、marshal 拒绝 → `kRejected`；`requestedAtMs != 0` → `kHandoffAlreadyRequested`；与落盘快照相等 → `kAlreadyPersisted`；进入实际 Redis 保存队列 → `kQueued`。不更改现有 owner_epoch、probe、home_zone、DBTask 世代号、Redis 客户端重试语义。本批不解决 Redis 已排队而 Kafka 单表发送失败的既有跨通道原子性问题，不得把此结果称为“已 DB 落库”。

必须逐个改下面的内部调用者，**禁止枚举强转 bool**：

| `player_lifecycle.cpp` HEAD 行 | 调用点/前代码形状 | 改后必要行为 |
|---|---|---|
| 2209 | `MarshalPlayerForSave(...); payloadCurrent = IsPersistedPayloadCurrent(...)` | `if (!MarshalPlayerForSave(...))` 记录故障、保留实体和退出意图，停止本次收敛判定；真实 landed message 可记录为旧的已落盘快照，不能称当前态已收敛 |
| 2236 | 回调重存 `if (SavePlayerToRedis(...)) return;`，false 无 unsettled 就 Finish | 用内部结果；Queued 等回调；AlreadyPersisted 且无 unsettled 才 Finish；Rejected 保留退出实体；Handoff 按既有撤标记/交接裁决，不把它当相等证据 |
| 2966 | exhausted rekick 的 `!Save... && !HasUnsettled...` | 仅 AlreadyPersisted + 无 unsettled 才 Finish；Rejected 仍保留实体与退出标记，允许修复/重新加载/停机有界 drain 裁决 |
| 3027 / 3039 | 首次退出的普通/强制保存 | Rejected 不能 Finish，保留已有退出意图；强制写拒绝不能等待一条永远不会到的回调来假装会自动恢复，ERROR 明确故障；不得发疏散/改派 |
| 3890 | 保存本体 Marshal | 失败在 probe、stats、Redis Save、Kafka DBTask 之前返回 Rejected，保留已有落盘快照 |
| 4060 | stale-epoch rejection、仍是合法持有者时补存 | Rejected 必须保留实体，不能经“无 unsettled”销毁；真正废黜分支仍可按原所有权裁决销毁，与缺组件拒绝分开 |
| 4352 / 4366 | StartTravelHandoff 普通/强制保存 | Rejected → `AbortTravelHandoff`（标记未发送，可安全摘冻结/交接意图），回本次失败；绝不能 BeginTravelHandoff；Queued 才挂看门狗；AlreadyPersisted 且无 unsettled 才 Begin |
| 5259 | 回滚采纳后的强制保存 | 明确判断 Queued；Rejected 保留恢复后的本地实体并报错，不宣称“按构造不可达/不会丢数据”；不得发起下一次交接直到补全加载不变量 |

`StartTravelHandoff` 还应在其“从这里开始改状态”之前对完整持久状态做无副作用的 marshal 预检（函数已有 playerId），拒绝返回现有故障 tip。保存提交点仍复核，预检不能代替保存结果。这样既阻止挂冻结前的常见失败，又覆盖 Detach/事件链在预检之后破坏状态的情况。不要为本功能新增一个全局“缺组件”缓存去替代正式加载状态。

## 五、完整调用点清单（避免编过但测试夹具偷偷用空记录）

下面是 HEAD 中所有正式 marshal 调用，不含定义。生产调用只有 snapshot、保存 helper 与该 helper 的保存/退出比较入口；其他都是测试。

| 文件 | HEAD 行 | 必须处理 |
|---|---|---|
| `cpp/libs/modules/snapshot/snapshot_system.cpp` | 34 | 已检查 bool，保留 marshal → 序列化 → 领号 → Kafka 顺序 |
| `cpp/libs/services/scene/player/system/player_lifecycle.cpp` | 745、2209、3890 | helper、异步回调比较、保存入口，见上表 |
| `cpp/tests/bag_test/player_feature_persistence_test.cpp` | 182、207、238、252、262、288、298、316、331、393、448 | 工作区多数已 `ASSERT_TRUE`；每个成功夹具必须完整加载，拒绝用例必须填旧输出再比较序列化字节 |
| `cpp/tests/bag_test/player_equip_attribute_test.cpp` | 1644、2379 | 两处仍忽略 bool；改 `ASSERT_TRUE`，保证 fixtures 含新要求的持久组件，不允许以空存盘继续下一断言 |
| `cpp/tests/cross_zone_test/cross_zone_test.cpp` | 890、4480 | `MarshalAsSaved` 不能忽略结果；返回 bool+out 并由测试 `ASSERT_TRUE`，或用 `ADD_FAILURE` 后立即停止该 helper，不能只 EXPECT 失败后继续造“已保存”记录 |
| `tools/proto_generator/protogen/internal/template/player_data_loader.h.tmpl` | 11 起 | 模板的每个子表调用检查 bool，候选对象最后提交 |

`cross_zone_test` 的 `MakeExitingPlayer`（HEAD:862 附近）和 `MakeOnlinePlayer`（HEAD:1001 附近）现在只有 Guid/Session 等，过去靠 get_or_emplace 隐式补齐，必须显式恢复完整持久状态。推荐在现有 helper 中调用 `PlayerAllDataMessageFieldsUnMarshal` 加载一份测试消息，然后再配置会话/退出意图，组件字段使用可辨识非零值。不要调用完整 `InitPlayerFromAllData` 只为绕过夹具缺项，除非该用例本来就在测试加载全链（否则引入额外网络与 dispatcher 副作用）。

## 六、相邻已实施改动的审查边界

这些是第 1 批现有 diff，应并入该批核对，不能因为 F059 修了就省略：

| 文件 | HEAD 函数/行 | 工作区预期与必须守住的事 |
|---|---|---|
| `currency_system.cpp` | AddCurrency:75、DeductCurrency:203 | 已新增 `PlayerCurrencyComp* FindRequiredPlayerCurrencyComp(entt::entity, const char*)`；任何余额/债务/流水前拒绝缺债务组件；保持冻结、封禁、参数错误优先级；AttachDebt:304、AdjustDebt:389、FreezeDebt:480 等已有 try_get 路径不得漏查 |
| `player_attribute.cpp` | EnsureComp:571、InitializeOnLoad:614、Recalculate:650、BuildPanel:761、Allocate:847、Reset:886、AutoAllocate:922、CreateScheme:982、SwitchScheme:1015、RenameScheme:1038、GmSetLevel:1056、PushPanel:1076 | 已拆 `PlayerAttributeComp* FindComp(entt::entity)` 与加载期 `NormalizeSchemesOnLoad(PlayerAttributeComp&)`；Derived 只在加载创建，Recalculate 三组件齐才修改；缺持久组件应拒绝，不是正常空面板。BuildPanel 返回 void、RPC 成功空面板仍需主批核对错误传播 |
| `player_pet.cpp` | EnsureComp:211、InitializeOnLoad:423、RecalculateAll:440、BuildList:458、Summon:478、Recall:503、Allocate:517、Reset:561、AutoAllocate:601、Rename:652、GrantPet:689、RemovePetForTrade:758、RestorePetFromSnapshot:793、BuildBattleSnapshot:867、ApplyBattleSettlement:914、PushList:943 | 已改 `PlayerPetComp* FindComp(entt::entity)`，不再补建持久宠物；读列表/战斗快照当前仍将“缺组件”折叠为空/无宠物，主批必须核对这是否违反用户要求的拒绝处理。尤其缺宠物不能静默丢结算数据仍写幂等账本 |
| `player_battle.cpp` | SettlementLedgerOf:324、ack:427附近、RebuildBattleFreezeFromLock:1440、RestoreBattleFreezeOnLogin:1527、ApplySettlementToEntity:1613 | 已改 `BattleSettlementLedgerComp* FindSettlementLedger(entt::entity, uint64_t, uint64_t, const char*)`；缺账本不应用、不销账、不补建；记录前重取避免指针跨事件。若副作用中本实体被拆，当前代码仅记 ERROR，仍返回 true，不能把它声称为持久幂等完整保证 |

这些接口之外的客户端 handler 必须一并纳入第 1 批审查：`player_attribute_handler.cpp` HEAD:34 `GetAttributePanel`、`player_pet_handler.cpp` HEAD:34 `GetPetList` 均当前调用 void builder、不填故障码。若正式批次选择把 builder 改为 bool/错误码，应逐一改所有成功写后附带 builder 的调用者，不只改这两个 Get。

## 七、确定性回归用例与红因

复用现有 gtest 宿主，不连真实 Redis/Kafka，不用墙钟等待。每个创建的实体、SessionMap、dispatcher sink 必须由 fixture 清理。生产副作用若需要 fake，优先复用现有 Redis 客户端测试访问与 Kafka 计数接缝；不能靠“空客户端崩溃就算证明没发送”。

| 用例名（新增/加强） | 夹具、断言 | 修复前为何红 |
|---|---|---|
| `PlayerFeaturePersistenceTest.MarshalRefusesWhenAnyPersistedComponentIsMissing`（加强） | `StockedPlayer` + 完整加载组件，每次摘 15 类之一；`saved` 预填 player_id、非零 currency、旧账本和顶层 bag/quest，保存序列化字节；EXPECT_FALSE；字节完全一致；组件仍不存在 | HEAD 会补建并答成功；在途 wrapper 会在旧输出没有该子消息时改变 presence；原测试只断子消息 ByteSize=0，未覆盖旧输出 |
| `PlayerFeaturePersistenceTest.MarshalInvalidEntityPreservesExistingOutput` | create/destroy 后用失效句柄、另测 entt::null；输出预填；拒绝、不崩溃、输出不变 | 原路径无 valid 守卫，可能断言/非法访问 |
| `PlayerFeaturePersistenceTest.SaveRefusesIncompletePlayerBeforeTouchingRedis`（加强） | 保留现有用例；先挂一份有内容的 `PlayerLastPersistedSnapshotComp`，摘账本；保存后相同指针/字节仍在，Redis保存/DBTask计数均0，缺组件没补回 | HEAD 补零后继续 Redis；在途只证明没新建快照，没有证明旧快照保留 |
| `ExitPersistEcs.MissingPersistedComponentDoesNotFinishInline` | 修好 MakeOnlinePlayer 夹具后先 MarkPersisted，再改一项货币、摘账本；调用首次退出；实体/退出意图/playerList 保留，不能 Finish 或发疏散；快照仍为旧字节 | 在途 Save false 无 unsettled → Finish，实体被销毁 |
| `ExitPersistEcs.ExhaustedRekickWithMissingComponentRetainsEntity` | 完整 MakeExitingPlayer、resaveRounds=kMax、已有旧快照、摘账本；调用退出；实体与意图保留，无 Finish | HEAD:2966 把拒绝false当clean |
| `ExitPersistEcs.LandedCallbackWithMissingComponentDoesNotConverge` | 先由完整玩家 marshal 成功得到 landed，再摘 PlayerCurrencyComp；驱动 HandlePlayerAsyncSaved；实体保留、无第二笔保存副作用，不发改派；允许将真实 landed 作为旧落盘证据更新 | 现回调忽略marshal结果，resave false后销毁；这是最容易漏掉的同类入口 |
| `ExitPersistEcs.StaleEpochResaveRejectedRetainsExitingEntity` | 完整退出玩家、currentEpoch 与 rejectedEpoch 不同，摘账本，无 unsettled；调 HandlePlayerSaveRejected；实体/意图保留 | stale拒绝补存false分支仍直接 Finish |
| `TravelPersistEcs.IncompletePlayerNeverPublishesHandoff` | 复用现有带会话/SM可达的 travel 夹具；完整加载后摘账本，保证在纯前置校验后能走到保存分支；请求交接；失败、无 handoff SET/EnterScene/销毁；冻结与travel未创建或已安全撤回；旧快照保留 | 在途缺组件false → BeginTravelHandoff；单靠找不到SM的早退无法验证这个问题 |
| `PlayerFeatureSnapshotTest.RollbackSnapshotOfIncompletePlayerDoesNotCreateComponents`（加强） | 保留已有缺持久组件用例；额外提供一段可用 snapshot ID，记录下一号前后与发送计数；捕获返回0，号段未动，发送0，组件未补回 | HEAD marshal补建零值并消耗号/发送；只用未初始化号段时新旧都可能返回0 |
| `PlayerDataLoaderTemplateTest.MarshalUsesCandidateAndRejectsMissingComponents`（拟新增 Go） | 在 `internal/generator/cpp/player_data_test.go` 构造两表descriptor字段，直接执行两个模板到t.TempDir；检查bool签名、全部 try_get 校验、无 get_or_emplace、总对象只最后Swap，第二表失败在提交之前 | HEAD模板void/get_or_emplace；在途模板仍直接mutable调用方输出 |

现有已写用例需保留：currency 的 `AddCurrencyMissingPlayerCurrencyCompFailsClosed`、`DeductCurrencyMissingPlayerCurrencyCompFailsClosed`；battle 的 `MissingLedgerRejectsSettlementWithoutSideEffects`、`MissingLedgerAckDoesNotFabricateLedger`；属性和宠物的 ownership 用例。它们的通过不等于退出/交接调用链通过。

## 八、模板输出边界和执行次序

`tools/proto_generator/protogen/internal/generator/cpp/player_data.go:164` 通过模板生成 `.cpp`，配置 `proto_gen.yaml:40` 将它写进临时 storage 目录；`:44` 的头文件生成则按 `:77` 的 `player_data_loader_file` 直接写正式头。生成模板当前没有 PlayerCurrencyComp 的额外 Load/Save、技能 sanitize、等级夹紧、属性/宝宝初始化与账本校验。**本批不要把临时 `.cpp` 复制覆盖正式 `.cpp`**。仅改模板并对临时产物做差异审读，验证通用只读契约；若要全面改生成/手写扩展边界，另在任务单列为单独设计，不偷偷夹带。

1. 先补失败回归及完整 fixtures，明确哪些红来自真实行为，哪些是旧夹具靠 marshal 补组件。
2. 两份模板/正式总 loader 一起实现候选提交；检查所有调用 bool 的地方。
3. 生命周期文件交接之后，串行实现 SaveResult 与所有内部结果分支；保存 helper 失败检查一并完成。
4. 补 snapshot 号段/发送证据；修正第 1 批相邻入口中的缺持久组件成功空输出问题。
5. 独立复核者用上述 HEAD 清单和新 `git grep` 查调用遗漏，最后才进入受影响构建/测试。

可并行：模板及 Go 测试、snapshot 及其测试、currency/pet/attribute 静态核对可由不同执行者负责。不可并行写：`player_lifecycle.{h,cpp}` 必须等“登录代次/玩家生命周期”会话确认文件定稿后接手；`cross_zone_test.cpp` 同步归同一个执行者；全量 proto 生成必须与“RPC/muduo HTTP/2”会话协调，不能覆盖它尚未同步的生成物。没有收到文件释放信号不代表自动允许覆盖；文档可先完成。

## 九、待执行验证命令与通过标准

全部从仓库根执行，C++ MSBuild 串行 `/m:1`，失败保留首错和日志并停止；当前已有构建阻塞解除之前不重复运行。本批不要求压测、不清真实 DB/Redis、不上传日志、不 commit/push/tag。

```powershell
git diff --check
git grep -n -E 'Player(AllData|Database1?)MessageFieldsMarshal|MarshalPlayerForSave' -- cpp tools/proto_generator
```

通过：无 whitespace 错；所有调用者显式处理失败；存盘函数不含持久组件 get_or_emplace；未出现客户端仓 diff。`[[nodiscard]]` 用于把今后遗漏暴露到编译，不能用 `(void)` 绕过。

生成器：工作目录 `tools/proto_generator/protogen`，命令 `go test -count=1 ./internal/generator/cpp/...`；通过标准包含新增模板用例与原有 handler 保留测试。只为模板测试不必完整重生成 proto；确需生成时按 AGENTS §4 使用 no-unity 配置，并先协调其他在途工作。

```powershell
msbuild cpp/libs/services/scene/scene.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
msbuild cpp/libs/modules/modules.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
pwsh -File tools/scripts/run_cpp_tests.ps1 -Build -Filter bag_test
pwsh -File tools/scripts/run_cpp_tests.ps1 -Build -Filter currency_test
pwsh -File tools/scripts/run_cpp_tests.ps1 -Build -Filter cross_zone_test
```

以上五步顺序运行，每步退出码0、没有编译错误，gtest 输出有明确 PASS 汇总。快速定位可直接运行 `build/cpp/tests/bag_test.exe --gtest_filter=PlayerFeaturePersistenceTest.*:PlayerFeatureSnapshotTest.*:PlayerBattleSettlementTest.*` 和 `build/cpp/tests/cross_zone_test.exe --gtest_filter=ExitPersist*:TravelPersist*`；最终应跑这三个工程全量，避免新 strict 规则破坏其他完整加载夹具。不要把脚本 `-Filter` 误当成 gtest 用例过滤，它过滤的是工程名。

失败保留：MSBuild 第一批错误的文件与行、gtest失败段、`[PlayerSave]`/`[ZoneTravel]`/退出日志及实际命令。没有 Redis/Kafka fake 计数接缝时，发送零次仍属未验证，不能仅凭静态早退注释升级为运行证据。

Java 状态：本补充当前仅细化 C++ ECS 内部生命周期修正，未改客户端契约。Java 不使用 entt，但“保存被拒不能当成已落盘、不能推进所有权交接”是共同数据安全约束；本页未核对 Java 对应行为，交付须列为待核对，不能写“不适用且已验证”。未执行 Java 构建、未改 PARITY。
