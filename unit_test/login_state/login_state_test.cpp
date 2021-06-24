#include <gtest/gtest.h>

#include "src/return_code/return_notice_code.h"
#include "src/login_player/login_state_machine.h"

using namespace common;

TEST(LoginStateMachineTest, LoginRepetition)
{
    LoginStateMachine lsm;
    EXPECT_EQ(RET_LOGIN_LOGIN_NOT_COMPLETE, lsm.CreatePlayer());
    EXPECT_EQ(RET_LOGIN_LOGIN_NOT_COMPLETE, lsm.EnterGame());
    EXPECT_EQ(RET_OK, lsm.Lgoin());
    EXPECT_EQ(RET_LOGIN_REPETITION_LOGIN, lsm.Lgoin());
    EXPECT_EQ(RET_LOGIN_LOGIN_NO_PLAYER, lsm.EnterGame());
}

TEST(LoginStateMachineTest, LoginNoPlayer)
{
    LoginStateMachine lsm;
    EXPECT_EQ(RET_OK, lsm.Lgoin());
    lsm.NoPlayer();
    EXPECT_EQ(RET_LOGIN_LOGIN_NO_PLAYER, lsm.EnterGame());
    EXPECT_EQ(RET_OK, lsm.CreatePlayer());
    lsm.WaitingEnterGame();
    EXPECT_EQ(RET_OK, lsm.EnterGame());
    EXPECT_EQ(RET_LOGIN_BEING_ENTER_GAME, lsm.EnterGame());
}

TEST(LoginStateMachineTest, LoginNoPlayerCreateFull)
{
    LoginStateMachine lsm;
    EXPECT_EQ(RET_OK, lsm.Lgoin());
    lsm.NoPlayer();
    EXPECT_EQ(RET_LOGIN_LOGIN_NO_PLAYER, lsm.EnterGame());
    EXPECT_EQ(RET_OK, lsm.CreatePlayer());
    EXPECT_EQ(RET_LOGIN_BEING_CREATE_PLAYER, lsm.CreatePlayer());
    EXPECT_EQ(RET_LOGIN_BEING_CREATE_PLAYER, lsm.EnterGame());
    lsm.WaitingEnterGame();
    EXPECT_EQ(RET_OK, lsm.CreatePlayer());
    EXPECT_EQ(RET_LOGIN_BEING_CREATE_PLAYER, lsm.CreatePlayer());
    lsm.WaitingEnterGame();
    EXPECT_EQ(RET_OK, lsm.CreatePlayer());
    EXPECT_EQ(RET_LOGIN_BEING_CREATE_PLAYER, lsm.CreatePlayer());
    lsm.FullPlayer();
    EXPECT_EQ(RET_LOGIN_MAX_PLAYER_SIZE, lsm.CreatePlayer());
    EXPECT_EQ(RET_OK, lsm.EnterGame());
    EXPECT_EQ(RET_LOGIN_BEING_ENTER_GAME, lsm.EnterGame());
}

TEST(LoginStateMachineTest, RenterGame)
{
    LoginStateMachine lsm;
    EXPECT_EQ(RET_OK, lsm.Lgoin());
    lsm.WaitingEnterGame();
    EXPECT_EQ(RET_OK, lsm.EnterGame());
    EXPECT_EQ(RET_LOGIN_BEING_ENTER_GAME, lsm.EnterGame());
    lsm.Playing();
    EXPECT_EQ(RET_LOGIN_PLAYEING, lsm.EnterGame());
}


int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

