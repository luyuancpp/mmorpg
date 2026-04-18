# PROJECT KNOWLEDGE BASE

**Updated:** 2026-03-24
**Branch:** main

## OVERVIEW
Polyglot MMORPG backend. C++ carries runtime nodes and scene-domain logic; Go carries go-zero services; Java carries auth/web helpers; `proto/` is the contract source; `generated/` holds checked-in outputs; `deploy/k8s/` and `tools/` hold operational workflows.

## STRUCTURE
```text
mmorpg/
├── cpp/          # Runtime nodes, shared C++ services, tests
│   ├── nodes/    #   Node entrypoints (scene, gate, centre) and handlers
│   ├── libs/     #   Shared engine, ECS services, and game modules
│   ├── generated/#   Auto-generated proto/grpc/table outputs
│   └── tests/    #   C++ test suites (GTest)
├── go/           # Go microservices (go-zero based)
│   ├── login/    #   Player auth + node registration
│   ├── scene_manager/ # Scene allocation + load balancing
│   ├── data_service/  # Data layer RPC
│   ├── db/            # Kafka consumer + MySQL persistence
│   ├── player_locator/# Player location lookup
│   └── shared/        # Shared Go utilities (kafkautil, etc.)
├── java/         # Spring Boot services (auth + gateway/zone directory)
├── proto/        # Authoritative source contracts and service IDs
├── generated/    # Checked-in generated proto/table outputs
├── deploy/       # Docker Compose (dev) + Kubernetes (prod) manifests
├── tools/        # proto-gen, exporters, robot tooling, dev scripts
│   ├── scripts/  #   Central PowerShell dispatcher (dev_tools.ps1)
│   ├── proto_generator/ # Canonical proto-gen source (Go)
│   ├── archived/ #   Legacy scripts (retained for reference)
│   └── dev/      #   mprocs dashboard configs
├── docs/         # Ops/docs snapshots (design/, notes/, ops/)
├── third_party/  # Vendored C++ dependencies (source)
└── lib/          # Compiled library outputs (Windows .lib/.dll)
```

### Dead-weight directories (safe to delete)
| Directory | Contents | Why it's dead |
|-----------|----------|---------------|
| `cpp/libs/base/` | Only `.vcxproj.user` files | No source code; VS user settings only |
| `cpp/libs/game/` | Only `.vcxproj.user` files | Abandoned stub; `.gitignore` marks it as such |

### Cleaned up (2026-03-24)
The following were removed from git tracking:
- `bin/zoneinfo/` — 1800 IANA timezone files (available at runtime, not for VCS)
- `bin/start_server.*`, `bin/stop_server.*` — superseded by `dev_tools.ps1`
- `proto/player/player.proto` — empty 0-byte file
- `cpp/tests/draw_test/` — unrelated MFC GUI app
- `cpp/tests/zone_and_area_test/` — obsolete test referencing old namespaces
- `docs/design/ai_session_*.md` — AI chat logs, not design docs
- `docs/notes/questions.md` — ChatGPT transcript
- `tools/contracts/`, `tools/data_service/`, `tools/scene_manager/`, `tools/github.com/` — duplicate .pb.go outputs (canonical location: `go/`)
- `go/chat/` — stub directory with only generated files, no service code
- `cpp/libs/modules/game_logic.vcxproj.filters` — orphaned project file
- `bin/cpp_nodes.pid.json` — runtime state file

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Scene node startup | `cpp/nodes/scene/main.cpp` | Node bootstrap + Kafka consumer registration |
| Gate node startup | `cpp/nodes/gate/main.cpp` | Client TCP bridge + Kafka routing |
| C++ node main templates | `cpp/nodes/_template/` | Use `main.simple.cpp.example` or `main.with_context.cpp.example` |
| C++ gameplay/domain logic | `cpp/libs/services/scene/` | Systems, comps, world, actor/player logic |
| Go login service | `go/login/login.go` | etcd registration + grpc startup |
| Go scene manager | `go/scene_manager/scene_manager_service.go` | go-zero scene manager + load reporter |
| Go data service | `go/data_service/data_service.go` | go-zero RPC service pattern |
| Java gateway (zone directory) | `java/gateway_node/` | Server list, gate assignment, zone health probe, admin APIs |
| Proto contracts | `proto/` | Authoritative source; regenerate consumers after edits |
| Checked-in generated contracts | `generated/proto/` | Review only; do not hand-edit |
| Local/dev infra | `deploy/docker-compose.yml` | Kafka, Redis, MySQL, Nacos, etcd |
| Local Go service launcher | `tools/scripts/go_services.ps1` | start/stop/status/list all Go-zero services |
| Local C++ node launcher | `tools/scripts/cpp_nodes.ps1` | start/stop/status/list gate/scene nodes |
| Go service Docker images | `tools/scripts/go_svc_image.ps1` | build/push per-service Docker images |
| K8s release flow | `deploy/k8s/README.md` | Runtime image + zone lifecycle |
| Shared dev scripts | `tools/scripts/dev_tools.ps1` | proto-gen (pbgen), k8s, tree, naming audit/apply |
| Robot proto/handlers | `tools/robot/` | Proto defs + message handlers for load testing |
| Robot Go module | `robot/go.mod` | Standalone load client (one-client-one-goroutine rule) |
| C++ traffic statistics | `cpp/libs/engine/core/network/traffic_statistics.h/.cpp` | Per-message atomic send/recv counters, periodic reporter |
| Go gRPC traffic statistics | `go/shared/grpcstats/collector.go` | gRPC interceptor: per-method count, bytes, latency |
| Traffic stats design doc | `docs/design/traffic-statistics-design.md` | Design rationale + production safety analysis |

