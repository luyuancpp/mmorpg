#include <gtest/gtest.h>

#include "base/core/type_define/type_define.h"

#include "table/item_table.h"
#include "bag/bag_system.h"
#include "proto/table/tip/common_error_tip.pb.h"
#include "proto/table/tip/bag_error_tip.pb.h"

decltype(auto) GetItemTable(int32_t itemTableId) {
    return ItemTableManager::Instance().GetTable(itemTableId);
}

TEST(BagTest, NullItem)
{
    Bag bag;
    EXPECT_EQ(nullptr, bag.GetItemBaseByGuid(0));
}

TEST(BagTest, AddNewGridItem)
{
    Bag bag;
    InitItemParam p;
    p.itemPBComp.set_config_id(1);
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    EXPECT_EQ(1, bag.ItemGridSize());
    EXPECT_EQ(1, bag.PosSize());
    EXPECT_EQ(bag.GetItemBaseByPos(0)->config_id(), p.itemPBComp.config_id());
    EXPECT_EQ(bag.GetItemBaseByPos(0)->size(), p.itemPBComp.size());
    EXPECT_EQ(bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->config_id(), p.itemPBComp.config_id());
    EXPECT_EQ(bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->size(), p.itemPBComp.size());
    EXPECT_EQ(Bag::LastGeneratorItemGuid(), bag.GetItemBaseByPos(0)->item_id());
    EXPECT_EQ(0, bag.GetItemPos(Bag::LastGeneratorItemGuid()));
    EXPECT_EQ(Bag::LastGeneratorItemGuid(), bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->item_id());
}

TEST(BagTest, AddNewGridItemFull)
{
    Bag bag;
    InitItemParam p;
    p.itemPBComp.set_config_id(1);
    for (uint32_t i = 0; i < (uint32_t)kDefaultCapacity; i++)
    {
        EXPECT_EQ(kSuccess, bag.AddItem(p));
        EXPECT_EQ(i + 1, bag.ItemGridSize());
        EXPECT_EQ(i + 1, bag.PosSize());
        EXPECT_EQ(bag.GetItemBaseByPos(i)->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.GetItemBaseByPos(i)->size(), p.itemPBComp.size());
        EXPECT_EQ(bag.GetItemBaseByPos(i)->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.GetItemBaseByPos(i)->size(), p.itemPBComp.size());
        EXPECT_EQ(bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->size(), p.itemPBComp.size());
        EXPECT_EQ(i, bag.GetItemPos(Bag::LastGeneratorItemGuid()));
        EXPECT_EQ(Bag::LastGeneratorItemGuid(), bag.GetItemBaseByPos(i)->item_id());
        EXPECT_EQ(Bag::LastGeneratorItemGuid(), bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->item_id());
    }   
    EXPECT_EQ(kDefaultCapacity, bag.ItemGridSize());
    EXPECT_EQ(kDefaultCapacity, bag.PosSize());

    p.itemPBComp.set_config_id(1);
    EXPECT_EQ(kBagAddItemBagFull, bag.AddItem(p));
    bag.Unlock(kDefaultCapacity);
    for (uint32_t i = 0; i < (uint32_t)kDefaultCapacity; i++)
    {
        EXPECT_EQ(kSuccess, bag.AddItem(p));
        uint32_t newindex = i + (uint32_t)kDefaultCapacity;
        EXPECT_EQ(newindex + 1 , bag.ItemGridSize());
        EXPECT_EQ(newindex + 1, bag.PosSize());
        EXPECT_EQ(bag.GetItemBaseByPos(newindex)->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.GetItemBaseByPos(newindex)->size(), p.itemPBComp.size());
        EXPECT_EQ(bag.GetItemBaseByPos(newindex)->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.GetItemBaseByPos(newindex)->size(), p.itemPBComp.size());
        EXPECT_EQ(bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->size(), p.itemPBComp.size());
        EXPECT_EQ(newindex, bag.GetItemPos(Bag::LastGeneratorItemGuid()));
        EXPECT_EQ(Bag::LastGeneratorItemGuid(), bag.GetItemBaseByPos(newindex)->item_id());
        EXPECT_EQ(Bag::LastGeneratorItemGuid(), bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->item_id());
    }
    EXPECT_EQ(kDefaultCapacity * 2, bag.ItemGridSize());
    EXPECT_EQ(kDefaultCapacity * 2, bag.PosSize());
}

