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

decltype(auto) CreateMission()
{
	const auto player = tls.registry.create();
	auto& ms = tls.registry.emplace<MissionsComp>(player);
    ms.set_event_owner(player);
    MissionEventHandler::Register();
    return &ms;
}

decltype(auto) CreatePlayerMission()
{
	const auto player = tls.registry.create();
	auto& ms = tls.registry.emplace<MissionsComp>(player);
	ms.set_event_owner(player);
	MissionEventHandler::Register();
	return player;
}

TEST(MissionsComp, AcceptMission)
{
	constexpr uint32_t mission_id = 1;
    const auto player = CreatePlayerMission();
	auto& ms = tls.registry.get<MissionsComp>(player);
	ms.SetMissionTypeNotRepeated(false);
    AcceptMissionEvent accept_mission_event;
    accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
    auto& data = mission_config::GetSingleton().all();
    std::size_t sz = 0;
    for (int32_t i = 0; i < data.data_size(); ++i)
    {
        accept_mission_event.set_mission_id(data.data(i).id());
        auto m = MissionSystem::Accept(accept_mission_event);
        ++sz;
    }
    EXPECT_EQ(sz, ms.MissionSize());
    EXPECT_EQ(0, ms.CompleteSize());
    MissionSystem::CompleteAllMission(player, 0);
    EXPECT_EQ(0, ms.MissionSize());
    EXPECT_EQ(sz, ms.CompleteSize());
}

TEST(MissionsComp, RepeatedMission)
{
	const auto player = CreatePlayerMission();
	auto& ms = tls.registry.get<MissionsComp>(player);
    {
	    constexpr uint32_t mission_id = 1;
		AcceptMissionEvent accept_mission_event;
    	accept_mission_event.set_entity(entt::to_integral(player));
		accept_mission_event.set_mission_id(mission_id);
        EXPECT_EQ(kRetOK, MissionSystem::Accept(accept_mission_event));
        EXPECT_EQ(kRetMissionIdRepeated, MissionSystem::Accept(accept_mission_event));
    }

    {
		AcceptMissionEvent accept_mission_event1;
    	accept_mission_event1.set_entity(entt::to_integral(player));
        accept_mission_event1.set_mission_id(3);
		AcceptMissionEvent accept_mission_event2;
    	accept_mission_event2.set_entity(entt::to_integral(player));
        accept_mission_event2.set_mission_id(2);
		EXPECT_EQ(kRetOK, MissionSystem::Accept(accept_mission_event1));
		EXPECT_EQ(kRetMissionTypeRepeated, MissionSystem::Accept(accept_mission_event2));
    }
}

TEST(MissionsComp, TriggerCondition)
{
	const auto player = CreatePlayerMission();
	auto& ms = tls.registry.get<MissionsComp>(player);
	constexpr uint32_t mission_id = 1;
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
    EXPECT_EQ(kRetOK, MissionSystem::Accept(accept_mission_event));
    EXPECT_EQ(1, ms.TypeSetSize());
    MissionConditionEvent ce;
    ce.set_entity(ms);
    ce.set_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
    ce.add_condtion_ids(1);
    ce.set_amount(1);
    MissionSystem::Receive(ce);
    EXPECT_EQ(1, ms.MissionSize());
    EXPECT_EQ(0, ms.CompleteSize());

    ce.clear_condtion_ids();
    ce.add_condtion_ids(2);
    MissionSystem::Receive(ce);
    EXPECT_EQ(1, ms.MissionSize());
    EXPECT_EQ(0, ms.CompleteSize());

	ce.clear_condtion_ids();
	ce.add_condtion_ids(3);
    MissionSystem::Receive(ce);
    EXPECT_EQ(1, ms.MissionSize());
    EXPECT_EQ(0, ms.CompleteSize());

	ce.clear_condtion_ids();
	ce.add_condtion_ids(4);
    MissionSystem::Receive(ce);
    EXPECT_EQ(0, ms.MissionSize());
    EXPECT_EQ(1, ms.CompleteSize());
    EXPECT_EQ(0, ms.TypeSetSize());
}

