#pragma once
#pragma once

#include <array>

#include "component_proto/mission_comp.pb.h"
#include "type_define/type_define.h"
#include "system/mission/missions_config_template.h"
#include "util/game_registry.h"


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

class MissionSystem
{
public:
	using event_mission_classify_type = std::unordered_map<uint32_t, UInt32Set>;

	static uint32_t GetReward(const GetRewardParam& param);
	static uint32_t Accept(const AcceptMissionEvent& accept_event);
	static uint32_t Abandon(const AbandonParam& param);
	static void CompleteAllMission(entt::entity player, uint32_t op);

	static void Receive(const MissionConditionEvent& condition_event);

private:
	static void DeleteMissionClassify(entt::entity player, uint32_t mission_id);
	static bool UpdateMission(const MissionConditionEvent& condition_event, MissionPbComp& mission);
	static void OnMissionComplete(entt::entity player, const UInt32Set& completed_missions);

};





