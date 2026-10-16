# Codex 任务单：第 3 批：背包内部不再用 entt（2026-10-15，第 1 版）

> **谁执行**：Codex。总规则、批次顺序、组件三类初始化口径、协调与回执格式见 [README.md](README.md)，本单不重复。
> **本版性质**：设计稿加挑战者意见的原样整理，还没有细化到「改前/改后代码片段」。Claude 的额度恢复后会出第 2 版（补丁级）。**先读 §8 挑战意见**：标为 high/medium 的问题，实施时必须按其中的修法修正设计。凡与 README §2 三类初始化规则冲突的地方，一律以 README 为准。
> **行号**：设计时取自当时的 HEAD，此后第 1 批和其他会话都改过代码，实施前请按函数名重新定位。

> **后续完成口径**：源码及额外原子性修正见文末 §10；早期设计列出的 C004 业务决策和 Java 历史差异已继续处理。统一生成、构建和运行测试仍按负责人指令暂缓。

## 1. 目标

第 3 批：背包实例层（cpp/libs/modules/bag）彻底去掉 entt。ItemStore 不再为每个 Bag 私有一个 entt::registry，改成以 guid 为键的单一确定性容器。对外 API 不再暴露 entt::entity。「并堆填哪一堆 / 扣哪一堆 / 合并留哪一堆 / 回收顺序 / 整理平局 / 批量按 config 的写入次序」全部改成写死、可测的规则，并与 Java 版 Bag.java 同一操作的客户端可见结果对齐。覆盖 F024（私有 registry 嵌在组件里、句柄外泄）、F023（选堆取决于 entt pool 的逆序与 swap-and-pop 重排）、F021（itemRegistry 是死的、registry 划分与用法不符）。不改 proto、不改生成产物、不动布局层（container_layout.*），遵守 AGENTS §11.6 与 docs/design/bag-instance-layout-split.md 的接缝。

## 2. 采用的标准做法

【为什么不用 ECS】EnTT 惯用法里，registry 只装「大量同构实体，按组件组合批量处理」的数据。一个背包里的物品是某个玩家私有的小集合（每包 ≤200 件）。它需要的是按 guid O(1) 查找，加上显式、确定的选择顺序；从来不按组件组合做 view。所以它属于普通容器，不该是一个 registry。现状的问题：
- 每个 Bag 带一个完整 registry（PlayerBagsComp 是 actorRegistry 的组件，等于 registry 套在组件里）；
- guid→entity 和 entity→ItemComp 两张表要手工保持同步；
- Bag::GetItemByGuid / GetItemByPos 把私有 registry 的句柄暴露出去（这两个函数全仓 0 个调用方）；
- 所有「选哪一堆」都跟着 view<ItemComp>().each() 走。entt 3.13.2 的 sparse_set::begin() 从 packed 末尾倒序遍历（已核对 third_party/entt/.../sparse_set.hpp:606），删除时 swap-and-pop 还会重排。结果是：新入包的堆先被扣、先被灌，合并时留下哪个 guid 取决于历史删除序列。

【选用的方案】
1. 单一权威容器：`std::unordered_map<Guid, ItemComp> items_`，键等于 item_id。节点容器在 rehash 后元素地址仍然有效，比 entt 的 swap-and-pop 更稳。没有第二张表要同步。
2. 只定义一个顺序，叫「实例序」：`(acquire_seq 升序, item_id 升序)`，由 `ItemStore::AcquiredBefore` 唯一定义。acquire_seq 本来就是淘汰排序的依据：由 ItemStore::Insert 唯一盖章，随快照原样带走，还原时恢复水位。把它扩展到并堆、合并、回收、ForEach、整理平局，就只有一个真相来源（DRY）。Java 的 `BagItem.ACQUIRE_ORDER` 正是这个定义。
3. 「扣哪一堆」按槽位号升序，同槽位再按实例序。这是 Java 的 D22（scene-battle-spec：消耗按格子号升序，客户端可见的是 191 里哪一堆变少）。实例层不认识槽位，所以由桥层 Bag 传一个中性的排序键回调（DrainRank）给 ItemStore::DrainStacks。实例层只做数量算术，布局层零改动，接缝不破。
4. ItemCountMap（unordered_map）在 bag 模块里所有「按 config 逐个写、逐个报错」的循环，都改成 config_id 升序。批量入包时先处理的 config 先占 first-fit 的低号格，客户端可见。这与 Java 的 Bag.add 和 BagService.removeClamped（TreeMap、无符号升序）一致。

【没选的备选】
- std::map<Guid,ItemComp>：遍历确定，但 guid 序没有业务含义（号段 guid 不按时间排，邮件附件可以预设 guid），各处仍要再排一次。
- 按插入序的 vector 加 guid 索引：旧存档还原后插入序不等于 acquire 序；要同步两份结构；插入会让指针失效。
- std::map<(seq,guid),ItemComp> 加 guid→key 索引：两份结构要同步。key 还会和 Find 返回的可写 ItemComp 里的 acquire_seq 分叉。
单容器加按需排序（n ≤ 200，调用点都不在每帧路径上）最简单，一致性也最好。

## 3. 覆盖的审计编号

F024、F023、F021、F020（其中 itemRegistry 闲置的部分）

## 4. 实施步骤（预计改动约 17 个文件）

### 4.1 1. 实例层改为确定性容器，并写死实例序（ItemStore）

**文件**：
- `cpp/libs/modules/bag/item_store.h`
- `cpp/libs/modules/bag/item_store.cpp`

【头文件】
- 删除 `#include "entt/src/entt/entity/registry.hpp"`、`using ItemsMap`、`EntityOf()`，以及成员 `guidToEntity_` 和 `registry_`。新增 `#include <functional>`；<unordered_map>、<unordered_set>、<vector> 保留。
- 新成员 `std::unordered_map<Guid, ItemComp> items_{};`，注释写明：唯一权威，键 == item.item_id()；所有创建走 Insert，所有销毁走 Erase / Clear。删掉原来「guidToEntity_ 与 registry_ 必须同步」那段。指针契约照旧保守地写「下一次 Insert/Erase/整理之前有效」，不放宽，避免调用方依赖实现细节。

