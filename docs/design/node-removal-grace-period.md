# Node Removal Grace Period (Consumer-side)

## Problem
Breakpoint debugging pauses keepalive → etcd DELETE fires after TTL (60s) → other nodes immediately remove the debugged node → connections torn down → debugging session ruined.

## Solution: Consumer-side Grace Period
Industry standard (Google/Alibaba/Netflix): etcd DELETE timing unchanged; **observers defer removal** with a cancellable timer.

### Config
- **Proto**: `BaseDeployConfig.node_removal_grace_seconds` (field 13) in `proto/common/base/config.proto`
- **YAML**: `Etcd.NodeRemovalGraceSeconds` in `bin/etc/base_deploy_config.yaml` (dev: 300, prod: 0)
- **Go**: `NodeRemovalGraceSeconds` in `go/scene_manager/etc/scene_manager_service.yaml`

### C++ Implementation
- `Node::HandleServiceNodeStop()` — grace > 0: creates `PendingNodeRemoval` with `RunAfter` timer; grace == 0: immediate removal (backward compatible)
- `Node::CancelPendingNodeRemoval(uuid)` — called from `ServiceDiscoveryManager::AddServiceNode()` when a PUT arrives, cancels the pending timer
- `Node::ExecuteNodeRemoval()` — extracted actual removal logic, called by both paths
- `PendingNodeRemoval` struct in `node.h`: holds `TimerTaskComp` + `NodeInfo`
- `pendingNodeRemovals_` map: `unordered_map<string(uuid), unique_ptr<PendingNodeRemoval>>`

### Go Implementation (scene_manager)
- `missingSince` map in `load_reporter.go`: tracks when a node was first observed missing
- `syncSceneNodes()`: only removes from Redis ZSet after grace period expired; node reappearing clears the entry

### Safety: Node ID Reuse
All matching is by **uuid**, not node_id:
- Node A (uuid-A) paused → DELETE → pending removal for uuid-A
- Node B (uuid-B) takes same node_id → PUT(uuid-B) → `CancelPendingNodeRemoval(uuid-B)` finds nothing (uuid-A != uuid-B)
- Grace timer expires → `ExecuteNodeRemoval(uuid-A)` → only removes uuid-A snapshots
- `DestroyEntity` has existing uuid guard: if entity's current uuid != stale uuid, skip

### Flow
```
Breakpoint pause → keepalive stops → 60s → etcd DELETE
                                              ↓
                              Observer receives DELETE event
                                              ↓
                              grace > 0? → Start 300s timer, do NOT remove yet
                                              ↓
                              Breakpoint resumes → node re-registers → PUT event
                                              ↓
                              CancelPendingNodeRemoval() → cancel timer → everything normal
```

### Environment Config
| Environment | `NodeRemovalGraceSeconds` | Effect |
|-------------|--------------------------|--------|
| Dev/Debug   | 300                      | 5-minute window survives breakpoints |
| Production  | 0                        | Identical behavior to before (immediate removal) |

### Files Changed
- `proto/common/base/config.proto`
- `bin/etc/base_deploy_config.yaml`
- `cpp/libs/engine/config/config.cpp`
- `cpp/libs/engine/core/node/system/node/node.h`
- `cpp/libs/engine/core/node/system/node/node.cpp`
- `cpp/libs/engine/core/node/system/discovery/service_discovery_manager.cpp`
- `go/scene_manager/internal/config/config.go`
- `go/scene_manager/internal/logic/load_reporter.go`
- `go/scene_manager/etc/scene_manager_service.yaml`
