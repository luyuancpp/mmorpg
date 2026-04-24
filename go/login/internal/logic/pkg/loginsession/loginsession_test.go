package loginsession

import "testing"

func TestSessionKeyFormat(t *testing.T) {
	// Use math.MaxUint32 to exercise the upper bound of the session-id space
	// without overflowing the parameter type (the original literal 4294967301
	// was 6 past the uint32 ceiling).
	const maxUint32 uint32 = 0xFFFFFFFF
	got := sessionKey(maxUint32)
	want := "login_session:4294967295"
	if got != want {
		t.Errorf("sessionKey(%d) = %q, want %q", maxUint32, got, want)
	}
}
