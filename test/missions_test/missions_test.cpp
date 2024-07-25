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


TEST(MissionsComp, TypeSize)
{
	auto player = CreatePlayerWithMissionComponent();
	auto& ms = tls.registry.get<MissionsComp>(player);
	tls.dispatcher.update<AcceptMissionEvent>();
    uint32_t mission_id = 6;
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
    EXPECT_EQ(kOK, MissionSystem::AcceptMission(accept_mission_event));
    EXPECT_TRUE(ms.IsAccepted(mission_id));
    EXPECT_FALSE(ms.IsComplete(mission_id));
    for (uint32_t i = static_cast<uint32_t>(eCondtionType::kConditionKillMonster); i < static_cast<uint32_t>(eCondtionType::kConditionCustom); ++i)
    {
        EXPECT_EQ(1, ms.classify_for_unittest().find(i)->second.size());
    }    
    
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    MissionSystem::HandleMissionConditionEvent(ce);
    EXPECT_EQ(1, ms.MissionSize());
    EXPECT_EQ(0, ms.CompleteSize());

    ce.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionTalkWithNpc));
    MissionSystem::HandleMissionConditionEvent(ce);
    EXPECT_EQ(1, ms.MissionSize());
    EXPECT_EQ(0, ms.CompleteSize());

    ce.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionCompleteCondition));
    MissionSystem::HandleMissionConditionEvent(ce);
    EXPECT_EQ(1, ms.MissionSize());
    EXPECT_EQ(0, ms.CompleteSize());

    ce.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionUseItem));
    ce.add_condtion_ids(2);
    MissionSystem::HandleMissionConditionEvent(ce);
    EXPECT_EQ(1, ms.MissionSize());
    EXPECT_EQ(0, ms.CompleteSize());

	ce.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionLevelUp));
    ce.clear_condtion_ids();
    ce.add_condtion_ids(10);
    MissionSystem::HandleMissionConditionEvent(ce);
    EXPECT_EQ(1, ms.MissionSize());
    EXPECT_EQ(0, ms.CompleteSize());

	ce.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionInteraction));
	ce.clear_condtion_ids();
	ce.add_condtion_ids(1);
    ce.add_condtion_ids(2);
    MissionSystem::HandleMissionConditionEvent(ce);
    tls.dispatcher.update<MissionConditionEvent>();
    EXPECT_EQ(0, ms.MissionSize());
    EXPECT_EQ(1, ms.CompleteSize());
    EXPECT_FALSE(ms.IsAccepted(mission_id));
    EXPECT_TRUE(ms.IsComplete(mission_id));
    EXPECT_EQ(0, ms.TypeSetSize());
    for (uint32_t i = static_cast<uint32_t>(eCondtionType::kConditionKillMonster); i < static_cast<uint32_t>(eCondtionType::kConditionCustom); ++i)
    {
        EXPECT_EQ(0, ms.classify_for_unittest().find(i)->second.size());
    }
}

TEST(MissionsComp, CompleteAcceptMission)
{
	const auto player = CreatePlayerWithMissionComponent();
	auto& ms = tls.registry.get<MissionsComp>(player);
	constexpr uint32_t mission_id = 4;
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
    EXPECT_EQ(kOK, MissionSystem::AcceptMission(accept_mission_event));
    EXPECT_EQ(1, ms.TypeSetSize());
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    MissionSystem::HandleMissionConditionEvent(ce);
    EXPECT_FALSE(ms.IsAccepted(mission_id));
    EXPECT_TRUE(ms.IsComplete(mission_id));
    EXPECT_EQ(kMissionAlreadyCompleted, MissionSystem::AcceptMission(accept_mission_event));
}

