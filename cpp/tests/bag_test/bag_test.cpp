#include <gtest/gtest.h>

#include "engine/core/type_define/type_define.h"

#include "table/code/item_table.h"
#include "modules/bag/bag_system.h"
#include "modules/gain_block/gain_block_service.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/bag_error_tip.pb.h"
#include <node_config_manager.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

uint32_t MaxStack(uint32_t configId) {
    return ItemTableManager::Instance().GetTable(configId).first->max_statck_size();
}

InitItemParam MakeItem(uint32_t configId, uint32_t size) {
    InitItemParam p;
    p.itemPBComp.set_config_id(configId);
    p.itemPBComp.set_size(size);
    return p;
}

InitItemParam MakeItem(uint32_t configId) {
    return MakeItem(configId, MaxStack(configId));
}

/// Verify the last added item is at `pos` with expected config and size.
void VerifyLastAdded(Bag& bag, uint32_t pos, uint32_t configId, uint32_t size) {
    const auto guid = Bag::LastGeneratorItemGuid();
    auto* byPos  = bag.GetItemBaseByPos(pos);
    auto* byGuid = bag.GetItemBaseByGuid(guid);
    ASSERT_NE(nullptr, byPos);
    ASSERT_NE(nullptr, byGuid);
    EXPECT_EQ(configId, byPos->config_id());
    EXPECT_EQ(size,     byPos->size());
    EXPECT_EQ(configId, byGuid->config_id());
    EXPECT_EQ(size,     byGuid->size());
    EXPECT_EQ(guid,     byPos->item_id());
    EXPECT_EQ(guid,     byGuid->item_id());
    EXPECT_EQ(pos,      bag.GetItemPos(guid));
}

RemoveItemByPosParam MakeRemoveParam(Bag& bag, uint32_t pos, uint32_t configId, uint32_t removeSize = 1) {
    RemoveItemByPosParam dp;
    dp.pos_ = pos;
    dp.item_guid_ = bag.GetItemBaseByPos(pos)->item_id();
    dp.item_config_id_ = configId;
    dp.size_ = removeSize;
    return dp;
}

// Config IDs (matches item_table.json)
constexpr uint32_t kNonStack1  = 1;   // non-stackable
constexpr uint32_t kNonStack2  = 2;   // non-stackable
constexpr uint32_t kStack9     = 9;   // stackable
constexpr uint32_t kStack10    = 10;  // stackable
constexpr uint32_t kStack11    = 11;  // stackable

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(BagTest, NullItem)
{
    Bag bag;
    EXPECT_EQ(nullptr, bag.GetItemBaseByGuid(0));
}

TEST(BagTest, AddNewGridItem)
{
    Bag bag;
    auto item = MakeItem(kNonStack1);
    EXPECT_EQ(kSuccess, bag.AddItem(item));
    EXPECT_EQ(1, bag.ItemGridSize());
    EXPECT_EQ(1, bag.PosSize());
    VerifyLastAdded(bag, 0, kNonStack1, item.itemPBComp.size());
}

TEST(BagTest, AddNewGridItemFull)
{
    Bag bag;
    auto item = MakeItem(kNonStack1);

    // Fill initial capacity
    for (uint32_t i = 0; i < (uint32_t)kDefaultCapacity; i++)
    {
        EXPECT_EQ(kSuccess, bag.AddItem(item));
        EXPECT_EQ(i + 1, bag.ItemGridSize());
        EXPECT_EQ(i + 1, bag.PosSize());
        VerifyLastAdded(bag, i, kNonStack1, item.itemPBComp.size());
    }
    EXPECT_EQ(kDefaultCapacity, bag.ItemGridSize());
    EXPECT_EQ(kDefaultCapacity, bag.PosSize());

    // Full — adding one more fails
    EXPECT_EQ(kBagAddItemBagFull, bag.AddItem(item));

    // Unlock more slots and fill again
    bag.Unlock(kDefaultCapacity);
    for (uint32_t i = 0; i < (uint32_t)kDefaultCapacity; i++)
    {
        EXPECT_EQ(kSuccess, bag.AddItem(item));
        uint32_t idx = i + (uint32_t)kDefaultCapacity;
        EXPECT_EQ(idx + 1, bag.ItemGridSize());
        EXPECT_EQ(idx + 1, bag.PosSize());
        VerifyLastAdded(bag, idx, kNonStack1, item.itemPBComp.size());
    }
    EXPECT_EQ(kDefaultCapacity * 2, bag.ItemGridSize());
    EXPECT_EQ(kDefaultCapacity * 2, bag.PosSize());
}

