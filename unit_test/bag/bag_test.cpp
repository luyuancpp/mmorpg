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
    p.item_base_db.set_config_id(1);
    auto item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(1, bag.item_size());
    EXPECT_EQ(1, bag.pos_size());
    EXPECT_EQ(bag.GetItemByBos(0)->config_id(), p.item_base_db.config_id());
    EXPECT_EQ(bag.GetItemByBos(0)->size(), p.item_base_db.size());
    EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.LastId())->config_id(), p.item_base_db.config_id());
    EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.LastId())->size(), p.item_base_db.size());
    EXPECT_EQ(g_server_sequence.LastId(), bag.GetItemByBos(0)->guid());
    EXPECT_EQ(0, bag.GetItemPos(g_server_sequence.LastId()));
    EXPECT_EQ(g_server_sequence.LastId(), bag.GetItemByGuid(g_server_sequence.LastId())->guid());
}

//一个一个格子添加
TEST(BagTest, AddNewGridItemFull)
{
    Bag bag;
    CreateItemParam p;
    p.item_base_db.set_config_id(1);
    for (uint32_t i = 0; i < (uint32_t)BagCapacity::kDefualtCapacity; i++)
    {
        auto item = CreateItem(p);
        EXPECT_EQ(kRetOK, bag.AddItem(item));
        EXPECT_EQ(i + 1, bag.item_size());
        EXPECT_EQ(i + 1, bag.pos_size());
        EXPECT_EQ(bag.GetItemByBos(i)->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByBos(i)->size(), p.item_base_db.size());
        EXPECT_EQ(bag.GetItemByBos(i)->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByBos(i)->size(), p.item_base_db.size());
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.LastId())->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.LastId())->size(), p.item_base_db.size());
        EXPECT_EQ(i, bag.GetItemPos(g_server_sequence.LastId()));
        EXPECT_EQ(g_server_sequence.LastId(), bag.GetItemByBos(i)->guid());
        EXPECT_EQ(g_server_sequence.LastId(), bag.GetItemByGuid(g_server_sequence.LastId())->guid());
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
        EXPECT_EQ(bag.GetItemByBos(newindex)->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByBos(newindex)->size(), p.item_base_db.size());
        EXPECT_EQ(bag.GetItemByBos(newindex)->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByBos(newindex)->size(), p.item_base_db.size());
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.LastId())->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.LastId())->size(), p.item_base_db.size());
        EXPECT_EQ(newindex, bag.GetItemPos(g_server_sequence.LastId()));
        EXPECT_EQ(g_server_sequence.LastId(), bag.GetItemByBos(newindex)->guid());
        EXPECT_EQ(g_server_sequence.LastId(), bag.GetItemByGuid(g_server_sequence.LastId())->guid());
    }
    EXPECT_EQ(BagCapacity::kDefualtCapacity * 2, bag.item_size());
    EXPECT_EQ(BagCapacity::kDefualtCapacity * 2, bag.pos_size());
}

//添加可叠加测试1
TEST(BagTest, AddStackItemHalfAdd)
{
    Bag bag;
    CreateItemParam p;
    p.item_base_db.set_config_id(10);
    for (uint32_t i = 0; i < (uint32_t)BagCapacity::kDefualtCapacity * 2; i++)
    {
        if (i % 2 == 0)
        {
            p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size() / 2);
        }
        else
        {
            p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size() / 2 + 1);
        }
        auto item = CreateItem(p);
        EXPECT_EQ(kRetOK, bag.AddItem(item));
        auto index = i / 2;
        auto sz = i / 2 + 1;
        EXPECT_EQ(sz, bag.item_size());
        EXPECT_EQ(sz, bag.pos_size());
        if (i % 2 == 0)
        {
            EXPECT_EQ(bag.GetItemByBos(index)->size(), p.item_base_db.size());
            EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.LastId())->size(), p.item_base_db.size());
        }
        else
        {
            EXPECT_EQ(bag.GetItemByBos(index)->size(), get_item_conf(p.item_base_db.config_id())->max_statck_size());
            EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.LastId())->size(), get_item_conf(p.item_base_db.config_id())->max_statck_size());
        }
        
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.LastId())->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(index, bag.GetItemPos(g_server_sequence.LastId()));
        EXPECT_EQ(g_server_sequence.LastId(), bag.GetItemByBos(index)->guid());
        EXPECT_EQ(g_server_sequence.LastId(), bag.GetItemByGuid(g_server_sequence.LastId())->guid());
    }
    EXPECT_EQ(BagCapacity::kDefualtCapacity, bag.item_size());
    EXPECT_EQ(BagCapacity::kDefualtCapacity, bag.pos_size());
}

