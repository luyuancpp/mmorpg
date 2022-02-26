#include "Bag.h"

#ifdef __TEST__
#include <iostream>
#include <random>

#endif // !__TEST__

#include "GenCode/GameDefine_Result.h"
#include "GenCode/Bag/BagModule.h"
#include "GenCode/Config/MixInfoCfg.h"
#include "GenCode/Config/GlobalCfg.h"
#include "GenCode/Config/ItemDissolveFilterCfg.h"
#include "GenCode/Config/ItemProtoCfg.h"

#include "GenCode/ItemOperation/ItemOperationRpc.pb.h"

#include "GameEnum.h"
#include "GameStruct/GameDefine.h"
#include "MessageUtils.h"
#include "Game/DK_TimeManager.h"
#include "GameStruct/GameStruct_Resource.h"
#include "CommonLogic/FunctionSwitch/FunctionSwtich.h"
#include "CommonLogic/EventStruct/BagEventStruct.h"
#include "CommonLogic/EventStruct/CommonEventStruct.h"
#include "CommonLogic/GameEvent/GameEvent.h"
#include "GameDB.pb.h"

namespace HumanBag
{
    BagBase::item_op_output_type BagBase::kEmptyNewList;

    std::size_t BagBase::g_nBagMaxSize = 120;
    std::size_t BagBase::g_nTempBagMaxSize = 12;
    std::size_t BagBase::g_nBagDefaultCapacity = 60;
    std::size_t BagBase::g_nBagDeblockingSize = 5;
    std::size_t BagBase::g_nStorageInitMaxSize = 60;
    std::size_t BagBase::g_nStorageBagMaxSize = 120;
    std::size_t BagBase::g_nPetBagMaxSize = 100;
    std::size_t BagBase::g_nPetBagDeblockingSize = 5;
    std::size_t BagBase::g_nItemMaxSize = 1000;

    class TempBag : public BagBase
    {
    public:

        TempBag(int32_t nBagType)
            : BagBase(nBagType)
        {
            m_nCurCapacity = g_nTempBagMaxSize;
        }

        virtual int32_t AddItem(CreateItemParam & param, CreateOutItemParam& outparam)override
        {
            i32_map_type v;
            if (!param.m_vItemCountPairs.empty())
            {
                for (i32_v_type::const_iterator it = param.m_vItemCountPairs.begin(); it != param.m_vItemCountPairs.end(); )
                {
                    int32_t nItemId = *it;
                    ++it;
                    if (it == param.m_vItemCountPairs.end())
                    {
                        break;
                    }
                    v.emplace(nItemId, *it);
                }
            }
            else
            {
                v.emplace(param.m_nConfigId, param.m_nCount);
            }
            if (OR_OK != CanOverlayItem(v, param) && !m_vItemList.empty())
            {
                item_type pItem = (*m_vItemList.begin());
                ItemParam oParam;
                oParam.m_nUseSize = pItem->GetOverlaySize();
                oParam.m_nClassId = EconomisClassEnum::E_Bag;
                oParam.m_nWayId = EconomisClassEnum::E_TempBag_Del;
                pItem->OnRemove(oParam);
                m_vItemList.erase(m_vItemList.begin());
                ResizeGrid();
                OnAllChange();
            }
            return BagBase::AddItem(param, outparam);
        }

        virtual void ForwardItems()override
        {
            item_list_type empty_item_list;
            for (std::size_t i = 0; i < m_vItemList.size(); ++i)
            {
                item_type p_item = m_vItemList[i];
                if (nullptr == p_item)
                {
                    p_item = CreateNullItem();
                    continue;
                }
                if (p_item->IsEmptyItem())
                {
                    empty_item_list.push_back(p_item);
                }
                else
                {
                    break;
                }
            }

            for (auto && it : empty_item_list)
            {
                m_vItemList.erase(m_vItemList.begin());
            }
            ResizeGrid();
            ClearChangeItem();
            OnAllChange();
        }

        virtual int32_t GetFullError()override
        {
            return OR_OK;
        }

        virtual bool IsFull()override
        {
            return GetItemSize() >= g_nTempBagMaxSize;
        }

        virtual int32_t Deblocking()override
        {
            return OR_BAG_CANT_DEBLOCKING;
        }
    };

    class StorageBag : public BagBase
    {
    public:
        StorageBag(int32_t nBagType)
            : BagBase(nBagType)
        {
            m_nCurCapacity = g_nStorageInitMaxSize;
        }

        virtual int32_t Deblocking()override
        {
            if (m_nCurCapacity >= g_nStorageBagMaxSize)
            {
                return OR_BAG_CAPACITY_FULL;
            }
            m_nCurCapacity += g_nBagDeblockingSize;
            ResizeGrid();
            return OR_OK;
        }

        int32_t GetFullError() override
        {
            if (IsFull())
            {
                return OR_STORAGE_BAG_FULL;
            }
            return OR_OK;
        }
    };

    class Bag : public BagBase
    {
    public:
        using BagBase::BagBase;

        virtual int32_t OnUseItem(ItemParam & p)override
        {
            OR_CHECK_RESULT_RETURN_RET(CheckClientPos(p));
            item_type  pItem = m_vItemList[p.m_nItemPos];
            OR_CHECK_RESULT_RETURN_RET(pItem->CheckUse(p));
            OR_CHECK_RESULT_RETURN_RET(pItem->OnUse(p));
            return OR_OK;
        }

        virtual int32_t SaleItem(ItemParam &p)override
        {
            OR_CHECK_RESULT_RETURN_RET(CheckClientPos(p));
            item_type  pItem = m_vItemList[p.m_nItemPos];
            OR_CHECK_RESULT_RETURN_RET(pItem->CheckSale(p));
            OR_CHECK_RESULT_RETURN_RET(pItem->OnSale(p));

            return OR_OK;
        }

        virtual int32_t SetLock(ItemParam & p)override
        {
            OR_CHECK_RESULT_RETURN_RET(CheckClientPos(p));
            item_type  pItem = m_vItemList[p.m_nItemPos];
            pItem->SetLock(p.m_vLock);
            OnItemChange(p);
            return OR_OK;
        }

        virtual int32_t SetBind(ItemParam & p)override
        {
            OR_CHECK_RESULT_RETURN_RET(CheckClientPos(p));
            item_type  pItem = m_vItemList[p.m_nItemPos];
            pItem->SetBind(p.m_bBind);
            return OR_OK;
        }

        virtual int32_t Deblocking()override
        {
            if (m_nCurCapacity >= g_nBagMaxSize)
            {
                return OR_BAG_CAPACITY_FULL;
            }
            m_nCurCapacity += g_nBagDeblockingSize;
            ResizeGrid();
            return OR_OK;
        }
    };

    class PetBag :public Bag
    {
    public:
        using Bag::Bag;
        PetBag(int32_t nBagType)
            : Bag(nBagType)
        {
            m_nCurCapacity = g_nPetBagMaxSize;
        }
        //完全继承bag
        virtual int32_t OnUseItem(ItemParam & p)override
        {
            OR_CHECK_RESULT_RETURN_RET(CheckClientPos(p));
            item_type  pItem = m_vItemList[p.m_nItemPos];
            OR_CHECK_RESULT_RETURN_RET(pItem->CheckUse(p));
            OR_CHECK_RESULT_RETURN_RET(pItem->OnUse(p));
            OnItemChangeFromGuid(pItem->GetGuid());
            pItem = m_vItemList[p.m_nItemPos];
            OnItemChangeFromGuid(pItem->GetGuid());
            return OR_OK;
        }

        virtual int32_t SwapItem(int32_t nPos, item_type  &  newItem)override
        {
            ItemParam p;
            p.m_nItemPos = nPos;
            if (p.m_nItemPos >= (int32_t)m_vItemList.size())
            {
                return OR_POS;
            }
            if (p.m_nItemPos < 0)
            {
                bool bFind = false;
                for (std::size_t i = 0; i < m_vItemList.size(); ++i)
                {
                    if (nullptr == m_vItemList[i])
                    {
                        m_vItemList[i] = CreateNullItem();
                        continue;
                    }

                    if (m_vItemList[i]->IsEmptyItem())
                    {
                        p.m_nItemPos = i;
                        bFind = true;
                        break;
                    }
                }
                if (!bFind)
                {
                    OR_CHECK_RESULT_RETURN_RET(GetFullError());
                    p.m_nItemPos = GetItemSize();
                }
            }

            if (p.m_nItemPos < 0 || (std::size_t)p.m_nItemPos >= m_vItemList.size())
            {
                return OR_POS;
            }

            item_type & pItem = m_vItemList[p.m_nItemPos];
            pItem.swap(newItem);
            InitItemCallBack(pItem);
            OnItemChange(p);
            return OR_OK;
        }

        virtual void OnNewDayCome() override
        {
            for (auto& pItem : m_vItemList)
            {
                if (nullptr != pItem && !pItem->IsEmptyItem())
                {
                    pItem->OnNewDayCome();
                }
            }
        }

        virtual int32_t Deblocking() override
        {
            if (m_nCurCapacity >= g_nPetBagMaxSize)
            {
                return OR_BAG_CAPACITY_FULL;
            }
            m_nCurCapacity += g_nPetBagDeblockingSize;
            ResizeGrid();
            return OR_OK;
        }

        int32_t GetFullError() override
        {
            if (IsFull())
            {
                return OR_BAG_PET_BAG_FULL;
            }
            return OR_OK;
        }

        int32_t CanOverlayItemFullErro()override
        {
            return OR_BAG_PET_CAPYCITY_NOT_ENOUGH;
        }
    };

    BagBase::BagBase(int32_t nBagType)
        : m_nCurCapacity(g_nBagDefaultCapacity),
        m_nBagType(nBagType)
    {
    }

    int32_t BagBase::AddItem(CreateItemParam & param, CreateOutItemParam& outparam)
    {
        if (ItemFactory::AddCurrency(param))
        {
            return OR_OK;
        }
        auto p_ele = ItemProtoTable::Instance().GetElement(param.m_nConfigId);
        if (nullptr == p_ele)
        {
            return OR_TABLE_INDEX;
        }
        if (param.m_nCount <= 0)
        {
            return OR_TABLE_INDEX;
        }
        if (p_ele->stack > 1)
        {
            //优先叠加不空的,需求
            for (std::size_t nPos = 0; nPos < m_vItemList.size(); ++nPos)
            {
                auto& p_it_item = m_vItemList[nPos];
                if (nullptr == p_it_item)
                {
                    p_it_item = CreateNullItem();
                }
                if (nullptr == p_it_item)
                {
                    continue;
                }
                if (p_it_item->IsEmptyItem())
                {
                    continue;
                }

                if (OR_OK != CanOverlayItem(p_it_item, param))
                {
                    continue;
                }
                int32_t nMaxOverlaySize = p_it_item->GetItemProtoElement()->stack;
                if (p_it_item->GetOverlaySize() >= nMaxOverlaySize)
                {
                    continue;
                }
                if (p_it_item->GetOverlaySize() + param.m_nCount <= nMaxOverlaySize)
                {
                    p_it_item->AddOverlaySize(param.m_nCount, param);
                    OnAddOverlaydItemSucces(param, outparam, p_it_item, param.m_nCount);
                    param.m_nCount = 0;
                }
                else
                {
                    int32_t os = nMaxOverlaySize - p_it_item->GetOverlaySize();
                    p_it_item->AddOverlaySize(os, param);
                    param.m_nCount -= os;
                    OnAddOverlaydItemSucces(param, outparam, p_it_item, os);
                }

                SetChangeItemPb(p_it_item, nPos);
                if (param.m_nCount <= 0)
                {
                    return OR_OK;
                }
            }
        }

        ResizeGrid();

        if (IsFull())
        {
            OR_CHECK_RESULT_RETURN_RET(GetFullError());
        }
        //再叠加空的,需求
        for (std::size_t nPos = 0; nPos < m_vItemList.size(); ++nPos)
        {
            auto& p_it_item = m_vItemList[nPos];
            if (nullptr == p_it_item)
            {
                p_it_item = CreateNullItem();
            }
            if (nullptr == p_it_item)
            {
                continue;
            }
            if (!p_it_item->IsEmptyItem())
            {
                continue;
            }
            CreateItemParam p = param;
            if (param.m_nCount > p_ele->stack)
            {
                p.m_nCount = p_ele->stack;
            }
            else
            {
                p.m_nCount = param.m_nCount;
            }
            auto new_item = FillItem(p);
            if (nullptr == new_item || new_item->IsEmptyItem())
            {
                return OR_OK;
            }
            p_it_item = new_item;
            param.m_nCount -= p.m_nCount;
            p_it_item->AddRedPoint(param);
            SetChangeItemPb(p_it_item, nPos);
            OnAddNewGridItemSucces(param, outparam, p_it_item);
            if (param.m_nCount <= 0)
            {
                return OR_OK;
            }
        }
        OR_CHECK_RESULT_RETURN_RET(GetFullError());
        if (!(param.m_nCount <= 0))
        {
            //param = old_param;
            return OR_BAG_CANT_FULL_OVERLAYITEM;
        }
        return OR_OK;
    }

    int32_t BagBase::AddItemForTest(CreateItemParam & inparam)
    {
		CreateOutItemParam outparam;
        CreateItemParam cip = inparam;
        outparam.m_ItemCB = inparam.m_ItemCB;
        return AddItem(cip, outparam);
    }

