package handler

import (
	"reflect"

	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"

	"robot/generated/pb/game"
	"robot/pkg"
	"robot/proto/common/base"
)

type handlerFunc func(*pkg.GameClient, []byte)

// unmarshalAndCall creates a handlerFunc that unmarshals body into a new
// message of type PT and forwards it to the typed handler function.
func unmarshalAndCall[PT proto.Message](fn func(*pkg.GameClient, PT)) handlerFunc {
	var zero PT
	msgType := reflect.TypeOf(zero).Elem()
	return func(gc *pkg.GameClient, body []byte) {
		msg := reflect.New(msgType).Interface().(PT)
		if err := proto.Unmarshal(body, msg); err != nil {
			zap.L().Error("unmarshal failed", zap.Error(err))
			return
		}
		fn(gc, msg)
	}
}

var messageHandlers = map[uint32]handlerFunc{
	// Chat
	game.ClientPlayerChatSendChatMessageId:        unmarshalAndCall(handleSendChat),
	game.ClientPlayerChatPullChatHistoryMessageId:  unmarshalAndCall(handlePullChatHistory),
	// Login
	game.ClientPlayerLoginLoginMessageId:           unmarshalAndCall(handleLogin),
	game.ClientPlayerLoginCreatePlayerMessageId:    unmarshalAndCall(handleCreatePlayer),
	game.ClientPlayerLoginEnterGameMessageId:       unmarshalAndCall(handleEnterGame),
	game.ClientPlayerLoginLeaveGameMessageId:       unmarshalAndCall(handleLeaveGame),
	game.ClientPlayerLoginDisconnectMessageId:      unmarshalAndCall(handleDisconnect),
	// Scene common
	game.SceneClientPlayerCommonSendTipToClientMessageId:    unmarshalAndCall(handleSendTip),
	game.SceneClientPlayerCommonKickPlayerMessageId:         unmarshalAndCall(handleKickPlayer),
	game.SceneSceneClientPlayerEnterSceneMessageId:          unmarshalAndCall(handleEnterScene),
	game.SceneSceneClientPlayerNotifyEnterSceneMessageId:    unmarshalAndCall(handleNotifyEnterScene),
	game.SceneSceneClientPlayerSceneInfoC2SMessageId:        unmarshalAndCall(handleSceneInfoC2S),
	game.SceneSceneClientPlayerNotifySceneInfoMessageId:     unmarshalAndCall(handleNotifySceneInfo),
	// Scene actors
	game.SceneSceneClientPlayerNotifyActorCreateMessageId:      unmarshalAndCall(handleNotifyActorCreate),
	game.SceneSceneClientPlayerNotifyActorDestroyMessageId:     unmarshalAndCall(handleNotifyActorDestroy),
	game.SceneSceneClientPlayerNotifyActorListCreateMessageId:  unmarshalAndCall(handleNotifyActorListCreate),
	game.SceneSceneClientPlayerNotifyActorListDestroyMessageId: unmarshalAndCall(handleNotifyActorListDestroy),
	// Skill
	game.SceneSkillClientPlayerReleaseSkillMessageId:            unmarshalAndCall(handleReleaseSkill),
	game.SceneSkillClientPlayerNotifySkillUsedMessageId:         unmarshalAndCall(handleNotifySkillUsed),
	game.SceneSkillClientPlayerNotifySkillInterruptedMessageId:  unmarshalAndCall(handleNotifySkillInterrupted),
	game.SceneSkillClientPlayerGetSkillListMessageId:            unmarshalAndCall(handleGetSkillList),
}

func HandleMessage(gc *pkg.GameClient, msg *base.MessageContent) {
	zap.L().Debug("recv", zap.Uint32("msg_id", msg.MessageId))

	if h, ok := messageHandlers[msg.MessageId]; ok {
		h(gc, msg.SerializedMessage)
	} else {
		zap.L().Debug("unhandled message", zap.Uint32("msg_id", msg.MessageId))
	}
}
