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

TEST(MissionsComp, AcceptMission)
{
	const auto player = CreateTestPlayer();
	auto& missionsComp = GetPlayerMissionsComp(player);
	missionsComp.SetMissionTypeNotRepeated(false);

	AcceptMissionEvent acceptEvent;
	acceptEvent.set_entity(entt::to_integral(player));

	auto& missionConfigData = GetMissionAllTable();
	std::size_t acceptedCount = 0;

	for (int32_t i = 0; i < missionConfigData.data_size(); ++i)
	{
		acceptEvent.set_mission_id(missionConfigData.data(i).id());
		MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton());
		++acceptedCount;
	}

	EXPECT_EQ(acceptedCount, missionsComp.MissionSize());
	EXPECT_EQ(0, missionsComp.CompleteSize());

	// Complete all accepted missions
	MissionSystem::CompleteAllMissions(player, 0, missionsComp);
	EXPECT_EQ(0, missionsComp.MissionSize());
	EXPECT_EQ(acceptedCount, missionsComp.CompleteSize());
}

TEST(MissionsComp, RepeatedMissionId)
{
	const auto player = CreateTestPlayer();
	auto& missionsComp = GetPlayerMissionsComp(player);

	constexpr uint32_t missionId = 1;
	AcceptMissionEvent acceptEvent;
	acceptEvent.set_entity(entt::to_integral(player));
	acceptEvent.set_mission_id(missionId);

	// First accept should succeed
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));

	// Second accept should fail — same mission_id
	EXPECT_EQ(kMissionIdRepeated, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));
}

TEST(MissionsComp, RepeatedMissionType)
{
	const auto player = CreateTestPlayer();
	auto& missionsComp = GetPlayerMissionsComp(player);

	AcceptMissionEvent acceptEvent;
	acceptEvent.set_entity(entt::to_integral(player));

	// Mission 3 accepted successfully
	acceptEvent.set_mission_id(3);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));

	// Mission 2 rejected — same mission type as mission 3
	acceptEvent.set_mission_id(2);
	EXPECT_EQ(kMissionTypeAlreadyExists, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));

	// Mission 3 rejected again — already accepted
	acceptEvent.set_mission_id(3);
	EXPECT_EQ(kMissionIdRepeated, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));
}

TEST(MissionsComp, TriggerMissionCondition)
{
	const auto player = CreateTestPlayer();
	auto& missionsComp = GetPlayerMissionsComp(player);

	constexpr uint32_t missionId = 1;
	AcceptMissionEvent acceptEvent;
	acceptEvent.set_entity(entt::to_integral(player));
	acceptEvent.set_mission_id(missionId);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));
	EXPECT_EQ(1, missionsComp.TypeSetSize());

	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(player));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.set_amount(1);

	// Conditions 1–3: mission stays in progress
	for (uint32_t condId = 1; condId <= 3; ++condId)
	{
		conditionEvent.clear_condtion_ids();
		conditionEvent.add_condtion_ids(condId);
		MissionSystem::HandleMissionConditionEvent(conditionEvent, missionsComp, MissionConfig::GetSingleton());
		EXPECT_EQ(1, missionsComp.MissionSize());
		EXPECT_EQ(0, missionsComp.CompleteSize());
	}

	// Condition 4: mission completes
	conditionEvent.clear_condtion_ids();
	conditionEvent.add_condtion_ids(4);
	MissionSystem::HandleMissionConditionEvent(conditionEvent, missionsComp, MissionConfig::GetSingleton());
	EXPECT_EQ(0, missionsComp.MissionSize());
	EXPECT_EQ(1, missionsComp.CompleteSize());
	EXPECT_EQ(0, missionsComp.TypeSetSize());
}