    int32_t BagBase::AddItemFromOtherBag(item_type& newItem)
    {
        if (nullptr == newItem || newItem->IsEmptyItem())
        {
            return OR_EMPTY_ITEM;
        }
        CreateItemParam param;
        param.nClassId = E_Bag;
        param.nWayId = E_Bag_TakeItem;
        if (newItem->GetItemProtoElement()->stack > 1)
        {
            //优先叠加不空的,需求
            for (std::size_t nPos = 0; nPos < m_vItemList.size(); ++nPos)
            {
                auto& p_it_item = m_vItemList[nPos];
                if (nullptr == p_it_item)
                {
                    p_it_item = CreateNullItem();
                }
                if (nullptr == p_it_item)
                {
                    continue;
                }
                if (p_it_item->IsEmptyItem())
                {
                    continue;
                }

                if (OR_OK != CanOverlayItem(p_it_item, newItem))
                {
                    continue;
                }
                int32_t nMaxOverlaySize = p_it_item->GetItemProtoElement()->stack;
                if (p_it_item->GetOverlaySize() >= nMaxOverlaySize)
                {
                    continue;
                }
                if (p_it_item->GetOverlaySize() + newItem->GetOverlaySize() <= nMaxOverlaySize)
                {
                    p_it_item->AddOverlaySize(newItem->GetOverlaySize(), param);
                    newItem->DelOverlaySize(newItem->GetOverlaySize());
                }
                else
                {
                    int32_t os = nMaxOverlaySize - p_it_item->GetOverlaySize();
                    p_it_item->AddOverlaySize(os, param);
                    newItem->DelOverlaySize(os);
                }

                SetChangeItemPb(p_it_item, nPos);

                if (nullptr == newItem || newItem->IsEmptyItem())
                {
                    return OR_OK;
                }
            }
        }

        ResizeGrid();

        if (IsFull())
        {
            OR_CHECK_RESULT_RETURN_RET(GetFullError());
        }
        //再叠加空的,需求
        for (std::size_t nPos = 0; nPos < m_vItemList.size(); ++nPos)
        {
            auto& p_it_item = m_vItemList[nPos];
            if (nullptr == p_it_item)
            {
                p_it_item = CreateNullItem();
            }
            if (nullptr == p_it_item)
            {
                continue;
            }
            if (!p_it_item->IsEmptyItem())
            {
                continue;
            }
            if (newItem->GetOverlaySize() > newItem->GetItemProtoElement()->stack)
            {
                CreateItemParam p;
                p.m_nCount = newItem->GetItemProtoElement()->stack;
                p.m_nConfigId = newItem->GetItemProtoElement()->id;
                p.nClassId = EconomisClassEnum::E_Bag;
                p.nWayId = EconomisClassEnum::E_Bag_Add;
                p.m_vBind = newItem->GetBindRule();
                p.m_vLock = newItem->GetLock();
                p.m_pItemData = nullptr;
                p.m_nCD = newItem->GetSaleCd();
                p_it_item = FillItem(p);
                if (nullptr != p_it_item)
                {
                    newItem->DelOverlaySize(p.m_nCount);
                }
            }
            else
            {
                InitItemCallBack(newItem);
                std::swap(p_it_item, newItem);
            }
            p_it_item->AddRedPoint(param);

            SetChangeItemPb(p_it_item, nPos);
            if (nullptr == newItem || newItem->IsEmptyItem())
            {
                return OR_OK;
            }
        }

        OR_CHECK_RESULT_RETURN_RET(GetFullError());

        if (!(NULL == newItem || newItem->IsEmptyItem()))
        {
            return OR_BAG_CANT_FULL_OVERLAYITEM;
        }
        return OR_OK;
    }

    void BagBase::ResizeGrid()
    {
        while (m_nCurCapacity < g_nItemMaxSize &&
            m_vItemList.size() < GetCurCapacity() &&
            m_vItemList.size() < g_nItemMaxSize)
        {
            m_vItemList.push_back(CreateNullItem());
        }
    }

    int32_t BagBase::SwapItem(int32_t nPos, item_type  &  newItem)
    {
        ItemParam p;
        p.m_nItemPos = nPos;
        if (p.m_nItemPos >= (int32_t)m_vItemList.size())
        {
            return OR_POS;
        }
        if (p.m_nItemPos < 0)
        {
            bool bFind = false;
            for (std::size_t i = 0; i < m_vItemList.size(); ++i)
            {
                auto& p_it_item = m_vItemList[i];
                if (nullptr == p_it_item)
                {
                    p_it_item = CreateNullItem();
                }
                if (nullptr == p_it_item)
                {
                    continue;
                }
                if (p_it_item->IsEmptyItem())
                {
                    p.m_nItemPos = i;
                    bFind = true;
                    break;
                }
            }
            if (!bFind)
            {
                OR_CHECK_RESULT_RETURN_RET(GetFullError());
                p.m_nItemPos = GetItemSize();
            }
        }

        if (p.m_nItemPos < 0 || (std::size_t)p.m_nItemPos >= m_vItemList.size())
        {
            return OR_POS;
        }
        item_type & pItem = m_vItemList[p.m_nItemPos];
        pItem.swap(newItem);
        InitItemCallBack(pItem);
        if (m_oAutoSaleCB)
        {
            m_oAutoSaleCB();
        }
        OnItemChange(p.m_nItemPos);

        return OR_OK;
    }

    int32_t BagBase::CheckClientPos(ItemParam &p)
    {
        OR_CHECK_RESULT(CheckServerPos(p));
        if (NULL == m_vItemList[p.m_nItemPos] || m_vItemList[p.m_nItemPos]->IsEmptyItem())
        {
            return OR_BAG_NULL_ITEM;
        }
        return OR_OK;
    }

    int32_t BagBase::CheckServerPos(ItemParam& p)
    {
        if (p.m_nItemPos < 0 || (std::size_t)p.m_nItemPos >= m_vItemList.size())
        {
            return OR_POS;
        }
        return OR_OK;
    }

    int32_t BagBase::UseItem(ItemParam & p)
    {
        p.m_pBag = this;
        p.m_pTagetScript = p.m_pPlayerScript;
        return OnUseItem(p);
    }

    int32_t BagBase::OnUseItem(ItemParam &)
    {
        return OR_BAG_CANT_USE_ITEM;
    }

    int32_t BagBase::SaleItem(ItemParam &p)
    {
        return OR_BAG_CANT_SALE_ITEM;
    }

    int32_t BagBase::SetLock(ItemParam & p)
    {
        return OR_BAG_CANT_SET_LOCK_ITEM;
    }

    int32_t BagBase::SetBind(ItemParam &)
    {
        return OR_BAG_CANT_SET_LOCK_ITEM;
    }

    int32_t BagBase::RemoveItem(ItemParam & p)
    {
        OR_CHECK_RESULT_RETURN_RET(CheckClientPos(p));
        item_type  pItem = m_vItemList[p.m_nItemPos];
        m_vItemList[p.m_nItemPos]->OnRemove(p);
        if (m_vItemList[p.m_nItemPos]->IsEmptyItem())
        {
            m_vItemList[p.m_nItemPos] = CreateNullItem();
        }
        return OR_OK;
    }

    int32_t BagBase::RemoveItemByConfigId(int32_t &configId, int32_t& nRemoveCount, ItemParam & p)
    {
        int32_t nPos = 0;

        for (item_list_type::iterator it = m_vItemList.begin(); it != m_vItemList.end(); ++it, ++nPos)
        {
            auto& p_it_item = *it;
            if (nullptr == p_it_item)
            {
                p_it_item = CreateNullItem();
            }
            if (nullptr == p_it_item)
            {
                continue;
            }

            if (p_it_item->IsEmptyItem())
            {
                continue;
            }

            if (p_it_item->GetConfId() != configId)
            {
                continue;
            }
            nRemoveCount += p_it_item->GetOverlaySize();
            p.m_nItemPos = nPos;
            p.m_nUseSize = p_it_item->GetOverlaySize();
            RemoveItem(p);
        }

        return OR_OK;
    }

    int32_t BagBase::ClearBag()
    {
        m_vItemList.clear();
        ResizeGrid();
        OnAllChange();
        return OR_OK;
    }

    void BagBase::NotifyItemChange()
    {
#ifdef __TEST__
        ClearChangeItem();
#endif//__TEST__
        if (client_new_changed_vec_.empty())
        {
            return;
        }

        BagRpcItemChangeNotify msg;
        msg.set_bagtype(m_nBagType);
        for (auto it = client_new_changed_vec_.begin(); it != client_new_changed_vec_.end(); ++it)
        {
            msg.add_itemlist()->CopyFrom(*it);
        }

        emc::i().emit<MsgEventStruct>(emid(), ModuleBag::RPC_CODE_BAG_ITEMCHANGE_NOTIFY, &msg);

        ClearChangeItem();
    }

    void BagBase::OnItemChange(ItemParam &p)
    {
        OnItemChange(p.m_nItemPos);
    }
    void BagBase::OnItemChange(int32_t item_pos)
    {
        ItemParam p;
        p.m_nItemPos = item_pos;
        OR_CHECK_RESULT_RETURN_VOID(CheckServerPos(p));
        item_type& p_item = GetItem(item_pos);
        SetChangeItemPb(p_item, item_pos);
    }

    void BagBase::OnItemChangeFromGuid(GUID_t item_guid)
    {
        OnItemChange(GetItemPos(item_guid));
    }

    void BagBase::OnAllChange()
    {
        for (size_t i = 0; i < GetCurCapacity(); ++i)
        {
            OnItemChange(i);
        }
    }

    void BagBase::ClearChangeItem()
    {
        BagBase::item_change_type v;
        std::swap(v, client_new_changed_vec_);
        //client_new_changed_vec_.clear();
    }

    bool BagBase::CanAddSize(std::size_t gridSize)
    {
        return gridSize <= GetEmptySize();
    }

    int32_t BagBase::CanAddItem(item_type & newItem)
    {
        std::size_t nPlaceholdersSize = newItem->BeforeAddToBagPlaceholdersSize();
        if (nPlaceholdersSize > GetEmptySize())
        {
            return OR_OK;
        }
        return OR_BAG_FULL;
    }

    int32_t BagBase::CanOverlayItem(item_type & curItem, CreateItemParam& param)
    {
        if (curItem->GetItemProtoElement()->id != param.m_nConfigId)
        {
            return OR_BAG_OVERLAY_CONDITION;
        }

        if (curItem->GetLock() != param.m_vLock)
        {
            return OR_BAG_OVERLAY_CONDITION_LOCK;
        }

        if (curItem->GetSaleCd() != param.m_nCD)
        {
            return OR_BAG_OVERLAY_CONDITION_SALECD;
        }
        if ((int32_t)curItem->GetBind() != param.m_vBind)
        {
            return OR_BAG_OVERLAY_CONDITION_BIND;
        }
        return OR_OK;
    }

    int32_t BagBase::CanOverlayItem(item_type & newItem, item_type & curItem)
    {
        if (newItem->IsEmptyItem())
        {
            return OR_EMPTY_ITEM;
        }

        if (curItem->IsEmptyItem())
        {
            return OR_OK;
        }

        if (curItem->GetItemProtoElement()->id != newItem->GetItemProtoElement()->id)
        {
            return OR_BAG_OVERLAY_CONDITION;
        }

        if (curItem->GetLock() != newItem->GetLock())
        {
            return OR_BAG_OVERLAY_CONDITION_LOCK;
        }

        if (curItem->GetBind() != newItem->GetBind())
        {
            return OR_BAG_OVERLAY_CONDITION_BIND;
        }
        if (curItem->GetSaleCd() != newItem->GetSaleCd())
        {
            return OR_BAG_OVERLAY_CONDITION_SALECD;
        }
        return OR_OK;
    }

    int32_t BagBase::CanOverlayItem(const i32_map_type & oConfigList, CreateItemParam& param)
    {
        for (auto it : oConfigList)
        {
            const ItemProtoElement * pEle = ItemProtoTable::Instance().GetElement(it.first);

            OR_CHECK_RESULT_RETURN_TABLE(pEle);
        }

        ResizeGrid();

        i32_map_type ret;
        for (auto& it : oConfigList)
        {
            ret.emplace(it.first, 0);
        }
        int32_t nEmptySize = 0;

        for (auto& item : m_vItemList)
        {
            if (nullptr == item)
            {
                item = CreateNullItem();
            }
            if (nullptr == item)
            {
                continue;
            }
            if (item->IsEmptyItem())
            {
                ++nEmptySize;
                continue;
            }
            for (auto oConfig : oConfigList)
            {
                if (oConfig.first != item->GetItemProtoElement()->id)
                {
                    continue;
                }

                if (item->GetBind() != param.m_bIsBindItem)
                {
                    continue;
                }
                if (item->GetItemProtoElement()->stack > 1
                    && item->GetItemProtoElement()->stack > item->GetOverlaySize())
                {
                    ret[oConfig.first] += item->GetItemProtoElement()->stack - item->GetOverlaySize();
                }
            }
        }

        for (auto it : ret)
        {
            const ItemProtoElement * pEle = ItemProtoTable::Instance().GetElement(it.first);

            if (NULL == pEle)
            {
                ELOG("get ele error id:%d", it.first);
                continue;
            }

            if (pEle->type == Item::E_CURRENCY)
            {
                continue;
            }

            auto check_item_info = oConfigList.find(it.first);
            if (oConfigList.end() == check_item_info)
            {
                return OR_BAG_CONFIG_ERROR;
            }

            if (it.second >= check_item_info->second)
            {
                continue;
            }

            if (it.second + nEmptySize * pEle->stack < check_item_info->second)
            {
                return CanOverlayItemFullErro();
            }

            nEmptySize -= (int32_t)(ceil((float)(check_item_info->second - it.second) / pEle->stack));
        }

        return OR_OK;
    }

    int32_t BagBase::CanOverlayItem(int32_t nConfigId, int32_t nOverlaySize)
    {
        if (nOverlaySize <= 0)
        {
            return OR_OK;
        }
        i32_map_type oV;
        oV.emplace(nConfigId, nOverlaySize);
        CreateItemParam p;
        return CanOverlayItem(oV, p);
    }

    int32_t BagBase::CanOverlayItem(const i32_map_type & oConfigList)
    {
        CreateItemParam param;
        return CanOverlayItem(oConfigList, param);
    }

    //**
    // bool bMergeDifCd 不同交易Cd能否合并
    // bool bMergeBind  是否合并邦定和非邦定
    //**
    BagBase::item_list_type BagBase::GetSameItemList(item_type & newItem, bool bMergeDifCd, bool bMergeBind)
    {
        time_t now = g_pTimeManager->CurrentTimeS();
        bool isBind = newItem->GetBind();
        int new_cd = newItem->GetSaleCd();
        bool in_cd = (new_cd != 0 && new_cd > now);//是不是在交易cd中。
        item_list_type v;
        for (item_list_type::iterator it = m_vItemList.begin(); it != m_vItemList.end(); ++it)
        {
            auto& curItem = *it;
            if (nullptr == curItem)
            {
                curItem = CreateNullItem();
            }
            if (nullptr == curItem)
            {
                continue;
            }
            if (nullptr == curItem->GetItemProtoElement())
            {
                continue;
            }

            if (curItem->IsEmptyItem())
            {
                continue;
            }

            int32_t nMaxOverlaySize = curItem->GetItemProtoElement()->stack;

            if (curItem->GetOverlaySize() >= nMaxOverlaySize)
            {
                continue;
            }
            if (NULL == curItem->GetItemProtoElement())
            {
                continue;
            }
            if (curItem->GetItemProtoElement()->id != newItem->GetItemProtoElement()->id)
            {
                continue;
            }
            if (!bMergeBind && isBind != curItem->GetBind())
            {//不合并邦定和不邦定，邦定不相同
                continue;
            }
            if (!bMergeDifCd)
            {
                int cur_cd = curItem->GetSaleCd();
                if (cur_cd != new_cd)
                {//不相等，只要一个在冷却中那么不合
                    if (in_cd || (cur_cd != 0 && cur_cd > now))
                    {
                        continue;
                    }
                }
            }
            v.push_back((*it));
        }

        return v;
    }

