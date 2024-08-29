package handler

import (
	"client/pb/game"
	"client/pkg"
)

func LoginHandler(client *pkg.GameClient, response *game.LoginResponse) {
	client.Blackboard.SetMem("loginplayerlist", response.Players)
}
