# Distributed Tracing — Design (todo.md #152)

> **Status:** 2026-05-14 — W3C trace-context primitive landed
> (`trace_context.h`); wire propagation, span lifecycle, and backend
> selection are not done. This doc explains the staging.

---

## Problem

When prod blows up, "which RPC chain triggered this?" is the first
question and the hardest one without trace IDs. A single client login
touches Java Gateway → go-zero login → Redis/MySQL/etcd → cpp gate →
cpp scene → Kafka → cpp scene again. Each hop logs independently;
correlating them by timestamp + player_id is doable but slow, and
falls apart entirely when multiple players' requests interleave on a
shared thread.

todo #152 calls out the gap. The fix is a propagated trace_id so each
log line, each span, each error event can be filtered to "this one
request, end to end."

---

## Decision

### W3C Trace Context shape, not a project-local format

128-bit `trace_id` + 64-bit `span_id` formatted as lowercase hex, no
separators, propagated via the standard `traceparent` header.
Rationale:

- **Direct OpenTelemetry compatibility** — any OTel collector / Jaeger
  agent / Tempo distributor / Honeycomb ingester accepts this without
  translation. Slice B picks a backend; the wire format is decided
  here in slice A.
- **128-bit space** avoids collision at our planned scale. 64-bit
  trace IDs (Jaeger v1's default before OTel) start colliding at
  ~few billion traces, which is ~1 week of busy traffic for an open
  MMO server. 128-bit is forever.
- **Lowercase hex no-separator** is parser-friendly across Go, Java,
  C++, JavaScript with one-liners in each.

### Engine ships only the primitive, not the SDK

`trace_context.h` provides:
- `TraceId` / `SpanId` types with hex serialization
- `NewTraceId() / NewSpanId()` — thread-local PRNG, no syscalls on
  the hot path
- `TraceContext::ToTraceparent()` / `ParseTraceparent()` — round-trip
  for the W3C header
- `NewRoot()` / `ChildSpan(parent)` — convenience constructors

It does **not** provide:
- Span lifecycle (start/end, tagged timing). OTel SDK or a thin
  in-house collector does this — slice B.
- Sampling policy. The `flags` byte holds the sampled bit per W3C
  but the choice belongs in slice B's collector config.
- Wire propagation through gRPC metadata / Kafka headers — slices
  C and D.

This split matters because **the SDK is the heavy decision**
(performance, exporter compatibility, multi-language SDK alignment).
Locking in the trace-ID shape now keeps slice B from being
controversial later — "we already produce W3C-compatible IDs; pick
whatever exporter eats them."

### Sampling defaults to "always sample"

The `flags` byte defaults to `01` (sampled) so the rollout actually
produces traces from day one. Operators dial it down by configuring
the collector / exporter to drop traces post-receipt; we don't
pre-decide a sampling rate in code.

### Why not crypto/rand for the IDs?

W3C trace-context security considerations explicitly say trace IDs
do NOT need to be unpredictable. They need to be globally unique with
high probability — `mt19937_64` thread-seeded with
high-resolution-clock + thread address gives ~80 bits of effective
entropy per call, well above the birthday-bound for 128-bit IDs.
Crypto-quality RNG would add 1µs of syscall cost per RPC for no
real benefit.

---

## What landed in this commit (2026-05-14)

| File | Change |
|---|---|
| `cpp/libs/engine/core/tracing/trace_context.h` (new) | W3C-shaped `TraceId` (16 bytes) / `SpanId` (8 bytes) / `TraceContext`. Generators (`NewTraceId`, `NewSpanId`, `NewRoot`, `ChildSpan`). Parser / serializer for `traceparent` HTTP header. Header-only, no protobuf, no logging dep. |
| `docs/design/distributed-tracing.md` (this file) | Decision record + slice plan. |

Nothing is wired to this primitive yet. Slice A wires the existing
RPC boundary code; slice B picks a backend; slices C–E handle the
remaining stacks.

---

## What's NOT done (slices to track)

### A. Server-side trace_id allocation + log correlation — **S, 1 day**

Every gate / scene / login handler entry point gets:

```cpp
// At RPC entry — if traceparent header present, parse; else allocate root.
tracing::TraceContext ctx;
if (auto header = TryGetTraceparent(request); !header.empty()) {
    tracing::ParseTraceparent(header, ctx);
} else {
    ctx = tracing::NewRoot();
}

// Stash in TLS so LOG_* macros pick it up
tlsTraceContext.Set(ctx);
```

Plus a one-line patch to `muduo::Logging` to include `trace_id=...`
in every log entry while a TLS context is active. After slice A,
on-call can grep a single trace_id across all nodes' logs.

### B. Pick a backend + collector — **M, 1–2 days; ops decision**

Three reasonable choices, ordered by what I'd recommend:

1. **Self-hosted Tempo + Grafana** (recommended). Tempo is
   Grafana's trace storage — pairs with the Prometheus + Grafana
   stack the project already runs (`deploy/k8s/scene-manager-alerts.yaml`,
   `docs/ops/grafana-login-path-deprecation.json`). Single sign-on
   for metrics + traces; on-call uses one UI.
2. **Self-hosted Jaeger**. Standalone, well-known, slightly more
   feature-rich (service maps, deep query language). Separate UI
   from Grafana — extra context switch on triage.
3. **SaaS (Honeycomb / Datadog APM)**. Best ergonomics, ongoing
   spend, vendor lock-in. Skip unless ops specifically wants it.

Backend selection is independent of the primitive. The same
`trace_context.h` ships traces to any of the three via the OTel
Collector (which speaks W3C natively).

### C. gRPC interceptor — **M, 1 day**

Wire trace propagation into:
- C++ gate → go-zero login (outgoing gRPC: write `traceparent` to
  metadata; receive: parse before handler runs)
- C++ scene → go-zero scene_manager (same)
- Java gateway → go-zero login (Java OTel auto-instrumentation
  handles this if slice B picks an OTel-compatible backend)

Spec-compliant grpc context propagation is well-trodden ground —
both grpc-go and grpc-cpp ship interceptor scaffolding that's
maybe 50 lines of glue total.

### D. Kafka header propagation — **S, ~half day**

Kafka records can carry headers (since Kafka 0.11). Producer side:
serialize the current TraceContext into a `traceparent` header on
every emitted record. Consumer side: extract and install before
calling the handler. The `KeyOrderedKafkaProducer` (see
`go/db/internal/kafka/key_ordered_consumer.go` for the matching
consumer) is the natural choke point.

### E. Java + Go alignment — **S, ~half day each**

The cpp primitive has parallel files needed in:
- `go/shared/tracing/` (Go) — same `traceparent` format. Use the
  official `go.opentelemetry.io/otel/trace` types and a tiny
  shim if needed; the OTel Go SDK already produces W3C-compatible
  IDs.
- `java/shared/tracing/` (Java) — the OTel Java agent
  (`opentelemetry-javaagent.jar`) auto-instruments Spring Boot
  endpoints. If we attach the agent at JVM start, the gateway
  controller layer needs zero code changes.

The cpp side stays manual because we don't run any OTel agent in
the C++ build. That's fine for a first slice — slice A's TLS
log correlation gives most of the value.

### F. Span lifecycle (timing) — **L, 1 week**

Only after slices A–E land: wire `Span::Start()` / `Span::End()`
around handler boundaries, RPC calls, Redis commands, Kafka sends.
Each span exports `span_id, parent_span_id, name, start_ns,
end_ns, status`. This is where OTel SDK earns its keep — don't
try to reinvent it.

Cost note: a full traced RPC adds ~5–10µs per span on cpp. For a
login chain with ~12 spans that's ~120µs added latency at p50.
Acceptable; revisit if profiling shows otherwise.

---

## Open questions

1. **Where does TLS context live?** Likely
   `cpp/libs/engine/core/thread_context/` next to `ecs_context.h` —
   same lifetime, same threading discipline. Slice A picks the
   exact file.

2. **Trace ID in the proto error envelope?** Today `TipInfoMessage`
   doesn't carry trace_id. If the client is at any point shown an
   error, including the trace_id makes player support easier ("the
   player gave us this code, we found the failed request"). Two
   options:
   - Add `string trace_id` to TipInfoMessage. Pro: client and
     player see it. Con: proto schema change visible to client.
   - Log trace_id alongside the error tip on the server side only.
     Pro: zero client impact. Con: support has to ask for
     timestamp + player_id and join logs.

   Pick when slice A lands.

3. **High-cardinality sampling.** A single login burst that times
   out can produce ~50 traces from one player retrying. Cardinality
   explosion on the collector. Sampling should default to "always
   sample errors, sample 10% of successes" in the slice B collector
   config — but this is a collector-side knob, not a code change.

---

## References

- `cpp/libs/engine/core/tracing/trace_context.h` — the primitive
  (this slice)
- W3C Trace Context spec — https://www.w3.org/TR/trace-context/
- OpenTelemetry — https://opentelemetry.io
- todo.md #152, #27 (production fast localization — same goal,
  different angle), #250 (error aggregation — pairs with traces
  for end-to-end triage)
