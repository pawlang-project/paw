#!/bin/bash
set -e

echo "=========================================="
echo "QEMU Test Script for PawLang"
echo "Architecture: $(uname -m)"
echo "=========================================="

# Update and install dependencies
export DEBIAN_FRONTEND=noninteractive
apt-get update -q
apt-get install -y wget curl xz-utils

# Install LLVM
echo ""
echo "Installing LLVM..."
apt-get install -y llvm-13 llvm-13-dev clang-13 || \
apt-get install -y llvm llvm-dev clang || {
  echo "Warning: LLVM installation failed, continuing anyway..."
}

# Install Zig
echo ""
echo "Installing Zig..."
wget -q https://ziglang.org/download/0.15.1/zig-linux-x86_64-0.15.1.tar.xz
tar -xf zig-linux-x86_64-0.15.1.tar.xz
mv zig-linux-x86_64-0.15.1 /usr/local/zig
ln -s /usr/local/zig/zig /usr/local/bin/zig

# Verify tools
echo ""
echo "Zig version:"
zig version
echo ""
echo "LLVM version:"
llvm-config-13 --version || llvm-config --version || echo "LLVM not available"

# Build
echo ""
echo "=========================================="
echo "Building PawLang compiler..."
echo "=========================================="
zig build

# Test 1: C backend
echo ""
echo "Test 1: C backend"
./zig-out/bin/pawc examples/hello.paw --backend=c
if [ -f output.c ]; then
  echo "✅ C backend test passed"
  rm output.c
else
  echo "❌ C backend test failed"
  exit 1
fi

# Test 2: Type casting
echo ""
echo "Test 2: Type casting"
./zig-out/bin/pawc examples/type_cast_demo.paw --backend=c
if [ -f output.c ]; then
  echo "✅ Type casting test passed"
  rm output.c
else
  echo "❌ Type casting test failed"
  exit 1
fi

# Test 3: Integration test
echo ""
echo "Test 3: Integration test"
./zig-out/bin/pawc tests/integration/v0.1.8_test.paw --backend=c
if [ -f output.c ]; then
  echo "✅ Integration test passed"
  rm output.c
else
  echo "❌ Integration test failed"
  exit 1
fi

# Test 4: LLVM backend
echo ""
echo "Test 4: LLVM backend"
./zig-out/bin/pawc examples/hello.paw --backend=llvm
if [ -f output.ll ]; then
  echo "✅ LLVM backend test passed"
  rm output.ll
else
  echo "❌ LLVM backend test failed"
  exit 1
fi

# Test 5: Self-contained distribution
echo ""
echo "=========================================="
echo "Test 5: Self-contained distribution"
echo "=========================================="
zig build package

# Uninstall LLVM
echo "Uninstalling system LLVM..."
apt-get remove -y llvm-13 llvm-13-dev clang-13 llvm llvm-dev clang 2>/dev/null || true
rm -rf /usr/lib/llvm* 2>/dev/null || true

# Create test directory
mkdir -p /tmp/pawlang-test
cd /tmp/pawlang-test
tar -xzf /workspace/pawlang-linux.tar.gz

# Set library path
export LD_LIBRARY_PATH="$PWD/lib:$LD_LIBRARY_PATH"

# Test C backend without system LLVM
echo ""
echo "Test 5a: C backend without system LLVM"
./bin/pawc /workspace/examples/hello.paw --backend=c
if [ -f output.c ]; then
  echo "✅ Self-contained C backend works"
  rm output.c
else
  echo "❌ Self-contained C backend failed"
  exit 1
fi

# Test LLVM backend without system LLVM (critical!)
echo ""
echo "Test 5b: LLVM backend without system LLVM"
./bin/pawc /workspace/examples/hello.paw --backend=llvm
if [ -f output.ll ]; then
  echo "✅ Self-contained LLVM backend works"
  rm output.ll
else
  echo "❌ Self-contained LLVM backend failed"
  ls -la lib/ || true
  exit 1
fi

# Verify bundled libraries
echo ""
echo "Test 5c: Check bundled libraries"
LIB_COUNT=$(ls lib/*.so* 2>/dev/null | wc -l || echo "0")
echo "Bundled libraries: $LIB_COUNT"
if [ "$LIB_COUNT" -gt 0 ]; then
  echo "✅ Libraries properly bundled"
  ls -lh lib/*.so* 2>/dev/null || true
else
  echo "⚠️  Warning: No libraries found"
fi

echo ""
echo "=========================================="
echo "✅ All tests passed!"
echo "   • C backend ✅"
echo "   • LLVM backend ✅"
echo "   • Type casting ✅"
echo "   • Integration test ✅"
echo "   • Self-contained distribution ✅"
echo "=========================================="

