# Proto (Legacy) - DEPRECATED

This directory contains legacy protocol definitions and is retained for backward compatibility only.

## Status

⚠️ **DEPRECATED** — New protocol definitions should use the root-level `proto/` directory instead.

## Purpose

This legacy directory exists to support:
- Old build configurations that reference this location
- Backward compatibility with existing deployments
- Gradual migration to the new schema location

## Migration Path

If you're adding new protocol definitions:

1. **Don't** add to `tools/proto/`
2. **Do** add to the root `proto/` directory instead
3. Update build configurations to reference the new location
4. Remove references to `tools/proto/` once migration is complete

## Contents

This directory may contain:
- `service_id.txt` - Legacy service ID mappings
- `.gitignore` - Ignored files list

## Notes

- This directory will be removed in a future major version
- Coordinate with the team before removing this path
- Update CI/CD pipelines if they reference this location
