#include "mission_comp.h"

#include <ranges>

#include "src/game_config/condition_config.h"
#include "src/game_logic/constants/mission_constants.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/tips_id.h"

#include "component_proto/mission_comp.pb.h"
#include "event_proto/mission_event.pb.h"

MissionsComp::MissionsComp()
	: mission_config_(&MissionConfig::GetSingleton()),
	  mission_type_not_repeated_(mission_config_->CheckTypeRepeated())
{
	for (uint32_t i = kConditionKillMonster; i < kConditionTypeMax; ++i)
	{
		event_missions_classify_.emplace(i, UInt32Set{});
	}
}

std::size_t MissionsComp::CanGetRewardSize() const
{
	const auto* const try_mission_reward = tls.registry.try_get<MissionRewardPbComp>(event_owner());
	if (nullptr == try_mission_reward)
	{
		return 0;
	}
	return static_cast<std::size_t>(try_mission_reward->can_reward_mission_id_size());
}

uint32_t MissionsComp::IsUnAccepted(const uint32_t mission_id) const
{
	if (missions_comp_.missions().find(mission_id) != missions_comp_.missions().end())
	{
		return kRetMissionIdRepeated;
	}
	return kRetOK;
}

uint32_t MissionsComp::IsUnCompleted(const uint32_t mission_id) const
{
	if (missions_comp_.complete_missions().count(mission_id) > 0) //已经完成
	{
		return kRetMissionComplete;
	}
	return kRetOK;
}
