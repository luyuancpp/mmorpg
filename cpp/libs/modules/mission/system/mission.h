#pragma once

#include <array>
#include <unordered_map>
#include <google/protobuf/repeated_field.h>

#include "proto/common/component/mission_comp.pb.h"
#include "core/type_define/type_define.h"
#include "modules/mission/comp/missions_config_comp.h"
#include "core/utils/registry/game_registry.h"
#include "table/code/condition_table.h"

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

struct UpdateProgressParam {
	entt::entity playerEntity;
	uint32_t missionId{ 0 };
	uint32_t conditionId{ 0 };
	uint32_t progressValue{ 0 };
};

class AcceptMissionEvent;
class MissionConditionEvent;
class MissionsComponent;

class MissionSystem {
public:
	using event_mission_classify_type = std::unordered_map<uint32_t, UInt32Set>;

	static uint32_t GetMissionReward(const GetRewardParam& param);
	static uint32_t AcceptMission(const AcceptMissionEvent& acceptEvent, MissionsComponent& comp, const IMissionConfig& config);
	static uint32_t AbandonMission(const AbandonParam& param, MissionsComponent& comp, const IMissionConfig& config);
	static void CompleteAllMissions(entt::entity player, uint32_t op, MissionsComponent& comp);
	static void HandleMissionConditionEvent(const MissionConditionEvent& conditionEvent, MissionsComponent& comp, const IMissionConfig& config);

private:
	static void DeleteMissionClassification(entt::entity player, uint32_t missionId, const IMissionConfig& config);
	static bool UpdateMission(const MissionConditionEvent& conditionEvent, MissionPBComponent& mission);
	static bool UpdateMissionProgress(const MissionConditionEvent& conditionEvent, MissionPBComponent& mission, const IMissionConfig& config);
	static void UpdateMissionStatus(MissionPBComponent& mission, const google::protobuf::RepeatedField<uint32_t>& missionConditions);
	static void OnMissionCompletion(entt::entity player, const UInt32Set& completedMissions, const IMissionConfig& config);
	static uint32_t CheckMissionAcceptance(const AcceptMissionEvent& acceptEvent, MissionsComponent& missionComp, const IMissionConfig& config);
	static void RemoveMissionClassification(MissionsComponent& missionComp, uint32_t missionId, const IMissionConfig& config);
	static bool AreAllConditionsFulfilled(const MissionPBComponent& mission, uint32_t missionId, MissionsComponent& missionComp, const IMissionConfig& config);
	static bool UpdateProgressIfConditionMatches(const MissionConditionEvent& conditionEvent, MissionPBComponent& mission, int index, const ConditionTable* conditionRow);
};

//todo auto-reward grants XP via event (so level-up can immediately trigger new missions
// without disrupting the current mission logic flow).
// Activity rewards should be handled separately from the main mission system.