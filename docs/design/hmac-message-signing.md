# Per-Message HMAC Signing — Design (todo.md #76)

> **Status:** 2026-05-14 — first slice landed (key delivery channel ready);
> wire integration of business-message HMAC tagging is **not yet wired**.
> This document captures the decision, what's done, and what remains.

---

## Problem

`cpp/libs/engine/core/network/codec/codec.cpp` already computes an
adler32 checksum over each on-wire frame and rejects mismatches in
`ProtobufCodec::parse`. adler32 protects against **accidental** corruption
(network noise, TCP failure modes that the kernel didn't catch) but not
against **deliberate** tampering — anyone who can flip bits on the wire
can recompute adler32 in microseconds.

todo #76 calls out the gap. Adding HMAC-SHA256 over the body, keyed by a
session-scoped secret only the legitimate client and server know, raises
the bar from "kernel checksum" to "you need the key to forge".

---

## Decision

### 1. Don't touch `codec.cpp`

`codec.cpp` carries a muduo upstream copyright header. CLAUDE.md §2 #1
forbids modifying muduo framework code. **adler32 stays.** HMAC sits in
the application layer, not in the codec.

### 2. Reuse the existing GateToken delivery channel for the key

The HMAC key is carried in `GateTokenPayload.hmac_session_key` (new
field, byte 32 random). The existing flow does the work for us:

```
Login (AssignGate)
    ↓ generate 32 random bytes via crypto/rand
    ↓ stuff into GateTokenPayload.hmac_session_key
    ↓ HMAC-SHA256 sign the WHOLE payload with the per-deployment gate secret
    ↓ return (payload + signature) to the client through Java Gateway
Client → TCP Gate
    ↓ ClientTokenVerifyRequest{ payload, signature }
Gate verifies signature → unmarshal → install hmac_session_key into SessionInfo
```

The integrity of `hmac_session_key` rides on the existing HMAC-SHA256
signature over `payload`. An attacker who can flip bits in transit also
needs the gate secret (the per-deployment one held by Login + Gate) to
re-sign the tampered payload, which is exactly the security boundary
the gate token is already enforcing. No new trust assumptions.

**Why this and not a fresh per-session key generated from a DH handshake**:
DH would survive a Login compromise but adds a round trip and meaningful
client SDK complexity. Login already holds the master gate secret; if
that's compromised the entire authentication story falls anyway.

### 3. One key per `AssignGate` call

Every gate token request gets a freshly generated 32-byte key.
- No Redis state to rotate
- A reconnect that goes through `AssignGate` again gets a new key by
  construction
- The 30s lease window in `player_locator` already keeps reconnect
  inside the same key (no new AssignGate during that window)

### 4. Layered with adler32, not replacing it

`codec.cpp` keeps adler32 for malformed-frame rejection. HMAC is
checked **after** parsing inside the business-message handler. This
means:
- a frame with bad adler32 is dropped at the codec layer (fast, no
  proto parse)
- a frame with valid adler32 but wrong HMAC reaches the handler and
  is dropped there with a `LOG_WARN` carrying enough triage detail
  (#70/#125's hook covers this once the handler returns an error)

---

## What landed in this commit (2026-05-14)

| Layer | File | Change |
|---|---|---|
| proto | `proto/common/base/message.proto` | `GateTokenPayload.hmac_session_key = 4` (bytes) — declared, awaiting `proto-gen-run` to materialize in `go/proto/common/base/*.pb.go` and `cpp/generated/proto/common/base/*.pb.{h,cc}` |
| docs | `docs/design/hmac-message-signing.md` | this file |

**Intentionally NOT in this commit**: Go-side population of
`payload.HmacSessionKey` in `gatetoken.go`. That file is part of the
in-flight `#102` (login queue) change set in the working tree, with its
own pending `proto-gen` regeneration. Touching it from a separate
commit would interleave the two work streams. The chain finishes when:

  1. `#102` commits `gatetoken.go` + the regenerated proto stubs.
  2. A follow-up `#76` commit adds the `crypto/rand` 32-byte
     `sessionKey` generation + assignment to `payload.HmacSessionKey`.

The proto field is empty/unused as long as the rest of the chain isn't
wired — Gate just ignores `hmac_session_key`, falls back to the
adler32-only path. **Backward compatible**: an old client and a new
server (or vice versa) keep working at the adler32 floor.

---

## What's NOT done yet (tracked as #76 follow-ups)

These are real work, sized correctly so we don't pretend the whole HMAC
story is done with just the key delivery in place:

### A. Gate-side key intake — **S, ~half day**
- After `ClientTokenVerify` succeeds in `client_message_processor.cpp`,
  unmarshal `GateTokenPayload` and stash `hmac_session_key` into the
  per-session state (alongside `session_id` / player binding).
- Empty key → leave `enable_hmac` false on the session and skip
  verification, so the rollout is not gated on every client updating
  at once.

### B. Wire-format slot for the HMAC tag — **S, ~half day**
- Add `bytes hmac_tag = N;` to the carrier message
  (`GameRpcMessage` or whichever envelope wraps client-to-gate
  business calls — pick the same level adler32 covers in codec).
- Empty/missing `hmac_tag` is allowed when the session has no
  `hmac_session_key` (rollback path).

### C. Compute + verify — **M, 1–2 days**
- Client SDK computes `HMAC-SHA256(hmac_session_key, body)` and
  populates the tag. Body = the same byte range adler32 covers, so
  the two checksums are independently computable and one's failure
  doesn't poison the other.
- Gate verifies on receipt. Mismatch → `SendErrorResponse` with a
  new error code `kHmacMismatch`; the #70/#125 hook already logs
  the message_id + method + stack.
- Cost note: per-message HMAC-SHA256 over a 1KB body is ~3μs on
  modern x86 with hardware SHA. For a 10k connection gate at 50
  msgs/s/client = 500k msgs/s = ~1.5s of CPU per second across
  the box — fits in budget but worth a stress run before final
  rollout.

### D. Counter / metric — **S, ~half day**
- `gate_hmac_mismatch_total{zone}` Prometheus counter so an attack
  campaign shows up on the dashboard rather than being lost in
  per-session WARN spam.

### E. Client SDK rollout — **separate, owner: client team**
- Out of scope for the server side. Server keeps the empty-key
  fallback indefinitely so client rollout can happen on its own
  schedule.

---

## Open questions

1. **Key rotation on long-lived sessions?** Currently a session keeps
   the same key for the lifetime of the TCP connection (potentially
   hours). Acceptable for an anti-tamper key (vs anti-replay where
   you'd want a counter). Revisit if a deployment specifically wants
   keys to roll within a session.

2. **HMAC over compressed body or pre-compression?** Wait until B/C
   land — the codec doesn't compress today, so the question is
   theoretical. When/if compression is added it should sit between
   HMAC and adler32 (adler32 over wire bytes, HMAC over plaintext
   body), so a corrupted compression can't masquerade as a HMAC bug.

---

## References

- `proto/common/base/message.proto` — `GateTokenPayload` /
  `ClientTokenVerifyRequest`
- `go/login/internal/logic/pkg/loginqueue/gatetoken.go` — key
  generation site
- `cpp/libs/engine/core/network/codec/codec.cpp` — adler32 floor
  (untouched, intentionally)
- `cpp/nodes/gate/handler/rpc/client_message_processor.cpp` — where
  the gate-side key intake will land in slice A
- todo.md #76, #70, #125 — origin and triage interactions
