# Async Load Race Conditions: Disconnect & Reconnect During Redis Load

## Overview

When a player enters a Scene node, their data is loaded asynchronously from Redis
(`MessageAsyncClient::AsyncLoad`). During this async window (typically 1-20ms),
two race conditions can occur:

1. **Disconnect during load** -- ExitGame arrives before load completes
2. **Disconnect + immediate reconnect** -- a second PlayerEnterGameNode arrives
   while the first load is still in flight

Both must be handled to prevent ghost entities, stuck players, and Gate WARN logs.

## Architecture: PendingEnterMap

Session/scene context for in-flight loads is stored in a `thread_local PendingEnterMap`
(`unordered_map<Guid, PlayerGameNodeEntryInfoComp>`), owned by `PlayerLifecycleSystem`.

Previously this used `std::any extra_data` on `MessageAsyncClient`. The new design:
- **No business logic in the generic Redis client** -- `MessageAsyncClient` is a pure
  key->value async loader
- **Reconnect = overwrite** -- `PendingEnterMap[player_id] = newInfo` naturally handles
  the reconnect case without any `UpdateExtraData()` API
- **No type erasure** -- eliminates `std::any_cast` branches and the dead
  `PlayerSceneEnterContext` struct

Two external maps collaborate:
- `SessionMap` (`unordered_map<SessionId, Guid>`) -- routing + cancellation token
- `PendingEnterMap` (`unordered_map<Guid, PlayerGameNodeEntryInfoComp>`) -- load context

## Flow 1: Normal Async Load (no race)

```
t0  PlayerEnterGameNode(session_A, player_id)
    -> RemovePlayerSessionSilently(player_id)         // no-op (player not online)
    -> SessionMap[session_A] = player_id              // pre-register for routing
    -> PendingEnterMap[player_id] = enterInfo_A       // store context
    -> AsyncLoad(player_id)                           // fire Redis GET

t1  Redis responds
    -> HandlePlayerAsyncLoaded(player_id, data)
    -> PendingEnterMap.find(player_id) -> enterInfo_A // consume & erase from map
    -> SessionMap.find(session_A) -> found            // session still valid
    -> InitPlayerFromAllData(data, enterInfo_A)
      -> EnterScene -> HandleEnterScene
        -> bind player to scene, add to ScenePlayers, add to AOI grid
        -> SendMessageToClientViaGate(EnterSceneS2C)  // CLIENT GETS RESPONSE
```

## Flow 2: Disconnect During Async Load (cancel)

```
t0  PlayerEnterGameNode(session_A)
    -> SessionMap[session_A] = player_id
    -> PendingEnterMap[player_id] = enterInfo_A
    -> AsyncLoad(player_id)

t1  Client disconnects -> Gate sends ExitGame(session_A)
    -> ProcessClientPlayerMessage
    -> SessionMap.find(session_A) -> found, but player entity == null
    -> msg == ExitGame -> SessionMap.erase(session_A)  // CANCELLATION SIGNAL

t2  Redis responds
    -> HandlePlayerAsyncLoaded(player_id, data)
    -> PendingEnterMap.find(player_id) -> enterInfo_A  // found
    -> SessionMap.find(session_A) -> NOT FOUND         // cancelled!
    -> log + return                                    // no entity created
```

**Result**: No ghost entity. No message sent to deleted Gate session.

## Flow 3: Disconnect + Immediate Reconnect During Async Load

This is the critical race condition. The key insight is that `AsyncLoad` deduplicates
by key -- if a load is already in flight for the same `player_id`, the second call
is silently dropped. But `PendingEnterMap` overwrite ensures the callback uses the
latest session info.

```
t0  PlayerEnterGameNode(session_A, player_id)
    -> SessionMap[session_A] = player_id
    -> PendingEnterMap[player_id] = enterInfo_A
    -> AsyncLoad(player_id)                           // Redis GET starts

t1  Client disconnects -> ExitGame(session_A)
    -> SessionMap.erase(session_A)                    // cancel signal for session_A

t2  Client reconnects -> PlayerEnterGameNode(session_B, player_id)
    -> RemovePlayerSessionSilently(player_id)         // no-op (no entity yet)
    -> SessionMap[session_B] = player_id              // NEW session registered
    -> PendingEnterMap[player_id] = enterInfo_B       // OVERWRITES enterInfo_A
    -> AsyncLoad(player_id)                           // DEDUP: silently skipped
                                                      // (loading_queue_ already has key)

t3  Redis responds (from t0's GET -- data is the same regardless)
    -> HandlePlayerAsyncLoaded(player_id, data)
    -> PendingEnterMap.find(player_id) -> enterInfo_B // LATEST info (session_B!)
    -> SessionMap.find(session_B) -> FOUND            // valid!
    -> InitPlayerFromAllData(data, enterInfo_B)
      -> EnterScene(session_B) -> HandleEnterScene
        -> bind player to scene with session_B
        -> SendMessageToClientViaGate(EnterSceneS2C)  // CLIENT B GETS RESPONSE
```

