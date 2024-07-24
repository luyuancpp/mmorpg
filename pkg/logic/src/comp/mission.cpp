#include "mission.h"

#include <ranges>

#include "condition_config.h"
#include "constants/mission.h"
#include "thread_local/storage.h"
#include "constants/tips_id.h"

#include "proto/logic/component/mission_comp.pb.h"
#include "proto/logic/event/mission_event.pb.h"

MissionsComp::MissionsComp()
	: mission_config_(&MissionConfig::GetSingleton()),
	mission_type_not_repeated_(mission_config_->CheckTypeRepeated())
{
	// Initialize event-related mission classification containers
	for (uint32_t i = static_cast<uint32_t>(eCondtionType::kConditionKillMonster); i < static_cast<uint32_t>(eCondtionType::kConditionTypeMax); ++i)
	{
		event_missions_classify_.emplace(i, UInt32Set{});
	}
}

std::size_t MissionsComp::CanGetRewardSize() const
{
	// Retrieve mission reward component
	const auto* const mission_reward = tls.registry.try_get<MissionRewardPbComp>(event_owner());
	if (nullptr == mission_reward)
	{
		return 0; // Return 0 if reward component is not found
	}
	return static_cast<std::size_t>(mission_reward->can_reward_mission_id_size()); // Return the number of missions eligible for reward
}

uint32_t MissionsComp::IsMissionUnaccepted(const uint32_t mission_id) const
{
	// Check if the mission has been accepted
	if (missions_comp_.missions().find(mission_id) != missions_comp_.missions().end())
	{
		return kMissionIdRepeated; // Return kMissionIdRepeated if mission is already accepted
	}
	return kOK; // Return kOK if mission is not accepted
}

uint32_t MissionsComp::IsMissionUncompleted(const uint32_t mission_id) const
{
	// Check if the mission is completed
	if (missions_comp_.complete_missions().count(mission_id) > 0)
	{
		return kMissionAlreadyCompleted; // Return kMissionAlreadyCompleted if mission is already completed
	}
	return kOK; // Return kOK if mission is not completed
}
