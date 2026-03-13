# Development Scripts

PowerShell and shell scripts for common development tasks.

## Available Scripts

### dev-tools.ps1

Main entry point for tool commands on Windows.

#### Commands

```powershell
# Build pbgen
pwsh -File dev-tools.ps1 -Command pbgen-build

# Run pbgen with default config
pwsh -File dev-tools.ps1 -Command pbgen-run

# Run pbgen with custom config
pwsh -File dev-tools.ps1 -Command pbgen-run -ConfigPath <path-to-config>

# Generate project tree report
pwsh -File dev-tools.ps1 -Command tree
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
pwsh -File tools/scripts/dev-tools.ps1 -Command pbgen-run

# Generate project tree
pwsh -File tools/scripts/dev-tools.ps1 -Command tree
```

### From tools/scripts Directory

```powershell
# Build pbgen
pwsh -File dev-tools.ps1 -Command pbgen-build

# Run pbgen
pwsh -File dev-tools.ps1 -Command pbgen-run -ConfigPath ../proto-generator/pbgen/etc/proto_gen.yaml
```

## Notes

- All scripts are Windows PowerShell 5.1+ compatible
- Use `-Verbose` flag for detailed output
- Scripts are designed to be cross-platform compatible where possible
