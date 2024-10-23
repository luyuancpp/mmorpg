package handler

import (
	"robot/pb/game"
	"robot/logic/gameobject"
)

func EntitySyncServiceSyncAttribute5FramesHandler(player *gameobject.Player, response *game.AttributeDelta5Frames) {