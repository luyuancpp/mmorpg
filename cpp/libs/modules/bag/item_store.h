#pragma once

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "engine/core/type_define/type_define.h"

#include "item_system.h"

// ─────────────────────────────────────────────────────────────────────────
// 实例层(item store)
//
// 物品实例是**领域实体**:有自己的身份(item_uuid,全局唯一、跨堆叠拆合稳定)、
// 自己的生命期(铸号 -> 存在 -> 销毁),以及自己的状态(config_id + size,
// 2026-10-07 起装备实例还带一段 equip —— 随机属性行,二期起强化等级与已镶宝石也在
// 这一段里(EquipInstanceData,docs/design/equipment-enhance-inlay.md §3.1);绑定态 /
// 限时等仍是预留)。本层把 ItemComp 当**整份**搬运与存放,不解释 equip 的内容,
// 只在堆叠判定上认它的 presence(见 CanStack)。
//
// 这一层只管"有哪些实例、它们各是什么、数量怎么在实例之间分配"。
// 它**不认识槽位、不认识容量、不知道背包满没满** —— 那些全在布局层
// (container_layout.h)。两层的唯一接头是一个 Guid。
//
// 堆叠算术之所以留在这一层而不是布局层,是因为它回答的是"这批数量要拆成
// 几个实例",不是"占几格"。拆分前这两个问题被 GridsNeededFor 这个名字混成
// 了一个,正是两层焊死的症状之一。
// ─────────────────────────────────────────────────────────────────────────

class ItemStore
{
public:
    // ── 身份与生命期 ──────────────────────────────────────────────────
    [[nodiscard]] std::size_t Size() const { return items_.size(); }
    [[nodiscard]] bool Contains(Guid guid) const { return items_.contains(guid); }

    // 返回仓库内的实例。调用方不得改 item_id / acquire_seq,也不得把指针留到下一次
    // Insert / Erase / Clear / 整理之后;接口不承诺跨写操作保留借用。
    ItemComp *Find(Guid guid);
    [[nodiscard]] const ItemComp *Find(Guid guid) const;

    // 当前水位(下一个会盖出的序号)。只读,给用例断言用。
    [[nodiscard]] uint64_t NextAcquireSeq() const { return nextAcquireSeq_; }

    // 存入整份实例并给 acquire_seq 盖章(0 则盖当前水位;非 0 则保留并抬高水位)。
    // guid 撞车时返回 nullptr,实例和入包水位都不改变。
    // proto 必须已经填好 item_id / config_id / size。
    //
    // items_ 是唯一权威,键始终等于 item_id;创建只走 Insert,销毁只走 Erase / Clear。
    ItemComp *Insert(ItemComp proto);

    // 销毁一个实例。未知 guid 是安全的空操作,返回 false。
    // 注意:**它不管槽位** —— 释放槽位是布局层的事,由桥层 Bag 成对调用。
    bool Erase(Guid guid);

    // 丢掉所有实例。容量 / 槽位不归它管,一点都不会动。
    void Clear();

    // 包内实例序:(acquire_seq 升序,item_id 升序),与 Java BagItem.ACQUIRE_ORDER 同口径。
    // guid 在包内唯一,所以这是一条全序;理论不可达的零序号排最前。
    [[nodiscard]] static bool AcquiredBefore(const ItemComp &lhs, const ItemComp &rhs);

    // 按实例序以 (guid, ItemComp) 遍历。回调不得修改本仓库,借用遵守 Find 的寿命约束。
    // guid 取唯一权威 map 的键,不从可写消息字段重新推导身份。
    template <typename Fn>
    void ForEach(Fn &&fn) const
    {
        for (const auto *entry : InAcquireOrder())
        {
            fn(entry->first, entry->second);
        }
    }

    // 某个 config 在本仓库里的总数量(跨所有堆叠求和)。
    [[nodiscard]] std::size_t TotalCountOf(uint32_t configId) const;

