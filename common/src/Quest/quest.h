#ifndef QUEST_H
#define QUEST_H

#include <functional>
#include <memory>
#include <vector>

#include "GenCode/Config/QuestCfg.h"
#include "PublicStruct.pb.h"

#include "QuestStep.h"

#include "Game/GameTimer.h"
#include "Game/StlDefineType.h"

#include "LuaScript/IScriptHost.h"

#include "QuestBase.h"


//https://www.reddit.com/r/gamedev/comments/6h5npm/how_to_implement_missionsquests_into_a_game_c/
//https://www.reddit.com/r/unrealengine/comments/5xp5ds/free_rpg_quest_system/

namespace AvatarQuest
{

class QuestBaseFactory;

class Quest
    : public QuestBase , public IScriptHost

{
public:

    Quest(const QuestElement* pQuestElement,
        CreateQuestParam& param
         );
    Quest(const QuestData& pb,
        CreateQuestParam& param
         );

    void OnSave(QuestData& pb);
    virtual void ToClientPb(::google::protobuf::Message* mpb)override;
    QuestData GetQuestDataPB()
    {
        return m_oData;
    }

    bool IsActivityQuest();
    virtual int32_t GetActivityId()override;

    int32_t CompleteQuest();

    int32_t GiveUpQuest();
    int32_t GetQuestStatus()const;

    std::size_t GetQuestStepSize()const
    {
        return m_vStepList.size();
    }
    std::size_t GetQuestConfigStepSize()const;

    step_ptr_type GetQuestStep(int32_t nStep)const
    {
        if (nStep < (int32_t)GetQuestStepSize())
        {
            return m_vStepList[nStep];
        }
        return nullptr;
    }
    bool IsAcceptedStatus()const
    {
        return GetQuestStatus() == E_QUEST_STATUS_ACCEPTED;
    }

    bool IsQuestStepAccepted(int32_t nStep);
    bool IsQuestStepCompleted(int32_t nStep);
    int32_t GetQuestStepRemainAmount(int32_t quest_step);
    int32_t GetQuestStepProgress(int32_t quest_step);
    bool IsQuestLastStep(int32_t quest_step);

    bool IsComleted()const
    {
        return m_oData.status() == E_QUEST_STATUS_COMPLETED;
    }

    bool IsAchieved()const
    {
        return m_oData.status() == E_QUEST_STATUS_ACHIEVED;
    }

    bool IsTimeOut()const
    {
        return m_oData.status() == E_QUEST_STATUS_TIEMOUT;
    }

    bool IsFailed()const
    {
        return m_oData.status() == E_QUEST_STATUS_FAILED;
    }

    bool IsUnAccept()const
    {
        return m_oData.status() == E_QUEST_STATUS_UN_ACCEPT;
    }

    //return change
    bool TriggerEvent(const AvatarQuest::QuestEvent& oEvent)override;
    void TriggerCompleteEvent()override;
    void OneStep();

    void SetHuman(Obj_Human* pHuman)
    {
        m_pHuman = pHuman;
    }

    virtual void Reset() override;

    virtual int GetId() const override
    {
        return GetQuestId();
    }
    virtual int GetConfId()  override
    {
        return GetQuestId();
    }

    int32_t GetQuestId()const
    {
        if (NULL == m_pQuestElement)
        {
            return 0;
        }
        return m_pQuestElement->id;
    }


    int32_t GetQuestType() const
    {
        if (NULL == m_pQuestElement)
        {
            return 0;
        }
        return m_pQuestElement->quest_type;
    }

    int32_t GetQuestSubType() const
    {
        if (NULL == m_pQuestElement)
        {
            return 0;
        }
        return m_pQuestElement->sub_quest_type;
    }

    bool IsAutoReward()const
    {
        if (NULL == m_pQuestElement)
        {
            return false;
        }
        return m_pQuestElement->is_auto_awarded > 0;
    }

    bool HasReward()const
    {
        if (NULL == m_pQuestElement)
        {
            return false;
        }
        return m_pQuestElement->quest_award > 0;
    }

    bool HasCompleteNpc()
    {
        if (NULL == m_pQuestElement)
        {
            return false;
        }
        return m_pQuestElement->npc_accept > 0;
    }

    int32_t Achieve();

    void Award();


    void SetStatus(int32_t nstatus)
    {

        if (nstatus < E_QUEST_STATUS_UN_ACCEPT || nstatus > E_QUEST_STATUS_MAX)
        {
            return;
        }

        m_oData.set_status(nstatus);
    }

    int32_t TryTriggerEventToCompleteQuestStep();
    int32_t GetQuestCurrentStep();

    void OnTimeOut();
    int32_t DefeatedQuest();
    void OnAcceptQuest();

    void SetUpdateDataCallBack(const update_quest_callback_type& cb)
    {
        update_quest_callback_ = cb;
    }

    void SetQuestProgressAddCallbacks(const QuestStep::progress_add_callbacks_type& cbs);


    time_t GetBeginTime()
    {
        return m_oData.questbegintime();
    }

    void ScriptSetQuestCompleteStatus()
    {
        TriggerCompleteEvent();
    }
    IScriptHost* GetQuestOwner()
    {
        return (IScriptHost*)m_pHuman;
    }

    bool IsOneTarget()
    {
        return m_vStepList.size() == 1;
    }
    int32_t GetJustOneTagetQuestProgress()
    {
        if (m_vStepList.empty())
        {
            return 0;
        }
        if (IsOneTarget()&& m_vStepList[0])
        {
            return m_vStepList[0]->GetProgress();
        }
        return 0;
    }

    virtual void SetTimeoutTimerId(BaseModule::GameTimer* pTimerId)override
    {
        m_pTimerId = pTimerId;
    }

    virtual BaseModule::GameTimer* GetTimeoutTimerId()override
    {
        return m_pTimerId;
    }

    i32_v_type GetQuestEventList();

    int32_t CheckGoOn();
    void RestSteps();

    virtual void SetDefeatCallBack(const defeat_callback_type& cb)override
    {
        m_oDefeatCallBack = cb;
    }
    

    void SetQuestFactory(quest_factory_ptr_type& ptr) { quest_factory_ptr_ = ptr; }
private:
    int32_t CheckGetReward();
    void OnInit();

    void LoadStep();
    void AddStep(int32_t nAmount, int32_t targetType);
    void AddClientCompleteStep(QuestData& oData);
    bool IsAllStepsCompleted();
    void OnStepComplete(int32_t nStep);
    void OnDefeat();


private:
    defeat_callback_type m_oDefeatCallBack;
    const QuestElement* m_pQuestElement;
    BaseModule::GameTimer* m_pTimerId{ nullptr };
};

} // namespace AvatarQuest

#endif // QUEST_H
