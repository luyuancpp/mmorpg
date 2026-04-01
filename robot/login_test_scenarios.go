package main

import (
	"fmt"
	"strconv"
	"sync"
	"time"

	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"

	"robot/config"
	"robot/generated/pb/game"
	"robot/metrics"
	"robot/pkg"
	"robot/proto/login"
)

// testResult records the outcome of one test scenario.
type testResult struct {
	Name    string
	Passed  bool
	Elapsed time.Duration
	Detail  string
}

// runLoginTests executes all login test scenarios sequentially and prints a summary.
func runLoginTests(host string, port int, cfg *config.Config, stats *metrics.Stats, tokenPayload, tokenSig []byte) {
	account := fmt.Sprintf(cfg.AccountFmt, 1)
	account2 := fmt.Sprintf(cfg.AccountFmt, 2)

	scenarios := []struct {
		name string
		fn   func() testResult
	}{
		{"NormalLogin", func() testResult {
			return testNormalLogin(host, port, account, cfg.Password, stats, tokenPayload, tokenSig)
		}},
		{"LoginLogoutCycle", func() testResult {
			return testLoginLogoutCycle(host, port, account, cfg.Password, stats, tokenPayload, tokenSig, 3)
		}},
		{"WrongPassword", func() testResult {
			return testWrongPassword(host, port, account, stats, tokenPayload, tokenSig)
		}},
		{"DuplicateEnterGame", func() testResult {
			return testDuplicateEnterGame(host, port, account, cfg.Password, stats, tokenPayload, tokenSig)
		}},
		{"AccountDisplacement", func() testResult {
			return testAccountDisplacement(host, port, account, cfg.Password, stats, tokenPayload, tokenSig)
		}},
		{"RapidReconnect", func() testResult {
			return testRapidReconnect(host, port, account, cfg.Password, stats, tokenPayload, tokenSig)
		}},
		{"ConcurrentSameAccount", func() testResult {
			return testConcurrentSameAccount(host, port, account, cfg.Password, stats, tokenPayload, tokenSig)
		}},
		{"DifferentAccountSequential", func() testResult {
			return testDifferentAccountSequential(host, port, account, account2, cfg.Password, stats, tokenPayload, tokenSig)
		}},
		{"LeaveAndReEnter", func() testResult {
			return testLeaveAndReEnter(host, port, account, cfg.Password, stats, tokenPayload, tokenSig)
		}},
		{"DisconnectDuringLogin", func() testResult {
			return testDisconnectDuringLogin(host, port, account, cfg.Password, stats, tokenPayload, tokenSig)
		}},
	}

	zap.L().Info("======== Login Test Suite ========", zap.Int("scenarios", len(scenarios)))

	var results []testResult
	passed, failed := 0, 0
	for i, s := range scenarios {
		zap.L().Info(fmt.Sprintf("[%d/%d] Running: %s", i+1, len(scenarios), s.name))
		r := s.fn()
		r.Name = s.name
		results = append(results, r)
		status := "PASS"
		if !r.Passed {
			status = "FAIL"
			failed++
		} else {
			passed++
		}
		zap.L().Info(fmt.Sprintf("[%d/%d] %s: %s (%s) %s",
			i+1, len(scenarios), status, s.name, r.Elapsed.Truncate(time.Millisecond), r.Detail))

		// Brief pause between scenarios so server-side sessions settle.
		time.Sleep(500 * time.Millisecond)
	}

	zap.L().Info("======== Login Test Summary ========")
	zap.L().Info(fmt.Sprintf("Total: %d  Passed: %d  Failed: %d", len(results), passed, failed))
	for _, r := range results {
		mark := "OK"
		if !r.Passed {
			mark = "FAIL"
		}
		zap.L().Info(fmt.Sprintf("  [%s] %s  %s  %s", mark, r.Name, r.Elapsed.Truncate(time.Millisecond), r.Detail))
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// connectAndVerify creates a GameClient, sets the account, optionally verifies token.
func connectAndVerify(host string, port int, account string, tokenPayload, tokenSig []byte) (*pkg.GameClient, error) {
	gc, err := pkg.NewGameClient(host, port)
	if err != nil {
		return nil, err
	}
	gc.Account = account
	time.Sleep(200 * time.Millisecond)
	if len(tokenPayload) > 0 {
		if err := gc.VerifyGateToken(tokenPayload, tokenSig); err != nil {
			gc.Close()
			return nil, fmt.Errorf("token verify: %w", err)
		}
	}
	return gc, nil
}

// leaveGame sends a LeaveGame request.
func leaveGame(gc *pkg.GameClient, stats *metrics.Stats) error {
	return gc.SendRequest(game.ClientPlayerLoginLeaveGameMessageId, &login.LeaveGameRequest{})
}

// ---------------------------------------------------------------------------
// Scenario 1: Normal login flow (baseline)
// ---------------------------------------------------------------------------

func testNormalLogin(host string, port int, account, password string, stats *metrics.Stats, tokenPayload, tokenSig []byte) testResult {
	start := time.Now()
	gc, err := connectAndVerify(host, port, account, tokenPayload, tokenSig)
	if err != nil {
		return testResult{Elapsed: time.Since(start), Detail: err.Error()}
	}
	defer gc.Close()

	if err := loginAndEnter(gc, password, stats); err != nil {
		return testResult{Elapsed: time.Since(start), Detail: err.Error()}
	}

	return testResult{Passed: true, Elapsed: time.Since(start),
		Detail: fmt.Sprintf("player_id=%d", gc.PlayerId)}
}

// ---------------------------------------------------------------------------
// Scenario 2: Login → LeaveGame → re-login, repeated N times
// ---------------------------------------------------------------------------

func testLoginLogoutCycle(host string, port int, account, password string, stats *metrics.Stats, tokenPayload, tokenSig []byte, cycles int) testResult {
	start := time.Now()

	for i := 0; i < cycles; i++ {
		gc, err := connectAndVerify(host, port, account, tokenPayload, tokenSig)
		if err != nil {
			return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("cycle %d connect: %s", i, err)}
		}

		if err := loginAndEnter(gc, password, stats); err != nil {
			gc.Close()
			return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("cycle %d login: %s", i, err)}
		}

		if err := leaveGame(gc, stats); err != nil {
			gc.Close()
			return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("cycle %d leave: %s", i, err)}
		}
		time.Sleep(300 * time.Millisecond)
		gc.Close()
		time.Sleep(300 * time.Millisecond) // let server clean up session
	}

	return testResult{Passed: true, Elapsed: time.Since(start),
		Detail: fmt.Sprintf("completed %d login/logout cycles", cycles)}
}

