# Paw

一个用 **Cranelift** 做后端的小型教学/实验语言编译器。
目前已支持：**一等整数/浮点/布尔/字符/字符串、`Byte`（真 `u8`）、控制流（`if/while/for/match`）、函数/重载、泛型（单态化）、`trait/impl`、简单标准库 `std::fmt::println`**，并生成本机目标的 **object file**。

---

## 快速开始

### 依赖

* Rust（建议 stable，`cargo` 可用）
* 本机目标工具链（用于链接生成的对象文件，如果你要把 `.o` 链成可执行程序）

### 构建

```bash
git clone <your-repo-url> paw
cd paw
cargo build
```

### 运行一个示例

项目包含若干 `.paw` 示例/测试。假设驱动程序接收源文件路径：

```bash
cargo run -- examples/hello.paw
```

### 运行测试

```bash
cargo test
```

> 关于如何把生成的 `.o` 链成可执行：取决于你的驱动实现。一般是把 `compile_program(...)` 返回的字节写成 `*.o`，再用系统链接器链接到运行时（`runtime`）产生的静态/动态库。

---

## 目录结构（概览）

```
src/
  frontend/
    ast.rs              # 语法树与语言节点
  backend/
    codegen.rs          # Cranelift IR 生成与单态化调度
    mangle.rs           # 函数/impl 方法/泛型实例的符号整形
  typecheck.rs          # 类型检查、重载解析、where 约束校验
  runtime/
    fmt_runtime.rs      # 与 println 等相关的最小运行时（extern "C"）
  utils/
    fast.rs             # 轻量数据结构：FastMap/FastSet/SmallVec4
  diag/
    mod.rs              # 诊断汇集（Ariadne sink）
main.rs                 # 入口（可能根据你的仓库而定）
```

---

## 语言速览

### 基本类型

* `Void`（仅用于函数返回）
* `Bool`（内部用 `u8` 表示，0/非0）
* `Byte`（**真 `u8`**）
* `Char`（Unicode 标量，语义上对应 `u32`）
* `Int`（i32）
* `Long`（i64）
* `Float`（f32）
* `Double`（f64）
* `String`（运行时以 C 字符串指针传递，打印时视为 UTF-8）

### 字面量

```
0, 1, 123                 # Int
0L, 123L                  # Long（如果语法支持后缀）
true, false               # Bool
'a', '\n'                 # Char
"hello"                   # String
1.0f, 2.3f                # Float（如果语法支持）
1.0, 2.3                  # Double
```

### 变量与常量

```paw
let x: Int = 42;
let y: Byte = 255;
let z: Double = 3.14;
```

> `let ... is_const: true` 的形式在中间表示里存在；源语言层面与 `const` 等价。

### 表达式与控制流

* 算术：`+ - * /`
* 比较：`< <= > >= == !=`
* 逻辑：`&& || !`
* 块/作用域：`{ ... }`（块表达式有值；无尾表达式时默认类型 `Int`）
* `if .. else`、`while`、`for(init; cond; step)`、`match`

### 函数 / 泛型 / 重载

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

* **泛型函数**采用**单态化**（monomorphization）：`foo<Int>` 会生成一个专门化符号。
* **重载解析**在类型检查阶段完成：按参数个数筛选候选 → 用约束/统一与可赋值规则打分 → 选择唯一最佳项。
* **trait/impl**：

  * `impl Trait<Args...>` 会在 **类型检查**时校验 *arity*、方法集合相等性、签名与返回类型是否与 `trait` 匹配（经形参替换后）。
  * **合格名调用** `Trait::<T...>::method(...)`：

    * 当 `T...` **全为具体类型**：要求存在相应 `impl`（在 `ImplEnv` 中登记），再调用对应被降解后的自由函数符号（通过 `mangle_impl_method`）。
    * 当 `T...` **含有类型形参**：必须在当前函数的 `where` 约束中显式允许该调用。

### 标准库（fmt）

导入：

```paw
import "std::fmt";
```

打印：

```paw
println(123);            // Int
println(3.14);           // Double
println(true);           // Bool -> "true"/"false"
println('A');            // Char (UTF-8)
println("hello");        // String (UTF-8)
println<Byte>(255);      // 显式以 Byte 语境打印
```

`println` 的底层由运行时 `extern "C"` 导出的一组 `print_*` + `rt_println` 组成，类型检查会把不同类型求值后路由到相应符号（或在泛型情况下进行单态化）。

---

## `Byte`（u8）语义

