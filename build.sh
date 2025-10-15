#!/bin/bash

# PawLang Compiler Build Script

set -e

echo "========================================="
echo "  PawLang Compiler (pawc) Build Script"
echo "========================================="
echo ""

# Create build directory
mkdir -p build
cd build

# Configure (CMake会自动检查并下载LLVM)
echo "[1/3] Configuring..."
cmake ..

# Build
echo "[2/3] Building..."
cmake --build .

# Test
echo "[3/3] Testing..."
if [ -f "./pawc" ]; then
    echo "✓ pawc built successfully"
    ./pawc --help
    echo ""
    echo "Build complete!"
    echo "Executable: $(pwd)/pawc"
else
    echo "Error: pawc binary not found"
    exit 1
fi
