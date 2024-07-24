#pragma once
#pragma once

#include <array>

#include "proto/logic/component/mission_comp.pb.h"
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
class MissionsComp;
class condition_row;

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
	static bool UpdateMissionProgress(const MissionConditionEvent& condition_event, MissionPbComp& mission, int index, const condition_row* const condition_row);
	static void UpdateMissionStatus(MissionPbComp& mission, const google::protobuf::RepeatedField<uint32_t>& mission_conditions);
	static void OnMissionComplete(entt::entity player, const UInt32Set& completed_missions);
	static uint32_t CheckAcceptConditions(const AcceptMissionEvent& accept_event, MissionsComp* mission_comp);
	static void RemoveMissionFromClassify(MissionsComp* mission_comp, uint32_t mission_id);
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