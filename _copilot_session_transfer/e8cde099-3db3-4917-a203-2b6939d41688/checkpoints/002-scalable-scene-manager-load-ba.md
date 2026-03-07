<overview>
The user is refactoring a C++ MMORPG scene management system into a horizontally scalable Go-Zero service (`scene_manager`) backed by Redis. The goals include implementing idempotent scene transitions, supporting rapid scene switching (A->B->C) without client blocking ("Last Write Wins"), and adding load balancing for scene node selection. The current focus is bridging the new Go architecture with the legacy C++ game node logic to ensure data consistency during these transitions.
</overview>

<history>
1.  **Go Service Implementation**:
    *   Created `scene_manager` service structure with Redis configuration.
    *   Defined `proto/scene_manager/storage.proto` for Redis data structures (`SceneInfo`, `PlayerLocation`).
    *   Implemented `EnterSceneByCentre` using an idempotent, location-based approach (removing legacy "Pending" state machines) to support rapid switching.
    *   Implemented `LeaveSceneByCentre` for logging and cleanup.

2.  **Load Balancing Feature**:
    *   User requested a strategy to select the least loaded node for new scenes.
    *   Implemented `load_reporter.go`: A background task in Go that reports node load to a Redis ZSet (`scene_nodes:load`).
    *   Updated `CreateSceneLogic`: Now queries Redis for the node with the lowest score (load) if no specific target is requested.
    *   Updated `scene_manager_service.proto` to return the selected `node_id`.

3.  **C++ Integration Analysis**:
    *   User asked how to implement the C++ side of "Team Dragging" (Leader pulls team A->B->C) while ensuring data consistency (Memory -> Redis).
    *   Explored the C++ codebase (`cpp/nodes/scene/...`) to understand existing RPC handlers (`SceneHandler`, `SceneScenePlayerHandler`) and data flow.
</history>

<work_done>
**Files Updated (Go):**
-   `go/scene_manager/internal/logic/load_reporter.go`: Created. Implements load reporting (ZAdd) and best node selection (ZRange).
-   `go/scene_manager/internal/logic/createscenelogic.go`: Integrated `GetBestNode` for dynamic load balancing.
-   `go/scene_manager/internal/logic/enterscenebycentrelogic.go`: Implemented "Last Write Wins" entry logic.
-   `go/scene_manager/internal/logic/changesceneutil.go`: Cleaned up unused legacy state checks.
-   `proto/scene_manager/scene_manager_service.proto`: Updated `CreateScene` signature to return `node_id`.
-   `go/scene_manager/scenemanagerservice.go`: Added startup call for `StartLoadReporter`.

**Files Viewed (C++):**
-   `cpp/nodes/scene/handler/rpc/player/game_player_scene_handler.cpp`: Existing C++ logic for `EnterScene`/`LeaveScene`.
-   `cpp/nodes/scene/handler/rpc/scene_handler.cpp`: Node-level RPCs.

**Current State:**
-   Go `scene_manager` is fully implemented with Redis storage, idempotency, and basic load balancing.
-   C++ integration strategy needs to be defined for the "Team Dragging" scenario.
</work_done>

<technical_details>
-   **Redis Schema**:
    -   `player:{id}:location`: Protobuf `PlayerLocation` (Source of Truth).
    -   `scene:{id}:node`: String (NodeID mapping).
    -   `scene_nodes:load`: ZSet (Score = Load, Member = NodeID).
-   **Load Balancing**: Uses a "Self-Reporting" pattern where nodes push their own load metrics to Redis. The `CreateScene` logic simply picks the lowest score.
-   **Rapid Switching (A->B->C)**: The architecture assumes "Server Authority". If a player moves A->B->C quickly, the server updates the location A->B->C immediately. The client is responsible for discarding stale loading requests (e.g., ignoring B if C is already processing).
-   **C++ Legacy**: The C++ `SceneScenePlayerHandler` currently relies on `ChangeSceneInfo` logic which matches the legacy state machine we are replacing.
</technical_details>

<important_files>
-   `go/scene_manager/internal/logic/createscenelogic.go`: Handles scene creation distribution.
-   `go/scene_manager/internal/logic/load_reporter.go`: logic for calculating and reporting node pressure.
-   `go/scene_manager/internal/logic/enterscenebycentrelogic.go`: Core entry logic ensuring idempotency.
-   `cpp/nodes/scene/handler/rpc/player/game_player_scene_handler.cpp`: The C++ file that handles player scene transitions; this is where the new Redis data syncing logic must be injected.
</important_files>

<next_steps>
-   **Design C++ Data Sync**: Explain/Implement how the C++ `LeaveScene` should serialize player memory data to Protobuf and save to Redis.
-   **Implement C++ Enter Strategy**: Define how the C++ `EnterScene` reads this data from Redis to restore player state.
-   **Team Dragging**: Detail the flow for the "Leader pulls team" scenario to ensure followers skip intermediate scenes (B) if the leader reaches C efficiently.
</next_steps>