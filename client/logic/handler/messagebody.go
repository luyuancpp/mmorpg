package handler

import (
	"client/logic"
	"client/pb/game"
	"client/pkg"
	"github.com/golang/protobuf/proto"
	"log"
)

func MessageBodyHandler(client *pkg.GameClient, response *game.MessageBody) {
	//log.Println(response)
	player, ok := logic.PlayerList.Get(client.PlayerId)
	if !ok {
		log.Println("player not found ", client.PlayerId)
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
	}
}
