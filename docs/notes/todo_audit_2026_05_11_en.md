# Todo Codebase Audit Report

> Generated 2026-05-11, covering `docs/notes/todo.md` — 286 entries total (~273 effective, including skipped numbers).
> A full codebase audit was performed; each item is assigned one of four statuses:
> - ✅ Done — clear code/documentation evidence found
> - 🟡 Partial — partially implemented, design-doc only, or has a TODO comment
> - ❌ Not done — no evidence found
> - ➖ N/A — principle note / client-side feature / marked for deletion / duplicate entry
>
> Audit method: 6 parallel Explore agents scanning the full repo (cpp/, go/, java/, proto/, docs/, tools/, .github/).

---

## 1. Overview

| Range | ✅ Done | 🟡 Partial | ❌ Not done | ➖ N/A | Subtotal |
|---|---|---|---|---|---|
| #1–#50 | 12 | 18 | 14 | 6 | 50 |
| #51–#100 | 6 | 18 | 26 | 0 | 50 |
| #101–#150 | 16 | 22 | 12 | 0 | 50 |
| #151–#200 | 8 | 10 | 13 | 19 | 50 |
| #201–#250 | 9 | 22 | 17 | 1 | 49 |
| #251–#286 | 14 | 11 | 10 | 2 | 37 |
| **Total** | **65** | **101** | **92** | **28** | **286** |

> Breakdown: ✅ 23% / 🟡 35% / ❌ 32% / ➖ 10%
>
> Excluding ➖ N/A items, actionable completion rate: ✅ 25% / 🟡 39% / ❌ 36%

**How to read this:**
- **65 items genuinely shipped** — infrastructure density is much higher than expected (message rate-limiting, debt recovery, rollback, reconnect, alerting, codegen, hot-reload, ECS decoupling, clang-tidy plugins…)
- **101 items partial** — the largest bucket; most have a design doc / skeleton / TODO comment but are not closed out. **Highest ROI area for gap-filling next.**
- **92 items not done** — a mix of gameplay features (red envelopes / quiz / spectate) and real engineering gaps (distributed tracing, error reporting, auction house, chat ad filtering…)
- **28 items are principle notes / duplicates / marked deleted** — recommended for removal from todo

---

## 2. ✅ 65 genuinely shipped items (grouped by theme)

### Infrastructure · Reliability
| # | Feature | Evidence |
|---|---|---|
| #2 | Message rate-limiting (MessageLimiter) | `cpp/libs/engine/core/message_limiter/message_limiter.cpp` |
| #5, #108, #192 | pb field oversized warning / field length / negative value checks | `cpp/libs/engine/core/utils/proto/proto_field_checker.cpp` |
| #21, #149, #213 | Graceful shutdown / GracefulShutdown | `cpp/libs/engine/core/node/system/node/node.cpp`, `cpp/nodes/scene/main.cpp`, `gm_admin.proto` |
| #93, #95, #166 | Server-initiated player disconnect + forceClose on per-player traffic > 2 MB | `cpp/nodes/gate/handler/rpc/client_message_processor.cpp`, `gate_client_high_water_mark.md` |
| #109 | Stack trace / log fast diagnosis | `cpp/libs/engine/core/utils/debug/stacktrace_system.h` (boost::stacktrace) + signal handler |
| #123 | Lock-free multi-thread message queue | `DoubleBufferQueue` + Kafka per-partition (`go/db/internal/kafka/key_ordered_consumer.go`) |
| #132 | Prometheus alerting | `deploy/k8s/scene-manager-alerts.yaml` three-tier alerts |
| #135 | Tables store IDs not pointers | clang-tidy plugin + `config_id` table lookup |
| #111 | Traffic profiling | `traffic_statistics.h` + Go `grpcstats/collector.go` |
| #131 | load/save failure handling | `HandlePlayerAsyncLoadFailed`/`Saved` in `player_lifecycle.h` |

