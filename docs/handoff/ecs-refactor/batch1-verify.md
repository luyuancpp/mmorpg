# Codex 任务单：第 1 批核验与收尾（2026-10-15）

> **谁执行**：Codex。总规则、三类初始化口径、协调与回执格式见 [README.md](README.md)。
> **现状**：第 1 批代码已由 Claude 的工作流写进主工作区，**未编译、未测试**。每组都走过「实施 → 对抗审查 → 修正」，6 组的文件互不重叠。
> **你要做的**：
> 1. 逐组审读 diff（`git diff -- <§附录各组列出的文件>`）。工作区里混有别的会话的改动，只看与本组目标相关的部分。
> 2. 按 §1 的收尾清单补完代码。
> 3. 按 §2 的顺序编译、测试。
> 4. 按 README §6 交回执。

## 1. 收尾清单（必须做）

### 1.1 合入门禁：补齐测试夹具（阻塞）

存盘 Marshal 现在只读、遇缺即拒。下面这些手搭的玩家夹具缺持久化组件，以前靠 `get_or_emplace` 被动补齐，现在会变红或「假绿」（走拒存路径，测不到原本要测的分支）。补齐写法照 `cpp/tests/bag_test/player_feature_persistence_test.cpp` 的 `CompletePersistedComps`。15 个持久化组件是：Transform、PlayerUint64Comp、PlayerSkillListComp、PlayerUint32Comp、BaseAttributesComp、LevelComp、PlayerAttributeComp、PlayerPetComp、PlayerProfileComp、PlayerBagsComp、MissionsContainerComp、PlayerAssetOpLedgerComp、BattleSettlementLedgerComp、CurrencyComp、PlayerCurrencyComp。

- `cpp/tests/bag_test/bag_test.cpp`：`GuidSourcePolicyTest.SnapshotIsSkippedUntilItsOwnKindHasARange`（约 3825 行），在 `emplace<Guid>` 之后补齐。
- `cpp/tests/cross_zone_test/cross_zone_test.cpp`：
  - `CrossZoneBagMarshal.EquipInstanceDataSurvivesThePlayerAllDataHop`（约 4480 行）补齐，Marshal 包上 `ASSERT_TRUE`。
  - `MakeExitingPlayer`（约 864 行）、`MakeOnlinePlayer`（约 1001 行）补齐。
  - `MarshalAsSaved`（约 887 行）对返回值加 `EXPECT_TRUE`。
  - 补完后，`ExitPersistEcs.*`、`ExitReleaseEcs.*` 的收敛分支和脏比较快路径才重新被覆盖。
- `cpp/tests/bag_test/player_equip_attribute_test.cpp`：另一会话已补 `CompletePersistedComps`。第 1696、2433 行 Marshal 的返回值包上 `ASSERT_TRUE`。

### 1.2 存盘结果改为三态（审查 medium，正确性）

`SavePlayerToRedisImpl` 的「拒存」与「盘上已是同一份」都返回 `false`，调用方分不出：
- 交接会照常写 handoff 标记，此时骨架不变量 I1「SET 发出 ⇒ Redis ⊇ 冻结内存」不成立；
- 退出会打 `already persisted` / `converged` 并收尾。

改法：
1. 在 `player_lifecycle.h` 新增 `enum class SaveOutcome : uint8_t { kWritten, kAlreadyPersisted, kHandoffRequested, kRefused };`，`SavePlayerToRedisImpl` 返回它。
2. `bool SavePlayerToRedis` 保留为薄包装，这样 `asset_op_system.cpp` 和 `player_battle.cpp` 的 `PersistFn` 函数指针不用改。
3. 逐个调用点分支：
   - `StartTravelHandoff`（约 4395 行）：`kRefused` 时改为 `AbortTravelHandoff(playerId, "handoff save refused: persisted component missing")` 并 `return kTravelAccepted`，**不调** `BeginTravelHandoff`。
   - `HandleExitGameNode`（约 3033 / 3045 行）、exhausted re-kick（约 2972 行）、`HandlePlayerSaveRejected`（约 4103 行）：`kRefused` 时打 ERROR 写明真实原因，再 `FinishExitAfterPersist`。
   - `HandlePlayerAsyncSaved`：约 2215 行的收敛比对检查 `MarshalPlayerForSave` 的返回值，失败显式打 ERROR；约 2242–2257 行重存收到 `kRefused` 时，不再打 `converged on re-save`。
   - B5 RollbackAdopt（约 5302 行）：ERROR 文案补上「拒存」原因。
4. 同步更新 `player_lifecycle.h` 中 `SavePlayerToRedis`（约 1073–1082 行）、`SavePlayerToRedisImpl`（约 1400–1410 行）的返回值说明。
5. 低基数计数：在 `dirty_save_stats.h` 加 `save_marshal_refused`（不带 player_id label），拒存分支自增，由 `redis.cpp` 每 30s 的汇总行输出。
6. `asset_op_system.cpp` 的 `PersistAndProbeDurable`：拒存时的日志原因改为「拒存」。

先写回归测试：交接时拒存不写 handoff 标记、退出时拒存打出正确原因。

### 1.3 按三类初始化规则收口（负责人确认的口径，见 README §2）

第 1 批是在规则细化之前写的，当时的口径是「必需组件只在创建点挂」。请逐处核对 actor、combat 两组的 diff：凡是**按需状态**组件（冷却表、技能上下文、AOI 列表、施法定时器、属性脏标记、战斗状态集合等），必须在**该功能唯一的写入口**延迟创建，读路径 `try_get` 拿不到就当「空 / 未激活」。不能被改成「actor 初始化时一律挂上」，也不能变成「写入口也 `try_get`，于是没人创建」。
- 发现被改成创建时一律挂上的：改回写入口延迟创建，入口加注释「此处延迟创建」。
- 发现读路径拿不到时报错拒绝的：除非该组件属于持久数据或派生数据，否则改为按空处理。
- spatial 组已核对：AOI 列表在「第一次进入格子」时延迟创建，符合规则。场景格子表在场景创建时挂上，可以保留。

### 1.4 其他需列表外改动的项

见附录各组的「需要列表外改动」。其中涉及 `client_message_processor.cpp`、`service_discovery_manager.cpp` 的节点类型校验，**等 RPC 链式调用会话落地后再做**（见 README §4）。

## 2. 编译与测试顺序

全部在仓库根执行，Debug|x64，MSBuild 一律 `/m:1` 串行。**不要** stash / reset / checkout。失败后不连续重试，保留首个 error 和 gtest 输出。

1. `msbuild cpp\libs\engine\thread_context\thread_context.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64`（工程名以 game.sln 为准）
2. `msbuild cpp\libs\modules\modules.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64`（/WX，必须 0 warning）
3. `msbuild cpp\libs\services\scene\scene.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64`（/WX）
4. `msbuild cpp\nodes\scene\scene.vcxproj /m:1 ...`，`msbuild cpp\nodes\gate\gate.vcxproj /m:1 ...`
5. 测试，`pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter <工程>`，依次：`bag_test`、`cross_zone_test`、`aoi_test`、`aoi_delivery_test`、`skill_test`、`buff_test`、`cool_down_time_test`、`currency_test`、`routing_identity_test`。各组新增的用例名见附录。
6. 生成器：`cd tools/proto_generator/protogen; go build ./...; go vet ./internal/generator/cpp/...`。**不要**为验证去跑完整生成器：它会把 `player_data_loader.h` 写回仓库，与其他会话冲突。

各组代理给出的逐组命令与预期见附录。若与本节冲突，以本节为准。

---

## 附录：各组交付原文（代理自述，Codex 以 diff 为准核对）

### 组：存盘与快照只读化（save）

**修了**：F059、F060、F070、F073

**改动**：
- `cpp/libs/services/scene/player/system/player_database_loader.cpp`：新增匿名命名空间模板 FindPersistedComp：只用 try_get，缺失时把组件名记下。PlayerDatabaseMessageFieldsMarshal 改为返回 bool：先检查 15 个持久化组件（含 PlayerBagsComp、MissionsContainerComp），缺任何一个就 LOG_ERROR（实体号、player_id、缺失列表）并返回 false，不写 message、不补建组件；全部在场才 CopyFrom，currency 的 CopyFrom / SaveToProto 顺序保持不变。Unmarshal 中 PlayerCurrencyComp 改为 emplace，表达唯一创建点。补了 <string>、player_bags_comp.h、mission_comp.h 三个头文件。
- `cpp/libs/services/scene/player/system/player_database_1_loader.cpp`：PlayerDatabase1MessageFieldsMarshal 改为返回 bool；这张子表目前没有持久化组件，恒返回 true。
- `cpp/libs/services/scene/player/system/player_data_loader.h`：两个子表的 Marshal 声明改为返回 bool；PlayerAllDataMessageFieldsMarshal 改为返回 bool，任一子表失败立即返回 false，不再写顶层兼容镜像。注释写明 false 时 message 必须整份丢弃。内容与新模板的输出一致。
- `tools/proto_generator/protogen/internal/template/player_data_loader.cpp.tmpl`：生成的 Marshal 改为返回 bool：先对每个字段的组件调用 FindPersistedComp（BagAllData 查 PlayerBagsComp，QuestAllData 查 MissionsContainerComp），缺失时 LOG_ERROR 并返回 false，再按指针 CopyFrom，去掉 get_or_emplace。补了 <string>、Logging、背包与任务组件头文件。
- `tools/proto_generator/protogen/internal/template/player_data_loader.h.tmpl`：与手改的 player_data_loader.h 同步：Marshal 声明返回 bool，PlayerAllDataMessageFieldsMarshal 返回 bool 并逐表检查。这个 header 由生成器直接写进仓库，两边必须一致。
- `cpp/libs/services/scene/player/system/player_lifecycle.cpp`：只改了三个函数体。MarshalPlayerForSave：改为返回 bool，失败时不补 player_id。InitPlayerFromAllData：先用 playerList.contains 查重再 create（F070）；首登两处 get_or_emplace 改为 get。SavePlayerToRedisImpl：try_get<Guid> 守卫，缺失或无效时 ERROR 并返回 false（F073）；MarshalPlayerForSave 失败时 fail-closed 返回 false，不写 Redis、不发 DBTask、不计 dirty_save_stats，注释写明各调用方对 false 的收尾语义以及为什么不重试。
- `cpp/libs/modules/snapshot/snapshot_system.cpp`：CaptureAndSend 检查 PlayerAllDataMessageFieldsMarshal 的返回值：失败时 LOG_ERROR 并返回 0，不发快照。检查在领 snapshot_id 之前，失败不消耗号段。
- `cpp/libs/modules/snapshot/snapshot_system.h`：注释补充：只读，持久化组件缺失时返回 0。
- `cpp/libs/services/scene/player/system/bag_marshal.h`：只改注释：Marshal 只读；它的「空记录」语义不适用于存盘，存盘必须走 PlayerDatabaseMessageFieldsMarshal 入口。
- `cpp/libs/services/scene/player/system/mission_marshal.h`：只改注释：Marshal 只读；缺组件时输出的是「权威空记录」，存盘入口会先拦住。
- `cpp/tests/bag_test/player_feature_persistence_test.cpp`：新增 EmplaceIfAbsent / CompletePersistedComps 测试替身：把手搭的实体补成「加载创建点走完」的形状。StockedPlayer 和装备往返用例的玩家都补齐了组件。原有 Marshal 调用全部包上 ASSERT_TRUE。新增 fixture 模板 ExpectMarshalRefusedWithout，以及 4 个用例（见 tests_added）。
- `cpp/tests/bag_test/player_feature_snapshot_test.cpp`：新增用例 RollbackSnapshotOfIncompletePlayerDoesNotCreateComponents，补了 snapshot_system.h、actor_comp.pb.h、asset_op_ledger_comp.pb.h 三个头文件。
- （修正轮）`cpp/libs/services/scene/player/system/player_lifecycle.cpp`：只改了三个函数。
- 本轮：SavePlayerToRedisImpl 拒存分支的注释改为如实描述。拒存与快路径同为 false，调用方分不出；退出和交接会按快路径收尾，交接时 I1 不成立；数据结局等同于上次落盘后宕机；根治要三态返回，改动在本函数之外。删掉了「最多一条 ERROR」的错误说法。新增 LOG_ERROR "[SavePlayerToRedis] Save refused ..."，带 entity / player_id / allow_skip_when_persisted / exiting / handoff_pending。
- 前轮，保留：MarshalPlayerForSave 改为返回 bool；InitPlayerFromAllData 先查重再 create，首登两处 get_or_emplace 改为 get；SavePlayerToRedisImpl 加 try_get<Guid> 守卫，marshal 失败时 fail-closed 返回 false。
- （修正轮）`cpp/tests/bag_test/player_feature_persistence_test.cpp`：- 本轮：SaveRefusesIncompletePlayerBeforeTouchingRedis 改为先预置一份 PlayerLastPersistedSnapshotComp，内容是完整玩家 marshal 的结果加上两张子表的 player_id，然后再摘掉账本。若有人把 get_or_emplace 改回来，旧写法会走脏比较快路径、不碰 Redis，用例在「账本被补建」断言上干净地失败，不会让进程崩溃。去掉了原来那条「无落盘快照」断言：它本来就恒真，测不到东西。
- 前轮，保留：CompletePersistedComps 测试替身，Marshal 调用包上 ASSERT_TRUE，以及 MarshalRefusedWithout×15、SaveWithoutGuidIsRefused、InitFromAllDataRejectsDuplicateWithoutLeavingOrphanEntity 三个用例。
- （修正轮）`cpp/libs/services/scene/player/system/player_database_loader.cpp`：前轮，本轮复核后未改：
- 新增 FindPersistedComp，只用 try_get。
- Marshal 改为返回 bool：先确认 15 个组件齐全，缺失就 ERROR + false，不写 message、不补建组件。
- Unmarshal 里 PlayerCurrencyComp 改为 emplace，作为唯一创建点。
- （修正轮）`cpp/libs/services/scene/player/system/player_database_1_loader.cpp`：前轮，未改：Marshal 改为返回 bool。这张子表没有持久化组件，恒返回 true。
- （修正轮）`cpp/libs/services/scene/player/system/player_data_loader.h`：前轮，未改：两个 Marshal 声明和 PlayerAllDataMessageFieldsMarshal 都改为返回 bool，任一子表失败立即返回 false。内容与 h.tmpl 的输出逐行一致，已核对。
- （修正轮）`tools/proto_generator/protogen/internal/template/player_data_loader.cpp.tmpl`：前轮，未改：生成的 Marshal 先对每个字段调用 FindPersistedComp 校验，缺失时 ERROR 并返回 false，再按指针 CopyFrom，去掉了 get_or_emplace。
- （修正轮）`tools/proto_generator/protogen/internal/template/player_data_loader.h.tmpl`：前轮，未改：与 player_data_loader.h 同步，Marshal 返回 bool。
- （修正轮）`cpp/libs/modules/snapshot/snapshot_system.cpp`：前轮，未改：在领 snapshot_id 之前检查 marshal 结果，失败时 ERROR 并返回 0。
- （修正轮）`cpp/libs/modules/snapshot/snapshot_system.h`：前轮，未改：注释写明只读，以及持久化组件缺失时返回 0。
- （修正轮）`cpp/libs/services/scene/player/system/bag_marshal.h`：前轮，未改：只改注释。写明 Marshal 只读，「空记录」语义不适用于存盘。
- （修正轮）`cpp/libs/services/scene/player/system/mission_marshal.h`：前轮，未改：只改注释。写明 Marshal 只读，缺组件时输出的是「权威空记录」，存盘入口会先拦住。
- （修正轮）`cpp/tests/bag_test/player_feature_snapshot_test.cpp`：前轮，未改：新增 RollbackSnapshotOfIncompletePlayerDoesNotCreateComponents，验证只有 Guid 的玩家快照返回 0，且不会被补建任何组件。

**新增测试**：
- `cpp/tests/bag_test/player_feature_persistence_test.cpp` `PlayerFeaturePersistenceTest.MarshalRefusesWhenAnyPersistedComponentIsMissing`：修复前为什么红：每次从完整玩家上摘掉 15 个持久化组件中的一个。修复前 Marshal 返回 void，用例直接编译不过。只看副作用断言也会红：旧代码对 13 个普通组件 get_or_emplace，被摘的组件会被重新造出来（all_of 为真）；15 种情况都会写出非空的 player_database（ByteSizeLong > 0），零值货币、空账本照样写出去。
- `cpp/tests/bag_test/player_feature_persistence_test.cpp` `PlayerFeaturePersistenceTest.SaveRefusesIncompletePlayerBeforeTouchingRedis`：修复前为什么红：玩家缺 PlayerAssetOpLedgerComp 时调用 SavePlayerToRedis。修复前 get_or_emplace 补出空账本，marshal「成功」后一路走到 tlsRedisSystem.GetPlayerDataRedis()->Save。单测宿主里这个客户端是空指针，进程直接崩溃（红）。修复后在 marshal 处返回 false，不补建账本，也没有落盘快照。
- `cpp/tests/bag_test/player_feature_persistence_test.cpp` `PlayerFeaturePersistenceTest.SaveWithoutGuidIsRefused`：修复前为什么红：实体只缺 Guid，其余持久化组件齐全。修复前 SavePlayerToRedisImpl 对缺失组件无守卫 get<Guid>，Debug 下 ENTT_ASSERT 失败、进程中止（红）。修复后 try_get 守卫，返回 false。
- `cpp/tests/bag_test/player_feature_persistence_test.cpp` `PlayerFeaturePersistenceTest.InitFromAllDataRejectsDuplicateWithoutLeavingOrphanEntity`：修复前为什么红：先把 kPlayerId 登记进 playerList，再用同一 id 调用 InitPlayerFromAllData。修复前先 create 后查重，返回 null 时多留一个存活实体，storage<entt::entity>().free_list() 比调用前大 1，EXPECT_EQ 失败（红，干净失败，不崩溃）。函数签名没变，可以直接在修复前代码上编译运行。
- `cpp/tests/bag_test/player_feature_snapshot_test.cpp` `PlayerFeatureSnapshotTest.RollbackSnapshotOfIncompletePlayerDoesNotCreateComponents`：修复前为什么红：对只有 Guid 的玩家调用 SnapshotSystem::CaptureAndSend。修复前 marshal 用 get_or_emplace，在实体上造出 Transform / CurrencyComp / PlayerAssetOpLedgerComp，any_of 断言失败（红，干净失败）；返回值前后都是 0（修复前因为号段未就绪或没有 Kafka）。函数签名没变，可以在修复前代码上直接编译。

**对抗审查**：issues
- [medium] `cpp/libs/services/scene/player/system/player_lifecycle.cpp:3910`：SavePlayerToRedisImpl 拒存时（marshal 拒绝，第 3910-3923 行）返回普通的 false。这和脏比较快路径的「盘上已是同一份」（第 3961-3970 行）是同一个返回值，调用方区分不出来。新注释写「各调用方都已按这个含义收尾」，但实际有调用方按「已落盘」处理：
(1) StartTravelHandoff（第 4385-4421 行）：savePending=false 且没有未落地存盘时，直接 BeginTravelHandoff 写交接标记。该标记的契约是骨架 I1「SET 发出 ⇒ Redis ⊇ 冻结内存」，拒存时这条不成立。目标节点会凭标记免存盘过门、读到旧档，源实体随后不存盘销毁。
(2) HandleExitGameNode（第 3056-3070 行）打 "already persisted (dirty-save fast path)" 后进入 FinishExitAfterPersist，写 A1′ 断线释放标记，声明盘上是最终态。
(3) HandlePlayerAsyncSaved 第 2255 行打 "converged on re-save"。
后果等同「上次落盘后宕机」，并且比旧行为（零值落盘）好；但 fail-closed 事件在交接 / 退出链上被当成正常快路径静默吞掉：没有计数，日志写的原因也不对。实施者已在 needs_outside (c) 提到，但本批注释把它写成了「已正确收尾」。
  - 建议：本批内：把第 3914-3921 行注释改成如实描述——交接 / 退出会把拒存当成快路径继续，I1 不成立，需要后续修。然后同批（或紧接着）在函数外修：
- SavePlayerToRedisImpl 改为返回三态或枚举，例如 kWritten / kAlreadyPersisted / kSkippedHandoff / kRefused；
- StartTravelHandoff 收到 kRefused 时 AbortTravelHandoff，不写标记；
- HandleExitGameNode / HandlePlayerSaveRejected / HandlePlayerAsyncSaved 收到 kRefused 时打 ERROR、写明真实原因；
- 同时加一个低基数计数（如 dirty_save_stats 的 save_marshal_refused，不带 player_id label）。
asset_op_system / player_battle 的 PersistFn 只需把 kWritten 映射为 true。
- [medium] `cpp/tests/bag_test/bag_test.cpp:3825`：本批改动单独合入后，列表外的现有测试会变红：
- GuidSourcePolicyTest.SnapshotIsSkippedUntilItsOwnKindHasARange：玩家只有 Guid，CaptureAndSend 现在在 marshal 处就返回 0，号段统计 unavailable/issued/transport.requests 全部变成 0，第 3833-3843 行至少 4 条断言失败。
- cross_zone_test.cpp:4480 的 CrossZoneBagMarshal.EquipInstanceDataSurvivesThePlayerAllDataHop：source 只挂了 PlayerBagsComp，Marshal 被拒，saved 为空，items_size 断言失败。
另外有一批用例会「假绿」：cross_zone_test 的 MakeExitingPlayer / MakeOnlinePlayer（第 864、1001 行）手搭的玩家只有 Guid，MarshalAsSaved（第 887-894 行）忽略返回值。ExitPersistEcs.ExitSessionStillMappedIsDestroyed、LandedPayloadWithProbeStillConverges、FirstExitOnFastPathFinishesInline、ReexitAfterExhaustedRekicksAndConverges 以及 ExitReleaseEcs.ConvergedExit…（第 1428 行起）现在都走「marshal 拒绝 → current 缺 player_id → 判不收敛 → 重存被拒 → 收尾」这条路，结果和原来一样，所以仍然是绿的。但它们原本要测的收敛分支、脏比较快路径已经没有覆盖。
  - 建议：bag_test.cpp 与 cross_zone_test.cpp 要和本批同批落地：
- 在 bag_test.cpp:3829 之后，以及 cross_zone_test 的 MakeExitingPlayer、MakeOnlinePlayer、EquipInstanceDataSurvivesThePlayerAllDataHop 的 source 上，补齐 15 个持久化组件（可复用 player_feature_persistence_test.cpp 的 CompletePersistedComps 写法）；
- MarshalAsSaved 里对 PlayerAllDataMessageFieldsMarshal 加 EXPECT_TRUE / ASSERT_TRUE，避免以后再出现假绿。
在这之前不要单独合入本批。
- [low] `cpp/libs/services/scene/player/system/player_lifecycle.cpp:3919`：注释说「周期存盘:每个存盘周期每名玩家最多一条 ERROR」，与实际不符。每次拒绝都由 PlayerDatabaseMessageFieldsMarshal 打一条 ERROR，而以下调用都会各自再触发一次：资产通道 RequestAssetOpPersist、结算 RequestSettlementPersist、登录 / 下线快照 CaptureAndSend，以及 HandlePlayerAsyncSaved 的收敛比对（只是比对，并不存盘）。那条 ERROR 也不带调用来源（存盘 / 快照 / 比对），排障时分不清是哪条路径拒的。SavePlayerToRedisImpl 拒存分支本身不打日志、不计数，与 AGENTS §11.3「关键路径低基数指标」不符。
  - 建议：在 SavePlayerToRedisImpl 的拒存分支补一条带 entity / playerId / allowSkipWhenPersisted 的 ERROR（或做限频），加一个低基数计数，并删掉「最多一条」的说法。快照侧的 ERROR 已经带 trigger，可以保留。

