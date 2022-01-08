#include <gtest/gtest.h>

#include "src/game_config/condition_config.h"
#include "src/game_config/mission_config.h"
#include "src/game_logic/comp/mission_comp.hpp"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/op_code.h"
#include "src/game_logic/missions/missions_base.h"
#include "src/game_logic/missions/player_mission.h"
#include "src/random/random.h"
#include "src/return_code/error_code.h"

#include "comp.pb.h"

using namespace common;

TEST(MissionsComp, AcceptMission)
{
    uint32_t mid = 1;
    MissionsComp ms;
    reg.remove<CheckSubType>(ms.entity());
    AcceptMissionBaseP param{mid,mission_config::GetSingleton().get(mid)->condition_id()};
    auto& data = mission_config::GetSingleton().all();
    std::size_t sz = 0;
    for (int32_t i = 0; i < data.data_size(); ++i)
    {
        auto id = data.data(i).id();
        param.mission_id_ = id;
        param.conditions_id_ = &mission_config::GetSingleton().get(id)->condition_id();
        auto m = ms.Accept(param);
        ++sz;
    }

    EXPECT_EQ(sz, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());
    ms.CompleteAllMission();
    EXPECT_EQ(0, ms.mission_size());
    EXPECT_EQ(sz, ms.complete_size());

    
}

TEST(MissionsComp, RadomCondtion1)
{
    std::unordered_map<uint32_t, MissionsComp> tv;
    tv.emplace(1, MissionsComp());
}

TEST(MissionsComp, RadomCondtion)
{
    uint32_t mid = 3;
    MissionsComp ms;
    AcceptPlayerRandomMissionP param{ mid};    
    auto cids = mission_config::GetSingleton().get(mid);    
    RandomMision(param, ms);
    auto& missions = ms.missions();
    auto it =  std::find(cids->random_condition_pool().begin(), cids->random_condition_pool().end(),
        missions.missions().find(mid)->second.conditions(0).id());
    EXPECT_TRUE(it != cids->random_condition_pool().end());
    EXPECT_EQ(1, missions.missions().find(mid)->second.conditions_size());
}

TEST(MissionsComp, RepeatedMission)
{
    MissionsComp ms;
    {
        uint32_t mid = 1;
        AcceptMissionP param{mid};
        EXPECT_EQ(RET_OK, ms.Accept(param));
        EXPECT_EQ(RET_MISSION_ID_REPTEATED, ms.Accept(param));
    }

    {
        AcceptPlayerRandomMissionP param{3};
        AcceptMissionP param2{2};
        EXPECT_EQ(RET_OK, RandomMision(param, ms));
        EXPECT_EQ(RET_MISSION_TYPE_REPTEATED, ms.Accept(param2));
    }
}

TEST(MissionsComp, TriggerCondition)
{
    MissionsComp ms;
    uint32_t mid = 1;
    //auto mrow = mission_config::GetSingleton().get(mid);
    AcceptMissionP param{mid};
    EXPECT_EQ(RET_OK, ms.Accept(param));
    EXPECT_EQ(1, ms.type_set_size());
    ConditionEvent ce{ E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.match_condtion_ids_ = { 2 };
    ms.receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.match_condtion_ids_ = { 3 };
    ms.receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.match_condtion_ids_ = { 4 };
    ms.receive(ce);
    EXPECT_EQ(0, ms.mission_size());
    EXPECT_EQ(1, ms.complete_size());
    EXPECT_EQ(0, ms.type_set_size());
}

TEST(MissionsComp, TypeSize)
{
    MissionsComp ms;
    uint32_t mid = 6;
    //auto mrow = mission_config::GetSingleton().get(mid);
    AcceptMissionP param{ mid };
    EXPECT_EQ(RET_OK, ms.Accept(param));
    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
    for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_COMSTUM; ++i)
    {
        EXPECT_EQ(1, ms.classify_for_unittest().find(i)->second.size());
    }    
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.type_ = E_CONDITION_TALK_WITH_NPC;
    ce.match_condtion_ids_ = { 1 };
    ms.receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.type_ = E_CONDITION_COMPLELETE_CONDITION;
    ms.receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.type_ = E_CONDITION_USE_ITEM;
    ms.receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.type_ = E_CONDITION_LEVEUP;
    ce.match_condtion_ids_ = { 10 };
    ms.receive(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.complete_size());

    ce.type_ = E_CONDITION_INTERATION;
    ce.match_condtion_ids_ = { 1};
    ms.receive(ce);
    
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
    MissionsComp ms;
    uint32_t mid = 4;
    //auto mrow = mission_config::GetSingleton().get(mid);
    AcceptMissionP param{ mid };
    EXPECT_EQ(RET_OK, ms.Accept(param));
    EXPECT_EQ(1, ms.type_set_size());
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));
    EXPECT_EQ(RET_MISSION_COMPLETE, ms.Accept(param));
}

