# PROTO KNOWLEDGE BASE

## OVERVIEW
`proto/` is the contract source of truth. Cross-language interface changes should start here, then flow into generated outputs and consuming services.

## STRUCTURE
```text
proto/
├── common/          # Shared/common messages
├── contracts/       # Cross-service command contracts
├── login/           # Login service contracts
├── scene/           # Scene-related contracts
├── scene_manager/   # Scene manager contracts
├── data_service/    # Data service contracts
├── db/              # DB service contracts
├── player_locator/  # Player-location contracts
├── gate/            # Gate-side contracts
└── *.txt            # message_id / event_id registries
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Message IDs | `message_id.txt` | Shared message-id registry |
| Event IDs | `event_id.txt` | Shared event-id registry |
| Kafka command contracts | `contracts/kafka/` | Gate/scene command payloads |
| Scene APIs | `scene/` | Player/scene/game proto files |
| Login APIs | `login/login.proto` | Login contract root |
| Scene manager APIs | `scene_manager/` | Manager-side contracts |

## CONVENTIONS
- Edit contracts here first; generated outputs elsewhere are downstream artifacts.
- Keep naming aligned with current runtime terminology, especially **Scene Node** rather than legacy/game-node phrasing.
- When routing behavior is Kafka-based, make sure contract updates match topic/consumer expectations in C++ and Go consumers.
- After proto changes, regenerate outputs and rebuild all affected languages.

## FIELD TYPE CONSTRAINTS
- **double (NOT float)**: `Location`, `Rotation`, `Scale`, `Vector3`, `Velocity`, `Acceleration` x/y/z — must match UE4 client double precision.
- **uint64 (NOT uint32)**: `BaseAttributesComp.strength/stamina/health/mana/critchance/armor/resistance` — combat formulas cast to double; uint64 keeps headroom.
- **uint64 (mandatory)**: All SnowFlake GUIDs (`player_id`, `entity`, `scene_id`, `item_id`, `buff_id`, `skill_id` instance, `tx_id`, `snapshot_id`); all Unix-ms timestamps; currency values.
- **uint32 (safe)**: `session_id`, `session_version`, `attack_power`, `defense_power`, `max_health`, `skill_table_id`, `current_frame`.
- **float (safe)**: `ViewRadius.radius`.

## ANTI-PATTERNS
- Patching generated `.pb.go`, `.pb.h`, `.pb.cc`, or checked-in generated proto trees instead of updating source `.proto` files.
- Making service/event ID changes without updating the registry files.
- Treating a contract change as language-local; this repo has multiple consumers.
- Shrinking coordinate doubles to float, BaseAttributesComp uint64 to uint32, or any SnowFlake GUID / Unix-ms timestamp field — see FIELD TYPE CONSTRAINTS above.

## COMMANDS
```bash
pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build
pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run
cd go && build.bat
```

## NOTES
- `generated/proto/` is checked in, so proto edits usually create visible downstream diffs.
- `proto/scene/` and `proto/login/` are high-churn service areas; inspect them first for gameplay/auth contract work.
