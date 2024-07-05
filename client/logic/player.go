package logic

import (
	"client/pkg"
	"github.com/golang/protobuf/proto"
)

type Player struct {
	PlayerId uint64
	Client   *pkg.GameClient
}

type MainPlayer struct {
	SceneId uint32
	Player
}

var GMainPlayer *MainPlayer

func NewMainPlayer(playerId uint64, client *pkg.GameClient) *MainPlayer {
	return &MainPlayer{Player: Player{PlayerId: playerId, Client: client}}
}

func (p *Player) Send(message proto.Message, messageId uint32) {
	p.Client.Send(message, messageId)
}
