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
	case game.ClientPlayerLoginLoginMessageId:
		handleClientPlayerLoginLogin(player, response.SerializedMessage)
	case game.ClientPlayerLoginCreatePlayerMessageId:
		handleClientPlayerLoginCreatePlayer(player, response.SerializedMessage)
	case game.ClientPlayerLoginEnterGameMessageId:
		handleClientPlayerLoginEnterGame(player, response.SerializedMessage)
	case game.ClientPlayerLoginLeaveGameMessageId:
		handleClientPlayerLoginLeaveGame(player, response.SerializedMessage)
	case game.ClientPlayerLoginDisconnectMessageId:
		handleClientPlayerLoginDisconnect(player, response.SerializedMessage)
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
func handleClientPlayerLoginLogin(player *gameobject.Player, body []byte) {
	message := &game.LoginResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal LoginResponse", zap.Error(err))
		return
	}
	ClientPlayerLoginLoginHandler(player, message)
}
func handleClientPlayerLoginCreatePlayer(player *gameobject.Player, body []byte) {
	message := &game.CreatePlayerResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal CreatePlayerResponse", zap.Error(err))
		return
	}
	ClientPlayerLoginCreatePlayerHandler(player, message)
}
func handleClientPlayerLoginEnterGame(player *gameobject.Player, body []byte) {
	message := &game.EnterGameResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal EnterGameResponse", zap.Error(err))
		return
	}
	ClientPlayerLoginEnterGameHandler(player, message)
}
func handleClientPlayerLoginLeaveGame(player *gameobject.Player, body []byte) {
	message := &game.LeaveGameRequest{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal LeaveGameRequest", zap.Error(err))
		return
	}
	ClientPlayerLoginLeaveGameHandler(player, message)
}
func handleClientPlayerLoginDisconnect(player *gameobject.Player, body []byte) {
	message := &game.LoginNodeDisconnectRequest{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal LoginNodeDisconnectRequest", zap.Error(err))
		return
	}
	ClientPlayerLoginDisconnectHandler(player, message)
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
