
#include "QuestBase.h"

#ifndef __TEST__
#include "Obj/Obj_Human/Obj_Human.h"
#include "ActivityQuestAcceptor.h"

#include "DK_TimeManager.h"
#endif // __TEST__

#include "GenCode/GameDefine_Result.h"



namespace AvatarQuest
{

QuestBase::QuestBase(const QuestData& pb, CreateQuestParam& param)
    :
    m_pHuman(param.p_human_),
    quest_factory_ptr_(param.quest_factory_ptr_)
{
    m_oData.CopyFrom(pb);
    if (m_oData.status() <= E_QUEST_STATUS_UN_ACCEPT)
    {
        SetStatus(E_QUEST_STATUS_UN_ACCEPT);
    }
}

void QuestBase::OnSave(QuestData& pb)
{
    pb.CopyFrom(m_oData);
}

void QuestBase::ToClientPb(::google::protobuf::Message* mpb)
{
}

bool QuestBase::IsActivityQuest()
{
    return GetActivityId() > 0;
}

int32_t QuestBase::GetActivityId()
{
    return 0;
}





int32_t QuestBase::CompleteQuest()
{
    OR_CHECK_RESULT_RETURN_RET(CheckGoOn());
    OR_CHECK_RESULT_RETURN_RET(TryTriggerEventToCompleteQuestStep());
    OR_CHECK_RESULT_RETURN_RET(CheckGetReward());
    SetStatus(E_QUEST_STATUS_COMPLETED);
    if (update_quest_callback_)
    {
        auto p_quest_pb = update_quest_callback_(E_CLIENT_QUEST_COMPLETE, GetQuestId());
        ToClientPb(p_quest_pb);
    }
    return OR_OK;
}

int32_t QuestBase::CheckGoOn()
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

i32_v_type QuestBase::GetQuestEventList()
{
    return i32_v_type();
}


int32_t QuestBase::CheckGetReward()
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

int32_t QuestBase::GiveUpQuest()
{
    return OR_OK;
}

int32_t QuestBase::GetQuestStatus()const
{
    return m_oData.status();
}

std::size_t QuestBase::GetQuestConfigStepSize() const
{
    std::size_t s = 0;
    return s;
}


bool QuestBase::TriggerEvent(const AvatarQuest::QuestEvent& oEvent)
{
    if (IsComleted())
    {
        return false;
    }
    bool bChange = false;
	for (auto& it : m_vStepList)
	{
		it->TriggerEvent(oEvent, bChange);
	}
	if (bChange)
	{
		Achieve();
	}
    return bChange;
}

void QuestBase::TriggerCompleteEvent()
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

	if (bChange)
	{
		Achieve();
	}
    
}

void QuestBase::OneStep()
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
    Achieve();
    if (bNotifyClient)
    {
        NotifyQuestUpdate();
    }
}

void QuestBase::LoadStep()
{
}

void QuestBase::RestSteps()
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

bool QuestBase::IsAllStepsCompleted()
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

void QuestBase::OnStepComplete(int32_t nStep)
{

}

void QuestBase::OnDefeat()
{
  
}

void QuestBase::NotifyQuestUpdate()
{
	if (update_quest_callback_)
	{
		auto p_quest_pb = update_quest_callback_(E_CLIENT_QUEST_UPDATE, GetQuestId());
		ToClientPb(p_quest_pb);
	}
}


void QuestBase::NotifyQuestReset()
{
    if (update_quest_callback_)
    {
        auto p_quest_pb = update_quest_callback_(E_CLIENT_QUEST_RESET, GetQuestId());
        ToClientPb(p_quest_pb);
    }
}

int32_t QuestBase::Achieve()
{
    if (IsComleted())
    {
        return OR_QUEST_COMPLETED;
    }
    if (!IsAllStepsCompleted())
    {
        return OR_QUEST_COMPLETED_CONDITION;
    }

	if (IsAchieved())
	{
		return OR_OK;
	}
    //m_oData.clear_queststeps();

	
    SetStatus(E_QUEST_STATUS_ACHIEVED);
	if (achieve_call_back_)
	{
		achieve_call_back_(GetQuestId());
	}
    NotifyQuestUpdate();
    return OR_OK;
}

void QuestBase::Award()
{

}

int32_t QuestBase::TryTriggerEventToCompleteQuestStep()
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


int32_t QuestBase::GetQuestCurrentStep()
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

void QuestBase::OnTimeOut()
{
}

int32_t QuestBase::DefeatedQuest()
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

void QuestBase::OnAcceptQuest()
{
#ifndef __TEST__
    if (!m_pHuman)
    {
        return;
    }

    if (update_quest_callback_)
    {
        auto p_quest_pb = update_quest_callback_(E_CLIENT_QUEST_ACCEPET, GetQuestId());
        ToClientPb(p_quest_pb);
    }
#endif // __TEST__
}

void QuestBase::SetQuestProgressAddCallbacks(const QuestStep::progress_add_callbacks_type& cbs)
{
    for (auto& it : m_vStepList)
    {
        it->SetQuestProgressAddCallbacks(cbs);
    }
}

bool QuestBase::IsQuestStepAccepted(int32_t nStep)
{
    if (nStep < 0 || nStep >= (int32_t)GetQuestStepSize())
    {
        return false;
    }
  
    return m_vStepList[nStep]->IsAcceptedStatus();
}

bool QuestBase::IsQuestStepCompleted(int32_t quest_step)
{
    if (quest_step < 0 || quest_step >= (int32_t)GetQuestStepSize())
    {
        return false;
    }
    return m_vStepList[quest_step]->IsCompletedStatus();
}

int32_t QuestBase::GetQuestStepRemainAmount(int32_t quest_step)
{
    if (quest_step < 0 || quest_step >= (int32_t)GetQuestStepSize())
    {
        return 0;
    }

    return m_vStepList[quest_step]->GetQuestStepRemainAmount();
}

int32_t QuestBase::GetQuestStepProgress(int32_t quest_step)
{
    if (quest_step < 0 || quest_step >= (int32_t)GetQuestStepSize())
    {
        return 0;
    }
    return m_vStepList[quest_step]->GetProgress();
}

bool QuestBase::IsQuestLastStep(int32_t quest_step)
{
    return (m_vStepList.size() == ((std::size_t)quest_step));
}

} // namespace AvatarQuest


