# Redis Async Client Retry Fix

Updated: 2026-04-21

## Scope

This document records the retry and reconnect behavior in [redis_client.h](../../cpp/libs/engine/infra/storage/redis_client/redis_client.h) and the scene-side wiring in [redis.cpp](../../cpp/libs/services/scene/core/system/redis.cpp).

## Problems Found

1. Periodic retry logic and reconnect retry logic were sharing the same entry point.
   The old flow could clear `loading_queue_` on a timer, re-issue in-flight GETs, and allow duplicate `load_callback_` delivery for the same key.
2. NIL retries were too aggressive.
   A fixed 2 second interval with only 5 retries gave about 10 seconds of total retry budget, which was not enough for the Kafka -> db service -> Redis write-back path under load.
3. Save operations were dropped on disconnect.
   That made Redis cache recovery depend entirely on later writes from db service.
4. Save retries re-serialized protobuf payloads and used `EVAL` every time.
   That was safe, but not efficient.

## Fix Applied

- Split the retry path into two entry points.
  - `OnReconnected()` is only used by the reconnect callback. It can safely move in-flight loads into the retry queue and force-flush all pending work.
  - `RetryDuePending()` is used by the periodic timer. It only drains due entries from the pending queues and never touches `loading_queue_`.
- Added per-element exponential backoff.
  - Load retries use 2, 4, 8, 16, 32, 60 second intervals.
  - `kMaxLoadRetries` was increased to 6.
  - Each element tracks `next_retry_at` with `steady_clock`.
- Added a save retry queue.
  - `Save()` now serializes once into `Element::serialized_payload` and enqueues on disconnect.
  - Save retries use the same backoff ladder as loads, with `kMaxSaveRetries = 6`.
  - Latest value per key wins because the pending queue is keyed by Redis key.
- Added Lua script caching.
  - The save path now prefers `SCRIPT LOAD` + `EVALSHA`.
  - If Redis replies with `NOSCRIPT`, the code clears the cached sha, reloads the script, and falls back to `EVAL` without consuming a retry slot.
- Reduced logging noise.
  - `AsyncLoad()` debug state is logged at `LOG_DEBUG` instead of `LOG_INFO`.
  - Save disconnects now log at `LOG_ERROR`.

## Cold-Start Reconnect Fix

The original bootstrap sequence in `node.cpp` constructed `Hiredis`, called `connect()`, and only then ran `SetupReconnect()` to register the disconnect callback. If Redis was not yet running when scene started, the very first `SYN` failure happened before any callback was registered and was silently dropped, so the reconnect timer never started.

The fix in [redis_manager.cpp](../../cpp/libs/engine/thread_context/redis_manager.cpp):

- New `RedisManager::Connect(loop, addr)` creates the Hiredis instance, registers connect and disconnect callbacks via the new private `InstallCallbacks()` helper, and only then calls `connect()`. Bootstrap in `node.cpp` switched to this single call.
- `SetupReconnect()` is kept as a thin backwards-compatible alias. If it is ever invoked while `connected()` is false, it now schedules a reconnect immediately so cold start still recovers.
- The connect-callback failure path now also calls `ScheduleReconnect()` (previously it only logged a warning, which left the system stuck).
- `ScheduleReconnect()` is now a no-op when a timer is already active, so duplicated connect-failure and disconnect events do not stack timers.
- A local `kRedisOk = 0` constant is used in place of the `REDIS_OK` macro to avoid relying on transitive includes from muduo's hiredis wrapper.

## Defensive Guards in Raw `tlsRedis.GetZoneRedis()->command(...)` Callers

Two best-effort lookups in [player_scene.cpp](../../cpp/libs/services/scene/player/system/player_scene.cpp) (leader location and team info on enter scene) called `tlsRedis.GetZoneRedis()->command(...)` without null or `connected()` checks. During `DoReconnect()` the unique_ptr is briefly reset, and any business call between the disconnect event and the new connection's `connected = true` would dereference a null pointer or fail silently. Both call sites now guard with `if (!zoneRedis || !zoneRedis->connected())` and return early with a `LOG_WARN`. The two etcd-side callers in `etcd_service.cpp` and `etcd_manager.cpp` already used the correct guard pattern.

## Eager-Serialize Caller Contract

`MessageAsyncClient::Save()` now serializes the protobuf payload into `Element::serialized_payload` at call time so the retry path does not have to re-serialize. The hidden contract: **callers must not mutate the message after passing it to `Save()`** — any post-Save mutation is invisible to the Redis blob.

`PlayerLifecycleSystem::SavePlayerToRedis` previously set `player_id` on the `player_database_data` and `player_database_1_data` sub-tables _after_ calling `Save()` (those fields were used by the Kafka DB-task path immediately following). After the eager-serialize switch, the Redis blob lost those sub-table primary keys. The fix moves `set_player_id` to before `Save()`. The Kafka path still sees the populated values because the mutation is in-place on the same shared message.

## Observability and Lifecycle Hardening

`MessageAsyncClient` now exposes `in_flight_load_count()`, `pending_load_count()`, `pending_save_count()`, and a one-shot `LogQueueSnapshot(tag)` helper. The snapshot logs nothing when all three queues are empty, so it is safe to call from a periodic timer. `RedisSystem::Initialize` runs it every 30 seconds — if the scene cannot drain pending loads or saves, the operator now sees rising counts in the log without enabling per-call DEBUG output.

`RedisManager` is now non-copyable / non-movable and has a destructor that cancels the reconnect timer when the EventLoop pointer is still set. This prevents a stray `DoReconnect()` from firing against a destroyed `this` if the thread-local `tlsRedis` is torn down while the loop is still alive.

## Operational Note

- Windows native Redis service: actual runtime target
- Docker Redis container: idle, empty database

This can mislead diagnosis because the container can look healthy while the scene node is actually talking to the Windows service. Only keep one Redis listener on 6379 during local debugging.

## Verification

All modified files compile cleanly (no errors reported by the language server after each change set).
