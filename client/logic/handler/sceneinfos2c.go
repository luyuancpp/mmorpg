package handler

import (
	"client/logic"
	"client/pb/game"
	"log"
	"math/rand"
)

func SceneInfoS2CHandler(player *logic.Player, response *game.SceneInfoS2C) {
	req := &game.EnterSceneC2SRequest{}
	randomIndex := rand.Intn(len(response.SceneInfo))
	req.SceneInfo = response.SceneInfo[randomIndex]
	for player.SceneId == req.SceneInfo.Guid {
		randomIndex := rand.Intn(len(response.SceneInfo))
		req.SceneInfo = response.SceneInfo[randomIndex]
	}
	log.Println("SceneInfo ", player.Client.PlayerId, req)
	player.Client.Send(req, 16)
}