【实例序】
- 新增 public `[[nodiscard]] static bool AcquiredBefore(const ItemComp &lhs, const ItemComp &rhs);`：`lhs.acquire_seq() != rhs.acquire_seq()` 时比 acquire_seq，否则比 `item_id()`，都是 uint64 无符号比较。
- 注释写清楚：这是包内的全序（guid 在包内唯一）；全仓只在这里定义一次；与 Java BagItem.ACQUIRE_ORDER 同口径；acquire_seq==0（理论不可达）排在最前。
- 新增 private `std::vector<const ItemComp *> InAcquireOrder() const;`，以及非 const 版 `std::vector<ItemComp *> InAcquireOrder();`：收集 items_ 的元素地址后用 AcquiredBefore 排序，结果只在本次调用内有效。
- 新增 private static `bool IsMergeCandidate(const ItemComp &item, uint32_t maxStack)`：返回 `!item.has_equip() && maxStack > 1 && item.size() > 0 && item.size() < maxStack`。

【身份与生命期】
- Size / Contains / Find(const 与非 const) / Erase / Clear 直接操作 items_。
- Insert：先查 `items_.contains(proto.item_id())`。撞车就返回 nullptr，不盖章、不抬水位。原实现是先盖章再撞车，水位会白涨一格，顺带修掉。然后照原规则盖章 / 抬水位，再 `items_.emplace(guid, std::move(proto))`，返回 `&it->second`。
- ForEach（模板）改为 `for (const ItemComp *item : InAcquireOrder()) fn(static_cast<Guid>(item->item_id()), *item);`。注释：按实例序遍历，是对外契约（bag_marshal 写盘顺序、淘汰都依赖它），回调里不得改本仓库。

【结果与顺序无关的查询】
TotalCountOf / HasAll / MeasureFreeRoomPerConfig / HasEmptyInstances 直接遍历 items_，各加一句注释「结果与遍历顺序无关」。逻辑不变（MeasureFreeRoomPerConfig 仍把 size==0 的空堆算作空余，并堆侧本来就允许往空堆回填）。

【并堆】
- `struct StackFill { Guid guid{kInvalidGuid}; uint32_t amount{0}; };`，原来存的是 entity。
- PlanStackIntoExistingStacks：签名不变。先筛出 `CanStack(item, proto) && !excluded` 的候选，按 AcquiredBefore 排序，再依次填。size > maxStackSize 的脏数据照旧 LOG_ERROR 后跳过，room==0 跳过。
- ApplyStackFill：`ItemComp *item = Find(plan.guid)`；为空时照原文案 LOG_ERROR（计划引用的实例已被销毁，是桥层顺序 bug）并 continue。原来的 `registry_.valid/get` 删除。

【扣除】
- 新类型 `using DrainRank = std::function<uint64_t(Guid)>;`
- 新签名 `uint32_t DrainStacks(uint32_t configId, uint32_t count, const DrainRank &drainRank, std::vector<StackDrain> *drainedOut = nullptr);`
- 候选 = 该 configId 的全部实例。按 `(drainRank ? drainRank(guid) : 0, 实例序)` 升序排序后依次抽。size==0 的跳过、不进回执；返回实扣量。夹紧语义与原来一致。
- 注释：rank 由调用方给，实例层不解释它；今天桥层传的是槽位号。

【合并与回收】
- HasMergeablePartials 和 MergePartialStacks 都改用 IsMergeCandidate，新增 size>0 条件：空堆不参与合并，只由 CollectEmptyInstances 回收。这样「留下来的那一堆」永远不会是一个空堆。
- MergePartialStacks：候选按实例序排好，按 config 分组到 `std::map<uint32_t, std::vector<ItemComp *>>`；组内前面的填满，最后一个放余数，其余清 0。算法和原来一样，只是组内顺序变成实例序，所以最早入包的堆被填满并留下，后面的被回收。
- CollectEmptyInstances 按实例序返回 guid。

【CanStack 注释】
更新「四处同口径」那段：has_equip 的排除仍在 CanStack / MeasureFreeRoomPerConfig / HasMergeablePartials / MergePartialStacks 四处一致；size>0 只属于合并两处（IsMergeCandidate），并堆侧刻意允许回填空堆。

【保持不变】
CanStack、StacksNeededFor、MintGuid、CanMintGuid(s)、IsInvalidGuid、NextAcquireSeq。

### 4.2 2. 淘汰策略复用实例序（不再自己定义一份排序）

**文件**：
- `cpp/libs/modules/bag/eviction_policy.h`
- `cpp/libs/modules/bag/eviction_policy.cpp`

- 删除 `EvictOldestFirst::AcquisitionOrderOf` 的声明和实现。已 grep：全仓除 eviction_policy.cpp 自己外 0 个调用方。
- SelectVictims：保留 `needed==0` 和 `store.Size() < needed` 返回空的两道早退。然后改为 `GuidVector victims; victims.reserve(needed); store.ForEach([&](Guid guid, const ItemComp &) { if (victims.size() < needed) victims.push_back(guid); });`，也就是直接取实例序（ForEach 的契约）的前 needed 个。
- 头文件注释改为：排序依据 = ItemStore::AcquiredBefore（入包序号，同序号再比 guid），与并堆、合并同一个定义，与 Java 临时格淘汰同口径。原先写「刻意不用 guid」的那段理由保留（guid 只做平局次键）。
- RejectWhenFull 不动。行为与原来一致：原来就按 (seq, guid) partial_sort，只是删掉了重复定义。

### 4.3 3. 桥层 Bag：删除外泄的实体访问器，扣除改按槽位号升序，整理平局与按 config 的批量顺序写死

**文件**：
- `cpp/libs/modules/bag/bag_system.h`
- `cpp/libs/modules/bag/bag_system.cpp`

(a) 删除 `entt::entity GetItemByGuid(Guid)` 和 `entt::entity GetItemByPos(uint32_t)` 的声明与实现（bag_system.cpp 114-117、127-131）。已 grep 全仓 cpp/ 与 tools/：0 个调用方。GetItemCompByGuid / GetItemCompByPos / GetItemPosByGuid / ForEachItem 等基于 guid 的 API 原样保留。

(b) 在 bag_system.h 新增自由函数 `[[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>> SortedByConfigId(const ItemCountMap &counts);`，实现放在 bag_system.cpp：拷成 vector 后按 first 升序排序。注释：ItemCountMap 是 unordered_map，遍历序未定义；凡是按 config 逐个写或逐个报错的循环都走它；先处理的 config 先占 first-fit 的低号格，客户端可见；与 Java Bag.add / BagService.removeClamped 的「配置号升序」同口径。

