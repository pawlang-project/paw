# PawLang LLVM Build Guide

Complete guide for building and running PawLang programs with LLVM.

---

## 🚀 Quick Start

### Method 1: Zig Build System (Recommended)

```bash
# Setup (one time)
zig build -Dwith-llvm=true

# Compile and run
zig build run-llvm -Dwith-llvm=true

# Use custom file
zig build run-llvm -Dwith-llvm=true -Dllvm-example=my_program.paw
```

### Method 2: Manual Commands

```bash
# Build compiler
zig build -Dwith-llvm=true

# Compile .paw to LLVM IR
./zig-out/bin/pawc hello.paw --backend=llvm-native

# Compile IR to executable
llvm/install/bin/clang output.ll -o hello \
  -L/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib -lSystem

# Run
./hello
```

---

## 📋 Available Build Commands

### Core Commands

| Command | Description | Output |
|---------|-------------|--------|
| `zig build -Dwith-llvm=true` | Build PawLang compiler with LLVM support | `zig-out/bin/pawc` |
| `zig build compile-llvm` | Compile .paw to LLVM IR only | `output_zig.ll` |
| `zig build build-llvm` | Compile .paw to executable | `output_zig_exec` |
| `zig build run-llvm` | Compile and run .paw program | (runs program) |
| `zig build clean-llvm` | Clean LLVM build artifacts | (cleanup) |

### Options

- `-Dwith-llvm=true` - Enable LLVM native backend
- `-Dllvm-example=<file>` - Specify input file (default: `examples/llvm_demo.paw`)

---

## 🔧 Complete Workflow

### Step-by-Step

```bash
# 1. Build LLVM (one time setup)
./scripts/setup_llvm_source.sh
./scripts/build_llvm_local.sh

# 2. Build PawLang compiler with LLVM
zig build -Dwith-llvm=true

# 3. Write your program
cat > hello.paw << 'EOF'
fn main() -> i32 {
    return 42;
}
EOF

# 4. Compile and run
zig build run-llvm -Dwith-llvm=true -Dllvm-example=hello.paw
```

### What Happens Under the Hood

```
┌─────────────────────────────────────────────────────────────┐
│  PawLang Source (.paw)                                      │
└───────────────────────┬─────────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────────┐
│  PawLang Compiler (pawc)                                    │
│  - Lexer                                                    │
│  - Parser                                                   │
│  - Type Checker                                             │
│  - LLVM Native Backend (llvm_native_backend.zig)            │
└───────────────────────┬─────────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────────┐
│  LLVM IR (.ll)                                              │
│  - Text format                                              │
│  - Human readable                                           │
│  - Generated via LLVM C API                                 │
└───────────────────────┬─────────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────────┐
│  Local Clang (llvm/install/bin/clang)                      │
│  - LLVM optimizer                                           │
│  - Code generator                                           │
│  - Linker                                                   │
└───────────────────────┬─────────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────────┐
│  Native Executable                                          │
│  - Platform-specific binary                                 │
│  - Ready to run                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 🎯 Backend Comparison

### Three Available Backends

| Backend | Command | Dependencies | Speed | Use Case |
|---------|---------|--------------|-------|----------|
| **C** (default) | `--backend=c` | TCC (embedded) | Fast | Development, quick testing |
| **LLVM Text** | `--backend=llvm` | None | Fast | Debugging, IR inspection |
| **LLVM Native** | `--backend=llvm-native` | Local LLVM | Fastest | Production, optimization |

### Backend Selection

```bash
# C backend (default, stable)
./zig-out/bin/pawc hello.paw
./zig-out/bin/pawc hello.paw --backend=c

# LLVM text mode (IR generation without linking)
./zig-out/bin/pawc hello.paw --backend=llvm

# LLVM native mode (direct API, requires -Dwith-llvm=true)
./zig-out/bin/pawc hello.paw --backend=llvm-native
```

---

## 📊 Performance Comparison

### Compilation Time

```
Source: examples/llvm_demo.paw (3 functions, 30 lines)

C Backend:          0.002s
LLVM Text Mode:     0.003s
LLVM Native Mode:   0.003s
```

### Generated Code Quality

```
Binary Size:
  C Backend:          8.2 KB
  LLVM (no opt):     16.4 KB
  LLVM (-O2):         8.8 KB
  
Execution Speed:
  C Backend:          1.00x (baseline)
  LLVM (no opt):      0.95x
  LLVM (-O2):         1.20x
```

---

## 🛠 Advanced Usage

### Custom Optimization Levels

```bash
# No optimization
llvm/install/bin/clang output.ll -o program

# Optimize for speed
llvm/install/bin/clang output.ll -o program -O3

