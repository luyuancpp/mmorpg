#include <gtest/gtest.h>

#include "src/game_config/condition_config.h"
#include "src/game_config/mission_config.h"
#include "src/game_logic/constants/mission_constants.h"
#include "src/game_logic/missions/missions_base.h"
#include "src/game_logic/missions/mission_system.h"
#include "src/game_logic/tips_id.h"
#include "src/game_logic/missions/missions_base.h"
#include "src/event_handler/mission_event_handler.h"
#include "src/util/random.h"
#include "src/game_logic/tips_id.h"

#include "component_proto/mission_comp.pb.h"
#include "event_proto/mission_event.pb.h"

decltype(auto) CreateMission()
{
	auto player = tls.registry.create();
	auto& ms = tls.registry.emplace<MissionsComp>(player);
    ms.set_event_owner(player);
    MissionEventHandler::Register();
    return &ms;
}

decltype(auto) CreatePlayerMission()
{
	auto player = tls.registry.create();
	auto& ms = tls.registry.emplace<MissionsComp>(player);
	ms.set_event_owner(player);
	MissionEventHandler::Register();
	return player;
}

TEST(MissionsComp, AcceptMission)
{
    uint32_t mid = 1;
    auto& ms = *CreateMission();
	ms.set_check_mission_type_repeated(false);
    AcceptMissionEvent accept_mission_event;
    accept_mission_event.set_mission_id(mid);
    auto& data = mission_config::GetSingleton().all();
    std::size_t sz = 0;
    for (int32_t i = 0; i < data.data_size(); ++i)
    {
        accept_mission_event.set_mission_id(data.data(i).id());
        auto m = ms.Accept(accept_mission_event);
        ++sz;
    }
    EXPECT_EQ(sz, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());
    ms.CompleteAllMission();
    EXPECT_EQ(0, ms.mission_size());
    EXPECT_EQ(sz, ms.complete_size());
}

TEST(MissionsComp, RepeatedMission)
{
    auto& ms = *CreateMission();
    {
        uint32_t mid = 1;
		AcceptMissionEvent accept_mission_event;
		accept_mission_event.set_mission_id(mid);
        EXPECT_EQ(kRetOK, ms.Accept(accept_mission_event));
        EXPECT_EQ(kRetMissionIdRepeated, ms.Accept(accept_mission_event));
    }

    {
		AcceptMissionEvent accept_mission_event1;
        accept_mission_event1.set_mission_id(3);
		AcceptMissionEvent accept_mission_event2;
        accept_mission_event2.set_mission_id(2);
		EXPECT_EQ(kRetOK, ms.Accept(accept_mission_event1));
		EXPECT_EQ(kRetMissionTypeRepeated, ms.Accept(accept_mission_event2));
    }
}

TEST(MissionsComp, TriggerCondition)
{
    auto& ms = *CreateMission();
    uint32_t mid = 1;
    //auto mrow = mission_config::GetSingleton().get(mid);
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mid);
    EXPECT_EQ(kRetOK, ms.Accept(accept_mission_event));
    EXPECT_EQ(1, ms.type_set_size());
    MissionConditionEvent ce;
    ce.set_entity(ms);
    ce.set_type(kConditionKillMonster);
    ce.add_condtion_ids(1);
    ce.set_amount(1);
    ms.Receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.clear_condtion_ids();
    ce.add_condtion_ids(2);
    ms.Receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

	ce.clear_condtion_ids();
	ce.add_condtion_ids(3);
    ms.Receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

	ce.clear_condtion_ids();
	ce.add_condtion_ids(4);
    ms.Receive(ce);
    EXPECT_EQ(0, ms.mission_size());
    EXPECT_EQ(1, ms.complete_size());
    EXPECT_EQ(0, ms.type_set_size());
}

