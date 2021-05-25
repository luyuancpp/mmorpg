#ifndef QUEST_STEP_BASE_H
#define QUEST_STEP_BASE_H

#include <functional>

#include "GenCode/Config/QuestCfg.h"
#include "PublicStruct.pb.h"

#include "QuestEvent.h"

class Obj_Human;

struct AchievementDataElement;
struct SevenDayQuestElement;

namespace AvatarQuest
{

    class QuestBaseFactory;
    class QuestBase;
    struct CreateQuestParam
    {
        using quest_factory_ptr_type = std::shared_ptr<QuestBaseFactory>;
		using get_progress_type = std::function<int32_t(int32_t, int32_t)>;
        using bool_quest_id_function = std::function<bool(int32_t)>;
        Obj_Human * p_human_{ nullptr };
        quest_factory_ptr_type  quest_factory_ptr_;
		get_progress_type get_progress_;
        bool_quest_id_function check_trigger_callback_;
    };

    struct CreateStepParam
    {
        using step_complete_function = std::function< void(int32_t) >;
        using  notify_client_function = std::function< void() >;
        using quest_ptr = std::shared_ptr<QuestBase>;
        using quest_factory_ptr_type = std::shared_ptr<QuestBaseFactory>;
        Obj_Human * p_human_{ nullptr };
        int32_t config_id_{ 0 };
        int32_t index_{ 0 };
        QuestBase* p_quest_ptr_{ nullptr };
        step_complete_function step_complete_function_;
        notify_client_function notify_client_function_;
        const QuestElement* p_quest_ele{ nullptr };
        const void* p_achivement_element_{ nullptr };
        quest_factory_ptr_type  quest_factory_ptr_;
        bool is_parent_achivement_{ false };
    };

class QuestStepBase
{
public:

    enum eQuestStatusEnum
    {
        E_QUEST_STEP_STATUS_ACCEPTED,
        E_QUEST_STEP_STATUS_COMPLETED,
        E_QUEST_STEP_STATUS_MAX,
    };


    typedef std::function< void() > notify_client_function;

    typedef std::vector<int32_t> i32_v_type;

    typedef std::function<void(int32_t, int32_t, int32_t)> quest_progress_add_callback_type;
    typedef std::vector<quest_progress_add_callback_type> progress_add_callbacks_type;
    using quest_factory_ptr_type = std::shared_ptr<QuestBaseFactory>;
	using get_progress_type = std::function<int32_t(int32_t, int32_t)>;

    QuestStepBase(CreateStepParam& p);
    virtual ~QuestStepBase() {}

    virtual void OnLoad(QuestStepData& pb);
    virtual void OnSave(QuestStepData& pb);

    virtual void Reset();

    virtual void ToClientPb(::google::protobuf::Message* mpb);

    virtual void TriggerCompleteEvent(bool& bChange);
    //Returns change of this step
    virtual void TriggerEvent(const QuestEvent& oEvent, bool& bChange);
 
    bool IsCompletedStatus()const
    {
        if (!m_pData)
        {
            return false;
        }
        return m_pData->status() == E_QUEST_STEP_STATUS_COMPLETED;
    }

    bool IsAcceptedStatus() const
    {
        if (!m_pData)
        {
            return false;
        }
        return m_pData->status() == E_QUEST_STEP_STATUS_ACCEPTED;
    }

    void SetHuman(Obj_Human* pHuman);


    virtual bool DontCheckSceneStep();

    int32_t GetStepIndex()const
    {
        return m_nStepIndex;
    }

    void SetQuestProgressAddCallbacks(const progress_add_callbacks_type& cbs)
    {
        on_quest_progress_add_callbacks_ = cbs;
    }

    virtual bool IsLevelStep();

    virtual int32_t GetTargetType()const;
    virtual int32_t GetTargetAmountType();
   
    virtual const i32_v_type& GetTargetIds()const;
    virtual int32_t GetTargetAmount()const;

    virtual int32_t TryTriggerEventToCompleteQuestStep();
    int32_t GetProgress()
    {
        if (!m_pData)
        {
            return 0;
        }
        return m_pData->progress();
    }

    int32_t GetQuestStepRemainAmount();
protected:
    QuestStepData* m_pData;
    int32_t m_nStepIndex;
    
    notify_client_function notify_client_update_;
    progress_add_callbacks_type on_quest_progress_add_callbacks_;
    Obj_Human* m_pHuman{ nullptr };
    quest_factory_ptr_type  quest_factory_ptr_;
};

}//namespace AvatarQuest

#endif // !QUEST_STEP_BASE_H
