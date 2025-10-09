#!/bin/bash
# Build LLVM from source in llvm/19.1.6
# This creates a local LLVM installation for PawLang

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LLVM_SRC="$PROJECT_ROOT/llvm/19.1.6"
LLVM_BUILD="$PROJECT_ROOT/llvm/build"
LLVM_INSTALL="$PROJECT_ROOT/llvm/install"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║           Building LLVM 19.1.6 from source                   ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check if source exists
if [ ! -d "$LLVM_SRC/llvm" ]; then
    echo "❌ Error: LLVM source not found at $LLVM_SRC/llvm"
    exit 1
fi

echo "📁 Source: $LLVM_SRC"
echo "📦 Build:  $LLVM_BUILD"
echo "🎯 Install: $LLVM_INSTALL"
echo ""

# Check if already built
if [ -d "$LLVM_INSTALL/bin" ] && [ -f "$LLVM_INSTALL/bin/llvm-config" ]; then
    echo "✅ LLVM already built at $LLVM_INSTALL"
    echo ""
    "$LLVM_INSTALL/bin/llvm-config" --version
    echo ""
    read -p "Rebuild? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "✅ Using existing build"
        exit 0
    fi
    echo "🗑️  Removing old build..."
    rm -rf "$LLVM_BUILD" "$LLVM_INSTALL"
fi

# Create build directory
mkdir -p "$LLVM_BUILD"
cd "$LLVM_BUILD"

echo "⚙️  Configuring LLVM..."
echo ""

# Configure with CMake
# Build only what we need for PawLang
cmake "$LLVM_SRC/llvm" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$LLVM_INSTALL" \
    -DLLVM_ENABLE_PROJECTS="clang" \
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

if [ $? -ne 0 ]; then
    echo "❌ CMake configuration failed"
    echo "   Make sure you have cmake and ninja installed:"
    echo "   brew install cmake ninja"
    exit 1
fi

echo ""
echo "🔨 Building LLVM (this will take 30-60 minutes)..."
echo "   CPU cores: $(sysctl -n hw.ncpu)"
echo "   Using all cores for parallel build"
echo ""

ninja

if [ $? -ne 0 ]; then
    echo "❌ Build failed"
    exit 1
fi

echo ""
echo "📦 Installing to $LLVM_INSTALL..."
ninja install

if [ $? -ne 0 ]; then
    echo "❌ Installation failed"
    exit 1
fi

echo ""
echo "════════════════════════════════════════════════════════════════"
echo "✅ LLVM built successfully!"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "📊 Installation info:"
echo "   Location: $LLVM_INSTALL"
echo "   Version: $("$LLVM_INSTALL/bin/llvm-config" --version)"
echo "   Size: $(du -sh "$LLVM_INSTALL" | cut -f1)"
echo ""
echo "🔗 Library paths:"
echo "   Include: $LLVM_INSTALL/include"
echo "   Lib: $LLVM_INSTALL/lib"
echo ""
echo "🎯 Next steps:"
echo "   1. Update build.zig to use $LLVM_INSTALL"
echo "   2. Run: zig build -Dwith-llvm=true"
echo ""

