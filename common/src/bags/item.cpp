#include "Item.h"

#include "ItemPet.h"

#include <vector>

#ifdef __TEST__
#include "../testdef.h"
#endif // __TEST__
#include "CommonLogic/Bag/Bag.h"
#include "GenCode/GameDefine_Result.h"
#include "Game/Snowflake.h"
#include "Game/DK_Random.h"
#include "GenCode/BaseAttr/BaseAttrModule.h"
#include "GenCode/Config/StringItemCfg.h"
#include "CommonLogic/Equipment/Equipment.h"
#include "CommonLogic/Emblem/Emblem.h"
#include "GameEnum.h"
#include "CommonLogic/SoulStone/SoulStone.h"
#include "GameStruct/GameStruct_Resource.h"
#include "GenCode/Config/ItemOptionalCfg.h"
#include "GenCode/Config/PetRankCfg.h"
#include "GenCode/Config/ItemProtoCfg.h"
#include "CommonLogic/FunctionSwitch/FunctionSwtich.h"
#include "CommonLogic/Equipment/MountEquipment.h"

#include "CommonLogic/GameEvent/GameEvent.h"
#include "CommonLogic/EventStruct/CommonEventStruct.h"
#include "GenCode/StringNotice.h"

#ifdef __TEST__
int  Equip::OnUse(class HumanBag::ItemParam &)
{
    return OR_OK;
}
int  Equip::GetLevel(void)const
{
    return 1;
}
#endif // __TEST__

namespace HumanBag
{
    int32_t Item::CheckUse(ItemParam & nUseCondition)
    {
        OR_CHECK_RESULT_RETURN_RET(CheckLockUse());
        OR_CHECK_RESULT_RETURN_RET(EnoughLevel(nUseCondition));
        OR_CHECK_RESULT_RETURN_RET(CheckEnoughOverlaySize(nUseCondition));
        OR_CHECK_RESULT(FunctionSwitchManager::Instance().CheckOpen(OR_FUNCTION_ITEM_CLOSE, GetConfId()));
#ifndef __TEST__
        OR_CHECK_RESULT_RETURN_RET(CheckUseTarget(nUseCondition));
        if (NULL == m_pItemProtoElement)
        {
            return OR_TABLE_DATA_ERROR;
        }
        if (m_pItemProtoElement->server_script_id.empty())
        {
            return OR_BAG_USE_ITEM_ERROR;
        }
        //check cd

        if (NULL != nUseCondition.m_pPlayerScript)
        {
            OR_CHECK_RESULT_RETURN_RET(nUseCondition.m_pPlayerScript->CheckUse(m_pItemProtoElement));
        }
        int32_t ret = OR_OK;
        Call("CheckUse", this, nUseCondition.m_pPlayerScript, nUseCondition.m_pTagetScript, m_pItemProtoElement->effect_param, nUseCondition.m_nUseSize, ret);
        OR_CHECK_RESULT(ret);

        if (m_pItemProtoElement->group > 0 && nUseCondition.m_pPlayerScript)
        {
            OR_CHECK_RESULT_RETURN_RET(nUseCondition.m_pPlayerScript->CheckCD(m_pItemProtoElement->group, GetCooldownTime()));
        }

#endif // !__TEST__

        return OR_OK;
    }

    int32_t Item::CheckSale(ItemParam & nUseCondition)
    {
        if (m_oData.islock())
        {
            return OR_BAG_ITEM_LOCAK;
        }
        if (NULL == GetItemProtoElement())
        {
            return OR_TABLE_INDEX;
        }

        if (GetItemProtoElement()->price < 0)
        {
            return OR_SALE_SYSTEM_PRICE_ERROR;
        }
        OR_CHECK_RESULT_RETURN_RET(CheckEnoughOverlaySize(nUseCondition));
        return OR_OK;
    }
    int32_t Item::CheckUseTarget(ItemParam & nUseCondition)
    {
        const ItemProtoElement * pElement = GetItemProtoElement();
        if (!pElement)
        {
            return OR_TABLE_INDEX;
        }
        if (!nUseCondition.m_pPlayerScript || !nUseCondition.m_pTagetScript)
        {
            return OR_NULL_PTR;
        }

        bool isEnemy = false;
        int relation = EIUR_SELF;
        if (nUseCondition.m_pTagetScript != nUseCondition.m_pPlayerScript)
        {
            isEnemy = nUseCondition.m_pPlayerScript->IsEnemy(nUseCondition.m_pTagetScript);
            relation = isEnemy ? EIUR_ENEMY : EIUR_FRIEND;
        }

        uint64_t obj_type = (nUseCondition.m_pTagetScript->GetGuid());
        int targe_type = -1;
        if (obj_type == E_GUID_TYPE_ROLE || obj_type == E_GUID_TYPE_ROBOT)
        {
            targe_type = EIUTT_HUMAN;
        }
        else if (obj_type == E_GUID_TYPE_MONSTER || obj_type == E_GUID_TYPE_PET)
        {
            targe_type = EIUTT_NPC;
        }
        else if (obj_type == E_GUID_TYPE_EVENT_OBJ)
        {
            targe_type = EIUTT_EVENTOBJ;
        }
        if (relation != pElement->relations)
        {
            switch (relation)
            {
            case EIUR_SELF:
                return OR_BAG_ITEM_USE_NOT_SELF;
                break;
            case EIUR_ENEMY:
                return OR_BAG_ITEM_USE_NOT_ENEMY;
                break;
            case EIUR_FRIEND:
                return OR_BAG_ITEM_USE_NOT_FRIEND;
                break;
            default:
                break;
            }
        }
        if (targe_type != pElement->target_type)
        {
            switch (targe_type)
            {
            case EIUTT_HUMAN:
                return OR_BAG_ITEM_USE_NOT_PLAYER;
                break;
            case EIUTT_NPC:
                return OR_BAG_ITEM_USE_NOT_NPC;
                break;
            case EIUTT_EVENTOBJ:
                return OR_BAG_ITEM_USE_NOT_EVENTOBJ;
                break;
            default:
                break;
            }
        }
        if (pElement->type == Item::E_POTION && pElement->subtype == 1
            && targe_type == EIUTT_HUMAN
            && nUseCondition.m_pTagetScript->GetDuelStatus() == EDS_Dueling)
        {//决斗中不能吃药
            return OR_BAG_ITEM_USE_TARGET_DUELING;
        }

        return OR_OK;
    }

