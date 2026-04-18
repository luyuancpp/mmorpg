# Docker & K8s Build/Deploy Guide (Windows)

**Updated:** 2026-03-27

## Overview

Three types of Docker images, all buildable on Windows via Docker Desktop (Linux containers mode).

| Type | Dockerfile | Build Context | Image Name |
|------|-----------|---------------|--------|
| C++ Nodes (gate+scene) | `deploy/k8s/Dockerfile.cpp` | Repository root | `mmorpg-node` |
| Go Services (5) | `deploy/k8s/Dockerfile.go-svc` | `go/` | `mmorpg-{service}` |
| Java Gateway | `deploy/k8s/Dockerfile.java-svc` | `java/gateway_node/` | `mmorpg-gateway` |

## C++ Node Image — Multi-Stage Build

```
Stage 1 (deps)    : gcc:13 + setup_dependencies.sh
                    → Compile gRPC, muduo, hiredis, librdkafka, yaml-cpp, zlib
Stage 2 (builder) : vcxproj2cmake.py → build_linux.sh --skip-deps --release
                    → Compile 14 static libraries + gate/scene executables
Stage 3 (runtime) : ubuntu:24.04
                    → Copy only gate/scene binaries + data tables + zoneinfo
```

First build takes approximately 30–60 minutes (gRPC compiled from source). Subsequent builds leverage Docker layer caching — dependencies won't be recompiled as long as `third_party/` hasn't changed.

## Full Commands

### Prerequisites

- Install Docker Desktop with Linux containers mode enabled
- Install `kubectl` and configure cluster connection
- (Optional) Local infrastructure: `cd deploy; docker compose up -d`

### Build Images

```powershell
# C++ nodes (multi-stage compilation, no Linux environment required)
docker build -f deploy/k8s/Dockerfile.cpp -t ghcr.io/luyuancpp/mmorpg-node:v1 .

# Go services (5)
pwsh -File tools/scripts/dev_tools.ps1 -Command go-svc-build-images `
  -GoSvcRegistry ghcr.io/luyuancpp -GoSvcTag v1

# Java Auth
pwsh -File tools/scripts/dev_tools.ps1 -Command java-svc-build-image `
  -JavaSvcRegistry ghcr.io/luyuancpp -JavaSvcTag v1
```

### Push Images

```powershell
docker push ghcr.io/luyuancpp/mmorpg-node:v1

pwsh -File tools/scripts/dev_tools.ps1 -Command go-svc-push-images `
  -GoSvcRegistry ghcr.io/luyuancpp -GoSvcTag v1

pwsh -File tools/scripts/dev_tools.ps1 -Command java-svc-push-image `
  -JavaSvcRegistry ghcr.io/luyuancpp -JavaSvcTag v1
```

### Deploy to K8s

```powershell
# Launch a single zone
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-up `
  -ZoneName yesterday -ZoneId 101 `
  -OpsProfile managed-cloud `
  -NodeImage ghcr.io/luyuancpp/mmorpg-node:v1 `
  -GoSvcRegistry ghcr.io/luyuancpp -GoSvcTag v1 `
  -JavaSvcRegistry ghcr.io/luyuancpp -JavaSvcTag v1 `
  -WaitReady

# Batch launch zones
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-all-up `
  -ZonesConfigPath deploy/k8s/zones.ops-recommended.yaml `
  -OpsProfile managed-cloud `
  -NodeImage ghcr.io/luyuancpp/mmorpg-node:v1 `
  -GoSvcRegistry ghcr.io/luyuancpp -GoSvcTag v1 `
  -JavaSvcRegistry ghcr.io/luyuancpp -JavaSvcTag v1 `
  -WaitReady
```

### Tear Down Zone (Dangerous — deletes the entire namespace)

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-down -ZoneName yesterday
```

## Alternative: Pre-compiled Binaries

If C++ has already been compiled on a Linux host/WSL, you can skip the multi-stage build and use the lightweight `Dockerfile.runtime` directly:

```powershell
# 1. Stage binaries to a staging directory
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-stage-runtime `
  -BinarySourceRoot D:/linux-build/bin `
  -ZoneInfoSource bin/zoneinfo `
  -TableSource generated/tables

# 2. Build runtime-only image (seconds, no compilation)
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-build-image `
  -ImageRepository ghcr.io/luyuancpp/mmorpg-node -ImageTag v1
```

## Key Files

| File | Purpose |
|------|------|
| `.dockerignore` | Excludes bin/, .git, docs, etc. to reduce build context |
| `deploy/k8s/Dockerfile.cpp` | Multi-stage C++ compilation image |
| `deploy/k8s/Dockerfile.runtime` | Lightweight runtime image (requires pre-compiled binaries) |
| `deploy/k8s/Dockerfile.go-svc` | Go service multi-stage image |
| `deploy/k8s/Dockerfile.java-svc` | Java Auth multi-stage image |
| `tools/scripts/k8s_image.ps1` | Image preflight / build / push |
| `tools/scripts/k8s_stage_runtime.ps1` | Stage Linux binaries |
| `tools/scripts/k8s_deploy.ps1` | zone-up / zone-down / zone-status |
| `deploy/k8s/zones.ops-recommended.yaml` | Recommended zone launch configuration |

## Notes

- The `centre` node is deprecated and has been removed from preflight/staging checks
- K8s infrastructure (mysql/redis/kafka/etcd) uses `emptyDir` — dev only; production requires PVC
- Gate exposure: managed cloud → `LoadBalancer`; self-hosted bare metal → `NodePort` + external L4
- Each zone = 1 K8s namespace (`mmorpg-zone-{name}`)
