
#include "Quest/Quest.h"

#include "GenCode/Config/ServerScriptCfg.h"

#ifndef __TEST__
#include "GameEnum.h"
#include "GenCode/Quest/QuestModule.h"
#include "Obj/Obj_Human/Obj_Human.h"
#include "Obj/Obj_Module/ObjVechicleModule.h"
#include "Obj/Obj_Module/Exp/Obj_HumanExpModule.h"
#include "ActivityQuestAcceptor.h"
#include "GenCode/Config/VechicleCfg.h"
#include "GenCode/Config/StringQuestCfg.h"

#include "BILogger.h"

#include "Drop/DropManager.h"
#include "DK_TimeManager.h"
#endif // __TEST__

#include "GenCode/GameDefine_Result.h"

#include "QuestFactory.h"

namespace AvatarQuest
{

Quest::Quest(const QuestElement* pQuestElement, CreateQuestParam& param
            )
    : QuestBase(m_oData, param),
      m_pQuestElement(pQuestElement),
      m_pTimerId(nullptr)
{
    OnInit();
}

Quest::Quest(const QuestData& pb, CreateQuestParam& param)
    :
    QuestBase(pb, param),
    m_pTimerId(nullptr)

{
    const QuestElement* pElem = QuestTable::Instance().GetElement(pb.configid());
    if (NULL == pElem)
    {
        return ;
    }

    m_pQuestElement = pElem;
    OnInit();
}

void Quest::OnSave(QuestData& pb)
{
    pb.CopyFrom(m_oData);
}

void Quest::ToClientPb(::google::protobuf::Message* mpb)
{
    QuestData* pb = ::google::protobuf::down_cast<QuestData*>(mpb);
#ifndef __TEST__
    if (!m_pHuman || !m_pHuman->m_pAtivityQuest)
    {
        return;
    }
#endif//__TEST_
	pb->Clear();
    pb->set_configid(m_oData.configid());
    pb->set_status(m_oData.status());
    pb->set_questbegintime(m_oData.questbegintime());

    for (auto it : m_vStepList)
    {
        it->ToClientPb(pb->add_queststeps());
    }

    if (m_vStepList.empty())
    {
        AddClientCompleteStep(*pb);
    }
    

    if (nullptr != m_pQuestElement && m_pQuestElement->quest_limit_time > 0)
    {
        pb->set_questbegintime(m_oData.questbegintime());
    }

#ifndef __TEST__
    if (IsActivityQuest() &&
        m_pHuman->m_pAtivityQuest->WhetherShowedQuestIndex(GetActivityId()))
    {
        pb->set_questindex(m_pHuman->m_pAtivityQuest->GetQuestIndex(GetActivityId()));
        pb->set_questmaxindex(m_pHuman->m_pAtivityQuest->GetQuestMaxIndex(GetActivityId()));
    }

#endif // __TEST__

}

bool Quest::IsActivityQuest()
{
    return GetActivityId() > 0;
}

int32_t Quest::GetActivityId()
{
    if (nullptr == m_pQuestElement)
    {
        return 0;
    }

    return m_pQuestElement->quest_activity_id;
}

void Quest::OnInit()
{
    if (!m_pQuestElement)
    {
        return;
    }
    if (!m_oData.has_questbegintime())
    {
        m_oData.set_questbegintime(g_pTimeManager->CurrentTimeS());
    }
#ifndef __TEST__
    if (!m_pQuestElement->behaviac.empty())
    {
        SetBTPath(m_pQuestElement->behaviac.c_str());
    }
    else
    {
        if (Script())
        {
            Script()->Load(m_pQuestElement->ServerScriptId);
        }
    }

#endif // __TEST__



    if (IsUnAccept())
    {
#ifdef __TEST__
        assert(E_QUEST_STATUS_UN_ACCEPT == m_oData.status());
#endif // 
        SetStatus(E_QUEST_STATUS_ACCEPTED);
        AddStep(m_pQuestElement->target_1_amount, m_pQuestElement->target_1_type);
        AddStep(m_pQuestElement->target_2_amount, m_pQuestElement->target_2_type);
        AddStep(m_pQuestElement->target_3_amount, m_pQuestElement->target_3_type);
        AddStep(m_pQuestElement->target_4_amount, m_pQuestElement->target_4_type);
        AddStep(m_pQuestElement->target_5_amount, m_pQuestElement->target_5_type);
    }

    LoadStep();

}


int32_t Quest::CompleteQuest()
{
#ifndef __TEST__
    if (!m_pHuman || !m_pHuman->m_pAtivityQuest || !m_pHuman->m_pExpModule || !m_pQuestElement)
    {
        return OR_ERROR;
    }
#endif // !__TEST__
    OR_CHECK_RESULT_RETURN_RET(CheckGoOn());
    OR_CHECK_RESULT_RETURN_RET(TryTriggerEventToCompleteQuestStep());
    OR_CHECK_RESULT_RETURN_RET(CheckGetReward());
    SetStatus(E_QUEST_STATUS_COMPLETED);

#ifndef __TEST__

    if (update_quest_callback_)
    {
        auto p_quest_pb = update_quest_callback_(E_CLIENT_QUEST_COMPLETE, GetQuestId());
        ToClientPb(p_quest_pb);
    }

    Call("OnCompleteQuest", this, m_pHuman);
    IScriptHost* pSceneHost = m_pHuman->GetSceneStrategyScriptHost();
    if (pSceneHost)
    {
        pSceneHost->Call("OnCompleteQuest", pSceneHost, m_pHuman, m_pQuestElement->id);
    }

    ILOG("Quest::CompleteQuest guid %llu  quest_configid%d ", m_pHuman->GetGuid(), m_pQuestElement->id);
    {
        const StringQuestElement* pStringQusetElement = StringQuestTable::Instance().GetElement(m_pQuestElement->string_title);
        BILOG::Mission(m_pHuman->GetClientIp(),
                       m_pHuman->GetSnId(),
                       m_pHuman->GetOpenId(),
                       m_pHuman->GetGuid(),
                       m_pHuman->GetLevel(),
                       m_pHuman->GetVipLevel(),
                       2,
                       std::to_string(0),
                       pStringQusetElement == nullptr ? "" : pStringQusetElement->sc,
                       m_pQuestElement->id,
                       2,
                       m_pHuman->GetServiceCode());

    }

#endif // __TEST__


    return OR_OK;
}

int32_t Quest::CheckGoOn()
{
    if (IsTimeOut())
    {
        return OR_QUEST_TIMEOUT_ERROR;
    }

    if (IsFailed())
    {
        return OR_QUEST_FAILED_ERROR;
    }


    if (IsComleted())
    {
        return OR_QUEST_COMPLETED;
    }

    return OR_OK;
}

i32_v_type Quest::GetQuestEventList()
{
    i32_v_type v;

    if (nullptr == m_pQuestElement)
    {
        return v;
    }

    if (m_pQuestElement->target_1_type > 0)
    {
        v.push_back(m_pQuestElement->target_1_type);
    }
    if (m_pQuestElement->target_2_type > 0)
    {
        v.push_back(m_pQuestElement->target_2_type);
    }
    if (m_pQuestElement->target_3_type > 0)
    {
        v.push_back(m_pQuestElement->target_3_type);
    }
    if (m_pQuestElement->target_4_type > 0)
    {
        v.push_back(m_pQuestElement->target_4_type);
    }
    if (m_pQuestElement->target_5_type > 0)
    {
        v.push_back(m_pQuestElement->target_5_type);
    }
    return v;
}

int32_t Quest::CheckGetReward()
{
    if (IsComleted())
    {
        return OR_QUEST_COMPLETED;
    }

    if (!IsAchieved())
    {
        return OR_QUEST_ACHIVE_ERROR;
    }

    return OR_OK;
}

int32_t Quest::GiveUpQuest()
{
#ifndef __TEST__
    if (NULL == m_pQuestElement || nullptr == m_pHuman)
    {
        return OR_OK;
    }
#endif//__TEST__

    if (GetQuestType() == E_MAIN_LINE_TYPE)
    {

        return OR_QUEST_GIVE_UP_QUEST;
    }

    if (m_pQuestElement->prohibit_abandon_quest > 0)
    {

        return OR_QUEST_GIVE_UP_QUEST;
    }
    if (m_pQuestElement->group_quest > 0)
    {

        return OR_QUEST_GIVE_UP_QUEST;
    }
#ifndef __TEST__
    if (nullptr != m_pHuman)
    {
        m_pHuman->OnGiveUpQuest(m_pQuestElement);
    }
#endif // !__TEST__

#ifndef __TEST__
    ILOG("Quest::GiveUpQuest %llu questconfigid%d", m_pHuman->GetGuid(), m_pQuestElement->id);
#endif//__TEST__
    OnDefeat();
    return OR_OK;
}

int32_t Quest::GetQuestStatus()const
{
    return m_oData.status();
}

std::size_t Quest::GetQuestConfigStepSize() const
{
    std::size_t s = 0;
    if (nullptr == m_pQuestElement)
    {
        return s;
    }
    if (m_pQuestElement->target_1_amount > 0)
    {
        ++s;
    }
    if (m_pQuestElement->target_2_amount > 0)
    {
        ++s;
    }
    if (m_pQuestElement->target_3_amount > 0)
    {
        ++s;
    }
    if (m_pQuestElement->target_4_amount > 0)
    {
        ++s;
    }
    if (m_pQuestElement->target_5_amount > 0)
    {
        ++s;
    }
    return s;
}


bool Quest::TriggerEvent(const AvatarQuest::QuestEvent& oEvent)
{
	if (!IsAccept())
	{
		return false;
	}
    bool bChange = false;

    if (nullptr == m_pQuestElement)
    {
        return false;
    }

    if (m_pQuestElement->target_display_type == 1)
    {
        step_list_type::iterator prvit = m_vStepList.end();
        for (step_list_type::iterator it = m_vStepList.begin(); it != m_vStepList.end(); ++it)
        {
            if (prvit != m_vStepList.end() && (*prvit)&&
                !(*prvit)->IsCompletedStatus())
            {
                continue;
            }
            prvit = it;
            if (!(*it))
            {
                continue;
            }
            (*it)->TriggerEvent(oEvent, bChange);
            if (bChange)
            {
                break;
            }
        }
    }
    else
    {
        for (step_list_type::iterator it = m_vStepList.begin(); it != m_vStepList.end(); ++it)
        {
            if (!(*it))
            {
                continue;
            }
            (*it)->TriggerEvent(oEvent, bChange);
        }

    }

	if (bChange)
	{
		Achieve();
	}
    

    return bChange;
}

void Quest::TriggerCompleteEvent()
{
    if (IsComleted())
    {
        return;
    }
    bool bChange = false;
    for (step_list_type::iterator it = m_vStepList.begin(); it != m_vStepList.end(); ++it)
    {
        if (!(*it))
        {
            continue;
        }
        (*it)->TriggerCompleteEvent(bChange);
    }

    Achieve();

}

void Quest::OneStep()
{
    if (m_vStepList.empty())
    {
        return;
    }
    if (IsComleted())
    {
        return;
    }

    bool bNotifyClient = false;
    step_list_type::iterator it = m_vStepList.begin();
    if (!(*it)||(*it)->GetTargetIds().empty())
    {
        return;
    }
    AvatarQuest::QuestEvent  oEvent((*it)->GetTargetType(), *(*it)->GetTargetIds().begin(), 1);
    (*it)->TriggerEvent(oEvent, bNotifyClient);
	if (bNotifyClient)
	{
		Achieve();
	}    
    if (bNotifyClient)
    {
        NotifyQuestUpdate();
    }
}


void Quest::Reset()
{
    if (IsComleted())
    {
        return;
    }

    if (IsAchieved())
    {
        return;
    }

    for (auto& it : m_vStepList)
    {
        it->Reset();
    }
    NotifyQuestReset();
}

void Quest::LoadStep()
{
    if (nullptr == quest_factory_ptr_)
    {
        return;
    }
    for (int32_t i = 0; i < m_oData.queststeps_size(); ++i)
    {
        CreateStepParam param;
        param.p_human_ = m_pHuman;
        param.config_id_ = m_oData.configid();
        param.index_ = i;
        param.p_quest_ptr_ = this;
        param.step_complete_function_ = std::bind(&Quest::OnStepComplete, this, std::placeholders::_1);
        param.notify_client_function_ = std::bind(&Quest::NotifyQuestUpdate, this);
        param.p_quest_ele = m_pQuestElement;

        step_ptr_type ptr = quest_factory_ptr_->CreateStep(param);
        //step_ptr_type ptr(new QuestStep(m_pQuestElement, i, std::bind(&Quest::OnStepComplete, this, std::placeholders::_1),
                                        //std::bind(&Quest::NotifyQuestUpdate, this)));
        if (!ptr)
        {
            continue;
        }
        ptr->OnLoad(*m_oData.mutable_queststeps(i));
        ptr->SetHuman(m_pHuman);
        m_vStepList.push_back(ptr);
    }
}

void Quest::AddStep( int32_t nAmount, int32_t targetType)
{
    if (targetType > 0  && nAmount > 0)
    {
        ::QuestStepData* pData = m_oData.add_queststeps();
        if (pData)
        {
            pData->set_status(QuestStep::E_QUEST_STEP_STATUS_ACCEPTED);
            pData->set_progress(0);
        }
    }
}
void Quest::RestSteps()
{
    SetStatus(E_QUEST_STATUS_ACCEPTED);
    m_oData.set_questbegintime(g_pTimeManager->CurrentTimeS());
    for (int i = 0; i < m_oData.queststeps_size(); ++i)
    {
        ::QuestStepData* pData = m_oData.mutable_queststeps(i);
        if (pData)
        {
            pData->set_status(QuestStep::E_QUEST_STEP_STATUS_ACCEPTED);
            pData->set_progress(0);
        }
    }
    NotifyQuestUpdate();
}

void Quest::AddClientCompleteStep( QuestData& oData)
{
   std::size_t s = GetQuestConfigStepSize();
   for (size_t i = 0; i < s; ++i)
   {
       ::QuestStepData* pData = oData.add_queststeps();
       if (pData)
       {
           pData->set_status(QuestStep::E_QUEST_STEP_STATUS_COMPLETED);
           pData->set_progress(QuestStep::GetTargetAmount(m_pQuestElement, i));
       }
   }
}

bool Quest::IsAllStepsCompleted()
{
    for (step_list_type::iterator it = m_vStepList.begin(); it != m_vStepList.end(); ++it)
    {
        if (!(*it) || !(*it)->IsCompletedStatus())
        {
            return false;
        }

    }
    return true;
}

void Quest::OnStepComplete(int32_t nStep)
{
#ifndef __TEST__
    if (!m_pHuman || !m_pQuestElement)
    {
        return;
    }
    Call("OnStepComplete", this, m_pHuman, nStep);
    IScriptHost* pSceneHost = m_pHuman->GetSceneStrategyScriptHost();
    if (pSceneHost)
    {
        pSceneHost->Call("OnStepComplete", pSceneHost, m_pHuman, m_pQuestElement->id, nStep, GetQuestStepSize());
    }
    {
        //
        const StringQuestElement* pStringQusetElement = StringQuestTable::Instance().GetElement(m_pQuestElement->string_title);
        BILOG::Mission(m_pHuman->GetClientIp(),
                       m_pHuman->GetSnId(),
                       m_pHuman->GetOpenId(),
                       m_pHuman->GetGuid(),
                       m_pHuman->GetLevel(),
                       m_pHuman->GetVipLevel(),
                       2,
                       std::to_string(nStep + 1),
                       pStringQusetElement == nullptr ? "" : pStringQusetElement->sc,
                       m_pQuestElement->id,
                       2,
                       m_pHuman->GetServiceCode());
    }
#endif // !__TEST__
}

void Quest::OnDefeat()
{
    if (m_oDefeatCallBack)
    {
        m_oDefeatCallBack();
    }
}

int32_t Quest::Achieve()
{
    if (IsComleted())
    {
        return OR_QUEST_COMPLETED;
    }
    if (!IsAllStepsCompleted())
    {
        return OR_QUEST_COMPLETED_CONDITION;
    }
    //m_oData.clear_queststeps();
    SetStatus(E_QUEST_STATUS_ACHIEVED);
    NotifyQuestUpdate();
    return OR_OK;
}

void Quest::Award()
{
#ifndef __TEST__
    if (!m_pHuman)
    {
        return;
    }
    if (!m_pHuman->m_pAtivityQuest)
    {
        return;
    }
    if (!m_pHuman->m_pExpModule)
    {
        return;
    }
    if (!m_pQuestElement)
    {
        return;
    }
    ExpModule::self_auto_type p;

    if (m_pHuman->m_pAtivityQuest->IsUseExpBuffFromQuestType(GetActivityId()))
    {
        p = m_pHuman->m_pExpModule->AutoUseExpBuff();
    }

    m_pHuman->SetActiviyId(GetActivityId());

    if (m_pQuestElement->quest_award > 0
        && m_pHuman->m_pAtivityQuest->CanReward(GetActivityId()))
    {
        int32_t reward_size = m_pHuman->m_pAtivityQuest->GetRewardSize(m_pQuestElement->quest_activity_id);
        if (reward_size > 0)
        {
            DropParam oDropParam;
            oDropParam.m_nDropType = DropManager::DROP_RATE;
            oDropParam.m_nDropID = m_pQuestElement->quest_award;
            oDropParam.m_nDropNum = reward_size;
            oDropParam.m_nClassId = EconomisClassEnum::E_QUEST;
            oDropParam.m_nWayID = EconomisClassEnum::E_QUEST_AWARD;
            oDropParam.m_ExpOpID = res::eExpOpEnum::E_Mission;
            oDropParam.m_nQuestDaiyCount = m_pHuman->m_pAtivityQuest->GetProgress(GetActivityId()) + 1;
            g_pDropManager->DropPackageToBag(oDropParam, m_pHuman);
        }
    }

    {
        auto pElement = m_pHuman->GetVechicleModule()->GetVechicleElement();
        if (pElement != nullptr && pElement->quest_id == m_pQuestElement->id)
        {
            m_pHuman->ExitVechicle();
        }
    }


    m_pHuman->SetActiviyId(0);
#endif//__TEST__
}

int32_t Quest::TryTriggerEventToCompleteQuestStep()
{
    OR_CHECK_RESULT_RETURN_RET(CheckGoOn());
    for (auto it : m_vStepList)
    {
        if (!it)
        {
            continue;
        }
        OR_CHECK_RESULT(it->TryTriggerEventToCompleteQuestStep());
    }

    return OR_OK;
}


int32_t Quest::GetQuestCurrentStep()
{
    int32_t cur_idx = -1;
    if (m_vStepList.empty())
    {
        return cur_idx;
    }
    ++cur_idx;
    for (auto& it : m_vStepList)
    {
        if (it->IsAcceptedStatus())
        {
            break;
        }
        ++cur_idx;
    }
    return cur_idx;
}

void Quest::OnTimeOut()
{

    if (IsAchieved() || IsComleted())
    {
        return;
    }
#ifdef __TEST__

    if (m_bTestTimeOutComplete)
    {
        TriggerCompleteEvent();
    }
#endif // __TEST__

#ifndef __TEST__
    if (!m_pHuman || !m_pQuestElement)
    {
        return;
    }
    Call("OnTimeOut", this, m_pHuman, m_pQuestElement->id);

    IScriptHost* pSceneHost = m_pHuman->GetSceneStrategyScriptHost();
    if (pSceneHost)
    {
        pSceneHost->Call("OnTimeOut", pSceneHost, m_pHuman, m_pQuestElement->id);
    }
#endif // !__TEST__

    if (IsAchieved() || IsComleted())
    {
        return;
    }
    SetStatus(E_QUEST_STATUS_TIEMOUT);
    NotifyQuestUpdate();
    OnDefeat();
}

int32_t Quest::DefeatedQuest()
{
    if (IsComleted())
    {
        return OR_QUEST_COMPLETED;
    }
    SetStatus(E_QUEST_STATUS_FAILED);
    NotifyQuestUpdate();
    OnDefeat();

    return OR_OK;
}

void Quest::OnAcceptQuest()
{

#ifndef __TEST__
    if (!m_pHuman)
    {
        return;
    }
    if (!m_pQuestElement)
    {
        return;
    }
    m_pHuman->OnAcceptQuest(m_pQuestElement);
    Call("OnAcceptQuest", this, m_pHuman);
    IScriptHost* pSceneHost = m_pHuman->GetSceneStrategyScriptHost();
    if (pSceneHost)
    {
        pSceneHost->Call("OnAcceptQuest", pSceneHost,  m_pHuman, m_pQuestElement->id);
    }
    if (update_quest_callback_)
    {
        auto p_quest_pb = update_quest_callback_(E_CLIENT_QUEST_ACCEPET, GetQuestId());
        ToClientPb(p_quest_pb);
    }
#endif // __TEST__
}

void Quest::SetQuestProgressAddCallbacks(const QuestStep::progress_add_callbacks_type& cbs)
{
    for (auto& it : m_vStepList)
    {
        it->SetQuestProgressAddCallbacks(cbs);
    }
}

bool Quest::IsQuestStepAccepted(int32_t nStep)
{
    if (nullptr == m_pQuestElement)
    {
        return false;
    }
    if (nStep < 0 || nStep >= (int32_t)GetQuestStepSize())
    {
        return false;
    }
    if (m_pQuestElement->target_display_type == 1)
    {
        for (int32_t i = 0; i < nStep; ++i)
        {
            if (!m_vStepList[i]->IsCompletedStatus())
            {
                return false;
            }
        }
    }
    return m_vStepList[nStep]->IsAcceptedStatus();
}

bool Quest::IsQuestStepCompleted(int32_t quest_step)
{
    if (quest_step < 0 || quest_step >= (int32_t)GetQuestStepSize())
    {
        return false;
    }
    return m_vStepList[quest_step]->IsCompletedStatus();
}

int32_t Quest::GetQuestStepRemainAmount(int32_t quest_step)
{
    if (quest_step < 0 || quest_step >= (int32_t)GetQuestStepSize())
    {
        return 0;
    }

    return m_vStepList[quest_step]->GetQuestStepRemainAmount();
}

int32_t Quest::GetQuestStepProgress(int32_t quest_step)
{
    if (quest_step < 0 || quest_step >= (int32_t)GetQuestStepSize())
    {
        return 0;
    }
    return m_vStepList[quest_step]->GetProgress();
}

bool Quest::IsQuestLastStep(int32_t quest_step)
{
    return (m_vStepList.size() == ((std::size_t)quest_step));
}

} // namespace AvatarQuest


