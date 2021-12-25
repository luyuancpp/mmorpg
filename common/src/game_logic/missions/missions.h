#ifndef COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_H_
#define COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_H_

#include "src/common_type/common_type.h"
#include "src/game_logic/comp/mission_comp.hpp"
#include "src/game_logic/entity_class/entity_class.h"
#include "src/game_logic/missions/missions_config_template.h"

#include "comp.pb.h"

namespace common
{
    struct MakeMissionP
    {
        using PBUint32V = ::google::protobuf::RepeatedField<::google::protobuf::uint32 >;
        MakeMissionP(
            uint32_t mid,
            const PBUint32V* condition_id)
            :
            missionid_(mid),
            conditions_id_(condition_id) {}
        MakeMissionP(
            uint32_t mision_id,
            const PBUint32V& condition_id)
            : 
            missionid_(mision_id),
            conditions_id_(&condition_id) {}

        entt::entity e_{ entt::null };
        uint32_t missionid_{ 0 };
        const PBUint32V* conditions_id_{ nullptr };
    };

    struct MakePlayerMissionParam
    {
        MakePlayerMissionParam(
            uint32_t mision_id)
            : 
            mission_id_(mision_id)
        {}
        uint32_t mission_id_{ 0 };
    };

    struct ConditionEvent
    {
        uint32_t condition_type_{ 0 };
        UInt32Vector condtion_ids_{};
        uint32_t ammount_{ 1 };
    };

    struct MissionIdParam
    {
        uint32_t missin_id_{ 0 };
    };

    class Missions : public EntityHandle
    {
    public:
        using TypeMissionIdMap = std::unordered_map<uint32_t, UInt32Set>;
        Missions();
        Missions(IMissionConfig* config);

        const TypeMissionIdMap& type_mission_id() const { return  type_missions_; }
        const MissionMap& missions() { return missions_; }
        const CompleteMissionsId& complete_ids() { return complete_ids_; }
        std::size_t mission_size()const { return missions_.missions().size(); }
        std::size_t completemission_size()const { return complete_ids_.missions().size(); }
        std::size_t type_set_size()const { return type_set_.size(); }
        std::size_t can_reward_mission_id_size()const { return complete_ids_.can_reward_mission_id().size(); }

        bool IsAcceptedMission(uint32_t mission_id)const
        {
            auto& mission = missions_.missions();
            return mission.find(mission_id) != mission.end();
        }
        bool IsCompleteMission(uint32_t mission_id)const
        {
            auto& complete_ids = complete_ids_.missions();
            return complete_ids.find(mission_id) != complete_ids.end();
        }

        uint32_t GetMissionReward(uint32_t missin_id);
        uint32_t Accept(const MakeMissionP& param);
        uint32_t AcceptCheck(const MakeMissionP& param);
        
        void Abandon(uint32_t mission_id);
        
        void TriggerConditionEvent(const ConditionEvent& c);

        void CompleteAllMission();
       
    private:
        void RemoveMissionTypeSubType(uint32_t mission_id);
       
        bool TriggerCondition(const ConditionEvent& c, Mission& mission);
        
        void OnCompleteMission(const ConditionEvent& c, const TempCompleteList& temp_complete);
        
        IMissionConfig* config_{ nullptr };
        MissionMap missions_;
        CompleteMissionsId complete_ids_;  
        TypeMissionIdMap type_missions_;
        UInt32PairSet type_set_;
    };

    uint32_t RandomMision(const MakePlayerMissionParam& param, Missions& ms);
}//namespace common

#endif // !COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_H_
