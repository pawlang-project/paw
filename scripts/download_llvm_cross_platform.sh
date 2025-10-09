#!/bin/bash
# Cross-platform LLVM prebuilt download script for PawLang
# Supports: macOS, Linux, Windows (WSL/MSYS2), FreeBSD

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LLVM_VERSION="19.1.6"
DOWNLOAD_DIR="$PROJECT_ROOT/llvm"
INSTALL_DIR="$DOWNLOAD_DIR/install"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║      🌍 跨平台预编译 LLVM $LLVM_VERSION 下载脚本                ║"
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
            os="win"
            case "$(uname -m)" in
                x86_64) arch="64" ;;
                i686) arch="32" ;;
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

# Get download URL for platform
get_download_url() {
    local platform="$1"
    local base_url="https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION"
    
    case "$platform" in
        "macOS-ARM64")
            echo "$base_url/LLVM-$LLVM_VERSION-macOS-ARM64.tar.xz"
            ;;
        "macOS-X64")
            echo "$base_url/LLVM-$LLVM_VERSION-macOS-X64.tar.xz"
            ;;
        "Linux-X64")
            echo "$base_url/LLVM-$LLVM_VERSION-Linux-X64.tar.xz"
            ;;
        "Linux-ARM64")
            echo "$base_url/LLVM-$LLVM_VERSION-Linux-ARM64.tar.xz"
            ;;
        "win-64")
            echo "$base_url/LLVM-$LLVM_VERSION-win64.exe"
            ;;
        "win-32")
            echo "$base_url/LLVM-$LLVM_VERSION-win32.exe"
            ;;
        "FreeBSD-X64"|"FreeBSD-ARM64")
            echo "❌ Prebuilt LLVM not available for FreeBSD"
            echo "   Please use source build: ./scripts/build_llvm_cross_platform.sh"
            exit 1
            ;;
        *)
            echo "❌ Unsupported platform: $platform"
            echo "   Available platforms:"
            echo "   - macOS (ARM64, X64)"
            echo "   - Linux (X64, ARM64)"
            echo "   - Windows (32, 64)"
            echo "   - FreeBSD (use source build)"
            exit 1
            ;;
    esac
}

# Download with progress
download_file() {
    local url="$1"
    local output="$2"
    
    echo "📥 Downloading: $url"
    echo "💾 To: $output"
    echo ""
    
    if command -v curl >/dev/null 2>&1; then
        curl -L --progress-bar -o "$output" "$url"
    elif command -v wget >/dev/null 2>&1; then
        wget --progress=bar -O "$output" "$url"
    else
        echo "❌ Need curl or wget to download"
        exit 1
    fi
    
    if [ $? -eq 0 ]; then
        echo "✅ Download completed"
    else
        echo "❌ Download failed"
        exit 1
    fi
}

# Extract archive
extract_archive() {
    local archive="$1"
    local extract_to="$2"
    
    echo "📦 Extracting: $(basename "$archive")"
    
    case "$archive" in
        *.tar.xz)
            if command -v tar >/dev/null 2>&1; then
                tar -xf "$archive" -C "$extract_to"
            else
                echo "❌ Need tar to extract .tar.xz"
                exit 1
            fi
            ;;
        *.exe)
            echo "ℹ️  Windows installer detected"
            echo "   Please run the installer manually and set install path to:"
            echo "   $INSTALL_DIR"
            return 0
            ;;
        *)
            echo "❌ Unknown archive format: $archive"
            exit 1
            ;;
    esac
    
    echo "✅ Extraction completed"
}

# Verify installation
verify_installation() {
    local llvm_config="$INSTALL_DIR/bin/llvm-config"
    
    if [ -f "$llvm_config" ]; then
        echo "✅ LLVM installation verified"
        echo "   Version: $($llvm_config --version)"
        echo "   Location: $INSTALL_DIR"
        echo ""
        
        # Show size
        local size=$(du -sh "$INSTALL_DIR" | cut -f1)
        echo "📊 Installation size: $size"
        
        return 0
    else
        echo "❌ LLVM installation verification failed"
        echo "   Expected: $llvm_config"
        return 1
    fi
}