TEST(BagTest, Add10CanStack10CanNotStack)
{
    Bag bag;
    bag.Unlock(kDefaultCapacity);

    // Fill all initial slots with non-stackable items
    auto nonStack = MakeItem(kNonStack1, MaxStack(kNonStack1) * kDefaultCapacity);
    EXPECT_EQ(kSuccess, bag.AddItem(nonStack));

    // Fill unlocked slots with stackable items
    auto stackable = MakeItem(kStack10, MaxStack(kStack10) * kDefaultCapacity);
    EXPECT_EQ(kSuccess, bag.AddItem(stackable));

    EXPECT_EQ(kDefaultCapacity * 2, bag.ItemGridSize());
    EXPECT_EQ(kDefaultCapacity * 2, bag.PosSize());
}

TEST(BagTest, AddStackItem12121212)
{
    Bag bag;
    auto maxStack = MaxStack(kStack9);
    auto halfStack = maxStack / 2;
    uint32_t totalSize = 0;

    for (uint32_t i = 0; i < (uint32_t)kDefaultCapacity * 2; i++)
    {
        uint32_t addSize = (i % 2 == 0) ? halfStack : maxStack;
        auto item = MakeItem(kStack9, addSize);
        totalSize += addSize;

        auto ret = bag.AddItem(item);
        if (ret != kSuccess) break;
        EXPECT_EQ(kSuccess, ret);

        auto gridSize = Bag::CalculateStackGridSize(totalSize, maxStack);
        uint32_t lastIdx = uint32_t(gridSize - 1);

        EXPECT_EQ(gridSize, bag.ItemGridSize());
        EXPECT_EQ(gridSize, bag.PosSize());

        if (totalSize % maxStack == 0)
        {
            EXPECT_EQ(maxStack, bag.GetItemBaseByPos(lastIdx / 2)->size());
        }
        else
        {
            EXPECT_EQ(halfStack, (uint32_t)bag.GetItemBaseByPos(lastIdx)->size());
            EXPECT_EQ(halfStack, bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->size());
        }

        EXPECT_EQ(kStack9, bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->config_id());
        EXPECT_EQ(lastIdx, bag.GetItemPos(Bag::LastGeneratorItemGuid()));
        EXPECT_EQ(Bag::LastGeneratorItemGuid(), bag.GetItemBaseByPos(lastIdx)->item_id());
        EXPECT_EQ(Bag::LastGeneratorItemGuid(), bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->item_id());
    }

    EXPECT_EQ(kDefaultCapacity, bag.ItemGridSize());
    EXPECT_EQ(kDefaultCapacity, bag.PosSize());
}

