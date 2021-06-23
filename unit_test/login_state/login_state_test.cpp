#include <gtest/gtest.h>

#include "src/return_code/return_notice_code.h"
#include "src/login_player/login_state_machine.h"

using namespace common;

TEST(LoginStateMachineTest, LoginStateTest)
{
    LoginStateMachine lsm;
    EXPECT_EQ(common::RET_LOGIN_LOGIN_NOT_COMPLETE, lsm.CreatePlayer());
    EXPECT_EQ(common::RET_LOGIN_LOGIN_NOT_COMPLETE, lsm.EnterGame());
    EXPECT_EQ(common::RET_OK, lsm.Lgoin());
    EXPECT_EQ(common::RET_LOGIN_REPETITION_LOGIN, lsm.Lgoin());
    EXPECT_EQ(common::RET_LOGIN_LOGIN_NO_PLAYER, lsm.EnterGame());
}


int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