//解锁可以继续叠加
TEST(BagTest, AddStackItemUnlock)
{
    Bag bag;
    CreateItemParam p;
    p.item_base_db.set_config_id(10);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());
    for (uint32_t i = 0; i < (uint32_t)BagCapacity::kDefualtCapacity; i++)
    {
        auto item = CreateItem(p);
        EXPECT_EQ(kRetOK, bag.AddItem(item));
        EXPECT_EQ(i + 1, bag.item_size());
        EXPECT_EQ(i + 1, bag.pos_size());
        EXPECT_EQ(bag.GetItemByBos(i)->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByBos(i)->size(), p.item_base_db.size());
        EXPECT_EQ(bag.GetItemByBos(i)->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByBos(i)->size(), p.item_base_db.size());
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.LastId())->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.LastId())->size(), p.item_base_db.size());
        EXPECT_EQ(i, bag.GetItemPos(g_server_sequence.LastId()));
        EXPECT_EQ(g_server_sequence.LastId(), bag.GetItemByBos(i)->guid());
        EXPECT_EQ(g_server_sequence.LastId(), bag.GetItemByGuid(g_server_sequence.LastId())->guid());
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
        EXPECT_EQ(bag.GetItemByBos(newindex)->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByBos(newindex)->size(), p.item_base_db.size());
        EXPECT_EQ(bag.GetItemByBos(newindex)->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByBos(newindex)->size(), p.item_base_db.size());
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.LastId())->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.LastId())->size(), p.item_base_db.size());
        EXPECT_EQ(newindex, bag.GetItemPos(g_server_sequence.LastId()));
        EXPECT_EQ(g_server_sequence.LastId(), bag.GetItemByBos(newindex)->guid());
        EXPECT_EQ(g_server_sequence.LastId(), bag.GetItemByGuid(g_server_sequence.LastId())->guid());
    }
    EXPECT_EQ(BagCapacity::kDefualtCapacity * 2, bag.item_size());
    EXPECT_EQ(BagCapacity::kDefualtCapacity * 2, bag.pos_size());
}

//不可叠加物品
TEST(BagTest, AdequateSizeAddItemCannotStackItemFull)
{
    uint32_t config_id = 1;
    Bag bag;
    UInt32UInt32UnorderedMap adequate_add{ {config_id, (uint32_t)BagCapacity::kDefualtCapacity + 1 } };
    EXPECT_EQ(kRetBagAdequateAddItemSize, bag.AdequateSizeAddItem(adequate_add));
    adequate_add[config_id] = (uint32_t)BagCapacity::kDefualtCapacity;
    EXPECT_EQ(kRetOK, bag.AdequateSizeAddItem(adequate_add));
}