(c) 以下循环改为 `for (const auto &[configId, count] : SortedByConfigId(...))`：
- `Bag::PlanInstances`（第 153 行，多个 config 都非法时返回的错误码因此确定）；
- `Bag::AddItems(const ItemCountMap&)`（第 1034 行）；
- `Bag::RemoveItems`（第 389 行）；
- `Bag::RemoveItemsClamped`（第 481 行）。
ReserveForBatchAdd 里的零数量预检、CanReserve 里的部位汇总只影响日志、不影响结果，不改。

(d) `Bag::DrainOneConfig`：构造 `const ItemStore::DrainRank bySlot = [this](Guid guid) { return static_cast<uint64_t>(layout_->SlotOf(guid)); };`，两条分支都调 `store_.DrainStacks(configId, count, bySlot, ...)`。
- SlotOf 查不到返回 kInvalidSlot（UINT32_MAX），两层不一致时这样的实例排在最后，HasAll 算过的数量仍然一定扣得到。
- 注释写明扣除规则：槽位号升序，同槽位按实例序；与 Java drainClamped 的 D22 同口径；RemoveItems（全或无，强化 / 镶嵌扣材料）和 RemoveItemsClamped（战斗结算消耗）共用这一条。

(e) `Bag::DesiredOrder`：OrderKey 增加 `uint64_t acquireSeq`。比较器改为 configId 升序 → size 降序 → acquireSeq 升序 → guid 升序，即完整平局键。注释：平局按实例序，与 Java 整理的（配置升序、数量降序、入包先后）同口径；原先 std::sort 不稳定，平局位置未定义。
- IsOrderedByConfigThenSize 不改：相邻比较不看平局，与 Java isCompactAndOrdered 一致，已最优时早退。

(f) 注释修正：
- 类头第 44-45 行「公开 API 与拆分前逐字一致」补一句：2026-10-15 删除两个返回 entt::entity 的访问器（0 个调用方）。
- 第 216-218 行改为：Bag 不可复制，原因是 unique_ptr 成员（不再是 entt::registry）。
- AddStackableItem 第 945-948 行的「entity 已经失效 / 悬空的 entity」改为「计划里的 guid 可能已被淘汰销毁」。
- DestroyItem 第 1421 行「仍在 view<ItemComp> 里」改为「仍在实例表里」。
- MergeAndCompact ② 的注释补一句：空堆只回收、不参与合并。

(g) 布局层（container_layout.*）、准入、PlaceInstance、具名槽相关逻辑一行不动。

### 4.4 4. BagService：批量入包按 config 升序写入与记流水

**文件**：
- `cpp/libs/modules/bag/bag_service.cpp`
- `cpp/libs/modules/bag/bag_service.h`

- `BagService::AddItems(..., const ItemCountMap &itemsToAdd, ...)`：开头 `const auto ordered = SortedByConfigId(itemsToAdd);`，封禁检查循环（第 196 行）和逐 config 写入、记流水的循环（第 228 行）都改为遍历 ordered。ReserveForBatchAdd 仍传原 map，它内部会排序。
- RemoveItemsClamped / RemoveItemsExact 不改代码，Bag 内部已经排序。只在 bag_service.h 这两个函数 drainedOut 的注释里写明：抽取顺序 = config_id 升序，同 config 内槽位号升序（D22）。
- 参数 `entt::entity playerEntity` 是 actorRegistry 里的玩家句柄，属于进程内的正确用法，不在本批范围内，不改。
- RemoveItemsExact 里零数量预检的循环只影响日志，不改。

### 4.5 5. 回归测试：新增选择顺序用例文件；其余 12 个 bag_test 文件逐一核对，无需改动

**文件**：
- `cpp/tests/bag_test/bag_selection_order_test.cpp（新建）`
- `cpp/tests/bag_test/bag_test.vcxproj`
- `cpp/tests/bag_test/bag_test.vcxproj.filters`

【新文件与夹具】
新建 bag_selection_order_test.cpp，在 bag_test.vcxproj 的 ClCompile 段和 .vcxproj.filters 各加一行。如果 vcxproj 正被别的工作流改，先重读再插入；实在冲突就把用例追加到 bag_instance_data_test.cpp 末尾。
夹具 `BagSelectionOrderTest` 照抄 bag_instance_data_test.cpp 的 ArmItemSegment 写法：
- SetUp：Load ItemTableManager 和 EquipSlotTableManager；`ArmItemSegment(95000000, 96000000)`；断言 MaxStack(10)、MaxStack(11)、MaxStack(15) 都 ≥10。
- TearDown：`ArmItemSegment(1, uint64_t{1} << 54)` 恢复基线。
- 这样 gtest 跨文件的执行先后不会让 bag_test.cpp 末尾 BagSegmentNotReadyTest 的「号段永久不可用」波及本文件。
- 铺底一律用 `InsertItemForRestore(guid, config, size, pos, acquireSeq)`。预设 guid 取 `(1<<60)+95xx`。故意让 guid 序、槽位序、seq 序三者互不相同，每条规则才能区分出来。