TEST(BagTest, AddStackItemUnlock)
{
    Bag bag;
    auto item = MakeItem(kStack10);

    // Fill initial capacity with full stacks
    for (uint32_t i = 0; i < (uint32_t)kDefaultCapacity; i++)
    {
        EXPECT_EQ(kSuccess, bag.AddItem(item));
        EXPECT_EQ(i + 1, bag.ItemGridSize());
        EXPECT_EQ(i + 1, bag.PosSize());
        VerifyLastAdded(bag, i, kStack10, item.itemPBComp.size());
    }
    EXPECT_EQ(kDefaultCapacity, bag.ItemGridSize());
    EXPECT_EQ(kDefaultCapacity, bag.PosSize());

    // Full — unlock and fill again
    EXPECT_EQ(kBagAddItemBagFull, bag.AddItem(item));
    bag.Unlock(kDefaultCapacity);
    for (uint32_t i = 0; i < (uint32_t)kDefaultCapacity; i++)
    {
        EXPECT_EQ(kSuccess, bag.AddItem(item));
        uint32_t idx = i + (uint32_t)kDefaultCapacity;
        EXPECT_EQ(idx + 1, bag.ItemGridSize());
        EXPECT_EQ(idx + 1, bag.PosSize());
        VerifyLastAdded(bag, idx, kStack10, item.itemPBComp.size());
    }
    EXPECT_EQ(kDefaultCapacity * 2, bag.ItemGridSize());
    EXPECT_EQ(kDefaultCapacity * 2, bag.PosSize());
}

TEST(BagTest, AdequateSizeAddItemCannotStackItemFull)
{
    Bag bag;
    ItemCountMap needed{{kNonStack1, (uint32_t)kDefaultCapacity + 1}};
    EXPECT_EQ(kBagItemNotStacked, bag.HasEnoughSpace(needed));

    needed[kNonStack1] = (uint32_t)kDefaultCapacity;
    EXPECT_EQ(kSuccess, bag.HasEnoughSpace(needed));
}

TEST(BagTest, AdequateSizeAddItemmixtureFull)
{
    Bag bag;
    auto maxStack10 = MaxStack(kStack10);
    ItemCountMap needed{
        {kNonStack1, 1},
        {kStack10, maxStack10 * (uint32_t)kDefaultCapacity}
    };

    // 1 non-stackable + 10 full stacks > capacity
    EXPECT_EQ(kBagItemNotStacked, bag.HasEnoughSpace(needed));

    // 1 non-stackable + 9 full stacks = capacity
    needed[kStack10] = (uint32_t)(kDefaultCapacity - 1) * maxStack10;
    EXPECT_EQ(kSuccess, bag.HasEnoughSpace(needed));

    // Occupy one slot with a non-stackable item
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kNonStack1)));

    // Now 9 remaining slots — 1 non-stackable + 9 stacks won't fit
    EXPECT_EQ(kBagItemNotStacked, bag.HasEnoughSpace(needed));

    // 1 non-stackable + 8 stacks = 9 slots needed
    needed[kStack10] = (uint32_t)(kDefaultCapacity - 2) * maxStack10;
    EXPECT_EQ(kSuccess, bag.HasEnoughSpace(needed));

    // Occupy one more slot with a stackable item
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kStack10)));

    // 8 remaining — 1 + 8 won't fit
    EXPECT_EQ(kBagItemNotStacked, bag.HasEnoughSpace(needed));

    // 1 + 7 = 8 slots needed
    needed[kStack10] = (uint32_t)(kDefaultCapacity - 3) * maxStack10;
    EXPECT_EQ(kSuccess, bag.HasEnoughSpace(needed));

    // Add a partially-filled stack (max - 100)
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kStack10, maxStack10 - 100)));
    EXPECT_EQ(kBagItemNotStacked, bag.HasEnoughSpace(needed));
}