**修正轮**：第 1 批「存盘与快照只读化」：已逐条核对审查意见并修复，代码未编译、测试未运行，等 Codex 验证。

先复核了前一位实施者的改动，结论如下：
- F059/F060：15 个持久化组件与 Unmarshal 的创建点一一对应。Marshal 先确认组件齐全再写 message，缺任何一个就整份拒绝并返回 false。
- F070：先查重再 create。
- F073：改用 try_get<Guid> 守卫。
- 两个生成器模板与手改的 player_data_loader.h 一致。
以上都没发现问题，保留原样。

本轮在负责范围内补了 3 处：
1. 审查意见 1（medium）的范围内部分：SavePlayerToRedisImpl 拒存分支的注释原来写「各调用方都已按这个含义收尾」，与事实不符，已改成如实描述。
   - 拒存与脏比较快路径同为 false，调用方分不出来。
   - 退出流程会把拒存当成「盘上已是同一份」照常收尾，日志写的却是 already persisted / converged。
   - 交接流程会照常写 handoff 标记，此时骨架 I1「SET 发出 ⇒ Redis ⊇ 冻结内存」不成立。
   - 数据结局等同于「上次成功落盘后宕机」，不会写出零值。
   - 根治需要存盘返回三态，改动在本函数之外。
2. 审查意见 3（low）：删掉了「每周期每名玩家最多一条 ERROR」的错误说法。拒存分支新增一条 ERROR，带 entity / player_id / allow_skip_when_persisted / exiting / handoff_pending，拒存不会再被调用方的快路径日志吞掉。
3. 回归测试 SaveRefusesIncompletePlayerBeforeTouchingRedis 改为预置一份「旧 Marshal 补建空账本后会写出的」落盘快照。以后若有人把 get_or_emplace 改回来，旧写法会走脏比较快路径、不碰 Redis，用例在「账本被补建」这条断言上干净地变红，不会解引用空的 Redis 客户端把整个 bag_test 进程带崩。

还有两件事超出本组可改范围，只能交给后续：
- 审查意见 1 的根治：三态返回值，以及各调用方对「拒存」显式分支。
- 审查意见 2：bag_test.cpp / cross_zone_test.cpp 的夹具要补齐持久化组件。这一条阻塞合入，本批不得单独合入。

player_equip_attribute_test.cpp 已由另一会话补了 CompletePersistedComps，那两个 Reload 用例预计不再变红。

Java 版：本批属于 C++ 版内部 ECS 修复，不涉及客户端契约，未改。
- 未解决：审查意见 1（medium）的根治，需要改本组范围外的函数，未改：
- 涉及：player_lifecycle.h，以及 player_lifecycle.cpp 中不归本组的函数体。
- 改法：新增 enum class SaveOutcome : uint8_t { kWritten, kAlreadyPersisted, kHandoffRequested, kRefused }，由 SavePlayerToRedisImpl 返回，或另加一个 detailed 版本。
- 保持兼容：bool SavePlayerToRedis 保留，作为「== kWritten」的薄包装。这样 asset_op_system.cpp / player_battle.cpp 的 PersistFn 函数指针不用改。
- 需要逐个分支的调用点（行号为当前工作区）：
  (a) StartTravelHandoff 约 4395 行：kRefused 时改为 AbortTravelHandoff(playerId, "handoff save refused: persisted component missing") 并 return kTravelAccepted，不调 BeginTravelHandoff。此时 SET 未发，解冻安全（I0），I1 保持成立。
  (b) HandleExitGameNode 约 3033 行，以及 M4 强制写 3045 行：kRefused 时打 ERROR 写明真实原因，再 FinishExitAfterPersist。实体永远存不了盘，保留只会让退出卡死；盘上是上次完好落盘的那份。
  (c) HandlePlayerAsyncSaved：约 2215 行的收敛比对需要检查 MarshalPlayerForSave 的返回值，失败时显式分支打 ERROR；约 2242–2257 行重存分支收到 kRefused 时，不能再打 "converged on re-save"。
  (d) 约 2972 行的 exhausted re-kick，以及约 4103 行的 HandlePlayerSaveRejected：同 (b)。
  (e) 约 5302 行的 B5 RollbackAdopt：ERROR 文案补上「拒存」这一原因。
- 同时更新 player_lifecycle.h 第 1073–1082 行 SavePlayerToRedis 的返回值说明，以及第 1400–1410 行 SavePlayerToRedisImpl 的返回值说明。
- 建议另立项：存盘被拒的实体每次后续存盘都会被拒，丢失窗口会一直增长到它离开节点。考虑对 kRefused 的在线实体主动踢下线，让它从上次完好落盘的数据重载。
- 未解决：审查意见 2（medium，阻塞合入）：测试夹具在本组范围外，未改。本批不得单独合入，必须与下列夹具修复同批落地。
(1) cpp/tests/bag_test/bag_test.cpp 第 3825–3846 行 GuidSourcePolicyTest.SnapshotIsSkippedUntilItsOwnKindHasARange：玩家只有 Guid，CaptureAndSend 现在在 marshal 处就返回 0，unavailable / issued / transport.requests 的断言会失败。需要在第 3829 行之后补齐 15 个持久化组件，写法照 player_feature_persistence_test.cpp 的 CompletePersistedComps。
(2) cpp/tests/cross_zone_test/cross_zone_test.cpp：
- 第 4480 行 CrossZoneBagMarshal.EquipInstanceDataSurvivesThePlayerAllDataHop：source 只挂了 PlayerBagsComp，会红，需要补齐其余组件，并给 Marshal 加 ASSERT_TRUE。
- MakeExitingPlayer（约 864 行）、MakeOnlinePlayer（约 1001 行）需要补齐组件。MarshalAsSaved（约 887 行）需要对 PlayerAllDataMessageFieldsMarshal 的返回值加 EXPECT_TRUE。否则 ExitPersistEcs.* / ExitReleaseEcs.* / MarkPersisted 相关的交接应答用例会「假绿」：走的是拒存路径，原本要测的收敛分支和脏比较快路径没有覆盖到。
(3) cpp/tests/bag_test/player_equip_attribute_test.cpp：另一会话已补 CompletePersistedComps，但第 1696、2433 行 Marshal 的返回值仍没检查，建议包上 ASSERT_TRUE（low）。
- 未解决：审查意见 3 的计数部分：计数器要加在本组范围外的文件里，未改。
- 在 cpp/libs/services/scene/player/system/dirty_save_stats.h 加一个低基数计数，例如 save_marshal_refused，不带 player_id label。
- 在 SavePlayerToRedisImpl 拒存分支自增。
- 由 cpp/libs/services/scene/core/system/redis.cpp 每 30s 那行汇总输出，可选让 tools/scripts/stress_summarize.ps1 解析。
- 在那之前，告警只能靠 grep "[SavePlayerToRedis] Save refused"。
- 未解决：日志措辞（low，范围外）：cpp/libs/services/scene/player/system/asset_op_system.cpp 的 PersistAndProbeDurable 在拒存时打 "Save skipped by dirty comparison but outcome is absent"，原因写错了，但 durable=false 的结论正确。要等三态返回值落地后才能区分。
- 未解决：文档（范围外）：
- docs/design/ecs-entt-misuse-audit-20261015.md：把 F059 / F060 / F070 / F073 标为「已修（未编译验证）」，并登记三态返回值这项待办。
- docs/PROGRESS.md：追加一条记录。
- 按 AGENTS §12：在 Java 仓库 PARITY.md 登记「C++ 内部 ECS 修复，不涉及客户端契约」，并请 Java 侧核对自己的存盘路径是否也会给缺失的状态补默认值再落盘。
- 未解决：生成器模板只靠人工核对，没有实际渲染。建议在 tools/proto_generator/protogen 新增模板渲染单测（新文件），断言两个 player_data_loader 模板的输出不含 get_or_emplace，且 header 输出与仓库里的 player_data_loader.h 一致。另外，生成版 Marshal 的 ERROR 不带 player_id（手写版带），属于已知的小差异，产物只落临时目录，不影响仓库。
- 未解决：既有问题，不在本批审计条目内：手写的 player_database_loader 从来没有 Unmarshal / Marshal player_database.merge_state（字段 10，PlayerMergeStateComp）。tools/merge_zone 写进去的合服状态，会在 scene 第一次存盘时被整条记录覆盖掉。建议单独立项核对。
- 未解决：全部改动未编译、未运行测试（AGENTS §10.1），等 Codex 按 codex_steps 验证。

**需要列表外改动**：
- cpp/tests/bag_test/bag_test.cpp（约 3825 行，GuidSourcePolicyTest.SnapshotIsSkippedUntilItsOwnKindHasARange）：这个玩家只有 Guid。现在 CaptureAndSend 在 marshal 阶段就返回 0，走不到号段，s.unavailable 会从 1 变 0，用例变红。改法：emplace<Guid> 之后补齐 15 个持久化组件（Transform、PlayerUint64Comp、PlayerSkillListComp、PlayerUint32Comp、BaseAttributesComp、LevelComp、PlayerAttributeComp、PlayerPetComp、PlayerProfileComp、PlayerBagsComp、MissionsContainerComp、PlayerAssetOpLedgerComp、BattleSettlementLedgerComp、CurrencyComp、PlayerCurrencyComp），写法可参照 player_feature_persistence_test.cpp 里的 CompletePersistedComps。
- cpp/tests/bag_test/player_equip_attribute_test.cpp（NewPlayer 夹具，约 868 行）：缺 Transform、PlayerUint64Comp、PlayerSkillListComp、PlayerUint32Comp、PlayerPetComp、PlayerProfileComp、MissionsContainerComp、PlayerAssetOpLedgerComp、BattleSettlementLedgerComp。ReloadFromTheDatabaseRecordRecomputesTheEquipmentBonus 和 ReloadFromTheDatabaseRecordRecomputesEnhanceAndGemBonuses 的 Marshal 会被拒，saved 是空的，用例变红。需要在夹具里或这两个用例的 Marshal 之前补齐，并把 Marshal 调用包上 ASSERT_TRUE。注意另一个会话正在改这个文件，它新增的 NewUnloadedEntity 是刻意做成不完整的，不要动。
- cpp/tests/cross_zone_test/cross_zone_test.cpp：CrossZoneBagMarshal.EquipInstanceDataSurvivesThePlayerAllDataHop（约 4426 行）的 source 只有 PlayerBagsComp，会变红，需要补齐其余持久化组件。另外，MakeExitingPlayer / MakeOnlinePlayer / RegisterNotSentRelocate 等夹具手搭的玩家缺持久化组件，现在仍是绿的，但走的是「marshal 拒绝 → 返回 false」，测不到原本要测的脏比较快路径。建议同样补齐，让 MarkPersisted 和快路径重新被覆盖。
- cpp/libs/services/scene/player/system/player_lifecycle.cpp 中不属于我负责的函数：(a) HandlePlayerAsyncSaved 的退出收敛比对（约 2215 行）没有检查 MarshalPlayerForSave 的返回值。现在失败时一定判「不收敛」，走重存，SavePlayerToRedis 返回 false 后收尾，结果正确，但日志写的是 "converged on re-save"，有误导，应显式分支打 ERROR。(b) HandleExitGameNode / HandlePlayerSaveRejected 在存盘被拒时打的是 "already persisted (dirty-save fast path)"，同样有误导。(c) StartTravelHandoff 在存盘被拒时仍会 BeginTravelHandoff，目标节点载入的是最后一次完好落盘的数据。可以考虑在这种情况下改为中止交接；要做到这一点，需要 SavePlayerToRedis 区分「盘上已是最新」和「拒绝写盘」，例如改为三态返回值，这会牵动 asset_op_system / player_battle 的 PersistFn 函数指针。
- cpp/libs/services/scene/player/system/asset_op_system.cpp 的 PersistAndProbeDurable：存盘被拒时会打 "Save skipped by dirty comparison but outcome is absent"，原因写错了，不过 durable=false 的结论本身是对的。可选：在 dirty_save_stats.h 加一个 save_marshal_refused 计数，供告警使用（低基数，不带 player_id label）。
- docs/design/ecs-entt-misuse-audit-20261015.md：把 F059 / F060 / F070 / F073 的状态改为「已修（未编译验证）」，并补一句：存盘入口 fail-closed，按「上次成功落盘后宕机」的语义收尾。docs/PROGRESS.md：追加一条记录。按 AGENTS §12，在 Java 仓库 PARITY.md 登记「C++ 版内部 ECS 修复、不涉及客户端契约」，并请 Java 侧核对自己的存盘路径是否也会给缺失的状态补默认值再落盘。
- tools/proto_generator/protogen：建议新增一个模板渲染单测（新文件），用 player_database / player_database_1 的样例 DescData / HeaderTemplateInput 渲染两个 player_data_loader 模板，断言输出不含 get_or_emplace，且 header 输出与仓库里的 player_data_loader.h 一致。我按约束没有新建文件。

**风险**：
- 行为变化：生产中如果某个玩家实体缺了任何持久化组件（不变量已被破坏），以后每次存盘、快照都会被拒并打 ERROR，内存中自上次成功落盘以来的改动无法保存。退出或交接照常收尾后，玩家会回到盘上那份完好数据，等同节点在上次落盘后宕机。旧行为是悄悄把零值货币、空账本、空背包写进 Redis 和 DB；新行为把数据丢失收窄为「回退到上次存档」，并且显式可见。目前生产代码里没有任何路径会摘这 15 个组件，这条分支应当不可达。
- 该玩家的资产通道指令和战斗结算会一直报 durable=false，Go 侧会按自身退避持续重查，直到玩家离开节点，期间日志会有对应的 ERROR，重查是有界的、按 Go 侧的节奏。
- 列表外有 4 个测试会变红，需要按 needs_outside 修夹具；cross_zone_test 里多数退出 / 交接用例虽然仍是绿的，但实际走的是 marshal 拒绝路径，而不是它们原本要测的快路径。
- Unmarshal 中 PlayerCurrencyComp 改为 emplace：将来如果有代码在加载之前就挂上它，Debug 下会断言，Release 下 entt 不做检查，后果未定义。当前已确认没有这样的路径。
- 两个生成器模板没有实际渲染验证（只能靠人工核对 Go template 语法和作用域）。header 模板的输出路径就是仓库里的 player_data_loader.h，将来重新生成会覆盖手改版本；两者已经手工对齐，但空行可能略有出入。
- 已有问题，本批未处理：手工维护的 player_database_loader 从来没有对 player_database.merge_state（字段 10，PlayerMergeStateComp）做 Unmarshal / Marshal。tools/merge_zone 写进去的合服状态会在 scene 第一次存盘时被整条记录覆盖掉。模板生成的版本反而会处理这个字段。这不在本批审计条目里，建议单独立项核对。
- 全部改动未编译、未运行测试（AGENTS §10.1），等 Codex 按 codex_steps 验证。

**该组代理给的验证步骤**：
- 前置：在仓库根 D:/luyuan/xuanming-server-mmo-cpp 下执行，同一工作区有其他会话在并发改代码。不要 git stash / reset / checkout，只编译和运行。所有 MSBuild 命令都串行执行，并加 /m:1（并发会报假的 C1041 / LNK1104）。配置 Debug|x64，与 tools/scripts/run_cpp_tests.ps1 的默认值一致。
- 1. 编 modules 库（含 snapshot_system.cpp）：msbuild cpp\libs\modules\modules.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1。通过标准：0 error。该工程开了 TreatWarningAsError，也必须 0 warning。
- 2. 编 scene 库（含 player_database_loader.cpp、player_database_1_loader.cpp、player_lifecycle.cpp）：msbuild cpp\libs\services\scene\scene.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1。通过标准：0 error / 0 warning（WX）。重点确认 player_lifecycle.cpp 的 HandlePlayerAsyncSaved 那处没有用返回值的 MarshalPlayerForSave 调用不报警告：函数没有加 [[nodiscard]]，应该不报。
- 3. 编 scene 节点，确认链接与签名变化没有破坏其他调用点：msbuild cpp\nodes\scene\scene.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1。通过标准：0 error。
- 4. 编并运行 bag_test：pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter bag_test。另外单独用 gtest 过滤器跑本批用例：在 build\cpp\tests\ 下找到 bag_test 的 exe，工作目录设为仓库根（用例要读 bin/etc 配表），执行 <bag_test.exe> --gtest_filter=PlayerFeaturePersistenceTest.*:PlayerFeatureSnapshotTest.*。通过标准：这两组全绿，其中包括新增的 MarshalRefusesWhenAnyPersistedComponentIsMissing、SaveRefusesIncompletePlayerBeforeTouchingRedis、SaveWithoutGuidIsRefused、InitFromAllDataRejectsDuplicateWithoutLeavingOrphanEntity、RollbackSnapshotOfIncompletePlayerDoesNotCreateComponents。
- 5. 在 bag_test 全量运行里，下面 3 个用例预期会红：它们的手搭玩家缺持久化组件，以前靠 get_or_emplace 补齐。文件不在我负责的范围，见 needs_outside。分别是 GuidSourcePolicyTest.SnapshotIsSkippedUntilItsOwnKindHasARange（bag_test.cpp）、PlayerEquipAttributeTest.ReloadFromTheDatabaseRecordRecomputesTheEquipmentBonus、PlayerEquipAttributeTest.ReloadFromTheDatabaseRecordRecomputesEnhanceAndGemBonuses。请记录失败输出（含 [PlayerSave] Persisted component(s) missing ... missing=<列表> 那条 ERROR），确认失败原因确实是缺组件。除这 3 个以外，bag_test 的其他用例都应保持原有结果。
- 6. 编并运行 cross_zone_test：pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter cross_zone_test。预期只有 CrossZoneBagMarshal.EquipInstanceDataSurvivesThePlayerAllDataHop 会红（source 实体只有 PlayerBagsComp，同属 needs_outside）。ExitPersistEcs.* / ExitReleaseEcs.* / EnterFenceEcs.* 以及 RelocateConfirm、DispatchEnterSceneReply 相关用例应保持全绿：它们的手搭玩家现在走的是「marshal 拒绝 → SavePlayerToRedis 返回 false」，外部结果与原来的快路径相同。若其中任何一个变红，请保留完整 gtest 输出与日志，不要改代码。
- 7. 可选 Linux：bash tools/scripts/build_linux.sh（按该脚本的既有口径），确认 gcc/clang 下 scene 与 modules 能编过。
- 8. 生成器模板：cd tools/proto_generator/protogen && go build ./... && go vet ./internal/generator/cpp/...。注意这一步只验证 Go 代码，模板是运行时才解析的，没有被验证。不要为了验证去跑完整生成器：它会把 player_data_loader.h 直接写回仓库，与其他会话冲突。
- 失败时保留：MSBuild 的首个 error 行及所在文件和行号；gtest 失败用例名与断言输出；含 [PlayerSave] / [SavePlayerToRedis] / [SnapshotSystem] 的日志行。命令失败后不要连续自动重试。
- （修正轮补充）以下只列与前一版 codex_steps 不同的地方，其余照旧：仓库根执行；Debug|x64；MSBuild 一律串行 /m:1；不要 stash / reset / checkout；失败后不连续重试。
- （修正轮补充）步骤 2（编 scene 库，cpp\libs\services\scene\scene.vcxproj，/m:1）：额外确认 player_lifecycle.cpp 约 3927–3931 行新增的 LOG_ERROR 能编过、无警告。它往 muduo LogStream 输出 bool（allowSkipWhenPersisted 和两个 any_of 的结果），LogStream 有 operator<<(bool)。通过标准：0 error、0 warning（/WX）。
- （修正轮补充）步骤 4（bag_test）：执行 <bag_test.exe> --gtest_filter=PlayerFeaturePersistenceTest.*:PlayerFeatureSnapshotTest.*，工作目录设为仓库根。通过标准是全绿。其中 PlayerFeaturePersistenceTest.SaveRefusesIncompletePlayerBeforeTouchingRedis 已改为先预置落盘快照，日志里应同时出现以下两条 ERROR：
- [PlayerSave] Persisted component(s) missing ... missing=PlayerAssetOpLedgerComp
- [SavePlayerToRedis] Save refused ... allow_skip_when_persisted=1 exiting=0 handoff_pending=0
- （修正轮补充）步骤 5（bag_test 全量）更新预期：
- player_equip_attribute_test.cpp 已由另一会话补了 CompletePersistedComps，PlayerEquipAttributeTest.ReloadFromTheDatabaseRecordRecomputesTheEquipmentBonus 和 ReloadFromTheDatabaseRecordRecomputesEnhanceAndGemBonuses 预期应变绿。若仍红，保留输出，并检查日志里 missing= 后面列的组件。
- 预期只有 GuidSourcePolicyTest.SnapshotIsSkippedUntilItsOwnKindHasARange（bag_test.cpp:3825）会红，原因是夹具缺持久化组件，修复属于本组范围外。请记录它的断言输出和 [PlayerSave] / [SnapshotSystem] 日志行，确认原因确实是缺组件。
- （修正轮补充）步骤 6（cross_zone_test）补充：
- 预期只有 CrossZoneBagMarshal.EquipInstanceDataSurvivesThePlayerAllDataHop 会红。
- ExitPersistEcs.* / ExitReleaseEcs.* / 交接应答相关用例应仍然绿。请用 Select-String 在测试输出里统计 "[SavePlayerToRedis] Save refused" 的条数，并记录带 exiting=1 或 handoff_pending=1 的行数。这些行证明这批用例目前走的是拒存路径、不是原本要测的快路径（即审查所说的「假绿」），数字留给后续修夹具时对照。
- （修正轮补充）合入门禁：bag_test.cpp 和 cross_zone_test.cpp 的夹具修复（见 unresolved）落地之前，本批不得单独合入。全部验证完，请把上面两个预期变红的用例名与失败原因一并写进验证报告。

### 组：空间/AOI 热路径（spatial）

**修了**：F075、F077、F078

