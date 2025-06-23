package handler

import (
	"go.uber.org/zap"
	"robot/logic/btree"
	"robot/logic/gameobject"
	"robot/pb/game"
	"robot/pkg"
)

func ClientPlayerLoginEnterGameHandler(client *pkg.GameClient, response *game.EnterGameResponse) {
	zap.L().Info("Player login", zap.Uint64("player id", response.PlayerId))
	player := gameobject.NewMainPlayer(response.PlayerId, client)
	gameobject.PlayerList.Set(response.PlayerId, player)
	btreePath := "etc/robot.b3"
	behaviorTree, currentTree, ok := btree.InitPlayerBehaviorTree(btreePath)
	if !ok {
		zap.L().Error("Failed to initialize behavior tree",
			zap.String("config_path", btreePath),
			zap.String("reason", "InitPlayerBehaviorTree returned false"))
		return
	}
	player.BehaviorTree = behaviorTree
	player.CurrentTree = currentTree
}
