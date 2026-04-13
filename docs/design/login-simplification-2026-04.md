# Login Service Simplification (2026-04)

## Overview
Two rounds of simplification to make `go/login/` easier to understand, debug, and safer.

## Round 1: FSM → loginstep + Dead Code Removal

### Problem
- `looplab/fsm` library added unnecessary complexity: 3-step Redis roundtrip (load→transition→save), opaque error messages, retry-with-sleep hacks in EnterGame.
- Inline lock retry loop was 25 lines of boilerplate.
- Dead code cluttered the codebase (centre_client, taskmanager, async data loader).

### Changes
1. **Replaced FSM with `loginstep` package** (`go/login/internal/logic/pkg/loginstep/`):
   - Plain string comparison + map lookup replaces FSM library.
   - Valid transitions: `""→logged_in`, `logged_in→creating_char/entering_game`, `creating_char→logged_in/entering_game`.
   - Redis key: `login_step:{sessionId}` (replaces old `fsm_state:{sessionId}:` keys).
   - Migration-safe: old FSM keys treated as fresh sessions (no step = initial state).

2. **Extracted `TryLockWithRetry`** on `RedisLocker` (3 lines at call site vs 25 lines inline).

3. **Deleted dead code**:
   - `centre/` (centre_client.go — never imported)
   - `taskmanager/` (task_manager.go, worker_pool.go — only used by removed async loader)
   - `fsmstore/` (replaced by loginstep)
   - `data/` (player_login_state_machine.go — replaced by loginstep)
   - Async functions in `data_loader.go` (`Load`, `LoadWithPlayerId` — never called)

4. **Removed dependencies**: `looplab/fsm`, `panjf2000/ants`, `muduoclient`.

### Net result: ~1500 lines removed, ~75 lines added.

## Round 2: AccountLocker Safety Fix + More Dead Code

### Problem
- `AccountLocker` used `SetNX("1")` + `Del` for lock release — **unsafe**: if TTL expires and another process acquires the lock, `Del` deletes the wrong lock.
- Dead packages still present: `gatenotifier/`, `cache/`.
- Dead function: `GetPlayerDataKey()`.

### Changes
1. **AccountLocker → RedisLocker** (safety fix):
   - `RedisLocker` uses UUID value + Lua CAS script for safe release:
     ```lua
     if redis.call("get", KEYS[1]) == ARGV[1] then
         return redis.call("del", KEYS[1])
     end
     ```
   - Lock keys unchanged (`account_lock:login:{account}`, `account_lock:create:{account}`).
   - Applied in `loginlogic.go` and `createplayerlogic.go`.
   - Deleted `account_locker.go`.

2. **Deleted dead code**:
   - `gatenotifier/` (never imported; Gate commands already in `ServiceContext`).
   - `cache/` (single function `BuildRedisKey` inlined into `sync_loader.go`).
   - `GetPlayerDataKey()` from `constants/` (never called).

## Message Ordering & Concurrency Safety

### Two-layer guarantee for same-player serialization:

1. **`player_locker` distributed lock (mutual exclusion)**:
   - `EnterGame` Step 3: `TryLockWithRetry("player_locker:{playerId}", TTL, 12 retries, 500ms)`.
   - Same `PlayerId` at same time → only one `EnterGame` enters steps 4-8.
   - Second request waits or fails with error.

2. **Kafka `KeyOrderedKafkaProducer` (ordered delivery)**:
   - DB read requests sent with `key = playerIdStr`.
   - Consistent hash maps same key to same partition.
   - Single partition = FIFO order.
   - `Idempotent=true` + `MaxOpenRequests=1` prevents reordering.

### Replace Login (顶号) scenario:
```
Session A (playing) → Session B (EnterGame, same playerId)
  B: TryLockWithRetry("player_locker:42") → acquires lock
  B: LoadPlayerDataSync → loads data (sync, blocking)
  B: DecideEnterGame → sees existing session A → ReplaceLogin
  B: kickReplacedSession(A) → kicks A via Kafka
  B: persistEnterGameSession → writes new session
  B: Release("player_locker:42")

If Session C arrives simultaneously:
  C: TryLockWithRetry → waits for B to release
  C: B finishes → C acquires lock → processes serially
```

### Lock scope by operation:
| Operation | Lock Key | Scope |
|-----------|----------|-------|
| Login | `account_lock:login:{account}` | Account-level |
| CreatePlayer | `account_lock:create:{account}` | Account-level |
| EnterGame | `player_locker:{playerId}` | Player-level |

Account-level locks are correct for Login/CreatePlayer as they operate on `UserAccounts` data, not player data.

## All locks now unified under `RedisLocker` (UUID + Lua safe release pattern).

## File Inventory (post-simplification)
```
go/login/internal/logic/
├── clientplayerlogin/
│   ├── loginlogic.go          (RedisLocker, loginstep)
│   ├── createplayerlogic.go   (RedisLocker, loginstep)
│   ├── entergamelogic.go      (RedisLocker, loginstep, LoadPlayerDataSync)
│   ├── disconnectlogic.go     (unchanged)
│   ├── leavegamelogic.go      (unchanged)
│   └── session_lifecycle.go   (shared helpers)
├── pkg/
│   ├── loginstep/             (NEW: replaces FSM)
│   ├── locker/                (player_locker.go only; account_locker.go deleted)
│   ├── loginsessionstore/     (unchanged)
│   ├── sessionmanager/        (unchanged)
│   ├── dataloader/            (sync_loader.go + data_loader.go utilities only)
│   ├── consistent/            (Kafka partition hash)
│   ├── ctxkeys/               (unchanged)
│   ├── etcd/                  (unchanged)
│   └── node/                  (unchanged)
└── utils/sessioncleaner/      (uses loginstep.Delete)
```
