#include <gtest/gtest.h>

#include "src/game_config/condition_config.h"
#include "src/game_config/mission_config.h"
#include "src/game_logic/comp/mission.hpp"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/op_code.h"
#include "src/game_logic/missions/missions.h"
#include "src/random/random.h"
#include "src/return_code/error_code.h"

#include "comp.pb.h"

using namespace common;

TEST(Missions, MakeMission)
{
    uint32_t mid = 1;
    Missions ms;
    reg().remove<CheckSubType>(ms.entity());
    MakeMissionParam param{ 
    mid,
        mission_config::GetSingleton().key_id(mid)->condition_id()};
    
    auto& data = mission_config::GetSingleton().all();

    std::size_t s = 0;
    for (int32_t i = 0; i < data.data_size(); ++i)
    {
        auto id = data.data(i).id();
        param.mission_id_ = id;
        param.condition_id_ = &mission_config::GetSingleton().key_id(id)->condition_id();
        auto m = ms.MakeMission(param);
        ++s;
    }

    EXPECT_EQ(s, ms.mission_size());
    EXPECT_EQ(0, ms.completemission_size());
    ms.CompleteAllMission();
    EXPECT_EQ(0, ms.mission_size());
    EXPECT_EQ(s, ms.completemission_size());
}

TEST(Missions, RadomCondtion)
{
    uint32_t mid = 3;
    Missions ms;
    MakePlayerMissionParam param{ mid};    
    auto cids = mission_config::GetSingleton().key_id(mid);    
    RandomMision(param, ms);
    auto& missions = ms.missions();
    auto it =  std::find(cids->random_condition_pool().begin(), cids->random_condition_pool().end(),
        missions.missions().find(mid)->second.conditions(0).id());
    EXPECT_TRUE(it != cids->random_condition_pool().end());
    EXPECT_EQ(1, missions.missions().find(mid)->second.conditions_size());
}

TEST(Missions, RepeatedMission)
{
    Missions ms;
    {
        uint32_t mid = 1;
        MakeMissionParam param{ 
    mid,
        mission_config::GetSingleton().key_id(mid)->condition_id()};
        EXPECT_EQ(RET_OK, ms.MakeMission(param));
        EXPECT_EQ(RET_MISSION_ID_REPTEATED, ms.MakeMission(param));
    }

    {
        MakePlayerMissionParam param{  3};
        MakePlayerMissionParam param2{  2};
        EXPECT_EQ(RET_OK, RandomMision(param, ms));
        EXPECT_EQ(RET_MISSION_TYPE_REPTEATED, RandomMision(param2, ms));
    }
}

TEST(Missions, TriggerCondition)
{
    Missions ms;
    uint32_t mid = 1;
    //auto mrow = mission_config::GetSingleton().key_id(mid);
    MakePlayerMissionParam param{mid};
    EXPECT_EQ(RET_OK, RandomMision(param, ms));
    EXPECT_EQ(1, ms.type_set_size());
    ConditionEvent ce{ E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.TriggerConditionEvent(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.completemission_size());

    ce.condtion_ids_ = { 2 };
    ms.TriggerConditionEvent(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.completemission_size());

    ce.condtion_ids_ = { 3 };
    ms.TriggerConditionEvent(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.completemission_size());

    ce.condtion_ids_ = { 4 };
    ms.TriggerConditionEvent(ce);
    EXPECT_EQ(0, ms.mission_size());
    EXPECT_EQ(1, ms.completemission_size());
    EXPECT_EQ(0, ms.type_set_size());
}

TEST(Missions, TypeSize)
{
    Missions ms;
    uint32_t mid = 6;
    //auto mrow = mission_config::GetSingleton().key_id(mid);
    MakePlayerMissionParam param{mid};
    EXPECT_EQ(RET_OK, RandomMision(param, ms));
    EXPECT_TRUE(ms.IsAcceptedMission(mid));
    EXPECT_FALSE(ms.IsCompleteMission(mid));
    for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_COMSTUM; ++i)
    {
        EXPECT_EQ(1, ms.type_mission_id().find(i)->second.size());
    }    
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.TriggerConditionEvent(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.completemission_size());

    ce.condition_type_ = E_CONDITION_TALK_WITH_NPC;
    ce.condtion_ids_ = { 1 };
    ms.TriggerConditionEvent(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.completemission_size());

    ce.condition_type_ = E_CONDITION_COMPLELETE_CONDITION;
    ms.TriggerConditionEvent(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.completemission_size());

    ce.condition_type_ = E_CONDITION_USE_ITEM;
    ms.TriggerConditionEvent(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.completemission_size());

    ce.condition_type_ = E_CONDITION_LEVEUP;
    ce.condtion_ids_ = { 10 };
    ms.TriggerConditionEvent(ce);
    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.completemission_size());

    ce.condition_type_ = E_CONDITION_INTERATION;
    ce.condtion_ids_ = { 1};
    ms.TriggerConditionEvent(ce);
    
    EXPECT_EQ(0, ms.mission_size());
    EXPECT_EQ(1, ms.completemission_size());
    EXPECT_FALSE(ms.IsAcceptedMission(mid));
    EXPECT_TRUE(ms.IsCompleteMission(mid));
    EXPECT_EQ(0, ms.type_set_size());
    for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_COMSTUM; ++i)
    {
        EXPECT_EQ(0, ms.type_mission_id().find(i)->second.size());
    }
}

