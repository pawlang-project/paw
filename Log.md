# Paw — 现在阶段的说明书（WIP）

> 一个用 Rust + Pest + Cranelift 实现的小型静态语言编译器与运行时。
> 本文档描述**当前阶段**已经实现的语法、类型系统、代码生成与使用方法，并给出可运行的示例。

---

## 1. 快速上手

### 环境

* Rust stable（建议最新版）
* 一个基础 C/Rust 工具链（用于链接生成的目标文件）
* 本仓库包含：

    * 词法/语法：`grammar/grammar.pest`
    * AST/Parser/Typecheck/Codegen：`src/`
    * 运行时（FFI）：`libpawrt`（Rust，`extern "C"` 导出）

### 构建与运行（范例）

```bash
# 构建
cargo build

# 运行编译器（示例命令，按你的 CLI 入口调整）
cargo run -- examples/main.paw
```

> 编译器会完成：解析 → 类型检查 → 用 Cranelift 生成目标文件/可执行文件，并与内置 `libpawrt` 链接。
> 具体输出路径/命令行参数以你的 `main.rs/cli` 实现为准（通常 `cargo run -- path/to/main.paw` 即可）。

---

## 2. 语言概览

### 基本类型（及 ABI 映射）

| 语言类型     | 语义/位宽           | ABI 映射（Cranelift Type）          |
| -------- | --------------- | ------------------------------- |
| `Int`    | 32 位有符号整数       | `i32`                           |
| `Long`   | 64 位有符号整数       | `i64`                           |
| `Bool`   | 逻辑布尔            | **表达式值/参数**: `i8`；条件判断内部使用 `b1` |
| `Char`   | Unicode 标量（u32） | `i32`                           |
| `Float`  | 32 位浮点          | `f32`                           |
| `Double` | 64 位浮点          | `f64`                           |
| `String` | 运行时句柄/指针        | `i64`（指针）                       |
| `Void`   | 无返回值            | —（无返回寄存器）                       |

> 注：`Bool` 在表达式里统一用 `i8(0/1)` 表示；在条件分支处与 Cranelift 的 `b1` 互转。

### 字面量

* `123`（Int），`3000000000`（Long）
* `true/false`（Bool）
* `3.14`、`-0.25`（Double；也支持 `Float` 常量）
* `'\n'`、`'\u{1F600}'`（Char）
* `"Hello, Paw!"`（String）

### 表达式与运算符（已实现）

* 一元：`-`（数值取负）、`!`（逻辑非）
* 二元：

    * 算术：`+ - * /`
    * 比较：`< <= > >= == !=`（结果为 `Bool`/`i8`）
    * 逻辑：`&& ||`（**已实现短路**）
* 调用：`foo(a, b, ...)`
* 分组：`(expr)`
* 代码块表达式：`{ ... tail_expr? }`（块有值）

### 语句与控制流

* 变量/常量：`let/const name: Ty = expr;`
* 赋值：`name = expr;`
* `if (cond) { ... } else { ... }`（语句版和表达式版均支持）
* `while (cond) { ... }`
* `for (init?; cond?; step?) { ... }`

    * `init` 可为 `let/const`、赋值、或任意表达式
    * `step` 可为赋值或表达式
    * `break` / `continue`
* `match (expr) { pattern => block, ..., _ => block }`

    * 目前支持 `Int/Long/Bool/Char` 与 `_` 通配
* `return expr?;`

### 名称与作用域

* 词法块作用域，禁止同一作用域内重名
* 支持局部变量、函数参数、全局（全局常量在编译期可内联为寄存器值）

---

## 3. 类型规则（要点）

### 混合类型算术（隐式提升）

* 整数：`Int (+-*/...) Long` → **统一为 `Long`**
* 浮点：`Float (+-*/...) Double` → **统一为 `Double`**
* 整数与浮点混合暂按**显式**为佳（如需要可在 typecheck/IR 层扩展）

### 比较/逻辑

* 比较两侧需类型可比较（数值间自动对齐位宽/精度；`Char` 与整型比较按整型处理）
* `&&` / `||` 是**短路**的（在 Codegen 里以分支实现，避免副作用提前求值）

### 条件/返回

* `if/while/for` 条件从 `i8` 转 `b1`；比较/逻辑结果最终回落为 `i8`
* 函数返回若非 `Void` 而缺省，当前实现**宽松补零**（与早期行为保持一致）

