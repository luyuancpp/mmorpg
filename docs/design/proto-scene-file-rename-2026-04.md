# Proto Scene File Rename (2026-04)

## Rename Map
| Old name | New name | Why |
|----------|----------|-----|
| `game_client_player.proto` | `client_player_common.proto` | Was client-facing, `game_` prefix misleading |
| `game_player.proto` | `player_lifecycle.proto` | Handles login/exit lifecycle |
| `game_player_scene.proto` | `s2s_player_scene.proto` | Internal centre→scene transfer (s2s = server-to-server) |
| `game_scene.proto` | `scene_admin.proto` | Test/admin stub |
| `game_rpc.proto` | `rpc_message.proto` | Transport envelope, not gameplay |
| `comp.proto` | `base_comp.proto` | Was too generic (just Vector3) |
| `game_node_comp.proto` | `node_player_comp.proto` | Removed vague `game_` prefix |

## Corresponding C++ Handler Renames
- `game_client_player_handler.*` → `client_player_common_handler.*`
- `game_player_handler.*` → `player_lifecycle_handler.*`
- `game_player_scene_handler.*` → `s2s_player_scene_handler.*`
- `game_scene_handler.*` → `scene_admin_handler.*`
- Response handlers follow same pattern with `_response_handler` suffix

## Naming Convention (Established)
- **`client_*` / `player_*`**: Client-facing (C2S/S2C) feature handlers
- **`s2s_*`**: Server-to-server internal messages (centre→scene, gate→scene)
- **No prefix (scene.proto)**: Infrastructure backbone / node-to-node RPCs
- **`*_comp.proto`**: ECS component definitions
- **`base_*`**: Primitive/foundational types

## Post-rename: re-run proto-gen to regenerate
- `generated/proto/` outputs (C++ .pb.h/.pb.cc, Go .pb.go)
- `generated/robot/proto/` outputs
- `cpp/generated/rpc/service_metadata/` (metadata headers)
- `tools/generated/temp/` outputs
