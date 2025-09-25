package behaviortree

import (
	b3 "github.com/magicsea/behavior3go"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
	"go.uber.org/zap"
	"math/rand"
	"robot/generated/pb/game"
	"robot/logic/gameobject"
	"robot/proto/common"
	"robot/proto/service/cpp/rpc/scene"
)

type RandomEnterScene struct {
	Action
}

func (res *RandomEnterScene) Initialize(setting *BTNodeCfg) {
	res.Action.Initialize(setting)
}

func (res *RandomEnterScene) OnTick(tick *Tick) b3.Status {
	sceneInfo, ok := tick.Blackboard.GetMem(SceneInformationBoardKey).([]*common.RoomInfoPBComponent)
	if !ok {
		zap.L().Debug("Failed to cast scene info  from blackboard", zap.Any(PlayerListBoardKey, tick.Blackboard.GetMem(PlayerListBoardKey)))
		return b3.FAILURE
	}

	player, ok := tick.Blackboard.GetMem(PlayerBoardKey).(*gameobject.Player)
	if !ok {
		zap.L().Error("Failed to get player player id :", zap.Any(PlayerBoardKey, tick.Blackboard.GetMem(PlayerBoardKey)))
		return b3.FAILURE
	}

	rq := &scene.EnterSceneC2SRequest{}
	randomIndex := rand.Intn(len(sceneInfo))
	rq.SceneInfo = sceneInfo[randomIndex]
	for player.SceneID == rq.SceneInfo.Guid {
		randomIndex := rand.Intn(len(sceneInfo))
		rq.SceneInfo = sceneInfo[randomIndex]
	}

	player.Send(rq, game.SceneSceneClientPlayerEnterSceneMessageId)

	tick.Blackboard.SetMem(SceneInformationBoardKey, nil)

	return b3.SUCCESS
}
