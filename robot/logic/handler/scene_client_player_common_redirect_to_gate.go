package handler

import (
	"log"

	"proto/scene"
	"robot/logic/gameobject"
)

func SceneClientPlayerCommonRedirectToGateHandler(player *gameobject.Player, response *scene.RedirectToGateNotify) {
	log.Printf("[Redirect] Player %d redirected to gate %s:%d",
		player.ID, response.GetTargetIp(), response.GetTargetPort())
}
