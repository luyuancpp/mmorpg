#include <gtest/gtest.h>

#include "type_define/type_define.h"

#include "item_config.h"

#include "bag/bag_system.h"

#include "pbc/common_error_tip.pb.h"
#include "pbc/bag_error_tip.pb.h"


TEST(BagTest, NullItem)
{
    Bag bag;
    EXPECT_EQ(nullptr, bag.FindItemByGuid(0));
}

TEST(BagTest, AddNewGridItem)
{
    Bag bag;
    InitItemParam p;
    p.itemPBComp.set_config_id(1);
    auto item = InitItem(p);
    EXPECT_EQ(kOK, bag.AddItem(item));
    EXPECT_EQ(1, bag.item_size());
    EXPECT_EQ(1, bag.pos_size());
    EXPECT_EQ(bag.FindtemByBos(0)->config_id(), p.itemPBComp.config_id());
    EXPECT_EQ(bag.FindtemByBos(0)->size(), p.itemPBComp.size());
    EXPECT_EQ(bag.FindItemByGuid(g_bag_node_sequence.LastId())->config_id(), p.itemPBComp.config_id());
    EXPECT_EQ(bag.FindItemByGuid(g_bag_node_sequence.LastId())->size(), p.itemPBComp.size());
    EXPECT_EQ(g_bag_node_sequence.LastId(), bag.FindtemByBos(0)->Guid());
    EXPECT_EQ(0, bag.GetItemPos(g_bag_node_sequence.LastId()));
    EXPECT_EQ(g_bag_node_sequence.LastId(), bag.FindItemByGuid(g_bag_node_sequence.LastId())->Guid());
}

TEST(BagTest, AddNewGridItemFull)
{
    Bag bag;
    InitItemParam p;
    p.itemPBComp.set_config_id(1);
    for (uint32_t i = 0; i < (uint32_t)BagCapacity::kDefaultCapacity; i++)
    {
        auto item = InitItem(p);
        EXPECT_EQ(kOK, bag.AddItem(item));
        EXPECT_EQ(i + 1, bag.item_size());
        EXPECT_EQ(i + 1, bag.pos_size());
        EXPECT_EQ(bag.FindtemByBos(i)->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.FindtemByBos(i)->size(), p.itemPBComp.size());
        EXPECT_EQ(bag.FindtemByBos(i)->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.FindtemByBos(i)->size(), p.itemPBComp.size());
        EXPECT_EQ(bag.FindItemByGuid(g_bag_node_sequence.LastId())->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.FindItemByGuid(g_bag_node_sequence.LastId())->size(), p.itemPBComp.size());
        EXPECT_EQ(i, bag.GetItemPos(g_bag_node_sequence.LastId()));
        EXPECT_EQ(g_bag_node_sequence.LastId(), bag.FindtemByBos(i)->Guid());
        EXPECT_EQ(g_bag_node_sequence.LastId(), bag.FindItemByGuid(g_bag_node_sequence.LastId())->Guid());
    }   
    EXPECT_EQ(BagCapacity::kDefaultCapacity, bag.item_size());
    EXPECT_EQ(BagCapacity::kDefaultCapacity, bag.pos_size());

    auto item = InitItem(p);
    EXPECT_EQ(kBagAddItemBagFull, bag.AddItem(item));
    bag.Unlock(BagCapacity::kDefaultCapacity);
    for (uint32_t i = 0; i < (uint32_t)BagCapacity::kDefaultCapacity; i++)
    {
        auto item = InitItem(p);
        EXPECT_EQ(kOK, bag.AddItem(item));
        uint32_t newindex = i + (uint32_t)BagCapacity::kDefaultCapacity;
        EXPECT_EQ(newindex + 1 , bag.item_size());
        EXPECT_EQ(newindex + 1, bag.pos_size());
        EXPECT_EQ(bag.FindtemByBos(newindex)->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.FindtemByBos(newindex)->size(), p.itemPBComp.size());
        EXPECT_EQ(bag.FindtemByBos(newindex)->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.FindtemByBos(newindex)->size(), p.itemPBComp.size());
        EXPECT_EQ(bag.FindItemByGuid(g_bag_node_sequence.LastId())->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.FindItemByGuid(g_bag_node_sequence.LastId())->size(), p.itemPBComp.size());
        EXPECT_EQ(newindex, bag.GetItemPos(g_bag_node_sequence.LastId()));
        EXPECT_EQ(g_bag_node_sequence.LastId(), bag.FindtemByBos(newindex)->Guid());
        EXPECT_EQ(g_bag_node_sequence.LastId(), bag.FindItemByGuid(g_bag_node_sequence.LastId())->Guid());
    }
    EXPECT_EQ(BagCapacity::kDefaultCapacity * 2, bag.item_size());
    EXPECT_EQ(BagCapacity::kDefaultCapacity * 2, bag.pos_size());
}

