#pragma once
#pragma once

#include <array>

#include "component_proto/mission_comp.pb.h"
#include "src/common_type/common_type.h"
#include "src/game_logic/missions/missions_config_template.h"
#include "src/util/game_registry.h"


struct GetRewardParam
{
	entt::entity player_{entt::null};
	uint32_t mission_id_{ 0 };
	uint32_t op_{ 0 };
};

struct AbandonParam
{
	entt::entity player_{ entt::null };
	uint32_t mission_id_{ 0 };
	uint32_t op_{ 0 };
};

struct CompleteMissionParam
{
	entt::entity player_{ entt::null };
	uint32_t mission_id_{ 0 };
	uint32_t op_{ 0 };
};

class AcceptMissionEvent;
class MissionConditionEvent;

class MissionSystem : public EventOwner
{
public:
	using event_mission_classify_type = std::unordered_map<uint32_t, UInt32Set>;


	static bool IsConditionCompleted(uint32_t condition_id, uint32_t progress_value);
	uint32_t IsUnAccepted(uint32_t mission_id) const;
	uint32_t IsUnCompleted(uint32_t mission_id) const;

	uint32_t GetReward(const GetRewardParam& param) const;
	uint32_t Accept(const AcceptMissionEvent& accept_event);
	uint32_t Abandon(const AbandonParam& param);
	void CompleteAllMission(entt::entity player, uint32_t op);

	void Receive(const MissionConditionEvent& condition_event);

private:
	void DeleteMissionClassify(entt::entity player, uint32_t mission_id);
	bool UpdateMission(const MissionConditionEvent& condition_event, MissionPbComp& mission) const;
	void OnMissionComplete(entt::entity player, const UInt32Set& completed_missions);

};





