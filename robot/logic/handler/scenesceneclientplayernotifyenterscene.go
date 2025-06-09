package handler

import (
	"robot/logic/gameobject"
	"robot/pb/game"
)

func SceneSceneClientPlayerNotifyEnterSceneHandler(player *gameobject.Player, response *game.EnterSceneS2C) {

	oldSceneID := player.SceneID

	player.SceneID = response.SceneInfo.Guid
	player.Client.Send(&game.SceneInfoRequest{}, game.SceneSceneClientPlayerSceneInfoC2SMessageId)

	player.HandleServerSceneTransitionSuccess()

	if oldSceneID <= 0 {
		player.SetupPlayerSkillsOnLogin()
	}
}
