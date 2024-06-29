package handler

import (
	"client/logic"
	"client/pb/game"
	"client/pkg"
)

func EnterSceneS2CHandler(client *pkg.GameClient, response *game.EnterSceneS2C) {
	logic.GMainPlayer.SceneId = response.SceneInfo.Guid
	logic.GMainPlayer.Send(&game.SceneInfoRequest{}, 23)
}