TEST(BagTest, Add10CanStack10CanNotStack)
{
    Bag bag;
    bag.Unlock(BagCapacity::kDefaultCapacity);
    InitItemParam p;
    uint32_t config_id10 = 10;
    uint32_t config_id1 = 1;
    p.itemPBComp.set_config_id(config_id1);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * BagCapacity::kDefaultCapacity);
    auto item = InitItem(p);
    EXPECT_EQ(kOK, bag.AddItem(item));
    p.itemPBComp.set_config_id(config_id10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * BagCapacity::kDefaultCapacity);
    item = InitItem(p);
    EXPECT_EQ(kOK, bag.AddItem(item));
    EXPECT_EQ(BagCapacity::kDefaultCapacity * 2, bag.item_size());
    EXPECT_EQ(BagCapacity::kDefaultCapacity * 2, bag.pos_size());
}

TEST(BagTest, AddStackItemHalfAdd)
{
    Bag bag;
    InitItemParam p;
    p.itemPBComp.set_config_id(10);
    for (uint32_t i = 0; i < (uint32_t)BagCapacity::kDefaultCapacity * 2; i++)
    {
        if (i % 2 == 0)
        {
            p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() / 2);
        }
        else
        {
            p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() / 2 + 1);
        }
        auto item = InitItem(p);
        EXPECT_EQ(kOK, bag.AddItem(item));
        auto index = i / 2;
        auto sz = i / 2 + 1;
        EXPECT_EQ(sz, bag.item_size());
        EXPECT_EQ(sz, bag.pos_size());
        if (i % 2 == 0)
        {
            EXPECT_EQ(bag.FindtemByBos(index)->size(), p.itemPBComp.size());
            EXPECT_EQ(bag.FindItemByGuid(g_bag_node_sequence.LastId())->size(), p.itemPBComp.size());
        }
        else
        {
            EXPECT_EQ(bag.FindtemByBos(index)->size(), GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());
            EXPECT_EQ(bag.FindItemByGuid(g_bag_node_sequence.LastId())->size(), GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());
        }
        
        EXPECT_EQ(bag.FindItemByGuid(g_bag_node_sequence.LastId())->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(index, bag.GetItemPos(g_bag_node_sequence.LastId()));
        EXPECT_EQ(g_bag_node_sequence.LastId(), bag.FindtemByBos(index)->Guid());
        EXPECT_EQ(g_bag_node_sequence.LastId(), bag.FindItemByGuid(g_bag_node_sequence.LastId())->Guid());
    }
    EXPECT_EQ(BagCapacity::kDefaultCapacity, bag.item_size());
    EXPECT_EQ(BagCapacity::kDefaultCapacity, bag.pos_size());
}

