# PawLang (WIP) — README

> 一个用 Rust + Cranelift 实现的玩具/教学语言。当前版本已支持：基本类型与表达式、语句块与控制流、函数与泛型（单态化）、`trait/impl` 与 `where` 约束、限定调用 `Trait::method<...>(...)`、`import` 简单合并、以及通过 `zig cc` 链接成可执行文件。

---

## 快速上手

```bash
# 1) 构建编译器
cargo build

# 2) 编译 Paw 源码为目标文件（对象文件）
#    默认输出 a.o，可用 -o 指定
target/debug/PawLang path/to/main.paw -o build/out.o

# 3) 链接为可执行（需要 zig 与运行时静态库 libpawrt.a）
#    推荐用自带的 link_zig（见下文“一键链接”）
```

### 一键链接（使用 `link_zig`）

* 需要安装 **zig**（或设置 `ZIG` / `ZIG_BIN` 环境变量指向可执行文件）。
* 需要可用的 **运行时静态库** `libpawrt.a`：

    * 缺省在 `deps/<rust_triple>/libpawrt.a`
    * 或设置 `PAWRT_LIB=/absolute/path/to/libpawrt.a`。

可选环境变量：

* `PAW_TARGET`：`x86_64-unknown-linux-gnu`（默认）/ `x86_64-pc-windows-gnu` / `x86_64-apple-darwin` / `aarch64-apple-darwin`
* `PAW_VERBOSE=1`：打印链接命令

> 你也可以直接调用 `link_zig::link_many_with_zig(&[obj], exe, target)` 从你的工具程序里进行链接。

---

## 语言总览

### 基本类型

* `Int`（i32）
* `Long`（i64）
* `Bool`（i8，语义为 0/1）
* `Char`（u32，ABI 以 i32 传递）
* `Float`（f32）
* `Double`（f64）
* `String`（运行时句柄，ABI 以 i64 传递）
* `Void`（仅作返回类型）

### 字面量

* 整数：`0`、`-42` 等（自动落在 `Int`；超出 i32 会落在 `Long`）
* 长整：`7L`、`-9l`  → `Long`
* 浮点：

    * `2.5` → `Double`
    * `2.5f` / `2.5F` → `Float`
* 字符：`'A'`、`'\n'`、`'\u{4E2D}'` → `Char`
* 布尔：`true` / `false` → `Bool`
* 字符串：`"hello\nworld"` → `String`

### 运算符（从低到高）

* 逻辑：`||`、`&&`（短路）
* 比较：`== != <= >= < >`
* 算数：`+ - * /`
* 一元：`!`（逻辑非）、`-`（数值负号）

> 数值二元运算会在 **整型** 或 **浮点** 内部自洽：
>
> * 整型同宽或按更宽位宽符号扩展；
> * 浮点在 `Float`/`Double` 之间向 `Double` 提升；
> * 不同域（整/浮）不自动互转。

### 变量与作用域

```paw
let x: Int = 1;
const K: Int = 10;   // 顶层 const 可被后端“内联常量”优化
x = x + 1;
```

### 控制流

```paw
// if 语句
if (x < 3) {
    println_int(x);
} else {
    println_int(0);
}

// while
while (x < 5) {
    x = x + 1;
}

// for(init; cond; step) { ... }
for (let i: Int = 0; i < 3; i = i + 1) {
    println_int(i);
}

// if 表达式（必须有 else 分支才有值）
let y: Int = if (x < 0) { 1 } else { 2 };

// match 表达式（当前支持 Int/Long/Bool/Char/通配）
let z: Int = match (x) {
    0   => { 10 },
    1   => { 20 },
    _   => { 30 },
};
```

### 函数与泛型

```paw
// 普通函数
fn add(a: Int, b: Int) -> Int {
    return a + b;
}

// 泛型函数
fn id<T>(x: T) -> T {
    return x;
}

// 多类型形参
fn first<A, B>(a: A, _b: B) -> A { return a; }

// 调用时显式类型实参写在“函数名后、括号前”
let a: Int  = id<Int>(41);
let b: Long = id<Long>(7L);
let c: Int  = first<Int, Long>(a, b);
```

> **注意**：当前不会做类型参数推断；**需要显式写出** `<...>`。

### trait / impl / where

