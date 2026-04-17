package handler

import (
	"proto/scene"
	"robot/logic/gameobject"
)

func SceneSceneClientPlayerNotifyActorDestroyHandler(player *gameobject.Player, response *scene.ActorDestroyS2C) {
	player.RemoveEntity(response.Entity)
}
