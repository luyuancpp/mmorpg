#pragma once

#include <array>

#include "proto/logic/component/mission_comp.pb.h"
#include "type_define/type_define.h"
#include "system/mission/missions_config_template.h"
#include "util/game_registry.h"


class AcceptMissionEvent;
class MissionConditionEvent;

class MissionsComp : public EventOwner
{
public:
    using event_mission_classify_type = std::unordered_map<uint32_t, UInt32Set>;
    MissionsComp();
	
    [[nodiscard]] const auto& classify_for_unittest() const { return eventMissionsClassify; }
    [[nodiscard]] const MissionsPbComp& GetMissionsComp() const { return missionsComp; }
    [[nodiscard]] std::size_t MissionSize() const { return missionsComp.missions().size(); }
    [[nodiscard]] std::size_t CompleteSize() const { return static_cast<std::size_t>(missionsComp.complete_missions_size()); }
    [[nodiscard]] std::size_t TypeSetSize() const { return typeFilter.size(); }
    [[nodiscard]] std::size_t CanGetRewardSize() const;

    [[nodiscard]] const IMissionConfig* GetMissionConfig() const
    {
	    return missionConfig;
    }

    void SetMissionConfig(const IMissionConfig* const mission_config)
    {
	    missionConfig = mission_config;
    }

    [[nodiscard]] MissionsPbComp& GetMissionsComp()
    {
	    return missionsComp;
    }
	
    [[nodiscard]] event_mission_classify_type& GetEventMissionsClassify()
    {
	    return eventMissionsClassify;
    }
	
    [[nodiscard]] UInt32PairSet& GetTypeFilter()
    {
	    return typeFilter;
    }
	
    [[nodiscard]] bool IsMissionTypeNotRepeated() const
    {
	    return missionTypeNotRepeated;
    }

    void SetMissionTypeNotRepeated(const bool mission_type_not_repeated)
    {
	    missionTypeNotRepeated = mission_type_not_repeated;
    }

	[[nodiscard]] bool IsAccepted(const uint32_t mission_id) const
    { return missionsComp.missions().find(mission_id) != missionsComp.missions().end(); }
	[[nodiscard]] bool IsComplete(const uint32_t mission_id) const
    { return missionsComp.complete_missions().find(mission_id) != missionsComp.complete_missions().end(); }
	[[nodiscard]] uint32_t IsMissionUnaccepted(uint32_t mission_id) const;
	[[nodiscard]] uint32_t IsMissionUncompleted(uint32_t mission_id) const;
	
private:
	const IMissionConfig* missionConfig{ nullptr };
	MissionsPbComp missionsComp;
	event_mission_classify_type eventMissionsClassify; //key : classify mission by event type  , value :  mission list
	UInt32PairSet typeFilter;
	bool missionTypeNotRepeated{ true }; //任务类型不能重复
};

using PlayerMissionList = std::array<MissionsComp, MissionsPbComp::kPlayerMissionSize>;




