#pragma once

#include <unordered_set>

#include "core/type_define/type_define.h"
#include "modules/mission/comp/missions_config_comp.h"
#include "core/utils/registry/game_registry.h"

class ConditionTable;

struct GetRewardParam {
	entt::entity playerEntity{ entt::null };
	uint32_t missionId{ 0 };
	uint32_t op{ 0 };
};

struct AbandonParam {
	entt::entity playerEntity{ entt::null };
	uint32_t missionId{ 0 };
	uint32_t op{ 0 };
};

struct CompleteMissionParam {
	entt::entity playerEntity{ entt::null };
	uint32_t missionId{ 0 };
	uint32_t op{ 0 };
};

class AcceptMissionEvent;
class MissionConditionEvent;
class MissionComp;
class MissionsComp;

/// Stateless system that drives the full mission lifecycle:
/// accept, abandon, progress, complete, and reward.
class MissionSystem {
public:
	using event_mission_classify_type = std::unordered_map<uint32_t, UInt32Set>;

	static uint32_t GetMissionReward(const GetRewardParam& param, MissionsComp& comp);
	static uint32_t AcceptMission(const AcceptMissionEvent& acceptEvent, MissionsComp& comp, const IMissionConfig& config);
	static uint32_t AbandonMission(const AbandonParam& param, MissionsComp& comp, const IMissionConfig& config);
	static void CompleteAllMissions(entt::entity player, uint32_t op, MissionsComp& comp);
	static void HandleMissionConditionEvent(const MissionConditionEvent& conditionEvent, MissionsComp& comp, const IMissionConfig& config);

private:
	static uint32_t CheckMissionAcceptance(const AcceptMissionEvent& acceptEvent, MissionsComp& missionComp, const IMissionConfig& config);
	static bool UpdateMissionProgress(const MissionConditionEvent& conditionEvent, MissionComp& mission, const IMissionConfig& config);
	static bool UpdateProgressIfConditionMatches(const MissionConditionEvent& conditionEvent, MissionComp& mission, int index, const ConditionTable* conditionRow, uint32_t targetCount);
	static void UpdateMissionStatus(MissionComp& mission, const google::protobuf::RepeatedField<uint32_t>& missionConditions, const google::protobuf::RepeatedField<uint32_t>& targetCounts);
	static bool AreAllConditionsFulfilled(const MissionComp& mission, uint32_t missionId, MissionsComp& missionComp, const IMissionConfig& config);
	static void OnMissionCompletion(entt::entity player, const std::unordered_set<uint32_t>& completedMissions, MissionsComp& comp, const IMissionConfig& config);
	static void DeleteMissionClassification(MissionsComp& comp, uint32_t missionId, const IMissionConfig& config);
	static void RemoveMissionClassification(MissionsComp& missionComp, uint32_t missionId, const IMissionConfig& config);
};