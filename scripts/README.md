# PawLang LLVM Setup Scripts

This directory contains cross-platform scripts to download and build LLVM for PawLang compiler.

## 📦 Available Scripts

### Cross-Platform Scripts (Python) ⭐

These scripts work on **Windows, Linux, and macOS**:

| Script | Description | Time |
|--------|-------------|------|
| **`install_llvm_complete.py`** | ⭐ One-click complete install (Download+Install+Build+Test) | 10-25 min |

**Requirements**: 
- Python 3.6+
- Zig compiler
- gcc or clang (system compiler)

## 🚀 Quick Start

### ⭐ Option 1: One-Click Complete Install (Recommended!)

```bash
# One command to rule them all!
python3 scripts/install_llvm_complete.py --yes

# Or interactive mode (see detailed progress)
python3 scripts/install_llvm_complete.py
```

**What it does**:
1. Detects your platform (macOS/Linux, Intel/ARM)
2. Downloads LLVM 19.1.7 prebuilt binaries (~600 MB)
3. Extracts and installs to `llvm/install/`
4. Builds PawLang with LLVM backend support
5. Runs test compilation
6. Shows you how to use it

**Time**: 10-25 minutes (mostly downloading)  
**Space**: ~650-800 MB

---

### Alternative: Build LLVM from Source (Advanced Users)

If you need custom LLVM configuration, see [LLVM Build Guide](../docs/LLVM_BUILD_GUIDE.md).

**Quick steps**:
```bash
# 1. Download LLVM source
git clone --depth 1 --branch llvmorg-19.1.7 \
  https://github.com/llvm/llvm-project.git llvm/19.1.7

# 2. Configure and build
mkdir -p llvm/build llvm/install
cd llvm/build
cmake -G Ninja ../19.1.7/llvm \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=../install \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DLLVM_TARGETS_TO_BUILD="host"
ninja install

# 3. Build PawLang
cd ../..
zig build
```

**Note**: Time: 30-60 min, Space: ~10 GB. Only recommended for advanced users.

## 📋 Prerequisites

### All Platforms
- **Python 3.6+** (usually pre-installed on Linux/macOS)
- **CMake 3.13+**
- **Ninja build system**
- **C++ compiler** (GCC 7+, Clang 5+, or MSVC 2019+)
- **~10GB free disk space**
- **~8GB RAM** (16GB recommended)

### Installation Guides

#### Windows

**Option 1: Using Chocolatey** (Recommended)
```powershell
choco install python cmake ninja visualstudio2022buildtools
```

**Option 2: Manual Installation**
- Python: https://www.python.org/downloads/
- CMake: https://cmake.org/download/
- Ninja: https://ninja-build.org/ or `choco install ninja`
- Visual Studio 2019/2022: https://visualstudio.microsoft.com/downloads/
  - Install "Desktop development with C++" workload

#### Linux

**Ubuntu/Debian**:
```bash
sudo apt install python3 cmake ninja-build build-essential
```

**CentOS/RHEL**:
```bash
sudo yum install python3 cmake ninja-build gcc-c++
```

**Arch Linux**:
```bash
sudo pacman -S python cmake ninja gcc
```

#### macOS

```bash
brew install python cmake ninja
xcode-select --install  # For C++ compiler
```

---

## 🥷 Detailed Ninja Installation Guide

### Windows

#### Option 1: Chocolatey (Easiest) ⭐
```powershell
# Install Ninja
choco install ninja -y

# Verify
ninja --version
```

#### Option 2: Scoop
```powershell
scoop install ninja
ninja --version
```

#### Option 3: Manual Installation
1. **Download**:
   - Visit: https://github.com/ninja-build/ninja/releases/latest
   - Download: `ninja-win.zip`

