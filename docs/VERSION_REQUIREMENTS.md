# Version Requirements

This document specifies the exact versions of dependencies required for building and running PawLang.

---

## Required Versions

### Zig Compiler

**Version**: `0.15.1` (required)

- **Required**: `0.15.1` (exact version)
- **Status**: ✅ Fully tested and validated
- **Note**: This project requires Zig 0.15.1 features and APIs

**Download**: https://ziglang.org/download/

**Verification**:
```bash
zig version
# Expected output: 0.15.1 (or 0.14.0+)
```

---

### LLVM (for LLVM Backend)

**Version**: `19.1.7` (exact)

- **Required**: `19.1.7`
- **Not Compatible**: Other versions (18.x, 20.x) may have API incompatibilities
- **Status**: ✅ Fully tested and validated

**Why 19.1.7?**
- Stable API with excellent C bindings
- Well-tested on all platforms (Linux, macOS, Windows)
- Long-term support release
- Optimal performance and compatibility

---

## Platform-Specific Notes

### Linux

**LLVM 19.1.7 Installation**:
```bash
# Ubuntu 24.04 (Noble)
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 19

# Ubuntu 22.04 (Jammy)
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
echo "deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-19 main" | sudo tee /etc/apt/sources.list.d/llvm.list
sudo apt-get update
sudo apt-get install -y llvm-19 llvm-19-dev libllvm-19-ocaml-dev
```

**Zig Installation**:
```bash
# Download from ziglang.org
wget https://ziglang.org/download/0.15.1/zig-linux-x86_64-0.15.1.tar.xz
tar -xf zig-linux-x86_64-0.15.1.tar.xz
sudo mv zig-linux-x86_64-0.15.1 /usr/local/zig
export PATH="/usr/local/zig:$PATH"
```

### macOS

**LLVM 19.1.7 Installation**:
```bash
# Using Homebrew
brew install llvm@19

# Set environment variables
export LLVM_SYS_190_PREFIX="/opt/homebrew/opt/llvm@19"  # ARM64
# or
export LLVM_SYS_190_PREFIX="/usr/local/opt/llvm@19"     # Intel
```

**Zig Installation**:
```bash
# Using Homebrew (recommended)
brew install zig

# Or download manually
wget https://ziglang.org/download/0.15.1/zig-macos-aarch64-0.15.1.tar.xz  # ARM64
# or
wget https://ziglang.org/download/0.15.1/zig-macos-x86_64-0.15.1.tar.xz   # Intel
```

### Windows

**LLVM 19.1.7 Installation**:
```powershell
# Using Chocolatey
choco install llvm --version=19.1.7

# Or download from LLVM releases
# https://github.com/llvm/llvm-project/releases/tag/llvmorg-19.1.7
```

**Zig Installation**:
```powershell
# Using Chocolatey
choco install zig --version=0.15.1

# Or download from ziglang.org
# https://ziglang.org/download/0.15.1/zig-windows-x86_64-0.15.1.zip
```

---

## Automated Setup

For the easiest installation, use our automated script:

```bash
# One-command setup (includes LLVM 19.1.7)
python3 scripts/install_llvm_complete.py --yes
```

This script:
- ✅ Downloads LLVM 19.1.7 pre-built binaries
- ✅ Extracts to `llvm/install/`
- ✅ Configures environment variables
- ✅ Builds PawLang compiler
- ✅ Runs verification tests

See [scripts/INSTALL_GUIDE.md](../scripts/INSTALL_GUIDE.md) for details.

---

## Verification

After installation, verify your setup:

```bash
# Check Zig version
zig version
# Expected: 0.15.1 (or 0.14.0+)

# Check LLVM version (Linux/macOS)
llvm-config --version
# Expected: 19.1.7

# Check LLVM version (Windows)
clang --version
# Expected: clang version 19.1.7

# Build PawLang
zig build

# Test both backends
./zig-out/bin/pawc examples/hello.paw --backend=c
./zig-out/bin/pawc examples/hello.paw --backend=llvm
```

---

## Version Compatibility Matrix

| Component | Version | Status | Notes |
|-----------|---------|--------|-------|
| **Zig** | 0.15.1 | ✅ Required | Only supported version |
| **Zig** | 0.15.0 | ❌ Not Supported | Use 0.15.1 instead |
| **Zig** | 0.14.x | ❌ Not Supported | API incompatible |
| **Zig** | 0.13.x | ❌ Not Supported | API incompatible |
| **LLVM** | 19.1.7 | ✅ Required | Only supported version |
| **LLVM** | 19.1.x | ⚠️ May Work | Not officially tested |
| **LLVM** | 18.x | ❌ Not Compatible | API differences |
| **LLVM** | 20.x | ❌ Not Compatible | API changes |

---

## Troubleshooting

### "LLVM not found" Error

**Linux/macOS**:
```bash
# Find your LLVM installation
which llvm-config
# or
find /usr -name "llvm-config" 2>/dev/null

# Set environment variable
export LLVM_SYS_190_PREFIX="/path/to/llvm"
```

**Windows**:
```powershell
# Check LLVM installation
where clang
# Expected: C:\Program Files\LLVM\bin\clang.exe

# Verify PATH includes LLVM
echo $env:PATH
```

### Zig Version Mismatch

If you have multiple Zig versions:

```bash
# Check which zig is being used
which zig

# Remove old versions
sudo rm -rf /usr/local/zig-old

# Verify version
zig version
```

### Build Fails with "API incompatible"

This usually means your LLVM version is not 19.1.7:

```bash
# Uninstall other LLVM versions
brew uninstall llvm@18 llvm@20  # macOS
sudo apt remove llvm-18 llvm-20  # Linux

# Install LLVM 19.1.7
# See platform-specific instructions above
```

---

## CI/CD Versions

Our GitHub Actions CI uses the following versions:

- **Zig**: `0.15.1` (via `goto-bus-stop/setup-zig@v2`)
- **LLVM**: `19.1.7` (platform-specific installation)
  - **Linux**: From official LLVM APT repository
  - **macOS**: From Homebrew (`llvm@19`)
  - **Windows**: From Chocolatey (`llvm --version=19.1.7`)

See [.github/workflows/ci.yml](../.github/workflows/ci.yml) for details.

---

## Future Versions

### Planned Support

- **Zig 0.16.x**: Will be supported when released
- **LLVM 20.x**: Will be evaluated after stable release

### Migration Strategy

When new versions are released:

1. Test compatibility in separate branch
2. Update version requirements document
3. Update CI configurations
4. Update installation scripts
5. Release new PawLang version

---

## Getting Help

If you have issues with versions:

1. Check this document for correct versions
2. Verify installation: `zig version` and `llvm-config --version`
3. Use automated setup script: `python3 scripts/install_llvm_complete.py`
4. Report issues: https://github.com/KinLeoapple/PawLang/issues

---

## Summary

**Quick Reference**:
- ✅ **Zig**: 0.15.1 (required, exact version)
- ✅ **LLVM**: 19.1.7 (required, exact version)
- ✅ **Automated Setup**: Use `scripts/install_llvm_complete.py`
- ✅ **Verification**: Run `zig version` and `llvm-config --version`

**Last Updated**: October 12, 2025 (v0.1.8)

