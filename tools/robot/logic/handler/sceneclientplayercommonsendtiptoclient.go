package handler

import (
	"go.uber.org/zap"
	"robot/logic/gameobject"
	"robot/proto/common"
)

func SceneClientPlayerCommonSendTipToClientHandler(player *gameobject.Player, response *common.TipInfoMessage) {
	zap.L().Info("tip", zap.String("response", response.String()))
}
