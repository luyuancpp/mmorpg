package handler

import (
	"robot/pb/game"
	"robot/logic/gameobject"
)

func EntityStateSyncServiceSyncEntityStateHandler(player *gameobject.Player, response *game.SyncBaseStateAttributeDeltaS2C) {