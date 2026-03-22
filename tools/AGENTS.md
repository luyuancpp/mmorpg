# TOOLS KNOWLEDGE BASE

## OVERVIEW
`tools/` hosts developer tooling, code generators, exporters, robot/load clients, snapshots, and the preferred PowerShell entrypoints for common repo workflows.

## STRUCTURE
```text
tools/
├── proto_generator/      # Canonical proto-gen source project
├── data_table_exporter/  # Table export scripts/templates
├── robot/                # Robot proto definitions + message handlers
├── contracts/            # Tool-side Kafka message stubs
├── data_service/         # Tool-side gRPC data service stubs
├── scene_manager/        # Tool-side gRPC scene manager stubs
├── proto/                # Legacy proto-gen/pbgen tool bundle kept for compatibility
├── generated/            # Temporary/ignored generation workspace
├── archived/             # Legacy scripts retained for reference
├── dev/                  # mprocs dashboard configs
├── docs/                 # Tool snapshots and archived logs
├── scripts/              # Preferred script entrypoints
└── github.com/           # Go import path mirrors for proto extensions
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Main script entrypoint | `scripts/dev_tools.ps1` | proto-gen (pbgen), k8s, tree, naming audit/apply |
| proto-gen source | `proto_generator/protogen/` | Canonical generator project |
| Compatibility proto-gen bundle | `proto/` | Retained for existing local toolchains |
| Robot proto/handlers | `robot/` | Proto defs + message handlers for load testing |
| Archived generator logs | `docs/protogen/` | Historical runs only |

## CONVENTIONS
- Keep runnable source projects in dedicated subdirectories; do not scatter standalone scripts across `tools/` root.
- Put reports/dumps/snapshots under `docs/`.
- Keep temp generation output under ignored paths like `generated/` and local binaries.
- `tools/scripts/dev_tools.ps1` is the preferred shell entrypoint for routine commands.
- `tools/proto_generator/protogen` is the canonical proto-gen project; `tools/proto/protogen` exists for compatibility.
- Robot client logic assumes one client binds to exactly one goroutine.

## ANTI-PATTERNS
- Committing IDE metadata such as `.idea/`.
- Editing temporary generated outputs as if they were source.
- Renaming broad source trees directly when `naming-audit` / `naming-apply` already exist.
- Replacing `dev_tools.ps1` flows with ad hoc one-off command docs.

## COMMANDS
```bash
pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build
pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run
pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run -ConfigPath tools/proto_generator/protogen/etc/proto_gen.yaml
pwsh -File tools/scripts/dev_tools.ps1 -Command tree
pwsh -File tools/scripts/dev_tools.ps1 -Command naming-audit
pwsh -File tools/scripts/dev_tools.ps1 -Command naming-apply -MaxChanges 100
```

## NOTES
- `tools/scripts/README.md` is the practical command catalog; keep new tool workflows wired through it.
- High-risk rename exclusions already include generated files, IDE files, `*.vcxproj*`, `*.sln*`, and `*.pb.{h,cc,go}`.
- `tools/robot/` contains proto definitions and message handlers for the robot load-testing framework.