TEST(MissionsComp, TypeSize)
{
    auto& ms = *CreateMission();
	tls.dispatcher.update<AcceptMissionEvent>();
    uint32_t mid = 6;
    //auto mrow = mission_config::GetSingleton().get(mid);
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mid);
    EXPECT_EQ(kRetOK, ms.Accept(accept_mission_event));
    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
    for (uint32_t i = kConditionKillMonster; i < kConditionCustom; ++i)
    {
        EXPECT_EQ(1, ms.classify_for_unittest().find(i)->second.size());
    }    
    
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(kConditionKillMonster);
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    ms.Receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.set_type(kConditionTalkWithNpc);
    ms.Receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.set_type(kConditionCompleteCondition);
    ms.Receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.set_type(kConditionUseItem);
    ce.add_condtion_ids(2);
    ms.Receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

	ce.set_type(kConditionLevelUp);
    ce.clear_condtion_ids();
    ce.add_condtion_ids(10);
    ms.Receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

	ce.set_type(kConditionInteraction);
	ce.clear_condtion_ids();
	ce.add_condtion_ids(1);
    ce.add_condtion_ids(2);
    ms.Receive(ce);
    tls.dispatcher.update<MissionConditionEvent>();
    EXPECT_EQ(0, ms.mission_size());
    EXPECT_EQ(1, ms.complete_size());
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));
    EXPECT_EQ(0, ms.type_set_size());
    for (uint32_t i = kConditionKillMonster; i < kConditionCustom; ++i)
    {
        EXPECT_EQ(0, ms.classify_for_unittest().find(i)->second.size());
    }
}

TEST(MissionsComp, CompleteAcceptMission)
{
    auto& ms = *CreateMission();
    uint32_t mid = 4;
    //auto mrow = mission_config::GetSingleton().get(mid);
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mid);
    EXPECT_EQ(kRetOK, ms.Accept(accept_mission_event));
    EXPECT_EQ(1, ms.type_set_size());
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(kConditionKillMonster);
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    ms.Receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));
    EXPECT_EQ(kRetMissionComplete, ms.Accept(accept_mission_event));
}

TEST(MissionsComp, EventTriggerMutableMission)
{
    auto& ms = *CreateMission();
	AcceptMissionEvent accept_mission_event;
    uint32_t misid1 = 1;
    uint32_t misid2 = 2;
	accept_mission_event.set_mission_id(misid1);
	EXPECT_EQ(kRetOK, ms.Accept(accept_mission_event));
	accept_mission_event.set_mission_id(misid2);
	EXPECT_EQ(kRetOK, ms.Accept(accept_mission_event));

	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(kConditionKillMonster);
    ce.set_amount(4);
    ce.clear_condtion_ids();
	ce.add_condtion_ids(1);
    ms.Receive(ce);
    ce.clear_condtion_ids();
	ce.add_condtion_ids(2);
    ms.Receive(ce);
    ce.clear_condtion_ids();
	ce.add_condtion_ids(3);
    ms.Receive(ce);
    ce.clear_condtion_ids();
	ce.add_condtion_ids(4);	
	ms.Receive(ce);
    EXPECT_TRUE(ms.IsComplete(misid1));
    EXPECT_TRUE(ms.IsComplete(misid2));
}

TEST(MissionsComp, OnCompleteMission)
{
    auto& ms = *CreateMission();
    uint32_t mid = 7;
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mid);
    EXPECT_EQ(kRetOK, ms.Accept(accept_mission_event));
    EXPECT_EQ(1, ms.type_set_size());
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(kConditionKillMonster);
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    ms.Receive(ce);
    tls.dispatcher.update<AcceptMissionEvent>();
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));

    auto next_mission = ++mid;
    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
    for (uint32_t i = kConditionKillMonster; i < kConditionInteraction; ++i)
    {
        ce.clear_condtion_ids();
        ce.add_condtion_ids(i);
        ms.Receive(ce);
        EXPECT_FALSE(ms.IsAccepted(mid));
        EXPECT_TRUE(ms.IsComplete(mid));
        tls.dispatcher.update<AcceptMissionEvent>();
        EXPECT_EQ(0, tls.dispatcher.size<AcceptMissionEvent>());
        EXPECT_TRUE(ms.IsAccepted(++mid));
        EXPECT_FALSE(ms.IsComplete(mid));
    }
}

