package behaviortree

import (
	b3 "github.com/magicsea/behavior3go"
	"go.uber.org/zap"
	"robot/interfaces"
	"robot/pb/game"

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
	SkillIndex     int
	SkillIDBoard   string
}

func (s *GetSkillID) Initialize(setting *BTNodeCfg) {
	s.Action.Initialize(setting)
	s.SkillListBoard = setting.GetPropertyAsString("SkillListBoard")
	s.SkillIndex = setting.GetPropertyAsInt("SkillIndex")
	s.SkillIDBoard = setting.GetPropertyAsString("SkillIDBoard")
}

func (s *GetSkillID) OnTick(tick *Tick) b3.Status {
	playerSkillListPBComp, ok := tick.Blackboard.GetMem(s.SkillListBoard).(*game.PlayerSkillListPBComponent)
	if !ok {
		zap.L().Debug("Failed to retrieve skill list from blackboard",
			zap.String("Key", s.SkillListBoard),
			zap.Any("Value", tick.Blackboard.GetMem(SkillListBoardKey)))
		return b3.FAILURE
	}

	if s.SkillIndex >= len(playerSkillListPBComp.SkillList) {
		zap.L().Debug("Invalid skill index",
			zap.Int("SkillIndex", s.SkillIndex),
			zap.Int("SkillListLength", len(playerSkillListPBComp.SkillList)))
		return b3.FAILURE
	}

	tick.Blackboard.Set(s.SkillIDBoard, playerSkillListPBComp.SkillList[s.SkillIndex].SkillTableId, "", "")

	return b3.SUCCESS
}

type ReleaseSkill struct {
	Action
	SkillIDBoard string
}

func (s *ReleaseSkill) Initialize(setting *BTNodeCfg) {
	s.Action.Initialize(setting)
	s.SkillIDBoard = setting.GetPropertyAsString("SkillIDBoard")
}

func (s *ReleaseSkill) OnTick(tick *Tick) b3.Status {
	// 从黑板中获取客户端
	client, ok := tick.Blackboard.GetMem(ClientBoardKey).(interfaces.GameClientInterface)
	if !ok {
		zap.L().Error("Failed to cast client from blackboard", zap.String("Key", ClientBoardKey), zap.Any("Value", tick.Blackboard.GetMem(ClientBoardKey)))
		return b3.FAILURE
	}

	rq := &game.ReleaseSkillSkillRequest{}

	rq.SkillTableId, ok = tick.Blackboard.Get(s.SkillIDBoard, "", "").(uint32)
	if !ok {
		zap.L().Error("Failed to cast skill id from blackboard",
			zap.String("Key", s.SkillIDBoard),
			zap.Any("Value", tick.Blackboard.GetMem(s.SkillIDBoard)))
		return b3.FAILURE
	}

	rq.TargetId = tick.Blackboard.GetUInt64(HatredTargetBoardKey, "", "")

	zap.L().Info("Sending skill release request", zap.Uint32("SkillTableId", rq.SkillTableId))
	client.Send(rq, game.ClientPlayerSkillServiceReleaseSkillMessageId)

	return b3.SUCCESS
}