2. **Extract**:
   - Extract `ninja.exe` from the ZIP file
   - Recommended location: `C:\Program Files\Ninja\`

3. **Add to PATH**:
   - Press `Win + X` → Select "System"
   - Click "Advanced system settings"
   - Click "Environment Variables"
   - Under "System variables", find "Path"
   - Click "Edit" → "New"
   - Add: `C:\Program Files\Ninja`
   - Click "OK" on all dialogs

4. **Verify** (open new terminal):
   ```powershell
   ninja --version
   ```

#### Option 4: Build from Source
```powershell
git clone https://github.com/ninja-build/ninja.git
cd ninja
python configure.py --bootstrap
# Copy ninja.exe to C:\Program Files\Ninja\ and add to PATH
```

---

### Linux

#### Ubuntu / Debian
```bash
# Standard package
sudo apt install ninja-build -y

# Verify
ninja --version

# Alternative: Using snap
sudo snap install ninja --classic
```

#### CentOS / RHEL / Fedora
```bash
# Fedora / RHEL 8+ / CentOS Stream
sudo dnf install ninja-build -y

# Older RHEL / CentOS
sudo yum install ninja-build -y

# Or build from EPEL
sudo yum install epel-release -y
sudo yum install ninja-build -y
```

#### Arch Linux
```bash
sudo pacman -S ninja
```

#### openSUSE
```bash
sudo zypper install ninja
```

#### Alpine Linux
```bash
apk add ninja
```

#### Gentoo
```bash
emerge dev-util/ninja
```

#### Build from Source (Any Linux)
```bash
# Clone
git clone https://github.com/ninja-build/ninja.git
cd ninja

# Build
python3 configure.py --bootstrap

# Install
sudo cp ninja /usr/local/bin/

# Verify
ninja --version
```

---

### macOS

#### Option 1: Homebrew (Recommended) ⭐
```bash
brew install ninja

# Verify
ninja --version
```

#### Option 2: MacPorts
```bash
sudo port install ninja

# Verify
ninja --version
```

#### Option 3: Build from Source
```bash
# Clone
git clone https://github.com/ninja-build/ninja.git
cd ninja

# Build
python3 configure.py --bootstrap

# Install
sudo cp ninja /usr/local/bin/

# Verify
ninja --version
```

---

## 🛠️ Before Building LLVM - Complete Setup Guide

### Windows Preparation 🪟

#### Step 1: Install Python
```powershell
# Check if installed
python --version

# If not installed, use Chocolatey:
choco install python -y

# Or download from: https://www.python.org/downloads/
```

#### Step 2: Install CMake
```powershell
# Check if installed
cmake --version

# Method A: Chocolatey (Recommended)
choco install cmake -y

# Method B: Manual download
# Visit: https://cmake.org/download/
# Download: cmake-x.xx.x-windows-x86_64.msi
# During install, check "Add CMake to system PATH"
```

#### Step 3: Install Ninja
```powershell
# Check if installed
ninja --version

# Method A: Chocolatey (Recommended)
choco install ninja -y

# Method B: Scoop
scoop install ninja

# Method C: Manual
# 1. Download: https://github.com/ninja-build/ninja/releases
# 2. Extract ninja.exe to C:\Program Files\Ninja\
# 3. Add C:\Program Files\Ninja to PATH
```

#### Step 4: Install C++ Compiler

**Option A: Visual Studio (Recommended for Windows)**
```powershell
# Install Visual Studio Build Tools
choco install visualstudio2022buildtools --package-parameters "--add Microsoft.VisualStudio.Workload.NativeDesktop" -y

# Or download manually:
# https://visualstudio.microsoft.com/downloads/
# Select "Desktop development with C++" workload
```

**Option B: MinGW-w64**
```powershell
choco install mingw -y
```

#### Step 5: Verify Installation (Important!)
```powershell
# Close current terminal and open a NEW PowerShell window
# Then verify:

python --version    # Should show 3.6+
cmake --version     # Should show 3.13+
ninja --version     # Should show 1.8+

