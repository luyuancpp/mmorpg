package handler

import (
	"robot/pb/game"
	"robot/logic/gameobject"
)

func EntitySyncServiceSyncBaseAttributeHandler(player *gameobject.Player, response *game.BaseAttributeDeltaS2C) {