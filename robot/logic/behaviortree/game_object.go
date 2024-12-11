package behaviortree

import (
	b3 "github.com/magicsea/behavior3go"
	"go.uber.org/zap"
	"robot/logic/gameobject"

	//. "github.com/magicsea/behavior3go/actions"
	//. "github.com/magicsea/behavior3go/composites"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
)

type GetHatredTarget struct {
	Action
	TargetIDBoard string
}

func (b *GetHatredTarget) Initialize(setting *BTNodeCfg) {
	b.Action.Initialize(setting)
	b.TargetIDBoard = setting.GetPropertyAsString("TargetIDBoard")
}

func (b *GetHatredTarget) OnTick(tick *Tick) b3.Status {
	actorListFromBlackboard := tick.Blackboard.GetMem(ActorListBoardKey)

	rawActorList, ok := actorListFromBlackboard.(*gameobject.ActorList)
	if !ok {
		zap.L().Error("Failed to cast actor list from blackboard", zap.String("Key", ActorListBoardKey), zap.Any("Value", actorListFromBlackboard))
		return b3.FAILURE
	}

	actorID, ok := rawActorList.GetRandomActor()
	if !ok {
		return b3.FAILURE
	}

	tick.Blackboard.SetMem(b.TargetIDBoard, actorID)

	return b3.SUCCESS
}

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