TEST(BagTest, AddStackItemUnlock)
{
    Bag bag;
    InitItemParam p;
    p.itemPBComp.set_config_id(10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());
    for (uint32_t i = 0; i < (uint32_t)BagCapacity::kDefaultCapacity; i++)
    {
        auto item = InitItem(p);
        EXPECT_EQ(kOK, bag.AddItem(item));
        EXPECT_EQ(i + 1, bag.item_size());
        EXPECT_EQ(i + 1, bag.pos_size());
        EXPECT_EQ(bag.FindtemByBos(i)->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.FindtemByBos(i)->size(), p.itemPBComp.size());
        EXPECT_EQ(bag.FindtemByBos(i)->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.FindtemByBos(i)->size(), p.itemPBComp.size());
        EXPECT_EQ(bag.FindItemByGuid(g_bag_node_sequence.LastId())->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.FindItemByGuid(g_bag_node_sequence.LastId())->size(), p.itemPBComp.size());
        EXPECT_EQ(i, bag.GetItemPos(g_bag_node_sequence.LastId()));
        EXPECT_EQ(g_bag_node_sequence.LastId(), bag.FindtemByBos(i)->Guid());
        EXPECT_EQ(g_bag_node_sequence.LastId(), bag.FindItemByGuid(g_bag_node_sequence.LastId())->Guid());
    }
    EXPECT_EQ(BagCapacity::kDefaultCapacity, bag.item_size());
    EXPECT_EQ(BagCapacity::kDefaultCapacity, bag.pos_size());

    auto item = InitItem(p);
    EXPECT_EQ(kBagAddItemBagFull, bag.AddItem(item));
    bag.Unlock(BagCapacity::kDefaultCapacity);
    for (uint32_t i = 0; i < (uint32_t)BagCapacity::kDefaultCapacity; i++)
    {
        auto item = InitItem(p);
        EXPECT_EQ(kOK, bag.AddItem(item));
        uint32_t newindex = i + (uint32_t)BagCapacity::kDefaultCapacity;
        EXPECT_EQ(newindex + 1, bag.item_size());
        EXPECT_EQ(newindex + 1, bag.pos_size());
        EXPECT_EQ(bag.FindtemByBos(newindex)->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.FindtemByBos(newindex)->size(), p.itemPBComp.size());
        EXPECT_EQ(bag.FindtemByBos(newindex)->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.FindtemByBos(newindex)->size(), p.itemPBComp.size());
        EXPECT_EQ(bag.FindItemByGuid(g_bag_node_sequence.LastId())->config_id(), p.itemPBComp.config_id());
        EXPECT_EQ(bag.FindItemByGuid(g_bag_node_sequence.LastId())->size(), p.itemPBComp.size());
        EXPECT_EQ(newindex, bag.GetItemPos(g_bag_node_sequence.LastId()));
        EXPECT_EQ(g_bag_node_sequence.LastId(), bag.FindtemByBos(newindex)->Guid());
        EXPECT_EQ(g_bag_node_sequence.LastId(), bag.FindItemByGuid(g_bag_node_sequence.LastId())->Guid());
    }
    EXPECT_EQ(BagCapacity::kDefaultCapacity * 2, bag.item_size());
    EXPECT_EQ(BagCapacity::kDefaultCapacity * 2, bag.pos_size());
}

TEST(BagTest, AdequateSizeAddItemCannotStackItemFull)
{
    uint32_t config_id = 1;
    Bag bag;
    U32U32UnorderedMap adequate_add{ {config_id, (uint32_t)BagCapacity::kDefaultCapacity + 1 } };
    EXPECT_EQ(kBagItemNotStacked, bag.HasEnoughSpace(adequate_add));
    adequate_add[config_id] = (uint32_t)BagCapacity::kDefaultCapacity;
    EXPECT_EQ(kOK, bag.HasEnoughSpace(adequate_add));
}

