package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"time"

	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"

	"proto/common/base"
	"proto/login"
	"robot/generated/pb/game"
	"robot/metrics"
	"robot/pkg"
)

// defaultLoginTimeout is the maximum time to wait for any single login-flow
// request/response. If a response is not received within this duration, the
// login is considered "stuck" and the attempt fails with a timeout error.
const defaultLoginTimeout = 15 * time.Second

// loginAndEnter performs: login → create player (if needed) → enter game.
func loginAndEnter(gc *pkg.GameClient, password string, stats *metrics.Stats) error {
	// 1. Login
	var lr login.LoginResponse
	if err := sendAndRecv(gc, stats,
		game.ClientPlayerLoginLoginMessageId,
		&login.LoginRequest{Account: gc.Account, Password: password},
		&lr,
	); err != nil {
		stats.LoginFail()
		return fmt.Errorf("login: %w", err)
	}
	if lr.ErrorMessage != nil {
		stats.LoginFail()
		return fmt.Errorf("login: server error %v", lr.ErrorMessage)
	}

	// Store access/refresh tokens for potential reconnect. Use SetTokens so
	// all token-field writes go through the same mutex as the RefreshToken
	// handler — keeps the API uniform and survives a future refactor that
	// re-runs login on a gc whose RecvLoop already started.
	if lr.AccessToken != "" {
		gc.SetTokens(lr.AccessToken, lr.RefreshToken, lr.AccessTokenExpire, lr.RefreshTokenExpire)
		zap.L().Debug("received auth tokens",
			zap.String("account", gc.Account),
			zap.Int64("access_expire", lr.AccessTokenExpire),
			zap.Int64("refresh_expire", lr.RefreshTokenExpire),
		)
	}

	// 2. Create player if needed
	if len(lr.Players) == 0 {
		var cr login.CreatePlayerResponse
		if err := sendAndRecv(gc, stats,
			game.ClientPlayerLoginCreatePlayerMessageId,
			&login.CreatePlayerRequest{},
			&cr,
		); err != nil {
			stats.LoginFail()
			return fmt.Errorf("create player: %w", err)
		}
		if cr.ErrorMessage != nil {
			stats.LoginFail()
			return fmt.Errorf("create player: server error %v", cr.ErrorMessage)
		}
		lr.Players = cr.Players
	}
	if len(lr.Players) == 0 {
		stats.LoginFail()
		return fmt.Errorf("no players after create")
	}

	// 3. Enter game
	playerId := lr.Players[0].GetPlayer().GetPlayerId()
	var er login.EnterGameResponse
	if err := sendAndRecv(gc, stats,
		game.ClientPlayerLoginEnterGameMessageId,
		&login.EnterGameRequest{PlayerId: playerId},
		&er,
	); err != nil {
		stats.EnterFail()
		return fmt.Errorf("enter game: %w", err)
	}
	if er.ErrorMessage != nil {
		stats.EnterFail()
		return fmt.Errorf("enter game: server error %v", er.ErrorMessage)
	}

	gc.PlayerId = er.PlayerId
	return nil
}

// sendAndRecv sends a request and blocks until the matching response arrives,
// then unmarshals it into resp.
func sendAndRecv(gc *pkg.GameClient, stats *metrics.Stats, msgId uint32, req, resp proto.Message) error {
	return sendAndRecvTimeout(gc, stats, msgId, req, resp, defaultLoginTimeout)
}

// sendAndRecvTimeout is like sendAndRecv but with an explicit timeout.
// If the response is not received within the timeout, it returns an error
// indicating the login is stuck.
func sendAndRecvTimeout(gc *pkg.GameClient, stats *metrics.Stats, msgId uint32, req, resp proto.Message, timeout time.Duration) error {
	if err := gc.SendRequest(msgId, req); err != nil {
		return err
	}
	stats.MsgSent()

	type recvResult struct {
		msg *base.MessageContent
		err error
	}
	ch := make(chan recvResult, 1)

	go func() {
		for {
			raw, err := gc.RecvOne()
			if err != nil {
				ch <- recvResult{err: err}
				return
			}
			stats.MsgRecv()
			if raw.MessageId == msgId {
				ch <- recvResult{msg: raw}
				return
			}
			zap.L().Debug("skipping msg during login",
				zap.Uint32("got", raw.MessageId),
				zap.Uint32("want", msgId),
			)
		}
	}()

	select {
	case r := <-ch:
		if r.err != nil {
			return r.err
		}
		return proto.Unmarshal(r.msg.SerializedMessage, resp)
	case <-time.After(timeout):
		stats.LoginStuck()
		return fmt.Errorf("STUCK: no response for msg_id=%d within %s", msgId, timeout)
	}
}