//可叠加混合,测试物品里面全满的情况，如999
TEST(BagTest, AdequateSizeAddItemmixtureFull)
{
    uint32_t cannot_stack_config_id = 1;//不可以叠加的物品id
    uint32_t stack_config_id = 10;//可以叠加的物品id
    Bag bag;
    //一个不可叠加，10个可以叠加
    UInt32UInt32UnorderedMap adequate_add{ {cannot_stack_config_id, 1 },
        {stack_config_id, get_item_conf(stack_config_id)->max_statck_size() * (uint32_t)BagCapacity::kDefualtCapacity} };
    EXPECT_EQ(kRetBagAdequateAddItemSize, bag.AdequateSizeAddItem(adequate_add));
    //改成一个不可叠加，九个可以叠加
    adequate_add[stack_config_id] = (uint32_t)(BagCapacity::kDefualtCapacity - 1) * get_item_conf(stack_config_id)->max_statck_size();
    EXPECT_EQ(kRetOK, bag.AdequateSizeAddItem(adequate_add));

    //添加一个格子以后不可以叠加了，添加一个可以叠加的物品
    CreateItemParam p;
    p.item_base_db.set_config_id(cannot_stack_config_id);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());
    auto item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetBagAdequateAddItemSize, bag.AdequateSizeAddItem(adequate_add));//因为占用了一个格子，所以总共不满十个格子
    //改成8个可以叠加的格子,一个不可叠加，总共需要九个格子
    adequate_add[stack_config_id] = (uint32_t)(BagCapacity::kDefualtCapacity - 2) * get_item_conf(stack_config_id)->max_statck_size();
    EXPECT_EQ(kRetOK, bag.AdequateSizeAddItem(adequate_add));
    //放一个可以叠加的格子
    p.item_base_db.set_config_id(stack_config_id);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());
    item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetBagAdequateAddItemSize, bag.AdequateSizeAddItem(adequate_add));//因为占用了两个格子，所以总共不满十个格子
    //改成7个可以叠加的格子,1个不可叠加，总共需要8个格子
    adequate_add[stack_config_id] = (uint32_t)(BagCapacity::kDefualtCapacity - 3) * get_item_conf(stack_config_id)->max_statck_size();
    EXPECT_EQ(kRetOK, bag.AdequateSizeAddItem(adequate_add));
    //放一个可以叠加的格子，个数少100
    p.item_base_db.set_config_id(stack_config_id);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size() - 100);
    item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetBagAdequateAddItemSize, bag.AdequateSizeAddItem(adequate_add));//因为占用了三个格子，所以总共不满十个格子
}

//物品足够测试
TEST(BagTest, AdequateItem)
{
    Bag bag;
    bag.Unlock(20);
    uint32_t config_id10 = 10;
    uint32_t config_id1 = 1;
    uint32_t config_id2 = 2;
    uint32_t config_id11 = 11;
    common::UInt32UInt32UnorderedMap adequate_item{ {config_id10 , 1} };
    EXPECT_EQ(kRetBagAdequatetem, bag.AdequateItem(adequate_item));//空背包测试
    CreateItemParam p;
    p.item_base_db.set_config_id(config_id10);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());
    auto item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetOK, bag.AdequateItem(adequate_item));
    adequate_item[config_id10] = get_item_conf(p.item_base_db.config_id())->max_statck_size() / 2;
    EXPECT_EQ(kRetOK, bag.AdequateItem(adequate_item));
    adequate_item.emplace(config_id1, 1);//不可叠加一个
    EXPECT_EQ(kRetBagAdequatetem, bag.AdequateItem(adequate_item));

    p.item_base_db.set_config_id(config_id1);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());
    item = CreateItem(p);//创建一个不可以叠加的
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetOK, bag.AdequateItem(adequate_item));
    adequate_item[config_id10] = get_item_conf(config_id10)->max_statck_size();//1个10可叠加999
    EXPECT_EQ(kRetOK, bag.AdequateItem(adequate_item));
    adequate_item[config_id10] = get_item_conf(config_id10)->max_statck_size() + 1;//1个10可叠加1000
    EXPECT_EQ(kRetBagAdequatetem, bag.AdequateItem(adequate_item));
    adequate_item[config_id10] = get_item_conf(config_id10)->max_statck_size() * 3;//3个10可叠加999*3
    EXPECT_EQ(kRetBagAdequatetem, bag.AdequateItem(adequate_item));

    p.item_base_db.set_config_id(config_id10);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());
    item = CreateItem(p);//创建一个可以叠加的
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetBagAdequatetem, bag.AdequateItem(adequate_item));//2个10的叠加999

    p.item_base_db.set_config_id(config_id11);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size() * 3);
    item = CreateItem(p);//创建一个可以叠加的
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetBagAdequatetem, bag.AdequateItem(adequate_item));//2个10的叠加999

    p.item_base_db.set_config_id(config_id10);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());
    item = CreateItem(p);//创建一个可以叠加的
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetOK, bag.AdequateItem(adequate_item));//3个10的叠加999

    adequate_item[config_id11] = get_item_conf(config_id11)->max_statck_size() * 3;//3个10可叠加999 3个11可叠加999
    EXPECT_EQ(kRetOK, bag.AdequateItem(adequate_item));
}

