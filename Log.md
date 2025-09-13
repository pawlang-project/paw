# Paw — README (current snapshot)

> A tiny, ahead-of-time compiled language with traits, generics, and a strict module system.
> Status: **experimental**, feature set still evolving.

---

## Quick start

```bash
# in your project root (same folder as Paw.toml)
pawc build dev       # or: pawc build release
# output: ./build/<dev|release>/<package_name>[.exe]
```

### Minimal project layout

```
your-app/
├─ Paw.toml
├─ main.paw
└─ std/                # example module tree you can import from
   └─ prelude.paw
```

**Paw\.toml**

```toml
[package]
name = "Paw"
version = "0.0.1"

[module]
modules = ["std"]   # expose modules by folder name (optional but recommended)
```

**main.paw**

```paw
import "std::prelude";

fn main() -> Int {
    println_int(42);
    0
}
```

---

## Toolchain & runtime

* **Compiler**: `pawc` (this repo).
* **Linker**: uses `zig cc`. Set `ZIG_BIN` if it’s not on PATH.
* **Runtime static lib**: `libpawrt.a`.
  Place it at `./deps/<rust-triple>/libpawrt.a` **relative to the Rust project root** (where `pawc` is built),
  e.g.:

  ```
  deps/
    x86_64-unknown-linux-gnu/libpawrt.a
    x86_64-apple-darwin/libpawrt.a
    aarch64-apple-darwin/libpawrt.a
    x86_64-pc-windows-gnu/libpawrt.a
  ```

  Or set `PAWRT_LIB` to an absolute file path.

### Useful env vars

* `PAW_TARGET`: overrides target triplet understood by `pawc` (e.g., `x86_64-unknown-linux-gnu`).
* `PAWRT_LIB`: absolute path to `libpawrt.a` (if you don’t use the `deps/` layout above).
* `ZIG_BIN` or `ZIG`: absolute path to the `zig` executable.
* `PAW_VERBOSE=1`: prints the external commands executed.

---

## Module system & imports

Only **double-colon** imports are allowed:

```paw
import "std::prelude";
import "mylib::algo::sort";
```

Rules:

* An import like `"a::b::c"` maps to a file path `a/b/c.paw` under your **project root**.
* No legacy forms (`"std/prelude.paw"`, `"std.prelude"`, relative paths, etc.) are accepted.
* Module names must be identifier-like (`[A-Za-z_][A-Za-z0-9_]*` per segment).

---

## Language tour

### Types (first-class, no user-defined structs yet)

* `Int` (i32), `Long` (i64)
* `Bool` (1-byte, handled as `i8` in ABI)
* `Char` (Unicode scalar, ABI as `i32`)
* `Float` (f32), `Double` (f64)
* `String` (opaque runtime handle; passed as `i64` ABI-wise)
* `Void` (no value)
* **Type variables**: `T`, `U`, …
* **Type application**: `Name<T, U>`. (Parsed/checked; runtime struct types not emitted yet.)
* Return `Void` for side-effect-only functions.

### Literals

* Integers: `0`, `-12` (fits `Int`, otherwise `Long`)
* Long: `123L`
* Float/Double: `1.0`, `-2.5`, `1.0e-3` (currently parsed as **Double**; use `println_double` to print)
* Char: `'A'`, `'\n'`, `'\u{4E2D}'`
* String: `"hello\nworld"`

> Note: `2.5` is `Double`. There’s no `2.5f` suffix yet.

### Expressions & statements

* Arithmetic: `+ - * /`
* Comparisons: `< <= > >= == !=`
* Logical: `&& || !`
* Variables:

  ```paw
  let x: Int = 10;
  const K: Int = 7;      // may be inlined if used
  x = x + 1;
  ```
* If (stmt & expr):

  ```paw
  if (x < 0) { ... } else { ... }
  let y: Int = if (flag) { 1 } else { 0 };
  ```
* While:

  ```paw
  while (i < 10) { i = i + 1; }
  ```
* For (C-style):

  ```paw
  for (let j: Int = 0; j < 3; j = j + 1) { ... }
  ```