【用例】
1. ForEachVisitsInAcquireOrderThenGuid：seq 30/10/20/20（两个 seq 20 的 guid 一大一小），ForEachItem 依次是 seq10、seq20 小 guid、seq20 大 guid、seq30。
2. StackFillGoesToEarliestAcquiredPartialFirst：config 10 有三堆，size 都是 m-2。A 在 pos0、seq30、guid 最小；B 在 pos2、seq10、guid 最大；C 在 pos1、seq20。先 AddItem(10, 2)，写入回执 == [B]，B==m；再 AddItem(10, 3)，回执 == [C, A]，C==m，A==m-1。全程不铸号。
3. ClampedDrainTakesLowestSlotFirst：X 在 pos2、seq1、size 5；Y 在 pos0、seq3、size 5；Z 在 pos1、seq2、size 5。`RemoveItemsClamped({10:7}, &drained)`，回执依次是 (Y,5)、(Z,2)，X 不动。
4. ExactDrainTakesLowestSlotFirst：同样的布局走 RemoveItems，结果相同。
5. DrainSkipsEmptyStackWithoutReceipt：pos0 是 size 0 的空堆，它不进回执，从 pos1 开始扣。
6. MergeKeepsEarliestAcquiredStackAndRetiresLaterOnes：P 在 pos0、seq5、size3；Q 在 pos1、seq1、size4。MergeAndCompact(&destroyed, kMergeOnly) 之后 Q==7 仍在 pos1，P 被回收，destroyed == [{P,10,0}]。
7. MergeNeverPicksAnEmptyStackAsSurvivor：Z 在 pos0、seq1、size0；P 在 pos1、seq2、size3；Q 在 pos2、seq3、size4。kMergeOnly 之后 P==7 在 pos1，destroyed 的 guid 依次 == [Z, Q]（实例序）。这条钉住新规则，旧实现会让 Z 留下来。
8. ReorderBreaksTiesByAcquireOrder：pos0 放一个 config 11 满堆（制造乱序以越过早退）。pos1/2/3 是三个 config 10 满堆，seq 分别为 3、1、2，guid 序与 seq 序不同。kMergeAndReorder 之后，pos0/1/2 依次是 seq1/2/3 的三个 config 10 满堆，pos3 是 config 11。
9. CountMapBatchPlacesConfigsInAscendingConfigOrder：空 Flat(10) 包，ItemCountMap 按 15、11、10 的顺序插入，各给一个满堆的量。分别验证 Bag::AddItems 与 `BagService::AddItems(entt::null, bag, blockList, map)`，结果都是 pos0==10、pos1==11、pos2==15。需要铸号，夹具已经装好号段。
10. EvictionTieOnEqualSeqEvictsSmallerGuidFirst：Temporary(2) 包，两件不可叠加物品 seq 同为 5，guid 一大一小。AddItem 一件新的，evicted == [小 guid]。
11. SnapshotReplayKeepsIterationOrder：把 (guid, config, size, pos, seq) 按 ForEachItem 顺序抓出来，在新 Bag 上按这个顺序 InsertItemForRestore 重放，ForEachItem 序列完全相同。这个用例保证存档往返后写盘顺序稳定；旧实现每往返一次顺序就反一次。
12. ItemStoreTest.DuplicateInsertDoesNotAdvanceWatermark：直接构造 ItemStore，同一个 guid Insert 两次，第二次返回 nullptr，NextAcquireSeq 不变。
13. ItemStoreTest.ApplyStackFillSkipsDestroyedGuid：`ApplyStackFill({{未知guid,3}}, &written)` 不崩溃，written 为空，其他实例不变。

【既有 13 个 cpp 逐个核对】（task 说 12 个，实际编进 bag_test 的是 13 个；已 grep 过 GetItemByGuid / GetItemByPos / EntityOf / ItemStore 内部 API）
- bag_test.cpp：只用到 GetItemCompBy* / GetItemPosByGuid / ItemStore 的静态函数 / `ItemStore store;` 加 RejectWhenFull，与顺序无关。
- bag_instance_data_test.cpp：ItemStore::CanStack、IsInvalidGuid、ForEachItem 都在集合语义上使用。
- bag_remove_exact_test.cpp：用 ForEachItem 收集进 std::map 再比较，跨两堆的断言注释写的是「无论先抽哪一堆」。
- bag_remove_by_guid_test.cpp、player_equip_test.cpp、player_equip_attribute_test.cpp、player_feature_persistence_test.cpp、player_mission_system_test.cpp（marshal 往返只比较 guid 集合，或在无改动时比较字节）：只用 guid API。
- player_feature_snapshot_test.cpp（快照已显式排序）、player_battle_settlement_test.cpp（第 1 批正在改，本批不碰）、player_activity_schedule_test.cpp、player_feature_handler_test.cpp、scene_spawn_test.cpp：没有用到受影响的 API。
以上全部只需重编，不改源码。
另外 cpp/tests/cross_zone_test/cross_zone_test.cpp 和 cpp/tests/currency_test/asset_op_system_test.cpp 包含了 bag 头文件，同样只需重编。

### 4.6 6. （需协调）删除死的 tlsEcs.itemRegistry（F021 的另一半）

**文件**：
- `cpp/libs/engine/thread_context/ecs_context.h`
- `cpp/libs/engine/thread_context/ecs_context.cpp`

- 删除 `entt::registry itemRegistry;`（ecs_context.h:30）和 `itemRegistry.clear();`（ecs_context.cpp:57）。已 grep 全仓 cpp/ 与 tools/：除这两行外 0 个引用。container_layout.h:18 只是历史注释，不动，以免触碰布局层文件。
- 前置条件：开工前先 `git status` / `git log -3 -- cpp/libs/engine/thread_context/ecs_context.*`。如果「单例改 ctx()」（F018/F019/F020）那一批已经认领了 ecs_context.*，就把这两行删除移交给它，本批只在审计文档里注明「F021 剩余：itemRegistry 删除已并入 X 批」。
- 这一步改变 sizeof(EcsContext)，所有包含 ecs_context.h 的编译单元都会重编（等于整个 game.sln），所以放在最后单独做。

### 4.7 7. 文档：设计、审计状态、进度，以及双版本对账

**文件**：
- `docs/design/bag-instance-layout-split.md`
- `docs/design/bag-rule-policy-layering.md`
- `docs/design/ecs-entt-misuse-audit-20261015.md`
- `docs/PROGRESS.md`
- `D:/luyuan/xuanming-server-mmo/PARITY.md（跨仓库，需用户同意，见 decisions_needed）`

【bag-instance-layout-split.md】
- §3.1：「拥有」改为 `items_`（guid → ItemComp，唯一权威），删除 EntityOf 和「guidToEntity_ 与 registry_ 必须同步」那段。
- §3.3 / 第 183 行：不可复制的理由改为 unique_ptr 成员。
- §4「搬到实例层」表：`ItemStore::guidToEntity_ + registry_` 一行加注「2026-10-15 改为 items_」。
- 第 225 行：「view<ItemComp>」改为「实例表」。
- 第 496 行：「每个玩家 4+N 个 entt::registry」补注已消除。
- 在「## 关联」之前新增「## 14. 实例层去 entt 与选择顺序（2026-10-15）」，内容如下：
  - 一张规则表：实例序的定义；并堆 = 实例序；合并分组与幸存者 = 实例序，空堆不参与；回收回执 = 实例序；扣除 = 槽位号升序再实例序（D22）；整理平局 = 实例序；批量按 config = 升序；淘汰 = 实例序；ForEach / 写盘 = 实例序；
  - 与 Java Bag.java 的逐条对照；
  - 删除两个实体访问器的说明；
  - §9.1 重建注意：sizeof(Bag) 又变了；
  - 未编译，待 Codex 验证。

