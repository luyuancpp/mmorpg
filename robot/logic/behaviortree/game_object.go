package behaviortree

import (
	b3 "github.com/magicsea/behavior3go"
	//. "github.com/magicsea/behavior3go/actions"
	//. "github.com/magicsea/behavior3go/composites"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
)

type GetTargetDistance struct {
	Action
	TargetBoard   string
	DistanceBoard string
}

func (s *GetTargetDistance) Initialize(setting *BTNodeCfg) {
	s.Action.Initialize(setting)
	s.TargetBoard = setting.GetPropertyAsString("TargetBoard")
	s.DistanceBoard = setting.GetPropertyAsString("DistanceBoard")
}

func (s *GetTargetDistance) OnTick(tick *Tick) b3.Status {

	return b3.SUCCESS
}
