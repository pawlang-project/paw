# 🐾 PawLang Quick Start Guide

## Installation

### Extract the Package

**Windows:**
```powershell
# Extract pawlang-windows.zip
# Navigate to the bin directory
cd pawlang-windows\bin
```

**macOS/Linux:**
```bash
tar -xzf pawlang-macos.tar.gz  # or pawlang-linux.tar.gz
cd zig-out
```

---

## Basic Usage

### Compile a Program

```bash
# Windows
pawc.exe hello.paw

# macOS/Linux
./bin/pawc hello.paw
```

This generates `output.c` by default (C backend).

### Run with LLVM Backend

```bash
pawc hello.paw --backend=llvm
```

This generates `output.ll` (LLVM IR).

### LLVM Optimizations

```bash
pawc program.paw --backend=llvm -O2   # Standard optimization
pawc program.paw --backend=llvm -O3   # Aggressive optimization
```

---

## Examples

Try the included examples:

```bash
pawc examples/hello.paw
pawc examples/type_cast_demo.paw
pawc examples/generics_demo.paw
pawc examples/module_demo.paw
```

---

## Command Line Options

```
pawc <file.paw> [options]

Options:
  --backend=c      Use C backend (default, zero dependencies)
  --backend=llvm   Use LLVM backend (generates optimized IR)
  -O0/-O1/-O2/-O3  LLVM optimization levels
  --help           Show help message
```

---

## Platform-Specific Notes

### Windows
- All LLVM DLLs are included
- Works out-of-the-box, no installation needed
- Just run `pawc.exe`

### macOS
- LLVM libraries included in `lib/` directory
- For C backend: Works immediately
- For LLVM backend: May need `brew install llvm@19` or use bundled libs

### Linux
- LLVM libraries included in `lib/` directory
- Set `LD_LIBRARY_PATH` for bundled libs:
  ```bash
  export LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH
  ```
- Or install system LLVM: `sudo apt install llvm-19`

---

## Troubleshooting

### "LLVM backend not available"
**Solution:** Use C backend instead:
```bash
pawc file.paw --backend=c
```

### Missing DLL (Windows)
**Solution:** Ensure all DLL files are in the same directory as `pawc.exe`

### Library not found (Linux/macOS)
**Solution:** Set library path:
```bash
export LD_LIBRARY_PATH=/path/to/zig-out/lib:$LD_LIBRARY_PATH
```

---

## More Information

- **Repository:** https://github.com/KinLeoapple/PawLang
- **Documentation:** See `docs/` directory
- **Examples:** See `examples/` directory

---

**Version:** 0.1.7+  
**License:** See LICENSE file

