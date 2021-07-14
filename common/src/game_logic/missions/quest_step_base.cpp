
#include "QuestStepBase.h"

#include <climits>

#include "CommonLogic/Quest/QuestDefine.h"
#include "GenCode/GameDefine_Result.h"

#ifdef __TEST__
int32_t g_nLevel = 0;
#endif // __TEST__
#ifndef __TEST__
#include "Obj/Obj_Human/Obj_Human.h"
#include "Scene/Scene.h"
#endif//__TEST__
namespace AvatarQuest
{

QuestStepBase::QuestStepBase(CreateStepParam& p)
    : 
      m_nStepIndex(p.index_),
      m_pHuman(NULL),
      m_pData(NULL),
      
      notify_client_update_(p.notify_client_function_),
      quest_factory_ptr_(p.quest_factory_ptr_)
{

}

void QuestStepBase::OnLoad(QuestStepData& pb)
{
    m_pData = &pb;
    if (IsCompletedStatus())
    {
        m_pData->set_progress(GetTargetAmount());
    }
    else
    {
        m_pData->set_progress(std::min(m_pData->progress(), GetTargetAmount()));
    }
}

void QuestStepBase::OnSave(QuestStepData& pb)
{
    if (m_pData)
    {
        pb.CopyFrom(*m_pData);
    }
}

void QuestStepBase::ToClientPb(::google::protobuf::Message* mpb)
{
    
    if (nullptr == m_pData)
    {
        return;
    }
    QuestStepData* pb = ::google::protobuf::down_cast<QuestStepData*>(mpb);
    pb->CopyFrom(*m_pData);
}



void QuestStepBase::TriggerCompleteEvent(bool& bChange)
{

}

void QuestStepBase::TriggerEvent(const QuestEvent& oEvent, bool& bChange)
{

}

void QuestStepBase::SetHuman(Obj_Human* pHuman)
{
    m_pHuman = pHuman;
}


bool QuestStepBase::DontCheckSceneStep()
{
    if (IsLevelStep())
    {
        return true;
    }

    return false;

}

bool QuestStepBase::IsLevelStep()
{
    if (GetTargetType() == E_QUEST_LEVELUP)
    {
        return true;
    }

    return false;
}


int32_t QuestStepBase::GetTargetType()const
{
    return E_QUEST_EVENT_MAX;
}

int32_t QuestStepBase::GetTargetAmountType()
{
    return 0;
}

const i32_v_type& QuestStepBase::GetTargetIds()const
{
    static i32_v_type v {0};
    return v;
}

int32_t QuestStepBase::GetTargetAmount()const
{
    return INT_MAX;
}

int32_t QuestStepBase::TryTriggerEventToCompleteQuestStep()
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

int32_t QuestStepBase::GetQuestStepRemainAmount()
{
    return GetTargetAmount() - GetProgress();
}

void QuestStepBase::Reset()
{

}

}//namespace AvatarQuest
