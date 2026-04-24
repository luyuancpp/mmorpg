package kafka

import (
	"bytes"
	"encoding/binary"
	"testing"
)

// These tests guard the on-wire retry-queue payload format produced by
// wrapRetryPayload / consumed by unwrapRetryPayload. The format is the
// single source of truth for the per-key applied-seq guard (Bug 3 / TC5b),
// so a silent change here would re-open the regression without any of the
// higher-level kafka consumer tests noticing.

func TestRetryPayload_RoundTrip(t *testing.T) {
	cases := []struct {
		name string
		seq  uint64
		body []byte
	}{
		{"empty body", 1, nil},
		{"small body", 42, []byte("hello")},
		{"max seq", ^uint64(0), bytes.Repeat([]byte{0xAB}, 256)},
		{"binary body that looks like proto", 7, []byte{0x08, 0x01, 0x12, 0x03, 'a', 'b', 'c'}},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			wrapped := wrapRetryPayload(tc.seq, tc.body)
			if got := wrapped[0]; got != retryPayloadMagic {
				t.Fatalf("magic byte: want 0x%02x got 0x%02x", retryPayloadMagic, got)
			}
			if got := binary.BigEndian.Uint64(wrapped[1:9]); got != tc.seq {
				t.Fatalf("seq round-trip: want %d got %d", tc.seq, got)
			}
			if !bytes.Equal(wrapped[9:], tc.body) {
				t.Fatalf("body round-trip mismatch")
			}

			gotSeq, gotBody := unwrapRetryPayload(wrapped)
			if gotSeq != tc.seq {
				t.Fatalf("unwrap seq: want %d got %d", tc.seq, gotSeq)
			}
			if !bytes.Equal(gotBody, tc.body) {
				t.Fatalf("unwrap body mismatch")
			}
		})
	}
}

// TestRetryPayload_LegacyPayloadDrainsAsZeroSeq guards the backward-
// compat path: any in-flight retry produced by an older binary (which
// pushed bare DBTask bytes onto the queue) MUST still drain after a
// rolling upgrade. The applied-seq guard then degrades to "best-effort
// last-write-wins" for that single message, which is acceptable.
//
// The unambiguity argument: proto3 wire format always starts with a
// non-zero field tag, and the smallest possible tag byte is 0x08 (field
// 1, varint). 0x01 cannot appear as the first byte of a valid proto3
// payload, so unwrapRetryPayload's discriminator is unconditionally safe.
func TestRetryPayload_LegacyPayloadDrainsAsZeroSeq(t *testing.T) {
	legacy := []byte{0x08, 0x01, 0x12, 0x03, 'a', 'b', 'c'}
	seq, body := unwrapRetryPayload(legacy)
	if seq != 0 {
		t.Fatalf("legacy payload should yield seq=0, got %d", seq)
	}
	if !bytes.Equal(body, legacy) {
		t.Fatalf("legacy payload body must pass through unchanged")
	}
}

// TestRetryPayload_TruncatedFallsBackToLegacy guards a corner case:
// a payload short enough to lack the [magic][seq] prefix MUST be
// surfaced as a legacy payload (seq=0, body unchanged) rather than
// panic on a slice-out-of-range read.
func TestRetryPayload_TruncatedFallsBackToLegacy(t *testing.T) {
	for _, length := range []int{0, 1, 5, 8} {
		buf := bytes.Repeat([]byte{retryPayloadMagic}, length)
		seq, body := unwrapRetryPayload(buf)
		if seq != 0 {
			t.Fatalf("len=%d: short payload must yield seq=0, got %d", length, seq)
		}
		if !bytes.Equal(body, buf) {
			t.Fatalf("len=%d: short payload body must pass through unchanged", length)
		}
	}
}

// TestRetryPayload_MagicByteIsUnambiguous documents the invariant that
// retryPayloadMagic cannot collide with any legitimate first byte of a
// proto3-serialized DBTask. If this test ever fires, either the magic
// byte must change or the format must gain a length-prefix instead.
func TestRetryPayload_MagicByteIsUnambiguous(t *testing.T) {
	if retryPayloadMagic >= 0x08 {
		t.Fatalf("retryPayloadMagic=0x%02x can collide with proto3 wire-format first byte (≥ 0x08)",
			retryPayloadMagic)
	}
}
