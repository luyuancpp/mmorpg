#include <gtest/gtest.h>

#include "condition_config.h"
#include "mission_config.h"
#include "constants/mission.h"
#include "comp/mission_comp.h"
#include "system/mission/mission_system.h"
#include "constants/tips_id.h"
#include "handler/event/mission_event_handler.h"
#include "util/random.h"
#include "thread_local/storage.h"

#include "proto/logic/component/mission_comp.pb.h"
#include "proto/logic/event/mission_event.pb.h"

decltype(auto) CreatePlayerEntityWithMissionComponent()
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
	const auto playerEntity = CreatePlayerEntityWithMissionComponent();
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
	const auto playerEntity = CreatePlayerEntityWithMissionComponent();
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
	const auto playerEntity = CreatePlayerEntityWithMissionComponent();
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
	const auto playerEntity = CreatePlayerEntityWithMissionComponent();
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
	auto playerEntity = CreatePlayerEntityWithMissionComponent();
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


MissionsComp& GetMissionsComponent(entt::entity playerEntity)
{
	return tls.registry.get<MissionsComp>(playerEntity);
}

TEST(MissionsComp, CompleteAcceptMission)
{
	// Create a player entity with a mission component
	const auto playerEntity = CreatePlayerEntityWithMissionComponent();
	auto& missionsComponent = GetMissionsComponent(playerEntity);

	// Set the accept mission event for mission ID 4
	constexpr uint32_t missionId = 4;
	AcceptMissionEvent acceptMissionEvent;
	acceptMissionEvent.set_entity(entt::to_integral(playerEntity));
	acceptMissionEvent.set_mission_id(missionId);

	// Verify if accepting the mission is successful
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));

	// Verify the size of the mission type set
	EXPECT_EQ(1, missionsComponent.TypeSetSize());

	// Prepare mission condition event
	MissionConditionEvent missionConditionEvent;
	missionConditionEvent.set_entity(entt::to_integral(playerEntity));
	missionConditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	missionConditionEvent.add_condtion_ids(1);
	missionConditionEvent.set_amount(1);

	// Handle mission condition event to mark mission as complete
	MissionSystem::HandleMissionConditionEvent(missionConditionEvent);

	// Verify the mission is no longer in accepted state, but complete
	EXPECT_FALSE(missionsComponent.IsAccepted(missionId));
	EXPECT_TRUE(missionsComponent.IsComplete(missionId));

	// Attempt to accept the already completed mission again, expect kMissionAlreadyCompleted
	EXPECT_EQ(kMissionAlreadyCompleted, MissionSystem::AcceptMission(acceptMissionEvent));
}

TEST(MissionsComp, EventTriggerMutableMission)
{
	// Create a player entity with a mission component
	const auto playerEntity = CreatePlayerEntityWithMissionComponent();
	auto& missionsComponent = GetMissionsComponent(playerEntity);

	// Accept mission events for mission IDs 1 and 2
	constexpr uint32_t missionId1 = 1;
	AcceptMissionEvent acceptMissionEvent1;
	acceptMissionEvent1.set_entity(entt::to_integral(playerEntity));
	acceptMissionEvent1.set_mission_id(missionId1);

	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent1));

	constexpr uint32_t missionId2 = 2;
	AcceptMissionEvent acceptMissionEvent2;
	acceptMissionEvent2.set_entity(entt::to_integral(playerEntity));
	acceptMissionEvent2.set_mission_id(missionId2);

	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent2));

	// Prepare mission condition event
	MissionConditionEvent missionConditionEvent;
	missionConditionEvent.set_entity(entt::to_integral(playerEntity));
	missionConditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	missionConditionEvent.set_amount(4);

	// Handle mission condition event to progressively complete missions
	for (int i = 1; i <= 4; ++i)
	{
		missionConditionEvent.clear_condtion_ids();
		missionConditionEvent.add_condtion_ids(i);
		MissionSystem::HandleMissionConditionEvent(missionConditionEvent);
	}

	// Verify missions with ID 1 and 2 are complete
	EXPECT_TRUE(missionsComponent.IsComplete(missionId1));
	EXPECT_TRUE(missionsComponent.IsComplete(missionId2));
}

