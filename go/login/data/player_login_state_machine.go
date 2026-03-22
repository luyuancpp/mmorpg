package data

import (
	fsm "github.com/looplab/fsm"
)

// States
const (
	LoginProcessing    = "LoginProcessing"
	WaitingToEnterGame = "WaitingToEnterGame"
	CreatingCharacter  = "CreatingCharacter"
	EnteringGame       = "EnteringGame"
)

// Events
const (
	EventProcessLogin = "processLogin"
	EventCreateChar   = "createCharacter"
	EventEnterGame    = "enterGame"
)

// InitPlayerFSM creates the player login state machine.
func InitPlayerFSM() *fsm.FSM {
	return fsm.NewFSM(
		LoginProcessing,
		fsm.Events{
			{Name: EventProcessLogin, Src: []string{LoginProcessing, CreatingCharacter}, Dst: WaitingToEnterGame},
			{Name: EventCreateChar, Src: []string{WaitingToEnterGame}, Dst: CreatingCharacter},
			{Name: EventEnterGame, Src: []string{WaitingToEnterGame, CreatingCharacter}, Dst: EnteringGame},
		},
		fsm.Callbacks{},
	)
}
