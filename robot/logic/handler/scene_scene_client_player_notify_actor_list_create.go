package handler

import (
	"proto/scene"
	"robot/logic/gameobject"
)

func SceneSceneClientPlayerNotifyActorListCreateHandler(player *gameobject.Player, response *scene.ActorListCreateS2C) {
	for _, actor := range response.ActorList {
		player.AddEntity(actor.Entity)
	}
}
