﻿#include <gtest/gtest.h>

#include "game_entity.pb.h"
#include "src/game_config/generator/json_cpp/mission_json.h"
#include "src/game_config/generator/json_cpp/condition_json.h"
#include "src/game_ecs/game_registry.h"
#include "src/game_logic/missions/mission.h"
#include "src/game_logic/missions/condition.h"

using namespace common;

TEST(Missions, Decorate)
{

}

TEST(Missions, RadomCondtion)
{

}

TEST(Missions, CondtionList)
{

}

TEST(Missions, MissionListInit)
{
    auto e = reg().create();
    
}

TEST(Missions, MissionEntity)
{
    reg().clear();
    auto e = reg().create();
    reg().emplace<Mission>(e);
    reg().emplace<Conditions>(e);
}


int main(int argc, char** argv)
{
    ConditionJson::GetSingleton().Load("config/json/condition.json");
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

