#include <gtest/gtest.h>

#include "src/return_code/return_notice_code.h"
#include "src/login_player/login_state_machine.h"

using namespace common;

TEST(LoginStateMachineTest, LoginRepetition)
{
    LoginStateMachine lsm;
    EXPECT_EQ(RET_LOGIN_DONOT_LOGIN, lsm.CreatePlayer());
    EXPECT_EQ(RET_LOGIN_DONOT_LOGIN, lsm.EnterGame());
    EXPECT_EQ(RET_OK, lsm.Login());
    EXPECT_EQ(RET_LOGIN_LOGIN_ING, lsm.Login());
    EXPECT_EQ(RET_LOGIN_LOGIN_ING, lsm.EnterGame());
}

TEST(LoginStateMachineTest, LoginNoPlayer)
{
    LoginStateMachine lsm;
    EXPECT_EQ(RET_OK, lsm.Login());
    lsm.OnEmptyPlayer();
    EXPECT_EQ(RET_LOGIN_WAITING_ENTER_GAME, lsm.Login());
    EXPECT_EQ(RET_LOGIN_LOGIN_NO_PLAYER, lsm.EnterGame());
    EXPECT_EQ(RET_OK, lsm.CreatePlayer());
    lsm.WaitingEnterGame();
    EXPECT_EQ(RET_OK, lsm.EnterGame());
    EXPECT_EQ(RET_LOGIN_BEING_ENTER_GAME, lsm.EnterGame());
}

TEST(LoginStateMachineTest, LoginNoPlayerCreateFull)
{
    LoginStateMachine lsm;
    EXPECT_EQ(RET_OK, lsm.Login());
    lsm.OnEmptyPlayer();
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
    lsm.OnFullPlayer();
    EXPECT_EQ(RET_LOGIN_MAX_PLAYER_SIZE, lsm.CreatePlayer());
    EXPECT_EQ(RET_OK, lsm.EnterGame());
    EXPECT_EQ(RET_LOGIN_BEING_ENTER_GAME, lsm.EnterGame());
}

TEST(LoginStateMachineTest, RenterGame)
{
    LoginStateMachine lsm;
    EXPECT_EQ(RET_OK, lsm.Login());
    lsm.WaitingEnterGame();
    EXPECT_EQ(RET_OK, lsm.EnterGame());
    EXPECT_EQ(RET_LOGIN_BEING_ENTER_GAME, lsm.EnterGame());
    lsm.OnPlaying();
    EXPECT_EQ(RET_LOGIN_PLAYEING, lsm.EnterGame());
}


TEST(LoginStateMachineTest, LoginNotReturn)
{
    LoginStateMachine lsm;
    EXPECT_EQ(RET_OK, lsm.Login());
    EXPECT_EQ(RET_LOGIN_LOGIN_ING, lsm.CreatePlayer());
    EXPECT_EQ(RET_LOGIN_LOGIN_ING, lsm.EnterGame());
}

TEST(LoginStateMachineTest, CreatePlayerNotReturn)
{
    LoginStateMachine lsm;
    EXPECT_EQ(RET_OK, lsm.Login());
    lsm.WaitingEnterGame();
    EXPECT_EQ(RET_LOGIN_WAITING_ENTER_GAME, lsm.Login());
    EXPECT_EQ(RET_OK, lsm.CreatePlayer());
    EXPECT_EQ(RET_LOGIN_BEING_CREATE_PLAYER, lsm.Login());
    EXPECT_EQ(RET_LOGIN_BEING_CREATE_PLAYER, lsm.CreatePlayer());
    EXPECT_EQ(RET_LOGIN_BEING_CREATE_PLAYER, lsm.EnterGame());
}

TEST(LoginStateMachineTest, EnterGameNotReturn)
{
    LoginStateMachine lsm;
    EXPECT_EQ(RET_OK, lsm.Login());
    lsm.WaitingEnterGame();
    EXPECT_EQ(RET_OK, lsm.CreatePlayer());
    lsm.WaitingEnterGame();
    EXPECT_EQ(RET_OK, lsm.EnterGame());
    EXPECT_EQ(RET_LOGIN_BEING_ENTER_GAME, lsm.Login());
    EXPECT_EQ(RET_LOGIN_BEING_ENTER_GAME, lsm.CreatePlayer());
    EXPECT_EQ(RET_LOGIN_BEING_ENTER_GAME, lsm.EnterGame());
}

TEST(LoginStateMachineTest, PlayerLoginLogout)
{
    LoginStateMachine lsm;
    EXPECT_EQ(RET_OK, lsm.Login());
    EXPECT_EQ(RET_OK, lsm.Logout());
    lsm.WaitingEnterGame();
    EXPECT_EQ(RET_LOGIN_DONOT_LOGIN, lsm.CreatePlayer());
    EXPECT_EQ(RET_LOGIN_DONOT_LOGIN, lsm.EnterGame());
    EXPECT_EQ(RET_OK, lsm.Login());
}

TEST(LoginStateMachineTest, PlayerCreatePlayerLogout)
{
    LoginStateMachine lsm;
    EXPECT_EQ(RET_OK, lsm.Login());
    lsm.WaitingEnterGame();
    EXPECT_EQ(RET_OK, lsm.CreatePlayer());
    EXPECT_EQ(RET_OK, lsm.Logout());
    lsm.WaitingEnterGame();
    EXPECT_EQ(RET_LOGIN_DONOT_LOGIN, lsm.EnterGame());
    EXPECT_EQ(RET_OK, lsm.Login());
}

TEST(LoginStateMachineTest, PlayerEnterGameLogout)
{
    LoginStateMachine lsm;
    EXPECT_EQ(RET_OK, lsm.Login());
    lsm.WaitingEnterGame();
    EXPECT_EQ(RET_OK, lsm.CreatePlayer());
    lsm.WaitingEnterGame();
    EXPECT_EQ(RET_OK, lsm.EnterGame());
    EXPECT_EQ(RET_OK, lsm.Logout());
    lsm.OnPlaying();
    EXPECT_EQ(RET_LOGIN_DONOT_LOGIN, lsm.CreatePlayer());
    EXPECT_EQ(RET_LOGIN_DONOT_LOGIN, lsm.EnterGame());
    EXPECT_EQ(RET_OK, lsm.Login());
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

