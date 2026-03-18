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
| Node-local RPC handlers | `nodes/*/handler/rpc/` | Thin transport adapters |
| Node-local event handlers | `nodes/*/handler/event/` | Event-driven edge logic |
| Async/reply handling | `nodes/*/rpc_replies/` | `On<Domain><Method>Reply` style |
| Scene gameplay/domain | `libs/services/scene/` | actor/player/world/frame/team/etc. |
| Static analysis rule | `.clang-tidy` | Custom `myplugin-no-member-pointer` check only |

## CONVENTIONS
- Keep handlers thin; move gameplay/state transitions into `libs/services/scene/*` systems/components/world code.
- Respect `///<<< BEGIN WRITING YOUR CODE` regions in scaffolded/generated-style C++ files.
- Do not hand-edit `cpp/generated/` outputs.
- Use **Scene Node** naming, matching current project terminology.
- Scene/gate command consumers are wired in `main.cpp` via `topicPrefix` / `groupPrefix` options.
- Reply handlers are adapters; they should not become business-logic owners.

## ANTI-PATTERNS
- Embedding large gameplay branches inside node handlers.
- Rewiring Kafka command flow ad hoc from inside unrelated handlers.
- Editing generated protobuf/codegen outputs directly.
- Broad edits in vendored engine trees when a node/service-layer fix is enough.

## COMMANDS
```bash
msbuild game.sln /m /p:Configuration=Debug /p:Platform=x64
cd cpp\nodes\scene && cmake -S . -B build && cmake --build build --config Debug
clang-tidy <file.cpp> --config-file=cpp\.clang-tidy
```

## NOTES
- `nodes/centre/` currently shows handlers/replies in this tree; confirm entrypoint before assuming a standalone `main.cpp` there.
- `libs/services/scene/` is the highest-value subtree for durable domain fixes.
- Tests are native binaries/projects under `cpp/tests/`; run the relevant target rather than assuming a single root test runner.