**Result**: The reconnected client successfully enters the scene. No second Redis
GET is needed -- the single in-flight GET serves both requests. The `PendingEnterMap`
overwrite is the mechanism that "redirects" the load result to the new session.

## Why PendingEnterMap Overwrite Works

`AsyncLoad` deduplicates by `player_id` (translated to a Redis key). When the
second `PlayerEnterGameNode` arrives during an in-flight load:

1. `PendingEnterMap[player_id] = enterInfo_B` overwrites the old entry
2. `AsyncLoad(player_id)` finds the key in `loading_queue_` and returns immediately
3. When the Redis GET callback fires, it calls `HandlePlayerAsyncLoaded(player_id, data)`
4. The handler looks up `PendingEnterMap[player_id]` and finds `enterInfo_B`
5. `SessionMap.find(session_B)` succeeds, so the entity is created with session_B

The `PendingEnterMap` and `loading_queue_` are decoupled:
- `loading_queue_` gates Redis network calls (one GET per player)
- `PendingEnterMap` carries the latest session context (updated on each reconnect)

## Robot Test

`testRapidDisconnectReconnect` in `robot/login_test_scenarios.go`:

1. Setup: full login + clean leave (ensures player exists in Redis)
2. 3 cycles of:
   - Login + EnterGame (triggers async load on Scene)
   - Abrupt close (no LeaveGame) -- triggers ExitGame on server
   - Immediate reconnect (zero delay) -- may hit async load window
   - Login + EnterGame -- must succeed, client must receive EnterSceneS2C
   - Clean leave
3. All 3 cycles must complete without STUCK timeout

## Files Changed

| File | Change |
|------|--------|
| `redis_client.h` | Removed `std::any extra_data`, `UpdateExtraData()`, `EventCallbackWithExtra`, `load_callback_with_extra_`. `AsyncLoad` takes only `key`. |
| `player_lifecycle.h` | Added `PendingEnterMap` typedef + `GetPendingEnterMap()`. `HandlePlayerAsyncLoaded` takes `(Guid, PlayerAllData&)` only. |
| `player_lifecycle.cpp` | `HandlePlayerAsyncLoaded` reads from `PendingEnterMap`. Removed dead `PlayerSceneEnterContext`. |
| `redis.cpp` | `SetLoadCallback` instead of `SetLoadCallbackWithExtra`. |
| `scene_handler.cpp` | `PlayerEnterGameNode`: cleans stale session from PendingEnterMap before overwrite + `AsyncLoad(player_id)`. `ProcessClientPlayerMessage`, `SendMessageToPlayer`, `InvokePlayerService`: all three message dispatch paths have defensive session mismatch guard. |
| `login_test_scenarios.go` | New `testRapidDisconnectReconnect` scenario. |

## Flow 4: Late ExitGame After Reconnect Completes

Without the session mismatch guard, a late ExitGame from the old connection can
kill the reconnected player:

```
t0  PlayerEnterGameNode(session_A)
    -> SessionMap[A]=pid, PendingEnterMap[pid]=infoA, AsyncLoad(pid)

t2  PlayerEnterGameNode(session_B)  (reconnect, ExitGame not yet arrived)
    -> RemovePlayerSessionSilently(pid) -> no-op (player not in playerList)
    -> SessionMap[B]=pid              // now SessionMap has BOTH {A:pid, B:pid}
    -> PendingEnterMap: erase old session_A from SessionMap  <-- FIX (proactive)
    -> PendingEnterMap[pid]=infoB     // overwrite
    -> AsyncLoad(pid)                 // dedup

t3  Redis responds -> create entity with session_B

t1  Late ExitGame(session_A) arrives
    -> SessionMap.find(A) -> pid
    -> player entity exists
    -> snapshot.gate_session_id()==session_B != session_A  <-- FIX (defensive)
    -> stale session, erase SessionMap[A], drop message
    -> player survives
```

Two-layer defense:
1. **Proactive** (`PlayerEnterGameNode`): Before overwriting PendingEnterMap, check
   if old entry has a different session_id and erase it from SessionMap.
2. **Defensive** (all three message dispatch paths in `scene_handler.cpp`): After
   finding the player entity, verify `PlayerSessionSnapshotComp.gate_session_id()
   == sessionId`. If mismatch, it's from a superseded connection -- erase stale
   SessionMap entry and drop. Applied to:
   - `ProcessClientPlayerMessage` (Kafka-routed client messages)
   - `SendMessageToPlayer` (gRPC-routed gate relay)
   - `InvokePlayerService` (gRPC player service invocation)
