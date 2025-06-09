package behaviortree

import (
	b3 "github.com/magicsea/behavior3go"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
	"go.uber.org/zap"
	"math/rand"
	"robot/interfaces"
	"robot/logic/gameobject"
	"robot/pb/game"
)

type RandomEnterScene struct {
	Action
}

func (res *RandomEnterScene) Initialize(setting *BTNodeCfg) {
	res.Action.Initialize(setting)
}

func (res *RandomEnterScene) OnTick(tick *Tick) b3.Status {
	// 从黑板中获取客户端
	client, ok := tick.Blackboard.GetMem(ClientBoardKey).(interfaces.GameClientInterface)
	if !ok {
		zap.L().Error("Failed to cast client from blackboard", zap.Any(ClientBoardKey, tick.Blackboard.GetMem(ClientBoardKey)))
		return b3.FAILURE
	}

	sceneInfo, ok := tick.Blackboard.GetMem(SceneInformationBoardKey).([]*game.SceneInfoPBComponent)
	if !ok {
		zap.L().Debug("Failed to cast scene info  from blackboard", zap.Any(PlayerListBoardKey, tick.Blackboard.GetMem(PlayerListBoardKey)))
		return b3.FAILURE
	}

	player, ok := gameobject.PlayerList.Get(client.GetPlayerId())
	if !ok {
		zap.L().Error("Failed to get player player id :", zap.Any(ClientBoardKey, client.GetPlayerId()))
		return b3.FAILURE
	}

	rq := &game.EnterSceneC2SRequest{}
	randomIndex := rand.Intn(len(sceneInfo))
	rq.SceneInfo = sceneInfo[randomIndex]
	for player.SceneID == rq.SceneInfo.Guid {
		randomIndex := rand.Intn(len(sceneInfo))
		rq.SceneInfo = sceneInfo[randomIndex]
	}

	client.Send(rq, game.SceneSceneClientPlayerEnterSceneMessageId)

	tick.Blackboard.SetMem(SceneInformationBoardKey, nil)

	return b3.SUCCESS
}
