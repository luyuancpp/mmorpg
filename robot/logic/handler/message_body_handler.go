package handler

import (
	"reflect"

	"robot/generated/pb/game"
	"robot/logic/gameobject"
	"robot/pkg"
	base "robot/proto/common/base"

	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"
)

type handlerFunc func(*gameobject.Player, []byte)

// unmarshalAndCall creates a handlerFunc that unmarshals body into a new
// message of type PT and forwards it to the typed handler function.
func unmarshalAndCall[PT proto.Message](fn func(*gameobject.Player, PT)) handlerFunc {
	var zero PT
	msgType := reflect.TypeOf(zero).Elem()
	return func(player *gameobject.Player, body []byte) {
		msg := reflect.New(msgType).Interface().(PT)
		if err := proto.Unmarshal(body, msg); err != nil {
			zap.L().Error("unmarshal failed", zap.Error(err))
			return
		}
		fn(player, msg)
	}
}

var messageHandlers = map[uint32]handlerFunc{
	game.ClientPlayerChatSendChatMessageId: unmarshalAndCall(ClientPlayerChatSendChatHandler),
	game.ClientPlayerChatPullChatHistoryMessageId: unmarshalAndCall(ClientPlayerChatPullChatHistoryHandler),
	game.ClientPlayerLoginLoginMessageId: unmarshalAndCall(ClientPlayerLoginLoginHandler),
	game.ClientPlayerLoginCreatePlayerMessageId: unmarshalAndCall(ClientPlayerLoginCreatePlayerHandler),
	game.ClientPlayerLoginEnterGameMessageId: unmarshalAndCall(ClientPlayerLoginEnterGameHandler),
	game.ClientPlayerLoginLeaveGameMessageId: unmarshalAndCall(ClientPlayerLoginLeaveGameHandler),
	game.ClientPlayerLoginDisconnectMessageId: unmarshalAndCall(ClientPlayerLoginDisconnectHandler),
	game.SceneClientPlayerCommonSendTipToClientMessageId: unmarshalAndCall(SceneClientPlayerCommonSendTipToClientHandler),
	game.SceneClientPlayerCommonKickPlayerMessageId: unmarshalAndCall(SceneClientPlayerCommonKickPlayerHandler),
	game.SceneSceneClientPlayerEnterSceneMessageId: unmarshalAndCall(SceneSceneClientPlayerEnterSceneHandler),
	game.SceneSceneClientPlayerNotifyEnterSceneMessageId: unmarshalAndCall(SceneSceneClientPlayerNotifyEnterSceneHandler),
	game.SceneSceneClientPlayerSceneInfoC2SMessageId: unmarshalAndCall(SceneSceneClientPlayerSceneInfoC2SHandler),
	game.SceneSceneClientPlayerNotifySceneInfoMessageId: unmarshalAndCall(SceneSceneClientPlayerNotifySceneInfoHandler),
	game.SceneSceneClientPlayerNotifyActorCreateMessageId: unmarshalAndCall(SceneSceneClientPlayerNotifyActorCreateHandler),
	game.SceneSceneClientPlayerNotifyActorDestroyMessageId: unmarshalAndCall(SceneSceneClientPlayerNotifyActorDestroyHandler),
	game.SceneSceneClientPlayerNotifyActorListCreateMessageId: unmarshalAndCall(SceneSceneClientPlayerNotifyActorListCreateHandler),
	game.SceneSceneClientPlayerNotifyActorListDestroyMessageId: unmarshalAndCall(SceneSceneClientPlayerNotifyActorListDestroyHandler),
	game.SceneSkillClientPlayerReleaseSkillMessageId: unmarshalAndCall(SceneSkillClientPlayerReleaseSkillHandler),
	game.SceneSkillClientPlayerNotifySkillUsedMessageId: unmarshalAndCall(SceneSkillClientPlayerNotifySkillUsedHandler),
	game.SceneSkillClientPlayerNotifySkillInterruptedMessageId: unmarshalAndCall(SceneSkillClientPlayerNotifySkillInterruptedHandler),
	game.SceneSkillClientPlayerGetSkillListMessageId: unmarshalAndCall(SceneSkillClientPlayerGetSkillListHandler),
}

func MessageBodyHandler(client *pkg.GameClient, response *base.MessageContent) {
	zap.L().Debug("Received message body", zap.String("response", response.String()))

	player, ok := gameobject.PlayerList.Get(client.PlayerId)
	if !ok {
		zap.L().Error("Player not found", zap.Uint64("player_id", client.PlayerId))

		return
	}

	if h, ok := messageHandlers[response.MessageId]; ok {
		h(player, response.SerializedMessage)
	} else {
		zap.L().Info("Unhandled message", zap.Uint32("message_id", response.MessageId))
	}
}

// HandleMessage is the public entry point called from main.go RecvLoop.
func HandleMessage(client *pkg.GameClient, response *base.MessageContent) {
	MessageBodyHandler(client, response)
}
