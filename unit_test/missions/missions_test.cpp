#include <gtest/gtest.h>

#include "src/game_config/generator/json_cpp/mission_json.h"
#include "src/game_config/generator/json_cpp/condition_json.h"

using namespace common;

TEST(MissionsTest, Decorate)
{

}

TEST(MissionsTest, RadomCondtion)
{

}

TEST(MissionsTest, CondtionList)
{

}

int main(int argc, char** argv)
{
    ConditionJson::GetSingleton().Load("config/json/condition.json");
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