## CONVENTIONS
- Prefer verb-based, thin RPC handlers. Heavy logic belongs in systems/services, not transport wrappers.
- `proto/` is the edit target. `generated/` and language-local generated `proto/` trees are outputs.
- In generated-style C++ files with `///<<< BEGIN WRITING YOUR CODE`, keep custom logic inside guarded regions.
- Use **Scene Node** naming; do not introduce fresh **Game Node** terminology for the same role.
- Gate/scene control routing is Kafka-based now. Trace request flow through topic wiring, not just direct RPC.
- New C++ node entrypoints should start from `cpp/nodes/_template/README.md` and reuse `node::entry::RunSimpleNodeMain...` helpers.
- Go services follow go-zero / `zrpc.MustNewServer` bootstrap and enable grpc reflection only in dev/test modes.
- K8s exposure is environment-specific: managed cloud prefers `LoadBalancer`; bare metal prefers `NodePort` + external L4.
- `tools/scripts/dev_tools.ps1` is the preferred shell entrypoint for proto-gen (pbgen), tree, naming, and k8s operations.

### C++ naming standards
- ECS components use `*Comp` suffix (e.g. `MissionsComp`, `SceneEntityComp`). Never `*Component`.
- Type aliases live in `type_define/type_define.h` for shared types; service-specific aliases stay in their service's `type_alias/` dir.
- Use inline getter functions instead of `#define` macros for global ECS accessors (e.g. `GetActorCreateMessage()` not `actorCreateMessage`).
- SnowFlake: single production implementation at `utils/id/snow_flake.h` (kNodeBits=17, kStepBits=15, epoch=1773446400).
- Disambiguate same-name types across services: prefix with service name (e.g. `CentrePlayerDataRedis` vs `PlayerDataRedis`, `GateSessionMap` vs `SessionList`).

### Kafka zombie message protection (`target_instance_id`)
- Topics use `{node_type}-{node_id}` naming (e.g. `gate-1001`, `scene-2001`). Node IDs can be reused after restart.
- All `*_command.proto` contracts include a `target_instance_id` field. C++ consumer validates it in `node_kafka_command_handler.h` → `ValidateCommandTarget()` — messages with mismatched instance UUID are silently dropped.
- **Code review rule**: Any Go/C++ producer sending to a `{type}-{id}` Kafka topic **MUST** populate `target_instance_id` with the target node's UUID. Omitting it disables zombie filtering and risks delivering stale commands to a restarted node.
- **New node types**: If adding a new `*_command.proto`, it **MUST** include a `target_instance_id` string field. Consumer-side validation is automatic via shared `RegisterKafkaCommandHandler<T>`.
- Three-layer defense: (1) `target_instance_id` validation, (2) 60s topic retention, (3) node ID reuse cooldown (etcd lease 60s + SnowFlake guard 600s).