```paw
trait Eq<T> {
    fn eq(x: T, y: T) -> Bool;
}

impl Eq<Int> {
    fn eq(x: Int, y: Int) -> Bool { x == y }
}

impl Eq<Long> {
    fn eq(x: Long, y: Long) -> Bool { x == y }
}

// 需要约束的泛型函数：T: Eq<T>
fn needEq<T>(a: T, b: T) -> Int where T: Eq<T> {
    return if (Eq::eq<T>(a, b)) { 1 } else { 0 };
}

// 双参 trait 示例
trait PairEq<A, B> {
    fn eq2(x: A, y: B) -> Bool;
}

impl PairEq<Int, Long> {
    fn eq2(x: Int, y: Long) -> Bool { x == y }
}

// 关于 PairEq<A,B> 的 where 写法：
// 目前用占位类型 __Self 表示“谓词约束自身”
//    where __Self: PairEq<T, U>
fn needPair<T, U>(a: T, b: U) -> Int where __Self: PairEq<T, U> {
    if (PairEq::eq2<T, U>(a, b)) {
        return 1;
    }
    return 0;
}
```

**限定调用（Qualified Call）**

* 语法：`Trait::method<...>(args...)`
* 必须带显式类型实参 `<...>`；否则会报错。
* `Trait::method` **不能**当作值使用（必须“调用”它）。

### import

```paw
import "std/prelude.paw";
import "traits.paw";
```

> 目前 `passes::expand_imports` 会把 import 指向的文件 **内联合并**到同一个 `Program`；
> 解析路径采用 `std::fs::canonicalize`，确保文件存在。

---

## 外部函数与运行时（ABI）

当前内建了一批典型外部函数（用于打印、字符串、向量等），示例：

```paw
extern fn println_int(x: Int) -> Void;
extern fn println_long(x: Long) -> Void;
extern fn println_bool(x: Bool) -> Void;
extern fn println_char(x: Char) -> Void;
extern fn println_float(x: Float) -> Void;
extern fn println_double(x: Double) -> Void;
extern fn println_str(p: String) -> Void;

// 还有若干 paw_string_* / paw_vec_* / paw_malloc/free/realloc 等
```

> 这些符号由运行时静态库 `libpawrt.a` 提供。
> `String` 在 ABI 中为 i64 句柄；打印/字符串操作会经由运行时实现。

---

## 编译与链接流程

### 1) 解析 → 类型检查 → 声明符号 → 生成 CLIF → 对象文件

编译器入口（`main.rs`）大致流程：

1. 读取源文件文本（**注意不是文件名字符串**）
2. `parser::parse_program(...)` 解析
3. `passes::expand_imports(...)` 合并 import
4. `typecheck_program(...)` 类型检查（含 where/trait/impl 检查）
5. 后端：

    * 收集顶层 `const` 作为可内联全局常量
    * `declare_fns(...)` 声明非泛型/extern
    * `declare_impls_from_program(...)` 扫描并声明所有 `impl` 方法（降级为自由函数，形如 `__impl_Trait$Args__method`）
    * `declare_mono_from_program(...)` 扫描**显式**泛型调用并声明各专门化实例（如 `id$Int`）
    * `define_fn(...)` / `define_impls_from_program(...)` / `define_mono_from_program(...)` 依次生成与定义函数体
6. `finish()` 输出对象文件字节

### 2) 链接为可执行（`zig cc`）

* 使用 `link_zig` 模块，自动选择系统库、平台选项与运行时静态库。
* Windows（gnu）会自动加上一些常用系统库（可按需调整）。

---

## 浮点字面量规则（重要）

* 无后缀：`2.5` 解析为 `Double`（f64）
* 后缀 `f/F`：`2.5f` / `2.5F` 解析为 `Float`（f32）

因此：

```paw
println_float(2.5f);     // OK
println_double(2.5);     // OK
println_float(2.5);      // ❌ 类型不匹配：Double 传给 Float
```

---

## 示例：覆盖主要语法与类型

