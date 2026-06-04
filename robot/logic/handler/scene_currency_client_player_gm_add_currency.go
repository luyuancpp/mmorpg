package handler

import (
	"go.uber.org/zap"

	"proto/scene"
	"robot/logic/gameobject"
)

// SceneCurrencyClientPlayerGmAddCurrencyHandler records the post-add balance
// returned by the scene server's GmAddCurrency RPC. The currency-crash-window
// scenario uses balance_after to confirm the GM mutation actually landed in
// scene memory before we kill the process.
func SceneCurrencyClientPlayerGmAddCurrencyHandler(player *gameobject.Player, response *scene.GmAddCurrencyResponse) {
	if response == nil {
		zap.L().Warn("nil GmAddCurrencyResponse", zap.Uint64("player", player.ID))
		return
	}
	balance := response.GetBalanceAfter()
	player.SetLastBalanceAfter(balance)
	zap.L().Info("GmAddCurrency response",
		zap.Uint64("player", player.ID),
		zap.Uint64("balance_after", balance))
}