    int32_t BagBase::MergeItem(ItemParam &p)
    {
        p.m_nClassId = E_Bag;
        p.m_nClassId = E_Bag_MergeItem;
        OR_CHECK_RESULT_RETURN_RET(CheckClientPos(p));

        item_type  pItem = m_vItemList[p.m_nItemPos];
        if (NULL == pItem->GetItemProtoElement())
        {
            return OR_TABLE_INDEX;
        }
        OR_CHECK_RESULT_RETURN_RET(pItem->CanMerge());

        item_list_type  vSameItems = GetSameItemList(pItem, p.m_bClientRpc, p.m_bClientRpc);
        auto lower = vSameItems.begin();
        auto upper = vSameItems.end();
        if (vSameItems.size() <= 1)
        {
            return OR_BAG_ITEM_BIND_ONE_ERROR;
        }
        MergeItem(lower, upper, p.m_bClientRpc);

        OnAllChange();
        return OR_OK;
    }

    int32_t BagBase::MergeItem(item_list_type::iterator & lower, item_list_type::iterator & upper, bool m_bMergeDiffCd/*= false*/)
    {
        for (auto it = lower; it != upper; ++it)
        {
            item_type & rItem = (*it);
            const ItemProtoElement * pItemProtoElement = rItem->GetItemProtoElement();
            if (!pItemProtoElement)
            {
                return OR_PLAYER_ENTER_SCENE;
            }
            int cnt = rItem->GetOverlaySize();
            if (cnt >= pItemProtoElement->stack)
            {//当前满的
                continue;
            }
            int maxCd = rItem->GetSaleCd();
            bool bind = cnt > 0 ? rItem->GetBind() : false;
            bool fullStack = false;
            for (auto next_it = it + 1; next_it != upper; ++next_it)
            {
                item_type & rNextItem = (*next_it);
                const ItemProtoElement * pNextItemProtoElement = rNextItem->GetItemProtoElement();
                if (!pNextItemProtoElement)
                {
                    return OR_PLAYER_ENTER_SCENE;
                }
                if (pItemProtoElement->id != pNextItemProtoElement->id)
                {
                    continue;
                }
                int nextCnt = rNextItem->GetOverlaySize();
                if (nextCnt >= pItemProtoElement->stack)
                {//跳过满的
                    continue;
                }
                bind |= rNextItem->GetBind();
                cnt += nextCnt;
                if (cnt >= pItemProtoElement->stack)
                {
                    rItem->GetItemData().set_overlaycount(pItemProtoElement->stack);
                    nextCnt = cnt - pItemProtoElement->stack;
                    fullStack = true;
                }
                else
                {
                    rItem->GetItemData().set_overlaycount(cnt);
                    nextCnt = 0;
                }
                rNextItem->GetItemData().set_overlaycount(nextCnt);

                int saleCd = rNextItem->GetSaleCd();
                if (m_bMergeDiffCd && saleCd > maxCd)
                {
                    rItem->SetSaleCd(saleCd);
                }
                //如果有绑定在里面设置绑定
                rItem->SetBind(bind);
                if (fullStack)
                {
                    break;
                }
            }
        }

        return OR_OK;
    }

    int32_t BagBase::GetFullError()
    {
        if (IsFull())
        {
            return OR_BAG_FULL;
        }
        return OR_OK;
    }

    bool BagBase::IsFull()
    {
        return GetItemSize() >= m_nCurCapacity;
    }

    bool BagBase::ItemSortFn(const item_type & lhs, const item_type & rhs)
    {
        return *lhs.get() < *rhs.get();
    }

    std::size_t BagBase::CalcReturnEmptySize() const
    {
        std::size_t nSize = 0;
        for (item_list_type::const_iterator it = m_vItemList.begin(); it != m_vItemList.end(); ++it)
        {
            auto& curItem = *it;

            if (nullptr == curItem)
            {
                continue;
            }
            if (curItem->IsEmptyItem())
            {
                ++nSize;
            }
        }

        return nSize;
    }

    int32_t BagBase::UseItem(pos_index_list_type & indexs, ItemParam &p)
    {
        OR_CHECK_RESULT(FunctionSwitchManager::Instance().CheckOpen(OR_FUNCTION_ITEM_CLOSE, p.m_nConfigId));

        item_op_output_type  changeVec;

#ifndef __TEST__
        p.m_pPlayerScript = (IScriptHost *)m_pHuman;
#endif // !__TEST__
        for (auto it : indexs)
        {
            p.m_nItemPos = it.first;
            p.m_nUseSize = it.second;
            p.m_pTagetScript = p.m_pPlayerScript;
            OR_CHECK_RESULT(UseItem(p))
        }

        NotifyItemChange();
        return OR_OK;
    }

    void BagBase::UpdateUpper(item_list_type::iterator & lower, item_list_type::iterator & upper)
    {
        if (lower == m_vItemList.end())
        {
            return;
        }

        if (NULL == (*lower)->GetItemProtoElement())
        {
            return;
        }

        upper = m_vItemList.end();
        auto it = lower;
        ++it;
        for (; it != m_vItemList.end(); ++it)
        {
            item_type & rItem = (*it);
            if (nullptr == rItem)
            {
                rItem = CreateNullItem();
                continue;
            }
            if (NULL == rItem->GetItemProtoElement())
            {
                continue;
            }

            if (rItem->IsEmptyItem())
            {
                upper = it;
                break;
            }

            if (OR_OK != CanOverlayItem(rItem, *lower))
            {
                upper = it;
                break;
            }
        }
    }

    void BagBase::InitItemCallBack(item_type&  newItem)
    {
        if (nullptr == newItem)
        {
            return;
        }
        newItem->SetCheckCapacityCallback(std::bind(&BagBase::EnoughCapacity, this, std::placeholders::_1));
        newItem->SetItemChange(std::bind(&BagBase::OnItemChangeFromGuid, this, std::placeholders::_1));
        newItem->SetAddItemGuid(add_guid_item_callback_);
        if (newItem->GetItemType() == Item::E_EQUIPMENT)
        {
            newItem->SetGetEquipBySlotType(m_oGetEquipBySlotType);
        }
    }

    BagBase::item_type BagBase::CreateNullItem()
    {
        CreateItemParam p;
        p.m_nConfigId = NULL_ITEM_CONFIG;
        return CreateItem(p);
    }

    BagBase::item_type BagBase::FillItem(CreateItemParam & pCreateItemParam)
    {
        pCreateItemParam.pPlayerScript = (IScriptHost *)m_pHuman;
        item_type newItem;
        if (nullptr != pCreateItemParam.m_pItemData)
        {
            ItemData orig = *pCreateItemParam.m_pItemData;
            orig.set_overlaycount(pCreateItemParam.m_nCount);
            newItem = ItemFactory::CreateItem(orig, pCreateItemParam);
        }
        else
        {
            newItem = ItemFactory::CreateItem(pCreateItemParam);
        }

        InitItemCallBack(newItem);
        if (nullptr != newItem)
        {
            auto p_ele = ItemProtoTable::Instance().GetElement(pCreateItemParam.m_nConfigId);
            if (p_ele != nullptr && p_ele->type == Item::E_EQUIPMENT) //fix taskId:21115 勾选了自动一键出售后，获取装备时有可洗炼的装备被出售了
            {
                newItem->SetGetEquipBySlotType(GetGetEquipBySlotTypeCallBack());
            }
        }
        return newItem;
    }

    BagBase::item_type BagBase::CreateItem(CreateItemParam & pCreateItemParam)
    {
        item_type newItem = FillItem(pCreateItemParam);

        if (NULL_ITEM_CONFIG != pCreateItemParam.m_nConfigId)
        {
            emc::i().emit<CreateItemParam>(emid(), pCreateItemParam);
        }

        return newItem;
    }

    int32_t BagBase::Tidy()
    {
        if (clear_read_point_callback_)
        {
            clear_read_point_callback_();
        }
        std::sort(m_vItemList.begin(), m_vItemList.end(), ItemSortFn);

        item_list_type::iterator lower = m_vItemList.begin();
        item_list_type::iterator upper = m_vItemList.end();

        UpdateUpper(lower, upper);

        while (lower != m_vItemList.end() && nullptr != *lower && !(*lower)->IsEmptyItem())
        {
            if (nullptr == (*lower))
            {
                (*lower) = CreateNullItem();
                continue;
            }
            const ItemProtoElement * pItemProtoElement = (*lower)->GetItemProtoElement();

            if (NULL == pItemProtoElement)
            {
                continue;
            }

            if (pItemProtoElement->stack > 1)
            {
                MergeItem(lower, upper);
            }
            if (upper != m_vItemList.end())
            {
                lower = upper;
                UpdateUpper(lower, upper);
            }
            else
            {
                lower = upper;
                break;
            }
        }
        for (std::size_t i = 0; i <= m_vItemList.size(); ++i)
        {
            OnItemChange(i);
        }
        return OR_OK;
    }

    int32_t BagBase::EnoughCapacity(std::size_t nSize) const
    {
        if (GetEmptySize() < nSize)
        {
            return OR_BAG_ENOUGH_CAPACITY_FULL;
        }
        return OR_OK;
    }

    int32_t BagBase::BreakItem(ItemParam & p, i32_map_type  &oConf, bag_ptr_type pBag)
    {
        OR_CHECK_RESULT_RETURN_RET(CheckClientPos(p));
        item_type pItem = m_vItemList[p.m_nItemPos];
        CreateItemParam cp;
        cp.m_nConfigId = pItem->GetItemId();
        OR_CHECK_RESULT_RETURN_RET(pItem->CanBreak((IScriptHost *)m_pHuman, p));
        OR_CHECK_RESULT_RETURN_RET(CanOverlayBreakItem(pItem, oConf, p.m_nUseSize));
        OR_CHECK_RESULT_RETURN_RET(pItem->OnRemove(p));

        item_op_output_type  changeVec;
        item_op_output_type newVec;
       
        cp.m_nCount = p.m_nUseSize;
        cp.nClassId = EconomisClassEnum::E_Bag;
        cp.nWayId = EconomisClassEnum::E_Bag_Break;

		CreateOutItemParam outparam;
        for (auto it : oConf)
        {
            CreateItemParam ic;
            ic.orginal_break_config_id = cp.m_nConfigId;
            ic.m_nConfigId = it.first;
            ic.m_nCount = it.second;
            ic.nClassId = EconomisClassEnum::E_Bag;
            ic.nWayId = EconomisClassEnum::E_Bag_Break;
            ic.pPlayerScript = (IScriptHost *)m_pHuman;
            if (nullptr != pBag)
            {
                pBag->AddItem(ic, outparam);
            }
            else
            {
                AddItem(ic, outparam);
            }
        }

        return OR_OK;
    }

    int32_t BagBase::TryBreakSale(item_type & pItem)
    {
        if ((pItem->GetItemProtoElement()->rank == Item::E_Quailty0
            || pItem->GetItemProtoElement()->rank == Item::E_Quailty1)
            && pItem->GetItemType() == Item::E_EQUIPMENT)
        {
            ItemParam  nUseCondition;
            nUseCondition.m_nClassId = EconomisClassEnum::E_Bag;
            nUseCondition.m_nWayId = EconomisClassEnum::E_Bag_Break;
            nUseCondition.m_nUseSize = pItem->GetOverlaySize();
#ifndef __TEST__
            nUseCondition.m_pPlayerScript = (IScriptHost *)m_pHuman;
#endif // __TEST__

            pItem->OnSale(nUseCondition);
            OnItemChange(GetItemPos(pItem->GetGuid()));

            return OR_OK;
        }

        return OR_BAG_CANT_SALE_ITEM;
    }

    int32_t BagBase::BreakItem(ItemParam & p, int32_t nPos, i32_map_type  &oConf, bag_ptr_type pBag)
    {
        p.m_nItemPos = nPos;
        p.m_nClassId = EconomisClassEnum::E_Bag;
        p.m_nWayId = EconomisClassEnum::E_Bag_Break;
        OR_CHECK_RESULT_RETURN_RET(CheckClientPos(p));
        item_type pItem = m_vItemList[p.m_nItemPos];
        p.m_nUseSize = pItem->GetOverlaySize();
        return BreakItem(p, oConf, pBag);
    }

    int32_t BagBase::BreakItem(ItemParam & p, i32_v_type & pItemList, bag_ptr_type pBag)
    {
        i32_map_type  oConfigList;
        OR_CHECK_RESULT(CanOverlayBreakItem(pItemList, oConfigList));

        for (auto && i : pItemList)
        {
            oConfigList.clear();
            BreakItem(p, i, oConfigList, pBag);
        }
        return OR_OK;
    }

    int32_t BagBase::CanOverlayBreakItem(item_type pItem, i32_map_type & oConf, int32_t nOverlaySize)
    {
        ItemParam p;
        p.m_nUseSize = nOverlaySize;
        OR_CHECK_RESULT_RETURN_RET(pItem->CheckEnoughOverlaySize(p));

        if (NULL == pItem->GetItemProtoElement())
        {
            return OR_TABLE_INDEX;
        }

        if (pItem->GetItemProtoElement()->break_get.empty()
            || pItem->GetItemProtoElement()->break_count.empty())
        {
            return OR_TABLE_DATA_ERROR;
        }
        if (pItem->GetItemProtoElement()->break_get.size() != pItem->GetItemProtoElement()->break_count.size())
        {
            return OR_TABLE_DATA_NOT_MATCH_ERROR;
        }

        for (std::size_t i = 0; i < pItem->GetItemProtoElement()->break_get.size(); ++i)
        {
            oConf[pItem->GetItemProtoElement()->break_get[i]] += (pItem->GetItemProtoElement()->break_count[i] * nOverlaySize);
        }
        OR_CHECK_RESULT_RETURN_RET(CanOverlayItem(oConf));
        return OR_OK;
    }

    int32_t BagBase::CanOverlayBreakItem(i32_v_type & pItemList, i32_map_type & oConfigList)
    {
        for (auto && i : pItemList)
        {
            ItemParam p;
            p.m_nItemPos = i;
            OR_CHECK_RESULT(CheckClientPos(p));
        }

        for (auto && i : pItemList)
        {
            OR_CHECK_RESULT(CanOverlayBreakItem(m_vItemList[i], oConfigList, m_vItemList[i]->GetOverlaySize()));
        }
        return OR_OK;
    }

