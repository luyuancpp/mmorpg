#include <gtest/gtest.h>

#include "condition_config.h"
#include "mission_config.h"
#include "constants/mission.h"
#include "comp/mission.h"
#include "system/mission/mission_system.h"
#include "constants/tips_id.h"
#include "handler/event/mission_event_handler.h"
#include "util/random.h"
#include "thread_local/storage.h"

#include "proto/logic/component/mission_comp.pb.h"
#include "proto/logic/event/mission_event.pb.h"

decltype(auto) CreatePlayerWithMissionComponent()
{
    const auto playerEntity = tls.registry.create();
    tls.registry.emplace<Guid>(playerEntity);
    auto& missionsComponent = tls.registry.emplace<MissionsComp>(playerEntity);
    missionsComponent.set_event_owner(playerEntity);
    MissionEventHandler::Register();
    return playerEntity;
}

TEST(MissionsComp, AcceptMission)
{
	constexpr uint32_t testMissionId = 1;
	const auto playerEntity = CreatePlayerWithMissionComponent();
	auto& missionsComponent = tls.registry.get<MissionsComp>(playerEntity);
	missionsComponent.SetMissionTypeNotRepeated(false);

	// Simulating accepting missions from a list
	AcceptMissionEvent acceptMissionEvent;
	acceptMissionEvent.set_mission_id(testMissionId);
	acceptMissionEvent.set_entity(entt::to_integral(playerEntity));
	auto& missionConfigData = mission_config::GetSingleton().all();
	std::size_t acceptedMissionCount = 0;
	for (int32_t i = 0; i < missionConfigData.data_size(); ++i)
	{
		acceptMissionEvent.set_mission_id(missionConfigData.data(i).id());
		auto acceptResult = MissionSystem::AcceptMission(acceptMissionEvent);
		++acceptedMissionCount;
	}

	EXPECT_EQ(acceptedMissionCount, missionsComponent.MissionSize());
	EXPECT_EQ(0, missionsComponent.CompleteSize());

	// Complete all accepted missions
	MissionSystem::CompleteAllMissions(playerEntity, 0);
	EXPECT_EQ(0, missionsComponent.MissionSize());
	EXPECT_EQ(acceptedMissionCount, missionsComponent.CompleteSize());
}

TEST(MissionsComp, RepeatedMissionId)
{
	const auto playerEntity = CreatePlayerWithMissionComponent();
	auto& missionsComponent = tls.registry.get<MissionsComp>(playerEntity);

	// Test case : Repeating mission_id = 1
	{
		constexpr uint32_t missionId1 = 1;
		AcceptMissionEvent acceptMissionEvent;
		acceptMissionEvent.set_entity(entt::to_integral(playerEntity));
		acceptMissionEvent.set_mission_id(missionId1);

		// First accept should succeed
		EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));

		// Second accept should fail due to mission_id being repeated
		EXPECT_EQ(kMissionIdRepeated, MissionSystem::AcceptMission(acceptMissionEvent));
	}
}

TEST(MissionsComp, RepeatedMissionType)
{
	const auto playerEntity = CreatePlayerWithMissionComponent();
	auto& missionsComponent = tls.registry.get<MissionsComp>(playerEntity);

	// Test case : Repeating different mission types
	{
		AcceptMissionEvent acceptMissionEvent1;
		acceptMissionEvent1.set_entity(entt::to_integral(playerEntity));
		acceptMissionEvent1.set_mission_id(3);

		AcceptMissionEvent acceptMissionEvent2;
		acceptMissionEvent2.set_entity(entt::to_integral(playerEntity));
		acceptMissionEvent2.set_mission_id(2);

		// First accept for mission_id = 3 should succeed
		EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent1));

		// First accept for mission_id = 2 should succeed
		EXPECT_EQ(kMissionTypeRepeated, MissionSystem::AcceptMission(acceptMissionEvent2));

		// Second accept for mission_id = 3 (same type as mission_id = 1) should fail
		EXPECT_EQ(kMissionIdRepeated, MissionSystem::AcceptMission(acceptMissionEvent1));
	}
}