TEST(Missions, CompleteRemakeMission)
{
    Missions ms;
    uint32_t mid = 4;
    //auto mrow = mission_config::GetSingleton().key_id(mid);
    MakePlayerMissionParam param{  mid};
    EXPECT_EQ(RET_OK, RandomMision(param, ms));
    EXPECT_EQ(1, ms.type_set_size());
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.TriggerConditionEvent(ce);
    EXPECT_FALSE(ms.IsAcceptedMission(mid));
    EXPECT_TRUE(ms.IsCompleteMission(mid));
    EXPECT_EQ(RET_MISSION_COMPLETE, RandomMision(param, ms));
}

TEST(Missions, OnCompleteMission)
{
    Missions ms;
    uint32_t mid = 7;

    MakePlayerMissionParam param{mid};
    EXPECT_EQ(RET_OK, RandomMision(param, ms));
    EXPECT_EQ(1, ms.type_set_size());
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.TriggerConditionEvent(ce);
    EXPECT_FALSE(ms.IsAcceptedMission(mid));
    EXPECT_TRUE(ms.IsCompleteMission(mid));

    auto next_mission = ++mid;
    EXPECT_TRUE(ms.IsAcceptedMission(mid));
    EXPECT_FALSE(ms.IsCompleteMission(mid));
    for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_INTERATION; ++i)
    {
        ce.condtion_ids_ = { i };
        ms.TriggerConditionEvent(ce);
        EXPECT_FALSE(ms.IsAcceptedMission(mid));
        EXPECT_TRUE(ms.IsCompleteMission(mid));

        auto next_mission = ++mid;
        EXPECT_TRUE(ms.IsAcceptedMission(mid));
        EXPECT_FALSE(ms.IsCompleteMission(mid));
    }
}

TEST(Missions, AcceptNextMirroMission)
{
    Missions ms;
    uint32_t mid = 7;
    MakePlayerMissionParam param{mid};
    auto& next_mission_set =  reg().emplace<NextTimeAcceptMission>(ms.entity());
    EXPECT_EQ(RET_OK, RandomMision(param, ms));
    EXPECT_EQ(1, ms.type_set_size());
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.TriggerConditionEvent(ce);
    EXPECT_FALSE(ms.IsAcceptedMission(mid));
    EXPECT_TRUE(ms.IsCompleteMission(mid));

    auto next_mission_id = ++mid;
    EXPECT_FALSE(ms.IsAcceptedMission(next_mission_id));
    EXPECT_FALSE(ms.IsCompleteMission(next_mission_id));
    EXPECT_TRUE(next_mission_set.next_time_accept_mission_id_.find(next_mission_id)
        != next_mission_set.next_time_accept_mission_id_.end());
}

