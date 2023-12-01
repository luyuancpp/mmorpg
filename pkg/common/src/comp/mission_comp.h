#pragma once

#include <array>

#include "component_proto/mission_comp.pb.h"
#include "src/type_define/type_define.h"
#include "src/system/mission/missions_config_template.h"
#include "src/util/game_registry.h"


class AcceptMissionEvent;
class MissionConditionEvent;

class MissionsComp : public EventOwner
{
public:
    using event_mission_classify_type = std::unordered_map<uint32_t, UInt32Set>;
    MissionsComp();
	
    [[nodiscard]] const auto& classify_for_unittest() const { return event_missions_classify_; }
    [[nodiscard]] const MissionsPbComp& GetMissionsComp() const { return missions_comp_; }
    [[nodiscard]] std::size_t MissionSize() const { return missions_comp_.missions().size(); }
    [[nodiscard]] std::size_t CompleteSize() const { return static_cast<std::size_t>(missions_comp_.complete_missions_size()); }
    [[nodiscard]] std::size_t TypeSetSize() const { return type_filter_.size(); }
    [[nodiscard]] std::size_t CanGetRewardSize() const;

    [[nodiscard]] const IMissionConfig* GetMissionConfig() const
    {
	    return mission_config_;
    }

    void SetMissionConfig(const IMissionConfig* const mission_config)
    {
	    mission_config_ = mission_config;
    }

    [[nodiscard]] MissionsPbComp& GetMissionsComp()
    {
	    return missions_comp_;
    }
	
    [[nodiscard]] event_mission_classify_type& GetEventMissionsClassify()
    {
	    return event_missions_classify_;
    }
	
    [[nodiscard]] UInt32PairSet& GetTypeFilter()
    {
	    return type_filter_;
    }
	
    [[nodiscard]] bool IsMissionTypeNotRepeated() const
    {
	    return mission_type_not_repeated_;
    }

    void SetMissionTypeNotRepeated(const bool mission_type_not_repeated)
    {
	    mission_type_not_repeated_ = mission_type_not_repeated;
    }

	[[nodiscard]] bool IsAccepted(const uint32_t mission_id) const
    { return missions_comp_.missions().find(mission_id) != missions_comp_.missions().end(); }
	[[nodiscard]] bool IsComplete(const uint32_t mission_id) const
    { return missions_comp_.complete_missions().find(mission_id) != missions_comp_.complete_missions().end(); }
	[[nodiscard]] uint32_t IsUnAccepted(uint32_t mission_id) const;
	[[nodiscard]] uint32_t IsUnCompleted(uint32_t mission_id) const;
	
private:
	const IMissionConfig* mission_config_{ nullptr };
	MissionsPbComp missions_comp_;
	event_mission_classify_type event_missions_classify_; //key : classify mission by event type  , value :  mission list
	UInt32PairSet type_filter_;
	bool mission_type_not_repeated_{ true }; //任务类型不能重复
};

using PlayerMissionList = std::array<MissionsComp, MissionsPbComp::kPlayerMissionSize>;




