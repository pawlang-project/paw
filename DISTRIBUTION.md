# 📦 PawLang Distribution Guide

This guide explains how to create and distribute PawLang packages for different platforms.

## 🚀 Quick Start

### Build Distribution Package

```bash
# Step 1: Build the compiler with dependencies
zig build

# Step 2: Create distribution package
zig build package
```

This will create:
- **Windows**: `pawlang-windows.zip` (~200MB with DLLs)
- **macOS**: `pawlang-macos.tar.gz` (~800KB compressed)
- **Linux**: `pawlang-linux.tar.gz` (~800KB compressed)

---

## 📊 Distribution Options

### Option 1: Full Package (with LLVM Support) ⭐ Recommended

**Includes:**
- Compiler executable (`pawc` / `pawc.exe`)
- LLVM runtime libraries (DLLs/dylibs/so files)
- Example `.paw` files

**Build:**
```bash
zig build package
```

**Size:**
- Windows: ~200MB (includes all DLLs)
- macOS: ~800KB compressed
- Linux: ~800KB compressed

**Features:**
- ✅ C backend (default)
- ✅ LLVM backend (`--backend=llvm`)
- ✅ LLVM optimizations (`-O0`, `-O1`, `-O2`, `-O3`)
- ✅ Zero configuration needed
- ✅ Works out-of-the-box

---

### Option 2: Lightweight (C Backend Only)

**Includes:**
- Only the compiler executable (~2.5MB)

**Build:**
```bash
zig build -Denable-llvm=false
```

**Size:**
- All platforms: ~2.5MB (single executable)

**Features:**
- ✅ C backend only
- ✅ Zero dependencies
- ✅ Smallest footprint
- ❌ No LLVM backend

---

## 📦 Package Contents

### Windows (`pawlang-windows.zip`)

```
pawlang-windows/
├── bin/
│   ├── pawc.exe                    (~2.5MB)
│   ├── LLVM-C.dll                  (~50MB)
│   ├── libclang.dll                (~65MB)
│   ├── liblldb.dll                 (~87MB)
│   └── ... (other LLVM DLLs)
└── examples/
    ├── hello.paw
    ├── type_cast_demo.paw
    └── ... (more examples)
```

**Usage after extraction:**
```powershell
cd pawlang-windows\bin
.\pawc.exe ..\examples\hello.paw
```

---

### macOS (`pawlang-macos.tar.gz`)

```
zig-out/
├── bin/
│   └── pawc                        (~2.5MB)
├── lib/
│   └── libLLVM-C.dylib             (~85KB symlink)
└── examples/
    ├── hello.paw
    └── ...
```

**Usage after extraction:**
```bash
tar -xzf pawlang-macos.tar.gz
cd zig-out
./bin/pawc examples/hello.paw
```

**Note:** Users may need to install LLVM or use C backend only:
```bash
# Install LLVM (optional, for LLVM backend)
brew install llvm@19

# Or just use C backend (works without LLVM)
./bin/pawc examples/hello.paw --backend=c
```

---

### Linux (`pawlang-linux.tar.gz`)

```
zig-out/
├── bin/
│   └── pawc                        (~2.5MB)
├── lib/
│   ├── libLLVM-19.so.1
│   └── ... (LLVM shared libraries)
└── examples/
    └── ...
```

**Usage after extraction:**
```bash
tar -xzf pawlang-linux.tar.gz
cd zig-out

# Set library path
export LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH

./bin/pawc examples/hello.paw
```

Or install system-wide LLVM:
```bash
sudo apt install llvm-19-dev
./bin/pawc examples/hello.paw
```

---

## 🛠️ Build Commands Reference

| Command | Description |
|---------|-------------|
| `zig build` | Build with LLVM support (if available) |
| `zig build -Denable-llvm=false` | Build lightweight version (C backend only) |
| `zig build dist` | Prepare distribution (copy libs, show instructions) |
| `zig build package` | Create distribution archive |
| `zig build lite` | Show how to build without LLVM |

---

## 📋 Distribution Checklist

### For Windows

- [ ] Run `zig build package`
- [ ] Test `pawlang-windows.zip` on clean Windows machine
- [ ] Verify `pawc.exe` runs without external LLVM
- [ ] Include README with usage instructions

### For macOS

- [ ] Run `zig build package`
- [ ] Test on both Apple Silicon and Intel Macs
- [ ] Note: Users can use C backend without LLVM
- [ ] Include setup instructions

### For Linux

- [ ] Run `zig build package` on target architecture
- [ ] Test with `LD_LIBRARY_PATH` setup
- [ ] Provide installation script
- [ ] Document LLVM dependencies

---

## 💡 Recommendations

### For End Users
**Recommended:** Use C backend (default)
- Zero dependencies
- Works everywhere
- Fast compilation

### For Developers
**Recommended:** Full package with LLVM
- Both backends available
- Optimization support
- Maximum flexibility

### For CI/CD
**Recommended:** Build both variants
- Lightweight for quick testing
- Full version for releases

---

## 🎯 User Experience

After extracting the distribution package, users can:

**Immediate use (Windows):**
```powershell
pawc.exe hello.paw        # Works immediately!
```

**Set up once (macOS/Linux):**
```bash
# Option A: Set library path
export LD_LIBRARY_PATH=/path/to/zig-out/lib:$LD_LIBRARY_PATH

# Option B: Install system LLVM
brew install llvm@19      # macOS
sudo apt install llvm-19  # Linux

# Then use normally
pawc hello.paw
```

**Or just use C backend:**
```bash
pawc hello.paw --backend=c   # No LLVM needed!
```

---

## 📈 File Sizes

| Platform | Full Package | Lightweight | LLVM Libs |
|----------|--------------|-------------|-----------|
| Windows | ~200MB | 2.5MB | ~197MB |
| macOS | ~800KB | 2.5MB | ~85KB (symlink) |
| Linux | ~800KB | 2.5MB | Varies |

**Note:** Compressed sizes are much smaller due to compression.

---

## 🔧 Advanced: Custom Distribution

You can customize the package contents by modifying `build.zig`:

```zig
// Add custom files to distribution
const copy_docs = b.addSystemCommand(&[_][]const u8{
    "cp", "-r", "docs", "zig-out/docs"
});
```

---

## ✅ Testing Distribution

After creating the package, test on a clean system:

```bash
# Extract
tar -xzf pawlang-macos.tar.gz  # or unzip on Windows

# Test C backend (should work without LLVM)
./bin/pawc examples/hello.paw

# Test LLVM backend (requires LLVM or bundled libs)
./bin/pawc examples/hello.paw --backend=llvm
```

---

## 📞 Support

If users encounter issues:
1. Try C backend: `pawc file.paw --backend=c`
2. Check LLVM installation: `clang --version`
3. Verify library path (Linux/macOS): `echo $LD_LIBRARY_PATH`

---

**Created:** 2025-10-11  
**Version:** 0.1.7+  
**Platform Support:** Windows, macOS (ARM64/Intel), Linux (x86_64/ARM64)

