#include <gtest/gtest.h>

#include "src/game_config/generator/json_cpp/mission_json.h"
#include "src/game_config/generator/json_cpp/condition_json.h"
#include "src/game_logic/comp/mission.hpp"
#include "src/game_logic/factories/factories.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/op_code.h"
#include "src/game_logic/sys/mission_sys.hpp"
#include "src/random/random.h"

#include "comp.pb.h"

using namespace common;

TEST(Missions, MakeQuest)
{
    auto mm = MakePlayerMission(reg());
    std::size_t s = 0;
    auto lmake_mission = [mm, &s](uint32_t id)-> void
    {
        auto m = MakeMission(reg(), mm, id);
        ++s;
    };
    
    MissionJson::GetSingleton().IdListCallback(lmake_mission);

    EXPECT_EQ(s, reg().get<MissionMap>(mm).missions().size());
    EXPECT_EQ(0, reg().get<CompleteMissionsId>(mm).missions_size());
    CompleteAllMission(reg(), mm, E_OP_CODE_TEST);
    EXPECT_EQ(0, reg().view<MissionMap>().size());
    EXPECT_EQ(s, reg().get<CompleteMissionsId>(mm).missions_size());
}

TEST(Missions, RadomCondtion)
{
    uint32_t rid = 3;
    auto cids = MissionJson::GetSingleton().Primary1KeyRow(rid);
    auto mm = MakePlayerMission(reg());
    MakeMission(reg(), mm, rid);
    auto& missions = reg().get<MissionMap>(mm).missions();
    auto it =  std::find(cids->random_condition_pool().begin(), cids->random_condition_pool().end(),
        missions.find(rid)->second.conditions(0).id());
    EXPECT_TRUE(it != cids->random_condition_pool().end());
    EXPECT_EQ(1, missions.find(rid)->second.conditions_size());
}

TEST(Missions, RemakeMission)
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
    Random::GetSingleton();
    ConditionJson::GetSingleton().Load("config/json/condition.json");
    MissionJson::GetSingleton().Load("config/json/mission.json");
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

