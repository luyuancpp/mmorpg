# Proto Generator (pbgen)

Canonical protocol buffer code generation tool for the MMORPG project.

## Overview

pbgen is a code generator that processes `.proto` files and generates language-specific bindings for:
- Go gRPC services
- C++ gRPC stubs
- Java protobuf classes
- Python protobuf definitions

## Directory Structure

```
proto-generator/
├── pbgen/                  # Main pbgen source code
│   ├── bin/               # Compiled pbgen executable
│   ├── src/               # Go source code
│   ├── etc/               # Configuration files
│   │   └── proto_gen.yaml # Default generation config
│   └── templates/         # Code generation templates
└── README.md
```

## Configuration

The main configuration file is `pbgen/etc/proto_gen.yaml`. It specifies:
- Input proto directories
- Output directories for each language
- Code generation options
- Plugin paths (e.g., gRPC C++ plugin)

## Usage

### Build pbgen

```powershell
pwsh -File ../../scripts/dev-tools.ps1 -Command pbgen-build
```

### Generate Code

```powershell
# Using default config
pwsh -File ../../scripts/dev-tools.ps1 -Command pbgen-run

# Using custom config
pwsh -File ../../scripts/dev-tools.ps1 -Command pbgen-run -ConfigPath etc/proto_gen.yaml
```

## Output

Generated code is placed in:
- `generated/generated_proto/` - All language proto outputs
- `go/generated/proto/` - Go-specific bindings
- `cpp/generated/` - C++ protobuf and gRPC stubs
- `java/` - Java generated classes

## Logs

Generation logs are stored in:
- `pbgen/` directory (*.log, *.txt files)
- Check `docs/pbgen/` for historical run logs

## Notes

- pbgen is idempotent; running it multiple times produces the same output
- Always validate generated code before committing
- Keep config file in sync with actual proto locations