TEST(MissionsComp, TriggerMissionCondition)
{
	// Create a player entity with a mission component
	const auto playerEntity = CreatePlayerWithMissionComponent();
	auto& missionsComponent = tls.registry.get<MissionsComp>(playerEntity);

	constexpr uint32_t mission_id = 1;
	AcceptMissionEvent acceptMissionEvent;
	acceptMissionEvent.set_mission_id(mission_id);
	acceptMissionEvent.set_entity(entt::to_integral(playerEntity));

	// Accept mission with mission_id = 1
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));

	// Ensure there is 1 type of mission accepted
	EXPECT_EQ(1, missionsComponent.TypeSetSize());

	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(missionsComponent);
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1); // Condition id 1
	conditionEvent.set_amount(1);

	// Handle condition event for mission
	MissionSystem::HandleMissionConditionEvent(conditionEvent);

	// After handling condition 1, expect 1 mission in progress and 0 completed missions
	EXPECT_EQ(1, missionsComponent.MissionSize());
	EXPECT_EQ(0, missionsComponent.CompleteSize());

	conditionEvent.clear_condtion_ids();
	conditionEvent.add_condtion_ids(2); // Condition id 2
	MissionSystem::HandleMissionConditionEvent(conditionEvent);

	// After handling condition 2, expect 1 mission still in progress and 0 completed missions
	EXPECT_EQ(1, missionsComponent.MissionSize());
	EXPECT_EQ(0, missionsComponent.CompleteSize());

	conditionEvent.clear_condtion_ids();
	conditionEvent.add_condtion_ids(3); // Condition id 3
	MissionSystem::HandleMissionConditionEvent(conditionEvent);

	// After handling condition 3, expect 1 mission still in progress and 0 completed missions
	EXPECT_EQ(1, missionsComponent.MissionSize());
	EXPECT_EQ(0, missionsComponent.CompleteSize());

	conditionEvent.clear_condtion_ids();
	conditionEvent.add_condtion_ids(4); // Condition id 4
	MissionSystem::HandleMissionConditionEvent(conditionEvent);

	// After handling condition 4, expect 0 missions in progress and 1 completed mission
	EXPECT_EQ(0, missionsComponent.MissionSize());
	EXPECT_EQ(1, missionsComponent.CompleteSize());

	// Ensure there are no more mission types being tracked
	EXPECT_EQ(0, missionsComponent.TypeSetSize());
}

