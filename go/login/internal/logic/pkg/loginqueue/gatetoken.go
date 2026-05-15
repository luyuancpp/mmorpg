// gatetoken.go — gate selection + HMAC signing primitives shared by the
// AssignGate handler (fast path, no queue) and the dispatcher (slow path,
// signing on behalf of a popped queue entry).
//
// This is a lift-and-shift from the legacy `pregate.AssignGate` /
// `pregate.signHMAC` pair, made into a reusable helper so the queue
// dispatcher and the handler both use a single source of truth for gate
// token shape. The legacy file `pregate/getgatelistlogic.go` is removed in
// PR3.
//
// signQueueToken signs the OPAQUE queue token (different from gate token):
// it carries (queueId, zoneId, expire) and is verified server-side only.
// Reusing the gate HMAC secret is fine because the wire formats are
// disjoint — gate verifies a serialized GateTokenPayload proto, queue
// verifies a JSON blob with a "kind":"queue" tag.
package loginqueue

import (
	"crypto/hmac"
	"crypto/sha256"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"sort"
	"time"

	commonpb "proto/common/base"

	"google.golang.org/protobuf/proto"
)

// GateCandidate is the projection of NodeInfo we need for selection. Kept
// minimal so capacity.go can build it from any source (etcd watcher today,
// possibly a metrics-driven view later) without lugging the full proto.
type GateCandidate struct {
	NodeID      uint32
	IP          string
	Port        uint32
	PlayerCount uint32
	ZoneID      uint32
}

// PickAndSignGateToken selects the least-loaded gate from the candidate
// list (already filtered by zone) and signs a one-time HMAC token. Returns
// (admit, error) — admit is non-nil iff a gate was selected.
//
// Returned AdmitToken matches the wire shape AssignGateResponse expects, so
// callers can copy fields directly without re-marshalling.
func PickAndSignGateToken(candidates []GateCandidate, secret []byte, ttl time.Duration) (*AdmitToken, error) {
	if len(candidates) == 0 {
		return nil, fmt.Errorf("no gate candidates")
	}

	// Stable sort by player count so ties resolve deterministically — useful
	// when reading dispatcher logs to figure out why gate X kept getting
	// picked. Cost is O(n log n) where n = #gates per zone (small).
	sorted := make([]GateCandidate, len(candidates))
	copy(sorted, candidates)
	sort.SliceStable(sorted, func(i, j int) bool {
		if sorted[i].PlayerCount != sorted[j].PlayerCount {
			return sorted[i].PlayerCount < sorted[j].PlayerCount
		}
		return sorted[i].NodeID < sorted[j].NodeID
	})
	best := sorted[0]

	expireTS := time.Now().Add(ttl).Unix()
	payload := &commonpb.GateTokenPayload{
		GateNodeId:      best.NodeID,
		ZoneId:          best.ZoneID,
		ExpireTimestamp: expireTS,
	}
	payloadBytes, err := proto.Marshal(payload)
	if err != nil {
		return nil, fmt.Errorf("marshal gate token payload: %w", err)
	}

	signature := hmacHex(secret, payloadBytes)

	return &AdmitToken{
		IP:             best.IP,
		Port:           best.Port,
		TokenPayload:   payloadBytes,
		TokenSignature: signature,
		TokenDeadline:  expireTS,
	}, nil
}

// hmacHex computes HMAC-SHA256(secret, data) and returns the hex-encoded
// string as bytes. Matches the cpp gate verifier's HmacSha256Hex output
// (lowercase hex, no separators) so the on-wire bytes line up.
func hmacHex(secret, data []byte) []byte {
	mac := hmac.New(sha256.New, secret)
	mac.Write(data)
	return []byte(hex.EncodeToString(mac.Sum(nil)))
}

// ── Queue token (server-only, opaque to client) ────────────────────────

// queueTokenBody is the JSON we sign + base64-encode as the queue_token.
// Clients MUST treat it as opaque; we re-parse on the server only.
type queueTokenBody struct {
	Kind     string `json:"k"`
	QueueID  string `json:"q"`
	ZoneID   uint32 `json:"z"`
	ExpireTS int64  `json:"e"`
}

// signQueueToken returns base64url(JSON | "." | hex(HMAC)). We pick this
// shape over a Redis-stored opaque token because:
//   - Self-validating: server can reject expired/forged tokens before
//     touching Redis.
//   - No extra Redis key per token (the queue:token:{token} index is for
//     fast Lookup, but we could fall back to parse-only if Redis evicted
//     the index — Lookup currently relies on the index, kept for clarity).
func signQueueToken(secret []byte, queueID string, zoneID uint32, expireTS int64) (string, error) {
	body := queueTokenBody{Kind: "queue", QueueID: queueID, ZoneID: zoneID, ExpireTS: expireTS}
	bodyBytes, err := json.Marshal(body)
	if err != nil {
		return "", err
	}
	sig := hmacHex(secret, bodyBytes)
	combined := append(append(bodyBytes, '.'), sig...)
	return base64.RawURLEncoding.EncodeToString(combined), nil
}

// ParseAndVerifyQueueToken returns the embedded queueId/zoneId or an error
// if the signature is bad / token expired. Pure CPU — no Redis touch.
//
// On rejection it increments expiredTotal with a reason label so dashboards
// can distinguish "client tampering / forged token" from "client polled
// slower than the TTL". Note we record under zone=unknown when we can't
// parse the body (signature mismatch before unmarshalling); for TTL
// rejection we have the real zoneID.
func ParseAndVerifyQueueToken(secret []byte, token string) (queueID string, zoneID uint32, err error) {
	raw, err := base64.RawURLEncoding.DecodeString(token)
	if err != nil {
		recordExpired(0, ExpireReasonBadSignature)
		return "", 0, fmt.Errorf("decode queue token: %w", err)
	}
	dot := -1
	for i := len(raw) - 1; i >= 0; i-- {
		if raw[i] == '.' {
			dot = i
			break
		}
	}
	if dot <= 0 || dot == len(raw)-1 {
		recordExpired(0, ExpireReasonBadSignature)
		return "", 0, fmt.Errorf("malformed queue token")
	}
	bodyBytes := raw[:dot]
	sig := raw[dot+1:]
	expected := hmacHex(secret, bodyBytes)
	if !hmac.Equal(sig, expected) {
		recordExpired(0, ExpireReasonBadSignature)
		return "", 0, fmt.Errorf("queue token signature mismatch")
	}
	var body queueTokenBody
	if err := json.Unmarshal(bodyBytes, &body); err != nil {
		recordExpired(0, ExpireReasonBadSignature)
		return "", 0, fmt.Errorf("unmarshal queue token: %w", err)
	}
	if body.Kind != "queue" {
		recordExpired(body.ZoneID, ExpireReasonBadSignature)
		return "", 0, fmt.Errorf("queue token wrong kind: %s", body.Kind)
	}
	if time.Now().Unix() > body.ExpireTS {
		recordExpired(body.ZoneID, ExpireReasonTTL)
		return "", 0, fmt.Errorf("queue token expired")
	}
	return body.QueueID, body.ZoneID, nil
}
