package handler

import (
	"client/logic"
	"client/pb/game"
	"go.uber.org/zap"
)

func TipS2CHandler(player *logic.Player, response *game.TipS2C) {
	zap.L().Info("tip", zap.String("response", response.String()))
}