# Check compiler
cl                  # For Visual Studio (should show MSVC version)
# OR
g++ --version       # For MinGW

# If commands not found, restart your computer
```

#### Step 6: For Visual Studio Users
```powershell
# Run builds from "Developer Command Prompt for VS 2022"
# OR set up environment in normal PowerShell:

# For VS 2022
& "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

# For VS 2019
& "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
```

---

### Linux Preparation 🐧

#### Ubuntu / Debian

```bash
# Step 1: Update package list
sudo apt update

# Step 2: Install all dependencies at once
sudo apt install -y \
    python3 \
    python3-pip \
    cmake \
    ninja-build \
    build-essential \
    git

# Step 3: Verify installation
python3 --version   # Should show 3.6+
cmake --version     # Should show 3.13+
ninja --version     # Should show 1.8+
g++ --version       # Should show 7.0+

# If cmake version is too old:
pip3 install cmake --user
export PATH="$HOME/.local/bin:$PATH"
```

#### CentOS / RHEL / Fedora

```bash
# For Fedora / RHEL 8+ / CentOS Stream
sudo dnf install -y \
    python3 \
    cmake \
    ninja-build \
    gcc-c++ \
    git

# For older CentOS / RHEL 7
sudo yum install -y \
    python3 \
    cmake \
    ninja-build \
    gcc-c++ \
    git

# Enable EPEL if ninja-build not found:
sudo yum install epel-release -y
sudo yum install ninja-build -y

# Verify
python3 --version
cmake --version
ninja --version
g++ --version
```

#### Arch Linux

```bash
# Install dependencies
sudo pacman -S \
    python \
    cmake \
    ninja \
    gcc \
    git

# Verify
python --version
cmake --version
ninja --version
g++ --version
```

#### openSUSE

```bash
sudo zypper install -y \
    python3 \
    cmake \
    ninja \
    gcc-c++ \
    git

# Verify
python3 --version
cmake --version
ninja --version
g++ --version
```

#### Alpine Linux

```bash
apk add \
    python3 \
    cmake \
    ninja \
    gcc \
    g++ \
    linux-headers \
    git

# Verify
python3 --version
cmake --version
ninja --version
g++ --version
```

---

### macOS Preparation 🍎

#### Step 1: Install Homebrew (if not installed)
```bash
# Check if Homebrew is installed
brew --version

# If not installed:
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

#### Step 2: Install Dependencies
```bash
# Install all dependencies
brew install python cmake ninja git

# Verify
python3 --version
cmake --version
ninja --version
```

#### Step 3: Install Xcode Command Line Tools
```bash
# Install C++ compiler
xcode-select --install

# If already installed, it will show a message
# Verify
clang++ --version
```

#### Alternative: Using MacPorts
```bash
sudo port install python311 cmake ninja git
xcode-select --install
```

---

## ✅ Pre-Build Verification Checklist

Before running `build_llvm.py`, verify all tools are working:

### Windows
```powershell
Write-Host "=== Checking Build Environment ===" -ForegroundColor Cyan
Write-Host "Python:  $(python --version 2>&1)"
Write-Host "CMake:   $(cmake --version 2>&1 | Select-Object -First 1)"
Write-Host "Ninja:   $(ninja --version 2>&1)"

# Check compiler
if (Get-Command cl -ErrorAction SilentlyContinue) {
    Write-Host "Compiler: Visual Studio (cl.exe) - OK" -ForegroundColor Green
} elseif (Get-Command g++ -ErrorAction SilentlyContinue) {
    Write-Host "Compiler: MinGW (g++) - OK" -ForegroundColor Green
} else {
    Write-Host "Compiler: NOT FOUND" -ForegroundColor Red
}

# Check disk space (need ~10GB free on C:)
$drive = Get-PSDrive C
$freeGB = [math]::Round($drive.Free / 1GB, 2)
Write-Host "Free space on C:: $freeGB GB" -ForegroundColor $(if($freeGB -gt 10){"Green"}else{"Red"})
```

