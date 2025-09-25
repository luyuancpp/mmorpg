package handler

import (
	"go.uber.org/zap"
	"robot/logic/behaviortree"
	"robot/logic/btree"
	"robot/logic/gameobject"
	"robot/pkg"
	"robot/proto/service/go/grpc/login"
)

func ClientPlayerLoginEnterGameHandler(client *pkg.GameClient, response *login.EnterGameResponse) {
	if response.ErrorMessage.Id != 0 {
		zap.L().Error("received error response",
			zap.Uint32("error_id", response.ErrorMessage.Id),
			zap.String("error_message", response.ErrorMessage.String()), // 假设有 Message 字段
			zap.String("error_message", client.Account),
		)
		return
	}

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
	btree.InitializePlayerBehaviorTreeBlackboard(player.Blackboard)
	player.Blackboard.SetMem(behaviortree.ClientBoardKey, client)
	player.Blackboard.SetMem(behaviortree.PlayerBoardKey, player)
	client.Blackboard.SetMem(behaviortree.PlayerBoardKey, player)
}