    int32_t BagBase::CanOverlayItemFullErro()
    {
        return OR_BAG_ENOUGH_CAPACITY_FULL;
    }

    int32_t BagBase::CheckEnoughItem(i32_map_type & oConsumeList, pos_index_list_type &outindexlist)
    {
        i32_map_type vList;

        if (oConsumeList.empty())
        {
            return OR_ITEM_CONSUM_ITEM_EMPTY;
        }

        for (auto jt : oConsumeList)
        {
            int32_t nPos = 0;
            for (item_list_type::iterator it = m_vItemList.begin(); it != m_vItemList.end(); ++it, ++nPos)
            {
                if (nullptr == (*it))
                {
                    (*it) = CreateNullItem();
                    continue;
                }
                if (NULL == (*it)->GetItemProtoElement())
                {
                    continue;
                }
                if ((*it)->IsEmptyItem())
                {
                    continue;
                }

                if ((*it)->GetItemProtoElement()->id == jt.first)
                {
                    if (vList[jt.first] >= jt.second)
                    {
                        continue;
                    }
                    int32_t nSize = 0;
                    if (vList[jt.first] + (*it)->GetOverlaySize() >= jt.second)
                    {
                        nSize = jt.second - vList[jt.first];
                    }
                    else
                    {
                        nSize = (*it)->GetOverlaySize();
                    }
                    if (vList[jt.first] >= jt.second)
                    {
                        break;
                    }

                    outindexlist.emplace(nPos, nSize);
                    vList[jt.first] += outindexlist[nPos];
                }
            }
        }

        for (auto jt : oConsumeList)
        {
            if (vList[jt.first] < jt.second)
            {
                return OR_ITEM_SIZE_ERROR;
            }
        }
        return OR_OK;
    }

    int32_t BagBase::SaleBreakAllItem(int32_t nQuality, ItemParam & player, BreakInParam& intparam, BreakOutParam& outparam)
    {
        i32_map_type  oConf;
        int nSize = 0;
        for (std::size_t i = 0; i < m_vItemList.size(); ++i)
        {
            item_type & pItem = m_vItemList[i];
            if (nullptr == pItem)
            {
                pItem = CreateNullItem();
                continue;
            }
            if (pItem->IsEmptyItem() || m_vItemList[i]->GetLock())
            {
                continue;
            }
            if (pItem->GetItemProtoElement()->rank != nQuality)
            {
                continue;
            }
            if (pItem->GetItemType() != player.item_type_)
            {
                continue;
            }
            if (pItem->CanRefining())
            {
                continue;
            }

            int nRet = OR_OK;
            oConf.clear();
            if (player.item_type_ == HumanBag::Item::E_SOUL_STONE)
            {
                BreakItem(player, i, oConf); // 分解背包里所有的能分解的魂石，而不是遇到不能分解的就停止
                if (nRet == OR_OK)
                {
                    nSize++;
                }
            }
            else
            {
                nRet = BreakItem(player, i, oConf);
            }

            if (outparam.use_return_ && nRet == OR_OK)
            {
                auto bit = outparam.break_item_map_.find(pItem->GetConfId());
                if (bit == outparam.break_item_map_.end())
                {
                    outparam.break_item_map_.emplace(pItem->GetConfId(), pItem->GetOverlaySize());
                    bit = outparam.break_item_map_.find(pItem->GetConfId());
                }
                else if (bit != outparam.break_item_map_.end())
                {
                    bit->second += pItem->GetOverlaySize();
                }
            }
        }

        if (player.item_type_ == HumanBag::Item::E_SOUL_STONE && m_vItemList.size() && !nSize)
        {
            return OR_BAG_RESOLVE_BAG_ITEM_ERROR;
        }
        return OR_OK;
    }

    std::size_t BagBase::GetEmptySize()const
    {
        return m_nCurCapacity - GetItemSize();
    }

    std::size_t BagBase::GetTailEmptySize()const
    {
        return m_nCurCapacity - m_vItemList.size();
    }

    int32_t BagBase::TakeItem(ItemParam & p, item_type &outItem)
    {
        OR_CHECK_RESULT_RETURN_RET(CheckClientPos(p));
        item_type  pItem = m_vItemList[p.m_nItemPos];
        outItem = pItem;
        m_vItemList[p.m_nItemPos] = CreateNullItem();
        OnItemChange(p);
        return OR_OK;
    }

    int32_t BagBase::TakeItem(int32_t nPos)
    {
        ItemParam  p;
        p.m_nItemPos = nPos;
        p.m_nClassId = E_Bag;
        p.m_nWayId = E_Bag_TakeItem;
        item_type outItem;
        TakeItem(p, outItem);
        return OR_OK;
    }

    int32_t BagBase::GetAllItem(item_list_type & v)
    {
        v = m_vItemList;
        return OR_OK;
    }

    int32_t BagBase::TakeAllItem(item_list_type & v)
    {
        v = m_vItemList;
        m_vItemList.clear();
        return OR_OK;
    }

#ifdef __TEST__
    void BagBase::TestOutPut()
    {
        return;
        std::cout << "---------------------------" << std::endl;
        std::size_t nSize = 0;
        for (item_list_type::iterator it = m_vItemList.begin(); it != m_vItemList.end(); ++it)
        {
            if ((*it)->IsEmptyItem())
            {
                std::cout << "(0 0 0)";
            }
            else
            {
                (*it)->TestOutPut();
            }

            if (++nSize % 5 == 0)
            {
                std::cout << std::endl;
            }
        }
        std::cout << "---------------------------" << std::endl;
    }

    int32_t BagBase::RandomItemPos()
    {
        std::default_random_engine generator;
        std::uniform_int_distribution<int> distribution(0, m_vItemList.size() - 1);
        int bag_dice_roll = distribution(generator);

        while (GetItemSize() > 0 && m_vItemList[bag_dice_roll]->IsEmptyItem())
        {
            bag_dice_roll = distribution(generator);
        }

        return bag_dice_roll;
    }

#endif // __TEST__

    BagBase::item_type& BagBase::GetItem(int32_t nPos)
    {
        if (nPos < 0 || (std::size_t)nPos >= m_vItemList.size())
        {
            static BagBase::item_type s = CreateNullItem();
            return s;
        }

        return m_vItemList[nPos];
    }

    float BagBase::GetItemTotalEffectParam(int32_t nPos)
    {
        if (nPos < 0 || (std::size_t)nPos >= m_vItemList.size())
        {
            return 0.0f;
        }
        return GetItemTotalEffectParam(nPos, m_vItemList[nPos]->GetOverlaySize());
    }

    float BagBase::GetItemTotalEffectParam(int32_t nPos, int32_t nOverLaySize)
    {
        if (nPos < 0 || (std::size_t)nPos >= m_vItemList.size())
        {
            return 0.0f;
        }
        return m_vItemList[nPos]->GetTotalEffectParam(nOverLaySize);
    }

    BagBase::item_type BagBase::GetItemByGuid(GUID_t nGuid)
    {
        for (auto it : m_vItemList)
        {
            if (nullptr == it)
            {
                it = CreateNullItem();
                continue;
            }
            if (nGuid == it->GetGuid() && !it->IsEmptyItem())
            {
                return it;
            }
        }
        return NULL;
    }

    bool BagBase::HasItem(GUID_t nGuid)
    {
        if (nGuid <= 0)
        {
            return false;
        }
        for (auto it : m_vItemList)
        {
            if (nullptr == it)
            {
                it = CreateNullItem();
                continue;
            }
            if (nGuid == it->GetGuid())
            {
                return true;
            }
        }
        return false;
    }

    BagBase::item_type BagBase::GetItemByGuid(GUID_t nGuid, int32_t& nPos)
    {
        // 可以使用这种方式
        // 	item_list_type::iterator iter = std::find_if(m_vItemList.begin(), m_vItemList.end(), [&](auto pItem) {
        // 		return pItem->GetGuid() == nGuid;
        // 	});
        // 	if (iter != m_vItemList.end())
        // 	{
        // 		nPos = std::distance(m_vItemList.begin(), iter);
        // 		return *iter;
        // 	}
        // 	nPos = INVALID_ID;
        // 	return nullptr;

        for (std::size_t i = 0; i < m_vItemList.size(); ++i)
        {
            item_type pItem = m_vItemList[i];
            if (pItem != nullptr
                && !pItem->IsEmptyItem()
                && pItem->GetGuid() == nGuid)
            {
                nPos = i;
                return pItem;
            }
        }
        nPos = -1;
        return NULL;
    }

    int32_t BagBase::GetItemPos(GUID_t nGuid)
    {
        if (nGuid <= 0)
        {
            return -1;
        }
        for (std::size_t i = 0; i < m_vItemList.size(); ++i)
        {
            if (nullptr == m_vItemList[i])
            {
                m_vItemList[i] = CreateNullItem();
                continue;
            }
            if (nGuid == m_vItemList[i]->GetGuid())
            {
                return i;
            }
        }
        return -1;
    }

    int32_t BagBase::GetOverlaySize()
    {
        int32_t nSize = 0;
        for (item_list_type::iterator it = m_vItemList.begin(); it != m_vItemList.end(); ++it)
        {
            if (nullptr == (*it))
            {
                (*it) = CreateNullItem();
                continue;
            }
            nSize += (*it)->GetOverlaySize();
        }
        return nSize;
    }

    int32_t BagBase::GetOverlaySize(int32_t nPos)
    {
        ItemParam p;
        p.m_nItemPos = nPos;
        if (OR_OK != CheckClientPos(p))
        {
            return 0;
        }
        return m_vItemList[nPos]->GetOverlaySize();
    }

    int32_t BagBase::GetItemCount(int32_t nItemID)
    {
        int nCount = 0;
        for (auto& item : m_vItemList)
        {
            if (nullptr == item)
            {
                item = CreateNullItem();
                continue;
            }
            if (item->GetItemId() == nItemID)
            {
                nCount += item->GetOverlaySize();
            }
        }
        return nCount;
    }

    int32_t  BagBase::Deblocking()
    {
        if (m_nCurCapacity >= g_nBagMaxSize)
        {
            return OR_BAG_CAPACITY_FULL;
        }
        m_nCurCapacity += g_nBagDeblockingSize;
        return OR_OK;
    }

    const int32_t PET_BAG_CAPACITY_GLOBAL_ID = 802;
    bool BagBase::InitStaticSize()
    {
        auto pTempBagMaxSize = GlobalTable::Instance().GetElement(E_GLOBAL_TEAM_BAG_MAX_SIZE);
        if (nullptr == pTempBagMaxSize)
        {
            return false;
        }
        auto pBagDefaultCapacity = GlobalTable::Instance().GetElement(E_GLOBAL_BAG_INIT_SIZE);
        if (nullptr == pBagDefaultCapacity)
        {
            return false;
        }
        auto pBagMaxSize = GlobalTable::Instance().GetElement(E_GLOBAL_BAG_MAX_SIZE);
        if (nullptr == pBagMaxSize)
        {
            return false;
        }
        auto pStorageInitSize = GlobalTable::Instance().GetElement(E_GLOBAL_STORAGE_BAG_INIT_SIZE);
        if (nullptr == pStorageInitSize)
        {
            return false;
        }
        auto pStorageMaxSize = GlobalTable::Instance().GetElement(E_GLOBAL_STORAGE_BAG_MAX_SIZE);
        if (nullptr == pStorageMaxSize)
        {
            return false;
        }
        auto pDeblockingSize = GlobalTable::Instance().GetElement(E_GLOBAL_DEBLOCKING_SIZE);
        if (nullptr == pDeblockingSize)
        {
            return false;
        }
        auto pPetMaxSizeEle = GlobalTable::Instance().GetElement(PET_BAG_CAPACITY_GLOBAL_ID);
        if (nullptr == pPetMaxSizeEle)
        {
            return false;
        }

        g_nBagMaxSize = pBagMaxSize->int_value;
        g_nTempBagMaxSize = pTempBagMaxSize->int_value;
        g_nBagDefaultCapacity = pBagDefaultCapacity->int_value;
        g_nStorageInitMaxSize = pStorageInitSize->int_value;
        g_nStorageBagMaxSize = pStorageMaxSize->int_value;
        g_nBagDeblockingSize = pDeblockingSize->int_value;
        g_nPetBagMaxSize = pPetMaxSizeEle->int_value;

        return true;
    }

    void BagBase::OnLoad(const BagData &pb)
    {
        m_vItemList.clear();
        m_nBagType = pb.bagtype();
        m_nCurCapacity = pb.curcapacity();
        if ((std::size_t)pb.items_size() > m_nCurCapacity)
        {
            ELOG("pb.items_size() > m_nCurCapacity");
        }
        ResizeGrid();
        for (std::size_t i = 0; i < (std::size_t)pb.items_size() && i < m_vItemList.size(); ++i)
        {
            int32_t ItemId = pb.items(i).itemdata().configid();

            CreateItemParam cp;
            cp.m_nConfigId = pb.items(i).itemdata().configid();

            cp.nClassId = EconomisClassEnum::E_Bag;
            cp.nWayId = EconomisClassEnum::E_Bag_Add;
            cp.m_pItemData = &pb.items(i).itemdata();
            cp.m_loadfromdb = true;
            item_type pItem = FillItem(cp);
            if (pItem != nullptr)
            {
                pItem->OnLoad(pb.items(i).itemdata());
                m_vItemList[i] = pItem;
            }
        }
    }

    void BagBase::OnAddNewGridItemSucces(CreateItemParam & cip, CreateOutItemParam& outparam, BagBase::item_type& p_new_item)
    {
        
        if (nullptr == p_new_item)
        {
            return;
        }
        CreateItemParam eip = cip;
        eip.m_nCount = p_new_item->GetOverlaySize();
        emc::i().emit<CreateItemParam>(emid(), eip);

        IScriptHost * pHuman = (IScriptHost *)m_pHuman;
        if (nullptr != pHuman && GetBagType() != BagBase::E_BAG_STORAGE_TYPE)
        {
            emc::i().emit<DungeonItemInfo>(emid(), DungeonItemInfo{ pHuman->GetGuid(), p_new_item->GetItemData() });
        }
        if (nullptr != pHuman && GetBagType() == BagBase::E_BAG_PET_TYPE)
        {
            pHuman->OnCaptureAdd(p_new_item.get());
        }
        if (nullptr != outparam.m_ItemCB)
        {
			auto itemIter = std::find(outparam.m_ItemCB->begin(), outparam.m_ItemCB->end(), p_new_item);
			if (itemIter == outparam.m_ItemCB->end())
			{
				outparam.m_ItemCB->push_back(p_new_item);
			}
        }
		if (nullptr != outparam.m_pItemChangeCB)
		{
			auto itemIter = std::find_if(outparam.m_pItemChangeCB->begin(), outparam.m_pItemChangeCB->end(), [&](auto& oSimpleItemData) {
				return oSimpleItemData.confid() == p_new_item->GetConfId();
			});
			if (itemIter != outparam.m_pItemChangeCB->end())
			{
				itemIter->set_count(itemIter->count() + p_new_item->GetOverlaySize());
			}
			else
			{
				ItemSimpleData oItemSimpleData;
				oItemSimpleData.set_confid(p_new_item->GetConfId());
				oItemSimpleData.set_count(p_new_item->GetOverlaySize());
				outparam.m_pItemChangeCB->emplace_back(std::move(oItemSimpleData));
			}
		}
        {
            auto p_ele = ItemProtoTable::Instance().GetElement(cip.m_nConfigId);
            if (nullptr == p_ele)
            {
                return;
            }
            CreateOutItemParam::AddInfo ai{ cip.m_nConfigId , p_new_item->GetGuid(), p_new_item->GetOverlaySize() };
            outparam.add_list_.push_back(ai);
        }
    }

