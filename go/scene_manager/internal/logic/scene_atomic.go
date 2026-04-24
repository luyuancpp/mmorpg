package logic

import (
	"fmt"
	"strconv"

	"scene_manager/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
)

// This file holds the server-side Lua scripts that make scene-manager's
// two critical paths — "player enters a scene" and "lifecycle destroys an
// idle scene" — race-safe against each other.
//
// Problem without CAS:
//   goroutine A (lifecycle tick):
//     1. GET instance:{id}:player_count == "0"
//     2. (context switch)
//     3. DEL scene:{id}:node, ZREM ..., DEL instance:{id}:player_count
//   goroutine B (EnterScene RPC, between A.1 and A.3):
//     1. GET scene:{id}:node -> "node-7" (scene still alive)
//     2. UpdatePlayerLocation, route to gate
//     3. INCR instance:{id}:player_count -> new key "1"
//   Result: player routed into a dead scene; an orphan player_count key
//   lives forever; node-scene-count drifts.
//
// Fix: both paths touch the same keys inside a single Lua script so Redis
// serializes them.

// luaAtomicIncrPlayerCount atomically:
//   1. checks scene:{id}:node still exists,
//   2. if yes, INCRs instance:{id}:player_count and returns the new count,
//   3. if no, returns -1 without creating the player_count key.
//
// KEYS[1] = scene:{id}:node
// KEYS[2] = instance:{id}:player_count
// Return: (int) new count, or -1 if scene is gone.
const luaAtomicIncrPlayerCount = `
if redis.call('EXISTS', KEYS[1]) == 0 then
    return -1
end
return redis.call('INCR', KEYS[2])
`

// luaAtomicDestroyInstance atomically:
//   1. checks instance:{id}:player_count is missing or equals "0",
//   2. if yes, reads scene:{id}:node into a local, then wipes every
//      scene-scoped Redis key and removes the scene from the active set,
//      and returns the nodeId (so the caller can fire DestroyScene RPC and
//      decrement node counters outside the script),
//   3. if no (player count > 0), returns empty string without touching
//      anything — destroy was a false alarm, try again next tick.
//
// KEYS[1] = scene:{id}:node
// KEYS[2] = instance:{id}:player_count
// KEYS[3] = instances:zone:{zone}:active (sorted set)
// KEYS[4] = scene:{id}:mirror
// KEYS[5] = scene:{id}:source
// KEYS[6] = scene:{id}:zone
// ARGV[1] = sceneId as string (for ZREM)
// Return: (string) nodeId on destroy, "" on abort (still has players).
//
// Note: we do NOT touch node:{nodeId}:scenes inside the script because we
// don't know the node id until after we read it (and the key name is
// dynamic). The caller does that SREM outside, which is fine: the
// reconciliation loop double-checks scene:{id}:node before destroying so
// a stale node-scenes set entry is harmless.
const luaAtomicDestroyInstance = `
local pc = redis.call('GET', KEYS[2])
if pc and pc ~= '0' and pc ~= '' then
    return ''
end
local nodeId = redis.call('GET', KEYS[1])
redis.call('DEL', KEYS[1], KEYS[2], KEYS[4], KEYS[5], KEYS[6])
redis.call('ZREM', KEYS[3], ARGV[1])
if nodeId then
    return nodeId
end
return ''
`

// AtomicIncrPlayerCountIfSceneExists is the race-safe replacement for
// IncrInstancePlayerCount used inside EnterScene. It guarantees the
// player_count key is only ever created/incremented when scene:{id}:node
// still exists, eliminating orphan keys when the scene gets destroyed in
// the middle of EnterScene.
//
// Returns the new count, or -1 if the scene was destroyed between
// resolveScene and this call. Callers should treat -1 as "retry or
// fail the request".
func AtomicIncrPlayerCountIfSceneExists(svcCtx *svc.ServiceContext, sceneId uint64) (int64, error) {
	keys := []string{
		sceneNodeKey(sceneId),
		fmt.Sprintf(InstancePlayerCountKey, sceneId),
	}
	raw, err := svcCtx.Redis.Eval(luaAtomicIncrPlayerCount, keys)
	if err != nil {
		return 0, err
	}
	return toInt64(raw), nil
}

// AtomicDestroyIfIdle is the race-safe replacement for the multi-step
// Redis cleanup in destroyInstance. If the scene still has players
// (player_count > 0), returns "" without touching anything. Otherwise
// wipes the scene's Redis state and returns the nodeId the scene was on
// so the caller can notify C++ and fix counters.
//
// Returns ("", nil) when the destroy was aborted because the scene picked
// up a player in the meantime; callers must skip further work in that
// case (no RPC, no counter decrement).
func AtomicDestroyIfIdle(svcCtx *svc.ServiceContext, zoneId uint32, sceneId uint64) (string, error) {
	keys := []string{
		sceneNodeKey(sceneId),
		fmt.Sprintf(InstancePlayerCountKey, sceneId),
		activeInstancesKey(zoneId),
		sceneMirrorFlagKey(sceneId),
		sceneSourceKey(sceneId),
		sceneZoneKey(sceneId),
	}
	raw, err := svcCtx.Redis.Eval(luaAtomicDestroyInstance, keys, fmt.Sprintf("%d", sceneId))
	if err != nil {
		return "", err
	}
	return toString(raw), nil
}

// toInt64 converts the untyped return of Redis.Eval to int64.
// go-zero's redis client returns int64 for RESP integer replies but some
// transports surface it as string or []byte. Be defensive.
func toInt64(v any) int64 {
	switch x := v.(type) {
	case int64:
		return x
	case int:
		return int64(x)
	case string:
		n, _ := strconv.ParseInt(x, 10, 64)
		return n
	case []byte:
		n, _ := strconv.ParseInt(string(x), 10, 64)
		return n
	}
	logx.Errorf("[Lua] unexpected int reply type %T: %v", v, v)
	return 0
}

// toString converts the untyped return of Redis.Eval to string. Same
// defensiveness as toInt64 — the return shape is backend-dependent.
func toString(v any) string {
	switch x := v.(type) {
	case string:
		return x
	case []byte:
		return string(x)
	case nil:
		return ""
	}
	return fmt.Sprint(v)
}
