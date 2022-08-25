#pragma once

#include "missions_base.h"

class MissionSystem
{
public:
	bool IsConditionCompleted(uint32_t condition_id, uint32_t progress_value);

	uint32_t GetReward(entt::entity player, uint32_t mission_id);
	uint32_t Accept(entt::entity player, const AcceptMissionP& param);
	uint32_t AcceptCheck(entt::entity player, const AcceptMissionP& param);
	uint32_t Abandon(entt::entity player, uint32_t mission_id);
	void CompleteAllMission(entt::entity player);

	void receive(entt::entity player, const ConditionEvent& c);

private:
	void DelMissionClassify(entt::entity player, uint32_t mission_id);

	bool UpdateMissionByCompareCondition(entt::entity player, const ConditionEvent& c, Mission& mission);

	void OnMissionComplete(entt::entity player, const ConditionEvent& c, const TempCompleteList& temp_complete);
};