TEST(MissionsComp, EventTriggerMutableMission)
{
	const auto player = CreatePlayerWithMissionComponent();
	auto& ms = tls.registry.get<MissionsComp>(player);
	AcceptMissionEvent accept_mission_event;
	constexpr uint32_t mission_id1 = 1;
	constexpr uint32_t mission_id2 = 2;
	accept_mission_event.set_mission_id(mission_id1);
	accept_mission_event.set_entity(entt::to_integral(player));
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(accept_mission_event));
	accept_mission_event.set_mission_id(mission_id2);
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(accept_mission_event));

	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
    ce.set_amount(4);
    ce.clear_condtion_ids();
	ce.add_condtion_ids(1);
    MissionSystem::HandleMissionConditionEvent(ce);
    ce.clear_condtion_ids();
	ce.add_condtion_ids(2);
    MissionSystem::HandleMissionConditionEvent(ce);
    ce.clear_condtion_ids();
	ce.add_condtion_ids(3);
    MissionSystem::HandleMissionConditionEvent(ce);
    ce.clear_condtion_ids();
	ce.add_condtion_ids(4);	
	MissionSystem::HandleMissionConditionEvent(ce);
    EXPECT_TRUE(ms.IsComplete(mission_id1));
    EXPECT_TRUE(ms.IsComplete(mission_id2));
}

TEST(MissionsComp, OnCompleteMission)
{
	const auto player = CreatePlayerWithMissionComponent();
	auto& ms = tls.registry.get<MissionsComp>(player);
    uint32_t mission_id = 7;
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
    EXPECT_EQ(kOK, MissionSystem::AcceptMission(accept_mission_event));
    EXPECT_EQ(1, ms.TypeSetSize());
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    MissionSystem::HandleMissionConditionEvent(ce);
    tls.dispatcher.update<AcceptMissionEvent>();
    EXPECT_FALSE(ms.IsAccepted(mission_id));
    EXPECT_TRUE(ms.IsComplete(mission_id));

    auto next_mission = ++mission_id;
    EXPECT_TRUE(ms.IsAccepted(mission_id));
    EXPECT_FALSE(ms.IsComplete(mission_id));
    for (uint32_t i = static_cast<uint32_t>(eCondtionType::kConditionKillMonster); i < static_cast<uint32_t>(eCondtionType::kConditionInteraction); ++i)
    {
        ce.clear_condtion_ids();
        ce.add_condtion_ids(i);
        MissionSystem::HandleMissionConditionEvent(ce);
        EXPECT_FALSE(ms.IsAccepted(mission_id));
        EXPECT_TRUE(ms.IsComplete(mission_id));
        tls.dispatcher.update<AcceptMissionEvent>();
        EXPECT_EQ(0, tls.dispatcher.size<AcceptMissionEvent>());
        EXPECT_TRUE(ms.IsAccepted(++mission_id));
        EXPECT_FALSE(ms.IsComplete(mission_id));
    }
}

TEST(MissionsComp, AcceptNextMirroMission)
{
	const auto player = CreatePlayerWithMissionComponent();
	auto& ms = tls.registry.get<MissionsComp>(player);
	uint32_t mission_id = 7;
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(accept_mission_event));
	EXPECT_EQ(1, ms.TypeSetSize());
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	ce.add_condtion_ids(1);
	ce.set_amount(1);
	MissionSystem::HandleMissionConditionEvent(ce);
	EXPECT_FALSE(ms.IsAccepted(mission_id));
	EXPECT_TRUE(ms.IsComplete(mission_id));
	const auto next_mission_id = ++mission_id;
    tls.dispatcher.update<AcceptMissionEvent>();
	EXPECT_TRUE(ms.IsAccepted(next_mission_id));
	EXPECT_FALSE(ms.IsComplete(next_mission_id));
}

TEST(MissionsComp, MissionCondition)
{
	const auto player = CreatePlayerWithMissionComponent();
	auto& ms = tls.registry.get<MissionsComp>(player);
    uint32_t mission_id = 14;
    uint32_t mission_id1 = 15;
    uint32_t mission_id2 = 16;
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
    EXPECT_EQ(kOK, MissionSystem::AcceptMission(accept_mission_event));
	AcceptMissionEvent accept_mission_event1;
	accept_mission_event1.set_mission_id(mission_id1);
	accept_mission_event1.set_entity(entt::to_integral(player));
    EXPECT_EQ(kOK, MissionSystem::AcceptMission(accept_mission_event1));
	AcceptMissionEvent accept_mission_event2;
	accept_mission_event2.set_mission_id(mission_id2);
	accept_mission_event2.set_entity(entt::to_integral(player));
    EXPECT_EQ(kOK, MissionSystem::AcceptMission(accept_mission_event2));

    EXPECT_TRUE(ms.IsAccepted(mission_id));
    EXPECT_FALSE(ms.IsComplete(mission_id));
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    MissionSystem::HandleMissionConditionEvent(ce);
    tls.dispatcher.update<AcceptMissionEvent>();
    tls.dispatcher.update<MissionConditionEvent>();
    EXPECT_FALSE(ms.IsAccepted(mission_id));
    EXPECT_TRUE(ms.IsComplete(mission_id));
    EXPECT_FALSE(ms.IsAccepted(mission_id1));
    EXPECT_TRUE(ms.IsComplete(mission_id1));
    EXPECT_FALSE(ms.IsAccepted(mission_id2));
    EXPECT_TRUE(ms.IsComplete(mission_id2));
}

