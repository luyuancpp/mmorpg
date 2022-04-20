#include <gtest/gtest.h>

#include "src/return_code/error_code.h"
#include "src/game_logic/login_state/login_state_machine.h"

using namespace common;

TEST(LoginStateMachineTest, LoginRepetition)
{
    LoginStateMachine lsm;
    EXPECT_EQ(kRetLoginHadnotLogin, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoginHadnotLogin, lsm.EnterGame());
    EXPECT_EQ(kRetOK, lsm.Login());
    EXPECT_EQ(kRetLoginIng, lsm.Login());
    EXPECT_EQ(kRetLoginIng, lsm.EnterGame());
}

TEST(LoginStateMachineTest, LoginNoPlayer)
{
    LoginStateMachine lsm;
    EXPECT_EQ(kRetOK, lsm.Login());
    lsm.OnEmptyPlayer();
    EXPECT_EQ(kRetLoignWatingEnterGame, lsm.Login());
    EXPECT_EQ(kRetLoginNotPlayer, lsm.EnterGame());
    EXPECT_EQ(kRetOK, lsm.CreatePlayer());
    lsm.WaitingEnterGame();
    EXPECT_EQ(kRetOK, lsm.EnterGame());
    EXPECT_EQ(kRetLoginEnteringGame, lsm.EnterGame());
}

TEST(LoginStateMachineTest, LoginNoPlayerCreateFull)
{
    LoginStateMachine lsm;
    EXPECT_EQ(kRetOK, lsm.Login());
    lsm.OnEmptyPlayer();
    EXPECT_EQ(kRetLoginNotPlayer, lsm.EnterGame());
    EXPECT_EQ(kRetOK, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoignCreatingPlayer, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoignCreatingPlayer, lsm.EnterGame());
    lsm.WaitingEnterGame();
    EXPECT_EQ(kRetOK, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoignCreatingPlayer, lsm.CreatePlayer());
    lsm.WaitingEnterGame();
    EXPECT_EQ(kRetOK, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoignCreatingPlayer, lsm.CreatePlayer());
    lsm.OnFullPlayer();
    EXPECT_EQ(kRetLoginAccountPlayerFull, lsm.CreatePlayer());
    EXPECT_EQ(kRetOK, lsm.EnterGame());
    EXPECT_EQ(kRetLoginEnteringGame, lsm.EnterGame());
}

TEST(LoginStateMachineTest, RenterGame)
{
    LoginStateMachine lsm;
    EXPECT_EQ(kRetOK, lsm.Login());
    lsm.WaitingEnterGame();
    EXPECT_EQ(kRetOK, lsm.EnterGame());
    EXPECT_EQ(kRetLoginEnteringGame, lsm.EnterGame());
    lsm.OnPlaying();
    EXPECT_EQ(kRetLoginPlaying, lsm.EnterGame());
}


TEST(LoginStateMachineTest, LoginNotReturn)
{
    LoginStateMachine lsm;
    EXPECT_EQ(kRetOK, lsm.Login());
    EXPECT_EQ(kRetLoginIng, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoginIng, lsm.EnterGame());
}

TEST(LoginStateMachineTest, CreatePlayerNotReturn)
{
    LoginStateMachine lsm;
    EXPECT_EQ(kRetOK, lsm.Login());
    lsm.WaitingEnterGame();
    EXPECT_EQ(kRetLoignWatingEnterGame, lsm.Login());
    EXPECT_EQ(kRetOK, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoignCreatingPlayer, lsm.Login());
    EXPECT_EQ(kRetLoignCreatingPlayer, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoignCreatingPlayer, lsm.EnterGame());
}

TEST(LoginStateMachineTest, EnterGameNotReturn)
{
    LoginStateMachine lsm;
    EXPECT_EQ(kRetOK, lsm.Login());
    lsm.WaitingEnterGame();
    EXPECT_EQ(kRetOK, lsm.CreatePlayer());
    lsm.WaitingEnterGame();
    EXPECT_EQ(kRetOK, lsm.EnterGame());
    EXPECT_EQ(kRetLoginEnteringGame, lsm.Login());
    EXPECT_EQ(kRetLoginEnteringGame, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoginEnteringGame, lsm.EnterGame());
}

TEST(LoginStateMachineTest, PlayerLoginLogout)
{
    LoginStateMachine lsm;
    EXPECT_EQ(kRetOK, lsm.Login());
    EXPECT_EQ(kRetOK, lsm.Logout());
    lsm.WaitingEnterGame();
    EXPECT_EQ(kRetLoginHadnotLogin, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoginHadnotLogin, lsm.EnterGame());
    EXPECT_EQ(kRetOK, lsm.Login());
}

TEST(LoginStateMachineTest, PlayerCreatePlayerLogout)
{
    LoginStateMachine lsm;
    EXPECT_EQ(kRetOK, lsm.Login());
    lsm.WaitingEnterGame();
    EXPECT_EQ(kRetOK, lsm.CreatePlayer());
    EXPECT_EQ(kRetOK, lsm.Logout());
    lsm.WaitingEnterGame();
    EXPECT_EQ(kRetLoginHadnotLogin, lsm.EnterGame());
    EXPECT_EQ(kRetOK, lsm.Login());
}

TEST(LoginStateMachineTest, PlayerEnterGameLogout)
{
    LoginStateMachine lsm;
    EXPECT_EQ(kRetOK, lsm.Login());
    lsm.WaitingEnterGame();
    EXPECT_EQ(kRetOK, lsm.CreatePlayer());
    lsm.WaitingEnterGame();
    EXPECT_EQ(kRetOK, lsm.EnterGame());
    EXPECT_EQ(kRetOK, lsm.Logout());
    lsm.OnPlaying();
    EXPECT_EQ(kRetLoginHadnotLogin, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoginHadnotLogin, lsm.EnterGame());
    EXPECT_EQ(kRetOK, lsm.Login());
}

TEST(LoginStateMachineTest, PlayerAfterEnterGameLogin)
{
    LoginStateMachine lsm;
    EXPECT_EQ(kRetOK, lsm.Login());
    lsm.WaitingEnterGame();
    EXPECT_EQ(kRetLoignWatingEnterGame, lsm.Login());
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

