#!/bin/bash
# Cross-platform LLVM build script for PawLang
# Supports: macOS, Linux, Windows (WSL/MSYS2), FreeBSD

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LLVM_SRC="$PROJECT_ROOT/llvm/19.1.6"
LLVM_BUILD="$PROJECT_ROOT/llvm/build"
LLVM_INSTALL="$PROJECT_ROOT/llvm/install"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║        🌍 跨平台 LLVM 19.1.6 构建脚本                        ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Detect platform and architecture
detect_platform() {
    local os=""
    local arch=""
    local platform=""
    
    case "$(uname -s)" in
        Darwin*)
            os="macOS"
            case "$(uname -m)" in
                arm64) arch="ARM64" ;;
                x86_64) arch="X64" ;;
                *) echo "❌ Unsupported macOS architecture: $(uname -m)"; exit 1 ;;
            esac
            ;;
        Linux*)
            os="Linux"
            case "$(uname -m)" in
                x86_64) arch="X64" ;;
                aarch64) arch="ARM64" ;;
                armv7l) arch="ARM32" ;;
                *) echo "❌ Unsupported Linux architecture: $(uname -m)"; exit 1 ;;
            esac
            ;;
        CYGWIN*|MINGW*|MSYS*)
            os="Windows"
            case "$(uname -m)" in
                x86_64) arch="X64" ;;
                i686) arch="X86" ;;
                *) echo "❌ Unsupported Windows architecture: $(uname -m)"; exit 1 ;;
            esac
            ;;
        FreeBSD*)
            os="FreeBSD"
            case "$(uname -m)" in
                amd64) arch="X64" ;;
                arm64) arch="ARM64" ;;
                *) echo "❌ Unsupported FreeBSD architecture: $(uname -m)"; exit 1 ;;
            esac
            ;;
        *)
            echo "❌ Unsupported OS: $(uname -s)"
            exit 1
            ;;
    esac
    
    platform="$os-$arch"
    echo "$platform"
}

# Get CPU count for parallel builds
get_cpu_count() {
    case "$(uname -s)" in
        Darwin*|FreeBSD*)
            sysctl -n hw.ncpu
            ;;
        Linux*)
            nproc
            ;;
        CYGWIN*|MINGW*|MSYS*)
            # Windows/WSL
            if command -v nproc >/dev/null 2>&1; then
                nproc
            else
                echo 4  # fallback
            fi
            ;;
        *)
            echo 4  # fallback
            ;;
    esac
}

# Check build dependencies
check_dependencies() {
    local missing=()
    
    # Check CMake
    if ! command -v cmake >/dev/null 2>&1; then
        missing+=("cmake")
    fi
    
    # Check Ninja
    if ! command -v ninja >/dev/null 2>&1; then
        missing+=("ninja")
    fi
    
    # Check C++ compiler
    local cxx_compiler=""
    case "$(uname -s)" in
        Darwin*)
            if command -v clang++ >/dev/null 2>&1; then
                cxx_compiler="clang++"
            elif command -v g++ >/dev/null 2>&1; then
                cxx_compiler="g++"
            else
                missing+=("C++ compiler (clang++ or g++)")
            fi
            ;;
        Linux*|FreeBSD*)
            if command -v g++ >/dev/null 2>&1; then
                cxx_compiler="g++"
            elif command -v clang++ >/dev/null 2>&1; then
                cxx_compiler="clang++"
            else
                missing+=("C++ compiler (g++ or clang++)")
            fi
            ;;
        CYGWIN*|MINGW*|MSYS*)
            if command -v g++ >/dev/null 2>&1; then
                cxx_compiler="g++"
            else
                missing+=("C++ compiler (g++)")
            fi
            ;;
    esac
    
    if [ ${#missing[@]} -gt 0 ]; then
        echo "❌ Missing dependencies: ${missing[*]}"
        echo ""
        echo "Install missing dependencies:"
        case "$(uname -s)" in
            Darwin*)
                echo "  macOS: brew install cmake ninja"
                ;;
            Linux*)
                echo "  Ubuntu/Debian: sudo apt install cmake ninja-build build-essential"
                echo "  CentOS/RHEL: sudo yum install cmake ninja-build gcc-c++"
                echo "  Arch: sudo pacman -S cmake ninja gcc"
                ;;
            FreeBSD*)
                echo "  FreeBSD: sudo pkg install cmake ninja gcc"
                ;;
            CYGWIN*|MINGW*|MSYS*)
                echo "  Windows (MSYS2): pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gcc"
                echo "  Windows (WSL): sudo apt install cmake ninja-build build-essential"
                ;;
        esac
        exit 1
    fi
    
    echo "✅ Dependencies check passed"
    echo "   CMake: $(cmake --version | head -1)"
    echo "   Ninja: $(ninja --version)"
    echo "   C++: $cxx_compiler"
}