TEST(MissionsComp, OnCompleteMission)
{
	// Create a player entity with a mission component
	const auto playerEntity = CreatePlayerEntityWithMissionComponent();
	auto& missionsComponent = GetMissionsComponent(playerEntity);

	// Accept mission ID 7
	uint32_t missionId = 7;
	AcceptMissionEvent acceptMissionEvent;
	acceptMissionEvent.set_entity(entt::to_integral(playerEntity));
	acceptMissionEvent.set_mission_id(missionId);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));
	EXPECT_EQ(1, missionsComponent.TypeSetSize());

	// Set mission condition event
	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(playerEntity));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	conditionEvent.set_amount(1);
	MissionSystem::HandleMissionConditionEvent(conditionEvent);

	// Update mission status and verify completion
	tls.dispatcher.update<AcceptMissionEvent>();
	EXPECT_FALSE(missionsComponent.IsAccepted(missionId));
	EXPECT_TRUE(missionsComponent.IsComplete(missionId));

	// Accept next mission and verify its status
	auto nextMission = ++missionId;
	EXPECT_TRUE(missionsComponent.IsAccepted(missionId));
	EXPECT_FALSE(missionsComponent.IsComplete(missionId));

	// Loop through mission conditions and verify state changes after completion
	for (uint32_t i = static_cast<uint32_t>(eCondtionType::kConditionKillMonster); i < static_cast<uint32_t>(eCondtionType::kConditionInteraction); ++i)
	{
		conditionEvent.clear_condtion_ids();
		conditionEvent.add_condtion_ids(i);
		MissionSystem::HandleMissionConditionEvent(conditionEvent);

		EXPECT_FALSE(missionsComponent.IsAccepted(missionId));
		EXPECT_TRUE(missionsComponent.IsComplete(missionId));

		tls.dispatcher.update<AcceptMissionEvent>();
		EXPECT_EQ(0, tls.dispatcher.size<AcceptMissionEvent>());

		EXPECT_TRUE(missionsComponent.IsAccepted(++missionId));
		EXPECT_FALSE(missionsComponent.IsComplete(missionId));
	}
}

TEST(MissionsComp, AcceptNextMirroMission)
{
	// Create a player entity with a mission component
	const auto playerEntity = CreatePlayerEntityWithMissionComponent();
	auto& missionsComponent = GetMissionsComponent(playerEntity);

	// Accept mission ID 7
	uint32_t missionId = 7;
	AcceptMissionEvent acceptMissionEvent;
	acceptMissionEvent.set_entity(entt::to_integral(playerEntity));
	acceptMissionEvent.set_mission_id(missionId);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));
	EXPECT_EQ(1, missionsComponent.TypeSetSize());

	// Set mission condition event
	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(playerEntity));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	conditionEvent.set_amount(1);
	MissionSystem::HandleMissionConditionEvent(conditionEvent);

	// Update mission status and verify completion
	tls.dispatcher.update<AcceptMissionEvent>();
	EXPECT_FALSE(missionsComponent.IsAccepted(missionId));
	EXPECT_TRUE(missionsComponent.IsComplete(missionId));

	// Accept next mission and verify its status
	const auto nextMissionId = ++missionId;
	tls.dispatcher.update<AcceptMissionEvent>();
	EXPECT_TRUE(missionsComponent.IsAccepted(nextMissionId));
	EXPECT_FALSE(missionsComponent.IsComplete(nextMissionId));
}

TEST(MissionsComp, MissionCondition)
{
	// Create a player entity with a mission component
	const auto playerEntity = CreatePlayerEntityWithMissionComponent();
	auto& missionsComponent = GetMissionsComponent(playerEntity);

	// Accept three different missions
	uint32_t missionId = 14;
	uint32_t missionId1 = 15;
	uint32_t missionId2 = 16;

	AcceptMissionEvent acceptMissionEvent;
	acceptMissionEvent.set_entity(entt::to_integral(playerEntity));

	acceptMissionEvent.set_mission_id(missionId);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));

	acceptMissionEvent.set_mission_id(missionId1);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));

	acceptMissionEvent.set_mission_id(missionId2);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));

	// Verify all three missions are accepted but not complete
	EXPECT_TRUE(missionsComponent.IsAccepted(missionId));
	EXPECT_FALSE(missionsComponent.IsComplete(missionId));
	EXPECT_TRUE(missionsComponent.IsAccepted(missionId1));
	EXPECT_FALSE(missionsComponent.IsComplete(missionId1));
	EXPECT_TRUE(missionsComponent.IsAccepted(missionId2));
	EXPECT_FALSE(missionsComponent.IsComplete(missionId2));

	// Set mission condition event (kill monster)
	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(playerEntity));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	conditionEvent.set_amount(1);
	MissionSystem::HandleMissionConditionEvent(conditionEvent);

	// Update mission status and verify completion
	tls.dispatcher.update<AcceptMissionEvent>();
	tls.dispatcher.update<MissionConditionEvent>();

	// Verify mission completion status
	EXPECT_FALSE(missionsComponent.IsAccepted(missionId));
	EXPECT_TRUE(missionsComponent.IsComplete(missionId));
	EXPECT_FALSE(missionsComponent.IsAccepted(missionId1));
	EXPECT_TRUE(missionsComponent.IsComplete(missionId1));
	EXPECT_FALSE(missionsComponent.IsAccepted(missionId2));
	EXPECT_TRUE(missionsComponent.IsComplete(missionId2));
}

