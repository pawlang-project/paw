#!/bin/bash
# Setup LLVM source code for PawLang
# Downloads LLVM source and prepares for local compilation

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LLVM_VERSION="19.1.6"
LLVM_SRC_DIR="$PROJECT_ROOT/llvm/19.1.6"
LLVM_BUILD_DIR="$PROJECT_ROOT/llvm/build"
LLVM_INSTALL_DIR="$PROJECT_ROOT/llvm/install"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║        📦 LLVM Source Setup Script v$LLVM_VERSION             ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check if LLVM source already exists
check_existing_source() {
    if [ -d "$LLVM_SRC_DIR/llvm" ]; then
        echo "✅ LLVM source already exists: $LLVM_SRC_DIR"
        echo "   Size: $(du -sh "$LLVM_SRC_DIR" | cut -f1)"
        echo ""
        read -p "Re-download source? (y/N) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            echo "🗑️  Removing existing source..."
            rm -rf "$LLVM_SRC_DIR"
            return 1
        else
            echo "✅ Using existing source"
            return 0
        fi
    fi
    return 1
}

# Download LLVM source code
download_llvm_source() {
    echo "📥 Downloading LLVM $LLVM_VERSION source..."
    echo "   Target directory: $LLVM_SRC_DIR"
    echo ""
    
    # Create directory
    mkdir -p "$LLVM_SRC_DIR"
    cd "$LLVM_SRC_DIR"
    
    # Download using git (recommended)
    if command -v git >/dev/null 2>&1; then
        echo "🔗 Cloning LLVM project with Git..."
        git clone --depth 1 --branch "llvmorg-$LLVM_VERSION" \
            https://github.com/llvm/llvm-project.git .
        
        if [ $? -eq 0 ]; then
            echo "✅ Git clone completed"
        else
            echo "❌ Git clone failed, trying archive download..."
            download_llvm_archive
        fi
    else
        echo "⚠️  Git not installed, downloading archive..."
        download_llvm_archive
    fi
}

