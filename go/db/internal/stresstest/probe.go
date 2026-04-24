// Package stresstest provides shared helpers for the data-consistency
// stress-testing pipeline: stamping the PlayerStressTestProbe field on
// outgoing writes (data_stress driver, robot data-stress mode, cpp Scene
// when STRESS_TEST_PROBE=1) and re-verifying it on the consumer side
// (verifier CLI).
//
// The test_sig is intentionally NOT a cryptographic MAC — it has no secret
// key. Its only purpose is to detect torn / mis-routed / re-assembled
// payloads (e.g. cache key collision, partition mis-route, body swap).
// Two independent FNV-1a-64 hashes (seeded differently) over
// (player_id_be8 || msg_type_bytes || test_seq_be8) give 128 bits of
// collision resistance for tear detection — overkill for the use case but
// trivial to reimplement in any language without crypto dependencies.
//
// The cpp Scene-side mirror lives in
// cpp/libs/services/scene/player/system/stress_test_probe.{h,cpp};
// keep the algorithm in lockstep across both languages.
package stresstest

import (
	"encoding/binary"

	componentpb "proto/common/component"
)

// SigLen is the test_sig length in bytes (16 = 128 bits).
const SigLen = 16

// FNV-1a 64-bit constants (RFC, see github.com/golang/go/src/hash/fnv).
const (
	fnv64Offset uint64 = 0xcbf29ce484222325
	fnv64Prime  uint64 = 0x00000100000001b3

	// Two distinct seeds give two independent 64-bit hashes that we
	// concatenate to form the 128-bit signature. Mixing the seed in via
	// XOR before processing is functionally equivalent to "salting" the
	// hash and stays trivial to reproduce in C++.
	sigSeedHi uint64 = 0x9e3779b97f4a7c15 // golden ratio constant
	sigSeedLo uint64 = 0xbf58476d1ce4e5b9 // splitmix64 constant
)

func fnv1a64(seed uint64, parts ...[]byte) uint64 {
	h := fnv64Offset ^ seed
	for _, p := range parts {
		for _, b := range p {
			h ^= uint64(b)
			h *= fnv64Prime
		}
	}
	return h
}

// ComputeSig returns the deterministic test_sig for a given probe payload.
// Inputs are concatenated in canonical big-endian form so producers and
// verifiers across languages compute the same value.
func ComputeSig(playerID uint64, msgType string, testSeq uint64) []byte {
	var pid, seq [8]byte
	binary.BigEndian.PutUint64(pid[:], playerID)
	binary.BigEndian.PutUint64(seq[:], testSeq)

	hi := fnv1a64(sigSeedHi, pid[:], []byte(msgType), seq[:])
	lo := fnv1a64(sigSeedLo, pid[:], []byte(msgType), seq[:])

	out := make([]byte, SigLen)
	binary.BigEndian.PutUint64(out[0:8], hi)
	binary.BigEndian.PutUint64(out[8:16], lo)
	return out
}

// MakeProbe builds a fully-populated PlayerStressTestProbe for a write.
func MakeProbe(playerID uint64, msgType string, testSeq uint64) *componentpb.PlayerStressTestProbe {
	return &componentpb.PlayerStressTestProbe{
		TestSeq: testSeq,
		TestSig: ComputeSig(playerID, msgType, testSeq),
	}
}

// VerifyProbe re-computes the expected sig and reports whether the probe
// matches. A nil probe returns (seq=0, sigOK=false). The verifier treats
// "row exists, probe missing" as "seq=0, sig invalid" so it can
// distinguish "never stamped" from "wrong seq".
func VerifyProbe(playerID uint64, msgType string, p *componentpb.PlayerStressTestProbe) (seq uint64, sigOK bool) {
	if p == nil {
		return 0, false
	}
	want := ComputeSig(playerID, msgType, p.GetTestSeq())
	if len(p.GetTestSig()) != len(want) {
		return p.GetTestSeq(), false
	}
	for i := range want {
		if want[i] != p.GetTestSig()[i] {
			return p.GetTestSeq(), false
		}
	}
	return p.GetTestSeq(), true
}
