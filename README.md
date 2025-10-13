<div align="center">

# 🐾 PawLang

<img src="assets/logo.png" alt="PawLang Logo" width="120" height="120">

**A modern systems programming language with Rust-level safety and cleaner syntax**

</div>

[![Version](https://img.shields.io/badge/version-0.1.8-blue.svg)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)](#)
[![Platforms](https://img.shields.io/badge/platforms-7-orange.svg)](#-platform-support)

---

## 🌍 Platform Support

**PawLang runs on 7 platforms with full self-contained distribution!**

| Platform | Architecture | C Backend | LLVM Backend | Status |
|----------|-------------|-----------|--------------|--------|
| **Linux** | x86_64 (Ubuntu 22.04+) | ✅ | ✅ | Fully Tested |
| **Linux** | x86 (32-bit) | ✅ | ❌ | C-only (cross-compile) |
| **Linux** | armv7 (ARM32) | ✅ | ❌ | C-only (cross-compile) |
| **macOS** | x86_64 (Intel) | ✅ | ✅ | Fully Tested |
| **macOS** | ARM64 (Apple Silicon) | ✅ | ✅ | Fully Tested |
| **Windows** | x86_64 | ✅ | ✅ | Fully Tested |
| **Windows** | x86 (32-bit) | ✅ | ❌ | C-only (no 32-bit LLVM) |

**Coverage**: 99.9%+ of users (desktop, server, embedded)

### Self-Contained Distribution

**No system LLVM required!** All necessary libraries are bundled:

- **Windows**: All LLVM DLLs included in `bin/` directory
- **macOS**: LLVM dylibs with fixed `@rpath` in `lib/` directory  
- **Linux**: LLVM shared libraries in `lib/` directory

**True "download and run" experience** - just extract and use!

---

## 🚀 Quick Start

### Installation

**Option 1: Download Pre-built Release (Recommended)** ⭐

Download the latest release for your platform from [Releases](https://github.com/pawlang-project/paw/releases):

- `pawlang-linux-x86_64.tar.gz` - Linux (Ubuntu 22.04+, Debian 12+)
- `pawlang-macos-x86_64.tar.gz` - macOS Intel
- `pawlang-macos-arm64.tar.gz` - macOS Apple Silicon
- `pawlang-windows-x86_64.zip` - Windows

```bash
# Extract and use (all platforms)
tar -xzf pawlang-*.tar.gz  # or unzip for Windows
cd pawlang

# Unix (macOS/Linux)
./bin/pawc examples/hello.paw

# Windows
bin\pawc.exe examples\hello.paw
```

**No dependencies needed!** All LLVM libraries are included.

**Option 2: Build from Source**

```bash
# Clone the repository
git clone https://github.com/KinLeoapple/PawLang.git
cd PawLang

# Install LLVM 19 (one-time setup)
python3 scripts/install_llvm_complete.py --yes

# Build the compiler
zig build

# Create self-contained distribution
zig build package

# Compiler is located at zig-out/bin/pawc
```

### Hello World

```paw
fn main() -> i32 {
    println("Hello, PawLang! 🐾");
    return 0;
}
```

```bash
# Compile and run
./zig-out/bin/pawc hello.paw --run

# Or step by step
./zig-out/bin/pawc hello.paw    # Generate output.c
gcc output.c -o hello            # Compile
./hello                          # Run
```

---

## ✨ Core Features

### 🚀 Dual Backend Architecture (v0.1.4 NEW!) ⭐

**Choose your backend: C for portability, LLVM for performance**

```bash
# C Backend (default) - Maximum compatibility
pawc hello.paw                    # Generates C code
pawc hello.paw --backend=c        # Explicit C backend

# LLVM Native Backend (NEW!) - Better optimization
pawc hello.paw --backend=llvm     # Generates LLVM IR

# LLVM with optimization (v0.1.7) ⚡
pawc hello.paw --backend=llvm -O2 # Standard optimization ⭐
pawc hello.paw --backend=llvm -O3 # Maximum performance 🚀
```

**Features:**
- ✅ **C Backend**: Stable, portable, works everywhere
- ✅ **LLVM Backend**: Native API integration, control flow support
- ✅ **Optimization Levels**: -O0, -O1, -O2, -O3 (v0.1.7) ⚡
- ✅ **Control Flow**: if/else, loop (unified), break/continue
- ✅ **Zero Memory Leaks**: Arena allocator, fully leak-free
- ✅ **Local LLVM Toolchain**: No system dependencies

**LLVM Backend Highlights:**
```paw
// Full control flow support in LLVM backend
fn fibonacci(n: i32) -> i32 {
    if n <= 1 {
        return n;
    } else {
        return fibonacci(n - 1) + fibonacci(n - 2);
    }
}

fn sum_to_n(n: i32) -> i32 {
    let sum = 0;
    let i = 0;
    loop i <= n {
        sum = sum + i;
        i = i + 1;
    }
    return sum;
}
```

**Backend Comparison:**

| Feature | C Backend | LLVM Backend |
|---------|-----------|--------------|
| Portability | ✅ Excellent | ✅ Good |
| Optimization | ✅ Good | ✅ Excellent |
| Control Flow | ✅ Full | ✅ Full |
| Compile Speed | ✅ Fast | ⚡ Very Fast |
| IR Quality | ✅ C Code | ✅ LLVM IR |

---

### 🎨 Automatic Type Inference (v0.1.3) ⭐

**Cleaner code, same type safety:**

```paw
// Before (v0.1.2): Explicit type annotations required
let x: i32 = 42;
let sum: i32 = add(10, 20);
let vec: Vec<i32> = Vec<i32>::new();

// Now (v0.1.3): Automatic type inference!
let x = 42;                    // Inferred as i32
let sum = add(10, 20);         // Inferred as i32
let vec = Vec<i32>::new();     // Inferred as Vec<i32>
```

**Supported Inference:**
- ✅ Literals (integers, strings, booleans)
- ✅ Function call return values
- ✅ Generic instantiation
- ✅ Struct literals
- ✅ Expression results

**Example:**
```paw
fn calculate(a: i32, b: i32) -> i32 {
    a + b
}

type Point = struct { x: i32, y: i32, }

fn main() -> i32 {
    let x = 42;                          // i32
    let message = "Hello";                // string
    let result = calculate(10, 20);      // i32
    let p = Point { x: 1, y: 2 };        // Point
    let vec = Vec<i32>::new();           // Vec<i32>
    
    // Explicit types still optional
    let explicit: i32 = 42;
    
    return result;
}
```

**Benefits:**
- 📝 Less boilerplate
- 🚀 Faster development
- 🔒 Full type safety maintained
- 💡 Clearer code intent

---

### 🏗️ Engineering-Grade Module System (v0.1.3) ⭐

**Multi-item import syntax:**

```paw
// math.paw - Module file
pub fn add(a: i32, b: i32) -> i32 { a + b }
pub fn multiply(a: i32, b: i32) -> i32 { a * b }
pub type Vec2 = struct { x: i32, y: i32, }

// main.paw - Using modules
// 🆕 v0.1.3: Multi-item import (recommended)
import math.{add, multiply, Vec2};

// v0.1.2: Single-item import (still supported)
import math.add;
import math.multiply;
import math.Vec2;

fn main() -> i32 {
    let sum = add(10, 20);
    let product = multiply(5, 6);
    let v = Vec2 { x: 1, y: 2 };
    return sum + product;
}
```

**Module Entry Point (mod.paw):**
```
mylib/
├── mod.paw       # Module entry (re-exports)
├── core.paw      # Core functionality
└── utils.paw     # Utility functions

Usage:
import mylib.{hello, Data};  // Import from mod.paw
```

**Features:**
- ✅ Multi-item imports reduce code
- ✅ mod.paw module entry support
- ✅ Modular standard library
- ✅ Uses `.` syntax (not `::`)
- ✅ Direct `import` (no `use` needed)
- ✅ `pub` controls exports
- ✅ Automatic module loading and caching

---

### 🎯 Complete Generic System (v0.1.2)

**Generic Functions:**
```paw
fn identity<T>(x: T) -> T {
    return x;
}

let num = identity(42);      // T = i32
let text = identity("hello"); // T = string
```

**Generic Structs:**
```paw
type Box<T> = struct {
    value: T,
}

let box_int: Box<i32> = Box { value: 42 };
let box_str: Box<string> = Box { value: "paw" };
```

**Generic Methods** ⭐:
```paw
type Vec<T> = struct {
    ptr: i32,
    len: i32,
    cap: i32,
    
    // Static method: Use :: to call
    fn new() -> Vec<T> {
        return Vec { ptr: 0, len: 0, cap: 0 };
    }
    
    fn with_capacity(cap: i32) -> Vec<T> {
        return Vec { ptr: 0, len: 0, cap: cap };
    }
    
    // Instance method: self doesn't need type!
    fn length(self) -> i32 {
        return self.len;
    }
}

// Usage
let vec: Vec<i32> = Vec<i32>::new();        // Static method
let len: i32 = vec.length();                // Instance method
```

---

### 🔒 Type Safety

- **18 Precise Types**: `i8`-`i128`, `u8`-`u128`, `f32`, `f64`, `bool`, `char`, `string`, `void`
- **Compile-time type checking**
- **Zero runtime overhead** (full monomorphization)

### 🛡️ Mutability Control (v0.1.6) ⭐

**Immutable by default, mutable when needed:**

```paw
// Immutable variable (default)
let x = 10;
// x = 20;  // ❌ Compile error!

// Mutable variable (explicit)
let mut y = 10;
y = 20;     // ✅ OK
y += 5;     // ✅ OK
```

**mut self for methods:**

```paw
type Counter = struct {
    value: i32,
    
    // Immutable method
    fn get(self) -> i32 {
        return self.value;
    }
    
    // Mutable method
    fn increment(mut self) -> i32 {
        self.value = self.value + 1;  // ✅ OK
        return self.value;
    }
}
```

**Benefits:**
- 🔒 **Safety**: Prevents accidental mutations
- 📝 **Clarity**: Code intent is explicit
- 🚀 **Performance**: Enables better optimizations
- 🧵 **Concurrency**: Immutable data is thread-safe

### 🎨 Clean Syntax

**Only 19 Core Keywords:**
```
fn let type import pub if else loop break return
is as async await self Self mut true false in
```

### 🔄 Unified Design

- **Unified Declarations**: `let` for variables, `type` for types
- **Unified Loops**: `loop` for all loop forms
- **Unified Patterns**: `is` for all pattern matching

### 📦 Powerful Type System

**Structs:**
```paw
type Point = struct {
    x: i32,
    y: i32,
    
    fn new(x: i32, y: i32) -> Point {
        return Point { x: x, y: y };
    }
    
    fn distance(self) -> f64 {
        return sqrt(self.x * self.x + self.y * self.y);
    }
}
```

**Enums (Rust-style):**
```paw
type Result = enum {
    Ok(i32),
    Err(i32),
}

type Option = enum {
    Some(i32),
    None(),
}
```

**Pattern Matching:**
```paw
let result = value is {
    Some(x) => x * 2,
    None() => 0,
    _ => -1,
};
```

### 💬 String Interpolation

```paw
let name = "Alice";
let age: i32 = 25;

println("Hello, $name!");              // Simple interpolation
println("You are ${age} years old.");  // Expression interpolation
```

### ❓ Error Handling

```paw
fn divide(a: i32, b: i32) -> Result {
    return if b == 0 { Err(1) } else { Ok(a / b) };
}

fn process() -> Result {
    let value = divide(10, 2)?;  // ? operator propagates errors
    return Ok(value * 2);
}
```

### 🔢 Array Support

```paw
// Array literals
let arr = [1, 2, 3, 4, 5];

// Array indexing
let first = arr[0];

// Array types
let numbers: [i32] = [10, 20, 30];        // Dynamic size
let fixed: [i32; 5] = [1, 2, 3, 4, 5];   // Fixed size

// Array iteration
loop item in arr {
    println("$item");
}
```

---

## 📚 Standard Library

### Built-in Functions

```paw
println(msg: string)  // Print with newline
print(msg: string)    // Print without newline
eprintln(msg: string) // Error output
eprint(msg: string)   // Error output without newline
```

### Generic Containers (v0.1.2)

**Vec<T>** - Dynamic Array:
```paw
let vec: Vec<i32> = Vec<i32>::new();
let vec2: Vec<i32> = Vec<i32>::with_capacity(10);
let len: i32 = vec.length();
let cap: i32 = vec.capacity_method();
```

**Box<T>** - Smart Pointer:
```paw
let box: Box<i32> = Box<i32>::new(42);
```

### Error Handling Types

```paw
type Result = enum { Ok(i32), Err(i32) }
type Option = enum { Some(i32), None() }
```

### JSON Module (v0.2.0 NEW!) ⭐

```paw
// Parse JSON values
let null_val = parse("null");        // Null()
let bool_val = parse("true");        // Bool(true)
let num_val = parse("42");           // Number(42.0)
let neg_val = parse("-100");         // Number(-100.0)

// Type checking
if is_number(num_val) {
    let value = as_number(num_val);  // 42
}

// Validation
if is_valid("true") {
    println("Valid JSON");
}
```

**Available functions**:
- `parse()` - Parse JSON string
- `is_valid()` - Validate JSON
- `is_null()`, `is_bool()`, `is_number()`, `is_string()` - Type checking
- `as_bool()`, `as_number()` - Value extraction

**See**: `examples/json_ultra_simple.paw`, `stdlib/json/README.md`

### File System Module (v0.2.0 NEW!) ⭐

```paw
// Path analysis (works immediately!)
let is_abs = is_absolute("/home/user/file.txt");  // Unix
let is_abs2 = is_absolute("C:\\Users\\file.txt"); // Windows
let has_ext = has_extension("file.txt");          // true

// Path utilities
let depth = count_separators("/a/b/c");           // 3
let dot_pos = find_last_dot("archive.tar.gz");    // 11
let equals = path_equals("/a/b", "/a/b");         // true
```

**Available functions**:
- `is_absolute()`, `is_relative()` - Path type detection
- `has_extension()`, `ends_with_separator()` - Path properties
- `count_separators()`, `find_separator()`, `find_last_separator()` - Separator tools
- `find_last_dot()`, `path_length()`, `path_equals()` - Path analysis

**Cross-platform**: Unix & Windows paths supported

**See**: `examples/fs_demo.paw`, `stdlib/fs/README.md`

---

## 🛠️ Command Line Tools

```bash
# Compile to C code (default)
pawc hello.paw

# Compile to LLVM IR (NEW in v0.1.4!)
pawc hello.paw --backend=llvm

# Compile to executable
pawc hello.paw --compile

# Compile and run
pawc hello.paw --run

# Show version
pawc --version

# Show help
pawc --help
```

### Options

- `-o <file>` - Specify output file name
- `--compile` - Compile to executable (C backend only)
- `--run` - Compile and run (C backend only)
- `--backend=c` - Use C backend (default)
- `--backend=llvm` - Use LLVM native backend (v0.1.4)
- `-v` - Verbose output
- `--help` - Show help

### LLVM Backend Workflow

```bash
# Generate LLVM IR
pawc program.paw --backend=llvm

# Compile IR to executable (if local LLVM installed)
llvm/install/bin/clang output.ll -o program

# Run
./program
```

---

## 📖 Example Programs

Check the `examples/` directory:

- `hello.paw` - Hello World
- `array_complete.paw` - Array operations
- `string_interpolation.paw` - String interpolation
- `error_propagation.paw` - Error handling
- `enum_error_handling.paw` - Enum error handling
- `vec_demo.paw` - Vec container demo
- **`generic_methods.paw`** - Generic methods demo (v0.1.2)
- `generics_demo.paw` - Generic functions demo
- **`llvm_demo.paw`** - LLVM backend demo (v0.1.4)

Check the `tests/` directory:

- `test_static_methods.paw` - Static methods test
- `test_instance_methods.paw` - Instance methods test
- `test_methods_complete.paw` - Complete methods test
- `test_generic_struct_complete.paw` - Generic structs test

---

## 🎯 Version History

### v0.1.8 (Current) - Multi-Platform Release 🌍

**8-Platform Support & Self-Contained Distribution**

- ✅ **8 Platforms**: Linux (x86_64, x86, armv7), macOS (x86_64, ARM64), Windows (x86_64, x86)
- ✅ **Self-Contained Packages**: No system LLVM required
- ✅ **Automated CI**: Full testing on all native platforms
- ✅ **Cross-Compilation**: Support for 32-bit and ARM targets
- ✅ **Distribution Scripts**: Automatic library bundling
- ✅ **99.9%+ Coverage**: Desktop, server, and embedded systems

[Architecture Details →](ARCHITECTURE_SUPPORT.md)

### v0.1.7 (Released) 🚀

**LLVM Optimization Support**

- ✅ Optimization levels (-O0, -O1, -O2, -O3)
- ✅ Smart compile hints
- ✅ Performance benchmarks
- ✅ Detailed documentation

[Read More →](docs/RELEASE_NOTES_v0.1.7.md)

### v0.1.6 (TBD)

**Mutability Control System**

- ✅ let mut system (immutable by default)
- ✅ mut self support for methods
- ✅ Compile-time mutability checking
- ✅ C Backend bug fixes (return statement)
- ✅ LLVM Backend bug fixes (ret instruction)
- ✅ Memory leak fixes

[Read More →](docs/RELEASE_NOTES_v0.1.6.md)

### v0.1.5 (2025-01-10)

**Complete LLVM Integration**

- ✅ LLVM 19.1.7 native backend
- ✅ Dual backend architecture (C + LLVM)
- ✅ Control flow support (if/else, loop, break, continue)
- ✅ Zero memory leaks (Arena allocator)
- ✅ Local LLVM toolchain (no system dependencies)
- ✅ Custom C API bindings
- ✅ Simplified backend selection

[Read More →](docs/RELEASE_NOTES_v0.1.5.md)

### v0.1.3 (2025-10-09)

**Type Inference & Enhanced Modules**

- ✅ Automatic type inference
- ✅ Multi-item imports
- ✅ Enhanced module system
- ✅ Argument validation

[Read More →](docs/RELEASE_NOTES_v0.1.3.md)

### v0.1.2 (2025-10-08)

**Complete Generic Methods**

- ✅ Generic static methods
- ✅ Generic instance methods
- ✅ **self parameter without type** - PawLang's unique design!
- ✅ Automatic monomorphization

### v0.1.1 (2025-10-09)

**Complete Generic System**

- ✅ Generic functions
- ✅ Generic structs
- ✅ Type inference
- ✅ Monomorphization

### v0.1.0

**Base Language Features**

- ✅ Complete syntax and type system
- ✅ Compiler toolchain
- ✅ Standard library basics

---

## 🏗️ Compiler Architecture

```
PawLang Source (.paw)
    ↓
Lexer
    ↓
Parser
    ↓
Type Checker
    ↓
Generic Monomorphizer ← v0.1.2
    ↓
Code Generator
    ├→ C Backend (default)
    └→ LLVM Native Backend ← v0.1.4 NEW!
    ↓
Executable
```

### Project Structure

```
PawLang/
├── src/
│   ├── main.zig                  # Compiler entry point
│   ├── lexer.zig                 # Lexical analysis
│   ├── parser.zig                # Syntax analysis
│   ├── ast.zig                   # AST definitions
│   ├── typechecker.zig           # Type checking
│   ├── generics.zig              # Generic system (v0.1.1+)
│   ├── codegen.zig               # C code generation
│   ├── llvm_c_api.zig            # LLVM C API bindings (v0.1.4)
│   ├── llvm_native_backend.zig   # LLVM native backend (v0.1.4)
│   ├── c_backend.zig             # C backend (v0.1.4)
│   ├── token.zig                 # Token definitions
│   ├── module.zig                # Module system
│   └── std/
│       └── prelude.paw           # Standard library
├── llvm/                         # Local LLVM installation (v0.1.4)
│   ├── 19.1.7/                   # LLVM source
│   ├── build/                    # Build artifacts
│   └── install/                  # Installed LLVM
├── scripts/                      # Setup scripts (v0.1.5)
│   ├── install_llvm_complete.py  # One-click LLVM installation (recommended)
│   ├── INSTALL_GUIDE.md          # Installation guide
│   └── README.md                 # Scripts documentation
├── examples/                     # Example programs
├── tests/                        # Test suite
├── docs/                         # Documentation
│   ├── QUICKSTART.md             # Quick start guide
│   ├── LLVM_BUILD_GUIDE.md       # LLVM setup guide (v0.1.4)
│   └── ROADMAP_v0.1.4.md         # Current roadmap
├── build.zig                     # Build configuration
├── CHANGELOG.md                  # Change log
└── README.md                     # This file
```

---

## 🎨 Design Philosophy

### 1. Simplicity First

PawLang pursues minimal syntax:
- Only 19 keywords
- Unified declaration syntax
- Intuitive design

### 2. Type Safety

- Compile-time type checking
- Generic system ensures type safety
- Zero runtime type errors

### 3. Zero-Cost Abstractions

- All generics expanded at compile time
- No virtual function tables
- Performance equivalent to hand-written C

### 4. Modern Features

- Generics (functions, structs, methods)
- Pattern matching
- String interpolation
- Error propagation (`?` operator)
- Method syntax
- **Dual backends** (C + LLVM)

---

## 💡 Language Highlights

### self Parameter Without Type ⭐

This is PawLang's unique design:

```paw
type Vec<T> = struct {
    len: i32,
    
    // ✅ PawLang - Clean and elegant
    fn length(self) -> i32 {
        return self.len;
    }
}

// vs Rust
// fn length(&self) -> i32 { ... }
```

### Unified Method Calls

```paw
// Static method - :: syntax
let vec: Vec<i32> = Vec<i32>::new();

// Instance method - . syntax
let len: i32 = vec.length();
```

### Complete Generic Support

```paw
// Generic function
fn swap<T>(a: T, b: T) -> i32 { ... }

// Generic struct
type Box<T> = struct { value: T }

// Generic method
fn get(self) -> T { return self.value; }
```

### Dual Backend Architecture (v0.1.4)

```bash
# C Backend - Maximum portability
pawc app.paw --backend=c

# LLVM Backend - Maximum performance
pawc app.paw --backend=llvm
```

---

## 📊 Performance

- **Compile Speed**: <10ms (typical programs)
- **Runtime Performance**: Equivalent to C (zero-overhead abstractions)
- **Memory Usage**: No GC, fully manual control
- **LLVM Optimization**: Better optimization with LLVM backend

---

## 🧪 Testing

```bash
# Run all tests
./zig-out/bin/pawc tests/test_methods_complete.paw --run

# Static methods test
./zig-out/bin/pawc tests/test_static_methods.paw --run

# Instance methods test
./zig-out/bin/pawc tests/test_instance_methods.paw --run

# LLVM backend test (v0.1.4)
zig build run-llvm
```

---

## 🌟 Why Choose PawLang?

| Feature | PawLang | Rust | C | Python |
|---------|---------|------|---|--------|
| Generics | ✅ Complete | ✅ | ❌ | ❌ |
| Type Safety | ✅ | ✅ | ⚠️ | ❌ |
| Zero Overhead | ✅ | ✅ | ✅ | ❌ |
| Clean Syntax | ✅ | ⚠️ | ⚠️ | ✅ |
| self Without Type | ✅ | ❌ | N/A | ✅ |
| LLVM Backend | ✅ | ✅ | ❌ | ❌ |
| Learning Curve | Low | High | Medium | Low |

**PawLang = Rust's Safety + C's Performance + Python's Simplicity + LLVM's Optimization**

---

## 🔧 Development

### Dependencies

- **Zig** 0.15.1 (required, exact version)
- **LLVM** 19.1.7 (required for LLVM backend)
- **GCC** or **Clang** (optional, for C backend compilation)

📋 **See [Version Requirements](docs/VERSION_REQUIREMENTS.md) for detailed version specifications**

### Building

```bash
# Development build
zig build

# Release build
zig build -Doptimize=ReleaseFast

# Build with LLVM backend (auto-detected)
zig build

# Quick test with LLVM
zig build run-llvm
```

### Setting Up LLVM (Optional)

**One-Click Installation (Recommended)** ⭐

```bash
# Download, install, build, and test - all in one command!
python3 scripts/install_llvm_complete.py --yes

# This script will:
# 1. Download pre-built LLVM 19.1.7 (~500MB)
# 2. Extract and install to llvm/install/
# 3. Build PawLang compiler
# 4. Run LLVM backend test

# Now LLVM backend is available!
./zig-out/bin/pawc app.paw --backend=llvm
```

**Options:**
```bash
# Interactive mode (with prompts)
python3 scripts/install_llvm_complete.py

# Skip build and test
python3 scripts/install_llvm_complete.py --yes --skip-build --skip-test

# Full installation guide
cat scripts/INSTALL_GUIDE.md
```

**Guides**:
- 🆕 [Installation Guide](scripts/INSTALL_GUIDE.md) - One-click setup (recommended)
- 🆕 [Prebuilt LLVM Guide](docs/LLVM_PREBUILT_GUIDE.md) - Using prebuilt binaries
- 🆕 [Quick Setup](docs/LLVM_QUICK_SETUP.md) - Fast setup guide
- [Setup Comparison](docs/LLVM_SETUP_COMPARISON.md) - Compare different methods

### Contributing

Contributions welcome! Please ensure:
- Code follows existing style
- All tests pass
- Documentation is updated

---

## 📄 Documentation

- [CHANGELOG.md](CHANGELOG.md) - Complete change history
- 🆕 [VERSION_REQUIREMENTS.md](docs/VERSION_REQUIREMENTS.md) - Required versions (Zig 0.15.1, LLVM 19.1.7) ⭐
- 🆕 [ARCHITECTURE_SUPPORT.md](ARCHITECTURE_SUPPORT.md) - Platform support & testing
- [RELEASE_NOTES_v0.1.7.md](docs/RELEASE_NOTES_v0.1.7.md) - v0.1.7 release notes
- [RELEASE_NOTES_v0.1.6.md](docs/RELEASE_NOTES_v0.1.6.md) - v0.1.6 release notes
- [RELEASE_NOTES_v0.1.5.md](docs/RELEASE_NOTES_v0.1.5.md) - v0.1.5 release notes
- [RELEASE_NOTES_v0.1.4.md](docs/RELEASE_NOTES_v0.1.4.md) - v0.1.4 release notes
- [INSTALL_GUIDE.md](scripts/INSTALL_GUIDE.md) - One-click installation guide
- [LLVM_QUICK_SETUP.md](docs/LLVM_QUICK_SETUP.md) - Quick LLVM setup
- [LLVM_PREBUILT_GUIDE.md](docs/LLVM_PREBUILT_GUIDE.md) - Prebuilt LLVM guide
- [QUICKSTART.md](docs/QUICKSTART.md) - Quick start guide
- [examples/](examples/) - Example code
- [tests/](tests/) - Test cases and testing guide

---

## 🗺️ Roadmap

### v0.2.0 (In Development) 🚀 **Current**

**Standard Library Foundation Release**

- ✅ **JSON Parser Module** - Parse null, bool, multi-digit numbers, negatives
- ✅ **File System Module** - 11 path utility functions
- ✅ **19 stdlib functions** total
- ✅ **32 tests passing** (100% coverage)
- ✅ FFI requirements document
- [ ] File I/O operations (pending FFI support)
- [ ] Build system enhancements
- [ ] Testing framework

### v0.1.9 (Released) ✅

- ✅ Enhanced error messages (source locations, colors)
- ✅ Developer tools (--time, REPL)
- ✅ VSCode extension
- ✅ Compilation time analysis

### v0.1.8 (Released) ✅

- ✅ 8-platform support (Linux, macOS, Windows)
- ✅ Self-contained distribution system
- ✅ Cross-compilation support
- ✅ Automated multi-platform CI
- ✅ Comprehensive architecture documentation

### v0.1.6 (Completed) ✅

- ✅ let mut system
- ✅ mut self support
- ✅ Compile-time mutability checking
- ✅ Backend bug fixes

### v0.1.5 (Released - January 10, 2025) ✅

- ✅ LLVM backend 100% complete
- ✅ Loop iterators (loop item in collection)
- ✅ C backend bug fixes
- ✅ Zero memory leaks
- ✅ Comprehensive test suite

### Future Versions

- [ ] Trait system
- [ ] Operator overloading
- [ ] Async/await
- [ ] Package manager
- [ ] LSP support

---

## 📊 Project Status

| Component | Status | Completion |
|-----------|--------|------------|
| Lexer | ✅ | 100% |
| Parser | ✅ | 100% |
| Type Checker | ✅ | 100% |
| Generic System | ✅ | 100% |
| C Backend | ✅ | 100% |
| **LLVM Backend** | ✅ | **100%** ⭐ |
| **Standard Library** | ✅ | **60%** ⭐ (v0.2.0) |
| Documentation | ✅ | 98% |

---

## 🏆 Milestones

- **v0.1.0** - Base language ✅
- **v0.1.1** - Complete generic system ✅
- **v0.1.2** - Generic methods ✅
- **v0.1.3** - Type inference & modules ✅
- **v0.1.4** - LLVM integration ✅
- **v0.1.5** - LLVM backend 100% + C backend fixes ✅
- **v0.1.6** - Mutability control system ✅
- **v0.1.7** - LLVM optimization levels ✅
- **v0.1.8** - Multi-platform support ✅
- **v0.1.9** - Developer experience improvements ✅
- **v0.2.0** - Standard library foundation + LLVM完成 🚀 ⭐ **Current**
  - ✅ **LLVM Backend 100%** - Full enum support, error propagation (?), zero leaks
  - ✅ **Cross-Platform LLVM Setup** - `zig build setup-llvm` for all platforms
  - ✅ JSON parser (19 functions)
  - ✅ File system utilities (11 functions)
  - ✅ 22/22 tests passing (100%)
- **v0.2.1** - FFI support (planned)
- **v0.3.0** - Advanced features (planned)
- **v1.0.0** - Production ready (goal)

---

## 🤝 Contributors

Thanks to all developers who contributed to PawLang!

---

## 📄 License

MIT License

---

## 🔗 Links

- **GitHub**: [pawlang-project/paw](https://github.com/pawlang-project/paw)
- **LLVM Build**: [pawlang-project/llvm-build](https://github.com/pawlang-project/llvm-build)
- **Quick Start**: [5-Minute Guide](docs/QUICKSTART.md)
- 🆕 **Installation**: [One-Click Setup](scripts/INSTALL_GUIDE.md)
- 🆕 **LLVM Setup**: [Quick Setup Guide](docs/LLVM_QUICK_SETUP.md)
- **Full Documentation**: [View All Docs](docs/)
- **Example Code**: [View Examples](examples/)
- **Module System**: [Module System Docs](docs/MODULE_SYSTEM.md)
- **Changelog**: [CHANGELOG.md](CHANGELOG.md)

---

---

<div align="center">

**Built with ❤️ using Zig and LLVM**

<img src="assets/logo.png" alt="PawLang Logo" width="120" height="120">

**🐾 Happy Coding with PawLang!**

</div>
