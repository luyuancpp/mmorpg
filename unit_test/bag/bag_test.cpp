#include <gtest/gtest.h>

#include "src/game_config/item_config.h"

#include "src/game_logic/bag/bag.h"
#include "src/return_code/error_code.h"

using namespace common;

TEST(BagTest, NullItem)
{
    Bag bag;
    EXPECT_EQ(nullptr, bag.GetItemByGuid(0));
}

TEST(BagTest, AddNewGridItem)
{
    Bag bag;
    CreateItemParam p;
    p.config_id_ = 1;
    auto item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(1, bag.item_size());
    EXPECT_EQ(1, bag.pos_size());
    EXPECT_EQ(bag.GetItemByBos(0)->config_id(), p.config_id_);
    EXPECT_EQ(bag.GetItemByBos(0)->size(), p.size_);
    EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->config_id(), p.config_id_);
    EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->size(), p.size_);
    EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByBos(0)->guid());
    EXPECT_EQ(0, bag.GetItemPos(g_server_sequence.Current()));
    EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByGuid(g_server_sequence.Current())->guid());
}

TEST(BagTest, AddNewGridItemFull)
{
    Bag bag;
    CreateItemParam p;
    p.config_id_ = 1;
    for (uint32_t i = 0; i < (uint32_t)BagCapacity::kDefualtCapacity; i++)
    {
        auto item = CreateItem(p);
        EXPECT_EQ(kRetOK, bag.AddItem(item));
        EXPECT_EQ(i + 1, bag.item_size());
        EXPECT_EQ(i + 1, bag.pos_size());
        EXPECT_EQ(bag.GetItemByBos(i)->config_id(), p.config_id_);
        EXPECT_EQ(bag.GetItemByBos(i)->size(), p.size_);
        EXPECT_EQ(bag.GetItemByBos(i)->config_id(), p.config_id_);
        EXPECT_EQ(bag.GetItemByBos(i)->size(), p.size_);
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->config_id(), p.config_id_);
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->size(), p.size_);
        EXPECT_EQ(i, bag.GetItemPos(g_server_sequence.Current()));
        EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByBos(i)->guid());
        EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByGuid(g_server_sequence.Current())->guid());
    }   
    EXPECT_EQ(BagCapacity::kDefualtCapacity, bag.item_size());
    EXPECT_EQ(BagCapacity::kDefualtCapacity, bag.pos_size());

    auto item = CreateItem(p);
    EXPECT_EQ(kRetBagAddItemBagFull, bag.AddItem(item));
    bag.Unlock(BagCapacity::kDefualtCapacity);
    for (uint32_t i = 0; i < (uint32_t)BagCapacity::kDefualtCapacity; i++)
    {
        auto item = CreateItem(p);
        EXPECT_EQ(kRetOK, bag.AddItem(item));
        uint32_t newindex = i + (uint32_t)BagCapacity::kDefualtCapacity;
        EXPECT_EQ(newindex + 1 , bag.item_size());
        EXPECT_EQ(newindex + 1, bag.pos_size());
        EXPECT_EQ(bag.GetItemByBos(newindex)->config_id(), p.config_id_);
        EXPECT_EQ(bag.GetItemByBos(newindex)->size(), p.size_);
        EXPECT_EQ(bag.GetItemByBos(newindex)->config_id(), p.config_id_);
        EXPECT_EQ(bag.GetItemByBos(newindex)->size(), p.size_);
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->config_id(), p.config_id_);
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->size(), p.size_);
        EXPECT_EQ(newindex, bag.GetItemPos(g_server_sequence.Current()));
        EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByBos(newindex)->guid());
        EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByGuid(g_server_sequence.Current())->guid());
    }
    EXPECT_EQ(BagCapacity::kDefualtCapacity * 2, bag.item_size());
    EXPECT_EQ(BagCapacity::kDefualtCapacity * 2, bag.pos_size());
}

TEST(BagTest, AddStackItemFull)
{
    Bag bag;
    CreateItemParam p;
    p.config_id_ = 15;
    p.size_ = get_item_conf(p.config_id_)->max_statck_size();
    for (uint32_t i = 0; i < (uint32_t)BagCapacity::kDefualtCapacity; i++)
    {
        auto item = CreateItem(p);
        EXPECT_EQ(kRetOK, bag.AddItem(item));
        EXPECT_EQ(i + 1, bag.item_size());
        EXPECT_EQ(i + 1, bag.pos_size());
        EXPECT_EQ(bag.GetItemByBos(i)->config_id(), p.config_id_);
        EXPECT_EQ(bag.GetItemByBos(i)->size(), p.size_);
        EXPECT_EQ(bag.GetItemByBos(i)->config_id(), p.config_id_);
        EXPECT_EQ(bag.GetItemByBos(i)->size(), p.size_);
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->config_id(), p.config_id_);
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->size(), p.size_);
        EXPECT_EQ(i, bag.GetItemPos(g_server_sequence.Current()));
        EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByBos(i)->guid());
        EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByGuid(g_server_sequence.Current())->guid());
    }
    EXPECT_EQ(BagCapacity::kDefualtCapacity, bag.item_size());
    EXPECT_EQ(BagCapacity::kDefualtCapacity, bag.pos_size());

    auto item = CreateItem(p);
    EXPECT_EQ(kRetBagAddItemBagFull, bag.AddItem(item));
    bag.Unlock(BagCapacity::kDefualtCapacity);
    for (uint32_t i = 0; i < (uint32_t)BagCapacity::kDefualtCapacity; i++)
    {
        auto item = CreateItem(p);
        EXPECT_EQ(kRetOK, bag.AddItem(item));
        uint32_t newindex = i + (uint32_t)BagCapacity::kDefualtCapacity;
        EXPECT_EQ(newindex + 1, bag.item_size());
        EXPECT_EQ(newindex + 1, bag.pos_size());
        EXPECT_EQ(bag.GetItemByBos(newindex)->config_id(), p.config_id_);
        EXPECT_EQ(bag.GetItemByBos(newindex)->size(), p.size_);
        EXPECT_EQ(bag.GetItemByBos(newindex)->config_id(), p.config_id_);
        EXPECT_EQ(bag.GetItemByBos(newindex)->size(), p.size_);
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->config_id(), p.config_id_);
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->size(), p.size_);
        EXPECT_EQ(newindex, bag.GetItemPos(g_server_sequence.Current()));
        EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByBos(newindex)->guid());
        EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByGuid(g_server_sequence.Current())->guid());
    }
    EXPECT_EQ(BagCapacity::kDefualtCapacity * 2, bag.item_size());
    EXPECT_EQ(BagCapacity::kDefualtCapacity * 2, bag.pos_size());
}

TEST(BagTest, Del)
{
    Bag bag;
    Guid guid{kInvalidGuid};
    EXPECT_EQ(kRetOK, bag.DelItem(guid));
}

TEST(BagTest, Update)
{
}

TEST(BagTest, Query)
{
    Bag bag;
    Guid guid{ kInvalidGuid };
    EXPECT_EQ(kRetOK, bag.DelItem(guid));
}

int main(int argc, char** argv)
{
    item_config::GetSingleton().load();
    return RUN_ALL_TESTS();
}

