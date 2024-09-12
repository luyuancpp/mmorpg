package handler

import (
	"github.com/golang/protobuf/proto"
	"go.uber.org/zap"
	"robot/logic"
	"robot/pb/game"
	"robot/pkg"
)

func MessageBodyHandler(client *pkg.GameClient, response *game.MessageBody) {
	// Log the incoming message body for debugging
	zap.L().Debug("Received message body", zap.String("response", response.String()))

	// Retrieve the player from the player list
	player, ok := logic.PlayerList.Get(client.PlayerId)
	if !ok {
		zap.L().Error("Player not found", zap.Uint64("player_id", client.PlayerId))
		return
	}

	// Handle different message types
	switch response.MessageId {
	case game.PlayerSkillServiceReleaseSkillMessageId:
		handlePlayerSkillServiceReleaseSkill(player, response.Body)
	case game.PlayerSkillServiceNotifySkillUsedMessageId:
		handlePlayerSkillServiceNotifySkillUsed(player, response.Body)
	case game.PlayerSkillServiceNotifySkillInterruptedMessageId:
		handlePlayerSkillServiceNotifySkillInterrupted(player, response.Body)
	case game.PlayerSkillServiceGetSkillListMessageId:
		handlePlayerSkillServiceGetSkillList(player, response.Body)
	case game.PlayerClientCommonServiceSendTipToClientMessageId:
		handlePlayerClientCommonServiceSendTipToClient(player, response.Body)
	case game.PlayerClientCommonServiceKickPlayerMessageId:
		handlePlayerClientCommonServiceKickPlayer(player, response.Body)
	case game.ClientPlayerSceneServiceEnterSceneMessageId:
		handleClientPlayerSceneServiceEnterScene(player, response.Body)
	case game.ClientPlayerSceneServiceNotifyEnterSceneMessageId:
		handleClientPlayerSceneServiceNotifyEnterScene(player, response.Body)
	case game.ClientPlayerSceneServiceSceneInfoC2SMessageId:
		handleClientPlayerSceneServiceSceneInfoC2S(player, response.Body)
	case game.ClientPlayerSceneServiceNotifySceneInfoMessageId:
		handleClientPlayerSceneServiceNotifySceneInfo(player, response.Body)
	case game.ClientPlayerSceneServiceNotifyActorCreateMessageId:
		handleClientPlayerSceneServiceNotifyActorCreate(player, response.Body)
	case game.ClientPlayerSceneServiceNotifyActorDestroyMessageId:
		handleClientPlayerSceneServiceNotifyActorDestroy(player, response.Body)
	case game.ClientPlayerSceneServiceNotifyActorListCreateMessageId:
		handleClientPlayerSceneServiceNotifyActorListCreate(player, response.Body)
	case game.ClientPlayerSceneServiceNotifyActorListDestroyMessageId:
		handleClientPlayerSceneServiceNotifyActorListDestroy(player, response.Body)
	default:
		// Handle unknown message IDs
		zap.L().Info("Unhandled message", zap.Uint32("message_id", response.MessageId), zap.String("response", response.String()))
	}
}
func handlePlayerSkillServiceReleaseSkill(player *logic.Player, body []byte) {
	message := &game.ReleaseSkillSkillResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal ReleaseSkillSkillResponse", zap.Error(err))
		return
	}
	PlayerSkillServiceReleaseSkillHandler(player, message)
}
func handlePlayerSkillServiceNotifySkillUsed(player *logic.Player, body []byte) {
	message := &game.SkillUsedS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal SkillUsedS2C", zap.Error(err))
		return
	}
	PlayerSkillServiceNotifySkillUsedHandler(player, message)
}
func handlePlayerSkillServiceNotifySkillInterrupted(player *logic.Player, body []byte) {
	message := &game.SkillInterruptedS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal SkillInterruptedS2C", zap.Error(err))
		return
	}
	PlayerSkillServiceNotifySkillInterruptedHandler(player, message)
}
func handlePlayerSkillServiceGetSkillList(player *logic.Player, body []byte) {
	message := &game.GetSkillListResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal GetSkillListResponse", zap.Error(err))
		return
	}
	PlayerSkillServiceGetSkillListHandler(player, message)
}
func handlePlayerClientCommonServiceSendTipToClient(player *logic.Player, body []byte) {
	message := &game.TipInfoMessage{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal TipInfoMessage", zap.Error(err))
		return
	}
	PlayerClientCommonServiceSendTipToClientHandler(player, message)
}
func handlePlayerClientCommonServiceKickPlayer(player *logic.Player, body []byte) {
	message := &game.TipInfoMessage{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal TipInfoMessage", zap.Error(err))
		return
	}
	PlayerClientCommonServiceKickPlayerHandler(player, message)
}
func handleClientPlayerSceneServiceEnterScene(player *logic.Player, body []byte) {
	message := &game.EnterSceneC2SResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal EnterSceneC2SResponse", zap.Error(err))
		return
	}
	ClientPlayerSceneServiceEnterSceneHandler(player, message)
}
func handleClientPlayerSceneServiceNotifyEnterScene(player *logic.Player, body []byte) {
	message := &game.EnterSceneS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal EnterSceneS2C", zap.Error(err))
		return
	}
	ClientPlayerSceneServiceNotifyEnterSceneHandler(player, message)
}
func handleClientPlayerSceneServiceSceneInfoC2S(player *logic.Player, body []byte) {
	message := &game.SceneInfoRequest{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal SceneInfoRequest", zap.Error(err))
		return
	}
	ClientPlayerSceneServiceSceneInfoC2SHandler(player, message)
}
func handleClientPlayerSceneServiceNotifySceneInfo(player *logic.Player, body []byte) {
	message := &game.SceneInfoS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal SceneInfoS2C", zap.Error(err))
		return
	}
	ClientPlayerSceneServiceNotifySceneInfoHandler(player, message)
}
func handleClientPlayerSceneServiceNotifyActorCreate(player *logic.Player, body []byte) {
	message := &game.ActorCreateS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal ActorCreateS2C", zap.Error(err))
		return
	}
	ClientPlayerSceneServiceNotifyActorCreateHandler(player, message)
}
func handleClientPlayerSceneServiceNotifyActorDestroy(player *logic.Player, body []byte) {
	message := &game.ActorDestroyS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal ActorDestroyS2C", zap.Error(err))
		return
	}
	ClientPlayerSceneServiceNotifyActorDestroyHandler(player, message)
}
func handleClientPlayerSceneServiceNotifyActorListCreate(player *logic.Player, body []byte) {
	message := &game.ActorListCreateS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal ActorListCreateS2C", zap.Error(err))
		return
	}
	ClientPlayerSceneServiceNotifyActorListCreateHandler(player, message)
}
func handleClientPlayerSceneServiceNotifyActorListDestroy(player *logic.Player, body []byte) {
	message := &game.ActorListDestroyS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal ActorListDestroyS2C", zap.Error(err))
		return
	}
	ClientPlayerSceneServiceNotifyActorListDestroyHandler(player, message)
}
