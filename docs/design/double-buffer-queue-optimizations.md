# DoubleBufferQueue Optimizations (2026-04-16)

File: `cpp/libs/engine/core/utils/data_structures/double_buffer_queue.h`

## Applied optimizations (all done, no further low-hanging fruit)

1. **Removed `pendingSize_` atomic** — `size()`/`empty()` were only used in tests, not production. Removing the `std::atomic<size_t>` eliminated `fetch_add` on every `put()` and `fetch_sub` on every `swapReadBuffer()`. Producer hot path now: mutex + push_back only.

2. **Cache line padding (false sharing prevention)** — Producer fields (`mutex_`, `buffers_`, `writeIndex_`) and consumer fields (`readIndex_`, `readPos_`, `readEnd_`) are now on separate cache lines via `alignas(std::hardware_destructive_interference_size)`. Consumer `take()` steady-state path no longer suffers cache invalidation from producer writes.

3. **`emplace()` support** — Added variadic template `emplace(Args&&...)` for in-place construction, avoiding temporary object + move overhead.

## Remaining bottleneck (not worth changing now)
- `put()` uses mutex. Acceptable for game server pattern (few IO threads → one main loop). Only worth replacing with lock-free MPSC if profiling shows real contention, which adds significant complexity.
