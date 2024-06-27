package logic

import (
	"client/pkg"
	"github.com/golang/protobuf/proto"
)

type Player struct {
	PlayerId uint64
	Client   *pkg.GameClient
}

var MainPlayer *Player

func NewPlayer(playerId uint64, client *pkg.GameClient) *Player {
	return &Player{PlayerId: playerId, Client: client}
}

func (p *Player) Send(message proto.Message, messageId uint32) {
	p.Client.Send(message, messageId)
}