    int32_t Item::EnoughLevel(ItemParam & nUseCondition)
    {
        if (NULL == GetItemProtoElement())
        {
            return OR_TABLE_INDEX;
        }
#ifndef __TEST__
        if (nullptr != nUseCondition.m_pPlayerScript)
        {
            nUseCondition.m_nPlayerLevel = nUseCondition.m_pPlayerScript->GetLevel();
        }

#endif
        if (nUseCondition.m_nPlayerLevel < GetItemProtoElement()->level)
        {
            return OR_USEITEM_LEVEL_ERROR;
        }
        return OR_OK;
    }

    int32_t Item::CheckEnoughOverlaySize(ItemParam & nUseCondition)
    {
        if (nUseCondition.m_nUseSize <= 0)
        {
            return OR_NEGATIVE_COUNT;
        }

        if (nUseCondition.m_nUseSize > m_oData.overlaycount())
        {
            return OR_USEITEM_ENOUGH_COUNT_ERROR;
        }
        return OR_OK;
    }

    int32_t Item::EnoughBagCapacity(ItemParam & nUseCondition)
    {
        if (m_bCheckCapacityCallback)
        {
            OR_CHECK_RESULT_RETURN_RET(m_bCheckCapacityCallback(1));
        }
        return OR_OK;
    }

    std::string Item::GetItemName() const
    {
        if (m_pItemProtoElement != nullptr)
        {
            const StringItemElement* pStringItemElement = StringItemTable::Instance().GetElement(m_pItemProtoElement->name);
            if (pStringItemElement != nullptr)
            {
                return pStringItemElement->sc;
            }
        }
        return "";
    }

    Item::Item(const ItemProtoElement * pItemProtoElement, int32_t nCount)
        : m_pItemProtoElement(pItemProtoElement)
    {
        m_oData.set_overlaycount(nCount);
        if (NULL != pItemProtoElement)
        {
            m_oData.set_configid(pItemProtoElement->id);
        }

#ifndef __TEST__
        if (NULL != pItemProtoElement)
        {
            if (!pItemProtoElement->behaviac.empty())
            {
                SetBTPath(pItemProtoElement->behaviac.c_str());
            }
            else if (Script())
            {
                Script()->Load(pItemProtoElement->server_script_id);
            }
        }
#endif // __TEST__
    }

    Item::Item(const ItemData & pb)
    {
        //     m_oData.set_overlaycount(pb.overlaycount());
        //     m_oData.set_configid(pb.configid());
        m_oData.CopyFrom(pb);
        m_pItemProtoElement = nullptr;

        if (pb.configid() != 0)
        {
            const ItemProtoElement * pItemProtoElement = ItemProtoTable::Instance().GetElement(pb.configid());
#ifndef __TEST__
            if (pItemProtoElement != nullptr)
            {
                m_pItemProtoElement = pItemProtoElement;

                if (!pItemProtoElement->behaviac.empty())
                {
                    SetBTPath(pItemProtoElement->behaviac.c_str());
                }
                else if (Script())
                {
                    Script()->Load(pItemProtoElement->server_script_id);
                }
            }
#else
            m_pItemProtoElement = pItemProtoElement;
#endif // __TEST__
        }
    }

    Item::Item()
    {
        m_pItemProtoElement = nullptr;
    }

    Item::~Item()
    {
    }

    void Item::OnLoad(const ItemData & pb)
    {
        if (ItemProtoTable::Instance().HasElement(pb.configid()))
        {
            m_pItemProtoElement = ItemProtoTable::Instance().GetElement(pb.configid());
        }

        m_oData.CopyFrom(pb);
        if (m_oData.configid() <= 0)
        {
            m_oData.set_configid(INVALID_ID);
        }
    }

    void Item::OnSave(ItemData & pb)
    {
        pb.Clear();
        pb.CopyFrom(m_oData);
    }

    void Item::ToClientPb(ItemData & pb)
    {
        pb.Clear();
        pb.CopyFrom(m_oData);
        if (pb.configid() <= 0)
        {
            pb.set_configid(INVALID_ID);
        }
    }

    void Item::ToJson(Json::Value& item)
    {
        if (NULL == GetItemProtoElement())
        {
            return;
        }
        item["item_name"] = GetItemName();
        item["item_id"] = m_pItemProtoElement->id;
        item["item_count"] = GetOverlaySize();
    }

    int32_t Item::OnUse(ItemParam & nUseCondition)
    {
        if (NULL == GetItemProtoElement())
        {
            return OR_TABLE_INDEX;
        }

        OR_CHECK_RESULT_RETURN_RET(DelOverlaySize(nUseCondition.m_nUseSize));

        OnChange(nUseCondition);

        if (IsEmptyItem())
        {
#ifndef __TEST__
            int32_t ret = OR_OK;

            Call("OmEmpty", this, nUseCondition.m_pPlayerScript, nUseCondition.m_pTagetScript, m_pItemProtoElement->effect_param, nUseCondition.m_nUseSize, ret);
#endif // __TEST__
            m_oData.clear_guid();
        }

        return OR_OK;
    }

    int32_t Item::OnRemove(ItemParam & nUseCondition)
    {
        if (NULL == GetItemProtoElement())
        {
            return OR_TABLE_INDEX;
        }
        if (m_oData.islock())
        {
            return OR_BAG_ITEM_LOCAK;
        }

        OR_CHECK_RESULT_RETURN_RET(DelOverlaySize(nUseCondition.m_nUseSize));

        OnChange(nUseCondition);
        return OR_OK;
    }

    INT Item::GetConfId()
    {
        if (nullptr != m_pItemProtoElement)
        {
            return m_pItemProtoElement->id;
        }
        return INVALID_ID;
    }

    int32_t Item::OnSale(ItemParam & nUseCondition)
    {
        if (m_oData.islock())
        {
            return OR_BAG_ITEM_LOCAK;
        }
        OR_CHECK_RESULT_RETURN_RET(DelOverlaySize(nUseCondition.m_nUseSize));

        OnChange(nUseCondition);
        // add gold
#ifndef __TEST__
        ResourceAddParam oAddParam;
        oAddParam.nClassId = EconomisClassEnum::E_Bag;
        oAddParam.nWayId = EconomisClassEnum::E_Bag_Sale;
        if (NULL != nUseCondition.m_pPlayerScript && NULL != m_pItemProtoElement)
        {
            nUseCondition.m_pPlayerScript->AddResource(0, m_pItemProtoElement->price * nUseCondition.m_nUseSize, oAddParam);
        }
#endif // !__TEST__

        return OR_OK;
    }

