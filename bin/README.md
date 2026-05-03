# `bin/` — C++ Runtime Working Directory

`bin/` hosts the C++ executables and the runtime working directory the C++ nodes
expect when launched locally on Windows. It is **not** a pure build-output dir:
the C++ exes read `bin/etc/`, `bin/nodes/`, `bin/script/`, and look up tables
relative to this folder.

## What lives here

| Subpath | Purpose | Tracked in git |
|---------|---------|-----------|
| `*.exe`, `*.lib`, `*.exp`, `*.pdb` | MSVC build output for nodes and tests | no (build artifacts) |
| `etc/` | C++ node configs (`base_deploy_config.yaml`, etc.) | yes |
| `nodes/` | Per-node working data | mixed |
| `script/` | Helper scripts shipped alongside binaries | yes |
| `go_services/` | Built Go service binaries (so `cwd=bin` runs everything) | no |
| `vsout/` | MSVC intermediate object files | no |
| `Debug/` | Legacy MSBuild Debug output | no |
| `zoneinfo/` | Optional IANA tz data for Linux runtime image | no |

## What used to live here but moved

Everything **transient** now lives under `run/` (separate from build outputs):

| Old path | New path |
|----------|----------|
| `bin/logs/go_services/` | `run/logs/go_services/` |
| `bin/logs/cpp_nodes/`   | `run/logs/cpp_nodes/` |
| `bin/logs/sa_token.log` | `run/logs/sa_token.log` |
| `bin/robot_logs/`       | `run/logs/robot/` |
| `bin/go_services.pid.json` | `run/pids/go_services.pid.json` |
| `bin/cpp_nodes.pid.json`   | `run/pids/cpp_nodes.pid.json` |
| `bin/sa_token.pid`         | `run/pids/sa_token.pid` |
| `bin/XMLGoogleTestResults.xml`, `bin/*.testdurations`, `bin/z*_stats.txt`, `bin/redis_*.txt`, `bin/go_*.txt` | `run/scratch/` |

The launcher scripts (`tools/scripts/go_services.ps1`,
`tools/scripts/cpp_nodes.ps1`, `dev.bat`) still read the legacy pid file
locations once for backwards compatibility, then rewrite them under `run/`.

## Future work (not done yet)

Fully separating **build outputs** from the **runtime working directory** would
also require:

1. Changing the MSVC `.vcxproj` `OutDir`/`IntDir` for ~50+ projects so test
   binaries (`*_test.exe`, `*.exp`, `*.lib`, `*.pdb`) land in `build/` instead
   of `bin/`.
2. Updating `deploy/k8s/Dockerfile.cpp`, `deploy/k8s/Dockerfile.runtime`, and
   the K8s ConfigMap mount paths (`/app/bin/etc`, `/app/bin/logs`) to a new
   layout — coordinated change needed across runtime image and ConfigMaps.
3. Updating C++ source code that opens config files relative to the working
   directory (currently assumes `cwd=bin`).

These are intentionally deferred because they touch the build/deploy contract
and need verification on Windows + Linux. The runtime/scratch separation
captured above already gets the main benefits: clean `git status`, safe
`rm -rf run/`, and clear distinction between artifacts that survive a build
clean and artifacts that don't.