**改动**：
- `cpp/libs/services/scene/spatial/system/aoi.h`：新增 static void OnSceneCreatedHandler(const OnSceneCreated&)，并前置声明 OnSceneCreated。注释写明它是格子表的唯一创建点，之后的读写一律 try_get。
- `cpp/libs/services/scene/spatial/system/aoi.cpp`：1) 实现 OnSceneCreatedHandler：场景实体先 valid()；已有格子表时记 LOG_ERROR 并保留原表，否则 emplace<SceneGridListComp>。2) Update 与 BeforeLeaveSceneHandler 把 get_or_emplace<SceneGridListComp> 改为 try_get，缺失时 LOG_ERROR 并 continue/return(两处原本都已先做 sceneRegistry.valid())。3) UpdateGridState 的首次进场分支成为 AoiListComp 的唯一创建点：没有时才 emplace，瞬移或换场景重走该分支时保留原表和钉住条目。4) NotifyEntityVisibilityChanges/BroadcastEntityLeave 的 ActorListCreateS2C/ActorListDestroyS2C/ActorDestroyS2C 改为函数内 static thread_local 缓冲，不再对 GlobalEntity 做 get_or_emplace。
- `cpp/libs/services/scene/spatial/system/grid.cpp`：新增文件内辅助函数 FindSceneGridList：先 sceneRegistry.valid() 再 try_get，失效或缺表时 LOG_ERROR 并返回 nullptr。GetEntitiesInGridAndNeighbors 和 GetEntitiesInViewAndNearby 改用它，查不到就返回空结果，不再 get_or_emplace。
- `cpp/libs/services/scene/spatial/system/interest.cpp`：AddAoiEntity：先校验 watcher/target 都还存活(actorRegistry.valid)，再 try_get<AoiListComp>。缺表或句柄失效时 LOG_ERROR 并返回 false，不再补建。新增 muduo Logging 头文件。
- `cpp/libs/services/scene/spatial/system/interest.h`：AddAoiEntity 注释补上前置条件：watcher/target 都存活，且 watcher 已加入 AOI；PinAoiEntity 同样受此约束。
- `cpp/nodes/scene/handler/event/scene_event_handler.cpp`：只在 OnSceneCreatedHandler 守护段内追加 AoiSystem::OnSceneCreatedHandler(event)。注释用 ASCII，因为该文件注明没有 BOM、按 CP936 处理；aoi.h 原本就在顶部守护段里包含了，没有改动 include。
- `cpp/tests/aoi_test/aoi_system_test.cpp`：删掉已失效的 Register/UnregisterGlobalMessageComponents 及其在 6 个 fixture 里的调用。新增 JoinAoi 辅助函数，模拟生产创建点；在 9 个直接调用 AddAoiEntity/Pin 的原有用例里先 JoinAoi(watcher)。新增 4 个 AoiGridTest、3 个 AoiVisibilityTest、3 个 AoiPriorityTest 回归用例。
- `cpp/tests/aoi_test/interest_system_mock.cpp`：这个文件不参与编译，只镜像生产实现：AddAoiEntity 同步改为校验存活 + try_get，不补建。
- `cpp/tests/aoi_delivery_test/aoi_delivery_test.cpp`：SetUp 不再往 GlobalEntity 预挂消息组件，场景格子表改走生产的 AoiSystem::OnSceneCreatedHandler。新增 F075 回归用例。
- （修正轮）`cpp/libs/services/scene/spatial/system/interest.cpp`：AddAoiEntity 在 try_get 判空之后，所有 `comp.entries` 改成 `comp->entries`(find/end/size/emplace/at/erase)，`FindLowestWeightEntity(comp, policy)` 改成 `FindLowestWeightEntity(*comp, policy)`，修复指针按引用使用导致的编译错误。语义不变。

**新增测试**：
- `cpp/tests/aoi_test/aoi_system_test.cpp` `AoiGridTest.UpdateDoesNotCreateGridListForSceneWithoutOne`：修复前为什么红：修复前 Update 对场景实体做 get_or_emplace<SceneGridListComp>，会当场挂出格子表，并把实体放进格子、挂上 Hex。所以 EXPECT_FALSE(any_of<SceneGridListComp>) 和 EXPECT_FALSE(any_of<Hex>) 必定失败。
- `cpp/tests/aoi_test/aoi_system_test.cpp` `AoiGridTest.SpatialQueryOnDestroyedSceneDoesNotCreateGridList`：修复前为什么红：修复前 grid.cpp 两个查询不做 valid()，直接 get_or_emplace。EnTT 3.13 的 get_or_emplace 不检查存活，会在已销毁的场景句柄上挂出孤儿格子表，sparse_set 按含版本号的完整句柄判断 contains，所以 try_get<SceneGridListComp>(已销毁场景) 不为空，EXPECT_EQ(..., nullptr) 必定失败。Debug 和 Release 都不会触发断言。
- `cpp/tests/aoi_test/aoi_system_test.cpp` `AoiGridTest.OnSceneCreatedAttachesGridList`：修复前为什么红：修复前没有 AoiSystem::OnSceneCreatedHandler 这个入口，测试本身编译不过。它验证新的场景创建点会挂上空表，之后 Update 能正常把实体放进格子。
- `cpp/tests/aoi_test/aoi_system_test.cpp` `AoiGridTest.DuplicateOnSceneCreatedKeepsExistingGridList`：修复前为什么红：同样依赖新入口，修复前编译不过。它守住一点：重复收到 OnSceneCreated 时不能覆盖已登记实体的格子表。
- `cpp/tests/aoi_test/aoi_system_test.cpp` `AoiVisibilityTest.FirstGridEntryCreatesInterestList`：修复前为什么红：修复前 AoiListComp 只在 AddAoiEntity 里懒建，周围没人的实体首次 Update 后仍然没有兴趣表，ASSERT_NE(aoiList, nullptr) 必定失败。
- `cpp/tests/aoi_test/aoi_system_test.cpp` `AoiVisibilityTest.FirstEntryAfterResetKeepsPinnedEntries`：修复前为什么红：这是守护用例，修复前也是绿的，因为 get_or_emplace 不会替换已有的表。如果以后有人把首次进场的创建点改成 emplace_or_replace，瞬移后钉住的条目会丢，它就会变红。
- `cpp/tests/aoi_test/aoi_system_test.cpp` `AoiVisibilityTest.BeforeLeaveSceneDoesNotRecreateMissingGridList`：修复前为什么红：修复前 BeforeLeaveSceneHandler 对场景实体做 get_or_emplace<SceneGridListComp>，会把被删掉的格子表重新挂回来，EXPECT_FALSE(any_of<SceneGridListComp>) 必定失败。
- `cpp/tests/aoi_test/aoi_system_test.cpp` `AoiPriorityTest.AddAoiEntityRejectsWatcherNotJoinedAoi`：修复前为什么红：修复前 AddAoiEntity 用 get_or_emplace，对还没进过格子的观察者会当场挂上 AoiListComp 并返回 true，EXPECT_FALSE(AddAoiEntity)、EXPECT_FALSE(PinAoiEntity) 和 EXPECT_FALSE(any_of<AoiListComp>) 都会失败。
- `cpp/tests/aoi_test/aoi_system_test.cpp` `AoiPriorityTest.AddAoiEntityRejectsDestroyedWatcher`：修复前为什么红：修复前不校验观察者是否存活，get_or_emplace 在已销毁的句柄上挂出孤儿 AoiListComp，策略和容量都回落到默认值后插入成功、返回 true。EXPECT_FALSE 和 EXPECT_EQ(try_get, nullptr) 都会失败，不会崩溃。
- `cpp/tests/aoi_test/aoi_system_test.cpp` `AoiPriorityTest.AddAoiEntityRejectsDestroyedTarget`：修复前为什么红：修复前不校验目标是否存活，已销毁的目标会被直接插进兴趣表并返回 true，EXPECT_FALSE(AddAoiEntity) 和 EXPECT_FALSE(IsInAoiList) 必定失败。
- `cpp/tests/aoi_delivery_test/aoi_delivery_test.cpp` `AoiDeliveryTest.VisibilityNotificationsDoNotAttachComponentsToGlobalEntity`：修复前为什么红：修复前进视野、出视野、离场三条通知路径都用 globalRegistry.get_or_emplace<ActorListCreateS2C/ActorListDestroyS2C/ActorDestroyS2C>(GlobalEntity()) 取缓冲。SetUp 已不再预挂这些组件，跑完后 GlobalEntity 上一定会多出这三个组件，最后三条 EXPECT_FALSE(any_of) 必定失败；前面的 ASSERT 确认三条路径确实发过消息、消息体正确。

**对抗审查**：issues
- [high] `cpp/libs/services/scene/spatial/system/interest.cpp:107`：AddAoiEntity 第 97 行把 `auto& comp = get_or_emplace<AoiListComp>(watcher)` 改成了 `auto* comp = try_get<AoiListComp>(watcher)`，但函数后半段仍按引用写法使用：107 行 `comp.entries.find(target)`、`comp.entries.end()`，118 行 `comp.entries.size()`，120 行 `comp.entries.emplace`，125 行 `FindLowestWeightEntity(comp, policy)`（形参是 `const AoiListComp&`，传进去的是指针），128 行 `comp.entries.at`，131/132 行 `comp.entries.erase/emplace`。对指针用 `.` 访问成员，MSVC 报 C2228，GCC 报 request for member in pointer。scene.lib（scene.vcxproj 和 CMakeLists.txt 都会编 interest.cpp）编不过，aoi_delivery_test.vcxproj 也直接编这个文件，同样编不过。scene 节点、aoi_test、aoi_delivery_test 以及所有链接 scene.lib 的工程都会失败，本组声称的 11 个回归用例一个也跑不起来。同目录的 interest_system_mock.cpp 已经改成 `comp->`，说明实施者本来想同步改，只是生产文件漏了。
  - 建议：把 AddAoiEntity 中 try_get 之后所有 `comp.` 改成 `comp->`，把 `FindLowestWeightEntity(comp, policy)` 改成 `FindLowestWeightEntity(*comp, policy)`。也可以在判空之后写 `auto& list = *comp;`，后面统一用 list，函数体其余部分就不用动。
- [low] `cpp/libs/services/scene/spatial/system/aoi.cpp:51`：缺格子表时，Update 每个 tick 对该场景里每个 actor 各记一次 LOG_ERROR，20FPS 下按 actor 数线性刷屏。grid.cpp 的 FindSceneGridList 在每次视野广播（BroadcastMessageToVisiblePlayers → GetEntitiesInViewAndNearby）时也会记一次。fail-closed 的方向是对的，实施者也在 risks 里写了，但没有任何限流。真出现初始化缺口时，日志量会把真正的根因淹没。
  - 建议：可选：同一场景只在第一次发现缺表时记 ERROR（例如按场景实体去重，或用计数器每 N 次记一次）。不改的话，在交付说明里把它列为已知风险即可。

**修正轮**：审查提的 high 问题(interest.cpp 编译不过)已经修好，代码未编译，等 Codex 验证。

原因是 AddAoiEntity 把取兴趣表那一行从 `auto& comp = get_or_emplace<AoiListComp>(watcher)` 改成了 `auto* comp = try_get<...>`，判空之后的代码却还按引用写法用 `comp.`。现在后面的访问统一改成 `comp->`，`FindLowestWeightEntity(comp, policy)` 改成 `FindLowestWeightEntity(*comp, policy)`，和同文件 RemoveAoiEntity/UnpinAoiEntity 等函数的写法一致。改完 grep 过，`comp.` 只剩 FindLowestWeightEntity 函数体里一处，那里的形参本来就是 `const AoiListComp&`，是对的。

其余改动也按当前工作区逐条核过，没再动：
- 场景格子表只在 OnSceneCreatedHandler 里挂一次。
- AoiSystem::Update、BeforeLeaveSceneHandler 和 grid.cpp 的两个空间查询都改成先 valid() 再 try_get，缺表时 LOG_ERROR 并跳过，不再补建。
- 兴趣表唯一的创建点是首次进格分支，先用 any_of 判断再 emplace，瞬移或换场景回到这个分支时沿用原表。
- 三处 S2C 缓冲改成函数内 static thread_local。
- 场景事件 handler 只在守护段内加了一行转调。

全仓搜过，Hex 和 AoiListComp 的 emplace 都只剩 aoi.cpp 的 UpdateGridState 一处。cpp/tests 下除了 aoi_test 和 aoi_delivery_test，没有别的测试调 AoiSystem::Update，也没有依赖懒建格子表的。skill_test 用的是 mockview。

测试也逐条核过：
- 原有 9 处 AddAoiEntity/Pin 用例都在调用前先 JoinAoi 或先跑过 Update。
- 6 个 fixture 和 AoiGridTest 里自建场景的用例都自己挂了格子表。
- 对照仓库 third_party 里的 EnTT 源码，try_get、any_of、all_of 只调 contains，不断言实体存活，所以对已销毁句柄调 try_get 的两个新用例在 Debug 下也不会触发断言。
- 未解决：low(aoi.cpp:51 缺格子表时每个 tick 每个 actor 都记一次 LOG_ERROR)：结论成立，但本次没改，作为已知风险交付。理由：(1) 唯一会持续触发的情况是新增了一条不 trigger OnSceneCreated 的场景创建路径，第一次联调就会暴露，大声报错正是 fail-closed 想要的效果；(2) 同一循环里原有的 'Scene not found' 日志也是按 actor×tick 记，这里保持一致；(3) 仓库唯一现成的限流工具 FailureLogThrottle 是 kafka 专用的，要限流就得在热路径加 static 可变状态或引入新工具，超出本组「不扩大改动」的约束。grid.cpp 的 FindSceneGridList 只有已进格(有 Hex)的实体才会走到，正常情况下表必然存在，刷屏面更小。以后要处理，建议按场景实体去重，单独立项。
- 未解决：F076(格子集合和兴趣表里存的是裸 entity 句柄，没有 on_destroy 钩子)不在本组分配范围内，未处理。AddAoiEntity 新加的 target 存活校验只能防止陈旧句柄被重新登记，表里已有的陈旧条目还要等 F076 修复后才能清掉。
- 未解决：C006(审计附录同在 interest.cpp：ScenePriorityPolicyComp、AoiClientCapacityComp、ScenePressureComp 在生产代码里没有创建点，属于死代码)不在本组分配范围，也不是补建类误用，未处理。
- 未解决：需要改本组范围外的文件(未改)：docs/design/ecs-entt-misuse-audit-20261015.md 附录 F075/F077/F078 的状态列，应标为「已修复，待 Codex 验证」；docs/PROGRESS.md 需要追加本批记录，并按 §12 注明 Java 版状态：都是 C++ 内部 EnTT 用法，不涉及客户端可见协议、tip 码或配置表，Java 版不需要对应改动，是否在 PARITY.md 登记「不适用」由编排方决定。

**需要列表外改动**：
- docs/design/ecs-entt-misuse-audit-20261015.md：附录表 F075/F077/F078 的状态列需标为「已修复，待 Codex 验证」。该文件不在我负责的范围内，没有改。
- docs/PROGRESS.md：需追加一条本批记录(按 AGENTS §5，只追加)，并按 §12 注明 Java 版状态：这三条都是 C++ 侧 EnTT 内部用法，没有改客户端可见协议、tip 码或配置表，Java 版不需要对应改动；是否在 Java 仓库 PARITY.md 登记「不适用」由编排方决定。
- F076(格子集合和兴趣表里存的是裸 entity 句柄，没有 on_destroy 钩子)不在本组分配范围内，没有处理。本次 AddAoiEntity 的 target 存活校验只能挡住陈旧句柄被再次登记，已在表里的陈旧条目仍要靠 F076 的修复来清理。

**风险**：
- 未编译、未运行测试，所有结论都来自静态阅读，待 Codex 验证。
- 格子表现在只在 OnSceneCreated 时创建。如果以后新增的场景创建路径不 trigger OnSceneCreated，该场景里的 actor 会每帧打一条 'SceneGridListComp missing' 错误并被跳过，互相不可见。这是有意 fail-closed，但日志会按 actor×tick 刷屏；同一循环里原有的 'Scene not found' 日志也是这样。目前生产代码里只有 scene_node_service.cpp 和 scene_handler.cpp 两处创建场景，都会 trigger。
- PinAoiEntity/AddAoiEntity 现在要求观察者已经被 AoiSystem::Update 放进过格子，否则返回 false。目前生产代码没有 Pin 的调用方；以后 buff/skill 接入钉住功能时，要处理目标刚进场、还没跑过首次 Update 时返回 false 的情况。
- 兴趣表进场后一直随实体存在：离场只清空条目，不摘组件，与原行为一致。同场景瞬移和换场景都沿用原表，这依赖首次进场分支里的 any_of 判断，由 FirstEntryAfterResetKeepsPinnedEntries 守住。
- S2C 缓冲改成函数内 static thread_local：它和 tlsEcs 一样跟着线程析构。已确认发送路径同步序列化(SerializePartialToArray)、不会回调进 AOI，所以没有重入覆盖的风险，复用语义和原来挂在 GlobalEntity 上时一致。
- scene_event_handler.cpp 是带守护段的生成文件，这次只改了 OnSceneCreatedHandler 守护段，重新生成会保留守护段内容。

**该组代理给的验证步骤**：
- 工作目录 D:\luyuan\xuanming-server-mmo-cpp，在 VS 开发者命令行中执行，所有 MSBuild 串行 /m:1。同一工作区有其他会话在并发改动，不要用 git stash/checkout/reset。
- 1) 编译 scene 库(aoi/grid/interest 都在这里，aoi_test 链接它)：msbuild cpp\libs\services\scene\scene.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1。期望产物 lib\scene.lib 更新，0 error。
- 2) 编译 scene 节点，验证 scene_event_handler.cpp 守护段里的新调用能编译和链接：msbuild cpp\nodes\scene\scene.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1。期望 0 error。
- 3) 编译 AOI 单测：msbuild cpp\tests\aoi_test\aoi_test.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1。期望产物 build\cpp\tests\aoi_test.exe。
- 4) 编译 AOI 投递测试(不在 game.sln 里，直接编 vcxproj)：msbuild cpp\tests\aoi_delivery_test\aoi_delivery_test.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1。期望产物 build\cpp\tests\aoi_delivery_test.exe。
- 5) 运行 build\cpp\tests\aoi_test.exe，全量跑、不加过滤器。通过标准：0 failed。重点关注 --gtest_filter=AoiGridTest.OnSceneCreatedAttachesGridList:AoiGridTest.DuplicateOnSceneCreatedKeepsExistingGridList:AoiGridTest.UpdateDoesNotCreateGridListForSceneWithoutOne:AoiGridTest.SpatialQueryOnDestroyedSceneDoesNotCreateGridList:AoiVisibilityTest.FirstGridEntryCreatesInterestList:AoiVisibilityTest.FirstEntryAfterResetKeepsPinnedEntries:AoiVisibilityTest.BeforeLeaveSceneDoesNotRecreateMissingGridList:AoiPriorityTest.AddAoiEntityRejects*，以及改过前置条件的原有用例 AoiPriorityTest.*:AoiDynamicCapacityTest.*:AoiPriorityPolicyTest.*:AoiTeammateRefreshTest.*。
- 6) 运行 build\cpp\tests\aoi_delivery_test.exe，全量跑。通过标准：0 failed，含新用例 AoiDeliveryTest.VisibilityNotificationsDoNotAttachComponentsToGlobalEntity 和原有 5 个投递用例。
- 7) 失败时保留：MSBuild 的第一个 error 行及所在文件和行号；gtest 失败用例名和断言输出；同时出现的 LOG_ERROR 行(如 'SceneGridListComp missing'、'AddAoiEntity: watcher has no AoiListComp')。只汇报，不要自行改测试或跳过。
- 8) (可选，回归面)这次改了 scene.lib 里 AddAoiEntity 的语义，如果还有时间，串行编译整个 game.sln：msbuild game.sln /p:Configuration=Debug /p:Platform=x64 /m:1，确认其他链接 scene.lib 的工程不受影响。
- （修正轮补充）前置说明：interest.cpp 的编译阻断已修复，可以按实施者原来给的 codex_steps 1→6 串行执行，命令不变。工作目录 D:\luyuan\xuanming-server-mmo-cpp，在 VS 开发者命令行里执行，所有 MSBuild 都加 /m:1。同一工作区还有其他会话在改代码，不要用 git stash/checkout/reset。
- （修正轮补充）1) msbuild cpp\libs\services\scene\scene.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1。通过标准：0 error，重点确认 interest.cpp 没有 C2228/C2664。
- （修正轮补充）2) msbuild cpp\nodes\scene\scene.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1。通过标准：0 error。
- （修正轮补充）3) msbuild cpp\tests\aoi_test\aoi_test.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1，然后运行 build\cpp\tests\aoi_test.exe，全量跑、不加过滤器。通过标准：0 failed。
- （修正轮补充）4) msbuild cpp\tests\aoi_delivery_test\aoi_delivery_test.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1(该工程直接编 aoi/grid/interest/view.cpp)，然后运行 build\cpp\tests\aoi_delivery_test.exe，全量跑。通过标准：0 failed，包括 AoiDeliveryTest.VisibilityNotificationsDoNotAttachComponentsToGlobalEntity。
- （修正轮补充）5) 失败时保留：MSBuild 的第一个 error 行(文件:行号)、失败的 gtest 用例名和断言输出、同时出现的 LOG_ERROR 行。只汇报，不要改测试，也不要跳过用例。
- （修正轮补充）6) 可选回归：msbuild game.sln /p:Configuration=Debug /p:Platform=x64 /m:1，确认其他链接 scene.lib 的工程不受 AddAoiEntity 语义变化影响。

### 组：actor 属性与状态（actor）

**修了**：F035、F034、F057、F074
- 推迟 F035（生成的同步脏位部分）：ResetCombatStateFlags 调用的 SetActorBaseAttributesS2CAttrDirtyBit 在 cpp/libs/services/scene/generated/attribute/ 下，是生成产物，对 ActorBaseAttributesS2CDirtyMaskComp 用 get_or_emplace，同步之后只清位不摘组件。按 §3 不能手改，需要改 protogen 模板，不在本批文件范围内。
- 推迟 F058：GmSetLevel 用 to_integral 把实体整数写进 PlayerUpgradeEvent 的问题按任务指示留给后续批次，这次只把 GmSetLevel 里的 get_or_emplace<LevelComp> 改成了 try_get + fail-closed。

