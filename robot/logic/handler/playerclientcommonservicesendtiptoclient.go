package handler

import (
	"go.uber.org/zap"
	"robot/logic/gameobject"
	"robot/pb/game"
)

func PlayerClientCommonServiceSendTipToClientHandler(player *gameobject.Player, response *game.TipInfoMessage) {
	zap.L().Info("tip", zap.String("response", response.String()))
}
