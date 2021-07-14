#include "QuestFactory.h"

#include "Quest.h"
#include "GenCode/GameDefine_Result.h"
#include "GenCode/Config/RepeatedQuestCfg.h"
#include "CommonLogic/Quest/QuestDefine.h"
#include "Quest/QuestList.h"
#include "GenCode/Config/QuestGroupCfg.h"
#ifndef __TEST__
#include "Quest/AchievementEvent.h"
#include "Obj/Obj_Human/Obj_Human.h"
#endif // !__TEST__


namespace AvatarQuest
{

const i32_v_type QuestBaseFactory::kEmptyI32v;

QuestBaseFactory::quest_ptr QuestBaseFactory::CreateQuest(const QuestData& oData, CreateQuestParam& p)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(oData.configid());
    if (NULL == pElem)
    {
        return nullptr;
    }
    quest_ptr ptr(new Quest(oData, p));
    
    return ptr;
}

AvatarQuest::QuestBaseFactory::step_ptr_type QuestBaseFactory::CreateStep(CreateStepParam& p)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(p.config_id_);
    if (NULL == pElem)
    {
        return nullptr;
    }
    if (nullptr == p.p_quest_ptr_)
    {
        return nullptr;
    }
    step_ptr_type ptr(new QuestStep(p));
    return ptr;

}

int32_t QuestBaseFactory::OnComplete(int32_t quest_id, QuestList * p_quest_list)
{
    return OR_OK;
}

int32_t QuestBaseFactory::OnQuestAchieve(int32_t quest_id, QuestList * p_quest_list)
{
    return OR_OK;
}

int32_t QuestBaseFactory::GetCompleteQuestTypeType()
{
    return 0;
}

int32_t QuestBaseFactory::GetCompleteQuestIdType()
{
    return 0;
}

AvatarQuest::QuestFactory::quest_ptr QuestFactory::CreateQuest(const QuestData& oData, CreateQuestParam& p)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(oData.configid());
    if (NULL == pElem)
    {
        return nullptr;
    }
    quest_ptr ptr(new Quest(oData, p));
    return ptr;
}

QuestFactory::step_ptr_type QuestFactory::CreateStep(CreateStepParam& p)
{
    const QuestElement* pElem = p.p_quest_ele;
    if (nullptr == p.p_quest_ele)
    {
        pElem = QuestTable::Instance().GetElement(p.config_id_);
    }

    if (NULL == pElem)
    {
        return nullptr;
    }
    if (nullptr == p.p_quest_ptr_)
    {
        return nullptr;
    }
    step_ptr_type ptr(new QuestStep(p));
    return ptr;
}

int32_t QuestFactory::GetEventTypeMin() const
{
    return E_QUEST_KILL_MONSTER;
}

int32_t QuestFactory::GetEventTypeMax() const
{
    return E_QUEST_EVENT_MAX;
}

int32_t QuestFactory::GetCompleteQuestTypeType()
{
    return AvatarQuest::E_COMPLTE_TYPE_QUEST;
}

int32_t QuestFactory::GetCompleteQuestIdType()
{
    return AvatarQuest::E_COMPLETE_QUEST_ID;
}

int32_t QuestFactory::OnComplete(int32_t quest_id, QuestList * p_quest_list)
{
    if (nullptr == p_quest_list)
    {
        return OR_NULL_PTR;
    }
#ifndef __TEST__
    auto p_human = p_quest_list->GetHuman();
    if (nullptr == p_human)
    {
        return OR_NULL_PTR;
    }
#endif // !__TEST__

    int32_t nProgress = complete_quest_add_progress_type(quest_id);
    if (nProgress <= 0)
    {
        nProgress = 1;
    }

    int32_t n_quest_type = quest_type(quest_id);
    int32_t complete_quest_type_type = GetCompleteQuestTypeType();

    AvatarQuest::QuestEvent  oEvent(AvatarQuest::E_COMPLTE_TYPE_QUEST, n_quest_type, nProgress);
#ifndef __TEST__
    p_human->emit<AvatarQuest::QuestEvent>(oEvent);
#else
    p_quest_list->TriggerEvent(oEvent);
#endif // !__TEST__

    

    AvatarQuest::QuestEvent  oEventComplete(AvatarQuest::E_COMPLETE_QUEST_ID, quest_id, nProgress);
#ifndef __TEST__
    p_human->emit<AvatarQuest::QuestEvent>(oEventComplete);
#else
    p_quest_list->TriggerEvent(oEventComplete);
#endif // !__TEST__
#ifndef __TEST__
    AchievementEvent(p_quest_list->GetHuman(), AvatarQuest::E_ACHIEVEMENT_FINISH_QUEST, quest_id, 0, 0, 0, 0, 1);
#endif//__TEST__
    
    return OR_OK;
}

int32_t QuestFactory::npc_accept(int32_t config_id)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_id);
    if (NULL == pElem)
    {
        return kInvalidQuestConfig;
    }
    return pElem->npc_accept;
}

int32_t QuestFactory::sub_quest_type(int32_t config_id)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_id);
    if (NULL == pElem)
    {
        return kInvalidQuestConfig;
    }
    return pElem->sub_quest_type;
}

const i32_v_type& QuestFactory::target_1_id(int32_t config_id)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_id);
    if (NULL == pElem)
    {
        return kEmptyI32v;
    }
    return pElem->target_1_id;
}

const i32_v_type& QuestFactory::target_2_id(int32_t config_id)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_id);
    if (NULL == pElem)
    {
        return kEmptyI32v;
    }
    return pElem->target_2_id;
}

const i32_v_type& QuestFactory::target_3_id(int32_t config_id)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_id);
    if (NULL == pElem)
    {
        return kEmptyI32v;
    }
    return pElem->target_3_id;
}

