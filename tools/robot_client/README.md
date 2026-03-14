# Robot Client

Automated test client and robot tooling for the MMORPG project.

## Overview

The robot client is a Go-based testing framework that:
- Simulates player client behavior
- Automated load testing and stress testing
- Gameplay scenario validation
- RPC call simulation and mocking

## Directory Structure

```
robot_client/
├── main.go               # Entry point
├── go.mod, go.sum        # Go module dependencies
├── build.bat             # Build script
├── config/               # Configuration files
├── etc/                  # Runtime configuration
├── interfaces/           # Protocol interfaces
├── pkg/                  # Internal packages
├── proto/                # Generated protobuf files
├── common/               # Shared utilities
└── logic/                # Test logic implementations
```

## Building

### Windows

```powershell
.\build.bat
```

### Linux/macOS

```bash
go build -o bin/robot_client main.go
```

## Running

```bash
# With default config
./bin/robot_client

# With custom config
./bin/robot_client -config etc/custom.yaml
```

## Configuration

Robot behavior is configured via YAML files in `etc/`:
- `robot.yaml` - Main configuration
- `scenario.yaml` - Test scenarios
- Connection settings (server address, port, etc.)

## Development

### Dependencies

Install dependencies:
```bash
go mod tidy
```

### Testing

```bash
go test ./...
```

## Notes

- Robot client connects to the actual game server
- Use for load testing and integration validation only
- Keep test scenarios in version control
- Results are useful for performance profiling
