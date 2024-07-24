﻿#pragma once
#pragma once

#include <array>

#include "proto/logic/component/mission_comp.pb.h"
#include "type_define/type_define.h"
#include "system/mission/missions_config_template.h"
#include "util/game_registry.h"


struct GetRewardParam
{
	entt::entity playerId{entt::null};
	uint32_t missionId{ 0 };
	uint32_t op{ 0 };
};

struct AbandonParam
{
	entt::entity playerId{ entt::null };
	uint32_t missionId{ 0 };
	uint32_t op{ 0 };
};

struct CompleteMissionParam
{
	entt::entity playerId{ entt::null };
	uint32_t missionId{ 0 };
	uint32_t op{ 0 };
};

class AcceptMissionEvent;
class MissionConditionEvent;
class MissionsComp;
class condition_row;

class MissionSystem
{
public:
	using event_mission_classify_type = std::unordered_map<uint32_t, UInt32Set>;

	static uint32_t GetMissionReward(const GetRewardParam& param);
	static uint32_t AcceptMission(const AcceptMissionEvent& accept_event);
	static uint32_t AbandonMission(const AbandonParam& param);
	static void CompleteAllMissions(entt::entity player, uint32_t op);

	static void HandleMissionConditionEvent(const MissionConditionEvent& condition_event);

private:
	static void DeleteMissionClassification(entt::entity player, uint32_t mission_id);
	static bool UpdateMission(const MissionConditionEvent& condition_event, MissionPbComp& mission);
	static bool UpdateMissionProgress(const MissionConditionEvent& conditionEvent, MissionPbComp& mission);
	static void UpdateMissionStatus(MissionPbComp& mission, const google::protobuf::RepeatedField<uint32_t>& mission_conditions);
	static void OnMissionCompletion(entt::entity player, const UInt32Set& completed_missions);
	static uint32_t CheckMissionAcceptance(const AcceptMissionEvent& accept_event, MissionsComp* mission_comp);
	static void RemoveMissionClassification(MissionsComp* mission_comp, uint32_t mission_id);
	static bool AreAllConditionsFulfilled(const MissionPbComp& mission, uint32_t missionId, MissionsComp* missionComp);
	static bool UpdateProgressIfConditionMatches(const MissionConditionEvent& conditionEvent, MissionPbComp& mission, int index, const condition_row* conditionRow);
};





/*
```

### 主要功能点和结构分析：

1. * *任务系统类 MissionSystem * *：
- 包含了接受任务（Accept）、放弃任务（Abandon）、完成任务（CompleteAllMission）等操作的方法。
- 使用了多种数据结构和工具，包括Google Protobuf、Entt（一个ECS库）、条件配置（condition_config.h）、日志记录等。

2. * *任务状态和进度管理 * *：
- 任务状态通过枚举和Protobuf消息进行管理（MissionPbComp），任务的进度通过更新、检查条件来动态变化。
- 每个任务有特定的条件（通过condition_config.h配置），任务完成时需要满足所有条件。

3. * *条件检查和更新 * *：
- `IsConditionCompleted`函数根据条件配置和实际进度判断条件是否完成。
- `UpdateMission`和相关函数用于更新任务进度和状态，并在满足条件时触发任务完成事件。

4. * *任务事件处理 * *：
- `Receive`函数处理任务相关事件，根据事件类型和条件更新任务进度和状态，并触发相应的任务完成事件。

5. * *任务分类和过滤 * *：
- 使用了分类集合（`EventMissionsClassify`和`TypeFilter`）来区分不同类型的任务，并进行相关的过滤和管理。

6. * *奖励和事件触发 * *：
- 完成任务时，根据配置自动或手动领取奖励，并触发相关的奖励事件（`OnMissionAwardEvent`）和后续任务接受事件。

这些功能共同构成了一个完整的任务系统，管理着游戏中玩家的任务接受、进度更新和奖励领取等流程。*/


//自动领奖,给经验，为什么发事件？因为给经验升级了会马上接任务，或者触发一些任务的东西,
//但是我需要不影响当前任务逻辑流程,也可以马上触发，看情况而定
//todo 如果是活动不用走,让活动去接,这里应该是属于主任务系统的逻辑，想想怎么改方便，活动和任务逻辑分开，互不影响