TEST(BagTest, AdequateSizeAddItemmixtureFull)
{
    uint32_t cannot_stack_config_id = 1;
    uint32_t stack_config_id = 10;
    Bag bag;
    U32U32UnorderedMap adequate_add{ {cannot_stack_config_id, 1 },
        {stack_config_id, GetItemTable(stack_config_id).first->max_statck_size() * (uint32_t)BagCapacity::kDefaultCapacity} };
    EXPECT_EQ(kBagItemNotStacked, bag.HasEnoughSpace(adequate_add));
    adequate_add[stack_config_id] = (uint32_t)(BagCapacity::kDefaultCapacity - 1) * GetItemTable(stack_config_id).first->max_statck_size();
    EXPECT_EQ(kOK, bag.HasEnoughSpace(adequate_add));

    InitItemParam p;
    p.itemPBComp.set_config_id(cannot_stack_config_id);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());
    auto item = InitItem(p);
    EXPECT_EQ(kOK, bag.AddItem(item));
    EXPECT_EQ(kBagItemNotStacked, bag.HasEnoughSpace(adequate_add));
    adequate_add[stack_config_id] = (uint32_t)(BagCapacity::kDefaultCapacity - 2) * GetItemTable(stack_config_id).first->max_statck_size();
    EXPECT_EQ(kOK, bag.HasEnoughSpace(adequate_add));
    p.itemPBComp.set_config_id(stack_config_id);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());
    item = InitItem(p);
    EXPECT_EQ(kOK, bag.AddItem(item));
    EXPECT_EQ(kBagItemNotStacked, bag.HasEnoughSpace(adequate_add));//��Ϊռ�����������ӣ������ܹ�����ʮ������
    adequate_add[stack_config_id] = (uint32_t)(BagCapacity::kDefaultCapacity - 3) * GetItemTable(stack_config_id).first->max_statck_size();
    EXPECT_EQ(kOK, bag.HasEnoughSpace(adequate_add));
    p.itemPBComp.set_config_id(stack_config_id);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() - 100);
    item = InitItem(p);
    EXPECT_EQ(kOK, bag.AddItem(item));
    EXPECT_EQ(kBagItemNotStacked, bag.HasEnoughSpace(adequate_add));//��Ϊռ�����������ӣ������ܹ�����ʮ������
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
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(adequate_item));//�ձ�������
    InitItemParam p;
    p.itemPBComp.set_config_id(config_id10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());
    auto item = InitItem(p);
    EXPECT_EQ(kOK, bag.AddItem(item));
    EXPECT_EQ(kOK, bag.HasSufficientItems(adequate_item));
    adequate_item[config_id10] = GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() / 2;
    EXPECT_EQ(kOK, bag.HasSufficientItems(adequate_item));
    adequate_item.emplace(config_id1, 1);//���ɵ���һ��
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(adequate_item));

    p.itemPBComp.set_config_id(config_id1);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());
    item = InitItem(p);//����һ�������Ե��ӵ�
    EXPECT_EQ(kOK, bag.AddItem(item));
    EXPECT_EQ(kOK, bag.HasSufficientItems(adequate_item));
    adequate_item[config_id10] = GetItemTable(config_id10).first->max_statck_size();//1��10�ɵ���999
    EXPECT_EQ(kOK, bag.HasSufficientItems(adequate_item));
    adequate_item[config_id10] = GetItemTable(config_id10).first->max_statck_size() + 1;//1��10�ɵ���1000
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(adequate_item));
    adequate_item[config_id10] = GetItemTable(config_id10).first->max_statck_size() * 3;//3��10�ɵ���999*3
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(adequate_item));

    p.itemPBComp.set_config_id(config_id10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());
    item = InitItem(p);//����һ�����Ե��ӵ�
    EXPECT_EQ(kOK, bag.AddItem(item));
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(adequate_item));//2��10�ĵ���999

    p.itemPBComp.set_config_id(config_id11);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * 3);
    item = InitItem(p);//����һ�����Ե��ӵ�
    EXPECT_EQ(kOK, bag.AddItem(item));
    EXPECT_EQ(kBagInsufficientItems, bag.HasSufficientItems(adequate_item));//2��10�ĵ���999

    p.itemPBComp.set_config_id(config_id10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());
    item = InitItem(p);//����һ�����Ե��ӵ�
    EXPECT_EQ(kOK, bag.AddItem(item));
    EXPECT_EQ(kOK, bag.HasSufficientItems(adequate_item));//3��10�ĵ���999

    adequate_item[config_id11] = GetItemTable(config_id11).first->max_statck_size() * 3;//3��10�ɵ���999 3��11�ɵ���999
    EXPECT_EQ(kOK, bag.HasSufficientItems(adequate_item));
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

    InitItemParam p;
    p.itemPBComp.set_config_id(test_config_id10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * 2);// 999 * 2
    auto item = InitItem(p);
    EXPECT_EQ(kOK, bag.AddItem(item));

    p.itemPBComp.set_config_id(config_id1);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());// 1
    item = InitItem(p);//����һ�������Ե��ӵ�
    EXPECT_EQ(kOK, bag.AddItem(item));

    p.itemPBComp.set_config_id(config_id2);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());//  1
    item = InitItem(p);//����һ�����Ե��ӵ�
    EXPECT_EQ(kOK, bag.AddItem(item));

    p.itemPBComp.set_config_id(config_id11);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());// 999 * 1
    item = InitItem(p);//����һ�����Ե��ӵ�
    EXPECT_EQ(kOK, bag.AddItem(item));

    U32U32UnorderedMap try_del;
    try_del.emplace(test_config_id10, 1);
    EXPECT_EQ(kOK, bag.DelItem(try_del));
    EXPECT_EQ(GetItemTable(test_config_id10).first->max_statck_size() * 2 - 1, bag.GetItemStackSize(test_config_id10));
    EXPECT_EQ(GetItemTable(config_id1).first->max_statck_size(), bag.GetItemStackSize(config_id1));
    EXPECT_EQ(GetItemTable(config_id2).first->max_statck_size(), bag.GetItemStackSize(config_id2));
    EXPECT_EQ(GetItemTable(config_id11).first->max_statck_size(), bag.GetItemStackSize(config_id11));
    try_del[test_config_id10] = GetItemTable(test_config_id10).first->max_statck_size() * 2 - 1;
    try_del[config_id1] = GetItemTable(config_id1).first->max_statck_size();
    try_del[config_id2] = GetItemTable(config_id2).first->max_statck_size();
    try_del[config_id11] = GetItemTable(config_id11).first->max_statck_size();
    EXPECT_EQ(kOK, bag.DelItem(try_del));
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
    InitItemParam p;
    uint32_t config_id1 = 1;
    p.itemPBComp.set_config_id(config_id1);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());// 1
    auto item = InitItem(p);//����һ�������Ե��ӵ�
    EXPECT_EQ(kOK, bag.AddItem(item));
    EXPECT_EQ(1, bag.item_size());
    EXPECT_EQ(1, bag.pos_size());
    EXPECT_EQ(kOK, bag.RemoveItem(g_bag_node_sequence.LastId()));
    EXPECT_EQ(0, bag.item_size());
    EXPECT_EQ(0, bag.pos_size());
}

