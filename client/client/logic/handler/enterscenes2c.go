package handler

import (
	"client/logic"
	"client/pb/game"
	"client/pkg"
)

func EnterSceneS2CHandler(client *pkg.GameClient, response *game.EnterSceneS2C) {
	logic.MainPlayer.Send(&game.SceneInfoRequest{}, 23)
}
