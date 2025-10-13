# PawLang Compiler Source Code

This directory contains the core PawLang compiler implementation.

---

## 📁 Directory Structure

```
src/
├── main.zig                    (29KB) - Compiler entry point
├── lexer.zig                   (13KB) - Lexical analysis
├── token.zig                   (3KB)  - Token definitions
├── parser.zig                  (72KB) - Syntax analysis
├── ast.zig                     (18KB) - Abstract Syntax Tree
├── typechecker.zig             (59KB) - Type checking
├── generics.zig                (29KB) - Generic type system
├── module.zig                  (6KB)  - Module system
├── diagnostic.zig              (7KB)  - Error diagnostics
├── codegen.zig                (107KB) - C backend code generation
├── c_backend.zig               (7KB)  - C backend interface
├── llvm_native_backend.zig     (79KB) - LLVM backend
├── llvm_c_api.zig              (23KB) - LLVM C API bindings
├── repl.zig                    (5KB)  - Interactive REPL
├── builtin/                           - Built-in functions
│   ├── mod.zig                 - Module entry
│   ├── fs.zig                  (8KB)  - File system operations
│   └── memory.zig              (4KB)  - Memory management
└── prelude/                           - Standard prelude
    └── prelude.paw             (8KB)  - Standard library prelude
```

**Total**: 14 core files + 2 directories

---

## 🔄 Compilation Pipeline

```
Source File (.paw)
    ↓
[Lexer] → Tokens
    ↓
[Parser] → AST
    ↓
[TypeChecker] → Typed AST
    ↓
    ├─→ [C Backend] → output.c
    └─→ [LLVM Backend] → output.ll
```

---

## 📦 Core Modules

### Frontend (Lexical & Syntax Analysis)

**lexer.zig** (13KB)
- Tokenizes source code
- Handles string literals, numbers, keywords
- Line and column tracking

**token.zig** (3KB)
- Token type definitions
- Keywords, operators, literals

**parser.zig** (72KB)
- Builds Abstract Syntax Tree
- Handles all PawLang syntax
- Error recovery

**ast.zig** (18KB)
- AST node definitions
- Expression and statement types
- Type representations

---

### Middle-end (Semantic Analysis)

**typechecker.zig** (59KB)
- Type inference and checking
- Generic instantiation
- Error reporting

**generics.zig** (29KB)
- Generic type system
- Type parameter resolution
- Generic constraints

**module.zig** (6KB)
- Module loading and resolution
- Import/export handling
- Standard library integration

**diagnostic.zig** (7KB)
- Error message formatting
- Warning system
- Source location tracking

---

### Backend (Code Generation)

**codegen.zig** (107KB)
- C backend code generation
- Handles all PawLang constructs
- Runtime support code generation

**c_backend.zig** (7KB)
- C backend interface
- Output formatting
- C code emission

**llvm_native_backend.zig** (79KB)
- LLVM IR generation
- Direct LLVM API usage
- Optimization support

**llvm_c_api.zig** (23KB)
- LLVM C API bindings for Zig
- Type-safe wrappers
- Platform abstraction

---

### Interactive & Built-ins

**main.zig** (29KB)
- Command-line interface
- Compiler driver
- Build configuration

**repl.zig** (5KB)
- Read-Eval-Print Loop
- Interactive mode
- Expression evaluation

**builtin/** (3 files)
- `fs.zig` - File system operations (read, write, exists)
- `memory.zig` - Memory allocation primitives
- `mod.zig` - Built-in module exports

**prelude/** (1 file)
- `prelude.paw` - Standard prelude (println, assert, etc.)

---

## 🎯 Key Files by Size

| File | Size | Purpose |
|------|------|---------|
| codegen.zig | 107KB | C backend (largest) |
| llvm_native_backend.zig | 79KB | LLVM backend |
| parser.zig | 72KB | Syntax parsing |
| typechecker.zig | 59KB | Type checking |
| main.zig | 29KB | Compiler entry |
| generics.zig | 29KB | Generic system |
| llvm_c_api.zig | 23KB | LLVM bindings |

**Total source code**: ~500KB

---

## 🔧 Backend Comparison

| Feature | C Backend | LLVM Backend |
|---------|-----------|--------------|
| File | codegen.zig | llvm_native_backend.zig |
| Output | C code | LLVM IR |
| Dependencies | None | LLVM 21.1.3 |
| Optimization | GCC/Clang | LLVM optimizer |
| Status | ✅ Stable | ✅ Stable |

---

## 💡 Development Guide

### Adding New Language Features

1. **Lexer**: Add tokens in `token.zig`, implement in `lexer.zig`
2. **Parser**: Add AST nodes in `ast.zig`, parse in `parser.zig`
3. **TypeChecker**: Add type rules in `typechecker.zig`
4. **Codegen**: Implement in both `codegen.zig` (C) and `llvm_native_backend.zig` (LLVM)

### Modifying Built-ins

- Add functions in `builtin/fs.zig` or `builtin/memory.zig`
- Update exports in `builtin/mod.zig`
- Add Zig implementations with `export fn paw_*`

### Updating Standard Library

- Modify `prelude/prelude.paw` for PawLang-level functions
- Use `builtin` module for low-level operations

---

## 📊 Code Organization

**Well-organized** ✅
- Clear separation of concerns
- Modular design
- No temporary files
- Consistent naming

**Total files**: 14 + 4 (builtin/prelude)  
**Total size**: ~500KB

---

**Last Updated**: 2025-10-13  
**PawLang Version**: 0.2.0

