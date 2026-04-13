# Gate Entity ID Truncation Fix (2026-04-04)

## Bug
`SessionInfo::NodeMap` stored entity IDs as `NodeId` (`uint32_t`), but `ENTT_ID_TYPE=uint64_t` makes `entt::entity` 64-bit. When entt recycles an entity index (after destroy+create), the version bits go into the upper 32 bits (e.g., entity index 2 at version 1 = `0x100000002` = `4294967298`). Storing this in `uint32_t` truncates to `2`, which is entity index 2 at version 0 — the **destroyed** entity. `registry.valid()` correctly rejects it.

## Symptom
- "Bound node is invalid. nodeType: 5" for ALL sessions
- Works on fresh gate start (entities at version 0 fit in 32 bits)
- Breaks after etcd purge + restart (entities recycled with version 1+)

## Fix
Changed `SessionInfo` to store entity IDs as `uint64_t`:
- `session_info_comp.h`: `NodeMap` → `NodeEntityMap<uint32_t, uint64_t>`, sentinel `kInvalidEntityId = UINT64_MAX`
- `client_message_processor.cpp`: `GetEffectiveNodeId` returns `uint64_t`, uses `SessionInfo::kInvalidEntityId`
- `gate_event_handler.cpp`: `ForwardLoginToScene` takes `uint64_t`, `BindSessionEventHandler` compares with `kInvalidEntityId`

## Key Design Lesson
- `NodeId` (uint32) = the node's protocol ID from etcd/proto (sequential: 1, 2, 3...)
- Entity ID (uint64) = entt registry handle with version bits
- These are different concepts; never conflate them in session storage