---

## 4. 标准库与运行时（FFI）

通过 `import "std/prelude.paw"` 引入外部函数声明，对应 `libpawrt` 里的 `extern "C"` 实现。

### I/O

```paw
extern fn print_int(x: Int) -> Void
extern fn println_int(x: Int) -> Void
extern fn print_long(x: Long) -> Void
extern fn println_long(x: Long) -> Void
extern fn print_bool(x: Bool) -> Void
extern fn println_bool(x: Bool) -> Void
extern fn print_char(x: Char) -> Void
extern fn println_char(x: Char) -> Void
extern fn print_float(x: Float) -> Void
extern fn println_float(x: Float) -> Void
extern fn print_double(x: Double) -> Void
extern fn println_double(x: Double) -> Void
extern fn print_str(p: String) -> Void
extern fn println_str(p: String) -> Void
extern fn paw_exit(code: Int) -> Void
```

### 内存与字符串（句柄均用 `u64/i64`）

```paw
extern fn paw_malloc(size: Long) -> Long
extern fn paw_free(ptr: Long, cap: Long) -> Long
extern fn paw_realloc(ptr: Long, old_cap: Long, new_cap: Long) -> Long

extern fn paw_string_new() -> String
extern fn paw_string_from_cstr(cptr: Long) -> String
extern fn paw_string_push_cstr(handle: String, cptr: Long) -> Int
extern fn paw_string_push_char(handle: String, ch: Char) -> Int
extern fn paw_string_as_cstr(handle: String) -> Long
extern fn paw_string_len(handle: String) -> Long
extern fn paw_string_clear(handle: String) -> Void
extern fn paw_string_free(handle: String) -> Void
```

### 工具 Vec（示例）

```paw
extern fn paw_vec_u8_new() -> String
extern fn paw_vec_u8_push(handle: String, b: Int) -> Long
extern fn paw_vec_u8_len(handle: String) -> Long
extern fn paw_vec_u8_data_ptr(handle: String) -> Long
extern fn paw_vec_u8_free(handle: String) -> Void

extern fn paw_vec_i64_new() -> String
extern fn paw_vec_i64_push(handle: String, v: Long) -> Long
extern fn paw_vec_i64_pop(handle: String, out_ptr: Long) -> Bool
extern fn paw_vec_i64_len(handle: String) -> Long
extern fn paw_vec_i64_get(handle: String, idx: Long, out_ptr: Long) -> Bool
extern fn paw_vec_i64_free(handle: String) -> Void
```

---

## 5. 示例程序（可直接运行）