TEST(MissionsComp, TypeSize)
{
	auto player = CreatePlayerMission();
	auto& ms = tls.registry.get<MissionsComp>(player);
	tls.dispatcher.update<AcceptMissionEvent>();
    uint32_t mission_id = 6;
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
    EXPECT_EQ(kRetOK, MissionSystem::Accept(accept_mission_event));
    EXPECT_TRUE(ms.IsAccepted(mission_id));
    EXPECT_FALSE(ms.IsComplete(mission_id));
    for (uint32_t i = static_cast<uint32_t>(eCondtionType::kConditionKillMonster); i < static_cast<uint32_t>(eCondtionType::kConditionCustom); ++i)
    {
        EXPECT_EQ(1, ms.classify_for_unittest().find(i)->second.size());
    }    
    
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    MissionSystem::Receive(ce);
    EXPECT_EQ(1, ms.MissionSize());
    EXPECT_EQ(0, ms.CompleteSize());

    ce.set_type(static_cast<uint32_t>(eCondtionType::kConditionTalkWithNpc));
    MissionSystem::Receive(ce);
    EXPECT_EQ(1, ms.MissionSize());
    EXPECT_EQ(0, ms.CompleteSize());

    ce.set_type(static_cast<uint32_t>(eCondtionType::kConditionCompleteCondition));
    MissionSystem::Receive(ce);
    EXPECT_EQ(1, ms.MissionSize());
    EXPECT_EQ(0, ms.CompleteSize());

    ce.set_type(static_cast<uint32_t>(eCondtionType::kConditionUseItem));
    ce.add_condtion_ids(2);
    MissionSystem::Receive(ce);
    EXPECT_EQ(1, ms.MissionSize());
    EXPECT_EQ(0, ms.CompleteSize());

	ce.set_type(static_cast<uint32_t>(eCondtionType::kConditionLevelUp));
    ce.clear_condtion_ids();
    ce.add_condtion_ids(10);
    MissionSystem::Receive(ce);
    EXPECT_EQ(1, ms.MissionSize());
    EXPECT_EQ(0, ms.CompleteSize());

	ce.set_type(static_cast<uint32_t>(eCondtionType::kConditionInteraction));
	ce.clear_condtion_ids();
	ce.add_condtion_ids(1);
    ce.add_condtion_ids(2);
    MissionSystem::Receive(ce);
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
	const auto player = CreatePlayerMission();
	auto& ms = tls.registry.get<MissionsComp>(player);
	constexpr uint32_t mission_id = 4;
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
    EXPECT_EQ(kRetOK, MissionSystem::Accept(accept_mission_event));
    EXPECT_EQ(1, ms.TypeSetSize());
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    MissionSystem::Receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mission_id));
    EXPECT_TRUE(ms.IsComplete(mission_id));
    EXPECT_EQ(kRetMissionComplete, MissionSystem::Accept(accept_mission_event));
}

TEST(MissionsComp, EventTriggerMutableMission)
{
	const auto player = CreatePlayerMission();
	auto& ms = tls.registry.get<MissionsComp>(player);
	AcceptMissionEvent accept_mission_event;
	constexpr uint32_t mission_id1 = 1;
	constexpr uint32_t mission_id2 = 2;
	accept_mission_event.set_mission_id(mission_id1);
	accept_mission_event.set_entity(entt::to_integral(player));
	EXPECT_EQ(kRetOK, MissionSystem::Accept(accept_mission_event));
	accept_mission_event.set_mission_id(mission_id2);
	EXPECT_EQ(kRetOK, MissionSystem::Accept(accept_mission_event));

	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
    ce.set_amount(4);
    ce.clear_condtion_ids();
	ce.add_condtion_ids(1);
    MissionSystem::Receive(ce);
    ce.clear_condtion_ids();
	ce.add_condtion_ids(2);
    MissionSystem::Receive(ce);
    ce.clear_condtion_ids();
	ce.add_condtion_ids(3);
    MissionSystem::Receive(ce);
    ce.clear_condtion_ids();
	ce.add_condtion_ids(4);	
	MissionSystem::Receive(ce);
    EXPECT_TRUE(ms.IsComplete(mission_id1));
    EXPECT_TRUE(ms.IsComplete(mission_id2));
}

TEST(MissionsComp, OnCompleteMission)
{
	const auto player = CreatePlayerMission();
	auto& ms = tls.registry.get<MissionsComp>(player);
    uint32_t mission_id = 7;
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
    EXPECT_EQ(kRetOK, MissionSystem::Accept(accept_mission_event));
    EXPECT_EQ(1, ms.TypeSetSize());
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    MissionSystem::Receive(ce);
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
        MissionSystem::Receive(ce);
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
	const auto player = CreatePlayerMission();
	auto& ms = tls.registry.get<MissionsComp>(player);
	uint32_t mission_id = 7;
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
	EXPECT_EQ(kRetOK, MissionSystem::Accept(accept_mission_event));
	EXPECT_EQ(1, ms.TypeSetSize());
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	ce.add_condtion_ids(1);
	ce.set_amount(1);
	MissionSystem::Receive(ce);
	EXPECT_FALSE(ms.IsAccepted(mission_id));
	EXPECT_TRUE(ms.IsComplete(mission_id));
	const auto next_mission_id = ++mission_id;
    tls.dispatcher.update<AcceptMissionEvent>();
	EXPECT_TRUE(ms.IsAccepted(next_mission_id));
	EXPECT_FALSE(ms.IsComplete(next_mission_id));
}

