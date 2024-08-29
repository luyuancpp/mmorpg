package handler

import (
	"client/logic"
	"client/pb/game"
	"client/pkg"
	"github.com/golang/protobuf/proto"
	"go.uber.org/zap"
)

// MessageBodyHandler 处理接收到的消息体
func MessageBodyHandler(client *pkg.GameClient, response *game.MessageBody) {
	// Log the incoming message body for debugging
	zap.L().Info("Received message body", zap.String("response", response.String()))

	// Retrieve the player from the player list
	player, ok := logic.PlayerList.Get(client.PlayerId)
	if !ok {
		zap.L().Error("Player not found", zap.Uint64("player_id", client.PlayerId))
		return
	}

	// Handle different message types
	switch response.MessageId {
	case game.ClientPlayerSceneServiceNotifySceneInfoMessageId:
		handleSceneInfoS2C(player, response.Body)
	case game.ClientPlayerSceneServiceNotifyEnterSceneMessageId:
		handleEnterSceneS2C(player, response.Body)
	case game.PlayerClientCommonServiceSendTipToClientMessageId:
		handleTipS2C(player, response.Body)
	case game.CentrePlayerSceneServiceEnterSceneMessageId:
		// Handle CentrePlayerSceneServiceEnterSceneMessageId if needed
		zap.L().Info("Received CentrePlayerSceneServiceEnterSceneMessageId, but handler is not implemented")
	case game.ClientPlayerSceneServiceNotifyActorListCreateMessageId:
	case game.ClientPlayerSceneServiceNotifyActorListDestroyMessageId:
	case game.ClientPlayerSceneServiceSceneInfoC2SMessageId:
	case game.ClientPlayerSceneServiceEnterSceneMessageId:
	case game.ClientPlayerSceneServiceNotifyActorDestroyMessageId:
	default:
		// Handle unknown message IDs
		zap.L().Info("Unhandled message", zap.Uint32("message_id", response.MessageId), zap.String("response", response.String()))
	}
}

// handleSceneInfoS2C 处理场景信息消息
func handleSceneInfoS2C(player *logic.Player, body []byte) {
	message := &game.SceneInfoS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal SceneInfoS2C", zap.Error(err))
		return
	}
	SceneInfoS2CHandler(player, message)
}

// handleEnterSceneS2C 处理进入场景消息
func handleEnterSceneS2C(player *logic.Player, body []byte) {
	message := &game.EnterSceneS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal EnterSceneS2C", zap.Error(err))
		return
	}
	EnterSceneS2CHandler(player, message)
}

// handleTipS2C 处理提示消息
func handleTipS2C(player *logic.Player, body []byte) {
	message := &game.TipInfoMessage{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal TipInfoMessage", zap.Error(err))
		return
	}
	TipS2CHandler(player, message)
}