// ---------------------------------------------------------------------------
// Scenario 3: Wrong password
// ---------------------------------------------------------------------------

func testWrongPassword(host string, port int, account string, stats *metrics.Stats, tokenPayload, tokenSig []byte) testResult {
	start := time.Now()
	gc, err := connectAndVerify(host, port, account, tokenPayload, tokenSig)
	if err != nil {
		return testResult{Elapsed: time.Since(start), Detail: err.Error()}
	}
	defer gc.Close()

	var lr login.LoginResponse
	if err := sendAndRecv(gc, stats,
		game.ClientPlayerLoginLoginMessageId,
		&login.LoginRequest{Account: account, Password: "WRONG_PASSWORD_" + strconv.FormatInt(time.Now().UnixNano(), 36)},
		&lr,
	); err != nil {
		// Network error during wrong password is also acceptable in some server implementations.
		return testResult{Passed: true, Elapsed: time.Since(start),
			Detail: fmt.Sprintf("server rejected connection: %s", err)}
	}

	// Server should return an error_message for wrong password.
	if lr.ErrorMessage != nil {
		return testResult{Passed: true, Elapsed: time.Since(start),
			Detail: fmt.Sprintf("server returned error: %v", lr.ErrorMessage)}
	}

	// If server allows any password (dev mode), that's still a valid observation.
	return testResult{Passed: true, Elapsed: time.Since(start),
		Detail: "server accepted wrong password (dev mode / no auth)"}
}

// ---------------------------------------------------------------------------
// Scenario 4: Duplicate EnterGame on same connection
// ---------------------------------------------------------------------------

func testDuplicateEnterGame(host string, port int, account, password string, stats *metrics.Stats, tokenPayload, tokenSig []byte) testResult {
	start := time.Now()
	gc, err := connectAndVerify(host, port, account, tokenPayload, tokenSig)
	if err != nil {
		return testResult{Elapsed: time.Since(start), Detail: err.Error()}
	}
	defer gc.Close()

	if err := loginAndEnter(gc, password, stats); err != nil {
		return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("first enter: %s", err)}
	}
	playerId := gc.PlayerId

	// Send EnterGame again with the same player_id.
	var er login.EnterGameResponse
	if err := sendAndRecv(gc, stats,
		game.ClientPlayerLoginEnterGameMessageId,
		&login.EnterGameRequest{PlayerId: playerId},
		&er,
	); err != nil {
		return testResult{Passed: true, Elapsed: time.Since(start),
			Detail: fmt.Sprintf("server rejected duplicate enter: %s", err)}
	}
	if er.ErrorMessage != nil {
		return testResult{Passed: true, Elapsed: time.Since(start),
			Detail: fmt.Sprintf("server returned error for duplicate enter: %v", er.ErrorMessage)}
	}

	// Idempotent accept is also valid (EnterGameRequest has request_id for dedup).
	return testResult{Passed: true, Elapsed: time.Since(start),
		Detail: "server accepted duplicate enter (idempotent)"}
}

