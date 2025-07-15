package handler

import (
	"robot/logic/gameobject"
	"robot/pb/game"
)

func SceneClientPlayerCommonSendTipToClientHandler(player *gameobject.Player, response *game.TipInfoMessage) {
	zap.L().Info("tip", zap.String("response", response.String()))
	
}
