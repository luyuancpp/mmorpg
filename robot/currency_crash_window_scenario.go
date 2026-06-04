package main

// Currency-crash-window verification scenario.
//
// Background: docs/notes/currency-crash-window-verification.md.
// Goal: quantify the data-loss window for in-flight currency mutations
// when the scene process is killed (kill -9) between two periodic
// SavePlayerToRedis ticks.
//
// This file only implements the *robot* side — login → snapshot pre
// balance → GmAddCurrency → re-snapshot to confirm mutation landed in
// scene memory → exit (without LeaveGame so we can be killed by the
// driver script). The "kill scene" + "restart scene" steps belong to
// the orchestration script (tools/scripts/currency_crash_window.ps1
// or similar), not the robot. The robot is run twice per case: once
// before the kill (mode=before), once after the restart (mode=after);
// the script diffs balances across runs.
//
// We avoid embedding kill orchestration here so the robot stays a
// pure client (no special privileges to send Stop-Process), and so
// the scenario can be re-used by the cpp/tests integration variant
// later if we ever consolidate the two test paths.

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"time"

	"go.uber.org/zap"

	"proto/scene"
	"robot/config"
	"robot/generated/pb/game"
	"robot/logic/gameobject"
	"robot/metrics"
	"robot/pkg"
)

// CurrencyType.Gold mirrors cpp/libs/modules/currency/constants/currency.h
// kCurrencyGold = 0. Hard-coded here rather than imported from a generated
// enum because the proto side only exposes uint32 (no Go enum is generated
// for the cpp-only header).
const currencyTypeGold uint32 = 0

// readGoldBalance asks the scene for the player's full CurrencyComp and
// returns the value at slot=Gold. Caller owns the lifecycle of `gc` /
// `player` — this helper is just the request/wait pair.
func readGoldBalance(gc *pkg.GameClient, player *gameobject.Player, stats *metrics.Stats, timeout time.Duration) (uint64, error) {
	// Reset the ready channel for the *next* response: this helper may be
	// called twice in the same scenario (pre/post AddCurrency) and we want
	// each call to wait for a fresh response, not return immediately on the
	// channel that was closed last time.
	player.ResetCurrencyListReady()

	if err := gc.SendRequest(game.SceneCurrencyClientPlayerGetCurrencyListMessageId, &scene.GetCurrencyListRequest{}); err != nil {
		return 0, fmt.Errorf("send GetCurrencyList: %w", err)
	}
	stats.MsgSent()

	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()
	if err := player.WaitCurrencyListReady(ctx); err != nil {
		return 0, fmt.Errorf("wait GetCurrencyList response: %w", err)
	}
	bal, ok := player.GetCurrencyValue(currencyTypeGold)
	if !ok {
		return 0, fmt.Errorf("currency list landed but slot=gold absent")
	}
	return bal, nil
}

// gmAddGold sends GmAddCurrency for `amount` gold and waits for the
// response so we know the mutation has been applied to in-memory
// CurrencyComp on the scene side. Returns the post-add balance.
func gmAddGold(gc *pkg.GameClient, player *gameobject.Player, stats *metrics.Stats, amount int64, timeout time.Duration) (uint64, error) {
	player.ResetCurrencyAddReady()

	req := &scene.GmAddCurrencyRequest{
		CurrencyType: currencyTypeGold,
		Amount:       amount,
	}
	if err := gc.SendRequest(game.SceneCurrencyClientPlayerGmAddCurrencyMessageId, req); err != nil {
		return 0, fmt.Errorf("send GmAddCurrency: %w", err)
	}
	stats.MsgSent()

	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()
	if err := player.WaitCurrencyAddReady(ctx); err != nil {
		return 0, fmt.Errorf("wait GmAddCurrency response: %w", err)
	}
	bal, _ := player.GetLastBalanceAfter()
	return bal, nil
}

// currencyCrashWindowSnapshot is the round-trip the scenario performs:
// login → read pre balance → GM-add → read post balance. Returns the
// numbers the driver script needs to log. addAmount=0 disables the
// AddCurrency step (used by the post-kill replay — we just want to
// re-read the balance after the scene restart).
func currencyCrashWindowSnapshot(
	host string, port int, account, password string,
	stats *metrics.Stats, tokenPayload, tokenSig []byte,
	addAmount int64,
) (preBalance, postBalance uint64, err error) {
	gc, player, prepErr := prepareBehaviorClient(host, port, account, password, stats, tokenPayload, tokenSig)
	if prepErr != nil {
		err = fmt.Errorf("prepare client: %w", prepErr)
		return
	}
	defer func() {
		gameobject.PlayerList.Delete(gc.PlayerId)
		gc.Close()
	}()

	pre, readErr := readGoldBalance(gc, player, stats, 5*time.Second)
	if readErr != nil {
		err = fmt.Errorf("read pre balance: %w", readErr)
		return
	}
	preBalance = pre

	if addAmount == 0 {
		postBalance = pre
		return
	}

	post, addErr := gmAddGold(gc, player, stats, addAmount, 5*time.Second)
	if addErr != nil {
		err = fmt.Errorf("gm add: %w", addErr)
		return
	}
	postBalance = post

	zap.L().Info("currency-crash-window snapshot",
		zap.Uint64("player", gc.PlayerId),
		zap.Uint64("pre", pre),
		zap.Int64("delta", addAmount),
		zap.Uint64("post", post))
	return
}

