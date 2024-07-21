package handler

import (
	"client/logic"
	"client/pb/game"
	"log"
)

func TipS2CHandler(player *logic.Player, response *game.TipS2C) {
	log.Println(response)
}
