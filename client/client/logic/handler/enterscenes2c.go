package handler

import (
	"client/pb/game"
	"client/pkg"
	"log"
)

func EnterSceneS2CHandler(client *pkg.GameClient, response *game.EnterSceneS2C) {
	log.Println(response)
}
