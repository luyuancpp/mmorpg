#include "Quest/QuestList.h"

#include "GenCode/GameDefine_Result.h"
#include "GenCode/Config/ActivityCfg.h"
#include "GenCode/Config/QuestCfg.h"
#include "GenCode/Achivement/AchivementModule.h"
#include "GenCode/Quest/QuestModule.h"
#include "GenCode/Config/QuestGroupCfg.h"
#include "GenCode/Config/ClientQuestTargetTypeCfg.h"
#include "CommonLogic/EventStruct/CommonEventStruct.h"
#include "CommonLogic/EventStruct/QuestEventStruct.h"

#include "QuestNPCAcceptManager.h"


#include "ActivityQuestAcceptor.h"
#include "MessageUtils.h"
#include "DK_TimeManager.h"

#ifndef __TEST__
#include "Obj/Obj_Human/Obj_Human.h"
#include "Scene/Scene.h"
#include "Obj/Obj_Module/Team/ObjTeamModule.h"
#include "Obj/Obj_Human/Obj_HumanManager.h"
#include "ModuleQuest.pb.h"
#include "CommonLogic/Quest/QuestDefine.h"
#include "Obj/Obj_Human/Combat/Obj_HumanCombat.h"
#include "Obj/Obj_Module/Guild/ObjGuildModule.h"
#include "CommonLogic/FunctionSwitch/FunctionSwtich.h"
#include "Obj/Obj_Module/DirectPurchase/ObjEventPackModule.h"
#include "GameManager/ObjModuleManager.h"
#include "CommonLogic/GameEvent/GameEvent.h"
#endif//__TEST__

#include "QuestFactory.h"

#ifdef __TEST__
extern int32_t g_nLevel;

