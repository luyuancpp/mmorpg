#include <gtest/gtest.h>

#include "table/code/condition_table.h"
#include "table/code/mission_table.h"
#include "modules/mission/constants/mission.h"
#include "modules/mission/comp/mission_comp.h"
#include "modules/mission/comp/missions_config_comp.h"
#include "modules/mission/system/mission.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/mission_error_tip.pb.h"
#include "../../nodes/scene/handler/event/mission_event_handler.h"
#include "engine/core/utils/random/random.h"

#include "proto/common/component/mission_comp.pb.h"
#include "proto/common/event/mission_event.pb.h"
#include <thread_context/registry_manager.h>
#include <thread_context/dispatcher_manager.h>
#include <config.h>
#include <node_config_manager.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

entt::entity CreateTestPlayer()
{
	const auto player = tlsRegistryManager.actorRegistry.create();
	(void)tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);
	MissionEventHandler::Register();
	return player;
}

MissionsComp& GetPlayerMissionsComp(entt::entity player)
{
	auto& container = tlsRegistryManager.actorRegistry.get_or_emplace<MissionsContainerComp>(player);
	return container.GetOrCreate(MissionListComp::kPlayerMission);
}

AcceptMissionEvent MakeAcceptEvent(entt::entity player, uint32_t missionId)
{
	AcceptMissionEvent e;
	e.set_entity(entt::to_integral(player));
	e.set_mission_id(missionId);
	return e;
}

MissionConditionEvent MakeConditionEvent(entt::entity player, eConditionType type, uint32_t amount = 1)
{
	MissionConditionEvent e;
	e.set_entity(entt::to_integral(player));
	e.set_condition_type(static_cast<uint32_t>(type));
	e.set_amount(amount);
	return e;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(MissionsComp, AcceptMission)
{
	const auto player = CreateTestPlayer();
	auto& missions = GetPlayerMissionsComp(player);
	missions.SetMissionTypeNotRepeated(false);

	auto& allMissions = GetMissionAllTable();
	std::size_t acceptedCount = 0;

	for (int32_t i = 0; i < allMissions.data_size(); ++i)
	{
		auto ev = MakeAcceptEvent(player, allMissions.data(i).id());
		MissionSystem::AcceptMission(ev, missions, MissionConfig::GetSingleton());
		++acceptedCount;
	}

	EXPECT_EQ(acceptedCount, missions.MissionSize());
	EXPECT_EQ(0, missions.CompleteSize());

	MissionSystem::CompleteAllMissions(player, 0, missions);
	EXPECT_EQ(0, missions.MissionSize());
	EXPECT_EQ(acceptedCount, missions.CompleteSize());
}

TEST(MissionsComp, RepeatedMissionId)
{
	const auto player = CreateTestPlayer();
	auto& missions = GetPlayerMissionsComp(player);

	auto ev = MakeAcceptEvent(player, 1);

	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(ev, missions, MissionConfig::GetSingleton()));
	EXPECT_EQ(kMissionIdRepeated, MissionSystem::AcceptMission(ev, missions, MissionConfig::GetSingleton()));
}

TEST(MissionsComp, RepeatedMissionType)
{
	const auto player = CreateTestPlayer();
	auto& missions = GetPlayerMissionsComp(player);

	// Mission 3 accepted OK
	auto ev3 = MakeAcceptEvent(player, 3);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(ev3, missions, MissionConfig::GetSingleton()));

	// Mission 2 rejected — same type as mission 3
	auto ev2 = MakeAcceptEvent(player, 2);
	EXPECT_EQ(kMissionTypeAlreadyExists, MissionSystem::AcceptMission(ev2, missions, MissionConfig::GetSingleton()));

	// Mission 3 rejected again — already accepted
	EXPECT_EQ(kMissionIdRepeated, MissionSystem::AcceptMission(ev3, missions, MissionConfig::GetSingleton()));
}

