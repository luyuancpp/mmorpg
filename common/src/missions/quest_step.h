#ifndef QUEST_STEP_H
#define QUEST_STEP_H

#include <functional>

#include "GenCode/Config/QuestCfg.h"
#include "PublicStruct.pb.h"

#include "QuestStepBase.h"

class Obj_Human;

namespace AvatarQuest
{

class QuestStep : public QuestStepBase
{
public:
    enum eQuestStatusEnum
    {
        E_QUEST_STEP_STATUS_ACCEPTED,
        E_QUEST_STEP_STATUS_COMPLETED,
        E_QUEST_STEP_STATUS_MAX,
    };

    typedef std::function< void(int32_t) > step_complete_function;
    typedef std::function< void() > notify_client_function;

    typedef std::vector<int32_t> i32_v_type;

    typedef std::function<void(int32_t, int32_t, int32_t)> quest_progress_add_callback_type;
    typedef std::vector<quest_progress_add_callback_type> progress_add_callbacks_type;
	

    QuestStep(CreateStepParam& p);

    void OnLoad(QuestStepData& pb);
    void OnSave(QuestStepData& pb);

    virtual void Reset();

    virtual void ToClientPb(QuestStepData& pb);

    virtual void TriggerCompleteEvent(bool& bChange)override;
    //Returns change of this step
    virtual void TriggerEvent(const QuestEvent& oEvent, bool& bChange)override ;
    bool DontCheckSceneStep();

    virtual bool IsLevelStep() override;

    virtual int32_t GetTargetType()const override;
    virtual int32_t GetTargetAmountType() override;
   
    virtual const i32_v_type& GetTargetIds()const override;
    virtual int32_t GetTargetAmount()const override;
    static int32_t GetTargetAmount(const QuestElement* m_pQuestElement, int32_t nStepIndex);

    virtual int32_t TryTriggerEventToCompleteQuestStep() override;

private:
    const QuestElement* m_pQuestElement;
    step_complete_function m_oCompleteCb;
    
};

}//namespace AvatarQuest

#endif // !QUEST_STEP_H
