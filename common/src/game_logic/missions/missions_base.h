#pragma once

#include <array>

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/missions/missions_config_template.h"

#include "component_proto/mission_comp.pb.h"

struct CheckTypeRepeatd {};

class AcceptMissionEvent;
class MissionConditionEvent;

class MissionsComp : public EventOwner
{
public:
    using event_mission_classify_type = std::unordered_map<uint32_t, UInt32Set>;
    MissionsComp();
   
    const event_mission_classify_type& classify_for_unittest() const { return   event_missions_classify_; }
    const MissionsPbComp& missions() { return missions_comp_pb_; }
    std::size_t mission_size()const { return missions_comp_pb_.missions().size(); }
    std::size_t complete_size()const { return missions_comp_pb_.complete_missions_size(); }
    std::size_t type_set_size()const { return type_filter_.size(); }
    std::size_t can_reward_size();

	void Init();

    bool IsAccepted(uint32_t mission_id)const { return missions_comp_pb_.missions().find(mission_id) != missions_comp_pb_.missions().end(); }
    bool IsComplete(uint32_t mission_id)const { return missions_comp_pb_.complete_missions().find(mission_id) != missions_comp_pb_.complete_missions().end();  }
    bool IsConditionCompleted(uint32_t condition_id, uint32_t progress_value);
    uint32_t IsDoNotAccepted(uint32_t mission_id)const;
    uint32_t IsDoNotCompleted(uint32_t mission_id)const;

    uint32_t GetReward(uint32_t mission_id);
    uint32_t Accept(const AcceptMissionEvent& accept_event);
    uint32_t Abandon(uint32_t mission_id);
    void CompleteAllMission();

    void Receive(const MissionConditionEvent& c);

private:
    void DelMissionClassify(uint32_t mission_id);
    bool UpdateMissionByCompareCondition(const MissionConditionEvent& c, MissionPbComp& mission);
    void OnMissionComplete(const UInt32Set& completed_missions);

    const IMissionConfig* mission_config_{ nullptr };
    MissionsPbComp missions_comp_pb_;
    event_mission_classify_type  event_missions_classify_;//key : classify mision by event type  , value :  misison list
    UInt32PairSet type_filter_;
    
};

using PlayerMissionList = std::array<MissionsComp, MissionsPbComp::kPlayerMissionSize>;




