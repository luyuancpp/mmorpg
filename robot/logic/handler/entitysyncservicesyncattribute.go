package handler

import (
	"robot/pb/game"
	"robot/logic/gameobject"
)

func EntitySyncServiceSyncAttributeHandler(player *gameobject.Player, response *game.AttributeDelta) {