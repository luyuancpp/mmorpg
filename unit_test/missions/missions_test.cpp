﻿#include <gtest/gtest.h>

#include "src/game_config/generator/json_cpp/mission_json.h"
#include "src/game_config/generator/json_cpp/condition_json.h"
#include "src/game_logic/comp/mission.hpp"
#include "src/game_logic/factories/mission_factories.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/op_code.h"
#include "src/game_logic/sys/mission_sys.hpp"
#include "src/random/random.h"
#include "src/return_code/return_notice_code.h"

#include "comp.pb.h"

using namespace common;

TEST(Missions, MakeMission)
{
    uint32_t mid = 1;
    auto mm = MakePlayerMissionMap();
    MakeMissionParam param{ mm,
        mid,
        MissionJson::GetSingleton().Primary1KeyRow(mid)->condition_id(), 
        E_OP_CODE_TEST };
    
    std::size_t s = 0;
    auto lmake_mission = [ &s, &param](uint32_t id)-> void
    {
        param.mission_id_ = id;
        param.condition_id_ = &MissionJson::GetSingleton().Primary1KeyRow(id)->condition_id();
        auto m = MakeMission(param);
        ++s;
    };
    
    MissionJson::GetSingleton().IdListCallback(lmake_mission);

    EXPECT_EQ(s, reg().get<MissionMap>(mm).missions().size());
    EXPECT_EQ(0, reg().get<CompleteMissionsId>(mm).missions_size());
    CompleteAllMission(mm, E_OP_CODE_TEST);
    EXPECT_EQ(0, reg().view<MissionMap>().size());
    EXPECT_EQ(s, reg().get<CompleteMissionsId>(mm).missions_size());
}

TEST(Missions, RadomCondtion)
{
    uint32_t mid = 3;
    auto mm = MakePlayerMissionMap();
    MakePlayerMissionParam param{mm, mid,  E_OP_CODE_TEST };    
    auto cids = MissionJson::GetSingleton().Primary1KeyRow(mid);    
    MakePlayerMission(param);
    auto& missions = reg().get<MissionMap>(mm).missions();
    auto it =  std::find(cids->random_condition_pool().begin(), cids->random_condition_pool().end(),
        missions.find(mid)->second.conditions(0).id());
    EXPECT_TRUE(it != cids->random_condition_pool().end());
    EXPECT_EQ(1, missions.find(mid)->second.conditions_size());
    reg().clear();
}

TEST(Missions, RepeatedMission)
{
    auto mm = MakePlayerMissionMap();
    {
        uint32_t mid = 1;
        MakeMissionParam param{ mm,
        mid,
        MissionJson::GetSingleton().Primary1KeyRow(mid)->condition_id(), E_OP_CODE_TEST };
        EXPECT_EQ(RET_OK, MakeMission(param));
        EXPECT_EQ(RET_MISSION_ID_REPTEATED, MakeMission(param));
    }

    {
        MakePlayerMissionParam param{ mm, 3,  E_OP_CODE_TEST };
        MakePlayerMissionParam param2{ mm, 2,  E_OP_CODE_TEST };
        EXPECT_EQ(RET_OK, MakePlayerMission(param));
        EXPECT_EQ(RET_MISSION_TYPE_REPTEATED, MakePlayerMission(param2));
    }
    reg().clear();
}

TEST(Missions, TriggerCondition)
{
    auto mm = MakePlayerMissionMap();
    uint32_t mid = 1;
    //auto mrow = MissionJson::GetSingleton().Primary1KeyRow(mid);
    MakePlayerMissionParam param{ mm,   mid,  E_OP_CODE_TEST };
    EXPECT_EQ(RET_OK, MakePlayerMission(param));
    EXPECT_EQ(1, reg().get<UI32PairSet>(mm).size());
    ConditionEvent ce{ mm, E_CONDITION_KILL_MONSTER, {1}, 1 };
    TriggerConditionEvent(ce);
    EXPECT_EQ(1, reg().get<MissionMap>(mm).missions().size());
    EXPECT_EQ(0, reg().get<CompleteMissionsId>(mm).missions_size());

    ce.condtion_ids_ = { 2 };
    TriggerConditionEvent(ce);
    EXPECT_EQ(1, reg().get<MissionMap>(mm).missions().size());
    EXPECT_EQ(0, reg().get<CompleteMissionsId>(mm).missions_size());

    ce.condtion_ids_ = { 3 };
    TriggerConditionEvent(ce);
    EXPECT_EQ(1, reg().get<MissionMap>(mm).missions().size());
    EXPECT_EQ(0, reg().get<CompleteMissionsId>(mm).missions_size());

    ce.condtion_ids_ = { 4 };
    TriggerConditionEvent(ce);
    EXPECT_EQ(0, reg().get<MissionMap>(mm).missions().size());
    EXPECT_EQ(1, reg().get<CompleteMissionsId>(mm).missions_size());
    EXPECT_EQ(0, reg().get<UI32PairSet>(mm).size());
    reg().clear();
}

