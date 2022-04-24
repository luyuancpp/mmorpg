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

//һ��һ���������
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

//��ӿɵ��Ӳ���1
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

//�������Լ�������
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

//���ɵ�����Ʒ
TEST(BagTest, AdequateSizeAddItemCannotStackItemFull)
{
    uint32_t config_id = 1;
    Bag bag;
    UInt32UInt32UnorderedMap adequate_add{ {config_id, (uint32_t)BagCapacity::kDefualtCapacity + 1 } };
    EXPECT_EQ(kRetBagAdequateAddItemSize, bag.AdequateSizeAddItem(adequate_add));
    adequate_add[config_id] = (uint32_t)BagCapacity::kDefualtCapacity;
    EXPECT_EQ(kRetOK, bag.AdequateSizeAddItem(adequate_add));
}

//�ɵ��ӻ��,������Ʒ����ȫ�����������999
TEST(BagTest, AdequateSizeAddItemmixtureFull)
{
    uint32_t cannot_stack_config_id = 1;//�����Ե��ӵ���Ʒid
    uint32_t stack_config_id = 10;//���Ե��ӵ���Ʒid
    Bag bag;
    //һ�����ɵ��ӣ�10�����Ե���
    UInt32UInt32UnorderedMap adequate_add{ {cannot_stack_config_id, 1 },
        {stack_config_id, get_item_conf(stack_config_id)->max_statck_size() * (uint32_t)BagCapacity::kDefualtCapacity} };
    EXPECT_EQ(kRetBagAdequateAddItemSize, bag.AdequateSizeAddItem(adequate_add));
    //�ĳ�һ�����ɵ��ӣ��Ÿ����Ե���
    adequate_add[stack_config_id] = (uint32_t)(BagCapacity::kDefualtCapacity - 1) * get_item_conf(stack_config_id)->max_statck_size();
    EXPECT_EQ(kRetOK, bag.AdequateSizeAddItem(adequate_add));

    //���һ�������Ժ󲻿��Ե����ˣ����һ�����Ե��ӵ���Ʒ
    CreateItemParam p;
    p.item_base_db.set_config_id(cannot_stack_config_id);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());
    auto item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetBagAdequateAddItemSize, bag.AdequateSizeAddItem(adequate_add));//��Ϊռ����һ�����ӣ������ܹ�����ʮ������
    //�ĳ�8�����Ե��ӵĸ���,һ�����ɵ��ӣ��ܹ���Ҫ�Ÿ�����
    adequate_add[stack_config_id] = (uint32_t)(BagCapacity::kDefualtCapacity - 2) * get_item_conf(stack_config_id)->max_statck_size();
    EXPECT_EQ(kRetOK, bag.AdequateSizeAddItem(adequate_add));
    //��һ�����Ե��ӵĸ���
    p.item_base_db.set_config_id(stack_config_id);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());
    item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetBagAdequateAddItemSize, bag.AdequateSizeAddItem(adequate_add));//��Ϊռ�����������ӣ������ܹ�����ʮ������
    //�ĳ�7�����Ե��ӵĸ���,1�����ɵ��ӣ��ܹ���Ҫ8������
    adequate_add[stack_config_id] = (uint32_t)(BagCapacity::kDefualtCapacity - 3) * get_item_conf(stack_config_id)->max_statck_size();
    EXPECT_EQ(kRetOK, bag.AdequateSizeAddItem(adequate_add));
    //��һ�����Ե��ӵĸ��ӣ�������100
    p.item_base_db.set_config_id(stack_config_id);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size() - 100);
    item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetBagAdequateAddItemSize, bag.AdequateSizeAddItem(adequate_add));//��Ϊռ�����������ӣ������ܹ�����ʮ������
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
    common::UInt32UInt32UnorderedMap adequate_item{ {config_id10 , 1} };
    EXPECT_EQ(kRetBagAdequatetem, bag.AdequateItem(adequate_item));//�ձ�������
    CreateItemParam p;
    p.item_base_db.set_config_id(config_id10);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());
    auto item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetOK, bag.AdequateItem(adequate_item));
    adequate_item[config_id10] = get_item_conf(p.item_base_db.config_id())->max_statck_size() / 2;
    EXPECT_EQ(kRetOK, bag.AdequateItem(adequate_item));
    adequate_item.emplace(config_id1, 1);//���ɵ���һ��
    EXPECT_EQ(kRetBagAdequatetem, bag.AdequateItem(adequate_item));

    p.item_base_db.set_config_id(config_id1);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());
    item = CreateItem(p);//����һ�������Ե��ӵ�
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetOK, bag.AdequateItem(adequate_item));
    adequate_item[config_id10] = get_item_conf(config_id10)->max_statck_size();//1��10�ɵ���999
    EXPECT_EQ(kRetOK, bag.AdequateItem(adequate_item));
    adequate_item[config_id10] = get_item_conf(config_id10)->max_statck_size() + 1;//1��10�ɵ���1000
    EXPECT_EQ(kRetBagAdequatetem, bag.AdequateItem(adequate_item));
    adequate_item[config_id10] = get_item_conf(config_id10)->max_statck_size() * 3;//3��10�ɵ���999*3
    EXPECT_EQ(kRetBagAdequatetem, bag.AdequateItem(adequate_item));

    p.item_base_db.set_config_id(config_id10);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());
    item = CreateItem(p);//����һ�����Ե��ӵ�
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetBagAdequatetem, bag.AdequateItem(adequate_item));//2��10�ĵ���999

    p.item_base_db.set_config_id(config_id11);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size() * 3);
    item = CreateItem(p);//����һ�����Ե��ӵ�
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetBagAdequatetem, bag.AdequateItem(adequate_item));//2��10�ĵ���999

    p.item_base_db.set_config_id(config_id10);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());
    item = CreateItem(p);//����һ�����Ե��ӵ�
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetOK, bag.AdequateItem(adequate_item));//3��10�ĵ���999

    adequate_item[config_id11] = get_item_conf(config_id11)->max_statck_size() * 3;//3��10�ɵ���999 3��11�ɵ���999
    EXPECT_EQ(kRetOK, bag.AdequateItem(adequate_item));
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

    CreateItemParam p;
    p.item_base_db.set_config_id(test_config_id10);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size() * 2);// 999 * 2
    auto item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));

    p.item_base_db.set_config_id(config_id1);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());// 1
    item = CreateItem(p);//����һ�������Ե��ӵ�
    EXPECT_EQ(kRetOK, bag.AddItem(item));

    p.item_base_db.set_config_id(config_id2);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());//  1
    item = CreateItem(p);//����һ�����Ե��ӵ�
    EXPECT_EQ(kRetOK, bag.AddItem(item));

    p.item_base_db.set_config_id(config_id11);
    p.item_base_db.set_size(get_item_conf(p.item_base_db.config_id())->max_statck_size());// 999 * 1
    item = CreateItem(p);//����һ�����Ե��ӵ�
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
    auto item = CreateItem(p);//����һ�������Ե��ӵ�
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

//����1��ÿ������ʹ��һ��
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

int main(int argc, char** argv)
{
    item_config::GetSingleton().load();
    testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}

