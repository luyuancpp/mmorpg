#include <gtest/gtest.h>

#include "StlDefineType.h"

#include "CommonLogic/Bag/Bag.h"
#include "CommonLogic/Bag/ItemModule.h"
#include "GenCode/GameDefine_Result.h"
#include "GenCode/Config/ConfigModule.h"
#include "CommonLogic/Equipment/MountEquipment.h"

using namespace HumanBag;

void ItemMountEquip::gen_base_attr_by_config(const std::vector<int> &config)
{

}

int ItemMountEquip::GetLevel()const
{
	return 0;
}

float ItemMountEquip::CaculBattleScore()
{
	return 0;
}



void AddFiveItem(BagModule::bag_ptr_type & pBag)
{
	ItemFactory factory;
	BagBase::item_op_output_type vOut;
    CreateItemParam p;
	for (int32_t i = 0; i < 5; ++i)
	{
        p.m_nConfigId = 11;
        p.m_nCount = 1;
		EXPECT_EQ(OR_OK, pBag->AddItemForTest(p));
	}
}

TEST(BagBase, TestAddGuid)
{
   /* ItemLogic::ItemModule im;
    im.ResetBagModule();
    BagModule::bag_ptr_type pBag = im.GetBagModule()->GetBag(BagBase::E_BAG_TYPE);
    CreateItemParam cip;
    cip.m_bIsBindItem = false;
    cip.m_nConfigId = 6;
    cip.m_nCount = 7;

    im.GetBagModule()->CreateItem(cip);
    int32_t count = im.GetBagModule()->GetNewOneAddedOrChangeItem().itemdata().overlaycount();
    EXPECT_EQ(im.GetBagModule()->GetNewOneAddedOrChangeItem().itemdata().guid(), pBag->GetItem(0)->GetGuid());
    EXPECT_EQ(count, pBag->GetItem(0)->GetOverlaySize());
    count += im.GetBagModule()->GetNewOneAddedOrChangeItem().itemdata().overlaycount();
    im.GetBagModule()->CreateItem(cip);
    EXPECT_EQ(im.GetBagModule()->GetNewOneAddedOrChangeItem().itemdata().guid(), pBag->GetItem(0)->GetGuid());
    EXPECT_EQ(count, pBag->GetItem(0)->GetOverlaySize());*/
}

TEST(BagBase, AddItem1)
{
    BagModule oModule;

    BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);
    BagModule::bag_ptr_type pBag1 = oModule.GetBag(BagBase::E_BAG_TEMP_TYPE);
    CreateItemParam p;
    p.m_nConfigId = 4;
    std::size_t  grid = HumanBag::BagBase::g_nBagDefaultCapacity - 2;
    for (std::size_t i = 0; i < HumanBag::BagBase::g_nBagDefaultCapacity - 2; ++i)
    {
        EXPECT_EQ(OR_OK, oModule.CreateItem(p));
    }
    EXPECT_EQ(grid, pBag->GetItemSize());
    p.m_nConfigId = 5;
    p.m_nCount = 297;
    EXPECT_EQ(OR_OK, oModule.CreateItem(p));
    EXPECT_EQ(99, pBag->GetItem(grid)->GetOverlaySize());
    EXPECT_EQ(99, pBag->GetItem(grid + 1)->GetOverlaySize());
    EXPECT_EQ(99, pBag1->GetItem(0)->GetOverlaySize());
    
}

TEST(BagBase, BagFull)
{
	BagModule oModule;
	BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);
	
	ItemFactory factory;
	BagBase::item_op_output_type vOut;
    CreateItemParam p;
    p.m_nConfigId = 11;
	for (std::size_t i = 0; i < HumanBag::BagBase::g_nBagDefaultCapacity; ++i)
	{
		EXPECT_EQ(OR_OK, oModule.CreateItem(p));
	}
    p.m_nCount = 1;
	EXPECT_EQ(OR_BAG_FULL, pBag->AddItemForTest(p));

    EXPECT_EQ(OR_OK, pBag->Deblocking());
	AddFiveItem(pBag);

	EXPECT_EQ(OR_BAG_FULL, pBag->AddItemForTest(p));

	for (std::size_t i = 0; i < (BagBase::g_nBagMaxSize - BagBase::g_nBagDefaultCapacity) / BagBase::g_nBagDeblockingSize - 1; ++i)
	{
        EXPECT_EQ(OR_OK, pBag->Deblocking());
		AddFiveItem(pBag);
	}

	EXPECT_EQ(OR_BAG_FULL, pBag->AddItemForTest(p));
	EXPECT_EQ(HumanBag::BagBase::g_nBagMaxSize, pBag->GetItemSize());
	EXPECT_EQ(HumanBag::BagBase::g_nBagMaxSize, pBag->GetCurCapacity());
}

TEST(BagBase, StorageBagFull)
{
	BagModule oModule;
	BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_STORAGE_TYPE);

	ItemFactory factory;
    CreateItemParam p;
	for (std::size_t i = 0; i < HumanBag::BagBase::g_nBagDefaultCapacity; ++i)
	{
        p.m_nConfigId = 11;
        p.m_nCount = 1;
		EXPECT_EQ(OR_OK, pBag->AddItemForTest(p));
	}
    p.m_nConfigId = 11;
	BagBase::item_type pItem = factory.CreateItem(p);
	BagBase::item_op_output_type vOut;
	EXPECT_EQ(OR_STORAGE_BAG_FULL, pBag->AddItemForTest(p));
}


TEST(BagBase, TempBagFull)
{
	BagModule oModule;
	BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TEMP_TYPE);

	ItemFactory factory;
    CreateItemParam p;
	for (std::size_t i = 0; i < HumanBag::BagBase::g_nTempBagMaxSize; ++i)
	{
        p.m_nConfigId = 11;
		BagBase::item_op_output_type vOut;
		EXPECT_EQ(OR_OK, pBag->AddItemForTest(p));
	}

	EXPECT_EQ(HumanBag::BagBase::g_nTempBagMaxSize, pBag->GetItemSize());
	EXPECT_EQ(HumanBag::BagBase::g_nTempBagMaxSize, pBag->GetCurCapacity());
    p.m_nConfigId = 11;
	BagBase::item_op_output_type vOut;
	EXPECT_EQ(OR_OK, pBag->AddItemForTest(p));
	EXPECT_EQ(HumanBag::BagBase::g_nTempBagMaxSize, pBag->GetItemSize());
	EXPECT_EQ(HumanBag::BagBase::g_nTempBagMaxSize, pBag->GetCurCapacity());
}

