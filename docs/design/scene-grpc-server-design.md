# Scene Node gRPC Server Design

**Date:** 2026-04-15
**Status:** Implemented

## Problem

Go `scene_manager` connects to C++ Scene Node via `grpc.NewClient()`, but Scene Node only exposes a TCP port with RPC0 (ProtobufCodecLite) codec. The gRPC HTTP/2 connection preface (`PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n`) is misinterpreted by the RPC0 codec as a 1.3-billion-byte message length, triggering `kInvalidLength` errors. The scene_manager retries every few seconds, flooding Scene logs.

```
ERROR ProtobufCodecLite::defaultErrorCallback - InvalidLength
  peer: 172.27.16.1:57211 local: 172.27.16.1:20000
```

### Root Cause

| Component | Behavior |
|-----------|----------|
| C++ Scene Node | Registers to etcd with `protocol_type = PROTOCOL_TCP`, exposes TCP(RPC0) port |
| Go scene_manager | Reads endpoint from etcd, dials with `grpc.NewClient()` regardless of `protocol_type` |
| Wire format mismatch | RPC0 = `[len:4]["RPC0":4][payload][checksum:4]`; gRPC = HTTP/2 framing |

## Decision: Dual-Port Architecture

Scene Node exposes **two ports**:

```
Scene Node (C++)
├── TCP  :N     ← Gate node (RPC0, node-to-node, ProtobufCodecLite)
└── gRPC :N+1   ← Go services (scene_manager, future cross-server services)
```

### Why scene_manager Connects TO Scene (Not the Reverse)

**Principle: whoever issues commands is the client.**

| System | Orchestrator → Worker | Pattern |
|--------|----------------------|---------|
| Kubernetes | API Server → kubelet | Master dials worker |
| Agones | Allocator → GameServer | Allocator dials game server |
| This project | scene_manager → Scene Node | Orchestrator dials executor |

Scene does not need to RPC-call scene_manager:
- Load reporting already uses Redis/etcd metadata
- Player enters scene via Gate → scene_manager gRPC, not Scene → scene_manager

### Why Not Bidirectional Streaming

| Aspect | Unary gRPC | Bidirectional Stream |
|--------|-----------|---------------------|
| Complexity | Simple request-response | Heartbeat, reconnect, stream multiplex |
| Connection management | gRPC handles it | Manual stream-to-node mapping |
| Cross-server | Any zone dials any scene directly | Need cross-zone stream management |
| Error handling | gRPC status codes, immediate | Async, slow failure detection |

### Why Not Kafka for Scene Commands

| Aspect | gRPC | Kafka |
|--------|------|-------|
| CreateScene/DestroyScene | Request-response, immediate result | Needs reply topic + correlation ID |
| Latency | Sub-millisecond | Consumer poll interval (~100ms) |
| Error handling | Status codes, deadline | Async, no built-in reply |
| Appropriate for | Commands needing response | Fire-and-forget notifications |

Kafka remains appropriate for Gate's `RoutePlayer`/`KickPlayer` (notifications, no reply needed).

## Architecture

```
                                    ┌─────────────────────────┐
                                    │    Scene Node (C++)     │
                                    │                         │
  Gate Node ──── TCP (RPC0) ──────► │  TCP  :N   (RPC0)      │
                                    │                         │
  scene_manager ── gRPC ──────────► │  gRPC :N+1 (HTTP/2)    │
                                    │                         │
  cross-zone    ── gRPC ──────────► │  (same gRPC port)       │
  scene_manager                     └─────────────────────────┘
```

## Implementation (Completed)

### 1. Proto: Added gRPC Endpoint to NodeInfo

**File:** `proto/common/base/common.proto`

```protobuf
message NodeInfo {
    // ... existing fields 1-9 ...
    EndpointComp grpc_endpoint = 10; // gRPC server endpoint (for Go services to connect)
}
```

### 2. Proto codegen: Enabled gRPC C++ stubs for scene domain

**File:** `tools/proto_generator/protogen/etc/proto_gen.yaml`

Changed scene domain `rpc.type` from `rpc` to `both`. The `both` type generates gRPC `.grpc.pb.h/.grpc.pb.cc` stubs while preserving muduo RPC handler generation (which checks `IsGRPC()` -> returns false for `both`).

**File:** `tools/proto_generator/protogen/internal/generator/cpp/gen.go`

Added `both` to the allowed types for gRPC C++ stub generation.

### 3. C++ Node base class: Added optional gRPC server

**File:** `cpp/libs/engine/core/node/system/node/node.h`

- `RegisterGrpcService(grpc::Service*)` — register before `StartRpcServer()`
- `StartGrpcServer()` / `ShutdownGrpcServer()` — lifecycle methods
- `grpcServer_` (unique_ptr), `grpcServerThread_` (std::thread), `grpcServices_` (vector)

**File:** `cpp/libs/engine/core/node/system/node/node.cpp`

- `StartGrpcServer()` called in `StartRpcServer()` after TCP server, only if services registered
- `ShutdownGrpcServer()` called first in `ShutdownInLoop()`
- gRPC server runs on its own thread (`grpcServer_->Wait()`)
- Banner updated to include gRPC endpoint

