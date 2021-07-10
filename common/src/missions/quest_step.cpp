#include "QuestStep.h"
#include <climits>

#include "CommonLogic/Quest/QuestDefine.h"
#include "GenCode/GameDefine_Result.h"

#ifndef __TEST__
#include "Obj/Obj_Human/Obj_Human.h"
#include "Scene/Scene.h"
#endif//__TEST__
namespace AvatarQuest
{

QuestStep::QuestStep(CreateStepParam& p)
    : 
      QuestStepBase(p),
      m_pQuestElement(p.p_quest_ele),
      m_oCompleteCb(p.step_complete_function_)
{

}

void QuestStep::OnLoad(QuestStepData& pb)
{
    m_pData = &pb;
    if (m_pQuestElement && IsCompletedStatus())
    {
        m_pData->set_progress(GetTargetAmount());
    }
    else
    {
        m_pData->set_progress(std::min(m_pData->progress(), GetTargetAmount()));
    }
}

void QuestStep::OnSave(QuestStepData& pb)
{
    if (m_pData)
    {
        pb.CopyFrom(*m_pData);
    }
}

void QuestStep::Reset()
{
    if (!m_pData)
    {
        return;
    }
    m_pData->set_status(QuestStep::E_QUEST_STEP_STATUS_ACCEPTED);
    m_pData->set_progress(0);
}

void QuestStep::ToClientPb(QuestStepData& pb)
{
    if (nullptr == m_pData)
    {
        return;
    }
    pb.CopyFrom(*m_pData);
    /*pb.set_progress(m_pData->progress());
    pb.set_status(m_pData->status());*/
}



void QuestStep::TriggerCompleteEvent(bool& bChange)
{

    if (GetTargetIds().empty())
    {
        return;
    }

#ifndef __TEST__

    if (!m_pHuman)
    {
        return;
    }
    if (GetTargetIds().empty())
    {
        return;
    }
    int32_t nLevel = m_pHuman->GetLevel();
    QuestEvent oEvent { GetTargetType(), GetTargetIds()[0], nLevel };
    TriggerEvent(oEvent, bChange);
#else
    QuestEvent oEvent { GetTargetType(), GetTargetIds()[0], GetTargetAmount() };
    TriggerEvent(oEvent, bChange);
#endif//__TEST__


}

void QuestStep::TriggerEvent(const QuestEvent& oEvent, bool& bChange)
{
#ifndef __TEST__
	if (!IsAcceptedStatus())
	{
		return;
	}
    if (!m_pHuman || !m_pData || !m_pQuestElement)
    {
        return;
    }

    if (oEvent.GetCount() <= 0)
    {
        return;
    }

    //升级任务不判断场景
    if (!DontCheckSceneStep())
    {

        if (m_pQuestElement->dungeon_config_id > 0)
        {
            if (m_pHuman->GetDungeonConfigId() != m_pQuestElement->dungeon_config_id)
            {
                return;
            }
        }
        else if (m_pQuestElement->scene_config_id > 0)
        {

            if (m_pHuman->GetSceneConfigId() != m_pQuestElement->scene_config_id)
            {
                return;
            }
        }
    }


#endif
    if (!IsAcceptedStatus())
    {
        return;
    }

    int32_t nTargetType = GetTargetType();
    if (oEvent.GetEventType() != nTargetType)
    {
        return;
    }
    int32_t nOldProgress = m_pData->progress();
    for (auto id : GetTargetIds())
    {
        if (id == 0 && nTargetType == 0)
        {
            continue;
        }
        if (oEvent.GetTargetId() != id && !IsLevelStep())
        {
            continue;

        }
        if (GetTargetAmountType() <= 0)
        {
            m_pData->set_progress(m_pData->progress() + oEvent.GetCount());
            for (auto& it : on_quest_progress_add_callbacks_)
            {
                it(m_pQuestElement->id, GetStepIndex(), oEvent.GetCount());
            }
        }
        else
        {
            m_pData->set_progress(oEvent.GetCount());
        }

        if (nOldProgress != m_pData->progress())
        {
            bChange = true;
        }

        break;
    }

    if (!bChange)
    {
        return;
    }

    if (notify_client_update_)
    {
        notify_client_update_();
    }
    int32_t nTargetAmount = GetTargetAmount();
    m_pData->set_progress(std::min(m_pData->progress(), nTargetAmount));

    if (m_pData->progress() >= nTargetAmount)
    {
        m_pData->set_status(E_QUEST_STEP_STATUS_COMPLETED);
        if (notify_client_update_)
        {
            notify_client_update_();
        }
    }


    if (IsCompletedStatus() && m_oCompleteCb)
    {
        m_oCompleteCb(m_nStepIndex);
    }
}

bool QuestStep::DontCheckSceneStep()
{
    if (IsLevelStep())
    {
        return true;
    }
    return false;
}

bool QuestStep::IsLevelStep()
{
    if (GetTargetType() == E_QUEST_LEVELUP)
    {
        return true;
    }

    return false;
}


int32_t QuestStep::GetTargetType()const
{
    if (!m_pQuestElement)
    {
        return E_QUEST_EVENT_MAX;
    }
    if (m_nStepIndex == 0)
    {
        return m_pQuestElement->target_1_type;
    }
    else if (m_nStepIndex == 1)
    {
        return m_pQuestElement->target_2_type;
    }
    else  if (m_nStepIndex == 2)
    {
        return m_pQuestElement->target_3_type;
    }
    else  if (m_nStepIndex == 3)
    {
        return m_pQuestElement->target_4_type;
    }
    else  if (m_nStepIndex == 4)
    {
        return m_pQuestElement->target_5_type;
    }

    return E_QUEST_EVENT_MAX;

}

int32_t QuestStep::GetTargetAmountType()
{
    if (!m_pQuestElement)
    {
        return 0;
    }
    if (m_nStepIndex == 0)
    {
        return m_pQuestElement->target_1_amount_type;
    }
    if (m_nStepIndex == 1)
    {
        return m_pQuestElement->target_2_amount_type;
    }
    if (m_nStepIndex == 2)
    {
        return m_pQuestElement->target_3_amount_type;
    }
    if (m_nStepIndex == 3)
    {
        return m_pQuestElement->target_4_amount_type;
    }
    if (m_nStepIndex == 4)
    {
        return m_pQuestElement->target_5_amount_type;
    }

    return 0;
}



const i32_v_type& QuestStep::GetTargetIds()const
{
    static i32_v_type v {0};

    if (!m_pQuestElement)
    {
        return v;
    }
    if (m_nStepIndex == 0)
    {
        if (m_pQuestElement->target_1_id.empty())
        {
            return v;
        }
        return m_pQuestElement->target_1_id;
    }
    if (m_nStepIndex == 1)
    {
        if (m_pQuestElement->target_2_id.empty())
        {
            return v;
        }
        return m_pQuestElement->target_2_id;
    }
    if (m_nStepIndex == 2)
    {
        if (m_pQuestElement->target_3_id.empty())
        {
            return v;
        }
        return m_pQuestElement->target_3_id;
    }
    if (m_nStepIndex == 3)
    {
        if (m_pQuestElement->target_4_id.empty())
        {
            return v;
        }
        return m_pQuestElement->target_4_id;
    }
    if (m_nStepIndex == 4)
    {
        if (m_pQuestElement->target_5_id.empty())
        {
            return v;
        }
        return m_pQuestElement->target_5_id;
    }

    return v;
}

int32_t QuestStep::GetTargetAmount()const
{
    return GetTargetAmount(m_pQuestElement, m_nStepIndex);
    if (!m_pQuestElement)
    {
        return INT_MAX;
    }
    if (m_nStepIndex == 0)
    {
        return m_pQuestElement->target_1_amount;
    }
    if (m_nStepIndex == 1)
    {
        return m_pQuestElement->target_2_amount;
    }
    if (m_nStepIndex == 2)
    {
        return m_pQuestElement->target_3_amount;
    }
    if (m_nStepIndex == 3)
    {
        return m_pQuestElement->target_4_amount;
    }
    if (m_nStepIndex == 4)
    {
        return m_pQuestElement->target_5_amount;
    }
    return INT_MAX;
}

int32_t QuestStep::GetTargetAmount(const QuestElement* pQuestElement, int32_t nStepIndex)
{
    if (!pQuestElement)
    {
        return INT_MAX;
    }
    if (nStepIndex == 0)
    {
        return pQuestElement->target_1_amount;
    }
    if (nStepIndex == 1)
    {
        return pQuestElement->target_2_amount;
    }
    if (nStepIndex == 2)
    {
        return pQuestElement->target_3_amount;
    }
    if (nStepIndex == 3)
    {
        return pQuestElement->target_4_amount;
    }
    if (nStepIndex == 4)
    {
        return pQuestElement->target_5_amount;
    }
    return INT_MAX;
}


int32_t QuestStep::TryTriggerEventToCompleteQuestStep()
{
#ifndef __TEST__
    if (!m_pHuman || !m_pHuman->m_oBagModule)
    {
        return OR_QUEST_ERROR;
    }
#endif//__TEST__
    if (IsCompletedStatus())
    {
        return OR_OK;
    }
    if (GetTargetType() == E_QUEST_COMMIT)
    {

#ifndef __TEST__

        std::unordered_map<int32_t, int32_t> v;
        for (auto it : GetTargetIds())
        {
            v.emplace(it, GetTargetAmount());
        }
        ItemParam oItemParam;

        if (!v.empty())
        {
            oItemParam.m_nConfigId = v.begin()->first;
            oItemParam.m_nUseSize = v.begin()->second;
        }

        oItemParam.m_nClassId = EconomisClassEnum::E_QUEST;
        oItemParam.m_nWayId = EconomisClassEnum::E_QUEST_COMMIT;
        OR_CHECK_RESULT(m_pHuman->m_oBagModule->CheckAndRemove(v, oItemParam));
#endif

    }

    return OR_OK;
}


}//namespace AvatarQuest
