# Paw

一个用 **Cranelift** 做后端的小型教学/实验语言编译器。  
目前已支持：**一等整数/浮点/布尔/字符/字符串、`Byte`（真 `u8`）、控制流（`if/while/for/match`）、函数/重载、泛型（单态化）、`trait/impl`、简单标准库 `std::fmt::println`**，并生成本机目标的 **object file**。

---

## 快速开始

### 依赖

* Rust（建议 stable，`cargo` 可用）
* 本机目标工具链（用于把 `.o` 链成可执行程序）
* （可选）Zig：若你使用 `zig cc` 做跨平台链接

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

> 如何把生成的 `.o` 链成可执行：将 `compile_program(...)` 返回的字节写到 `*.o`，再用系统链接器或 `zig cc` 与运行时库链接。

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
* `String`（运行时以指针传递，打印时视为 UTF-8）

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

> 中间表示支持 `is_const` 标记；与源语言层的 `const` 等价。

### 表达式与控制流

* 算术：`+ - * /`
* 比较：`< <= > >= == !=`
* 逻辑：`&& || !`
* 块/作用域：`{ ... }`（块表达式可有值；无尾表达式时默认类型 `Int`）
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

* **泛型函数**采用**单态化**（monomorphization）：`foo<Int>` 会生成一个专门化符号并参与链接。
* **重载解析**在类型检查阶段完成；生成阶段使用带类型编码的 **mangle** 符号（`name__ol__P...__R...`）。
* **trait/impl**：
  * `impl Trait<Args...>` 在类型检查时校验 *arity*、方法集合与签名（经形参替换后）。
  * **合格名调用** `Trait::<T...>::method(...)`：若 `T...` 含类型变量则按需单态化；若全为具体类型则直接降解为自由函数符号。

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
println<Byte>(255);      // 以 Byte 语境打印
```

> `print`（不换行）可能受行缓冲影响；建议使用 `println` 或在运行时调用 `fflush(stdout)`/`stdout().flush()` 刷新。

---

## `Byte`（u8）要点

* **类型**：`Byte` 是**真 `u8`**，范围 0..=255。
* **与其他数值交互**：二元数值运算要求两侧 IR 类型一致；需要时请显式 `as` 转换（例如将 `Byte` 扩展为 `Int`）。
* **示例**：

```paw
import "std::fmt";

fn main() -> Int {
    let x: Byte = 0;

    println<Byte>(x - 1);  // 255（按 Byte 语境打印）
    let y: Byte = x - 1;   // 赋值到 Byte 时按 8 位截断
    println(y);            // 255

    // 与 Int 运算时可显式提升：
    println((x as Int) - 1);  // -1

    0
}
```

---

## 典型输出（节选）

```
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
42
```

---

## 诊断与错误码（节选）

**类型检查**

* `E2101`：重复的局部变量名
* `E2102`：未知变量
* `E2103`：对常量赋值
* `E2210`：`if` 分支类型不一致
* `E2311`：函数调用没有可匹配的重载
* `E2303`：合格名调用缺少显式类型实参
* `E2308`：合格名调用不被当前 `where` 约束允许
* ……

**代码生成**

* `CG0001`：缺失符号/函数 ID
* `CG0002`：ABI 不可用的类型
* `CG0003`：impl 方法符号未知（未声明）
* `CG0004`：泛型模板/单态化问题
* `CG0005`：重载解析产生多候选（调用不唯一）

---

## 实现细节（简述）

* **Cranelift IR**：
  * 语言→IR 映射：`Byte/Bool → i8`，`Int → i32`，`Long → i64`，`Float → f32`，`Double → f64`，`Char → i32`，`String → i64`（指针）。
  * 布尔在条件处使用 `b1`，进/出位置做 `i8`↔`b1` 转换。
  * 字符串常量以只读数据驻留，函数内通过 `global_value(i64)` 取址。
* **单态化**：
  * 预扫描显式 `<...>` 调用并声明实例；隐式常见形态（如 `println<T>(x:T)`）在调用点按需实例化。
  * impl 泛型方法在合格名调用处单态化并立刻定义。
* **符号整形（mangle）**：重载与泛型实例的参数/返回类型会编码入符号名，确保链接唯一。

---

## FAQ

**Q: 为何 `println(x - 1)` 有时打印 255，有时打印 -1？**  
A: 取决于表达式的**静态类型**与所选重载：按 `Byte` 语境打印会显示 0..=255；若先显式转为 `Int` 再打印，则是有符号整型表现。

**Q: `print` 没反应？**  
A: 可能被缓冲了。建议用 `println`，或在运行时调用 `fflush(stdout)`/`stdout().flush()`。

---

## 路线图（可能）

* 数组/切片与切片字面量
* 结构体/枚举与模式匹配解构
* 更完善的标准库与 I/O
* 更强的常量折叠与优化（基于 Cranelift 的 pass）
* 更灵活的隐式泛型推断
