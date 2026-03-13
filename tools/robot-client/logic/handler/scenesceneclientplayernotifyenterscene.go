package handler

import (
	"robot/generated/pb/game"
	"robot/logic/gameobject"
	"robot/proto/service/rpc/scene"
)

func SceneSceneClientPlayerNotifyEnterSceneHandler(player *gameobject.Player, response *scene.EnterSceneS2C) {

	oldSceneID := player.SceneID

	player.SceneID = response.SceneInfo.Guid
	player.Client.Send(&scene.SceneInfoRequest{}, game.SceneSceneClientPlayerSceneInfoC2SMessageId)

	player.HandleServerSceneTransitionSuccess()

	if oldSceneID <= 0 {
		player.SetupPlayerSkillsOnLogin()
	}
}