TEST(MissionsComp, ConditionTypeSize)
{
	const auto player = CreateTestPlayer();
	auto& missionsComp = GetPlayerMissionsComp(player);

	dispatcher.update<AcceptMissionEvent>();

	constexpr uint32_t missionId = 6;
	AcceptMissionEvent acceptEvent;
	acceptEvent.set_entity(entt::to_integral(player));
	acceptEvent.set_mission_id(missionId);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));

	EXPECT_TRUE(missionsComp.IsAccepted(missionId));
	EXPECT_FALSE(missionsComp.IsComplete(missionId));

	// Each condition type should track one mission
	for (uint32_t i = static_cast<uint32_t>(eCondtionType::kConditionKillMonster); i < static_cast<uint32_t>(eCondtionType::kConditionCustom); ++i)
	{
		EXPECT_EQ(1, missionsComp.GetEventMissionsClassifyForUnitTest().find(i)->second.size());
	}

	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(player));
	conditionEvent.set_amount(1);

	// kConditionKillMonster
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	MissionSystem::HandleMissionConditionEvent(conditionEvent, missionsComp, MissionConfig::GetSingleton());
	EXPECT_EQ(1, missionsComp.MissionSize());
	EXPECT_EQ(0, missionsComp.CompleteSize());

	// kConditionTalkWithNpc
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionTalkWithNpc));
	MissionSystem::HandleMissionConditionEvent(conditionEvent, missionsComp, MissionConfig::GetSingleton());
	EXPECT_EQ(1, missionsComp.MissionSize());
	EXPECT_EQ(0, missionsComp.CompleteSize());

	// kConditionCompleteCondition
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionCompleteCondition));
	MissionSystem::HandleMissionConditionEvent(conditionEvent, missionsComp, MissionConfig::GetSingleton());
	EXPECT_EQ(1, missionsComp.MissionSize());
	EXPECT_EQ(0, missionsComp.CompleteSize());

	// kConditionUseItem
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionUseItem));
	conditionEvent.clear_condtion_ids();
	conditionEvent.add_condtion_ids(1);
	conditionEvent.add_condtion_ids(2);
	MissionSystem::HandleMissionConditionEvent(conditionEvent, missionsComp, MissionConfig::GetSingleton());
	EXPECT_EQ(1, missionsComp.MissionSize());
	EXPECT_EQ(0, missionsComp.CompleteSize());

	// kConditionLevelUp
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionLevelUp));
	conditionEvent.clear_condtion_ids();
	conditionEvent.add_condtion_ids(10);
	MissionSystem::HandleMissionConditionEvent(conditionEvent, missionsComp, MissionConfig::GetSingleton());
	EXPECT_EQ(1, missionsComp.MissionSize());
	EXPECT_EQ(0, missionsComp.CompleteSize());

	// kConditionInteraction — final condition, completes the mission
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionInteraction));
	conditionEvent.clear_condtion_ids();
	conditionEvent.add_condtion_ids(1);
	conditionEvent.add_condtion_ids(2);
	MissionSystem::HandleMissionConditionEvent(conditionEvent, missionsComp, MissionConfig::GetSingleton());

	dispatcher.update<MissionConditionEvent>();

	EXPECT_EQ(0, missionsComp.MissionSize());
	EXPECT_EQ(1, missionsComp.CompleteSize());
	EXPECT_FALSE(missionsComp.IsAccepted(missionId));
	EXPECT_TRUE(missionsComp.IsComplete(missionId));
	EXPECT_EQ(0, missionsComp.TypeSetSize());

	// No condition types should track missions after completion
	for (uint32_t i = static_cast<uint32_t>(eCondtionType::kConditionKillMonster); i < static_cast<uint32_t>(eCondtionType::kConditionCustom); ++i)
	{
		EXPECT_EQ(0, missionsComp.GetEventMissionsClassifyForUnitTest().find(i)->second.size());
	}
}


TEST(MissionsComp, CompleteAcceptMission)
{
	const auto player = CreateTestPlayer();
	auto& missionsComp = GetPlayerMissionsComp(player);

	constexpr uint32_t missionId = 4;
	AcceptMissionEvent acceptEvent;
	acceptEvent.set_entity(entt::to_integral(player));
	acceptEvent.set_mission_id(missionId);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));
	EXPECT_EQ(1, missionsComp.TypeSetSize());

	// Fulfill condition to complete the mission
	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(player));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	conditionEvent.set_amount(1);
	MissionSystem::HandleMissionConditionEvent(conditionEvent, missionsComp, MissionConfig::GetSingleton());

	EXPECT_FALSE(missionsComp.IsAccepted(missionId));
	EXPECT_TRUE(missionsComp.IsComplete(missionId));

	// Re-accepting a completed mission should fail
	EXPECT_EQ(kMissionAlreadyCompleted, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));
}

TEST(MissionsComp, EventTriggerMutableMission)
{
	const auto player = CreateTestPlayer();
	auto& missionsComp = GetPlayerMissionsComp(player);

	// Accept missions 1 and 2
	AcceptMissionEvent acceptEvent;
	acceptEvent.set_entity(entt::to_integral(player));

	constexpr uint32_t missionId1 = 1;
	acceptEvent.set_mission_id(missionId1);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));

	constexpr uint32_t missionId2 = 2;
	acceptEvent.set_mission_id(missionId2);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));

	// Fire conditions 1–4 to progressively complete both missions
	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(player));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.set_amount(4);

	for (uint32_t i = 1; i <= 4; ++i)
	{
		conditionEvent.clear_condtion_ids();
		conditionEvent.add_condtion_ids(i);
		MissionSystem::HandleMissionConditionEvent(conditionEvent, missionsComp, MissionConfig::GetSingleton());
	}

	EXPECT_TRUE(missionsComp.IsComplete(missionId1));
	EXPECT_TRUE(missionsComp.IsComplete(missionId2));
}