TEST(MissionsComp, TriggerMissionCondition)
{
	const auto player = CreateTestPlayer();
	auto& missions = GetPlayerMissionsComp(player);

	auto acceptEv = MakeAcceptEvent(player, 1);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEv, missions, MissionConfig::GetSingleton()));
	EXPECT_EQ(1, missions.TypeSetSize());

	auto condEv = MakeConditionEvent(player, eConditionType::kConditionKillMonster);

	// Conditions 1-3: mission stays in progress
	for (uint32_t condId = 1; condId <= 3; ++condId)
	{
		condEv.clear_condtion_ids();
		condEv.add_condtion_ids(condId);
		MissionSystem::HandleMissionConditionEvent(condEv, missions, MissionConfig::GetSingleton());
		EXPECT_EQ(1, missions.MissionSize());
		EXPECT_EQ(0, missions.CompleteSize());
	}

	// Condition 4: mission completes
	condEv.clear_condtion_ids();
	condEv.add_condtion_ids(4);
	MissionSystem::HandleMissionConditionEvent(condEv, missions, MissionConfig::GetSingleton());
	EXPECT_EQ(0, missions.MissionSize());
	EXPECT_EQ(1, missions.CompleteSize());
	EXPECT_EQ(0, missions.TypeSetSize());
}

TEST(MissionsComp, ConditionTypeSize)
{
	const auto player = CreateTestPlayer();
	auto& missions = GetPlayerMissionsComp(player);

	dispatcher.update<AcceptMissionEvent>();

	constexpr uint32_t missionId = 6;
	auto acceptEv = MakeAcceptEvent(player, missionId);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEv, missions, MissionConfig::GetSingleton()));
	EXPECT_TRUE(missions.IsAccepted(missionId));
	EXPECT_FALSE(missions.IsComplete(missionId));

	// Mission 6 has one condition per type — verify each type bucket has exactly 1 entry
	for (uint32_t t = static_cast<uint32_t>(eConditionType::kConditionKillMonster);
		 t < static_cast<uint32_t>(eConditionType::kConditionCustom); ++t)
	{
		EXPECT_EQ(1, missions.GetEventMissionsClassifyForUnitTest().find(t)->second.size());
	}

	auto condEv = MakeConditionEvent(player, eConditionType::kConditionKillMonster);
	condEv.add_condtion_ids(1);

	// KillMonster
	MissionSystem::HandleMissionConditionEvent(condEv, missions, MissionConfig::GetSingleton());
	EXPECT_EQ(1, missions.MissionSize());

	// TalkWithNpc
	condEv.set_condition_type(static_cast<uint32_t>(eConditionType::kConditionTalkWithNpc));
	MissionSystem::HandleMissionConditionEvent(condEv, missions, MissionConfig::GetSingleton());
	EXPECT_EQ(1, missions.MissionSize());

	// CompleteCondition
	condEv.set_condition_type(static_cast<uint32_t>(eConditionType::kConditionCompleteCondition));
	MissionSystem::HandleMissionConditionEvent(condEv, missions, MissionConfig::GetSingleton());
	EXPECT_EQ(1, missions.MissionSize());

	// UseItem (needs condition ids {1, 2})
	condEv.set_condition_type(static_cast<uint32_t>(eConditionType::kConditionUseItem));
	condEv.clear_condtion_ids();
	condEv.add_condtion_ids(1);
	condEv.add_condtion_ids(2);
	MissionSystem::HandleMissionConditionEvent(condEv, missions, MissionConfig::GetSingleton());
	EXPECT_EQ(1, missions.MissionSize());

	// LevelUp
	condEv.set_condition_type(static_cast<uint32_t>(eConditionType::kConditionLevelUp));
	condEv.clear_condtion_ids();
	condEv.add_condtion_ids(10);
	MissionSystem::HandleMissionConditionEvent(condEv, missions, MissionConfig::GetSingleton());
	EXPECT_EQ(1, missions.MissionSize());

	// Interaction — final condition, completes the mission
	condEv.set_condition_type(static_cast<uint32_t>(eConditionType::kConditionInteraction));
	condEv.clear_condtion_ids();
	condEv.add_condtion_ids(1);
	condEv.add_condtion_ids(2);
	MissionSystem::HandleMissionConditionEvent(condEv, missions, MissionConfig::GetSingleton());

	dispatcher.update<MissionConditionEvent>();

	EXPECT_EQ(0, missions.MissionSize());
	EXPECT_EQ(1, missions.CompleteSize());
	EXPECT_FALSE(missions.IsAccepted(missionId));
	EXPECT_TRUE(missions.IsComplete(missionId));
	EXPECT_EQ(0, missions.TypeSetSize());

	// After completion no condition type should track any mission
	for (uint32_t t = static_cast<uint32_t>(eConditionType::kConditionKillMonster);
		 t < static_cast<uint32_t>(eConditionType::kConditionCustom); ++t)
	{
		EXPECT_EQ(0, missions.GetEventMissionsClassifyForUnitTest().find(t)->second.size());
	}
}