# Download LLVM as archive
download_llvm_archive() {
    local archive_url="https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-$LLVM_VERSION.tar.gz"
    local archive_name="llvmorg-$LLVM_VERSION.tar.gz"
    
    echo "📦 Downloading archive: $archive_url"
    
    # Download
    if command -v curl >/dev/null 2>&1; then
        curl -L --progress-bar -o "$archive_name" "$archive_url"
    elif command -v wget >/dev/null 2>&1; then
        wget --progress=bar -O "$archive_name" "$archive_url"
    else
        echo "❌ Need curl or wget to download"
        exit 1
    fi
    
    if [ $? -ne 0 ]; then
        echo "❌ Download failed"
        exit 1
    fi
    
    echo "📦 Extracting source..."
    tar -xzf "$archive_name"
    
    # Move contents to current directory
    mv "llvm-project-llvmorg-$LLVM_VERSION"/* .
    mv "llvm-project-llvmorg-$LLVM_VERSION"/.* . 2>/dev/null || true
    rmdir "llvm-project-llvmorg-$LLVM_VERSION"
    
    # Clean up
    rm -f "$archive_name"
    
    echo "✅ Source extraction completed"
}

# Verify LLVM source
verify_llvm_source() {
    echo "🔍 Verifying LLVM source..."
    
    if [ ! -d "$LLVM_SRC_DIR/llvm" ]; then
        echo "❌ LLVM source directory not found: $LLVM_SRC_DIR/llvm"
        exit 1
    fi
    
    if [ ! -f "$LLVM_SRC_DIR/llvm/CMakeLists.txt" ]; then
        echo "❌ LLVM CMakeLists.txt not found"
        exit 1
    fi
    
    echo "✅ LLVM source verification passed"
    echo "   Version: $LLVM_VERSION"
    echo "   Size: $(du -sh "$LLVM_SRC_DIR" | cut -f1)"
    echo "   Directory: $LLVM_SRC_DIR"
}

# Check build dependencies
check_build_dependencies() {
    echo "🔧 Checking build dependencies..."
    
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
        echo "Install dependencies:"
        case "$(uname -s)" in
            Darwin*)
                echo "  brew install cmake ninja"
                ;;
            Linux*)
                echo "  Ubuntu/Debian: sudo apt install cmake ninja-build build-essential"
                echo "  CentOS/RHEL: sudo yum install cmake ninja-build gcc-c++"
                echo "  Arch: sudo pacman -S cmake ninja gcc"
                ;;
            FreeBSD*)
                echo "  sudo pkg install cmake ninja gcc"
                ;;
            CYGWIN*|MINGW*|MSYS*)
                echo "  pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gcc"
                ;;
        esac
        exit 1
    fi
    
    echo "✅ Build dependencies check passed"
    echo "   CMake: $(cmake --version | head -1)"
    echo "   Ninja: $(ninja --version)"
    echo "   C++: $cxx_compiler"
}

# Create build configuration
create_build_config() {
    echo "⚙️  Creating build configuration..."
    
    # Create build directory
    mkdir -p "$LLVM_BUILD_DIR"
    mkdir -p "$LLVM_INSTALL_DIR"
    
    # Create build script
    cat > "$PROJECT_ROOT/scripts/build_llvm_local.sh" << 'EOF'
#!/bin/bash
# Build LLVM locally for PawLang

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LLVM_SRC="$PROJECT_ROOT/llvm/19.1.6"
LLVM_BUILD="$PROJECT_ROOT/llvm/build"
LLVM_INSTALL="$PROJECT_ROOT/llvm/install"

echo "🔨 开始构建 LLVM..."

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
echo "🔨 构建中... (使用 $CPU_COUNT 个核心)"
ninja -j "$CPU_COUNT"

# Install
echo "📦 安装中..."
ninja install

echo "✅ LLVM 构建完成!"
echo "   位置: $LLVM_INSTALL"
echo "   版本: $($LLVM_INSTALL/bin/llvm-config --version)"
EOF
    
    chmod +x "$PROJECT_ROOT/scripts/build_llvm_local.sh"
    
    echo "✅ Build script created: scripts/build_llvm_local.sh"
}

# Update build.zig for LLVM integration
update_build_zig() {
    echo "🔧 Updating build.zig configuration..."
    
    local build_zig="$PROJECT_ROOT/build.zig"
    
    # Check if LLVM integration already exists
    if grep -q "with-llvm" "$build_zig"; then
        echo "✅ build.zig already contains LLVM configuration"
        return 0
    fi
    
    # Add LLVM integration to build.zig
    cat >> "$build_zig" << 'EOF'

    // LLVM integration for PawLang
    if (b.option(bool, "with-llvm", "Enable LLVM backend (requires local LLVM build)") orelse false) {
        const local_llvm = "llvm/install";
        
        // Check if local LLVM exists
        const llvm_config_path = b.fmt("{s}/bin/llvm-config", .{local_llvm});
        if (std.fs.cwd().access(llvm_config_path, .{})) {
            std.debug.print("✓ Using local LLVM from {s}\n", .{local_llvm});
            
            // Add LLVM paths
            exe.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/lib", .{local_llvm}) });
            exe.addIncludePath(.{ .cwd_relative = b.fmt("{s}/include", .{local_llvm}) });
            exe.linkSystemLibrary("LLVM");
            
            // Add llvm-zig module
            const llvm_dep = b.dependency("llvm", .{
                .target = target,
                .optimize = optimize,
            });
            const llvm_mod = llvm_dep.module("llvm");
            exe.root_module.addImport("llvm", llvm_mod);
        } else |_| {
            std.debug.print("⚠️  Local LLVM not found. Build it first:\n", .{});
            std.debug.print("   ./scripts/build_llvm_local.sh\n", .{});
        }
    }
EOF
    
    echo "✅ build.zig updated"
}

# Main execution
main() {
    echo "🎯 Setting up LLVM source environment"
    echo ""
    
    # Check existing source
    if ! check_existing_source; then
        # Download source
        download_llvm_source
    fi
    
    # Verify source
    verify_llvm_source
    echo ""
    
    # Check dependencies
    check_build_dependencies
    echo ""
    
    # Create build configuration
    create_build_config
    echo ""
    
    # Update build.zig
    update_build_zig
    echo ""
    
    echo "════════════════════════════════════════════════════════════════"
    echo "✅ LLVM source setup completed!"
    echo "════════════════════════════════════════════════════════════════"
    echo ""
    echo "📊 Setup information:"
    echo "   Version: $LLVM_VERSION"
    echo "   Source: $LLVM_SRC_DIR"
    echo "   Build: $LLVM_BUILD_DIR"
    echo "   Install: $LLVM_INSTALL_DIR"
    echo ""
    echo "🎯 Next steps:"
    echo "   1. Build LLVM:"
    echo "      ./scripts/build_llvm_local.sh"
    echo ""
    echo "   2. Build PawLang (with LLVM):"
    echo "      zig build -Dwith-llvm=true"
    echo ""
    echo "   3. Test LLVM backend:"
    echo "      ./zig-out/bin/pawc hello.paw --backend=llvm"
    echo ""
}

# Run main function
main
