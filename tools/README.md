# tools

This directory hosts developer tooling and generated helper code.

## Layout

- `proto_generator/`: source code of `pbgen` (the canonical proto code generator project).
- `data_table_exporter/`: table export scripts and templates.
- `robot/`: robot client/tooling code and generated robot protobuf files.
- `scene_manager/`: generated scene manager Go protobuf artifacts used by tools.
- `proto/`: legacy binary/tool bundle retained for compatibility.
- `generated/`: temporary generation workspace (ignored).
- `github.com/`: generated/imported Go package-style mirror paths used by tooling.
- `docs/`: tool-related snapshots and tree reports.
- `scripts/`: utility scripts for developers.

## Conventions

- Keep runnable source projects in dedicated subdirectories (`proto_generator`, `data_table_exporter`, etc.).
- Put one-off reports, dumps, and snapshots under `docs/`.
- Put helper scripts under `scripts/`.
- Do not commit IDE metadata directories (for example `.idea/`).
- Keep temporary generation output under ignored paths (`generated/`, temp logs, and local binaries).

## Archived Logs

- Historical `pbgen` run logs are archived in `docs/pbgen/`.
- Keep source directories under `proto_generator/pbgen/` focused on code and config.

## Quick Start

- Build pbgen:
	`pwsh -File tools/scripts/dev_tools.ps1 -Command pbgen-build`
- Run pbgen with default config:
	`pwsh -File tools/scripts/dev_tools.ps1 -Command pbgen-run`
- Run pbgen with custom config:
	`pwsh -File tools/scripts/dev_tools.ps1 -Command pbgen-run -ConfigPath tools/proto_generator/pbgen/etc/proto_gen.yaml`
- Refresh tree output script:
	`pwsh -File tools/scripts/dev_tools.ps1 -Command tree`

## Notes

- Existing build/run tasks already target `tools/proto_generator/pbgen` and remain unchanged.
- `tools/proto/pbgen` is currently retained for compatibility with existing local toolchains.
- `tools/scripts/dev_tools.ps1` is the preferred shell entry point for common tool commands.