TEST(MissionsComp, CompleteAcceptMission)
{
	const auto player = CreateTestPlayer();
	auto& missions = GetPlayerMissionsComp(player);

	constexpr uint32_t missionId = 4;
	auto acceptEv = MakeAcceptEvent(player, missionId);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEv, missions, MissionConfig::GetSingleton()));
	EXPECT_EQ(1, missions.TypeSetSize());

	// Fulfill condition → mission completes
	auto condEv = MakeConditionEvent(player, eConditionType::kConditionKillMonster);
	condEv.add_condtion_ids(1);
	MissionSystem::HandleMissionConditionEvent(condEv, missions, MissionConfig::GetSingleton());

	EXPECT_FALSE(missions.IsAccepted(missionId));
	EXPECT_TRUE(missions.IsComplete(missionId));

	// Re-accepting a completed mission must fail
	EXPECT_EQ(kMissionAlreadyCompleted, MissionSystem::AcceptMission(acceptEv, missions, MissionConfig::GetSingleton()));
}

TEST(MissionsComp, EventTriggerMutableMission)
{
	const auto player = CreateTestPlayer();
	auto& missions = GetPlayerMissionsComp(player);

	constexpr uint32_t missionId1 = 1;
	constexpr uint32_t missionId2 = 2;

	auto ev1 = MakeAcceptEvent(player, missionId1);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(ev1, missions, MissionConfig::GetSingleton()));

	auto ev2 = MakeAcceptEvent(player, missionId2);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(ev2, missions, MissionConfig::GetSingleton()));

	// Fire conditions 1-4 with amount=4 to complete both missions
	auto condEv = MakeConditionEvent(player, eConditionType::kConditionKillMonster, 4);
	for (uint32_t i = 1; i <= 4; ++i)
	{
		condEv.clear_condtion_ids();
		condEv.add_condtion_ids(i);
		MissionSystem::HandleMissionConditionEvent(condEv, missions, MissionConfig::GetSingleton());
	}

	EXPECT_TRUE(missions.IsComplete(missionId1));
	EXPECT_TRUE(missions.IsComplete(missionId2));
}

