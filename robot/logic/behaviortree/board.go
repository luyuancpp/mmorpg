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

	if b.value == HatredTargetId {
		return b3.FAILURE
	}

	return b3.SUCCESS
}

type SetBoardTargetPos struct {
	Action
	targetIDBoard string
	storeBoard    string
}

func (s *SetBoardTargetPos) Initialize(setting *BTNodeCfg) {
	s.Action.Initialize(setting)
	s.targetIDBoard = setting.GetPropertyAsString("targetIDBoard")
	s.storeBoard = setting.GetPropertyAsString("storeBoard")
}

func (s *SetBoardTargetPos) OnTick(tick *Tick) b3.Status {
	targetIdBoard := tick.Blackboard.GetMem(s.targetIDBoard)
	tick.Blackboard.SetMem(s.storeBoard, targetIdBoard)

	return b3.SUCCESS
}

type MoveToEntity struct {
	Action
	origMaxDistance  int
	targetBoard      string
	originalPosBoard string
}

func (s *MoveToEntity) Initialize(setting *BTNodeCfg) {
	s.Action.Initialize(setting)
	s.origMaxDistance = setting.GetPropertyAsInt("origMaxDistance")
	s.targetBoard = setting.GetPropertyAsString("targetBoard")
	s.originalPosBoard = setting.GetPropertyAsString("originalPosBoard")
}

func (s *MoveToEntity) OnTick(tick *Tick) b3.Status {

	return b3.SUCCESS
}
