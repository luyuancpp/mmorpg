# Robot Login Test Scenarios

## Overview

Location: `robot/login_test_scenarios.go`
Activation: Set `mode: "login-test"` in `robot/etc/robot.yaml`, then run the robot binary.
Runs login, gameplay, and multi-robot scenarios sequentially, prints a PASS/FAIL summary, then exports CSV/JSONL traces for later analysis.

## Scenario Catalog

| # | Name | Description | Pass Criteria |
|---|------|-------------|---------------|
| 1 | **NormalLogin** | Baseline: login → create player (if needed) → enter game | Player ID returned |
| 2 | **LoginLogoutCycle** | Login → LeaveGame → close → reconnect → login, repeated 3 times | All cycles complete without error |
| 3 | **WrongPassword** | Send login with invalid password | Server returns error_message or rejects |
| 4 | **DuplicateEnterGame** | Enter game, then send EnterGame again on same connection | Server returns error or accepts idempotently |
| 5 | **AccountDisplacement (顶号)** | Robot A enters game; Robot B logs in with **same account** — A should be kicked | A receives KickPlayer msg or connection close within 5s |
| 6 | **RapidReconnect** | Login → abrupt TCP close (no LeaveGame) → immediate reconnect + login | Second session succeeds |
| 7 | **ConcurrentSameAccount** | Two goroutines race to login the same account simultaneously | At least one succeeds; no server crash |
| 8 | **DifferentAccountSequential** | Two different accounts login back-to-back on separate connections | Both succeed independently |
| 9 | **LeaveAndReEnter** | Enter game → LeaveGame → EnterGame again (no new Login call) | Server accepts or cleanly rejects |
| 10 | **DisconnectDuringLogin** | Send Login request then close connection before reading reply; reconnect | Server recovers; new full login succeeds |

## Key Proto Messages

| Message ID | Name | Used For |
|------------|------|----------|
| 48 | `ClientPlayerLoginLoginMessageId` | Account authentication |
| 14 | `ClientPlayerLoginCreatePlayerMessageId` | First-time player creation |
| 26 | `ClientPlayerLoginEnterGameMessageId` | Enter game world |
| 17 | `ClientPlayerLoginLeaveGameMessageId` | Graceful leave game |
| 34 | `SceneClientPlayerCommonKickPlayerMessageId` | Server-push kick notification |

## Configuration

```yaml
# In robot/etc/robot.yaml
mode: "login-test"       # Runs test suite then exits
# mode: "stress"         # Default: mass concurrent bots with AI actions
```

## Added Gameplay Scenarios

| Scenario | Description | Output |
|----------|-------------|--------|
| SkillCast | Login then explicitly release a real skill against a visible/self entity | PASS/FAIL + behavior trace |
| SceneSwitch | Request a same-scene/channel switch and verify a fresh enter-scene notification | PASS/FAIL + behavior trace |
| MultiRobotBehavior | Start multiple robots with mixed AI actions for behavior-pressure testing | CSV/JSONL trace set |

## Exports

After running, the robot now writes:

- login_test_results.csv
- behavior_test_results.csv
- behavior_test_results.jsonl

The JSONL file is intended for later LLM/behavior-learning or replay analysis.

## Future Scenarios (Not Yet Implemented)

| Scenario | Blocker / Note |
|----------|---------------|
| Token expiry | Wait past `token_deadline` before connecting to gate |
| Login flood / rate limiting | Rapid-fire N login requests on same connection |
| Multi-zone switching | Login zone A → leave → assign gate for zone B → login |
| Login during server shutdown | Need graceful-shutdown signal coordination |
| Reconnect after server restart | Requires server lifecycle control |
| Device ID switching | `LoginRequest` doesn't yet have `device_id` field |

## Running

```bash
# Edit mode in config
# robot/etc/robot.yaml → mode: "login-test"

# Run
cd robot && go run .

# Or build + run
cd robot && go build -o robot.exe . && .\robot.exe
```

## Output Example

```
[1/10] Running: NormalLogin
[1/10] PASS: NormalLogin (245ms) player_id=12345
...
======== Login Test Summary ========
Total: 10  Passed: 9  Failed: 1
  [OK]   NormalLogin       245ms  player_id=12345
  [OK]   LoginLogoutCycle  1.2s   completed 3 login/logout cycles
  [FAIL] AccountDisplacement 5.1s  A was NOT kicked after B logged in
```
