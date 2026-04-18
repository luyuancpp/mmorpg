package handler

import (
	"proto/scene"
	"robot/logic/gameobject"
)

func SceneSceneClientPlayerNotifyActorListCreateHandler(player *gameobject.Player, response *scene.ActorListCreateS2C) {
	for _, actor := range response.ActorList {
		if actor.Guid == player.ID {
			player.SetEntityID(actor.Entity)
		}
		player.AddEntity(actor.Entity)
	}
}
