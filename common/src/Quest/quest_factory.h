#ifndef __QUEST_FACTORY_H__
#define __QUEST_FACTORY_H__

#include "QuestBase.h"

class Obj_Human;
namespace AvatarQuest
{

    class QuestBaseFactory;
    class QuestBase;
    class QuestList;

class QuestBaseFactory
{
public:
    static const int32_t kInvalidQuestConfig = -1;
    static const i32_v_type kEmptyI32v;
    using quest_factory_ptr_type = std::shared_ptr<QuestBaseFactory>;
    using quest_ptr = std::shared_ptr<QuestBase>;
    using step_ptr_type = std::shared_ptr<QuestStepBase>;

    virtual ~QuestBaseFactory() {}

    virtual quest_ptr CreateQuest(const QuestData& oData, CreateQuestParam& p);
    virtual step_ptr_type CreateStep(CreateStepParam& p);
    

    virtual int32_t GetEventTypeMin()const { return 0; }
    virtual int32_t GetEventTypeMax()const { return 0; }

    virtual int32_t OnComplete(int32_t quest_id, QuestList * p_quest_list);
    virtual int32_t OnQuestAchieve(int32_t quest_id, QuestList * p_quest_list);
    
    virtual bool CheckSubType() { return false; }
    //quest 
    virtual int32_t GetCompleteQuestTypeType();
    virtual int32_t GetCompleteQuestIdType();
    virtual int32_t npc_accept(int32_t config_id) { return kInvalidQuestConfig; }
    virtual int32_t sub_quest_type(int32_t config_id) { return kInvalidQuestConfig; }
    virtual int32_t group_quest(int32_t config_id) { return kInvalidQuestConfig; }
    virtual int32_t groupbegin_quest(int32_t config_id) { return kInvalidQuestConfig; }
    virtual int32_t groupend_quest(int32_t config_id) { return kInvalidQuestConfig; }
    virtual int32_t quest_activity_id(int32_t config_id) { return kInvalidQuestConfig; }
    virtual int32_t share_target_parameter(int32_t config_id) { return kInvalidQuestConfig; }
    virtual int32_t recur_quest(int32_t config_id) { return kInvalidQuestConfig; }
    virtual int32_t quest_type(int32_t config_id) { return kInvalidQuestConfig; }
    virtual int32_t quest_required_level(int32_t config_id) { return kInvalidQuestConfig; }
    virtual int32_t complete_quest_add_progress_type(int32_t config_id) { return kInvalidQuestConfig; }
    virtual int32_t quest_limit_time(int32_t config_id) { return kInvalidQuestConfig; }
    virtual const i32_v_type& all_id() { return kEmptyI32v; }
    virtual const i32_v_type& target_1_id(int32_t config_id) { return kEmptyI32v; }
    virtual const i32_v_type& target_2_id(int32_t config_id) { return kEmptyI32v; }
    virtual const i32_v_type& target_3_id(int32_t config_id) { return kEmptyI32v; }
    virtual const i32_v_type& target_4_id(int32_t config_id) { return kEmptyI32v; }
    virtual const i32_v_type& target_5_id(int32_t config_id) { return kEmptyI32v; }
    virtual const i32_v_type& pre_quest(int32_t config_id) { return kEmptyI32v; }
    virtual const i32_v_type& share_target_amount(int32_t config_id) { return kEmptyI32v; };
    virtual const i32_v_type& nextquests_ids(int32_t config_id) { return kEmptyI32v; };
    virtual bool check_pre_quest(int32_t config_id, QuestList * p_quest_list) { return true; }
    virtual bool RepeatedAccept(int32_t quest_type) { return false; }
    virtual void ToCompletedClientPb(int32_t nConfigId, QuestData& pb) {}
    virtual void ToUnAcceptClientPb(int32_t nConfigId, QuestData& pb) {}
    virtual void ToCanAcceptClientPb(int32_t nConfigId, QuestData& pb) {}

    //achivement
   
};

class QuestFactory : public QuestBaseFactory
{
public:


    using quest_ptr = std::shared_ptr<QuestBase>;

    virtual quest_ptr CreateQuest(const QuestData& oData, CreateQuestParam& p)override;
    virtual step_ptr_type CreateStep(CreateStepParam& p)override;

    virtual int32_t GetEventTypeMin()const override;
    virtual int32_t GetEventTypeMax()const override;
    virtual bool CheckSubType() { return true; }
    virtual int32_t GetCompleteQuestTypeType()override;
    virtual int32_t GetCompleteQuestIdType()override;
    virtual int32_t OnComplete(int32_t quest_id, QuestList * p_quest_list);
    //quest 
    virtual int32_t npc_accept(int32_t config_id) override;
    virtual int32_t sub_quest_type(int32_t config_id)override;
    virtual int32_t group_quest(int32_t config_id)override;
    virtual int32_t groupbegin_quest(int32_t config_id) override;
    virtual int32_t groupend_quest(int32_t config_id) override;
    virtual int32_t quest_activity_id(int32_t config_id)override;
    virtual int32_t share_target_parameter(int32_t config_id)override;
    virtual int32_t recur_quest(int32_t config_id)override;
    virtual int32_t quest_type(int32_t config_id)override;
    virtual int32_t quest_required_level(int32_t config_id)override;
    virtual int32_t complete_quest_add_progress_type(int32_t config_id)override;
    virtual int32_t quest_limit_time(int32_t config_id) override;
    virtual bool RepeatedAccept(int32_t quest_type)override;
    virtual const i32_v_type& target_1_id(int32_t config_id)override;
    virtual const i32_v_type& target_2_id(int32_t config_id)override;
    virtual const i32_v_type& target_3_id(int32_t config_id)override;
    virtual const i32_v_type& target_4_id(int32_t config_id)override;
    virtual const i32_v_type& target_5_id(int32_t config_id)override;
    virtual const i32_v_type& pre_quest(int32_t config_id)override;
    virtual const i32_v_type& all_id()override;
    virtual const i32_v_type& share_target_amount(int32_t config_id)override;
    virtual const i32_v_type& nextquests_ids(int32_t config_id) override;
    virtual bool check_pre_quest(int32_t config_id, QuestList * p_quest_list)override;
    virtual void ToCompletedClientPb(int32_t nConfigId, QuestData& pb) override;
    virtual void ToUnAcceptClientPb(int32_t nConfigId, QuestData& pb) override;
    virtual void ToCanAcceptClientPb(int32_t nConfigId, QuestData& pb) override;

};


}//AvatarQuest

#endif // !__QUEST_FACTORY_H__
