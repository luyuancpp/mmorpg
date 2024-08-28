package behaviortree

import (
	b3 "github.com/magicsea/behavior3go"
	"go.uber.org/zap"

	//. "github.com/magicsea/behavior3go/actions"
	//. "github.com/magicsea/behavior3go/composites"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
)

type SendCreatePlayer struct {
	Action
	info string
}

func (this *SendCreatePlayer) Initialize(setting *BTNodeCfg) {
	this.Action.Initialize(setting)
}

func (this *SendCreatePlayer) OnTick(tick *Tick) b3.Status {
	zap.L().Error("player tree tick")
	return b3.SUCCESS
}

type IsRoleListEmpty struct {
	Action
	info string
}

func (this *IsRoleListEmpty) Initialize(setting *BTNodeCfg) {
	this.Action.Initialize(setting)
}

func (this *IsRoleListEmpty) OnTick(tick *Tick) b3.Status {
	zap.L().Error("player tree tick")
	if !tick.Blackboard.GetBool("loginsucess", "", "") {
		return b3.FAILURE
	}
	return b3.SUCCESS
}