#endif // __TEST__
namespace AvatarQuest
{

AvatarQuest::QuestList::quest_factory_ptr_type QuestList::kDefaultFactoryPtr(new QuestFactory());

Obj_Human* QuestList::kTestEmptyHuman{nullptr};

QuestList::QuestList(Obj_Human* pHuman, quest_factory_ptr_type&ptr)
    : 
    m_vMaxComplteQuest(GetMaxGroupSize()),
    bchange_scene_accept_(false),
    quest_factory_ptr_(ptr),
    m_pHuman(pHuman),
    select_quest_(kEmptySelectQuest)
{

    if (nullptr != ptr)
    {
        quest_factory_ptr_ = ptr;
    }
    else
    {
        quest_factory_ptr_ = kDefaultFactoryPtr;
    }
#ifdef __TEST__
    //check level table is right
    InitQuestEventObserverList();
#endif // __TEST__

}


void QuestList::OnGmClear()
{

    QuestRpcRemoveQuestNotifyNotify pb;
    for (auto it : m_vQestsList)
    {
        pb.add_questconfigid(it.first);
    }
    for (auto it : m_vCompleteQuestList)
    {
        pb.add_questconfigid(it);
    }

    for (max_complete_id_list::iterator it = m_vMaxComplteQuest.begin(); it != m_vMaxComplteQuest.end(); ++it)
    {
        pb.add_questconfigid(*it);
        (*it) = 0;
    }
    Clear();

    InitQuestEventObserverList();

    emc::i().emit<MsgEventStruct>(emid(), ModuleQuest::RPC_CODE_QUEST_REMOVEQUESTNOTIFY_NOTIFY, &pb);
    QuestRpcGmClearQuestNotify n;
    emc::i().emit<MsgEventStruct>(emid(), ModuleQuest::RPC_CODE_QUEST_GMCLEARQUEST_NOTIFY, &n);

}


void QuestList::OnRegister()
{
}

void QuestList::OnLogin()
{
 

    QuestRpcSendQuestListNotify pb;
    for (quest_list_type::iterator it = m_vQestsList.begin(); it != m_vQestsList.end(); ++it)
    {
        ::QuestData* pQuestPb = pb.mutable_questlist()->add_questslist();
        it->second->ToClientPb(pQuestPb);
    }

    std::size_t nType = 0;
    for (max_complete_id_list::iterator it = m_vMaxComplteQuest.begin(); it != m_vMaxComplteQuest.end(); ++it)
    {
        ::MaxQuestCompleteId* p = pb.mutable_completquestids()->add_maxcompleteidlist();
        p->set_questtype(nType++);
        p->set_maxconfigid(*it);
    }

    for (auto it : m_vCompleteQuestList)
    {
        pb.mutable_completquestids()->add_idlist(it);
    }

    for (quest_id_list_type::iterator it = m_vCanAcceptNPCQuestList.begin(); it != m_vCanAcceptNPCQuestList.end(); ++it)
    {
        pb.add_canacceptquest(*it);
    }

    emc::i().emit<MsgEventStruct>(emid(), ModuleQuest::RPC_CODE_QUEST_SENDQUESTLIST_NOTIFY, &pb);
}

void QuestList::Clear()
{
    m_vTimerList.Clear();
    m_vQestsList.clear();
    m_vCompleteQuestList.clear();
    max_complete_id_list v(GetMaxGroupSize());
    m_vMaxComplteQuest.swap(v);
    m_vQestsList.clear();
    m_vCompleteQuestList.clear();
    m_vCanAcceptNPCQuestList.clear();
    m_vQuestEventObservers.clear();
    m_vAcceptQuestTypeSize.clear();
    m_vAcceptQuestSubTypeSize.clear();
    on_quest_progress_add_callbacks_.clear();
    select_quest_ = kEmptySelectQuest;
    trigger_callbackv_.clear();

}

void QuestList::OnLoad(const QuestListData& pb)
{
#ifndef __TEST__
    if (g_QuestNPCAcceptManager_ptr)
    {
        g_QuestNPCAcceptManager_ptr->GetCanAcceptQuest(m_pHuman, m_vCanAcceptNPCQuestList);
    }
#endif//__TEST__
    SetCheckUniqueQuest(true);
    m_vTimerList.Clear();
    InitQuestEventObserverList();
    for (int32_t i = 0; i < pb.questslist_size(); ++i)
    {
        CreateQuest(pb.questslist(i));
    }
}

void QuestList::OnSave(QuestListData& pb)
{
    pb.Clear();
    for (quest_list_type::iterator it = m_vQestsList.begin(); it != m_vQestsList.end(); ++it)
    {
        it->second->OnSave(*pb.add_questslist());
    }
}

void QuestList::OnSave(user& db, user& dbcache)
{
    {
        QuestListData ndata;
        this->OnSave(ndata);

        auto odata = *dbcache.mutable_quest_list();
        if (MessageUtils::Equals(ndata, odata))
        {
            db.clear_quest_list();
        }
        else
        {
            db.mutable_quest_list()->CopyFrom(ndata);
            dbcache.mutable_quest_list()->CopyFrom(ndata);
        }
    }

    {
        CompletedQuestList ndata;
        this->OnSave(ndata);

        auto odata = *dbcache.mutable_completedquests();
        if (MessageUtils::Equals(ndata, odata))
        {
            db.clear_completedquests();
        }
        else
        {
            db.mutable_completedquests()->CopyFrom(ndata);
            dbcache.mutable_completedquests()->CopyFrom(ndata);
        }
    }
}

void QuestList::OnLoad(const CompletedQuestList& pb)
{
    for (int32_t i = 0; i < pb.idlist_size(); ++i)
    {
        m_vCompleteQuestList.emplace(pb.idlist(i));
    }

    for (int32_t i = 0; i < pb.maxcompleteidlist_size(); ++i)
    {
        if (i >= (int32_t)m_vMaxComplteQuest.size())
        {
            continue;
        }
        m_vMaxComplteQuest[i] = pb.maxcompleteidlist(i).maxconfigid();
    }
}

void QuestList::OnSave(CompletedQuestList& pb)
{
    pb.Clear();

    for (auto it : m_vCompleteQuestList)
    {
        pb.add_idlist(it);
    }
    std::size_t nType = 0;
    for (max_complete_id_list::iterator it = m_vMaxComplteQuest.begin(); it != m_vMaxComplteQuest.end(); ++it)
    {
        ::MaxQuestCompleteId* p = pb.add_maxcompleteidlist();
        p->set_questtype(nType++);
        p->set_maxconfigid(*it);
    }
}

void QuestList::ToClientPb(QuestRpcSyncQuestReply& pb)
{
    pb.Clear();
    for (quest_list_type::iterator it = m_vQestsList.begin(); it != m_vQestsList.end(); ++it)
    {
        ::QuestData* pQuestPb = pb.mutable_questlist()->add_questslist();
        it->second->ToClientPb(pQuestPb);
    }

    std::size_t nType = 0;
    for (max_complete_id_list::iterator it = m_vMaxComplteQuest.begin(); it != m_vMaxComplteQuest.end(); ++it)
    {
        ::MaxQuestCompleteId* p = pb.mutable_completquestids()->add_maxcompleteidlist();
        p->set_questtype(nType++);
        p->set_maxconfigid(*it);
    }

    for (auto it : m_vCompleteQuestList)
    {
        pb.mutable_completquestids()->add_idlist(it);
    }

    for (quest_id_list_type::iterator it = m_vCanAcceptNPCQuestList.begin(); it != m_vCanAcceptNPCQuestList.end(); ++it)
    {
        pb.add_canacceptquest(*it);
    }
}

void QuestList::ToClientPb(CompletedQuestList&   pb)
{
    std::size_t nType = 0;
    for (max_complete_id_list::iterator it = m_vMaxComplteQuest.begin(); it != m_vMaxComplteQuest.end(); ++it)
    {
        ::MaxQuestCompleteId* p = pb.add_maxcompleteidlist();
        p->set_questtype(nType++);
        p->set_maxconfigid(*it);
    }

    for (quest_id_list_type::iterator it = m_vCompleteQuestList.begin(); it != m_vCompleteQuestList.end(); ++it)
    {
        pb.add_idlist(*it);
    }
}

void QuestList::ToClientPb(int32_t nQuestConfigId, QuestData& pb)
{
    quest_ptr pQuest = GetQuest(nQuestConfigId);

    if (nullptr != pQuest)
    {
        pQuest->ToClientPb(&pb);
    }
    quest_id_list_type::iterator cit = m_vCompleteQuestList.find(nQuestConfigId);
    if (cit != m_vCompleteQuestList.end())
    {
        if (nullptr != quest_factory_ptr_)
        {
            quest_factory_ptr_->ToCompletedClientPb(nQuestConfigId, pb);
        }
    }
}

void QuestList::ToClientPb(AchivementRpcSyncAchivementReply& pb)
{
    for (quest_list_type::iterator it = m_vQestsList.begin(); it != m_vQestsList.end(); ++it)
    {
        ::AchivementData* pQuestPb = pb.mutable_achivementlist()->add_achivementlist();
        it->second->ToClientPb(pQuestPb);
    }

    for (auto it : m_vCompleteQuestList)
    {
        pb.mutable_completachivementids()->add_idlist(it);
    }
}

int32_t QuestList::CompletetQuestFromQuestType(int32_t nQuestType)
{
    int32_t questId = GetQuestFromType(nQuestType);
    OR_CHECK_RESULT(DirectToCompleteQuest(questId));
    return OR_OK;
}

int32_t QuestList::AcceptQuest(int32_t nQuestConfigId)
{
    quest_ptr pQuest = GetQuest(nQuestConfigId);

#ifndef __TEST__
    if (nullptr != pQuest)
    {
        return OR_QUEST_ACCPECTED;
    }
    if (NULL == quest_factory_ptr_)
    {
        return OR_TABLE_INDEX;
    }
    if (!m_pHuman)
    {
        return OR_NULL_PTR;
    }
#endif // !__TEST__

  
    int32_t quest_type = quest_factory_ptr_->quest_type(nQuestConfigId);
#ifndef __TEST__
    if (quest_type != E_MAIN_LINE_TYPE)
    {
        OR_CHECK_RESULT(FunctionSwitchManager::Instance().CheckOpen(OR_FUNCTION_QUEST_CLOSE, nQuestConfigId));
    }
#endif//__TEST__
    int32_t quest_activity_id = quest_factory_ptr_->quest_activity_id(nQuestConfigId);
    int32_t quest_required_level = quest_factory_ptr_->quest_required_level(nQuestConfigId);
    if (quest_activity_id <= 0 && quest_required_level > 0)
    {
        if (GetLevel() < quest_required_level)
        {
#ifndef __TEST__
            ILOG("QuestManager::AcceptQuest %llu quest questconfigid%d (OR_QUEST_LEVEL_QUEST)", m_pHuman->GetGuid(), nQuestConfigId);
#endif//__TEST__
            return OR_QUEST_LEVEL_QUEST;
        }
    }

    if (CouldNotAcceptQuestType(nQuestConfigId))
    {
#ifndef __TEST__
        ILOG("QuestManager::AcceptQuest %llu quest questconfigid%d (OR_QUEST_TYPE_UNIQUE)", m_pHuman->GetGuid(), nQuestConfigId);
#endif//__TEST__
        return OR_QUEST_TYPE_UNIQUE;
    }

    for (auto& qit : check_accept_quest_callback_list_)
    {
        OR_CHECK_RESULT(qit(nQuestConfigId));
    }

    if (!quest_factory_ptr_->check_pre_quest(nQuestConfigId, this))
    {
#ifndef __TEST__
        ILOG("QuestManager::AcceptQuest %llu quest questconfigid%d (OR_QUEST_PREV_QUEST_UNCOMPLETE)", m_pHuman->GetGuid(), nQuestConfigId);
#endif//__TEST__
        return OR_QUEST_PREV_QUEST_UNCOMPLETE;
    }

    pQuest = GetQuest(nQuestConfigId);
    if (nullptr != pQuest)
    {
        return OR_QUEST_ACCPECTED;
    }

    if (FindCompletedQuest(nQuestConfigId))
    {

        return OR_QUEST_COMPLETED;
    }
    int32_t npc_accept = quest_factory_ptr_->npc_accept(quest_activity_id);
    // 从NPC接的任务
    if (0 < npc_accept)
    {
        if (!FindCanAcceptNPCQuest(nQuestConfigId))
        {
#ifndef __TEST__
            ILOG("QuestManager::AcceptQuest %llu quest questconfigid%d (OR_QUEST_CANACCEPTNPC)", m_pHuman->GetGuid(), nQuestConfigId);
#endif//__TEST__
            return OR_QUEST_CANACCEPTNPC;
        }
    }

    if (!CreateQuest(nQuestConfigId))
    {
#ifndef __TEST__
        ILOG("QuestManager::AcceptQuest %llu quest questconfigid%d (OR_TABLE_INDEX)", m_pHuman->GetGuid(), nQuestConfigId);
#endif//__TEST__
        return OR_TABLE_INDEX;
    }


    // 从NPC接的任务(成功接到任务 从可接列表删掉)
    if (0 < npc_accept)
    {
        m_vCanAcceptNPCQuestList.erase(nQuestConfigId);
    }

    pQuest = GetQuest(nQuestConfigId);

    if (nullptr != pQuest)
    {
        pQuest->OnAcceptQuest();

		if (m_oQuestAcceptCalculator)
		{
			m_oQuestAcceptCalculator(nQuestConfigId);
		}
    }

    return OR_OK;
}

int32_t QuestList::TryAchieveNextQuest(int32_t nQuestConfigId)
{
    quest_ptr pQuest = GetQuest(nQuestConfigId);
    if (nullptr == pQuest)
    {
        return OR_QUEST_HASNT_QUEST;
    }
    pQuest->Achieve();
    return OR_OK;
}

int32_t QuestList::CompleteQuest(int32_t nQuestConfigId)
{
    if (FindCompletedQuest(nQuestConfigId))
    {
        return OR_QUEST_COMPLETED;
    }

    quest_ptr pQuest = GetQuest(nQuestConfigId);

    if (nullptr == pQuest)
    {
        return OR_QUEST_HASNT_QUEST;
    }

    OR_CHECK_RESULT_RETURN_RET(pQuest->CompleteQuest());
	pQuest->Award();
    RecordCompleteQuest(nQuestConfigId);
    OnErase(nQuestConfigId);
    
    OnCompleteQuest(nQuestConfigId);

#ifndef __TEST__
    if (m_pHuman)
    {
        m_pHuman->GetObjEventPackModule()->OnTrigger(EVENT_ON_QUEST_COMPLETE, nQuestConfigId);
    }
#endif // !__TEST__

    return OR_OK;
}


int32_t QuestList::DirectToCompleteQuest(int32_t nQuestConfigId)
{
    quest_ptr pQuest = GetQuest(nQuestConfigId);
    if (nullptr == pQuest)
    {
        return OR_QUEST_HASNT_QUEST;
    }
    if (m_vCompleteQuestList.find(nQuestConfigId) != m_vCompleteQuestList.end())
    {
        return OR_QUEST_COMPLETED;
    }
    pQuest->TriggerCompleteEvent();
    CompleteQuest(nQuestConfigId);
    return OR_OK;
}

int32_t QuestList::DirectToAchiveQuest(int32_t nQuestConfigId)
{
    quest_ptr pQuest = GetQuest(nQuestConfigId);
    if (nullptr == pQuest)
    {
        return OR_QUEST_HASNT_QUEST;
    }
    if (m_vCompleteQuestList.find(nQuestConfigId) != m_vCompleteQuestList.end())
    {
        return OR_QUEST_COMPLETED;
    }

    pQuest->TriggerCompleteEvent();
    return OR_OK;
}

int32_t QuestList::TriggerAchieveCompleteQuest(int32_t nQuestConfigId)
{
    quest_ptr pQuest = GetQuest(nQuestConfigId);

    if (nullptr == pQuest)
    {
        return OR_QUEST_HASNT_QUEST;
    }

    if (m_vCompleteQuestList.find(nQuestConfigId) != m_vCompleteQuestList.end())
    {
        return OR_QUEST_COMPLETED;
    }

    pQuest->TriggerCompleteEvent();
    return OR_OK;
}

int32_t QuestList::GiveUpQuest(int32_t nQuestConfigId)
{
    if (FindCompletedQuest(nQuestConfigId))
    {
        return OR_QUEST_COMPLETED;
    }

    quest_ptr pQuest = GetQuest(nQuestConfigId);

    if (nullptr == pQuest)
    {
        return OR_QUEST_HASNT_QUEST;
    }


    OR_CHECK_RESULT_RETURN_RET(pQuest->GiveUpQuest());

    OnErase(nQuestConfigId);

    // 从NPC处接的任务 把任务放回canaccept列表
  
    if (nullptr != quest_factory_ptr_)
    {
        if (0 < quest_factory_ptr_->npc_accept(nQuestConfigId))
        {
            m_vCanAcceptNPCQuestList.emplace(nQuestConfigId);

            QuestRpcSyncCanAcceptQuestNotify msg;
            msg.add_canacceptquest(nQuestConfigId);

            if (0 < msg.canacceptquest_size())
            {
                emc::i().emit<MsgEventStruct>(emid(), ModuleQuest::RPC_CODE_QUEST_SYNCCANACCEPTQUEST_NOTIFY, &msg);             
            }
        }
    }

    return OR_OK;
}

int32_t QuestList::GetQuestStatus(int32_t nQuestConfigId)
{
    quest_ptr pQuest = GetQuest(nQuestConfigId);

    if (nullptr != pQuest)
    {
        return pQuest->GetQuestStatus();
    }

    if (FindCompletedQuest(nQuestConfigId))
    {
        return E_QUEST_STATUS_COMPLETED;
    }

    return E_QUEST_STATUS_UN_ACCEPT;
}

int32_t QuestList::GetReward(int32_t nQuestConfigId)
{
    OR_CHECK_RESULT_RETURN_RET(CompleteQuest(nQuestConfigId));
    return OR_OK;
}

void QuestList::AcceptAllQuest()
{
    if (nullptr == quest_factory_ptr_)
    {
        return;
    }
    auto& oQuestList = quest_factory_ptr_->all_id();
    for (auto it = oQuestList.begin(); it != oQuestList.end(); ++it)
    {
        if (!CheckRecordToCompleteQuest(*it))
        {
            continue;
        }
        AcceptQuest(*it);
    }
}

void QuestList::CompleteAllQuest()
{
    SetCheckUniqueQuest(false);
    AcceptAllQuest();
    if (nullptr == quest_factory_ptr_)
    {
        return;
    }
    auto& oQuestList = quest_factory_ptr_->all_id();
    for (auto it = oQuestList.begin(); it != oQuestList.end(); ++it)
    {
        DirectToCompleteQuest(*it);
    }
    SetCheckUniqueQuest(true);
}

bool QuestList::FindAcceptedQuest(int32_t nQuestConfigId)
{
    quest_ptr pQuest = GetQuest(nQuestConfigId);

    if (nullptr == pQuest)
    {
        return false;
    }
    return pQuest->IsAcceptedStatus();
}

bool QuestList::FindCompletedQuest(int32_t quest_config_id)
{
    if (nullptr == quest_factory_ptr_)
    {
        return false;
    }
    int32_t group_quest = quest_factory_ptr_->group_quest(quest_config_id);
    if (IsAutoIncrement(quest_config_id))
    {
        if (group_quest < 0 || (std::size_t)group_quest >= GetMaxGroupSize())
        {
            return false;
        }
        return quest_config_id <= m_vMaxComplteQuest[group_quest];
    }
    return m_vCompleteQuestList.find(quest_config_id) != m_vCompleteQuestList.end();
}

bool QuestList::FindAchievedQuest(int32_t nQuestConfigId)
{
    quest_ptr pQuest = GetQuest(nQuestConfigId);
    if (nullptr == pQuest)
    {
        return false;
    }
    return pQuest->IsAchieved();
}

bool QuestList::FindCanAcceptNPCQuest(int32_t nQuestConfigId)
{
    auto iter = m_vCanAcceptNPCQuestList.find(nQuestConfigId);
    if (iter == m_vCanAcceptNPCQuestList.end())
    {
        return false;
    }

    return true;
}

bool QuestList::CheckTrigger(int32_t nConfId)
{
    return true;
}

std::size_t QuestList::GetAcceptQuestSize()const
{
    std::size_t nSize = 0;
    for (quest_list_type::const_iterator it = m_vQestsList.begin(); it != m_vQestsList.end(); ++it)
    {
        if (it->second->IsAcceptedStatus())
        {
            ++nSize;
        }
    }
    return nSize;
}

std::size_t QuestList::GetCompleteQuestSize()
{
    if (nullptr == quest_factory_ptr_)
    {
        return 0;
    }

    std::size_t index = GetMinGroupIndex();
    std::size_t nSize = 0;
    for (max_complete_id_list::iterator ji = ++m_vMaxComplteQuest.begin(); ji != m_vMaxComplteQuest.end(); ++ji)
    {
        int32_t group_index = quest_factory_ptr_->group_quest(index);
        nSize = nSize + (quest_factory_ptr_->groupbegin_quest(group_index) - quest_factory_ptr_->groupend_quest(group_index));
        ++index;
    }

    return m_vCompleteQuestList.size() + nSize;
}

std::size_t QuestList::GetAcceptQuestSizeFromQuestType(int32_t quest_type)const
{
    auto it = m_vAcceptQuestTypeSize.find(quest_type);
    if (it == m_vAcceptQuestTypeSize.end())
    {
        return 0;
    }
    return it->second.size();
}

void QuestList::TriggerEvent(const AvatarQuest::QuestEvent& oEvent)
{
    if (oEvent.QuestConfigId() > 0)
    {
        TriggerEvent(oEvent.QuestConfigId(), oEvent);
    }
    else
    {
        i32_set_type   quest_lists = GetQuestListIdFromEventType(oEvent.GetEventType());
        for (auto& it : quest_lists)
        {
            TriggerEvent(it, oEvent);
        }
    }
    
    for (auto& it : trigger_callbackv_)
    {
        it(oEvent);
    }
}

void QuestList::TriggerEvent(int32_t nQuestConfigId, const AvatarQuest::QuestEvent& oEvent, bool bShared)
{
#ifndef __TEST__

    if (!m_pHuman)
    {
        return;
    }
    if (!quest_factory_ptr_)
    {
        return;
    }
    if (m_pHuman->IsRobot())
    {
        return;
    }
    if (quest_factory_ptr_->quest_activity_id(nQuestConfigId) > 0)
    {
        OR_CHECK_RESULT_RETURN_VOID(m_pHuman->CanAwardSceneHuman(m_pHuman->GetGuid()));
    }

    if (!quest_factory_ptr_->share_target_amount(nQuestConfigId).empty() && bShared)
    {
        role_id_list_type share_role_list;
        for (auto&& sit : quest_factory_ptr_->share_target_amount(nQuestConfigId))
        {
            if (sit == E_QUEST_SHARED_RADIUS && m_pHuman->m_pObjHumanCombat)
            {
                m_pHuman->m_pObjHumanCombat->GetBeKillOnlyRadius(share_role_list, quest_factory_ptr_->share_target_parameter(nQuestConfigId));
            }
        }
        if (nullptr != m_pHuman)
        {
            for (auto&& sit : share_role_list)
            {
                if (sit == m_pHuman->GetGuid())
                {
                    continue;
                }

                Obj_Human* pHuman = m_pHuman->GetSceneHumanFromGuid(sit);
                if (nullptr == pHuman)
                {
                    continue;
                }
                pHuman->m_pQuestList->TriggerEvent(nQuestConfigId, oEvent, false);
            }
        }


    }
#endif//__TEST__

    quest_ptr pQuest = GetQuest(nQuestConfigId);


    if (nullptr == pQuest)
    {

        return;
    }

    Quest::quests_id_type quest_lists;

    if (nullptr == pQuest)
    {
        return;
    }

    if (pQuest->TriggerEvent(oEvent)
        || pQuest->IsAchieved())
    {
        quest_lists.push_back(nQuestConfigId);
        m_bQuestStatusChange = true;
    }
    if (oEvent.GetTargetId() > 0 && TriggerAnyType(oEvent.GetEventType(), nQuestConfigId))
    {
        AvatarQuest::QuestEvent  oEmptyIdEvent = oEvent;
        oEmptyIdEvent.SetTargetId(0);
        if (pQuest->TriggerEvent(oEmptyIdEvent))
        {
            quest_lists.push_back(nQuestConfigId);
        }
    }


    if (quest_lists.empty())
    {
        return;
    }
    TryAutoCompleteQuest(quest_lists);
}


bool QuestList::CouldNotAcceptQuestType(int32_t quest_config_id)
{

    if (!m_bCheckUniqueQuest)
    {
        return false;
    }


    if (NULL == quest_factory_ptr_)
    {
        return false;
    }

    if (!quest_factory_ptr_->CheckSubType())
    {
        return false;
    }

    int32_t sub_quest_type = quest_factory_ptr_->sub_quest_type(quest_config_id);
    int32_t quest_type = quest_factory_ptr_->quest_type(quest_config_id);
    i32_i32_pair_type p = GetQuestTypePair(quest_config_id);
    if (sub_quest_type <= 0)
    {
        if (quest_factory_ptr_->RepeatedAccept(quest_type))
        {
            return false;
        }
        return m_vAcceptQuestSubTypeSize.find(p) != m_vAcceptQuestSubTypeSize.end();
    }
    else if (sub_quest_type > 0 )
    {
        return m_vAcceptQuestSubTypeSize.find(p) != m_vAcceptQuestSubTypeSize.end();
    }

    return false;
}

void QuestList::TriggerEvent(int32_t nQuestConfigId, const AvatarQuest::QuestEvent& oEvent)
{
    TriggerEvent(nQuestConfigId, oEvent, true);
}

void QuestList::TriggerNPCAcceptQuest()
{
#ifndef __TEST__
    if (NULL == g_QuestNPCAcceptManager_ptr)
    {
        return;
    }
    quest_id_list_type tempCanAccept;

    g_QuestNPCAcceptManager_ptr->GetCanAcceptQuest(m_pHuman, tempCanAccept);

    QuestRpcSyncCanAcceptQuestNotify msg;
    msg.Clear();
    for (auto iter : tempCanAccept)
    {
        int32_t nQuestConfigId = iter;
        m_vCanAcceptNPCQuestList.emplace(nQuestConfigId);
        msg.add_canacceptquest(nQuestConfigId);
    }

    if (0 < msg.canacceptquest_size())
    {
        emc::i().emit<MsgEventStruct>(emid(), ModuleQuest::RPC_CODE_QUEST_SYNCCANACCEPTQUEST_NOTIFY, &msg);
    }

#endif // !__TEST__
    return;
}

void QuestList::LevelUp(int32_t nLevel)
{
#ifdef __TEST__
    g_nLevel = nLevel;
#endif // __TEST__
    TriggerEvent(AvatarQuest::E_QUEST_LEVELUP, nLevel, nLevel);
}

int32_t QuestList::RpcCompleteQuestStep(QuestRpcCompleteQuestStepAsk& ask, QuestRpcCompleteQuestStepReply& reply)
{
    m_bQuestStatusChange = false;
    //交付任务
    if (FindAchievedQuest(ask.questconfigid()))
    {
        quest_ptr pQuest = GetQuest(ask.questconfigid());
        if (nullptr == pQuest)
        {
            reply.set_result(OR_QUEST_COMPLETE_QUEST_ERROR_ERROR);
            return OR_QUEST_COMPLETE_QUEST_ERROR_ERROR;
        }
        if (nullptr == quest_factory_ptr_)
        {
            return OR_QUEST_COMPLETE_QUEST_ERROR_ERROR;
        }
        if (ask.targetid() != quest_factory_ptr_->npc_accept(ask.questconfigid()))
        {
            reply.set_result(OR_QUEST_COMPLETE_STEP_COMMIT_NPC_ID_ERROR);
            return OR_QUEST_COMPLETE_STEP_COMMIT_NPC_ID_ERROR;

        }
        if (!CheckNpcDistance(ask.targetid()))
        {
            reply.set_result(OR_QUEST_NPC_DISTANCE_ERROR);
            return OR_QUEST_NPC_DISTANCE_ERROR;
        }
        CompleteQuest(ask.questconfigid());
    }
    else if (IsDirectToCompleteStepQuestType(ask.queststepeventtype()))
    {
        if (ask.questconfigid() > 0)
        {
            //对应某个npc谈话
            //判断距离
#ifndef __TEST__
            //改成对话的时候发消息到服务器
            if (ask.queststepeventtype() == AvatarQuest::E_QUEST_TALKING_WITH_NPC)
            {
                if (!CheckNpcDistance(ask.targetid()))
                {
                    reply.set_result(OR_QUEST_NPC_DISTANCE_ERROR);
                    return OR_QUEST_NPC_DISTANCE_ERROR;
                }
            }
#endif
            TriggerEvent(ask.questconfigid(), ask.queststepeventtype(), ask.targetid(), 1);
        }
    }

    if (m_bQuestStatusChange
        || !CheckRecordToCompleteQuest(ask.questconfigid()))
    {
        reply.set_result(OR_OK);
    }
    else
    {
        reply.set_result(OR_QUEST_COMPLETED_CONDITION);
    }

    m_bQuestStatusChange = false;
    return reply.result();
}

int32_t QuestList::RpcCompleteQuest(QuestRpcCompleteQuestAsk& ask, QuestRpcCompleteQuestReply& reply)
{
    m_bQuestStatusChange = false;
    if (FindCompletedQuest(ask.questconfigid()))
    {
        reply.set_result(OR_QUEST_COMPLETED);

        return reply.result();
    }

    if (IsDirectToCompleteStepQuestType(ask.questeventtype()))
    {
        TriggerEvent(ask.questeventtype(), ask.targetid(), 1);
    }

    if (FindCompletedQuest(ask.questconfigid()))
    {
        reply.set_result(OR_OK);
        return reply.result();
    }
    reply.set_result(CompleteQuest(ask.questconfigid()));
    return reply.result();
}

int32_t QuestList::RpcGiveUpQuest(QuestRpcGiveUpQuestAsk& ask, QuestRpcGiveUpQuestReply& reply)
{
    quest_ptr pQuest = GetQuest(ask.questconfigid());
    if (nullptr == pQuest)
    {
        reply.set_result(OR_QUEST_HASNT_QUEST);
        return OR_QUEST_HASNT_QUEST;
    }
    reply.set_result(GiveUpQuest(ask.questconfigid()));
    OR_CHECK_RESULT_RETURN_RET(reply.result());
    return OR_OK;
}

int32_t QuestList::RpcGetQuestData(QuestRpcGetQuestDataAsk& ask, QuestRpcGetQuestDataReply& reply)
{
    if (!quest_factory_ptr_)
    {
        return OR_NULL_PTR;
    }
    for (int32_t i = 0; i < ask.questconfigid_size(); ++i)
    {
        quest_list_type::iterator it = m_vQestsList.find(ask.questconfigid(i));
        if (it != m_vQestsList.end())
        {
            if (it->second)
            {
                it->second->ToClientPb(reply.add_questlist());
            }
            continue;
        }

        if (FindCompletedQuest(ask.questconfigid(i)))
        {
            quest_factory_ptr_->ToCompletedClientPb(ask.questconfigid(i), *reply.add_questlist());
            continue;
        }

        quest_id_list_type::iterator it0 = m_vCanAcceptNPCQuestList.find(ask.questconfigid(i));
        if (it0 != m_vCanAcceptNPCQuestList.end())
        {
            quest_factory_ptr_->ToCanAcceptClientPb(ask.questconfigid(i), *reply.add_questlist());
            continue;
        }

        quest_factory_ptr_->ToUnAcceptClientPb(ask.questconfigid(i), *reply.add_questlist());
    }

    reply.set_result(OR_OK);
    return OR_OK;
}

#ifdef __TEST__
void QuestList::OnKillMonster(int32_t nTargetId, int32_t nCount)
{
    TriggerEvent(AvatarQuest::E_QUEST_KILL_MONSTER, nTargetId, nCount);
}
#endif//__TEST__

void QuestList::ComsueItemToCompleteQuest(i32_map_type& vConfigList)
{
    for (i32_map_type::iterator it = vConfigList.begin(); it != vConfigList.end(); ++it)
    {
        TriggerEvent(AvatarQuest::E_QUEST_COMMIT, it->first, it->second);
    }
}

void QuestList::TriggerEvent(int32_t eventType, int32_t nTargetId, int32_t count)
{
    AvatarQuest::QuestEvent oQuestEvent(eventType, nTargetId, count);
    TriggerEvent(oQuestEvent);
}

void QuestList::TriggerEvent(int32_t nQuestConfigId, int32_t eventType, int32_t nTargetId, int32_t count)
{
    AvatarQuest::QuestEvent oQuestEvent(eventType, nTargetId, count);
    TriggerEvent(nQuestConfigId, oQuestEvent);
}

void QuestList::ProtectNpc(int32_t nTargetId, int32_t nCount)
{
    TriggerEvent(AvatarQuest::E_QUEST_PROTECT, nTargetId, nCount);
}

bool QuestList::TriggerAnyType(int32_t nId, int32_t nQuestConfigId)
{
    if (nId == E_PVP_TYPE_COMPLETE)
    {
        return false;
    }

    if (NULL == quest_factory_ptr_)
    {
        return false;
    }

    for (auto&& it : quest_factory_ptr_->target_1_id(nQuestConfigId))
    {
        if (it == 0)
        {
            return true;
        }
    }

    for (auto&& it : quest_factory_ptr_->target_2_id(nQuestConfigId))
    {
        if (it == 0)
        {
            return true;
        }
    }

    for (auto&& it : quest_factory_ptr_->target_3_id(nQuestConfigId))
    {
        if (it == 0)
        {
            return true;
        }
    }

    for (auto&& it : quest_factory_ptr_->target_4_id(nQuestConfigId))
    {
        if (it == 0)
        {
            return true;
        }
    }


    for (auto&& it : quest_factory_ptr_->target_5_id(nQuestConfigId))
    {
        if (it == 0)
        {
            return true;
        }
    }

    return false;
}

void QuestList::AddOnAhieveQuestCallback(const quest_id_callback_type& cb)
{
	achieve_callback_list_.emplace_back(cb);
}

void QuestList::AddCheckAcceptQuestCallback(const int32_t_quest_id_callback_type& cb)
{
    check_accept_quest_callback_list_.emplace_back(cb);
}

void QuestList::OneStep(int32_t quest_config_id)
{
    quest_ptr pQuest = GetQuest(quest_config_id);
    if (nullptr == pQuest)
    {
        return;
    }
    pQuest->OneStep();
    CompleteQuest(quest_config_id);
}

QuestData QuestList::GetQuestPb(int32_t quest_config_id)
{
    quest_ptr pQuest = GetQuest(quest_config_id);
    if (nullptr == pQuest)
    {
        return QuestData();
    }
    return pQuest->GetQuestDataPB();
}

void QuestList::ResetQuest(int32_t quest_config_id)
{
    if (quest_config_id <= 0)
    {
        return;
    }
    auto quest_ptr = GetQuest(quest_config_id);
    if (nullptr == quest_ptr)
    {
        return;
    }
    quest_ptr->Reset();
}

void QuestList::ResetQuest(const QuestData& pb)
{
    int32_t quest_config_id = pb.configid();
    if (quest_config_id <= 0)
    {
        return;
    }
	if (nullptr == quest_factory_ptr_)
	{
		return;
	}

    RemoveQuestFromType(quest_factory_ptr_->quest_type(quest_config_id));
    RemoveQuest(quest_config_id);
    CreateQuest(pb);
  
    quest_ptr pQuest = GetQuest(quest_config_id);
    if (nullptr == pQuest)
    {
        return;
    }
    if (nullptr != pQuest)
    {
        pQuest->OnAcceptQuest();
    }
    pQuest->NotifyQuestUpdate();
}

QuestList::quest_ptr QuestList::GetQuest(int32_t nQuestConfigId)
{
    quest_ptr pQuest;
    quest_list_type::iterator it = m_vQestsList.find(nQuestConfigId);
    if (m_vQestsList.end() == it)
    {
        return pQuest;
    }
    pQuest = it->second;
    return pQuest;
}

bool QuestList::CreateQuest(int32_t quest_config_id)
{
    QuestData oData;
    oData.set_configid(quest_config_id);
	bool ret = CreateQuest(oData);
	
	return ret;
}

bool QuestList::CreateQuest(const QuestData& oData)
{
    if (nullptr == quest_factory_ptr_)
    {
        return false;
    }
    CreateQuestParam p;
    p.p_human_ = m_pHuman;
    p.quest_factory_ptr_ = quest_factory_ptr_;
	p.get_progress_ = std::bind(&QuestList::GetQuestStepProgress, this, std::placeholders::_1, std::placeholders::_2);
    p.check_trigger_callback_ = std::bind(&QuestList::CheckTrigger, this, std::placeholders::_1);
    quest_ptr ptr = quest_factory_ptr_->CreateQuest(oData, p);
    if (nullptr == ptr)
    {
        return false;
    }
    OnAddQuest(oData.configid(), ptr);
    InitTimeoutQuest(oData.configid());
    return true;
}

void QuestList::InitTimeoutQuest(int32_t nQuestConfigId)
{
    quest_ptr pQuest = GetQuest(nQuestConfigId);
    if (nullptr == pQuest)
    {
        return;
    }

    if (NULL == quest_factory_ptr_)
    {
        return ;
    }
    int32_t quest_limit_time = quest_factory_ptr_->quest_limit_time(nQuestConfigId);
    if (quest_factory_ptr_->quest_limit_time(nQuestConfigId) <= 0)
    {
        return;
    }

    time_t now = g_pTimeManager->CurrentTimeS();
    time_t endTime = now + quest_limit_time;
    if (pQuest->GetBeginTime() > 0)
    {
        endTime = pQuest->GetBeginTime() + quest_limit_time;
    }

    if (endTime < now)
    {
        OnTimeOut(nQuestConfigId);
        return;
    }
    BaseModule::GameTimer* p_timeout_timer_id = m_vTimerList.runAt(muduo::Timestamp::fromUnixTime(endTime), std::bind(&QuestList::OnTimeOut, this, nQuestConfigId));
    pQuest->SetTimeoutTimerId(p_timeout_timer_id);
}

void QuestList::OnTimeOut(int32_t nQuestConfigId)
{
    quest_ptr pQuest = GetQuest(nQuestConfigId);
    if (nullptr == pQuest)
    {
        return;
    }
    if (FindCompletedQuest(nQuestConfigId))
    {
        return;
    }
    pQuest->OnTimeOut();
    CompleteQuest(nQuestConfigId);
}

void QuestList::RecordCompleteQuest(int32_t nQuestConfigId)
{
    if (!CheckRecordToCompleteQuest(nQuestConfigId))
    {
        return;
    }
    if (NULL == quest_factory_ptr_)
    {
        return ;
    }

    int32_t group_quest = quest_factory_ptr_->group_quest(nQuestConfigId);

    if (IsAutoIncrement(nQuestConfigId))
    {
        if (group_quest < (int32_t)GetMinGroupIndex() || group_quest >= (int32_t)GetMaxGroupSize())
        {
            return;
        }
        m_vMaxComplteQuest[group_quest] = std::max(m_vMaxComplteQuest[group_quest], nQuestConfigId);

    }
    else
    {
        m_vCompleteQuestList.emplace(nQuestConfigId);
    }

}

int32_t QuestList::GetLevel()
{
#ifndef __TEST__
    if (!m_pHuman)
    {
        return 0;
    }
    return m_pHuman->GetLevel();
#else
    return g_nLevel;
#endif
}

int32_t QuestList::OnCompleteQuest(int32_t nQuestConfigId)
{
   
    if (NULL == quest_factory_ptr_)
    {
        return OR_TABLE_INDEX;
    }

    emc::i().emit<CompleteQuestES>(emid(), nQuestConfigId);

    quest_factory_ptr_->OnComplete(nQuestConfigId, this);
   
    AcceptNextQuestList(nQuestConfigId);
	
    TriggerNPCAcceptQuest();

    //CheckUIGategoryChanged();

#ifndef __TEST__
    if (nullptr != m_pHuman)
    {
        //m_pHuman->GetGuildModule()->OnActiveGuildRed(nQuestConfigId);
        ObjModuleManager::Instance().OnQuestComplete(m_pHuman, nQuestConfigId);
    }
  
#endif//

    return OR_OK;
}

void QuestList::TryAutoCompleteQuest(i32_v_type& quest_lists)
{
    for (auto nQuestConfigId : quest_lists)
    {
        quest_ptr pQuest = GetQuest(nQuestConfigId);
        if (nullptr == pQuest)
        {
            continue;;
        }

        //如果有交接任务的npc，该任务则是achieve转态，需要客户端去交付
        if (pQuest->HasCompleteNpc())
        {
            continue;
        }

        //需要客户端去领奖才能完成任务
        if (!pQuest->IsAutoReward())
        {
            continue;
        }
        //没有奖励，或者有奖励自动领奖，或者没有交接的npc
        CompleteQuest(nQuestConfigId);


    }
}


void QuestList::InitQuestEventObserverList()
{
    if (nullptr == quest_factory_ptr_)
    {
        return;
    }
    m_vQuestEventObservers.clear();
    i32_set_type emptyList;
    int32_t min_event_id = quest_factory_ptr_->GetEventTypeMin();
    int32_t max_event_id = quest_factory_ptr_->GetEventTypeMax();
    for (int32_t i = min_event_id; i < max_event_id; i++)
    {
        m_vQuestEventObservers.emplace(i, emptyList);
    }

}

void QuestList::OnAddQuest(int32_t quest_config_id, quest_ptr& pQuest)
{
    if (nullptr == quest_factory_ptr_||!pQuest)
    {
        return;
    }
    pQuest->SetQuestFactory(quest_factory_ptr_);
    m_vQestsList.emplace(quest_config_id, pQuest);

    i32_v_type v = pQuest->GetQuestEventList();
    for (auto&& tit : v)
    {
        quest_event_observers_type::iterator eit = m_vQuestEventObservers.find(tit);
        if (eit == m_vQuestEventObservers.end())
        {
            i32_set_type emptyList;
            m_vQuestEventObservers.emplace(tit, emptyList);
            eit = m_vQuestEventObservers.find(tit);

        }
        if (eit == m_vQuestEventObservers.end())
        {
            ELOG("Add quest event observer error%d %d", tit, quest_config_id);
            continue;
        }
        eit->second.emplace(quest_config_id);
    }
    int32_t quest_type = quest_factory_ptr_->quest_type(quest_config_id);
    auto tit = m_vAcceptQuestTypeSize.find(quest_type);
    if (tit == m_vAcceptQuestTypeSize.end())
    {
        i32_set_type v;
        v.emplace(quest_config_id);
        m_vAcceptQuestTypeSize.emplace(quest_type, v);
    }
    else
    {
        tit->second.emplace(quest_config_id);;
    }

    i32_i32_pair_type p = GetQuestTypePair(quest_config_id);
#ifdef __TEST__
    if (pQuest->GetQuestSubType() > 0 && m_bCheckUniqueQuest)
    {
        auto stit = m_vAcceptQuestSubTypeSize.find(p);
        assert(stit == m_vAcceptQuestSubTypeSize.end());
    }
#endif // __TEST__

    m_vAcceptQuestSubTypeSize.emplace(p);

	pQuest->SetUpdateDataCallBack(std::bind(&QuestList::GetAddQuestUpdateObj, this, std::placeholders::_1, std::placeholders::_2));
	pQuest->SetAchieveCallback(std::bind(&QuestList::OnQuestAchieve, this, std::placeholders::_1));
    pQuest->SetQuestProgressAddCallbacks(on_quest_progress_add_callbacks_);
	
    for (auto& it : on_accept_quest_callback_list_)
    {
        it(quest_config_id);
    }
}

bool QuestList::OnErase(int32_t quest_config_id)
{
    quest_ptr pQuest = GetQuest(quest_config_id);

    if (nullptr == pQuest)
    {
        return false;
    }

    if (nullptr == quest_factory_ptr_)
    {
        return false;
    }

    i32_v_type v = pQuest->GetQuestEventList();
    for (auto&& vi : v)
    {
        quest_event_observers_type::iterator eit = m_vQuestEventObservers.find(vi);
        if (eit == m_vQuestEventObservers.end())
        {
            continue;
        }
        eit->second.erase(quest_config_id);
    }

    

    int32_t quest_type = quest_factory_ptr_->quest_type(quest_config_id);

    m_vTimerList.cancel(pQuest->GetTimeoutTimerId());

    auto&& tit = m_vAcceptQuestTypeSize.find(quest_type);
    if (tit != m_vAcceptQuestTypeSize.end())
    {
        tit->second.erase(quest_config_id);
        if (tit->second.empty())
        {
            m_vAcceptQuestTypeSize.erase(tit);
        }
    }
    i32_i32_pair_type p = GetQuestTypePair(quest_config_id);
#ifdef __TEST__
    if (pQuest->GetQuestSubType() > 0 && m_bCheckUniqueQuest)
    {
        auto stit = m_vAcceptQuestSubTypeSize.find(p);
        assert(stit != m_vAcceptQuestSubTypeSize.end());
    }
#endif // __TEST__
    m_vAcceptQuestSubTypeSize.erase(p);

    for (auto& it : on_remove_callback_list_)
    {
        it(quest_config_id);
    }
    return m_vQestsList.erase(quest_config_id);
}

i32_set_type  QuestList::GetQuestListIdFromEventType(int32_t nQuestType)
{
    quest_event_observers_type::iterator eit = m_vQuestEventObservers.find(nQuestType);
    if (eit == m_vQuestEventObservers.end())
    {
        static i32_set_type v;
        return v;
    }
    return eit->second;
}


//退出公会时放弃公会任务和公会活动主任务
void QuestList::OnNotInGuild()
{
    const ActivityElement* pActivityElement = ActivityTable::Instance().GetElement(E_ACTIVITY_GUILDQUEST);
    if (nullptr == pActivityElement)
    {
        return;
    }

    RemoveQuestFromType(E_GUILD);
    //  RemoveQuest(pActivityElement->quest_id);

    return;
}

bool QuestList::FindQuest(int32_t nQuestConfigId)
{
    if (nullptr != GetQuest(nQuestConfigId))
    {
        return true;
    }
    if (FindCompletedQuest(nQuestConfigId))
    {
        return true;
    }
    return false;
}
bool QuestList::IsHaveQuest(int32_t nQuestConfigId)
{
    quest_ptr pQuest = GetQuest(nQuestConfigId);
    if (nullptr != pQuest)
    {
        return true;
    }
    return false;
}

int32_t QuestList::DefeatedQuest(int32_t nQuestConfigId)
{
    quest_ptr pQuest = GetQuest(nQuestConfigId);
    if (nullptr == pQuest)
    {
        return OR_QUEST_UN_ACCEPT;
    }

    return pQuest->DefeatedQuest();
}

int32_t QuestList::GetQuestFromType(int32_t quest_type)
{
    auto it = m_vAcceptQuestTypeSize.find(quest_type);
    if (it == m_vAcceptQuestTypeSize.end())
    {
        return -1;
    }
    auto&& quest_list = it->second;
    return *quest_list.begin();
}

std::size_t QuestList::GetQuestSizeFromType(int32_t quest_type)
{
    auto it = m_vAcceptQuestTypeSize.find(quest_type);
    if (it == m_vAcceptQuestTypeSize.end())
    {
        return 0;
    }
    auto&& quest_list = it->second;
    return quest_list.size();
}

std::size_t QuestList::GetSubQuestTypeListSize()
{
    return m_vAcceptQuestSubTypeSize.size();
}

bool QuestList::HasQuestType(int32_t type)
{
    return GetQuestFromType(type) > 0;
}

QuestList::quest_id_list_type QuestList::GetQuestsFromType(int32_t type)
{
    quest_id_list_type v;
    for (auto it : m_vQestsList)
    {
        if (it.second->GetQuestType() == type)
        {
            v.emplace(it.first);
        }
    }
    return v;
}

void QuestList::GetQuestIdFromMonsterEffect(AvatarQuest::QuestEvent& oEvent, quest_id_list_type& quest_id_list, int32_t effect_type)
{
    if (nullptr == quest_factory_ptr_)
    {
        return;
    }
    i32_set_type   quest_lists = GetQuestListIdFromEventType(oEvent.GetEventType());
    for (auto&& it : quest_lists)
    {
      
        for (auto& sit : quest_factory_ptr_->share_target_amount(it))
        {
            if (sit == effect_type)
            {
                quest_id_list.emplace(it);
            }
        }
    }
}

int32_t QuestList::RemoveQuest(int32_t nQuestConfigId)
{
    if (nQuestConfigId <= 0)
    {
        return OR_OK;
    }
    if (!FindQuest(nQuestConfigId) && FindCompletedQuest(nQuestConfigId))
    {
        return OR_OK;
    }
    OR_CHECK_RESULT(DoRemoveQuest(nQuestConfigId));
    SendToClientRemoveQuest(nQuestConfigId);
    return OR_OK;
}

void QuestList::SendToClientRemoveQuest(int32_t nQuestConfigId)
{
    auto p =  GetAddQuestUpdateObj(E_CLIENT_QUEST_REMOVE, nQuestConfigId);
	QuestData* pb = ::google::protobuf::down_cast<QuestData*>(p);
	pb->set_configid(nQuestConfigId);
}

int32_t QuestList::DoRemoveQuest(int32_t nQuestConfigId)
{
    if (nullptr == quest_factory_ptr_)
    {
        return OR_TABLE_INDEX;
    }
    int32_t group_quest = quest_factory_ptr_->group_quest(nQuestConfigId);
 
    if (group_quest > 0)
    {
        return OR_QUEST_GIVE_UP_QUEST;
    }

    if (OnErase(nQuestConfigId) || m_vCompleteQuestList.erase(nQuestConfigId) > 0)
    {
        return OR_OK;
    }
    return OR_OK;
}


AvatarQuest::QuestEvent::quest_event_ptr QuestList::CreateForTrigger()
{
    static AvatarQuest::QuestEvent q;
    for_trigger_ = true;
    AvatarQuest::QuestEvent::quest_event_ptr ptr(&q, std::bind(&QuestList::AcceptNextQuestListForTrigger, this));
    return ptr;
}

void QuestList::AcceptNextQuestListForTrigger()
{
    for_trigger_ = false;
    if (next_quest_list_.empty())
    {
        return;
    }
    i32_set_type v;
    v.swap(next_quest_list_);
    for (auto& it : v)
    {
        AcceptQuest(it);
    }
}

void QuestList::AcceptNextQuestList(int32_t quest_config_id)
{
    if (!quest_factory_ptr_)
    {
        return;
    }
    auto& oNextQuestsList = quest_factory_ptr_->nextquests_ids(quest_config_id);
    for (auto it = oNextQuestsList.begin(); it != oNextQuestsList.end(); ++it)
    {
        if (*it == select_quest_)
        {
            AcceptNextSelectQuest();
            return;
        }
    }

    for (auto it = oNextQuestsList.begin(); it != oNextQuestsList.end(); ++it)
    {
        AcceptNextQuestListEx(*it);
    }
}

void QuestList::ClearClientSelectQuest()
{
    select_quest_ = kEmptySelectQuest;
}

void QuestList::AcceptNextQuestListEx(int32_t nConfId)
{
    
    if (for_trigger_)
    {
        next_quest_list_.emplace(nConfId);
    }
    else
    {
        AcceptQuest(nConfId);
    }
}

bool QuestList::AcceptNextSelectQuest()
{
    if (nullptr == quest_factory_ptr_)
    {
        return false;
    }

    auto p_quest_ele = QuestTable::Instance().GetElement(select_quest_);
    if (nullptr == p_quest_ele)
    {
        return false;
    }
    if (select_quest_ == kEmptySelectQuest )
    {
        return false;
    }
    AcceptNextQuestListEx(select_quest_);
    return false;
}

void QuestList::InitQuestCallBack(quest_ptr& pQuest)
{
	
}

int32_t QuestList::RemoveQuestFromType(int32_t nQuestType)
{
    quest_id_list_type v = GetQuestsFromType(nQuestType);
    for (quest_id_list_type::iterator  it = v.begin(); it != v.end(); ++it)
    {
        RemoveQuest(*it);
    }
    return OR_OK;
}

int32_t QuestList::RemoveQuestFromTypeDoNotNotifyClient(int32_t nQuestType)
{
    quest_id_list_type v = GetQuestsFromType(nQuestType);
    for (quest_id_list_type::iterator it = v.begin(); it != v.end(); ++it)
    {
        DoRemoveQuest(*it);
    }
    return OR_OK;
}

int32_t QuestList::RemoveAllQuest()
{
    m_vQestsList.clear();
    m_vCompleteQuestList.clear();

    return OR_OK;
}

bool QuestList::CheckRecordToCompleteQuest(int32_t nQuestConfigId)
{
    if (nullptr == quest_factory_ptr_)
    {
        return false;
    }
    //活动任务
    if (quest_factory_ptr_->quest_activity_id(nQuestConfigId) > 0)
    {
        for (auto& it : recoder_quest_callback_)
        {
            if (it(nQuestConfigId))
            {
                return true;
            }
        }
        return false;
    }

    if (quest_factory_ptr_->recur_quest(nQuestConfigId) > 0)
    {
        return false;
    }

    return true;
}

bool QuestList::IsAutoIncrement(int32_t configId)
{
   if (nullptr == quest_factory_ptr_)
   {
       return false;
   }

    if (quest_factory_ptr_->group_quest(configId) > 0)
    {
        return true;
    }

    return false;
}

bool QuestList::IsTeamQuest(int32_t questType)
{
    if (E_LIEMO == questType)
    {
        return true;
    }

    return false;
}

bool QuestList::IsDirectToCompleteStepQuestType(int32_t nEventType)
{
    return nullptr != ClientQuestTargetTypeTable::Instance().GetElement(nEventType);
}

void QuestList::SetDefeatCallBack(int32_t nQuestConfigId, const Quest::defeat_callback_type& cb)
{
    quest_ptr pQuest = GetQuest(nQuestConfigId);
    if (nullptr == pQuest)
    {
        return;
    }

    pQuest->SetDefeatCallBack(cb);
}

std::size_t QuestList::GetMaxGroupSize()
{
    static std::size_t n(QuestGroupTable::Instance().GetAllID().size() + 1);
    return n;
}

std::size_t QuestList::GetQuestStepSize(int32_t quest_id)
{
    quest_ptr pQuest = GetQuest(quest_id);
    if (nullptr == pQuest)
    {
        return 0;
    }
    return pQuest->GetQuestStepSize();
}

void QuestList::UpdateQuestStep(int nQuestConfigId, int Step, int Count)
{
    quest_ptr pQuest = GetQuest(nQuestConfigId);
    if (nullptr == pQuest)
    {
        return;
    }

    if (Step > (int32_t)pQuest->GetQuestStepSize())
    {
        return;
    }
    AvatarQuest::Quest::step_ptr_type pQuestStep = pQuest->GetQuestStep(Step - 1);
    if (pQuestStep)
    {
        if (pQuestStep->IsCompletedStatus())
        {
            return;
        }
        const i32_v_type  ids = pQuestStep->GetTargetIds();
        for (auto id : ids)
        {
            AvatarQuest::QuestEvent  oEvent(pQuestStep->GetTargetType(), id, Count);
            TriggerEvent(nQuestConfigId, oEvent);
        }
    }

}

void QuestList::RestartQuest(int nQuestId)
{
    quest_ptr pQuest = GetQuest(nQuestId);
    if (nullptr == pQuest)
    {
        return;
    }
    pQuest->RestSteps();
    m_vTimerList.cancel(pQuest->GetTimeoutTimerId());
    InitTimeoutQuest(nQuestId);
}

bool QuestList::IsQuestStepAccepted(int32_t nQuestConfigId, int32_t nStep)
{
    quest_ptr pQuest = GetQuest(nQuestConfigId);
    if (nullptr != pQuest)
    {
        return pQuest->IsQuestStepAccepted(nStep);
    }

    if (FindCompletedQuest(nQuestConfigId))
    {
        return false;
    }

    return false;
}

bool QuestList::IsQuestStepComplete(int32_t quest_id, int32_t nStep)
{
    quest_ptr pQuest = GetQuest(quest_id);
    if (nullptr != pQuest)
    {
        return pQuest->IsQuestStepCompleted(nStep);
    }

    if (FindCompletedQuest(quest_id))
    {
        return true;
    }

    return false;
}

bool QuestList::IsQuestLastStep(int32_t quest_id, int32_t nStep)
{
    quest_ptr pQuest = GetQuest(quest_id);
    if (nullptr == pQuest)
    {
        return false;
    }
    pQuest->IsQuestLastStep(nStep);
    return false;
}

int32_t QuestList::GetQuestStepRemainAmount(int32_t quest_id, int32_t quest_step)
{
    quest_ptr pQuest = GetQuest(quest_id);
    if (nullptr == pQuest)
    {
        return 0;
    }
    return pQuest->GetQuestStepRemainAmount(quest_step);
}

int32_t QuestList::GetQuestStepProgress(int32_t quest_id, int32_t quest_step)
{
    quest_ptr pQuest = GetQuest(quest_id);
    if (nullptr == pQuest)
    {
        return 0;
    }
    return pQuest->GetQuestStepProgress(quest_step);
}

int32_t QuestList::GetQuestType(int32_t quest_id)
{
    quest_ptr pQuest = GetQuest(quest_id);
    if (nullptr == quest_factory_ptr_ || nullptr == pQuest)
    {
        return 0;
    }
    return quest_factory_ptr_->quest_type(quest_id);
}

i32_i32_pair_type QuestList::GetQuestTypePair(int32_t quest_config_id)
{
    static i32_i32_pair_type sp(0, 0);
    if (nullptr == quest_factory_ptr_ || 
        nullptr == quest_factory_ptr_)
    {
        return sp;
    }
    int32_t sub_quest_type = quest_factory_ptr_->sub_quest_type(quest_config_id);
    int32_t quest_type = quest_factory_ptr_->quest_type(quest_config_id);
    i32_i32_pair_type p(quest_type, sub_quest_type);
    return p;
}

int32_t QuestList::GetQuestSubType(int32_t quest_id)
{
    quest_ptr pQuest = GetQuest(quest_id);
    if (nullptr == pQuest)
    {
        return 0;
    }
    return pQuest->GetQuestSubType();
}

bool QuestList::CheckNpcDistance(int32_t configId)
{
#ifndef __TEST__
    if (nullptr == m_pHuman)
    {
        return false;
    }

    return m_pHuman->CheckNpcDistance(configId);
#endif//__TEST__
    return true;
}

int32_t QuestList::GetQuestCurrentStep(int32_t quest_id)
{
    auto p_quest = GetQuest(quest_id);
    if (nullptr == p_quest)
    {
        return -1;
    }
    return p_quest->GetQuestCurrentStep();
}

void QuestList::SendUpdateQuest()
{
#ifndef __TEST__
    if (!m_pHuman)
    {
        return;
    }
#endif//__TEST__
    if (update_quest_list_.quest_size() <= 0)
    {
        return;
    }

    emc::i().emit<MsgEventStruct>(emid(), ModuleQuest::RPC_CODE_QUEST_UPDATEQUESTLIST_NOTIFY, &update_quest_list_);
    
	update_quest_list_.clear_quest();
    update_quest_list_.Clear();
}

void QuestList::SendLeaveMirror()
{
    GetAddQuestUpdateObj(E_CLIENT_QUEST_LEAVE_MIRROR, 0);
    SendUpdateQuest();
    bchange_scene_accept_ = true;
}
void QuestList::RestQuestByEventType(int32_t nType)
{
    i32_set_type   quest_lists = GetQuestListIdFromEventType(nType);
    for (auto&& it : quest_lists)
    {
        quest_ptr pQuest =  GetQuest(it);
        if (pQuest)
        {
            pQuest->Reset();
        }
    }
}

void QuestList::OnWorldQuestLoadComplete()
{
    bchange_scene_accept_ = false;
}

::google::protobuf::Message* QuestList::GetAddQuestUpdateObj(int32_t op_code, int32_t quest_config_id)
{
    auto p_quest_pb = update_quest_list_.mutable_quest()->Add();
    p_quest_pb->set_operatator(op_code);
    return p_quest_pb->mutable_updatequestdata();
}

void QuestList::OnQuestAchieve(int32_t quest_config_id)
{
    if (nullptr == quest_factory_ptr_)
    {
        return;
    }
	for (auto& it : achieve_callback_list_)
	{
		it(quest_config_id);
	}    
    quest_factory_ptr_->OnQuestAchieve(quest_config_id, this);
}

const QuestList::quest_list_type& QuestList::GetAllQuest()
{
    return m_vQestsList;
}

const AvatarQuest::QuestList::quest_id_list_type& QuestList::GetAllCompleteIdList() const
{
    return m_vCompleteQuestList;
}

void QuestList::receive(const AvatarQuest::QuestEvent& oEvent)
{
    TriggerEvent(oEvent);
}

void AchievmentList::SendUpdateQuest()
{
	if (achievement_data_list_.empty())
	{
		return;
	}
    AchivementRpcUpdateAchivementNotify client_pb_achievemtn_list_;
	for (auto& it : achievement_data_list_)
	{
		client_pb_achievemtn_list_.add_achivementlist()->CopyFrom(*it.second);
	}
    emc::i().emit<MsgEventStruct>(emid(), 
    ModuleAchivement::RPC_CODE_ACHIVEMENT_UPDATEACHIVEMENT_NOTIFY, &client_pb_achievemtn_list_);

	achievement_data_list_.clear();
	
}

google::protobuf::Message* AchievmentList::GetAddQuestUpdateObj(int32_t op_code, int32_t quest_config_id)
{
	auto it = achievement_data_list_.find(quest_config_id);
	if (achievement_data_list_.find(quest_config_id) != achievement_data_list_.end())
	{
		return it->second->mutable_achivementquestdata();
	}

	achievement_pb_data_ptr_type ptr(new UpdateAchivementList);
	achievement_data_list_.emplace(quest_config_id, ptr);
	it = achievement_data_list_.find(quest_config_id);
	if (it == achievement_data_list_.end())
	{
		static UpdateAchivementList s;
		return s.mutable_achivementquestdata();
	}
	ptr->set_operatator(op_code);
	return ptr->mutable_achivementquestdata();
}

void AchievmentList::SendToClientRemoveQuest(int32_t nConfId)
{
	auto p = GetAddQuestUpdateObj(E_CLIENT_QUEST_REMOVE, nConfId);
	AchivementData * pb = ::google::protobuf::down_cast<AchivementData*>(p);
	pb->set_configid(nConfId);
}

void AchievmentList::OnLogin()
{

	AchivementRpcSendAchievementListNotify pb;

	for (quest_list_type::iterator it = m_vQestsList.begin(); it != m_vQestsList.end(); ++it)
	{
		::AchivementData* pQuestPb = pb.mutable_achivementlist()->add_achivementlist();
		it->second->ToClientPb(pQuestPb);
	}

	for (auto it : m_vCompleteQuestList)
	{
		pb.mutable_completachivementids()->add_idlist(it);
	}

    emc::i().emit<MsgEventStruct>(emid(), ModuleAchivement::RPC_CODE_ACHIVEMENT_SENDACHIEVEMENTLIST_NOTIFY, &pb);
}

bool AchievmentList::CheckTrigger(int32_t nConfId)
{
    return FindAchievedQuest(nConfId) || FindCompletedQuest(nConfId);
}

} // AvatarQuest