TEST(MissionsComp, OnCompleteMission)
{
    MissionsComp ms;
    uint32_t mid = 7;

    AcceptMissionP param{ mid };
    EXPECT_EQ(RET_OK, ms.Accept(param));
    EXPECT_EQ(1, ms.type_set_size());
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));

    auto next_mission = ++mid;
    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
    for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_INTERATION; ++i)
    {
        ce.match_condtion_ids_ = { i };
        ms.receive(ce);
        EXPECT_FALSE(ms.IsAccepted(mid));
        EXPECT_TRUE(ms.IsComplete(mid));

        auto next_mission = ++mid;
        EXPECT_TRUE(ms.IsAccepted(mid));
        EXPECT_FALSE(ms.IsComplete(mid));
    }
}

TEST(MissionsComp, AcceptNextMirroMission)
{
    MissionsComp ms;
    uint32_t mid = 7;
    auto& next_mission_set =  reg.emplace<NextTimeAcceptMission>(ms.entity());
    AcceptMissionP param{ mid };
    EXPECT_EQ(RET_OK, ms.Accept(param));
    EXPECT_EQ(1, ms.type_set_size());
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));

    auto next_mission_id = ++mid;
    EXPECT_FALSE(ms.IsAccepted(next_mission_id));
    EXPECT_FALSE(ms.IsComplete(next_mission_id));
    EXPECT_TRUE(next_mission_set.next_time_accept_mission_id_.find(next_mission_id)
        != next_mission_set.next_time_accept_mission_id_.end());
}

TEST(MissionsComp, MissionCondition)
{
    MissionsComp ms;

    uint32_t mid = 14;
    uint32_t mid1 = 15;
    uint32_t mid2 = 16;
    AcceptMissionP param{ mid };
    EXPECT_EQ(RET_OK, ms.Accept(param));
    AcceptMissionP param1{ mid1 };
    EXPECT_EQ(RET_OK, ms.Accept(param1));
    AcceptMissionP param2{ mid2 };
    EXPECT_EQ(RET_OK, ms.Accept(param2));

    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));
    EXPECT_FALSE(ms.IsAccepted(mid1));
    EXPECT_TRUE(ms.IsComplete(mid1));
    EXPECT_FALSE(ms.IsAccepted(mid2));
    EXPECT_TRUE(ms.IsComplete(mid2));
}

TEST(MissionsComp, ConditionAmount)
{
    MissionsComp ms;

    uint32_t mid = 13;

    AcceptMissionP param{ mid };
    EXPECT_EQ(RET_OK, ms.Accept(param));

    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.receive(ce);
    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
    ms.receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));
}

TEST(MissionsComp, MissionRewardList)
{
    MissionsComp ms;

    reg.emplace<MissionReward>(ms.entity());

    uint32_t mid = 12;

    AcceptMissionP param{ mid };
    EXPECT_EQ(RET_OK, ms.Accept(param));
    EXPECT_EQ(RET_MISSION_GET_REWARD_NO_MISSION_ID, ms.GetReward(mid));
    EXPECT_TRUE(ms.IsAccepted(mid));
    EXPECT_FALSE(ms.IsComplete(mid));
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.receive(ce);
    EXPECT_FALSE(ms.IsAccepted(mid));
    EXPECT_TRUE(ms.IsComplete(mid));
    EXPECT_EQ(RET_OK, ms.GetReward(mid));
    EXPECT_EQ(RET_MISSION_GET_REWARD_NO_MISSION_ID, ms.GetReward(mid));
    EXPECT_EQ(0, ms.can_reward_size());
}

TEST(MissionsComp, RemoveMission)
{
    MissionsComp ms;
    uint32_t mid = 12;
    AcceptMissionP param{ mid };
    EXPECT_EQ(RET_OK, ms.Accept(param));

    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.can_reward_size());
    EXPECT_EQ(1, ms.type_set_size());
    auto& type_missions = ms.classify_for_unittest();

    EXPECT_EQ(1, type_missions.find(E_CONDITION_KILL_MONSTER)->second.size());
    auto& cm = ms.complete_ids();
    ((CompleteMissionsId&)cm).mutable_can_reward_mission_id()->insert({ mid, true });
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

int main(int argc, char** argv)
{
    Random::GetSingleton();
    condition_config::GetSingleton().load();
    mission_config::GetSingleton().load();
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