TEST(BagBase, TempBagCanoverLayFull)
{
	BagModule oModule;
	BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TEMP_TYPE);

	ItemFactory factory;
	CreateItemParam p;
	p.m_nConfigId = 5;
	p.m_nCount = 1188 - 1;

	EXPECT_EQ(OR_OK, pBag->AddItemForTest(p));

	EXPECT_EQ(HumanBag::BagBase::g_nTempBagMaxSize, pBag->GetItemSize());
	EXPECT_EQ(HumanBag::BagBase::g_nTempBagMaxSize, pBag->GetCurCapacity());
	p.m_nCount = 1;
	EXPECT_EQ(OR_OK, pBag->AddItemForTest(p));
	EXPECT_EQ(HumanBag::BagBase::g_nTempBagMaxSize, pBag->GetItemSize());
	EXPECT_EQ(HumanBag::BagBase::g_nTempBagMaxSize, pBag->GetCurCapacity());
}

TEST(BagBase, TidyBagSameType)
{
	BagModule oModule;
    CreateItemParam p;
    p.m_nConfigId = 5;
    p.m_nCount = 999;
	BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);
    EXPECT_EQ(OR_OK, oModule.CreateItem(p));
	EXPECT_EQ(999, pBag->GetOverlaySize());
	pBag->TestOutPut();
}

TEST(BagBase, TidyBag1)
{
	BagModule oModule;
	BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);
    
	BagBase::item_op_output_type vOut;
    CreateItemParam cip;
    cip.m_nConfigId = 11;
    cip.m_nCount = 10;
	EXPECT_EQ(OR_OK, oModule.CreateItem(cip));

    cip.m_nConfigId = 5;
    cip.m_nCount = 800;
    EXPECT_EQ(OR_OK, oModule.CreateItem(cip));

    cip.m_nConfigId = 11;
    cip.m_nCount = 11;

	EXPECT_EQ(OR_OK, oModule.CreateItem(cip));

	//EXPECT_EQ(999, pBag->GetOverlaySize());
	pBag->TestOutPut();

	pBag->Tidy();
	pBag->TestOutPut();
}

TEST(BagBase, UseItem)
{
	BagModule oModule;
	BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);
	GUID_t nGuid = 0;
	BagBase::item_op_output_type vOut;
    CreateItemParam cip;
    cip.m_nConfigId = 11;
    cip.m_nCount = 10;

	EXPECT_EQ(OR_OK, oModule.CreateItem(cip));
	
    cip.m_nConfigId = 5;
    cip.m_nCount = 10;
	
	EXPECT_EQ(OR_OK, oModule.CreateItem(cip));

	
	EXPECT_EQ(11, pBag->GetItemSize());
	pBag->TestOutPut();

	ItemParam p{1, 1, -1, 1};
	EXPECT_EQ(OR_POS, pBag->UseItem(p));
	ItemParam p1{ 1, 1, 2, 1 };
	EXPECT_EQ(OR_POS, pBag->UseItem(p));

	ItemParam p2{ 5, 1, 0, 1 };
	pBag->TestOutPut();
	EXPECT_EQ(OR_OK, pBag->UseItem(p2));

	pBag->TestOutPut();
}

TEST(BagBase, UseItemTidy998)
{
	BagBase::item_op_output_type vOut;
	BagModule oModule;
	BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);
	
    CreateItemParam cip;
    cip.m_nConfigId = 5;
    cip.m_nCount = 999;

    EXPECT_EQ(OR_OK, oModule.CreateItem(cip));
	
	ItemParam p2{ 5, 1, 0, 1 };

	pBag->TestOutPut();

	EXPECT_EQ(OR_OK, pBag->UseItem(p2));
	EXPECT_EQ(998, pBag->GetOverlaySize());
	pBag->TestOutPut();

	pBag->Tidy();
	pBag->TestOutPut();

}

TEST(BagBase, UseItemTidy99)
{
	BagModule oModule;
	BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);
	
	BagBase::item_op_output_type vOut;
    CreateItemParam cip;
    cip.m_nConfigId = 5;
    cip.m_nCount = 999;

    EXPECT_EQ(OR_OK, oModule.CreateItem(cip));
	

	ItemParam p3{ 5, 98, 0, 1 };
	ItemParam p4{ 5, 98, 1, 1 };

	EXPECT_EQ(OR_OK, pBag->UseItem(p3));
	EXPECT_EQ(OR_OK, pBag->UseItem(p4));

	pBag->TestOutPut();

	pBag->Tidy();
	pBag->TestOutPut();

	EXPECT_EQ(999 - 98 - 98, pBag->GetOverlaySize());
}

TEST(BagBase, UseItemTidyLock)
{
	
	BagModule oModule;
	BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);
    CreateItemParam cip;
    cip.m_nConfigId = 5;
	BagBase::item_op_output_type vOut;
    cip.m_nCount = 999;

    EXPECT_EQ(OR_OK, oModule.CreateItem(cip));
	

	ItemParam p3{ 5, 98, 9, 1, true};
	ItemParam p4{ 5, 98, 10, 1, true };

	EXPECT_EQ(OR_OK, pBag->SetLock(p3));
	EXPECT_EQ(OR_OK, pBag->SetLock(p4));

	pBag->TestOutPut();

	pBag->Tidy();
	pBag->TestOutPut();

	
}

