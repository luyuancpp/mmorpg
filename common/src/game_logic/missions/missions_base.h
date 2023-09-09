#pragma once

#include <array>

#include "component_proto/mission_comp.pb.h"
#include "src/common_type/common_type.h"
#include "src/game_logic/missions/missions_config_template.h"
#include "src/util/game_registry.h"


class AcceptMissionEvent;
class MissionConditionEvent;

class MissionsComp : public EventOwner
{
public:
    using event_mission_classify_type = std::unordered_map<uint32_t, UInt32Set>;
    MissionsComp();
	
    const auto& classify_for_unittest() const { return event_missions_classify_; }
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
	
    [[nodiscard]] const IMissionConfig* GetMission_Config() const
    {
	    return mission_config_;
    }

    void SetMission_Config(const IMissionConfig* const mission_config)
    {
	    mission_config_ = mission_config;
    }

    [[nodiscard]] MissionsPbComp& GetMissions_Comp()
    {
	    return missions_comp_;
    }

    void SetMissions_Comp(const MissionsPbComp& missions_comp)
    {
	    missions_comp_ = missions_comp;
    }

    [[nodiscard]] event_mission_classify_type& GetEvent_Missions_Classify()
    {
	    return event_missions_classify_;
    }

    void SetEvent_Missions_Classify(const event_mission_classify_type& event_missions_classify)
    {
	    event_missions_classify_ = event_missions_classify;
    }

    [[nodiscard]] UInt32PairSet& GetType_Filter()
    {
	    return type_filter_;
    }

    void SetType_Filter(const UInt32PairSet& type_filter)
    {
	    type_filter_ = type_filter;
    }

    [[nodiscard]] bool IsMission_Type_Not_Repeated() const
    {
	    return mission_type_not_repeated_;
    }

    void SetMission_Type_Not_Repeated(const bool mission_type_not_repeated)
    {
	    mission_type_not_repeated_ = mission_type_not_repeated;
    }

private:
	const IMissionConfig* mission_config_{ nullptr };
	MissionsPbComp missions_comp_;
	event_mission_classify_type event_missions_classify_; //key : classify mission by event type  , value :  mission list
	UInt32PairSet type_filter_;
	bool mission_type_not_repeated_{ true }; //任务类型不能重复
};

using PlayerMissionList = std::array<MissionsComp, MissionsPbComp::kPlayerMissionSize>;