* Match (integers/longs/bool/char/wild):

  ```paw
  let r: Int = match (a) {
    0      => { 100 },
    1      => { 200 },
    _      => { 0 },
  };
  ```

### Functions

```paw
fn add(a: Int, b: Int) -> Int {
  return a + b;
}
```

* `return` is optional at tail; last expression of a block is the value if used as tail.

### Extern functions

Declare things provided by the runtime:

```paw
extern fn println_int(x: Int) -> Void;
extern fn println_long(x: Long) -> Void;
extern fn println_bool(x: Bool) -> Void;
extern fn println_char(x: Char) -> Void;
extern fn println_float(x: Float) -> Void;
extern fn println_double(x: Double) -> Void;
extern fn print_str(p: String) -> Void;
extern fn println_str(p: String) -> Void;
```

### Generics (functions)

```paw
fn id<T>(x: T) -> T { x }
fn first<A, B>(a: A, _b: B) -> A { a }
fn choose<T>(b: Bool, x: T, y: T) -> T {
  if (b) { x } else { y }
}
```

* Call with **explicit type args**:

  ```paw
  let a: Int  = id<Int>(41);
  let b: Long = id<Long>(7L);
  let c: Int  = first<Int, Long>(a, b);
  ```
* The compiler **monomorphizes** only sites with explicit type args (e.g. `id$Int`).
* Free type variables cannot reach codegen; typecheck enforces concreteness.

### Traits & impl

Declare a trait:

```paw
trait Eq<T> {
  fn eq(x: T, y: T) -> Bool;
}
```

Provide impls (with **explicit** type args):

```paw
impl Eq<Int> {
  fn eq(x: Int, y: Int) -> Bool { x == y }
}
impl Eq<Long> {
  fn eq(x: Long, y: Long) -> Bool { x == y }
}
```

Use in a generic with a `where` clause:

```paw
fn needEq<T>(a: T, b: T) -> Int
where T: Eq<T> 
{
  if (Eq::eq<T>(a, b)) { 1 } else { 0 }
}
```

* **Qualified call is required**: `Trait::method<...>(args...)`.
* Internally, impl methods are lowered to free functions like
  `__impl_Eq$Int__eq(x:Int,y:Int)->Bool`. This is why the compiler needs to
  **declare impls before codegen** (already handled in the current pipeline).

Multi-parameter trait:

```paw
trait PairEq<A, B> {
  fn eq2(x: A, y: B) -> Bool;
}

impl PairEq<Int, Long> {
  fn eq2(x: Int, y: Long) -> Bool { x == y }
}

fn needPair<T, U>(a: T, b: U) -> Int
where __Self: PairEq<T, U>   // special “self” bound form
{
  if (PairEq::eq2<T, U>(a, b)) { return 1; }
  0
}
```

> The `__Self` trick is a temporary way to constrain “something must implement this trait”; it’s accepted by the typechecker and lowered by codegen.

---

## Standard library (thin FFI, WIP)

What’s currently expected by examples:

* printing: `print_*`, `println_*` for each primitive type
* basic string/vec handles in the runtime (opaque `String` / `Vec`-like via FFI)
* memory helpers: `paw_malloc/paw_free/paw_realloc` (FFI)

These are provided by `libpawrt.a`. If you don’t link it, you’ll get unresolved symbols during the link step.

---

## Build outputs

* Objects: `./build/<dev|release>/out.<o|obj>`
* Executable: `./build/<dev|release>/<package_name>[.exe]`

The target triple is auto-detected; override with `PAW_TARGET`.

---

## Sample program (covers most features)

