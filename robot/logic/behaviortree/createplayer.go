package behaviortree

import (
	b3 "github.com/magicsea/behavior3go"
	"go.uber.org/zap"

	//. "github.com/magicsea/behavior3go/actions"
	//. "github.com/magicsea/behavior3go/composites"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
)

// 自定义action节点
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
