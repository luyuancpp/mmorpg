# Proto Event Message Suffix Rename (2026-06)

## Status: COMPLETED

## Rename Map (12 messages)
| Old name | New name | Rule |
|----------|----------|------|
| `OnConnect2CentrePbEvent` | `OnConnect2CentreEvent` | Drop `Pb` infix |
| `InterruptCurrentStatePbEvent` | `InterruptCurrentStateEvent` | Drop `Pb` infix |
| `CombatStateAddedPbEvent` | `CombatStateAddedEvent` | Drop `Pb` infix |
| `CombatStateRemovedPbEvent` | `CombatStateRemovedEvent` | Drop `Pb` infix |
| `OnNodeAddPbEvent` | `OnNodeAddEvent` | Drop `Pb` infix |
| `OnNodeRemovePbEvent` | `OnNodeRemoveEvent` | Drop `Pb` infix |
| `ConnectToNodePbEvent` | `ConnectToNodeEvent` | Drop `Pb` infix |
| `OnNodeConnectedPbEvent` | `OnNodeConnectedEvent` | Drop `Pb` infix |
| `PlayerMigrationPbEvent` | `PlayerMigrationEvent` | Drop `Pb` infix |
| `InitializePlayerComponentsEvent` | `InitializePlayerCompsEvent` | `Components` → `Comps` |
| `InitializeNpcComponentsEvent` | `InitializeNpcCompsEvent` | `Components` → `Comps` |
| `InitializeActorComponentsEvent` | `InitializeActorCompsEvent` | `Components` → `Comps` |

## Scope of Changes
- `proto/common/event/*.proto` — source proto files
- `proto/event_id.txt` — event ID registry
- `cpp/libs/` — C++ service implementations (~10 files)
- `cpp/nodes/scene/handler/event/` — all event handler .h/.cpp (~14 files)
- `cpp/nodes/gate/handler/rpc/` — client_message_processor .h/.cpp
- `cpp/generated/proto/common/event/` — generated .pb.h/.pb.cc
- `cpp/generated/rpc/service_metadata/` — event_id metadata headers
- `generated/proto/` — all generated proto copies
- `go/*/proto/common/event/` — all Go service .pb.go files
- `go/*/generated/pb/game/event_id.go` — Go event_id generated code

## Convention Established
- Event message suffix: `*Event` (no `Pb` infix)
- ECS component suffix: `*Comp` or `*PBComp` (not `*Component` or `*Components`)
- After proto renames, always re-run proto-gen to regenerate serialized descriptors