    void Item::OnChange(ItemParam & nUseCondition)
    {
        if (item_change_callback_)
        {
            item_change_callback_(GetGuid());
        }

        if ( nullptr != nUseCondition.m_pPlayerScript)
        {
            //ItemParam oItemParam;
            nUseCondition.m_nItemGuid = GetGuid();
            nUseCondition.m_nConfigId = GetItemId();
            //oItemParam.nWayId = EconomisClassEnum::E_Bag;
            //oItemParam.nClassId = EconomisClassEnum::E_Bag_UseItem;
            emc::i().emit<ItemParam>(nUseCondition.m_pPlayerScript->emid(), nUseCondition);
        }
    }

    int32_t Item::CheckLockUse()
    {
        if (m_oData.islock())
        {
            return OR_BAG_ITEM_LOCAK;
        }
        return OR_OK;
    }

    int32_t Item::CheckOccupation(int32_t nOccupation) const
    {
        if (nullptr == m_pItemProtoElement)
        {
            return OR_NULL_PTR;
        }
        auto iter = std::find(m_pItemProtoElement->occupation.begin(), m_pItemProtoElement->occupation.end(), nOccupation);
        if (iter == m_pItemProtoElement->occupation.end())
        {
            return OR_ITEM_EQUIP_OUCC_UNFIT;
        }
        return OR_OK;
    }

    int32_t Item::CanMerge()
    {
        if (NULL == m_pItemProtoElement)
        {
            return OR_TABLE_INDEX;
        }

        if (m_pItemProtoElement->stack <= 1)
        {
            return OR_BAG_CANT_MERGE_ITEM;
        }
        return OR_OK;
    }

    int32_t Item::CanTrade()
    {
        if (GetBind())
        {
            return OR_ITEM_BIND_CANT_TRADE;
        }
        if (::time(NULL) < uint32_t(m_oData.salecd()))
        {
            return OR_ITEM_SALE_CD;
        }
        if (GetLock())
        {
            return OR_ITEM_LOCKED;
        }
        return OR_OK;
    }

    int32_t Item::CanBreak(IScriptHost * pIScriptHost, ItemParam & p)
    {
        if (NULL == m_pItemProtoElement)
        {
            return OR_TABLE_INDEX;
        }
        if (!m_pItemProtoElement->can_break)
        {
            return OR_ITEM_CANT_BREAK;
        }

        if (IsRefined() && !p.m_bClientBreak)
        {
            return OR_ITEM_CANT_BREAK;
        }

        if (GetLock())
        {
            return OR_ITEM_CANT_BREAK_LOCK;
        }

#ifndef __TEST__
        if ((Item::ItemEnum::E_EQUIPMENT == GetItemType() ||
            E_SOUL_STONE == GetItemType()) &&
            nullptr != pIScriptHost &&
            !p.m_bClientBreak)
        {
            int32_t nBattleScore = pIScriptHost->GetEquipBattleScore(GetItemType(), GetItemProtoElement()->subtype);
            if (nBattleScore <= 0
                || GetBattleScore() > nBattleScore)
            {
                if (E_SOUL_STONE == GetItemType())
                    return OR_SOULSTONE_CANT_BREAK_BATTLE_SCORE;
                else
                    return OR_ITEM_CANT_BREAK_BATTLE_SCORE;
            }
        }
#endif//__TEST__
        return OR_OK;
    }

    int32_t Item::CanEquip(int nPlayerLevel, int nOccupation) const
    {
        if (NULL == m_pItemProtoElement)
        {
            return OR_NULL_PTR;
        }

        if (m_pItemProtoElement->type != E_EQUIPMENT)
        {
            ELOG("OR_ITEM_WRONG_TYPE");
            return OR_ITEM_WRONG_TYPE;
        }
        if (nPlayerLevel < m_pItemProtoElement->level)
        {
            ELOG("OR_ITEM_EQUIP_LVL_OVTOP");
            return OR_ITEM_EQUIP_LVL_OVTOP;
        }

        if (OR_OK != CheckOccupation(nOccupation))
        {
            ELOG("OR_ITEM_EQUIP_OUCC_UNFIT");
            return OR_ITEM_EQUIP_OUCC_UNFIT;
        }
        return OR_OK;
    }

    int32_t Item::CanEquipEmblem(int nPlayerLevel) const
    {
        if (NULL == m_pItemProtoElement)
        {
            return OR_NULL_PTR;
        }

        if (m_pItemProtoElement->type != E_EMBLEM)
        {
            ELOG("OR_ITEM_WRONG_TYPE");
            return OR_ITEM_WRONG_TYPE;
        }
        if (nPlayerLevel < m_pItemProtoElement->level)
        {
            ELOG("OR_ITEM_EQUIP_LVL_OVTOP");
            return OR_ITEM_EQUIP_LVL_OVTOP;
        }

        return OR_OK;
    }

    int32_t Item::GetItemType() const
    {
        if (NULL == GetItemProtoElement())
        {
            return -1;
        }
        return GetItemProtoElement()->type;
    }

    bool Item::IsGad() const
    {
        if (GetItemProtoElement())
        {
            return GetItemProtoElement()->subtype == 9;
        }
        return false;
    }

    int32_t Item::GetQuality() const
    {
        if (nullptr != m_pItemProtoElement)
        {
            return m_pItemProtoElement->rank;
        }
        else
        {
            return 0;
        }
    }

    int32_t Item::GetOriginalQuality() const
    {
        return Item::GetQuality();
    }

    Item::item_type Item::Clone()
    {
        CreateItemParam cp;
        cp.m_nConfigId = m_oData.configid();
        cp.m_nCount = m_oData.overlaycount();
        Item::item_type p = ItemFactory::CreateItem(m_oData, cp);
        if (nullptr != p)
        {
            p->SetCheckCapacityCallback(m_bCheckCapacityCallback);
            p->SetRemoveItemCallBack(m_oRemoveItemCb);
        }
        return p;
    }

