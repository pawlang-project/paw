# PawLang

PawLang 是一个用 **Rust** 开发的实验性静态类型语言，最初通过 **Cranelift** 后端生成机器码，支持跨平台链接（使用 **Zig cc** 统一构建）。本项目目标是逐步实现一个能 **自举** 的编译器。

---

## 阶段目标

### 🔹 M0 — 基础设施

* ✅ 选择 **Rust** 作为实现语言
* ✅ 使用 **pest** 实现语法解析
* ✅ 定义抽象语法树（AST）
* ✅ 实现最小类型系统（Int / Bool）
* ✅ 错误处理框架（anyhow）

### 🔹 M1 — 足以写编译器的最小能力

> **目标**：语言具备“能实现编译器本身”的核心表达能力

* ✅ 全局变量（`const` / `let`）
* ✅ 函数定义与调用（`fn`）
* ✅ 基础类型：`Int`, `Bool`, `String`（字符串字面量先支持最简单形式）
* ✅ 表达式运算：算术、比较、逻辑
* ✅ 控制流：`if-else`, `while`
* ✅ 返回语句：`return`
* ✅ 语法块（block）与块尾表达式

### 🔹 M2 — 语言运行时支持

> **目标**：能写更复杂的程序

* ⬜ 内置运行时函数（打印、退出等）
* ⬜ 字符串常量和运行时管理
* ⬜ Zig cc 跨平台链接（Windows/macOS/Linux 一套流程）

### 🔹 M3 — 开始自举

> **目标**：用 PawLang 本身写编译器的部分逻辑

* ⬜ 用 PawLang 实现基础数据结构（字符串处理、符号表雏形）
* ⬜ 实现最简解释器或 IR 打印器
* ⬜ 通过 Zig cc 链接 runtime + PawLang 生成的对象文件，跑出第一个“自编译”产物

### 🔹 M4 — 完整前端

> **目标**：支持 PawLang 自举

* ⬜ 扩展类型系统（数组、结构体、指针/引用）
* ⬜ 文件 I/O、字符串拼接
* ⬜ 模块/包系统

### 🔹 M5 — 优化与生态

> **目标**：让 PawLang 能用于中小规模项目

* ⬜ 优化器（常量折叠、死代码消除）
* ⬜ 标准库（字符串、容器、文件、网络）
* ⬜ 更丰富的错误提示（源码位置、hint）

---

## 构建与运行

### 依赖

* Rust (>= 1.77)
* Zig (>= 0.13) — 用作跨平台链接器
* Cranelift crates

### 构建

```bash
cargo build
```

### 运行示例

```bash
cargo run -- examples/hello.paw build hello
```

### 跨平台编译

使用 Zig cc 内置工具链：

```bash
# Linux 上编译 Windows 可执行
PAW_TARGET=x86_64-windows-gnu cargo run --release -- examples/hello.paw build hello
```

---

## 示例

```paw
const N: Int = 10;

fn add(a: Int, b: Int) -> Int {
    a + b
}

fn main() -> Int {
    let x: Int = 32;
    let y: Int = add(x, N);
    if (y > 40) {
        0
    } else {
        1
    }
}
```

---

## 未来计划

* 自举：用 PawLang 实现 PawLang 编译器
* 支持更多后端（LLVM IR / WASM）
* 丰富标准库与生态工具