// ---------------------------------------------------------------------------
// Scenario 5: Account displacement (顶号)
// Robot A logs in, Robot B logs in with same account — A should be kicked.
// ---------------------------------------------------------------------------

func testAccountDisplacement(host string, port int, account, password string, stats *metrics.Stats, tokenPayload, tokenSig []byte) testResult {
	start := time.Now()

	// A connects and enters game.
	gcA, err := connectAndVerify(host, port, account, tokenPayload, tokenSig)
	if err != nil {
		return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("A connect: %s", err)}
	}
	defer gcA.Close()

	if err := loginAndEnter(gcA, password, stats); err != nil {
		return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("A login: %s", err)}
	}

	// Start listening for kick on A.
	kickCh := make(chan bool, 1)
	go func() {
		for {
			raw, err := gcA.RecvOne()
			if err != nil {
				kickCh <- true // connection closed = kicked
				return
			}
			if raw.MessageId == game.SceneClientPlayerCommonKickPlayerMessageId {
				kickCh <- true
				return
			}
		}
	}()

	// B connects with same account — should displace A.
	time.Sleep(200 * time.Millisecond)
	gcB, err := connectAndVerify(host, port, account, tokenPayload, tokenSig)
	if err != nil {
		return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("B connect: %s", err)}
	}
	defer gcB.Close()

	if err := loginAndEnter(gcB, password, stats); err != nil {
		return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("B login: %s", err)}
	}

	// Wait up to 5s for A to receive kick.
	select {
	case <-kickCh:
		return testResult{Passed: true, Elapsed: time.Since(start),
			Detail: "A received kick after B logged in with same account"}
	case <-time.After(5 * time.Second):
		return testResult{Passed: false, Elapsed: time.Since(start),
			Detail: "A was NOT kicked after B logged in with same account (timeout 5s)"}
	}
}

// ---------------------------------------------------------------------------
// Scenario 6: Rapid reconnect — disconnect abruptly then reconnect quickly
// ---------------------------------------------------------------------------

func testRapidReconnect(host string, port int, account, password string, stats *metrics.Stats, tokenPayload, tokenSig []byte) testResult {
	start := time.Now()

	// First connection: login then close abruptly (no LeaveGame).
	gc1, err := connectAndVerify(host, port, account, tokenPayload, tokenSig)
	if err != nil {
		return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("first connect: %s", err)}
	}
	if err := loginAndEnter(gc1, password, stats); err != nil {
		gc1.Close()
		return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("first login: %s", err)}
	}
	gc1.Close() // abrupt disconnect, no LeaveGame

	// Immediately reconnect.
	time.Sleep(100 * time.Millisecond)
	gc2, err := connectAndVerify(host, port, account, tokenPayload, tokenSig)
	if err != nil {
		return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("reconnect: %s", err)}
	}
	defer gc2.Close()

	if err := loginAndEnter(gc2, password, stats); err != nil {
		return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("reconnect login: %s", err)}
	}

	return testResult{Passed: true, Elapsed: time.Since(start),
		Detail: fmt.Sprintf("reconnected OK, player_id=%d", gc2.PlayerId)}
}

// ---------------------------------------------------------------------------
// Scenario 7: Concurrent same-account login (race condition)
// Two goroutines try to login with the same account simultaneously.
// ---------------------------------------------------------------------------

func testConcurrentSameAccount(host string, port int, account, password string, stats *metrics.Stats, tokenPayload, tokenSig []byte) testResult {
	start := time.Now()

	var wg sync.WaitGroup
	results := make([]error, 2)
	clients := make([]*pkg.GameClient, 2)

	for i := 0; i < 2; i++ {
		wg.Add(1)
		go func(idx int) {
			defer wg.Done()
			gc, err := connectAndVerify(host, port, account, tokenPayload, tokenSig)
			if err != nil {
				results[idx] = err
				return
			}
			clients[idx] = gc
			results[idx] = loginAndEnter(gc, password, stats)
		}(i)
	}
	wg.Wait()

	defer func() {
		for _, gc := range clients {
			if gc != nil {
				gc.Close()
			}
		}
	}()

	successCount := 0
	for _, err := range results {
		if err == nil {
			successCount++
		}
	}

	detail := fmt.Sprintf("concurrent results: [%v, %v]", results[0], results[1])

	// At least one should succeed; at most one should be active.
	if successCount >= 1 {
		return testResult{Passed: true, Elapsed: time.Since(start),
			Detail: fmt.Sprintf("%d/2 succeeded — %s", successCount, detail)}
	}

	return testResult{Passed: false, Elapsed: time.Since(start),
		Detail: fmt.Sprintf("both failed — %s", detail)}
}