TEST(BagBase, ItemMergeLock)
{
	BagBase::item_op_output_type vOut;
	BagModule oModule;
	BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);
    
    CreateItemParam cip;
    cip.m_nConfigId = 5;
    cip.m_nCount = 999;
    std::size_t merge_remain_size = 9;
    std::size_t stack = 99;
    std::size_t item_size = (cip.m_nCount / stack) + ((cip.m_nCount % stack) > 0 ? 1 : 0);
    EXPECT_EQ(OR_OK, oModule.CreateItem(cip));
	
	ItemParam p3{ 5, 98, 8, 1, true };
	ItemParam p4{ 5, 98, 9, 1, true };

	EXPECT_EQ(OR_OK, pBag->SetLock(p3));
	EXPECT_EQ(OR_OK, pBag->SetLock(p4));
    EXPECT_EQ(OR_BAG_ITEM_LOCAK, pBag->UseItem(p3));
    EXPECT_EQ(OR_BAG_ITEM_LOCAK, pBag->UseItem(p4));
    EXPECT_EQ(999, pBag->GetOverlaySize());
    EXPECT_EQ(11, pBag->GetItemSize());

    p3.m_vLock = false;
    pBag->SetLock(p3);
    p4.m_vLock = false;
    pBag->SetLock(p4);
	EXPECT_EQ(OR_OK, pBag->UseItem(p3));
	EXPECT_EQ(OR_OK, pBag->UseItem(p4));

	EXPECT_EQ(item_size, pBag->GetItemSize());

	pBag->TestOutPut();

	pBag->MergeItem(p3);
	pBag->TestOutPut();
	EXPECT_EQ(999 - 98 - 98, pBag->GetOverlaySize());
	EXPECT_EQ(merge_remain_size, pBag->GetItemSize());
    EXPECT_EQ(HumanBag::BagBase::g_nBagDefaultCapacity - merge_remain_size, pBag->GetEmptySize());

}



TEST(BagBase, TakeItem)
{
	BagModule oModule;
	BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);
	
	BagBase::item_op_output_type vOut;
    CreateItemParam cip;
    cip.m_nConfigId = 5;
    cip.m_nCount = 999;
	EXPECT_EQ(OR_OK, oModule.CreateItem(cip));
	

	ItemParam p{ 5, 0, 0, 1, true };
	BagBase::item_type pget;
	EXPECT_EQ(OR_OK, pBag->TakeItem(p, pget));
	pBag->Tidy();
	EXPECT_EQ(10, pBag->GetItemSize());

}

TEST(BagBase, TakeItemTempToBagEmptyBag)
{
	BagModule oModule;
	BagModule::bag_ptr_type pTempBag = oModule.GetBag(BagBase::E_BAG_TEMP_TYPE);
	BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);

	ItemFactory factory;
	BagBase::item_op_output_type vOut;
    CreateItemParam cip;
    cip.m_nConfigId = 5;
    cip.m_nCount = HumanBag::BagBase::g_nTempBagMaxSize * 99;
	EXPECT_EQ(OR_OK, pTempBag->AddItemForTest(cip));
	
	EXPECT_EQ(0, pBag->GetItemSize());
	EXPECT_EQ(HumanBag::BagBase::g_nTempBagMaxSize, pTempBag->GetItemSize());
	EXPECT_EQ(OR_OK, oModule.TakeAllItemFromTempBag());

	for (std::size_t i = 0; i < pBag->GetItemSize(); ++i)
	{
		EXPECT_FALSE(pBag->GetItem(i)->IsEmptyItem());
	}

	EXPECT_EQ(0, pTempBag->GetItemSize());
	EXPECT_EQ(HumanBag::BagBase::g_nTempBagMaxSize, pBag->GetItemSize());
}

TEST(BagBase, TakeItemTempToBagNotEnoughBag)
{
	BagModule oModule;
	BagModule::bag_ptr_type pTempBag = oModule.GetBag(BagBase::E_BAG_TEMP_TYPE);
	BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);
	ItemFactory factory;
	BagBase::item_op_output_type vOut;
    CreateItemParam cip;
    cip.m_nConfigId = 5;
    int32_t useSize = BagBase::g_nBagDefaultCapacity ;
    int32_t s = useSize * 99;
	for (int32_t i = 0; i < s; ++i)
	{
		EXPECT_EQ(OR_OK, pBag->AddItemForTest(cip));
	}
	for (int32_t i = 0; i < 999; ++i)
	{
		EXPECT_EQ(OR_OK, pTempBag->AddItemForTest(cip));
	}
	EXPECT_EQ(useSize, pBag->GetItemSize());
	EXPECT_EQ(11, pTempBag->GetItemSize());
	EXPECT_EQ(OR_BAG_FULL, oModule.TakeAllItemFromTempBag());
	EXPECT_EQ(useSize, pBag->GetItemSize());
	EXPECT_EQ(s, pBag->GetOverlaySize());
	EXPECT_EQ(11, pTempBag->GetItemSize());
	for (std::size_t i = 0; i < pBag->GetItemSize(); ++i)
	{
		EXPECT_FALSE(pBag->GetItem(i)->IsEmptyItem());
	}

}

void TestCantTakeError(int32_t nFromType, int32_t nToType )
{
	BagModule oModule;
	BagModule::bag_ptr_type pFromBag = oModule.GetBag(nFromType);
	BagModule::bag_ptr_type pToBag = oModule.GetBag(nToType);
	ItemFactory factory;
	BagBase::item_op_output_type vOut;
    CreateItemParam cip;
    cip.m_nConfigId = 5;
	for (int32_t i = 0; i < 999; ++i)
	{
		EXPECT_EQ(OR_OK, pFromBag->AddItemForTest(cip));
	}
	
	ItemParam p{ 5, 0, 0, 1, true };
	EXPECT_EQ(OR_BAG_CANT_TAKE_ERROR, oModule.TakeItem(nFromType, nToType, p));
	EXPECT_EQ(0, pToBag->GetItemSize());
	EXPECT_EQ(11, pFromBag->GetItemSize());
}