TEST(BagTest, Add10CanStack10CanNotStack)
{
    Bag bag;
    bag.Unlock(kDefaultCapacity);
    InitItemParam p;
    uint32_t config_id10 = 10;
    uint32_t config_id1 = 1;
    p.itemPBComp.set_config_id(config_id1);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * kDefaultCapacity);
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    p.itemPBComp.set_config_id(config_id10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * kDefaultCapacity);
  
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    EXPECT_EQ(kDefaultCapacity * 2, bag.ItemGridSize());
    EXPECT_EQ(kDefaultCapacity * 2, bag.PosSize());
}

TEST(BagTest, AddStackItem12121212)
{
    Bag bag;
    InitItemParam p;
    p.itemPBComp.set_config_id(9);
    auto sz = 0;

    auto max_statck_size = GetItemTable(p.itemPBComp.config_id()).first->max_statck_size();
    auto half_max_statck_size = max_statck_size / 2;


    for (uint32_t i = 0; i < (uint32_t)kDefaultCapacity * 2 ; i++)
    {

        if (i % 2 == 0)
        {
            p.itemPBComp.set_size(half_max_statck_size);
        }
        else
        {
            p.itemPBComp.set_size(max_statck_size);
        }

        sz += p.itemPBComp.size();

        auto ret = bag.AddItem(p);
        if (ret != kSuccess)
        {
            break;
        }

        EXPECT_EQ(kSuccess, ret);
        auto gridSize = Bag::CalculateStackGridSize(sz, max_statck_size);
        uint32_t index = uint32_t(gridSize - 1);

        EXPECT_EQ(gridSize, bag.ItemGridSize());
        EXPECT_EQ(gridSize, bag.PosSize());
        if (sz % max_statck_size == 0)
        {
            EXPECT_EQ(bag.GetItemBaseByPos(index / 2)->size(), max_statck_size);
    
        }
        else
        {
            EXPECT_EQ(std::size_t(bag.GetItemBaseByPos(index)->size()), std::size_t(half_max_statck_size));
            EXPECT_EQ(bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->size(), half_max_statck_size);
        }
        
        EXPECT_EQ(bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(index, bag.GetItemPos(Bag::LastGeneratorItemGuid()));
        EXPECT_EQ(Bag::LastGeneratorItemGuid(), bag.GetItemBaseByPos(index)->item_id());
        EXPECT_EQ(Bag::LastGeneratorItemGuid(), bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->item_id());

        //bag.ToString();
    }

    EXPECT_EQ(kDefaultCapacity, bag.ItemGridSize());
    EXPECT_EQ(kDefaultCapacity, bag.PosSize());
}

TEST(BagTest, AddStackItemUnlock)
{
    Bag bag;
    InitItemParam p;
    p.itemPBComp.set_config_id(10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());
    for (uint32_t i = 0; i < (uint32_t)kDefaultCapacity; i++)
    {
        EXPECT_EQ(kSuccess, bag.AddItem(p));
        EXPECT_EQ(i + 1, bag.ItemGridSize());
        EXPECT_EQ(i + 1, bag.PosSize());
        EXPECT_EQ(bag.GetItemBaseByPos(i)->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.GetItemBaseByPos(i)->size(), p.itemPBComp.size());
        EXPECT_EQ(bag.GetItemBaseByPos(i)->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.GetItemBaseByPos(i)->size(), p.itemPBComp.size());
        EXPECT_EQ(bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->size(), p.itemPBComp.size());
        EXPECT_EQ(i, bag.GetItemPos(Bag::LastGeneratorItemGuid()));
        EXPECT_EQ(Bag::LastGeneratorItemGuid(), bag.GetItemBaseByPos(i)->item_id());
        EXPECT_EQ(Bag::LastGeneratorItemGuid(), bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->item_id());
    }
    EXPECT_EQ(kDefaultCapacity, bag.ItemGridSize());
    EXPECT_EQ(kDefaultCapacity, bag.PosSize());

    EXPECT_EQ(kBagAddItemBagFull, bag.AddItem(p));
    bag.Unlock(kDefaultCapacity);
    for (uint32_t i = 0; i < (uint32_t)kDefaultCapacity; i++)
    {
        EXPECT_EQ(kSuccess, bag.AddItem(p));
        uint32_t newindex = i + (uint32_t)kDefaultCapacity;
        EXPECT_EQ(newindex + 1, bag.ItemGridSize());
        EXPECT_EQ(newindex + 1, bag.PosSize());
        EXPECT_EQ(bag.GetItemBaseByPos(newindex)->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.GetItemBaseByPos(newindex)->size(), p.itemPBComp.size());
        EXPECT_EQ(bag.GetItemBaseByPos(newindex)->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.GetItemBaseByPos(newindex)->size(), p.itemPBComp.size());
        EXPECT_EQ(bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->size(), p.itemPBComp.size());
        EXPECT_EQ(newindex, bag.GetItemPos(Bag::LastGeneratorItemGuid()));
        EXPECT_EQ(Bag::LastGeneratorItemGuid(), bag.GetItemBaseByPos(newindex)->item_id());
        EXPECT_EQ(Bag::LastGeneratorItemGuid(), bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->item_id());
    }
    EXPECT_EQ(kDefaultCapacity * 2, bag.ItemGridSize());
    EXPECT_EQ(kDefaultCapacity * 2, bag.PosSize());
}

TEST(BagTest, AdequateSizeAddItemCannotStackItemFull)
{
    uint32_t config_id = 1;
    Bag bag;
    U32U32UnorderedMap adequate_add{ {config_id, (uint32_t)kDefaultCapacity + 1 } };
    EXPECT_EQ(kBagItemNotStacked, bag.HasEnoughSpace(adequate_add));
    adequate_add[config_id] = (uint32_t)kDefaultCapacity;
    EXPECT_EQ(kSuccess, bag.HasEnoughSpace(adequate_add));
}

//可叠加混合,测试物品里面全满的情况，如999
TEST(BagTest, AdequateSizeAddItemmixtureFull)
{
    uint32_t cannot_stack_config_id = 1;//不可以叠加的物品id
    uint32_t stack_config_id = 10;//可以叠加的物品id
    //一个不可叠加，10个可以叠加
    Bag bag;
    U32U32UnorderedMap adequate_add{ {cannot_stack_config_id, 1 },
        {stack_config_id, GetItemTable(stack_config_id).first->max_statck_size() * (uint32_t)kDefaultCapacity} };


    EXPECT_EQ(kBagItemNotStacked, bag.HasEnoughSpace(adequate_add));
    //改成一个不可叠加，九个可以叠加
    adequate_add[stack_config_id] = (uint32_t)(kDefaultCapacity - 1) * GetItemTable(stack_config_id).first->max_statck_size();
    EXPECT_EQ(kSuccess, bag.HasEnoughSpace(adequate_add));
    //添加一个格子以后不可以叠加了，添加一个可以叠加的物品
    InitItemParam p;
    p.itemPBComp.set_config_id(cannot_stack_config_id);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());

    
    EXPECT_EQ(kSuccess, bag.AddItem(p));//剩九个格子
    EXPECT_EQ(kBagItemNotStacked, bag.HasEnoughSpace(adequate_add));//因为占用了一个格子，所以总共不满十个格子
    //改成8个可以叠加的格子,一个不可叠加，总共需要九个格子
    adequate_add[stack_config_id] = (uint32_t)(kDefaultCapacity - 2) * GetItemTable(stack_config_id).first->max_statck_size();
    EXPECT_EQ(kSuccess, bag.HasEnoughSpace(adequate_add));
    p.itemPBComp.set_config_id(stack_config_id);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    //改成7个可以叠加的格子,1个不可叠加，总共需要8个格子
    EXPECT_EQ(kBagItemNotStacked, bag.HasEnoughSpace(adequate_add));
    adequate_add[stack_config_id] = (uint32_t)(kDefaultCapacity - 3) * GetItemTable(stack_config_id).first->max_statck_size();
    EXPECT_EQ(kSuccess, bag.HasEnoughSpace(adequate_add));

    p.itemPBComp.set_config_id(stack_config_id);
    //放一个可以叠加的格子，个数少100
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() - 100);
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    EXPECT_EQ(kBagItemNotStacked, bag.HasEnoughSpace(adequate_add));
}

