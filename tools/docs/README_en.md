# Documentation and Reports

Historical logs, reports, and documentation for tools and build processes.

## Contents

- `protogen/` - Proto generator run logs and reports (historical archive)
- `proto_gen_naming_audit.md` / `proto_gen_naming_audit_zh.md` - Current naming-state snapshot across `proto-gen`, `protogen`, and retained compatibility aliases
- `proto_gen_naming_migration.md` / `proto_gen_naming_migration_zh.md` - Naming boundary, remaining intentional references, and migration checklist for `proto-gen` / `protogen` / `pbgen`
- Other tool-specific documentation and outputs

## Organization

Keep documentation organized by tool:
- One subdirectory per major tool
- One file per run or report
- Include timestamps for historical tracking

## Notes

- Documentation in this directory is for reference and debugging
- Logs are typically ignored in version control (see `../.gitignore`)
- Archive important reports or milestone logs for future reference
