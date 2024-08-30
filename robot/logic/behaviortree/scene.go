package behaviortree

import (
	b3 "github.com/magicsea/behavior3go"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
	"go.uber.org/zap"
	"math/rand"
	"robot/config"
	"robot/interfaces"
	"robot/logic"
	"robot/pb/game"
)

type RandomEnterScene struct {
	Action
}

func (this *RandomEnterScene) Initialize(setting *BTNodeCfg) {
	this.Action.Initialize(setting)
}

func (this *RandomEnterScene) OnTick(tick *Tick) b3.Status {
	// 从黑板中获取客户端
	client, ok := tick.Blackboard.GetMem(ClientIdentifier).(interfaces.GameClientInterface)
	if !ok {
		zap.L().Error("Failed to cast client from blackboard", zap.Any(ClientIdentifier, tick.Blackboard.GetMem(ClientIdentifier)))
		return b3.FAILURE
	}

	sceneInfo, ok := tick.Blackboard.GetMem(SceneInformationKey).([]*game.SceneInfoPBComp)
	if !ok {
		zap.L().Debug("Failed to cast scene info  from blackboard", zap.Any(PlayerListIdentifier, tick.Blackboard.GetMem(PlayerListIdentifier)))
		return b3.FAILURE
	}

	player, ok := logic.PlayerList.Get(client.GetPlayerId())
	if !ok {
		zap.L().Error("Failed to get player player id :", zap.Any(ClientIdentifier, client.GetPlayerId()))
		return b3.FAILURE
	}

	rq := &game.EnterSceneC2SRequest{}
	randomIndex := rand.Intn(len(sceneInfo))
	rq.SceneInfo = sceneInfo[randomIndex]
	for player.SceneId == rq.SceneInfo.Guid {
		randomIndex := rand.Intn(len(sceneInfo))
		rq.SceneInfo = sceneInfo[randomIndex]
	}

	client.Send(rq, game.ClientPlayerSceneServiceEnterSceneMessageId)

	tick.Blackboard.SetMem(SceneInformationKey, nil)

	return b3.SUCCESS
}

type CheckSceneSwitchCount struct {
	Action
}

func (this *CheckSceneSwitchCount) Initialize(setting *BTNodeCfg) {
	this.Action.Initialize(setting)
}

func (this *CheckSceneSwitchCount) OnTick(tick *Tick) b3.Status {
	count := tick.Blackboard.GetInt32(SceneSwitchCountKey, "", "")

	if count >= config.AppConfig.Robots.SceneSwitchCount {
		return b3.FAILURE
	}

	return b3.SUCCESS
}

type AddSceneSwitchCount struct {
	Action
}

func (this *AddSceneSwitchCount) Initialize(setting *BTNodeCfg) {
	this.Action.Initialize(setting)
}

func (this *AddSceneSwitchCount) OnTick(tick *Tick) b3.Status {
	count := tick.Blackboard.GetInt32(SceneSwitchCountKey, "", "")
	tick.Blackboard.Set(SceneSwitchCountKey, count+1, "", "")
	return b3.SUCCESS
}
