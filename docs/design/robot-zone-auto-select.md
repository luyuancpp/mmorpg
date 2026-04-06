# Robot Zone Auto-Select Feature

## Date: 2026-04-06

## Problem
Robot client hardcoded `zone_id: 0` everywhere when calling `AssignGate` (both gRPC and HTTP paths).
No integration with Gateway's `/api/server-list` endpoint for automatic zone selection.

## Solution
When `zone_id=0` in config, robot calls `GET /api/server-list` first, picks the recommended zone (or first zone if none recommended), then passes that zone_id to `/api/assign-gate`. If server-list call fails, gracefully falls back to `zone_id=0`.

When a specific `zone_id` is set in config, it uses that directly (no server-list call).

## Files Changed
- `robot/config/config.go` — added `ZoneID uint32` field (`yaml:"zone_id"`)
- `robot/gate.go` — added `resolveZoneID()`, updated `resolveGateViaHTTP()` (zone auto-detect when 0), updated `resolveGateViaGRPC()` (pass `cfg.ZoneID`)
- `robot/main.go` — same changes to "Local" suffix copies: added `resolveZoneIDLocal()`, updated `resolveGateViaHTTPLocal()` and `resolveGateAddrLocal()`
- `robot/etc/robot.yaml` — added `zone_id: 0` with comment
- `deploy/k8s/robot_stress.ps1` — K8s ConfigMap template updated with `gate_mode`, `gateway_addr`, `zone_id` fields

## resolveZoneID Logic
1. `GET {gatewayAddr}/api/server-list` with 5s timeout
2. Parse JSON: `{"zones": [{"zone_id": N, "recommended": true/false}]}`
3. Return first zone with `recommended=true`, else return first zone
4. On any error, return `(0, err)` — caller falls back to 0

## Config Example
```yaml
gate_mode: "http"
gateway_addr: "http://127.0.0.1:8080"
zone_id: 0        # 0 = auto-select recommended zone from /api/server-list
```
