package handler

import (
	"github.com/golang/protobuf/proto"
	"go.uber.org/zap"
	"robot/logic/gameobject"
	"robot/pb/game"
	"robot/pkg"
)

func MessageBodyHandler(client *pkg.GameClient, response *game.MessageContent) {
	// Log the incoming message body for debugging
	zap.L().Debug("Received message body", zap.String("response", response.String()))

	// Retrieve the player from the player list
	player, ok := gameobject.PlayerList.Get(client.PlayerId)
	if !ok {
		zap.L().Error("Player not found", zap.Uint64("player_id", client.PlayerId))
		return
	}

	// Handle different message types
	switch response.MessageId {
	case game.PlayerClientCommonServiceSendTipToClientMessageId:
		handlePlayerClientCommonServiceSendTipToClient(player, response.SerializedMessage)
	case game.PlayerClientCommonServiceKickPlayerMessageId:
		handlePlayerClientCommonServiceKickPlayer(player, response.SerializedMessage)
	default:
		// Handle unknown message IDs
		zap.L().Info("Unhandled message", zap.Uint32("message_id", response.MessageId), zap.String("response", response.String()))
	}
}
func handlePlayerClientCommonServiceSendTipToClient(player *gameobject.Player, body []byte) {
	message := &game.TipInfoMessage{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal TipInfoMessage", zap.Error(err))
		return
	}
	PlayerClientCommonServiceSendTipToClientHandler(player, message)
}
func handlePlayerClientCommonServiceKickPlayer(player *gameobject.Player, body []byte) {
	message := &game.TipInfoMessage{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal TipInfoMessage", zap.Error(err))
		return
	}
	PlayerClientCommonServiceKickPlayerHandler(player, message)
}