【bag-rule-policy-layering.md】
- 第 432 行「悬空的 entity」改为「已销毁的 guid」。
- 第 567 行 P2 那一行后面补注：2026-10-15 起 StackFill 存 guid，失效时 Find 为空、记 LOG_ERROR，不再有 entt 的未定义行为。

【ecs-entt-misuse-audit-20261015.md】
附录里 F021 / F023 / F024 三行的「HEAD 状态」填「已改（第 3 批，待 Codex 验证）」。F021 是否连 itemRegistry 一起修，按第 6 步的实际情况写。这个文件第 1 批也会改，编辑前重读，只改这三格。

【PROGRESS.md】
只追加一条，写明：
- 改动文件；
- 规则表；
- 客户端可见的变化（哪一堆被扣、被灌、被留下；批量入包的格位；整理平局）；
- 「未编译，待 Codex 验证」；
- Java 状态：0.1.0-SNAPSHOT 已经按同一口径实现，本批是 C++ 向 Java 对齐，Java 无需改代码。

【Java PARITY.md 背包行】（需用户同意）
- 有意差异 ② 改为「mmorpg <本批 commit> 起同基线」；
- 「mmorpg 待做（可选）」里删掉「确定遍历顺序」；
- 补注：C++ 独有的 RemoveItemsExact（强化 / 镶嵌扣材料）也按格子号升序，Java 将来移植时照此实现。

## 6. 测试

- 【以下全部由 Codex 执行，Claude 不编译、不跑测试（AGENTS §10.1）】工作目录 D:\luyuan\xuanming-server-mmo-cpp。Debug|x64，MSBuild 一律 /m:1 串行。
- 前置（基线）：在应用改动之前，或者用改动前的最近一次结果，记录 bag_test 已知失败的用例清单。docs/PROGRESS.md:6011 记过 6 个 BagRemoveByGuidTest 的顺序依赖失败；bag_test.cpp 末尾 BagSegmentNotReadyTest 之后会铸号的用例也可能受影响。用来区分既有问题和本批引入的问题。
- 防旧 obj（split 文档 §9.1）：sizeof(Bag) 和 PlayerBagsComp 的步长都变了。cross_zone_test 等工程链接 modules.lib 加 scene.lib，bag_marshal.obj 里 entt::basic_storage<PlayerBagsComp> 的实例化必须是新步长。先执行 `Get-ChildItem cpp/libs/modules/bag -Recurse -File | ForEach-Object { $_.LastWriteTime = Get-Date }`。
- 编译：`msbuild game.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64`，至少覆盖 cpp\libs\modules\modules.vcxproj → cpp\libs\services\scene\scene.vcxproj → cpp\nodes\scene\scene.vcxproj。如果做了第 6 步（ecs_context），就必须编完整个 sln。编完核对 bag_marshal.obj、player_database_loader.obj 比 item_store.h、bag_system.h 新。
- 单测：依次执行 `pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter bag_test`、`-Filter cross_zone_test`、`-Filter currency_test`（asset_op_system_test 链接了背包），顺序不能并行。
- 通过标准一：BagSelectionOrderTest.*（11 条）和 ItemStoreTest.*（2 条）全部 PASS。
- 通过标准二：bag_test 原有套件里，除基线已知的顺序依赖失败外全部 PASS，重点看 BagTest.* 的 Merge / Neaten / Drain、CompactPolicyTest.*、EvictionPolicyTest.*、AcquireSeqTest.*、FixedSlotLayoutTest.*、BagBatchAtomicityTest.*、BagRemoveExactTest.*、BagInstanceDataTest.*、PlayerEquip*。cross_zone_test 和 currency_test 全部 PASS。
- 可选红证据（只在本地临时回退，验证完恢复）：DrainOneConfig 传空 rank，ClampedDrainTakesLowestSlotFirst 和 ExactDrainTakesLowestSlotFirst 应该 FAIL；IsMergeCandidate 去掉 size>0，MergeNeverPicksAnEmptyStackAsSurvivor 应该 FAIL；SortedByConfigId 改成直接遍历 map，CountMapBatch 用例可能 FAIL（取决于哈希序，不保证一定变红）。
- 静态核对：`git grep -n "GetItemByGuid\|GetItemByPos\|EntityOf(\|guidToEntity_\|registry_" -- cpp/libs/modules/bag cpp/tests` 应该零命中；`git grep -n "entt" -- cpp/libs/modules/bag/item_store.* cpp/libs/modules/bag/eviction_policy.* cpp/libs/modules/bag/bag_system.* cpp/libs/modules/bag/container_layout.*` 只允许出现在注释或历史说明里。
- 失败处理：保留 MSBuild 前 50 条 error 和 gtest 失败输出原文，不连续重试。如果是旧用例失败，先用 --gtest_filter 单独跑一遍，确认是不是既有的顺序依赖，再报告。

## 7. 并行拆分、风险与拍板项

**并行拆分**：文件规模小（实质代码约 8 个文件），推荐一个实施代理按 1→2→3→4→5→7 顺序做完，第 6 步最后单独做或移交出去。如果一定要并行，按下面这样切，彼此文件不重叠，接口以本规格里的签名为准：
- A：item_store.{h,cpp} + eviction_policy.{h,cpp}。产出 AcquiredBefore、DrainRank 和 DrainStacks 新签名、StackFill.guid。
- B：bag_system.{h,cpp} + bag_service.{h,cpp}。按规格调用 A 的新签名，只是要等 A 合入后才能编译。
- C：bag_selection_order_test.cpp + bag_test.vcxproj + .vcxproj.filters。
- D：四份文档。
- E（需协调）：ecs_context.{h,cpp}。
与其他工作流的边界：
- 第 1 批独占的 player_battle.cpp、player_database_loader.cpp、player_battle_settlement_test.cpp 等文件，本批一律不改。bag_marshal.cpp 和 player_feature_snapshot.cpp 也不改，它们只是跟着 ForEachItem 变成按实例序。
- 审计文档、PROGRESS.md（union 合并）、bag_test.vcxproj 可能被其他批次同时编辑，改之前重读，只做局部插入。
- gRPC 迁移会话（grpc-muduo-transport.md §10.1）和本批没有交集。