TEST(BagTest, DelItemByPos)
{
    Bag bag;
    InitItemParam p;
    uint32_t config_id10 = 10;
    p.itemPBComp.set_config_id(config_id10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size());// 999
    auto item = InitItem(p);
    EXPECT_EQ(kOK, bag.AddItem(item));
    EXPECT_EQ(1, bag.item_size());
    EXPECT_EQ(1, bag.pos_size());
    DelItemByPosParam dp;
    EXPECT_EQ(kBagDelItemPos, bag.DelItemByPos(dp));
    dp.pos_ = 0;
    EXPECT_EQ(kBagDelItemGuid, bag.DelItemByPos(dp));
    dp.item_guid_ = g_bag_node_sequence.LastId();
    EXPECT_EQ(kBagDelItemConfig, bag.DelItemByPos(dp));
    dp.item_config_id_ = config_id10;
    EXPECT_EQ(kOK, bag.DelItemByPos(dp));
    EXPECT_EQ(GetItemTable(config_id10).first->max_statck_size() - 1, bag.GetItemStackSize(config_id10));
    dp.size_ = GetItemTable(config_id10).first->max_statck_size() - 1;
    EXPECT_EQ(kOK, bag.DelItemByPos(dp));
    EXPECT_EQ(0, bag.GetItemStackSize(config_id10));
    EXPECT_EQ(1, bag.item_size());
    EXPECT_EQ(1, bag.pos_size());
    EXPECT_EQ(0, bag.FindtemByBos(0)->size());
    EXPECT_EQ(0, bag.FindItemByGuid(g_bag_node_sequence.LastId())->size());
}

