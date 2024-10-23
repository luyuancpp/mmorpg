package handler

import (
	"robot/pb/game"
	"robot/logic/gameobject"
)

func EntityStateSyncServiceSyncBaseStateAttributeHandler(player *gameobject.Player, response *game.SyncBaseStateAttributeS2C) {