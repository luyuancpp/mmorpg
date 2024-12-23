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

func (c *CheckTestCount) Initialize(setting *BTNodeCfg) {
	c.Action.Initialize(setting)
	c.count = int32(setting.GetPropertyAsInt("count"))
}

func (c *CheckTestCount) OnTick(tick *Tick) b3.Status {
	count := tick.Blackboard.GetInt32(TestCountBoardKey, "", "")
	if count >= c.count {
		return b3.FAILURE
	}

	return b3.SUCCESS
}

type IncrementTestCount struct {
	Action
}

func (ic *IncrementTestCount) Initialize(setting *BTNodeCfg) {
	ic.Action.Initialize(setting)
}

func (ic *IncrementTestCount) OnTick(tick *Tick) b3.Status {
	count := tick.Blackboard.GetInt32(TestCountBoardKey, "", "")
	tick.Blackboard.Set(TestCountBoardKey, count+1, "", "")
	return b3.SUCCESS
}

type ResetTestCount struct {
	Action
}

func (ic *ResetTestCount) Initialize(setting *BTNodeCfg) {
	ic.Action.Initialize(setting)
}

func (ic *ResetTestCount) OnTick(tick *Tick) b3.Status {
	tick.Blackboard.Set(TestCountBoardKey, int32(0), "", "")
	return b3.SUCCESS
}
