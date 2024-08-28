package handler

import (
	"client/logic"
	"client/pb/game"
	"client/pkg"
	"go.uber.org/zap"
)

func CreatePlayerHandler(client *pkg.GameClient, response *game.CreatePlayerResponse) {
	if response.ErrorMessage.Id > 0 {
		return
	}
	player := logic.NewMainPlayer(response.Players[0].Player.PlayerId, client)
	zap.L().Info("create player ", zap.Uint64("player id", player.Client.GetPlayerId()))
	rq := &game.EnterGameRequest{PlayerId: player.Client.GetPlayerId()}
	client.Send(rq, game.LoginServiceEnterGameMessageId)
}
