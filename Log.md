# Paw 语言（PawLang）README

> 一个带静态类型、泛型与 trait 的小型语言与编译器。
> 目前编译后端使用 \[Cranelift] 生成目标文件，再由 \[Zig] 负责链接，内置一个用 Rust 写的运行时（打印、字符串与基础内存 API）。

---

## ✨ 当前能力概览

* **静态类型与数值提升**：`Int | Long | Float | Double | Bool | Char | String | Void`
* **函数与泛型**：支持类型形参 `fn foo<T>(x: T) -> T`
* **trait / impl / where**：可声明 trait、为具体类型实现，并在函数上施加 `where` 约束
* **import / prelude / 标准库雏形**：`import "std::fmt"` 引入打印相关 API
* **模式匹配**：`match`（当前支持对原生数值/布尔/字符）
* **控制流**：`if/else`, `while`, `for`, `break`, `continue`, `return`
* **全局常量内联**：常量全局会在后端被内联成字面量
* **FFI（到运行时）**：通过一组 `extern` C ABI 函数与运行时交互（打印/内存/字符串等）
* **单态化（Monomorphization）**：

  * 对**显式**泛型调用（如 `foo<Int>(...)`）在编译声明期收集并生成实例；
  * 对**隐式**泛型的**简单形态**（如 `println(x: T)` 这一元模板）在**codegen 阶段**按需推断并即时生成实例（详见下文）。
* **跨平台链接**：使用 Zig 链接器并自动打包运行时静态库

---

## 🚀 快速开始

### 1. 前置工具

* Rust 稳定版（用于构建编译器）
* Zig（用作链接器）
* （可选）本地 C 工具链，用于 Zig 在某些平台上的协作

### 2. 构建编译器

```bash
# 在仓库根目录
cargo build --release
# 或开发版
cargo build
```

编译器可执行一般叫 **`pawc`**（target 下的相应目录）。

### 3. 工程结构与构建

Paw 的项目根目录需要一个入口文件（默认 `main.paw`），以及可选的 `Paw.toml` 配置。构建命令：

```bash
# 在项目根目录执行
pawc build dev
# 或
pawc build release
```

输出位于：

```
./build/dev/     # 或 ./build/release/
  ├─ out.<obj>   # 编译生成的目标文件（平台相关扩展名）
  └─ <name>[.exe]# 可执行文件（名称来自 Paw.toml 的包名；否则为 app）
```

> `pawc` 会自动展开 `import`（以项目根为搜索根），并使用 Zig 链接运行时静态库。

---

## 🧪 一个最小示例

`main.paw`：

```paw
import "std::fmt";

fn main() -> Int {
  println(42);          // 隐式：推断为 println<Int>
  println("hello");     // 隐式：推断为 println<String>
  print(3.14); println('\n');  // 混合使用
  0
}
```

构建并运行：

```bash
pawc build dev
./build/dev/app     # 或你的包名
```

---

## 🖨️ 打印系统（现在的行为）

运行时（Rust）里保留了**具体类型**的外部函数：

```
print_int / println_int
print_long / println_long
print_bool / println_bool
print_char / println_char
print_float / println_float
print_double / println_double
print_str / println_str
```

而在 Paw 的 **prelude / std::fmt** 中，提供了**用户态分发**：

```paw
// 伪代码示意：泛型一元模板，用于分发到具体 extern 实现
fn print<T>(x: T) -> Void { /* 根据 T 调用对应的 print_* */ }
fn println<T>(x: T) -> Void { /* 根据 T 调用对应的 println_* */ }
```

### 隐式泛型推断（对 println/print）

* 只要函数模板是**单类型形参**且**唯一形参就是该形参**（典型：`fn println<T>(x: T) -> Void`），
* 调用形如 `println(expr)` 时，**后端 codegen 会从 `expr` 的静态类型快速推断** `T`，
* 然后**即时**生成并调用对应的实例（例如 `println$Int` / `println$String`），
* 无需在源码处写 `println<Int>(...)`。

> 对更复杂的泛型形态，请使用**显式**类型实参 `<...>`。

---

## 🧩 语言速查

### 基本类型

* `Int`(32 位), `Long`(64 位), `Float`(32 位), `Double`(64 位)
* `Bool`(8 位，语义真/假), `Char`(32 位 Unicode 标量值)
* `String`（运行时约定：指向以 `\0` 结尾的字节序列的指针）
* `Void`（无返回）

### 变量与常量

```paw
let x: Int = 1;
const PI: Double = 3.14159;
x = 2;        // 给 let 变量赋值
// PI = 3.14; // ❌ 常量不可赋值
```

### 表达式与控制流

```paw
if cond { ... } else { ... }

while cond { ... }

for (let i: Int = 0; i < 10; i = i + 1) { ... }

match x {
  0 => { ... }
  1 => { ... }
  _ => { ... }
}
```