TEST(MissionsComp, OnCompleteMission)
{
	const auto player = CreateTestPlayer();
	auto& missionsComp = GetPlayerMissionsComp(player);

	uint32_t missionId = 7;
	AcceptMissionEvent acceptEvent;
	acceptEvent.set_entity(entt::to_integral(player));
	acceptEvent.set_mission_id(missionId);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));
	EXPECT_EQ(1, missionsComp.TypeSetSize());

	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(player));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	conditionEvent.set_amount(1);
	MissionSystem::HandleMissionConditionEvent(conditionEvent, missionsComp, MissionConfig::GetSingleton());

	// Dispatch queued AcceptMissionEvent for next mission in chain
	dispatcher.update<AcceptMissionEvent>();
	EXPECT_FALSE(missionsComp.IsAccepted(missionId));
	EXPECT_TRUE(missionsComp.IsComplete(missionId));

	// Next mission in chain should now be accepted
	auto nextMission = ++missionId;
	EXPECT_TRUE(missionsComp.IsAccepted(missionId));
	EXPECT_FALSE(missionsComp.IsComplete(missionId));

	// Walk through remaining chained missions
	for (uint32_t i = static_cast<uint32_t>(eCondtionType::kConditionKillMonster); i < static_cast<uint32_t>(eCondtionType::kConditionInteraction); ++i)
	{
		conditionEvent.clear_condtion_ids();
		conditionEvent.add_condtion_ids(i);
		MissionSystem::HandleMissionConditionEvent(conditionEvent, missionsComp, MissionConfig::GetSingleton());

		EXPECT_FALSE(missionsComp.IsAccepted(missionId));
		EXPECT_TRUE(missionsComp.IsComplete(missionId));

		dispatcher.update<AcceptMissionEvent>();
		EXPECT_EQ(0, dispatcher.size<AcceptMissionEvent>());

		EXPECT_TRUE(missionsComp.IsAccepted(++missionId));
		EXPECT_FALSE(missionsComp.IsComplete(missionId));
	}
}

TEST(MissionsComp, AcceptNextMirroMission)
{
	const auto player = CreateTestPlayer();
	auto& missionsComp = GetPlayerMissionsComp(player);

	uint32_t missionId = 7;
	AcceptMissionEvent acceptEvent;
	acceptEvent.set_entity(entt::to_integral(player));
	acceptEvent.set_mission_id(missionId);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));
	EXPECT_EQ(1, missionsComp.TypeSetSize());

	// Complete mission 7
	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(player));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	conditionEvent.set_amount(1);
	MissionSystem::HandleMissionConditionEvent(conditionEvent, missionsComp, MissionConfig::GetSingleton());

	dispatcher.update<AcceptMissionEvent>();
	EXPECT_FALSE(missionsComp.IsAccepted(missionId));
	EXPECT_TRUE(missionsComp.IsComplete(missionId));

	// Next mission in chain should now be accepted
	const auto nextMissionId = ++missionId;
	dispatcher.update<AcceptMissionEvent>();
	EXPECT_TRUE(missionsComp.IsAccepted(nextMissionId));
	EXPECT_FALSE(missionsComp.IsComplete(nextMissionId));
}

TEST(MissionsComp, MissionCondition)
{
	const auto player = CreateTestPlayer();
	auto& missionsComp = GetPlayerMissionsComp(player);

	constexpr uint32_t missionId  = 14;
	constexpr uint32_t missionId1 = 15;
	constexpr uint32_t missionId2 = 16;

	AcceptMissionEvent acceptEvent;
	acceptEvent.set_entity(entt::to_integral(player));

	acceptEvent.set_mission_id(missionId);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));

	acceptEvent.set_mission_id(missionId1);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));

	acceptEvent.set_mission_id(missionId2);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));

	// All three are accepted, none completed
	EXPECT_TRUE(missionsComp.IsAccepted(missionId));
	EXPECT_FALSE(missionsComp.IsComplete(missionId));
	EXPECT_TRUE(missionsComp.IsAccepted(missionId1));
	EXPECT_FALSE(missionsComp.IsComplete(missionId1));
	EXPECT_TRUE(missionsComp.IsAccepted(missionId2));
	EXPECT_FALSE(missionsComp.IsComplete(missionId2));

	// Single condition event completes all three
	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(player));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	conditionEvent.set_amount(1);
	MissionSystem::HandleMissionConditionEvent(conditionEvent, missionsComp, MissionConfig::GetSingleton());

	dispatcher.update<AcceptMissionEvent>();
	dispatcher.update<MissionConditionEvent>();

	EXPECT_FALSE(missionsComp.IsAccepted(missionId));
	EXPECT_TRUE(missionsComp.IsComplete(missionId));
	EXPECT_FALSE(missionsComp.IsAccepted(missionId1));
	EXPECT_TRUE(missionsComp.IsComplete(missionId1));
	EXPECT_FALSE(missionsComp.IsAccepted(missionId2));
	EXPECT_TRUE(missionsComp.IsComplete(missionId2));
}

