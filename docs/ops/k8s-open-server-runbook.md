# K8s Open-Server Runbook

This runbook is for production-style Kubernetes open-server operations only.
It does not cover Docker Compose or local process startup.

## 1. Preconditions

1. `kubectl` can access target cluster and namespace operations are allowed.
2. Runtime Linux payload is available (no `.exe` binaries).
3. Target image repository is writable.
4. Zone planning is ready: zone name, zone id, and replica counts.

Recommended quick checks:

```powershell
kubectl version --short
kubectl get nodes
```

## 2. Stage Runtime Payload

Copy Linux binaries and assets into `deploy/k8s/runtime/linux`:

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-stage-runtime `
  -BinarySourceRoot D:/linux-build/bin `
  -ZoneInfoSource bin/zoneinfo `
  -TableSource generated/tables
```

Run preflight to block incomplete runtime inputs:

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-image-preflight
```

## 3. Build And Push Runtime Image

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-build-image `
  -ImageRepository ghcr.io/luyuancpp/mmorpg-node `
  -ImageTag v1

pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-push-image `
  -ImageRepository ghcr.io/luyuancpp/mmorpg-node `
  -ImageTag v1
```

## 4. Exposure Sanity Check

Before production open-server, run a dry-run exposure check so the selected profile and service type are explicit in command output.

Expected results:

1. `custom` without explicit `GateServiceType` resolves to `NodePort`.
2. `managed-cloud` resolves to `LoadBalancer`.
3. `custom + LoadBalancer` prints a warning so the operator explicitly confirms cluster LB capability.

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-exposure-preflight
```

If the output does not match the expected service type resolution, stop before deployment and correct the command profile or service type.

## 5. Open One Zone

Managed cloud profile example:

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-up `
  -ZoneName yesterday `
  -ZoneId 101 `
  -OpsProfile managed-cloud `
  -NodeImage ghcr.io/luyuancpp/mmorpg-node:v1 `
  -WaitReady
```

Bare metal profile example:

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-up `
  -ZoneName yesterday `
  -ZoneId 101 `
  -OpsProfile bare-metal `
  -NodeImage ghcr.io/luyuancpp/mmorpg-node:v1 `
  -WaitReady
```

Post-check:

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-status -ZoneName yesterday
```

## 6. Open All Zones

Use YAML zone config for ops readability:

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-all-up `
  -ZonesConfigPath deploy/k8s/zones.ops-recommended.yaml `
  -OpsProfile managed-cloud `
  -NodeImage ghcr.io/luyuancpp/mmorpg-node:v1 `
  -WaitReady
```

Post-check:

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-all-status `
  -ZonesConfigPath deploy/k8s/zones.ops-recommended.yaml
```

## 7. Release Shortcut Commands

One-command zone release:

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-release-zone `
  -ZoneName yesterday `
  -ZoneId 101 `
  -OpsProfile managed-cloud `
  -ImageRepository ghcr.io/luyuancpp/mmorpg-node `
  -ImageTag v1 `
  -WaitReady
```

One-command all-zone release:

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-release-all `
  -ZonesConfigPath deploy/k8s/zones.ops-recommended.yaml `
  -OpsProfile managed-cloud `
  -ImageRepository ghcr.io/luyuancpp/mmorpg-node `
  -ImageTag v1 `
  -WaitReady
```

## 8. Rollback

Use last known good image tag and redeploy.

Single zone rollback:

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-up `
  -ZoneName yesterday `
  -ZoneId 101 `
  -OpsProfile managed-cloud `
  -NodeImage ghcr.io/luyuancpp/mmorpg-node:v0 `
  -WaitReady
```

All zones rollback:

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-all-up `
  -ZonesConfigPath deploy/k8s/zones.ops-recommended.yaml `
  -OpsProfile managed-cloud `
  -NodeImage ghcr.io/luyuancpp/mmorpg-node:v0 `
  -WaitReady
```

Emergency close:

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-down -ZoneName yesterday
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-all-down -ZonesConfigPath deploy/k8s/zones.ops-recommended.yaml
```

## 9. Troubleshooting

Common checks:

```powershell
kubectl get ns
kubectl get pods -n mmorpg-zone-yesterday -o wide
kubectl describe pod <pod-name> -n mmorpg-zone-yesterday
kubectl logs <pod-name> -n mmorpg-zone-yesterday --tail=200
kubectl get svc -n mmorpg-zone-yesterday
```

If `WaitReady` fails:

1. Check image pull errors and registry credentials.
2. Check ConfigMap data and mounted runtime paths.
3. Check gate service exposure mode against cluster type.
4. Verify infra pods (`etcd`, `redis`, `kafka`) are healthy.

## 10. Change Record Template

Keep a short release note per operation:

1. Date/time and operator.
2. Command executed.
3. Image tag.
4. Zones affected.
5. Validation result.
6. Rollback decision and final status.