void SameTypeget(int32_t nType)
{
	BagModule oModule;
	BagModule::bag_ptr_type pFromBag = oModule.GetBag(nType);
	BagModule::bag_ptr_type pToBag = oModule.GetBag(nType);
	BagBase::item_op_output_type vOut;
	ItemFactory factory;
    CreateItemParam cip;
    cip.m_nConfigId = 5;
	for (int32_t i = 0; i < 999; ++i)
	{
		EXPECT_EQ(OR_OK, pFromBag->AddItemForTest( cip));
	}

	ItemParam p{ 5, 0, 0, 1, true };
	EXPECT_EQ(OR_BAG_SAMETYPE_TAKE_ERROR, oModule.TakeItem(nType, nType, p));
	EXPECT_EQ(11, pToBag->GetItemSize());
	EXPECT_EQ(11, pFromBag->GetItemSize());
}

TEST(BagBase, TempBagToStorage)
{
	TestCantTakeError(BagBase::E_BAG_TEMP_TYPE, BagBase::E_BAG_STORAGE_TYPE);
}

TEST(BagBase, StorageToTempBag)
{
	TestCantTakeError(BagBase::E_BAG_STORAGE_TYPE, BagBase::E_BAG_TEMP_TYPE);
}

TEST(BagBase, BagToTempBag)
{
	TestCantTakeError(BagBase::E_BAG_TYPE, BagBase::E_BAG_TEMP_TYPE);
}

void getOk(int32_t nFromType, int32_t nToType)
{
	BagModule oModule;
	BagModule::bag_ptr_type pFromBag = oModule.GetBag(nFromType);
	BagModule::bag_ptr_type pToBag = oModule.GetBag(nToType);
    CreateItemParam cip;
    cip.m_nConfigId = 5;
	ItemFactory factory;
	BagBase::item_op_output_type vOut;
	for (int32_t i = 0; i < 999; ++i)
	{
		EXPECT_EQ(OR_OK, pFromBag->AddItemForTest(cip));
	}

	ItemParam p{ 5, 1, 0, 1, true };
	EXPECT_EQ(OR_OK, oModule.TakeItem(nFromType, nToType, p));
	pFromBag->Tidy();
	EXPECT_EQ(1, pToBag->GetItemSize());
	EXPECT_EQ(10, pFromBag->GetItemSize());
}

void getOkNil(int32_t nFromType, int32_t nToType)
{
	BagModule oModule;
	BagModule::bag_ptr_type pFromBag = oModule.GetBag(nFromType);
	BagModule::bag_ptr_type pToBag = oModule.GetBag(nToType);

	ItemFactory factory;
	BagBase::item_op_output_type vOut;
    CreateItemParam cip;
    cip.m_nConfigId = 5;
	for (int32_t i = 0; i < 999; ++i)
	{
		EXPECT_EQ(OR_OK, pFromBag->AddItemForTest(cip));
	}

	for (int32_t i = 0; i < 999; ++i)
	{
		EXPECT_EQ(OR_OK, pToBag->AddItemForTest(cip));
	}
	ItemParam pUse{ 5, 99, 0, 1, true };
	EXPECT_EQ(OR_OK, pToBag->GetItem(0)->OnUse(pUse));
	EXPECT_EQ(5, pToBag->GetItem(0)->GetItemProtoElement()->id);
	EXPECT_EQ(900, pToBag->GetOverlaySize());
	ItemParam pFrom{ 5, 1, 0, 1, true };
	EXPECT_EQ(OR_OK, oModule.TakeItem(nFromType, nToType, pFrom));
	pFromBag->Tidy();
	EXPECT_EQ(999, pToBag->GetOverlaySize());
	EXPECT_EQ(5, pToBag->GetItem(0)->GetItemProtoElement()->id);
}

TEST(BagBase, BagToStorage)
{
	getOk(BagBase::E_BAG_TYPE, BagBase::E_BAG_STORAGE_TYPE);
}

TEST(BagBase, StorageToBag)
{
	getOk(BagBase::E_BAG_STORAGE_TYPE, BagBase::E_BAG_TYPE);
}

TEST(BagBase, TempBagToBag)
{
	getOk(BagBase::E_BAG_TEMP_TYPE, BagBase::E_BAG_TYPE);
}

TEST(BagBase, BagToStorageNil)
{
	getOkNil(BagBase::E_BAG_TYPE, BagBase::E_BAG_STORAGE_TYPE);
}

TEST(BagBase, StorageToBagNil)
{
	getOkNil(BagBase::E_BAG_STORAGE_TYPE, BagBase::E_BAG_TYPE);
}

TEST(BagBase, TempBagToBagNil)
{
	getOkNil(BagBase::E_BAG_TEMP_TYPE, BagBase::E_BAG_TYPE);
}

TEST(BagBase, getSameBagType)
{
	SameTypeget(BagBase::E_BAG_TYPE);
	SameTypeget(BagBase::E_BAG_STORAGE_TYPE);
	SameTypeget(BagBase::E_BAG_TEMP_TYPE);
}

TEST(BagBase, EquipSort)
{
	BagModule oModule;
	
	BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);

    CreateItemParam cip;
    cip.m_nConfigId = 5;
	BagBase::item_op_output_type vOut;
	for (int32_t i = 0; i < 999; ++i)
	{
        cip.m_nCount = 1;
		EXPECT_EQ(OR_OK, oModule.CreateItem(cip));
	}

}