**风险**：
- sizeof(Bag) 和 PlayerBagsComp 的步长变了。如果只增量重编一部分，链接不会报错，但运行时内存会损坏（split 文档 §9.1：is_power_of_two 断言或 emplace 读 0xFFFF… 访问违例）。必须先把 bag 目录的时间戳刷新再全量编译，并核对 bag_marshal.obj 比头文件新。
- 客户端可见的行为变化（刻意的，为了与 Java 对齐）：并堆先灌最早入包的未满堆（原来是最新的）；扣除先扣槽位号最小的堆（原来按 entt 逆序）；合并时留下最早入包的堆，空堆不再成为幸存者；批量入包按 config 升序占格；整理平局按入包先后。上线后同一操作和旧版本结果不同，但不涉及数据迁移，存档格式不变。
- 写盘顺序变成实例序。上线后第一次存盘，blob 字节和读入时不同。如果有按字节判断「没变就跳过存盘」的逻辑，每个玩家会多一次写入，只发生一次，无害。之后存档往返的顺序稳定；原来每往返一次顺序就反转一次。
- 滚动升级期间新旧 scene 节点并存，同一玩家在不同版本节点上的选堆结果会不同。存档格式没变，跨节点交接安全。
- item_store.h 去掉了 registry.hpp。如果某个编译单元靠这条间接包含使用 entt::registry，会编译失败。已核对 bag 模块内部没有这种用法；player_bags_comp.h 仍然包含 registry.hpp，可以兜住大多数使用方。万一报错，在那个编译单元里显式 include 即可。
- ForEach 和几个选堆函数每次调用都要排序（n ≤ 约 500 件/玩家）。调用方有存盘 marshal、191 快照、属性重算（只遍历装备栏 ≤10 件）、战斗快照、强化预检，都不在每帧路径上。以后如果有人在每帧路径上调用 ForEachItem，需要重新评估。
- 仍然存在的 Java 差异（不在本批范围）：C++ 扣到 0 的「空堆」要等紧接着的 kMergeOnly 才回收，Java 是扣完当场回收。生产里扣除之后都会立刻调用 MergeOnly，最终状态一致；但旧存档里 size=0 的条目，C++ 会在 191 里显示 count=0（Java 有意差异 ③）。
- 既有的用例顺序依赖：bag_test.cpp 末尾会把号段永久置为不可用，它后面的铸号用例会失败。新用例自己装号段来规避，但 Codex 判定结果时必须对照基线失败清单。
- 并发编辑：审计文档、PROGRESS.md、bag_test.vcxproj、ecs_context.* 可能同时被其他工作流修改。第 6 步如果和「单例改 ctx()」那一批撞上，要移交给对方。

**需负责人拍板**（未拍板前不要实施相关步骤，其余步骤照常进行）：
- 是否允许实施代理在 Java 仓库 D:/luyuan/xuanming-server-mmo/PARITY.md 的「背包」行登记本次对齐（只改文档）。要改的内容：有意差异 ② 改为同基线；删掉「mmorpg 待做：确定遍历顺序」；注明 RemoveItemsExact 也按格子号升序。AGENTS §12 要求登记，但这是跨仓库写入；如果不允许，就由用户或 Java 会话来登记。

**Java 版（AGENTS §12）**：已对照 Java 版 D:/luyuan/xuanming-server-mmo（HEAD 18c48b8，0.1.0-SNAPSHOT）的 game-scene/.../player/Bag.java、BagItem.java、bag/BagService.java 以及 PARITY.md 的背包行。Java 已经把「基线依赖 EnTT 遍历序」列为有意差异 ②，并按以下口径实现，本批让 C++ 逐条对齐：
- 并堆目标、合并分组与幸存者、合并回收顺序、临时格淘汰，都按 BagItem.ACQUIRE_ORDER，即 (acquireSeq, guid 无符号)。C++ 的 ItemStore::AcquiredBefore 是同一个定义。
- 战斗结算夹紧扣除（drainClamped）按格子号升序（scene-battle-spec D22，客户端可见的是 191 里哪一堆变少）。C++ 的 DrainOneConfig 传槽位号作 rank，同口径。C++ 独有的 RemoveItemsExact（强化 / 镶嵌，Java 未移植）也用这一条，Java 将来移植时照此实现。
- 批量入包和夹紧扣除按配置号无符号升序（TreeMap）。C++ 用 SortedByConfigId。
- 整理重排键为（配置升序、数量降序、ACQUIRE_ORDER）。C++ 的 DesiredOrder 补上了平局键。
- Java 的合并分组写的是包含空堆，但 Java 扣空就当场回收、加载时丢弃 count=0，实际上不会有空堆参与合并。C++ 合并排除空堆、再由 CollectEmptyInstances 回收，所以在「扣除 → mergeOnly」这条生产路径上，两版最终状态相同（幸存 guid、数量、格位都一致）。
结论：Java 无需改代码，属于「已对齐」。需要在 PARITY.md 背包行更新状态（见 decisions_needed），并在本仓库 PROGRESS.md 注明 Java 版本 0.1.0-SNAPSHOT。仍保留、不在本批范围的差异：Java ①（整批原子先铸齐号）、③（不留空堆、加载时丢弃 count=0）。

## 8. 挑战者意见（实施时必须处理）

结论：**needs_changes**

- **[medium]** 第 2 步把临时格「先进先出」的正确性挪到了 ItemStore::ForEach 的遍历顺序上。现状 eviction_policy.cpp:34-42 是自己按 (seq, guid) 做 partial_sort；规格改成「直接取 ForEach 的前 needed 个」。这样一条会销毁玩家资产的规则，依赖的是另一个函数注释里写的顺序约定。哪天有人为了性能，把 ForEach 改回「不保证顺序」（191 快照 player_feature_snapshot.cpp:145、CollectBonus、HasStackWithRoom 都不关心顺序），FIFO 会悄悄挤错人，没有任何编译期或接口层面的提示。这违背 §11「显式依赖 / 信息隐藏」，也没有做到规格自己说的「实例序只在 AcquiredBefore 一处定义」：淘汰用的是 ForEach 的副作用，而不是那个定义本身。
  - 修法：SelectVictims 显式用 ItemStore::AcquiredBefore：收集 (const ItemComp*) 后对前 needed 个做 partial_sort，或者 ItemStore 公开一个 `GuidsInAcquireOrder()`，淘汰直接调它。ForEach 写盘按实例序可以保留，但淘汰不要依赖它。删除 AcquisitionOrderOf 的做法照旧。
