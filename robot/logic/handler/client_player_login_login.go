package handler

import (
	"go.uber.org/zap"

	"robot/logic/gameobject"
	"robot/proto/login"
)

func ClientPlayerLoginLoginHandler(player *gameobject.Player, response *login.LoginResponse) {
	zap.L().Debug("login response (dispatch)", zap.Uint64("player", player.PlayerId), zap.Int("players", len(response.GetPlayers())))
}