# Optimize for size
llvm/install/bin/clang output.ll -o program -Os

# Debug build
llvm/install/bin/clang output.ll -o program -g
```

### Cross-Compilation

```bash
# Compile for different target
llvm/install/bin/clang output.ll -o program \
  --target=aarch64-linux-gnu

# Check supported targets
llvm/install/bin/llc --version
```

### Viewing Generated Assembly

```bash
# Generate assembly
llvm/install/bin/llc output.ll -o output.s

# View assembly
cat output.s

# Assemble manually
as output.s -o output.o
ld output.o -o program -lSystem
```

---

## 🔍 Debugging

### Inspect LLVM IR

```bash
# Generate IR
zig build compile-llvm -Dwith-llvm=true

# View IR
cat output_zig.ll

# Verify IR
llvm/install/bin/opt -verify output_zig.ll

# Optimize IR
llvm/install/bin/opt -O3 output_zig.ll -o output_opt.ll
```

### Run with Interpreter

```bash
# No compilation needed
llvm/install/bin/lli output_zig.ll
echo "Exit code: $?"
```

### Verbose Compilation

```bash
# See all clang stages
llvm/install/bin/clang output.ll -o program -v

# See optimization passes
llvm/install/bin/opt -O3 output.ll -o output_opt.ll -debug-pass=Structure
```

---

## 🌍 Cross-Platform Notes

### macOS

```bash
# SDK path (usually automatic)
-L/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib

# Required libraries
-lSystem
```

### Linux

```bash
# Usually works without extra flags
llvm/install/bin/clang output.ll -o program

# If needed
-lc -lm
```

### Windows (MinGW)

```bash
# Use MinGW clang
llvm/install/bin/clang output.ll -o program.exe -lmsvcrt
```

---

## 📁 File Structure

```
PawLang/
├── llvm/
│   ├── 19.1.7/               # LLVM source code
│   ├── build/                # Build directory
│   └── install/              # Local installation
│       ├── bin/
│       │   ├── clang         # C/C++ compiler ⭐
│       │   ├── llc           # LLVM static compiler
│       │   ├── lli           # LLVM interpreter
│       │   ├── opt           # LLVM optimizer
│       │   └── llvm-config   # Configuration tool
│       ├── lib/              # LLVM libraries
│       └── include/          # LLVM headers
├── src/
│   ├── llvm_c_api.zig        # LLVM C API bindings
│   ├── llvm_backend.zig      # Text IR backend
│   └── llvm_native_backend.zig  # Native API backend ⭐
├── scripts/
│   ├── download_llvm_prebuilt.py  # Download prebuilt LLVM (recommended)
│   ├── setup_llvm.py          # Setup LLVM (unified entry)
│   └── build_llvm.py          # Build LLVM from source
├── build.zig                 # Zig build configuration ⭐
└── zig-out/
    └── bin/
        └── pawc              # PawLang compiler
```

---

## 🐛 Troubleshooting

### "LLVM native backend not available"

```bash
# Solution: Build with LLVM support
zig build -Dwith-llvm=true
```

### "Local LLVM not found"

```bash
# Solution: Build LLVM first
./scripts/setup_llvm_source.sh
./scripts/build_llvm_local.sh
```

### "library 'System' not found" (macOS)

```bash
# Solution: Add SDK path
llvm/install/bin/clang output.ll -o program \
  -L/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib \
  -lSystem
```

### Memory leaks reported

```bash
# Known issue: Parser memory leaks (not critical)
# The generated code is correct, leaks are in compiler itself
# Will be fixed in future versions
```

---

## 📚 Additional Resources

- [LLVM Language Reference](https://llvm.org/docs/LangRef.html)
- [LLVM C API Documentation](https://llvm.org/doxygen/group__LLVMC.html)
- [Clang Command Line Reference](https://clang.llvm.org/docs/ClangCommandLineReference.html)
- [PawLang Documentation](../README.md)

---

## ✅ Summary

**Recommended workflow:**

```bash
# One-time setup
./scripts/setup_llvm_source.sh && ./scripts/build_llvm_local.sh
zig build -Dwith-llvm=true

# Development
zig build run-llvm -Dwith-llvm=true -Dllvm-example=my_program.paw

# Production
zig build build-llvm -Dwith-llvm=true -Dllvm-example=my_program.paw
llvm/install/bin/clang output_zig.ll -o my_program -O3
./my_program
```

**Key benefits:**
- ✅ Complete local LLVM toolchain
- ✅ No system dependencies
- ✅ Reproducible builds
- ✅ Integrated Zig workflow
- ✅ Production-ready optimization

🐾 **Happy coding with PawLang + LLVM!** 🚀

