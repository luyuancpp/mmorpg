package data_test

import (
	"testing"

	"context"
	"login/data"

	"github.com/stretchr/testify/assert"
)

func TestPlayerFSM_InitialState(t *testing.T) {
	fsm := data.InitPlayerFSM()

	assert.Equal(t, data.LoginProcessing, fsm.Current(), "Initial state should be LoginProcessing")
}

func TestPlayerFSM_EnterGameWithoutLogin(t *testing.T) {
	fsm := data.InitPlayerFSM()

	// Entering game without login should fail
	err := fsm.Event(context.Background(), data.EventEnterGame)
	assert.Error(t, err, "Player should not be able to enter game without logging in")
	assert.Equal(t, data.LoginProcessing, fsm.Current(), "State should remain LoginProcessing")
}

func TestPlayerFSM_CreateCharacterWithoutLogin(t *testing.T) {
	fsm := data.InitPlayerFSM()

	// Creating character without login should fail
	err := fsm.Event(context.Background(), data.EventCreateChar)
	assert.Error(t, err, "Player should not be able to create character without logging in")
	assert.Equal(t, data.LoginProcessing, fsm.Current(), "State should remain LoginProcessing")
}

func TestPlayerFSM_LoginAndWaitingToEnterGame(t *testing.T) {
	fsm := data.InitPlayerFSM()

	err := fsm.Event(context.Background(), data.EventProcessLogin)
	assert.NoError(t, err, "Login event should succeed")
	assert.Equal(t, data.WaitingToEnterGame, fsm.Current(), "State should transition to WaitingToEnterGame")
}

func TestPlayerFSM_CreateCharacterAfterLogin(t *testing.T) {
	fsm := data.InitPlayerFSM()

	fsm.Event(context.Background(), data.EventProcessLogin)

	err := fsm.Event(context.Background(), data.EventCreateChar)
	assert.NoError(t, err, "Create character event should succeed")
	assert.Equal(t, data.CreatingCharacter, fsm.Current(), "State should transition to CreatingCharacter")
}

func TestPlayerFSM_CannotCreateCharacterAgain(t *testing.T) {
	fsm := data.InitPlayerFSM()

	fsm.Event(context.Background(), data.EventProcessLogin)
	fsm.Event(context.Background(), data.EventCreateChar)

	// Cannot create character again after creation
	err := fsm.Event(context.Background(), data.EventCreateChar)
	assert.Error(t, err, "Player should not be able to create character after character creation")
	assert.Equal(t, data.CreatingCharacter, fsm.Current(), "State should remain CreatingCharacter")
}

func TestPlayerFSM_EnterGameAfterCharacterCreation(t *testing.T) {
	fsm := data.InitPlayerFSM()

	fsm.Event(context.Background(), data.EventProcessLogin)
	fsm.Event(context.Background(), data.EventCreateChar)
	fsm.Event(context.Background(), data.EventProcessLogin)

	// Can enter game after character creation
	err := fsm.Event(context.Background(), data.EventEnterGame)
	assert.NoError(t, err, "Enter game event should succeed")
	assert.Equal(t, data.EnteringGame, fsm.Current(), "State should transition to EnteringGame")
}

func TestPlayerFSM_CannotLoginOrCreateCharacterAfterEnteringGame(t *testing.T) {
	fsm := data.InitPlayerFSM()

	fsm.Event(context.Background(), data.EventProcessLogin)
	fsm.Event(context.Background(), data.EventCreateChar)
	fsm.Event(context.Background(), data.EventProcessLogin)
	fsm.Event(context.Background(), data.EventEnterGame)

	// Cannot login again after entering game
	err := fsm.Event(context.Background(), data.EventProcessLogin)
	assert.Error(t, err, "Player should not be able to log in after entering game")
	assert.Equal(t, data.EnteringGame, fsm.Current(), "State should remain EnteringGame")

	// Cannot create character after entering game
	err = fsm.Event(context.Background(), data.EventCreateChar)
	assert.Error(t, err, "Player should not be able to create character after entering game")
	assert.Equal(t, data.EnteringGame, fsm.Current(), "State should remain EnteringGame")
}