const i32_v_type& QuestFactory::target_4_id(int32_t config_id)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_id);
    if (NULL == pElem)
    {
        return kEmptyI32v;
    }
    return pElem->target_4_id;
}

const i32_v_type& QuestFactory::target_5_id(int32_t config_id)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_id);
    if (NULL == pElem)
    {
        return kEmptyI32v;
    }
    return pElem->target_5_id;
}

int32_t QuestFactory::group_quest(int32_t config_id)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_id);
    if (NULL == pElem)
    {
        return kInvalidQuestConfig;
    }
    return pElem->group_quest;
}

static const std::size_t kRangeSize = 2;
int32_t QuestFactory::groupbegin_quest(int32_t config_id)
{
    auto p_ele = QuestGroupTable::Instance().GetElement(config_id);
    if (nullptr == p_ele)
    {
        return 0;
    }
    if (p_ele->quest_range_id.size() < kRangeSize)
    {
        return 0;
    }
    return p_ele->quest_range_id[0];
}

int32_t QuestFactory::groupend_quest(int32_t config_id)
{
    auto p_ele = QuestGroupTable::Instance().GetElement(config_id);
    if (nullptr == p_ele)
    {
        return 0;
    }

    if (p_ele->quest_range_id.size() < kRangeSize)
    {
        return 0;
    }
    return p_ele->quest_range_id[1];
}

int32_t QuestFactory::quest_activity_id(int32_t config_id)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_id);
    if (NULL == pElem)
    {
        return kInvalidQuestConfig;
    }
    return pElem->quest_activity_id;
}

const i32_v_type& QuestFactory::share_target_amount(int32_t config_id)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_id);
    if (NULL == pElem)
    {
        return kEmptyI32v;
    }
    return pElem->share_target_amount;
}

const i32_v_type& QuestFactory::nextquests_ids(int32_t config_id)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_id);
    if (NULL == pElem)
    {
        return kEmptyI32v;
    }
    const QuestElement* m_pQuestElement = QuestTable::Instance().GetElement(config_id);
    if (NULL == m_pQuestElement)
    {
        return kEmptyI32v;
    }

    if (!m_pQuestElement->quest_next_id.empty())
    {
        if (0 == m_pQuestElement->quest_next_id[0])
        {
            return kEmptyI32v;
        }
    }
    return pElem->quest_next_id;
}

bool QuestFactory::check_pre_quest(int32_t config_id, QuestList * p_quest_list)
{
    if (nullptr == p_quest_list)
    {
        return false;
    }
    auto& v = pre_quest(config_id);
    for (std::vector<int32_t>::const_iterator qit = v.begin(); qit != v.end(); ++qit)
    {
        if (*qit <= 0)
        {
            continue;
        }

        if (!p_quest_list->FindCompletedQuest(*qit))
        {
            return false;
        }
    }
    return true;
}

int32_t QuestFactory::share_target_parameter(int32_t config_id)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_id);
    if (NULL == pElem)
    {
        return kInvalidQuestConfig;
    }
    return pElem->share_target_parameter;
}

int32_t QuestFactory::recur_quest(int32_t config_id)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_id);
    if (NULL == pElem)
    {
        return kInvalidQuestConfig;
    }
    return pElem->recur_quest;
}

int32_t QuestFactory::quest_type(int32_t config_id)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_id);
    if (NULL == pElem)
    {
        return kInvalidQuestConfig;
    }
    return pElem->quest_type;
}

int32_t QuestFactory::quest_required_level(int32_t config_idid)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_idid);
    if (NULL == pElem)
    {
        return kInvalidQuestConfig;
    }
    return pElem->quest_required_level;
}

const i32_v_type& QuestFactory::pre_quest(int32_t config_id)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_id);
    if (NULL == pElem)
    {
        return kEmptyI32v;
    }
    return pElem->pre_quest;
}

const i32_v_type& QuestFactory::all_id()
{
    return QuestTable::Instance().GetAllID();
}

int32_t QuestFactory::complete_quest_add_progress_type(int32_t config_id)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_id);
    if (NULL == pElem)
    {
        return kInvalidQuestConfig;
    }
    return pElem->complete_quest_add_progress_type_;
}

int32_t QuestFactory::quest_limit_time(int32_t config_id)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(config_id);
    if (NULL == pElem)
    {
        return kInvalidQuestConfig;
    }
    return pElem->quest_limit_time;
}

bool QuestFactory::RepeatedAccept(int32_t quest_type)
{
    return nullptr != RepeatedQuestTable::Instance().GetElement(quest_type);
}


void QuestFactory::ToCompletedClientPb(int32_t nConfigId, QuestData& pb)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(nConfigId);
    if (NULL == pElem)
    {
        return;
    }
    ::QuestStepData* pStep = pb.add_queststeps();
    if (!pStep)
    {
        return;
    }
    pStep->set_progress(pElem->target_1_amount);

    pb.set_configid(nConfigId);
    pb.set_status(E_QUEST_STATUS_COMPLETED);
}

void QuestFactory::ToUnAcceptClientPb(int32_t nConfigId, QuestData& pb)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(nConfigId);
    if (NULL == pElem)
    {
        return;
    }

    pb.set_configid(nConfigId);
    pb.set_status(E_QUEST_STATUS_UN_ACCEPT);
}

void QuestFactory::ToCanAcceptClientPb(int32_t nConfigId, QuestData& pb)
{
    const QuestElement* pElem = QuestTable::Instance().GetElement(nConfigId);
    if (NULL == pElem)
    {
        return;
    }

    pb.set_configid(nConfigId);
    pb.set_status(E_QUEST_STATUS_CANACCEPT);
}


}