**改动**：
- `cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp`：UpdateMoveSpeed 和 ResetCombatStateFlags 改为 try_get MoveSpeedComp / CombatStateFlagsComp，缺了记 ERROR 并跳过，不再补建。没有 BuffListComp（按需组件）时移速按 0 算，以前是直接 return，旧加成会一直留着。MarkAttributeForUpdate 在碰组件之前先校验位越界和实体有效，已有标记就合并位，没有才 emplace。Update 先拷出脏位，再 remove 当前实体的 AttributeDirtyFlagsComp，然后执行重算函数，消费即摘除。
- `cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.h`：补上接口契约注释：按需脏标记、合并规则、线程模型，以及缺组件时跳过不补建。
- `cpp/libs/services/scene/actor/action_state/system/actor_action_state.cpp`：新增 FindActorStateComp（先 valid 再 try_get，只取不建）。TryPerformAction 把「状态组件缺失」和「非法 successState」两项校验挪到打断事件和状态改动之前。CanExecuteActionWithoutStateChange 缺组件时不再当成放行。AddState 去掉 get_or_emplace。三个入口缺组件都返回 kThisEntityIsInvalid；RemoveState 的缺组件错误码也统一成 kThisEntityIsInvalid（原来是 kInvalidParameter）。
- `cpp/libs/services/scene/actor/action_state/system/actor_action_state.h`：补上类契约注释：前置条件、缺组件时的返回码，以及在任何副作用之前返回。
- `cpp/nodes/scene/handler/event/actor_event_handler.cpp`：只改了守护段：InitializeActorCompsEventHandler 新增挂 ActorStateComp、MoveSpeedComp、CombatStateFlagsComp（与既有的两张技能表一样用幂等的 get_or_emplace），并补了对应 include。
- `cpp/libs/services/scene/player/system/player_attribute.cpp`：EnsureComp 拆成两个函数：FindComp（try_get，缺了记 ERROR）和 NormalizeSchemesOnLoad（只在 InitializeOnLoad 调用）。InitializeOnLoad 不再补建存档组件；DerivedAttributesComp 由它 emplace_or_replace 挂上，它是唯一创建点。Recalculate 先确认三个必需组件都在再动手，缺了就跳过，去掉了两处 get_or_emplace。BuildPanel 和 AutoAllocate 只读不建。写入口在扣费之前返回 kThisEntityIsInvalid。GmSetLevel 改为 try_get<LevelComp>，缺了就拒绝（F058 的 to_integral 问题未动）。PushPanel 缺组件时不推送。
- `cpp/libs/services/scene/player/system/player_attribute.h`：补上组件归属与缺组件处置的契约注释，更新 InitializeOnLoad 的说明。
- `cpp/libs/services/scene/player/system/player_pet.cpp`：EnsureComp 改为 FindComp（try_get + ERROR）。14 个入口逐个处理：写入口和交易原语在扣费、铸号、改动之前返回 kThisEntityIsInvalid；BuildList、BuildBattleSnapshot、ApplyBattleSettlement、InitializeOnLoad、RecalculateAll 缺组件时跳过，不补建；PushList 缺组件时不推送。
- `cpp/libs/services/scene/player/system/player_pet.h`：补上组件归属契约注释。交易原语的返回值映射说明加了一条例外：kThisEntityIsInvalid 表示加载链路故障，入口层应按 RETRY 处理，不能判成终局。
- `cpp/tests/bag_test/player_equip_attribute_test.cpp`：NewPlayer 先 emplace 空的 PlayerAttributeComp 再调 InitializeOnLoad，与 Unmarshal 同序。新增 NewUnloadedEntity（没走加载期初始化的实体）。新增 EmplaceIfAbsent / CompletePersistedComps，只在两条存档往返用例存盘之前调用，适配另一会话已改成 fail-closed 的 Marshal。补了对应 include。新增 6 条组件归属用例。
- `cpp/tests/bag_test/player_feature_handler_test.cpp`：新增 ActorEntityFixture，测试自建的实体在 TearDown 统一销毁。新增 ActorAttributeCalculatorTest 6 条、ActorActionStateTest 3 条、PetComponentOwnershipTest 2 条，并补了对应 include。
- （修正轮）`cpp/libs/services/scene/player/system/player_attribute.cpp`：BuildPanel 改为返回 uint32_t：实体无效返回 kEntityIsNull，缺 PlayerAttributeComp 返回 kServiceUnavailable，末尾返回 kSuccess。6 个写入口的缺组件分支和 GmSetLevel 的缺 LevelComp 分支由 kThisEntityIsInvalid 改为 kServiceUnavailable。PushPanel 改为 BuildPanel 不成功就不推，删掉原来的 valid 检查和单独的 FindComp 预检。
- （修正轮）`cpp/libs/services/scene/player/system/player_attribute.h`：BuildPanel 声明改为 uint32_t，补上返回值契约（非成功时 panel 已清空，调用方必须回错误码）。类注释的缺组件处置改为 kServiceUnavailable 并写明错误码口径。PushPanel 注释注明 BuildPanel 失败时不推。
- （修正轮）`cpp/libs/services/scene/player/system/player_pet.cpp`：BuildList 改为返回 uint32_t：实体无效返回 kEntityIsNull，缺 PlayerPetComp 返回 kServiceUnavailable，宝宝池缺配和正常结束都返回 kSuccess（宝宝池缺配沿用既有口径）。9 个写入口 / 交易原语的缺组件分支由 kThisEntityIsInvalid 改为 kServiceUnavailable。PushList 改为 BuildList 不成功就不推，删掉重复预检。
- （修正轮）`cpp/libs/services/scene/player/system/player_pet.h`：BuildList 声明改为 uint32_t 并补上返回值契约。类注释的缺组件码改为 kServiceUnavailable，注明错误码口径。交易原语映射说明里的例外码改为 kServiceUnavailable（RETRY、不记账）。PushList 注释同步更新。
- （修正轮）`cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp`：只改了注释：UpdateMoveSpeed 里 BuffListComp 的说明改为引用 buff.h 的按需契约（AddOrUpdateBuff 创建、BuffSystem::Update 在帧末摘掉空列表），并写明最后一条移速 buff 到期时这条分支会被真实走到。
- （修正轮）`cpp/tests/bag_test/player_feature_handler_test.cpp`：PetComponentOwnershipTest 两条用例的断言由 kThisEntityIsInvalid 改为 kServiceUnavailable，BuildList 补上返回值断言。新增 PetComponentOwnershipTest.BuildListReportsInvalidEntityAndSucceedsOnceLoaded。InvalidSuccessStateIsRejectedBeforeAnyStateChange 补注释，如实写明它守不住顺序。ActorActionStateTest 的其余内容属于并发会话，未动。
- （修正轮）`cpp/tests/bag_test/player_equip_attribute_test.cpp`：WriteEntriesRejectAnEntityWithoutTheSaveComponent 和 GmSetLevelRejectsAnEntityWithoutALevelComponent 的断言改为 kServiceUnavailable。PanelOfAnUnloadedEntityIsEmptyAndCreatesNothing 补上 BuildPanel 返回 kServiceUnavailable 的断言。新增 BuildPanelReportsInvalidEntityAndSucceedsForALoadedPlayer。分节注释里的错误码说明同步更新。

**新增测试**：
- `cpp/tests/bag_test/player_feature_handler_test.cpp` `ActorAttributeCalculatorTest.UpdateRemovesTheDirtyFlagComponentOnceConsumed`：修复前为什么红：修复前 Update 只 reset 位、不摘组件，Update 之后 AttributeDirtyFlagsComp 仍在，EXPECT_FALSE(Has<AttributeDirtyFlagsComp>) 失败。
- `cpp/tests/bag_test/player_feature_handler_test.cpp` `ActorAttributeCalculatorTest.FrozenPlayerKeepsItsDirtyFlagsUntilTheFreezeEnds`：修复前为什么红：冻结期间的断言修复前后都成立。解冻后那次 Update 在修复前不摘组件，而且没有 BuffListComp 时 ECS_GET_OR_VOID 直接返回，移速停在 2.0，所以最后两条断言失败。
- `cpp/tests/bag_test/player_feature_handler_test.cpp` `ActorAttributeCalculatorTest.RecalculationDoesNotCreateMissingRequiredComponents`：修复前为什么红：修复前 UpdateMoveSpeed 和 ResetCombatStateFlags 用 get_or_emplace，会给没初始化的实体补出 MoveSpeedComp 和 CombatStateFlagsComp，标记组件也不会被摘。
- `cpp/tests/bag_test/player_feature_handler_test.cpp` `ActorAttributeCalculatorTest.MoveSpeedFallsBackToZeroWhenNoBuffListIsLeft`：修复前为什么红：修复前没有 BuffListComp 时 ECS_GET_OR_VOID 直接返回，moveSpeed 停在 3.5，不会回到 0。
- `cpp/tests/bag_test/player_feature_handler_test.cpp` `ActorAttributeCalculatorTest.CombatStateFlagsMirrorTheActiveCombatStates`：修复前为什么红：修复前后都应为绿。这是正常路径的回归护栏，确认改成只取不建之后投影口径没变。
- `cpp/tests/bag_test/player_feature_handler_test.cpp` `ActorAttributeCalculatorTest.MarkIgnoresAnOutOfRangeAttributeBit`：修复前为什么红：修复前先 get_or_emplace 出标记组件，再 bitset::set(kAttributeCalculatorMax) 抛 out_of_range。EXPECT_NO_THROW 失败，组件也已经挂上（异常被 EXPECT_NO_THROW 捕获，进程不会崩）。
- `cpp/tests/bag_test/player_feature_handler_test.cpp` `ActorActionStateTest.EntriesRejectAnActorWithoutStateComp`：修复前为什么红：修复前 TryPerformAction 发现没有组件就走 AddState 的 get_or_emplace，返回 kSuccess 并挂上 ActorStateComp；CanExecuteActionWithoutStateChange 缺组件时也返回 kSuccess。
- `cpp/tests/bag_test/player_feature_handler_test.cpp` `ActorActionStateTest.InitializedActorEntersTheSuccessState`：修复前为什么红：修复前后都应为绿。确认挂好 ActorStateComp 的 actor 施法仍能正常进入战斗状态。
- `cpp/tests/bag_test/player_feature_handler_test.cpp` `ActorActionStateTest.InvalidSuccessStateIsRejectedBeforeAnyStateChange`：修复前为什么红：修复前后都应为绿（表中第 0 行没有打断态，测不出副作用的先后）。用来钉住非法目标状态返回 kInvalidParameter、已有状态不变。
- `cpp/tests/bag_test/player_feature_handler_test.cpp` `PetComponentOwnershipTest.ReadAndRecalculateEntriesCreateNothing`：修复前为什么红：修复前 BuildList 的 EnsureComp 会补出空的 PlayerPetComp，EXPECT_FALSE(Has<PlayerPetComp>) 失败。
- `cpp/tests/bag_test/player_feature_handler_test.cpp` `PetComponentOwnershipTest.WriteEntriesAndTradePrimitivesRejectWithoutTheComponent`：修复前为什么红：修复前各入口先补建组件再往下走：Summon 返回 kPetNotFound，GrantPet 真的铸号发出一只宝宝，RestorePetFromSnapshot 返回 kSuccess。返回码都不是 kThisEntityIsInvalid，组件也被补出来了。
- `cpp/tests/bag_test/player_equip_attribute_test.cpp` `PlayerEquipAttributeTest.InitializeOnLoadFillsTheDefaultSchemeAndCreatesTheDerivedCache`：修复前为什么红：修复前后都应为绿。确认默认方案的修补挪到加载期之后口径没变（方案一、已激活、next_scheme_id=2），二级属性缓存也已挂上。
- `cpp/tests/bag_test/player_equip_attribute_test.cpp` `PlayerEquipAttributeTest.InitializeOnLoadDoesNotCreateTheSaveComponentOnBehalfOfTheLoadPath`：修复前为什么红：修复前 EnsureComp 会 get_or_emplace 出 PlayerAttributeComp，Recalculate 也会 get_or_emplace 出 DerivedAttributesComp，两条 EXPECT_FALSE 都失败。
- `cpp/tests/bag_test/player_equip_attribute_test.cpp` `PlayerEquipAttributeTest.RecalculateSkipsAndCreatesNothingWhenARequiredComponentIsMissing`：修复前为什么红：修复前 Recalculate 会补建两个组件，并按「旧上限 0」把当前血量从 kFarAboveAnyMax 夹到新上限，组件断言和血量断言都失败。
- `cpp/tests/bag_test/player_equip_attribute_test.cpp` `PlayerEquipAttributeTest.PanelOfAnUnloadedEntityIsEmptyAndCreatesNothing`：修复前为什么红：修复前 BuildPanel 补建组件，并给出完整的 level、池、维度、方案，面板非空，组件也被补出来了。
- `cpp/tests/bag_test/player_equip_attribute_test.cpp` `PlayerEquipAttributeTest.WriteEntriesRejectAnEntityWithoutTheSaveComponent`：修复前为什么红：修复前各写入口先补建带默认方案的组件再往下走，返回码各不相同（例如 Allocate 返回 kSuccess、Reset 返回 kAttributeNothingToChange、SwitchScheme 返回 kAttributeSchemeAlreadyActive），而且 PlayerAttributeComp 和 DerivedAttributesComp 都被补出来了。
- `cpp/tests/bag_test/player_equip_attribute_test.cpp` `PlayerEquipAttributeTest.GmSetLevelRejectsAnEntityWithoutALevelComponent`：修复前为什么红：修复前 get_or_emplace<LevelComp> 补建并改了等级，返回 kSuccess。

**对抗审查**：issues
- [high] `cpp/nodes/scene/handler/event/actor_event_handler.cpp:45`：按当前工作区合起来看,BuffListComp 在生产代码里一个创建点都没有。F037 批已经去掉了 buff.cpp 里的两处 get_or_emplace<BuffListComp>(AddOrUpdateBuff / HandleExistingBuff),buff.cpp:142-148 和 buff.h 的契约写明「BuffListComp 是必需组件,只在 ActorEventHandler::InitializeActorCompsEventHandler 挂上,缺了返回 kServiceUnavailable」。但这个守护段归本批所有,本批只加了 ActorStateComp / MoveSpeedComp / CombatStateFlagsComp,没有加 BuffListComp。全仓 grep 'BuffListComp>' 加 emplace,结果为零。后果是线上每次 AddOrUpdateBuff 都直接返回 kServiceUnavailable,buff、技能命中挂 buff、移速 buff 整条失效。守护段承担的是 actor 必需组件的唯一创建点,本批复核同组件的创建/读取路径时漏掉了这一项。
  - 建议：在守护段里补 `tlsEcs.actorRegistry.get_or_emplace<BuffListComp>(actor);`,同时补 `#include "combat/buff/comp/buff_comp.h"`,注释里写明 BuffListComp 的唯一创建点在这里(与 buff.cpp:142 的契约一致)。另外在 docs/design/ecs-component-access-rules.md 对应条目里登记这个创建点。
- [medium] `cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:40`：UpdateMoveSpeed 的注释把 BuffListComp 定成「按需组件:没有它就是没有 buff,移速按 0 算」,与工作区里 buff.cpp / buff.h 已经立下的「BuffListComp 是 actor 必需组件」契约直接矛盾。按本批标准做法第 1 条,必需组件缺失应当 LOG_ERROR 并跳过,不能静默按 0 写回。生产代码里也没有任何路径会 remove BuffListComp,所以「旧加成残留」这个修复理由在生产上不成立。新增的两条用例 MoveSpeedFallsBackToZeroWhenNoBuffListIsLeft 和 FrozenPlayerKeepsItsDirtyFlagsUntilTheFreezeEnds(最后一条断言)把这个 fail-open 语义钉成了契约。
  - 建议：先和 buff 批对齐 BuffListComp 属于哪一类。若定为必需组件(推荐,与 buff.cpp 一致):UpdateMoveSpeed 缺 BuffListComp 时 LOG_ERROR 并跳过,不写 MoveSpeedComp。两条用例改成先 emplace<BuffListComp>(空表)再断言移速回到 0,另加一条「缺 BuffListComp 时移速保持原值、不建组件」。若定为按需组件:buff.cpp 那边必须恢复写入口的延迟创建。两批不能各执一词。
- [medium] `cpp/tests/bag_test/player_equip_test.cpp:821`：Recalculate 改成三个必需组件齐全才执行后,PlayerEquipTest 夹具 NewPlayer 不挂 PlayerAttributeComp 和 DerivedAttributesComp,于是 6 条既有用例(EquipMovesTheSameInstanceIntoTheWeaponSlot、UnequipReturnsTheSameInstanceToTheInventory、Enhance/EnhanceFailure/InlayGem/RemoveGem…Recalculates…)会红。实施者自己也承认这一点。本批如果单独落地,bag_test 不是全绿,违反 §11.5「改动小而完整」。
  - 建议：把 needs_outside 第 1 项(夹具补 emplace PlayerAttributeComp/DerivedAttributesComp,AttributesWereRecalculated 改成看 max_health()!=0,并同步四处断言与注释)作为本批的阻塞前置,与本批一起合入;没落地之前不要宣称本批完成。
- [medium] `cpp/libs/services/scene/player/system/player_attribute.cpp:796`：BuildPanel 和 PetSystem::BuildList(player_pet.cpp:481)是 void,组件缺失时只能返回空面板或空列表,调用方无从得知失败。GetAttributePanel(player_attribute_handler.cpp:38)和 GetPetList 会以成功码把空面板/空列表回给客户端。实施者在 PushPanel 的注释里自己写了「客户端按整体覆盖处理面板,推一张空面板等于把真实数据抹掉」,RPC 回包却是同样的效果。fail-closed 之后上游仍被判成成功,而 needs_outside 里只把它列为「建议」。
  - 建议：让 BuildPanel / BuildList 返回 uint32_t(缺组件时返回 kThisEntityIsInvalid)或 bool,handler 守护段据此 SetTip 并不填 panel/pets。或者至少把 needs_outside 里 handler 先判组件存在再回错误码那一条升级为必做,与本批同批落地。
- [low] `cpp/libs/services/scene/combat/skill/system/skill.cpp:321`：施法时缺 ActorStateComp 要到 CheckState 才被拒,而它排在 CheckRecovery / CheckChannel 之后,后两者会打断在途的后摇或引导(skill_test 的 CheckChannel_ImmediateInterrupt_EndsWholeChannelPhase 证明这里有副作用)。这违反本批「校验前置」的要求。实施者已列入 needs_outside,但仍是本批契约的缺口,只在不变量被破坏时才会触发。
  - 建议：在 CheckRequiredSkillComps 的 all_of 里加入 ActorStateComp(三者同一创建点),与技能批一起落地。
- [low] `cpp/tests/bag_test/player_feature_handler_test.cpp:418`：InvalidSuccessStateIsRejectedBeforeAnyStateChange 在修复前后都绿:ActorActionState 表第 0 行三个状态全是 Permitted,没有打断态。所以 TryPerformAction 里「successState 校验挪到打断之前」这一处行为变化没有任何回归测试守护。另外,守护段新增的三个组件创建点没有被任何测试工程编译(只有 cpp/nodes/scene 编它),创建点本身也没有测试覆盖。
  - 建议：在用例注释里如实写明它守不住重排序。若想覆盖,可以在测试里临时 Load 一份带 kActionStateInterrupt 的表数据,或抽出纯函数来测。创建点覆盖可以在交付说明里列为未验证项,由集成或压测的入场路径确认玩家实体上带有 ActorStateComp / MoveSpeedComp / CombatStateFlagsComp(以及 BuffListComp)。
- [low] `cpp/libs/services/scene/actor/action_state/system/actor_action_state.cpp:70`：同一类「必需组件缺失」的故障,错误码前后不一致:本批用 kThisEntityIsInvalid(1009),技能批 CheckRequiredSkillComps 和 buff 批用 kServiceUnavailable(1003)。同一次 CheckSkillPrerequisites 里,缺技能表回 1003,缺 ActorStateComp 回 1009。两者在 tip 表里都是 fault,不影响分类,但排障口径不统一。
  - 建议：与技能批、buff 批约定一个统一的「必需组件缺失」错误码,三处保持一致;或在 actor_action_state.h 的契约注释里说明为什么取 1009。
- [low] `cpp/tests/bag_test/player_equip_attribute_test.cpp:1696`：两条存档往返用例(1696 和 2433 行)新加了 CompletePersistedComps,但没有断言 PlayerAllDataMessageFieldsMarshal 的 bool 返回值。如果补齐列表与 Marshal 的检查集合将来不一致,返回 false 时 saved 是空的,失败会表现成后面一串属性或背包断言不符,很难定位。
  - 建议：改为 ASSERT_TRUE(PlayerAllDataMessageFieldsMarshal(player, saved)) << "fixture misses a persisted component";

**修正轮**：逐条核实了审查意见，并按「标准做法」修完能修的部分。代码没有编译，也没有跑测试，等 Codex 验证。

先说一个重要的并发情况：04:47~04:50 之间，另一个会话改写了本组的 actor_action_state.{h,cpp}、actor_event_handler.cpp 的守护段，以及 player_feature_handler_test.cpp 里的 ActorActionStateTest。同一时段它也改了 skill.cpp / skill.h。它把 ActorStateComp、CooldownTimeListComp、SkillContextCompMap 都改成了按需组件：由各自唯一的写入口延迟创建，初始化处理器只再挂 MoveSpeedComp 和 CombatStateFlagsComp。skill.cpp 的 CheckRequiredSkillComps 也换成检查 BaseAttributesComp、LevelComp、DerivedAttributesComp。

按协作约束，这些改动我全部保留、没有回退。我中途往 actor_action_state.cpp 插过一个辅助函数，发现文件被重写后已经删掉，净改动为零。所以上一轮 F034 的「ActorStateComp 是必需组件」口径已被那个会话的设计取代；上一轮 needs_outside 里关于 ActorStateComp 的两条也随之作废。

这一轮实际改动：
1. BuildPanel / BuildList 从 void 改为返回 uint32_t：实体无效返回 kEntityIsNull，缺必需组件返回 kServiceUnavailable，成功返回 kSuccess。PushPanel / PushList 直接按这个返回值决定推不推，去掉了重复的 FindComp 和重复的 ERROR 日志。
2. 属性、宝宝两个系统里「实体有效但缺必需组件」的返回码统一成 kServiceUnavailable。这与 HEAD 上 player_mission.cpp / player_feature_snapshot.cpp 的既有口径一致，也与 skill.cpp 的 CheckRequiredSkillComps 一致。涉及属性 6 个写入口 + GmSetLevel，宝宝 9 个写入口 / 交易原语。
3. UpdateMoveSpeed 的注释改为引用 buff.h 的按需契约。
4. 相应测试的断言跟着改，新增 2 条用例，并在 InvalidSuccessStateIsRejectedBeforeAnyStateChange 上如实注明它守不住检查顺序。

Java 版不用 entt，这批是服务端内部实现与返回码口径的调整。kServiceUnavailable 与 kThisEntityIsInvalid 都是已有的 common 码，客户端契约没变，不需要在 PARITY.md 登记。
- 未解决：【并发冲突，需编排方裁决】04:47~04:50 另一会话改写了本组负责的 actor_action_state.{h,cpp}、actor_event_handler.cpp 守护段和 player_feature_handler_test.cpp 的 ActorActionStateTest，同时改了 skill.cpp / skill.h。新设计里 ActorStateComp、CooldownTimeListComp、SkillContextCompMap 都是按需组件，分别由 AddState、skill.cpp 第 70 行 / 第 160 行的写入口延迟创建；初始化处理器只挂 MoveSpeedComp 和 CombatStateFlagsComp。我按约束保留、未回退，中途插入 actor_action_state.cpp 的辅助函数已删除，净零。影响：上一轮实施者自述的 F034 口径（必需组件，缺了拒绝施法）已作废；上一轮 needs_outside 第 2 条（把 ActorStateComp 加进 CheckRequiredSkillComps）和第 4 条（新 NPC 创建点必须挂齐五个组件）也一并作废。文档登记 F034 时应以那个会话的设计为准。
- 未解决：审查 high（BuffListComp 在生产代码里没有创建点）不成立。当前工作区的 buff.cpp / buff.h（另一批，04:39）已把 BuffListComp 定为按需状态：AddOrUpdateBuff 在免疫、驱散、叠层判断之后第一次插入条目时 get_or_emplace（buff.cpp:169，唯一创建入口），BuffSystem::Update 在帧末发现列表空了就 remove（buff.cpp:665-671）。审查看到的是 buff 批更早的「必需组件、缺了返回 kServiceUnavailable」版本。若在初始化处理器里挂它，会与 buff.h 契约矛盾（第一帧就会被摘掉），所以不加。
- 未解决：审查 medium（UpdateMoveSpeed 缺 BuffListComp 时按 0 算是 fail-open）不成立。BuffListComp 按 buff.h 契约是按需状态，「没有 = 身上没有 buff」是正常分支，不是缺组件故障。现在生产上会走到这条路径：最后一条移速 buff 到期时，OnBuffRemove（modifier_buff_impl.cpp:23-26）标脏 kMoveSpeed，同一帧 BuffSystem::Update 摘掉空列表；如果这里按旧代码直接 return，到期前的加成就会一直留着。注释已改为引用 buff.h 契约，两条相关用例保持不变。
- 未解决：审查 low（skill.cpp CheckRequiredSkillComps 应纳入 ActorStateComp）不成立，已被并发会话的设计取代。ActorStateComp 现在是按需组件：TryPerformAction 在缺组件时把它当作空状态，由 AddState 延迟创建，不再是必需组件，也不存在「CheckState 才发现缺组件」的问题。
- 未解决：【needs_outside，阻塞前置，未落地前本批不算完成】审查 medium（PlayerEquipTest 夹具）成立，但文件不在本组范围。cpp/tests/bag_test/player_equip_test.cpp 需要改四处：(a) NewPlayer（约第 821 行）在 emplace PlayerCurrencyComp 之后加 tlsEcs.actorRegistry.emplace<PlayerAttributeComp>(entity); 和 tlsEcs.actorRegistry.emplace<DerivedAttributesComp>(entity);，并补 #include "proto/common/component/player_attribute_comp.pb.h"（DerivedAttributesComp 的头第 48 行已经有）。(b) AttributesWereRecalculated()（约第 971 行）改为 const auto* d = tlsEcs.actorRegistry.try_get<DerivedAttributesComp>(player); return d != nullptr && d->max_health() != 0;，因为 Recalculate 总把 max_health 写成至少 1。(c) 第 1318、1510 行的 ASSERT_EQ(nullptr, try_get<DerivedAttributesComp>(player)) 改为 ASSERT_FALSE(AttributesWereRecalculated())；第 1334、1521 行的 EXPECT_NE(nullptr, ...) 改为 EXPECT_TRUE(AttributesWereRecalculated())。(d) 文件头第 13 行和第 969-970 行注释里「DerivedAttributesComp 出现」的说法改为「max_health 被重算写成非 0」。不改的话，PlayerEquipTest 的 6 条会红：EquipMovesTheSameInstanceIntoTheWeaponSlot、UnequipReturnsTheSameInstanceToTheInventory、EnhanceOnWornEquipmentRecalculatesAttributes、EnhanceFailureWithLevelLossOnWornEquipmentRecalculates、InlayGemOnWornEquipmentRecalculatesAttributes、RemoveGemOnWornEquipmentRecalculatesAttributes。
- 未解决：【needs_outside，必做，与本批同批落地】cpp/nodes/scene/handler/rpc/player/player_attribute_handler.cpp 的 GetAttributePanel 守护段，改为 if (const auto err = PlayerAttributeSystem::BuildPanel(player, *response->mutable_panel()); err != kSuccess) { SetTip(err); }。player_pet_handler.cpp 的 GetPetList 守护段同理：if (const auto err = PetSystem::BuildList(player, *response->mutable_pets()); err != kSuccess) { SetTip(err); }。其余写操作 handler 是先调写入口、成功后再 Build*，写入口已经先拒绝了缺组件的情况，可以不改。在这两处落地之前，组件缺失时仍然以成功码回一个空面板 / 空列表。返回值没有加 [[nodiscard]]，handler 现在丢弃返回值也能编译（scene 工程开了 TreatWarningAsError）。
- 未解决：【needs_outside，文档】docs/design/ecs-entt-misuse-audit-20261015.md 附录：F035、F057、F074 的「HEAD 状态」填「已修」；F034 按并发会话的按需设计填写。docs/design/ecs-component-access-rules.md 需要登记以下内容：MoveSpeedComp / CombatStateFlagsComp 的创建点是 ActorEventHandler::InitializeActorCompsEventHandler，DerivedAttributesComp 的创建点是 PlayerAttributeSystem::InitializeOnLoad，BuffListComp 和 AttributeDirtyFlagsComp 是按需状态，以及「缺必需组件 → kServiceUnavailable，实体无效 → kEntityIsNull / kThisEntityIsInvalid」的错误码口径。docs/PROGRESS.md 追加一条，注明 Java 版不适用。
- 未解决：【needs_outside，资产通道】PlayerAssetOpSystem 以后接入 PetSystem::RemovePetForTrade / RestorePetFromSnapshot 时，kServiceUnavailable（宝宝组件缺失）应与账本组件缺失同口径处理，即 RETRY、不记账，不能映射成终局 REJECTED。目前资产通道还没有调用这两个原语。
- 未解决：未覆盖项：InvalidSuccessStateIsRejectedBeforeAnyStateChange 守不住「successState 校验在打断之前」的顺序。ActorActionState 表里没有 kActionStateInterrupt 行，表管理器只有 Load()（从 GetConfigDir() 读 JSON），没有注入测试数据的接缝。要覆盖，需要表里有打断行，或者给生成的表管理器加注入接口（属生成模板改动，不在本组范围）。
- 未解决：未覆盖项：初始化处理器（actor_event_handler.cpp）只编进 cpp/nodes/scene，没有任何测试工程编译它，创建点本身没有单测。如需覆盖，需要把它加进 bag_test.vcxproj 和对应的 CMakeLists（不在本组范围），再写触发 InitializeActorCompsEvent 后断言组件齐全的用例；否则只能靠集成测试或压测的入场路径确认。
- 未解决：全部改动都没有编译、没有跑测试，等 Codex 按 codex_steps_delta 验证。同一工作区还有别的会话在并发改 skill / buff / actor_action_state，Codex 的结果会反映执行那一刻的工作区整体状态。

