# Login Test Anti-Stuck System

## Problem
Players can get stuck during login if the server fails to respond to any step in the
Login → CreatePlayer → EnterGame handshake. This is a critical P0 issue because a
stuck player cannot play the game at all.

## Solution: Comprehensive Login Test Suite + ML Data Collection

### Test Scenarios (17 total, `robot/login_test_scenarios.go`)
| # | Scenario | What it tests |
|---|----------|---------------|
| 1 | NormalLogin | Baseline happy path |
| 2 | LoginLogoutCycle | Repeated login/leave/reconnect (3 cycles) |
| 3 | WrongPassword | Server returns error, does not crash |
| 4 | DuplicateEnterGame | Same connection sends EnterGame twice |
| 5 | DuplicateLoginRequest | Two Login requests on same connection |
| 6 | AccountDisplacement | B logs in with A's account → A gets kicked |
| 7 | ConcurrentSameAccount | Two goroutines race login on same account |
| 8 | RapidReconnect | Close abruptly then reconnect immediately |
| 9 | DisconnectDuringLogin | Send Login, close before reading response |
| 10 | DisconnectDuringEnter | Login succeeds, close before EnterGame |
| 11 | DifferentAccountSequential | Two different accounts in sequence |
| 12 | LeaveAndReEnter | Leave game, re-enter on same connection |
| 13 | LeaveAndReLogin | Leave game, reconnect, full re-login |
| 14 | RapidLoginSpam | N rapid connect-login-close cycles, then clean login |
| 15 | MessageBeforeLogin | Send game message before login (invalid state) |
| 16 | LoginStuckDetection | Timed full flow, fails if >10s or STUCK |
| 17 | BatchConcurrentLogin | N different accounts login simultaneously (uses robot_count) |

### Anti-Stuck Mechanism (`robot/login.go`)
- `sendAndRecv()` now has a **15-second timeout** (via `sendAndRecvTimeout`)
- If no response within timeout, returns `STUCK:` error, increments `login_stuck` counter
- Stats report shows `login_stuck` count alongside `login_ok` / `login_fail`

### ML Data Collection (`robot/metrics/stats.go`)
- `LoginRecord` struct captures per-attempt features:
  - `timestamp` — when the attempt occurred
  - `account` — which account
  - `scenario` — test scenario name or "stress" / "BatchConcurrentLogin"
  - `latency_ms` — end-to-end login latency
  - `success` — boolean
  - `stuck` — true if timed out waiting for response
  - `error` — error message if failed
  - `attempt` — retry number (1-based)
  - `concurrent` — concurrent connections at time of attempt
- `Stats.RecordLogin()` — goroutine-safe append
- `Stats.ExportLoginCSV(path)` — exports to CSV after test suite completes

### CSV Output Format
```
timestamp,account,scenario,latency_ms,success,stuck,error,attempt,concurrent
2026-04-11T12:00:00Z,robot_0001,NormalLogin,523,true,false,,0,0
2026-04-11T12:00:01Z,robot_0100,BatchConcurrentLogin,1204,true,false,,0,10
2026-04-11T12:00:02Z,robot_0105,BatchConcurrentLogin,15001,false,true,STUCK: no response...,0,10
```

### ML Analysis Workflow
1. Run login-test suite repeatedly under different loads/configs
2. Collect `login_test_results.csv` files
3. Train anomaly detection model on latency distribution:
   - Features: `latency_ms`, `concurrent`, `scenario`, `time_of_day`
   - Labels: `stuck` (binary), `success` (binary)
4. Set alerting threshold: if predicted stuck probability > 0.1, trigger investigation
5. Use the model to detect regressions in CI: run login-test before deploy, fail if stuck > 0

### Usage
```yaml
# robot/etc/robot.yaml
mode: "login-test"
robot_count: 10  # used by BatchConcurrentLogin scenario
```
```bash
cd robot && go run . -c etc/robot.yaml
```

### Key Invariant
**After any sequence of abuse (spam, disconnect, duplicate), a clean login attempt
on the same account MUST succeed within 15 seconds.** If it doesn't, the test suite
marks it as STUCK and FAIL.

### Test Categories
- **Basic flow**: NormalLogin, LoginLogoutCycle, WrongPassword
- **Duplicate/idempotent**: DuplicateEnterGame, DuplicateLoginRequest
- **Displacement/conflict**: AccountDisplacement, ConcurrentSameAccount
- **Reconnect/disconnect**: RapidReconnect, DisconnectDuringLogin, DisconnectDuringEnter
- **Session lifecycle**: LeaveAndReEnter, LeaveAndReLogin
- **Spam/abuse**: RapidLoginSpam, MessageBeforeLogin
- **Stuck detection**: LoginStuckDetection, BatchConcurrentLogin
