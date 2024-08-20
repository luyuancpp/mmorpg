#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "muduo/base/Logging.h"
#include "test_config.h"

void LoadAllConfig();

TEST(LoadAllConfigTest, TestLoadTestXlsx)
{
    LoadAllConfig();

    LOG_INFO << GetTestAllTable().DebugString();
}

// Main function
int main(int argc, char** argv) {

    ::testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
