package handler

import (
	"robot/pb/game"
	"robot/logic/gameobject"
)

func EntityStateSyncServiceSyncStateAttributeHandler(player *gameobject.Player, response *game.SyncStateAttributeS2C) {