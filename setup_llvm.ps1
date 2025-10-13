# LLVM Auto-Download Script for Windows
# Detects architecture and downloads corresponding pre-compiled LLVM

$ErrorActionPreference = "Stop"

$LLVM_VERSION = "21.1.3"
$BASE_URL = "https://github.com/pawlang-project/llvm-build/releases/download/llvm-$LLVM_VERSION"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "   LLVM Auto-Download for Windows" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

# Detect architecture
$ARCH = [System.Environment]::GetEnvironmentVariable("PROCESSOR_ARCHITECTURE")

# Map to target name
switch ($ARCH) {
    "AMD64" {
        $TARGET = "windows-x86_64"
    }
    "ARM64" {
        $TARGET = "windows-aarch64"
    }
    "x86" {
        $TARGET = "windows-x86"
    }
    default {
        Write-Host "❌ Unsupported architecture: $ARCH" -ForegroundColor Red
        Write-Host ""
        Write-Host "Supported architectures:" -ForegroundColor Yellow
        Write-Host "  • x86_64 (AMD64)"
        Write-Host "  • ARM64"
        Write-Host "  • x86 (32-bit)"
        exit 1
    }
}

Write-Host "Detected platform: $TARGET" -ForegroundColor Green
Write-Host ""

# Check if already exists
$VENDOR_DIR = "vendor\llvm\$TARGET"
if (Test-Path "$VENDOR_DIR\install") {
    Write-Host "⚠️  LLVM already exists: $VENDOR_DIR\install\" -ForegroundColor Yellow
    $confirm = Read-Host "Re-download? [y/N]"
    if ($confirm -ne "y" -and $confirm -ne "Y") {
        Write-Host "Cancelled"
        exit 0
    }
    Remove-Item -Recurse -Force "$VENDOR_DIR"
}

# Download
$FILENAME = "llvm-$LLVM_VERSION-$TARGET.tar.gz"
$URL = "$BASE_URL/$FILENAME"

Write-Host "📥 Downloading LLVM $LLVM_VERSION for $TARGET..." -ForegroundColor Cyan
Write-Host "   URL: $URL"
Write-Host ""

try {
    # Use .NET WebClient for download with progress
    $webClient = New-Object System.Net.WebClient
    $webClient.DownloadFile($URL, $FILENAME)
    Write-Host "✅ Download completed" -ForegroundColor Green
} catch {
    Write-Host "❌ Download failed: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "📦 Extracting..." -ForegroundColor Cyan

# Create vendor directory structure
New-Item -ItemType Directory -Force -Path "$VENDOR_DIR" | Out-Null

# Extract using tar (available in Windows 10+)
if (Get-Command tar -ErrorAction SilentlyContinue) {
    tar -xzf $FILENAME -C "$VENDOR_DIR\"
} else {
    # Fallback: use 7-Zip if available
    if (Get-Command 7z -ErrorAction SilentlyContinue) {
        7z x $FILENAME -so | 7z x -si -ttar -o"$VENDOR_DIR"
    } else {
        Write-Host "❌ Missing extraction tool" -ForegroundColor Red
        Write-Host "   Please install tar (Windows 10+) or 7-Zip" -ForegroundColor Yellow
        exit 1
    }
}

# Clean up archive
Remove-Item $FILENAME

Write-Host ""
Write-Host "==========================================" -ForegroundColor Green
Write-Host "   ✅ LLVM Installed Successfully!" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Installation location: $VENDOR_DIR\install\" -ForegroundColor Cyan
Write-Host ""
Write-Host "Included tools:" -ForegroundColor Yellow
Write-Host "  • clang.exe - C/C++ compiler"
Write-Host "  • llc.exe - LLVM compiler"
Write-Host "  • lld.exe - LLVM linker"
Write-Host ""
Write-Host "Verify installation:" -ForegroundColor Cyan
Write-Host "  $VENDOR_DIR\install\bin\clang.exe --version"
Write-Host ""
Write-Host "🚀 Now you can build PawLang:" -ForegroundColor Green
Write-Host "   zig build"
Write-Host ""
Write-Host "Use LLVM backend:" -ForegroundColor Green
Write-Host "   .\zig-out\bin\pawc.exe hello.paw --backend=llvm"
Write-Host ""