### Go Kafka config standard
- All services use `Brokers []string` (not comma-separated `string`).
- login and db use IBM/sarama (ordered, transactional).
- scene_manager and player_locator use segmentio/kafka-go (simple fire-and-forget).
- Shared Kafka utilities (expand status, partition helpers) live in `go/shared/kafkautil/`; login and db import via `replace shared => ../shared`.
- Shared gRPC traffic stats interceptor lives in `go/shared/grpcstats/`; all Go services import via `replace shared => ../shared`.

### Traffic statistics
- C++: `TrafficStatsCollector` in `cpp/libs/engine/core/network/traffic_statistics.h/.cpp`. Registered automatically in `Node` constructor via `RegisterTrafficStatsReporter()`.
- Go: `grpcstats.Collector` in `go/shared/grpcstats/collector.go`. Wired as `UnaryServerInterceptor` in all Go service entry points.
- Toggle C++: env `NODE_TRAFFIC_STATS_ENABLED=1`, optional `NODE_TRAFFIC_STATS_AUTO_DISABLE_MINUTES`, `NODE_TRAFFIC_STATS_INTERVAL_SECONDS`.
- Toggle Go: env `GRPC_TRAFFIC_STATS_ENABLED=1`, optional `GRPC_TRAFFIC_STATS_AUTO_DISABLE_MINUTES`, `GRPC_TRAFFIC_STATS_INTERVAL_SECONDS`.
- Safe for temporary production use: atomic-only counters, periodic summary logging (not per-message).

### gRPC server thread pool (C++ nodes)
- C++ nodes embed a gRPC sync server for control-plane RPCs (CreateScene, DestroyScene, etc.).
- All RPC handlers dispatch to the muduo EventLoop via `runInLoop` + `promise/future`; business logic stays single-threaded.
- Toggle: env `GRPC_SERVER_MAX_POLLERS` (default `2`). Controls the max gRPC server poller threads. 1-2 is enough for control-plane operations.
- Multiple gRPC services (scene, mail, guild, etc.) share the same port and thread pool.

### Proto source conventions
- All source `.proto` files in `proto/` should include `option go_package = "{service}/proto/{subdir}";` for documentation clarity.
- Proto-gen tool rewrites `go_package` in generated copies under `generated/proto/`; source values serve as documentation.
- Proto message suffix should standardize on `PBComp` or `Comp` (current mix: `PBComponent`, `PbComponent`, `Comp`, `PBComp` — cleanup planned).

### C++ Node Main PR Checklist (Summary)
- Start from `cpp/nodes/_template/main.simple.cpp.example` or `cpp/nodes/_template/main.with_context.cpp.example`.
- Keep startup logic thin; do not move gameplay/business logic into `main.cpp`.
- Ensure node main uses `node::entry::RunSimpleNodeMain...` helpers.
- Ensure Kafka topic/group and routing fields match the node contract.
- Ensure thread observability is active by default (or intentionally disabled via `NODE_THREAD_MONITOR_ENABLED=0`).
- Do not hand-edit generated outputs while wiring new node startup.

## ANTI-PATTERNS (THIS PROJECT)
- Hand-editing files under `generated/` or generated `*.pb.go` / generated proto output trees.
- Expanding transport handlers with gameplay/business logic instead of delegating into services/systems.
- Editing scaffolded C++ code outside `BEGIN WRITING YOUR CODE` blocks without deliberate framework work.
- Assuming gate/scene communication is still direct stream-based; current architecture routes control messages via Kafka.
- Recommending `LoadBalancer` as a universal K8s default.
- Treating root `Dockerfile` as the production K8s runtime image.
- Routine changes under `third_party/`.
- Renaming large source trees without first using naming audit/apply workflow.
- Using `#define` macros for global ECS singleton accessors (use inline functions instead).
- Defining the same type alias name in different services without disambiguation prefix.
- Using `string` for Kafka Brokers config (use `[]string` consistently across all Go services).
- Keeping dead SnowFlake implementations or duplicate `Guid` typedefs in headers (consolidate to `type_define.h`).
- Sending Kafka messages to `{type}-{id}` topics without populating `target_instance_id` — risks delivering stale commands to a restarted node with a recycled node_id.