TEST(BagTest, AdequateItem)
{
    Bag bag;
    bag.Unlock(20);
    auto maxStack10 = MaxStack(kStack10);
    auto maxStack11 = MaxStack(kStack11);

    // Empty bag — no items
    ItemCountMap required{{kStack10, 1}};
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(required));

    // Add one full stack of id 10
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kStack10)));
    EXPECT_EQ(kSuccess, bag.HasSufficientItems(required));

    required[kStack10] = maxStack10 / 2;
    EXPECT_EQ(kSuccess, bag.HasSufficientItems(required));

    // Also require 1 non-stackable — not in bag yet
    required.emplace(kNonStack1, 1);
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(required));

    // Add a non-stackable item
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kNonStack1)));
    EXPECT_EQ(kSuccess, bag.HasSufficientItems(required));

    // Require exactly 1 full stack of id 10
    required[kStack10] = maxStack10;
    EXPECT_EQ(kSuccess, bag.HasSufficientItems(required));

    // Require more than available
    required[kStack10] = maxStack10 + 1;
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(required));

    required[kStack10] = maxStack10 * 3;
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(required));

    // Add second stack of id 10 — still not enough for 3 stacks
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kStack10)));
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(required));

    // Add id 11 — doesn't help with id 10 requirement
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kStack11, maxStack11 * 3)));
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(required));

    // Third stack of id 10 — now have 3 stacks
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kStack10)));
    EXPECT_EQ(kSuccess, bag.HasSufficientItems(required));

    // Also require 3 stacks of id 11 — we added 3 stacks above
    required[kStack11] = maxStack11 * 3;
    EXPECT_EQ(kSuccess, bag.HasSufficientItems(required));
}

TEST(BagTest, DelItem)
{
    Bag bag;
    bag.Unlock(20);
    auto maxStack10 = MaxStack(kStack10);
    auto maxStack11 = MaxStack(kStack11);

    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kStack10, maxStack10 * 2)));
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kNonStack1)));
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kNonStack2)));
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kStack11)));

    // Remove 1 unit of id 10
    ItemCountMap toRemove{{kStack10, 1}};
    EXPECT_EQ(kSuccess, bag.RemoveItems(toRemove));
    EXPECT_EQ(maxStack10 * 2 - 1, bag.GetItemStackSize(kStack10));
    EXPECT_EQ(MaxStack(kNonStack1), bag.GetItemStackSize(kNonStack1));
    EXPECT_EQ(MaxStack(kNonStack2), bag.GetItemStackSize(kNonStack2));
    EXPECT_EQ(maxStack11, bag.GetItemStackSize(kStack11));

    // Remove everything remaining
    toRemove[kStack10]  = maxStack10 * 2 - 1;
    toRemove[kNonStack1] = MaxStack(kNonStack1);
    toRemove[kNonStack2] = MaxStack(kNonStack2);
    toRemove[kStack11]  = maxStack11;
    EXPECT_EQ(kSuccess, bag.RemoveItems(toRemove));
    EXPECT_EQ(0, bag.GetItemStackSize(kStack10));
    EXPECT_EQ(0, bag.GetItemStackSize(kNonStack1));
    EXPECT_EQ(0, bag.GetItemStackSize(kNonStack2));
    EXPECT_EQ(0, bag.GetItemStackSize(kStack11));

    // Grid slots remain (empty)
    EXPECT_EQ(5, bag.ItemGridSize());
    EXPECT_EQ(5, bag.PosSize());
    for (uint32_t p : {0u, 1u, 2u, 3u, 4u})
    {
        EXPECT_TRUE(bag.pos().find(p) != bag.pos().end());
    }
}

TEST(BagTest, Del)
{
    Bag bag;
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kNonStack1)));
    EXPECT_EQ(1, bag.ItemGridSize());
    EXPECT_EQ(1, bag.PosSize());

    EXPECT_EQ(kSuccess, bag.RemoveItem(Bag::LastGeneratorItemGuid()));
    EXPECT_EQ(0, bag.ItemGridSize());
    EXPECT_EQ(0, bag.PosSize());
}

