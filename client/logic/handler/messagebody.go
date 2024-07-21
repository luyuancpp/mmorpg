package handler

import (
	"client/logic"
	"client/pb/game"
	"client/pkg"
	"github.com/golang/protobuf/proto"
	"go.uber.org/zap"
)

func MessageBodyHandler(client *pkg.GameClient, response *game.MessageBody) {
	zap.L().Debug("message body", zap.String("response ", response.String()))
	player, ok := logic.PlayerList.Get(client.PlayerId)
	if !ok {
		zap.L().Error("player not found", zap.Uint64("player id ", client.PlayerId))
		return
	}
	if response.MessageId == 18 {
		message := &game.SceneInfoS2C{}
		err := proto.Unmarshal(response.Body, message)
		if err != nil {
			return
		}
		SceneInfoS2CHandler(player, message)
	} else if response.MessageId == 17 {
		message := &game.EnterSceneS2C{}
		err := proto.Unmarshal(response.Body, message)
		if err != nil {
			return
		}
		EnterSceneS2CHandler(player, message)
	} else if response.MessageId == 0 {
		message := &game.TipS2C{}
		err := proto.Unmarshal(response.Body, message)
		if err != nil {
			return
		}
		TipS2CHandler(player, message)
	} else if response.MessageId == 16 {
		message := &game.EnterSceneC2SResponse{}
		err := proto.Unmarshal(response.Body, message)
		if err != nil {
			return
		}
	} else if response.MessageId == 23 {

	} else {
		zap.L().Info("un handle message", zap.String("response", response.String()))
	}
}