**需要列表外改动**：
- cpp/tests/bag_test/player_equip_test.cpp（必须改，否则 6 条用例会红）：(a) 夹具 NewPlayer（约第 821 行）在 emplace PlayerCurrencyComp 之后加两行：tlsEcs.actorRegistry.emplace<PlayerAttributeComp>(entity); 和 tlsEcs.actorRegistry.emplace<DerivedAttributesComp>(entity);，模拟一个已加载、但二级属性还没算过的玩家（max_health 为 0），并补 #include "proto/common/component/player_attribute_comp.pb.h"。(b) AttributesWereRecalculated() 改为 const auto* d = try_get<DerivedAttributesComp>(player); return d != nullptr && d->max_health() != 0;（Recalculate 总会把 max_health 写成至少 1）。(c) 第 1318、1510 行的 ASSERT_EQ(nullptr, try_get<DerivedAttributesComp>(player)) 改为 ASSERT_FALSE(AttributesWereRecalculated())；第 1334、1521 行的 EXPECT_NE(nullptr, ...) 改为 EXPECT_TRUE(AttributesWereRecalculated())。(d) 同步修改文件头第 13 行和第 969-970 行注释里「DerivedAttributesComp 出现」的说法。
- cpp/libs/services/scene/combat/skill/system/skill.cpp：CheckSkillPrerequisites 开头的必需组件校验（技能批正在加 all_of<CooldownTimeListComp, SkillContextCompMap>）应把 ActorStateComp 也加进去。原因：CheckState→TryPerformAction 现在缺 ActorStateComp 会拒绝，但它排在 CheckRecovery 和 CheckChannel 之后，那两步可能已经打断了在途施法，等到 CheckState 才发现缺组件就违反了「校验前置」。三个组件在同一个创建点（InitializeActorCompsEventHandler）挂上，一起校验口径一致。
- 文档：docs/design/ecs-entt-misuse-audit-20261015.md 附录里 F034、F035、F057、F074 的「HEAD 状态」填「已修」。docs/design/ecs-component-access-rules.md 的「Problematic uses」表中 actor_attribute_calculator.cpp 和 actor_action_state.cpp 两行标记已修，并写明创建点是 ActorEventHandler::InitializeActorCompsEventHandler，DerivedAttributesComp 的创建点是 PlayerAttributeSystem::InitializeOnLoad。docs/PROGRESS.md 追加一条进度，注明 Java 版不适用（服务端 ECS 内部实现）。
- 以后新增 NPC 或怪物的创建点时，必须触发 InitializeActorCompsEvent，或者挂齐 ActorStateComp、MoveSpeedComp、CombatStateFlagsComp、CooldownTimeListComp、SkillContextCompMap。否则施法会被拒（kThisEntityIsInvalid），移速重算和战斗状态同步会记 ERROR 后跳过。目前生产代码里没有 NPC 创建点。
- 资产通道入口层接入 PetSystem::RemovePetForTrade / RestorePetFromSnapshot 时，kThisEntityIsInvalid 应与「账本组件缺失」同口径处理（RETRY、不记账），不要映射成终局 REJECTED。
- （建议）cpp/nodes/scene/handler/rpc/player/player_attribute_handler.cpp 和 player_pet_handler.cpp：GetPanel / GetPetList 遇到组件缺失时，现在会返回一个没有错误码的空面板或空列表。可以在 handler 里先判断组件是否存在，缺了就回 kThisEntityIsInvalid，让客户端能区分。

**风险**：
- 没有编译、没有跑任何测试，全部待 Codex 按 codex_steps 验证。
- needs_outside 第 1 项（player_equip_test.cpp 夹具）落地之前，bag_test 里 PlayerEquipTest 会有 6 条用例变红。这是预期的契约变化：Recalculate 不再替没加载完的实体补建组件。
- 生产行为变化：只有在组件缺失、不变量已被破坏时才会触发。届时属性/宠物写入口返回 kThisEntityIsInvalid（故障码），以前是静默补建再继续；面板和宠物列表 RPC 返回空内容且没有错误码；PushPanel / PushList 不推送；BuildBattleSnapshot 不带宠物。走 InitPlayerFromAllData 的正常玩家不受影响。
- 施法的「校验前置」只完成了 ActorActionStateSystem 内部这一层。在 skill.cpp 把 ActorStateComp 纳入前置检查（needs_outside 第 2 项）之前，缺 ActorStateComp 的施法者要到 CheckRecovery / CheckChannel 之后才被拒，在途的后摇或引导可能已经被打断。按现在的创建点，正常玩家不会缺这个组件。
- ActorAttributeCalculatorSystem::Update 在 view 遍历中移除当前实体的被遍历组件，这是 EnTT 文档明确允许的。约束是：重算函数今后不能在遍历中删除别的实体的 AttributeDirtyFlagsComp，或销毁别的实体，否则是未定义行为。
- 移速语义有一处小变化：标了 kMoveSpeed 而实体没有 BuffListComp 时，MoveSpeedComp 会被写成 0，以前是保留旧值。目前 MoveSpeedComp 在生产里没有读者。
- player_equip_attribute_test.cpp 里两条存档往返用例新增的 CompletePersistedComps，是为了适配另一会话已改成 fail-closed 的 Marshal。如果那批改动被撤回，这段补齐仍能编译、也不改变预期，只是注释里「缺组件整份拒绝」的说法会不准。
- 生成代码 SetActorBaseAttributesS2CAttrDirtyBit 仍对 ActorBaseAttributesS2CDirtyMaskComp 用 get_or_emplace，并且同步之后只清位、不摘组件，与 F035 是同一模式。这是生成产物（§3），本批没有改，需要另起批次改 protogen 模板。

**该组代理给的验证步骤**：
- 工作目录 D:\luyuan\xuanming-server-mmo-cpp，使用 VS x64 开发者命令行。同一工作区有其他会话未提交的改动，不要 git stash、reset 或 checkout，也不做额外清理。MSBuild 一律 /m:1 串行，按下列顺序执行。
- 1) msbuild cpp\libs\services\scene\scene.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64 —— 期望产出 scene.lib，0 error。注意 game.sln 里有两个工程都叫 scene，所以直接指定 vcxproj。
- 2) msbuild cpp\nodes\scene\scene.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64 —— 用来验证 actor_event_handler.cpp 守护段新增的 include 和 emplace 能编译，期望 0 error。
- 3) msbuild cpp\tests\bag_test\bag_test.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64 —— 期望产出 build\cpp\tests\bag_test.exe。
- 4) 在仓库根目录运行：build\cpp\tests\bag_test.exe --gtest_filter=ActorAttributeCalculatorTest.*:ActorActionStateTest.*:PetComponentOwnershipTest.*:PlayerEquipAttributeTest.* —— 通过标准：全部 PASSED，其中包括本批新增的 17 条，以及 PlayerEquipAttributeTest 下原有的全部用例，尤其是两条 ReloadFromTheDatabaseRecord*。
- 5) 不带 filter 运行完整的 build\cpp\tests\bag_test.exe。预期：PetIdSegmentTest.* 和 PetTradeAssetTest.* 必须绿。PlayerEquipTest 下 6 条在 needs_outside 第 1 项落地之前会红：EquipMovesTheSameInstanceIntoTheWeaponSlot、UnequipReturnsTheSameInstanceToTheInventory、EnhanceOnWornEquipmentRecalculatesAttributes、EnhanceFailureWithLevelLossOnWornEquipmentRecalculates、InlayGemOnWornEquipmentRecalculatesAttributes、RemoveGemOnWornEquipmentRecalculatesAttributes。原因是夹具玩家没有 PlayerAttributeComp 和 DerivedAttributesComp，Recalculate 现在会 fail-closed。除这 6 条外如有别的红，记下用例名和断言输出。
- 6) msbuild cpp\tests\skill_test\skill_test.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64，然后运行 build\cpp\tests\skill_test.exe；接着 msbuild cpp\tests\buff_test\buff_test.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64，然后运行 build\cpp\tests\buff_test.exe。这两个工程都链接 scene.lib，覆盖 CheckState→TryPerformAction 和 buff→标脏的路径。通过标准：全绿。
- 7)（可选）验证修复前会红：用 git worktree add <临时目录> HEAD 建一个独立 worktree，只把本批两个测试文件拷进去，在那里编译 bag_test 并跑第 4 步的 filter。期望 tests_added 里标为修复前会红的 13 条失败，其余 4 条通过。不要在当前工作区做任何回退。
- 失败时保留以下信息，命令不要连续重试：msbuild 第一条 error 的完整行（含文件和行号）；gtest 失败用例名及断言输出；如果是 EnTT 断言或崩溃，保留调用栈前 10 帧。
- （修正轮补充）工作目录 D:\luyuan\xuanming-server-mmo-cpp，使用 VS x64 开发者命令行。同一工作区有多个会话的未提交改动，不要 git stash、reset 或 checkout，也不做额外清理。MSBuild 一律 /m:1 串行，按下列顺序执行。
- （修正轮补充）1) msbuild cpp\libs\services\scene\scene.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64。期望产出 scene.lib，0 error。game.sln 里有两个工程都叫 scene，所以直接指定 vcxproj。
- （修正轮补充）2) msbuild cpp\nodes\scene\scene.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64。期望 0 error、0 warning-as-error。这一步验证 player_attribute_handler.cpp 和 player_pet_handler.cpp 在 BuildPanel / BuildList 改为返回 uint32_t、且返回值被丢弃时仍能编译。
- （修正轮补充）3) msbuild cpp\tests\bag_test\bag_test.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64。期望产出 build\cpp\tests\bag_test.exe。
- （修正轮补充）4) 在仓库根目录运行 build\cpp\tests\bag_test.exe --gtest_filter=ActorAttributeCalculatorTest.*:ActorActionStateTest.*:PetComponentOwnershipTest.*:PlayerEquipAttributeTest.*。通过标准：全部 PASSED。其中包括本轮新增的 PlayerEquipAttributeTest.BuildPanelReportsInvalidEntityAndSucceedsForALoadedPlayer、PetComponentOwnershipTest.BuildListReportsInvalidEntityAndSucceedsOnceLoaded，改过断言的 WriteEntriesRejectAnEntityWithoutTheSaveComponent、GmSetLevelRejectsAnEntityWithoutALevelComponent、PanelOfAnUnloadedEntityIsEmptyAndCreatesNothing、ReadAndRecalculateEntriesCreateNothing、WriteEntriesAndTradePrimitivesRejectWithoutTheComponent，以及两条 ReloadFromTheDatabaseRecord*。
- （修正轮补充）5) 不带 filter 运行完整的 build\cpp\tests\bag_test.exe。预期：PlayerEquipTest 下 6 条在 needs_outside 夹具改动落地之前会红，分别是 EquipMovesTheSameInstanceIntoTheWeaponSlot、UnequipReturnsTheSameInstanceToTheInventory、EnhanceOnWornEquipmentRecalculatesAttributes、EnhanceFailureWithLevelLossOnWornEquipmentRecalculates、InlayGemOnWornEquipmentRecalculatesAttributes、RemoveGemOnWornEquipmentRecalculatesAttributes。除此之外应全绿，尤其是 PetIdSegmentTest.*、PetTradeAssetTest.*、PlayerFeatureHandlerTest.*。如有其他红，记下用例名和断言输出。
- （修正轮补充）6) msbuild cpp\tests\skill_test\skill_test.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64，然后运行 build\cpp\tests\skill_test.exe。接着 msbuild cpp\tests\buff_test\buff_test.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64，然后运行 build\cpp\tests\buff_test.exe。通过标准：全绿。这两个工程同时受并发会话对 skill / buff / actor_action_state 改动的影响，失败时请同时记下用例所在文件。
- （修正轮补充）7)（可选）验证修复前会红：用 git worktree add <临时目录> HEAD 建独立 worktree，只把本批两个测试文件拷进去编译 bag_test。期望对 BuildPanel / BuildList 返回值做断言的 4 条用例编译失败（HEAD 上二者返回 void），这就是「红」。不要在当前工作区做任何回退。
- （修正轮补充）失败时保留以下信息，命令不要连续重试：msbuild 第一条 error 的完整行（含文件和行号）；gtest 失败用例名和断言输出；如果是 EnTT 断言或崩溃，保留调用栈前 10 帧。

### 组：战斗：buff 与技能（combat）

**修了**：C003:no_caster 的 buff 把 entt::null 写进 processed_caster,叠层永远匹配不上、max_layer 失效。现在新建和叠层共用 ProcessedCasterOf(no_caster 记 0,跨施法者叠层),并把新条目层数从 0 改为 1,对齐回合引擎口径、F037:buff.cpp 里 AddOrUpdateBuff、HandleExistingBuff、MarkBuffForRemoval 三处 get_or_emplace 已去掉。BuffListComp 改为必需组件,缺失时在副作用前 fail-closed;待删集合改为「标记时建、帧末处理完即摘」、F051:施法路径懒建四种定时器组件、CheckTimerPhase 每次施法再摘的问题已修。改为相位开始 emplace_or_replace、结束或打断 remove;顺带修了引导被打断后 ChannelIntervalTimerComp 的 RunEvery 不停的泄漏、技能链缺必需组件静默放行(F044／F050 剩余部分):CheckSkillPrerequisites 最先统一检查 CooldownTimeListComp 与 SkillContextCompMap,缺失时在相位打断和加状态之前返回 kServiceUnavailable。AddSkillContext、StartCooldown、HandleSkillFinish、FindCasterSkillContext 以及 CalculateSkillDamage 的施法者 LevelComp,缺失时一律记 LOG_ERROR,不再静默
- 推迟 skill.cpp IsTargetDead／DealDamage 的目标 BaseAttributesComp,AddTargetSkillContext／RemoveTargetSkillContext 的目标 SkillContextCompMap：这些是跨实体查询,目标的组件构成没有保证,仓库规则就是用 try_get。它们不属于施法者缺必需组件,保持原样。
- 推迟 CheckBuff 的 ECS_GET_OR_RETURN(CombatStateCollectionComp, kSuccess)：复核后不成立:CombatStateCollectionComp 是按需状态,第一次加战斗状态时才由 CombatStateSystem::AddCombatState 创建。组件不存在表示身上没有战斗状态,放行是正确的。只补了注释说明,逻辑不改。
- 推迟 CreateSkillContext 里 SkillContextCompMap 缺失时用 kEmptyContexts 兜底：前置校验已保证组件存在,这个兜底分支到不了。为避免无关改动,不在这一批收紧。
- 推迟 F042／F043／F045(相位组件 in_place_delete 只靠约定、SkillContextCompMap 语义混用、目标侧镜像上下文泄漏)：不在这一批的任务范围内。HandleGeneralSkillSpell 遇到施法者冻结时早退,会留下上下文不收尾,这属于 F045 一类,也没有改。

**改动**：
- `cpp/libs/services/scene/combat/buff/system/buff.cpp`：新增匿名命名空间里的 kNoBuffCaster=0 和 ProcessedCasterOf(C003)。AddOrUpdateBuff:入口用 all_of 检查 BuffListComp,缺了 MAKE_ERROR_MSG(kServiceUnavailable) 拒绝;去掉 get_or_emplace;驱散之后重新 try_get 列表;processed_caster 改由 ProcessedCasterOf 计算;新条目 set_layer(1)。HandleExistingBuff:no_caster 的 buff 没有上下文也照常叠层,按 processedCaster 匹配,列表改用 try_get 只读。MarkBuffForRemoval:先确认 buff 在列表里,再按需创建待删集合。RemovePendingBuffs:处理完 remove<BuffPendingRemoveBuffs>,不再只 clear
- `cpp/libs/services/scene/combat/buff/system/buff.h`：给 AddOrUpdateBuff、MarkBuffForRemoval、HandleExistingBuff 补上契约注释,写明返回值、必需组件和只读语义
- `cpp/libs/services/scene/combat/skill/system/skill.cpp`：新增 CheckRequiredSkillComps(放在 CheckSkillPrerequisites 最前面)和 EndAllSkillPhases。StartCooldown、AddSkillContext、HandleSkillFinish、FindCasterSkillContext、CalculateSkillDamage(施法者 LevelComp)在缺组件时改为 LOG_ERROR。SetupCastingTimer 先判定技能类型,再 emplace_or_replace,回调里先摘 CastingTimerComp 再施放。HandleSkillRecovery 改为 emplace_or_replace,回调里摘 RecoveryTimerComp。HandleChannelSkillSpell 改为 emplace_or_replace。HandleChannelFinish 把两个引导组件一起摘掉。CheckTimerPhase 改为只读:残留组件记错误后收掉,打断时调 EndAllSkillPhases。lambda 一律改用 init-capture。CheckBuff 补注释说明按需状态
- `cpp/libs/services/scene/combat/skill/system/skill.h`：给 CheckSkillPrerequisites 补契约注释:最先检查必需组件,缺失时返回 kServiceUnavailable,且不产生副作用
- `cpp/libs/services/scene/combat/skill/comp/skill_comp.h`：文件头补充相位组件的生命周期契约(开始 emplace_or_replace,结束或打断 remove),并说明它们和必需组件的区别
- `cpp/tests/buff_test/buff_test.cpp`：夹具配表新增 9005(no_caster=1、max_layer=3)和 9006(no_caster=0、max_layer=3)。夹具拆成 BuffTableFixture 基类,派生出 BuffIntervalThinkTest、BuffStackingTest、BuffListRequiredTest。新增 CreateBuffActor(模拟 actor 初始化,挂 BuffListComp)和 ContextFromCaster,原有 3 个用例改用 CreateBuffActor。新增 5 个回归用例
- `cpp/tests/skill_test/skill_test.cpp`：新增 5 个回归用例:必需组件前置拒绝、引导打断收尾、引导正常结束收尾、无前摇技能不挂组件,以及由 EventLoop 驱动的前摇和后摇相位组件生命周期
- （修正轮）`cpp/libs/services/scene/combat/buff/system/buff.cpp`：BuffListComp 从「必需组件、缺失拒绝」改为按需状态。去掉入口处的 all_of 拒绝，以及驱散后的 kServiceUnavailable 分支。改为在免疫、驱散、叠层都通过后，先复核 valid(parent)（对已销毁实体 emplace 是未定义行为），再用 get_or_emplace 延迟创建，作为唯一创建入口。HandleExistingBuff 缺列表时直接返回 false，不打错误日志。Update 在 RemovePendingBuffs 之后，列表为空就 remove<BuffListComp>（帧末、当前实体）。ProcessedCasterOf 的注释如实说明与回合引擎的差异。AddSubBuffsWithoutCheck 登记了 no_caster 子 buff 的归属缺口。C003 的修复（ProcessedCasterOf、新条目 layer=1）和待删集合的生命周期保持上一版
- （修正轮）`cpp/libs/services/scene/combat/buff/system/buff.h`：类注释写明 BuffListComp 按需状态的两个出入口：AddOrUpdateBuff 首次插入时创建，Update 帧末空表时摘除。Update、AddOrUpdateBuff、HandleExistingBuff 的契约注释改为按需口径
- （修正轮）`cpp/libs/services/scene/combat/skill/system/skill.cpp`：CheckTimerPhase 的残留分支改为先 HandleSkillFinish(残留相位的 skillId)、再 EndAllSkillPhases，与打断分支对称，函数头注释同步更新。CalculateFinalDamage 拆开判断施法者和目标：施法者缺 BaseAttributesComp 记 LOG_ERROR 后返回 0，目标缺失维持静默
- （修正轮）`cpp/libs/services/scene/combat/skill/comp/skill_comp.h`：相位生命周期注释补上「发现定时器不在跑的残留组件时，同样收掉上下文并摘除全部相位」
- （修正轮）`cpp/tests/buff_test/buff_test.cpp`：删掉预挂 BuffListComp 的 CreateBuffActor，所有用例改回裸 create()，与生产代码一致。BuffListRequiredTest 改名为 BuffListLifecycleTest。原「缺列表被拒」用例换成 AddOrUpdateBuff_FirstBuffCreatesBuffList。新增 AddOrUpdateBuff_PureDispelOnBareEntity_DoesNotCreateBuffList（HEAD 会红：驱散判断之前就 get_or_emplace）和 Update_RemovesEmptiedBuffListAtFrameEnd（HEAD 与上一版都会红：从不摘空表）。MarkBuffForRemoval 用例改为挂两条、删一条，验证列表保留、待删集合摘除。SubBuffDispelsOwner_ViaFrameUpdate 的断言改为帧末 BuffListComp 已被摘除。文件头说明同步更新
- （修正轮）`cpp/tests/skill_test/skill_test.cpp`：新增 SkillUtilTest.CheckChannel_StalePhase_FinishesItsCastAndEndsAllPhases：未武装的 ChannelFinishTimerComp、在跑的 ChannelIntervalTimerComp，加上这次施法的上下文，CheckChannel 后三者都要收掉；修复前（含 HEAD）会红。GeneralSkill_PhaseComponentsEndWithTheirPhase 增加 cast_point 和 recovery_time 为 0 的夹具前提断言

