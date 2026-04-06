# Login Gate Assignment Migration Plan

## Status: Phase 1 COMPLETE (2026-04-05)

## Current Architecture (after Phase 1)
- **Gateway service** (`go/gateway/`) provides HTTP REST endpoints — the public-facing API layer
- **Login service** (`go/login/`) retains the original gRPC `AssignGate` RPC for internal/robot use
- Client flow: `GET /api/server-list` → pick zone → `POST /api/assign-gate {zone_id}` → get `{ip, port, token}` → TCP connect to Gate
- Gate token verify (`ClientTokenVerifyRequest`) unchanged — same HMAC-SHA256 token format
- Robot client updated to call `/api/server-list` for automatic zone selection when `zone_id=0`

## Phase 1: HTTP Platform API (DONE)
- **Gateway service** created in `go/gateway/` (15 files, go-zero REST server)
- Endpoints:
  - `GET /api/server-list` → returns zone list with id, name, status, recommended flag
  - `POST /api/assign-gate` → accepts `{zone_id}`, returns `{ip, port, token, deadline}`
  - `GET /api/health` → health check
- Gateway discovers gates from etcd using zone-agnostic prefix `GateNodeService.rpc/`
- HMAC token signing reuses Login's algorithm (shared secret)
- Deployed: 2 replicas per zone (zones 3-10), 16 pods total, image `ghcr.io/luyuancpp/mmorpg-gateway:v1`
- Login's REST routes removed; Login is now pure gRPC
- The existing gRPC `AssignGate` stays for internal/robot backward compatibility

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
- `go/gateway/` — new Gateway service (15 files, go-zero REST)
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