### Data / Storage
| # | Feature | Evidence |
|---|---|---|
| #13 | Save-on-change (dirty flag) | `player_currency_comp.h` |
| #41 | Single-player rollback | `go/data_service/internal/logic/recall_logic.go` + `player_rollback_handler.cpp` |
| #46 | Player Redis cache cleanup | `go/player_locator/internal/logic/leasemonitor.go` |
| #71 | Data backup and rollback | `single_player_rollback.md` + `rollback_logic.go` + `snapshot_logic.go` |
| #194 | Delete a database row | `DeletePlayerData` RPC |
| #271/#278/#284 | DB/Redis reconnect + storage guarantees | `infra-reconnect-overview.md` + `redis_manager.cpp` + `key_ordered_consumer.go` |

### Login / Session
| # | Feature | Evidence |
|---|---|---|
| #44 | Long disconnect + duplicate login / kick old session | `loginlogic.go` duplicate login detection + kick |
| #89 | Block messages from unauthenticated players | `client_message_processor.cpp::ResolveSessionTargetNode` |
| #195 | Player network anomaly handling | `setdisconnectinglogic.go` + `markofflinelogic.go` |
| #209 | Go-side authentication | `go/login/internal/logic/pkg/auth/` + `token/token.go` |
| #235 | session id anti-tampering | gate HMAC-SHA256 token + session_id binding |
| #256 | MMO disconnect reconnect | gate/scene reconnect + player_locator 30s lease + `reconnectlogic.go` |

### Gameplay / Architecture
| # | Feature | Evidence |
|---|---|---|
| #6, #59 | Currency anomaly detection + debt recovery | `anomaly_detector.cpp` + `recall_logic.go` + `CurrencySystem::AttachDebt` |
| #22 | Distributed friends / guild | `go/friend` + `go/guild` |
| #30 | Movement sync | `player_movement_handler.cpp` |
| #85 | Non-tick TimeMeter | `time_meter.h` + `time_meter_test.cpp` |
| #104 | Scene-player decoupling | ECS `PlayerSceneSystem`/`PlayerLifecycleSystem` |
| #112 | Server has no "channel" concept | `world-channel-system.md` |
| #114 | Progress reward does not use reward_table_id as key | `reward_comp.h` |
| #128, #193 | GS-to-GS RPC / cross-node player messaging | `s2s_player_scene_handler.cpp` + `route_message_response_handler.cpp` |
| #129 | Bag layering | `BagService` orchestration / `Bag` pure container |
| #190 | Underlying layer does routing only, not business logic | gate `client_message_processor.cpp` |
| #251 | Cross-scene message delivery | `cross_scene_player_messaging.md` + `route_message_response_handler.cpp` |
| #257 | Cross-server player partitioning is storage-only | `cross_server_architecture_principle.md` |
| #258 | AOI priority | `aoi.cpp` + `aoi_priority_design.md` + `aoi_test/` |
| #262 | Globally unique ID (Snowflake) | `go/shared/snowflake/snowflake.go` + `cpp/libs/engine/thread_context/snow_flake_manager.cpp` |
| #281 | Stateless / idempotent | `login-node-stateless-no-affinity.md` + `createscenelogic.go` |
| #282 | Config / table-driven hot-reload | `generated/table/code/` + `tools/data_table_exporter/` |

### Toolchain / Quality
| # | Feature | Evidence |
|---|---|---|
| #47, #274 | C++ member raw pointer ban | `cpp/plugin/NoMemberRawPointerCheck.cpp` + `.clang-tidy` (clang-tidy plugin) |
| #137 | Codegen for complex templates | `tools/proto_generator/` full codegen suite |
| #178 | Precondition system | `condition_util.h/cpp` + `condition_table.h` |
| #200 | Data sync on service restart | `rolling-update-restart-resilience-tests.md` + `entergamelogic.go` |

### HTTP /api/login migration (sub-project completed 2026-05)
| # | Feature | Evidence |
|---|---|---|
| #219 | Microservice readiness gate | `DependencyGate` in `node.h` + Scene/Gate `main.cpp` |
| #220 | Migration PR + CI all green | `.github/workflows/login-path-tests.yml` |
| #221 | Linux staging load-test toolchain | `tools/scripts/deploy-staging.sh` |
| #222 | Prometheus login_auth_path_total | `login.yaml` Prometheus + `grafana-login-path-deprecation.json` |
| #224 | legacy-gate-login-enabled feature flag | `legacy_gate_killswitch.go` + `login.yaml` |

