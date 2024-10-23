package handler

import (
	"robot/pb/game"
	"robot/logic/gameobject"
)

func EntitySyncServiceSyncAttribute30FramesHandler(player *gameobject.Player, response *game.AttributeDelta30Frames) {