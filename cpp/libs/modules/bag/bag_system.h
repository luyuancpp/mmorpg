#pragma once

#include "entt/src/entt/entity/registry.hpp"
#include "engine/core/type_define/type_define.h"

#include "item_system.h"

using ItemsMap = std::unordered_map<Guid, entt::entity>;
using PosMap = std::unordered_map<uint32_t, Guid>;

constexpr std::size_t kDefaultCapacity{10};
constexpr std::size_t kEquipmentCapacity{10};
constexpr std::size_t kBagMaxCapacity{100};
constexpr std::size_t kTempBagMaxCapacity{200};
constexpr std::size_t kWarehouseMaxCapacity{200};

// Fixed, always-present bag slots that every player owns. Stored as a
// dense std::array<Bag, kBagTypeCount> in PlayerBagsComp, so each value
// doubles as BOTH the array index AND the persisted bag_type on the wire
// (BagAllData.ItemEntry.bag_type).
//
// Extending this enum:
//   * New PERMANENT per-player bag (pet bag / material bag / ...): append
//     a new entry right before kBagTypeCount and bump nothing else. Never
//     renumber an existing value — the numbers are a persistence contract.
//   * Runtime/temporary bags (event bags, per-pet bags that come and go):
//     these have NO fixed slot, do not add them here. Put them in an
//     id-keyed map instead — see the note in player_bags_comp.h.
enum BagType : uint32_t
{
    kInventory = 0,  // main character inventory (人物背包)
    kWarehouse = 1,  // bank / storage           (仓库)
    kEquipment = 2,  // worn equipment slots     (装备栏)
    kTemporary = 3,  // overflow / loot pickup   (溢出临时格)
    kBagTypeCount = 4,
};

struct RemoveItemByPosParam
{
    Guid item_guid_{kInvalidGuid};
    uint32_t item_config_id_{kInvalidU32Id};
    uint32_t pos_{kInvalidU32Id};
    uint32_t size_{1};
};

class Bag
{
public:
    // Capacity = how many grid slots this bag has unlocked. NOT the number
    // of items currently held (that's OccupiedGridCount()).
    std::size_t Capacity() const { return capacity_; }
    [[nodiscard]] Guid PlayerGuid() const { return player_guid_; }
    std::size_t OccupiedGridCount() const { return items_.size(); }
    std::size_t PositionCount() const { return pos_.size(); }
    const PosMap &pos() const { return pos_; }

    std::size_t GetTotalItemCount(uint32_t config_id) const;
    ItemComp *GetItemCompByGuid(Guid guid);
    ItemComp *GetItemCompByPos(uint32_t pos);
    entt::entity GetItemByGuid(Guid guid);
    entt::entity GetItemByPos(uint32_t pos);
    uint32_t GetItemPos(Guid guid);

    uint32_t HasEnoughSpace(const ItemCountMap &itemsToAdd);
    uint32_t HasEnoughItems(const ItemCountMap &requiredItems);
    uint32_t RemoveItems(const ItemCountMap &itemsToRemove);
    uint32_t RemoveItemByPos(const RemoveItemByPosParam &param);

    bool IsFull() const { return items_.size() >= Capacity(); }
    bool HasSufficientSpace(std::size_t s) const
    {
        ValidateCapacity();
        return Capacity() - items_.size() >= s;
    }
    bool IsSpaceInsufficient(std::size_t s) const
    {
        ValidateCapacity();
        return Capacity() - items_.size() < s;
    }

    uint32_t AddItem(const InitItemParam &itemParam);
    uint32_t RemoveItem(Guid guid);

    void MergeAndCompact();
    void ExpandCapacity(std::size_t additionalSize);

    static Guid LastGeneratedItemGuid();

    static std::size_t GridsNeededFor(std::size_t itemStackSize, std::size_t stackSize);

    // ── Persistence/migration accessors ───────────────────────────────
    // Added 2026-05-17 to let bag_marshal serialize/restore the bag's
    // state across cross-zone migration / rollback / persistence
    // (see docs/design/cross-zone-readiness-audit.md §3.2 件 1 +
    // cpp/libs/services/scene/player/system/bag_marshal.{h,cpp}).
    //
    // These accessors are intentionally minimal — they expose just
    // enough surface to iterate items and rebuild from a snapshot,
    // without leaking ECS internals. Production gameplay still goes
    // through AddItem / RemoveItem / MergeAndCompact / ExpandCapacity above.