TEST(BagTest, RemoveItemByPos)
{
    Bag bag;
    auto maxStack = MaxStack(kStack10);
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kStack10)));
    EXPECT_EQ(1, bag.ItemGridSize());
    EXPECT_EQ(1, bag.PosSize());

    // Missing fields → various errors
    RemoveItemByPosParam dp;
    EXPECT_EQ(kBagDelItemPos, bag.RemoveItemByPos(dp));

    dp.pos_ = 0;
    EXPECT_EQ(kBagDelItemGuid, bag.RemoveItemByPos(dp));

    dp.item_guid_ = Bag::LastGeneratorItemGuid();
    EXPECT_EQ(kBagDelItemConfig, bag.RemoveItemByPos(dp));

    // Remove 1 unit
    dp.item_config_id_ = kStack10;
    EXPECT_EQ(kSuccess, bag.RemoveItemByPos(dp));
    EXPECT_EQ(maxStack - 1, bag.GetItemStackSize(kStack10));

    // Remove the rest
    dp.size_ = maxStack - 1;
    EXPECT_EQ(kSuccess, bag.RemoveItemByPos(dp));
    EXPECT_EQ(0, bag.GetItemStackSize(kStack10));
    EXPECT_EQ(1, bag.ItemGridSize());
    EXPECT_EQ(1, bag.PosSize());
    EXPECT_EQ(0, bag.GetItemBaseByPos(0)->size());
    EXPECT_EQ(0, bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->size());
}

/// Helper: fill `count` slots of `configId`, then reduce each to 1 unit.
void FillAndReduceToOne(Bag& bag, uint32_t configId, uint32_t startPos, uint32_t count) {
    auto maxStack = MaxStack(configId);
    for (uint32_t i = startPos; i < startPos + count; ++i)
    {
        auto dp = MakeRemoveParam(bag, i, configId, maxStack - 1);
        EXPECT_EQ(kSuccess, bag.RemoveItemByPos(dp));
    }
}

TEST(BagTest, Neaten1)
{
    Bag bag;
    bag.Unlock(kDefaultCapacity);

    // Fill both halves with full stacks
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kStack10, MaxStack(kStack10) * kDefaultCapacity)));
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kStack11, MaxStack(kStack11) * kDefaultCapacity)));

    // Reduce every slot to 1 unit
    FillAndReduceToOne(bag, kStack10, 0, (uint32_t)kDefaultCapacity);
    FillAndReduceToOne(bag, kStack11, (uint32_t)kDefaultCapacity, (uint32_t)kDefaultCapacity);

    for (uint32_t i = 0; i < (uint32_t)bag.PosSize(); ++i)
        EXPECT_EQ(1, bag.GetItemBaseByPos(i)->size());

    bag.Neaten();

    // 10 units of each config → 1 slot each
    EXPECT_EQ(2, bag.ItemGridSize());
    EXPECT_EQ(2, bag.PosSize());
    for (auto& [pos, guid] : bag.pos())
        EXPECT_EQ(1, (std::size_t)bag.GetItemBaseByPos(bag.GetItemPos(guid))->size());
    EXPECT_EQ(kDefaultCapacity, bag.GetItemStackSize(kStack10));
    EXPECT_EQ(kDefaultCapacity, bag.GetItemStackSize(kStack11));
}

TEST(BagTest, Neaten400)
{
    Bag bag;
    constexpr std::size_t kSlots = 400;
    constexpr std::size_t kHalf = kSlots / 2;
    bag.Unlock(kSlots);

    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kStack10, MaxStack(kStack10) * (uint32_t)kHalf)));
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kStack11, MaxStack(kStack11) * (uint32_t)kHalf)));

    FillAndReduceToOne(bag, kStack10, 0, (uint32_t)kHalf);
    FillAndReduceToOne(bag, kStack11, (uint32_t)kHalf, (uint32_t)kHalf);

    for (uint32_t i = 0; i < (uint32_t)bag.PosSize(); ++i)
        EXPECT_EQ(1, bag.GetItemBaseByPos(i)->size());

    bag.Neaten();

    EXPECT_EQ(2, bag.ItemGridSize());
    EXPECT_EQ(2, bag.PosSize());
    for (auto& [pos, guid] : bag.pos())
        EXPECT_EQ(1, (std::size_t)bag.GetItemBaseByPos(bag.GetItemPos(guid))->size());
    EXPECT_EQ(kHalf, bag.GetItemStackSize(kStack10));
    EXPECT_EQ(kHalf, bag.GetItemStackSize(kStack11));
}

