#include <gtest/gtest.h>

#include "comp.pb.h"
#include "src/game_config/generator/json_cpp/mission_json.h"
#include "src/game_config/generator/json_cpp/condition_json.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/factories/factories.h"

using namespace common;

TEST(Missions, MakeQuest)
{
    auto e = MakeMission(reg(), 1);
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
     
}

int main(int argc, char** argv)
{
    ConditionJson::GetSingleton().Load("config/json/condition.json");
    MissionJson::GetSingleton().Load("config/json/mission.json");
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

