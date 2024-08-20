#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "muduo/base/Logging.h"
#include "test_config.h"
#include "ability_config.h"
#include "buff_config.h"

void LoadAllConfig();

TEST(LoadAllConfigTest, TestLoadTestXlsx)
{
    LoadAllConfig();

    LOG_INFO << GetAbilityAllTable().DebugString();
	LOG_INFO << GetBuffAllTable().DebugString();
    LOG_INFO << GetTestAllTable().DebugString();
}

// Main function
int main(int argc, char** argv) {

    ::testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
