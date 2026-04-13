# Gateway K8s Deployment Notes

## Deployed State
- Docker image: `ghcr.io/luyuancpp/mmorpg-gateway:v1` via `deploy/k8s/Dockerfile.go-svc`
- 2 replicas per zone (zones 3-10), 16 pods total
- ConfigMap: `go-svc-gateway-config` per zone, points to `etcd.mmorpg-infra:2379`

## Key Design Decisions
- **Zone-agnostic prefix**: Gateway uses `GateNodeService.rpc/` (no zone filter) to discover ALL gates across all zones. This enables `/api/server-list` to return a global view.
- **Removed `Gate.ZoneId`** from config — unnecessary since gateway is global.
- **No AutoSyncInterval** on etcd client — prevents endpoint corruption bug (see below).
- **Direct etcd Get** (`client.Get` with `WithPrefix`) instead of `namespace.NewKV` wrapper — the namespace wrapper had issues returning stale/empty results.

## Bug Found & Fixed: etcd AutoSync Endpoint Corruption
- **Root cause**: etcd `ETCD_ADVERTISE_CLIENT_URLS` was `http://etcd:2379`. When the Go etcd client's AutoSync (30s) called MemberList, it got this URL and replaced its internal endpoint list with `etcd:2379`. From cross-namespace pods, `etcd` (without namespace qualifier) can't be resolved → all subsequent queries return 0 keys.
- **Fix 1**: Changed `ETCD_ADVERTISE_CLIENT_URLS` to `http://etcd.mmorpg-infra.svc.cluster.local:2379` in `deploy/k8s/manifests/infra/etcd.yaml`.
- **Fix 2**: Removed `AutoSyncInterval` from gateway's etcd client config (defensive; single-node etcd doesn't need endpoint discovery).

## Files Changed
- `go/gateway/internal/svc/service_context.go` — zone-agnostic prefix, no AutoSync, direct Get
- `go/gateway/internal/config/config.go` — removed `ZoneId` from `GateConf`
- `go/gateway/etc/gateway.yaml` — removed `ZoneId` line
- `go/gateway/gateway.go` — removed zone_id from startup banner
- `tools/scripts/k8s_deploy.ps1` — removed `ZoneId` from gateway ConfigMap template
- `deploy/k8s/manifests/infra/etcd.yaml` — fixed `ETCD_ADVERTISE_CLIENT_URLS` to FQDN

## Smoke Test Results (verified)
| Endpoint | Input | Output |
|----------|-------|--------|
| GET /api/server-list | - | All zones with registered gates |
| POST /api/assign-gate | zone_id=N | Lowest-load gate IP/port + HMAC token |
| POST /api/assign-gate | zone_id=99 | Clean error: "no gate available" |
