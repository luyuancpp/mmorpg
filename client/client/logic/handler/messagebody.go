package handler

import (
	"client/pb/game"
	"client/pkg"
	"github.com/golang/protobuf/proto"
)

func MessageBodyHandler(client *pkg.GameClient, response *game.MessageBody) {
	if response.MessageId == 18 {
		message := &game.SceneInfoS2C{}
		err := proto.Unmarshal(response.Body, message)
		if err != nil {
			return
		}
		SceneInfoS2CHandler(client, message)
	}
}