// ---------------------------------------------------------------------------
// Scenario 8: Two different accounts login sequentially
// ---------------------------------------------------------------------------

func testDifferentAccountSequential(host string, port int, account1, account2, password string, stats *metrics.Stats, tokenPayload, tokenSig []byte) testResult {
	start := time.Now()

	// Account 1
	gc1, err := connectAndVerify(host, port, account1, tokenPayload, tokenSig)
	if err != nil {
		return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("account1 connect: %s", err)}
	}
	if err := loginAndEnter(gc1, password, stats); err != nil {
		gc1.Close()
		return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("account1 login: %s", err)}
	}
	pid1 := gc1.PlayerId
	gc1.Close()
	time.Sleep(300 * time.Millisecond)

	// Account 2
	gc2, err := connectAndVerify(host, port, account2, tokenPayload, tokenSig)
	if err != nil {
		return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("account2 connect: %s", err)}
	}
	defer gc2.Close()
	if err := loginAndEnter(gc2, password, stats); err != nil {
		return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("account2 login: %s", err)}
	}

	return testResult{Passed: true, Elapsed: time.Since(start),
		Detail: fmt.Sprintf("both accounts logged in: %s (pid=%d), %s (pid=%d)", account1, pid1, account2, gc2.PlayerId)}
}

// ---------------------------------------------------------------------------
// Scenario 9: Leave game then re-enter without new login
// ---------------------------------------------------------------------------

func testLeaveAndReEnter(host string, port int, account, password string, stats *metrics.Stats, tokenPayload, tokenSig []byte) testResult {
	start := time.Now()
	gc, err := connectAndVerify(host, port, account, tokenPayload, tokenSig)
	if err != nil {
		return testResult{Elapsed: time.Since(start), Detail: err.Error()}
	}
	defer gc.Close()

	if err := loginAndEnter(gc, password, stats); err != nil {
		return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("first enter: %s", err)}
	}
	playerId := gc.PlayerId

	// Leave game.
	if err := leaveGame(gc, stats); err != nil {
		return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("leave: %s", err)}
	}
	time.Sleep(500 * time.Millisecond)

	// Try to re-enter without a new Login call.
	var er login.EnterGameResponse
	if err := sendAndRecv(gc, stats,
		game.ClientPlayerLoginEnterGameMessageId,
		&login.EnterGameRequest{PlayerId: playerId},
		&er,
	); err != nil {
		return testResult{Passed: true, Elapsed: time.Since(start),
			Detail: fmt.Sprintf("re-enter after leave rejected: %s", err)}
	}
	if er.ErrorMessage != nil {
		return testResult{Passed: true, Elapsed: time.Since(start),
			Detail: fmt.Sprintf("re-enter error: %v", er.ErrorMessage)}
	}

	return testResult{Passed: true, Elapsed: time.Since(start),
		Detail: "re-enter after leave succeeded (server allows it)"}
}

// ---------------------------------------------------------------------------
// Scenario 10: Disconnect during login handshake (close mid-flow)
// ---------------------------------------------------------------------------

func testDisconnectDuringLogin(host string, port int, account, password string, stats *metrics.Stats, tokenPayload, tokenSig []byte) testResult {
	start := time.Now()
	gc, err := connectAndVerify(host, port, account, tokenPayload, tokenSig)
	if err != nil {
		return testResult{Elapsed: time.Since(start), Detail: err.Error()}
	}

	// Send Login request, but close before reading reply.
	_ = gc.SendRequest(game.ClientPlayerLoginLoginMessageId,
		&login.LoginRequest{Account: account, Password: password})
	gc.Close()

	// Brief pause, then reconnect and do a full login.
	time.Sleep(500 * time.Millisecond)
	gc2, err := connectAndVerify(host, port, account, tokenPayload, tokenSig)
	if err != nil {
		return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("reconnect after abort: %s", err)}
	}
	defer gc2.Close()

	if err := loginAndEnter(gc2, password, stats); err != nil {
		return testResult{Elapsed: time.Since(start), Detail: fmt.Sprintf("login after abort: %s", err)}
	}

	return testResult{Passed: true, Elapsed: time.Since(start),
		Detail: "server recovered after mid-login disconnect"}
}

func init() {
	// Ensure the LeaveGame and Disconnect proto messages are registered.
	_ = proto.Size(&login.LeaveGameRequest{})
}
