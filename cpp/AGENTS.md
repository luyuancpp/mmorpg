# CPP KNOWLEDGE BASE

## OVERVIEW
`cpp/` holds runtime node entrypoints, transport adapters, shared scene-domain services, plugins, tests, and checked-in/generated C++ artifacts.

## STRUCTURE
```text
cpp/
├── nodes/       # Runnable node entrypoints and node-local handlers/replies
├── libs/        # Shared C++ libraries; scene domain logic lives here
├── tests/       # Native test projects/binaries
├── plugin/      # Custom clang-tidy / plugin work
└── generated/   # Generated C++ artifacts
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Scene process bootstrap | `nodes/scene/main.cpp` | Registers node + Kafka command handlers |
| Gate process bootstrap | `nodes/gate/main.cpp` | TCP client bridge + session wiring |
| New node `main.cpp` templates | `nodes/_template/` | Use `main.simple.cpp.example` or `main.with_context.cpp.example` |
| Node-local RPC handlers | `nodes/*/handler/rpc/` | Thin transport adapters |
| Node-local event handlers | `nodes/*/handler/event/` | Event-driven edge logic |
| Async/reply handling | `nodes/*/rpc_replies/` | `On<Domain><Method>Reply` style |
| Scene gameplay/domain | `libs/services/scene/` | actor/player/world/frame/team/etc. |
| Scene node role config | `libs/engine/config/config.cpp` `readGameConfig` | Honours `SCENE_NODE_TYPE` / `ZONE_ID` / `GAME_CONFIG_PATH` env overrides |
| Static analysis rule | `.clang-tidy` | Custom `myplugin-no-member-pointer` check only |

## CONVENTIONS
- Keep handlers thin; move gameplay/state transitions into `libs/services/scene/*` systems/components/world code.
- Respect `///<<< BEGIN WRITING YOUR CODE` regions in scaffolded/generated-style C++ files.
- Do not hand-edit `cpp/generated/` outputs.
- Use **Scene Node** naming, matching current project terminology.
- Scene/gate command consumers are wired in `main.cpp` via `topicPrefix` / `groupPrefix` options.
- New node entrypoints should start from `nodes/_template/README.md` and reuse `node::entry::RunSimpleNodeMain...` helpers.
- Reply handlers are adapters; they should not become business-logic owners.

### Node Main PR Checklist (Summary)
- Start from `nodes/_template/main.simple.cpp.example` or `nodes/_template/main.with_context.cpp.example`.
- Keep startup logic thin; do not move gameplay/business logic into `main.cpp`.
- Ensure node main uses `node::entry::RunSimpleNodeMain...` helpers.
- Ensure Kafka topic/group and routing fields match the node contract.
- Ensure thread observability is active by default (or intentionally disabled via `NODE_THREAD_MONITOR_ENABLED=0`).
- Do not hand-edit generated outputs while wiring new node startup.

### ECS component access rules
- `get<T>`: only inside `view<T,...>` or after `any_of<T>`/`all_of<T>` guard. Asserts; crashes if absent.
- `try_get<T>`: cross-entity lookups, optional components. Returns nullptr when absent.
- `get_or_emplace<T>`: **only** during entity init/setup. **Never** in per-tick, combat, spatial, attribute sync.
- Return `std::optional<T>` from functions that depend on optional components.

## ANTI-PATTERNS
- Embedding large gameplay branches inside node handlers.
- Rewiring Kafka command flow ad hoc from inside unrelated handlers.
- Editing generated protobuf/codegen outputs directly.
- Broad edits in vendored engine trees when a node/service-layer fix is enough.
- Using `get_or_emplace<T>` in per-tick systems, combat, spatial queries, or attribute sync — silently creates default components and corrupts ECS state.

## COMMANDS
```bash
msbuild game.sln /m /p:Configuration=Debug /p:Platform=x64
cd cpp\nodes\scene && cmake -S . -B build && cmake --build build --config Debug
clang-tidy <file.cpp> --config-file=cpp\.clang-tidy
```

## NOTES
- `libs/services/scene/` is the highest-value subtree for durable domain fixes.
- Tests are native binaries/projects under `cpp/tests/`; run the relevant target rather than assuming a single root test runner.
