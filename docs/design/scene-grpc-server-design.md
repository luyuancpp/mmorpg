# Scene Node gRPC Server Design

**Date:** 2026-04-15
**Status:** Planned

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

## Implementation Plan

### 1. Proto: Add gRPC Endpoint to NodeInfo

**File:** `proto/common/base/common.proto`

Add `grpc_endpoint` field to `NodeInfo`:
```protobuf
message NodeInfo {
    // ... existing fields ...
    Endpoint grpc_endpoint = N;  // gRPC endpoint (for Go services)
}
```

### 2. C++ Scene Node: Add gRPC Server

**File:** `cpp/libs/engine/core/node/system/node/node.h/.cpp`

- Add optional gRPC server to `Node` base class
- gRPC port = TCP port + 1 (by convention, overridable by env)
- gRPC server runs on a separate thread pool (standard `grpc::ServerBuilder`)
- Register proto `Scene` service implementation

**File:** `cpp/nodes/scene/main.cpp`

- Enable gRPC server in Scene node startup
- Register Scene gRPC service handlers (CreateScene, DestroyScene, etc.)

### 3. etcd Registration: Publish Both Endpoints

**File:** `cpp/libs/engine/core/node/system/node/node.cpp`

- Populate `grpc_endpoint` in NodeInfo before etcd registration
- Single etcd key contains both TCP and gRPC endpoints

### 4. Go scene_manager: Use gRPC Endpoint

**File:** `go/scene_manager/internal/logic/scene_node_client.go`

- Parse `grpc_endpoint` from etcd NodeInfo JSON
- Dial using `grpc_endpoint` instead of `endpoint`
- No change to RPC call logic (already using Scene proto stubs)

**File:** `go/scene_manager/internal/logic/load_reporter.go`

- Parse `grpc_endpoint` from etcd watch events

## Cross-Server Implications

- Any zone's scene_manager can discover remote Scene nodes from shared etcd
- Direct gRPC dial to remote Scene nodes for cross-server dungeon creation
- No Kafka topic routing or cross-zone stream management needed
- Same `CreateScene` RPC works for local and cross-zone requests

## Files Affected

| File | Change |
|------|--------|
| `proto/common/base/common.proto` | Add `grpc_endpoint` to NodeInfo |
| `cpp/libs/engine/core/node/system/node/node.h` | Optional gRPC server support |
| `cpp/libs/engine/core/node/system/node/node.cpp` | gRPC server init, etcd registration |
| `cpp/nodes/scene/main.cpp` | Enable gRPC server, register service |
| `go/scene_manager/internal/logic/scene_node_client.go` | Use grpc_endpoint |
| `go/scene_manager/internal/logic/load_reporter.go` | Parse grpc_endpoint |

## Related

- Gate codec race fix (2026-04-15): moved Gate TcpServer callbacks before WaitAndRun to prevent `kUnknownMessageType` on early client connections.
- Scene creation architecture: `docs/design/scene-creation-architecture.md` (if exists), or see Go scene_manager source.