    void BagBase::OnAddOverlaydItemSucces(CreateItemParam & cip, CreateOutItemParam& outparam, BagBase::item_type& p_new_item, int32_t add_count)
    {
        emc::i().emit<CreateItemParam>(emid(), cip);
        if (nullptr == p_new_item)
        {
            return;
        }
        IScriptHost * pHuman = (IScriptHost *)m_pHuman;
        if (nullptr != pHuman && GetBagType() != BagBase::E_BAG_STORAGE_TYPE)
        {
            ItemData pb;
            p_new_item->ToClientPb(pb);
            pb.set_overlaycount(add_count);
            emc::i().emit<DungeonItemInfo>(emid(), DungeonItemInfo{ pHuman->GetGuid(), pb });
        }
        if (nullptr != pHuman && GetBagType() == BagBase::E_BAG_PET_TYPE)
        {
            pHuman->OnCaptureAdd(p_new_item.get());
        }
        if (nullptr != outparam.m_ItemCB)
        {
			auto itemIter = std::find(outparam.m_ItemCB->begin(), outparam.m_ItemCB->end(), p_new_item);
			if (itemIter == outparam.m_ItemCB->end())
			{
				outparam.m_ItemCB->push_back(p_new_item);
			}
        }

		if (nullptr != outparam.m_pItemChangeCB)
		{
			auto itemIter = std::find_if(outparam.m_pItemChangeCB->begin(), outparam.m_pItemChangeCB->end(), [&](auto& oSimpleItemData) {
				return oSimpleItemData.confid() == p_new_item->GetConfId();
			});
			if (itemIter != outparam.m_pItemChangeCB->end())
			{
				itemIter->set_count(itemIter->count() + add_count);
			}
			else
			{
				ItemSimpleData oItemSimpleData;
				oItemSimpleData.set_confid(p_new_item->GetConfId());
				oItemSimpleData.set_count(add_count);
				outparam.m_pItemChangeCB->emplace_back(std::move(oItemSimpleData));
			}
		}

        {
            auto p_ele = ItemProtoTable::Instance().GetElement(cip.m_nConfigId);
            if (nullptr == p_ele)
            {
                return;
            }
            CreateOutItemParam::AddInfo ai{ cip.m_nConfigId , p_new_item ->GetGuid(), add_count };
            outparam.add_list_.push_back(ai);
        }
    }

    void BagBase::OnSave(BagData &pb)
    {
        for (auto && it : m_vItemList)
        {
            it->OnSave(*pb.add_items()->mutable_itemdata());
        }
        pb.set_curcapacity((int32_t)m_nCurCapacity);
        pb.set_bagtype(m_nBagType);
    }

    void BagBase::ToClientPb(BagData & pb)
    {
        int32_t nPos = 0;
        for (item_list_type::iterator it = m_vItemList.begin(); it != m_vItemList.end(); ++it)
        {
            if (nullptr == (*it))
            {
                continue;
            }
            ::ItemObj* pItemPB = pb.add_items();
            pItemPB->set_pos(nPos++);

            (*it)->ToClientPb(*pItemPB->mutable_itemdata());
            if (m_oHasRedPoint)
            {
                pItemPB->set_redpoint(m_oHasRedPoint((*it)->GetGuid()));
            }
        }
        pb.set_curcapacity((int32_t)m_nCurCapacity);
        pb.set_bagtype(m_nBagType);
    }

    void BagBase::ToJson(Json::Value& bags)
    {
        int32_t nPos = 0;
        Json::Value bag;
        for (item_list_type::iterator it = m_vItemList.begin(); it != m_vItemList.end(); ++it)
        {
            if (nullptr == *it)
            {
                continue;
            }
            Json::Value item;
            item["pos"] = nPos++;
            (*it)->ToJson(item);
            bag["items"].append(item);
        }

        bag["bag_type"] = m_nBagType;

        bags.append(bag);
    }

    void BagBase::ToClientItemPb(ItemObj & pb, int32_t nPos)
    {
        ItemParam p;
        p.m_nItemPos = nPos;
        OR_CHECK_RESULT_RETURN_VOID(CheckServerPos(p));
        pb.set_pos(nPos);
        m_vItemList[nPos]->ToClientPb(*pb.mutable_itemdata());
        if (m_oHasRedPoint)
        {
            pb.set_redpoint(m_oHasRedPoint(m_vItemList[nPos]->GetGuid()));
        }
    }

    void BagBase::ToClientItemPb(item_op_output_type & changeVec, int32_t nPos)
    {
        ItemObj  pb;
        ToClientItemPb(pb, nPos);
        changeVec.push_back(std::move(pb));
    }

    void BagBase::SetChangeItemPb(const item_type & p_item,
        std::size_t pos_index)
    {
        for (auto& it : client_new_changed_vec_)
        {
            if (it.pos() == pos_index && it.itemdata().guid() == p_item->GetGuid())
            {
                p_item->ToClientPb(*it.mutable_itemdata());
                return;
            }
        }

        ItemObj changePB;
        p_item->ToClientPb(*changePB.mutable_itemdata());
        changePB.set_pos(pos_index);
        if (changePB.itemdata().overlaycount() > 0 && m_oHasRedPoint)
        {
            changePB.set_redpoint(m_oHasRedPoint(p_item->GetGuid()));
        }
        client_new_changed_vec_.emplace_back(changePB);
    }

    BagModule::BagModule()
    {
        Init();
    }

    void BagModule::SetGetEquipBySlotType(const BagBase::get_equip_by_slottype& oCB)
    {
        for (auto& oBag : m_vBags)
        {
            oBag.second->SetGetEquipBySlotType(oCB);
        }
    }

    void BagModule::OnLoad(const DBBags & pb)
    {
        for (int32_t i = 0; i < pb.bags_size(); ++i)
        {
            bag_list_type::iterator it = m_vBags.find(pb.bags(i).bagtype());
            if (it != m_vBags.end())
            {
                it->second->OnLoad(pb.bags(i));
            }
        }
        for (int32_t i = 0; i < pb.redpointguids_size(); ++i)
        {
            AddRedPoint(pb.redpointguids(i));
        }
    }

    void BagModule::OnSave(DBBags & pb)
    {
        pb.Clear();
        for (auto it : m_vBags)
        {
            it.second->OnSave(*pb.add_bags());
        }
        for (auto &&it : m_vRedGuids)
        {
            pb.add_redpointguids(it);
        }
    }

    void BagModule::OnSave(user& db, user& dbcache)
    {
        DBBags ndata;
        this->OnSave(ndata);

        auto odata = *dbcache.mutable_bags();
        if (MessageUtils::Equals(ndata, odata))
        {
            db.clear_bags();
        }
        else
        {
            db.mutable_bags()->CopyFrom(ndata);
            dbcache.mutable_bags()->CopyFrom(ndata);
        }
    }

    void BagModule::OnLoadItemAutoOperation(const ItemResolveDataList& pb)
    {
        for (int32_t i = 0; i < pb.resolvelist_size(); ++i)
        {
            int32_t equip_type = pb.resolvelist(i).itemtype();
            resolve_unit_type::iterator it = resolve_unit_.find(equip_type);
            if (it == resolve_unit_.end())
            {
                continue;
            }
            it->second.CopyFrom(pb.resolvelist(i));
        }
    }

    void BagModule::OnSaveItemAutoOperation(ItemResolveDataList& pb)
    {
        pb.clear_resolvelist();
        for (auto& it : resolve_unit_)
        {
            pb.add_resolvelist()->CopyFrom(it.second);
        }
    }

    void BagModule::OnSaveItemAutoOperation(user& db, user& dbcache)
    {
    }

    void BagModule::OnNewDayCome()
    {
        for (auto& pBag : m_vBags)
        {
            pBag.second->OnNewDayCome();
        }
    }

    HumanBag::BagModule::bag_ptr_type BagModule::GetBag(int32_t nBagType)
    {
        bag_list_type::iterator it = m_vBags.find(nBagType);
        if (it == m_vBags.end())
        {
            return bag_ptr_type();
        }
        return it->second;
    }

    HumanBag::BagModule::bag_ptr_type BagModule::GetBagByItem(BagBase::item_type pItem)
    {
        if (nullptr == pItem || pItem->IsEmptyItem())
        {
            return nullptr;
        }
        const ItemProtoElement* pElement = pItem->GetItemProtoElement();
        if (nullptr == pElement)
        {
            return nullptr;
        }
        switch (pElement->store_type)
        {
        case BagBase::BagTypeEnum::E_BAG_PET_TYPE:
            return GetBag(BagBase::BagTypeEnum::E_BAG_PET_TYPE);
            break;
        default:
            return GetBag(BagBase::BagTypeEnum::E_BAG_TYPE);
            break;
        }
        return nullptr;
    }

    HumanBag::BagModule::bag_ptr_type BagModule::GetBagByItemConfig(int32_t n_config_id)
    {
        const ItemProtoElement* pElement = ItemProtoTable::Instance().GetElement(n_config_id);
        if (nullptr == pElement)
        {
            return nullptr;
        }
        switch (pElement->store_type)
        {
        case BagBase::BagTypeEnum::E_BAG_PET_TYPE:
            return GetBag(BagBase::BagTypeEnum::E_BAG_PET_TYPE);
            break;
        default:
            return GetBag(BagBase::BagTypeEnum::E_BAG_TYPE);
            break;
        }
        return nullptr;
    }

    HumanBag::BagModule::bag_ptr_type BagModule::GetBagByItemId(int32_t nConfigId)
    {
        const ItemProtoElement* pElement = ItemProtoTable::Instance().GetElement(nConfigId);
        if (nullptr == pElement)
        {
            return nullptr;
        }
        switch (pElement->store_type)
        {
        case BagBase::BagTypeEnum::E_BAG_PET_TYPE:
            return GetBag(BagBase::BagTypeEnum::E_BAG_PET_TYPE);
            break;
        default:
            return GetBag(BagBase::BagTypeEnum::E_BAG_TYPE);
            break;
        }
        return nullptr;
    }

    void BagModule::ToClientPb(::google::protobuf::RepeatedPtrField< ::BagData > & pb)
    {
        pb.Clear();
        for (int32_t i = 0; i < BagBase::E_BAG_MAX_TYPE; ++i)
        {
            ::BagData * pData = pb.Add();
            GetBag(i)->ToClientPb(*pData);
        }
    }

    void BagModule::ToJson(Json::Value& bags)
    {
        for (int32_t i = 0; i < BagBase::E_BAG_MAX_TYPE; ++i)
        {
            GetBag(i)->ToJson(bags);
        }
    }

    int32_t BagModule::TakeAllItemFromTempBag()
    {
        bag_ptr_type  pTempBag = GetBag(BagBase::E_BAG_TEMP_TYPE);
        if (nullptr == pTempBag)
        {
            return OR_NULL_PTR;
        }
        CreateItemParam cp;
        cp.nClassId = E_Bag;
        cp.nWayId = E_Bag_TakeItem;
        bool bAllFull = true;

        ItemParam tcp;
        tcp.m_nClassId = E_Bag;
        tcp.m_nWayId = E_Bag_TakeItem;

        std::size_t nSize = pTempBag->GetCurCapacity();
        for (std::size_t nPos = 0; nPos < nSize; ++nPos)
        {
            tcp.m_nItemPos = nPos;
            BagBase::item_type  pItem;
            pTempBag->TakeItem(tcp, pItem);
            if (pItem == nullptr || pItem->IsEmptyItem())
            {
                continue;
            }
            bag_ptr_type pToBag = GetBagByItem(pItem);
            if (pToBag == nullptr)
            {
                continue;
            }

            int32_t ret = pToBag->AddItemFromOtherBag(pItem);
            if (ret == OR_OK)
            {
                bAllFull = false;
            }
            else
            {
                pTempBag->AddItemFromOtherBag(pItem);
            }
            if (pToBag->IsFull())
            {
                break;
            }

            pToBag->NotifyItemChange();
        }
        pTempBag->ForwardItems();
        if (bAllFull)
        {
            return OR_BAG_FULL;
        }
        return OR_OK;
    }

    int32_t BagModule::TakeItem(int32_t nFromBagType, int32_t nToBagType, ItemParam & p)
    {
        bag_ptr_type  pFromBag = GetBag(nFromBagType);
        bag_ptr_type  pToBag = GetBag(nToBagType);

        if (NULL == pFromBag || pToBag == NULL)
        {
            return OR_BAG_TYPE_ERROR;
        }

        if (pFromBag == pToBag)
        {
            return OR_BAG_SAMETYPE_TAKE_ERROR;
        }

        put_type::iterator it = m_Puts.find(nFromBagType);
        if (it == m_Puts.end())
        {
            return OR_BAG_TYPE_ERROR;
        }

        if (it->second.find(nToBagType) == it->second.end())
        {
            return OR_BAG_CANT_TAKE_ERROR;
        }

        OR_CHECK_RESULT_RETURN_RET(pToBag->EnoughCapacity(1));
        BagBase::item_type pTakeItem;
        OR_CHECK_RESULT_RETURN_RET(pFromBag->TakeItem(p, pTakeItem));

        int32_t nRet = pToBag->AddItemFromOtherBag(pTakeItem);
        if (OR_OK != nRet)
        {
            pFromBag->AddItemFromOtherBag(pTakeItem);
            return nRet;
        }

        return OR_OK;
    }