    int32_t Item::AddOverlaySize(int32_t nOverlaySize, CreateItemParam & cip)
    {
        if (nOverlaySize <= 0)
        {
            return OR_NEGATIVE_COUNT;
        }
        m_oData.set_overlaycount(m_oData.overlaycount() + nOverlaySize);
        AddRedPoint(cip);
        return OR_OK;
    }

    int32_t Item::AddRedPoint(CreateItemParam & cip)
    {
        if (!(cip.nClassId == EconomisClassEnum::E_Emblem ||
            cip.nClassId == EconomisClassEnum::E_OPERATION_LOTTERY ||
            cip.nWayId == EconomisClassEnum::E_Bag_TakeItem))
        {
            if (add_guid_item_callback_)
            {
                add_guid_item_callback_(GetGuid());
            }
        }

        return OR_OK;
    }

    int32_t Item::DelOverlaySize(int32_t nOverlaySize)
    {
        if (nOverlaySize <= 0)
        {
            return OR_NEGATIVE_COUNT;
        }
        if (nOverlaySize > m_oData.overlaycount())
        {
            return OR_USEITEM_ENOUGH_COUNT_ERROR;
        }
        m_oData.set_overlaycount(m_oData.overlaycount() - nOverlaySize);

        return OR_OK;
    }

    bool Item::IsFull() const
    {
        return nullptr == m_pItemProtoElement ? true : GetOverlaySize() >= m_pItemProtoElement->stack;
    }

    int32_t Item::BeforeAddToBagPlaceholdersSize()const
    {
        if (NULL == m_pItemProtoElement)
        {
            return 0;
        }
        return  GetFullPlaceholdersSize() + GetNotFullPlaceholdersSize();
    }

    int32_t Item::GetFullPlaceholdersSize()const
    {
        if (NULL == m_pItemProtoElement)
        {
            return 0;
        }
        return m_oData.overlaycount() / m_pItemProtoElement->stack;
    }

    int32_t Item::GetNotFullPlaceholdersSize()const
    {
        if (NULL == m_pItemProtoElement)
        {
            return 0;
        }
        return m_oData.overlaycount() % m_pItemProtoElement->stack == 0 ? 0 : 1;
    }

    void Item::SetGuid(GUID_t nGuid)
    {
        m_oData.set_guid(nGuid);
    }

    GUID_t Item::GetGuid()const
    {
        return m_oData.guid();
    }

    int32_t Item::GetCooldownGroup() const
    {
        if (GetItemProtoElement() == NULL)
        {
            return 0;
        }
        return GetItemProtoElement()->group;
    }

    int32_t Item::GetCooldownTime() const
    {
        if (GetItemProtoElement() == NULL)
        {
            return 0;
        }
        return GetItemProtoElement()->global_cd;
    }

#ifdef __TEST__
    void Item::TestOutPut() const
    {
        std::cout << "(" << m_pItemProtoElement->type << " " << m_oData.overlaycount() << " " << m_oData.islock() << ")";
    }
#endif // __TEST__

    float Item::GetTotalEffectParam(int32_t nOverLaySize)
    {
        float nF = 0.0f;
        if (NULL == m_pItemProtoElement)
        {
            return nF;
        }
        for (auto it : m_pItemProtoElement->effect_param)
        {
            nF += it;
        }
        return nF * nOverLaySize;
    }

    bool Item::operator<(const  Item & rhs)const
    {
        //http://en.cppreference.com/w/cpp/concept/Compare

        static std::vector<uint16_t> sweight{ 0,5,2,7,3,4,6,1,8,9,10,11,12,13,14,15,16,17,18,19 }; // 礼包>装备>宝石>药品>可使用品>材料

        if (IsEmptyItem() && rhs.IsEmptyItem())
        {
            return this < &rhs;
        }
        else if (IsEmptyItem() && !rhs.IsEmptyItem())
        {
            return false;
        }
        else if (!IsEmptyItem() && rhs.IsEmptyItem())
        {
            return true;
        }

        if (NULL == m_pItemProtoElement)
        {
            return false;
        }

        if (rhs.m_pItemProtoElement == nullptr)
        {
            return false;
        }

        if (m_pItemProtoElement->type == rhs.m_pItemProtoElement->type)
        {
            if (m_pItemProtoElement->type == E_PET)
            {
                if (GetQuality() > rhs.GetQuality())
                {
                    return true;
                }
                else if (GetQuality() < rhs.GetQuality())
                {
                    return false;
                }

                auto pPetRankElement = PetRankTable::Instance().GetElement(GetRank());
                auto pRhsPetRankElement = PetRankTable::Instance().GetElement(rhs.GetRank());
                if (pPetRankElement != nullptr
                    && pRhsPetRankElement != nullptr)
                {
                    if (pPetRankElement->rank_star > pRhsPetRankElement->rank_star)
                    {
                        return true;
                    }
                    else if (pPetRankElement->rank_star < pRhsPetRankElement->rank_star)
                    {
                        return false;
                    }
                }

                if (GetLevel() > rhs.GetLevel())
                {
                    return true;
                }
                else if (GetLevel() < rhs.GetLevel())
                {
                    return false;
                }
                if (GetGuid() > rhs.GetGuid())
                {
                    return true;
                }
                else if (GetGuid() < rhs.GetGuid())
                {
                    return false;
                }

                return false;
            }
        }

        if ((std::size_t)m_pItemProtoElement->type >= sweight.size() || 0 > m_pItemProtoElement->type)
        {
            return false;
        }
        if (sweight[m_pItemProtoElement->type] < sweight[rhs.m_pItemProtoElement->type])
        {
            return true;
        }
        else if (sweight[m_pItemProtoElement->type] > sweight[rhs.m_pItemProtoElement->type])
        {
            return false;
        }

        if ((std::size_t)m_pItemProtoElement->subtype >= sweight.size())
        {
            return false;
        }
        if (m_pItemProtoElement->subtype < rhs.m_pItemProtoElement->subtype)
        {
            return true;
        }
        else if (m_pItemProtoElement->subtype > rhs.m_pItemProtoElement->subtype)
        {
            return false;
        }

        // the same type

        if ((m_pItemProtoElement->type == rhs.m_pItemProtoElement->type)
            && (m_pItemProtoElement->type == E_EQUIPMENT))
        {
        }

        if (m_pItemProtoElement->id > rhs.m_pItemProtoElement->id)
        {
            return true;
        }
        else if (m_pItemProtoElement->id < rhs.m_pItemProtoElement->id)
        {
            return false;
        }
        if (GetBind() > rhs.GetBind())
        {
            return true;
        }
        else if (GetBind() < rhs.GetBind())
        {
            return false;
        }

        if (GetLock() > rhs.GetLock())
        {
            return true;
        }
        else if (GetLock() < rhs.GetLock())
        {
            return false;
        }

        // the same id

        if (m_oData.overlaycount() > (int32_t)rhs.m_oData.overlaycount())
        {
            return true;
        }
        else
        {
            return false;
        }
        if (GetGuid() > rhs.GetGuid())
        {
            return true;
        }
        else if (GetGuid() < rhs.GetGuid())
        {
            return false;
        }

        return false;
    }

