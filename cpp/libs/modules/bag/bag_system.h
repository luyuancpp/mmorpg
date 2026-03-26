#pragma once

#include <unordered_set>

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

struct BagCapacity
{
    std::size_t size_{kDefaultCapacity};
};

enum EnumBagType : uint32_t
{
    kBag = 0,
    kWarehouse = 1,
    kEquipment = 2,
    kTemporary = 3,
    kBagMax = 4,
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
    Bag();
    ~Bag();

    std::size_t size() const { return capacity_; }
    [[nodiscard]] Guid PlayerGuid() const { return player_guid_; }
    std::size_t ItemGridSize() const { return items_.size(); }
    std::size_t PosSize() const { return pos_.size(); }
    const PosMap &pos() const { return pos_; }

    std::size_t GetItemStackSize(uint32_t config_id) const;
    ItemComp *GetItemBaseByGuid(Guid guid);
    ItemComp *GetItemBaseByPos(uint32_t pos);
    entt::entity GetItemByGuid(Guid guid);
    entt::entity GetItemByPos(uint32_t pos);
    uint32_t GetItemPos(Guid guid);

    uint32_t HasEnoughSpace(const ItemCountMap &itemsToAdd);
    uint32_t HasSufficientItems(const ItemCountMap &requiredItems);
    uint32_t RemoveItems(const ItemCountMap &itemsToRemove);
    uint32_t RemoveItemByPos(const RemoveItemByPosParam &param);

    bool IsFull() const { return items_.size() >= size(); }
    bool HasSufficientSpace(std::size_t s) const
    {
        ValidateCapacity();
        return size() - items_.size() >= s;
    }
    bool IsSpaceInsufficient(std::size_t s) const
    {
        ValidateCapacity();
        return size() - items_.size() < s;
    }

    uint32_t AddItem(const InitItemParam &itemParam);
    uint32_t RemoveItem(Guid guid);

    void Neaten();
    void Unlock(std::size_t additionalSize);

    static Guid LastGeneratorItemGuid();

    static std::size_t CalculateStackGridSize(std::size_t itemStackSize, std::size_t stackSize);

    // ── GM block/unblock item acquisition ─────────────────────────────

    // Block a specific item config_id — all future AddItem calls for this
    // config will be rejected.
    void BlockItem(uint32_t configId);

    // Unblock a previously blocked item config_id.
    void UnblockItem(uint32_t configId);

    // Query whether an item config_id is currently blocked.
    bool IsItemBlocked(uint32_t configId) const;

    // Return all currently blocked config IDs (for GM query / serialisation).
    const std::unordered_set<uint32_t> &BlockedItems() const { return blocked_config_ids_; }

private:
    Guid GeneratorItemGuid();
    bool IsInvalidItemGuid(const ItemComp &item) const;
    void DestroyItem(Guid guid);
    uint32_t OnNewGrid(Guid guid);
    static bool CanStack(const ItemComp &item1, const ItemComp &item2);

    uint32_t AddNonStackableItem(ItemComp itemPBComp);
    uint32_t AddStackableItem(ItemComp itemPBComp, uint32_t maxStackSize);

    std::size_t empty_grid_size() const
    {
        ValidateCapacity();
        return size() - items_.size();
    }
    void ValidateCapacity() const { assert(size() >= items_.size()); }

    entt::entity entity_{};
    ItemsMap items_{};
    PosMap pos_{};
    uint32_t type_{};
    std::size_t capacity_{kDefaultCapacity};
    entt::registry itemRegistry_{};
    Guid player_guid_{kInvalidGuid};
    std::unordered_set<uint32_t> blocked_config_ids_;
};