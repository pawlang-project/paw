# PawLang

A small teaching/experimental language compiler using **Cranelift** as the backend.  
Supports: **first-class integers/floats/booleans/chars/strings, `Byte` (true `u8`), control flow (`if/while/for/match`), functions/overloading, generics (monomorphization), `trait/impl`, structs, smart pointers, standard library `std::fmt` and `std::mem`**, and generates **object files** for native targets with **cross-compilation support**.

---

## Quick Start

### Dependencies

* Rust (stable, `cargo` available)
* Zig compiler (for cross-compilation linking)
* Native target toolchain (for linking `.o` files to executables)

### Build

```bash
git clone <your-repo-url> PawLang
cd PawLang
cargo build
```

### Run an Example

The project contains several `.paw` examples/tests. The driver accepts source file paths:

```bash
# Build and run a Paw program
cargo run -- build dev --input paw/main.paw

# Cross-compile to different targets
cargo run -- build dev --target x86_64-unknown-linux-gnu
cargo run -- build release --target x86_64-pc-windows-gnu
cargo run -- build dev --target x86_64-apple-darwin
```

### Run Tests

```bash
cargo test
```

### Cross-Compilation

PawLang supports cross-compilation to 3 target platforms:

```bash
# List supported targets
cargo run -- --list-targets

# Cross-compile examples
cargo run -- build dev --target x86_64-unknown-linux-gnu
cargo run -- build release --target x86_64-pc-windows-gnu
cargo run -- build dev --target x86_64-apple-darwin
```

> **Note**: The compiler automatically generates object files in the correct format (ELF for Linux, COFF for Windows, Mach-O for macOS) and links them using Zig for cross-compilation.

---

## Language Overview

### Basic Types

* `Void` (only for function returns)
* `Bool` (internally represented as `u8`, 0/non-zero)
* `Byte` (**true `u8`**)
* `Char` (Unicode scalar, corresponds to `u32`)
* `Int` (i32)
* `Long` (i64)
* `Float` (f32)
* `Double` (f64)
* `String` (passed as pointer at runtime, treated as UTF-8 for printing)

### Literals

```
0, 1, 123                 # Int
0L, 123L                  # Long
true, false               # Bool
'a', '\n'                 # Char
"hello"                   # String
1.0f, 2.3f                # Float
1.0, 2.3                  # Double
```

### Variables and Constants

```paw
let x: Int = 42;
let y: Byte = 255;
let z: Double = 3.14;
```

The intermediate representation supports `is_const` marking, equivalent to `const` at the source language level.

### Expressions and Control Flow

* Arithmetic: `+ - * /`
* Comparison: `< <= > >= == !=`
* Logical: `&& || !`
* Blocks/scope: `{ ... }` (block expressions have values; default type `Int` with no tail expression)
* `if .. else`, `while`, `for(init; cond; step)`, `match`

### Structs

```paw
struct Point<T> {
    x: T,
    y: T,
}

fn main() -> Int {
    let p: Point<Int> = Point<Int>{ x: 10, y: 20 };
    println(p.x);  // 10
    println(p.y);  // 20
    0
}
```

* **Generic structs**: Support type parameters `struct Point<T>`
* **Field access**: Use dot notation `p.x`, `p.y`
* **Struct literals**: `Point<Int>{ x: 10, y: 20 }`
* **Memory layout**: Structs are allocated on the stack, fields arranged in declaration order

### Functions / Generics / Overloading

```paw
fn add(x: Int, y: Int) -> Int { x + y }

fn id<T>(x: T) -> T { x }

trait Show<T> {
    fn show(x: T) -> Void;
}

impl Show<Int> {
    fn show(x: Int) -> Void { println(x); }
}
```

