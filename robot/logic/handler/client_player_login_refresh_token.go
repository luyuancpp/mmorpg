package handler

import (
	"go.uber.org/zap"

	"proto/login"
	"robot/logic/gameobject"
	"robot/pkg"
)

// ClientPlayerLoginRefreshTokenHandler is invoked on the RecvLoop goroutine
// when the login service returns a RefreshToken response. Patches the live
// GameClient's token pair in place so subsequent reconnect/refresh cycles
// see the rotated values.
//
// Metrics for refresh success/failure are bumped by the robot's own ticker
// loop (which tracks per-request correlation) rather than here; this
// handler's only responsibility is state reconciliation.
func ClientPlayerLoginRefreshTokenHandler(player *gameobject.Player, response *login.RefreshTokenResponse) {
	if player == nil || response == nil {
		return
	}
	if response.ErrorMessage != nil {
		zap.L().Info("RefreshToken rejected by server",
			zap.Uint64("player_id", player.ID),
			zap.Any("error", response.ErrorMessage))
		return
	}
	gc := pkg.Clients.Get(player.ID)
	if gc == nil {
		// Session already torn down; nothing to update.
		return
	}
	gc.SetTokens(
		response.AccessToken,
		response.RefreshToken,
		response.AccessTokenExpire,
		response.RefreshTokenExpire,
	)
	zap.L().Debug("access token refreshed",
		zap.Uint64("player_id", player.ID),
		zap.Int64("new_access_expire", response.AccessTokenExpire),
	)
}