TEST(MissionsComp, AcceptNextMirroMission)
{
    auto& ms = *CreateMission();
	uint32_t mid = 7;
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mid);
	EXPECT_EQ(kRetOK, ms.Accept(accept_mission_event));
	EXPECT_EQ(1, ms.type_set_size());
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(kConditionKillMonster);
	ce.add_condtion_ids(1);
	ce.set_amount(1);
	ms.Receive(ce);
	EXPECT_FALSE(ms.IsAccepted(mid));
	EXPECT_TRUE(ms.IsComplete(mid));
	auto next_mission_id = ++mid;
    tls.dispatcher.update<AcceptMissionEvent>();
	EXPECT_TRUE(ms.IsAccepted(next_mission_id));
	EXPECT_FALSE(ms.IsComplete(next_mission_id));
}

TEST(MissionsComp, MissionCondition)
{
    auto& ms = *CreateMission();

    uint32_t mid = 14;
    uint32_t mid1 = 15;
    uint32_t mid2 = 16;
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mid);
    EXPECT_EQ(kRetOK, ms.Accept(accept_mission_event));
	AcceptMissionEvent accept_mission_event1;
	accept_mission_event1.set_mission_id(mid1);
    EXPECT_EQ(kRetOK, ms.Accept(accept_mission_event1));
	AcceptMissionEvent accept_mission_event2;
	accept_mission_event2.set_mission_id(mid2);
    EXPECT_EQ(kRetOK, ms.Accept(accept_mission_event2));

    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(kConditionKillMonster);
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    ms.Receive(ce);
    tls.dispatcher.update<AcceptMissionEvent>();
    tls.dispatcher.update<MissionConditionEvent>();
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));
    EXPECT_FALSE(ms.IsAccepted(mid1));
    EXPECT_TRUE(ms.IsComplete(mid1));
    EXPECT_FALSE(ms.IsAccepted(mid2));
    EXPECT_TRUE(ms.IsComplete(mid2));
}

TEST(MissionsComp, ConditionAmount)
{
    auto& ms = *CreateMission();
    uint32_t mid = 13;

    AcceptMissionEvent accept_mission_event;
    accept_mission_event.set_mission_id(mid);
    EXPECT_EQ(kRetOK, ms.Accept(accept_mission_event));

    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(kConditionKillMonster);
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    ms.Receive(ce);
    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
    ms.Receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));
}

TEST(MissionsComp, MissionRewardList)
{
    auto player = CreatePlayerMission();
    auto& ms = tls.registry.get<MissionsComp>(player);
    tls.registry.emplace<MissionRewardPbComp>(player);

    uint32_t mid = 12;

	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mid);
	EXPECT_EQ(kRetOK, ms.Accept(accept_mission_event));
    GetRewardParam param;
    param.mission_id_ = mid;
    param.player_ = player;
    EXPECT_EQ(kRetMissionGetRewardNoMissionId, MissionSystem::GetReward(param));
    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(kConditionKillMonster);
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    ms.Receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));
    EXPECT_EQ(kRetOK, ms.GetReward(mid));
    EXPECT_EQ(kRetMissionGetRewardNoMissionId, ms.GetReward(mid));
    EXPECT_EQ(0, ms.can_reward_size());
}

TEST(MissionsComp, RemoveMission)
{
    auto& ms = *CreateMission();
    uint32_t mid = 12;
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mid);
	EXPECT_EQ(kRetOK, ms.Accept(accept_mission_event));

    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.can_reward_size());
    EXPECT_EQ(1, ms.type_set_size());
    auto& type_missions = ms.classify_for_unittest();

    EXPECT_EQ(1, type_missions.find(kConditionKillMonster)->second.size());
    tls.registry.emplace_or_replace<MissionRewardPbComp>(ms).mutable_can_reward_mission_id()->insert({ mid, true });
    ms.Abandon(mid);

    EXPECT_EQ(0, ms.mission_size());
    EXPECT_EQ(0, ms.can_reward_size());
    EXPECT_EQ(0, ms.type_set_size());
    EXPECT_EQ(0, type_missions.find(kConditionKillMonster)->second.size());
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

