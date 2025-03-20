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
	case game.PlayerSkillServiceReleaseSkillMessageId:
		handlePlayerSkillServiceReleaseSkill(player, response.SerializedMessage)
	case game.PlayerSkillServiceNotifySkillUsedMessageId:
		handlePlayerSkillServiceNotifySkillUsed(player, response.SerializedMessage)
	case game.PlayerSkillServiceNotifySkillInterruptedMessageId:
		handlePlayerSkillServiceNotifySkillInterrupted(player, response.SerializedMessage)
	case game.PlayerSkillServiceGetSkillListMessageId:
		handlePlayerSkillServiceGetSkillList(player, response.SerializedMessage)
	case game.ClientPlayerSceneServiceEnterSceneMessageId:
		handleClientPlayerSceneServiceEnterScene(player, response.SerializedMessage)
	case game.ClientPlayerSceneServiceNotifyEnterSceneMessageId:
		handleClientPlayerSceneServiceNotifyEnterScene(player, response.SerializedMessage)
	case game.ClientPlayerSceneServiceSceneInfoC2SMessageId:
		handleClientPlayerSceneServiceSceneInfoC2S(player, response.SerializedMessage)
	case game.ClientPlayerSceneServiceNotifySceneInfoMessageId:
		handleClientPlayerSceneServiceNotifySceneInfo(player, response.SerializedMessage)
	case game.ClientPlayerSceneServiceNotifyActorCreateMessageId:
		handleClientPlayerSceneServiceNotifyActorCreate(player, response.SerializedMessage)
	case game.ClientPlayerSceneServiceNotifyActorDestroyMessageId:
		handleClientPlayerSceneServiceNotifyActorDestroy(player, response.SerializedMessage)
	case game.ClientPlayerSceneServiceNotifyActorListCreateMessageId:
		handleClientPlayerSceneServiceNotifyActorListCreate(player, response.SerializedMessage)
	case game.ClientPlayerSceneServiceNotifyActorListDestroyMessageId:
		handleClientPlayerSceneServiceNotifyActorListDestroy(player, response.SerializedMessage)
	case game.EntitySyncServiceSyncBaseAttributeMessageId:
		handleEntitySyncServiceSyncBaseAttribute(player, response.SerializedMessage)
	case game.EntitySyncServiceSyncAttribute2FramesMessageId:
		handleEntitySyncServiceSyncAttribute2Frames(player, response.SerializedMessage)
	case game.EntitySyncServiceSyncAttribute5FramesMessageId:
		handleEntitySyncServiceSyncAttribute5Frames(player, response.SerializedMessage)
	case game.EntitySyncServiceSyncAttribute10FramesMessageId:
		handleEntitySyncServiceSyncAttribute10Frames(player, response.SerializedMessage)
	case game.EntitySyncServiceSyncAttribute30FramesMessageId:
		handleEntitySyncServiceSyncAttribute30Frames(player, response.SerializedMessage)
	case game.EntitySyncServiceSyncAttribute60FramesMessageId:
		handleEntitySyncServiceSyncAttribute60Frames(player, response.SerializedMessage)
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
func handlePlayerSkillServiceReleaseSkill(player *gameobject.Player, body []byte) {
	message := &game.ReleaseSkillSkillResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal ReleaseSkillSkillResponse", zap.Error(err))
		return
	}
	PlayerSkillServiceReleaseSkillHandler(player, message)
}
func handlePlayerSkillServiceNotifySkillUsed(player *gameobject.Player, body []byte) {
	message := &game.SkillUsedS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal SkillUsedS2C", zap.Error(err))
		return
	}
	PlayerSkillServiceNotifySkillUsedHandler(player, message)
}
func handlePlayerSkillServiceNotifySkillInterrupted(player *gameobject.Player, body []byte) {
	message := &game.SkillInterruptedS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal SkillInterruptedS2C", zap.Error(err))
		return
	}
	PlayerSkillServiceNotifySkillInterruptedHandler(player, message)
}
func handlePlayerSkillServiceGetSkillList(player *gameobject.Player, body []byte) {
	message := &game.GetSkillListResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal GetSkillListResponse", zap.Error(err))
		return
	}
	PlayerSkillServiceGetSkillListHandler(player, message)
}
func handleClientPlayerSceneServiceEnterScene(player *gameobject.Player, body []byte) {
	message := &game.EnterSceneC2SResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal EnterSceneC2SResponse", zap.Error(err))
		return
	}
	ClientPlayerSceneServiceEnterSceneHandler(player, message)
}
func handleClientPlayerSceneServiceNotifyEnterScene(player *gameobject.Player, body []byte) {
	message := &game.EnterSceneS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal EnterSceneS2C", zap.Error(err))
		return
	}
	ClientPlayerSceneServiceNotifyEnterSceneHandler(player, message)
}
func handleClientPlayerSceneServiceSceneInfoC2S(player *gameobject.Player, body []byte) {
	message := &game.SceneInfoRequest{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal SceneInfoRequest", zap.Error(err))
		return
	}
	ClientPlayerSceneServiceSceneInfoC2SHandler(player, message)
}
func handleClientPlayerSceneServiceNotifySceneInfo(player *gameobject.Player, body []byte) {
	message := &game.SceneInfoS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal SceneInfoS2C", zap.Error(err))
		return
	}
	ClientPlayerSceneServiceNotifySceneInfoHandler(player, message)
}
func handleClientPlayerSceneServiceNotifyActorCreate(player *gameobject.Player, body []byte) {
	message := &game.ActorCreateS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal ActorCreateS2C", zap.Error(err))
		return
	}
	ClientPlayerSceneServiceNotifyActorCreateHandler(player, message)
}
func handleClientPlayerSceneServiceNotifyActorDestroy(player *gameobject.Player, body []byte) {
	message := &game.ActorDestroyS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal ActorDestroyS2C", zap.Error(err))
		return
	}
	ClientPlayerSceneServiceNotifyActorDestroyHandler(player, message)
}
func handleClientPlayerSceneServiceNotifyActorListCreate(player *gameobject.Player, body []byte) {
	message := &game.ActorListCreateS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal ActorListCreateS2C", zap.Error(err))
		return
	}
	ClientPlayerSceneServiceNotifyActorListCreateHandler(player, message)
}
func handleClientPlayerSceneServiceNotifyActorListDestroy(player *gameobject.Player, body []byte) {
	message := &game.ActorListDestroyS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal ActorListDestroyS2C", zap.Error(err))
		return
	}
	ClientPlayerSceneServiceNotifyActorListDestroyHandler(player, message)
}
func handleEntitySyncServiceSyncBaseAttribute(player *gameobject.Player, body []byte) {
	message := &game.BaseAttributeSyncDataS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal BaseAttributeSyncDataS2C", zap.Error(err))
		return
	}
	EntitySyncServiceSyncBaseAttributeHandler(player, message)
}
func handleEntitySyncServiceSyncAttribute2Frames(player *gameobject.Player, body []byte) {
	message := &game.AttributeDelta2FramesS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal AttributeDelta2FramesS2C", zap.Error(err))
		return
	}
	EntitySyncServiceSyncAttribute2FramesHandler(player, message)
}
func handleEntitySyncServiceSyncAttribute5Frames(player *gameobject.Player, body []byte) {
	message := &game.AttributeDelta5FramesS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal AttributeDelta5FramesS2C", zap.Error(err))
		return
	}
	EntitySyncServiceSyncAttribute5FramesHandler(player, message)
}
func handleEntitySyncServiceSyncAttribute10Frames(player *gameobject.Player, body []byte) {
	message := &game.AttributeDelta10FramesS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal AttributeDelta10FramesS2C", zap.Error(err))
		return
	}
	EntitySyncServiceSyncAttribute10FramesHandler(player, message)
}
func handleEntitySyncServiceSyncAttribute30Frames(player *gameobject.Player, body []byte) {
	message := &game.AttributeDelta30FramesS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal AttributeDelta30FramesS2C", zap.Error(err))
		return
	}
	EntitySyncServiceSyncAttribute30FramesHandler(player, message)
}
func handleEntitySyncServiceSyncAttribute60Frames(player *gameobject.Player, body []byte) {
	message := &game.AttributeDelta60FramesS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal AttributeDelta60FramesS2C", zap.Error(err))
		return
	}
	EntitySyncServiceSyncAttribute60FramesHandler(player, message)
}
