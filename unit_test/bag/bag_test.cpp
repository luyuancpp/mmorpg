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
    EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->config_id(), p.item_base_db.config_id());
    EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->size(), p.item_base_db.size());
    EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByBos(0)->guid());
    EXPECT_EQ(0, bag.GetItemPos(g_server_sequence.Current()));
    EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByGuid(g_server_sequence.Current())->guid());
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
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->size(), p.item_base_db.size());
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
        EXPECT_EQ(bag.GetItemByBos(newindex)->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByBos(newindex)->size(), p.item_base_db.size());
        EXPECT_EQ(bag.GetItemByBos(newindex)->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByBos(newindex)->size(), p.item_base_db.size());
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->size(), p.item_base_db.size());
        EXPECT_EQ(newindex, bag.GetItemPos(g_server_sequence.Current()));
        EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByBos(newindex)->guid());
        EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByGuid(g_server_sequence.Current())->guid());
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
            EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->size(), p.item_base_db.size());
        }
        else
        {
            EXPECT_EQ(bag.GetItemByBos(index)->size(), get_item_conf(p.item_base_db.config_id())->max_statck_size());
            EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->size(), get_item_conf(p.item_base_db.config_id())->max_statck_size());
        }
        
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(index, bag.GetItemPos(g_server_sequence.Current()));
        EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByBos(index)->guid());
        EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByGuid(g_server_sequence.Current())->guid());
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
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->size(), p.item_base_db.size());
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
        EXPECT_EQ(bag.GetItemByBos(newindex)->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByBos(newindex)->size(), p.item_base_db.size());
        EXPECT_EQ(bag.GetItemByBos(newindex)->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByBos(newindex)->size(), p.item_base_db.size());
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->config_id(), p.item_base_db.config_id());
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->size(), p.item_base_db.size());
        EXPECT_EQ(newindex, bag.GetItemPos(g_server_sequence.Current()));
        EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByBos(newindex)->guid());
        EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByGuid(g_server_sequence.Current())->guid());
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
    EXPECT_EQ(kRetOK, bag.DelItem(g_server_sequence.Current()));
    EXPECT_EQ(0, bag.item_size());
    EXPECT_EQ(0, bag.pos_size());
}

TEST(BagTest, Neaten)
{
    Bag bag;
    bag.Unlock(20);

}

int main(int argc, char** argv)
{
    item_config::GetSingleton().load();
    testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}

