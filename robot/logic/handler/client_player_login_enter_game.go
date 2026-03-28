package handler

import (
	"go.uber.org/zap"

	"robot/logic/gameobject"
	"robot/proto/login"
)

func ClientPlayerLoginEnterGameHandler(player *gameobject.Player, response *login.EnterGameResponse) {
	zap.L().Debug("enter game response (dispatch)", zap.Uint64("player", player.PlayerId))
}