```paw
import "std::prelude";

extern fn println_int(x: Int) -> Void;
extern fn println_long(x: Long) -> Void;
extern fn println_double(x: Double) -> Void;
extern fn println_bool(x: Bool) -> Void;
extern fn println_char(x: Char) -> Void;

const KInt: Int = 10;
const KLong: Long = 7L;

fn add(a: Int, b: Int) -> Int { a + b }

fn id<T>(x: T) -> T { x }
fn first<A, B>(a: A, _b: B) -> A { a }
fn choose<T>(b: Bool, x: T, y: T) -> T { if (b) { x } else { y } }

trait Eq<T> {
  fn eq(x: T, y: T) -> Bool;
}
impl Eq<Int>  { fn eq(x: Int,  y: Int)  -> Bool { x == y } }
impl Eq<Long> { fn eq(x: Long, y: Long) -> Bool { x == y } }

trait PairEq<A, B> {
  fn eq2(x: A, y: B) -> Bool;
}
impl PairEq<Int, Long> { fn eq2(x: Int, y: Long) -> Bool { x == y } }

fn needEq<T>(a: T, b: T) -> Int
where T: Eq<T>
{
  if (Eq::eq<T>(a, b)) { 1 } else { 0 }
}

fn needPair<T, U>(a: T, b: U) -> Int
where __Self: PairEq<T, U>
{
  if (PairEq::eq2<T, U>(a, b)) { return 1; }
  0
}

fn main() -> Int {
  let a: Int = id<Int>(41);
  let b: Long = id<Long>(KLong);
  let c: Int = first<Int, Long>(a, b);

  let d: Int = choose<Int>(true, c, 0);
  let e: Int = id<Int>(id<Int>(id<Int>(1)));

  let mut i: Int = 0;
  while (i < 3) { i = i + 1; }

  for (let j: Int = 0; j < 2; j = j + 1) { /* ... */ }

  let m1: Int = match (a) {
    40 => { 1 },
    41 => { 2 },
    _  => { 3 },
  };

  let ch: Char = 'A';
  let m2: Int = match (ch) { 'A' => { 10 }, _ => { 0 } };

  let flag: Bool = false;
  let m3: Int = match (flag) { true => { 1 }, false => { 0 } };

  let y: Long = first<Int, Long>(a, b); // or swap-like

  let _z1: Int = needEq<Int>(a, c);
  let _z2: Int = needEq<Long>(b, y);
  let _z3: Int = needPair<Int, Long>(a, b);

  let sum: Int = add(d, e) + m1 + m2 + m3 + i + KInt;
  println_int(sum);
  println_double(2.5);   // DOUBLE literal

  0
}
```

---

## Grammar cheat sheet (informal)

* **Program**: sequence of `item`
* **Item**:

    * `fn name<Ts?>(params) -> Ty where? { ... }`
    * `extern fn name(params) -> Ty;`
    * `let/const name: Ty = expr;`
    * `trait Name<Ts?> { fn ...; }`
    * `impl Name<Targs> { fn ... { ... } ... }`
    * `import "a::b::c";`
* **Types**: `Int | Long | Bool | Char | Float | Double | String | Void | T | Name<Ts?>`
* **Expr**: literals, variables, calls (`Name<types?>(args?)` or `Trait::method<types>(args)`),
  blocks `{...}`, `if (...) { ... } else { ... }`, `match (...) { ... }`, binary/unary ops
* **Statements**: `let/const`, assignment, `if`, `while`, `for (...) {}`, `break`, `continue`, `return`, expression `;`

---

## What’s missing / known limitations

* No user-defined structs/enums yet; `Ty::App` is parsed but not lowered to real data layouts.
* Inference at call sites is minimal; **explicit generic args** are required.
* `Float` literal suffix (`2.5f`) not supported—use `println_double` for now.
* Module privacy/visibility not implemented; everything is public.
* No packages/workspaces; modules are file-based only.

---

## Troubleshooting

* **“cannot resolve import …”**
  Ensure your path matches a file under the project root: `"a::b::c"` → `a/b/c.paw`.
* **“unknown impl method symbol … did you call declare\_impls\_from\_program()?”**
  You’re calling a qualified trait method without an impl for those concrete types, or the impl wasn’t visible via imports. Make sure the relevant `impl` file is imported.
* **Link error about `libpawrt.a`**
  Put the archive at `deps/<rust-triple>/libpawrt.a` (relative to the **compiler repo root**) or set `PAWRT_LIB` to its absolute path. Also ensure `zig` is installed/set via `ZIG_BIN`.
