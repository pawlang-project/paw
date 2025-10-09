# 🐾 PawLang

**A modern systems programming language with Rust-level safety and cleaner syntax**

[![Version](https://img.shields.io/badge/version-0.1.4-blue.svg)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)](#)

---

## 🚀 Quick Start

### Installation

```bash
# Clone the repository
git clone https://github.com/yourusername/PawLang.git
cd PawLang

# Build the compiler
zig build

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
```

**Features:**
- ✅ **C Backend**: Stable, portable, works everywhere
- ✅ **LLVM Backend**: Native API integration, control flow support
- ✅ **Control Flow**: if/else, while loops, break/continue
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
    while i <= n {
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

### v0.1.4 (2025-01-09) - Current Version 🚀

**Complete LLVM Integration**

- ✅ LLVM 19.1.6 native backend
- ✅ Dual backend architecture (C + LLVM)
- ✅ Control flow support (if/else, while, break, continue)
- ✅ Zero memory leaks (Arena allocator)
- ✅ Local LLVM toolchain (no system dependencies)
- ✅ Custom C API bindings
- ✅ Simplified backend selection

[Read More →](RELEASE_NOTES_v0.1.4.md)

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
│   ├── 19.1.6/                   # LLVM source
│   ├── build/                    # Build artifacts
│   └── install/                  # Installed LLVM
├── scripts/                      # Build scripts (v0.1.4)
│   ├── setup_llvm_source.sh      # LLVM setup
│   ├── build_llvm_local.sh       # LLVM build
│   └── compile_with_local_llvm.sh # LLVM compile helper
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

- **Zig** 0.14.0 or higher
- **GCC** or **Clang** (optional, for C backend)
- **CMake** and **Ninja** (optional, for building local LLVM)

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

### Building Local LLVM (Optional)

```bash
# Setup LLVM source
./scripts/setup_llvm_source.sh

# Build LLVM locally (~30-60 minutes)
./scripts/build_llvm_local.sh

# Now LLVM backend is available!
./zig-out/bin/pawc app.paw --backend=llvm
```

For detailed LLVM setup, see [LLVM Build Guide](docs/LLVM_BUILD_GUIDE.md).

### Contributing

Contributions welcome! Please ensure:
- Code follows existing style
- All tests pass
- Documentation is updated

---

## 📄 Documentation

- [CHANGELOG.md](CHANGELOG.md) - Complete change history
- [RELEASE_NOTES_v0.1.4.md](RELEASE_NOTES_v0.1.4.md) - v0.1.4 release notes
- [LLVM_BUILD_GUIDE.md](docs/LLVM_BUILD_GUIDE.md) - LLVM setup guide
- [QUICKSTART.md](docs/QUICKSTART.md) - Quick start guide
- [examples/](examples/) - Example code
- [tests/](tests/) - Test cases

---

## 🗺️ Roadmap

### v0.1.5 (Planned)

- [ ] for loops in LLVM backend
- [ ] More LLVM optimizations
- [ ] Enhanced error messages
- [ ] String type improvements

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
| LLVM Backend | ✅ | 80% |
| Standard Library | 🚧 | 30% |
| Documentation | ✅ | 90% |

---

## 🏆 Milestones

- **v0.1.0** - Base language ✅
- **v0.1.1** - Complete generic system ✅
- **v0.1.2** - Generic methods ✅
- **v0.1.3** - Type inference & modules ✅
- **v0.1.4** - LLVM integration ✅ ⭐ **Current**
- **v0.2.0** - Trait system (planned)
- **v1.0.0** - Production ready (goal)

---

## 🤝 Contributors

Thanks to all developers who contributed to PawLang!

---

## 📄 License

MIT License

---

## 🔗 Links

- **GitHub**: [PawLang Repository](#)
- **Quick Start**: [5-Minute Guide](docs/QUICKSTART.md)
- **Full Documentation**: [View All Docs](docs/)
- **Example Code**: [View Examples](examples/)
- **Module System**: [Module System Docs](docs/MODULE_SYSTEM.md)
- **LLVM Guide**: [LLVM Build Guide](docs/LLVM_BUILD_GUIDE.md)
- **Changelog**: [CHANGELOG.md](CHANGELOG.md)

---

**Built with ❤️ using Zig and LLVM**

**🐾 Happy Coding with PawLang!**
