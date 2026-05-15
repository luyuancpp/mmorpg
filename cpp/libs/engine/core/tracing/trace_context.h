#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <string_view>

// Distributed-tracing primitives — W3C Trace Context shape (todo.md #152).
//
// Goal of this slice: give every node a way to allocate a trace_id /
// span_id and pass it across the wire WITHOUT picking an OpenTelemetry
// SDK / Jaeger / Zipkin backend yet. The right backend decision lives at
// the deployment level (does ops want self-hosted Tempo? SaaS Honeycomb?
// per-zone Jaeger?) — see docs/design/distributed-tracing.md for the
// slice plan.
//
// What this header provides:
//   - 128-bit trace_id + 64-bit span_id generation matching the W3C
//     trace-id layout (lowercase hex, no separator).
//   - Parser / serializer for the `traceparent` HTTP header format
//     `00-{trace_id}-{span_id}-{flags}` so cross-service propagation is
//     wire-compatible with every existing observability vendor when
//     slice B picks one.
//   - Header-only, no protobuf dependency, no logging dependency. Safe
//     to drop into login (Go via cgo wrapper), gate (C++), scene (C++),
//     and even into the Java gateway via a one-page Java translation.
//
// What this header does NOT provide:
//   - Span lifecycle (start/end timing, parent/child tree). Those live
//     in the SDK that slice B picks.
//   - Sampling decisions. The `flags` byte holds the sampled bit per W3C
//     but the choice of "do we sample this trace" is policy.
//   - Wire propagation through Kafka / gRPC metadata. The shape is
//     here; the actual interceptors land in slice C (gRPC) / slice D
//     (Kafka headers).
//
// Why W3C trace-id, not a project-local id format:
//   - Direct compatibility with every OpenTelemetry exporter. Any
//     deployment that runs an OTel collector can adopt this slice
//     unchanged.
//   - 128-bit trace_id avoids global collision concerns at our scale
//     forever (~3.4e38 IDs, vs ~1.8e19 for 64-bit — meaningful when a
//     campaign generates billions of traces per day).
//   - Hex lowercase no-separator format is parser-friendly across all
//     stacks (Go / Java / cpp all have one-liners for both directions).

namespace tracing {

// 16 raw bytes = 32 lowercase hex chars on the wire. Fits the W3C
// trace-id field as `[0-9a-f]{32}`.
struct TraceId {
    std::array<uint8_t, 16> bytes{};

    bool IsValid() const
    {
        for (auto b : bytes) {
            if (b != 0) return true;
        }
        return false;
    }

    std::string ToHex() const
    {
        char buf[33];
        for (size_t i = 0; i < 16; ++i) {
            std::snprintf(&buf[i * 2], 3, "%02x", bytes[i]);
        }
        buf[32] = '\0';
        return std::string(buf, 32);
    }
};

// 8 raw bytes = 16 lowercase hex chars. Fits the W3C parent-id field as
// `[0-9a-f]{16}`.
struct SpanId {
    std::array<uint8_t, 8> bytes{};

    bool IsValid() const
    {
        for (auto b : bytes) {
            if (b != 0) return true;
        }
        return false;
    }

    std::string ToHex() const
    {
        char buf[17];
        for (size_t i = 0; i < 8; ++i) {
            std::snprintf(&buf[i * 2], 3, "%02x", bytes[i]);
        }
        buf[16] = '\0';
        return std::string(buf, 16);
    }
};

// Generator — uses a thread-local PRNG seeded once per thread from the
// system clock + thread id. We don't use crypto/rand here (the trace-id
// only needs to be globally unique, not unpredictable to an attacker;
// see W3C trace-context security considerations). Cheap to call on
// every incoming RPC.
inline TraceId NewTraceId()
{
    thread_local std::mt19937_64 rng(
        static_cast<uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()
        ) ^ reinterpret_cast<uintptr_t>(&rng));

    TraceId id;
    const uint64_t hi = rng();
    const uint64_t lo = rng();
    std::memcpy(&id.bytes[0], &hi, 8);
    std::memcpy(&id.bytes[8], &lo, 8);
    return id;
}

inline SpanId NewSpanId()
{
    thread_local std::mt19937_64 rng(
        static_cast<uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()
        ) ^ reinterpret_cast<uintptr_t>(&rng) ^ 0x9e3779b97f4a7c15ULL);

    SpanId id;
    const uint64_t v = rng();
    std::memcpy(&id.bytes[0], &v, 8);
    return id;
}

// W3C traceparent header: `00-{trace_id}-{span_id}-{flags}`
//   - version `00` fixed (W3C says future versions are unforced extension)
//   - flags `01` = sampled, `00` = not sampled. Default to `01` here so
//     the rollout actually produces traces; sampling policy belongs in
//     slice B's collector config, not at the recorder.
struct TraceContext {
    TraceId trace_id;
    SpanId span_id;
    uint8_t flags{0x01};

    std::string ToTraceparent() const
    {
        return "00-" + trace_id.ToHex() + "-" + span_id.ToHex() +
               (flags & 0x01 ? "-01" : "-00");
    }

    bool IsValid() const { return trace_id.IsValid() && span_id.IsValid(); }
};

// Hex char → 0..15. -1 on invalid.
inline int HexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline bool ParseHexBytes(std::string_view src, uint8_t* dst, size_t count)
{
    if (src.size() != count * 2) return false;
    for (size_t i = 0; i < count; ++i) {
        const int hi = HexDigit(src[i * 2]);
        const int lo = HexDigit(src[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        dst[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

// Parse a `traceparent` header into TraceContext. Returns true on
// success; on failure, ctx may be partially mutated — callers should
// either treat failure as "start a fresh trace" or reject the request.
inline bool ParseTraceparent(std::string_view header, TraceContext& ctx)
{
    // Expected: 00-{32 hex}-{16 hex}-{2 hex}  → exactly 55 chars
    if (header.size() != 55) return false;
    if (header.substr(0, 2) != "00") return false;
    if (header[2] != '-' || header[35] != '-' || header[52] != '-') return false;

    if (!ParseHexBytes(header.substr(3, 32), ctx.trace_id.bytes.data(), 16)) return false;
    if (!ParseHexBytes(header.substr(36, 16), ctx.span_id.bytes.data(), 8)) return false;
    const int flagHi = HexDigit(header[53]);
    const int flagLo = HexDigit(header[54]);
    if (flagHi < 0 || flagLo < 0) return false;
    ctx.flags = static_cast<uint8_t>((flagHi << 4) | flagLo);

    return ctx.IsValid();
}

// Convenience: fresh root context. Use at every "we received a request
// with no traceparent and we are the trace origin" site.
inline TraceContext NewRoot()
{
    TraceContext c;
    c.trace_id = NewTraceId();
    c.span_id = NewSpanId();
    c.flags = 0x01;
    return c;
}

// Convenience: child span under the same trace, fresh span_id.
inline TraceContext ChildSpan(const TraceContext& parent)
{
    TraceContext c;
    c.trace_id = parent.trace_id;
    c.span_id = NewSpanId();
    c.flags = parent.flags;
    return c;
}

} // namespace tracing