TEST(MissionsComp, ConditionAmount)
{
	const auto player = CreatePlayerWithMissionComponent();
	auto& ms = tls.registry.get<MissionsComp>(player);
	constexpr uint32_t mission_id = 13;

    AcceptMissionEvent accept_mission_event;
    accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
    EXPECT_EQ(kOK, MissionSystem::AcceptMission(accept_mission_event));

    EXPECT_TRUE(ms.IsAccepted(mission_id));
    EXPECT_FALSE(ms.IsComplete(mission_id));
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    MissionSystem::HandleMissionConditionEvent(ce);
    EXPECT_TRUE(ms.IsAccepted(mission_id));
    EXPECT_FALSE(ms.IsComplete(mission_id));
    MissionSystem::HandleMissionConditionEvent(ce);
    EXPECT_FALSE(ms.IsAccepted(mission_id));
    EXPECT_TRUE(ms.IsComplete(mission_id));
}

TEST(MissionsComp, MissionRewardList)
{
	const auto player = CreatePlayerWithMissionComponent();
    auto& ms = tls.registry.get<MissionsComp>(player);
    tls.registry.emplace<MissionRewardPbComp>(player);

	constexpr uint32_t mission_id = 12;

	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(accept_mission_event));
    GetRewardParam param;
    param.missionId = mission_id;
    param.playerId = player;
    EXPECT_EQ(kMissionIdNotInRewardList, MissionSystem::GetMissionReward(param));
    EXPECT_TRUE(ms.IsAccepted(mission_id));
    EXPECT_FALSE(ms.IsComplete(mission_id));
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    MissionSystem::HandleMissionConditionEvent(ce);
    EXPECT_FALSE(ms.IsAccepted(mission_id));
    EXPECT_TRUE(ms.IsComplete(mission_id));
    EXPECT_EQ(kOK, MissionSystem::GetMissionReward(param));
    EXPECT_EQ(kMissionIdNotInRewardList, MissionSystem::GetMissionReward(param));
    EXPECT_EQ(0, ms.CanGetRewardSize());
}

TEST(MissionsComp, AbandonMission)
{
	const auto player = CreatePlayerWithMissionComponent();
	auto& ms = tls.registry.get<MissionsComp>(player);
    uint32_t mission_id = 12;
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
	EXPECT_EQ(kOK, MissionSystem::AcceptMission(accept_mission_event));

    EXPECT_EQ(1, ms.MissionSize());
    EXPECT_EQ(0, ms.CanGetRewardSize());
    EXPECT_EQ(1, ms.TypeSetSize());
    auto& type_missions = ms.classify_for_unittest();

    EXPECT_EQ(1, type_missions.find(static_cast<uint32_t>(eCondtionType::kConditionKillMonster))->second.size());
    tls.registry.emplace_or_replace<MissionRewardPbComp>(ms).mutable_can_reward_mission_id()->insert({ mission_id, true });
    AbandonParam param;
	param.missionId = mission_id;
	param.playerId = player;
    
    MissionSystem::AbandonMission(param);

    EXPECT_EQ(0, ms.MissionSize());
    EXPECT_EQ(0, ms.CanGetRewardSize());
    EXPECT_EQ(0, ms.TypeSetSize());
    EXPECT_EQ(0, type_missions.find(static_cast<uint32_t>(eCondtionType::kConditionKillMonster))->second.size());
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