**新增测试**：
- `cpp/tests/buff_test/buff_test.cpp` `BuffStackingTest.NoCasterBuff_StacksAcrossCastersUpToMaxLayer`：修复前为什么红：修复前 processed_caster 存的是 entt::null,和施法者整数比较永远不等;无上下文时又直接返回 false。4 次施加会新建 4 条,ASSERT_EQ(1u, buffList.size()) 失败,第 2 到 4 次的 buffId 也不是 UINT64_MAX
- `cpp/tests/buff_test/buff_test.cpp` `BuffStackingTest.CasterScopedBuff_StacksPerCasterFromLayerOne`：修复前为什么红：修复前新条目的 layer 是 0。同一施法者挂两次只有 1 层,另一施法者的条目是 0 层,EXPECT_EQ(2u)／EXPECT_EQ(1u) 失败
- `cpp/tests/buff_test/buff_test.cpp` `BuffListRequiredTest.AddOrUpdateBuff_MissingBuffList_RejectedWithoutCreatingIt`：修复前为什么红：修复前 get_or_emplace 会懒建 BuffListComp 并返回 kSuccess。断言要求返回 kServiceUnavailable、buffId 为 UINT64_MAX、实体上没有 BuffListComp,全部失败
- `cpp/tests/buff_test/buff_test.cpp` `BuffListRequiredTest.HandleExistingBuff_DoesNotCreateBuffList`：修复前为什么红：修复前上下文非空时会 get_or_emplace<BuffListComp>,裸实体被挂上组件,EXPECT_FALSE(all_of<BuffListComp>) 失败
- `cpp/tests/buff_test/buff_test.cpp` `BuffListRequiredTest.MarkBuffForRemoval_PendingSetLivesOnlyUntilFrameEnd`：修复前为什么红：修复前对任何实体都 get_or_emplace 待删集合,裸实体被挂上组件;帧末处理完也只 clear 不摘。两处 EXPECT_FALSE(all_of<BuffPendingRemoveBuffs>) 都失败
- `cpp/tests/skill_test/skill_test.cpp` `SkillUtilTest.CheckSkillPrerequisites_MissingSkillContextMap_RejectsBeforeSideEffects`：修复前为什么红：修复前缺 SkillContextCompMap 不会被拒。1 号是立即技能,CheckRecovery 先把在途后摇打断并摘掉 RecoveryTimerComp,之后 CheckState 因 ActorActionState 表未加载返回 kInvalidTableId。返回码不是 kServiceUnavailable,ASSERT_NE(nullptr, stillRecovering) 也失败
- `cpp/tests/skill_test/skill_test.cpp` `SkillUtilTest.CheckChannel_ImmediateInterrupt_EndsWholeChannelPhase`：修复前为什么红：修复前打断时只摘 ChannelFinishTimerComp,ChannelIntervalTimerComp 还在,EXPECT_FALSE(all_of<ChannelIntervalTimerComp>) 失败
- `cpp/tests/skill_test/skill_test.cpp` `SkillUtilTest.HandleChannelFinish_EndsChannelPhase`：修复前为什么红：修复前 HandleChannelFinish 只摘 think 定时器,ChannelFinishTimerComp 留在实体上,EXPECT_FALSE(all_of<ChannelFinishTimerComp>) 失败
- `cpp/tests/skill_test/skill_test.cpp` `SkillUtilTest.SetupCastingTimer_NoCastPhaseSkill_DoesNotStartPhase`：修复前为什么红：修复前 SetupCastingTimer 先 get_or_emplace<CastingTimerComp>,再判断技能类型。技能 id 不存在时会留下一个没武装的组件,EXPECT_FALSE(all_of<CastingTimerComp>) 失败
- `cpp/tests/skill_test/skill_test.cpp` `SkillUtilTest.GeneralSkill_PhaseComponentsEndWithTheirPhase`：修复前为什么红：修复前前摇和后摇回调都不摘相位组件,要等下一次施法的 CheckTimerPhase 才摘。技能走完、上下文出表后,CastingTimerComp 与 RecoveryTimerComp 仍在实体上,两条 EXPECT_FALSE 失败。用例用轮询定时器在上下文出表后退出,另有 5 秒兜底,不按固定时长等待

**对抗审查**：issues
- [high] `cpp/libs/services/scene/combat/buff/system/buff.cpp:145`：BuffListComp 被改成必需组件，缺失时 fail-closed。但在当前工作区里，生产代码没有任何地方创建它。全仓 grep emplace<BuffListComp> 只命中测试；actor_event_handler.cpp 的 InitializeActorCompsEventHandler(第 35-43 行)只挂了 CooldownTimeListComp、SkillContextCompMap、ActorStateComp、MoveSpeedComp、CombatStateFlagsComp。所以这一组单独落地后，玩家身上每次 AddOrUpdateBuff 都返回 kServiceUnavailable 并打印栈：技能 effect、子 buff、target_sub_buff 全部失效，skill 1 一次命中会打 8 份栈。buff_test 的 CreateBuffActor 自己挂了组件，把这个缺口盖住了。实施者已在 needs_outside 里写明「必须同批落地」，但按现在的工作区，这一组单独交付就是功能回退。
  - 建议：把 actor_event_handler.cpp 守护段里的 tlsEcs.actorRegistry.get_or_emplace<BuffListComp>(actor);(以及 #include "combat/buff/comp/buff_comp.h")与本组设为同一个落地单元，由有权限的会话补上之后，本组才能合入或交 Codex 验证。在 codex_steps 里加一步：scene 节点起来后，用一个真实玩家放 1 号技能(或 GM 加 buff)，确认没有 "missing BuffListComp" 的错误日志。
- [low] `cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:40`：这里的注释写着「BuffListComp 是按需组件:没有它就是身上没有 buff」，而本组在 buff.cpp/buff.h 里把它定成「必需组件，只在 actor 初始化挂」。同一个组件在仓库里有两套互相矛盾的契约，后来的人照哪一份改都会把另一份弄错。
  - 建议：BuffListComp 的初始化落地时，把这条注释改成「必需组件，缺失属不变量破坏」(try_get 读法可以不动)。这是列表外的文件，交给负责它的会话或写进 needs_outside。
- [low] `cpp/libs/services/scene/combat/skill/system/skill.cpp:545`：CheckTimerPhase 的「残留组件」分支只 remove<PhaseComp>。如果残留的是 ChannelFinishTimerComp，ChannelIntervalTimerComp 的 RunEvery 会继续跑；残留相位的 skillId 也没有走 HandleSkillFinish，上下文泄漏。这和本组为打断路径修好的问题是同一类。按新的生命周期，这个分支在生产里基本到不了，所以影响小，但它和打断分支的收尾不对称。
  - 建议：残留分支和打断分支一样处理：先记下 phase->skillId，调 SkillSystem::HandleSkillFinish(casterEntity, skillId)，再 EndAllSkillPhases(casterEntity)，最后返回 kSuccess。
- [low] `cpp/libs/services/scene/combat/buff/system/buff.cpp:58`：ProcessedCasterOf 的注释说「与回合引擎 TurnBattleEngine::StackOrRefreshExistingBuff 的口径一致」，但只有写进去的值一致，叠层行为并不一致。no_caster=0 且没有技能上下文时，HandleExistingBuff(第 330 行)直接返回 false，每次新建一条；回合引擎会把 caster_id==0 的条目互相叠层。注释会让后来的人以为口径已经完全对齐(实施者在 risks 里承认这是有意保留的差异)。
  - 建议：把注释改成如实描述：processed_caster 的取值与回合引擎一致；「按施法者隔离、但没有上下文」的情形仍然每次新建，和回合引擎不同，是有意保留的差异(子 buff 链路依赖它)。
- [low] `cpp/libs/services/scene/combat/buff/system/buff.cpp:566`：C003 修好之后，no_caster 的子 buff 会跨 owner 叠到同一条上，AddOrUpdateBuff 对叠层返回 buff_id=UINT64_MAX。AddSubBuffsWithoutCheck 见到 UINT64_MAX 就 continue，于是只有最先挂上的 owner 在 sub_buff_list_id 里登记了这条子 buff。owner1 被移除时会经 RemoveSubBuff 把共享条目整条删掉，owner2 还活着却失去了子 buff；owner2 后被移除时则什么也不删。以前每个 owner 各占一条，没有这个问题。目前配表里没有 no_caster=1 的行，所以暂时没有影响。
  - 建议：不必在本组修，但要在 risks 或审计文档里登记：no_caster 的 buff 一旦被配成 sub_buff，需要先决定子 buff 的归属语义(引用计数，或者叠层时也登记到 owner)，再开放这类配表。
- [low] `cpp/libs/services/scene/combat/skill/system/skill.cpp:700`：同一条伤害链里口径不一致：CalculateSkillDamage 对施法者缺 LevelComp 改成了 LOG_ERROR，CalculateFinalDamage 在施法者缺 BaseAttributesComp 时仍然静默 return 0.0。BaseAttributesComp 同样是必需组件，由 player_database_loader.cpp:112 的 Unmarshal 创建。这恰好属于本组要清理的「把缺必需组件当成功」。
  - 建议：把施法者和目标分开判断：施法者缺 BaseAttributesComp 时 LOG_ERROR(带 caster、skill_table_id)再返回 0；目标缺失属于跨实体查询，维持静默。
- [low] `cpp/tests/skill_test/skill_test.cpp:412`：GeneralSkill_PhaseComponentsEndWithTheirPhase 由真实 EventLoop 和墙钟定时器驱动(0 秒定时器、0.01 秒轮询、5 秒兜底)，并且隐式依赖 generated/tables/skill.json 里 2 号技能的 cast_point 和 recovery_time 都是 0(当前核对为 0.0)。AGENTS §11.4 要求单元测试不依赖真实墙钟。配表一改成非 0(单位是秒)，用例就会在 5 秒兜底后变红，而且报错不指向配表。
  - 建议：加一条前提断言：ASSERT_EQ(0.0, skillRow->cast_point()) 和 ASSERT_EQ(0.0, skillRow->recovery_time())，失败信息写明「夹具前提」。或者改用本地构造、cast_point=recovery_time=0 的 SkillTable 驱动 SetupCastingTimer，HandleSkillRecovery 仍会按上下文去查表，所以上下文的 skilltableid 要指向一行确定是 0 的配表。

**修正轮**：按审查意见逐条复核了 buff 与技能这一组，并在负责的文件里修完。全部改动只做了静态阅读，未编译、未运行任何测试，待 Codex 验证。

【high 问题成立，换了修法】当前工作区的生产代码里没有任何地方创建 BuffListComp：actor_event_handler.cpp 的初始化只挂了冷却表、上下文表、ActorStateComp、MoveSpeedComp、CombatStateFlagsComp。另一个会话已在 actor_attribute_calculator.cpp 里写明「BuffListComp 是按需组件，没有它就是身上没有 buff」。仓库现行规则 docs/design/ecs-component-access-rules.md 的判据（「没有」本身有明确含义）也把它归为按需状态，概述里还点名 buff 是 NPC 桩体可以省掉的组件。上一版把它定成必需组件、缺了就拒绝，单独落地就是功能回退，还和另一份契约冲突。所以这次把 BuffListComp 改成按需状态，生命周期两端都收口：
- 开始：AddOrUpdateBuff 在免疫、驱散、叠层判断全部通过、真正要插入新条目时，用 get_or_emplace 创建，这是唯一的创建入口，已注明「此处延迟创建」。被拒绝、纯驱散、叠层的施加都不改变组件集合。容器不能用 emplace_or_replace，否则会清掉已有的 buff。
- 结束：BuffSystem::Update 在帧末处理完周期 buff 和待删集合后，发现列表空了就 remove。之所以只在帧末摘，是因为 buff 到期或驱散时调用栈上还可能有人端着这张表的引用。在 EnTT 3.13 的 view 遍历中移除当前实体的被遍历组件是允许的，我对照过 view_iterator 的实现。
- 读路径一律 try_get，拿不到按「没有 buff」处理。HandleExistingBuff 不再打错误日志。

这样本组不再依赖任何列表外的改动，和 actor 组的注释也一致了。不要在 actor 初始化里挂 BuffListComp。

【low 问题】
- CheckTimerPhase 的残留分支改为和打断分支一样收尾：先 HandleSkillFinish，再 EndAllSkillPhases。
- ProcessedCasterOf 的注释改为如实说明：取值与回合引擎一致，但叠层行为有一处有意保留的差异。
- no_caster 子 buff 的归属缺口在 AddSubBuffsWithoutCheck 旁登记了说明。目前配表 20 行里没有 no_caster=1 的，暂不改逻辑。
- CalculateFinalDamage 里施法者缺 BaseAttributesComp 改为记 LOG_ERROR（这是持久数据，由 PlayerDatabaseMessageFieldsUnmarshal 创建）；目标缺失属于跨实体查询，维持静默。
- 由 EventLoop 驱动的用例加了 2 号技能 cast_point 和 recovery_time 为 0 的前提断言。

技能侧维持上一版的口径：冷却表和上下文表在 actor 初始化时挂上（HEAD 已有这两行），CheckRequiredSkillComps 在任何副作用之前 fail-closed。ChatGPT 第 5 条说「生产代码没有创建点」，那是旧状态，F044 已经在 actor 初始化里补上。真正剩下的缺口是缺组件时被静默放行，上一版已修。

另外发现上一版 codex_steps 有误：run_cpp_tests.ps1 的 -Filter 用的是 -like 通配，不支持正则，'buff_test|skill_test|...' 一个工程也匹配不上。已改为逐个工程执行。
- 未解决：[low，成立但暂不修] no_caster 的 buff 被配成 sub_buff 时归属语义有缺口：叠层后只有第一个 owner 登记，它被移除时会删掉共享条目。目前 generated/tables/buff.json 20 行里没有 no_caster=1 的行，属于潜伏问题。要先由策划或架构决定子 buff 的归属（引用计数，或叠层时也登记），已在 AddSubBuffsWithoutCheck 旁写明
- 未解决：[low，部分处理] GeneralSkill_PhaseComponentsEndWithTheirPhase 仍由真实 muduo EventLoop 驱动：0 秒定时器、0.01 秒轮询、5 秒兜底。TimerTaskComp 直接依赖 EventLoop，没有可注入的时钟，要彻底去掉墙钟需要改 TimerTaskComp，超出本组范围。已用前提断言消除对配表的隐式依赖
- 未解决：需要列表外处理（文档）：docs/design/ecs-entt-misuse-audit-20261015.md 里 C003、F037、F051 改为已修，并把 C003 从「仍未修」表移走。F037 的落地方式是「BuffListComp 按需状态：AddOrUpdateBuff 新建条目时延迟创建，帧末空表摘除」，不是在 actor 初始化挂上
- 未解决：需要列表外处理（文档冲突）：docs/design/ecs-component-access-rules.md 的分类表把 CooldownTimeListComp、SkillContextCompMap 列为「按需状态（读不到是正常分支）」，但代码事实是在 actor 初始化时创建，CheckCooldown 和 CheckRequiredSkillComps 缺失即 fail-closed（HEAD 的 CheckCooldown 已经这样）。建议把这两个移到「实体创建时（InitializeActorCompsEvent）创建、缺失记错误」那一行，并把 BuffListComp 加进按需状态的典型组件。本组没有权限改文档
- 未解决：需要列表外处理：不要在 actor_event_handler.cpp 的 InitializeActorCompsEventHandler 挂 BuffListComp，上一版 needs_outside 第 1 条作废。挂上也会在第一帧被摘掉，只会制造两套契约
- 未解决：风险（NPC 接入时）：生产代码目前没有创建 Npc 实体的地方。将来 NPC 若不走 InitializeActorCompsEvent，它作为施法者时会被 CheckRequiredSkillComps 以 kServiceUnavailable 拒绝并打栈。作为 buff 目标则不受影响（BuffListComp 已是按需状态）
- 未解决：行为变化需确认（沿用上一版）：buff 新条目的 layer 从 0 改为 1，max_layer 变为真正的「最多几层」；no_caster 的 buff 跨施法者叠层。若客户端按旧口径展示 layer 需要确认。Java 版对齐（AGENTS §12）仍待在 Java 仓库核对，并在 PARITY.md 登记。BuffListComp 的生命周期是服务端内部实现，与客户端无关
- 未解决：沿用上一版的 deferred（不在本组范围）：F042（相位组件 in_place_delete 只靠约定）、F043（SkillContextCompMap 语义混用）、F045（施法者冻结早退时上下文不收尾，以及目标侧镜像上下文泄漏）
- 未解决：docs/PROGRESS.md 追加条目（注明 Java 版本号）由收尾会话统一处理

**需要列表外改动**：
- 【必须同批落地,否则功能回退】cpp/nodes/scene/handler/event/actor_event_handler.cpp 的 InitializeActorCompsEventHandler(只改 ///<<< BEGIN WRITING YOUR CODE 守护段内):在现有两行 get_or_emplace<CooldownTimeListComp>／get_or_emplace<SkillContextCompMap> 旁边加 tlsEcs.actorRegistry.get_or_emplace<BuffListComp>(actor);,并在文件顶部守护段的 include 区加 #include "combat/buff/comp/buff_comp.h"。原因:F037 把 BuffListComp 定为必需组件,AddOrUpdateBuff 不再懒建;不加这一行,玩家身上所有 buff 都会被 kServiceUnavailable 拒绝。建议与现有两行保持一致,用 get_or_emplace,因为该事件可能对同一实体重复触发(重新落地),改成 emplace 会在第二次时断言。
- docs/design/ecs-entt-misuse-audit-20261015.md:C003、F037、F051 的「HEAD 状态」改为已修,并把 C003 从「仍未修」表里移走;docs/design/ecs-component-access-rules.md 里 skill.cpp／buff.cpp 那几行 Problematic uses 已过时,需要更新。docs/PROGRESS.md 追加一条(注明对应的 Java 版本号)。
- Java 版对齐(AGENTS §12,客户端可见行为):no_caster 的 buff 跨施法者叠层;buff 新条目从第 1 层起、封顶 max_layer;技能施法者缺冷却表或上下文表时拒绝施法。要到 Java 仓库核对并在 PARITY.md 登记。审计文档也注明了 C003 在 Java 版是否等价尚未核对。
- cpp/libs/services/scene/battle/system/player_battle.cpp:1032 已核对,无需改动。那里读的是 BuffComp.caster()(不是 processed_caster),0 表示无施法者;本次没有改 caster 字段的写法,无上下文时仍然不设置(=0),约定一致。layer 那里用 std::max(layer,1),对新的从 1 起算也兼容。
- tip 码:缺必需组件沿用已有的 kServiceUnavailable(common_error_tip,和 CheckCooldown 现有口径一致),不需要新增码,也没有手写数字。

**风险**：
- 硬依赖:needs_outside 第 1 条(actor 初始化挂 BuffListComp)不落地的话,生产环境所有给玩家加 buff 的操作都会失败,并且每次命中都打 MAKE_ERROR_MSG 栈日志。skill_test／buff_test 测不到这一点,因为测试自己挂了组件。
- 行为变化:buff 新条目的 layer 从 0 改为 1,所有 buff 的层数整体 +1,max_layer 的语义变成真正的「最多几层」。如果有客户端展示或下游按旧口径读 layer,需要确认。回合战斗快照那边的 std::max(layer,1) 不受影响。
- 行为变化:no_caster 的 buff 现在会跨施法者叠层。按审计说法,现有配表里没有 no_caster=1 的行,所以当前没有实际影响。
- 有意保留的口径差异:按施法者隔离(no_caster=0)且没有技能上下文的 buff,仍然是每次新建一条,不叠层。回合引擎会把 caster_id=0 的条目互相叠层。如果要完全对齐,需要另行决策;它会影响子 buff 链路(buff_test 里 16 个子 buff 的用例依赖现有行为)。
- 以后如果引入刻意不挂 BuffListComp 的 NPC 桩体,技能命中它们时 AddOrUpdateBuff 会按「不变量被破坏」记错误并打栈。到时要么给它们挂组件,要么调用方先过滤。
- 相位组件在自己的定时器回调里被 remove(前摇、后摇、引导结束)。这依赖 TimerTaskComp::OnTimer 调完 cb() 后不再访问 this,以及这些组件 in_place_delete=true;已在代码注释里写明。将来改 TimerTaskComp 时要回头复核。
- EndAllSkillPhases 在打断时摘掉施法者全部相位组件,前提是同一施法者同一时刻至多一次施法在途(由前置校验保证)。如果将来允许并行施法,需要改成按 skillId 摘除。
- GeneralSkill_PhaseComponentsEndWithTheirPhase 依赖真实 EventLoop,以及运行时配表 2 号技能的 cast_point／recovery_time 为 0。已加前提断言和 5 秒兜底,但它仍是由墙钟定时器驱动的用例。
- 全部改动只做了静态阅读,没有编译、没有运行任何测试,待 Codex 按 codex_steps 验证。

**该组代理给的验证步骤**：
- 前置:工作目录为仓库根 D:\luyuan\xuanming-server-mmo-cpp,配置 Debug|x64。MSBuild 一律串行 /m:1,不要并发;下面的工程按顺序逐个编译。
- 1) 编译场景共享逻辑库(含 skill.cpp／buff.cpp):<MSBuild.exe> cpp\libs\services\scene\scene.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal。MSBuild 路径可以用 tools/scripts/run_cpp_tests.ps1 里 Resolve-MSBuild 的候选路径。通过标准:0 错误,lib/ 下的 scene.lib 已更新。失败时保留完整编译错误(文件:行 + 错误码)。
- 2) 编译场景节点,确认链接没问题:<MSBuild.exe> cpp\nodes\scene\scene.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal。通过标准:0 错误。如果 needs_outside 第 1 条(actor_event_handler.cpp 挂 BuffListComp)已同批落地,这一步也会把它编进去。
- 3) 编译并运行三个测试工程:pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter 'buff_test|skill_test|cool_down_time_test'。脚本按 /m:1 串行编译,并同步运行时 DLL。通过标准:汇总表里三个工程全部 PASS,退出码 0。
- 4) 如需单独复跑或看明细(工作目录仍为仓库根,exe 在 build\cpp\tests\):build\cpp\tests\buff_test.exe --gtest_filter=BuffIntervalThinkTest.*:BuffStackingTest.*:BuffListRequiredTest.*;build\cpp\tests\skill_test.exe --gtest_filter=SkillUtilTest.*:TransferErrorMessageTest.*:EcsContextTest.*:PlayerSkillSystemTest.*;build\cpp\tests\cool_down_time_test.exe(本批未改,作为回归,带 sleep,约 10 秒)。通过标准:全部 [  PASSED  ]。新增用例 SkillUtilTest.GeneralSkill_PhaseComponentsEndWithTheirPhase 会真实运行 EventLoop,通常约 0.1 到 0.5 秒;5 秒兜底触发即为失败。
- 5) 失败时保留:失败用例名、断言输出、MAKE_ERROR_MSG 打印的错误码和栈。skill_test 的两条夹具前提断言(1 号技能 immediate、2 号是普通技能)如果失败,说明运行时配表与 generated/tables/skill.json 不一致,要单独报告,不要改用例。
- 6)(可选,验证修复前是红的)在另一个 git worktree 里检出 HEAD 版本的 skill.cpp／buff.cpp,配上本次的测试文件编译运行,预期上面 tests_added 列出的 10 个用例全部失败。不要在当前工作区用 git checkout／stash 回退。
- （修正轮补充）替换上一版第 3 步（原 -Filter 'a|b|c' 无效，run_cpp_tests.ps1 用 -like "*$Filter*" 通配匹配，不支持正则）。前置：工作目录为仓库根 D:\luyuan\xuanming-server-mmo-cpp，配置 Debug|x64，MSBuild 一律 /m:1 串行，不要并发跑下面任何两步
- （修正轮补充）1) 编译场景共享逻辑库：<MSBuild.exe> cpp\libs\services\scene\scene.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal。通过标准：0 错误。失败时保留「文件:行 + 错误码」原文
- （修正轮补充）2) 编译场景节点，确认链接：<MSBuild.exe> cpp\nodes\scene\scene.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal。通过标准：0 错误。本组已不依赖 actor_event_handler.cpp 的任何改动
- （修正轮补充）3) 逐个工程串行编译并运行，前一条结束再跑下一条：pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter buff_test；pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter skill_test；pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter cool_down_time_test。通过标准：每次汇总表 PASS、退出码 0
- （修正轮补充）4) 需要明细时，在仓库根执行：build\cpp\tests\buff_test.exe --gtest_filter=BuffIntervalThinkTest.*:BuffStackingTest.*:BuffListLifecycleTest.*（注意 suite 名已从 BuffListRequiredTest 改为 BuffListLifecycleTest）；build\cpp\tests\skill_test.exe --gtest_filter=SkillUtilTest.*:TransferErrorMessageTest.*:EcsContextTest.*:PlayerSkillSystemTest.*；build\cpp\tests\cool_down_time_test.exe（未改动，作回归，约 10 秒）。通过标准：全部 [  PASSED  ]
- （修正轮补充）5) 本轮新增或改动、需重点看结果的用例：BuffListLifecycleTest.AddOrUpdateBuff_FirstBuffCreatesBuffList、BuffListLifecycleTest.AddOrUpdateBuff_PureDispelOnBareEntity_DoesNotCreateBuffList、BuffListLifecycleTest.Update_RemovesEmptiedBuffListAtFrameEnd、BuffListLifecycleTest.MarkBuffForRemoval_PendingSetLivesOnlyUntilFrameEnd、BuffIntervalThinkTest.SubBuffDispelsOwner_ViaFrameUpdate、SkillUtilTest.CheckChannel_StalePhase_FinishesItsCastAndEndsAllPhases、SkillUtilTest.GeneralSkill_PhaseComponentsEndWithTheirPhase。如果 GeneralSkill 用例的夹具前提断言（cast_point 或 recovery_time 为 0、2 号是普通技能）失败，说明运行时配表与 generated/tables/skill.json 不一致，单独报告，不要改用例
- （修正轮补充）6) 失败时保留：失败用例名、断言输出、MAKE_ERROR_MSG 打印的错误码和栈；buff_test 若在 Update 中崩溃，保留调用栈（重点看 BuffSystem::Update 帧末 remove<BuffListComp> 那一行）
- （修正轮补充）7)（可选，验证修复前是红的）在另一个 git worktree 里，把本次的测试文件配上 HEAD 版本的 buff.cpp 和 skill.cpp 编译运行：预期 PureDispelOnBareEntity、Update_RemovesEmptiedBuffListAtFrameEnd、HandleExistingBuff_DoesNotCreateBuffList、CheckChannel_StalePhase_FinishesItsCastAndEndsAllPhases 等用例失败。不要在当前工作区 checkout 或 stash 回退
- （修正轮补充）8)（可选，联调）完整拉起本地 scene 节点后，让真实玩家对另一名玩家释放 1 号技能（effect 会挂 buff），确认目标拿到 buff，scene 日志里没有 kServiceUnavailable 的 MAKE_ERROR_MSG 栈；随后等 buff 全部到期，确认后续帧没有 BuffListComp 相关错误日志

