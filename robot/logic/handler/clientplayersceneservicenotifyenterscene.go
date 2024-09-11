package handler

import (
	"robot/logic/gameobject"
	"robot/pb/game"
)

func ClientPlayerSceneServiceNotifyEnterSceneHandler(player *gameobject.Player, response *game.EnterSceneS2C) {
	player.SceneId = response.SceneInfo.Guid
	player.Client.Send(&game.SceneInfoRequest{}, game.ClientPlayerSceneServiceSceneInfoC2SMessageId)

	player.HandleServerSceneTransitionSuccess()
}
