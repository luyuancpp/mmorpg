<overview>
The user is refactoring a legacy C++ MMORPG scene system into a stateless, horizontally scalable architecture using Go-Zero and Redis. The core objective is to replace stateful C++ memory management with a "Redis as Source of Truth" model, enabling features like idempotent scene entry, load balancing, and "Team Dragging" (where a leader pulls the team across scenes). The decision was made to implement a **"No-Skip" (Strict Sequential)** transition flow (A -> B -> C) to guarantee data consistency and simplify the "In-Flight" state handling.
</overview>

<history>
1.  **Go-Zero Implementation**: Established the `scene_manager` service, defined Redis schemas for player location, and implemented idempotent `EnterScene`/`LeaveScene` logic. Added a load reporter to balance player distribution across nodes.
2.  **C++ Refactoring (Phase 1)**:
    *   Analyzed `game_player_scene_handler.cpp` and `player_lifecycle.cpp`.
    *   Removed legacy `ChangeSceneInfo` state machines (pending/waiting states) which were blocking rapid transitions.
    *   Refactored `LeaveScene` to immediately serialize player data to Redis and disconnect ("Save & Exit").
    *   Refactored `PlayerEnterGameNode` (Node Entry) to asynchronously load player data from Redis if not found locally.
3.  **Architecture Discussion**:
    *   Addressed the "In-Flight" problem (Team moves A->B->C, member is stuck loading B).
    *   Debated "Skip" vs "No-Skip" strategies.
    *   **Decision**: Adhere to a **Strict Sequential (No-Skip)** flow. Even if the leader is already at C, a member at A must fully load into B, check the leader's position, and then transition B -> C. This ensures data atomicity and reuses existing logic.
4.  **C++ Refactoring (Phase 2)**:
    *   Confirmed Gate and Centre roles: Gate is a stateless router; Centre commands routing updates.
    *   Verified C++ async architecture: Redis and RPCs must be non-blocking.
    *   Started implementing the "Auto-Follow" check in `PlayerSceneSystem`.
</history>

<work_done>
**Files Updated:**
-   `cpp/nodes/scene/handler/rpc/player/game_player_scene_handler.cpp`: Refactored `LeaveScene` to use `PlayerLifecycleSystem::HandleExitGameNode` (stateless exit). Simplified `EnterScene`.
-   `cpp/libs/services/scene/player/system/player_lifecycle.cpp` & `.h`: Updated `HandlePlayerAsyncLoaded` to support `PlayerSceneEnterContext` for correct scene entry after Redis load.
-   `cpp/nodes/scene/handler/rpc/scene_handler.cpp`: Updated `PlayerEnterGameNode` to trigger async Redis load.
-   `cpp/libs/services/scene/player/system/player_scene.cpp`: Added a `TODO` placeholder for the "Team Auto-Follow" logic inside `HandleEnterScene`.

**Current State:**
-   Go-Zero service is ready.
-   C++ Node Entry/Exit logic is now stateless and Redis-backed.
-   The "Team Dragging" logic is partially implemented; the specific check to redirect a player (B->C) upon entry needs to be written.
</work_done>

<technical_details>
-   **Source of Truth**: Redis `player:{id}:location` holds the authoritative SceneID and NodeID.
-   **No-Skip Flow**: Transitions are atomic. A->B->C implies `Save(A) -> Load(B) -> Logic(B) -> Save(B) -> Load(C)`.
-   **Async Requirement**: C++ GameNodes must use `MessageAsyncClient` for Redis and async RPCs for Gate communication to prevent blocking the main loop.
-   **Gate Scalability**: The "No-Skip" approach keeps the Gate simple (just routing), allowing it to scale linearly with connections.
</technical_details>

<important_files>
-   `cpp/nodes/scene/handler/rpc/player/game_player_scene_handler.cpp`: Core handler for scene entry/exit RPCs.
-   `cpp/libs/services/scene/player/system/player_lifecycle.cpp`: Manages the lifecycle (Init, Load, Save, Destroy) of the player entity.
-   `cpp/libs/services/scene/player/system/player_scene.cpp`: Contains `HandleEnterScene`, the strategic point for injecting the "Check Leader & Redirect" logic.
-   `go/scene_manager/internal/logic/enterscenebycentrelogic.go`: The Go counterpart ensuring global idempotency.
</important_files>

<next_steps>
-   **Implement Team Follow Logic**: In `PlayerSceneSystem::HandleEnterScene`, add async logic to check the team leader's location via Redis/TeamService.
-   **Trigger Redirect**: If the leader is in a different scene, immediately trigger `SceneCommon::LeaveScene` and initiate a transfer to the leader's scene.
-   **Verify Async Safety**: Ensure the added logic uses non-blocking callbacks.
</next_steps>