### 组：货币与战斗结算账本（currency）

**修了**：F025、F036

**改动**：
- `cpp/libs/modules/currency/system/currency_system.cpp`：匿名命名空间新增 FindRequiredPlayerCurrencyComp（try_get 加 LOG_ERROR，绝不补建）。AddCurrency 在封禁检查之后、动余额之前确认组件；DeductCurrency 在冻结检查之后、扣余额之前确认组件。缺失返回 kInvalidParameter；删掉两处 try_get 之后再 get_or_emplace 的补建逻辑。
- `cpp/libs/modules/currency/system/currency_system.h`：在 AddCurrency / DeductCurrency 的返回值契约里写明：缺组件指 CurrencyComp 或 PlayerCurrencyComp 任一缺失，本系统只取不建，失败时余额未动。
- `cpp/libs/services/scene/battle/system/player_battle.cpp`：SettlementLedgerOf（get_or_emplace）换成 FindSettlementLedger（try_get，缺失记 ERROR，带 player_id、entity、battle_id、context）。AckSettlementPending 缺账本时不销账；RebuildBattleFreezeFromLock 和 RestoreBattleFreezeOnLogin 缺账本时跳过本次重建；ApplySettlementToEntity 在任何副作用之前缺账本即返回 false；登记账本时重新取指针，缺失只记 ERROR、仍返回 true（遵守应用缓存契约）。
- `cpp/libs/services/scene/battle/system/player_battle.h`：ApplySettlementToEntity 注释补充：缺账本时返回 false 且零副作用，不补建账本。
- `cpp/tests/currency_test/currency_test.cpp`：新增 2 条用例：缺 PlayerCurrencyComp 时 AddCurrency / DeductCurrency 都 fail-closed。
- `cpp/tests/bag_test/player_battle_settlement_test.cpp`：夹具 CreatePlayerEntity 照加载路径挂一本空的 BattleSettlementLedgerComp；ReloginOld… 和 LedgerFromDisk… 两条用例里的 emplace 改成 get（否则会重复 emplace 触发断言）；更新 FrozenPlayer… 的一条过时注释；新增 2 条缺账本用例。

**新增测试**：
- `cpp/tests/currency_test/currency_test.cpp` `CurrencyTest.AddCurrencyMissingPlayerCurrencyCompFailsClosed`：修复前为什么红：修复前 AddCurrency 遇到组件缺失会 get_or_emplace 补一本空的后继续入账：返回 kSuccess（期望 kInvalidParameter），余额变成 100（期望 0），组件被补建出来（期望不存在），三条断言都红。
- `cpp/tests/currency_test/currency_test.cpp` `CurrencyTest.DeductCurrencyMissingPlayerCurrencyCompFailsClosed`：修复前为什么红：修复前 DeductCurrency 先扣余额、再补建组件：返回 kSuccess（期望 kInvalidParameter），余额变成 60（期望 100），组件重新出现。最后这条用 ASSERT_FALSE，失败即退出用例，后面的 emplace 不会执行，测试进程不会被 entt 断言带崩。
- `cpp/tests/bag_test/player_battle_settlement_test.cpp` `PlayerBattleSettlementTest.MissingLedgerRejectsSettlementWithoutSideEffects`：修复前为什么红：修复前应用入口 get_or_emplace 一本空账本，判成“没应用”后照发奖励：返回 true（期望 false），金币 12（期望 0），击杀事件 2 条（期望 0），血量 80（期望 100），压了 1 次存盘（期望 0），账本被补建（ASSERT_FALSE 失败后退出用例，不会带崩进程）。
- `cpp/tests/bag_test/player_battle_settlement_test.cpp` `PlayerBattleSettlementTest.MissingLedgerAckDoesNotFabricateLedger`：修复前为什么红：让 battle_id 不匹配的迟到结算走 ApplySettlement 的销账入口。修复前 AckSettlementPending 会 get_or_emplace 补建空账本，再按“没应用”去销账；单测没有 Redis，命令发不出去，但账本组件已被挂上，any_of<BattleSettlementLedgerComp> 为 true（期望 false），断言红。

**对抗审查**：issues
- [low] `cpp/libs/services/scene/battle/system/player_battle.cpp:337`：FindSettlementLedger 的 ERROR 文本固定写着「operation skipped, pending retained」。但第 1817 行的 "record" 调用点是在副作用已经发生之后才查账本：金币、HP、击杀事件都已生效，函数随后返回 true。于是同一次故障会先打出一条「操作已跳过」的 metric=battle_settlement_ledger_missing，紧接着又打出一条「Settlement side effects applied」。两条日志互相矛盾，按 metric 检索的人会误以为这一局什么都没发。
  - 建议：让日志尾巴随 context 区分，例如由调用方传入结论文本；或者 "record" 不走 FindSettlementLedger，直接 try_get，然后只打下面那条「已应用但无法登记」的 ERROR。
- [low] `cpp/libs/services/scene/battle/system/player_battle.cpp:2271`：reaper 里的注释写「稳态下这个 view 是空的(销账一确认就摘条目)」。本次修复把 BattleSettlementLedgerComp 明确成每个玩家加载时必挂的组件，所以 view<BattleSettlementLedgerComp> 实际包含本节点全部在线玩家。逻辑仍然正确（applied_size()==0 时 continue），只是注释与新的不变量矛盾。这条注释是既有的，本次没有改到。
  - 建议：把注释改成「view 覆盖所有在线玩家(账本是必需组件)，稳态下条目为空，靠 applied_size()==0 跳过，开销与在线人数成正比」。

**需要列表外改动**：
- cpp/libs/services/scene/player/system/asset_op_system.cpp 的应用闸门（约第 797 行，只检查了 CurrencyComp）：建议同时检查 PlayerCurrencyComp，缺失时回 ASSET_OP_OUTCOME_RETRY / kAssetPlayerNotHere。否则一个“先发道具、再发货币”的包在缺该组件时会出现：道具已发、货币返回 kInvalidParameter，最后记成 kAppliedPartial 转人工；放在闸门里就能在任何改动之前整笔 RETRY。修复前同样场景是靠补建一本空组件“成功”的。
- cpp/libs/services/scene/player/system/player_database_loader.cpp：Marshal 第 159/163 行对 BattleSettlementLedgerComp / PlayerCurrencyComp 用 get_or_emplace，存盘路径仍会补建空组件，并用空账本、空欠款覆盖盘上数据，属于审计“存盘偷偷创建组件”那一组。Unmarshal 第 129 行的 get_or_emplace<PlayerCurrencyComp> 作为唯一创建点，应改成 emplace。这两处不在本组负责范围内。
- docs/design/ecs-entt-misuse-audit-20261015.md 附录表里 F025、F036 的状态列需要标“已修”。docs/PROGRESS.md 需要追加一条进度（另起一条，并注明 Java 版对应状态）。
- （可选）deploy/k8s/asset-channel-alerts.yaml：为新日志指标 metric=battle_settlement_ledger_missing 增加告警，和已有的 battle_settlement_ledger_evicted 同级。

**风险**：
- 未编译、未运行任何测试，结论全部来自静态阅读，必须等 Codex 按 codex_steps 验证。
- 行为变化：加载不完整、缺 PlayerCurrencyComp 的实体，货币增减从“补建后成功”变成 kInvalidParameter。按现有代码，这个组件只在加载时挂上、全仓没有任何地方 remove 它，正常玩家不可达；如果线上真触发，会表现为该玩家所有货币操作失败、ERROR 日志刷屏。这正是想要的 fail-closed，但需要关注日志。
- 冻结重建的取舍：缺账本时我选择“跳过重建”，而不是“照样冻结”。依据是 battle:lock 仍在 Redis 里挡住下一次排队，结算应用路径对缺账本同样 fail-closed。代价是在这种不变量已被破坏的状态下，玩家实体上没有 InBattleComp。这两条路径都依赖 Redis 回调，单测覆盖不到。
- 登记账本时缺组件仍返回 true：只在应用途中实体被拆掉时才会发生（理论上不可达）。此时只有进程内缓存能挡住重复发放，持久幂等丢失，靠 ERROR 日志交人工按流水核对。
- 日志新增 metric=battle_settlement_ledger_missing 字样，但告警规则 yaml 没有登记，目前只能靠日志检索发现。

**该组代理给的验证步骤**：
- 前置：在 D:\luyuan\xuanming-server-mmo-cpp 打开 VS Developer PowerShell（x64），配置 Debug|x64。所有 MSBuild 都必须串行 /m:1，不要并行开多个 msbuild。
- 1. 编译 modules（含 currency_system.cpp）：msbuild cpp\libs\modules\modules.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1。通过标准：0 error。
- 2. 编译 scene 库（含 player_battle.cpp，开了 /W3 且警告即错误）：msbuild cpp\libs\services\scene\scene.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1。通过标准：0 error、0 warning-as-error。
- 3. 编译测试工程：先 msbuild cpp\tests\currency_test\currency_test.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1，完成后再 msbuild cpp\tests\bag_test\bag_test.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1。产物在 build\cpp\tests\currency_test.exe 和 build\cpp\tests\bag_test.exe。
- 4. 依赖 currency/scene 库的节点工程（scene 节点等）需要按仓库既定顺序重编一次，确认没有链接错误。本次没有改任何函数签名，预期只是重编译。
- 5. 跑 currency_test：build\cpp\tests\currency_test.exe --gtest_filter=CurrencyTest.*:*AssetOp*。通过标准：新增的 CurrencyTest.AddCurrencyMissingPlayerCurrencyCompFailsClosed 和 CurrencyTest.DeductCurrencyMissingPlayerCurrencyCompFailsClosed 为绿，其余既有用例不变红。未设置 MMORPG_TEST_REDIS_ADDR 时，两条真 Redis 用例显示 SKIPPED 属正常。然后不带过滤器再跑一遍全量，退出码 0。
- 6. 跑 bag_test（先确认表文件在 bin/etc/base_deploy_config.yaml 的 table_data_directory 下，否则会 LOG_FATAL 导致整个进程 abort）：build\cpp\tests\bag_test.exe --gtest_filter=PlayerBattleSettlementTest.*:PlayerBattleSettlementItemTest.*:BattleSettlementLedgerRule.*:PendingSettlementContract.*:PlayerEquipTest.*:PlayerEquipAttributeTest.*。通过标准：新增的 MissingLedgerRejectsSettlementWithoutSideEffects 和 MissingLedgerAckDoesNotFabricateLedger 为绿，既有结算与装备用例全绿（装备强化会走 DeductCurrency）。之后不带过滤器跑全量，退出码 0；退出码非 0 却没有 [  FAILED  ] 行，说明中途 LOG_FATAL，不算通过。
- 7. 失败时保留：MSBuild 输出的第一条 error 及其上下文；gtest 的 [  FAILED  ] 列表；每条失败用例附近日志里的 LOG_ERROR（关键字 PlayerCurrencyComp missing / battle_settlement_ledger_missing）。不要连续自动重试。
- 8.（可选，用于拿红/绿证据，不要在共享工作区里 stash/checkout）：用 git worktree add 在仓库外新建一个基于 HEAD 的干净 worktree，只把上面两份测试文件拷过去，按 1-6 步编译运行。期望修复前 4 条新用例 FAILED（不是崩溃），其余用例绿。

### 组：场景入口与节点 registry 边界（scene_entry）

**修了**：F091、F094、F096、F087、F088、F093、C007、registration_manager.cpp:151 OnHandshakeReplied(与 F093/C007 同类，审计重点注意事项一节点名)
- 推迟 F094(后半句:LastActiveFrameComp 的 get)：审计判定无实害。该处在 scene_handler.cpp 的 ProcessClientPlayerMessage 守护段(现约 398 行),不在本组负责的 CreateScene / UpdateSessionDetail / RoutePlayerStringMsg 范围内，未改。

**改动**：
- `cpp/libs/engine/thread_context/node_context_manager.h`：新增 static constexpr IsValidNodeType 和 [[nodiscard]] TryGetRegistry(nodeType)(越界返回 nullptr)。GetRegistry 保留原签名，去掉只在 Debug 生效的 assert,改为任何构建都检查：越界时调用 .cpp 里的 OutOfRangeRegistry,打 LOG_ERROR 并返回私有成员 outOfRangeRegistry_。这个隔离 registry 不清空，以免嵌套越界时让持有者的引用悬空。SetGlobalEntity / GetGlobalEntity / GetOrCreateGlobalEntity 也改成越界时 LOG_ERROR,不读写越界内存，后两个返回 entt::null。Clear() 连隔离 registry 一起清。
- `cpp/libs/engine/thread_context/node_context_manager.cpp`：新增 LogOutOfRange(LOG_ERROR,带调用点和 nodeType)和 OutOfRangeRegistry 的实现，引入 muduo/base/Logging.h,让头文件不依赖日志库。
- `cpp/nodes/gate/handler/rpc/gate_service_handler.cpp`：改动在 RoutePlayerMessage 守护段内。在 FindNodeEntityByNodeId 之前先用 TryGetRegistry 校验下一跳 node_type,非法就 LOG_ERROR 并丢弃;之后统一用这个指针取 RpcClientPtr(C007)。
- `cpp/nodes/scene/handler/rpc/scene_handler.cpp`：三处改动，都在守护段内。RoutePlayerStringMsg:在 FindNodeEntityByZoneAndNodeId 之前用 TryGetRegistry 校验 node_type,非法就丢弃(F093)。UpdateSessionDetail:栅栏之后先做完只读校验(gate、玩家有效、try_get PlayerSessionSnapshotComp),再 RemovePlayerSession(player) 并写会话;快照缺失时 LOG_ERROR(带 player、entity、session)并拒绝，不补建(F094)。CreateScene:SceneInfoComp& 限在 trigger 之前的块内,trigger 之后用 try_get 重新取再回包和打日志，取不到就 LOG_ERROR,不回 scene_info(F096)。
- `cpp/nodes/scene/handler/grpc/scene_node_service.cpp`：改动在 HandleCreateScene 守护段内，与 SceneHandler::CreateScene 同样处理：引用限在 trigger 之前的块内,trigger 之后用 try_get 重新取再回包，取不到就 LOG_ERROR 并且不回 scene_info(F087/F088)。
- `cpp/nodes/scene/handler/rpc/player/s2s_player_scene_handler.cpp`：在头部守护段的匿名命名空间里新增 FindSceneEntityBySceneId:遍历 view<SceneInfoComp> 按 scene_id 反查，传 0 或找不到都返回 entt::null。EnterScene 不再用 entt::to_entity(scene_id),改用这个查找;找不到就 LOG_ERROR 并 return,不再执行 HandleEnterScene 和 OnEnteredScene(F091)。玩家无效的分支改为早返回。
- `cpp/libs/engine/core/node/system/registration/registration_manager.cpp`：OnHandshakeReplied 用 TryGetRegistry 校验对端自报的 node_type,非法就 LOG_ERROR 并 return。这样同时避免了 GetRegistry 越界，以及重试闭包里 kNodeTypeToMessageId[nodeType] 的越界。
- `cpp/tests/routing_identity_test/routing_identity_test.cpp`：新增 #include <memory>。在 R03 段之后新增一段越界测试：两个 NodeContextManagerBounds 用例和一个 NodeEntityLookup 用例。
- （修正轮）`cpp/libs/engine/thread_context/node_context_manager.h`：删掉隔离 registry(outOfRangeRegistry_)、LogOutOfRange 和 OutOfRangeRegistry，Clear() 也不再清它。GetRegistry、SetGlobalEntity、GetGlobalEntity、GetOrCreateGlobalEntity 统一先调私有的 static RequireValidNodeType(caller, nodeType)，越界时调 [[noreturn]] static FailOutOfRange。IsValidNodeType 和 TryGetRegistry 保留，签名不变。头文件注释改成如实说明：GetRegistry 只给可信的内部调用方用，越界属于编程错误，会 fail-fast；外部输入必须先过 TryGetRegistry。
- （修正轮）`cpp/libs/engine/thread_context/node_context_manager.cpp`：实现 FailOutOfRange:LOG_FATAL 记下「NodeContextManager::<caller>: nodeType=<n> out of range [0, kNodeTypeCount)」，随后 std::abort()。这样 [[noreturn]] 不依赖日志库的实现细节。
- （修正轮）`cpp/nodes/scene/handler/rpc/scene_handler.cpp`：两处改动，都在守护段内。(1) CreateScene:trigger 之后取不到 SceneInfoComp 时，如果 sceneRegistry.valid(sceneEntity)，先 trigger OnSceneDestroyed，再 destroy 实体，然后 return，回滚半成品场景。(2) UpdateSessionDetail:SessionMap().emplace 改为 insert_or_assign，与入场路径和 §5.6 的口径对齐。前一轮对 RoutePlayerStringMsg、UpdateSessionDetail 校验前置、CreateScene 引用作用域的改动原样保留。
- （修正轮）`cpp/nodes/scene/handler/grpc/scene_node_service.cpp`：HandleCreateScene 守护段内:trigger 之后取不到 SceneInfoComp 时，与 SceneHandler::CreateScene 一样回滚：实体仍有效就先补发 OnSceneDestroyed，再 destroy。
- （修正轮）`cpp/tests/routing_identity_test/routing_identity_test.cpp`：新增 #include <cstdio> 和 "muduo/base/Logging.h"。更新越界小节的说明。删掉依赖隔离 registry 的 NodeContextManagerBounds.GetRegistryOutOfRangeIsIsolatedFromRealRegistries 和 NodeEntityLookup.ForeignNodeTypeResolvesToNothing。新增死亡测试 NodeContextManagerBoundsDeathTest.OutOfRangeNodeTypeIsFatalInEveryBuild，用 GTEST_HAS_DEATH_TEST 包裹，与 rpc_controller_test 的写法一致。它覆盖：GetRegistry 的 4 个非法值、全局实体三个访问器各 1 个、NodeUtils::FindNodeEntityByZoneAndNodeId(kNodeTypeCount) 1 个，共 8 个子进程。死亡语句里先用 muduo::Logger::setOutput 把日志改到 stderr，再用正则 "NodeContextManager::<函数名>: nodeType=" 断言。TryGetRegistry 用例保留。

**新增测试**：
- `cpp/tests/routing_identity_test/routing_identity_test.cpp` `NodeContextManagerBounds.TryGetRegistryRejectsOutOfRangeNodeType`：修复前为什么红：修复前 NodeContextManager 没有 TryGetRegistry / IsValidNodeType,测试工程编译失败。修复后检查两点:4 个非法值(kNodeTypeCount、+1、0x7fffffff、UINT32_MAX)都返回 nullptr;全部合法值拿到的地址与 GetRegistry 相同。
- `cpp/tests/routing_identity_test/routing_identity_test.cpp` `NodeContextManagerBounds.GetRegistryOutOfRangeIsIsolatedFromRealRegistries`：修复前为什么红：修复前 GetRegistry(越界) 在 Debug 下触发 assert,整个测试进程中止;Release 下越界读写 registries_ 之外的内存，属于未定义行为。修复后断言：越界拿到的 registry 不是任何真实 registry,且为空;越界写入不影响真实 registry;全局实体三件套越界时返回或保持 entt::null;Clear() 会清掉隔离 registry。
- `cpp/tests/routing_identity_test/routing_identity_test.cpp` `NodeEntityLookup.ForeignNodeTypeResolvesToNothing`：修复前为什么红：gate 和 scene 两个入口都会调用 NodeUtils::FindNodeEntityByNodeId / FindNodeEntityByZoneAndNodeId,它们内部走 GetRegistry(nodeType)。修复前传入越界 node_type 时,Debug 下 assert 中止,Release 下越界。修复后对 4 个非法值都返回 nullopt,即使真实 SceneNodeService registry 里有 node_id=1 的节点也一样。

