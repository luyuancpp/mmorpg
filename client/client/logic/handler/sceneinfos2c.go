package handler

import (
	"client/pb/game"
	"client/pkg"
	"log"
)

func SceneInfoS2CHandler(client *pkg.GameClient, response *game.SceneInfoS2C) {
	log.Println(response)
}
