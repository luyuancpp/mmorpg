package clientplayerloginlogic

import "testing"

// TestShouldRejectLegacyRequest pins the kill-switch truth table for the
// ARCH §12 T+2 step. If anyone later swaps the and/or, the legacy path
// either silently stays open (operationally invisible) or breaks the
// new-path traffic too — both produce production incidents that this
// table-driven test would catch at PR time.
//
// The test deliberately uses a name-driven table rather than parametric
// labels so the failure message in CI is self-explanatory ("isLegacy +
// flag-disabled should reject") rather than "case 3 failed".
func TestShouldRejectLegacyRequest(t *testing.T) {
	cases := []struct {
		name        string
		isLegacy    bool
		flagEnabled bool
		want        bool
	}{
		{
			name:        "new HTTP path is never rejected by the legacy kill switch",
			isLegacy:    false,
			flagEnabled: true,
			want:        false,
		},
		{
			name:        "new HTTP path stays allowed even when flag is off",
			isLegacy:    false,
			flagEnabled: false,
			want:        false,
		},
		{
			name:        "legacy gate path served while flag default-on",
			isLegacy:    true,
			flagEnabled: true,
			want:        false,
		},
		{
			name:        "legacy gate path rejected once ops flips flag off (T+2)",
			isLegacy:    true,
			flagEnabled: false,
			want:        true,
		},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			if got := shouldRejectLegacyRequest(tc.isLegacy, tc.flagEnabled); got != tc.want {
				t.Errorf("shouldRejectLegacyRequest(isLegacy=%v, flagEnabled=%v) = %v, want %v",
					tc.isLegacy, tc.flagEnabled, got, tc.want)
			}
		})
	}
}
