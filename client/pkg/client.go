package pkg

import (
	"client/pb/game"
	"github.com/golang/protobuf/proto"
	"github.com/luyuancpp/muduoclient/muduo"
	"log"
)

type GameClient struct {
	Client *muduo.Client
}

func NewGameClient(client *muduo.Client) *GameClient {
	return &GameClient{client}
}

func (c *GameClient) Send(message proto.Message, messageId uint32) {
	rq := &game.ClientRequest{Id: 1, MessageId: messageId}
	var err error
	rq.Body, err = proto.Marshal(message)
	if err != nil {
		log.Println(err)
		return
	}
	c.Client.Send(rq)
}

func (c *GameClient) Close() {
	defer func(client *muduo.Client) {
		err := client.Close()
		if err != nil {
			panic(err)
		}
	}(c.Client)
}
