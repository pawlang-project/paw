#!/bin/bash
# Build LLVM locally for PawLang

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LLVM_SRC="$PROJECT_ROOT/llvm/19.1.6"
LLVM_BUILD="$PROJECT_ROOT/llvm/build"
LLVM_INSTALL="$PROJECT_ROOT/llvm/install"

echo "🔨 Starting LLVM build..."

# Get CPU count
CPU_COUNT=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

cd "$LLVM_BUILD"

# Configure
cmake "$LLVM_SRC/llvm" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$LLVM_INSTALL" \
    -DLLVM_ENABLE_PROJECTS=clang \
    -DLLVM_TARGETS_TO_BUILD="AArch64;X86" \
    -DLLVM_ENABLE_ASSERTIONS=OFF \
    -DLLVM_ENABLE_RTTI=ON \
    -DLLVM_BUILD_TOOLS=ON \
    -DLLVM_BUILD_EXAMPLES=OFF \
    -DLLVM_BUILD_TESTS=OFF \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_INCLUDE_DOCS=OFF \
    -DLLVM_ENABLE_BINDINGS=OFF \
    -G Ninja

# Build
echo "🔨 Building... (using $CPU_COUNT cores)"
ninja -j "$CPU_COUNT"

# Install
echo "📦 Installing..."
ninja install

echo "✅ LLVM build completed!"
echo "   Location: $LLVM_INSTALL"
echo "   Version: $($LLVM_INSTALL/bin/llvm-config --version)"