TEST(MissionsComp, ConditionTypeSize)
{
	// Create a player entity with a mission component
	auto playerEntity = CreatePlayerWithMissionComponent();
	auto& missionsComponent = tls.registry.get<MissionsComp>(playerEntity);

	// Trigger update to handle any pending mission events
	tls.dispatcher.update<AcceptMissionEvent>();

	// Accept mission with mission_id = 6
	uint32_t mission_id = 6;
	AcceptMissionEvent acceptMissionEvent;
	acceptMissionEvent.set_mission_id(mission_id);
	acceptMissionEvent.set_entity(entt::to_integral(playerEntity));
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));

	// Ensure mission_id 6 is accepted but not completed
	EXPECT_TRUE(missionsComponent.IsAccepted(mission_id));
	EXPECT_FALSE(missionsComponent.IsComplete(mission_id));

	// Validate that each condition type has one mission tracked for testing purposes
	for (uint32_t i = static_cast<uint32_t>(eCondtionType::kConditionKillMonster); i < static_cast<uint32_t>(eCondtionType::kConditionCustom); ++i)
	{
		EXPECT_EQ(1, missionsComponent.classify_for_unittest().find(i)->second.size());
	}

	// Handle various mission condition events
	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(missionsComponent);

	// Handle condition: kConditionKillMonster, condition_ids = {1}, amount = 1
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	conditionEvent.set_amount(1);
	MissionSystem::HandleMissionConditionEvent(conditionEvent);

	// After handling kConditionKillMonster, expect 1 mission in progress and 0 completed missions
	EXPECT_EQ(1, missionsComponent.MissionSize());
	EXPECT_EQ(0, missionsComponent.CompleteSize());

	// Handle condition: kConditionTalkWithNpc, condition_ids = {1}, amount = 1
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionTalkWithNpc));
	MissionSystem::HandleMissionConditionEvent(conditionEvent);

	// After handling kConditionTalkWithNpc, expect 1 mission still in progress and 0 completed missions
	EXPECT_EQ(1, missionsComponent.MissionSize());
	EXPECT_EQ(0, missionsComponent.CompleteSize());

	// Handle condition: kConditionCompleteCondition, condition_ids = {1}, amount = 1
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionCompleteCondition));
	MissionSystem::HandleMissionConditionEvent(conditionEvent);

	// After handling kConditionCompleteCondition, expect 1 mission still in progress and 0 completed missions
	EXPECT_EQ(1, missionsComponent.MissionSize());
	EXPECT_EQ(0, missionsComponent.CompleteSize());

	// Handle condition: kConditionUseItem, condition_ids = {2}, amount = 1
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionUseItem));
	conditionEvent.clear_condtion_ids();
	conditionEvent.add_condtion_ids(1);
	conditionEvent.add_condtion_ids(2);
	MissionSystem::HandleMissionConditionEvent(conditionEvent);

	// After handling kConditionUseItem, expect 1 mission still in progress and 0 completed missions
	EXPECT_EQ(1, missionsComponent.MissionSize());
	EXPECT_EQ(0, missionsComponent.CompleteSize());

	// Handle condition: kConditionLevelUp, condition_ids = {10}, amount = 1
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionLevelUp));
	conditionEvent.clear_condtion_ids();
	conditionEvent.add_condtion_ids(10);
	MissionSystem::HandleMissionConditionEvent(conditionEvent);

	// After handling kConditionLevelUp, expect 1 mission still in progress and 0 completed missions
	EXPECT_EQ(1, missionsComponent.MissionSize());
	EXPECT_EQ(0, missionsComponent.CompleteSize());

	// Handle condition: kConditionInteraction, condition_ids = {1, 2}
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionInteraction));
	conditionEvent.clear_condtion_ids();
	conditionEvent.add_condtion_ids(1);
	conditionEvent.add_condtion_ids(2);
	MissionSystem::HandleMissionConditionEvent(conditionEvent);

	// Trigger update to handle any pending mission condition events
	tls.dispatcher.update<MissionConditionEvent>();

	// After handling kConditionInteraction, expect 0 missions in progress and 1 completed mission
	EXPECT_EQ(0, missionsComponent.MissionSize());
	EXPECT_EQ(1, missionsComponent.CompleteSize());

	// Ensure mission_id 6 is no longer accepted and is marked as complete
	EXPECT_FALSE(missionsComponent.IsAccepted(mission_id));
	EXPECT_TRUE(missionsComponent.IsComplete(mission_id));

	// Ensure there are no mission types being tracked after completion
	EXPECT_EQ(0, missionsComponent.TypeSetSize());

	// Validate that no mission types are tracked after completion
	for (uint32_t i = static_cast<uint32_t>(eCondtionType::kConditionKillMonster); i < static_cast<uint32_t>(eCondtionType::kConditionCustom); ++i)
	{
		EXPECT_EQ(0, missionsComponent.classify_for_unittest().find(i)->second.size());
	}
}


TEST(MissionsComp, CompleteAcceptMission)
{
	// 创建带有任务组件的玩家实体
	const auto playerEntity = CreatePlayerWithMissionComponent();
	auto& missionsComponent = tls.registry.get<MissionsComp>(playerEntity);

	// 设置任务ID为4的接受任务事件
	constexpr uint32_t mission_id = 4;
	AcceptMissionEvent acceptMissionEvent;
	acceptMissionEvent.set_entity(entt::to_integral(playerEntity));
	acceptMissionEvent.set_mission_id(mission_id);

	// 验证接受任务是否成功
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));

	// 验证任务类型集合的大小
	EXPECT_EQ(1, missionsComponent.TypeSetSize());

	// 准备任务条件事件
	MissionConditionEvent missionConditionEvent;
	missionConditionEvent.set_entity(entt::to_integral(playerEntity));
	missionConditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	missionConditionEvent.add_condtion_ids(1);
	missionConditionEvent.set_amount(1);

	// 处理任务条件事件，标记任务为完成状态
	MissionSystem::HandleMissionConditionEvent(missionConditionEvent);

	// 验证任务不再处于接受状态，已经完成
	EXPECT_FALSE(missionsComponent.IsAccepted(mission_id));
	EXPECT_TRUE(missionsComponent.IsComplete(mission_id));

	// 再次尝试接受已经完成的任务，预期返回kMissionAlreadyCompleted
	EXPECT_EQ(kMissionAlreadyCompleted, MissionSystem::AcceptMission(acceptMissionEvent));
}