//��Ʒ�㹻����
TEST(BagTest, AdequateItem)
{
    Bag bag;
    bag.Unlock(20);
    uint32_t config_id10 = 10;
    uint32_t config_id1 = 1;
    uint32_t config_id2 = 2;
    uint32_t config_id11 = 11;
    U32U32UnorderedMap adequate_item{ {config_id10 , 1} };
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(adequate_item));//空背包测试
    InitItemParam p;
    p.itemPBComp.set_config_id(config_id10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    EXPECT_EQ(kSuccess, bag.HasSufficientItems(adequate_item));
    adequate_item[config_id10] = GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() / 2;
    EXPECT_EQ(kSuccess, bag.HasSufficientItems(adequate_item));
    adequate_item.emplace(config_id1, 1);//不可叠加一个
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(adequate_item));

    //创建一个不可以叠加的
    p.itemPBComp.set_config_id(config_id1);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());
   
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    EXPECT_EQ(kSuccess, bag.HasSufficientItems(adequate_item));
    adequate_item[config_id10] = GetItemTable(config_id10).first->max_statck_size();//1个10可叠加999
    EXPECT_EQ(kSuccess, bag.HasSufficientItems(adequate_item));
    adequate_item[config_id10] = GetItemTable(config_id10).first->max_statck_size() + 1;//1个10可叠加1000
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(adequate_item));
    adequate_item[config_id10] = GetItemTable(config_id10).first->max_statck_size() * 3;//3个10可叠加999*3
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(adequate_item));

    //创建一个可以叠加的
    p.itemPBComp.set_config_id(config_id10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());

    EXPECT_EQ(kSuccess, bag.AddItem(p));
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(adequate_item));//2个10的叠加999

    p.itemPBComp.set_config_id(config_id11);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * 3);
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(adequate_item));//2个10的叠加999

    //创建一个可以叠加的
    p.itemPBComp.set_config_id(config_id10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    EXPECT_EQ(kSuccess, bag.HasSufficientItems(adequate_item));//3个10的叠加99

    adequate_item[config_id11] = GetItemTable(config_id11).first->max_statck_size() * 3;//3个10可叠加999 3个11可叠加999
    EXPECT_EQ(kSuccess, bag.HasSufficientItems(adequate_item));
}

