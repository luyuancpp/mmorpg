package loginsession

import "testing"

func TestTransitionTable(t *testing.T) {
	tests := []struct {
		from, to string
		valid    bool
	}{
		// Save: only from empty
		{"", StepLoggedIn, true},
		{"", StepCreatingChar, false},
		{"", StepEnteringGame, false},
		// From logged_in
		{StepLoggedIn, StepCreatingChar, true},
		{StepLoggedIn, StepEnteringGame, true},
		{StepLoggedIn, StepLoggedIn, false},
		// From creating_char
		{StepCreatingChar, StepLoggedIn, true},
		{StepCreatingChar, StepEnteringGame, true},
		{StepCreatingChar, StepCreatingChar, false},
		// From entering_game (dead end — key safety property)
		{StepEnteringGame, StepLoggedIn, false},
		{StepEnteringGame, StepCreatingChar, false},
		{StepEnteringGame, StepEnteringGame, false},
	}

	for _, tt := range tests {
		got := isValidTransition(tt.from, tt.to)
		if got != tt.valid {
			t.Errorf("%q -> %q: got %v, want %v", tt.from, tt.to, got, tt.valid)
		}
	}
}