    int32_t BagModule::SwapItem(int32_t nPos, BagBase::item_type  & newItem)
    {
        bag_ptr_type  pBag = GetBag(BagBase::E_BAG_TYPE);
        if (nullptr == pBag)
        {
            return OR_NULL_PTR;
        }
        OR_CHECK_RESULT_RETURN_RET(pBag->SwapItem(nPos, newItem));

        return OR_OK;
    }

    BagBase::item_type& BagModule::GetItem(int32_t nFromBagType, int32_t nPos)
    {
        bag_ptr_type pFromBag = GetBag(nFromBagType);
        if (NULL == pFromBag)
        {
            static BagBase::item_type a = NULL;
            return a;
        }
        return pFromBag->GetItem(nPos);
    }

    int32_t BagModule::CreateItem(CreateItemParam & p, CreateOutItemParam& outparam)
    {
        if (m_oOccupation)
        {
            p.m_nOccupation = m_oOccupation() - 1;
        }
        p.pPlayerScript = (IScriptHost *)m_pHuman;
        if (p.m_vItemCountPairs.empty())
        {
            OR_CHECK_RESULT(CreateItemEx(p, outparam));
            UsePickUpTypeItem(outparam);
            return OR_OK;
        }

        int32_t nRet = OR_OK;
        if (p.m_vItemCountPairs.size() % 2 != 0)
        {
            return OR_BAG_ITEM_CREATE_ITEM_LIST_SIZE_ERROR;
        }

        for (i32_v_type::const_iterator it = p.m_vItemCountPairs.begin(); it != p.m_vItemCountPairs.end(); )
        {
            int32_t nItemId = *it;
            ++it;
            if (it == p.m_vItemCountPairs.end())
            {
                break;
            }
            int32_t nCount = *it;
            p.m_nConfigId = nItemId;
            p.m_nCount = nCount;
            nRet = CreateItemEx(p, outparam);
            if (nRet != OR_OK)
            {
                return nRet;
            }
            ++it;
        }
        UsePickUpTypeItem(outparam);
        return nRet;
    }

    int32_t BagModule::CreateItem(ItemData & pb, CreateItemParam &p, CreateOutItemParam& outparam)
    {
        if (m_oOccupation)
        {
            p.m_nOccupation = m_oOccupation() - 1;
        }
        p.pPlayerScript = (IScriptHost *)m_pHuman;
        p.m_pItemData = &pb;
        return CreateItem(p, outparam);
    }

    int32_t BagModule::CreateItem(ItemData & pb, CreateItemParam &p)
    {
        CreateOutItemParam outparam;
        p.m_nConfigId = pb.configid();
        p.m_nCount = pb.overlaycount();
        return CreateItem(pb, p, outparam);
    }

    int32_t BagModule::CreateItem(CreateItemParam & p)
    {
        CreateOutItemParam outparam;
        return CreateItem(p, outparam);
    }

    int32_t BagModule::CreateItemEx(const CreateItemParam & inparam, CreateOutItemParam& outparam)
    {
        if (ItemFactory::AddCurrency(inparam))
        {
            return OR_OK;
        }
        CreateItemParam cip = inparam;
        outparam.m_ItemCB = inparam.m_ItemCB;
		outparam.m_pItemChangeCB = inparam.m_pItemChangeCB;

        if (m_oOccupation)
        {
            cip.m_nOccupation = m_oOccupation() - 1;
        }
        cip.pPlayerScript = (IScriptHost *)m_pHuman;
        int32_t config_id = cip.m_nConfigId;
        if (nullptr != inparam.m_pItemData)
        {
            config_id = inparam.m_pItemData->configid();
            cip.m_nConfigId = config_id;
            cip.m_nCount = inparam.m_pItemData->overlaycount();
        }
        bag_ptr_type  pBag = GetBagByItemConfig(config_id);

        if (nullptr == pBag)
        {
            return OR_OK;
        }

        if (cip.m_bAutoResolve && AutoSaleDisassemble(cip, outparam))
        {
            return OR_OK;
        }
        int32_t nRet = pBag->AddItem(cip, outparam);
        if (OR_OK == nRet)
        {
            return nRet;
        }

        if (OR_BAG_FULL != nRet &&
            OR_BAG_PET_BAG_FULL != nRet)
        {
            return nRet;
        }

        if (!cip.m_bInTempBag)
        {
            return nRet;
        }

        bag_ptr_type  pTempBag = GetBag(BagBase::E_BAG_TEMP_TYPE);
        if (nullptr == pTempBag)
        {
            return OR_NULL_PTR;
        }
        OR_CHECK_RESULT_RETURN_RET(pTempBag->AddItem(cip, outparam));

        return OR_OK;
    }

    void BagModule::UsePickUpTypeItem(CreateOutItemParam& outparam)
    {
        static const int32_t kPickUpUseType = 1;
        HumanBag::ItemParam p;
#ifndef __TEST__
        if (nullptr == m_pHuman)
        {
            return;
        }
        p.m_pPlayerScript = (IScriptHost *)m_pHuman;
        p.m_nPlayerLevel = p.m_pPlayerScript->GetLevel();
#endif // !__TEST__

        p.m_nClassId = EconomisClassEnum::E_Bag;
        p.m_nWayId = EconomisClassEnum::E_Bag_UseItem;
        i32_map_type v;
        for (auto& it : outparam.add_list_)
        {
            auto p_bag = GetBagByItemConfig(it.confid_);
            if (nullptr == p_bag)
            {
                continue;
            }
            auto p_ele = ItemProtoTable::Instance().GetElement(it.confid_);
            if (p_ele == nullptr)
            {
                continue;
            }
            if (p_ele->pickup_type != kPickUpUseType)
            {
                continue;
            }
            p.m_nConfigId = it.confid_;
            p.m_nItemPos = p_bag->GetItemPos(it.item_guid_);
            p.m_nUseSize = it.count_;
            v.emplace(p.m_nConfigId, p.m_nUseSize);
        }
        UseItem(v, p);
    }

    int32_t BagModule::SafeCreateItem(CreateItemParam& p, CreateOutItemParam& outparam)
    {
        BagModule::bag_ptr_type pBag = GetBagByItemId(p.m_nConfigId);
        if (pBag == nullptr)
            return OR_TABLE_INDEX;

        int nRet = pBag->CanOverlayItem(p.m_nConfigId, p.m_nCount);
        if (OR_OK != nRet)
            return nRet;

        return CreateItemEx(p, outparam);
    }

    bool BagModule::AutoSaleDisassemble(CreateItemParam & cip, CreateOutItemParam& outparam)
    {
        ItemParam  p;
        BagBase::item_type  pItem = ItemFactory::CreateItem(cip);
        OR_CHECK_RESULT_RETURN_BOOL(CheckAutoBreak(pItem));
        OR_CHECK_RESULT_RETURN_BOOL(CheckAutoSaleDisassembleFilter(pItem));

        bag_ptr_type  pBag = GetBagByItem(pItem);//GetBag(BagBase::E_BAG_TYPE);
        if (nullptr == pBag)
        {
            return false;
        }
        if (pItem->GetItemType() == Item::E_PET)
        {
            if (nullptr == m_oPetAutoBreakCB)
            {
                return false;
            }
            return m_oPetAutoBreakCB(pItem);
        }

        for (auto && it : resolve_unit_)
        {
            if (it.first != pItem->GetItemType())
            {
                continue;
            }
            for (int32_t i = 0; i < it.second.resolvelist_size(); ++i)
            {
                if (NULL == pItem->GetItemProtoElement())
                {
                    continue;
                }
                const::ItemResolveData& autosalepb = it.second.resolvelist(i);
                if (!autosalepb.isresolve())
                {
                    continue;
                }
                if (pItem->GetItemProtoElement()->rank != autosalepb.qulity())
                {
                    continue;
                }

                if (Item::ItemEnum::E_PET == pItem->GetItemType())
                {
                    continue;
                }
                if (pItem->CanRefining())
                {
                    continue;
                }

                if (OR_OK != pItem->CanBreak((IScriptHost *)m_pHuman, p))
                {
                    continue;
                }
                int32_t org_config_id = pItem->GetConfId();
                i32_map_type  oConfigList;
                if (OR_OK != pBag->CanOverlayBreakItem(pItem, oConfigList, pItem->GetOverlaySize()))
                {
                    continue;
                }
                for (auto it : oConfigList)
                {
                    CreateItemParam p;
                    p.m_nConfigId = it.first;
                    p.m_nCount = it.second;
                    p.nClassId = EconomisClassEnum::E_Bag;
                    p.nWayId = EconomisClassEnum::E_Bag_Break;
                    p.orginal_break_config_id = org_config_id;
#ifndef __TEST__
                    p.pPlayerScript = (IScriptHost *)m_pHuman;
#endif // !__TEST__
                    CreateItem(p, outparam);
                }

                return true;
            }
        }

        return false;
    }

