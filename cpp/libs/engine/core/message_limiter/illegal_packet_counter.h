#pragma once

#include <cstdint>
#include <cstdlib>

// Illegal-packet counter (todo.md #236).
//
// MessageLimiter (rate-limit per messageId) and the incoming validation
// chain in gate's client_message_processor — kRateLimitExceeded, unknown
// messageId, parse failures, send-without-auth, future HMAC mismatch
// from #76 — all reject individual packets. Each rejection alone is
// fine; what matters is the *cumulative* count for a session. A
// connected client that produces 1000 illegal packets in 30 seconds is
// almost certainly an attack tool — the right action is to drop the
// connection, not to keep replying with error tips.
//
// This counter lives in SessionInfo (see SessionInfo::illegalPacketCount).
// The helper here just centralizes "is the counter past the kill
// threshold?" so call sites in the gate don't all hard-code the
// threshold separately.
//
// Threshold:
//   - Default 50 — generous enough to absorb a misbehaving client's
//     handful of bugs across a session, tight enough that a fuzzer
//     hits it within seconds.
//   - Override via env GATE_ILLEGAL_PACKET_THRESHOLD for stress tests
//     that intentionally fire bad packets.
//   - 0 disables the kill (counter still increments — useful when
//     debugging which packets are being rejected without losing the
//     session).
class IllegalPacketCounter {
public:
    // Increment the per-session counter and return true iff this hit
    // crossed the kill threshold. The first call to return true is the
    // signal for the caller to forceClose the connection; subsequent
    // calls (if the connection isn't actually closed yet) keep
    // returning true, which is fine — forceClose is idempotent.
    static bool RegisterAndShouldKill(uint32_t& counter)
    {
        ++counter;
        const uint32_t threshold = ThresholdFromEnv();
        if (threshold == 0) {
            return false; // kill disabled
        }
        return counter >= threshold;
    }

    // Read the threshold from env once per call. Cheap (env reads are
    // a syscall on first use, cached afterwards via getenv); kept here
    // rather than memoized so a deployment can re-roll the threshold
    // without restart by re-exec'ing into a new env. Callers hit this
    // at most once per illegal packet, not per legal one.
    static uint32_t ThresholdFromEnv()
    {
        if (const char* env = std::getenv("GATE_ILLEGAL_PACKET_THRESHOLD"); env != nullptr) {
            const long parsed = std::strtol(env, nullptr, 10);
            if (parsed >= 0) {
                return static_cast<uint32_t>(parsed);
            }
        }
        return 50; // default kill threshold
    }

    // Reset on a fresh ClientTokenVerify (a successful re-handshake) so
    // the next session lifetime starts from zero. Exposed as a separate
    // method for clarity at the call site.
    static void Reset(uint32_t& counter) { counter = 0; }
};
