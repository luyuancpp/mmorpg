package handler

import (
	"client/logic"
	"client/pb/game"
	"client/pkg"
	"log"
	"math/rand"
)

func SceneInfoS2CHandler(client *pkg.GameClient, response *game.SceneInfoS2C) {
	req := &game.EnterSceneC2SRequest{}
	randomIndex := rand.Intn(len(response.SceneInfo))
	req.SceneInfo = response.SceneInfo[randomIndex]
	for logic.GMainPlayer.SceneId == req.SceneInfo.Guid {
		randomIndex := rand.Intn(len(response.SceneInfo))
		req.SceneInfo = response.SceneInfo[randomIndex]
	}
	log.Println(req)
	logic.GMainPlayer.Send(req, 16)
}
