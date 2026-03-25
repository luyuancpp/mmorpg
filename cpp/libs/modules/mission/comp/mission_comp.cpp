#include "mission_comp.h"

#include <ranges>

#include "table/code/condition_table.h"
#include "mission/constants/mission.h"

#include "core/macros/return_define.h"
#include "core/macros/error_return.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/mission_error_tip.pb.h"

#include "proto/common/component/mission_comp.pb.h"
#include "proto/common/event/mission_event.pb.h"

MissionsComp::MissionsComp()
{
	for (uint32_t i = static_cast<uint32_t>(eConditionType::kConditionKillMonster);
	     i < static_cast<uint32_t>(eConditionType::kConditionTypeMax); ++i)
	{
		eventMissionsClassify_.emplace(i, UInt32Set{});
	}
}

std::size_t MissionsComp::CanGetRewardSize() const
{
	return claimableRewards_.count();
}

void MissionsComp::AbandonMission(uint32_t missionId)
{
    SetBit(MissionBitMap, completedMissions_, missionId, false);
}

uint32_t MissionsComp::ValidateNotAccepted(uint32_t missionId) const
{
	if (missionList_.missions().find(missionId) != missionList_.missions().end())
	{
		return MAKE_ERROR_MSG(kMissionIdRepeated, "missionId=" << missionId);
	}
	return kSuccess;
}

uint32_t MissionsComp::ValidateNotCompleted(uint32_t missionId) const
{
	if (IsComplete(missionId))
	{
		return MAKE_ERROR_MSG(kMissionAlreadyCompleted, "missionId=" << missionId);
	}
	return kSuccess;
}