TEST(BagBase, TakeItemRpc)
{
	BagModule oModule;
	BagModule::bag_ptr_type pFromBag = oModule.GetBag(BagBase::E_BAG_TYPE);
	BagModule::bag_ptr_type pToBag = oModule.GetBag(BagBase::E_BAG_STORAGE_TYPE);
    CreateItemParam cip;
    cip.m_nConfigId = 5;
	ItemFactory factory;
	BagBase::item_op_output_type vOut;
	for (int32_t i = 0; i < 999; ++i)
	{
		EXPECT_EQ(OR_OK, pFromBag->AddItemForTest(cip));
	}

	ItemParam p{ 5, 1, 0, 1, true };
	BagRpcTakeItemAsk rq;
	rq.set_frombagtype(BagBase::E_BAG_TYPE);
	rq.set_tobagtype(BagBase::E_BAG_STORAGE_TYPE);
	rq.set_frompos(0);
	BagRpcTakeItemReply reply;
	oModule.RpcTakeItem(rq, reply);
	EXPECT_EQ(OR_OK, reply.result());

	EXPECT_EQ(1, pToBag->GetItemSize());
	EXPECT_EQ(10, pFromBag->GetItemSize());

	oModule.RpcTakeItem(rq, reply);
	EXPECT_EQ(OR_BAG_NULL_ITEM, reply.result());

	rq.set_frompos(1);
	oModule.RpcTakeItem(rq, reply);
	EXPECT_EQ(OR_OK, reply.result());
	EXPECT_EQ(1, reply.fromitemobj().pos());
	EXPECT_EQ(1, reply.toitemobj().pos());

	pFromBag->Tidy();

	EXPECT_EQ(2, pToBag->GetItemSize());
	EXPECT_EQ(9, pFromBag->GetItemSize());

}

TEST(BagBase, TakeToAndTakeBack)
{
	BagModule oModule;
	BagModule::bag_ptr_type pFromBag = oModule.GetBag(BagBase::E_BAG_TYPE);
	BagModule::bag_ptr_type pToBag = oModule.GetBag(BagBase::E_BAG_STORAGE_TYPE);

    CreateItemParam cip;
    cip.m_nConfigId = 6;
	for (int32_t i = 0; i < 99; ++i)
	{
        EXPECT_EQ(OR_OK, oModule.CreateItem(cip));
	}

	ItemParam p{ 5, 1, 0, 1, true };
	//to
	BagRpcTakeItemAsk rq;
	rq.set_frombagtype(0);
	rq.set_tobagtype(1);
	rq.set_frompos(0);
	BagRpcTakeItemReply reply;
	oModule.RpcTakeItem(rq, reply);
	EXPECT_EQ(OR_OK, reply.result());

	EXPECT_EQ(1, pToBag->GetItemSize());
	EXPECT_EQ(0, pFromBag->GetItemSize());

	
	//from 
	rq.set_frombagtype(1);
	rq.set_tobagtype(0);
	rq.set_frompos(0);
	oModule.RpcTakeItem(rq, reply);
	EXPECT_EQ(OR_OK, reply.result());

	EXPECT_EQ(1, pFromBag->GetItemSize());
	EXPECT_EQ(0, pToBag->GetItemSize());
	EXPECT_EQ(99, pFromBag->GetItem(0)->GetOverlaySize());
	EXPECT_EQ(6, pFromBag->GetItem(0)->GetItemProtoElement()->id);
}

TEST(BagBase, TestOverlayItem)
{
    BagModule oModule;
    BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);
    CreateItemParam cip;
    cip.m_nConfigId = 5;
    ItemFactory factory;
    EXPECT_EQ(OR_OK, pBag->CanOverlayItem(5, 9));
    BagBase::item_op_output_type vOut;
    std::size_t sz = BagBase::g_nBagDefaultCapacity * 99;
    for (std::size_t i = 0; i <  sz; ++i)
    {
        oModule.CreateItem(cip);
    }
    EXPECT_EQ(OR_BAG_ENOUGH_CAPACITY_FULL, pBag->CanOverlayItem(5, 9));

    pBag->ClearBag();

    int32_t nRemainSize = 9;
    int32_t nTotalSize = BagBase::g_nBagDefaultCapacity * 99;
    int32_t notEnoughSize = nTotalSize - nRemainSize;
    for (int32_t i = 0; i <  notEnoughSize; ++i)
    {
        oModule.CreateItem(cip);
    }
    EXPECT_EQ(OR_OK, pBag->CanOverlayItem(5, 9));
    
    for (int32_t i = 0; i < 9; ++i)
    {
        EXPECT_EQ(OR_OK, pBag->AddItemForTest(cip));
        EXPECT_EQ(OR_BAG_ENOUGH_CAPACITY_FULL, pBag->CanOverlayItem(5, 9));
    }

    pBag->ClearBag();

    for (int32_t i = 0; i < nTotalSize; ++i)
    {
        oModule.CreateItem(cip);
    }
    ItemParam p{ 1, 8, 0, 1 };
    EXPECT_EQ(OR_OK, pBag->UseItem(p));
    EXPECT_EQ(OR_BAG_ENOUGH_CAPACITY_FULL, pBag->CanOverlayItem(5, 9));
    ItemParam p1{ 1, 1, 0, 1 };
    EXPECT_EQ(OR_OK, pBag->UseItem(p1));
    EXPECT_EQ(OR_OK, pBag->CanOverlayItem(5, 9));
    EXPECT_EQ(OR_BAG_ENOUGH_CAPACITY_FULL, pBag->CanOverlayItem(5, 10));

    pBag->ClearBag();

    for (int32_t i = 0; i < 99; ++i)
    {
        oModule.CreateItem(cip);
    }

    cip.m_nConfigId = 3;
    for (std::size_t i = 0; i < BagBase::g_nBagDefaultCapacity - 1; ++i)
    {
        oModule.CreateItem(cip);
    }
    i32_map_type o;
    o.emplace(3, 2);
    o.emplace(5, 5);
    EXPECT_EQ(OR_BAG_ENOUGH_CAPACITY_FULL, pBag->CanOverlayItem(o));
    pBag->TakeItem(1);
    EXPECT_EQ(OR_BAG_ENOUGH_CAPACITY_FULL, pBag->CanOverlayItem(o));
    pBag->TakeItem(2);
    EXPECT_EQ(OR_BAG_ENOUGH_CAPACITY_FULL, pBag->CanOverlayItem(o));


    ItemParam p2{ 5, 3, 0 };
    EXPECT_EQ(OR_OK, pBag->UseItem(p2));
    EXPECT_EQ(OR_BAG_ENOUGH_CAPACITY_FULL, pBag->CanOverlayItem(o));
    ItemParam p3{ 5, 2, 0 };
    EXPECT_EQ(OR_OK, pBag->UseItem(p3));
    EXPECT_EQ(OR_OK, pBag->CanOverlayItem(o));
}