//物品足够测试
TEST(BagTest, DelItem)
{
    Bag bag;
    bag.Unlock(20);
    uint32_t test_config_id10 = 10;
    uint32_t config_id1 = 1;
    uint32_t config_id2 = 2;
    uint32_t config_id11 = 11;

    CreateItemParam p;
    p.item_base_db.set_config_id(test_config_id10);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size() * 2);// 999 * 2
    auto item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));

    p.item_base_db.set_config_id(config_id1);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());// 1
    item = CreateItem(p);//创建一个不可以叠加的
    EXPECT_EQ(kRetOK, bag.AddItem(item));

    p.item_base_db.set_config_id(config_id2);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());//  1
    item = CreateItem(p);//创建一个可以叠加的
    EXPECT_EQ(kRetOK, bag.AddItem(item));

    p.item_base_db.set_config_id(config_id11);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());// 999 * 1
    item = CreateItem(p);//创建一个可以叠加的
    EXPECT_EQ(kRetOK, bag.AddItem(item));

    UInt32UInt32UnorderedMap try_del;
    try_del.emplace(test_config_id10, 1);
    EXPECT_EQ(kRetOK, bag.DelItem(try_del));
    EXPECT_EQ(get_item_conf(test_config_id10)->max_statck_size() * 2 - 1, bag.GetItemStackSize(test_config_id10));
    EXPECT_EQ(get_item_conf(config_id1)->max_statck_size(), bag.GetItemStackSize(config_id1));
    EXPECT_EQ(get_item_conf(config_id2)->max_statck_size(), bag.GetItemStackSize(config_id2));
    EXPECT_EQ(get_item_conf(config_id11)->max_statck_size(), bag.GetItemStackSize(config_id11));
    try_del[test_config_id10] = get_item_conf(test_config_id10)->max_statck_size() * 2 - 1;
    try_del[config_id1] = get_item_conf(config_id1)->max_statck_size();
    try_del[config_id2] = get_item_conf(config_id2)->max_statck_size();
    try_del[config_id11] = get_item_conf(config_id11)->max_statck_size();
    EXPECT_EQ(kRetOK, bag.DelItem(try_del));
    EXPECT_EQ(0, bag.GetItemStackSize(test_config_id10));
    EXPECT_EQ(0, bag.GetItemStackSize(config_id1));
    EXPECT_EQ(0, bag.GetItemStackSize(config_id2));
    EXPECT_EQ(0, bag.GetItemStackSize(config_id11));

    EXPECT_EQ(5, bag.item_size());
    EXPECT_EQ(5, bag.pos_size());
    
    std::vector<uint32_t> ps{ 0, 1, 2, 3, 4 };
    for (auto& it : ps)
    {
        EXPECT_TRUE(bag.pos().find(it) != bag.pos().end());
    }
}

TEST(BagTest, Del)
{
    Bag bag;
    CreateItemParam p;
    uint32_t config_id1 = 1;
    p.item_base_db.set_config_id(config_id1);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());// 1
    auto item = CreateItem(p);//创建一个不可以叠加的
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(1, bag.item_size());
    EXPECT_EQ(1, bag.pos_size());
    EXPECT_EQ(kRetOK, bag.DelItem(g_server_sequence.LastId()));
    EXPECT_EQ(0, bag.item_size());
    EXPECT_EQ(0, bag.pos_size());
}