TEST(MissionsComp, OnCompleteMission)
{
	const auto player = CreateTestPlayer();
	auto& missions = GetPlayerMissionsComp(player);

	uint32_t missionId = 7;
	auto acceptEv = MakeAcceptEvent(player, missionId);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEv, missions, MissionConfig::GetSingleton()));
	EXPECT_EQ(1, missions.TypeSetSize());

	// Complete mission 7, triggering next mission in chain
	auto condEv = MakeConditionEvent(player, eConditionType::kConditionKillMonster);
	condEv.add_condtion_ids(1);
	MissionSystem::HandleMissionConditionEvent(condEv, missions, MissionConfig::GetSingleton());

	dispatcher.update<AcceptMissionEvent>();
	EXPECT_FALSE(missions.IsAccepted(missionId));
	EXPECT_TRUE(missions.IsComplete(missionId));

	// Next mission in chain should auto-accept
	auto nextMission = ++missionId;
	EXPECT_TRUE(missions.IsAccepted(missionId));
	EXPECT_FALSE(missions.IsComplete(missionId));

	// Walk through the chain: complete each mission, verify next auto-accepts
	for (uint32_t i = static_cast<uint32_t>(eConditionType::kConditionKillMonster);
		 i < static_cast<uint32_t>(eConditionType::kConditionInteraction); ++i)
	{
		condEv.clear_condtion_ids();
		condEv.add_condtion_ids(i);
		MissionSystem::HandleMissionConditionEvent(condEv, missions, MissionConfig::GetSingleton());

		EXPECT_FALSE(missions.IsAccepted(missionId));
		EXPECT_TRUE(missions.IsComplete(missionId));

		dispatcher.update<AcceptMissionEvent>();
		EXPECT_EQ(0, dispatcher.size<AcceptMissionEvent>());

		EXPECT_TRUE(missions.IsAccepted(++missionId));
		EXPECT_FALSE(missions.IsComplete(missionId));
	}
}

TEST(MissionsComp, AcceptNextMirroMission)
{
	const auto player = CreateTestPlayer();
	auto& missions = GetPlayerMissionsComp(player);

	uint32_t missionId = 7;
	auto acceptEv = MakeAcceptEvent(player, missionId);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEv, missions, MissionConfig::GetSingleton()));
	EXPECT_EQ(1, missions.TypeSetSize());

	// Complete mission 7
	auto condEv = MakeConditionEvent(player, eConditionType::kConditionKillMonster);
	condEv.add_condtion_ids(1);
	MissionSystem::HandleMissionConditionEvent(condEv, missions, MissionConfig::GetSingleton());

	dispatcher.update<AcceptMissionEvent>();
	EXPECT_FALSE(missions.IsAccepted(missionId));
	EXPECT_TRUE(missions.IsComplete(missionId));

	// Next mission in chain auto-accepted
	const auto nextMissionId = ++missionId;
	dispatcher.update<AcceptMissionEvent>();
	EXPECT_TRUE(missions.IsAccepted(nextMissionId));
	EXPECT_FALSE(missions.IsComplete(nextMissionId));
}

TEST(MissionsComp, MissionCondition)
{
	const auto player = CreateTestPlayer();
	auto& missions = GetPlayerMissionsComp(player);

	constexpr uint32_t missionId  = 14;
	constexpr uint32_t missionId1 = 15;
	constexpr uint32_t missionId2 = 16;

	// Accept three missions that share the same completion condition
	for (uint32_t id : {missionId, missionId1, missionId2})
	{
		auto ev = MakeAcceptEvent(player, id);
		EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(ev, missions, MissionConfig::GetSingleton()));
		EXPECT_TRUE(missions.IsAccepted(id));
	}

	// Single condition event completes all three
	auto condEv = MakeConditionEvent(player, eConditionType::kConditionKillMonster);
	condEv.add_condtion_ids(1);
	MissionSystem::HandleMissionConditionEvent(condEv, missions, MissionConfig::GetSingleton());

	dispatcher.update<AcceptMissionEvent>();
	dispatcher.update<MissionConditionEvent>();

	for (uint32_t id : {missionId, missionId1, missionId2})
	{
		EXPECT_FALSE(missions.IsAccepted(id));
		EXPECT_TRUE(missions.IsComplete(id));
	}
}

