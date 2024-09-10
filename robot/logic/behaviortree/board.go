package behaviortree

import (
	b3 "github.com/magicsea/behavior3go"
	//. "github.com/magicsea/behavior3go/actions"
	//. "github.com/magicsea/behavior3go/composites"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
)

type BoardEqualConst struct {
	Action
	key   string
	value uint64
}

func (b *BoardEqualConst) Initialize(setting *BTNodeCfg) {
	b.Action.Initialize(setting)
	b.key = setting.GetPropertyAsString("key")
	b.value = uint64(setting.GetProperty("value"))
}

func (b *BoardEqualConst) OnTick(tick *Tick) b3.Status {
	HatredTargetId := tick.Blackboard.GetUInt64(b.key, "", "")

	if b.value == HatredTargetId || b.value == 0 {
		return b3.FAILURE
	}

	return b3.SUCCESS
}
