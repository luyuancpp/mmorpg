#!/bin/bash
#
# setup_dependencies.sh — Build third-party C++ dependencies for Linux
#
# Builds: gRPC (+ protobuf, abseil, re2, c-ares, utf8_range),
#         muduo-linux, librdkafka, yaml-cpp, hiredis, zlib
#
# Run from repo root:  ./tools/archived/setup_dependencies.sh
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

CPU=$(nproc 2>/dev/null || echo 4)
echo "Build parallelism: $CPU"

LIB_DIR="$REPO_ROOT/lib"
mkdir -p "$LIB_DIR"

# ── 1. CMake (skip if >= 3.22) ──────────────────────────────────────────────

CMAKE_MIN="3.22"
CMAKE_VER=$(cmake --version 2>/dev/null | head -1 | grep -oP '\d+\.\d+' || echo "0.0")
if [ "$(printf '%s\n' "$CMAKE_MIN" "$CMAKE_VER" | sort -V | head -1)" != "$CMAKE_MIN" ]; then
    echo "=== Installing CMake >= $CMAKE_MIN ==="
    apt-get update && apt-get install -y cmake
fi
echo "cmake: $(cmake --version | head -1)"

# ── 2. gRPC + protobuf + abseil (from third_party/grpc) ────────────────────

GRPC_DIR="$REPO_ROOT/third_party/grpc"
GRPC_BUILD="$GRPC_DIR/.build_linux"
if [ ! -f /usr/local/lib/libgrpc++.a ] && [ ! -f /usr/local/lib/libgrpc.a ]; then
    echo "=== Building gRPC from source ==="
    if [ ! -f "$GRPC_DIR/CMakeLists.txt" ]; then
        echo "ERROR: $GRPC_DIR/CMakeLists.txt not found."
        echo "Run: git submodule update --init --recursive"
        exit 1
    fi

    mkdir -p "$GRPC_BUILD"
    cmake -S "$GRPC_DIR" -B "$GRPC_BUILD" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DCMAKE_CXX_STANDARD=23 \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_BUILD_CSHARP_EXT=OFF \
        -DgRPC_BUILD_GRPC_CSHARP_PLUGIN=OFF \
        -DgRPC_BUILD_GRPC_NODE_PLUGIN=OFF \
        -DgRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN=OFF \
        -DgRPC_BUILD_GRPC_PHP_PLUGIN=OFF \
        -DgRPC_BUILD_GRPC_PYTHON_PLUGIN=OFF \
        -DgRPC_BUILD_GRPC_RUBY_PLUGIN=OFF \
        -DABSL_PROPAGATE_CXX_STD=TRUE

    cmake --build "$GRPC_BUILD" -j "$CPU"
    cmake --install "$GRPC_BUILD"
    ldconfig
    echo "gRPC install ok"
else
    echo "gRPC already installed, skipping"
fi

# ── 3. muduo-linux ──────────────────────────────────────────────────────────

MUDUO_DIR="$REPO_ROOT/third_party/muduo-linux"
MUDUO_BUILD="$MUDUO_DIR/.build_linux"
if [ ! -f "$LIB_DIR/libmuduo_net.a" ]; then
    echo "=== Building muduo-linux ==="
    mkdir -p "$MUDUO_BUILD"
    cmake -S "$MUDUO_DIR" -B "$MUDUO_BUILD" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=23 \
        -DMUDUO_BUILD_EXAMPLES=OFF
    cmake --build "$MUDUO_BUILD" -j "$CPU"
    # Copy libs to repo lib/
    find "$MUDUO_BUILD" -name "libmuduo_*.a" -exec cp {} "$LIB_DIR/" \;
    echo "muduo install ok"
else
    echo "muduo already built, skipping"
fi

# ── 4. hiredis ──────────────────────────────────────────────────────────────

HIREDIS_DIR="$REPO_ROOT/third_party/redis/deps/hiredis"
if [ ! -f "$LIB_DIR/libhiredis.a" ]; then
    echo "=== Building hiredis ==="
    cd "$HIREDIS_DIR"
    make -j"$CPU"
    cp -f libhiredis.a "$LIB_DIR/"
    make install PREFIX=/usr/local
    cd "$REPO_ROOT"
    echo "hiredis install ok"
else
    echo "hiredis already built, skipping"
fi

# ── 5. librdkafka ──────────────────────────────────────────────────────────

RDKAFKA_DIR="$REPO_ROOT/third_party/librdkafka"
RDKAFKA_BUILD="$RDKAFKA_DIR/.build_linux"
if [ ! -f "$LIB_DIR/librdkafka++.a" ]; then
    echo "=== Building librdkafka ==="
    mkdir -p "$RDKAFKA_BUILD"
    cmake -S "$RDKAFKA_DIR" -B "$RDKAFKA_BUILD" \
        -DCMAKE_BUILD_TYPE=Release \
        -DRDKAFKA_BUILD_EXAMPLES=OFF \
        -DRDKAFKA_BUILD_TESTS=OFF \
        -DRDKAFKA_BUILD_STATIC=ON
    cmake --build "$RDKAFKA_BUILD" -j "$CPU"
    find "$RDKAFKA_BUILD" -name "librdkafka*.a" -exec cp {} "$LIB_DIR/" \;
    cmake --install "$RDKAFKA_BUILD" --prefix /usr/local
    echo "librdkafka install ok"
else
    echo "librdkafka already built, skipping"
fi

# ── 6. yaml-cpp ─────────────────────────────────────────────────────────────

YAMLCPP_DIR="$REPO_ROOT/third_party/yaml-cpp"
YAMLCPP_BUILD="$YAMLCPP_DIR/.build_linux"
if [ ! -f "$LIB_DIR/libyaml-cpp.a" ]; then
    echo "=== Building yaml-cpp ==="
    mkdir -p "$YAMLCPP_BUILD"
    cmake -S "$YAMLCPP_DIR" -B "$YAMLCPP_BUILD" \
        -DCMAKE_BUILD_TYPE=Release \
        -DYAML_CPP_BUILD_TESTS=OFF \
        -DYAML_CPP_BUILD_TOOLS=OFF \
        -DYAML_BUILD_SHARED_LIBS=OFF
    cmake --build "$YAMLCPP_BUILD" -j "$CPU"
    find "$YAMLCPP_BUILD" -name "libyaml-cpp.a" -exec cp {} "$LIB_DIR/" \;
    cmake --install "$YAMLCPP_BUILD" --prefix /usr/local
    echo "yaml-cpp install ok"
else
    echo "yaml-cpp already built, skipping"
fi

# ── 7. zlib ─────────────────────────────────────────────────────────────────

ZLIB_DIR="$REPO_ROOT/third_party/zlib"
if [ ! -f "$LIB_DIR/libz.a" ]; then
    echo "=== Building zlib ==="
    cd "$ZLIB_DIR"
    ./configure --64 --static --prefix=/usr/local
    make -j"$CPU"
    make install
    cp -f libz.a "$LIB_DIR/"
    cd "$REPO_ROOT"
    echo "zlib install ok"
else
    echo "zlib already built, skipping"
fi

# ── Done ────────────────────────────────────────────────────────────────────
echo ""
echo "=== All dependencies ready ==="
echo "  Installed to: /usr/local  +  $LIB_DIR"
echo ""
echo "System prerequisites (install with apt-get if missing):"
echo "  build-essential cmake git libssl-dev libboost-dev pkg-config"