### Linux / macOS
```bash
echo "=== Checking Build Environment ==="
echo "Python:   $(python3 --version 2>&1)"
echo "CMake:    $(cmake --version 2>&1 | head -1)"
echo "Ninja:    $(ninja --version 2>&1)"
echo "Compiler: $(g++ --version 2>&1 | head -1 || clang++ --version 2>&1 | head -1)"

# Check disk space (need ~10GB free)
df -h . | tail -1 | awk '{print "Free space: " $4}'
```

---

## 📊 Quick Install Reference Table

| Platform | Quick Command | Package Manager |
|----------|--------------|-----------------|
| **Windows** | `choco install ninja -y` | Chocolatey |
| Windows | `scoop install ninja` | Scoop |
| Windows | [Manual Download](https://github.com/ninja-build/ninja/releases) | N/A |
| **Ubuntu/Debian** | `sudo apt install ninja-build -y` | APT |
| **Fedora/RHEL 8+** | `sudo dnf install ninja-build -y` | DNF |
| CentOS/RHEL 7 | `sudo yum install ninja-build -y` | YUM |
| **Arch Linux** | `sudo pacman -S ninja` | Pacman |
| openSUSE | `sudo zypper install ninja` | Zypper |
| Alpine | `apk add ninja` | APK |
| Gentoo | `emerge dev-util/ninja` | Portage |
| **macOS** | `brew install ninja` | Homebrew |
| macOS | `sudo port install ninja` | MacPorts |
| **Any Platform** | Build from [source](https://github.com/ninja-build/ninja) | Git + Python |

---

## 🔧 Directory Structure

After running the scripts, you'll have:

```
PawLang/
├── llvm/
│   ├── 19.1.7/          # LLVM source code (~2GB)
│   ├── build/           # Build artifacts (~6GB, can be deleted after install)
│   └── install/         # Installed LLVM (~2GB)
│       ├── bin/         # LLVM tools (clang, llvm-config, etc.)
│       ├── lib/         # LLVM libraries
│       └── include/     # LLVM headers
└── scripts/
    ├── install_llvm_complete.py   # ⭐ One-click complete install
    ├── INSTALL_GUIDE.md           # Installation guide
    └── README.md                  # This file
```

## ⏱️ Time & Space Requirements

| Stage | Time | Disk Space |
|-------|------|------------|
| Download | 5-10 min | ~200MB download, ~2GB extracted |
| Build | 30-60 min | ~6GB (build artifacts) |
| Install | 2-5 min | ~2GB |
| **Total** | **~40-75 min** | **~10GB** |

💡 **Tip**: You can delete `llvm/build/` after successful installation to save ~6GB.

## 🐛 Troubleshooting

### Download Issues

**Problem**: Download is slow or fails
- Try the alternative download method in the script (Git vs Archive)
- Check internet connection
- Download manually: https://github.com/llvm/llvm-project/releases/tag/llvmorg-19.1.7

### Build Issues

**Problem**: "Out of memory" during build
- Close other applications
- Reduce parallel jobs (edit `build_llvm.py`, change `cpu_count`)
- Add more RAM or swap space

**Problem**: CMake configuration fails
- Verify CMake version: `cmake --version` (need 3.13+)
- On Windows, run from "Developer Command Prompt for VS"
- Check that C++ compiler is in PATH

**Problem**: Ninja build fails
- Check disk space (need ~10GB free)
- Try clean build: Delete `llvm/build` directory
- Check build log in `llvm/build/` for specific errors

**Problem**: Python not found on Windows
- Install from: https://www.python.org/downloads/
- Or: `choco install python`
- Restart terminal after installation

## 💡 Usage Tips

### First-Time Setup
```bash
# Complete setup in one go
python scripts/setup_llvm.py && python scripts/build_llvm.py
```

### Using Local LLVM

After building LLVM, PawLang will automatically detect and use it:

```bash
# Build PawLang (auto-detects LLVM)
zig build

# Use LLVM backend
./zig-out/bin/pawc program.paw --backend=llvm

# Compile LLVM IR to executable
llvm/install/bin/clang output.ll -o program
```

### Compiling Programs

After setup, compile and run programs:

**Windows**:
```powershell
# Compile to LLVM IR
.\zig-out\bin\pawc.exe examples\hello.paw --backend=llvm

# Compile to executable
llvm\install\bin\clang.exe output.ll -o hello.exe

# Run
.\hello.exe
```

**Linux/macOS**:
```bash
# Compile to LLVM IR
./zig-out/bin/pawc examples/hello.paw --backend=llvm

# Compile to executable
llvm/install/bin/clang output.ll -o hello

# Run
./hello
```

## ❓ FAQ

**Q: Do I need to build LLVM?**  
A: No, LLVM is optional. PawLang works fine with the C backend (default) without LLVM.

**Q: Why build LLVM locally?**  
A: Local build ensures version compatibility and gives you the full LLVM toolchain.

**Q: Can I use system LLVM instead?**  
A: Possibly, but local build is recommended to ensure compatibility with PawLang.

**Q: How much disk space do I need?**  
A: ~10GB during build. After deleting `llvm/build/`, you need ~4GB.

**Q: Can I stop and resume the build?**  
A: Yes! Just run `python scripts/build_llvm.py` again. Ninja will resume where it stopped.

**Q: How do I clean up?**  
A:
```bash
# Remove build artifacts (saves ~6GB)
rm -rf llvm/build

# Remove everything (to start over)
rm -rf llvm
```

**Q: Which Python version do I need?**  
A: Python 3.6 or higher. Check with: `python --version` or `python3 --version`

## 📚 Additional Resources

- **[LLVM Prebuilt Guide](../docs/LLVM_PREBUILT_GUIDE.md)** 🆕 - Guide for using prebuilt LLVM
- [LLVM Build Guide](../docs/LLVM_BUILD_GUIDE.md) - Building LLVM from source
- [LLVM Documentation](https://llvm.org/docs/)
- [PawLang Documentation](../docs/)
- [PawLang Quick Start](../docs/QUICKSTART.md)
- [PawLang/llvm-build Releases](https://github.com/PawLang/llvm-build/releases/tag/llvm-19.1.7) - Prebuilt LLVM repository

## 🎯 What Each Script Does

### `install_llvm_complete.py` ⭐
**One-Click Complete Installation Script**

Automates the entire process:
1. Detects platform (OS + architecture)
2. Downloads LLVM 19.1.7 prebuilt binaries (~600 MB)
3. Extracts files
4. Installs to `llvm/install/`
5. Builds PawLang with LLVM backend support
6. Runs test program
7. Shows usage guide

**Features**:
- ✅ Smart progress bar (standard + tqdm modes)
- ✅ Complete error handling
- ✅ Auto cleanup of temporary files
- ✅ Non-interactive mode (--yes)
- ✅ Optional skip build (--skip-build)
- ✅ Optional skip test (--skip-test)

**Supported Platforms**:
- macOS x86_64 (Intel)
- macOS arm64 (Apple Silicon M1/M2/M3)
- Linux x86_64
- Linux aarch64 (ARM64)

**Usage**:
```bash
# Interactive mode
python3 scripts/install_llvm_complete.py

# Auto mode (recommended)
python3 scripts/install_llvm_complete.py --yes

# Install LLVM only (skip PawLang build)
python3 scripts/install_llvm_complete.py --yes --skip-build

# Help
python3 scripts/install_llvm_complete.py --help
```

**For building from source**, see the inline CMake commands in Quick Start section above, or refer to [LLVM Build Guide](../docs/LLVM_BUILD_GUIDE.md).

## 📝 License

These scripts are part of the PawLang project and follow the same MIT license.
