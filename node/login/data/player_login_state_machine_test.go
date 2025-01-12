package data_test

import (
	"testing"

	"context"
	"github.com/stretchr/testify/assert"
	"login/data"
)

func TestPlayerFSM_InitialState(t *testing.T) {
	// 初始化玩家状态机
	fsm := data.InitPlayerFSM()

	// 初始状态应该是 LoginProcessing
	assert.Equal(t, data.LoginProcessing, fsm.Current(), "Initial state should be LoginProcessing")
}

func TestPlayerFSM_EnterGameWithoutLogin(t *testing.T) {
	// 初始化玩家状态机
	fsm := data.InitPlayerFSM()

	// 未登录直接进入游戏应该失败
	err := fsm.Event(context.Background(), data.EventEnterGame)
	assert.Error(t, err, "Player should not be able to enter game without logging in")
	assert.Equal(t, data.LoginProcessing, fsm.Current(), "State should remain LoginProcessing")
}

func TestPlayerFSM_CreateCharacterWithoutLogin(t *testing.T) {
	// 初始化玩家状态机
	fsm := data.InitPlayerFSM()

	// 未登录直接创建角色应该失败
	err := fsm.Event(context.Background(), data.EventCreateChar)
	assert.Error(t, err, "Player should not be able to create character without logging in")
	assert.Equal(t, data.LoginProcessing, fsm.Current(), "State should remain LoginProcessing")
}

func TestPlayerFSM_LoginAndWaitingToEnterGame(t *testing.T) {
	// 初始化玩家状态机
	fsm := data.InitPlayerFSM()

	// 玩家登录
	err := fsm.Event(context.Background(), data.EventProcessLogin)
	assert.NoError(t, err, "Login event should succeed")
	assert.Equal(t, data.WaitingToEnterGame, fsm.Current(), "State should transition to WaitingToEnterGame")
}

func TestPlayerFSM_CreateCharacterAfterLogin(t *testing.T) {
	// 初始化玩家状态机
	fsm := data.InitPlayerFSM()

	// 玩家登录
	fsm.Event(context.Background(), data.EventProcessLogin)

	// 玩家创建角色
	err := fsm.Event(context.Background(), data.EventCreateChar)
	assert.NoError(t, err, "Create character event should succeed")
	assert.Equal(t, data.CreatingCharacter, fsm.Current(), "State should transition to CreatingCharacter")
}

func TestPlayerFSM_CannotCreateCharacterAgain(t *testing.T) {
	// 初始化玩家状态机
	fsm := data.InitPlayerFSM()

	// 玩家登录并创建角色
	fsm.Event(context.Background(), data.EventProcessLogin)
	fsm.Event(context.Background(), data.EventCreateChar)

	// 玩家创建角色后不能再创建角色
	err := fsm.Event(context.Background(), data.EventCreateChar)
	assert.Error(t, err, "Player should not be able to create character after character creation")
	assert.Equal(t, data.CreatingCharacter, fsm.Current(), "State should remain CreatingCharacter")
}

func TestPlayerFSM_EnterGameAfterCharacterCreation(t *testing.T) {
	// 初始化玩家状态机
	fsm := data.InitPlayerFSM()

	// 玩家登录并创建角色
	fsm.Event(context.Background(), data.EventProcessLogin)
	fsm.Event(context.Background(), data.EventCreateChar)
	fsm.Event(context.Background(), data.EventProcessLogin)

	// 玩家角色创建完后可以进入游戏
	err := fsm.Event(context.Background(), data.EventEnterGame)
	assert.NoError(t, err, "Enter game event should succeed")
	assert.Equal(t, data.EnteringGame, fsm.Current(), "State should transition to EnteringGame")
}

func TestPlayerFSM_CannotLoginOrCreateCharacterAfterEnteringGame(t *testing.T) {
	// 初始化玩家状态机
	fsm := data.InitPlayerFSM()

	// 玩家登录并创建角色，然后进入游戏
	fsm.Event(context.Background(), data.EventProcessLogin)
	fsm.Event(context.Background(), data.EventCreateChar)
	fsm.Event(context.Background(), data.EventProcessLogin)
	fsm.Event(context.Background(), data.EventEnterGame)

	// 玩家进入游戏后不能再登录
	err := fsm.Event(context.Background(), data.EventProcessLogin)
	assert.Error(t, err, "Player should not be able to log in after entering game")
	assert.Equal(t, data.EnteringGame, fsm.Current(), "State should remain EnteringGame")

	// 玩家进入游戏后不能再创建角色
	err = fsm.Event(context.Background(), data.EventCreateChar)
	assert.Error(t, err, "Player should not be able to create character after entering game")
	assert.Equal(t, data.EnteringGame, fsm.Current(), "State should remain EnteringGame")
}
