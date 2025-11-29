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
	case game.SceneClientPlayerCommonSendTipToClientMessageId:
		handleSceneClientPlayerCommonSendTipToClient(player, response.SerializedMessage)
	case game.SceneClientPlayerCommonKickPlayerMessageId:
		handleSceneClientPlayerCommonKickPlayer(player, response.SerializedMessage)
	case game.SceneSceneClientPlayerEnterSceneMessageId:
		handleSceneSceneClientPlayerEnterScene(player, response.SerializedMessage)
	case game.SceneSceneClientPlayerNotifyEnterSceneMessageId:
		handleSceneSceneClientPlayerNotifyEnterScene(player, response.SerializedMessage)
	case game.SceneSceneClientPlayerSceneInfoC2SMessageId:
		handleSceneSceneClientPlayerSceneInfoC2S(player, response.SerializedMessage)
	case game.SceneSceneClientPlayerNotifySceneInfoMessageId:
		handleSceneSceneClientPlayerNotifySceneInfo(player, response.SerializedMessage)
	case game.SceneSceneClientPlayerNotifyActorCreateMessageId:
		handleSceneSceneClientPlayerNotifyActorCreate(player, response.SerializedMessage)
	case game.SceneSceneClientPlayerNotifyActorDestroyMessageId:
		handleSceneSceneClientPlayerNotifyActorDestroy(player, response.SerializedMessage)
	case game.SceneSceneClientPlayerNotifyActorListCreateMessageId:
		handleSceneSceneClientPlayerNotifyActorListCreate(player, response.SerializedMessage)
	case game.SceneSceneClientPlayerNotifyActorListDestroyMessageId:
		handleSceneSceneClientPlayerNotifyActorListDestroy(player, response.SerializedMessage)
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
func handleClientPlayerLoginLogin(player *gameobject.Player, body []byte) {
	message := &LoginResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal LoginResponse", zap.Error(err))
		return
	}
	ClientPlayerLoginLoginHandler(player, message)
}
func handleClientPlayerLoginCreatePlayer(player *gameobject.Player, body []byte) {
	message := &CreatePlayerResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal CreatePlayerResponse", zap.Error(err))
		return
	}
	ClientPlayerLoginCreatePlayerHandler(player, message)
}
func handleClientPlayerLoginEnterGame(player *gameobject.Player, body []byte) {
	message := &EnterGameResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal EnterGameResponse", zap.Error(err))
		return
	}
	ClientPlayerLoginEnterGameHandler(player, message)
}
func handleClientPlayerLoginLeaveGame(player *gameobject.Player, body []byte) {
	message := &LeaveGameRequest{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal LeaveGameRequest", zap.Error(err))
		return
	}
	ClientPlayerLoginLeaveGameHandler(player, message)
}
func handleClientPlayerLoginDisconnect(player *gameobject.Player, body []byte) {
	message := &LoginNodeDisconnectRequest{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal LoginNodeDisconnectRequest", zap.Error(err))
		return
	}
	ClientPlayerLoginDisconnectHandler(player, message)
}
func handleSceneClientPlayerCommonSendTipToClient(player *gameobject.Player, body []byte) {
	message := &TipInfoMessage{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal TipInfoMessage", zap.Error(err))
		return
	}
	SceneClientPlayerCommonSendTipToClientHandler(player, message)
}
func handleSceneClientPlayerCommonKickPlayer(player *gameobject.Player, body []byte) {
	message := &GameKickPlayerRequest{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal GameKickPlayerRequest", zap.Error(err))
		return
	}
	SceneClientPlayerCommonKickPlayerHandler(player, message)
}
func handleSceneSceneClientPlayerEnterScene(player *gameobject.Player, body []byte) {
	message := &EnterSceneC2SResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal EnterSceneC2SResponse", zap.Error(err))
		return
	}
	SceneSceneClientPlayerEnterSceneHandler(player, message)
}
func handleSceneSceneClientPlayerNotifyEnterScene(player *gameobject.Player, body []byte) {
	message := &EnterSceneS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal EnterSceneS2C", zap.Error(err))
		return
	}
	SceneSceneClientPlayerNotifyEnterSceneHandler(player, message)
}
func handleSceneSceneClientPlayerSceneInfoC2S(player *gameobject.Player, body []byte) {
	message := &SceneInfoRequest{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal SceneInfoRequest", zap.Error(err))
		return
	}
	SceneSceneClientPlayerSceneInfoC2SHandler(player, message)
}
func handleSceneSceneClientPlayerNotifySceneInfo(player *gameobject.Player, body []byte) {
	message := &SceneInfoS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal SceneInfoS2C", zap.Error(err))
		return
	}
	SceneSceneClientPlayerNotifySceneInfoHandler(player, message)
}
func handleSceneSceneClientPlayerNotifyActorCreate(player *gameobject.Player, body []byte) {
	message := &ActorCreateS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal ActorCreateS2C", zap.Error(err))
		return
	}
	SceneSceneClientPlayerNotifyActorCreateHandler(player, message)
}
func handleSceneSceneClientPlayerNotifyActorDestroy(player *gameobject.Player, body []byte) {
	message := &ActorDestroyS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal ActorDestroyS2C", zap.Error(err))
		return
	}
	SceneSceneClientPlayerNotifyActorDestroyHandler(player, message)
}
func handleSceneSceneClientPlayerNotifyActorListCreate(player *gameobject.Player, body []byte) {
	message := &ActorListCreateS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal ActorListCreateS2C", zap.Error(err))
		return
	}
	SceneSceneClientPlayerNotifyActorListCreateHandler(player, message)
}
func handleSceneSceneClientPlayerNotifyActorListDestroy(player *gameobject.Player, body []byte) {
	message := &ActorListDestroyS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal ActorListDestroyS2C", zap.Error(err))
		return
	}
	SceneSceneClientPlayerNotifyActorListDestroyHandler(player, message)
}
func handleSceneSkillClientPlayerReleaseSkill(player *gameobject.Player, body []byte) {
	message := &ReleaseSkillSkillResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal ReleaseSkillSkillResponse", zap.Error(err))
		return
	}
	SceneSkillClientPlayerReleaseSkillHandler(player, message)
}
func handleSceneSkillClientPlayerNotifySkillUsed(player *gameobject.Player, body []byte) {
	message := &SkillUsedS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal SkillUsedS2C", zap.Error(err))
		return
	}
	SceneSkillClientPlayerNotifySkillUsedHandler(player, message)
}
func handleSceneSkillClientPlayerNotifySkillInterrupted(player *gameobject.Player, body []byte) {
	message := &SkillInterruptedS2C{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal SkillInterruptedS2C", zap.Error(err))
		return
	}
	SceneSkillClientPlayerNotifySkillInterruptedHandler(player, message)
}
func handleSceneSkillClientPlayerGetSkillList(player *gameobject.Player, body []byte) {
	message := &GetSkillListResponse{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal GetSkillListResponse", zap.Error(err))
		return
	}
	SceneSkillClientPlayerGetSkillListHandler(player, message)
}
