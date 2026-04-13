# Proto-Gen Naming Audit

This document is a snapshot of the current naming state after the user-facing migration from `pbgen` to `proto-gen` and the internal canonical rename to `protogen`.

## Current Outcome

- User-facing command name is now `proto-gen`.
- User-facing binary name is now `proto-gen.exe`.
- Canonical internal directory/module name is now `protogen`.
- Legacy `pbgen` command names remain available as compatibility aliases.
- Legacy `pbgen.exe` remains available as a compatibility copy.
- VS Code task labels and launch labels prefer `proto-gen`.
- Help text and main tool documentation point new users to `proto-gen` first.

## Completed User-Facing Changes

- Added primary commands in `tools/scripts/dev_tools.ps1`:
  - `proto-gen-build`
  - `proto-gen-run`
- Build now emits:
  - `proto-gen.exe`
  - `pbgen.exe` (compatibility copy)
- Kept compatibility aliases:
  - `pbgen-build`
  - `pbgen-run`
- Added help output for command discovery.
- Updated root and tooling documentation to prefer `proto-gen` in examples.
- Updated VS Code tasks to use `proto-gen:*` labels.
- Updated launch configuration names to use `proto-gen`.

## Intentional Compatibility Retention

The following are intentionally unchanged and should not be treated as missed cleanup:

- Directory names:
  - `tools/proto_generator/protogen/`
  - `tools/proto/protogen/`
  - `tools/docs/protogen/`
- Binary and env names:
  - `proto-gen.exe`
  - `pbgen.exe`
  - `PROTOGEN_ENABLE_PPROF`
  - `PBGEN_ENABLE_PPROF`
- Internal source references:
  - Go module/import paths like `protogen/...`
- Generated and archived outputs:
  - tracked generated files under `generated/`
  - archived logs under `tools/docs/protogen/`

## No-Action Zones

These areas should not be renamed during routine cleanup work:

- `generated/**`
- `tools/proto_generator/protogen/**`
- `tools/docs/protogen/**`
- compatibility directories or binaries referenced by existing local workflows

## Low-Risk Hygiene Completed

- Root `.gitignore` covers both current and legacy local artifact paths for the generator.
- `tools/.gitignore` now ignores local binaries under the canonical current path:
  - `proto_generator/protogen/*.exe`
  - `proto_generator/protogen/*.dll`

## What Still Requires a Dedicated Migration

The following are separate projects, not cleanup tasks:

- removing `pbgen.exe` compatibility output
- removing `pbgen-build` / `pbgen-run` compatibility aliases
- removing support for `PBGEN_ENABLE_PPROF`

## Recommended Interpretation

If a future search still finds `pbgen`, classify it in this order:

1. Is it in user-facing command examples or labels?
2. Is it part of an explicit compatibility boundary?
3. Is it generated or archived output?
4. Is it an internal import/path that requires a real migration plan?

Only category 1 should be treated as routine cleanup.