* **Generic functions** use **monomorphization**: `foo<Int>` generates a specialized symbol and participates in linking.
* **Overload resolution** completes during type checking; code generation uses type-encoded **mangled** symbols (`name__ol__P...__R...`).
* **trait/impl**:
  * `impl Trait<Args...>` validates *arity*, method set, and signatures after parameter substitution during type checking.
  * **Qualified name calls** `Trait::<T...>::method(...)`: with type variables in `T...`, monomorphize as needed; with all concrete types, directly degrade to free function symbols.

### Smart Pointers

```paw
import "std::mem";

fn main() -> Int {
    // Box<T> - exclusive ownership
    let b: Box<Int> = Box::new<Int>(42);
    let value: Int = Box::get<Int>(b);
    println(value);  // 42
    Box::free<Int>(b);
    
    // Rc<T> - reference counting
    let r: Rc<Int> = Rc::new<Int>(100);
    let r2: Rc<Int> = Rc::clone<Int>(r);
    println(Rc::strong_count<Int>(r));  // 2
    Rc::drop<Int>(r2);
    println(Rc::strong_count<Int>(r));  // 1
    Rc::drop<Int>(r);
    
    // Arc<T> - atomic reference counting
    let a: Arc<Int> = Arc::new<Int>(200);
    let a2: Arc<Int> = Arc::clone<Int>(a);
    println(Arc::strong_count<Int>(a));  // 2
    Arc::drop<Int>(a2);
    println(Arc::strong_count<Int>(a));  // 1
    Arc::drop<Int>(a);
    
    0
}
```

* **Box<T>**: Smart pointer with exclusive ownership
* **Rc<T>**: Reference-counted smart pointer (not thread-safe)
* **Arc<T>**: Atomically reference-counted smart pointer (thread-safe)
* **Void type support**: `BoxVoid::new()`, `RcVoid::new()`, `ArcVoid::new()`
* **Supported types**: All basic types (`Byte`, `Int`, `Long`, `Bool`, `Float`, `Double`, `Char`, `String`, `Void`)

### Standard Library

#### fmt Module

Import:

```paw
import "std::fmt";
```

Printing:

```paw
println(123);            // Int
println(3.14);           // Double
println(true);           // Bool -> "true"/"false"
println('A');            // Char (UTF-8)
println("hello");        // String (UTF-8)
println<Byte>(255);      // Print in Byte context
```

#### mem Module

Import:

```paw
import "std::mem";
```

Smart pointer operations:

```paw
// Box<T>
let b: Box<Int> = Box::new<Int>(42);
let value: Int = Box::get<Int>(b);
Box::set<Int>(b, 100);
Box::free<Int>(b);

// Rc<T>
let r: Rc<Int> = Rc::new<Int>(42);
let r2: Rc<Int> = Rc::clone<Int>(r);
let count: Int = Rc::strong_count<Int>(r);
Rc::drop<Int>(r2);

// Arc<T>
let a: Arc<Int> = Arc::new<Int>(42);
let a2: Arc<Int> = Arc::clone<Int>(a);
let count: Int = Arc::strong_count<Int>(a);
Arc::drop<Int>(a2);
```

`print` (no newline) is affected by line buffering; use `println` or call `fflush(stdout)`/`stdout().flush()` at runtime to flush.

---

## `Byte` (u8) Key Points

* **Type**: `Byte` is a **true `u8`**, range 0..=255.
* **Interaction with other numerics**: Binary numeric operations require consistent IR types on both sides; use explicit `as` conversion for type promotion (e.g., extending `Byte` to `Int`).
* **Example**:

```paw
import "std::fmt";

fn main() -> Int {
    let x: Byte = 0;

    println<Byte>(x - 1);  // 255 (printed in Byte context)
    let y: Byte = x - 1;   // truncated to 8 bits for Byte assignment
    println(y);            // 255

    // Explicitly promote for Int operations:
    println((x as Int) - 1);  // -1

    0
}
```

---

## Typical Output (Excerpt)