    class Gem : public Item
    {
    public:
        using Item::Item;
        virtual int32_t OnUse(ItemParam & nUseCondition)
        {
            return OR_BAG_UES_GEM_ERROR;
            OR_CHECK_RESULT_RETURN_RET(Item::OnUse(nUseCondition));
#ifndef __TEST__
            int32_t ret = OR_OK;
            Call("OnUse", this, nUseCondition.m_pPlayerScript, nUseCondition.m_pTagetScript, m_pItemProtoElement->effect_param, nUseCondition.m_nUseSize, ret);
#endif // __TEST__
            return OR_OK;
        }
    };

    class Material : public Item
    {
    public:
        using Item::Item;
        virtual int32_t OnUse(ItemParam & nUseCondition)
        {
            OR_CHECK_RESULT_RETURN_RET(Item::OnUse(nUseCondition));

            if (NULL == m_pItemProtoElement)
            {
                return OR_TABLE_DATA_ERROR;
            }

            if (m_pItemProtoElement->server_script_id.empty())
            {
                return OR_BAG_UES_MATERIAL_ERROR;
            }
#ifndef __TEST__
            int32_t ret = OR_OK;
            Call("OnUse", this, nUseCondition.m_pPlayerScript, nUseCondition.m_pTagetScript, m_pItemProtoElement->effect_param, nUseCondition.m_nUseSize, ret);
#endif // __TEST__

            return OR_OK;
        }
    };

    class OtherCanUseItem : public Item
    {
    public:
        using Item::Item;
        virtual int32_t OnUse(ItemParam & nUseCondition)
        {
            OR_CHECK_RESULT_RETURN_RET(Item::OnUse(nUseCondition));
#ifndef __TEST__
            int32_t ret = OR_OK;

            if (NULL == m_pItemProtoElement)
            {
                return OR_TABLE_DATA_ERROR;
            }
            Call("OnUse", this, nUseCondition.m_pPlayerScript, nUseCondition.m_pTagetScript, m_pItemProtoElement->effect_param, nUseCondition.m_nUseSize, ret);
#endif // __TEST__
            return OR_OK;
        }
    };

    class Potion : public Item
    {
    public:
        using Item::Item;
        virtual int32_t CheckUse(ItemParam & nUseCondition)
        {
            if (!nUseCondition.m_pTagetScript)
            {
                return OR_NULL_PTR;
            }
            IScriptHost* pScene = nUseCondition.m_pTagetScript->GetSceneStrategyScriptHost();
            if (pScene)
            {
                if (!pScene->IsCanUsePotion())
                    return OR_BAG_ITEM_USE_SCENE_ERROR;
            }
            return Item::CheckUse(nUseCondition);
        }
        virtual int32_t OnUse(ItemParam & nUseCondition)
        {
            OR_CHECK_RESULT_RETURN_RET(Item::OnUse(nUseCondition));
#ifndef __TEST__
            int32_t ret = OR_OK;

            if (NULL == m_pItemProtoElement)
            {
                return OR_TABLE_DATA_ERROR;
            }
            Call("OnUse", this, nUseCondition.m_pPlayerScript, nUseCondition.m_pTagetScript, m_pItemProtoElement->effect_param, nUseCondition.m_nUseSize, ret);
#endif // __TEST__
            return OR_OK;
        }
    };

    class ItemGift : public Item
    {
    public:
        using Item::Item;
        virtual int32_t OnUse(ItemParam & nUseCondition)
        {
            if (NULL == m_pItemProtoElement)
            {
                return OR_TABLE_DATA_ERROR;
            }

            if (ItemFactory::drop_item_canoverlay_callback_ && !m_pItemProtoElement->effect_param.empty())
            {
                OR_CHECK_RESULT_RETURN_RET(ItemFactory::drop_item_canoverlay_callback_((int32_t)*m_pItemProtoElement->effect_param.begin(), nUseCondition.m_nUseSize, nUseCondition.m_pPlayerScript));
            }
            else if (m_bCheckCapacityCallback)
            {
                OR_CHECK_RESULT_RETURN_RET(m_bCheckCapacityCallback((int32_t)m_pItemProtoElement->effect_param.size()));
            }
            
            OR_CHECK_RESULT_RETURN_RET(Item::OnUse(nUseCondition));
            int32_t ret = OR_OK;
#ifndef __TEST__
            Call("OnUse", this, nUseCondition.m_pPlayerScript, nUseCondition.m_pTagetScript, m_pItemProtoElement->effect_param, nUseCondition.m_nUseSize, ret);
#endif // __TEST__
            return ret;
        }
    };

