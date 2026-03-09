# Copilot Instructions

## Project Guidelines
- Prefer clearer, verb-based RPC handler names and thin wrapper pattern: keep generated RPC method names but delegate work to well-named internal functions (e.g., ProcessClientPlayerMessage).

## Recent Architectural Decisions (2025-03-09)

### Scene Manager & Gate Communication
1. **Switch from gRPC Stream to Kafka**:
   - **Problem**: A full mesh of 40,000 Gates * 2000 Scene Managers would exhaust file descriptors and network resources.
   - **Solution**: Use Kafka for control messages (`RoutePlayer`, `KickPlayer`) to decouple Gate and SceneManager.
   - **Protocol**: 
     - Messages are defined in `GateCommand` within `scene_manager_service.proto`.
     - Topics follow the pattern `gate-{gate_id}`.
     - Messages include `target_instance_id` to prevent "zombie" messages from affecting restarted Gate nodes.

2. **Scene Manager Implementation (Go-Zero)**:
   - `SceneManager` is implemented in Go using go-zero.
   - It handles `EnterSceneByCentre` requests by looking up player location in Redis.
   - Instead of maintaining a gRPC stream to Gate, it now pushes `RoutePlayer` commands to the appropriate Kafka topic.
   - Configuration requires Kafka brokers in `etc/scenemanagerservice.yaml`.

3. **Gate Node Implementation (C++)**:
   - **Naming**: Use **Scene Node** (not Game Node) for consistency.
   - **Communication**: Gate Node must implement a Kafka Consumer to listen on `gate-{gate_id}`.
   - **Service Type**: Added `SceneManagerNodeService` (ID 25) to `node.proto` and `node_util.cpp` to distinguish the Go-based manager from C++ services.

4. **Pending Work**:
   - C++ side needs to link `librdkafka` or `cppkafka` and implement the consumer loop in `GateNode`.