TEST(BagTest, Neaten400_1)
{
    Bag bag;
    constexpr std::size_t kSlots = 400;
    constexpr std::size_t kHalf = kSlots / 2;
    constexpr std::size_t kQuarter = kSlots / 4;
    constexpr std::size_t kMaxStack = 999;
    bag.Unlock(kSlots);

    // Fill 200 slots of each item type
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kStack10, MaxStack(kStack10) * (uint32_t)kHalf)));
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kStack11, MaxStack(kStack11) * (uint32_t)kHalf)));

    // Reduce only the first quarter of each item to 1 unit
    FillAndReduceToOne(bag, kStack10, 0, (uint32_t)kQuarter);
    FillAndReduceToOne(bag, kStack11, (uint32_t)kHalf, (uint32_t)kQuarter);

    // Layout: [0..99]=1, [100..199]=999, [200..299]=1, [300..399]=999
    for (uint32_t i = 0; i < (uint32_t)bag.PosSize(); ++i)
    {
        if (i < kQuarter)
            EXPECT_EQ(1, bag.GetItemBaseByPos(i)->size());
        else if (i < kHalf)
            EXPECT_EQ(kMaxStack, bag.GetItemBaseByPos(i)->size());
        else if (i < kHalf + kQuarter)
            EXPECT_EQ(1, bag.GetItemBaseByPos(i)->size());
        else
            EXPECT_EQ(kMaxStack, bag.GetItemBaseByPos(i)->size());
    }

    bag.Neaten();

    // 200 full stacks + 2 partial (100 units each)
    EXPECT_EQ(kHalf + 2, bag.ItemGridSize());
    EXPECT_EQ(kHalf + 2, bag.PosSize());

    UInt32Set pos999, pos1;
    for (uint32_t i = 0; i < (uint32_t)bag.PosSize(); ++i)
    {
        auto sz = bag.GetItemBaseByPos(i)->size();
        if (sz == kMaxStack) pos999.emplace(i);
        else if (sz == 1) pos1.emplace(i);
    }
    EXPECT_EQ(kHalf, pos999.size());
    EXPECT_EQ(2, pos1.size());
    EXPECT_EQ(kHalf / 2 * kMaxStack + kQuarter, bag.GetItemStackSize(kStack10));
    EXPECT_EQ(kHalf / 2 * kMaxStack + kQuarter, bag.GetItemStackSize(kStack11));
}

TEST(BagTest, NeatenCanNotStack)
{
    Bag bag;
    bag.Unlock(kDefaultCapacity);

    // First half: stackable, second half: non-stackable
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kStack10, MaxStack(kStack10) * kDefaultCapacity)));
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kNonStack1, MaxStack(kNonStack1) * kDefaultCapacity)));

    // Reduce stackable slots to 1 unit each
    FillAndReduceToOne(bag, kStack10, 0, (uint32_t)kDefaultCapacity);

    for (uint32_t i = 0; i < (uint32_t)bag.PosSize(); ++i)
        EXPECT_EQ(1, bag.GetItemBaseByPos(i)->size());

    bag.Neaten();

    // 1 consolidated stack + kDefaultCapacity non-stackable slots
    EXPECT_EQ(kDefaultCapacity + 1, bag.ItemGridSize());
    EXPECT_EQ(kDefaultCapacity + 1, bag.PosSize());
    for (auto& [pos, guid] : bag.pos())
    {
        auto sz = (std::size_t)bag.GetItemBaseByPos(bag.GetItemPos(guid))->size();
        if (sz != kDefaultCapacity)
            EXPECT_EQ(1, sz);
    }
    EXPECT_EQ(kDefaultCapacity, bag.GetItemStackSize(kStack10));
    EXPECT_EQ(kDefaultCapacity, bag.GetItemStackSize(kNonStack1));
}

