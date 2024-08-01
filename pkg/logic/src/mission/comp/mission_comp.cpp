#include "mission_comp.h"

#include <ranges>

#include "condition_config.h"
#include "constants/mission_constants.h"
#include "thread_local/storage.h"
#include "constants/tips_id_constants.h"

#include "proto/logic/component/mission_comp.pb.h"
#include "proto/logic/event/mission_event.pb.h"

MissionsComp::MissionsComp()
	: missionConfig(&MissionConfig::GetSingleton()),
	missionTypeNotRepeated(missionConfig->CheckTypeRepeated())
{
	// Initialize event-related mission classification containers
	for (uint32_t i = static_cast<uint32_t>(eCondtionType::kConditionKillMonster); i < static_cast<uint32_t>(eCondtionType::kConditionTypeMax); ++i)
	{
		eventMissionsClassify.emplace(i, UInt32Set{});
	}
}

std::size_t MissionsComp::CanGetRewardSize() const
{
	// Retrieve mission reward component
	const auto* const missionReward = tls.registry.try_get<MissionRewardPbComp>(EventOwner());
	if (nullptr == missionReward)
	{
		return 0; // Return 0 if reward component is not found
	}
	return static_cast<std::size_t>(missionReward->can_reward_mission_id_size()); // Return the number of missions eligible for reward
}

uint32_t MissionsComp::IsMissionUnaccepted(const uint32_t missionId) const
{
	// Check if the mission has been accepted
	if (missionsComp.missions().find(missionId) != missionsComp.missions().end())
	{
		return kMissionIdRepeated; // Return kMissionIdRepeated if mission is already accepted
	}
	return kOK; // Return kOK if mission is not accepted
}

uint32_t MissionsComp::IsMissionUncompleted(const uint32_t missionId) const
{
	// Check if the mission is completed
	if (missionsComp.complete_missions().count(missionId) > 0)
	{
		return kMissionAlreadyCompleted; // Return kMissionAlreadyCompleted if mission is already completed
	}
	return kOK; // Return kOK if mission is not completed
}
