package handler

import (
	"robot/pb/game"
	"robot/logic/gameobject"
)

func EntityStateSyncServiceSyncVelocityHandler(player *gameobject.Player, response *game.SyncEntityVelocityS2C) {