// runTokenRefresher proactively refreshes the access_token when it's within
// refreshLead of expiry, and exits cleanly when either `stop` closes (global
// shutdown) or `done` closes (this session's RecvLoop ended).
//
// Semantics of the counters it touches:
//   - TokenRefreshOK   : refresh RPC successfully handed to the transport.
//   - TokenRefreshFail : transport send returned an error.
//
// Server-side rejection of the refresh (expired refresh_token, rotated
// elsewhere, etc.) arrives asynchronously in ClientPlayerLoginRefreshTokenHandler
// and is surfaced via its zap.L().Info log. Quantitatively that shows up as
// the next reconnect falling through to the primary auth provider, which is
// already counted by AccessReconnectFallback.
func runTokenRefresher(gc *pkg.GameClient, stats *metrics.Stats, stop, done <-chan struct{}) {
	const (
		checkInterval = 30 * time.Second
		refreshLead   = 10 * time.Minute // refresh when <10min of TTL remains
		sendCooldown  = 60 * time.Second // min gap between successive refreshes
	)
	ticker := time.NewTicker(checkInterval)
	defer ticker.Stop()
	var lastSent time.Time
	for {
		select {
		case <-stop:
			return
		case <-done:
			return
		case <-ticker.C:
			_, refresh, accessExp, _ := gc.SnapshotTokens()
			if refresh == "" || accessExp == 0 {
				continue
			}
			remaining := time.Until(time.Unix(accessExp, 0))
			if remaining > refreshLead {
				continue
			}
			// Dedup: if we already asked for a refresh recently, give the
			// server (and its handler → gc.SetTokens write) a window to
			// land before asking again. Without this, a slow-to-respond
			// server would eat N redundant refresh RPCs for every tick
			// that still sees the old accessExp.
			if !lastSent.IsZero() && time.Since(lastSent) < sendCooldown {
				continue
			}
			zap.L().Info("refreshing access_token",
				zap.String("account", gc.Account),
				zap.Duration("remaining", remaining),
			)
			if err := gc.SendRequest(
				game.ClientPlayerLoginRefreshTokenMessageId,
				&login.RefreshTokenRequest{RefreshToken: refresh},
			); err != nil {
				stats.TokenRefreshFail()
				zap.L().Warn("refresh send failed", zap.Error(err))
				continue
			}
			stats.TokenRefreshOK()
			lastSent = time.Now()
		}
	}
}

// fetchSaToken calls the SA-Token dev-login endpoint and returns the token value.
func fetchSaToken(saTokenAddr, account string) (string, error) {
	url := saTokenAddr + "/auth/dev-login?account=" + account
	resp, err := http.Get(url)
	if err != nil {
		return "", fmt.Errorf("SA-Token dev-login request: %w", err)
	}
	defer func() { _ = resp.Body.Close() }()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("read SA-Token response: %w", err)
	}
	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("SA-Token dev-login HTTP %d: %s", resp.StatusCode, string(body))
	}

	var result struct {
		OK         bool   `json:"ok"`
		TokenValue string `json:"token_value"`
		Message    string `json:"message"`
	}
	if err := json.Unmarshal(body, &result); err != nil {
		return "", fmt.Errorf("decode SA-Token response: %w", err)
	}
	if !result.OK {
		return "", fmt.Errorf("SA-Token dev-login failed: %s", result.Message)
	}
	if result.TokenValue == "" {
		return "", fmt.Errorf("SA-Token dev-login returned empty token")
	}
	return result.TokenValue, nil
}

// revokeSaToken calls the SA-Token logout endpoint to remove the manually-
// written Redis key so long-running stress tests don't accumulate stale
// satoken:login:token:* entries. Best-effort; errors are logged but not fatal.
func revokeSaToken(saTokenAddr, token string) {
	if saTokenAddr == "" || token == "" {
		return
	}
	url := saTokenAddr + "/auth/logout?token=" + token
	resp, err := http.Get(url)
	if err != nil {
		zap.L().Debug("SA-Token logout failed", zap.Error(err))
		return
	}
	_ = resp.Body.Close()
}

