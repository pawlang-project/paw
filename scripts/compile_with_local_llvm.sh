#!/bin/bash
# Compile PawLang programs using local LLVM toolchain
# Usage: ./scripts/compile_with_local_llvm.sh <file.paw> [output_name]

set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 <file.paw> [output_name]"
    echo ""
    echo "Example:"
    echo "  $0 hello.paw"
    echo "  $0 hello.paw my_program"
    exit 1
fi

SOURCE_FILE="$1"
OUTPUT_NAME="${2:-output}"

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PAWC="$PROJECT_ROOT/zig-out/bin/pawc"
CLANG="$PROJECT_ROOT/llvm/install/bin/clang"
LLC="$PROJECT_ROOT/llvm/install/bin/llc"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║        PawLang + Local LLVM Compilation                      ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check if pawc exists
if [ ! -f "$PAWC" ]; then
    echo "❌ PawLang compiler not found: $PAWC"
    echo "💡 Build it with: zig build -Dwith-llvm=true"
    exit 1
fi

# Check if local clang exists
if [ ! -f "$CLANG" ]; then
    echo "❌ Local Clang not found: $CLANG"
    echo "💡 Build LLVM first:"
    echo "   ./scripts/setup_llvm_source.sh"
    echo "   ./scripts/build_llvm_local.sh"
    exit 1
fi

echo "📝 Source: $SOURCE_FILE"
echo "🎯 Output: $OUTPUT_NAME"
echo ""

# Step 1: Compile PawLang to LLVM IR
echo "🔨 Step 1: PawLang → LLVM IR"
"$PAWC" "$SOURCE_FILE" --backend=llvm-native
if [ $? -ne 0 ]; then
    echo "❌ PawLang compilation failed"
    exit 1
fi
echo "✅ Generated output.ll"
echo ""

# Step 2: Compile LLVM IR to executable
echo "🔨 Step 2: LLVM IR → Executable"

# Detect OS and set appropriate flags
case "$(uname -s)" in
    Darwin*)
        # macOS
        SDK_PATH="/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib"
        if [ -d "$SDK_PATH" ]; then
            "$CLANG" output.ll -o "$OUTPUT_NAME" -L"$SDK_PATH" -lSystem
        else
            # Fallback: use system clang for linking
            echo "⚠️  SDK not found, using system linker"
            "$LLC" output.ll -o output.s
            gcc output.s -o "$OUTPUT_NAME"
            rm -f output.s
        fi
        ;;
    Linux*)
        # Linux
        "$CLANG" output.ll -o "$OUTPUT_NAME"
        ;;
    *)
        # Other Unix-like
        "$LLC" output.ll -o output.s
        gcc output.s -o "$OUTPUT_NAME"
        rm -f output.s
        ;;
esac

if [ $? -ne 0 ]; then
    echo "❌ LLVM compilation failed"
    exit 1
fi

echo "✅ Generated $OUTPUT_NAME"
echo ""

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║        ✅ Compilation Complete!                              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "🚀 Run your program:"
echo "   ./$OUTPUT_NAME"
echo ""
echo "📊 Using:"
echo "   PawLang: $PAWC"
echo "   Clang: $CLANG ($($CLANG --version | head -1))"
echo ""