//����1��ÿ������ʹ��һ��
TEST(BagTest, Neaten1)
{
    Bag bag;
    auto unlock_size = BagCapacity::kDefaultCapacity;
    bag.Unlock(unlock_size);
    InitItemParam p;
    uint32_t config_id10 = 10;
    uint32_t config_id11 = 11;
    p.itemPBComp.set_config_id(config_id10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * BagCapacity::kDefaultCapacity);// 999 * 10
    auto item = InitItem(p);
    EXPECT_EQ(kOK, bag.AddItem(item));
    auto id10 = g_bag_node_sequence.LastId();
    p.itemPBComp.set_config_id(config_id11);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * BagCapacity::kDefaultCapacity);// 999 * 10
    item = InitItem(p);
    EXPECT_EQ(kOK, bag.AddItem(item));
    auto id11 = g_bag_node_sequence.LastId();
    for (uint32_t i = 0; i < (uint32_t)BagCapacity::kDefaultCapacity; ++i)
    {
        DelItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = id10 - (BagCapacity::kDefaultCapacity - i) + 1;
        dp.item_config_id_ = config_id10;
        dp.size_ = GetItemTable(config_id10).first->max_statck_size() - 1;
        EXPECT_EQ(kOK, bag.DelItemByPos(dp));
    }   
    for (uint32_t i = (uint32_t)BagCapacity::kDefaultCapacity; i < bag.pos_size(); ++i)
    {
        DelItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = id11 - (BagCapacity::kDefaultCapacity - (i - BagCapacity::kDefaultCapacity)) + 1;
        dp.item_config_id_ = config_id11;
        dp.size_ = GetItemTable(config_id11).first->max_statck_size() - 1;
        EXPECT_EQ(kOK, bag.DelItemByPos(dp));
    }
    for (uint32_t i = 0; i < (uint32_t)bag.pos_size(); ++i)
    {
        EXPECT_EQ(1, bag.FindtemByBos(i)->size());
    }
    bag.Neaten();
    EXPECT_EQ(2, bag.item_size());
    EXPECT_EQ(2, bag.pos_size());
    for (auto& it : bag.pos())
    {
        EXPECT_EQ(BagCapacity::kDefaultCapacity, (std::size_t)bag.FindtemByBos(bag.GetItemPos(it.second))->size());
    }
    EXPECT_EQ(BagCapacity::kDefaultCapacity, bag.GetItemStackSize(config_id10));
    EXPECT_EQ(BagCapacity::kDefaultCapacity, bag.GetItemStackSize(config_id11));
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
    auto item = InitItem(p);
    EXPECT_EQ(kOK, bag.AddItem(item));
    auto id10 = g_bag_node_sequence.LastId();
    p.itemPBComp.set_config_id(config_id11);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * uint32_t(unlock_size / 2));// 999 * 200
    item = InitItem(p);
    EXPECT_EQ(kOK, bag.AddItem(item));
    auto id11 = g_bag_node_sequence.LastId();
    auto config_id10_sz = unlock_size / 2;
    for (uint32_t i = 0; i < config_id10_sz; ++i)
    {
        DelItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = id10 - (config_id10_sz - i) + 1;
        dp.item_config_id_ = config_id10;
        dp.size_ = GetItemTable(config_id10).first->max_statck_size() - 1;
        EXPECT_EQ(kOK, bag.DelItemByPos(dp));
    }
    for (uint32_t i = uint32_t(config_id10_sz); i < bag.pos_size(); ++i)
    {
        DelItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = id11 - (config_id10_sz - (i - config_id10_sz)) + 1;
        dp.item_config_id_ = config_id11;
        dp.size_ = GetItemTable(config_id11).first->max_statck_size() - 1;
        EXPECT_EQ(kOK, bag.DelItemByPos(dp));
    }
    for (uint32_t i = 0; i < (uint32_t)bag.pos_size(); ++i)
    {
        EXPECT_EQ(1, bag.FindtemByBos(i)->size());
    }
    bag.Neaten();
    EXPECT_EQ(2, bag.item_size());
    EXPECT_EQ(2, bag.pos_size());
    std::size_t grid_sz = 200;
    for (auto& it : bag.pos())
    {
        EXPECT_EQ(grid_sz, (std::size_t)bag.FindtemByBos(bag.GetItemPos(it.second))->size());
    }
    EXPECT_EQ(grid_sz, bag.GetItemStackSize(config_id10));
    EXPECT_EQ(grid_sz, bag.GetItemStackSize(config_id11));
}