//��Ʒ�㹻����
TEST(BagTest, DelItem)
{
    Bag bag;
    bag.Unlock(20);
    uint32_t test_config_id10 = 10;
    uint32_t config_id1 = 1;
    uint32_t config_id2 = 2;
    uint32_t config_id11 = 11;

    InitItemParam item;
    item.itemPBComp.set_config_id(test_config_id10);
    item.itemPBComp.set_size(GetItemTable(item.itemPBComp.config_id()).first->max_statck_size() * 2);// 999 * 2
   
    EXPECT_EQ(kSuccess, bag.AddItem(item));

    item.itemPBComp.set_config_id(config_id1);
    item.itemPBComp.set_size(GetItemTable(item.itemPBComp.config_id()).first->max_statck_size());// 1
    EXPECT_EQ(kSuccess, bag.AddItem(item));

    item.itemPBComp.set_config_id(config_id2);
    item.itemPBComp.set_size(GetItemTable(item.itemPBComp.config_id()).first->max_statck_size());//  1
    EXPECT_EQ(kSuccess, bag.AddItem(item));

    item.itemPBComp.set_config_id(config_id11);
    item.itemPBComp.set_size(GetItemTable(item.itemPBComp.config_id()).first->max_statck_size());// 999 * 1
    EXPECT_EQ(kSuccess, bag.AddItem(item));

    U32U32UnorderedMap try_del;
    try_del.emplace(test_config_id10, 1);
    EXPECT_EQ(kSuccess, bag.RemoveItems(try_del));
    EXPECT_EQ(GetItemTable(test_config_id10).first->max_statck_size() * 2 - 1, bag.GetItemStackSize(test_config_id10));
    EXPECT_EQ(GetItemTable(config_id1).first->max_statck_size(), bag.GetItemStackSize(config_id1));
    EXPECT_EQ(GetItemTable(config_id2).first->max_statck_size(), bag.GetItemStackSize(config_id2));
    EXPECT_EQ(GetItemTable(config_id11).first->max_statck_size(), bag.GetItemStackSize(config_id11));
    try_del[test_config_id10] = GetItemTable(test_config_id10).first->max_statck_size() * 2 - 1;
    try_del[config_id1] = GetItemTable(config_id1).first->max_statck_size();
    try_del[config_id2] = GetItemTable(config_id2).first->max_statck_size();
    try_del[config_id11] = GetItemTable(config_id11).first->max_statck_size();
    EXPECT_EQ(kSuccess, bag.RemoveItems(try_del));
    EXPECT_EQ(0, bag.GetItemStackSize(test_config_id10));
    EXPECT_EQ(0, bag.GetItemStackSize(config_id1));
    EXPECT_EQ(0, bag.GetItemStackSize(config_id2));
    EXPECT_EQ(0, bag.GetItemStackSize(config_id11));

    EXPECT_EQ(5, bag.ItemGridSize());
    EXPECT_EQ(5, bag.PosSize());
    
    std::vector<uint32_t> ps{ 0, 1, 2, 3, 4 };
    for (auto& it : ps)
    {
        EXPECT_TRUE(bag.pos().find(it) != bag.pos().end());
    }
}