TEST(Missions, MissionCondition)
{
    Missions ms;

    uint32_t mid = 14;
    uint32_t mid1 = 15;
    uint32_t mid2 = 16;
    MakePlayerMissionParam param{mid};
    EXPECT_EQ(RET_OK, RandomMision(param, ms));
    param.mission_id_ = mid1;
    EXPECT_EQ(RET_OK, RandomMision(param, ms));
    param.mission_id_ = mid2;
    EXPECT_EQ(RET_OK, RandomMision(param, ms));

    EXPECT_TRUE(ms.IsAcceptedMission(mid));
    EXPECT_FALSE(ms.IsCompleteMission(mid));
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.TriggerConditionEvent(ce);
    EXPECT_FALSE(ms.IsAcceptedMission(mid));
    EXPECT_TRUE(ms.IsCompleteMission(mid));
    EXPECT_FALSE(ms.IsAcceptedMission(mid1));
    EXPECT_TRUE(ms.IsCompleteMission(mid1));
    EXPECT_FALSE(ms.IsAcceptedMission(mid2));
    EXPECT_TRUE(ms.IsCompleteMission(mid2));
}

TEST(Missions, ConditionAmount)
{
    Missions ms;

    uint32_t mid = 13;

    MakePlayerMissionParam param{mid};
    EXPECT_EQ(RET_OK, RandomMision(param, ms));

    EXPECT_TRUE(ms.IsAcceptedMission(mid));
    EXPECT_FALSE(ms.IsCompleteMission(mid));
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.TriggerConditionEvent(ce);
    EXPECT_TRUE(ms.IsAcceptedMission(mid));
    EXPECT_FALSE(ms.IsCompleteMission(mid));
    ms.TriggerConditionEvent(ce);
    EXPECT_FALSE(ms.IsAcceptedMission(mid));
    EXPECT_TRUE(ms.IsCompleteMission(mid));
}

TEST(Missions, MissionRewardList)
{
    Missions ms;

    reg().emplace<MissionReward>(ms.entity());

    uint32_t mid = 12;

    MakePlayerMissionParam param{mid};
    EXPECT_EQ(RET_OK, RandomMision(param, ms));
    EXPECT_EQ(RET_MISSION_GET_REWARD_NO_MISSION_ID, ms.GetMissionReward(mid));
    EXPECT_TRUE(ms.IsAcceptedMission(mid));
    EXPECT_FALSE(ms.IsCompleteMission(mid));
    ConditionEvent ce{  E_CONDITION_KILL_MONSTER, {1}, 1 };
    ms.TriggerConditionEvent(ce);
    EXPECT_FALSE(ms.IsAcceptedMission(mid));
    EXPECT_TRUE(ms.IsCompleteMission(mid));
    EXPECT_EQ(RET_OK, ms.GetMissionReward(mid));
    EXPECT_EQ(RET_MISSION_GET_REWARD_NO_MISSION_ID, ms.GetMissionReward(mid));
    EXPECT_EQ(0, ms.can_reward_mission_id_size());
}

TEST(Missions, RemoveMission)
{
    Missions ms;
    uint32_t mid = 12;
    MakePlayerMissionParam param{mid};
    EXPECT_EQ(RET_OK, RandomMision(param, ms));

    EXPECT_EQ(1, ms.mission_size());
    EXPECT_EQ(0, ms.can_reward_mission_id_size());
    EXPECT_EQ(1, ms.type_set_size());
    auto& type_missions = ms.type_mission_id();

    EXPECT_EQ(1, type_missions.find(E_CONDITION_KILL_MONSTER)->second.size());
    auto& cm = ms.complete_ids();
    ((CompleteMissionsId&)cm).mutable_can_reward_mission_id()->insert({ mid, true });
    ms.RemoveMission(mid);

    EXPECT_EQ(0, ms.mission_size());
    EXPECT_EQ(0, ms.can_reward_mission_id_size());
    EXPECT_EQ(0, ms.type_set_size());
    EXPECT_EQ(0, type_missions.find(E_CONDITION_KILL_MONSTER)->second.size());
}

TEST(Missions, MissionAutoReward)
{
}

TEST(Missions, MissionTimeOut)
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

