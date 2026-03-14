# Development Scripts

PowerShell and shell scripts for common development tasks.

## Available Scripts

### dev_tools.ps1

Main entry point for tool commands on Windows.

#### Commands

```powershell
# Build pbgen
pwsh -File dev_tools.ps1 -Command pbgen-build

# Run pbgen with default config
pwsh -File dev_tools.ps1 -Command pbgen-run

# Run pbgen with custom config
pwsh -File dev_tools.ps1 -Command pbgen-run -ConfigPath <path-to-config>

# Generate project tree report
pwsh -File dev_tools.ps1 -Command tree

# Audit recursive naming issues (default snake_case)
pwsh -File dev_tools.ps1 -Command naming-audit

# Apply recursive naming normalization
pwsh -File dev_tools.ps1 -Command naming-apply

# Use kebab-case style with a capped batch size
pwsh -File dev_tools.ps1 -Command naming-audit -Style kebab -MaxChanges 200
```

### normalize_names.ps1

Recursively normalizes file and directory names to `snake_case` or `kebab-case`.

- Excludes `third_party` by default
- Also skips tooling/build artifact roots by default: `.git`, `.vs`, `.idea`, `.vscode`, `_copilot_session_transfer`, `x64`, `bin`, `generated`, `cpp/generated`, `go/generated`, `cpp/bin`
- Skips embedded muduo vendor trees under `cpp/libs/*/muduo_windows`
- By default only scans source roots: `cpp`, `go`, `java`, `proto`, `docs`, `tools`, `scripts`, `data`, `deploy`, `robot`, `test`, `etc`
- Skips dotfiles and dot-directories to avoid renaming repository metadata files
- Skips high-risk generated/IDE files: `*.vcxproj*`, `*.sln*`, `*.tlog`, `*.recipe`, `*.pb.{h,cc,go}`
- Supports conflict detection (duplicate targets / existing target path)
- Uses deep-first rename order to avoid parent path conflicts

Recommended workflow:

```powershell
# 1) Dry run first
pwsh -File tools/scripts/dev_tools.ps1 -Command naming-audit

# 2) Apply in small batches for safer rollout
pwsh -File tools/scripts/dev_tools.ps1 -Command naming-apply -MaxChanges 100

# 3) Build and run tests after each batch

# Optional: run against all non-excluded paths (advanced)
pwsh -File tools/scripts/normalize_names.ps1 -Mode audit -IncludeRelativePaths @()
```

### tree.ps1

Generates a tree structure report of the project directory.

```powershell
pwsh -File tree.ps1
```

## Usage Examples

### From Project Root

```powershell
# Generate all proto code
pwsh -File tools/scripts/dev_tools.ps1 -Command pbgen-run

# Generate project tree
pwsh -File tools/scripts/dev_tools.ps1 -Command tree
```

### From tools/scripts Directory

```powershell
# Build pbgen
pwsh -File dev_tools.ps1 -Command pbgen-build

# Run pbgen
pwsh -File dev_tools.ps1 -Command pbgen-run -ConfigPath ../proto_generator/pbgen/etc/proto_gen.yaml
```

## Notes

- All scripts are Windows PowerShell 5.1+ compatible
- Use `-Verbose` flag for detailed output
- Scripts are designed to be cross-platform compatible where possible
- For large repositories, prefer iterative rename batches with `-MaxChanges` to keep refactors reviewable and reduce breakage risk
