# Gateway Service Notes

## Current Implementation: Java (`java/gateway_node/`)
- **Replaced** Go gateway (`go/gateway/`) as of 2026-04-14
- Spring Boot service with MySQL + Redis + etcd
- Much richer feature set than the original Go prototype

### Endpoints
| Category | Endpoints |
|----------|-----------|
| Player-facing | `GET /api/server-list`, `POST /api/assign-gate` |
| Announcements | announcement CRUD + display |
| Hotfix | hotfix check |
| CDN | CDN URL signing |
| Admin | zone config, whitelist, announcement management |
| Health | zone health probe (periodic gate liveness check) |

### Key Services
- `ServerListService` — zone list from MySQL `zone_config` table + etcd gate discovery
- `AssignGateService` — least-load gate selection + HMAC token signing
- `ZoneHealthProbeService` — periodic check, marks zones offline if no gate heartbeat
- `GateWatcher` — etcd watcher for `GateNodeService.rpc/` prefix
- `AnnouncementService`, `CdnSignService`, `HotfixCheckService`

### Config: `application.yaml`
- etcd endpoints, MySQL datasource, Redis, gate token secret/TTL, zone probe interval

## Previous Implementation: Go (`go/gateway/`) — DEPRECATED
- Original prototype (2026-04-05), 15 files, go-zero REST server
- Only had 3 endpoints: server-list, assign-gate, health
- Superseded by Java version for richer admin/ops features

## Design Decisions (still apply)
- **Zone-agnostic prefix**: Gateway uses `GateNodeService.rpc/` to discover ALL gates globally
- **etcd FQDN fix**: `ETCD_ADVERTISE_CLIENT_URLS` must use `etcd.mmorpg-infra.svc.cluster.local:2379` (not short name) to prevent cross-namespace resolution failures
- **HMAC token**: same algorithm shared between gateway and gate node for token verification
