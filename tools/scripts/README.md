# Development Scripts

PowerShell and shell scripts for common development tasks.

## Available Scripts

### dev_tools.ps1

Main entry point for tool commands on Windows.

#### Commands

```powershell
# Build pbgen
pwsh -File dev_tools.ps1 -Command pbgen-build

# Run pbgen with default config
pwsh -File dev_tools.ps1 -Command pbgen-run

# Run pbgen with custom config
pwsh -File dev_tools.ps1 -Command pbgen-run -ConfigPath <path-to-config>

# Generate project tree report
pwsh -File dev_tools.ps1 -Command tree

# Audit recursive naming issues (default snake_case)
pwsh -File dev_tools.ps1 -Command naming-audit

# Apply recursive naming normalization
pwsh -File dev_tools.ps1 -Command naming-apply

# Use kebab-case style with a capped batch size
pwsh -File dev_tools.ps1 -Command naming-audit -Style kebab -MaxChanges 200

# Open one k8s zone
pwsh -File dev_tools.ps1 -Command k8s-zone-up -ZoneName yesterday -ZoneId 101 -OpsProfile managed-cloud -NodeImage <image> -WaitReady

# Open all zones from JSON/YAML config
pwsh -File dev_tools.ps1 -Command k8s-all-up -ZonesConfigPath deploy/k8s/zones.ops-recommended.yaml -OpsProfile managed-cloud -NodeImage <image> -WaitReady

# Check/close k8s zones
pwsh -File dev_tools.ps1 -Command k8s-zone-status -ZoneName yesterday
pwsh -File dev_tools.ps1 -Command k8s-zone-down -ZoneName yesterday
pwsh -File dev_tools.ps1 -Command k8s-all-status -ZonesConfigPath deploy/k8s/zones.yaml
pwsh -File dev_tools.ps1 -Command k8s-all-down -ZonesConfigPath deploy/k8s/zones.yaml

# Preflight/build/push/release runtime image for k8s
pwsh -File dev_tools.ps1 -Command k8s-stage-runtime -BinarySourceRoot D:/linux-build/bin -ZoneInfoSource bin/zoneinfo -TableSource generated/tables
pwsh -File dev_tools.ps1 -Command k8s-image-preflight
pwsh -File dev_tools.ps1 -Command k8s-build-image -ImageRepository ghcr.io/luyuancpp/mmorpg-node -ImageTag v1
pwsh -File dev_tools.ps1 -Command k8s-push-image -ImageRepository ghcr.io/luyuancpp/mmorpg-node -ImageTag v1
pwsh -File dev_tools.ps1 -Command k8s-release-zone -ZoneName yesterday -ZoneId 101 -ImageRepository ghcr.io/luyuancpp/mmorpg-node -ImageTag v1 -WaitReady
```

### normalize_names.ps1

Recursively normalizes file and directory names to `snake_case` or `kebab-case`.

- Excludes `third_party` by default
- Also skips tooling/build artifact roots by default: `.git`, `.vs`, `.idea`, `.vscode`, `_copilot_session_transfer`, `x64`, `bin`, `generated`, `cpp/generated`, `go/generated`, `cpp/bin`
- Skips embedded muduo vendor trees under `cpp/libs/*/muduo_windows`
- By default only scans source roots: `cpp`, `go`, `java`, `proto`, `docs`, `tools`, `scripts`, `data`, `deploy`, `robot`, `test`, `etc`
- Skips dotfiles and dot-directories to avoid renaming repository metadata files
- Skips high-risk generated/IDE files: `*.vcxproj*`, `*.sln*`, `*.tlog`, `*.recipe`, `*.pb.{h,cc,go}`
- Supports conflict detection (duplicate targets / existing target path)
- Uses deep-first rename order to avoid parent path conflicts

Recommended workflow:

```powershell
# 1) Dry run first
pwsh -File tools/scripts/dev_tools.ps1 -Command naming-audit

# 2) Apply in small batches for safer rollout
pwsh -File tools/scripts/dev_tools.ps1 -Command naming-apply -MaxChanges 100

# 3) Build and run tests after each batch

# Optional: run against all non-excluded paths (advanced)
pwsh -File tools/scripts/normalize_names.ps1 -Mode audit -IncludeRelativePaths @()
```

### tree.ps1

Generates a tree structure report of the project directory.

```powershell
pwsh -File tree.ps1
```

### k8s_deploy.ps1

Kubernetes-only deployment entrypoint used by `dev_tools.ps1`.

- Supports single-zone and multi-zone one-click open-server flows.
- Applies infra (`etcd`, `redis`, `kafka`) and game node workloads (`centre`, `gate`, `scene`) per namespace.
- Exposes `gate` through a per-zone Kubernetes Service and can wait for deployment readiness.
- Reads multi-zone definitions from `deploy/k8s/zones.json` or `deploy/k8s/zones.yaml` (or custom path).
- Do not assume `LoadBalancer` across all environments: managed cloud K8s usually prefers `LoadBalancer`, while self-hosted / bare metal K8s should usually prefer `NodePort` plus an external L4 load balancer.
- Prefer passing `-OpsProfile managed-cloud` or `-OpsProfile bare-metal` so the exposure choice is explicit in commands and change records.
- In `custom` mode, the default `-GateServiceType` baseline is `NodePort` (safer for clusters without mature LB support).

See `deploy/k8s/README.md` for full usage and behavior details.
Ops runbook: `docs/ops/k8s-open-server-runbook.md`.

### k8s_image.ps1

Kubernetes runtime image and release entrypoint used by `dev_tools.ps1`.

- Verifies staged Linux runtime files before image build.
- Builds and pushes the dedicated K8s runtime image from `deploy/k8s/Dockerfile.runtime`.
- Supports one-command release flows: build + push + deploy.

### k8s_stage_runtime.ps1

Stages Linux node binaries, `zoneinfo`, and generated tables into `deploy/k8s/runtime/linux` for the dedicated K8s runtime image.

## Usage Examples

### From Project Root

```powershell
# Generate all proto code
pwsh -File tools/scripts/dev_tools.ps1 -Command pbgen-run

# Generate project tree
pwsh -File tools/scripts/dev_tools.ps1 -Command tree
```

### From tools/scripts Directory

```powershell
# Build pbgen
pwsh -File dev_tools.ps1 -Command pbgen-build

# Run pbgen
pwsh -File dev_tools.ps1 -Command pbgen-run -ConfigPath ../proto_generator/pbgen/etc/proto_gen.yaml
```

## Notes

- All scripts are Windows PowerShell 5.1+ compatible
- Use `-Verbose` flag for detailed output
- Scripts are designed to be cross-platform compatible where possible
- For large repositories, prefer iterative rename batches with `-MaxChanges` to keep refactors reviewable and reduce breakage risk