    class ItemOptional : public Item
    {
    public:
        using Item::Item;
        virtual int32_t OnUse(ItemParam & nUseCondition)
        {
            if (NULL == m_pItemProtoElement)
            {
                return OR_TABLE_DATA_ERROR;
            }
            const ItemOptionalElement* p = ItemOptionalTable::Instance().GetElement(m_pItemProtoElement->id);
            if (!p)
            {
                return OR_TABLE_DATA_ERROR;
            }

            const std::vector<int>* arrOp[] = { &p->reward1,&p->reward2, &p->reward3, &p->reward4, &p->reward5, &p->reward6, &p->reward7, &p->reward8, &p->reward9, &p->reward10 };

            if (nUseCondition.m_nOptional >= 10)
            {//表只有0～9个选项最多
                return OR_REQ_PARAM_ERROR;
            }
            const std::vector<int>* pOpInfo = arrOp[nUseCondition.m_nOptional];
            if (!pOpInfo || pOpInfo->size() != 4)
            {//道具配置方式（道具ID|道具数量|是否中转|可否交易）
                return OR_TABLE_DATA_ERROR;
            }
            //检查下要获得的
            int ConfigId = (*pOpInfo)[0];
            const ItemProtoElement* pItemInfo = ItemProtoTable::Instance().GetElement(ConfigId);
            if (!pItemInfo)
            {
                ELOG("Item Id:%d not exist in ItemProtoTable", (*pOpInfo)[0]);
                return OR_TABLE_DATA_ERROR;
            }
            if (!nUseCondition.m_pBag)
            {
                ELOG("bag pointer is null");
                return OR_TABLE_DATA_ERROR;
            }

            int emptySize = (int)ceil((float)(*pOpInfo)[1] / pItemInfo->stack);
            if (pItemInfo->stack > 1)
            {
                int Ret = (nUseCondition.m_pBag->CanOverlayItem(ConfigId, (*pOpInfo)[1]) || m_bCheckCapacityCallback(emptySize));
                OR_CHECK_RESULT_RETURN_RET(Ret);
            }
            else
            {
                OR_CHECK_RESULT_RETURN_RET(m_bCheckCapacityCallback(emptySize));
            }

            OR_CHECK_RESULT_RETURN_RET(Item::OnUse(nUseCondition));
#ifndef __TEST__
            int32_t ret = OR_OK;
            Call("OnUse", this, nUseCondition.m_pPlayerScript, nUseCondition.m_pTagetScript, *pOpInfo, nUseCondition.m_nUseSize, ret);
#endif // __TEST__
            return OR_OK;
        }
    };

    class ItemPratice : public Item
    {
    public:
        using Item::Item;

        virtual int32_t OnUse(ItemParam & nUseCondition)
        {
            if (NULL == m_pItemProtoElement)
            {
                return OR_TABLE_DATA_ERROR;
            }
            const ItemOptionalElement* p = ItemOptionalTable::Instance().GetElement(m_pItemProtoElement->id);
            if (!p)
            {
                return OR_TABLE_DATA_ERROR;
            }
            OR_CHECK_RESULT_RETURN_RET(Item::OnUse(nUseCondition));

#ifndef __TEST__

#endif // __TEST__
            return OR_OK;
        }
    };

    class ItemConsumer : public Item
    {
    public:
        using Item::Item;
        virtual int32_t OnUse(ItemParam & nUseCondition)
        {
            OR_CHECK_RESULT_RETURN_RET(Item::OnUse(nUseCondition));
#ifndef __TEST__
            int32_t ret = OR_OK;

            if (NULL == m_pItemProtoElement)
            {
                return OR_TABLE_DATA_ERROR;
            }
            Call("OnUse", this, nUseCondition.m_pPlayerScript, nUseCondition.m_pTagetScript, m_pItemProtoElement->effect_param, nUseCondition.m_nUseSize, ret);
#endif // __TEST__
            return OR_OK;
        }
    };
    class ItemPetSkillBook : public Item
    {
    public:
        using Item::Item;
        virtual int32_t OnUse(ItemParam & nUseCondition)
        {
            OR_CHECK_RESULT_RETURN_RET(Item::OnUse(nUseCondition));
#ifndef __TEST__
            int32_t ret = OR_OK;

            if (NULL == m_pItemProtoElement)
            {
                return OR_TABLE_DATA_ERROR;
            }
            Call("OnUse", this, nUseCondition.m_pPlayerScript, nUseCondition.m_pTagetScript, m_pItemProtoElement->effect_param, nUseCondition.m_nUseSize, ret);
#endif // __TEST__
            return OR_OK;
        }
    };
    class ItemSoulStone :public Item
    {
        using Item::Item;
        virtual int32_t OnUse(ItemParam & nUseCondition)
        {
            OR_CHECK_RESULT_RETURN_RET(Item::OnUse(nUseCondition));
#ifndef __TEST__
            int32_t ret = OR_OK;
            if (NULL == m_pItemProtoElement)
            {
                return OR_TABLE_DATA_ERROR;
            }
            Call("OnUse", this, nUseCondition.m_pPlayerScript, nUseCondition.m_pTagetScript, m_pItemProtoElement->effect_param, nUseCondition.m_nUseSize, ret);
#endif // __TEST__
            return OR_OK;
        }
    };

    class ItemMount :public Item
    {
        using Item::Item;
        virtual int32_t OnUse(ItemParam & nUseCondition)
        {
            OR_CHECK_RESULT_RETURN_RET(Item::OnUse(nUseCondition));
            if (nullptr == m_pItemProtoElement)
            {
                return OR_TABLE_DATA_ERROR;
            }
            int32_t ret = OR_OK;
            Call("OnUse", this, nUseCondition.m_pPlayerScript, nUseCondition.m_pTagetScript, m_pItemProtoElement->effect_param, nUseCondition.m_nUseSize, ret);
            return ret;
        }
    };

    class ItemNormal : public Item
    {
    public:
        using Item::Item;
        virtual int32_t OnUse(ItemParam & nUseCondition)
        {
            return OR_BAG_USE_ITEM_ERROR;
        }
    };

    ItemFactory::i32_i32_callback_type ItemFactory::get_item_type_callback_;
    ItemFactory::float_player_i32_callback_type ItemFactory::drop_item_canoverlay_callback_;

    class NullItem : public Item
    {
    public:
        NullItem()
        {
            m_oData.set_overlaycount(0);
        }
    };

