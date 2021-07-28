#include <gtest/gtest.h>

#include "src/game_config/generator/json_cpp/mission_json.h"
#include "src/game_config/generator/json_cpp/condition_json.h"
#include "src/game_logic/comp/mission.hpp"
#include "src/game_logic/factories/factories.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/op_code.h"
#include "src/game_logic/sys/mission_sys.hpp"

#include "comp.pb.h"

using namespace common;

TEST(Missions, MakeQuest)
{
    auto mm = MakeMissionMap(reg());
    std::size_t s = 0;
    auto accept_mission = [mm, &s](uint32_t id)-> void
    {
        auto m = MakeMission(reg(), mm, id);
        ++s;
    };
    
    MissionJson::GetSingleton().IdListCallback(accept_mission);

    EXPECT_EQ(s, reg().get<MissionMap>(mm).size());
    EXPECT_EQ(0, reg().get<CompleteMissionsId>(mm).missions_size());
    CompleteAllMission(reg(), mm, E_OP_CODE_TEST);
    EXPECT_EQ(0, reg().view<Mission>().size());
    EXPECT_EQ(0, reg().view<MissionMap>().size());
    EXPECT_EQ(s, reg().get<CompleteMissionsId>(mm).missions_size());
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