# Get platform-specific CMake options
get_cmake_options() {
    local platform="$1"
    local cpu_count="$2"
    
    local cmake_options=(
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_INSTALL_PREFIX=$LLVM_INSTALL"
        "-DLLVM_ENABLE_PROJECTS=clang"
        "-DLLVM_TARGETS_TO_BUILD=AArch64;X86"
        "-DLLVM_ENABLE_ASSERTIONS=OFF"
        "-DLLVM_ENABLE_RTTI=ON"
        "-DLLVM_BUILD_TOOLS=ON"
        "-DLLVM_BUILD_EXAMPLES=OFF"
        "-DLLVM_BUILD_TESTS=OFF"
        "-DLLVM_INCLUDE_TESTS=OFF"
        "-DLLVM_INCLUDE_EXAMPLES=OFF"
        "-DLLVM_INCLUDE_DOCS=OFF"
        "-DLLVM_ENABLE_BINDINGS=OFF"
        "-G Ninja"
    )
    
    # Platform-specific options
    case "$platform" in
        "macOS-"*)
            # macOS specific
            cmake_options+=("-DCMAKE_OSX_DEPLOYMENT_TARGET=10.15")
            if [[ "$platform" == *"ARM64"* ]]; then
                cmake_options+=("-DCMAKE_OSX_ARCHITECTURES=arm64")
            else
                cmake_options+=("-DCMAKE_OSX_ARCHITECTURES=x86_64")
            fi
            ;;
        "Linux-"*)
            # Linux specific
            cmake_options+=("-DCMAKE_CXX_STANDARD=17")
            ;;
        "Windows-"*)
            # Windows specific
            cmake_options+=("-DCMAKE_CXX_STANDARD=17")
            cmake_options+=("-DLLVM_USE_CRT_RELEASE=MT")
            ;;
        "FreeBSD-"*)
            # FreeBSD specific
            cmake_options+=("-DCMAKE_CXX_STANDARD=17")
            ;;
    esac
    
    # Parallel build
    cmake_options+=("-DLLVM_PARALLEL_LINK_JOBS=$cpu_count")
    
    printf '%s\n' "${cmake_options[@]}"
}

# Main build function
main() {
    # Detect platform
    local platform=$(detect_platform)
    local cpu_count=$(get_cpu_count)
    
    echo "🖥️  Platform: $platform"
    echo "🔧 CPU cores: $cpu_count"
    echo "📁 Source: $LLVM_SRC"
    echo "📦 Build:  $LLVM_BUILD"
    echo "🎯 Install: $LLVM_INSTALL"
    echo ""
    
    # Check dependencies
    check_dependencies
    echo ""
    
    # Check if source exists
    if [ ! -d "$LLVM_SRC/llvm" ]; then
        echo "❌ Error: LLVM source not found at $LLVM_SRC/llvm"
        echo "   Please ensure LLVM 19.1.6 source is available"
        exit 1
    fi
    
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
    
    echo "⚙️  Configuring LLVM for $platform..."
    echo ""
    
    # Get CMake options
    local cmake_options=($(get_cmake_options "$platform" "$cpu_count"))
    
    # Configure with CMake
    cmake "$LLVM_SRC/llvm" "${cmake_options[@]}"
    
    if [ $? -ne 0 ]; then
        echo "❌ CMake configuration failed"
        echo "   Platform: $platform"
        echo "   Options: ${cmake_options[*]}"
        exit 1
    fi
    
    echo ""
    echo "🔨 Building LLVM (this will take 30-90 minutes)..."
    echo "   Platform: $platform"
    echo "   CPU cores: $cpu_count"
    echo "   Using parallel build"
    echo ""
    
    # Build with parallel jobs
    ninja -j "$cpu_count"
    
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
    echo "✅ LLVM built successfully for $platform!"
    echo "════════════════════════════════════════════════════════════════"
    echo ""
    echo "📊 Installation info:"
    echo "   Platform: $platform"
    echo "   Location: $LLVM_INSTALL"
    echo "   Version: $("$LLVM_INSTALL/bin/llvm-config" --version)"
    echo "   Size: $(du -sh "$LLVM_INSTALL" | cut -f1)"
    echo ""
    echo "🔗 Library paths:"
    echo "   Include: $LLVM_INSTALL/include"
    echo "   Lib: $LLVM_INSTALL/lib"
    echo ""
    echo "🎯 Next steps:"
    echo "   1. Build PawLang with LLVM:"
    echo "      zig build -Dwith-llvm=true"
    echo ""
    echo "   2. Test LLVM backend:"
    echo "      ./zig-out/bin/pawc hello.paw --backend=llvm"
    echo ""
}

# Run main function
main
