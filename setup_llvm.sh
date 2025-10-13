#!/bin/bash
# LLVM Auto-Download Script
# Detects platform and downloads corresponding pre-compiled LLVM

set -e

LLVM_VERSION="21.1.3"
BASE_URL="https://github.com/pawlang-project/llvm-build/releases/download/llvm-${LLVM_VERSION}"
VENDOR_DIR="vendor/llvm"

# Support non-interactive mode with --yes flag
AUTO_YES=false
if [ "$1" = "--yes" ] || [ "$1" = "-y" ]; then
  AUTO_YES=true
fi

echo "=========================================="
echo "   LLVM Auto-Download"
echo "=========================================="
echo ""

# Detect platform
PLATFORM=$(uname -s | tr '[:upper:]' '[:lower:]')
ARCH=$(uname -m)

# Map to target name
case "$PLATFORM-$ARCH" in
  darwin-arm64)
    TARGET="macos-aarch64"
    ;;
  darwin-x86_64)
    TARGET="macos-x86_64"
    ;;
  linux-x86_64)
    TARGET="linux-x86_64"
    ;;
  linux-aarch64)
    TARGET="linux-aarch64"
    ;;
  linux-armv7*)
    TARGET="linux-arm"
    ;;
  linux-riscv64)
    TARGET="linux-riscv64"
    ;;
  linux-ppc64le)
    TARGET="linux-powerpc64le"
    ;;
  linux-loongarch64)
    TARGET="linux-loongarch64"
    ;;
  linux-s390x)
    TARGET="linux-s390x"
    ;;
  mingw*|msys*|cygwin*)
    TARGET="windows-x86_64"
    ;;
  *)
    echo "❌ Unsupported platform: $PLATFORM-$ARCH"
    echo ""
    echo "Supported platforms:"
    echo "  • macOS: arm64, x86_64"
    echo "  • Linux: x86_64, aarch64, arm, riscv64, powerpc64le, loongarch64, s390x"
    echo "  • Windows: x86_64, aarch64"
    exit 1
    ;;
esac

echo "Detected platform: $TARGET"
echo ""

# Create vendor directory if it doesn't exist
mkdir -p "$VENDOR_DIR"

# Check if already exists
INSTALL_DIR="$VENDOR_DIR/$TARGET"
if [ -d "$INSTALL_DIR/install" ]; then
  echo "⚠️  LLVM already exists: $INSTALL_DIR/install/"
  if [ "$AUTO_YES" = false ]; then
    read -p "Re-download? [y/N]: " confirm
    if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
      echo "Cancelled"
      exit 0
    fi
  else
    echo "🔄 Auto-yes mode: Re-downloading..."
  fi
  rm -rf "$INSTALL_DIR"
fi

# Download
FILENAME="llvm-${LLVM_VERSION}-${TARGET}.tar.gz"
URL="${BASE_URL}/${FILENAME}"

echo "📥 Downloading LLVM ${LLVM_VERSION} for ${TARGET}..."
echo "   URL: $URL"
echo ""

if command -v wget > /dev/null; then
  wget -O "$FILENAME" "$URL"
elif command -v curl > /dev/null; then
  curl -L -o "$FILENAME" "$URL"
else
  echo "❌ Missing wget or curl"
  exit 1
fi

echo ""
echo "📦 Extracting to $INSTALL_DIR/install..."

mkdir -p "$INSTALL_DIR/install"
tar xzf "$FILENAME" -C "$INSTALL_DIR/install"/

rm "$FILENAME"

echo ""
echo "=========================================="
echo "   ✅ LLVM Installed Successfully!"
echo "=========================================="
echo ""
echo "Installation location: $INSTALL_DIR/install/"
echo ""
echo "Included tools:"
echo "  • clang - C/C++ compiler"
echo "  • llc - LLVM compiler"
echo "  • lld - LLVM linker"
echo ""
echo "Verify installation:"
echo "  $INSTALL_DIR/install/bin/clang --version"
echo ""
echo "🚀 Now you can build PawLang:"
echo "   zig build"
echo ""
echo "Use LLVM backend:"
echo "   ./zig-out/bin/pawc hello.paw --backend=llvm"
echo ""

