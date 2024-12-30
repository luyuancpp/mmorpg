#include "mission_comp.h"

#include <ranges>

#include "condition_config.h"
#include "mission/constants/mission_constants.h"
#include "thread_local/storage.h"
#include "macros/return_define.h"
#include "pbc/common_error_tip.pb.h"
#include "pbc/mission_error_tip.pb.h"

#include "proto/logic/component/mission_comp.pb.h"
#include "proto/logic/event/mission_event.pb.h"

MissionsComponent::MissionsComponent()
	: missionConfig(&MissionConfig::GetSingleton()),
	missionTypeNotRepeated(missionConfig->CheckTypeRepeated())
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
	const auto* const missionReward = tls.registry.try_get<RewardListPBComponent>(EventOwner());
	if (nullptr == missionReward)
	{
		return 0; // Return 0 if reward component is not found
	}
	return static_cast<std::size_t>(missionReward->can_reward_mission_id_size()); // Return the number of missions eligible for reward
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
	// Check if the mission is completed
	if (completedMissions.test(missionId))
	{
		return kMissionAlreadyCompleted; // Return kMissionAlreadyCompleted if mission is already completed
	}
	return kSuccess; // Return kSuccess if mission is not completed
}