### 4. C++ Node allocator: gRPC port allocation

**File:** `cpp/libs/engine/core/node/system/node/node_allocator.cpp`

After TCP port allocation, if node has gRPC services, allocates gRPC port = TCP port + 1 (with availability check and fallback scan). Sets `grpc_endpoint` in NodeInfo, which is then published to etcd as JSON.

### 5. C++ Scene Node: gRPC service implementation

**File:** `cpp/nodes/scene/handler/grpc/scene_grpc_service.h/.cpp`

Implements `Scene::Service` from generated `scene.grpc.pb.h`:
- `CreateScene` — dispatches to muduo event loop via promise/future for thread safety
- `DestroyScene` — same dispatch pattern
- Idempotent, matches existing muduo handler logic

**File:** `cpp/nodes/scene/main.cpp`

Registers `SceneGrpcServiceImpl` via `node.RegisterGrpcService()` before startup.
Changed from `RunSimpleNodeMainWithOwnedContext` to `RunNodeMain` to support constructing `SceneRuntimeContext` with `EventLoop*`.

### 6. Go scene_manager: Uses gRPC endpoint

**File:** `go/scene_manager/internal/logic/scene_node_client.go`

Updated `resolveNodeEndpoint()` to prefer `grpcEndpoint` from etcd JSON, with fallback to `endpoint` for backward compatibility.

**File:** `go/scene_manager/internal/logic/load_reporter.go`

Added `GrpcEndpoint` field to `sceneNodeRegistration` struct.

## Cross-Server Implications

- Any zone's scene_manager can discover remote Scene nodes from shared etcd
- Direct gRPC dial to remote Scene nodes for cross-server dungeon creation
- No Kafka topic routing or cross-zone stream management needed
- Same `CreateScene` RPC works for local and cross-zone requests

## Files Changed

| File | Change |
|------|--------|
| `proto/common/base/common.proto` | Added `grpc_endpoint` field 10 to NodeInfo |
| `tools/proto_generator/protogen/etc/proto_gen.yaml` | Scene domain `rpc.type: rpc` -> `both` |
| `tools/proto_generator/protogen/internal/generator/cpp/gen.go` | Added `both` to gRPC stub generation check |
| `cpp/libs/engine/core/node/system/node/node.h` | gRPC server members and methods |
| `cpp/libs/engine/core/node/system/node/node.cpp` | gRPC server lifecycle, banner update |
| `cpp/libs/engine/core/node/system/node/node_allocator.cpp` | gRPC port allocation |
| `cpp/nodes/scene/handler/grpc/scene_grpc_service.h` | **New** — gRPC service header |
| `cpp/nodes/scene/handler/grpc/scene_grpc_service.cpp` | **New** — gRPC service implementation |
| `cpp/nodes/scene/main.cpp` | Register gRPC service, use RunNodeMain |
| `go/scene_manager/internal/logic/scene_node_client.go` | Use grpcEndpoint |
| `go/scene_manager/internal/logic/load_reporter.go` | Parse grpcEndpoint |

## Multi-Node Multi-Zone: No Extra Mapping Needed

Each Scene node registers its own gRPC endpoint in etcd. No additional port mapping or service mesh is required.

```
etcd keys (example):
  SceneNodeService.rpc/zone/1/node_type/3/node_id/1 → {ip: x.x.x.x, port: 20000, grpc_port: 20001, zone_id: 1}
  SceneNodeService.rpc/zone/1/node_type/3/node_id/2 → {ip: x.x.x.x, port: 20002, grpc_port: 20003, zone_id: 1}
  SceneNodeService.rpc/zone/2/node_type/3/node_id/1 → {ip: y.y.y.y, port: 20000, grpc_port: 20001, zone_id: 2}
```

| Environment | Extra mapping? | Reason |
|-------------|---------------|--------|
| Local dev | No | Go services in Docker reach host IP directly |
| K8s | No | Each Pod has unique IP, no port conflict |
| Multi-zone | No | Each zone's Scene nodes register independently, scene_manager filters by zone |

- gRPC port = TCP port + 1 (convention, unique per node via etcd CAS port allocation).
- scene_manager's existing per-nodeId connection cache handles multi-node/multi-zone.
- gRPC port is **internal cluster communication only** — no LoadBalancer/NodePort exposure needed.

## Connection Scale / Cost Assessment

This design does **not** recreate the old full-mesh explosion problem.

| Dimension | Reality |
|-----------|---------|
| Who dials Scene gRPC | Mainly scene_manager, plus occasional cross-zone scene_manager |
| Per-Scene steady-state connections | Usually single-digit to low double-digit |
| Per-request connection creation | No — connections are cached and reused |
| Concurrency model | HTTP/2 multiplexing over a small number of TCP connections |
| High-fanout traffic | Still stays on Kafka or existing TCP(RPC0), not this gRPC path |

So the cost is acceptable because this path is for **low-caller-count, request/response orchestration**, not for Gate-scale fanout.

## Related

- Gate codec race fix (2026-04-15): moved Gate TcpServer callbacks before WaitAndRun to prevent `kUnknownMessageType` on early client connections.
- Scene creation architecture: `docs/design/scene-creation-architecture.md` (if exists), or see Go scene_manager source.
