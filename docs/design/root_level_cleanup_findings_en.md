# Root Level & Tools Directory Audit (2026-03-24)

## Critical Finding: robot_client Directory
- **Status**: Being deleted from git (staged deletions of all *.pb.go files under `tools/robot_client/proto/`)
- **Issue**: Documentation references `tools/robot_client/main.go` but this path doesn't exist
- **Reality**: Actual path is `robot/go.mod` at root level
- **Action needed**: Update all documentation references

## Stale/Questionable Files at Root
1. **game.slnLaunch.user**: User-specific VS config, likely should be in .gitignore
2. **Dockerfile**: Outdated (references "turn-based-game", old Linux build)
3. **Stale .exp files in lib/**: Many Windows build artifacts

## Directory Duplication Issues
1. **scripts/ vs tools/scripts/**: scripts/ is legacy; tools/scripts/ is current
2. **lib/ vs third_party/**: Different purposes (compiled output vs source) - not true duplication
3. **robot/ vs tools/robot/**: Confusing naming - should clarify

## bin/ Directory Content
- Most .exe, .pdb, .log, .txt are in .gitignore
- Config: bin/config/ and bin/etc/ contain legitimate configs

## Action Items
1. Fix robot_client references in AGENTS.md files
2. Migrate scripts/ to tools/scripts/ or remove
3. Add game.slnLaunch.user to .gitignore
4. Update or mark Dockerfile as legacy
5. Review bin/ output files
6. Verify lib/ artifacts should be checked in
