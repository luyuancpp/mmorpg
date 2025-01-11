package data

import (
	"context"
	"testing"
)

// 初始化状态机的测试函数
func TestPlayerFSM(t *testing.T) {
	playerFSM := InitPlayerFSM()

	// 检查初始状态
	if playerFSM.Current() != LoginProcessing {
		t.Errorf("初始状态错误，期望: %s, 实际: %s", LoginProcessing, playerFSM.Current())
	}

	// 测试状态: LoginProcessing -> WaitingToEnterGame
	if err := playerFSM.Event(context.Background(), EventProcessLogin); err != nil {
		t.Errorf("状态切换失败: %v", err)
	}
	if playerFSM.Current() != WaitingToEnterGame {
		t.Errorf("状态切换错误，期望: %s, 实际: %s", WaitingToEnterGame, playerFSM.Current())
	}

	// 测试角色没有时的状态切换: WaitingToEnterGame -> NoCharacter
	if err := playerFSM.Event(context.Background(), EventNoCharacter); err != nil {
		t.Errorf("状态切换失败: %v", err)
	}
	if playerFSM.Current() != NoCharacter {
		t.Errorf("状态切换错误，期望: %s, 实际: %s", NoCharacter, playerFSM.Current())
	}

	// 测试角色已满时的状态切换: CreatingCharacter -> CharacterLimitReached
	if err := playerFSM.Event(context.Background(), EventLimitReached); err != nil {
		t.Errorf("状态切换失败: %v", err)
	}
	if playerFSM.Current() != CharacterLimitReached {
		t.Errorf("状态切换错误，期望: %s, 实际: %s", CharacterLimitReached, playerFSM.Current())
	}

	// 测试正常状态转换: WaitingToEnterGame -> EnteringGame
	if err := playerFSM.Event(context.Background(), EventEnterGame); err != nil {
		t.Errorf("状态切换失败: %v", err)
	}
	if playerFSM.Current() != EnteringGame {
		t.Errorf("状态切换错误，期望: %s, 实际: %s", EnteringGame, playerFSM.Current())
	}

	// 测试正常状态转换: EnteringGame -> InGame
	if err := playerFSM.Event(context.Background(), EventStartPlaying); err != nil {
		t.Errorf("状态切换失败: %v", err)
	}
	if playerFSM.Current() != InGame {
		t.Errorf("状态切换错误，期望: %s, 实际: %s", InGame, playerFSM.Current())
	}
}
