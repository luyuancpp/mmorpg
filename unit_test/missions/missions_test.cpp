#include <gtest/gtest.h>

#include "src/game_config/condition_config.h"
#include "src/game_config/mission_config.h"
#include "src/game_logic/constants/mission_constants.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/op_code.h"
#include "src/game_logic/missions/missions_base.h"
#include "src/util/random.h"
#include "src/game_logic/tips_id.h"

#include "comp.pb.h"
#include "event_proto/mission_event.pb.h"

decltype(auto) CreateMission()
{
	auto player = registry.create();
	auto& ms = registry.emplace<MissionsComp>(player);
    ms.set_event_owner(player);
    registry.emplace<entt::dispatcher>(player);
    ms.Init();	
    return &ms;
}

TEST(MissionsComp, AcceptMission)
{
    uint32_t mid = 1;
    auto& ms = *CreateMission();
    registry.remove<CheckTypeRepeatd>(ms);
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
		EXPECT_EQ(kRetMisionTypeRepeated, ms.Accept(accept_mission_event2));
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
    ce.set_type(E_CONDITION_KILL_MONSTER);
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
	auto& dispatcher = registry.get<entt::dispatcher>(ms);
	dispatcher.update<AcceptMissionEvent>();
    uint32_t mid = 6;
    //auto mrow = mission_config::GetSingleton().get(mid);
	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mid);
    EXPECT_EQ(kRetOK, ms.Accept(accept_mission_event));
    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
    for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_COMSTUM; ++i)
    {
        EXPECT_EQ(1, ms.classify_for_unittest().find(i)->second.size());
    }    
    
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(E_CONDITION_KILL_MONSTER);
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    ms.Receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.set_type(E_CONDITION_TALK_WITH_NPC);
    ms.Receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.set_type(E_CONDITION_COMPLELETE_CONDITION);
    ms.Receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.set_type(E_CONDITION_USE_ITEM);
    ce.add_condtion_ids(2);
    ms.Receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

	ce.set_type(E_CONDITION_LEVEUP);
    ce.clear_condtion_ids();
    ce.add_condtion_ids(10);
    ms.Receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

	ce.set_type(E_CONDITION_INTERATION);
	ce.clear_condtion_ids();
	ce.add_condtion_ids(1);
    ce.add_condtion_ids(2);
    ms.Receive(ce);
    dispatcher.update<MissionConditionEvent>();
    EXPECT_EQ(0, ms.mission_size());
    EXPECT_EQ(1, ms.complete_size());
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));
    EXPECT_EQ(0, ms.type_set_size());
    for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_COMSTUM; ++i)
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
	ce.set_type(E_CONDITION_KILL_MONSTER);
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    ms.Receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));
    EXPECT_EQ(kRetMissionComplete, ms.Accept(accept_mission_event));
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
	ce.set_type(E_CONDITION_KILL_MONSTER);
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    ms.Receive(ce);
    auto& dispatcher = registry.get<entt::dispatcher>(ms);
    dispatcher.update<AcceptMissionEvent>();
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));

    auto next_mission = ++mid;
    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
    for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_INTERATION; ++i)
    {
        ce.clear_condtion_ids();
        ce.add_condtion_ids(i);
        ms.Receive(ce);
        EXPECT_FALSE(ms.IsAccepted(mid));
        EXPECT_TRUE(ms.IsComplete(mid));
        dispatcher.update<AcceptMissionEvent>();
        EXPECT_EQ(0, dispatcher.size<AcceptMissionEvent>());
        EXPECT_TRUE(ms.IsAccepted(++mid));
        EXPECT_FALSE(ms.IsComplete(mid));
    }
}

TEST(MissionsComp, AcceptNextMirroMission)
{
   /* MissionsComp ms(registry.create());
    registry.emplace<entt::dispatcher>(ms);
    uint32_t mid = 7;
    auto& next_mission_set =  registry.emplace<NextTimeAcceptMission>(ms);
    AcceptMissionP param{ mid };
    EXPECT_EQ(kRetOK, ms.Accept(param));
    EXPECT_EQ(1, ms.type_set_size());
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.Receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));

    auto next_mission_id = ++mid;
    EXPECT_FALSE(ms.IsAccepted(next_mission_id));
    EXPECT_FALSE(ms.IsComplete(next_mission_id));
    EXPECT_TRUE(next_mission_set.next_time_accept_mission_id_.find(next_mission_id)
        != next_mission_set.next_time_accept_mission_id_.end());*/
}

TEST(MissionsComp, MissionCondition)
{
    auto& ms = *CreateMission();
	auto& dispatcher = registry.get<entt::dispatcher>(ms);
	
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
	ce.set_type(E_CONDITION_KILL_MONSTER);
	ce.add_condtion_ids(1);
	ce.set_amount(1);
    ms.Receive(ce);
    dispatcher.update<AcceptMissionEvent>();
    dispatcher.update<MissionConditionEvent>();
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
	ce.set_type(E_CONDITION_KILL_MONSTER);
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
    auto& ms = *CreateMission();
    registry.emplace<MissionRewardPbComp>(ms);

    uint32_t mid = 12;

	AcceptMissionEvent accept_mission_event;
	accept_mission_event.set_mission_id(mid);
	EXPECT_EQ(kRetOK, ms.Accept(accept_mission_event));
    EXPECT_EQ(kRetMissionGetRewardNoMissionId, ms.GetReward(mid));
    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
	MissionConditionEvent ce;
	ce.set_entity(ms);
	ce.set_type(E_CONDITION_KILL_MONSTER);
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

    EXPECT_EQ(1, type_missions.find(E_CONDITION_KILL_MONSTER)->second.size());
    registry.emplace_or_replace<MissionRewardPbComp>(ms).mutable_can_reward_mission_id()->insert({ mid, true });
    ms.Abandon(mid);

    EXPECT_EQ(0, ms.mission_size());
    EXPECT_EQ(0, ms.can_reward_size());
    EXPECT_EQ(0, ms.type_set_size());
    EXPECT_EQ(0, type_missions.find(E_CONDITION_KILL_MONSTER)->second.size());
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

