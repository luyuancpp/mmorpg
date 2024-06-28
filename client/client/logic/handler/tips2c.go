package handler

import (
	"client/pb/game"
	"client/pkg"
	"log"
)

func TipS2CHandler(client *pkg.GameClient, response *game.TipS2C) {
	log.Println(response)
}