TEST(MissionsComp, ConditionAmount)
{
	const auto player = CreateTestPlayer();
	auto& missions = GetPlayerMissionsComp(player);

	constexpr uint32_t missionId = 13;
	auto acceptEv = MakeAcceptEvent(player, missionId);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEv, missions, MissionConfig::GetSingleton()));
	EXPECT_TRUE(missions.IsAccepted(missionId));

	auto condEv = MakeConditionEvent(player, eConditionType::kConditionKillMonster);
	condEv.add_condtion_ids(1);

	// First event: progress increases but not enough to complete
	MissionSystem::HandleMissionConditionEvent(condEv, missions, MissionConfig::GetSingleton());
	EXPECT_TRUE(missions.IsAccepted(missionId));
	EXPECT_FALSE(missions.IsComplete(missionId));

	// Second event: accumulated amount reaches threshold → completes
	MissionSystem::HandleMissionConditionEvent(condEv, missions, MissionConfig::GetSingleton());
	EXPECT_FALSE(missions.IsAccepted(missionId));
	EXPECT_TRUE(missions.IsComplete(missionId));
}

TEST(MissionsComp, MissionRewardList)
{
	const auto player = CreateTestPlayer();
	auto& missions = GetPlayerMissionsComp(player);

	constexpr uint32_t missionId = 12;
	auto acceptEv = MakeAcceptEvent(player, missionId);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEv, missions, MissionConfig::GetSingleton()));

	// Not yet claimable before completion
	GetRewardParam param;
	param.missionId = missionId;
	param.playerEntity = player;
	EXPECT_EQ(kMissionIdNotInRewardList, MissionSystem::GetMissionReward(param, missions));
	EXPECT_TRUE(missions.IsAccepted(missionId));

	// Complete the mission
	auto condEv = MakeConditionEvent(player, eConditionType::kConditionKillMonster);
	condEv.add_condtion_ids(1);
	MissionSystem::HandleMissionConditionEvent(condEv, missions, MissionConfig::GetSingleton());
	EXPECT_FALSE(missions.IsAccepted(missionId));
	EXPECT_TRUE(missions.IsComplete(missionId));

	// Claim once OK, second claim fails
	EXPECT_EQ(kSuccess, MissionSystem::GetMissionReward(param, missions));
	EXPECT_EQ(kMissionIdNotInRewardList, MissionSystem::GetMissionReward(param, missions));
	EXPECT_EQ(0, missions.CanGetRewardSize());
}

TEST(MissionsComp, AbandonMission)
{
	const auto player = CreateTestPlayer();
	auto& missions = GetPlayerMissionsComp(player);

	constexpr uint32_t missionId = 12;
	auto acceptEv = MakeAcceptEvent(player, missionId);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEv, missions, MissionConfig::GetSingleton()));

	EXPECT_EQ(1, missions.MissionSize());
	EXPECT_EQ(0, missions.CanGetRewardSize());
	EXPECT_EQ(1, missions.TypeSetSize());

	auto& classifyMap = missions.GetEventMissionsClassifyForUnitTest();
	EXPECT_EQ(1, classifyMap.find(static_cast<uint32_t>(eConditionType::kConditionKillMonster))->second.size());

	// Manually mark claimable, then abandon
	SetBit(MissionBitMap, missions.GetClaimableRewards(), missionId);

	AbandonParam abandonParam;
	abandonParam.missionId = missionId;
	abandonParam.playerEntity = player;
	MissionSystem::AbandonMission(abandonParam, missions, MissionConfig::GetSingleton());

	EXPECT_EQ(0, missions.MissionSize());
	EXPECT_EQ(0, missions.CanGetRewardSize());
	EXPECT_EQ(0, missions.TypeSetSize());
	EXPECT_EQ(0, classifyMap.find(static_cast<uint32_t>(eConditionType::kConditionKillMonster))->second.size());
}

TEST(MissionsComp, MissionAutoReward)
{
}

TEST(MissionsComp, MissionTimeOut)
{
}

int main(int argc, char** argv)
{
	readBaseDeployConfig("etc/base_deploy_config.yaml", tlsNodeConfigManager.GetBaseDeployConfig());
	readGameConfig("etc/game_config.yaml", tlsNodeConfigManager.GetGameConfig());
	ConditionTableManager::Instance().Load();
	MissionTableManager::Instance().Load();
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
