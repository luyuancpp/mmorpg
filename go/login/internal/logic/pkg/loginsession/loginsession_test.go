package loginsession

import "testing"

func TestSessionKeyFormat(t *testing.T) {
	got := sessionKey(4294967301)
	want := "login_session:4294967301"
	if got != want {
		t.Errorf("sessionKey(4294967301) = %q, want %q", got, want)
	}
}