TEST(MissionsComp, ConditionAmount)
{
	// Create a player entity with a mission component
	const auto playerEntity = CreatePlayerEntityWithMissionComponent();
	auto& missionsComponent = GetMissionsComponent(playerEntity);

	// Accept mission
	uint32_t missionId = 13;
	AcceptMissionEvent acceptMissionEvent;
	acceptMissionEvent.set_entity(entt::to_integral(playerEntity));
	acceptMissionEvent.set_mission_id(missionId);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));

	// Verify mission is accepted but not complete
	EXPECT_TRUE(missionsComponent.IsAccepted(missionId));
	EXPECT_FALSE(missionsComponent.IsComplete(missionId));

	// Set mission condition event (kill monster)
	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(playerEntity));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	conditionEvent.set_amount(1);

	// Handle mission condition event, should continue accepting mission the first time
	MissionSystem::HandleMissionConditionEvent(conditionEvent);
	EXPECT_TRUE(missionsComponent.IsAccepted(missionId));
	EXPECT_FALSE(missionsComponent.IsComplete(missionId));

	// Handle mission condition event, complete mission the second time
	MissionSystem::HandleMissionConditionEvent(conditionEvent);
	EXPECT_FALSE(missionsComponent.IsAccepted(missionId));
	EXPECT_TRUE(missionsComponent.IsComplete(missionId));
}

TEST(MissionsComp, MissionRewardList)
{
	// Create a player entity with mission and mission reward components
	const auto playerEntity = CreatePlayerEntityWithMissionComponent();
	auto& missionsComponent = GetMissionsComponent(playerEntity);
	tls.registry.emplace<MissionRewardPbComp>(playerEntity);

	// Accept mission
	uint32_t missionId = 12;
	AcceptMissionEvent acceptMissionEvent;
	acceptMissionEvent.set_entity(entt::to_integral(playerEntity));
	acceptMissionEvent.set_mission_id(missionId);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));

	// Set reward parameters
	GetRewardParam param;
	param.missionId = missionId;
	param.playerId = playerEntity;

	// Verify mission is not in reward list
	EXPECT_EQ(kMissionIdNotInRewardList, MissionSystem::GetMissionReward(param));
	EXPECT_TRUE(missionsComponent.IsAccepted(missionId));
	EXPECT_FALSE(missionsComponent.IsComplete(missionId));

	// Set mission condition event (kill monster)
	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(playerEntity));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	conditionEvent.set_amount(1);

	// Handle mission condition event, complete mission
	MissionSystem::HandleMissionConditionEvent(conditionEvent);
	EXPECT_FALSE(missionsComponent.IsAccepted(missionId));
	EXPECT_TRUE(missionsComponent.IsComplete(missionId));

	// Verify successfully getting mission reward and cannot repeat
	EXPECT_EQ(kOK, MissionSystem::GetMissionReward(param));
	EXPECT_EQ(kMissionIdNotInRewardList, MissionSystem::GetMissionReward(param));

	// Verify mission reward list size is 0 after mission completion
	EXPECT_EQ(0, missionsComponent.CanGetRewardSize());
}

TEST(MissionsComp, AbandonMission)
{
	// Create a player entity with mission and mission reward components
	const auto playerEntity = CreatePlayerEntityWithMissionComponent();
	auto& missionsComponent = GetMissionsComponent(playerEntity);

	// Accept mission
	uint32_t missionId = 12;
	AcceptMissionEvent acceptMissionEvent;
	acceptMissionEvent.set_entity(entt::to_integral(playerEntity));
	acceptMissionEvent.set_mission_id(missionId);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(acceptMissionEvent));

	// Verify state after accepting mission
	EXPECT_EQ(1, missionsComponent.MissionSize());
	EXPECT_EQ(0, missionsComponent.CanGetRewardSize());
	EXPECT_EQ(1, missionsComponent.TypeSetSize());

	auto& typeMissions = missionsComponent.classify_for_unittest();
	EXPECT_EQ(1, typeMissions.find(static_cast<uint32_t>(eCondtionType::kConditionKillMonster))->second.size());

	// Set mission as rewardable
	tls.registry.emplace_or_replace<MissionRewardPbComp>(playerEntity).mutable_can_reward_mission_id()->insert({ missionId, true });

	// Prepare abandon mission parameters
	AbandonParam abandonParam;
	abandonParam.missionId = missionId;
	abandonParam.playerId = playerEntity;

	// Perform abandon mission operation
	MissionSystem::AbandonMission(abandonParam);

	// Verify state after abandoning mission
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
    condition_config::GetSingleton().load();
    mission_config::GetSingleton().load();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