    nid_generator ItemFactory::m_item_ids;
    bool     ItemFactory::m_GuidInc = false;
    bool     ItemFactory::m_Inited = false;
    ItemFactory::item_shared_ptr ItemFactory::CreateItem(ItemData & pb, CreateItemParam & p)
    {
        p.m_nConfigId = pb.configid();

        p.m_nCount = pb.overlaycount();
        GUID_t nGuid = INVALID_ID;
        p.m_pItemData = &pb;

        ItemFactory::item_shared_ptr pItem = CreateItem(p);
        if (NULL == pItem)
        {
            return pItem;
        }
        pItem->OnLoad(pb);
        if (pItem->GetGuid() == INVALID_ID)
        {
            nGuid = ItemFactory::GenerateGuid();
            if (nGuid != INVALID_ID)
            {//使用新的guid
                pItem->SetGuid(nGuid);
            }
        }
        if (p.m_vBind >= 0)
        {
            pItem->SetBindRule(p.m_vBind);
            if (p.m_vBind == E_UNBIND || p.m_vBind == E_EQUIPBIND)
            {//默认是绑定
                pItem->SetBind(false);
            }
            else
            {
                pItem->SetBind(true);
            }
        }
        return pItem;
    }

    /*Snowflake & ItemFactory::GetGuidGenertor()
    {
        static Snowflake sItemSnowflake;
        return sItemSnowflake;
    }*/

    GUID_t ItemFactory::GenerateGuid()
    {
#ifndef __TEST__

        /*if (!m_pGameRedis)
        {
            return INVALID_ID;
        }*/
        uint64_t id = m_item_ids.get_next();
        uint64_t guid = MAKE_OBJECT_GUID(ZONE_ID, nid_generator::m_world_id, E_GUID_TYPE_ITEM, nid_generator::m_server_id, m_item_ids.generate());
        uint64_t guid_key = GUID_DB_KEY(ZONE_ID, nid_generator::m_world_id, E_GUID_TYPE_ITEM, nid_generator::m_server_id);

        m_GuidInc = true;
        //存下
        /*GuidDB oGuid;
        oGuid.set_guid(guid_key);
        oGuid.set_maxguid(id);
        m_pGameRedis->setGuidData(guid_key, &oGuid);
        m_pGameRedis->setMarkGuidSave(guid_key);*/
        return guid;
#else
        static Snowflake sItemSnowflake;
        GUID_t g;
        return sItemSnowflake.generate(g);
#endif//__TEST__

        return 0;
    }

    void ItemFactory::Init(uint64_t id)
    {
        if (false == m_Inited)
        {
            m_item_ids.set(id);
            m_Inited = true;
            ILOG("Item GuidGenerator init ok! serverid:%llu,worid:%llu,id:%llu", nid_generator::m_server_id, nid_generator::m_world_id, id);
        }
    }

    void ItemFactory::SetGetCurrencyCallBackType(const i32_i32_callback_type& get_item_type_callback)
    {
        get_item_type_callback_ = get_item_type_callback;
    }

    void ItemFactory::SetGetDropCanoverlay(const float_player_i32_callback_type& drop_item_canoverlay_callback)
    {
        drop_item_canoverlay_callback_ = drop_item_canoverlay_callback;
    }

    bool ItemFactory::AddCurrency(const CreateItemParam & pCreateItemParam)
{
        if (nullptr != pCreateItemParam.m_pItemData)
        {
            return false;
        }
#ifndef __TEST__
    const ItemProtoElement * pItemProtoElement = ItemProtoTable::Instance().GetElement(pCreateItemParam.m_nConfigId);
    if (pItemProtoElement == NULL)
    {
        return false;
    }

    switch (pItemProtoElement->type)
    {
        case Item::E_CURRENCY:
        {
            if (nullptr != pCreateItemParam.pPlayerScript &&
                get_item_type_callback_ &&
                !pItemProtoElement->effect_param.empty())
            {
                ResourceAddParam param;
                param.nClassId = pCreateItemParam.nClassId;
                param.nWayId = pCreateItemParam.nWayId;
                int64_t add_value = *pItemProtoElement->effect_param.begin() * pCreateItemParam.m_nCount;
                pCreateItemParam.pPlayerScript->AddResource(get_item_type_callback_(pItemProtoElement->id),
                    add_value, param);

                auto p_orig = ItemProtoTable::Instance().GetElement(pCreateItemParam.orginal_break_config_id);
                
                if (param.nWayId == EconomisClassEnum::E_Bag_Break && 
                    nullptr != pCreateItemParam.pPlayerScript && 
                    nullptr != p_orig)
                {
                    emc::i().emit<ChatNoticeES>(pCreateItemParam.pPlayerScript->emid(),
                        ChatNoticeES{ E_STRINGUI_UI_BREAK_ITEM, ChatChannel::CHAT_CHANNEL_SYSTEM, {std::to_string(p_orig->id), std::to_string(add_value)} });
                    emc::i().emit<TipNoticeES>(pCreateItemParam.pPlayerScript->emid(), 
                        TipNoticeES{ E_NOTICE_BREAK_ITEM, {std::to_string(p_orig->id), std::to_string(add_value)} });
                }

                if (EXEPERICE_ITEMID == pItemProtoElement->id)
                {
                    IScriptHost* pHost = pCreateItemParam.pPlayerScript;
                    if (nullptr != pHost)
                    {
                        pHost->AddExp(pCreateItemParam.m_nCount, res::eExpOpEnum::E_UseItem);
                    }
                }

            }
            return true;
        }
    }

#endif // !__TEST__
    return false;
}

