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

## Build, Test, and Lint Commands

### C++ (Core Game Server)
- **Build (Windows solution)**: `msbuild game.sln /m /p:Configuration=Debug /p:Platform=x64`
- **Build a node with CMake (example: scene)**: `cd cpp\nodes\scene && cmake -S . -B build && cmake --build build --config Debug`
- **Run C++ tests**: test projects are under `cpp\unit_test\` (run in Visual Studio Test Explorer or execute the built test binary directly).
- **Run a single C++ test**: `cpp\unit_test\bag_test\x64\Debug\bag_test.exe --gtest_filter=BagTest.AddNewGridItem`
- **Lint C++**: use `clang-tidy` with `cpp\.clang-tidy` (example: `clang-tidy <file.cpp> --config-file=cpp\.clang-tidy`)

### Go (Microservices)
- **Regenerate Go gRPC/proto code**: `cd go && build.bat`
- **Run login service**: `cd go\login && go run login.go`
- **Run all login tests**: `cd go\login && go test ./...`
- **Run a single Go test**: `cd go\login && go test ./data -run TestPlayerFSM_InitialState`
- **Dependencies**: `cd go\login && go mod tidy`

### Java (Auth/Web)
- **Build**: `cd java\sa-token-node && mvn clean install`
- **Run tests**: `cd java\sa-token-node && mvn test`
- **Run a single Java test**: `cd java\sa-token-node && mvn -Dtest=GrpcServerApplicationTests#contextLoads test`

## High-Level Architecture

- **Polyglot backend split by responsibility**:
  - **C++ nodes** (`cpp/nodes/*`): runtime node processes (scene/gate/centre) and RPC handlers.
  - **C++ shared game logic** (`cpp/libs/services/scene/*`): ECS-heavy domain logic, with `system` and `comp` subtrees.
  - **Go services** (`go/login` and peers): go-zero based microservices and grpc servers.
  - **Java auth/web** (`java/sa-token-node`): Spring Boot + sa-token + grpc auth service.
- **Proto-first contract flow**:
  - Source contracts live in `proto/`.
  - Generated protocol outputs are checked into `generated/generated_proto/` and language-specific trees (e.g., `go/login/proto/...`).
  - `go/build.bat` regenerates Go-side rpc/proto artifacts for login/db services.
- **Service discovery/runtime wiring**:
  - Go login starts grpc via `zrpc.MustNewServer`, registers node metadata, and watches centre nodes through `NodeWatcher` (etcd-backed).
  - C++ scene request handlers delegate gameplay work to systems such as `PlayerSceneSystem`/`PlayerLifecycleSystem` instead of embedding heavy logic in transport layer handlers.

## Key Conventions

- **Generated/editable boundary**:
  - Do not edit generated protocol outputs under `generated/` or language `proto` output trees.
  - In generated-style C++ handler files that contain `///<<< BEGIN WRITING YOUR CODE`, keep custom logic inside those guarded sections.
- **C++ naming and layering**:
  - RPC request handlers are `*Handler` classes in `cpp/nodes/scene/handler/rpc/...` (example: `SceneScenePlayerHandler`).
  - Async/reply handling uses dedicated reply handler functions (example pattern: `On<Domain><Method>Reply` in `cpp/nodes/scene/rpc_replies`).
  - ECS/domain logic stays in `*System` classes and `*Comp` structs under `cpp/libs/services/scene/...`.
- **Cross-service change workflow**:
  - When `.proto` changes, regenerate Go artifacts (`go\build.bat`) and then rebuild affected C++/Go/Java services that consume the updated contract.
