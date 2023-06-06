#include <gtest/gtest.h>

#include "src/game_logic/tips_id.h"
#include "src/game_logic/login_state/login_state_machine.h"

TEST(LoginStateMachineTest, LoginRepetition)
{
    LoginStateMachine lsm;
    EXPECT_EQ(kRetLoginUnLogin, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoginUnLogin, lsm.EnterGame());
    EXPECT_EQ(kRetOK, lsm.Login());
    EXPECT_EQ(kRetLoginIng, lsm.Login());
    EXPECT_EQ(kRetLoginIng, lsm.EnterGame());
}

TEST(LoginStateMachineTest, LoginNoPlayer)
{
    LoginStateMachine lsm;
    EXPECT_EQ(kRetOK, lsm.Login());
    lsm.OnEmptyPlayer();
    EXPECT_EQ(kRetLoginWaitingEnterGame, lsm.Login());
    EXPECT_EQ(kRetLoginPlayerGuidError, lsm.EnterGame());
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
    EXPECT_EQ(kRetLoginPlayerGuidError, lsm.EnterGame());
    EXPECT_EQ(kRetOK, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoginCreatingPlayer, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoginCreatingPlayer, lsm.EnterGame());
    lsm.WaitingEnterGame();
    EXPECT_EQ(kRetOK, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoginCreatingPlayer, lsm.CreatePlayer());
    lsm.WaitingEnterGame();
    EXPECT_EQ(kRetOK, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoginCreatingPlayer, lsm.CreatePlayer());
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
    EXPECT_EQ(kRetLoginWaitingEnterGame, lsm.Login());
    EXPECT_EQ(kRetOK, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoginCreatingPlayer, lsm.Login());
    EXPECT_EQ(kRetLoginCreatingPlayer, lsm.CreatePlayer());
    EXPECT_EQ(kRetLoginCreatingPlayer, lsm.EnterGame());
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


TEST(LoginStateMachineTest, PlayerCreatePlayerLogout)
{

}

TEST(LoginStateMachineTest, PlayerEnterGameLogout)
{

}

TEST(LoginStateMachineTest, PlayerAfterEnterGameLogin)
{
    LoginStateMachine lsm;
    EXPECT_EQ(kRetOK, lsm.Login());
    lsm.WaitingEnterGame();
    EXPECT_EQ(kRetLoginWaitingEnterGame, lsm.Login());
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

