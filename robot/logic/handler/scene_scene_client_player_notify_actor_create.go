package handler

import (
	"proto/scene"
	"robot/logic/gameobject"
)

func SceneSceneClientPlayerNotifyActorCreateHandler(player *gameobject.Player, response *scene.ActorCreateS2C) {
	if response.Guid == player.ID {
		player.SetEntityID(response.Entity)
	}
	player.AddEntity(response.Entity)
}