* **类型**：`Byte` 是**真 `u8`**，范围 0..=255。
* **算术时的整型提升**：参与 `+ - * /` 等**二元数值运算**时，`Byte` 会**提升**到至少 `Int`（i32）再计算，以避免无意间的“隐式取模”。
* **赋值/打印处的截断**：当结果被**赋值到 `Byte` 变量**或**以 `println<Byte>(...)` 打印**时，会在**代码生成**阶段以 `ireduce`/零扩展的方式截断到 8 位（等价 `mod 256`）。

示例：

```paw
import "std::fmt";

fn main() -> Int {
    let x: Byte = 0;

    println(x - 1);        // 提升到 Int 运算 → 打印 -1
    println<Byte>(x - 1);  // 显式以 Byte 打印 → 255

    let y: Byte = x - 1;   // 赋值到 Byte 截断 → y = 255
    println(y);            // 打印 255

    0
}
```

---

## 典型输出（Byte 测试）

```
== Byte basics ==
0
5
10
== Byte arithmetic ==
8
3
70
1
== Byte wrap (mod 256) ==
4
== Byte + Int -> Int ==
107
== for with Byte counter ==
45
```

---

## 诊断与错误码（节选）

**类型检查（`typecheck.rs`）**

* `E2101`：重复的局部变量名
* `E2102`：未知变量
* `E2103`：对常量赋值
* `E2106`：函数外的 `return`
* `E2200`：一元 `-` 需要数值类型
* `E2210`：`if` 分支类型不一致
* `E2311`：函数调用没有可匹配的重载
* `E2303`：合格名调用缺少显式类型实参
* `E2308`：合格名调用不被当前 `where` 约束允许
* ……

**代码生成（`backend/codegen.rs`）**

* `CG0001`：缺失符号/函数 ID
* `CG0002`：ABI 不可用的类型
* `CG0003`：impl 方法符号未知（未声明）
* `CG0004`：泛型模板/单态化问题
* ……

当同时提供 `DiagSink` 时，错误会带文件名/位置（若可用）。

---

## 实现细节（简述）

* **Cranelift IR**：

  * 各语言类型映射：

    * `Byte`/`Bool` → `i8`
    * `Int` → `i32`，`Long` → `i64`
    * `Float` → `f32`，`Double` → `f64`
    * `Char` → `i32`（UTF-32 标量）
    * `String` → `i64`（以指针传递到运行时）
  * **布尔**在 IR 中使用 `b1` 进行条件判断，进/出位置做 `i8` 与 `b1` 的显式互转。
  * **整数放大**：统一用 **零扩展**（`uextend`）以保持 `Byte` 的无符号语义。
  * **整型统一规则**：二元整数运算时取更宽者；若两边都是 `i8`，先提升到 `i32` 再算（与类型规则一致）。

* **单态化**：

  * 预扫描显式 `<...>` 调用并声明所有实例（`declare_mono_from_program`）。
  * 运行到\*\*隐式泛型（常见形态：`println<T>(x:T)`）\*\*时现场推断并 `ensure_monomorph`。
  * 符号整形：`mangle_name` / `mangle_impl_method`。

* **运行时**（`runtime/fmt_runtime.rs`）：

  * 提供 `extern "C"` 的 `print_i32/i64/f32/f64/bool/char/str`、`rt_println`、`flush_*` 等。
  * 布尔打印采用 `u8` 约定（0 → `false`，非 0 → `true`）。
  * 使用 `BufWriter` + `once_cell::sync::Lazy` 做缓冲输出。

---

## 常见问题（FAQ）

**Q: 为何 `println(x - 1)` 打印 `-1` 而不是 `255`？**
A: 因为 `Byte` 在二元算术中先**提升为 `Int`** 运算。若要得到 255，请用 `println<Byte>(x - 1)` 或先赋给 `Byte` 变量再打印。

**Q: 我能比较 `Byte` 和 `Int` 吗？**
A: 可以。比较时遵循与算术相同的**数值统一规则**（先提升，后比较）。

**Q: 字符与字符串打印是 UTF-8 吗？**
A: 是。`Char` 会转 UTF-8 输出，`String` 假定为 UTF-8（运行时退化为 “尽量按字节输出” 的策略）。

---

## 路线图（可能）

* 数组/切片与切片字面量
* 结构体/枚举与模式匹配解构
* 更完善的标准库与 I/O
* 更强的常量折叠与 SSA 优化（基于 Cranelift 的 pass）
* 更灵活的隐式泛型推断

---

## 致谢

* [Cranelift](https://github.com/bytecodealliance/wasmtime/tree/main/cranelift) 提供了优秀的后端基础设施。
* 本项目许多命名与组织基于教学/实验用途，欢迎 issue/PR 讨论改进。