// ---------------------------------------------------------------------------
// BlockItem / UnblockItem tests
// ---------------------------------------------------------------------------

TEST(BagTest, BlockItemPreventsAdd)
{
    Bag bag;
    bag.Unlock(kBagMaxCapacity);

    // Block the non-stackable item config
    bag.BlockItem(kNonStack1);
    EXPECT_TRUE(bag.IsItemBlocked(kNonStack1));

    // AddItem should fail for blocked config
    EXPECT_NE(kSuccess, bag.AddItem(MakeItem(kNonStack1)));
    EXPECT_EQ(0, bag.ItemGridSize());

    // Other items unaffected
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kStack10, 1)));
    EXPECT_EQ(1, bag.ItemGridSize());
}

TEST(BagTest, UnblockItemAllowsAdd)
{
    Bag bag;
    bag.Unlock(kBagMaxCapacity);

    bag.BlockItem(kNonStack1);
    EXPECT_NE(kSuccess, bag.AddItem(MakeItem(kNonStack1)));

    bag.UnblockItem(kNonStack1);
    EXPECT_FALSE(bag.IsItemBlocked(kNonStack1));
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kNonStack1)));
    EXPECT_EQ(1, bag.ItemGridSize());
}

TEST(BagTest, BlockItemIdempotent)
{
    Bag bag;
    bag.BlockItem(kNonStack1);
    bag.BlockItem(kNonStack1); // duplicate — should not crash
    EXPECT_TRUE(bag.IsItemBlocked(kNonStack1));

    bag.UnblockItem(kNonStack1);
    EXPECT_FALSE(bag.IsItemBlocked(kNonStack1));
}

TEST(BagTest, BlockedItemsQuery)
{
    Bag bag;
    EXPECT_TRUE(bag.BlockedItems().empty());

    bag.BlockItem(kNonStack1);
    bag.BlockItem(kStack10);
    EXPECT_EQ(2, bag.BlockedItems().size());
    EXPECT_TRUE(bag.BlockedItems().contains(kNonStack1));
    EXPECT_TRUE(bag.BlockedItems().contains(kStack10));
}

// ===========================================================================
// GainBlockService — server-wide (global) block tests
// ===========================================================================

TEST(GainBlockServiceTest, GlobalItemBlockPreventsAdd)
{
    GainBlockService::ClearAllGlobalBlocks();
    Bag bag;
    bag.Unlock(kBagMaxCapacity);

    // Before global block, item can be added
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kNonStack1)));
    EXPECT_EQ(1, bag.ItemGridSize());

    // Global block on kStack10
    GainBlockService::BlockGlobal(GainBlockService::GainType::kItem, kStack10);
    EXPECT_TRUE(GainBlockService::IsGloballyBlocked(GainBlockService::GainType::kItem, kStack10));

    // AddItem should fail for globally blocked item
    EXPECT_NE(kSuccess, bag.AddItem(MakeItem(kStack10, 1)));
    EXPECT_EQ(1, bag.ItemGridSize()); // unchanged

    // Non-blocked items still work
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kNonStack2)));
    EXPECT_EQ(2, bag.ItemGridSize());

    GainBlockService::ClearAllGlobalBlocks();
}

TEST(GainBlockServiceTest, GlobalUnblockAllowsAdd)
{
    GainBlockService::ClearAllGlobalBlocks();
    Bag bag;
    bag.Unlock(kBagMaxCapacity);

    GainBlockService::BlockGlobal(GainBlockService::GainType::kItem, kNonStack1);
    EXPECT_NE(kSuccess, bag.AddItem(MakeItem(kNonStack1)));

    GainBlockService::UnblockGlobal(GainBlockService::GainType::kItem, kNonStack1);
    EXPECT_FALSE(GainBlockService::IsGloballyBlocked(GainBlockService::GainType::kItem, kNonStack1));
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kNonStack1)));

    GainBlockService::ClearAllGlobalBlocks();
}

