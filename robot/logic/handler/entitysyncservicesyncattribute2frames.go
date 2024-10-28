package handler

import (
	"robot/pb/game"
	"robot/logic/gameobject"
)

func EntitySyncServiceSyncAttribute2FramesHandler(player *gameobject.Player, response *game.AttributeDelta2FramesS2C) {