    // 是否持有 required 列出的每一项(config_id -> 需求数量)。只读。
    [[nodiscard]] bool HasAll(const ItemCountMap &required) const;

    // ── 堆叠语义 ──────────────────────────────────────────────────────
    // 一次计划中的补堆:往既有 guid 里再倒 amount 个单位。
    struct StackFill
    {
        Guid guid{kInvalidGuid};
        uint32_t amount{0};
    };

    // 对 wanted 里的每个 config,汇总既有堆叠还空着多少(config_id -> 空余单位)。
    // 不在 wanted 里的、以及不可叠加(满于 1)的 config 一律贡献 0。
    //
    // exclude(可为 nullptr):把这些 guid 的实例当作**已经不存在**来算。
    // 淘汰腾位前用它回答"如果这几件被挤掉了,这批还要新建几个实例" —— 被挤掉的
    // 可能正是本批打算并进去的未满堆,不排除就会把需求算小,于是"腾完仍放不下"。
    [[nodiscard]] ItemCountMap MeasureFreeRoomPerConfig(
        const ItemCountMap &wanted, const std::unordered_set<Guid> *exclude = nullptr) const;

    // 规划(不改任何状态)进来的数量如何填进同 config 的既有堆叠。
    // 填充计划写进 outFillPlan,返回还需要**新建实例**才装得下的剩余量。
    // exclude 语义同 MeasureFreeRoomPerConfig。
    uint32_t PlanStackIntoExistingStacks(const ItemComp &proto, uint32_t maxStackSize,
                                         std::vector<StackFill> &outFillPlan,
                                         const std::unordered_set<Guid> *exclude = nullptr) const;

    // 执行 PlanStackIntoExistingStacks 的计划(真正改 size)。
    // writtenGuidsOut(可为 nullptr)按写入顺序收集被灌入的既有堆 guid ——
    // 并堆不铸新号,但这一堆确实是本次数量的去处,流水按它才追溯得到。
    void ApplyStackFill(const std::vector<StackFill> &fillPlan,
                        std::vector<Guid> *writtenGuidsOut);

    // 一条"被抽走"的回执:哪个实例、抽走多少。实例本身仍在(可能 size==0)。
    // 与 StackFill 是一对:入包记灌进了哪些堆,扣除记抽了哪些堆。
    struct StackDrain
    {
        Guid guid{kInvalidGuid};
        uint32_t amount{0};
    };

    // 从某个 config 的若干堆叠里抽走 count 个单位。数量可能分散在多堆,
    // 逐堆抽到够为止。被抽光的堆 size 变 0 但**实例保留**(槽位也就跟着留着),
    // 后续并堆可以再填回去 —— 这与 Erase(guid) 的"彻底销毁"是两回事。
    //
    // 库存不足时按实际持有夹紧(本来就是逐堆 min(需求, 库存)),**返回值即实扣量**;
    // 调用方要"不够就整体失败"的语义,必须自己先 HasAll(Bag::RemoveItems 就是这么做的)。
    // drainedOut(可为 nullptr)按抽取顺序收集 (guid, 抽走量),size==0 的僵尸堆不进回执。
    // rank 由调用方提供,实例层不解释它。桥层传槽位号,按 rank 再按实例序抽取。
    // 回调只读且不重入仓库;每个候选的 rank 在排序前只计算一次。
    using DrainRank = std::function<uint64_t(Guid)>;
    uint32_t DrainStacks(uint32_t configId, uint32_t count, const DrainRank &drainRank,
                         std::vector<StackDrain> *drainedOut = nullptr);

    // 存不存在"同一 config 有 >= 2 个非空未满堆"。空堆只回收,不当合并幸存者。
    [[nodiscard]] bool HasMergeablePartials() const;

    // 把同 config 的未满堆合并成尽量少的满堆:组内总量重新分配,前几个填满、
    // 最后一个放余数、多出来的清成 size=0。
    //
    // **它自己不销毁任何实例** —— 销毁必须两层成对进行(那是桥层的活)。
    // 被清空的那些不单独返回:它们已经是 size==0,由紧随其后的
    // CollectEmptyInstances 一并扫出来,不必维护两条重复的名单。
    void MergePartialStacks();