```paw
import "traits.paw";          // 可选：放你的 Eq/PairEq 定义
import "std/prelude.paw";     // 可选：放运行时声明（extern）

const KInt: Int = 10;
const KLong: Long = 7L;

extern fn println_int(x: Int) -> Void;
extern fn println_double(x: Double) -> Void;
extern fn println_float(x: Float) -> Void;
extern fn println_bool(x: Bool) -> Void;
extern fn println_char(x: Char) -> Void;

fn add(a: Int, b: Int) -> Int { return a + b; }

fn id<T>(x: T) -> T { return x; }
fn first<A, B>(a: A, _b: B) -> A { return a; }

trait Eq<T> { fn eq(x: T, y: T) -> Bool; }
impl Eq<Int>  { fn eq(x: Int,  y: Int)  -> Bool { x == y } }
impl Eq<Long> { fn eq(x: Long, y: Long) -> Bool { x == y } }

fn needEq<T>(a: T, b: T) -> Int where T: Eq<T> {
  return if (Eq::eq<T>(a, b)) { 1 } else { 0 };
}

trait PairEq<A, B> { fn eq2(x: A, y: B) -> Bool; }
impl PairEq<Int, Long> { fn eq2(x: Int, y: Long) -> Bool { x == y } }

fn needPair<T, U>(a: T, b: U) -> Int where __Self: PairEq<T, U> {
  if (PairEq::eq2<T, U>(a, b)) { return 1; }
  return 0;
}

fn main() -> Int {
  let a: Int  = id<Int>(41);
  let b: Long = id<Long>(KLong);
  let c: Int  = first<Int, Long>(a, b);
  let d: Int  = if (true) { c } else { 0 };

  let i: Int = 0;
  while (i < 2) {
    println_int(i);
    // i = i + 1;  // 示例里如果要改 i，记得声明为可变并赋值
    break;         // 控制流示范
  }

  for (let j: Int = 0; j < 2; j = j + 1) { }

  let ch: Char = 'A';
  let flag: Bool = false;

  let m1: Int = match (a) {
    40 => { 1 },
    41 => { 2 },
    _  => { 3 },
  };

  println_float(2.5f);
  println_double(2.5);
  println_char(ch);
  println_bool(flag);

  let _z1: Int = needEq<Int>(a, c);
  let _z2: Int = needEq<Long>(b, b);
  let _z3: Int = needPair<Int, Long>(a, b);

  let sum: Int = add(d, m1) + KInt;
  println_int(sum);
  return sum;
}
```

---

## 错误提示与常见坑

* **“unknown impl method symbol `__impl_...`”**
  没有先调用 `declare_impls_from_program(&program)`，或你的 `passes` 没有把 `impl` 方法降解为自由函数并参与声明。确保在 codegen 前调用：

    * `be.declare_impls_from_program(prog)?;`
    * 或者在 `passes` 中把 `impl` 展平成自由函数并让 `declare_fns` 处理。

* **“free type variable not allowed here: `T`”**
  使用了泛型但没有显式类型实参进行调用/单态化。写成 `id<Int>(...)` 这种形式。

* **浮点字面量类型不匹配**
  `println_float(2.5)` 报 `Double` → `Float`。改成 `println_float(2.5f)` 或换成 `println_double(2.5)`。

* **解析 import 失败**
  `expand_imports` 会 `canonicalize` 目标路径；请确保被导入文件真实存在（相对于工程根或给定工作目录的相对路径）。

---

## 构建/运行时依赖

* Rust stable（构建编译器）
* Cranelift（已在 `Cargo.toml` 中声明）
* zig（用于链接；也可以自行使用系统链接器）
* 运行时静态库 `libpawrt.a`（打印与字符串、向量、内存等）

常用环境变量：

* `ZIG` / `ZIG_BIN`：zig 可执行文件路径
* `PAW_TARGET`：目标三元组（见上文）
* `PAWRT_LIB`：`libpawrt.a` 的绝对路径
* `PAW_VERBOSE=1`：打印链接命令

---

## 当前实现要点（给贡献者/读者）

* **单态化**：仅对**显式**类型实参的泛型调用进行扫描与实例化（命名形如 `id$Int`）。
* **trait/impl**：`impl` 方法在后端按符号规则降级为自由函数 `__impl_{Trait}${Args}__{method}`；限定调用解析时会直接调用该符号。
* **类型与 ABI**：

    * `Int(i32)` / `Long(i64)` / `Bool(i8)` / `Char(i32)` / `Float(f32)` / `Double(f64)` / `String(i64 handle)` / `Void`
    * `Bool` 在 IR 中用 `i8` 承载；比较/逻辑在 b1 与 i8 间转换。
* **import**：目前是**文本级合并**到一个 `Program`，非模块系统。
* **代码生成**：Cranelift IR，简单的数值统一策略，副作用调用（如 `println_*`）即使返回 `Void` 也会真实发起。

---

## 目录建议

```
PawLang/
  src/
    ast.rs
    parser.rs
    typecheck.rs
    codegen.rs
    passes.rs
    mangle.rs
    link_zig.rs
    main.rs
  grammar/
    grammar.pest
  deps/
    x86_64-unknown-linux-gnu/
      libpawrt.a
    x86_64-apple-darwin/
      libpawrt.a
    aarch64-apple-darwin/
      libpawrt.a
  paw/
    main.paw
```