TEST(BagBase, TestOverlayNullItem)
{
    BagModule oModule;
    BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);
    BagBase::item_op_output_type vOut;
    CreateItemParam cip;
    cip.m_nConfigId = 5;
    for (std::size_t i = 0; i < (BagBase::g_nBagDefaultCapacity ) * 99; ++i)
    {
        oModule.CreateItem(cip);
    }
    EXPECT_EQ(OR_BAG_ENOUGH_CAPACITY_FULL, pBag->CanOverlayItem(5, 9));

    ItemParam p{ 1, 99, 0, 1 };
    EXPECT_EQ(OR_OK, pBag->UseItem(p));

    EXPECT_EQ(OR_OK, pBag->CanOverlayItem(5, 9));

}

TEST(BagBase, CompoundItem)
{
    /*ItemLogic::ItemModule im;
    BagModule::bag_ptr_type pBag = im.GetBagModule()->GetBag(BagBase::E_BAG_TYPE);
    ItemFactory factory;
    EXPECT_EQ(OR_COMPOUND_SIZE, im.CompoundItem(3));
    BagBase::item_op_output_type vOut;
    for (int32_t i = 0; i < 2; ++i)
    {
        im.GetBagModule()->CreateItem(4);
    }
    for (int32_t i = 0; i < 5; ++i)
    {
        im.GetBagModule()->CreateItem(5);
    }
    for (int32_t i = 0; i < 27; ++i)
    {
        im.GetBagModule()->CreateItem(11);
    }
    BagBase::item_type pItem = factory.CreateItem(11);
    EXPECT_EQ(OR_BAG_FULL, pBag->AddItemForTest(pItem, p));
    EXPECT_EQ(OR_OK , im.CompoundItem(3));
    EXPECT_EQ(3, pBag->GetItem(0)->GetItemProtoElement()->id);
    EXPECT_EQ(28, pBag->GetItemSize());*/
}


TEST(BagBase, PlaceholdersSize)
{
    ItemData pb;
    pb.set_configid(1);
    pb.set_overlaycount(9);

    Item i1(pb);
    EXPECT_EQ(1, i1.BeforeAddToBagPlaceholdersSize());

    pb.set_overlaycount(99);
    Item i2(pb);
    EXPECT_EQ(1, i2.BeforeAddToBagPlaceholdersSize());

    pb.set_overlaycount(101);
    Item i3(pb);
    EXPECT_EQ(2, i3.BeforeAddToBagPlaceholdersSize());

    pb.set_overlaycount(198);
    Item i4(pb);
    EXPECT_EQ(2, i4.BeforeAddToBagPlaceholdersSize());


    pb.set_overlaycount(0);
    Item i5(pb);
    EXPECT_EQ(0, i5.BeforeAddToBagPlaceholdersSize());
}



TEST(BagBase, DisassembleItem)
{
    ItemLogic::ItemModule im;
    
    im.ResetBagModule();
    BagModule::bag_ptr_type pBag = im.GetBagModule()->GetBag(BagBase::E_BAG_TYPE);
    ItemParam p;
    p.m_nItemPos = 0;
    CreateItemParam cip;
    cip.m_nConfigId = 4;
    cip.m_bIsBindItem = false;
    EXPECT_EQ(OR_POS, im.DisassembleItem(p));
    for (int32_t i = 0; i < 1; ++i)
    {
        im.GetBagModule()->CreateItem(cip);
    }
    p.m_nUseSize = pBag->GetItem(0)->GetOverlaySize();
    EXPECT_EQ(OR_OK, im.DisassembleItem(p));
   
    EXPECT_EQ(5, pBag->GetItem(0)->GetItemProtoElement()->id);
    EXPECT_EQ(5, pBag->GetItem(0)->GetOverlaySize());
    EXPECT_EQ(3, pBag->GetItem(1)->GetItemProtoElement()->id);
    EXPECT_EQ(1, pBag->GetItem(1)->GetOverlaySize());
    EXPECT_EQ(3, pBag->GetItem(2)->GetItemProtoElement()->id);
    EXPECT_EQ(1, pBag->GetItem(2)->GetOverlaySize());
    EXPECT_EQ(3, pBag->GetItemSize());
}

TEST(BagBase, DisassembleItemOverlaySize)
{
    ItemLogic::ItemModule im;

    im.ResetBagModule();
    BagModule::bag_ptr_type pBag = im.GetBagModule()->GetBag(BagBase::E_BAG_TYPE);
    CreateItemParam cip;
    cip.m_bIsBindItem = false;
    cip.m_nConfigId = 6;
    cip.m_nCount = 7;

    im.GetBagModule()->CreateItem(cip);
    ItemParam p;
    p.m_nItemPos = 0;
    p.m_nUseSize = pBag->GetItem(0)->GetOverlaySize();
    EXPECT_EQ(OR_OK, im.DisassembleItem(p));

    EXPECT_EQ(3, pBag->GetItem(0)->GetItemProtoElement()->id);
    EXPECT_EQ(28, pBag->GetItemSize());
    
}



TEST(BagBase, DisassembleItemSelfOverlaySize)
{
    ItemLogic::ItemModule im;

    im.ResetBagModule();
    BagModule::bag_ptr_type pBag = im.GetBagModule()->GetBag(BagBase::E_BAG_TYPE);
    CreateItemParam cip;
    cip.m_nConfigId = 6;
    cip.m_nCount = 7;
    cip.m_bIsBindItem = false;
    im.GetBagModule()->CreateItem(cip);
    ItemParam p;
    p.m_nItemPos = 0;
    p.m_nUseSize = 1;
    EXPECT_EQ(OR_OK, im.DisassembleItem(p));

    EXPECT_EQ(6, pBag->GetItem(0)->GetItemProtoElement()->id);
    EXPECT_EQ(3, pBag->GetItem(1)->GetItemProtoElement()->id);
    EXPECT_EQ(3, pBag->GetItem(2)->GetItemProtoElement()->id);
    EXPECT_EQ(3, pBag->GetItem(3)->GetItemProtoElement()->id);
    EXPECT_EQ(1, pBag->GetItem(3)->GetOverlaySize());
    EXPECT_EQ(5, pBag->GetItemSize());

}

