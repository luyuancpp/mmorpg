package handler

import (
	"client/logic"
	"client/logic/behaviortree"
	"client/pb/game"
	"client/pkg"
	"github.com/mohae/deepcopy"
	"go.uber.org/zap"
	"math/rand"
)

func SceneInfoS2CHandler(player *logic.Player, response *game.SceneInfoS2C) {
	client := player.Client.(*pkg.GameClient)
	sceneInfo := deepcopy.Copy(response.SceneInfo).([]*game.SceneInfoPBComp)
	client.Blackboard.SetMem(behaviortree.SceneInfo, sceneInfo)
	rq := &game.EnterSceneC2SRequest{}
	randomIndex := rand.Intn(len(response.SceneInfo))
	rq.SceneInfo = response.SceneInfo[randomIndex]
	for player.SceneId == rq.SceneInfo.Guid {
		randomIndex := rand.Intn(len(response.SceneInfo))
		rq.SceneInfo = response.SceneInfo[randomIndex]
	}
	zap.L().Debug("enter scene ", zap.String("request", rq.String()))
	player.Client.Send(rq, game.ClientPlayerSceneServiceEnterSceneMessageId)
}