# Check dependencies
check_dependencies() {
    local missing=()
    
    if ! command -v curl >/dev/null 2>&1 && ! command -v wget >/dev/null 2>&1; then
        missing+=("curl or wget")
    fi
    
    if ! command -v tar >/dev/null 2>&1; then
        missing+=("tar")
    fi
    
    if [ ${#missing[@]} -gt 0 ]; then
        echo "❌ Missing dependencies: ${missing[*]}"
        echo ""
        echo "Install missing dependencies:"
        case "$(uname -s)" in
            Darwin*)
                echo "  macOS: brew install curl tar"
                ;;
            Linux*)
                echo "  Ubuntu/Debian: sudo apt install curl tar"
                echo "  CentOS/RHEL: sudo yum install curl tar"
                echo "  Arch: sudo pacman -S curl tar"
                ;;
            FreeBSD*)
                echo "  FreeBSD: sudo pkg install curl tar"
                ;;
            CYGWIN*|MINGW*|MSYS*)
                echo "  Windows (MSYS2): pacman -S curl tar"
                echo "  Windows (WSL): sudo apt install curl tar"
                ;;
        esac
        exit 1
    fi
}

# Main execution
main() {
    # Detect platform
    local platform=$(detect_platform)
    echo "🖥️  Detected platform: $platform"
    
    # Get download URL
    local download_url=$(get_download_url "$platform")
    echo "🔗 Download URL: $download_url"
    echo ""
    
    # Create directories
    mkdir -p "$DOWNLOAD_DIR"
    mkdir -p "$INSTALL_DIR"
    
    # Check if already installed
    if [ -f "$INSTALL_DIR/bin/llvm-config" ]; then
        echo "✅ LLVM already installed at $INSTALL_DIR"
        verify_installation
        echo ""
        read -p "Re-download? (y/N) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            echo "✅ Using existing installation"
            exit 0
        fi
        echo "🗑️  Removing old installation..."
        rm -rf "$INSTALL_DIR"
        mkdir -p "$INSTALL_DIR"
    fi
    
    # Download
    local filename=$(basename "$download_url")
    local archive_path="$DOWNLOAD_DIR/$filename"
    
    download_file "$download_url" "$archive_path"
    
    # Extract
    extract_archive "$archive_path" "$DOWNLOAD_DIR"
    
    # Move extracted content to install directory
    echo "📁 Organizing installation..."
    
    # Find the extracted directory
    local extracted_dir=$(find "$DOWNLOAD_DIR" -maxdepth 1 -type d -name "LLVM*" | head -1)
    
    if [ -n "$extracted_dir" ] && [ "$extracted_dir" != "$INSTALL_DIR" ]; then
        # Move contents
        cp -r "$extracted_dir"/* "$INSTALL_DIR/"
        rm -rf "$extracted_dir"
    fi
    
    # Clean up archive
    echo "🧹 Cleaning up..."
    rm -f "$archive_path"
    
    # Verify
    echo ""
    if verify_installation; then
        echo ""
        echo "════════════════════════════════════════════════════════════════"
        echo "✅ LLVM $LLVM_VERSION prebuilt installation completed!"
        echo "════════════════════════════════════════════════════════════════"
        echo ""
        echo "🎯 Next steps:"
        echo "   1. Build PawLang with LLVM:"
        echo "      zig build -Dwith-llvm=true"
        echo ""
        echo "   2. Test LLVM backend:"
        echo "      ./zig-out/bin/pawc hello.paw --backend=llvm"
        echo ""
        echo "📊 Installation info:"
        echo "   Platform: $platform"
        echo "   Version: $LLVM_VERSION"
        echo "   Location: $INSTALL_DIR"
        echo "   Size: $(du -sh "$INSTALL_DIR" | cut -f1)"
        echo ""
    else
        echo "❌ Installation failed"
        exit 1
    fi
}

# Run main function
check_dependencies
main
