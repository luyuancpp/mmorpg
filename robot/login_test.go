package main

import (
	"testing"

	"robot/pkg"
)

// TestSendDisconnectBestEffort_NilClient_DoesNotPanic guards the primary
// robustness contract: this helper is registered as a `defer` on the robot
// exit path, so an exception inside it would abort the rest of Close()
// and, worse, bubble into the caller's deferred stats.Disconnected().
//
// Nil / zero-value GameClient is the pathological case we care about —
// early-exit branches in runRobotOnce() can land here before
// NewGameClient actually wires the inner muduo client.
func TestSendDisconnectBestEffort_NilClient_DoesNotPanic(t *testing.T) {
	defer func() {
		if r := recover(); r != nil {
			t.Fatalf("sendDisconnectBestEffort panicked on nil gc: %v", r)
		}
	}()
	// Pass a literal nil — the early "gc == nil" guard must short-circuit
	// before anything tries to poke the muduo client.
	sendDisconnectBestEffort(nil)
}

// TestSendDisconnectBestEffort_ZeroPlayerId_ShortCircuits covers the
// "robot hasn't reached EnterGame yet" window: gc exists, but PlayerId is
// still zero. Without the early guard we'd send a bogus Disconnect
// carrying playerId=0, which login cannot correlate with any session —
// it would just log-spam.
//
// We assert the guard by confirming no panic AND that a GameClient with
// a nil inner *muduo.Client still survives the call; if the helper
// forgot its guard and reached gc.SendRequest(), the nil inner client
// would nil-deref and crash.
func TestSendDisconnectBestEffort_ZeroPlayerId_ShortCircuits(t *testing.T) {
	defer func() {
		if r := recover(); r != nil {
			t.Fatalf("sendDisconnectBestEffort panicked on PlayerId=0: %v", r)
		}
	}()
	gc := &pkg.GameClient{Account: "nobody"} // PlayerId defaults to 0
	sendDisconnectBestEffort(gc)
}