TEST(BagBase, DisassembleAllItem)
{
    ItemLogic::ItemModule im;

    im.ResetBagModule();
    BagModule::bag_ptr_type pBag = im.GetBagModule()->GetBag(BagBase::E_BAG_TYPE);
    EXPECT_EQ(OR_OK, im.GetBagModule()->SaleBreakAllItem(0));
    CreateItemParam cip;
    cip.m_bIsBindItem = false;
    cip.m_nConfigId = 11;

    for (int32_t i = 0; i < 15; ++i)
    {
        im.GetBagModule()->CreateItem(cip);
    }
    cip.m_nConfigId = 20;
    for (int32_t i = 0; i < 15; ++i)
    {
        im.GetBagModule()->CreateItem(cip);
    }
    EXPECT_EQ(OR_OK, im.GetBagModule()->SaleBreakAllItem(0));
    
    EXPECT_EQ(15, pBag->GetItemSize());
    EXPECT_EQ(OR_OK, im.GetBagModule()->SaleBreakAllItem(1));
    EXPECT_EQ(0, pBag->GetItemSize());
    cip.m_nConfigId = 40;
    for (int32_t i = 0; i < 15; ++i)
    {
        im.GetBagModule()->CreateItem(cip);
    }
    
    EXPECT_EQ(OR_OK, im.GetBagModule()->SaleBreakAllItem(2));
    EXPECT_EQ(1, pBag->GetItemSize());
    EXPECT_EQ(5, pBag->GetItem(0)->GetItemProtoElement()->id);
    EXPECT_EQ(45, pBag->GetItem(0)->GetOverlaySize());
}


TEST(BagBase, ItemAutoOperato)
{
    ItemLogic::ItemModule im;

    im.ResetBagModule();
    ItemAutoOperatorData pb;
    BagModule::bag_ptr_type pBag = im.GetBagModule()->GetBag(BagBase::E_BAG_TYPE);
    EXPECT_EQ(OR_POS, im.GetBagModule()->SetItemAutoOperatorData(pb));
    pb.add_autosaledisassemble();
    pb.add_autosaledisassemble();
    pb.add_autosaledisassemble();
    EXPECT_EQ(OR_QULITY_SET_ERROR, im.GetBagModule()->SetItemAutoOperatorData(pb));
    pb.mutable_autosaledisassemble(0)->set_qulity(1);
    EXPECT_EQ(OR_QULITY_SET_ERROR, im.GetBagModule()->SetItemAutoOperatorData(pb));
    pb.mutable_autosaledisassemble(0)->set_qulity(0);
    pb.mutable_autosaledisassemble(0)->set_isresolve(1);
    pb.mutable_autosaledisassemble(1)->set_qulity(1);
    pb.mutable_autosaledisassemble(1)->set_isresolve(1);
    pb.mutable_autosaledisassemble(2)->set_qulity(2);
    pb.mutable_autosaledisassemble(2)->set_isresolve(1);

    EXPECT_EQ(OR_OK, im.GetBagModule()->SetItemAutoOperatorData(pb));
    CreateItemParam cip;
    cip.m_nConfigId = 4;
    for (int32_t i = 0; i < 29; ++i)
    {
        im.GetBagModule()->CreateItem(cip);
    }
    cip.m_nConfigId = 70;
    EXPECT_EQ(OR_OK, im.GetBagModule()->CreateItem(cip));
    EXPECT_EQ(4, pBag->GetItem(5)->GetItemProtoElement()->id);
    EXPECT_EQ(1, pBag->GetItem(0)->GetOverlaySize());
}

//白色品质——执行出售逻辑，按照道具的售价在其获得时将其自动出售（只限装备）
//绿色品质——执行出售逻辑，按照道具的售价在其获得时将其自动出售（只限装备）
//蓝色品质——执行分解逻辑，根据分解标签决定其是否可以被自动分解（只限装备）

TEST(BagBase, ItemAutoOperato1)
{
    ItemLogic::ItemModule im;
    CreateItemParam cip;
    im.ResetBagModule();
    ItemAutoOperatorData pb;
    BagModule::bag_ptr_type pBag = im.GetBagModule()->GetBag(BagBase::E_BAG_TYPE);

    pb.add_autosaledisassemble();
    pb.add_autosaledisassemble();
    pb.add_autosaledisassemble();
    pb.mutable_autosaledisassemble(0)->set_qulity(0);
    pb.mutable_autosaledisassemble(0)->set_isresolve(1);
    pb.mutable_autosaledisassemble(1)->set_qulity(1);
    pb.mutable_autosaledisassemble(1)->set_isresolve(1);
    pb.mutable_autosaledisassemble(2)->set_qulity(2);
    pb.mutable_autosaledisassemble(2)->set_isresolve(1);

    EXPECT_EQ(OR_OK, im.GetBagModule()->SetItemAutoOperatorData(pb));
    cip.m_nConfigId = 11;
    EXPECT_EQ(OR_OK, im.GetBagModule()->CreateItem(cip));
    EXPECT_EQ(0, pBag->GetItemSize());
    cip.m_nConfigId = 40;
    EXPECT_EQ(OR_OK, im.GetBagModule()->CreateItem(cip));
    EXPECT_EQ(1, pBag->GetItemSize());
}

TEST(BagBase, TestItemCurrency)
{
    ItemLogic::ItemModule im;
    CreateItemParam cip;
    im.ResetBagModule();
    ItemAutoOperatorData pb;
    BagModule::bag_ptr_type pBag = im.GetBagModule()->GetBag(BagBase::E_BAG_TYPE);
    cip.m_nConfigId = 1;
    im.GetBagModule()->CreateItem(cip);
    EXPECT_EQ(0, pBag->GetItemSize());
}


int32_t GetOccuption0()
{
    return 1;
}

