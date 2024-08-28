package handler

import (
	"client/logic"
	"client/pb/game"
	"client/pkg"
	"go.uber.org/zap"
)

func LoginHandler(client *pkg.GameClient, response *game.LoginResponse) {
	client.Blackboard.SetMem("loginplayerlist", response.Players)
	return
	if response.Players == nil || len(response.Players) <= 0 {
		rq := &game.CreatePlayerRequest{}
		client.Send(rq, game.LoginServiceCreatePlayerMessageId)
	} else {
		playerId := response.Players[0].Player.PlayerId
		zap.L().Info("player login", zap.Uint64("player id ", playerId))
		logic.PlayerList.Set(playerId, logic.NewMainPlayer(playerId, client))
		rq := &game.EnterGameRequest{PlayerId: playerId}
		client.Send(rq, game.LoginServiceEnterGameMessageId)
	}
}