## UNIQUE STYLES
- Repo keeps generated outputs checked in.
- C++ runtime code is split between node-facing adapters (`cpp/nodes/*`) and deeper reusable scene services (`cpp/libs/services/scene/*`).
- Go services are separate mini-roots, not a single monolith module.
- `tools/proto/protogen` is retained for compatibility, but `tools/proto_generator/protogen` is the canonical source project for proto-gen (historical name: pbgen).
- Robot tooling explicitly enforces one client per goroutine.

## COMMANDS
```bash
# C++
msbuild game.sln /m /p:Configuration=Debug /p:Platform=x64
cd cpp\nodes\scene && cmake -S . -B build && cmake --build build --config Debug
clang-tidy <file.cpp> --config-file=cpp\.clang-tidy

# Go
cd go && build.bat
cd go\login && go run login.go
cd go\login && go test ./...

# Java
cd java\gateway_node && mvn clean install
cd java\gateway_node && mvn test

# Tools / proto generation
pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build
pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run

# K8s
# Build all images before deploying
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-build-all
# Deploy shared infrastructure (etcd/redis/kafka/mysql → mmorpg-infra namespace)
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-infra-up
# Deploy a single zone (game services only → mmorpg-zone-{name} namespace)
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-up -ZoneName yesterday -ZoneId 101 -OpsProfile managed-cloud -NodeImage <image> -WaitReady
# Deploy all zones (infra + all zones)
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-all-up -ZonesConfigPath deploy/k8s/zones.ops-recommended.yaml -OpsProfile managed-cloud -NodeImage <image> -WaitReady

# Go services (local dev)
pwsh -File tools/scripts/dev_tools.ps1 -Command go-svc-build
pwsh -File tools/scripts/dev_tools.ps1 -Command go-svc-build -GoServices login,db
pwsh -File tools/scripts/dev_tools.ps1 -Command go-svc-start
pwsh -File tools/scripts/dev_tools.ps1 -Command go-svc-stop
pwsh -File tools/scripts/dev_tools.ps1 -Command go-svc-status
pwsh -File tools/scripts/dev_tools.ps1 -Command go-svc-list

# Go service Docker images
pwsh -File tools/scripts/dev_tools.ps1 -Command go-svc-build-images -GoSvcRegistry ghcr.io/luyuancpp -GoSvcTag v1
pwsh -File tools/scripts/dev_tools.ps1 -Command go-svc-push-images -GoSvcRegistry ghcr.io/luyuancpp -GoSvcTag v1

# C++ nodes (local dev)
pwsh -File tools/scripts/dev_tools.ps1 -Command cpp-node-start [-GateCount 2] [-SceneCount 4]
pwsh -File tools/scripts/dev_tools.ps1 -Command cpp-node-stop
pwsh -File tools/scripts/dev_tools.ps1 -Command cpp-node-status

# Unified dev (C++ nodes + Go services)
pwsh -File tools/scripts/dev_tools.ps1 -Command dev-start [-GateCount 2] [-SceneCount 4]
pwsh -File tools/scripts/dev_tools.ps1 -Command dev-stop
pwsh -File tools/scripts/dev_tools.ps1 -Command dev-status
```

## NOTES
- `.github/workflows/cr.yml` runs an OpenAI-backed code-review action and requires `OPENAI_API_KEY`.
- `go/build.bat` regenerates Go-side rpc/proto outputs for db/login/data-service-related flows.
- `deploy/docker-compose.yml` is for local/dev-style infra; `deploy/k8s/` is a separate production-style path.
- K8s infra manifests include etcd, redis, kafka, and mysql (per-zone namespace).
- `k8s-zone-down` deletes the entire namespace; treat it as destructive teardown.
- For cross-language changes: edit `proto/` first, regenerate, then rebuild the consuming C++/Go/Java services.
