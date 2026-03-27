#!/bin/bash
#
# autogen.sh — Full Linux build for C++ game nodes (gate + scene)
#
# Run from repo root:  ./tools/archived/autogen.sh
#
# Flow:
#   1. git submodule update
#   2. Build third-party dependencies (setup_dependencies.sh)
#   3. Generate CMakeLists.txt from vcxproj (vcxproj2cmake.py)
#   4. Build all libraries in dependency order
#   5. Build gate + scene executables → bin/
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

CPU=$(nproc 2>/dev/null || echo 4)
echo "=== autogen.sh: Linux build for MMORPG C++ nodes ==="
echo "  Repo root : $REPO_ROOT"
echo "  Parallel  : $CPU"
echo ""

# ── 0. Submodules ───────────────────────────────────────────────────────────

echo "[0] Checking submodules..."
git submodule update --init --recursive
echo ""

# ── 1. Dependencies ─────────────────────────────────────────────────────────

echo "[1] Building third-party dependencies..."
bash "$SCRIPT_DIR/setup_dependencies.sh"
echo ""

# ── 2. Generate CMakeLists.txt from vcxproj ─────────────────────────────────

echo "[2] Generating CMakeLists.txt from vcxproj files..."
python3 "$SCRIPT_DIR/vcxproj2cmake.py"
echo ""

# ── 3. Build libraries + executables ────────────────────────────────────────

buildproject() {
    local dir="$1"
    local name="$(basename "$dir")"
    echo "--- Building: $dir ($name) ---"
    cd "$REPO_ROOT/$dir"
    cmake -S . -B .build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="/usr/local"
    cmake --build .build -j "$CPU"
    cd "$REPO_ROOT"
    echo "--- $name ok ---"
    echo ""
}

echo "[3] Building libraries (dependency order)..."

# Third-party (navmesh/hexagon)
buildproject "third_party"

# Generated code
buildproject "cpp/generated/proto"
buildproject "cpp/generated/rpc"
buildproject "cpp/generated/proto_helpers"
buildproject "cpp/generated/table"
buildproject "cpp/generated/grpc_client"

# Engine
buildproject "cpp/libs/engine/core"
buildproject "cpp/libs/engine/config"
buildproject "cpp/libs/engine/session"
buildproject "cpp/libs/engine/infra"
buildproject "cpp/libs/engine/thread_context"

# Modules
buildproject "cpp/libs/modules"

# Services
buildproject "cpp/libs/services/scene"
buildproject "cpp/libs/services/gate"

echo "[4] Building executables..."

# Gate node
buildproject "cpp/nodes/gate"

# Scene node
buildproject "cpp/nodes/scene"

# ── Done ────────────────────────────────────────────────────────────────────

echo "========================================"
echo "  BUILD COMPLETE"
echo "  gate  : $REPO_ROOT/bin/gate"
echo "  scene : $REPO_ROOT/bin/scene"
echo "========================================"
