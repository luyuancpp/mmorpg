#include "mission_comp.h"

#include <ranges>

#include "table/code/condition_table.h"
#include "mission/constants/mission.h"

#include "core/macros/return_define.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/mission_error_tip.pb.h"

#include "proto/logic/component/mission_comp.pb.h"
#include "proto/logic/event/mission_event.pb.h"

MissionsComponent::MissionsComponent()
{
	// Initialize event-related mission classification containers
	for (uint32_t i = static_cast<uint32_t>(eCondtionType::kConditionKillMonster); i < static_cast<uint32_t>(eCondtionType::kConditionTypeMax); ++i)
	{
		eventMissionsClassify.emplace(i, UInt32Set{});
	}
}

std::size_t MissionsComponent::CanGetRewardSize() const
{
	// Retrieve mission reward component
	auto& missionReward = tlsRegistryManager.actorRegistry.get_or_emplace<RewardListPBComponent>(EventOwner());
	return static_cast<std::size_t>(missionReward.can_reward_mission_id_size()); // Return the number of missions eligible for reward
}


void MissionsComponent::AbandonMission(const uint32_t missionId)
{
    if (!MissionBitMap.contains(missionId))
    {
        return;
    }

    completedMissions.set(MissionBitMap.at(missionId), false);
}

uint32_t MissionsComponent::IsMissionUnaccepted(const uint32_t missionId) const
{
	// Check if the mission has been accepted
	if (missionsComp.missions().find(missionId) != missionsComp.missions().end())
	{
		return kMissionIdRepeated; // Return kMissionIdRepeated if mission is already accepted
	}
	return kSuccess; // Return kSuccess if mission is not accepted
}

uint32_t MissionsComponent::IsMissionUncompleted(const uint32_t missionId) const
{
	if (IsComplete(missionId))
	{
		return kMissionAlreadyCompleted; // Return kMissionAlreadyCompleted if mission is already completed
	}
	return kSuccess; // Return kSuccess if mission is not completed
}