```
[TEST] mem + fmt + syntax begin
[casts]
3
3
3
[ctrlflow]
if-true
15
0
1
2
100
200
999
314
314
echo!
echo!
[mem]
42
99
2
123
123
1
2
3.5
mem + fmt OK
255
[ops]
12
2
35
1
3.5
2.5
1.5
6
false
false
true
true
false
true
false
true
true
11
2.5
[block-expr]
30
[struct]
10
30
42
[TEST] mem + fmt + syntax end
```

---

## Diagnostics and Error Codes (Excerpt)

**Type Checking**

* `E2101`: Duplicate local variable name
* `E2102`: Unknown variable
* `E2103`: Assignment to constant
* `E2210`: `if` branch types inconsistent
* `E2311`: Function call has no matching overload
* `E2303`: Qualified name call missing explicit type arguments
* `E2308`: Qualified name call not allowed by current `where` constraints
* ……

**Code Generation**

* `CG0001`: Missing symbol/function ID
* `CG0002`: ABI-unavailable type
* `CG0003`: impl method symbol unknown (undeclared)
* `CG0004`: Generic template/monomorphization issues
* `CG0005`: Overload resolution produced multiple candidates (call not unique)

---

## Implementation Details (Brief)

* **Cranelift IR**:
  * Language→IR mapping: `Byte/Bool → i8`, `Int → i32`, `Long → i64`, `Float → f32`, `Double → f64`, `Char → i32`, `String → i64` (pointer).
  * Booleans use `b1` in conditions, with `i8`↔`b1` conversion at entry/exit points.
  * String constants reside as read-only data, accessed via `global_value(i64)` within functions.
  * Structs mapped as `i64` pointers, allocated on the stack.
* **Monomorphization**:
  * Pre-scans explicit `<...>` calls and declares instances; implicit common forms (like `println<T>(x:T)`) are instantiated on-demand at call sites.
  * impl generic methods are monomorphized and immediately defined at qualified name call sites.
* **Symbol mangling**: Overload and generic instance parameter/return types are encoded into symbol names to ensure linking uniqueness.
* **Memory management**:
  * Smart pointers are implemented via Rust runtime, providing C ABI interface.
  * Supports cross-platform compilation (Windows, Linux, macOS Intel).
* **Cross-compilation**:
  * Uses Cranelift for native object file generation (ELF, COFF, Mach-O)
  * Uses Zig as the cross-compilation linker
  * CLI module provides English error messages and help information

---

## FAQ

**Q: Why does `println(x - 1)` sometimes print 255, sometimes -1?**  
A: The expression's **static type** and selected overload determine the output: printing in `Byte` context shows 0..=255; explicit conversion to `Int` first shows signed integer behavior.

**Q: `print` not working?**  
A: It is buffered. Use `println`, or call `fflush(stdout)`/`stdout().flush()` at runtime.

**Q: How to create struct instances?**  
A: Use struct literal syntax: `Point<Int>{ x: 10, y: 20 }`. All field values must be provided.

**Q: When do smart pointers release memory?**  
A: `Box<T>` releases on calling `Box::free<T>()`; `Rc<T>` and `Arc<T>` automatically release on reference count reaching 0 (call `Rc::drop<T>()` or `Arc::drop<T>()` to decrease count).

**Q: How to cross-compile to different platforms?**  
A: Use the `--target` option: `pawc build dev --target x86_64-unknown-linux-gnu`. See `pawc --list-targets` for supported platforms.

**Q: What if I get linking errors?**  
A: Ensure Zig is installed and in PATH. The compiler uses Zig for cross-compilation linking.

---

## Roadmap

* Arrays/slices and slice literals
* Enums and pattern matching destructuring
* More comprehensive standard library and I/O
* Stronger constant folding and optimization using Cranelift passes
* More flexible implicit generic inference
* True generic smart pointer implementation (uses concrete type implementations)
* Support for `aarch64-apple-darwin` with improved Cranelift support
* Additional target platforms with expanded Cranelift support