TEST(MissionsComp, EventTriggerMutableMission)
{
	// 创建带有任务组件的玩家实体
	const auto playerEntity = CreatePlayerWithMissionComponent();
	auto& missionsComponent = tls.registry.get<MissionsComp>(playerEntity);

	// 设置任务ID为1的接受任务事件
	constexpr uint32_t mission_id1 = 1;
	AcceptMissionEvent acceptMissionEvent1;
	acceptMissionEvent1.set_entity(entt::to_integral(playerEntity));
	acceptMissionEvent1.set_mission_id(mission_id1);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent1));

	// 设置任务ID为2的接受任务事件
	constexpr uint32_t mission_id2 = 2;
	AcceptMissionEvent acceptMissionEvent2;
	acceptMissionEvent2.set_entity(entt::to_integral(playerEntity));
	acceptMissionEvent2.set_mission_id(mission_id2);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent2));

	// 准备任务条件事件
	MissionConditionEvent missionConditionEvent;
	missionConditionEvent.set_entity(entt::to_integral(playerEntity));
	missionConditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	missionConditionEvent.set_amount(4);

	// 处理任务条件事件，逐步完成任务
	for (int i = 1; i <= 4; ++i)
	{
		missionConditionEvent.clear_condtion_ids();
		missionConditionEvent.add_condtion_ids(i);
		MissionSystem::HandleMissionConditionEvent(missionConditionEvent);
	}

	// 验证任务ID为1和2的任务是否完成
	EXPECT_TRUE(missionsComponent.IsComplete(mission_id1));
	EXPECT_TRUE(missionsComponent.IsComplete(mission_id2));
}

TEST(MissionsComp, OnCompleteMission)
{
	// 创建带有任务组件的玩家实体
	const auto playerEntity = CreatePlayerWithMissionComponent();
	auto& missionsComponent = tls.registry.get<MissionsComp>(playerEntity);

	// 接受任务ID为7的任务
	uint32_t mission_id = 7;
	AcceptMissionEvent acceptMissionEvent;
	acceptMissionEvent.set_entity(entt::to_integral(playerEntity));
	acceptMissionEvent.set_mission_id(mission_id);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));
	EXPECT_EQ(1, missionsComponent.TypeSetSize());

	// 设置任务条件事件
	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(playerEntity));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	conditionEvent.set_amount(1);
	MissionSystem::HandleMissionConditionEvent(conditionEvent);

	// 更新任务状态，验证任务是否完成
	tls.dispatcher.update<AcceptMissionEvent>();
	EXPECT_FALSE(missionsComponent.IsAccepted(mission_id));
	EXPECT_TRUE(missionsComponent.IsComplete(mission_id));

	// 测试接受下一个任务，并验证其状态
	auto next_mission = ++mission_id;
	EXPECT_TRUE(missionsComponent.IsAccepted(mission_id));
	EXPECT_FALSE(missionsComponent.IsComplete(mission_id));

	// 循环处理任务条件，完成后验证任务状态变化
	for (uint32_t i = static_cast<uint32_t>(eCondtionType::kConditionKillMonster); i < static_cast<uint32_t>(eCondtionType::kConditionInteraction); ++i)
	{
		conditionEvent.clear_condtion_ids();
		conditionEvent.add_condtion_ids(i);
		MissionSystem::HandleMissionConditionEvent(conditionEvent);

		EXPECT_FALSE(missionsComponent.IsAccepted(mission_id));
		EXPECT_TRUE(missionsComponent.IsComplete(mission_id));

		tls.dispatcher.update<AcceptMissionEvent>();
		EXPECT_EQ(0, tls.dispatcher.size<AcceptMissionEvent>());

		EXPECT_TRUE(missionsComponent.IsAccepted(++mission_id));
		EXPECT_FALSE(missionsComponent.IsComplete(mission_id));
	}
}