TEST(BagTest, DelItemByPos)
{
    Bag bag;
    CreateItemParam p;
    uint32_t config_id10 = 10;
    p.item_base_db.set_config_id(config_id10);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());// 999
    auto item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(1, bag.item_size());
    EXPECT_EQ(1, bag.pos_size());
    DelItemByPosParam dp;
    EXPECT_EQ(kRetBagDelItemPos, bag.DelItemByPos(dp));
    dp.pos_ = 0;
    EXPECT_EQ(kRetBagDelItemGuid, bag.DelItemByPos(dp));
    dp.item_guid_ = g_server_sequence.LastId();
    EXPECT_EQ(kRetBagDelItemConfig, bag.DelItemByPos(dp));
    dp.item_config_id_ = config_id10;
    EXPECT_EQ(kRetOK, bag.DelItemByPos(dp));
    EXPECT_EQ(get_item_conf(config_id10)->max_statck_size() - 1, bag.GetItemStackSize(config_id10));
    dp.size_ = get_item_conf(config_id10)->max_statck_size() - 1;
    EXPECT_EQ(kRetOK, bag.DelItemByPos(dp));
    EXPECT_EQ(0, bag.GetItemStackSize(config_id10));
    EXPECT_EQ(1, bag.item_size());
    EXPECT_EQ(1, bag.pos_size());
    EXPECT_EQ(0, bag.GetItemByBos(0)->size());
    EXPECT_EQ(0, bag.GetItemByGuid(g_server_sequence.LastId())->size());
}

//整理1，每个格子使用一点
TEST(BagTest, Neaten1)
{
    Bag bag;
    auto unlock_size = BagCapacity::kDefualtCapacity;
    bag.Unlock(unlock_size);
    CreateItemParam p;
    uint32_t config_id10 = 10;
    uint32_t config_id11 = 11;
    p.item_base_db.set_config_id(config_id10);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size() * BagCapacity::kDefualtCapacity);// 999 * 10
    auto item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    auto id10 = g_server_sequence.LastId();
    p.item_base_db.set_config_id(config_id11);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size() * BagCapacity::kDefualtCapacity);// 999 * 10
    item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    auto id11 = g_server_sequence.LastId();
    for (uint32_t i = 0; i < (uint32_t)BagCapacity::kDefualtCapacity; ++i)
    {
        DelItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = id10 - (BagCapacity::kDefualtCapacity - i) + 1;
        dp.item_config_id_ = config_id10;
        dp.size_ = get_item_conf(config_id10)->max_statck_size() - 1;
        EXPECT_EQ(kRetOK, bag.DelItemByPos(dp));
    }   
    for (uint32_t i = (uint32_t)BagCapacity::kDefualtCapacity; i < bag.pos_size(); ++i)
    {
        DelItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = id11 - (BagCapacity::kDefualtCapacity - (i - BagCapacity::kDefualtCapacity)) + 1;
        dp.item_config_id_ = config_id11;
        dp.size_ = get_item_conf(config_id11)->max_statck_size() - 1;
        EXPECT_EQ(kRetOK, bag.DelItemByPos(dp));
    }
    for (uint32_t i = 0; i < (uint32_t)bag.pos_size(); ++i)
    {
        EXPECT_EQ(1, bag.GetItemByBos(i)->size());
    }
    bag.Neaten();
    EXPECT_EQ(2, bag.item_size());
    EXPECT_EQ(2, bag.pos_size());
    for (auto& it : bag.pos())
    {
        EXPECT_EQ(BagCapacity::kDefualtCapacity, (std::size_t)bag.GetItemByBos(bag.GetItemPos(it.second))->size());
    }
    EXPECT_EQ(BagCapacity::kDefualtCapacity, bag.GetItemStackSize(config_id10));
    EXPECT_EQ(BagCapacity::kDefualtCapacity, bag.GetItemStackSize(config_id11));
}