    bool BagModule::CheckAutoSaleDisassembleFilter(BagBase::item_type & pItem)
    {
        if (nullptr == pItem)
        {
            return false;
        }

        const ItemDissolveFilterTable::MapElementMap& vMap = ItemDissolveFilterTable::Instance().GetAllElement();
        for (auto && it : vMap)
        {
            if (it.second->item_sub_type.empty())
            {
                if (pItem->GetItemType() == it.second->item_type)
                {
                    return false;
                }
            }
            else
            {
                for (auto && ji : it.second->item_sub_type)
                {
                    if (nullptr != pItem->GetItemProtoElement()
                        && pItem->GetItemType() == it.second->item_type
                        && pItem->GetItemProtoElement()->subtype == it.second->item_type)
                    {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    std::map<int32_t, i32_map_type> BagModule::MakeConsumeListWithBagType(const i32_map_type& oConsumeList) const
    {
        std::map<int32_t, i32_map_type> mToBagConfigList;
        for (auto& it : oConsumeList)
        {
            auto pItemProtoElement = ItemProtoTable::Instance().GetElement(it.first);
            if (nullptr != pItemProtoElement)
            {
                switch (pItemProtoElement->store_type)
                {
                case BagBase::E_BAG_PET_TYPE:
                {
                    auto oIt = mToBagConfigList.find(BagBase::E_BAG_PET_TYPE);
                    if (oIt == mToBagConfigList.end())
                    {
                        i32_map_type vPetBagList;
                        vPetBagList.emplace(it.first, it.second);
                        mToBagConfigList.emplace(BagBase::E_BAG_PET_TYPE, vPetBagList);
                    }
                    else
                    {
                        i32_map_type& vPetBagList = oIt->second;
                        vPetBagList.emplace(it.first, it.second);
                    }
                }
                break;
                default:
                {
                    auto oIt = mToBagConfigList.find(BagBase::E_BAG_TYPE);
                    if (oIt == mToBagConfigList.end())
                    {
                        i32_map_type vBaseBagList;
                        vBaseBagList.emplace(it.first, it.second);
                        mToBagConfigList.emplace(BagBase::E_BAG_TYPE, vBaseBagList);
                    }
                    else
                    {
                        i32_map_type& vBaseBagList = oIt->second;
                        vBaseBagList.emplace(it.first, it.second);
                    }
                }
                break;
                }
            }
        }
        return mToBagConfigList;
    }

    int32_t BagModule::EnoughCapacity(std::size_t nSize, int32_t bag_type)
    {
        auto p_bag = GetBag(bag_type);
        if (nullptr == p_bag)
        {
            return OR_BAG_NULL_PTR_ERROR;
        }
        return p_bag->EnoughCapacity(nSize);
    }

    void BagModule::SetAllBagNotChnage()
    {
        /*for (auto it : m_vBags)
        {
            it.second->SetChange(false);
        }*/
    }

    bool BagModule::IsBagFull()
    {
        bag_ptr_type  pBag = GetBag(BagBase::E_BAG_TYPE);
        if (NULL == pBag)
        {
            return true;
        }

        return pBag->IsFull();
    }

    void BagModule::Tidy()
    {
        bag_ptr_type  pBag = GetBag(BagBase::E_BAG_TYPE);
        if (NULL == pBag)
        {
            return;
        }
        m_vRedGuids.clear();
        pBag->Tidy();

        return;
    }

    std::size_t BagModule::GetBagCurCapacity(int32_t bagType)
    {
        bag_ptr_type  pBag = GetBag(bagType);
        if (NULL == pBag)
        {
            return 0;
        }
        return pBag->GetCurCapacity();
    }

    int32_t BagModule::SaleBreakAllItem(int32_t nQuality, BreakInParam& intparam, BreakOutParam& outparam)
    {
        bag_ptr_type  pBag = GetBag(BagBase::E_BAG_TYPE);
        if (NULL == pBag)
        {
            return OR_BAG_NULL_PTR_ERROR;
        }
        ItemParam p;
        p.m_nClassId = EconomisClassEnum::E_Bag;
        p.m_nWayId = EconomisClassEnum::E_Bag_Break;
        p.item_type_ = intparam.auto_type_;
#ifndef __TEST__
        p.m_pPlayerScript = (IScriptHost *)m_pHuman;
#endif
        return pBag->SaleBreakAllItem(nQuality, p, intparam, outparam);
    }

    int32_t BagModule::SaleBreakAllItem(int32_t nQuality)
    {
        BreakInParam intparam;
        BreakOutParam outparam;
        return SaleBreakAllItem(nQuality, intparam, outparam);;
    }

    int32_t BagModule::SaleBreakAllItemNoParam()
    {
        BreakInParam intparam;
        BreakOutParam outparam;
        for (auto && it : resolve_unit_)
        {
            for (int32_t i = 0; i < it.second.resolvelist_size(); ++i)
            {
                auto & pb = it.second.resolvelist(i);
                if (!pb.isresolve())
                {
                    continue;
                }
                SaleBreakAllItem(pb.qulity(), intparam, outparam);
            }
        }
        return OR_OK;
    }

    int32_t BagModule::ClearBag()
    {
        m_vRedGuids.clear();
        for (auto && it : m_vBags)
        {
            it.second->ClearBag();
        }
        bag_ptr_type  pBag = GetBag(BagBase::E_BAG_TYPE);
        if (NULL == pBag)
        {
            return OR_BAG_NULL_PTR_ERROR;
        }
        return pBag->ClearBag();
    }

    int32_t BagModule::BreakItem(ItemParam & p)
    {
        bag_ptr_type  pBag = GetBag(BagBase::E_BAG_TYPE);
        if (NULL == pBag)
        {
            return OR_BAG_NULL_PTR_ERROR;
        }
        i32_map_type  oConf;
        p.m_nClassId = EconomisClassEnum::E_Bag;
        p.m_nWayId = EconomisClassEnum::E_Bag_Break;
        return  pBag->BreakItem(p, oConf);
    }

    int32_t BagModule::GetItemCount(int32_t configid)
    {
        bag_ptr_type  pBag = GetBagByItemId(configid);//GetBag(BagBase::E_BAG_TYPE);
        if (NULL == pBag)
        {
            return 0;
        }
        return  pBag->GetItemCount(configid);
    }

    int32_t BagModule::CheckEnoughItem(i32_map_type & oConsumeList, BagBase::pos_index_list_type &outindexlist)
    {
        bag_ptr_type  pBag = GetBag(BagBase::E_BAG_TYPE);
        if (NULL == pBag)
        {
            return OR_BAG_NULL_PTR_ERROR;
        }

        if (ItemFactory::get_item_type_callback_)
        {
            IScriptHost* p_human = (IScriptHost *)m_pHuman;
            for (auto& it : oConsumeList)
            {
                int32_t r_type = ItemFactory::get_item_type_callback_(it.first);
                if (r_type > 0)
                {
                    OR_CHECK_RESULT(p_human->EnoughResource(r_type, it.second, 1));
                }
            }
        }

        return pBag->CheckEnoughItem(oConsumeList, outindexlist);
    }

    int32_t BagModule::CheckEnoughItem(i32_map_type& oConsumeList, std::map<int32_t, BagBase::pos_index_list_type>& outindexlistWithBag)
    {
        auto mBagConsumeList = MakeConsumeListWithBagType(oConsumeList);
        for (auto& it : mBagConsumeList)
        {
            bag_ptr_type pBag = GetBag(it.first);
            if (nullptr == pBag)
            {
                return OR_BAG_NULL_PTR_ERROR;
            }
            int32_t nResult = pBag->CheckEnoughItem(it.second, outindexlistWithBag[it.first]);
            if (OR_OK != nResult)
            {
                return nResult;
            }
        }
        return OR_OK;
    }

    float BagModule::GetConsumeItemTotalEffectParam(BagBase::pos_index_list_type outindexlist)
    {
        bag_ptr_type  pBag = GetBag(BagBase::E_BAG_TYPE);
        if (NULL == pBag)
        {
            return OR_BAG_NULL_PTR_ERROR;
        }
        float nF = 0.0f;
        for (auto it : outindexlist)
        {
            nF += pBag->GetItemTotalEffectParam(it.first, it.second);
        }
        return nF;
    }

    int32_t BagModule::GetItemSubType(BagBase::pos_index_list_type outindexlist)
    {
        bag_ptr_type  pBag = GetBag(BagBase::E_BAG_TYPE);
        if (NULL == pBag)
        {
            return -1;
        }
        if (outindexlist.empty())
        {
            return -1;
        }
        BagBase::item_type p = pBag->GetItem(outindexlist.begin()->first);
        if (p == NULL)
        {
            return -1;
        }
        const ItemProtoElement * pItemProtoElement = p->GetItemProtoElement();
        if (NULL == p->GetItemProtoElement())
        {
            return -1;
        }

        return pItemProtoElement->subtype;
    }

    int32_t BagModule::RemoveItem(BagBase::pos_index_list_type &outindexlist, HumanBag::ItemParam & p, int32_t nBagType)
    {
        // need check first
        bag_ptr_type  pBag = GetBag(nBagType);
        if (NULL == pBag)
        {
            return OR_BAG_NULL_PTR_ERROR;
        }

#ifndef __TEST__
        p.m_pPlayerScript = (IScriptHost *)m_pHuman;
#endif // !__TEST__

        for (auto i : outindexlist)
        {
            p.m_nItemPos = i.first;
            p.m_nUseSize = i.second;
            RemoveItem(p, nBagType);
        }
        return OR_OK;
    }

    int32_t BagModule::RemoveItem(std::map<int32_t, BagBase::pos_index_list_type>& outindexWithBag, HumanBag::ItemParam& param)
    {
        param.m_pPlayerScript = (IScriptHost *)m_pHuman;
        for (auto& it : outindexWithBag)
        {
            bag_ptr_type pBag = GetBag(it.first);
            if (nullptr == pBag)
            {
                continue;
            }
            for (auto& index : it.second)
            {
                param.m_nItemPos = index.first;
                param.m_nUseSize = index.second;
                RemoveItem(param, it.first);
            }
        }
        return OR_OK;
    }

    int32_t BagModule::BreakItemByPos(BagBase::pos_index_list_type &outindexlist, int32_t nBagType)
    {
        bag_ptr_type  pBag = GetBag(nBagType);
        if (NULL == pBag)
        {
            return OR_BAG_NULL_PTR_ERROR;
        }

        bag_ptr_type  pDefaultBag = GetBag(BagBase::E_BAG_TYPE);
        if (NULL == pDefaultBag)
        {
            return OR_BAG_NULL_PTR_ERROR;
        }
        HumanBag::ItemParam p;
        p.m_nClassId = EconomisClassEnum::E_Bag;
        p.m_nWayId = EconomisClassEnum::E_Bag_Break;
#ifndef __TEST__
        p.m_pPlayerScript = (IScriptHost *)m_pHuman;
#endif // !__TEST__
        p.m_bClientBreak = true;
        i32_v_type vItemList;

        for (auto i : outindexlist)
        {
            vItemList.push_back(i.first);
        }

        OR_CHECK_RESULT(pBag->BreakItem(p, vItemList, pDefaultBag));

        return OR_OK;
    }

    int32_t BagModule::RemoveItemByConfigId(int32_t configId, int32_t& nRemoveCount, HumanBag::ItemParam & p, int32_t nBagType)
    {
        bag_ptr_type  pBag = GetBag(nBagType);
        if (NULL == pBag)
        {
            return OR_BAG_NULL_PTR_ERROR;
        }
#ifndef __TEST__
        p.m_pPlayerScript = (IScriptHost *)m_pHuman;
#endif // !__TEST__
        return pBag->RemoveItemByConfigId(configId, nRemoveCount, p);
    }

    int32_t BagModule::CheckAndRemove(i32_map_type & oConsumeList, HumanBag::ItemParam  & checkp)
    {
        if (!oConsumeList.empty())
        {
            checkp.m_nConfigId = oConsumeList.begin()->first;
            checkp.m_nUseSize = oConsumeList.begin()->second;
        }
        BagBase::pos_index_list_type outindexlist;
        OR_CHECK_RESULT_RETURN_RET(CheckEnoughItem(oConsumeList, outindexlist));
        OR_CHECK_RESULT_RETURN_RET(RemoveItem(outindexlist, checkp));
        return OR_OK;
    }

    int32_t BagModule::UseItem(i32_map_type & v, ItemParam &p)
    {
        bag_ptr_type  pBag = GetBag(BagBase::E_BAG_TYPE);
        if (NULL == pBag)
        {
            return OR_BAG_NULL_PTR_ERROR;
        }
        BagBase::pos_index_list_type outindexlist;
        p.m_pTagetScript = p.m_pPlayerScript;
        OR_CHECK_RESULT_RETURN_RET(pBag->CheckEnoughItem(v, outindexlist));
        return pBag->UseItem(outindexlist, p);
    }

    int32_t BagModule::UseItemCallback(i32_map_type & v, ItemParam &p)
    {
        return UseItem(v, p);
    }

    int32_t BagModule::RemoveItem(HumanBag::ItemParam & p, int32_t nBagType)
    {
        bag_ptr_type  pBag = GetBag(nBagType);
        if (NULL == pBag)
        {
            return OR_BAG_NULL_PTR_ERROR;
        }
        if (ItemFactory::get_item_type_callback_)
        {
            IScriptHost* p_human = (IScriptHost *)m_pHuman;

            int32_t r_type = ItemFactory::get_item_type_callback_(p.m_nConfigId);
            if (r_type > 0 && p.m_nConfigId > 0)
            {
                ResourceReduceParam param;
                param.nClassId = p.m_nClassId;
                param.nWayId = p.m_nWayId;
                p_human->ReduceResource(r_type, p.m_nUseSize, param);
            }
        }
        OR_CHECK_RESULT_RETURN_RET(pBag->RemoveItem(p));

        return OR_OK;
    }

    int32_t BagModule::RemoveItemNotifyClient(HumanBag::ItemParam & p, int32_t nBagType)
    {
        bag_ptr_type  pBag = GetBag(nBagType);
        if (NULL == pBag)
        {
            return OR_BAG_NULL_PTR_ERROR;
        }
        if (ItemFactory::get_item_type_callback_)
        {
            IScriptHost* p_human = (IScriptHost *)m_pHuman;

            int32_t r_type = ItemFactory::get_item_type_callback_(p.m_nConfigId);
            if (r_type > 0 && p.m_nConfigId > 0)
            {
                ResourceReduceParam param;
                param.nClassId = p.m_nClassId;
                param.nWayId = p.m_nWayId;
                p_human->ReduceResource(r_type, p.m_nUseSize, param);
            }
        }
        OR_CHECK_RESULT_RETURN_RET(pBag->RemoveItem(p));
        pBag->NotifyItemChange();

        return OR_OK;
    }

    int32_t BagModule::RemoveItemNotifyClient(BagBase::pos_index_list_type &outindexlist, HumanBag::ItemParam & param, int32_t nBagType)
    {
        // need check first
        bag_ptr_type  pBag = GetBag(nBagType);
        if (NULL == pBag)
        {
            return OR_BAG_NULL_PTR_ERROR;
        }

#ifndef __TEST__
        param.m_pPlayerScript = (IScriptHost *)m_pHuman;
#endif // !__TEST__

        for (auto i : outindexlist)
        {
            param.m_nItemPos = i.first;
            param.m_nUseSize = i.second;
            RemoveItem(param, nBagType);
        }
        NotifyClientBagChangeMsg();
        return OR_OK;
    }

    int32_t BagModule::SetItemAutoOperatorData(ItemAutoOperatorData & pb, int32_t auto_type)
    {
        resolve_unit_type::iterator it = resolve_unit_.find(auto_type);
        if (it == resolve_unit_.end())
        {
            return OR_BAG_RESOLVE_TYPE_ERROR;
        }

        if (pb.autosaledisassemble_size() <= 0 ||
            pb.autosaledisassemble_size() > kMaxResolveSize ||
            pb.autosaledisassemble_size() < it->second.resolvelist_size())
        {
            return OR_POS;
        }
        if (pb.autosaledisassemble(0).qulity() != Item::E_Quailty0)
        {
            return OR_QULITY_SET_ERROR;
        }
        if (pb.autosaledisassemble(1).qulity() != Item::E_Quailty1)
        {
            return OR_QULITY_SET_ERROR;
        }
        if (pb.autosaledisassemble(2).qulity() != Item::E_Quailty2)
        {
            return OR_QULITY_SET_ERROR;
        }

        for (int32_t i = 0; i < it->second.resolvelist_size(); ++i)
        {
            it->second.mutable_resolvelist(i)->CopyFrom(pb.autosaledisassemble(i));
        }

        return OR_OK;
    }

    void BagModule::RpcClickItem(BagRpcClickItemAsk & rq, BagRpcClickItemReply & rsp, ItemParam & p)
    {
        for (auto && it : m_vBags)
        {
            if (!it.second->HasItem(rq.guid()))
            {
                continue;
            }

            int32_t nPos = it.second->GetItemPos(rq.guid());
            if (nPos < 0)
            {
                continue;
            }
            m_vRedGuids.erase(rq.guid());
            it.second->OnItemChange(nPos);
            it.second->ToClientItemPb(*rsp.mutable_item(), nPos);
            rsp.set_result(OR_OK);
            return;
        }

        rsp.set_result(OR_BAG_ITEM_NONE_GUID_ERROR);
    }

    void BagModule::RpcClearReadPoint(BagRpcClearBagRedPointAsk & rq, BagRpcClearBagRedPointReply & rsp)
    {
        m_vRedGuids.clear();

        rsp.set_result(OR_OK);

        auto p_bag = GetBag(BagBase::E_BAG_TYPE);
        if (nullptr == p_bag)
        {
            return;
        }
        p_bag->OnAllChange();
    }

    void BagModule::RpcUseItem(BagRpcUseItemAsk & rq, BagRpcUseItemReply & rsp, ItemParam & p)
    {
        if (rq.bagtype() >= BagBase::E_BAG_TYPE && rq.bagtype() < BagBase::E_BAG_MAX_TYPE)
        {
            BagBase::item_op_output_type vOut;
            bag_ptr_type  pBag = GetBag(rq.bagtype());
            if (nullptr == pBag)
            {
                rsp.set_result(OR_NULL_PTR);
                return;
            }
            std::size_t nOldBagSize = pBag->GetItemSize();
            rsp.set_result(pBag->UseItem(p));
            OR_CHECK_RESULT_RETURN_VOID(rsp.result());
            if (!vOut.empty())
            {
                rsp.mutable_item()->CopyFrom(vOut[0]);
                if (rq.bagtype() == BagBase::E_BAG_PET_TYPE)
                {
                    rsp.mutable_nowitem()->CopyFrom(vOut[1]);
                }
            }
        }
        else
        {
            rsp.set_result(OR_BAG_TYPE_ERROR);
        }
    }

    void BagModule::RpcSync(BagRpcBagSyncAsk & rq, BagRpcBagSyncReply & rsp)
    {
        for (int32_t i = 0; i < BagBase::E_BAG_MAX_TYPE; ++i)
        {
            bag_ptr_type  pBag = GetBag(i);
            if (nullptr == pBag)
            {
                continue;
            }
            pBag->Tidy();
            pBag->ClearChangeItem();
            BagData* pBagData = rsp.add_bags();
            pBag->ToClientPb(*pBagData);
        }
        rsp.set_result(OR_OK);
    }

    void BagModule::RpcSync(ItemOperationRpcItemResolveSettingSyncAsk & rq, ItemOperationRpcItemResolveSettingSyncReply & rsp)
    {
        resolve_unit_type::iterator it = resolve_unit_.find(rq.settingtype());
        if (it == resolve_unit_.end())
        {
            rsp.set_result(OR_BAG_RESOLVE_TYPE_ERROR);
            return;
        }

        for (int32_t i = 0; i < it->second.resolvelist_size(); ++i)
        {
            ::ItemResolveData* pData = rsp.add_setting();
            pData->set_qulity(it->second.resolvelist(i).qulity());
            pData->set_isresolve(it->second.resolvelist(i).isresolve());
        }
        rsp.set_settingtype(rq.settingtype());
        rsp.set_result(OR_OK);
    }

    void BagModule::RpcSaleItem(BagRpcSaleItemAsk & rq, BagRpcSaleItemReply & rsp, ItemParam & p)
    {
        if (rq.bagtype() >= BagBase::BagTypeEnum::E_BAG_TYPE && rq.bagtype() < BagBase::BagTypeEnum::E_BAG_MAX_TYPE)
        {
            BagBase::item_op_output_type vOut;
            bag_ptr_type  pBag = GetBag(rq.bagtype());
            if (nullptr == pBag)
            {
                rsp.set_result(OR_NULL_PTR);
                return;
            }
            std::size_t nOldBagSize = pBag->GetItemSize();
            rsp.set_result(pBag->SaleItem(p));
            OR_CHECK_RESULT_RETURN_VOID(rsp.result());
            if (!pBag->GetChangedItem().empty())
            {
                rsp.mutable_item()->CopyFrom(*pBag->GetChangedItem().begin());
            }
        }
        else
        {
            rsp.set_result(OR_BAG_TYPE_ERROR);
        }
    }

    void BagModule::RpcLockItem(BagRpcLockItemAsk &rq, BagRpcLockItemReply & rsp, ItemParam & p)
    {
        if (rq.bagtype() >= BagBase::BagTypeEnum::E_BAG_TYPE && rq.bagtype() < BagBase::BagTypeEnum::E_BAG_MAX_TYPE)
        {
            BagBase::item_op_output_type vOut;
            bag_ptr_type  pBag = GetBag(rq.bagtype());
            rsp.set_result(pBag->SetLock(p));
            pBag->ToClientItemPb(*rsp.mutable_item(), p.m_nItemPos);
            OR_CHECK_RESULT_RETURN_VOID(rsp.result());
        }
        else
        {
            rsp.set_result(OR_BAG_TYPE_ERROR);
        }
    }

    void BagModule::RpcDeblocking(BagRpcDeblockingAsk & rq, BagRpcDeblockingReply & rsp)
    {
        bag_ptr_type  pBag = GetBag(rq.bagtype());
        if (NULL == pBag)
        {
            rsp.set_result(OR_BAG_NULL_PTR_ERROR);
            return;
        }
        rsp.set_bagtype(rq.bagtype());
        int32_t current_capacity = pBag->GetCurCapacity();
        rsp.set_result(pBag->Deblocking());
        OR_CHECK_RESULT_RETURN_VOID(rsp.result());
        for (size_t i = current_capacity; i < pBag->GetCurCapacity(); ++i)
        {
            pBag->OnItemChange(i);
        }
        rsp.set_curcapacity((int32_t)pBag->GetCurCapacity());
        rsp.set_result(OR_OK);
    }

    void BagModule::RpcTakeItem(BagRpcTakeItemAsk &rq, BagRpcTakeItemReply &rsp)
    {
        BagModule::bag_ptr_type pFromBag = GetBag(rq.frombagtype());
        BagModule::bag_ptr_type pToBag = GetBag(rq.tobagtype());
        if (NULL == pToBag || NULL == pFromBag)
        {
            rsp.set_result(OR_BAG_NULL_PTR_ERROR);
            return;
        }

        ItemParam p{ 0, 1, rq.frompos(), 1, true };
        rsp.set_result(TakeItem(rq.frombagtype(), rq.tobagtype(), p));
        OR_CHECK_RESULT_RETURN_VOID(rsp.result());

        BagBase::item_type pDelItem = pFromBag->GetItem(rq.frompos());
        if (pDelItem != NULL)
        {
            pDelItem->ToClientPb(*(rsp.mutable_fromitemobj()->mutable_itemdata()));
            rsp.mutable_fromitemobj()->set_pos(rq.frompos());
        }

        /* for (auto it : pToBag->GetChangedItem())
         {
             rsp.mutable_toitemobj()->CopyFrom(it);
         }*/
        for (auto it : pToBag->GetChangedItem())
        {
            rsp.mutable_toitemobj()->CopyFrom(it);
        }

        rsp.set_result(OR_OK);
    }

    void BagModule::RpcTidy(BagRpcTidyAsk &rq, BagRpcTidyReply &rsp)
    {
        BagModule::bag_ptr_type pBag = GetBag(rq.bagtype());

        if (NULL == pBag)
        {
            rsp.set_result(OR_BAG_NULL_PTR_ERROR);
            return;
        }
        rsp.set_result(pBag->Tidy());
        OR_CHECK_RESULT_RETURN_VOID(rsp.result());
        pBag->ToClientPb(*rsp.mutable_bags());
        rsp.set_result(OR_OK);
    }

    void BagModule::RpcGetAllItemFromTempBag(BagRpcTakeAllBackBagsAsk &rq, BagRpcTakeAllBackBagsReply&rsp)
    {
        BagModule::bag_ptr_type pFromBag = GetBag(BagBase::E_BAG_TEMP_TYPE);
        BagModule::bag_ptr_type pToBag = GetBag(BagBase::E_BAG_TYPE);
        if (NULL == pToBag || NULL == pFromBag)
        {
            rsp.set_result(OR_BAG_NULL_PTR_ERROR);
            return;
        }
        rsp.set_result(TakeAllItemFromTempBag());
        OR_CHECK_RESULT_RETURN_VOID(rsp.result());

        rsp.set_result(OR_OK);
    }

    void BagModule::RpcMergeItem(BagRpcMergeItemAsk &rq, BagRpcMergeItemReply&rsp)
    {
        if (rq.bagtype() >= BagBase::BagTypeEnum::E_BAG_TYPE && rq.bagtype() < BagBase::BagTypeEnum::E_BAG_MAX_TYPE)
        {
            BagModule::bag_ptr_type pToBag = GetBag(rq.bagtype());
            ItemParam p{ 0, 1, rq.pos(), 1, true };
            p.m_bClientRpc = true;
            rsp.set_result(pToBag->MergeItem(p));
            if (OR_OK == rsp.result())
            {
                pToBag->ToClientPb(*rsp.mutable_bags());
            }
        }
        else
        {
            rsp.set_result(OR_BAG_TYPE_ERROR);
        }
    }

    void BagModule::RpcItemResolve(ItemOperationRpcItemResolveAsk &rq, ItemOperationRpcItemResolveReply&rsp)
    {
        BagModule::bag_ptr_type pBag = GetBag(BagBase::E_BAG_TYPE);
        i32_map_type  oConf;
        ItemParam p;
        p.m_nItemPos = rq.itempos();
        p.m_nUseSize = rq.count();
        p.m_nClassId = EconomisClassEnum::E_Bag;
        p.m_nWayId = EconomisClassEnum::E_Bag_Break;
        p.m_bClientBreak = true;
#ifndef __TEST__
        p.m_pPlayerScript = (IScriptHost*)m_pHuman;
#endif // !__TEST__

        rsp.set_result(pBag->BreakItem(p, oConf));
        OR_CHECK_RESULT_RETURN_VOID(rsp.result());

        for (auto it : oConf)
        {
            ::ItemSimpleData* pPb = rsp.add_itemsimpledatalist();
            pPb->set_confid(it.first);
            pPb->set_count(it.second);
        }
    }

    void BagModule::RpcChangeSetting(ItemOperationRpcChangeSettingAsk &rq, ItemOperationRpcChangeSettingReply&rsp)
    {
        ItemAutoOperatorData pb;
        for (int32_t i = 0; i < rq.settinglist_size() && CHECK_FOR_INDEX(i); ++i)
        {
            ::ItemResolveData* pPb = pb.add_autosaledisassemble();
            pPb->set_qulity(rq.settinglist(i).qulity());
            pPb->set_isresolve(rq.settinglist(i).isresolve());
        }
        rsp.set_settingtype(rq.settingtype());
        rsp.set_result(SetItemAutoOperatorData(pb, rq.settingtype()));
        OR_CHECK_RESULT_RETURN_VOID(rsp.result());
    }

    void BagModule::RpcBatchResolve(ItemOperationRpcBatchResolveAsk &rq, ItemOperationRpcBatchResolveReply&rsp)
    {
        BreakInParam intparam;
        intparam.auto_type_ = rq.settingtype();
        BreakOutParam outparam;
        outparam.use_return_ = true;
        rsp.set_result(SaleBreakAllItem(rq.qulity(), intparam, outparam));
        for (auto&it : outparam.break_item_map_)
        {
            auto pb = rsp.add_configlist();
            pb->set_keyid(it.first);
            pb->set_keyvalue(it.second);
        }
        OR_CHECK_RESULT_RETURN_VOID(rsp.result());
    }

    const HumanBag::BagBase::item_change_type & BagModule::GetNewAddItem()
    {
        auto p_bag = GetBag(BagBase::BagTypeEnum::E_BAG_TYPE);
        if (nullptr == p_bag)
        {
            static HumanBag::BagBase::item_change_type  v;
            return v;
        }
        return p_bag->GetChangedItem();
    }

    const HumanBag::BagBase::item_change_type & BagModule::GetChangedItem()
    {
        auto p_bag = GetBag(BagBase::BagTypeEnum::E_BAG_TYPE);
        if (nullptr == p_bag)
        {
            static HumanBag::BagBase::item_change_type  v;
            return v;
        }
        return p_bag->GetChangedItem();
    }

    const ItemObj BagModule::GetFirstChangeItem()
    {
        if (GetChangedItem().empty())
        {
            static ItemObj o;
            return o;
        }
        return *GetChangedItem().begin();
    }

    ItemObj BagModule::GetNewOneAddedOrChangeItem()
    {
        if (GetNewAddItem().empty() && GetChangedItem().empty())
        {
            return ItemObj();
        }
        if (!GetNewAddItem().empty())
        {
            return *GetNewAddItem().begin();
        }
        if (!GetChangedItem().empty())
        {
            return *GetChangedItem().begin();
        }
        return ItemObj();
    }

    void BagModule::SetBagAllChange()
    {
        for (auto& it : m_vBags)
        {
            it.second->OnAllChange();
        }
    }

    void BagModule::DoAllChangeAndNotifyClient()
    {
        ClearChangeItem();
        SetBagAllChange();
        NotifyClientBagChangeMsg();
    }

    void BagModule::NotifyClientBagChangeMsg()
    {
        for (auto &it : m_vBags)
        {
            it.second->NotifyItemChange();
        }
    }

    void BagModule::ClearChangeItem()
    {
        for (auto &it : m_vBags)
        {
            it.second->ClearChangeItem();
        }
    }

    int32_t BagModule::CanOverlayItem(const i32_map_type & oConfigList, CreateItemParam& param)
    {
        std::map<int32_t, i32_map_type> mToBagConfigList = MakeConsumeListWithBagType(oConfigList);
        for (auto& it : mToBagConfigList)
        {
            bag_ptr_type pBag = GetBag(it.first);
            if (nullptr == pBag)
            {
                return OR_BAG_TYPE_ERROR;
            }
            int32_t nResult = pBag->CanOverlayItem(it.second, param);
            if (OR_OK != nResult)
            {
                return nResult;
            }
        }
        return OR_OK;
    }

    int32_t BagModule::CanOverlayItem(const i32_map_type & oConfigList)
    {
        CreateItemParam param;
        return CanOverlayItem(oConfigList, param);
    }

    void BagModule::OnRemoveItem(const ItemParam& param)
    {
#ifdef __TEST__
        //std::cout << " On Remove item :" << nGuid << std::endl;
#endif
        emc::i().emit<ItemParam>(emid(), param);
        m_vRedGuids.erase(param.m_nItemGuid);
    }

    void BagModule::Init()
    {
        m_vBags.emplace(BagBase::E_BAG_TYPE, new Bag(BagBase::E_BAG_TYPE));
        m_vBags.emplace(BagBase::E_BAG_STORAGE_TYPE, new StorageBag(BagBase::E_BAG_STORAGE_TYPE));
        m_vBags.emplace(BagBase::E_BAG_TEMP_TYPE, new TempBag(BagBase::E_BAG_TEMP_TYPE));
        m_vBags.emplace(BagBase::E_BAG_PET_TYPE, new PetBag(BagBase::E_BAG_PET_TYPE));

        //背包
        can_put_into_type v;
        v.emplace(BagBase::E_BAG_STORAGE_TYPE);
        m_Puts.emplace(BagBase::E_BAG_TYPE, v);

        //厂库
        v.clear();
        v.emplace(BagBase::E_BAG_TYPE);
        m_Puts.emplace(BagBase::E_BAG_STORAGE_TYPE, v);

        v.clear();
        v.emplace(BagBase::E_BAG_TYPE);
        m_Puts.emplace(BagBase::E_BAG_TEMP_TYPE, v);

        for (auto it : m_vBags)
        {
            it.second->emid(emid());
            it.second->SetReadPoint(std::bind(&BagModule::HasRedPoint, this, std::placeholders::_1));
            it.second->SetClearRedPoint(std::bind(&BagModule::ClearRedPoint, this));
            it.second->SetAddGuid(std::bind(&BagModule::AddRedPoint, this, std::placeholders::_1));
            if (it.second->GetBagType() == BagBase::E_BAG_STORAGE_TYPE ||
                it.second->GetBagType() == BagBase::E_BAG_TEMP_TYPE)
            {
                BagBase::void_item_guid_callback_type b;
                it.second->SetAddGuid(b);
            }

            it.second->SetHuman(m_pHuman);
            it.second->SetAutoSaleCB(std::bind(&BagModule::SaleBreakAllItemNoParam, this));
        }
        resolve_unit_.clear();
        InitResolvePb(Item::E_EQUIPMENT);
        InitResolvePb(Item::E_SOUL_STONE);
    }

    void BagModule::InitResolvePb(int32_t equip_type)
    {
        if (equip_type < Item::E_CURRENCY || equip_type >= Item::E_TYPE_MAX)
        {
            return;
        }
        ResolveUnit pb;
        pb.set_itemtype(equip_type);
        for (int32_t i = Item::E_Quailty0; i < Item::E_Quailty3; ++i)
        {
            ::ItemResolveData* pData = pb.add_resolvelist();
            pData->set_qulity(i);
            pData->set_isresolve(false);
        }
        resolve_unit_.emplace(equip_type, pb);
    }

    bool BagModule::HasRedPoint(GUID_t nGuid)
    {
        return m_vRedGuids.find(nGuid) != m_vRedGuids.end();
    }

    void BagModule::AddRedPoint(GUID_t nGuid)
    {
        m_vRedGuids.emplace(nGuid);
    }

    void BagModule::ClearRedPoint()
    {
        m_vRedGuids.clear();
    }

    bool BagModule::CheckAutoBreak(BagBase::item_type & pItem)
    {
        if (nullptr == pItem)
        {
            return false;
        }
        if (pItem->IsEmptyItem())
        {
            return true;
        }
        ItemParam  p;
        if (!pItem->CanBreak((IScriptHost*)m_pHuman, p))
        {
            return false;
        }

        return true;
    }
} // namespace HumanBag