TEST(GainBlockServiceTest, GlobalBlockIdempotent)
{
    GainBlockService::ClearAllGlobalBlocks();

    GainBlockService::BlockGlobal(GainBlockService::GainType::kItem, kNonStack1);
    GainBlockService::BlockGlobal(GainBlockService::GainType::kItem, kNonStack1);
    EXPECT_TRUE(GainBlockService::IsGloballyBlocked(GainBlockService::GainType::kItem, kNonStack1));
    EXPECT_EQ(1, GainBlockService::GlobalBlockedIds(GainBlockService::GainType::kItem).size());

    GainBlockService::UnblockGlobal(GainBlockService::GainType::kItem, kNonStack1);
    EXPECT_FALSE(GainBlockService::IsGloballyBlocked(GainBlockService::GainType::kItem, kNonStack1));

    GainBlockService::ClearAllGlobalBlocks();
}

TEST(GainBlockServiceTest, ClearAllGlobalBlocks)
{
    GainBlockService::ClearAllGlobalBlocks();

    GainBlockService::BlockGlobal(GainBlockService::GainType::kItem, kNonStack1);
    GainBlockService::BlockGlobal(GainBlockService::GainType::kItem, kStack10);
    GainBlockService::BlockGlobal(GainBlockService::GainType::kCurrency, 0);
    GainBlockService::BlockGlobal(GainBlockService::GainType::kCurrency, 1);

    EXPECT_EQ(2, GainBlockService::GlobalBlockedIds(GainBlockService::GainType::kItem).size());
    EXPECT_EQ(2, GainBlockService::GlobalBlockedIds(GainBlockService::GainType::kCurrency).size());

    GainBlockService::ClearAllGlobalBlocks();

    EXPECT_TRUE(GainBlockService::GlobalBlockedIds(GainBlockService::GainType::kItem).empty());
    EXPECT_TRUE(GainBlockService::GlobalBlockedIds(GainBlockService::GainType::kCurrency).empty());
}

TEST(GainBlockServiceTest, GlobalCurrencyBlockCheck)
{
    GainBlockService::ClearAllGlobalBlocks();

    EXPECT_FALSE(GainBlockService::IsGainBlocked(GainBlockService::GainType::kCurrency, 0));

    GainBlockService::BlockGlobal(GainBlockService::GainType::kCurrency, 0);
    EXPECT_TRUE(GainBlockService::IsGainBlocked(GainBlockService::GainType::kCurrency, 0));
    EXPECT_FALSE(GainBlockService::IsGainBlocked(GainBlockService::GainType::kCurrency, 1));

    GainBlockService::ClearAllGlobalBlocks();
}

TEST(GainBlockServiceTest, GlobalAndPerPlayerBlockCombo)
{
    GainBlockService::ClearAllGlobalBlocks();
    Bag bag;
    bag.Unlock(kBagMaxCapacity);

    // Per-player block on kNonStack1, global block on kStack10
    bag.BlockItem(kNonStack1);
    GainBlockService::BlockGlobal(GainBlockService::GainType::kItem, kStack10);

    EXPECT_NE(kSuccess, bag.AddItem(MakeItem(kNonStack1)));       // per-player blocked
    EXPECT_NE(kSuccess, bag.AddItem(MakeItem(kStack10, 1)));      // globally blocked
    EXPECT_EQ(kSuccess, bag.AddItem(MakeItem(kNonStack2)));       // neither blocked
    EXPECT_EQ(1, bag.ItemGridSize());

    GainBlockService::ClearAllGlobalBlocks();
}

int main(int argc, char** argv)
{
    readBaseDeployConfig("etc/base_deploy_config.yaml", tlsNodeConfigManager.GetBaseDeployConfig());
    readGameConfig("etc/game_config.yaml", tlsNodeConfigManager.GetGameConfig());
    ItemTableManager::Instance().Load();
    testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}