//测试400格子，每个用998
TEST(BagTest, Neaten400)
{
    Bag bag;
    std::size_t unlock_size = 400;
    bag.Unlock(unlock_size);
    CreateItemParam p;
    uint32_t config_id10 = 10;
    uint32_t config_id11 = 11;
    p.item_base_db.set_config_id(config_id10);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size() * uint32_t(unlock_size / 2));// 999 * 200
    auto item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    auto id10 = g_server_sequence.LastId();
    p.item_base_db.set_config_id(config_id11);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size() * uint32_t(unlock_size / 2));// 999 * 200
    item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    auto id11 = g_server_sequence.LastId();
    auto config_id10_sz = unlock_size / 2;
    for (uint32_t i = 0; i < config_id10_sz; ++i)
    {
        DelItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = id10 - (config_id10_sz - i) + 1;
        dp.item_config_id_ = config_id10;
        dp.size_ = get_item_conf(config_id10)->max_statck_size() - 1;
        EXPECT_EQ(kRetOK, bag.DelItemByPos(dp));
    }
    for (uint32_t i = uint32_t(config_id10_sz); i < bag.pos_size(); ++i)
    {
        DelItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = id11 - (config_id10_sz - (i - config_id10_sz)) + 1;
        dp.item_config_id_ = config_id11;
        dp.size_ = get_item_conf(config_id11)->max_statck_size() - 1;
        EXPECT_EQ(kRetOK, bag.DelItemByPos(dp));
    }
    for (uint32_t i = 0; i < (uint32_t)bag.pos_size(); ++i)
    {
        EXPECT_EQ(1, bag.GetItemByBos(i)->size());
    }
    bag.Neaten();
    EXPECT_EQ(2, bag.item_size());
    EXPECT_EQ(2, bag.pos_size());
    std::size_t grid_sz = 200;
    for (auto& it : bag.pos())
    {
        EXPECT_EQ(grid_sz, (std::size_t)bag.GetItemByBos(bag.GetItemPos(it.second))->size());
    }
    EXPECT_EQ(grid_sz, bag.GetItemStackSize(config_id10));
    EXPECT_EQ(grid_sz, bag.GetItemStackSize(config_id11));
}

//测试400格子，每种物品前100个用998
TEST(BagTest, Neaten400_1)
{
    Bag bag;
    std::size_t unlock_size = 400;
    bag.Unlock(unlock_size);
    CreateItemParam p;
    uint32_t config_id10 = 10;
    uint32_t config_id11 = 11;
    std::size_t item_statck_max_sz = 999;
    auto per_grid_size = unlock_size / 2;
    p.item_base_db.set_config_id(config_id10);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size() * uint32_t(per_grid_size));// 999 * 200
    auto item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    auto id10 = g_server_sequence.LastId();
    p.item_base_db.set_config_id(config_id11);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size() * uint32_t(per_grid_size));// 999 * 200
    item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    auto id11 = g_server_sequence.LastId();
    auto config_id10_sz = per_grid_size;
    auto use_config_id10_sz = unlock_size / 4;
    for (uint32_t i = 0; i < config_id10_sz; ++i)
    {
        if (i >= use_config_id10_sz)
        {
            break;
        }
        DelItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = id10 - (config_id10_sz - i) + 1;
        dp.item_config_id_ = config_id10;
        dp.size_ = get_item_conf(config_id10)->max_statck_size() - 1;
        EXPECT_EQ(kRetOK, bag.DelItemByPos(dp));
    }
    auto use_config_id11_sz = unlock_size / 4 + config_id10_sz;
    for (uint32_t i = uint32_t(config_id10_sz); i < bag.pos_size(); ++i)
    {
        if (i >= use_config_id11_sz)
        {
            break;
        }
        DelItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = id11 - (config_id10_sz - (i - config_id10_sz)) + 1;
        dp.item_config_id_ = config_id11;
        dp.size_ = get_item_conf(config_id11)->max_statck_size() - 1;
        EXPECT_EQ(kRetOK, bag.DelItemByPos(dp));
    }
    auto index1 = use_config_id10_sz;//第一百个格子以前1
    auto index2 = use_config_id10_sz * 2;//第二百个格子以前999
    auto index3 = use_config_id10_sz * 3;//第s三百个格子以前1
    auto index4 = use_config_id10_sz * 4;//第s三百个格子以前999
    for (uint32_t i = 0; i < (uint32_t)bag.pos_size(); ++i)
    {
        if (i < index1)
        {
            EXPECT_EQ(1, bag.GetItemByBos(i)->size());
        }
        else if (i < index2)
        {
            EXPECT_EQ(item_statck_max_sz, bag.GetItemByBos(i)->size());
        }
        else if (i < index3)
        {
            EXPECT_EQ(1, bag.GetItemByBos(i)->size());
        }
        else if (i < index4)
        {
            EXPECT_EQ(item_statck_max_sz, bag.GetItemByBos(i)->size());
        }
        else
        {
            EXPECT_EQ(0, bag.GetItemByBos(i)->size());
        }
    }
    bag.Neaten();
    std::size_t grid_sz = 200;
    std::size_t remain_sz = 100;
    EXPECT_EQ(grid_sz + 2, bag.item_size());
    EXPECT_EQ(grid_sz + 2, bag.pos_size());   
    common::UInt32Set pos999;
    common::UInt32Set pos100;
    for (uint32_t i = 0; i < (uint32_t)bag.pos_size(); ++i)
    {
        if (item_statck_max_sz == bag.GetItemByBos(i)->size())
        {
            pos999.emplace(i);
        }
        else if (100 == bag.GetItemByBos(i)->size())
        {
            pos100.emplace(i);
        }
        else
        {
            EXPECT_FALSE(true);
        }
    }
    EXPECT_EQ(per_grid_size , pos999.size());
    EXPECT_EQ(2, pos100.size());
    EXPECT_EQ(per_grid_size / 2 * item_statck_max_sz + remain_sz, bag.GetItemStackSize(config_id10));
    EXPECT_EQ(per_grid_size / 2 * item_statck_max_sz + remain_sz, bag.GetItemStackSize(config_id11));
}

