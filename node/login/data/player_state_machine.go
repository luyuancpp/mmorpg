package data

import (
	"context"
	"fmt"
	fsm "github.com/looplab/fsm"
)

// 状态定义
const (
	LoginProcessing       = "LoginProcessing"       // 登录处理中
	WaitingToEnterGame    = "WaitingToEnterGame"    // 等待进入游戏
	CreatingCharacter     = "CreatingCharacter"     // 创建角色
	EnteringGame          = "EnteringGame"          // 进入游戏
	InGame                = "InGame"                // 游戏中
	NoCharacter           = "NoCharacter"           // 没有角色
	CharacterLimitReached = "CharacterLimitReached" // 角色已满
)

// 事件定义
const (
	EventProcessLogin = "processLogin"
	EventCreateChar   = "createCharacter"
	EventEnterGame    = "enterGame"
	EventStartPlaying = "startPlaying"
	EventNoCharacter  = "noCharacter"
	EventLimitReached = "limitReached"
)

// 初始化玩家状态机
func InitPlayerFSM() *fsm.FSM {
	return fsm.NewFSM(
		LoginProcessing, // 初始状态
		fsm.Events{
			{Name: EventProcessLogin, Src: []string{LoginProcessing}, Dst: WaitingToEnterGame},
			{Name: EventCreateChar, Src: []string{WaitingToEnterGame}, Dst: CreatingCharacter},
			{Name: EventEnterGame, Src: []string{WaitingToEnterGame}, Dst: EnteringGame},
			{Name: EventStartPlaying, Src: []string{EnteringGame}, Dst: InGame},
			{Name: EventNoCharacter, Src: []string{WaitingToEnterGame}, Dst: NoCharacter},
			{Name: EventLimitReached, Src: []string{CreatingCharacter}, Dst: WaitingToEnterGame}, // 角色已满时进入 WaitingToEnterGame 状态
		},
		fsm.Callbacks{
			"enter_" + EnteringGame: func(_ context.Context, e *fsm.Event) {
				fmt.Printf("状态转换: %s -> %s. 进入游戏\n", e.Src, e.Dst)
			},
			"enter_" + InGame: func(_ context.Context, e *fsm.Event) {
				fmt.Printf("状态转换: %s -> %s. 开始游戏\n", e.Src, e.Dst)
			},
			"enter_" + NoCharacter: func(_ context.Context, e *fsm.Event) {
				fmt.Printf("状态转换: %s -> %s. 无角色，需要创建角色\n", e.Src, e.Dst)
			},
			"enter_" + WaitingToEnterGame: func(_ context.Context, e *fsm.Event) {
				fmt.Printf("状态转换: %s -> %s. 角色已满，进入等待状态\n", e.Src, e.Dst)
			},
		},
	)
}
