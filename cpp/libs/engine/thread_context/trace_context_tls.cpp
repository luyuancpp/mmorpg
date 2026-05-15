#include "thread_context/trace_context_tls.h"

namespace tracing {

// Single definition of the TLS slot. Each thread gets its own
// default-constructed TraceContext (both IDs all-zero, IsValid() = false)
// until the RPC dispatch layer installs a real one via Set() or
// tlsTrace = NewRoot().
thread_local TraceContext tlsTrace;

} // namespace tracing