```paw
import "std/prelude.paw";

// 短路逻辑演示
fn explode() -> Bool {
  println_str("should NOT print");
  true
}

fn test_short(b: Bool) -> Bool {
  // b 为 false 时，不会调用 explode()
  return b && explode();
}

// 一些示例函数
fn sum_to(n: Int) -> Int {
  let s: Int = 0;
  for (let i: Int = 1; i <= n; i = i + 1) {
    s = s + i;
  }
  return s;
}

fn first_i_sq_ge(th: Int) -> Int {
  let i: Int = 0;
  for (i = 0; i < 100; i = i + 1) {
    if (i * i >= th) { break; }
  }
  return i;
}

fn sum_skip_five() -> Int {
  let s: Int = 0;
  for (let i: Int = 0; i < 10; i = i + 1) {
    if (i == 5) { continue; }
    s = s + i;
  }
  return s;
}

fn fact_while(n: Int) -> Int {
  let i: Int = 1;
  let acc: Int = 1;
  while (i <= n) {
    acc = acc * i;
    i = i + 1;
  }
  return acc;
}

fn if_expr_demo(b: Bool) -> Int {
  let x: Int = if (b) { 123 } else { 456 };
  return x;
}

fn match_int(x: Int) -> Int {
  return match (x) {
    0 => { 100 },
    1 => { 200 },
    _ => { 999 },
  };
}

fn match_bool(b: Bool) -> Int {
  return match (b) { true => { 1 }, false => { 0 } };
}

fn match_long_one() -> Int {
  let big: Long = 3000000000;
  return match (big) { 3000000000 => { 1 }, _ => { 0 } };
}

fn long_add_demo(a: Long, b: Long) -> Long { a + b }

fn double_demo() -> Double { -125.0 + 0.5 }

fn char_demo() -> Int {
  println_char('A');
  println_char('\n');
  println_char('\u{263A}'); // ☺
  return 0;
}

fn string_demo() -> Int {
  println_str("Hello, Paw!");
  return 0;
}

fn main() -> Void {
  println_str("== short/logic ==");
  let _r0: Bool = test_short(false); // 不打印 "should NOT print"
  let _r1: Bool = test_short(true);  // 打印一次

  println_str("== basic/print ==");
  println_int(42);
  println_bool(true);
  println_double(3.14159);
  println_str("done");

  println_str("== for/sum_to ==");
  println_int(sum_to(9));

  println_str("== for/break ==");
  println_int(first_i_sq_ge(30));

  println_str("== for/continue ==");
  println_int(sum_skip_five());

  println_str("== while/fact ==");
  println_int(fact_while(5));

  println_str("== if/expr ==");
  println_int(if_expr_demo(true));
  println_int(if_expr_demo(false));

  println_str("== match/int ==");
  println_int(match_int(0));
  println_int(match_int(1));
  println_int(match_int(42));

  println_str("== match/bool ==");
  println_int(match_bool(true));
  println_int(match_bool(false));

  println_str("== long/op ==");
  let la: Long = 3000000000;
  let lb: Long = 2;
  let lc: Long = long_add_demo(la, lb);
  println_long(lc);
  println_int(match_long_one());

  println_str("== double/op ==");
  println_double(double_demo());

  println_str("== char ==");
  let _c0: Int = char_demo();

  println_str("== string ==");
  let _s0: Int = string_demo();

  println_str("== float/edge ==");
  println_double(-0.25);
  println_double(1200.0);
  println_double(-0.034);

  println_str("== cmp/chain ==");
  if ((1 <= 2) && (3 >= 2)) { println_str("ok"); }

  println_str("== char/escape ==");
  println_char('\t');
  println_char('\u{1F600}'); // 😀
}
```

---

## 6. 编译管线（实现细节）

1. **解析**：Pest (`grammar/grammar.pest`) 定义词法/语法 → 产生 parse tree
2. **AST**：将 parse tree 转换为简洁的 AST（`ast.rs`），涵盖 `Program/Item/FunDecl/Stmt/Expr/Pattern/Ty`
3. **类型检查**（`typecheck.rs`）：

    * 符号表：函数签名、全局、局部作用域、常量属性
    * 表达式推断与运算符检查：算术/比较/逻辑
    * 分支/合流的类型一致性（`if` 表达式、`match`）
    * 混合数值运算的**统一原则**（位宽/精度对齐）
4. **代码生成**（`codegen.rs`）：

    * `Bool i8` ↔ `b1` 转换：条件用 `b1`，结果回落为 `i8`
    * 算术/比较：根据 IR 类型选择 `iadd/isub/...` 或 `fadd/fsub/...`，`icmp/fcmp`
    * **短路逻辑**：`&&/||` 以基本块形式生成（避免副作用提前求值）
    * `if`/`match`：构造 then/else/merge 基本块，统一值类型
    * 运行时字符串驻留（以 `.data` 段保存，结尾 `\0`）

---

## 7. 当前限制与计划

**限制**

* 暂无用户自定义聚合类型（struct/数组/切片）
* 暂无泛型/重载/接口
* 类型推断较少（大多需要显式类型标注）
* `String`/`Vec` 为运行时句柄，缺少 GC；需要使用者谨慎管理生命周期
* `match` 目前不支持浮点模式

**计划**

* 数组与切片、结构体
* 更丰富的标准库与字符串 API
* 更完善的常量折叠与优化
* 错误信息与源位置报错优化
* 扩展 `import`/模块系统

---

## 8. 贡献与调试

* 将你的 `.paw` 文件放入 `examples/`，用 `cargo run -- examples/foo.paw` 试跑
* 遇到 “unknown function/variable/type” 之类错误，优先检查：

    * 是否 `import "std/prelude.paw"`
    * 是否在同一编译单元内定义了被调用的函数
    * 变量/常量是否已在可见作用域内声明
* 如需观测 IR，可在 Codegen 中打印 `ctx.func.display()`（按你项目的调试开关添加）