- **[low]** java_parity 写的是「Java 无需改代码，已对齐，仍保留的差异只有 ①③」，漏了 PARITY.md 背包行的有意差异 ⑥：老档 acquire_seq==0 的条目，Java 接在已有最大序号之后补盖，C++ 的 ItemStore::Insert 按重放顺序盖 nextAcquireSeq_++。按 item_store.cpp:41-49，重放 [A(seq0)→1, B(seq1)] 时 B 保留 1，会盖出重复序号。本批把实例序扩大到并堆、合并幸存者、整理平局和写盘，所以对这类老档，两版「先灌哪一堆、留哪一堆」在客户端看到的结果会不同，不再只影响淘汰。
  - 修法：二选一。(a) 在 java_parity、设计文档 §14 和 PROGRESS 里把 ⑥ 列为仍保留的差异（老档未盖章条目的实例序两版不同）。(b) 在 C++ 补一个还原收尾步骤（例如 Bag/ItemStore 提供 FinishRestore，把 seq==0 的条目按重放顺序重新盖在最大序号之后）。(b) 要改 bag_marshal::Unmarshal，超出本批文件范围，建议走 (a)，另开一项跟进。
- **[low]** 第 4 步写「在 bag_service.h 这两个函数 drainedOut 的注释里写明抽取顺序」。但 BagService::RemoveItemsExact（bag_service.h:187-193）没有 drainedOut 参数，它把回执只用在内部写流水（bag_service.cpp:466-479），只有 RemoveItemsClamped 带 drainedOut。
  - 修法：扣除顺序的说明改写到 RemoveItemsExact 的函数注释里（例如「逐条流水的顺序 = config_id 升序，同 config 内槽位号升序」），RemoveItemsClamped 照原计划写在 drainedOut 注释上。
- **[low]** 代码注释和设计文档准备写「与 Java drainClamped 的 D22 同口径」。本仓库已经有另一个 D22：docs/design/cross-zone-matchmaking.md:355（RatingEnabled 只控制 Kafka 消费）。scene-battle-spec 的 D22 在 Java 仓库里，本仓库的人按编号查会查到错误的决策。
  - 修法：注释和文档引用时写全来源，例如「Java 仓库 scene-battle-spec D22（战斗结算夹紧扣除按格子号升序）」，或者直接写规则本身，不只给编号。
- **[low]** 规则表把「ForEach / 写盘 = 实例序」和其他行一起列为「与 Java Bag.java 逐条对照、已对齐」。实际上 Java 存档按 (bag_type, 格子) 升序写出（PARITY.md 背包行「存档：…按 (bag_type, 格子) 升序写出」）。这一行两版不一样，只是存档不是客户端契约，所以无害；但文档不能写成已对齐。另外风险第 3 条说「之后存档往返的顺序稳定」，而 bag_marshal.cpp:130 遍历的 dynamicBags_ 是 std::unordered_map<uint64_t, Bag>（player_bags_comp.h），动态包之间的写出顺序仍是哈希序。
  - 修法：规则表的写盘行标成「C++ 实例序 / Java 格子序，存档非客户端契约，有意不同」。「写盘稳定」的说法限定在四个固定包内的条目，或者注明动态包之间仍是哈希序（本批不改 bag_marshal.cpp）。
- **[low]** 第 7 步的文档清单漏了几处：bag-instance-layout-split.md:270 的 §6 不变量 4（「ItemStore 内 guidToEntity_ 与 registry_ 严格同步」），删掉这两个成员后这条不变量就失效了；同文件的 §8「行为差异清单」（第 293 行起），本批是刻意的客户端可见行为变化（灌哪堆、扣哪堆、留哪堆、批量占格、整理平局），按该文档的惯例应在 §8 登记，不应只写进新的 §14；bag-rollback-feasibility-analysis.md:121/144/149/189/217 仍把 itemRegistry_ 描述成背包的现状。
  - 修法：§6 不变量 4 改为「items_ 是唯一权威，键 == item_id；创建只走 Insert，销毁只走 Erase/Clear」。§8 增加本批的行为差异行，或者指向 §14。rollback 分析文档顶部加一行「2026-10-15 起 ItemStore 不再用 entt，下文 itemRegistry_ 描述已过时」。
- **[low]** 规格宣布「items_ 的键是唯一权威，键 == item_id」，但 ForEach、CollectEmptyInstances、DrainStacks 回执、ApplyStackFill 回执用的都是 item->item_id()，而 Find()/GetItemCompByGuid 返回的是可写 ItemComp*（bag_system.h:268）。只要有调用方经可写指针改了 item_id，键和值就会分叉，后续 layout_->SlotOf(guid) 和流水会拿错身份，而且没有任何断言。旧实现也有同样的隐患，但新设计既然声明了键是唯一权威，就应该让它成立。
  - 修法：遍历和回执一律取 map 的键（it->first），item_id 只当冗余字段。或者在 InAcquireOrder 收集时加 `assert(key == item.item_id())`，至少在 Debug 下能发现分叉。
- **[low]** DrainStacks 规格写的是在排序比较器里调用 drainRank(guid)。这会做 O(n log n) 次 std::function 调用，每次都是一次 FlatLayout 反向索引的哈希查找。数量级很小，但写法比必要的复杂。还有一个更简单的形状：rank 回调本质上是桥层给出的「候选顺序」。
  - 修法：排序前先算出 (rank, ItemComp*) 对，再按 (rank, AcquiredBefore) 排序。也可以考虑让 Bag 把 config 的候选 guid 按槽位号排好后传给 DrainStacks(configId, count, const std::vector<Guid>& order)，不在 order 里的实例排在最后。两种做法都不破坏实例层不认识槽位的接缝。
- **[low]** 验证口径没有考虑与第 1 批共用同一个测试可执行文件。bag_test.vcxproj 编进了 player_battle_settlement_test.cpp、player_equip_attribute_test.cpp、player_feature_*_test.cpp，这几个文件当前工作区都有在途改动（git status 为 M）。bag_test 还链接 scene.lib，第 1 批正在改的 player_attribute / actor_attribute_calculator 等都在这个库里。如果第 1 批还没编通，第 3 批的 bag_test 会因为无关原因编译或运行失败，Codex 很难区分。
  - 修法：测试步骤要求 Codex 先确认第 1 批已在同一工作区编通，或者在同一工作区状态下先跑基线。判定时先用 --gtest_filter=BagSelectionOrderTest.*:ItemStoreTest.*:BagTest.*:CompactPolicyTest.*:EvictionPolicyTest.*:AcquireSeqTest.*:BagRemoveExactTest.*:BagInstanceDataTest.* 跑本批相关套件，再看全量。
