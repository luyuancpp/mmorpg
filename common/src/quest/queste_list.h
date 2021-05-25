#ifndef QUEST_MANAGER_H
#define QUEST_MANAGER_H

#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "Quest/QuestBase.h"

#include "PublicStruct.pb.h"
#include "GenCode/Quest/QuestModule.h"
#include "CommonLogic/TimerList/TimerList.h"
#include "LuaScript/IScriptHost.h"
#include "StlDefineType.h"
#include "GenCode/Achivement/AchivementModule.h"
#include "QuestFactory.h"
#include "CommonLogic/GameEvent/GameEvent.h"

class Obj_Human;
class ObjTeamModule;
class AchivementRpcSyncAchivementReply;

namespace AvatarQuest
{
class QuestList
    : public IScriptHost, public entityx::Receiver<QuestList>
{
public:
    typedef std::shared_ptr<QuestBase> quest_ptr;
    typedef std::unordered_map<int32_t, quest_ptr> quest_list_type;
    typedef std::unordered_set<int32_t> quest_id_list_type;
    //typedef std::unordered_set<int32_t> quest_type_list;
    typedef std::vector<int32_t> max_complete_id_list;
    typedef std::unordered_map<int32_t, int32_t> i32_map_type;
    typedef std::unordered_map<int32_t, i32_set_type> quest_event_observers_type;
    typedef std::set<GUID_t> role_id_list_type;
    typedef std::unordered_map<int32_t, i32_set_type> quest_type_size_type;
    typedef std::function<void (int32_t)> quest_id_callback_type;
    typedef std::vector<quest_id_callback_type> quest_id_vector_type;
    typedef std::function<bool(int32_t)> bool_quest_id_callback_type;
    typedef std::vector<bool_quest_id_callback_type> bool_quest_id_vector_type;
    typedef std::function<int32_t(int32_t)> int32_t_quest_id_callback_type;
    typedef std::vector<int32_t_quest_id_callback_type> int32_t_quest_id_vector_type;
    typedef std::vector<QuestData> quest_data_list_type;
    typedef std::function<void(int32_t, int32_t, int32_t)> quest_progress_add_callback_type;
    typedef std::vector<quest_progress_add_callback_type> progress_add_callbacks_type;
    using accpet_type = std::set<i32_i32_pair_type>;
    using trigger_callback = std::function<void(const AvatarQuest::QuestEvent&)>;
    using trigger_callbackv = std::vector<trigger_callback>;

    using quest_factory_ptr_type = std::shared_ptr<QuestBaseFactory>;

    static quest_factory_ptr_type kDefaultFactoryPtr;
    static Obj_Human* kTestEmptyHuman;
    static const int32_t kEmptySelectQuest = 0;

    QuestList(Obj_Human* pHuman = kTestEmptyHuman, quest_factory_ptr_type&ptr = kDefaultFactoryPtr);

    void OnGmClear();

    void OnRegister();
	virtual void OnLogin();

    void Clear();

    void OnLoad(const QuestListData& pb);
    void OnSave(QuestListData& pb);

    void OnLoad(const CompletedQuestList& pb);
    void OnSave(CompletedQuestList& pb);
    void OnSave(user& db, user& dbcache);

    void ToClientPb(QuestRpcSyncQuestReply& pb);
    void ToClientPb(AchivementRpcSyncAchivementReply& pb);
    void ToClientPb(CompletedQuestList& pb);
    void ToClientPb(int32_t nQuestId, QuestData& pb);

    int32_t CompletetQuestFromQuestType(int32_t nQuestType);

    int32_t AcceptQuest(int32_t nConfId);
    int32_t TryAchieveNextQuest(int32_t nConfId);
    int32_t CompleteQuest(int32_t nConfId);
    int32_t DirectToCompleteQuest(int32_t nConfId);
    int32_t DirectToAchiveQuest(int32_t nConfId);
    int32_t TriggerAchieveCompleteQuest(int32_t nConfId);
    int32_t GiveUpQuest(int32_t nConfId);
    int32_t GetQuestStatus(int32_t nConfId);
    int32_t GetReward(int32_t nConfId);

    void AcceptAllQuest();
    void CompleteAllQuest();

    bool FindAcceptedQuest(int32_t nConfId);
    bool FindCompletedQuest(int32_t nConfId);
    bool FindAchievedQuest(int32_t nConfId);
    bool FindCanAcceptNPCQuest(int32_t nConfId);
    virtual bool CheckTrigger(int32_t nConfId);

    std::size_t GetAcceptQuestSize()const;
    std::size_t GetCompleteQuestSize();
    std::size_t GetAcceptQuestSizeFromQuestType(int32_t quest_type)const;

    void TriggerNPCAcceptQuest();

    void LevelUp(int32_t nLevel);

    int32_t RpcCompleteQuestStep(QuestRpcCompleteQuestStepAsk& ask, QuestRpcCompleteQuestStepReply& reply);
    int32_t RpcCompleteQuest(QuestRpcCompleteQuestAsk& ask, QuestRpcCompleteQuestReply& reply);

    int32_t RpcGiveUpQuest(QuestRpcGiveUpQuestAsk& ask, QuestRpcGiveUpQuestReply& reply);

    int32_t RpcGetQuestData(QuestRpcGetQuestDataAsk& ask, QuestRpcGetQuestDataReply& reply);

#ifdef __TEST__
    void OnKillMonster(int32_t nConfigId, int32_t nCount);
#endif//__TEST__


    void ComsueItemToCompleteQuest(i32_map_type& vConfigList);

    void TriggerEvent(int32_t eventType, int32_t nConfigId, int32_t count);
    void TriggerEvent(int32_t questConfigId, int32_t eventType, int32_t nConfigId, int32_t count);
    void TriggerEvent(const AvatarQuest::QuestEvent& oEvent);
    void TriggerEvent(int32_t questConfigId, const AvatarQuest::QuestEvent& oEvent);

    void ProtectNpc(int32_t nConfigId, int32_t nCount);


    void OnNotInGuild();

    bool FindQuest(int32_t nConfId);
    bool IsHaveQuest(int32_t nConfId);//当前接的未完成任务

    int32_t DefeatedQuest(int32_t nDefeat);

    int32_t GetQuestFromType(int32_t type);
    std::size_t GetQuestSizeFromType(int32_t quest_type);
    std::size_t GetSubQuestTypeListSize();
    bool HasQuestType(int32_t type);
    quest_id_list_type GetQuestsFromType(int32_t type);

    void  GetQuestIdFromMonsterEffect(AvatarQuest::QuestEvent& oEvent, quest_id_list_type& quest_id_list, int32_t effect_type);

    int32_t RemoveQuest(int32_t nConfId);

    int32_t RemoveQuestFromType(int32_t nQuestType);
    int32_t RemoveQuestFromTypeDoNotNotifyClient(int32_t nQuestType);
    int32_t RemoveAllQuest();
 
    bool CheckRecordToCompleteQuest(int32_t nConfId);


    bool IsAutoIncrement(int32_t nConfId);
    void RecordCompleteQuest(int32_t nConfId);

    Obj_Human* GetHuman()
    {
        return m_pHuman;
    }

    int32_t GetLevel();


    bool IsTeamQuest(int32_t questType);

    bool IsDirectToCompleteStepQuestType(int32_t nEventType);

    void SetDefeatCallBack(int32_t questId, const QuestBase::defeat_callback_type& cb);

    static std::size_t GetMaxGroupSize();
    static std::size_t GetMinGroupIndex()
    {
        return 1;
    }

    std::size_t GetQuestStepSize(int32_t quest_id);

    void UpdateQuestStep(int QuestId, int Step, int Count);
    void RestartQuest(int QuestId);

    //lua function
    bool IsQuestStepAccepted(int32_t nQuestID, int32_t nStep) override;
    bool IsQuestStepComplete(int32_t nQuestID, int32_t nStep) override;
    bool IsQuestLastStep(int32_t nQuestID, int32_t nStep);
    int32_t GetQuestStepRemainAmount(int32_t quest_id, int32_t quest_step) override;
    int32_t GetQuestStepProgress(int32_t quest_id, int32_t quest_step) override;
    int32_t GetQuestType(int32_t quest_id);
    int32_t GetQuestSubType(int32_t quest_id);

    bool CheckNpcDistance(int32_t configId);

    int32_t GetQuestCurrentStep(int32_t quest_id);

    virtual void SendUpdateQuest();

    void SendLeaveMirror();
    void OnWorldQuestLoadComplete();

    virtual ::google::protobuf::Message* GetAddQuestUpdateObj(int32_t, int32_t);
 
	void OnQuestAchieve(int32_t);
    void RestQuestByEventType(int32_t nType);
#ifdef __TEST__
    void AddTestQuest(quest_ptr& p)
    {
        m_vQestsList.erase(p->GetQuestId());
        OnAddQuest(p->GetQuestId(), p);
        InitTimeoutQuest(p->GetQuestId());
    }



    std::size_t GetEventObserverSize()
    {
        std::size_t n = 0;
        for (auto&& it : m_vQuestEventObservers)
        {
            n += it.second.size();
        }
        return n;
    }


#endif // __TEST__
    void SetCheckUniqueQuest(bool b)
    {
        m_bCheckUniqueQuest = b;
    }
    bool TriggerAnyType(int32_t nId, int32_t nQuestId);

    quest_ptr GetQuest(int32_t nQuestConfigId);

    void SetQuestAcceptCalculator(const quest_id_callback_type& cb)
    {
        m_oQuestAcceptCalculator = cb;
    }

    void AddQuestProgressAdd(const quest_progress_add_callback_type& cb)
    {
        on_quest_progress_add_callbacks_.emplace_back(cb);
    }

    void AddCheckRecoderCallback(const bool_quest_id_callback_type& cb)
    {
        recoder_quest_callback_.emplace_back(cb);
    }

	void AddOnAhieveQuestCallback(const quest_id_callback_type& cb);

    void AddOnRemoveQuestCallback(const quest_id_callback_type& cb)
    {
        on_remove_callback_list_.emplace_back(cb);
    }

    void AddOnAcceptQuestCallback(const quest_id_callback_type& cb)
    {
        on_accept_quest_callback_list_.emplace_back(cb);
    }

    void AddTriggerEvent(const trigger_callback& cb) { trigger_callbackv_.push_back(cb); }
    
    void AddCheckAcceptQuestCallback(const int32_t_quest_id_callback_type& cb);

    void OneStep(int32_t quest_config_id);

    QuestData GetQuestPb(int32_t quest_config_id);

    void ResetQuest(int32_t quest_config_id);
    void ResetQuest(const QuestData& pb);
    const quest_list_type& GetAllQuest();
    const quest_id_list_type& GetAllCompleteIdList() const;

    void receive(const AvatarQuest::QuestEvent& oEvent);

    QuestEvent::quest_event_ptr CreateForTrigger();
    bool ForTrigger() { return for_trigger_; }

    void SetQuestFactory(quest_factory_ptr_type& ptr) { quest_factory_ptr_ = ptr; }

    void ClearClientSelectQuest();
    void SetSelectIndex(int32_t n_index) { select_quest_ = n_index; }
private:
    bool CreateQuest(int32_t nConfId);
    bool CreateQuest(const QuestData& oData);
    void InitTimeoutQuest(int32_t nConfId);
    void OnTimeOut(int32_t nConfigId);
    int32_t OnCompleteQuest(int32_t nConfigId);
    //尝试完成不用找npc交付的任务
    void TryAutoCompleteQuest(i32_v_type& quest_lists);

    void InitQuestEventObserverList();

    i32_i32_pair_type GetQuestTypePair(int32_t quest_config_id);

    void OnAddQuest(int32_t nQuestId, quest_ptr& p);
    bool OnErase(int32_t nQuestId);

    i32_set_type  GetQuestListIdFromEventType(int32_t nQuestType);
    void TriggerEvent(int32_t questConfigId, const AvatarQuest::QuestEvent& oEvent, bool bShared);

    bool CouldNotAcceptQuestType(int32_t quest_config_id);
    //void CheckUIGategoryChanged();
    virtual void SendToClientRemoveQuest(int32_t nConfId);
    int32_t DoRemoveQuest(int32_t nConfId);

    void AcceptNextQuestListForTrigger();
    void AcceptNextQuestList(int32_t nConfId);
    
protected:
    void AcceptNextQuestListEx(int32_t nConfId);
    bool AcceptNextSelectQuest();
	virtual void InitQuestCallBack(quest_ptr& pQuest);
    quest_list_type m_vQestsList;
    quest_id_list_type m_vCompleteQuestList;
    quest_id_list_type m_vCanAcceptNPCQuestList;
    Obj_Human* m_pHuman;
    max_complete_id_list m_vMaxComplteQuest;
    muduo::TimerList m_vTimerList;
    quest_event_observers_type m_vQuestEventObservers;
    quest_id_callback_type m_oQuestAcceptCalculator;
    quest_type_size_type m_vAcceptQuestTypeSize;
    accpet_type m_vAcceptQuestSubTypeSize;
    QuestRpcUpdateQuestListNotify update_quest_list_;
	quest_id_vector_type achieve_callback_list_;
    
    progress_add_callbacks_type on_quest_progress_add_callbacks_;
    bool m_bCheckUniqueQuest {true};
    bool m_bQuestStatusChange { false };
    i32_set_type next_quest_list_;
    bool bchange_scene_accept_{ false };
    bool for_trigger_{ false };
    quest_factory_ptr_type quest_factory_ptr_;
    bool_quest_id_vector_type recoder_quest_callback_;
    quest_id_vector_type on_remove_callback_list_;
    quest_id_vector_type on_accept_quest_callback_list_;
    int32_t_quest_id_vector_type check_accept_quest_callback_list_;
    int32_t select_quest_{ kEmptySelectQuest };
    trigger_callbackv trigger_callbackv_;
};

class AchievmentList : public QuestList
{
public:
	enum eTableId
	{
		E_ACHIEVMENT_TABLE = 0,
		E_SEVEN_DAY_QUEST_TABLE = 1,
	};
	using achievement_pb_data_ptr_type = std::shared_ptr<UpdateAchivementList>;
	using achievement_data_list_type = std::map<int32_t, achievement_pb_data_ptr_type>;

	using QuestList::QuestList;
	virtual void SendUpdateQuest()override;
	virtual ::google::protobuf::Message* GetAddQuestUpdateObj(int32_t, int32_t)override;
	virtual void SendToClientRemoveQuest(int32_t nConfId)override;
	virtual void OnLogin()override;
    virtual bool CheckTrigger(int32_t nConfId)override;
protected:
	
	achievement_data_list_type achievement_data_list_;
};

} // namespace AvatarQuest


#endif // QUEST_MANAGER_H
