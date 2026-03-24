#include "mission_comp.h"

#include <ranges>

#include "table/code/condition_table.h"
#include "mission/constants/mission.h"

#include "core/macros/return_define.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/mission_error_tip.pb.h"

#include "proto/common/component/mission_comp.pb.h"
#include "proto/common/event/mission_event.pb.h"

MissionsComp::MissionsComp()
{
	// Initialize event-related mission classification containers
	for (uint32_t i = static_cast<uint32_t>(eCondtionType::kConditionKillMonster); i < static_cast<uint32_t>(eCondtionType::kConditionTypeMax); ++i)
	{
		eventMissionsClassify.emplace(i, UInt32Set{});
	}
}

std::size_t MissionsComp::CanGetRewardSize() const
{
	return claimableRewards.count();
}


void MissionsComp::AbandonMission(const uint32_t missionId)
{
    SetBit(MissionBitMap, completedMissions, missionId, false);
}

uint32_t MissionsComp::IsMissionUnaccepted(const uint32_t missionId) const
{
	// Check if the mission has been accepted
	if (missionsComp.missions().find(missionId) != missionsComp.missions().end())
	{
		return kMissionIdRepeated; // Return kMissionIdRepeated if mission is already accepted
	}
	return kSuccess; // Return kSuccess if mission is not accepted
}

uint32_t MissionsComp::IsMissionUncompleted(const uint32_t missionId) const
{
	if (IsComplete(missionId))
	{
		return kMissionAlreadyCompleted; // Return kMissionAlreadyCompleted if mission is already completed
	}
	return kSuccess; // Return kSuccess if mission is not completed
}