//整理1，每个格子使用一点
TEST(BagTest, NeatenCanNotStack)
{
    Bag bag;
    auto unlock_size = BagCapacity::kDefualtCapacity;
    bag.Unlock(unlock_size);
    CreateItemParam p;
    uint32_t config_id10 = 10;
    uint32_t config_id1 = 1;
    p.item_base_db.set_config_id(config_id10);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size() * BagCapacity::kDefualtCapacity);// 999 * 10
    auto item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    auto id10 = g_server_sequence.LastId();
    p.item_base_db.set_config_id(config_id1);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size() * BagCapacity::kDefualtCapacity);// 999 * 10
    item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    auto id11 = g_server_sequence.LastId();
    for (uint32_t i = 0; i < (uint32_t)BagCapacity::kDefualtCapacity; ++i)
    {
        DelItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = id10 - (BagCapacity::kDefualtCapacity - i) + 1;
        dp.item_config_id_ = config_id10;
        dp.size_ = get_item_conf(config_id10)->max_statck_size() - 1;
        EXPECT_EQ(kRetOK, bag.DelItemByPos(dp));
    }
 
    for (uint32_t i = 0; i < (uint32_t)bag.pos_size(); ++i)
    {
        EXPECT_EQ(1, bag.GetItemByBos(i)->size());
    }
    bag.Neaten();
    EXPECT_EQ(BagCapacity::kDefualtCapacity + 1, bag.item_size());
    EXPECT_EQ(BagCapacity::kDefualtCapacity + 1, bag.pos_size());
    for (auto& it : bag.pos())
    {
        if ((std::size_t)bag.GetItemByBos(bag.GetItemPos(it.second))->size() != BagCapacity::kDefualtCapacity)
        {
            EXPECT_EQ(1, (std::size_t)bag.GetItemByBos(bag.GetItemPos(it.second))->size());
        }
    }
    EXPECT_EQ(BagCapacity::kDefualtCapacity, bag.GetItemStackSize(config_id10));
    EXPECT_EQ(BagCapacity::kDefualtCapacity, bag.GetItemStackSize(config_id1));
}

int main(int argc, char** argv)
{
    item_config::GetSingleton().load();
    testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}