//����400���ӣ�ÿ����Ʒǰ100����998
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
    auto item = InitItem(p);
    EXPECT_EQ(kOK, bag.AddItem(item));
    auto id10 = g_bag_node_sequence.LastId();
    p.itemPBComp.set_config_id(config_id11);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * uint32_t(per_grid_size));// 999 * 200
    item = InitItem(p);
    EXPECT_EQ(kOK, bag.AddItem(item));
    auto id11 = g_bag_node_sequence.LastId();
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
        dp.size_ = GetItemTable(config_id10).first->max_statck_size() - 1;
        EXPECT_EQ(kOK, bag.DelItemByPos(dp));
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
        dp.size_ = GetItemTable(config_id11).first->max_statck_size() - 1;
        EXPECT_EQ(kOK, bag.DelItemByPos(dp));
    }
    auto index1 = use_config_id10_sz;//��һ�ٸ�������ǰ1
    auto index2 = use_config_id10_sz * 2;//�ڶ��ٸ�������ǰ999
    auto index3 = use_config_id10_sz * 3;//��s���ٸ�������ǰ1
    auto index4 = use_config_id10_sz * 4;//��s���ٸ�������ǰ999
    for (uint32_t i = 0; i < (uint32_t)bag.pos_size(); ++i)
    {
        if (i < index1)
        {
            EXPECT_EQ(1, bag.FindtemByBos(i)->size());
        }
        else if (i < index2)
        {
            EXPECT_EQ(item_statck_max_sz, bag.FindtemByBos(i)->size());
        }
        else if (i < index3)
        {
            EXPECT_EQ(1, bag.FindtemByBos(i)->size());
        }
        else if (i < index4)
        {
            EXPECT_EQ(item_statck_max_sz, bag.FindtemByBos(i)->size());
        }
        else
        {
            EXPECT_EQ(0, bag.FindtemByBos(i)->size());
        }
    }
    bag.Neaten();
    std::size_t grid_sz = 200;
    std::size_t remain_sz = 100;
    EXPECT_EQ(grid_sz + 2, bag.item_size());
    EXPECT_EQ(grid_sz + 2, bag.pos_size());   
    UInt32Set pos999;
    UInt32Set pos100;
    for (uint32_t i = 0; i < (uint32_t)bag.pos_size(); ++i)
    {
        if (item_statck_max_sz == bag.FindtemByBos(i)->size())
        {
            pos999.emplace(i);
        }
        else if (100 == bag.FindtemByBos(i)->size())
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

//����1��ÿ������ʹ��һ��
TEST(BagTest, NeatenCanNotStack)
{
    Bag bag;
    auto unlock_size = BagCapacity::kDefaultCapacity;
    bag.Unlock(unlock_size);
    InitItemParam p;
    uint32_t config_id10 = 10;
    uint32_t config_id1 = 1;
    p.itemPBComp.set_config_id(config_id10);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * BagCapacity::kDefaultCapacity);// 999 * 10
    auto item = InitItem(p);
    EXPECT_EQ(kOK, bag.AddItem(item));
    auto id10 = g_bag_node_sequence.LastId();
    p.itemPBComp.set_config_id(config_id1);
    p.itemPBComp.set_size(GetItemTable(p.itemPBComp.config_id()).first->max_statck_size() * BagCapacity::kDefaultCapacity);// 999 * 10
    item = InitItem(p);
    EXPECT_EQ(kOK, bag.AddItem(item));
    auto id11 = g_bag_node_sequence.LastId();
    for (uint32_t i = 0; i < (uint32_t)BagCapacity::kDefaultCapacity; ++i)
    {
        DelItemByPosParam dp;
        dp.pos_ = i;
        dp.item_guid_ = id10 - (BagCapacity::kDefaultCapacity - i) + 1;
        dp.item_config_id_ = config_id10;
        dp.size_ = GetItemTable(config_id10).first->max_statck_size() - 1;
        EXPECT_EQ(kOK, bag.DelItemByPos(dp));
    }
 
    for (uint32_t i = 0; i < (uint32_t)bag.pos_size(); ++i)
    {
        EXPECT_EQ(1, bag.FindtemByBos(i)->size());
    }
    bag.Neaten();
    EXPECT_EQ(BagCapacity::kDefaultCapacity + 1, bag.item_size());
    EXPECT_EQ(BagCapacity::kDefaultCapacity + 1, bag.pos_size());
    for (auto& it : bag.pos())
    {
        if ((std::size_t)bag.FindtemByBos(bag.GetItemPos(it.second))->size() != BagCapacity::kDefaultCapacity)
        {
            EXPECT_EQ(1, (std::size_t)bag.FindtemByBos(bag.GetItemPos(it.second))->size());
        }
    }
    EXPECT_EQ(BagCapacity::kDefaultCapacity, bag.GetItemStackSize(config_id10));
    EXPECT_EQ(BagCapacity::kDefaultCapacity, bag.GetItemStackSize(config_id1));
}

int main(int argc, char** argv)
{
    ItemConfigurationTable::GetSingleton().Load();
    testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}