TEST(MissionsComp, MissionCondition)
{
	const auto player = CreatePlayerMission();
	auto& ms = tls.registry.get<MissionsComp>(player);
    uint32_t mission_id = 14;
    uint32_t mission_id1 = 15;
    uint32_t mission_id2 = 16;
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
    EXPECT_EQ(kRetOK, MissionSystem::Accept(accept_mission_event));
	AcceptMissionEvent accept_mission_event1;
	accept_mission_event1.set_mission_id(mission_id1);
	accept_mission_event1.set_entity(entt::to_integral(player));
    EXPECT_EQ(kRetOK, MissionSystem::Accept(accept_mission_event1));
	AcceptMissionEvent accept_mission_event2;
	accept_mission_event2.set_mission_id(mission_id2);
	accept_mission_event2.set_entity(entt::to_integral(player));
    EXPECT_EQ(kRetOK, MissionSystem::Accept(accept_mission_event2));

    EXPECT_TRUE(ms.IsAccepted(mission_id));
    EXPECT_FALSE(ms.IsComplete(mission_id));
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    MissionSystem::Receive(ce);
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
	const auto player = CreatePlayerMission();
	auto& ms = tls.registry.get<MissionsComp>(player);
	constexpr uint32_t mission_id = 13;

    AcceptMissionEvent accept_mission_event;
    accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
    EXPECT_EQ(kRetOK, MissionSystem::Accept(accept_mission_event));

    EXPECT_TRUE(ms.IsAccepted(mission_id));
    EXPECT_FALSE(ms.IsComplete(mission_id));
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    MissionSystem::Receive(ce);
    EXPECT_TRUE(ms.IsAccepted(mission_id));
    EXPECT_FALSE(ms.IsComplete(mission_id));
    MissionSystem::Receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mission_id));
    EXPECT_TRUE(ms.IsComplete(mission_id));
}

TEST(MissionsComp, MissionRewardList)
{
	const auto player = CreatePlayerMission();
    auto& ms = tls.registry.get<MissionsComp>(player);
    tls.registry.emplace<MissionRewardPbComp>(player);

	constexpr uint32_t mission_id = 12;

	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
	EXPECT_EQ(kRetOK, MissionSystem::Accept(accept_mission_event));
    GetRewardParam param;
    param.mission_id_ = mission_id;
    param.player_ = player;
    EXPECT_EQ(kRetMissionGetRewardNoMissionId, MissionSystem::GetReward(param));
    EXPECT_TRUE(ms.IsAccepted(mission_id));
    EXPECT_FALSE(ms.IsComplete(mission_id));
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(static_cast<uint32_t>(eCondtionType::kConditionKillMonster));
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    MissionSystem::Receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mission_id));
    EXPECT_TRUE(ms.IsComplete(mission_id));
    EXPECT_EQ(kRetOK, MissionSystem::GetReward(param));
    EXPECT_EQ(kRetMissionGetRewardNoMissionId, MissionSystem::GetReward(param));
    EXPECT_EQ(0, ms.CanGetRewardSize());
}

TEST(MissionsComp, AbandonMission)
{
	const auto player = CreatePlayerMission();
	auto& ms = tls.registry.get<MissionsComp>(player);
    uint32_t mission_id = 12;
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mission_id);
	accept_mission_event.set_entity(entt::to_integral(player));
	EXPECT_EQ(kRetOK, MissionSystem::Accept(accept_mission_event));

    EXPECT_EQ(1, ms.MissionSize());
    EXPECT_EQ(0, ms.CanGetRewardSize());
    EXPECT_EQ(1, ms.TypeSetSize());
    auto& type_missions = ms.classify_for_unittest();

    EXPECT_EQ(1, type_missions.find(static_cast<uint32_t>(eCondtionType::kConditionKillMonster))->second.size());
    tls.registry.emplace_or_replace<MissionRewardPbComp>(ms).mutable_can_reward_mission_id()->insert({ mission_id, true });
    AbandonParam param;
	param.mission_id_ = mission_id;
	param.player_ = player;
    
    MissionSystem::Abandon(param);

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

