package handler

import (
	"go.uber.org/zap"

	"proto/scene"
	"robot/logic/gameobject"
)

// SceneCurrencyClientPlayerGetCurrencyListHandler stashes the full
// CurrencyComp snapshot returned by GetCurrencyList. Used by the
// currency-crash-window scenario to read pre/post balances around a
// kill -9 of the scene process — see docs/notes/currency-crash-window-
// verification.md.
func SceneCurrencyClientPlayerGetCurrencyListHandler(player *gameobject.Player, response *scene.GetCurrencyListResponse) {
	if response == nil {
		zap.L().Warn("nil GetCurrencyListResponse", zap.Uint64("player", player.ID))
		return
	}
	if response.ErrorMessage != nil && response.ErrorMessage.Id != 0 {
		// Server reported a domain error; still mark the list ready so
		// the scenario can fail fast on its own timeline rather than
		// hanging on WaitCurrencyListReady.
		zap.L().Warn("GetCurrencyList domain error",
			zap.Uint64("player", player.ID),
			zap.Uint32("err_id", response.ErrorMessage.Id))
		player.SetCurrencyValues(nil)
		return
	}
	if response.Currency == nil {
		player.SetCurrencyValues(nil)
		return
	}
	player.SetCurrencyValues(response.Currency.Values)
	zap.L().Info("currency list received",
		zap.Uint64("player", player.ID),
		zap.Uint64s("values", response.Currency.Values))
}