TEST(MissionsComp, AcceptNextMirroMission)
{
	// 创建带有任务组件的玩家实体
	const auto playerEntity = CreatePlayerWithMissionComponent();
	auto& missionsComponent = tls.registry.get<MissionsComp>(playerEntity);

	// 接受任务ID为7的任务
	uint32_t mission_id = 7;
	AcceptMissionEvent acceptMissionEvent;
	acceptMissionEvent.set_entity(entt::to_integral(playerEntity));
	acceptMissionEvent.set_mission_id(mission_id);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));
	EXPECT_EQ(1, missionsComponent.TypeSetSize());

	// 设置任务条件事件
	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(playerEntity));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	conditionEvent.set_amount(1);
	MissionSystem::HandleMissionConditionEvent(conditionEvent);

	// 更新任务状态，验证任务完成
	tls.dispatcher.update<AcceptMissionEvent>();
	EXPECT_FALSE(missionsComponent.IsAccepted(mission_id));
	EXPECT_TRUE(missionsComponent.IsComplete(mission_id));

	// 接受下一个任务，并验证其状态
	const auto next_mission_id = ++mission_id;
	tls.dispatcher.update<AcceptMissionEvent>();
	EXPECT_TRUE(missionsComponent.IsAccepted(next_mission_id));
	EXPECT_FALSE(missionsComponent.IsComplete(next_mission_id));
}

TEST(MissionsComp, MissionCondition)
{
	// 创建带有任务组件的玩家实体
	const auto playerEntity = CreatePlayerWithMissionComponent();
	auto& missionsComponent = tls.registry.get<MissionsComp>(playerEntity);

	// 接受三个不同任务
	uint32_t mission_id = 14;
	uint32_t mission_id1 = 15;
	uint32_t mission_id2 = 16;

	AcceptMissionEvent acceptMissionEvent;
	acceptMissionEvent.set_entity(entt::to_integral(playerEntity));

	acceptMissionEvent.set_mission_id(mission_id);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));

	acceptMissionEvent.set_mission_id(mission_id1);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));

	acceptMissionEvent.set_mission_id(mission_id2);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));

	// 验证三个任务都已接受但未完成
	EXPECT_TRUE(missionsComponent.IsAccepted(mission_id));
	EXPECT_FALSE(missionsComponent.IsComplete(mission_id));
	EXPECT_TRUE(missionsComponent.IsAccepted(mission_id1));
	EXPECT_FALSE(missionsComponent.IsComplete(mission_id1));
	EXPECT_TRUE(missionsComponent.IsAccepted(mission_id2));
	EXPECT_FALSE(missionsComponent.IsComplete(mission_id2));

	// 设置任务条件事件（杀死怪物）
	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(playerEntity));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	conditionEvent.set_amount(1);
	MissionSystem::HandleMissionConditionEvent(conditionEvent);

	// 更新任务状态
	tls.dispatcher.update<AcceptMissionEvent>();
	tls.dispatcher.update<MissionConditionEvent>();

	// 验证任务完成状态
	EXPECT_FALSE(missionsComponent.IsAccepted(mission_id));
	EXPECT_TRUE(missionsComponent.IsComplete(mission_id));
	EXPECT_FALSE(missionsComponent.IsAccepted(mission_id1));
	EXPECT_TRUE(missionsComponent.IsComplete(mission_id1));
	EXPECT_FALSE(missionsComponent.IsAccepted(mission_id2));
	EXPECT_TRUE(missionsComponent.IsComplete(mission_id2));
}


TEST(MissionsComp, ConditionAmount)
{
	// 创建带有任务组件的玩家实体
	const auto playerEntity = CreatePlayerWithMissionComponent();
	auto& missionsComponent = tls.registry.get<MissionsComp>(playerEntity);

	// 接受任务
	uint32_t mission_id = 13;
	AcceptMissionEvent acceptMissionEvent;
	acceptMissionEvent.set_entity(entt::to_integral(playerEntity));
	acceptMissionEvent.set_mission_id(mission_id);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));

	// 验证任务已接受但未完成
	EXPECT_TRUE(missionsComponent.IsAccepted(mission_id));
	EXPECT_FALSE(missionsComponent.IsComplete(mission_id));

	// 设置任务条件事件（杀死怪物）
	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(playerEntity));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	conditionEvent.set_amount(1);

	// 处理任务条件事件，第一次应该能够继续接受任务
	MissionSystem::HandleMissionConditionEvent(conditionEvent);
	EXPECT_TRUE(missionsComponent.IsAccepted(mission_id));
	EXPECT_FALSE(missionsComponent.IsComplete(mission_id));

	// 处理任务条件事件，第二次完成任务条件，任务应该完成
	MissionSystem::HandleMissionConditionEvent(conditionEvent);
	EXPECT_FALSE(missionsComponent.IsAccepted(mission_id));
	EXPECT_TRUE(missionsComponent.IsComplete(mission_id));
}

