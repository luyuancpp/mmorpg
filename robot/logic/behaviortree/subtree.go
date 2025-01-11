package behaviortree

import (
	b3 "github.com/magicsea/behavior3go"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
	"go.uber.org/zap"
	"robot/interfaces"
)

const (
	LoginSubTree = "Login"
)

type SetSubTree struct {
	Action
	treeTitle string
}

func (s *SetSubTree) Initialize(setting *BTNodeCfg) {
	s.Action.Initialize(setting)
	s.treeTitle = setting.GetPropertyAsString("treeTitle")
}

func (s *SetSubTree) OnTick(tick *Tick) b3.Status {
	clientI := tick.Blackboard.GetMem(ClientBoardKey)

	client, ok := clientI.(interfaces.GameClientInterface)
	if !ok {
		zap.L().Error("Failed to cast client from blackboard", zap.Any("client", clientI))
		return b3.FAILURE
	}

	client.SetSubTree(s.treeTitle)

	return b3.SUCCESS
}

type InitTree struct {
	Action
	initialized bool
}

func (i *InitTree) Initialize(setting *BTNodeCfg) {
	i.Action.Initialize(setting)
}

func (i *InitTree) OnTick(tick *Tick) b3.Status {

	if i.initialized {
		return b3.FAILURE
	}

	i.initialized = true

	return b3.SUCCESS
}