TEST(Missions, TypeSize)
{
    auto mm = MakePlayerMissionMap();
    uint32_t mid = 6;
    //auto mrow = MissionJson::GetSingleton().Primary1KeyRow(mid);
    MakePlayerMissionParam param{ mm,   mid,  E_OP_CODE_TEST };
    EXPECT_EQ(RET_OK, MakePlayerMission(param));
    EXPECT_TRUE(IsAcceptedMission({ mm, mid }));
    EXPECT_FALSE(IsCompleteMission({ mm, mid }));
    auto& type_misison = reg().get<TypeMissionIdMap>(mm);
    for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_COMSTUM; ++i)
    {
        EXPECT_EQ(1, type_misison[i].size());
    }    
    ConditionEvent ce{ mm, E_CONDITION_KILL_MONSTER, {1}, 1 };
    TriggerConditionEvent(ce);
    EXPECT_EQ(1, reg().get<MissionMap>(mm).missions().size());
    EXPECT_EQ(0, reg().get<CompleteMissionsId>(mm).missions_size());

    ce.condition_type_ = E_CONDITION_TALK_WITH_NPC;
    ce.condtion_ids_ = { 1 };
    TriggerConditionEvent(ce);
    EXPECT_EQ(1, reg().get<MissionMap>(mm).missions().size());
    EXPECT_EQ(0, reg().get<CompleteMissionsId>(mm).missions_size());

    ce.condition_type_ = E_CONDITION_COMPLELETE_CONDITION;
    TriggerConditionEvent(ce);
    EXPECT_EQ(1, reg().get<MissionMap>(mm).missions().size());
    EXPECT_EQ(0, reg().get<CompleteMissionsId>(mm).missions_size());

    ce.condition_type_ = E_CONDITION_USE_ITEM;
    TriggerConditionEvent(ce);

    ce.condition_type_ = E_CONDITION_LEVEUP;
    ce.condtion_ids_ = { 10 };
    TriggerConditionEvent(ce);

    ce.condition_type_ = E_CONDITION_INTERATION;
    ce.condtion_ids_ = { 1};
    TriggerConditionEvent(ce);
    
    EXPECT_EQ(0, reg().get<MissionMap>(mm).missions().size());
    EXPECT_EQ(1, reg().get<CompleteMissionsId>(mm).missions_size());
    EXPECT_FALSE(IsAcceptedMission({ mm, mid }));
    EXPECT_TRUE(IsCompleteMission({ mm, mid }));    
    EXPECT_EQ(0, reg().get<UI32PairSet>(mm).size());
    for (uint32_t i = E_CONDITION_KILL_MONSTER; i < E_CONDITION_COMSTUM; ++i)
    {
        EXPECT_EQ(0, type_misison[i].size());
    }
    reg().clear();
}

TEST(Missions, CompleteRemakeMission)
{
    auto mm = MakePlayerMissionMap();
    uint32_t mid = 4;
    //auto mrow = MissionJson::GetSingleton().Primary1KeyRow(mid);
    MakePlayerMissionParam param{ mm,   mid,  E_OP_CODE_TEST };
    EXPECT_EQ(RET_OK, MakePlayerMission(param));
    EXPECT_EQ(1, reg().get<UI32PairSet>(mm).size());
    ConditionEvent ce{ mm, E_CONDITION_KILL_MONSTER, {1}, 1 };
    TriggerConditionEvent(ce);
    EXPECT_FALSE(IsAcceptedMission({ mm, mid }));
    EXPECT_TRUE(IsCompleteMission({ mm, mid }));    
    EXPECT_EQ(RET_MISSION_COMPLETE, MakePlayerMission(param));
    reg().clear();
}


TEST(Missions, AcceptNextMission)
{

}

TEST(Missions, AcceptNextMirroMission)
{

}

TEST(Missions, AcceptMissionCondition)
{

}

TEST(Missions, MissionTimeOut)
{

}

int main(int argc, char** argv)
{
    Random::GetSingleton();
    ConditionJson::GetSingleton().Load("config/json/condition.json");
    MissionJson::GetSingleton().Load("config/json/mission.json");
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