### 函数与返回

```paw
fn add(a: Int, b: Int) -> Int {
  a + b
}

fn noop() -> Void { }
```

### 泛型、trait、impl、where

```paw
trait Show<T> {
  fn show(x: T) -> Void;
}

// 为具体类型实现
impl Show<Int> {
  fn show(x: Int) -> Void { println(x); }
}

fn dump<T>(x: T) -> Void
where T: Show   // 一元 trait 可省略参数，表示作用于 T
{
  Show::show<T>(x);     // 合格名调用，复杂调用需显式 <T>
}

// 使用
fn main() -> Int {
  dump<Int>(42);
  0
}
```

> `typecheck` 会严格校验 trait/impl 的方法集合与签名、`where` 约束是否可满足、以及调用处是否给出了充分的信息（或属于隐式可推断的简单模板）。

### 数值提升与比较

* 算术运算会对参与类型做**公共数值类型**提升（例如 `Int + Double -> Double`）。
* 比较与相等遵循数值/布尔/同类型比较的规则，其他类型需相等类型方可比较。

---

## 📦 工程与标准库

* `import "std::fmt"`：引入打印 API（`print/println`）
* `prelude`：工程默认会引入 prelude（含基础内建/别名/便捷函数）
* `Paw.toml`（可选）：提供包名等元信息（用于生成最终可执行名）

> import 展开由 `passes::expand_imports_with_loader` 完成，搜索根为**项目根目录**。

---

## 🏗️ 编译管线（实现细节）

* **语法/解析**：`grammar.pest` + `parser.rs` → `ast.rs`
* **Import 展开**：`passes.rs`
* **类型检查**：`typecheck.rs`

  * 构建 `TraitEnv/ImplEnv`、检查 `impl` 对应 `trait` 的**方法集合与签名一致性**
  * 函数体内做**赋值兼容规则**、控制流与返回类型检查、`where` 约束验证等
* **名称改写**：`mangle.rs`（函数/实例/impl 方法符号名）
* **代码生成**：`codegen.rs`（Cranelift IR → 目标文件）

  * **显式**泛型调用：在声明期收集并生成实例
  * **隐式**泛型（如 `println<T>(x:T)`）：**codegen 阶段**从实参类型**即时推断并单态化**
* **链接**：`link_zig.rs` 使用 Zig 把目标文件与运行时静态库链接为可执行
* **项目与 CLI**：`project.rs`, `src/main.rs`（命令：`pawc build <dev|release>`）

---

## 🧰 运行时（Rust 实现）

运行时以静态库形式链接，导出 C ABI：

* **打印**：`print_*` / `println_*`（各具体类型）
  在 Paw 层由 `print<T>/println<T>` 统一分发、并支持隐式推断。
* **内存**：`paw_malloc` / `paw_free` / `paw_realloc`
* **字符串**（UTF-8，`Vec<u8>` 为底）：
  `paw_string_new/from_cstr/push_cstr/push_char/as_cstr/...`
  约定导出到 `print_str` 之前会在末尾补 `\0`。
* **通用容器**：`paw_vec_u8_*`, `paw_vec_i64_*`
* **退出**：`paw_exit(code)`

> 注意：`String` 底层以指针表示；向 `println` 传 `String` 时，由 `std::fmt` 分发到 `println_str`（C 字符串）完成输出。

---

## ⚠️ 已知限制与后续方向

* **隐式泛型推断**目前只覆盖**一元模板且唯一参数为该类型形参**的场景（为 `print/println` 量身定制）。更复杂的泛型调用请写显式 `<...>`。
* **模式匹配**暂只支持原生字面量与 `_`，尚不支持代数数据类型。
* **类型系统**尚不包含用户结构体/枚举、自定义泛型类型构造等。
* **字符串**以 C 风格 `\0` 结尾字节序列为主，与运行时互操作时请确保正确性。
* **跨平台支持**依赖 Zig；某些平台上可能需要本地工具链辅助。

**路线图（建议）**：

* 统一的模块系统与包管理
* 用户自定义聚合类型（record/enum）与模式匹配扩展
* 更通用的类型推断（跨表达式流）
* 更丰富的标准库（集合/IO/文件/时间等）
* 更细粒度的错误报告与 IDE 友好性（位置、修复建议）

---

## 🙌 贡献

欢迎提交 Issue / PR！
典型改动点：`grammar.pest`（语法）、`typecheck.rs`（规则/诊断）、`codegen.rs`（后端/优化）、`std/`（标准库）。

---

如需帮助或想扩展某部分（比如把隐式泛型推断推广到多参数、多约束），可以直接给我一个目标用例，我会基于当前实现给出最稳的落地方案。