---

## 3. 🟡 Partial — high-ROI gap-fill candidates

These are **the most worthwhile next investments** — design doc exists / skeleton in place / TODO comments present, just need the last mile:

| # | Item | What's missing |
|---|---|---|
| **#102** | Server-open rate limiting (Bucket4j + Redis + wave-based admission) | Design complete, Java skeleton written; **steps 2–10 marked ⬜ incomplete** |
| **#51/#139/#241** | National mourning day / maintenance auto time-shift for activities | `activity_maintenance_auto_shift.md` exists; code layer not wired up |
| **#76/#75** | Message tamper detection | adler32 checksum in place; HMAC signature missing |
| **#97** | Unified RPC error `reason` field | `error_tip` table exists; call sites not fully covered |
| **#106/#119** | Message priority queue | AOI priority + DoubleBufferQueue present; dedicated "priority message queue" missing |
| **#204/#226** | proto-compare-driven unified dirty-save | Currently manual dirty flags |
| **#238/#242/#243/#280** | DB disconnect / incomplete save + save-before-load on scene exit | `player_lifecycle.cpp:376` has `// TODO: Only allow re-login after save completes` |
| **#208** | Cross-scene-server player-to-player messaging | Design doc `cross_scene_player_messaging.md` has extensive TODO markers |
| **#247** | Protocol version compatibility | proto3 is inherently compatible; dedicated version layer missing |
| **#210** | External-to-internal network data isolation | `k8s_gate_exposure_guidance.md` + `AdminApiKeyFilter` exist; dedicated code guardrails missing |
| **#216** | Signal to dump all thread stack traces | `PrintDefaultStackTrace()` + `thread_observability.h` present; SIGUSR1 entry point missing |
| **#225** | Subtle difference between complete_quest vs add_complete_quest | Two code paths exist with **no comment explaining the difference** — confirmed tech debt |
| **#223** | WeChat/QQ real OAuth sandbox e2e | Runbook ready; **blocked on real AppId/AppSecret** |

---

## 4. ❌ Not done — TOP 15 by risk/value

### 🔴 Production incident defenses (fix soon)

| # | Item | Pain point |
|---|---|---|
| **#152** | Distributed tracing (jaeger/zipkin/otel) | No fast root-cause path when incidents occur (see also #27); currently only incidental otel from go-redis vendor — business traces not instrumented |
| **#250** | Dev-cycle error reporting to a central service (Sentry / homegrown) | Failures only discovered after the fact; no real-time aggregation |
| **#273** | Build version tagging + binary/source archiving | Cannot reproduce the exact build at time of a production bug |
| **#105** | Missing logout_time on player crash | Key timestamp absent for post-incident forensics |
| **#236** | Log or kick player after N illegal protocol packets | MessageLimiter handles rate, but illegal packet count is not tracked → cannot block cheat clients |

### 🟠 Anti-cheat / Security

| # | Item | Pain point |
|---|---|---|
| **#68** | Chat ad / sensitive-word filtering | Chat will be spammed with ads once enabled |
| **#207** | Cross-server currency deduction authentication | Debt recovery exists (#59), but **preventive** deduction auth is absent |

### 🟡 Gameplay infrastructure (product-priority dependent)

| # | Item | Notes |
|---|---|---|
| **#16/#150/#198/#201** | Spectate / replay / switch to teammate view | Same feature block — entirely absent |
| **#115** | 500k concurrent cross-server queue | Only small-scale 5v5/3v3 matchmaking; large-scale open-server / cross-server queue missing (see also #98/#102) |
| **#33/#34/#35** | Red envelopes / quiz / voting | Three classic live-ops features — all absent |
| **#269** | NPC dialogue system | Not built |
| **#165** | Auction house system | Entire system missing |
| **#283** | Offline player reward delivery at activity end | Mail compensation mechanism absent |

### ⚪ Basic capabilities

