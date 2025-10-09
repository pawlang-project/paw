#!/bin/bash
# Download precompiled LLVM to vendor directory
# This makes the project self-contained without polluting the system

set -e

VENDOR_DIR="$(dirname "$0")/../vendor"
mkdir -p "$VENDOR_DIR"
cd "$VENDOR_DIR"

echo "🔍 Detecting platform..."
ARCH=$(uname -m)
OS=$(uname -s)

if [ "$OS" = "Darwin" ]; then
    if [ "$ARCH" = "arm64" ]; then
        PLATFORM="arm64-apple-darwin23.0"
        LLVM_URL="https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.3/clang+llvm-19.1.3-arm64-apple-darwin22.0.tar.xz"
    else
        PLATFORM="x86_64-apple-darwin"
        LLVM_URL="https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.3/clang+llvm-19.1.3-x86_64-apple-darwin.tar.xz"
    fi
elif [ "$OS" = "Linux" ]; then
    if [ "$ARCH" = "x86_64" ]; then
        PLATFORM="x86_64-linux-gnu-ubuntu-22.04"
        LLVM_URL="https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.3/clang+llvm-19.1.3-x86_64-linux-gnu-ubuntu-22.04.tar.xz"
    else
        echo "❌ Unsupported architecture: $ARCH on $OS"
        exit 1
    fi
else
    echo "❌ Unsupported OS: $OS"
    exit 1
fi

echo "📦 Platform: $PLATFORM"
echo "🌐 Downloading LLVM 19.1.3..."
echo "   URL: $LLVM_URL"
echo ""

# Check if already downloaded
if [ -d "llvm" ]; then
    echo "⚠️  LLVM already exists in vendor/llvm"
    read -p "   Remove and re-download? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "✅ Using existing LLVM"
        exit 0
    fi
    rm -rf llvm
fi

# Download with wget or curl
LLVM_FILE="llvm-19.tar.xz"

if command -v wget &> /dev/null; then
    wget -O "$LLVM_FILE" "$LLVM_URL"
elif command -v curl &> /dev/null; then
    curl -L -o "$LLVM_FILE" "$LLVM_URL"
else
    echo "❌ Error: Neither wget nor curl found"
    echo "   Please install wget or curl to download LLVM"
    exit 1
fi

# Check download
if [ ! -f "$LLVM_FILE" ]; then
    echo "❌ Download failed"
    exit 1
fi

SIZE=$(du -h "$LLVM_FILE" | cut -f1)
if [ "$SIZE" = "9B" ] || [ "$SIZE" = "4.0K" ]; then
    echo "❌ Download failed (got HTML redirect page)"
    echo ""
    echo "📝 Please download manually from:"
    echo "   https://github.com/llvm/llvm-project/releases/tag/llvmorg-19.1.3"
    echo ""
    echo "   Look for: clang+llvm-19.1.3-$PLATFORM.tar.xz"
    echo "   Save to: $VENDOR_DIR/llvm-19.tar.xz"
    echo ""
    echo "   Then run: tar xf llvm-19.tar.xz && mv clang+llvm-* llvm"
    rm -f "$LLVM_FILE"
    exit 1
fi

echo "📦 Downloaded: $SIZE"
echo "📂 Extracting..."

tar xf "$LLVM_FILE"
rm -f "$LLVM_FILE"

# Rename to simple 'llvm' directory
mv clang+llvm-* llvm 2>/dev/null || true

if [ -d "llvm" ]; then
    echo "✅ LLVM installed to: vendor/llvm"
    echo ""
    echo "📊 Contents:"
    du -sh llvm
    ls -lh llvm/
    echo ""
    echo "🎉 Done! LLVM is now integrated into the project."
    echo ""
    echo "Next steps:"
    echo "  1. Update build.zig to use vendor/llvm"
    echo "  2. Test: zig build -Dwith-llvm=true"
else
    echo "❌ Extraction failed"
    exit 1
fi