TEST(BagTest, Del)
{
    Bag bag;
    InitItemParam p;
    uint32_t config_id1 = 1;
    p.itemPBComp.set_config_id(config_id1);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());// 1
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    EXPECT_EQ(1, bag.ItemGridSize());
    EXPECT_EQ(1, bag.PosSize());
    EXPECT_EQ(kSuccess, bag.RemoveItem(Bag::LastGeneratorItemGuid()));
    EXPECT_EQ(0, bag.ItemGridSize());
    EXPECT_EQ(0, bag.PosSize());
}

TEST(BagTest, RemoveItemByPos)
{
    Bag bag;
    InitItemParam p;
    uint32_t config_id10 = 10;
    p.itemPBComp.set_config_id(config_id10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());// 999
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    EXPECT_EQ(1, bag.ItemGridSize());
    EXPECT_EQ(1, bag.PosSize());
    RemoveItemByPosParam dp;
    EXPECT_EQ(kBagDelItemPos, bag.RemoveItemByPos(dp));
    dp.pos_ = 0;
    EXPECT_EQ(kBagDelItemGuid, bag.RemoveItemByPos(dp));
    dp.item_guid_ = Bag::LastGeneratorItemGuid();
    EXPECT_EQ(kBagDelItemConfig, bag.RemoveItemByPos(dp));
    dp.item_config_id_ = config_id10;
    EXPECT_EQ(kSuccess, bag.RemoveItemByPos(dp));
    EXPECT_EQ(GetItemTable(config_id10).first->max_statck_size() - 1, bag.GetItemStackSize(config_id10));
    dp.size_ = GetItemTable(config_id10).first->max_statck_size() - 1;
    EXPECT_EQ(kSuccess, bag.RemoveItemByPos(dp));
    EXPECT_EQ(0, bag.GetItemStackSize(config_id10));
    EXPECT_EQ(1, bag.ItemGridSize());
    EXPECT_EQ(1, bag.PosSize());
    EXPECT_EQ(0, bag.GetItemBaseByPos(0)->size());
    EXPECT_EQ(0, bag.GetItemBaseByGuid(Bag::LastGeneratorItemGuid())->size());
}

//����1��ÿ������ʹ��һ��
TEST(BagTest, Neaten1)
{
    Bag bag;
    auto unlock_size = kDefaultCapacity;
    bag.Unlock(unlock_size);
    InitItemParam p;
    uint32_t config_id10 = 10;
    uint32_t config_id11 = 11;
    p.itemPBComp.set_config_id(config_id10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * kDefaultCapacity);// 999 * 10
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    auto id10 = Bag::LastGeneratorItemGuid();
    p.itemPBComp.set_config_id(config_id11);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * kDefaultCapacity);// 999 * 10
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    auto id11 = Bag::LastGeneratorItemGuid();
    for (uint32_t i = 0; i < (uint32_t)kDefaultCapacity; ++i)
    {
        RemoveItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = bag.GetItemBaseByPos(i)->item_id();
        dp.item_config_id_ = config_id10;
        dp.size_ = GetItemTable(config_id10).first->max_statck_size() - 1;
        EXPECT_EQ(kSuccess, bag.RemoveItemByPos(dp));
    }   
    for (uint32_t i = (uint32_t)kDefaultCapacity; i < bag.PosSize(); ++i)
    {
        RemoveItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = bag.GetItemBaseByPos(i)->item_id();
        dp.item_config_id_ = config_id11;
        dp.size_ = GetItemTable(config_id11).first->max_statck_size() - 1;
        EXPECT_EQ(kSuccess, bag.RemoveItemByPos(dp));
    }
    for (uint32_t i = 0; i < (uint32_t)bag.PosSize(); ++i)
    {
        EXPECT_EQ(1, bag.GetItemBaseByPos(i)->size());
    }
    bag.Neaten();
    EXPECT_EQ(2, bag.ItemGridSize());
    EXPECT_EQ(2, bag.PosSize());

    for (auto& it : bag.pos())
    {
        EXPECT_EQ(1, (std::size_t)bag.GetItemBaseByPos(bag.GetItemPos(it.second))->size());
    }
    EXPECT_EQ(kDefaultCapacity, bag.GetItemStackSize(config_id10));
    EXPECT_EQ(kDefaultCapacity, bag.GetItemStackSize(config_id11));
}

