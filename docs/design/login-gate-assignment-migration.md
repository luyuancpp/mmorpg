# Login Gate Assignment Migration Plan

## Status: Phase 1 COMPLETE (2026-04-05), Gateway migrated to Java (2026-04-14)

## Current Architecture
- **Java Gateway** (`java/gateway_node/`) — Spring Boot, replaced Go prototype. Full admin/ops features: server-list, assign-gate, announcements, whitelist, CDN signing, hotfix check, zone health probe.
- **Login service** (`go/login/`) retains the original gRPC `AssignGate` RPC for internal/robot use
- Client flow: `GET /api/server-list` → pick zone → `POST /api/assign-gate {zone_id}` → get `{ip, port, token}` → TCP connect to Gate
- Gate token verify (`ClientTokenVerifyRequest`) unchanged — same HMAC-SHA256 token format
- Robot client updated to call `/api/server-list` for automatic zone selection when `zone_id=0`

## Phase 1: HTTP Platform API (DONE — then migrated Go→Java)
- **Original**: Go gateway in `go/gateway/` (15 files, go-zero REST, 2026-04-05)
- **Replaced by**: Java gateway in `java/gateway_node/` (Spring Boot, 2026-04-14)
- Reasons for migration: Java gateway supports MySQL-backed zone config, admin APIs, announcement management, CDN signing, hotfix checks — features that benefit from Spring Boot ecosystem
- Core endpoints unchanged:
  - `GET /api/server-list` → zone list with status/recommended
  - `POST /api/assign-gate` → least-load gate + HMAC token
- Go `go/gateway/` code still in repo but deprecated

### Phase 2: SDK Authentication
- Add **third-party OAuth** integration (WeChat, Apple, Google, etc.)
- Flow: Client → SDK → get `access_token` → send to our HTTP API for verification
- Our API calls SDK server to validate token (server-to-server callback)
- On success, issues internal `session_token` for gate assignment
- The Java `sa_token_node` may serve as the auth bridge or be replaced

### Phase 3: Session Key Upgrade
- Current: HMAC token tied to `gate_node_id` with 60s TTL
- Target: session_key with richer metadata (account_id, zone_id, client_ip binding, anti-replay nonce)
- Consider short-lived JWT or opaque token stored in Redis

## Key Files Affected
- `java/gateway_node/` — current Java Gateway (Spring Boot, replaced Go version)
- `go/gateway/` — deprecated Go prototype (still in repo)
- `go/login/internal/logic/pregate/getgatelistlogic.go` — current AssignGate (kept for internal use)
- `go/login/internal/svc/gate_watcher.go` — gate discovery (reused by Gateway)
- `cpp/nodes/gate/handler/rpc/client_message_processor.cpp` — token verify (upgrade to new format in Phase 3)
- `robot/gate.go`, `robot/main.go` — robot client (HTTP path with zone auto-select added)
- `robot/config/config.go` — added `ZoneID` field
- `deploy/k8s/robot_stress.ps1` — K8s robot ConfigMap updated with gateway fields

## Migration Strategy
- Phase 1 first: purely additive, no breaking changes
- Robot client keeps gRPC path; real game client uses HTTP path
- Gate token verify supports both old HMAC and new session_key format during transition

## Industry Reference: Major Studio Architectures

| Company | Platform Auth | Access Layer (Gateway) | Gate | Game Server |
|---------|--------------|----------------------|------|-------------|
| Tencent (MSDK) | MSDK → WeChat/QQ OAuth → get openid+token | Standalone HTTP service, verify MSDK token, issue session_key | TCP, verify session_key | Role logic |
| NetEase (URS) | URS SDK → NetEase account → token | Standalone HTTP "login gateway" | TCP | Role logic |
| miHoYo | HoYoverse SDK → token | HTTP API (international uses dispatch server) | KCP/TCP | Role logic |
| Lilith | In-house SDK → token | HTTP Gateway (Go) | TCP Gateway | Game Server |

### Key Observations
- All studios separate **platform auth** (SDK token verification) from **gate connection** (TCP/KCP session).
- The access layer is always an **independent HTTP service** sitting between SDK auth and the game gate.
- This matches our Phase 2 target: Gateway HTTP API verifies SDK token, issues session_key, client connects to Gate with session_key.
- miHoYo's "dispatch server" pattern is notable — it dynamically routes players to different regional server clusters.
- KCP (used by miHoYo/Genshin) is a reliable UDP protocol optimized for latency-sensitive games.