    // 存不存在 size==0 的实例。这种实例什么都不装,却仍然占着一个槽位。
    //
    // 两条来路:①可叠加物品被抽光(DrainStacks 刻意保留空堆,好让后续并堆填回去);
    // ②**不可叠加物品被 RemoveItemByPos 扣到 0** —— 这一类 MergePartialStacks
    // 永远碰不到(它跳过 max_stack_size() <= 1),拆分前没有任何路径回收得了,
    // 就是一个永久占格的僵尸。现在由整理统一回收。
    [[nodiscard]] bool HasEmptyInstances() const;

    // 列出所有 size==0 的实例 guid,交给桥层连同槽位一起销毁。
    // 与 MergePartialStacks 一样,**它自己不销毁任何实例**。
    [[nodiscard]] GuidVector CollectEmptyInstances() const;

    // 两个实例能不能并成一堆:同 config,且**双方都不带实例数据**(equip 段)。
    // 后一条是兜底:装备本该 max_stack_size == 1、根本不走堆叠,但表一旦被误配成可堆叠,
    // 并堆只加数量,两件属性不同的装备会并成一堆、其中一份属性无声消失。
    // 判的是 has_equip() 这个 presence,不是属性条数。
    //
    // MeasureFreeRoomPerConfig 和合并两处的 IsMergeCandidate 同样排除 equip。
    // size>0 只约束合并;并堆刻意允许往空堆回填。
    [[nodiscard]] static bool CanStack(const ItemComp &leftItem, const ItemComp &rightItem);

    // totalSize 个单位、每堆最多 maxStackSize 个,要拆成几个实例。
    // 拆分前它叫 GridsNeededFor("要几格"),名字把实例数和格子数混为一谈;
    // 在"1 实例恒占 1 格"的扁平布局下两者恰好相等,格子布局下就不等了。
    [[nodiscard]] static std::size_t StacksNeededFor(std::size_t totalSize,
                                                     std::size_t maxStackSize);

    // ── guid 发号(实例身份)────────────────────────────────────────
    // 唯一发号源 = item 种类的号段实例(tlsGuidSegmentRegistry.Get(GuidKind::kItem));
    // 拿不到号(未启用 / 两段耗尽且续段未到)返回 kInvalidGuid,**没有** snowflake 回退。
    [[nodiscard]] static Guid MintGuid();

    // 发号源当前能否铸出合法 guid(号段:手里有号)。
    // 铸号型写入必须在**改动任何状态之前**用它 fail-closed,见 .cpp 注释。
    [[nodiscard]] static bool CanMintGuid() { return CanMintGuids(1); }

    // 同上,但要求一次能铸出 count 个。号段是有限库存:段尾只剩 k 个而下一段又没到时,
    // 一批要 n > k 个号的入包会在循环中途铸出哨兵、留下半批 —— 整批预检必须按需求量问。
    [[nodiscard]] static bool CanMintGuids(std::size_t count);

    [[nodiscard]] static bool IsInvalidGuid(const ItemComp &item);

private:
    using Items = std::unordered_map<Guid, ItemComp>;
    // 节点容器在 rehash 时不移动实例,排序候选只借用节点地址,不复制物品数据。
    // 借用仅限本次调用;任何排序或 ForEach 回调都不得修改容器结构。
    [[nodiscard]] std::vector<const Items::value_type *> InAcquireOrder() const;
    [[nodiscard]] std::vector<Items::value_type *> InAcquireOrder();
    [[nodiscard]] static bool IsMergeCandidate(const ItemComp &item, uint32_t maxStack);

    Items items_{};

    // 下一个要盖的入包序号。1 起步,0 是"没盖章"的哨兵。
    // 只由 Insert 推进、只由 Clear 复位 —— 见 Insert 里的注释。
    uint64_t nextAcquireSeq_{1};
};
