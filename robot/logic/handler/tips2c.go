package handler

import (
	"go.uber.org/zap"
	"robot/logic"
	"robot/pb/game"
)

func TipS2CHandler(player *logic.Player, response *game.TipInfoMessage) {
	zap.L().Info("tip", zap.String("response", response.String()))
}
