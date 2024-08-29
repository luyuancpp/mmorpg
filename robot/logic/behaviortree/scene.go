package behaviortree

import (
	"client/interfaces"
	"client/logic"
	"client/pb/game"
	b3 "github.com/magicsea/behavior3go"
	"go.uber.org/zap"
	"math/rand"

	//. "github.com/magicsea/behavior3go/actions"
	//. "github.com/magicsea/behavior3go/composites"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
)

type RandomEnterScene struct {
	Action
}

func (this *RandomEnterScene) Initialize(setting *BTNodeCfg) {
	this.Action.Initialize(setting)
}

func (this *RandomEnterScene) OnTick(tick *Tick) b3.Status {
	// 从黑板中获取客户端
	client, ok := tick.Blackboard.GetMem(ClientName).(interfaces.GameClientInterface)
	if !ok {
		zap.L().Error("Failed to cast client from blackboard", zap.Any("client", tick.Blackboard.GetMem(ClientName)))
		return b3.FAILURE
	}

	sceneInfo, ok := tick.Blackboard.GetMem(SceneInfo).([]*game.SceneInfoPBComp)
	if !ok {
		zap.L().Error("Failed to cast scene info  from blackboard", zap.Any("client", tick.Blackboard.GetMem(SceneInfo)))
		return b3.FAILURE
	}

	player, ok := logic.PlayerList.Get(client.GetPlayerId())
	if !ok {
		zap.L().Error("Failed to get player player id :", zap.Any("client", client.GetPlayerId()))
		return b3.FAILURE
	}

	rq := &game.EnterSceneC2SRequest{}
	randomIndex := rand.Intn(len(sceneInfo))
	rq.SceneInfo = sceneInfo[randomIndex]

	for player.SceneId == rq.SceneInfo.Guid {
		randomIndex := rand.Intn(len(sceneInfo))
		rq.SceneInfo = sceneInfo[randomIndex]
	}

	zap.L().Info("enter scene ", zap.String("request", rq.String()))
	client.Send(rq, game.ClientPlayerSceneServiceEnterSceneMessageId)

	return b3.SUCCESS
}