//����400���ӣ�ÿ����998
TEST(BagTest, Neaten400)
{
    Bag bag;
    std::size_t unlock_size = 400;
    bag.Unlock(unlock_size);
    InitItemParam p;
    uint32_t config_id10 = 10;
    uint32_t config_id11 = 11;
    p.itemPBComp.set_config_id(config_id10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * uint32_t(unlock_size / 2));// 999 * 200
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    auto id10 = Bag::LastGeneratorItemGuid();
    p.itemPBComp.set_config_id(config_id11);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * uint32_t(unlock_size / 2));// 999 * 200
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    auto id11 = Bag::LastGeneratorItemGuid();
    auto config_id10_sz = unlock_size / 2;
    for (uint32_t i = 0; i < config_id10_sz; ++i)
    {
        RemoveItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = bag.GetItemBaseByPos(i)->item_id();
        dp.item_config_id_ = config_id10;
        dp.size_ = GetItemTable(config_id10).first->max_statck_size() - 1;
        EXPECT_EQ(kSuccess, bag.RemoveItemByPos(dp));
    }
    for (uint32_t i = uint32_t(config_id10_sz); i < bag.PosSize(); ++i)
    {
        RemoveItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = bag.GetItemBaseByPos(i)->item_id();
        dp.item_config_id_ = config_id11;
        dp.size_ = GetItemTable(config_id11).first->max_statck_size() - 1;
        EXPECT_EQ(kSuccess, bag.RemoveItemByPos(dp));
    }
    for (uint32_t i = 0; i < (uint32_t)bag.PosSize(); ++i)
    {
        EXPECT_EQ(1, bag.GetItemBaseByPos(i)->size());
    }
    bag.Neaten();
    EXPECT_EQ(2, bag.ItemGridSize());
    EXPECT_EQ(2, bag.PosSize());
    std::size_t grid_sz = 200;
    for (auto& it : bag.pos())
    {
        EXPECT_EQ(1, (std::size_t)bag.GetItemBaseByPos(bag.GetItemPos(it.second))->size());
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
    InitItemParam p;
    uint32_t config_id10 = 10;
    uint32_t config_id11 = 11;
    std::size_t item_statck_max_sz = 999;
    auto per_grid_size = unlock_size / 2;
    p.itemPBComp.set_config_id(config_id10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * uint32_t(per_grid_size));// 999 * 200
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    p.itemPBComp.set_config_id(config_id11);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * uint32_t(per_grid_size));// 999 * 200
    EXPECT_EQ(kSuccess, bag.AddItem(p));

    auto config_id10_sz = per_grid_size;
    auto use_config_id10_sz = unlock_size / 4;

    for (uint32_t i = 0; i < config_id10_sz; ++i)
    {
        if (i >= use_config_id10_sz)
        {
            break;
        }
        RemoveItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = bag.GetItemBaseByPos(i)->item_id();
        dp.item_config_id_ = config_id10;
        dp.size_ = GetItemTable(config_id10).first->max_statck_size() - 1;
        EXPECT_EQ(kSuccess, bag.RemoveItemByPos(dp));
    }

    auto use_config_id11_sz = unlock_size / 4 + config_id10_sz;
    for (uint32_t i = uint32_t(config_id10_sz); i < bag.PosSize(); ++i)
    {
        if (i >= use_config_id11_sz)
        {
            break;
        }
        RemoveItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = bag.GetItemBaseByPos(i)->item_id();
        dp.item_config_id_ = config_id11;
        dp.size_ = GetItemTable(config_id11).first->max_statck_size() - 1;
        EXPECT_EQ(kSuccess, bag.RemoveItemByPos(dp));
    }
    auto index1 = use_config_id10_sz;//第1百个格子以前1
    auto index2 = use_config_id10_sz * 2;//第2百个格子以前999
    auto index3 = use_config_id10_sz * 3;//第3百个格子以前1
    auto index4 = use_config_id10_sz * 4;//第4百个格子以前999
    for (uint32_t i = 0; i < (uint32_t)bag.PosSize(); ++i)
    {
        if (i < index1)
        {
            EXPECT_EQ(1, bag.GetItemBaseByPos(i)->size());
        }
        else if (i < index2)
        {
            EXPECT_EQ(item_statck_max_sz, bag.GetItemBaseByPos(i)->size());
        }
        else if (i < index3)
        {
            EXPECT_EQ(1, bag.GetItemBaseByPos(i)->size());
        }
        else if (i < index4)
        {
            EXPECT_EQ(item_statck_max_sz, bag.GetItemBaseByPos(i)->size());
        }
        else
        {
            EXPECT_EQ(0, bag.GetItemBaseByPos(i)->size());
        }
    }
    bag.Neaten();
    std::size_t grid_sz = 200;
    std::size_t remain_sz = 100;
    EXPECT_EQ(grid_sz + 2, bag.ItemGridSize());
    EXPECT_EQ(grid_sz + 2, bag.PosSize());   
    UInt32Set pos999;
    UInt32Set pos1;
    for (uint32_t i = 0; i < (uint32_t)bag.PosSize(); ++i)
    {
        if (item_statck_max_sz == bag.GetItemBaseByPos(i)->size())
        {
            pos999.emplace(i);
        }
        else if (1 == bag.GetItemBaseByPos(i)->size())
        {
            pos1.emplace(i);
        }
       
    }
    EXPECT_EQ(per_grid_size , pos999.size());
    EXPECT_EQ(2, pos1.size());
    EXPECT_EQ(per_grid_size / 2 * item_statck_max_sz + remain_sz, bag.GetItemStackSize(config_id10));
    EXPECT_EQ(per_grid_size / 2 * item_statck_max_sz + remain_sz, bag.GetItemStackSize(config_id11));
}