- **[low]** 可能与审计 C004 的修复撞文件。C004（asset_op_system.cpp:527，PlayerItemBlockList 没有写入点）如果归到别的批次，修法很可能要动 BagService::AddItem/AddItems 的 blockList 参数和封禁检查循环（bag_service.cpp:196-211、279-295）。第 3 批第 4 步恰好要把 bag_service.cpp:196 的循环改成遍历 ordered。规格的边界说明只列了第 1 批和 gRPC 迁移，没有提到这一处。
  - 修法：在 parallel_split 的边界里写明：动 bag_service.cpp 之前先确认 C004 是否已被其他批次认领。如果已认领，本批只改 228 行的写入循环，196 行封禁循环的排序交给 C004 那一批，或者约定以先合入的版本为准再改。

**遗漏的调用点**：
- cpp/libs/modules/bag/eviction_policy.cpp:34 —— 淘汰应显式用 ItemStore::AcquiredBefore 排序，不依赖 ForEach 的遍历顺序
- cpp/libs/modules/bag/bag_service.h:187 —— RemoveItemsExact 没有 drainedOut，扣除顺序说明应写在函数注释里
- cpp/libs/services/scene/player/system/bag_marshal.cpp:130 —— dynamicBags_（unordered_map）遍历，动态包写出顺序仍是哈希序（「写盘稳定」的说法须限定范围）
- docs/design/bag-instance-layout-split.md:270 —— §6 不变量 4 仍写 guidToEntity_ 与 registry_ 同步
- docs/design/bag-instance-layout-split.md:293 —— §8 行为差异清单未登记本批的客户端可见变化
- docs/design/bag-rollback-feasibility-analysis.md:121,144,149,189,217 —— 仍把 itemRegistry_ 描述成现状
- docs/design/cross-zone-matchmaking.md:355 —— 本仓库已有另一个 D22，引用 Java 的 D22 须写全来源
- D:/luyuan/xuanming-server-mmo/PARITY.md:77 有意差异 ⑥（老档 seq==0 补盖口径）—— java_parity 漏列，实例序在老档上两版不同
- cpp/libs/modules/bag/bag_service.cpp:196-211 —— 与审计 C004（PlayerItemBlockList）可能的修复改同一段，需协调

## 9. 回执

按 [README.md](README.md) §6 的格式交回。另外逐条说明 §8 每个 high/medium 问题是怎么处理的。

## 9. 2026-10-15 Codex 源码回执

源码完成：item_store、bag_system、bag_service、eviction_policy 八份生产文件；bag_selection_order_test.cpp 新增18条行为用例并登记工程。ItemStore 单一 Guid→ItemComp 字典替代私有 EnTT 池，map key 为身份权威，删除零调用方实体访问器；所有选择顺序显式，淘汰后重新规划，rank 每候选只计算一次，空堆不参与合并幸存者。布局层接缝与失败原子边界保留，未新增持久字段或其它号源。itemRegistry 的死字段已随第2批删除。

§8 挑战通过代码和用例处理：不借 ForEach 隐式顺序做 FIFO，不把排序比较器变成重复布局查询；保留已紧凑早退及实例/布局成对删除；批量配置升序覆盖预检和写入；C004 单人封禁没有新增写入口，仍按业务待定记录。Java 正常实例选择与现有 BagItem.ACQUIRE_ORDER 对齐，但发号原子、零堆、老档seq0补章及存档排列的历史差异保留，不宣称两版完全一致。

定向 diff 检查和工程XML解析通过，18条用例未运行，无本轮红/绿证据。Bag 布局变化后必须重编 modules/scene/相关测试，不能混链旧库。完整验证边界与 Java 0.1.0-SNAPSHOT 状态见 README §7；未 commit/push。

## 10. 全部源码收尾：原子提交、还原与 C004（2026-10-15）

本节接续 §9，覆盖当时保留的相关源码差异。`Bag::AddItem`、计数批量和完整实例批量共用候选提交：先校验并算出淘汰不动点，按最终所需实例数铸齐 ID，再复制仓库完成合堆、初始化及插入。预设 guid、equip presence 和每项真实写入回执保留；任何拒绝都不改原包、布局、入包序号或资产流水，允许已取号作废。

为保证布局试放失败也不破坏原包，最小扩展 `IContainerLayout::Clone() const -> std::unique_ptr<IContainerLayout>`；Flat/FixedSlot/Grid 及测试故障布局均保留动态类型与独立占用。在副本放置成功后，仓库和布局在无回调提交段一起发布。布局层仍不认识配置、ItemComp 或 entt。所有成功入包，包括只合堆，都会使原 `ItemComp*` 借用失效；公开契约已注明，活跃装备入口按 guid 重取。删除无调用的旧 ReserveForBatchAdd/ReserveBatch/ReserveOrEvict 旁路，旧测试改走真实批量入口。

扣到零当场删除实例与槽位；加载跳过零堆；每包先扫描非零最大 acquire_seq，再按存档顺序补旧零章。存盘固定包按 bag_type/slot，动态包按 bag_id/slot，保留布局。Java `BagItem.ACQUIRE_ORDER` 补为 `Long.compareUnsigned`，覆盖 uint64 跨符号位的序号与 guid；不新增协议字段。上述为本任务相关行为对齐，未扩展既有入包序号耗尽协议。

负责人选择 C004 删除方案：删除 PlayerItemBlockList 类型、服务参数和全部调用，使用现有全服封禁；任务奖励、装备退宝石等失败原子性与解除封禁后的成功断言保留。新增 `BagJavaParityTest` 11 例，覆盖最终规划后发号耗尽、服务 mutated=false、无空间早拒、回调前铸齐 ID、逐项回执、候选布局试放失败、三种布局 Clone、扣空释放、零章补盖、零堆还原和写盘顺序；Java 新增两项高位顺序回归。原有 18 项选择顺序测试仍保留。

静态替代证据：源签名/全部调用点、布局接缝与派生布局 Clone 已交叉核对，定向 diff 和工程 XML 检查通过。未实际编译或运行任何新旧测试，没有红/绿证据；生成后应串行重编 modules/scene/bag_test，运行整个 bag_test（含任务、装备、初始化用例），再执行 Java 对应 BagTest。完整验证命令与日志要求沿 README §7。Java 仍为 0.1.0-SNAPSHOT，本轮变化及未验收项已登记 PARITY。