    HumanBag::ItemFactory::item_shared_ptr ItemFactory::CreateItem(CreateItemParam & pCreateItemParam)
    {
        ItemFactory::item_shared_ptr newItem(new NullItem());
        if (NULL_ITEM_CONFIG == pCreateItemParam.m_nConfigId)
        {
            
            return newItem;
        }
        const ItemProtoElement * pItemProtoElement = ItemProtoTable::Instance().GetElement(pCreateItemParam.m_nConfigId);

        //
        if (pItemProtoElement == NULL)
        {
            ELOG("Create Item error item_id: %d", pCreateItemParam.m_nConfigId);
            return newItem;
        }

        //transfer
        int32_t real_config_id = pCreateItemParam.m_nConfigId;
        if (pCreateItemParam.m_bIsTransfer && !pItemProtoElement->transfer_id.empty())
        {
            if (pCreateItemParam.m_nOccupation < 0 || pCreateItemParam.m_nOccupation >= (int32_t)pItemProtoElement->transfer_id.size())
            {
                ELOG("Create Item transfer_id error item_id: %d Occupation %d", pCreateItemParam.m_nConfigId, pCreateItemParam.m_nOccupation);
                return newItem;
            }
            real_config_id = pItemProtoElement->transfer_id[pCreateItemParam.m_nOccupation];
            pItemProtoElement = ItemProtoTable::Instance().GetElement(real_config_id);
            if (pItemProtoElement == NULL)
            {
                ELOG("Create Item error item_id: %d", real_config_id);
                return newItem;
            }
        }

        switch (pItemProtoElement->type)
        {
        case Item::E_CURRENCY:
        {
            newItem.reset(new NullItem());
            return newItem;
        }
        break;

        case Item::E_EQUIPMENT:
        {
            newItem.reset(new Equip(pItemProtoElement, pCreateItemParam.m_nCount, pCreateItemParam.m_loadfromdb));
        }
        break;

        case Item::E_MATERIAL:
        {
            newItem.reset(new Material(pItemProtoElement, pCreateItemParam.m_nCount));
        }
        break;

        case Item::E_GEM:
        {
            newItem.reset(new Gem(pItemProtoElement, pCreateItemParam.m_nCount));
        }
        break;

        case Item::E_CONSUMABLE:
        {
            newItem.reset(new ItemConsumer(pItemProtoElement, pCreateItemParam.m_nCount));
        }
        break;

        case Item::E_POTION:
        {
            newItem.reset(new Potion(pItemProtoElement, pCreateItemParam.m_nCount));
        }
        break;
        case Item::E_GIFT:
        {
            newItem.reset(new ItemGift(pItemProtoElement, pCreateItemParam.m_nCount));
        }
        break;
#ifndef __TEST__
        case Item::E_PET:
        {
            int32_t nHumanConfId = INVALID_ID;
            if (pCreateItemParam.pPlayerScript != nullptr)
            {
                nHumanConfId = pCreateItemParam.pPlayerScript->GetConfId();
            }
            newItem.reset(new ItemPet(pItemProtoElement, pCreateItemParam.m_nCount, nHumanConfId));
            newItem->SetWild(pCreateItemParam.m_bWild);
        }
        break;

#endif
        case  Item::E_OTHER_UESE_ITEM:
        case  Item::E_DRESSUP:
        {
            newItem.reset(new OtherCanUseItem(pItemProtoElement, pCreateItemParam.m_nCount));
        }
        break;
        case Item::E_PET_SKILL_BOOK:
        {
            newItem.reset(new ItemPetSkillBook(pItemProtoElement, pCreateItemParam.m_nCount));
        }
        break;
        case Item::E_SOUL_STONE:
        {
#ifndef __TEST__
            Soulstone* p = new Soulstone(pItemProtoElement, pCreateItemParam.m_nCount);
            if (p && p->Init(pCreateItemParam))
            {
                newItem.reset(p);
            }
            else
            {
            }
#else
            newItem.reset(new Gem(pItemProtoElement, pCreateItemParam.m_nCount));
#endif//__TEST__
        }
        break;
        case Item::E_ITEMOPTION:
        {
            newItem.reset(new ItemOptional(pItemProtoElement, pCreateItemParam.m_nCount));
        }
        break;
        case Item::E_MOUNT:
        {
            newItem.reset(new ItemMount(pItemProtoElement, pCreateItemParam.m_nCount));
        }
        break;

        case Item::E_EMBLEM:
        {
            newItem = std::make_shared<Emblem>(pItemProtoElement, pCreateItemParam.m_nCount, pCreateItemParam.m_loadfromdb);
        }
        break;
        case Item::E_MOUNT_EQUIP:
        {
            if (pCreateItemParam.m_nAttackType==2 && pCreateItemParam.pPlayerScript!= nullptr )
            {
#ifndef __TEST__
                pCreateItemParam.m_nAttackType = pCreateItemParam.pPlayerScript->GetAttackType();
#endif//__TEST__
                
            }
            newItem = std::make_shared <ItemMountEquip>(pItemProtoElement, pCreateItemParam.m_nCount, pCreateItemParam.m_nAttackType);
        }
        break;
        default:
        {
            newItem = std::make_shared<ItemNormal>(pItemProtoElement, pCreateItemParam.m_nCount);
        }
        break;
        }

        if (nullptr == newItem)
        {
            return newItem;
            ELOG("CreateItem %d", real_config_id);
        }

        if (newItem->IsEmptyItem())
        {
            return newItem;
        }

        if (nullptr == pCreateItemParam.m_pItemData)
        {
            //GUID_t nGuid = 0;
            //GetGuidGenertor().generate(nGuid);
            GUID_t nGuid = GenerateGuid();
            if (nGuid == INVALID_ID)
            {
                return nullptr;
            }
            newItem->SetGuid(nGuid);
        }
        else
        {
            newItem->SetGuid(pCreateItemParam.m_pItemData->guid());
        }

        //newItem->SetBind(pCreateItemParam.m_vBind);
        newItem->SetBindRule(pCreateItemParam.m_vBind);
        newItem->SetSaleCd(pCreateItemParam.m_nCD);
        if (pCreateItemParam.m_vBind == (int32_t)E_UNBIND || pCreateItemParam.m_vBind == (int32_t)E_EQUIPBIND)
        {//默认是绑定
            newItem->SetBind(false);
        }
        else
        {
            newItem->SetBind(true);
        }

        newItem->SetLock(pCreateItemParam.m_vLock);

        return newItem;
    }

    ItemFactory::item_shared_ptr ItemFactory::RandomItem()
    {
        CreateItemParam  pCreateItemParam;
        pCreateItemParam.m_nConfigId = RandomItemId();
        pCreateItemParam.m_nCount = 1;
        return CreateItem(pCreateItemParam);
    }

    void ItemFactory::CreateItemData(ItemData & pb, int32_t nConfigid)
    {
        GUID_t nGuid = GenerateGuid();
        if (nGuid == INVALID_ID)
        {
            return;
        }
        pb.set_guid(nGuid);
        pb.set_configid(nConfigid);
        pb.set_overlaycount(1);

        return;
    }

    int32_t ItemFactory::RandomItemId()
    {
        const ItemProtoTable::vec_type&  vItems = ItemProtoTable::Instance().GetAllID();
        int32_t nPos = DK_Random::Instance().rand<int32_t>(0, vItems.size() - 1);
        return vItems[nPos];
    }
} // namespace HumanBag

#ifdef __TEST__
int32_t Equip::GetQuality()const { return 0; }
bool Equip::AddExp(int32_t) { return true; }
#endif