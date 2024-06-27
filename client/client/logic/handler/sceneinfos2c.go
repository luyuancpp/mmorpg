package handler

import (
	"client/logic"
	"client/pb/game"
	"client/pkg"
	"math/rand"
)

func SceneInfoS2CHandler(client *pkg.GameClient, response *game.SceneInfoS2C) {
	req := &game.EnterSceneC2SRequest{}
	randomIndex := rand.Intn(len(response.SceneInfo))
	req.SceneInfo = response.SceneInfo[randomIndex]
	logic.MainPlayer.Send(req, 16)
}
