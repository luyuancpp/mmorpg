package handler

import (
	"client/logic"
	"client/pb/game"
	"go.uber.org/zap"
	"math/rand"
)

func SceneInfoS2CHandler(player *logic.Player, response *game.SceneInfoS2C) {
	rq := &game.EnterSceneC2SRequest{}
	randomIndex := rand.Intn(len(response.SceneInfo))
	rq.SceneInfo = response.SceneInfo[randomIndex]
	for player.SceneId == rq.SceneInfo.Guid {
		randomIndex := rand.Intn(len(response.SceneInfo))
		rq.SceneInfo = response.SceneInfo[randomIndex]
	}
	zap.L().Debug("enter scene ", zap.String("request", rq.String()))
	player.Client.Send(rq, 16)
}