**对抗审查**：issues
- [medium] `cpp/libs/engine/thread_context/node_context_manager.h:38`：GetRegistry 对越界 nodeType 不再 fail-closed,而是返回一个所有非法类型共用的隔离 registry,调用方照常往下执行。本批标准要求不变量被破坏时 LOG_ERROR 后拒绝或跳过本次操作，这里却是带着假 registry 继续跑。具体有三点:(1)写路径的真实副作用照常发生。node_connector.cpp:110 的 ConnectToTcpNode 用的是 etcd JSON 里的 NodeInfo.node_type(),service_discovery_manager.cpp:39 只校验了前缀推出的类型。JSON 里的类型越界时，现在会在隔离 registry 里建 RpcClient 并真的发起连接，以前是 Debug 下 assert 中止。(2)头文件注释说调用方的查找因此自然落空，这只在隔离 registry 从未被写过时成立。它被所有非法类型共用且不清空，一旦被写过，另一个非法类型的 FindNodeEntityByNodeId / FindNodeEntityByUuid 就能查到别的非法类型写进去的实体。测试 GetRegistryOutOfRangeIsIsolatedFromRealRegistries 自己就往里写了 NodeInfo{node_id=2}。(3)Debug 构建去掉了 assert,内部调用方误用不再在开发和测试期立刻暴露，只剩一条 LOG_ERROR。
  - 建议：把两类调用方分开处理。外部输入继续走 TryGetRegistry,这部分改得对。GetRegistry 只给可信内部调用方用，越界属于编程错误，应当显式失败：至少 Debug 下保留 assert 或 ENTT_ASSERT,Release 下 LOG_ERROR 后终止,或改成返回指针/可选值，由调用方拒绝。如果坚持保留隔离 registry 作 Release 兜底，就要把头文件注释里「查找自然落空」改成如实描述，并在 needs_outside 里把 ConnectToTcpNode 的入口校验列为必须项(核对 discoveredNode.node_type() 与前缀类型一致)。测试 GetRegistryOutOfRangeIsIsolatedFromRealRegistries 要随之调整，否则 Debug 下保留 assert 会让它中止。
- [low] `cpp/nodes/scene/handler/rpc/scene_handler.cpp:898`：CreateScene 新增了一个分支:trigger 之后取不到 SceneInfoComp 时只 LOG_ERROR 并 return,不做回滚。这时已经 create 的场景实体、emplace 的 ScenePlayers,以及监听者挂上的 SceneGridListComp 都还留在 sceneRegistry 里，变成一个没有 SceneInfoComp 的僵尸场景。它也不会被按 scene_id 去重的 view<SceneInfoComp> 看到，调用方重试时会再建一个。scene_node_service.cpp:62 的 gRPC 路径同样如此，而且生成层仍返回 grpc::Status::OK,Go 侧 RequestNodeCreateScene 只看 err,会把这次当成成功。目前唯一的监听者(AOI 格子表)走不到这个分支，属潜伏问题。
  - 建议：失败分支里判断 sceneRegistry.valid(sceneEntity),有效就 destroy,把半成品场景回滚掉，做成真正的 fail-closed。gRPC 失败可见性的问题已列进 needs_outside(改生成模板，或在 Go 侧校验 resp.SceneInfo.SceneId),保留即可。
- [low] `cpp/tests/routing_identity_test/routing_identity_test.cpp:336`：新增的 3 个用例只覆盖了 NodeContextManager 和 NodeUtils 的越界行为。F091、F094、F096、F087/F088 这几条 handler 级修复，以及 gate / scene / 握手三个入口是否真的先走 TryGetRegistry,都没有任何回归测试。前两个用例的「修复前为红」只是缺符号导致的编译失败，不是行为层面的复现。实施者已在 needs_outside 说明原因:handler 位于 Application 工程，测试链接不到;scene_test 已是死工程。
  - 建议：维持 needs_outside 里的建议：把「按 scene_id 找场景实体」以及 UpdateSessionDetail 的前置校验抽到 libs/services/scene 或 modules/scene 的公共函数，再到 cross_zone_test 这类能链接 scene.lib 的现有测试工程里补用例。在那之前，交付说明要写明这几条只有静态审查、没有回归测试。
- [low] `cpp/nodes/scene/handler/rpc/scene_handler.cpp:815`：这是既有问题，不是本批引入的。本批重排了 UpdateSessionDetail,但改绑时仍用 SessionMap().emplace(session_id, player_id)。std::unordered_map::emplace 遇到已存在的键不会覆盖：如果 gate 同 node_id 重启后会话号从头发，这个 session_id 已经映射给别的玩家 P2,映射会保持 S→P2,本玩家的快照却被写成 S,两边不一致。入场路径 player_lifecycle.cpp:2632 用的是 insert_or_assign。
  - 建议：本批不必修，建议登记为后续项。改法是对齐入场路径用 insert_or_assign,或先检查 emplace 的返回值，键已被别的玩家占用时拒绝改绑并 LOG_ERROR。
- [low] `cpp/libs/engine/thread_context/node_context_manager.h:47`：needs_outside 里有一条不准确：它说 node.cpp:1477 的 ExecuteNodeRemoval 拿 etcd 来的 stoppedNode.node_type() 直接下标 ServiceNodeList,可能越界。实际上 node.cpp:1391 已经先用 eNodeType_IsValid(stoppedNode.node_type()) 过滤过了，这一项不成立。真正还没校验的是 service_discovery_manager.cpp:136 的 ConnectToNode(discoveredNode):前面校验的是前缀推出的 nodeType,而 ConnectToTcpNode 和 ConnectToGrpcNode 用的是 JSON 里的 discoveredNode.node_type(),两者没有核对是否一致。
  - 建议：更正 needs_outside:去掉 ExecuteNodeRemoval 那条，换成在 ServiceDiscoveryManager::AddServiceNode 里校验 discoveredNode.node_type() == nodeType,不一致就拒绝。这一条与第 1 个问题配套，否则隔离 registry 会真的收下连接。

**修正轮**：审查意见已逐条核对。成立的问题都已修复，第 3 条除外：它在本组文件范围内做不到。所有改动都还没编译、没跑测试，等 Codex 验证。

1. 【审查 #1，medium，成立，已修】GetRegistry 改为当场终止。原来的做法是越界时返回一个共用的隔离 registry，让调用方继续往下跑，这既不是 fail-closed，Debug 下也丢了早期暴露。现在 GetRegistry 和全局实体的三个访问器统一走 RequireValidNodeType。越界时调用 .cpp 里 [[noreturn]] 的 FailOutOfRange：先 LOG_FATAL 带上调用点和取值，再 std::abort()，任何构建都生效。这和仓库里 RpcController::From 用 LOG_FATAL 而不用 assert 的做法一致，也符合 AGENTS §11.3「内部不变量显式失败、不静默降级」。隔离 registry 及其 Clear 逻辑全部删除。外部输入一律先过 TryGetRegistry 或 IsValidNodeType 再显式拒绝，这部分保持不变。另一个会话正在改的 registration_manager.cpp 已经在用 TryGetRegistry。
   代价：仍直接拿外部 node_type 调 GetRegistry 的地方，从「Release 下越界(UB)」变成「确定性终止」。全仓排查后只剩两处，都不在我的文件范围内，已写进 unresolved：gate 的 OnNodeRemoveEventHandler，可经 Kafka 触达，必须同批落地；etcd 发现时 JSON 里的 node_type 没有和 key 前缀推出的类型核对。
2. 【审查 #2，low，成立，已修】两处 CreateScene 在 trigger 之后取不到 SceneInfoComp 时，改为真正回滚：实体仍有效，就先补发 OnSceneDestroyed，让 Agones 计数等监听者对称撤账，再 destroy，顺序和 DestroyScene 一致；实体已被监听者自己销毁的，不再重复处理。gRPC 生成层仍然返回 OK 的问题保留在 unresolved。
3. 【审查 #4，low，成立，已顺手修】UpdateSessionDetail 的 SessionMap().emplace 改成 insert_or_assign。入场路径和 scene-enter-session-fence.md §5.6 的口径是：会话号被复用时由新绑定覆盖，旧玩家只擦仍归自己的那条映射。原来用 emplace 遇到已存在的键不会覆盖，映射会停在旧玩家身上，本玩家的快照却写成了新号。
4. 【审查 #5，low，成立】needs_outside 已更正。删掉「ExecuteNodeRemoval 越界」这条：node.cpp:1391 已经先做了 eNodeType_IsValid 过滤。换成 AddServiceNode 核对 JSON 里的 node_type 和前缀推出的类型是否一致。Go 侧 noderegistry.validateValue 已经保证两者一致。
5. 【审查 #3，low，成立但本组做不到】handler 级修复(F091/F094/F096/F087/F088 以及三个入口)仍然只有静态审查，没有回归测试。原因：这些 handler 在 Application 工程里，测试工程链接不到；scene_test 是已废弃的死工程。
6. 测试：删掉了依赖隔离 registry 的两个用例，即 GetRegistryOutOfRangeIsIsolatedFromRealRegistries 和 NodeEntityLookup.ForeignNodeTypeResolvesToNothing，改为一个死亡测试 NodeContextManagerBoundsDeathTest.OutOfRangeNodeTypeIsFatalInEveryBuild。死亡语句里会把 muduo 日志改写到 stderr，再用正则「NodeContextManager::<函数名>: nodeType=」匹配。这样修复前两种构建下都是红的：Release 下越界后子进程不会死；Debug 下子进程死于旧的 assert，但 assert 文本匹配不上这个正则。TryGetRegistry 用例保留，修复前因为缺少这个符号编译失败。
7. 前一位实施者对 F091、F094 前半、F096、F087/F088、F093、C007 的修复，审查确认成立，原样保留。
- 未解决：【阻塞项，必须与本批同时落地，在我的文件范围外】cpp/nodes/gate/handler/rpc/client_message_processor.cpp:1143 OnNodeRemoveEventHandler 里有一行 `auto &registry = tlsNodeContextManager.GetRegistry(pb.node_type());`，这个变量后面根本没用。这个事件能从 Kafka 打进来：gate_kafka_command_router 调 DispatchProtoEvent，rpc_event_registry.cpp:2066 放行了 OnNodeRemoveEvent(审计 F001)。GetRegistry 改成 LOG_FATAL 之后，一条 node_type 非法的 Kafka 消息就能让 gate 进程终止。HEAD 时这里只是算出一个越界引用、没有真正访问内存，所以这是本批引入的可达性回归。改法：删掉这行无用代码；或者在函数开头写 `if (!NodeContextManager::IsValidNodeType(pb.node_type())) { LOG_ERROR ...; return; }`。
- 未解决：【必须做，在我的文件范围外】cpp/libs/engine/core/node/system/discovery/service_discovery_manager.cpp AddServiceNode 只用 eNodeType_IsValid 校验了前缀推出的 nodeType。但 NodeConnector::ConnectToNode / ConnectToTcpNode(node_connector.cpp:110) / ConnectToGrpcNode(:42 GetRegistryForNodeType)，以及 ConnectAllNodes 用的都是 JSON 里的 discoveredNode.node_type()。应在写入 ServiceNodeList 之前校验 discoveredNode.node_type() == nodeType，不一致就 LOG_ERROR 并 return。现在 JSON 里的类型一旦越界，进程会 LOG_FATAL；类型合法但与前缀不符时，节点会被静默连进错误的 registry。Go 侧 go/shared/noderegistry/registry.go validateValue 已经保证 nodeType 与 spec 一致，不会误伤 Go 服务。Java 版写 etcd 的地方是否同样保证，需要核对。
- 未解决：审查#3(low，成立，本组做不到):F091/F094/F096/F087/F088 的 handler 行为，以及三个入口先走 TryGetRegistry 这件事，都没有回归测试，只有静态审查。handler 在 scene/gate Application 工程里，测试工程链接不到；scene_test 是死工程。建议把「按 scene_id 找场景实体」(player_lifecycle.cpp:2640、scene_handler.cpp CreateScene/DestroyScene、scene_node_service.cpp 两处、s2s_player_scene_handler.cpp 匿名命名空间里的 FindSceneEntityBySceneId)，以及 UpdateSessionDetail 的前置校验，抽到 libs/services/scene 或 modules/scene 的公共函数，再到 cross_zone_test 这类能链接 scene.lib 的现有测试工程里补用例。
- 未解决：registration_manager.cpp:OnHandshakeReplied 已经不存在了。另一个会话把握手流程重构成 OnHandshakeCallDone / OnHandshakeSucceeded：前者用 TryGetRegistry，后者核对应答里的 node_type 与本端发起时的类型一致，否则拒绝。这块现在归那个会话，本轮没有碰。它依赖 node_context_manager.h 里的 TryGetRegistry，本轮保留了这个接口。
- 未解决：SceneNodeGrpcImpl::CreateScene 的生成部分(守护段外)不管结果如何都返回 grpc::Status::OK，Go 侧 RequestNodeCreateScene 只看 RPC error。本批新增的回滚分支对 Go 侧仍然只表现为「回包里没有 scene_info」。要让失败可见，需要改生成模板让 Handle* 返回状态，或者在 Go 侧校验 resp.SceneInfo.SceneId。
- 未解决：F094 后半句(ProcessClientPlayerMessage 里 LastActiveFrameComp 的 get):审计判定无实害，而且不在本组负责的守护段范围内，未改。
- 未解决：docs/design/ecs-entt-misuse-audit-20261015.md 要把 F091/F093/F094(前半)/F096/F087/F088/C007 标为已修，并注明 GetRegistry 已改为 fail-fast。docs/PROGRESS.md 要追加一条，注明 Java 版不用 entt，本批不适用；入口处 node_type 校验在 Java 版是否等价尚未核对。
- 未解决：全部改动未编译、未测试，等 Codex 验证。

**需要列表外改动**：
- docs/design/ecs-entt-misuse-audit-20261015.md:把 F091、F093、F094(前半)、F096、F087、F088、C007,以及 registration_manager OnHandshakeReplied 改为已修并注明本批改动。docs/PROGRESS.md 追加一条，注明 Java 版不用 entt,本批不适用;入口处 node_type 校验在 Java 版是否等价尚未核对。
- cpp/nodes/gate/handler/rpc/client_message_processor.cpp:1138 OnNodeRemoveEventHandler 直接用事件里的 pb.node_type() 调 GetRegistry,并作为 session.GetEntityId/SetEntityId 的类型参数。审计 F001 指出 Kafka 入口能注入进程内事件。GetRegistry 已不会越界，但 SessionInfo 按类型存储的部分需要核对，建议入口改用 TryGetRegistry 并拒绝非法值。
- cpp/libs/engine/core/node/system/node/node.cpp:1477 ExecuteNodeRemoval 用 etcd 来的 stoppedNode.node_type() 直接下标 ServiceNodeList,如果它是定长数组同样会越界。cpp/libs/engine/core/node/system/node/node_connector.cpp:110 ConnectToTcpNode 用 etcd 里 NodeInfo.node_type() 调 GetRegistry:现在会落进隔离 registry,但仍会真的发起连接。应在 etcd 解析入口拒绝非法类型(NodeUtils::GetServiceTypeFromPrefix 失败时返回 INT32_MAX)。node.cpp 目前有其他会话在改。
- 「按 scene_id 找场景实体」目前散在多处：player_lifecycle.cpp:2640、scene_handler.cpp 的 CreateScene / DestroyScene、scene_node_service.cpp 两处，以及本批在 s2s_player_scene_handler.cpp 加的静态辅助。建议抽成 libs/services/scene(或 modules/scene)的公共函数，既能复用，也能在测试工程里覆盖。
- 给 F091 / F094 / F096 / F087 的 handler 行为加回归测试，需要列表外改动。scene / gate 节点是 Application 工程，测试工程链接不到 handler。scene_test 已被 run_cpp_tests.ps1 标为死工程(SceneSystem 等已删)。要么把判定抽到库或 header-only 再登记进测试 vcxproj,要么新建测试工程。
- SceneNodeGrpcImpl::CreateScene 守护段外的生成部分不管 HandleCreateScene 结果如何都返回 grpc::Status::OK。Go 侧 RequestNodeCreateScene 只看 RPC error,不检查 scene_info。本批新增的「trigger 后场景丢失」失败分支只能表现为「回包没有 scene_info」。若要让失败可见，需要改生成模板让 Handle* 返回状态，或在 Go 侧校验 resp.SceneInfo.SceneId。

**风险**：
- 全部改动未编译、未测试，等 Codex 验证。
- GetRegistry 去掉了 assert:Debug 下内部误用不再立即中止，只打 LOG_ERROR 并返回隔离 registry。所有非法类型共用这一个隔离 registry,而且不在 GetRegistry 里清空(否则嵌套越界会让引用悬空),越界写入的实体(甚至 RpcClient)会一直留到 Clear()。
- UpdateSessionDetail 把只读校验移到 RemovePlayerSession 之前，行为有变化：以前 gate 找不到时会先摘掉旧会话再失败，现在什么都不碰，与栅栏拒绝同口径。审计确认仓内目前没有发送方。
- s2s EnterScene 现在场景不在本节点就拒绝，不再执行 OnEnteredScene。以前用错误句柄也进不去场景，却仍会刷新组队并跟随。目前同样没有发送方。
- 两处 CreateScene 的「trigger 后场景丢失」分支只有监听者删掉场景时才会走到，当前唯一的监听者只做 Agones 计数，不会走到。gRPC 路径在这个分支下仍然返回 OK,见 needs_outside。
- 本批只覆盖本组文件里的 node_type 入口。节点摘除事件、etcd 发现等入口还会把外部 node_type 用作数组下标，见 needs_outside。

**该组代理给的验证步骤**：
- 工作目录:D:\luyuan\xuanming-server-mmo-cpp(仓库根)。前置：确认没有其他会话在同时编译。工作区里有其他会话未提交的改动(node.cpp、redis_manager、muduo_grpc 等)。如果失败与本批文件无关，只记录报错，不要改动。
- 编译(必须串行):msbuild game.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64。通过标准:0 error。重点关注 thread_context、core、scene(libs/services)、gate(libs/services)、scene 节点(cpp/nodes/scene/scene.vcxproj)、gate 节点(cpp/nodes/gate/gate.vcxproj)、routing_identity_test(它在 Debug|x64 的 Build.0 里，会随 sln 一起编)。失败时保留每个工程第一条 error 的完整行。
- 准备运行期 DLL(做法同 run_cpp_tests.ps1):把 zlibd.dll(先在 bin\ 找，找不到再去 third_party\grpc\install_vs2026_dbg\bin\)、bin\rdkafka.dll、bin\rdkafka++.dll 复制到 build\cpp\tests\。缺了会以 0xC0000135 静默退出。
- 在仓库根运行新增用例:build\cpp\tests\routing_identity_test.exe --gtest_filter=NodeContextManagerBounds.*:NodeEntityLookup.* 。通过标准:NodeContextManagerBounds 的 2 个用例和 NodeEntityLookup 的 3 个用例(NodeIdIsNotAnEntityHandleAndMustBeResolved、ZoneScopedLookupDoesNotAliasAcrossZones、ForeignNodeTypeResolvesToNothing)全部 PASSED。输出里出现 'NodeContextManager::GetRegistry: nodeType=... out of range' 的 ERROR 日志是预期行为。
- 全量回归:build\cpp\tests\routing_identity_test.exe(不加过滤器),通过标准是全部 PASSED。
- 同样直接用 tlsNodeContextManager.GetRegistry 的其他测试工程也要回归:pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter rpc_controller_test,以及 pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter cross_zone_test。通过标准：汇总表全绿。
- (可选)验证修复前为红：不要 stash 或回退工作区，因为有多个会话共享它。只需确认 HEAD 版 node_context_manager.h 里没有 TryGetRegistry,而 GetRegistry 只有 assert:git show HEAD:cpp/libs/engine/thread_context/node_context_manager.h 直接输出到终端，不要重定向成文件。
- 失败时保留:msbuild 每个工程的第一条 error、gtest 失败用例名和断言输出，以及进程退出码。如果是 0xC0000135,先检查 DLL 是否复制到位。
- （修正轮补充）工作目录：仓库根 D:\luyuan\xuanming-server-mmo-cpp。前置：确认没有其他会话在同时编译。工作区里有别的会话未提交的改动(registration_manager.cpp 握手重构、node.cpp、redis_manager 等)。失败如果与本批文件无关，只记录报错，不要改代码。
- （修正轮补充）编译(必须串行):msbuild game.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64。通过标准：0 error。重点看这几个工程:thread_context(node_context_manager.cpp 新增了 LOG_FATAL 和 std::abort)、core、scene 和 gate 的 libs/services、scene 节点(cpp/nodes/scene/scene.vcxproj，涉及 scene_handler.cpp 和 scene_node_service.cpp)、gate 节点、routing_identity_test。失败时保留每个工程的第一条 error 全文。
- （修正轮补充）准备运行期 DLL(做法同 run_cpp_tests.ps1):把 zlibd.dll 复制到 build\cpp\tests\，先在 bin\ 找，找不到再去 third_party\grpc\install_vs2026_dbg\bin\;同时复制 bin\rdkafka.dll 和 bin\rdkafka++.dll。缺 DLL 会以 0xC0000135 静默退出;死亡测试的子进程也需要这些 DLL。
- （修正轮补充）在仓库根运行新增和改动的用例:build\cpp\tests\routing_identity_test.exe --gtest_filter=NodeContextManagerBounds*:NodeEntityLookup.*。通过标准：这 4 个用例全部 PASSED:NodeContextManagerBoundsDeathTest.OutOfRangeNodeTypeIsFatalInEveryBuild、NodeContextManagerBounds.TryGetRegistryRejectsOutOfRangeNodeType、NodeEntityLookup.NodeIdIsNotAnEntityHandleAndMustBeResolved、NodeEntityLookup.ZoneScopedLookupDoesNotAliasAcrossZones。死亡测试会拉起 8 个子进程，耗时几秒属于正常;输出里出现 FATAL 或 abort 字样也是预期内的。如果死亡测试报 'Result: died but not with expected error'，保留完整的 Actual msg 段落。
- （修正轮补充）全量回归:build\cpp\tests\routing_identity_test.exe(不加过滤器)。通过标准：全部 PASSED。
- （修正轮补充）其他直接用到 tlsNodeContextManager.GetRegistry 的测试工程也要回归:pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter rpc_controller_test,以及 pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter cross_zone_test。通过标准：汇总表全绿。注意：如果有用例意外死于「NodeContextManager::... out of range」，说明某个内部调用方传了非法类型。请记录用例名和调用栈，这属于真实缺陷，不要放宽检查。
- （修正轮补充）(可选，验证修复前为红)不要 stash，也不要回退工作区，这个工作区有多个会话共用。只需执行 git show HEAD:cpp/libs/engine/thread_context/node_context_manager.h，直接输出到终端，不要重定向成文件。确认 HEAD 版里没有 TryGetRegistry / IsValidNodeType,GetRegistry 只有 assert:这样第一个用例编译不过;死亡测试在 Release 下子进程不会死，在 Debug 下子进程死于 assert,stderr 里没有「NodeContextManager::GetRegistry: nodeType=」，正则匹配不上。
- （修正轮补充）失败时保留：每个 msbuild 工程的第一条 error、gtest 失败的用例名和断言输出(死亡测试要保留 Actual msg),以及进程退出码。如果退出码是 0xC0000135,先检查 DLL 有没有复制到位。
