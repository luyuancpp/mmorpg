#ifndef COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_BASE_H_
#define COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_BASE_H_

#include "src/common_type/common_type.h"
#include "src/game_logic/comp/mission_comp.hpp"
#include "src/game_logic/entity_class/entity_class.h"
#include "src/game_logic/missions/missions_config_template.h"

#include "comp.pb.h"

namespace common
{
    class AcceptMissionBaseP
    {
    public:
        using PBUint32V = ::google::protobuf::RepeatedField<::google::protobuf::uint32 >;
        AcceptMissionBaseP(uint32_t mid, const PBUint32V* condition_id) 
            : mission_id_(mid),
              conditions_id_(condition_id){}

        AcceptMissionBaseP(uint32_t mid, const PBUint32V& condition_id)
            : AcceptMissionBaseP(mid, &condition_id) {}

    protected:
        AcceptMissionBaseP(uint32_t mid) : mission_id_(mid) {}

    public:
        uint32_t mission_id_{ 0 };
        const PBUint32V* conditions_id_{ nullptr };
        bool is_random_condition_{ false };
    };
    
    struct ConditionEvent
    {
        uint32_t type_{ 0 };
        UInt32Vector match_condtion_ids_{};
        uint32_t ammount_{ 1 };
    };

    class MissionsComp : public EntityHandle
    {
    public:
        using event_mission_classify_type = std::unordered_map<uint32_t, UInt32Set>;
        MissionsComp();
        MissionsComp(IMissionConfig* config);

        const event_mission_classify_type& classify_for_unittest() const { return   event_missions_classify_; }
        const MissionMap& missions() { return missions_; }
        const CompleteMissionsId& complete_ids() { return complete_ids_; }
        std::size_t mission_size()const { return missions_.missions().size(); }
        std::size_t complete_size()const { return complete_ids_.missions().size(); }
        std::size_t type_set_size()const { return type_filter_.size(); }
        std::size_t can_reward_size()const { return complete_ids_.can_reward_mission_id().size(); }

        bool IsAccepted(uint32_t mission_id)const
        {
            auto& mission = missions_.missions();
            return mission.find(mission_id) != mission.end();
        }
        bool IsComplete(uint32_t mission_id)const
        {
            auto& complete_ids = complete_ids_.missions();
            return complete_ids.find(mission_id) != complete_ids.end();
        }

        uint32_t GetReward(uint32_t mission_id);
        uint32_t Accept(const AcceptMissionBaseP& param);
        uint32_t AcceptCheck(const AcceptMissionBaseP& param);
        uint32_t Abandon(uint32_t mission_id);
        void CompleteAllMission();

        void receive(const ConditionEvent& c);
       
    private:
        void DelClassify(uint32_t mission_id);
       
        bool UpdateWhenMatchCondition(const ConditionEvent& c, Mission& mission);
        
        void OnMissionComplete(const ConditionEvent& c, const TempCompleteList& temp_complete);
        
        IMissionConfig* config_{ nullptr };
        MissionMap missions_;
        CompleteMissionsId complete_ids_;  
        event_mission_classify_type  event_missions_classify_;//key : classify mision by event type  , value :  misison list
        UInt32PairSet type_filter_;
    };
}//namespace common

#endif // !COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_BASE_H_
