#include "thread_context/trace_context_tls.h"

namespace tracing {

// Single definition of the TLS slot. Each thread gets its own
// default-constructed TraceContext (both IDs all-zero, IsValid() = false)
// until the RPC dispatch layer installs a real one via Set() or
// tlsTrace = NewRoot() (followed by RefreshLogPrefix()).
thread_local TraceContext tlsTrace;

// Cached log prefix — refreshed by RefreshLogPrefix() (called from
// Set / Clear / ScopedSpan). Empty when tlsTrace is invalid. Read by
// TLOG_* macros on every log line; the cache exists so the hot path
// avoids a substr + ToHex per call (Review O2 fix, 2026-05-17).
thread_local std::string tlsTraceLogPrefix;

} // namespace tracing
