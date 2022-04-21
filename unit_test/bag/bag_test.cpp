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

//һ��һ���������
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

//��ӿɵ��Ӳ���1
TEST(BagTest, AddStackItemHalfAdd)
{
    Bag bag;
    CreateItemParam p;
    p.config_id_ = 10;
    for (uint32_t i = 0; i < (uint32_t)BagCapacity::kDefualtCapacity * 2; i++)
    {
        if (i % 2 == 0)
        {
            p.size_ = get_item_conf(p.config_id_)->max_statck_size() / 2;
        }
        else
        {
            p.size_ = get_item_conf(p.config_id_)->max_statck_size() / 2 + 1;
        }
        auto item = CreateItem(p);
        EXPECT_EQ(kRetOK, bag.AddItem(item));
        auto index = i / 2;
        auto sz = i / 2 + 1;
        EXPECT_EQ(sz, bag.item_size());
        EXPECT_EQ(sz, bag.pos_size());
        if (i % 2 == 0)
        {
            EXPECT_EQ(bag.GetItemByBos(index)->size(), p.size_);
            EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->size(), p.size_);
        }
        else
        {
            EXPECT_EQ(bag.GetItemByBos(index)->size(), get_item_conf(p.config_id_)->max_statck_size());
            EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->size(), get_item_conf(p.config_id_)->max_statck_size());
        }
        
        EXPECT_EQ(bag.GetItemByGuid(g_server_sequence.Current())->config_id(), p.config_id_);
        EXPECT_EQ(index, bag.GetItemPos(g_server_sequence.Current()));
        EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByBos(index)->guid());
        EXPECT_EQ(g_server_sequence.Current(), bag.GetItemByGuid(g_server_sequence.Current())->guid());
    }
    EXPECT_EQ(BagCapacity::kDefualtCapacity, bag.item_size());
    EXPECT_EQ(BagCapacity::kDefualtCapacity, bag.pos_size());
}

//�������Լ�������
TEST(BagTest, AddStackItemUnlock)
{
    Bag bag;
    CreateItemParam p;
    p.config_id_ = 10;
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
    p.config_id_ = cannot_stack_config_id;
    p.size_ = get_item_conf(p.config_id_)->max_statck_size();
    auto item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetBagAdequateAddItemSize, bag.AdequateSizeAddItem(adequate_add));//��Ϊռ����һ�����ӣ������ܹ�����ʮ������
    //�ĳ�8�����Ե��ӵĸ���,һ�����ɵ��ӣ��ܹ���Ҫ�Ÿ�����
    adequate_add[stack_config_id] = (uint32_t)(BagCapacity::kDefualtCapacity - 2) * get_item_conf(stack_config_id)->max_statck_size();
    EXPECT_EQ(kRetOK, bag.AdequateSizeAddItem(adequate_add));
    //��һ�����Ե��ӵĸ���
    p.config_id_ = stack_config_id;
    p.size_ = get_item_conf(p.config_id_)->max_statck_size();
    item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetBagAdequateAddItemSize, bag.AdequateSizeAddItem(adequate_add));//��Ϊռ�����������ӣ������ܹ�����ʮ������
    //�ĳ�7�����Ե��ӵĸ���,1�����ɵ��ӣ��ܹ���Ҫ8������
    adequate_add[stack_config_id] = (uint32_t)(BagCapacity::kDefualtCapacity - 3) * get_item_conf(stack_config_id)->max_statck_size();
    EXPECT_EQ(kRetOK, bag.AdequateSizeAddItem(adequate_add));
    //��һ�����Ե��ӵĸ��ӣ�������100
    p.config_id_ = stack_config_id;
    p.size_ = get_item_conf(p.config_id_)->max_statck_size() - 100;
    item = CreateItem(p);
    EXPECT_EQ(kRetOK, bag.AddItem(item));
    EXPECT_EQ(kRetBagAdequateAddItemSize, bag.AdequateSizeAddItem(adequate_add));//��Ϊռ�����������ӣ������ܹ�����ʮ������
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

