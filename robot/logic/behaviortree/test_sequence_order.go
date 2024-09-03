package behaviortree

import (
	b3 "github.com/magicsea/behavior3go"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
)

const (
	SceneTest  = "Scene"
	BattleTest = "Battle"
)

type TestSequenceOrder struct {
	Action
	treeTitle string
	count     int
}

func (this *TestSequenceOrder) Initialize(setting *BTNodeCfg) {
	this.Action.Initialize(setting)
	this.treeTitle = setting.GetPropertyAsString("treeTitle")
	this.count = setting.GetPropertyAsInt("count")
}

func (this *TestSequenceOrder) OnTick(tick *Tick) b3.Status {
	if this.count <= 0 {
		return b3.FAILURE
	}

	this.count--

	return b3.SUCCESS
}
