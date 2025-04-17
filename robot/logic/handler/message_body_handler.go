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
	case game.ClientPlayerSceneServiceEnterSceneMessageId:
		handleClientPlayerSceneServiceEnterScene(player, response.SerializedMessage)
	case game.ClientPlayerSceneServiceNotifyEnterSceneMessageId:
		handleClientPlayerSceneServiceNotifyEnterScene(player, response.SerializedMessage)