TEST(MissionsComp, ConditionAmount)
{
	const auto player = CreateTestPlayer();
	auto& missionsComp = GetPlayerMissionsComp(player);

	constexpr uint32_t missionId = 13;
	AcceptMissionEvent acceptEvent;
	acceptEvent.set_entity(entt::to_integral(player));
	acceptEvent.set_mission_id(missionId);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));

	EXPECT_TRUE(missionsComp.IsAccepted(missionId));
	EXPECT_FALSE(missionsComp.IsComplete(missionId));

	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(player));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	conditionEvent.set_amount(1);

	// First event: progress increases but mission not yet complete
	MissionSystem::HandleMissionConditionEvent(conditionEvent, missionsComp, MissionConfig::GetSingleton());
	EXPECT_TRUE(missionsComp.IsAccepted(missionId));
	EXPECT_FALSE(missionsComp.IsComplete(missionId));

	// Second event: mission completes
	MissionSystem::HandleMissionConditionEvent(conditionEvent, missionsComp, MissionConfig::GetSingleton());
	EXPECT_FALSE(missionsComp.IsAccepted(missionId));
	EXPECT_TRUE(missionsComp.IsComplete(missionId));
}

TEST(MissionsComp, MissionRewardList)
{
	const auto player = CreateTestPlayer();
	auto& missionsComp = GetPlayerMissionsComp(player);

	constexpr uint32_t missionId = 12;
	AcceptMissionEvent acceptEvent;
	acceptEvent.set_entity(entt::to_integral(player));
	acceptEvent.set_mission_id(missionId);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));

	// Not yet claimable before completion
	GetRewardParam param;
	param.missionId = missionId;
	param.playerEntity = player;
	EXPECT_EQ(kMissionIdNotInRewardList, MissionSystem::GetMissionReward(param, missionsComp));
	EXPECT_TRUE(missionsComp.IsAccepted(missionId));
	EXPECT_FALSE(missionsComp.IsComplete(missionId));

	// Complete the mission
	MissionConditionEvent conditionEvent;
	conditionEvent.set_entity(entt::to_integral(player));
	conditionEvent.set_condition_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	conditionEvent.add_condtion_ids(1);
	conditionEvent.set_amount(1);
	MissionSystem::HandleMissionConditionEvent(conditionEvent, missionsComp, MissionConfig::GetSingleton());
	EXPECT_FALSE(missionsComp.IsAccepted(missionId));
	EXPECT_TRUE(missionsComp.IsComplete(missionId));

	// Claim reward once, second claim fails
	EXPECT_EQ(kSuccess, MissionSystem::GetMissionReward(param, missionsComp));
	EXPECT_EQ(kMissionIdNotInRewardList, MissionSystem::GetMissionReward(param, missionsComp));
	EXPECT_EQ(0, missionsComp.CanGetRewardSize());
}

TEST(MissionsComp, AbandonMission)
{
	const auto player = CreateTestPlayer();
	auto& missionsComp = GetPlayerMissionsComp(player);

	constexpr uint32_t missionId = 12;
	AcceptMissionEvent acceptEvent;
	acceptEvent.set_entity(entt::to_integral(player));
	acceptEvent.set_mission_id(missionId);
	EXPECT_EQ(kSuccess, MissionSystem::AcceptMission(acceptEvent, missionsComp, MissionConfig::GetSingleton()));

	EXPECT_EQ(1, missionsComp.MissionSize());
	EXPECT_EQ(0, missionsComp.CanGetRewardSize());
	EXPECT_EQ(1, missionsComp.TypeSetSize());

	auto& typeMissions = missionsComp.GetEventMissionsClassifyForUnitTest();
	EXPECT_EQ(1, typeMissions.find(static_cast<uint32_t>(eCondtionType::kConditionKillMonster))->second.size());

	// Manually mark claimable, then abandon
	SetBit(MissionBitMap, missionsComp.GetClaimableRewards(), missionId);

	AbandonParam abandonParam;
	abandonParam.missionId = missionId;
	abandonParam.playerEntity = player;
	MissionSystem::AbandonMission(abandonParam, missionsComp, MissionConfig::GetSingleton());

	EXPECT_EQ(0, missionsComp.MissionSize());
	EXPECT_EQ(0, missionsComp.CanGetRewardSize());
	EXPECT_EQ(0, missionsComp.TypeSetSize());
	EXPECT_EQ(0, typeMissions.find(static_cast<uint32_t>(eCondtionType::kConditionKillMonster))->second.size());
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
