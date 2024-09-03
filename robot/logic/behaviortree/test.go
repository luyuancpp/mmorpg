package behaviortree

import (
	b3 "github.com/magicsea/behavior3go"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
)

type CheckTestCount struct {
	Action
	count int32
}

func (this *CheckTestCount) Initialize(setting *BTNodeCfg) {
	this.Action.Initialize(setting)
	this.count = int32(setting.GetPropertyAsInt("count"))
}

func (this *CheckTestCount) OnTick(tick *Tick) b3.Status {
	count := tick.Blackboard.GetInt32(TestCountKey, "", "")
	if count >= this.count {
		return b3.FAILURE
	}

	return b3.SUCCESS
}

type IncrementTestCount struct {
	Action
}

func (this *IncrementTestCount) Initialize(setting *BTNodeCfg) {
	this.Action.Initialize(setting)
}

func (this *IncrementTestCount) OnTick(tick *Tick) b3.Status {
	count := tick.Blackboard.GetInt32(TestCountKey, "", "")
	tick.Blackboard.Set(TestCountKey, count+1, "", "")
	return b3.SUCCESS
}
