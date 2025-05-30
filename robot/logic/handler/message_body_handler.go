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
	case game.CentreClientPlayerCommonServiceSendTipToClientMessageId:
		handleCentreClientPlayerCommonServiceSendTipToClient(player, response.SerializedMessage)
	case game.CentreClientPlayerCommonServiceKickPlayerMessageId:
		handleCentreClientPlayerCommonServiceKickPlayer(player, response.SerializedMessage)
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
	case game.ClientPlayerSkillServiceReleaseSkillMessageId:
		handleClientPlayerSkillServiceReleaseSkill(player, response.SerializedMessage)
	case game.ClientPlayerSkillServiceNotifySkillUsedMessageId:
		handleClientPlayerSkillServiceNotifySkillUsed(player, response.SerializedMessage)
	case game.ClientPlayerSkillServiceNotifySkillInterruptedMessageId:
		handleClientPlayerSkillServiceNotifySkillInterrupted(player, response.SerializedMessage)
	case game.ClientPlayerSkillServiceGetSkillListMessageId:
		handleClientPlayerSkillServiceGetSkillList(player, response.SerializedMessage)
	case game.GameClientPlayerCommonServiceSendTipToClientMessageId:
		handleGameClientPlayerCommonServiceSendTipToClient(player, response.SerializedMessage)
	case game.GameClientPlayerCommonServiceKickPlayerMessageId:
		handleGameClientPlayerCommonServiceKickPlayer(player, response.SerializedMessage)
	case game.GamePlayerSceneServiceEnterSceneMessageId:
		handleGamePlayerSceneServiceEnterScene(player, response.SerializedMessage)
	case game.GamePlayerSceneServiceLeaveSceneMessageId:
		handleGamePlayerSceneServiceLeaveScene(player, response.SerializedMessage)
	case game.GamePlayerSceneServiceEnterSceneS2CMessageId:
		handleGamePlayerSceneServiceEnterSceneS2C(player, response.SerializedMessage)
	case game.GamePlayerServiceCentre2GsLoginMessageId:
		handleGamePlayerServiceCentre2GsLogin(player, response.SerializedMessage)
	case game.GamePlayerServiceExitGameMessageId:
		handleGamePlayerServiceExitGame(player, response.SerializedMessage)
	default:
		// Handle unknown message IDs
		zap.L().Info("Unhandled message", zap.Uint32("message_id", response.MessageId), zap.String("response", response.String()))
	}
}
func handleCentreClientPlayerCommonServiceSendTipToClient(player *gameobject.Player, body []byte) {
	message := &game.TipInfoMessage{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal TipInfoMessage", zap.Error(err))
		return
	}
	CentreClientPlayerCommonServiceSendTipToClientHandler(player, message)
}
func handleCentreClientPlayerCommonServiceKickPlayer(player *gameobject.Player, body []byte) {
	message := &game.CentreKickPlayerRequest{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal CentreKickPlayerRequest", zap.Error(err))
		return
	}
	CentreClientPlayerCommonServiceKickPlayerHandler(player, message)
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
func handleClientPlayerSkillServiceReleaseSkill(player *gameobject.Player, body []byte) {
	message := &game.ReleaseSkillSkillResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal ReleaseSkillSkillResponse", zap.Error(err))
		return
	}
	ClientPlayerSkillServiceReleaseSkillHandler(player, message)
}
func handleClientPlayerSkillServiceNotifySkillUsed(player *gameobject.Player, body []byte) {
	message := &game.SkillUsedS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal SkillUsedS2C", zap.Error(err))
		return
	}
	ClientPlayerSkillServiceNotifySkillUsedHandler(player, message)
}
func handleClientPlayerSkillServiceNotifySkillInterrupted(player *gameobject.Player, body []byte) {
	message := &game.SkillInterruptedS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal SkillInterruptedS2C", zap.Error(err))
		return
	}
	ClientPlayerSkillServiceNotifySkillInterruptedHandler(player, message)
}
func handleClientPlayerSkillServiceGetSkillList(player *gameobject.Player, body []byte) {
	message := &game.GetSkillListResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal GetSkillListResponse", zap.Error(err))
		return
	}
	ClientPlayerSkillServiceGetSkillListHandler(player, message)
}
func handleGameClientPlayerCommonServiceSendTipToClient(player *gameobject.Player, body []byte) {
	message := &game.TipInfoMessage{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal TipInfoMessage", zap.Error(err))
		return
	}
	GameClientPlayerCommonServiceSendTipToClientHandler(player, message)
}
func handleGameClientPlayerCommonServiceKickPlayer(player *gameobject.Player, body []byte) {
	message := &game.GameKickPlayerRequest{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal GameKickPlayerRequest", zap.Error(err))
		return
	}
	GameClientPlayerCommonServiceKickPlayerHandler(player, message)
}
func handleGamePlayerSceneServiceEnterScene(player *gameobject.Player, body []byte) {
	message := &game.GsEnterSceneRequest{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal GsEnterSceneRequest", zap.Error(err))
		return
	}
	GamePlayerSceneServiceEnterSceneHandler(player, message)
}
func handleGamePlayerSceneServiceLeaveScene(player *gameobject.Player, body []byte) {
	message := &game.GsLeaveSceneRequest{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal GsLeaveSceneRequest", zap.Error(err))
		return
	}
	GamePlayerSceneServiceLeaveSceneHandler(player, message)
}
func handleGamePlayerSceneServiceEnterSceneS2C(player *gameobject.Player, body []byte) {
	message := &game.EnterScenerS2CResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal EnterScenerS2CResponse", zap.Error(err))
		return
	}
	GamePlayerSceneServiceEnterSceneS2CHandler(player, message)
}
func handleGamePlayerServiceCentre2GsLogin(player *gameobject.Player, body []byte) {
	message := &game.Centre2GsLoginRequest{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal Centre2GsLoginRequest", zap.Error(err))
		return
	}
	GamePlayerServiceCentre2GsLoginHandler(player, message)
}
func handleGamePlayerServiceExitGame(player *gameobject.Player, body []byte) {
	message := &game.GameNodeExitGameRequest{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal GameNodeExitGameRequest", zap.Error(err))
		return
	}
	GamePlayerServiceExitGameHandler(player, message)
}
