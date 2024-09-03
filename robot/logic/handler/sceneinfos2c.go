package handler

import (
	"go.uber.org/zap"
	"robot/logic"
	"robot/logic/behaviortree"
	"robot/pb/game"
	"robot/pkg"
)

var i = 0

func SceneInfoS2CHandler(player *logic.Player, response *game.SceneInfoS2C) {
	client := player.Client.(*pkg.GameClient)
	client.Blackboard.SetMem(behaviortree.SceneInformationKey, response.SceneInfo)
	zap.L().Error("change scene :", zap.Any("time", i))
}
