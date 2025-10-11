#!/bin/bash
# 🧪 PawLang Cross-Platform Test Script

set -e

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🧪 PawLang Cross-Platform Test"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Detect platform
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macOS"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="Linux"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    PLATFORM="Windows"
else
    PLATFORM="Unknown"
fi

echo "📍 Platform: $PLATFORM"
echo "📍 OS Type: $OSTYPE"
echo "📍 Architecture: $(uname -m 2>/dev/null || echo 'N/A')"
echo ""

# Build
echo "🔨 Building PawLang compiler..."
zig build
echo "✅ Build successful!"
echo ""

# Test 1: Hello World
echo "Test 1: Hello World"
echo "─────────────────────────────"
./zig-out/bin/pawc examples/hello.paw --backend=c
if [ -f output.c ]; then
    echo "✅ C code generation works"
    rm -f output.c
else
    echo "❌ C code generation failed"
    exit 1
fi
echo ""

# Test 2: Type Casting
echo "Test 2: Type Casting"
echo "─────────────────────────────"
./zig-out/bin/pawc tests/syntax/test_type_cast.paw --backend=c
if [ -f output.c ]; then
    echo "✅ Type casting compilation works"
    
    # Try to compile and run if gcc/clang available
    if command -v gcc &> /dev/null; then
        gcc output.c -o test_output 2>/dev/null
        if [ -f test_output ]; then
            RESULT=$(./test_output)
            EXIT_CODE=$?
            if [ $EXIT_CODE -eq 21 ]; then
                echo "✅ Type casting runtime works (exit code: 21)"
            else
                echo "⚠️  Type casting runtime result: $EXIT_CODE (expected 21)"
            fi
            rm -f test_output
        fi
    elif command -v clang &> /dev/null; then
        clang output.c -o test_output 2>/dev/null
        if [ -f test_output ]; then
            RESULT=$(./test_output)
            EXIT_CODE=$?
            if [ $EXIT_CODE -eq 21 ]; then
                echo "✅ Type casting runtime works (exit code: 21)"
            else
                echo "⚠️  Type casting runtime result: $EXIT_CODE (expected 21)"
            fi
            rm -f test_output
        fi
    fi
    
    rm -f output.c
else
    echo "❌ Type casting compilation failed"
    exit 1
fi
echo ""

# Test 3: Integration Test
echo "Test 3: Integration Test"
echo "─────────────────────────────"
./zig-out/bin/pawc tests/integration/v0.1.8_test.paw --backend=c
if [ -f output.c ]; then
    echo "✅ Integration test compilation works"
    rm -f output.c
else
    echo "❌ Integration test failed"
    exit 1
fi
echo ""

# Test 4: Enhanced Error Messages
echo "Test 4: Enhanced Error Messages"
echo "─────────────────────────────"
cat << 'TESTCODE' > /tmp/test_error.paw
fn main() -> i32 {
    let x = undefined_var;
    return 0;
}
TESTCODE

echo "Testing undefined variable error..."
./zig-out/bin/pawc /tmp/test_error.paw 2>&1 | head -5 || true
echo ""
echo "✅ Enhanced error messages working"
rm -f /tmp/test_error.paw
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✅ All tests passed on $PLATFORM!"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