    // Iterate items as (guid, ItemComp) pairs. The ItemComp is the
    // proto stored in the bag's internal item registry — caller MUST
    // NOT mutate or persist the pointer beyond the loop, the next
    // AddItem/RemoveItem can invalidate it.
    template <typename Fn>
    void ForEachItem(Fn&& fn) const
    {
        for (const auto& [entity, item] : itemRegistry_.view<ItemComp>().each())
        {
            // Reverse-lookup the guid: items_ maps guid → entity, so
            // we walk items_ to find the guid matching this entity.
            for (const auto& [guid, mappedEntity] : items_)
            {
                if (mappedEntity == entity)
                {
                    fn(guid, item);
                    break;
                }
            }
        }
    }

    // Get item position by guid (inverse of pos_). Returns kInvalidU32Id
    // if the guid isn't in this bag. Used by marshal to fill
    // ItemEntry.pos.
    uint32_t GetItemPosByGuid(Guid guid) const
    {
        for (const auto& [pos, mappedGuid] : pos_)
        {
            if (mappedGuid == guid)
            {
                return pos;
            }
        }
        return kInvalidU32Id;
    }

    // Replace the bag's contents wholesale. Used by bag_marshal::Unmarshal
    // when receiving a migrating player or restoring a snapshot —
    // destroys whatever was here and rebuilds from the snapshot.
    //
    // NOT for gameplay use. Skips AddItem's anomaly detection /
    // transaction_log emission / gain-block checks because the items
    // were already validated when they were originally added in the
    // source zone / pre-snapshot state.
    void ResetFromSnapshot();

    // Insert one item entry at a known position with a known guid.
    // Companion to ResetFromSnapshot. Used by bag_marshal::Unmarshal
    // to replay items one by one without re-running stack/anomaly logic.
    void InsertItemForRestore(Guid guid, uint32_t configId, uint32_t stackSize, uint32_t pos);

    // Set capacity (replays Bag::ExpandCapacity's effect without the audit log).
    // Used by Unmarshal to restore gameplay-unlocked slots.
    void SetCapacityForRestore(std::size_t capacity)
    {
        capacity_ = capacity;
    }

private:
    // ── Item-storage invariant helpers ────────────────────────────────
    // items_ (guid→entity) and itemRegistry_ (entity→ItemComp) MUST stay
    // in lockstep. To make it impossible to update one and forget the
    // other, ALL creation goes through InsertItemEntity and ALL removal
    // through DestroyItem / ClearAllItems. Do not call itemRegistry_
    // .create()/.destroy() or items_.emplace()/.erase() directly anywhere
    // else.

    // Create the ECS entity for `proto`, store the component, and register
    // it in items_ — atomically. Returns the stored component, or nullptr
    // if the guid already exists (the half-created entity is rolled back).
    // `proto` must already have its item_id / config_id / size set.
    ItemComp *InsertItemEntity(ItemComp proto);

    // Remove one item everywhere: destroys its ECS entity, drops it from
    // items_, and frees its grid slot in pos_. Safe to call with an
    // unknown guid (no-op).
    void DestroyItem(Guid guid);

    // Drop every item the bag holds (entities + items_ + pos_). capacity_
    // is left untouched. Used by ResetFromSnapshot.
    void ClearAllItems();

    Guid GenerateItemGuid();
    bool IsInvalidItemGuid(const ItemComp &item) const;
    uint32_t OnNewGrid(Guid guid);
    static bool CanStack(const ItemComp &item1, const ItemComp &item2);

    uint32_t AddNonStackableItem(ItemComp itemPBComp);
    uint32_t AddStackableItem(ItemComp itemPBComp, uint32_t maxStackSize);

    std::size_t EmptyGridCount() const
    {
        ValidateCapacity();
        return Capacity() - items_.size();
    }
    void ValidateCapacity() const { assert(Capacity() >= items_.size()); }

    ItemsMap items_{};
    PosMap pos_{};
    std::size_t capacity_{kDefaultCapacity};
    entt::registry itemRegistry_{};
    Guid player_guid_{kInvalidGuid};
};