// testCurrencyCrashWindow is the in-process scenario variant. It runs the
// pre-kill leg (snapshot pre, add gold, snapshot post) WITHOUT actually
// killing the scene — used as a smoke test that the wiring works and as
// the case D ("normal logout, no kill") regression check.
//
// The full 4-case matrix (A short window / B cross-period / C accelerated /
// D normal logout) is driven from the orchestration script which calls
// the robot in `currency-crash-snapshot` sub-mode (see currencyCrashSnapshotMode).
func testCurrencyCrashWindow(host string, port int, account, password string, stats *metrics.Stats, tokenPayload, tokenSig []byte) testResult {
	start := time.Now()
	const addAmount int64 = 10000

	pre, post, err := currencyCrashWindowSnapshot(host, port, account, password, stats, tokenPayload, tokenSig, addAmount)
	if err != nil {
		return testResult{Elapsed: time.Since(start), Detail: err.Error()}
	}
	if post != pre+uint64(addAmount) {
		return testResult{
			Elapsed: time.Since(start),
			Detail:  fmt.Sprintf("balance mismatch: pre=%d delta=%d post=%d (expected post=pre+delta)", pre, addAmount, post),
		}
	}
	return testResult{
		Passed:  true,
		Elapsed: time.Since(start),
		Detail:  fmt.Sprintf("pre=%d +%d -> post=%d", pre, addAmount, post),
	}
}

// currencyCrashSnapshot is the JSON shape written by mode=currency-crash-snapshot.
// One file per leg (pre-kill / post-restart); the orchestration script diffs
// the two and tabulates per-case results.
type currencyCrashSnapshot struct {
	Account     string `json:"account"`
	PlayerID    uint64 `json:"player_id"`
	PreBalance  uint64 `json:"pre_balance"`
	PostBalance uint64 `json:"post_balance"`
	AddAmount   int64  `json:"add_amount"` // 0 for read-only legs
	TimestampMs int64  `json:"timestamp_ms"`
	Error       string `json:"error,omitempty"`
}

// runCurrencyCrashSnapshotMode is the main.go entry point for
// `mode: currency-crash-snapshot`. It does ONE snapshot pass (login →
// optional GmAddCurrency → readback) and writes the result to
// CurrencyCrash.OutputPath as JSON. Caller (PowerShell script) is
// responsible for everything else: clearing data, starting/killing scene,
// retrying on transient failures, comparing snapshots across legs.
func runCurrencyCrashSnapshotMode(
	host string, port int, cfg *config.Config, stats *metrics.Stats,
	tokenPayload, tokenSig []byte,
) {
	account := cfg.CurrencyCrash.Account
	if account == "" {
		account = fmt.Sprintf(cfg.AccountFmt, 1)
	}
	outputPath := cfg.CurrencyCrash.OutputPath
	if outputPath == "" {
		outputPath = "logs/currency_crash_window/snapshot.json"
	}

	snap := currencyCrashSnapshot{
		Account:     account,
		AddAmount:   cfg.CurrencyCrash.AddAmount,
		TimestampMs: time.Now().UnixMilli(),
	}

	// Capture the player_id by piggybacking on currencyCrashWindowSnapshot.
	// We open a separate prepareBehaviorClient call instead of refactoring
	// the helper because mode-snapshot wants the player_id explicitly while
	// the in-process variant doesn't care; one extra getter is cheaper than
	// changing a stable function's signature.
	gc, player, prepErr := prepareBehaviorClient(host, port, account, cfg.Password, stats, tokenPayload, tokenSig)
	if prepErr != nil {
		snap.Error = "prepare client: " + prepErr.Error()
		writeCurrencyCrashSnapshot(outputPath, snap)
		zap.L().Error("currency-crash-snapshot prepare failed", zap.Error(prepErr))
		return
	}
	defer func() {
		gameobject.PlayerList.Delete(gc.PlayerId)
		gc.Close()
	}()
	snap.PlayerID = gc.PlayerId

	pre, readErr := readGoldBalance(gc, player, stats, 5*time.Second)
	if readErr != nil {
		snap.Error = "read pre balance: " + readErr.Error()
		writeCurrencyCrashSnapshot(outputPath, snap)
		zap.L().Error("currency-crash-snapshot pre-read failed", zap.Error(readErr))
		return
	}
	snap.PreBalance = pre
	snap.PostBalance = pre

	if cfg.CurrencyCrash.AddAmount != 0 {
		post, addErr := gmAddGold(gc, player, stats, cfg.CurrencyCrash.AddAmount, 5*time.Second)
		if addErr != nil {
			snap.Error = "gm add: " + addErr.Error()
			writeCurrencyCrashSnapshot(outputPath, snap)
			zap.L().Error("currency-crash-snapshot add failed", zap.Error(addErr))
			return
		}
		snap.PostBalance = post
	}

	writeCurrencyCrashSnapshot(outputPath, snap)
	zap.L().Info("currency-crash-snapshot leg complete",
		zap.String("account", account),
		zap.Uint64("player_id", snap.PlayerID),
		zap.Uint64("pre", snap.PreBalance),
		zap.Int64("delta", snap.AddAmount),
		zap.Uint64("post", snap.PostBalance),
		zap.String("output", outputPath))
}

// writeCurrencyCrashSnapshot serializes the snapshot to disk. Errors here
// are logged and ignored — the run already happened, we don't want a stat
// failure to mask the real result.
func writeCurrencyCrashSnapshot(path string, snap currencyCrashSnapshot) {
	if dir := filepath.Dir(path); dir != "" && dir != "." {
		if err := os.MkdirAll(dir, 0o755); err != nil {
			zap.L().Error("mkdir snapshot dir", zap.String("dir", dir), zap.Error(err))
		}
	}
	body, err := json.MarshalIndent(snap, "", "  ")
	if err != nil {
		zap.L().Error("marshal snapshot", zap.Error(err))
		return
	}
	if err := os.WriteFile(path, body, 0o644); err != nil {
		zap.L().Error("write snapshot", zap.String("path", path), zap.Error(err))
	}
}
