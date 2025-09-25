package handler

import (
	"go.uber.org/zap"
	"google.golang.org/protobuf/encoding/protojson"
	"robot/logic/behaviortree"
	"robot/pb/game"
	"robot/pkg"
)

func ClientPlayerLoginCreatePlayerHandler(client *pkg.GameClient, response *game.CreatePlayerResponse) {
	if response.ErrorMessage != nil && response.ErrorMessage.Id > 0 {
		zap.L().Error("failed to create player",
			zap.Uint32("error_id", response.ErrorMessage.Id),
			zap.String("error_message", response.ErrorMessage.String()),
		)
		return
	}

	client.Blackboard.SetMem(behaviortree.PlayerListBoardKey, response.Players)

	jsonData, err := protojson.Marshal(response)
	if err != nil {
		zap.L().Error("failed to marshal protobuf response", zap.Error(err))
	} else {
		zap.L().Info("create player success", zap.String("response", string(jsonData)))
	}
}
