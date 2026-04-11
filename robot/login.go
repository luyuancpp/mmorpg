package main

import (
	"fmt"
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
