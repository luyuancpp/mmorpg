#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "muduo/base/Logging.h"
#include "test_config.h"
#include "ability_config.h"
#include "buff_config.h"

void LoadAllConfig();

TEST(LoadAllConfigTest, GetAbilityAllTable)
{
	for (auto& it : GetAbilityAllTable().data())
	{
		LOG_INFO << it.DebugString();
	}
}

TEST(LoadAllConfigTest, GetBuffAllTable)
{
	for (auto& it : GetBuffAllTable().data())
	{
		LOG_INFO << it.DebugString();
	}
}


TEST(LoadAllConfigTest, GetTestAllTable)
{
    for (auto& it : GetTestAllTable().data())
    {
        LOG_INFO << it.DebugString();
    }
}

// Main function
int main(int argc, char** argv) {

    ::testing::InitGoogleTest(&argc, argv);
    LoadAllConfig();
    int ret = RUN_ALL_TESTS();
    return ret;
}
