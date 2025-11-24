package handler

import (
	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"
	"robot/generated/pb/game"
	"robot/proto/common"

	"robot/logic/gameobject"
	"robot/pkg"
)

func MessageBodyHandler(client *pkg.GameClient, response *common.MessageContent) {
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
	case game.SceneClientPlayerCommonSendTipToClientMessageId:
		handleSceneClientPlayerCommonSendTipToClient(player, response.SerializedMessage)
	case game.SceneClientPlayerCommonKickPlayerMessageId:
		handleSceneClientPlayerCommonKickPlayer(player, response.SerializedMessage)
	case game.SceneSkillClientPlayerReleaseSkillMessageId:
		handleSceneSkillClientPlayerReleaseSkill(player, response.SerializedMessage)
	case game.SceneSkillClientPlayerNotifySkillUsedMessageId:
		handleSceneSkillClientPlayerNotifySkillUsed(player, response.SerializedMessage)
	case game.SceneSkillClientPlayerNotifySkillInterruptedMessageId:
		handleSceneSkillClientPlayerNotifySkillInterrupted(player, response.SerializedMessage)
	case game.SceneSkillClientPlayerGetSkillListMessageId:
		handleSceneSkillClientPlayerGetSkillList(player, response.SerializedMessage)
	default:
		// Handle unknown message IDs
		zap.L().Info("Unhandled message", zap.Uint32("message_id", response.MessageId), zap.String("response", response.String()))
	}
}
func handleSceneClientPlayerCommonSendTipToClient(player *gameobject.Player, body []byte) {
	message := &common.TipInfoMessage{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal common.TipInfoMessage", zap.Error(err))
		return
	}
	SceneClientPlayerCommonSendTipToClientHandler(player, message)
}
func handleSceneClientPlayerCommonKickPlayer(player *gameobject.Player, body []byte) {
	message := &scene.GameKickPlayerRequest{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal scene.GameKickPlayerRequest", zap.Error(err))
		return
	}
	SceneClientPlayerCommonKickPlayerHandler(player, message)
}
func handleSceneSkillClientPlayerReleaseSkill(player *gameobject.Player, body []byte) {
	message := &scene.ReleaseSkillSkillResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal scene.ReleaseSkillSkillResponse", zap.Error(err))
		return
	}
	SceneSkillClientPlayerReleaseSkillHandler(player, message)
}
func handleSceneSkillClientPlayerNotifySkillUsed(player *gameobject.Player, body []byte) {
	message := &scene.SkillUsedS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal scene.SkillUsedS2C", zap.Error(err))
		return
	}
	SceneSkillClientPlayerNotifySkillUsedHandler(player, message)
}
func handleSceneSkillClientPlayerNotifySkillInterrupted(player *gameobject.Player, body []byte) {
	message := &scene.SkillInterruptedS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal scene.SkillInterruptedS2C", zap.Error(err))
		return
	}
	SceneSkillClientPlayerNotifySkillInterruptedHandler(player, message)
}
func handleSceneSkillClientPlayerGetSkillList(player *gameobject.Player, body []byte) {
	message := &scene.GetSkillListResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal scene.GetSkillListResponse", zap.Error(err))
		return
	}
	SceneSkillClientPlayerGetSkillListHandler(player, message)
}