// loginAndEnterSaToken performs: fetch SA-Token → login (auth_type=satoken) → create player → enter game.
func loginAndEnterSaToken(gc *pkg.GameClient, saTokenAddr string, stats *metrics.Stats) error {
	token, err := fetchSaToken(saTokenAddr, gc.Account)
	if err != nil {
		stats.LoginFail()
		return fmt.Errorf("fetch satoken: %w", err)
	}
	gc.SaToken = token

	var lr login.LoginResponse
	if err := sendAndRecv(gc, stats,
		game.ClientPlayerLoginLoginMessageId,
		&login.LoginRequest{
			Account:  gc.Account,
			AuthType: "satoken",
			AuthToken: token,
		},
		&lr,
	); err != nil {
		stats.LoginFail()
		return fmt.Errorf("login(satoken): %w", err)
	}
	if lr.ErrorMessage != nil {
		stats.LoginFail()
		return fmt.Errorf("login(satoken): server error %v", lr.ErrorMessage)
	}

	if lr.AccessToken != "" {
		gc.SetTokens(lr.AccessToken, lr.RefreshToken, lr.AccessTokenExpire, lr.RefreshTokenExpire)
	}

	if len(lr.Players) == 0 {
		var cr login.CreatePlayerResponse
		if err := sendAndRecv(gc, stats,
			game.ClientPlayerLoginCreatePlayerMessageId,
			&login.CreatePlayerRequest{},
			&cr,
		); err != nil {
			stats.LoginFail()
			return fmt.Errorf("create player: %w", err)
		}
		if cr.ErrorMessage != nil {
			stats.LoginFail()
			return fmt.Errorf("create player: server error %v", cr.ErrorMessage)
		}
		lr.Players = cr.Players
	}
	if len(lr.Players) == 0 {
		stats.LoginFail()
		return fmt.Errorf("no players after create")
	}

	playerId := lr.Players[0].GetPlayer().GetPlayerId()
	var er login.EnterGameResponse
	if err := sendAndRecv(gc, stats,
		game.ClientPlayerLoginEnterGameMessageId,
		&login.EnterGameRequest{PlayerId: playerId},
		&er,
	); err != nil {
		stats.EnterFail()
		return fmt.Errorf("enter game: %w", err)
	}
	if er.ErrorMessage != nil {
		stats.EnterFail()
		return fmt.Errorf("enter game: server error %v", er.ErrorMessage)
	}

	gc.PlayerId = er.PlayerId
	return nil
}

// loginAndEnterAccessToken performs: login (auth_type=access_token) → create
// player (if needed) → enter game. Used on reconnect to skip the SA-Token /
// password round-trip. The server does NOT rotate tokens for access_token auth,
// so gc.AccessToken stays the same across successful reconnects.
func loginAndEnterAccessToken(gc *pkg.GameClient, accessToken string, stats *metrics.Stats) error {
	var lr login.LoginResponse
	if err := sendAndRecv(gc, stats,
		game.ClientPlayerLoginLoginMessageId,
		&login.LoginRequest{
			Account:   gc.Account,
			AuthType:  "access_token",
			AuthToken: accessToken,
		},
		&lr,
	); err != nil {
		stats.LoginFail()
		return fmt.Errorf("login(access_token): %w", err)
	}
	if lr.ErrorMessage != nil {
		stats.LoginFail()
		return fmt.Errorf("login(access_token): server error %v", lr.ErrorMessage)
	}

	if len(lr.Players) == 0 {
		var cr login.CreatePlayerResponse
		if err := sendAndRecv(gc, stats,
			game.ClientPlayerLoginCreatePlayerMessageId,
			&login.CreatePlayerRequest{},
			&cr,
		); err != nil {
			stats.LoginFail()
			return fmt.Errorf("create player: %w", err)
		}
		if cr.ErrorMessage != nil {
			stats.LoginFail()
			return fmt.Errorf("create player: server error %v", cr.ErrorMessage)
		}
		lr.Players = cr.Players
	}
	if len(lr.Players) == 0 {
		stats.LoginFail()
		return fmt.Errorf("no players after create")
	}

	playerID := lr.Players[0].GetPlayer().GetPlayerId()
	var er login.EnterGameResponse
	if err := sendAndRecv(gc, stats,
		game.ClientPlayerLoginEnterGameMessageId,
		&login.EnterGameRequest{PlayerId: playerID},
		&er,
	); err != nil {
		stats.EnterFail()
		return fmt.Errorf("enter game: %w", err)
	}
	if er.ErrorMessage != nil {
		stats.EnterFail()
		return fmt.Errorf("enter game: server error %v", er.ErrorMessage)
	}

	gc.PlayerId = er.PlayerId
	return nil
}
