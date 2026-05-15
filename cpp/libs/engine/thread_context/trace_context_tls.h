#pragma once

#include <string>

#include "core/tracing/trace_context.h"

// Per-thread Trace Context slot (todo.md #152 slice A).
//
// Holds the active TraceContext for the current thread. Set at RPC entry
// from a parsed `traceparent` header (or by allocating a fresh root when
// there isn't one); consumed by:
//   - log lines via the TLOG_* macros below, so every log entry carries
//     a trace_id field on-format
//   - outgoing RPC stubs that need to write `traceparent` into the next
//     hop's metadata (slice C will pick this up)
//   - error_reporter::Record (slice A integration in #250) — could read
//     this to embed trace_id in every reported event for cross-event
//     correlation
//
// Lifetime: thread-local, lives for the duration of the thread. Reset at
// each RPC dispatch entry. A NOT-SET state is represented by a default-
// constructed TraceContext (both IDs all-zero, `IsValid()` returns false).

namespace tracing {

// The TLS slot. `extern thread_local` rather than `inline thread_local`
// keeps the symbol single-definition across translation units; matches
// the pattern of `tlsRpc` in `rpc_request_context.h`.
extern thread_local TraceContext tlsTrace;

// RAII scoped span: save the current context on entry, install a child
// span derived from it, restore the parent on exit. Use to bracket a
// section of work that should appear as a child span in traces.
//
// Usage:
//   {
//       tracing::ScopedSpan span;          // child of current tlsTrace
//       DoTheThing();                       // logs carry the child span_id
//   }                                       // tlsTrace restored
//
// Note: this doesn't emit a span record (timing, name, etc.) — that's
// slice F. For now it just gives child-span correlation in logs.
class ScopedSpan {
public:
    ScopedSpan()
        : prev_(tlsTrace)
    {
        if (prev_.IsValid()) {
            tlsTrace = ChildSpan(prev_);
        } else {
            tlsTrace = NewRoot();
        }
    }

    explicit ScopedSpan(const TraceContext& parent)
        : prev_(tlsTrace)
    {
        tlsTrace = ChildSpan(parent);
    }

    ~ScopedSpan()
    {
        tlsTrace = prev_;
    }

    ScopedSpan(const ScopedSpan&) = delete;
    ScopedSpan& operator=(const ScopedSpan&) = delete;
    ScopedSpan(ScopedSpan&&) = delete;
    ScopedSpan& operator=(ScopedSpan&&) = delete;

private:
    TraceContext prev_;
};

// Convenience: short hex prefix for log decoration. Returns the first 8
// hex chars of trace_id (32 bits of identifier — enough for human-eye
// correlation in a 10-minute log window). Empty string if no trace.
inline std::string LogPrefix()
{
    if (!tlsTrace.IsValid()) return std::string();
    const std::string hex = tlsTrace.trace_id.ToHex();
    return hex.substr(0, 8);
}

// Install or replace the current thread's TraceContext. Use at RPC
// dispatch entry — either from a parsed header (incoming traceparent)
// or by calling `tlsTrace = tracing::NewRoot();` for an origin trace.
inline void Set(const TraceContext& ctx) { tlsTrace = ctx; }

// Clear the current thread's TraceContext. Use when leaving an RPC
// handler if nothing further on this thread should belong to the
// trace (rare — the next handler will overwrite anyway).
inline void Clear() { tlsTrace = TraceContext{}; }

// Read the current thread's TraceContext (or default if unset).
inline const TraceContext& Current() { return tlsTrace; }

} // namespace tracing

// ─── Trace-aware logging macros ─────────────────────────────────────
//
// TLOG_* wraps LOG_* with a trace_id prefix when a trace is active.
// Falls back to plain LOG_* when no trace is set, so callers that don't
// participate in tracing pay zero cost beyond a thread_local read +
// a IsValid() branch.
//
// We deliberately do NOT modify muduo's Logging — that's framework code
// (CLAUDE.md §2 #1). Instead these macros prepend `[trace=xxxxxxxx]` to
// the message body. Grepping `trace=abc1234d` across all node logs
// correlates a single trace's spans across nodes.
//
// Why an 8-char prefix instead of the full 32: humans grep by eye-
// matchable prefixes; the full trace_id is preserved in the TLS slot
// for any structured-log path that wants it (slice B's exporter will).

#define TLOG_INFO  LOG_INFO  << (tracing::tlsTrace.IsValid() ? "[trace=" + tracing::LogPrefix() + "] " : std::string())
#define TLOG_WARN  LOG_WARN  << (tracing::tlsTrace.IsValid() ? "[trace=" + tracing::LogPrefix() + "] " : std::string())
#define TLOG_ERROR LOG_ERROR << (tracing::tlsTrace.IsValid() ? "[trace=" + tracing::LogPrefix() + "] " : std::string())
#define TLOG_DEBUG LOG_DEBUG << (tracing::tlsTrace.IsValid() ? "[trace=" + tracing::LogPrefix() + "] " : std::string())
