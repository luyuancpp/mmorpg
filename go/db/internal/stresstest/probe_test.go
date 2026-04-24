package stresstest

import (
	"encoding/hex"
	"testing"
)

// TestComputeSig_KnownVectors locks in the on-wire bytes of the test_sig
// algorithm. The cpp Scene-side mirror in
// cpp/libs/services/scene/player/system/stress_test_probe.cpp MUST produce
// the same bytes for the same inputs — if either side changes, this test
// (and its cpp twin) MUST be updated together so the verifier still
// matches stamped probes from both producers.
func TestComputeSig_KnownVectors(t *testing.T) {
	cases := []struct {
		name     string
		playerID uint64
		msgType  string
		testSeq  uint64
	}{
		{"zero", 0, "", 0},
		{"player_database/seq=1", 1_000_000, "player_database", 1},
		{"player_database/seq=42", 1_000_001, "player_database", 42},
		{"player_database_1/seq=1234567890", 1_234_567, "player_database_1", 1_234_567_890},
	}

	// Locked-in golden vectors. The cpp Scene-side mirror MUST produce
	// the same bytes for these inputs. If you change the algorithm, you
	// must update BOTH this map AND the parallel test fixture in
	// cpp/libs/services/scene/player/system/stress_test_probe_test.cpp.
	want := map[string]string{
		"zero":                             "2a17b88efd5cab301cf4e32e4be30d9c",
		"player_database/seq=1":            "f2ab46ca2c719701bac5fe735d87010d",
		"player_database/seq=42":           "8db98b225101f10d74304876a0197899",
		"player_database_1/seq=1234567890": "d7356f9c1397ad034095480e33287b97",
	}

	for _, c := range cases {
		got := hex.EncodeToString(ComputeSig(c.playerID, c.msgType, c.testSeq))
		expected, ok := want[c.name]
		if !ok {
			t.Errorf("%s: missing golden vector", c.name)
			continue
		}
		if got != expected {
			t.Errorf("%s: sig=%s, want=%s", c.name, got, expected)
		}
	}
}

func TestVerifyProbe_RoundTrip(t *testing.T) {
	const playerID uint64 = 42
	const msgType = "player_database"

	for seq := uint64(0); seq < 100; seq++ {
		p := MakeProbe(playerID, msgType, seq)
		gotSeq, ok := VerifyProbe(playerID, msgType, p)
		if !ok {
			t.Fatalf("seq=%d: VerifyProbe returned sigOK=false for our own probe", seq)
		}
		if gotSeq != seq {
			t.Fatalf("seq=%d: VerifyProbe returned seq=%d", seq, gotSeq)
		}
	}
}

func TestVerifyProbe_DetectsTampering(t *testing.T) {
	const playerID uint64 = 42
	const msgType = "player_database"

	// Tamper case 1: same seq stamped under a different player_id.
	p := MakeProbe(playerID+1, msgType, 5)
	if _, ok := VerifyProbe(playerID, msgType, p); ok {
		t.Errorf("must detect mis-routed payload across player_ids")
	}

	// Tamper case 2: wrong msg_type.
	p = MakeProbe(playerID, "player_database_1", 5)
	if _, ok := VerifyProbe(playerID, msgType, p); ok {
		t.Errorf("must detect mis-routed payload across msg_types")
	}

	// Tamper case 3: a single bit flipped in the sig.
	p = MakeProbe(playerID, msgType, 5)
	p.TestSig[0] ^= 0x01
	if _, ok := VerifyProbe(playerID, msgType, p); ok {
		t.Errorf("must detect single-bit corruption in sig")
	}

	// Tamper case 4: short sig.
	p = MakeProbe(playerID, msgType, 5)
	p.TestSig = p.TestSig[:8]
	if _, ok := VerifyProbe(playerID, msgType, p); ok {
		t.Errorf("must reject truncated sig")
	}
}
