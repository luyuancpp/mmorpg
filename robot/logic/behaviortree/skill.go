package behaviortree

import (
	b3 "github.com/magicsea/behavior3go"
	//. "github.com/magicsea/behavior3go/actions"
	//. "github.com/magicsea/behavior3go/composites"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
)

type GetReleasableSkillList struct {
	Action
	SkillGroupIndex      int
	ReleasableSkillBoard string
}

func (s *GetReleasableSkillList) Initialize(setting *BTNodeCfg) {
	s.Action.Initialize(setting)
	s.SkillGroupIndex = setting.GetPropertyAsInt("SkillGroupIndex")
	s.ReleasableSkillBoard = setting.GetPropertyAsString("ReleasableSkillBoard")
}

func (s *GetReleasableSkillList) OnTick(tick *Tick) b3.Status {

	return b3.SUCCESS
}

type CheckSkillByDistance struct {
	Action
	DistanceBoard                  string
	ReleasableSkillBoard           string
	AfterCheckReleasableSkillBoard string
}

func (s *CheckSkillByDistance) Initialize(setting *BTNodeCfg) {
	s.Action.Initialize(setting)
	s.DistanceBoard = setting.GetPropertyAsString("DistanceBoard")
	s.ReleasableSkillBoard = setting.GetPropertyAsString("ReleasableSkillBoard")
	s.AfterCheckReleasableSkillBoard = setting.GetPropertyAsString("AfterCheckReleasableSkillBoard")
}

func (s *CheckSkillByDistance) OnTick(tick *Tick) b3.Status {

	return b3.SUCCESS
}

type GetSkillID struct {
	Action
	SkillListBoard string
	SkillIndex     uint32
	SkillIDBoard   string
}

func (s *GetSkillID) Initialize(setting *BTNodeCfg) {
	s.Action.Initialize(setting)
	s.SkillListBoard = setting.GetPropertyAsString("SkillListBoard")
	s.SkillIndex = uint32(setting.GetProperty("SkillIndex"))
	s.SkillIDBoard = setting.GetPropertyAsString("SkillIDBoard")
}

func (s *GetSkillID) OnTick(tick *Tick) b3.Status {

	return b3.SUCCESS
}

type ReleaseSkill struct {
	Action
	skillIDBoard string
}

func (s *ReleaseSkill) Initialize(setting *BTNodeCfg) {
	s.Action.Initialize(setting)
	s.skillIDBoard = setting.GetPropertyAsString("skillIDBoard")
}

func (s *ReleaseSkill) OnTick(tick *Tick) b3.Status {

	return b3.SUCCESS
}
