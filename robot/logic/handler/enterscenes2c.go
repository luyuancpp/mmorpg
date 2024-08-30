package handler

import (
	"robot/logic"
	"robot/pb/game"
)

func EnterSceneS2CHandler(player *logic.Player, response *game.EnterSceneS2C) {
	player.SceneId = response.SceneInfo.Guid
	player.Client.Send(&game.SceneInfoRequest{}, game.ClientPlayerSceneServiceSceneInfoC2SMessageId)
}
