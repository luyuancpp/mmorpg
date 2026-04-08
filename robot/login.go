package main

import (
	"fmt"

	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"

	"robot/generated/pb/game"
	"robot/metrics"
	"robot/pkg"
	"proto/login"
)

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
	if err := gc.SendRequest(msgId, req); err != nil {
		return err
	}
	stats.MsgSent()

	for {
		raw, err := gc.RecvOne()
		if err != nil {
			return err
		}
		stats.MsgRecv()
		if raw.MessageId == msgId {
			return proto.Unmarshal(raw.SerializedMessage, resp)
		}
		// During login handshake the server may push notifications — skip them.
		zap.L().Debug("skipping msg during login",
			zap.Uint32("got", raw.MessageId),
			zap.Uint32("want", msgId),
		)
	}
}
