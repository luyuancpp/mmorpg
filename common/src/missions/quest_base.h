#ifndef __QUEST_BASE_H__
#define __QUEST_BASE_H__

#include <functional>
#include <memory>
#include <vector>

#include "PublicStruct.pb.h"

#include "QuestStep.h"
#include "CommonLogic/Quest/QuestDefine.h"

#include "Game/GameTimer.h"
#include "Game/StlDefineType.h"

#include "LuaScript/IScriptHost.h"



//https://www.reddit.com/r/gamedev/comments/6h5npm/how_to_implement_missionsquests_into_a_game_c/
//https://www.reddit.com/r/unrealengine/comments/5xp5ds/free_rpg_quest_system/

namespace AvatarQuest
{
    enum eClientUpdateOp
    {
        E_CLIENT_QUEST_ACCEPET = 0,//
        E_CLIENT_QUEST_UPDATE = 1,//
        E_CLIENT_QUEST_COMPLETE = 2,//
        E_CLIENT_QUEST_REMOVE = 3,//删除
        E_CLIENT_QUEST_LEAVE_MIRROR = 4,
        E_CLIENT_QUEST_RESET = 5,
};



class QuestBase
    

{
public:

    typedef std::function<void(int32_t)> quest_id_function;
    typedef std::vector<int32_t> quests_id_type;
    typedef std::shared_ptr<QuestStepBase> step_ptr_type;
    typedef std::vector<step_ptr_type> step_list_type;
    typedef std::function<void()> defeat_callback_type;
    typedef std::function<::google::protobuf::Message*(int32_t, int32_t)> update_quest_callback_type;
    using quest_factory_ptr_type = std::shared_ptr<QuestBaseFactory>;
    using bool_quest_id_function = std::function<bool(int32_t)> ;

    QuestBase(const QuestData& pb,
        CreateQuestParam& param
         );

    QuestBase(const QuestBase&) = delete;
    QuestBase operator=(const QuestBase&) = delete;

    virtual ~QuestBase() {}

    void OnSave(QuestData& pb);
    virtual void ToClientPb(::google::protobuf::Message* mpb);

    QuestData GetQuestDataPB()
    {
        return m_oData;
    }

    virtual bool IsActivityQuest();
    virtual int32_t GetActivityId();



    virtual int32_t CompleteQuest();

    virtual int32_t GiveUpQuest();
    int32_t GetQuestStatus()const;

    std::size_t GetQuestStepSize()const
    {
        return m_vStepList.size();
    }
    virtual std::size_t GetQuestConfigStepSize()const;

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

    virtual bool IsQuestStepAccepted(int32_t nStep);
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
	bool IsAccept()const
	{
		return m_oData.status() == E_QUEST_STATUS_ACCEPTED;
	}
    //return change
    virtual bool TriggerEvent(const AvatarQuest::QuestEvent& oEvent);
    virtual void TriggerCompleteEvent();
    virtual void OneStep();

    void SetHuman(Obj_Human* pHuman)
    {
        m_pHuman = pHuman;
    }

    virtual void Reset() {}

 
    virtual int32_t GetQuestId()const{ return 0;}
    virtual int32_t GetQuestType() const{ return 0; }
    virtual int32_t GetQuestSubType() const { return 0;}
    virtual bool IsAutoReward()const{  return false;  }

    virtual bool HasReward()const
    {
       return 0;
    }

    virtual bool HasCompleteNpc()
    {
       return 0;
    }

    virtual int32_t Achieve();

    virtual void Award();


    void SetStatus(int32_t nstatus)
    {
        if (nstatus < E_QUEST_STATUS_UN_ACCEPT || nstatus > E_QUEST_STATUS_MAX)
        {
            return;
        }
        m_oData.set_status(nstatus);
    }

    virtual int32_t TryTriggerEventToCompleteQuestStep();
    virtual int32_t GetQuestCurrentStep();

    virtual void OnTimeOut();
    virtual int32_t DefeatedQuest();
    virtual void OnAcceptQuest();

    virtual void SetDefeatCallBack(const defeat_callback_type& cb)
    {
    }

	void SetAchieveCallback(const quest_id_function& cb)
	{
		achieve_call_back_ = cb;
	}

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
        if (IsOneTarget())
        {
            return m_vStepList[0]->GetProgress();
        }
        return 0;
    }

    virtual void SetTimeoutTimerId(BaseModule::GameTimer* pTimerId){}

    virtual BaseModule::GameTimer* GetTimeoutTimerId() { return nullptr; }

    virtual i32_v_type GetQuestEventList();
 
#ifdef __TEST__
    bool m_bTestTimeOutComplete { false };
#endif // __TEST__

    virtual int32_t CheckGoOn();
    virtual void RestSteps();

	virtual void NotifyQuestUpdate();
    void NotifyQuestReset();

    void SetQuestFactory(quest_factory_ptr_type& ptr) { quest_factory_ptr_ = ptr; }

    virtual int32_t CheckGetReward();


    virtual void LoadStep();

    bool IsAllStepsCompleted();
    virtual void OnStepComplete(int32_t nStep);
    virtual void OnDefeat();


protected:
    QuestData m_oData;
	quest_id_function achieve_call_back_;
    step_list_type m_vStepList;
    Obj_Human* m_pHuman;
    update_quest_callback_type update_quest_callback_;
    quest_factory_ptr_type quest_factory_ptr_;

};

} // namespace AvatarQuest

#endif // __QUEST_BASE_H__
