package behaviortree

import (
	b3 "github.com/magicsea/behavior3go"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
	"go.uber.org/zap"
	"robot/interfaces"
)

const (
	LoginSubTree  = "Login"
	SceneSubTree  = "Scene"
	BattleSubTree = "Battle"
)

type SetSubTree struct {
	Action
	treeTitle string
	count     int
}

func (this *SetSubTree) Initialize(setting *BTNodeCfg) {
	this.Action.Initialize(setting)
	this.treeTitle = setting.GetPropertyAsString("treeTitle")
}

func (this *SetSubTree) OnTick(tick *Tick) b3.Status {
	clientI := tick.Blackboard.GetMem(ClientIdentifier)

	client, ok := clientI.(interfaces.GameClientInterface)
	if !ok {
		zap.L().Error("Failed to cast client from blackboard", zap.Any("client", clientI))
		return b3.FAILURE
	}

	client.SetSubTree(this.treeTitle)

	return b3.SUCCESS
}
