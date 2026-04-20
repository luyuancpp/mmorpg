# Async Load Race Conditions: Disconnect & Reconnect During Redis Load

## Overview

When a player enters a Scene node, their data is loaded asynchronously from Redis
(`MessageAsyncClient::AsyncLoad`). During this async window (typically 1-20ms),
two race conditions can occur:

1. **Disconnect during load** -- ExitGame arrives before load completes
2. **Disconnect + immediate reconnect** -- a second PlayerEnterGameNode arrives
   while the first load is still in flight

Both must be handled to prevent ghost entities, stuck players, and Gate WARN logs.

## Race Condition 1: Disconnect During Async Load

### Timeline

```
t0  PlayerEnterGameNode  -->  SessionMap[session_A] = player_id
                              AsyncLoad(player_id, enterInfo_A)
t1  ExitGame arrives     -->  player entity == null (not loaded yet)
                              *** BUG: ExitGame silently ignored ***
t2  Redis load completes -->  HandlePlayerAsyncLoaded
                              InitPlayerFromAllData -> creates entity
                              sends EnterSceneS2C to session_A
                              *** BUG: session_A already deleted on Gate ***
```

### Fix

- **t1**: When ExitGame arrives and player entity is null, erase `SessionMap[session_A]`
  as a cancellation signal.
- **t2**: `HandlePlayerAsyncLoaded` checks `SessionMap.find(session_id)` before
  `InitPlayerFromAllData`. If not found, skip entity creation.

### Code

```
// scene_handler.cpp - ProcessClientPlayerMessage
if (msgId == ExitGame && playerEntity == null) {
    SessionMap().erase(session_id);  // cancellation signal
    return;
}

// player_lifecycle.cpp - HandlePlayerAsyncLoaded
if (SessionMap().find(session_id) == SessionMap().end()) {
    LOG_INFO << "async load cancelled for session " << session_id;
    return;  // don't create entity
}
```

## Race Condition 2: Reconnect During In-Flight Load

### Timeline

```
t0  PlayerEnterGameNode(session_A)  -->  SessionMap[session_A] = player_id
                                         AsyncLoad(player_id, enterInfo_A)
t1  ExitGame(session_A)            -->  SessionMap.erase(session_A)  [Fix 1]
t2  PlayerEnterGameNode(session_B)  -->  SessionMap[session_B] = player_id
                                         AsyncLoad(player_id, enterInfo_B)
                                         *** DEDUP: silently dropped ***
t3  Redis load completes            -->  HandlePlayerAsyncLoaded with enterInfo_A
                                         session_A not in SessionMap -> skip
                                         *** BUG: session_B never gets loaded ***
```

### Root Cause

`MessageAsyncClient::AsyncLoad` deduplicates by key (player_id). If a load is
already in flight for the same key, the second call is silently dropped. The
callback will use the original `extra_data` (enterInfo_A with session_A).

### Fix

Before calling `AsyncLoad`, try `UpdateExtraData(player_id, enterInfo_B)`:
- If returns `true`: load already in flight, extra_data updated to enterInfo_B.
  When load completes, `HandlePlayerAsyncLoaded` will use enterInfo_B (session_B).
- If returns `false`: no in-flight load, proceed with normal `AsyncLoad`.

### Corrected Timeline

```
t0  PlayerEnterGameNode(session_A)  -->  SessionMap[session_A] = player_id
                                         AsyncLoad(player_id, enterInfo_A)
t1  ExitGame(session_A)            -->  SessionMap.erase(session_A)
t2  PlayerEnterGameNode(session_B)  -->  SessionMap[session_B] = player_id
                                         UpdateExtraData(player_id, enterInfo_B) -> true
                                         return (don't call AsyncLoad again)
t3  Redis load completes            -->  HandlePlayerAsyncLoaded with enterInfo_B
                                         session_B found in SessionMap -> OK
                                         InitPlayerFromAllData with session_B
```

## SessionMap as Cancellation Token

The `SessionMap` (session_id -> player_id) serves dual purpose:

1. **Routing**: Maps incoming client messages to player entities during async load
2. **Cancellation**: If erased before load completes, signals that the load result
   should be discarded

Pre-registration at `PlayerEnterGameNode` time ensures that:
- Client messages during load hit "player not loaded" guard (not "session not found")
- The session_id is available for cancellation check in `HandlePlayerAsyncLoaded`

## Robot Test

`testRapidDisconnectReconnect` in `robot/login_test_scenarios.go`:

1. Setup: full login + clean leave (ensures player exists in Redis)
2. 3 cycles of:
   - Login + EnterGame (triggers async load)
   - Abrupt close (no LeaveGame) -- triggers ExitGame on server
   - Immediate reconnect (zero delay) -- may hit async load window
   - Login + EnterGame -- must succeed
   - Clean leave
3. All 3 cycles must complete without STUCK timeout

The zero-delay reconnect maximizes the probability of hitting the async load
window, testing both race conditions.

## Files Changed

| File | Change |
|------|--------|
| `cpp/nodes/scene/handler/rpc/scene_handler.cpp` | ExitGame: erase SessionMap when entity null; PlayerEnterGameNode: UpdateExtraData before AsyncLoad |
| `cpp/libs/services/scene/player/system/player_lifecycle.cpp` | HandlePlayerAsyncLoaded: SessionMap check before InitPlayerFromAllData |
| `robot/login_test_scenarios.go` | New `testRapidDisconnectReconnect` scenario |
