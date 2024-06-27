package logic

import (
	"client/pkg"
)

type Player struct {
	PlayerId uint64
	Client   *pkg.GameClient
}

var MainPlayer *Player

func NewPlayer(playerId uint64, client *pkg.GameClient) *Player {
	return &Player{PlayerId: playerId, Client: client}
}