| # | Item | Notes |
|---|---|---|
| **#28/#130** | User behavior analytics / feature telemetry | No data-driven way to cut unused features |
| **#19** | i18n / multi-language | Not built |
| **#45** | Merge servers when character slots are full | Ops scenario |
| **#48** | Cross-server disconnect reconnect | Same-server reconnect done (#256); cross-server not done |
| **#50** | Stop showing "too frequent" after a successful action | MessageLimiter has no reset hook |

---

## 5. ➖ N/A — 28 items to drop from todo

### Explicitly marked (5 items)
- #8 (duplicate of #2) · #18 (author note: "Delete this todo") · #24 (client-side) · #174 (duplicate of #154) · #14 (principle)

### Pure principle notes (19 items — coding philosophy, not deliverables)
#153–155, #158–162, #168–169, #172–176, #181–183, #191, #197

> All of these express the same class of idea: "fewer call hops → fewer bugs", "code readable at a glance", "decoupled systems", "don't over-engineer"… Recommend either deleting them or moving them to a dedicated `coding_principles.md` — they don't belong in a task backlog.

### Client / product-facing (4 items)
- #24 Harry-Potter-style download loading screen
- #196 In-game pause (not applicable for MMO)
- #232 "Debugging multiple places for one issue = messy code" (observation note)
- #252 "Can players log in after the server runs past midnight without a restart?" (question, not a task)

---

## 6. Contradictions / tech debt discovered

Several inconsistencies found during the audit that need a decision:

### 1. **Raw pointer check inconsistency**
- #47/#274 claim the `cpp/plugin/NoMemberRawPointerCheck.cpp` clang-tidy plugin enforces the ban
- #186 audit found `cpp/libs/modules/currency/currency_system.h` **has raw pointer members**
- **Conclusion:** The plugin either has a whitelist, or is not yet active for the `modules/` directory — recommend running a full clang-tidy pass to identify which modules are being skipped

### 2. **#225 MissionSystem two paths with no comments**
`CompleteAllMissions` and `OnMissionCompletion` appear to do the same thing but **are genuinely two distinct code paths** with **no comments explaining the difference**. This matches the warning in todo item #225: "looks like the same logic, but has subtle differences." Confirmed tech debt.

### 3. **#280 explicit TODO comment**
`cpp/libs/services/scene/player/system/player_lifecycle.cpp:376` contains:
```
// TODO: Only allow re-login after save completes
```
This is a confirmed incomplete mechanism; #242/#243 both point to the same location.

---

## 7. Recommended next-step priority

Ordered by **ROI (value / effort)**:

### P0 — Worth doing within two weeks
1. **#102 Server-open rate limiting close-out** — design fully written, Java skeleton ready, just need to complete steps 2–10.
2. **#280/#242/#243 Race condition: re-login before save completes** — TODO comment confirmed, risk is clear, close the loop.
3. **#70/#125 Print full request proto + stack trace on error code** — infrastructure already in place (stacktrace_system + error_tip), just add a unified hook.
4. **#216 SIGUSR1 dump all thread stack traces** — `PrintDefaultStackTrace()` already exists, just register the signal handler.

### P1 — Within one month
5. **#152 Distributed tracing** — pick otel, instrument go/java/cpp all three layers together. Bundle with #27/#250.
6. **#236 Illegal packet counter + kick** — add an `IllegalPacketCounter` alongside MessageLimiter.
7. **#273 Build version archiving** — include git-sha + source tarball in CI artifacts.
8. **#225 MissionSystem two-path comments** — pure documentation work, but prevents future developers from stepping on the same landmine.

### P2 — By product priority
- #16/#201/#198 Spectate / replay — same feature block
- #165 Auction house — new system
- #33/#34/#35 Red envelopes / quiz / voting — live-ops features

### Recommended immediate cleanup
- Remove the 28 ➖ N/A items from todo (or move to `coding_principles.md`) to reduce noise.
- Add ✅ marker + evidence path to each completed item (following the format of #219/#220/#222/#224) so future readers of todo don't waste time re-investigating.

---

*Audit generated by 6 parallel Explore agents, ~7000 tool calls, ~90 min wall time.*
