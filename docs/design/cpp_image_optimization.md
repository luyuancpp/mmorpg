# C++ Docker Image Optimization & Split Debug Symbols

## Problem

| Image | Before | After |
|-------|--------|-------|
| C++ node (gate+scene) | **1.33 GB** | **194 MB** |
| Go services | ~64 MB | — |

Root cause of the 1.33 GB image:
- Unstripped binaries: gate = 174 MB, scene = 334 MB
- `/usr/local/lib/` copied in full: 173 MB (mostly `.a` static archives, `.so` files only ~1.3 MB)
- ubuntu:24.04 base + apt packages: ~88 MB

## Solution 1: Image Size Reduction

Two changes in `deploy/k8s/Dockerfile.cpp`:

1. **Strip binaries**: `strip --strip-unneeded` reduces gate 174→24 MB, scene 334→28 MB
2. **Copy only `.so` files**: `COPY --from=builder /usr/local/lib/*.so* /usr/local/lib/` skips `.a` static archives (saves ~170 MB)

Result: **85% reduction** (1.33 GB → 194 MB).

## Solution 2: Split Debug Symbols for Release Crash Analysis

Problem: after stripping, crash core dumps lose variable/line info.

Standard approach — **separate debug info files**:

### Build Flow

`build_linux.sh` new options:
- `--relwithdebinfo`: Uses `CMAKE_BUILD_TYPE=RelWithDebInfo` (keeps `-O2` + `-g`)
- `--split-debug`: Extracts `.debug` files, strips binaries, adds gnu-debuglink

Steps in `--split-debug`:
```bash
# 1. Extract debug info
objcopy --only-keep-debug bin/gate bin/symbols/gate.debug

# 2. Strip the binary for deployment
strip --strip-unneeded bin/gate

# 3. Link binary to its .debug file (GDB auto-finds it)
objcopy --add-gnu-debuglink=bin/symbols/gate.debug bin/gate
```

### Dockerfile Multi-Stage

```
Stage 1 (deps):    gcc:13 + third-party libs
Stage 2 (builder): build with --relwithdebinfo --split-debug
Stage 2.5 (symbols): FROM scratch, holds .debug files only
Stage 3 (runtime): ubuntu:24.04, stripped binaries + .so libs
```

The `symbols` stage is a `FROM scratch` stage. It is NOT included in the default build target, so the runtime image stays small.

### Usage

```bash
# Build runtime image (same as before, ~194 MB)
docker build -f deploy/k8s/Dockerfile.cpp -t mmorpg-node:v3 .

# Extract debug symbols to local filesystem
docker build -f deploy/k8s/Dockerfile.cpp --target=symbols -o ./debug-symbols .
# → debug-symbols/gate.debug (~150 MB)
# → debug-symbols/scene.debug (~300 MB)
```

### Crash Analysis Workflow

```bash
# 1. Copy core dump from crashed pod
kubectl cp <pod>:/tmp/core.12345 ./core.12345

# 2. Load in GDB with symbol file
gdb ./gate ./core.12345 -s gate.debug

# 3. Full stack trace with variables and line numbers
(gdb) bt full
(gdb) info locals
(gdb) frame 3
(gdb) print some_variable
```

### Archiving Symbols

For each release:
```bash
# Extract and archive
docker build -f deploy/k8s/Dockerfile.cpp --target=symbols -o ./symbols-v3 .
tar czf symbols-v3.tar.gz symbols-v3/
# Store alongside the release tag / in artifact storage
```

Keep symbol files for every deployed version. Core dumps are useless without matching symbols.

## Key Files

| File | Role |
|------|------|
| `deploy/k8s/Dockerfile.cpp` | Multi-stage build with `symbols` stage |
| `tools/scripts/build_linux.sh` | `--relwithdebinfo` + `--split-debug` options |

## Notes

- `RelWithDebInfo` produces slightly larger (pre-strip) binaries than `Release`, but the stripped output is equivalent in size.
- The `gnu-debuglink` embedded in the binary is a CRC + filename hint — GDB uses it to auto-locate the `.debug` file if placed in standard paths (`/usr/lib/debug/`, or same directory).
- For production K8s: enable core dumps via `ulimit -c unlimited` and configure core pattern (e.g., `/tmp/core.%e.%p`).
