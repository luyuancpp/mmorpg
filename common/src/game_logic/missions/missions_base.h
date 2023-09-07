#pragma once

#include <array>

#include "component_proto/mission_comp.pb.h"
#include "src/common_type/common_type.h"
#include "src/game_logic/missions/missions_config_template.h"
#include "src/util/game_registry.h"

struct CheckTypeRepeated
{
};

class AcceptMissionEvent;
class MissionConditionEvent;

class MissionsComp : public EventOwner
{
public:
    using event_mission_classify_type = std::unordered_map<uint32_t, UInt32Set>;
    MissionsComp();

    const event_mission_classify_type& classify_for_unittest() const { return event_missions_classify_; }
    const MissionsPbComp& missions() { return missions_comp_; }
    std::size_t mission_size() const { return missions_comp_.missions().size(); }
    std::size_t complete_size() const { return static_cast<std::size_t>(missions_comp_.complete_missions_size()); }
    std::size_t type_set_size() const { return type_filter_.size(); }
    std::size_t can_reward_size() const;
    void set_check_mission_type_repeated(const bool repeated) { mission_type_not_repeated_ = repeated; }

    bool IsAccepted(const uint32_t mission_id) const { return missions_comp_.missions().find(mission_id) != missions_comp_.missions().end(); }
    bool IsComplete(const uint32_t mission_id) const { return missions_comp_.complete_missions().find(mission_id) != missions_comp_.complete_missions().end(); }
    static bool IsConditionCompleted(uint32_t condition_id, uint32_t progress_value);
    uint32_t IsUnAccepted(uint32_t mission_id) const;
    uint32_t IsUnCompleted(uint32_t mission_id) const;

    uint32_t GetReward(uint32_t mission_id) const;
    uint32_t Accept(const AcceptMissionEvent& accept_event);
    uint32_t Abandon(uint32_t mission_id);
    void CompleteAllMission();

    void Receive(const MissionConditionEvent& condition_event);

private:
    void DeleteMissionClassify(uint32_t mission_id);
    bool UpdateMission(const MissionConditionEvent& condition_event, MissionPbComp& mission) const;
    void OnMissionComplete(const UInt32Set& completed_missions);

    const IMissionConfig* mission_config_{nullptr};
    MissionsPbComp missions_comp_;
    event_mission_classify_type event_missions_classify_; //key : classify mission by event type  , value :  mission list
    UInt32PairSet type_filter_;
    bool mission_type_not_repeated_{true}; //任务类型不能重复
};

using PlayerMissionList = std::array<MissionsComp, MissionsPbComp::kPlayerMissionSize>;