TEST(MissionsComp, MissionRewardList)
{
	// 创建带有任务和任务奖励组件的玩家实体
	const auto playerEntity = CreatePlayerWithMissionComponent();
	auto& missionsComponent = tls.registry.get<MissionsComp>(playerEntity);
	tls.registry.emplace<MissionRewardPbComp>(playerEntity);

	// 接受任务
	uint32_t mission_id = 12;
	AcceptMissionEvent acceptMissionEvent;
	acceptMissionEvent.set_entity(entt::to_integral(playerEntity));
	acceptMissionEvent.set_mission_id(mission_id);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));

	// 设置获取奖励的参数
	GetRewardParam param;
	param.missionId = mission_id;
	param.playerId = playerEntity;

	// 验证任务不在奖励列表中
	EXPECT_EQ(kMissionIdNotInRewardList, MissionSystem::GetMissionReward(param));
	EXPECT_TRUE(missionsComponent.IsAccepted(mission_id));
	EXPECT_FALSE(missionsComponent.IsComplete(mission_id));

	// 设置任务条件事件（杀死怪物）
	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(playerEntity));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	conditionEvent.set_amount(1);

	// 处理任务条件事件，任务完成
	MissionSystem::HandleMissionConditionEvent(conditionEvent);
	EXPECT_FALSE(missionsComponent.IsAccepted(mission_id));
	EXPECT_TRUE(missionsComponent.IsComplete(mission_id));

	// 验证获取任务奖励成功并且不能重复获取
	EXPECT_EQ(kOK, MissionSystem::GetMissionReward(param));
	EXPECT_EQ(kMissionIdNotInRewardList, MissionSystem::GetMissionReward(param));

	// 验证任务完成后奖励列表中的任务数为0
	EXPECT_EQ(0, missionsComponent.CanGetRewardSize());
}

TEST(MissionsComp, AbandonMission)
{
	// 创建带有任务和任务奖励组件的玩家实体
	const auto playerEntity = CreatePlayerWithMissionComponent();
	auto& missionsComponent = tls.registry.get<MissionsComp>(playerEntity);

	// 接受任务
	uint32_t mission_id = 12;
	AcceptMissionEvent acceptMissionEvent;
	acceptMissionEvent.set_entity(entt::to_integral(playerEntity));
	acceptMissionEvent.set_mission_id(mission_id);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));

	// 验证接受任务后的状态
	EXPECT_EQ(1, missionsComponent.MissionSize());
	EXPECT_EQ(0, missionsComponent.CanGetRewardSize());
	EXPECT_EQ(1, missionsComponent.TypeSetSize());

	auto& typeMissions = missionsComponent.classify_for_unittest();
	EXPECT_EQ(1, typeMissions.find(static_cast<uint32_t>(eCondtionType::kConditionKillMonster))->second.size());

	// 设置可奖励的任务
	tls.registry.emplace_or_replace<MissionRewardPbComp>(playerEntity).mutable_can_reward_mission_id()->insert({ mission_id, true });

	// 准备放弃任务的参数
	AbandonParam abandonParam;
	abandonParam.missionId = mission_id;
	abandonParam.playerId = playerEntity;

	// 执行放弃任务操作
	MissionSystem::AbandonMission(abandonParam);

	// 验证放弃任务后的状态
	EXPECT_EQ(0, missionsComponent.MissionSize());
	EXPECT_EQ(0, missionsComponent.CanGetRewardSize());
	EXPECT_EQ(0, missionsComponent.TypeSetSize());
	EXPECT_EQ(0, typeMissions.find(static_cast<uint32_t>(eCondtionType::kConditionKillMonster))->second.size());
}


TEST(MissionsComp, MissionAutoReward)
{
}

TEST(MissionsComp, MissionTimeOut)
{

}

class C
{
    uint32_t status = 2;
    uint32_t id = 1;
};

int main(int argc, char** argv)
{
    Random::GetSingleton();
    condition_config::GetSingleton().load();
    mission_config::GetSingleton().load();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

