package data

import (
	fsm "github.com/looplab/fsm"
)

// 状态定义
const (
	LoginProcessing    = "LoginProcessing"    // 登录处理中
	WaitingToEnterGame = "WaitingToEnterGame" // 等待进入游戏
	CreatingCharacter  = "CreatingCharacter"  // 创建角色
	EnteringGame       = "EnteringGame"       // 进入游戏
)

// 事件定义
const (
	EventProcessLogin = "processLogin"
	EventCreateChar   = "createCharacter"
	EventEnterGame    = "enterGame"
)

// 初始化玩家状态机
func InitPlayerFSM() *fsm.FSM {
	return fsm.NewFSM(
		LoginProcessing, // 初始状态
		fsm.Events{
			{Name: EventProcessLogin, Src: []string{LoginProcessing, CreatingCharacter}, Dst: WaitingToEnterGame},
			{Name: EventCreateChar, Src: []string{WaitingToEnterGame}, Dst: CreatingCharacter},
			{Name: EventEnterGame, Src: []string{WaitingToEnterGame, CreatingCharacter}, Dst: EnteringGame},
		},
		fsm.Callbacks{},
	)
}
