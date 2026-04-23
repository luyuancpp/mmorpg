# K8S KNOWLEDGE BASE

## OVERVIEW
`deploy/k8s/` is the Kubernetes-only release path for game zones. It is separate from local Docker Compose and assumes Linux runtime artifacts.

## STRUCTURE
```text
deploy/k8s/
├── manifests/         # Infra and workload manifests
├── runtime/           # Staged runtime files for Linux image
├── Dockerfile.runtime # Production K8s runtime image
├── zones.*            # One-click zone config examples / ops presets
└── README.md          # Authoritative flow description
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| End-to-end flow | `README.md` | One-zone/all-zone operations |
| Runtime staging layout | `runtime/README.md` | Required Linux file layout |
| Runtime image | `Dockerfile.runtime` | Use this, not root Dockerfile |
| Infra manifests | `manifests/infra/` | Shared infra (etcd/redis/kafka/mysql) deployed to `mmorpg-infra` namespace |
| Script entrypoint | `tools/scripts/dev_tools.ps1` | `k8s-*` commands drive this subtree |

## CONVENTIONS
- This subtree is Kubernetes-only; do not mix docker-compose/local process assumptions into it.
- Production image must use `deploy/k8s/Dockerfile.runtime`.
- Runtime expects Linux binaries staged under `deploy/k8s/runtime/linux/`.
- Managed cloud generally uses `LoadBalancer`; bare metal generally uses `NodePort` + external L4.
- Prefer explicit `-OpsProfile managed-cloud` / `-OpsProfile bare-metal` in commands and docs.

## ANTI-PATTERNS
- Using the repository root `Dockerfile` as the K8s runtime image.
- Treating `LoadBalancer` as a universal default.
- Assuming Windows `.exe` outputs in `bin/` are deployable to the runtime image.
- Describing `k8s-zone-down` as a partial cleanup; it deletes the full namespace.

## COMMANDS
```bash
# Build all images before deploying
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-build-all

# Deploy shared infrastructure (one-time, all zones share this)
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-infra-up
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-infra-status

# Deploy zones
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-up -ZoneName yesterday -ZoneId 101 -OpsProfile managed-cloud -WaitReady -NodeImage ghcr.io/luyuancpp/mmorpg-node:latest
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-all-up -ZonesConfigPath deploy/k8s/zones.yaml -OpsProfile managed-cloud -NodeImage ghcr.io/luyuancpp/mmorpg-node:latest
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-down -ZoneName yesterday

# Runtime staging (alternative: pre-built binaries)
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-image-preflight
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-stage-runtime -BinarySourceRoot D:/linux-build/bin -ZoneInfoSource bin/zoneinfo -TableSource generated/tables
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-build-image -ImageRepository ghcr.io/luyuancpp/mmorpg-node -ImageTag v1
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-push-image -ImageRepository ghcr.io/luyuancpp/mmorpg-node -ImageTag v1
```

## NOTES
- **Architecture**: All zones share one set of infra services (etcd, Redis, Kafka, MySQL) in the `mmorpg-infra` namespace. Zone namespaces contain only game nodes and microservices.
- Config templates use cross-namespace K8s DNS: `etcd.mmorpg-infra:2379`, `redis.mmorpg-infra:6379`, etc.
- `zones.ops-recommended.yaml` is the best starting point for multi-zone ops.
- `-SkipInfra`, `-DryRun`, and `-WaitReady` are the high-signal operational flags.
- `k8s-all-up` deploys infra first, then all zones. Use `-SkipInfra` to skip infra.
- Current manifests assume `/app/bin` runtime layout and fixed role ports (`gate` 18000, `scene` 19000).

## SCENE NODE ROLE SPLIT
- Production scene pods are expected to run in two Deployments per zone:
  - `scene-world`    — `env SCENE_NODE_TYPE=0`, hosts persistent main-world channels.
  - `scene-instance` — `env SCENE_NODE_TYPE=1`, hosts on-demand dungeons and battlegrounds.
- The zones YAML expresses this with `scene_world: N` / `scene_instance: N` under `replicas`. Legacy single-pool `scene: N` remains accepted for dev.
- C++ reads `SCENE_NODE_TYPE` and `GAME_CONFIG_PATH` from the pod env; the file baseline stays at `etc/game_config.yaml`.
- Go-side routing is driven by `StrictNodeTypeSeparation` in `scene_manager_service.yaml`. Keep `true` in production; flip to `false` only during the rollout window described in `docs/ops/scene-node-role-split.md`.