int32_t GetOccuption1()
{
    return 2;
}

int32_t GetOccuption2()
{
    return 3;
}


int32_t GetOccuption4()
{
    return 5;
}

int32_t GetOccuption5()
{
    return 6;
}


TEST(BagBase, TestTransferId)
{
    ItemLogic::ItemModule im;

    im.ResetBagModule();
    ItemAutoOperatorData pb;
    BagModule::bag_ptr_type pBag = im.GetBagModule()->GetBag(BagBase::E_BAG_TYPE);
    im.GetBagModule()->SetGetOccupationCB(std::bind(GetOccuption0));
    CreateItemParam p;
    p.m_nConfigId = 12;
    p.m_bIsTransfer = true;
    im.GetBagModule()->CreateItem(p);
    const ItemProtoElement * pItemProtoElement = ItemProtoTable::Instance().GetElement(12);
    EXPECT_EQ(pItemProtoElement->transfer_id[0], pBag->GetItem(0)->GetItemProtoElement()->id);

    p.m_nConfigId = 12;
    im.GetBagModule()->SetGetOccupationCB(std::bind(GetOccuption1));
    im.GetBagModule()->CreateItem(p);
    EXPECT_EQ(pItemProtoElement->transfer_id[1], pBag->GetItem(1)->GetItemProtoElement()->id);
    p.m_nConfigId = 12;
    im.GetBagModule()->SetGetOccupationCB(std::bind(GetOccuption2));
    im.GetBagModule()->CreateItem(p);
    EXPECT_EQ(pItemProtoElement->transfer_id[2], pBag->GetItem(2)->GetItemProtoElement()->id);

    p.m_nConfigId = 12;
    p.m_bIsTransfer = false;
    im.GetBagModule()->SetGetOccupationCB(std::bind(GetOccuption0));
    im.GetBagModule()->CreateItem(p);
    EXPECT_EQ(p.m_nConfigId, pBag->GetItem(3)->GetItemProtoElement()->id);
}

TEST(BagBase, SaleItem)
{
    ItemLogic::ItemModule im;

    im.ResetBagModule();
    ItemAutoOperatorData pb;

    CreateItemParam p;
    ItemParam dp;
    dp.m_nItemPos = 0;
    dp.m_nPlayerLevel = 100;
    p.m_nConfigId = 2;
    im.GetBagModule()->CreateItem(p);
    im.GetBagModule()->BreakItem(dp);
    EXPECT_EQ(0, im.GetBagModule()->GetBag(BagBase::E_BAG_TYPE)->GetItemSize());

    pb.add_autosaledisassemble();
    pb.add_autosaledisassemble();
    pb.add_autosaledisassemble();
    pb.mutable_autosaledisassemble(0)->set_qulity(0);
    pb.mutable_autosaledisassemble(0)->set_isresolve(1);
    pb.mutable_autosaledisassemble(1)->set_qulity(1);
    pb.mutable_autosaledisassemble(1)->set_isresolve(1);
    pb.mutable_autosaledisassemble(2)->set_qulity(2);
    pb.mutable_autosaledisassemble(2)->set_isresolve(1);

    EXPECT_EQ(OR_OK, im.GetBagModule()->SetItemAutoOperatorData(pb));
    im.GetBagModule()->CreateItem(p);
    EXPECT_EQ(0, im.GetBagModule()->GetBag(BagBase::E_BAG_TYPE)->GetItemSize());
}

TEST(BagBase, TestOverlayItemBind)
{
    BagModule oModule;
    BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);
    CreateItemParam cip;
    cip.m_nConfigId = 5;
    cip.m_vBind = 0;
    BagBase::item_op_output_type vOut;
    for (int32_t i = 0; i < 2; ++i)
    {
        oModule.CreateItem(cip);
    }
    cip.m_vBind = 1;
    for (int32_t i = 0; i < 2; ++i)
    {
        oModule.CreateItem(cip);
    }
    pBag->Tidy();
    EXPECT_EQ(2, pBag->GetItemSize());

}

TEST(BagBase, TestOverlayItemBind1)
{
    BagModule oModule;
    BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);
    CreateItemParam cip;
    cip.m_nConfigId = 5;
    cip.m_vBind = 0;
    BagBase::item_op_output_type vOut;
    for (int32_t i = 0; i < 2; ++i)
    {
        oModule.CreateItem(cip);
    }
    cip.m_vBind = 1;
    for (int32_t i = 0; i < 2; ++i)
    {
        oModule.CreateItem(cip);
    }
    pBag->Tidy();
    EXPECT_EQ(2, pBag->GetItemSize());

}

TEST(BagBase, TestBindLockSale)
{
    BagModule oModule;
    BagModule::bag_ptr_type pBag = oModule.GetBag(BagBase::E_BAG_TYPE);
    CreateItemParam cip;
    cip.m_nConfigId = 5;
    cip.m_bIsBindItem = true;
    BagBase::item_op_output_type vOut;
    for (int32_t i = 0; i < 2; ++i)
    {
        oModule.CreateItem(cip);
    }
    cip.m_bIsBindItem = false;
    cip.m_vLock = true;
    for (int32_t i = 0; i < 2; ++i)
    {
        oModule.CreateItem(cip);
    }
    oModule.SaleBreakAllItem(1);
    ItemParam sp;
    sp.m_nItemPos = 0;
    pBag->SaleItem(sp);
    sp.m_nItemPos = 1;
    pBag->SaleItem(sp);
    EXPECT_EQ(2, pBag->GetItemSize());
}



//int main(int argc, char **argv)
//{
//	ModuleConfig::Instance().Initialize();
//	testing::InitGoogleTest(&argc, argv);
//	//while (true)
//	{
//		//RUN_ALL_TESTS();
//	}
//	return RUN_ALL_TESTS();
//}

int main(int argc, char **argv)
{
    ModuleConfig::Instance().Initialize();
    testing::InitGoogleTest(&argc, argv);
   /* while (true)
    {
        RUN_ALL_TESTS();
    }*/
    return RUN_ALL_TESTS();
}