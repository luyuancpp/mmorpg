# GRPC Node Entity ID Collision (Fixed 2026-04-02)

## Problem
`ConnectToGrpcNode` used `entt::entity{node_id}` as entity ID. GRPC nodes (login) across zones share small node_ids (1, 2, 3) from per-zone SnowFlake allocation. When gate discovers all zones' login nodes via shared etcd, entities overwrite each other — only the last writer's zone_id survives, causing `PickRandomNode(nodeType)` to find no matching entities (zone_id filter fails).

## Root Cause
- `entt::entity{node_id}` is only unique within a zone, not globally
- 10 zones × 2 login nodes each → 20 nodes with only 3 distinct node_ids → 17 entity overwrites
- Gate log showed 17 WARN "GRPC node id reused by new uuid" messages

## Fix
- `ConnectToGrpcNode` (`node_connector.cpp`): Use `registry.create()` (auto-generated entity IDs) + uuid-based dedup scan
- `HandleServiceNodeStop` (`node.cpp`): For GRPC nodes, scan by uuid to find entity to destroy (since entity ID ≠ node_id)
- TCP nodes still use `entt::entity{node_id}` (they have globally unique IDs from C++ SnowFlake)

## Files Changed
- `cpp/libs/engine/core/node/system/node/node_connector.cpp` — ConnectToGrpcNode
- `cpp/libs/engine/core/node/system/node/node.cpp` — HandleServiceNodeStop