//����1��ÿ������ʹ��һ��
TEST(BagTest, NeatenCanNotStack)
{
    Bag bag;
    auto unlock_size = kDefaultCapacity;
    bag.Unlock(unlock_size);
    InitItemParam p;
    uint32_t config_id10 = 10;
    uint32_t config_id1 = 1;
    p.itemPBComp.set_config_id(config_id10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * kDefaultCapacity);// 999 * 10
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    auto id10 = Bag::LastGeneratorItemGuid();
    p.itemPBComp.set_config_id(config_id1);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * kDefaultCapacity);// 999 * 10
    EXPECT_EQ(kSuccess, bag.AddItem(p));
    auto id11 = Bag::LastGeneratorItemGuid();
    for (uint32_t i = 0; i < (uint32_t)kDefaultCapacity; ++i)
    {
        RemoveItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = bag.GetItemBaseByPos(i)->item_id();
        dp.item_config_id_ = config_id10;
        dp.size_ = GetItemTable(config_id10).first->max_statck_size() - 1;
        EXPECT_EQ(kSuccess, bag.RemoveItemByPos(dp));
    }
 
    for (uint32_t i = 0; i < (uint32_t)bag.PosSize(); ++i)
    {
        EXPECT_EQ(1, bag.GetItemBaseByPos(i)->size());
    }
    bag.Neaten();
    EXPECT_EQ(kDefaultCapacity + 1, bag.ItemGridSize());
    EXPECT_EQ(kDefaultCapacity + 1, bag.PosSize());
    for (auto& it : bag.pos())
    {
        if ((std::size_t)bag.GetItemBaseByPos(bag.GetItemPos(it.second))->size() != kDefaultCapacity)
        {
            EXPECT_EQ(1, (std::size_t)bag.GetItemBaseByPos(bag.GetItemPos(it.second))->size());
        }
    }
    EXPECT_EQ(kDefaultCapacity, bag.GetItemStackSize(config_id10));
    EXPECT_EQ(kDefaultCapacity, bag.GetItemStackSize(config_id1));
}

int main(int argc, char** argv)
{
    ItemTableManager::Instance().Load();
    testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}

