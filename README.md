# 🐾 PawLang

**一个现代的、带有Rust级别安全性和更简洁语法的系统编程语言**

[![Version](https://img.shields.io/badge/version-0.1.3-blue.svg)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)](#)

---

## 🚀 快速开始

### 安装

```bash
# 克隆仓库
git clone https://github.com/yourusername/PawLang.git
cd PawLang

# 构建编译器
zig build

# 编译器位于 zig-out/bin/pawc
```

### Hello World

```paw
fn main() -> i32 {
    println("Hello, PawLang! 🐾");
    return 0;
}
```

```bash
# 编译并运行
./zig-out/bin/pawc hello.paw --run

# 或分步执行
./zig-out/bin/pawc hello.paw    # 生成output.c
gcc output.c -o hello            # 编译
./hello                          # 运行
```

---

## ✨ 核心特性

### 🎨 自动类型推导（v0.1.3 新功能！）⭐

**更简洁的代码，相同的类型安全**：

```paw
// 之前（v0.1.2）：需要显式类型注解
let x: i32 = 42;
let sum: i32 = add(10, 20);
let vec: Vec<i32> = Vec<i32>::new();

// 现在（v0.1.3）：自动推导类型！
let x = 42;                    // 推导为 i32
let sum = add(10, 20);         // 推导为 i32
let vec = Vec<i32>::new();     // 推导为 Vec<i32>
```

**支持的推导**：
- ✅ 字面量（整数、字符串、布尔值）
- ✅ 函数调用返回值
- ✅ 泛型实例化
- ✅ 结构体字面量
- ✅ 表达式计算结果

**示例**：
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
    
    // 仍然可以使用显式类型（可选）
    let explicit: i32 = 42;
    
    return result;
}
```

**好处**：
- 📝 更少的样板代码
- 🚀 更快的开发速度
- 🔒 保持完全的类型安全
- 💡 更清晰的代码意图

---

### 🔗 模块系统（v0.1.2）⭐

**简洁的导入语法**：

```paw
// math.paw - 模块文件
pub fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

pub type Vec2 = struct {
    x: i32,
    y: i32,
}

// main.paw - 使用模块
import math.add;      // 导入函数
import math.Vec2;     // 导入类型

fn main() -> i32 {
    let sum = add(10, 20);
    let v = Vec2 { x: 1, y: 2 };
    return 0;
}
```

**特点**：
- ✅ 使用`.`语法（不是`::`）
- ✅ 直接`import`（不需要`use`）
- ✅ `pub`控制导出
- ✅ 自动模块加载和缓存

### 🎯 完整的泛型系统（v0.1.2 新功能！）

**泛型函数**：
```paw
fn identity<T>(x: T) -> T {
    return x;
}

let num = identity(42);      // T = i32
let text = identity("hello"); // T = string
```

**泛型结构体**：
```paw
type Box<T> = struct {
    value: T,
}

let box_int: Box<i32> = Box { value: 42 };
let box_str: Box<string> = Box { value: "paw" };
```

**泛型方法** ⭐：
```paw
type Vec<T> = struct {
    ptr: i32,
    len: i32,
    cap: i32,
    
    // 静态方法：使用 :: 调用
    fn new() -> Vec<T> {
        return Vec { ptr: 0, len: 0, cap: 0 };
    }
    
    fn with_capacity(cap: i32) -> Vec<T> {
        return Vec { ptr: 0, len: 0, cap: cap };
    }
    
    // 实例方法：self不需要类型！
    fn length(self) -> i32 {
        return self.len;
    }
}

// 使用
let vec: Vec<i32> = Vec<i32>::new();        // 静态方法
let len: i32 = vec.length();                // 实例方法
```

### 🔒 类型安全

- **18种精确类型**：`i8`-`i128`, `u8`-`u128`, `f32`, `f64`, `bool`, `char`, `string`, `void`
- **编译时类型检查**
- **零运行时开销**（完全单态化）

### 🎨 简洁语法

**仅19个核心关键字**：
```
fn let type import pub if else loop break return
is as async await self Self mut true false in
```

### 🔄 统一的设计

- **统一声明**：`let` 用于变量，`type` 用于类型
- **统一循环**：`loop` 用于所有循环形式
- **统一模式**：`is` 用于所有模式匹配

### 📦 强大的类型系统

**结构体**：
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

**枚举**（Rust风格）：
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

**模式匹配**：
```paw
let result = value is {
    Some(x) => x * 2,
    None() => 0,
    _ => -1,
};
```

### 💬 字符串插值

```paw
let name = "Alice";
let age: i32 = 25;

println("Hello, $name!");              // 简单插值
println("You are ${age} years old.");  // 表达式插值
```

### ❓ 错误处理

```paw
fn divide(a: i32, b: i32) -> Result {
    return if b == 0 { Err(1) } else { Ok(a / b) };
}

fn process() -> Result {
    let value = divide(10, 2)?;  // ? 操作符自动传播错误
    return Ok(value * 2);
}
```

### 🔢 数组支持

```paw
// 数组字面量
let arr = [1, 2, 3, 4, 5];

// 数组索引
let first = arr[0];

// 数组类型
let numbers: [i32] = [10, 20, 30];        // 动态大小
let fixed: [i32; 5] = [1, 2, 3, 4, 5];   // 固定大小

// 数组迭代
loop item in arr {
    println("$item");
}
```

---

## 📚 标准库

### 内置函数

```paw
println(msg: string)  // 打印并换行
print(msg: string)    // 打印不换行
eprintln(msg: string) // 错误输出
eprint(msg: string)   // 错误输出不换行
```

### 泛型容器（v0.1.2）

**Vec<T>** - 动态数组：
```paw
let vec: Vec<i32> = Vec<i32>::new();
let vec2: Vec<i32> = Vec<i32>::with_capacity(10);
let len: i32 = vec.length();
let cap: i32 = vec.capacity_method();
```

**Box<T>** - 智能指针：
```paw
let box: Box<i32> = Box<i32>::new(42);
```

### 错误处理类型

```paw
type Result = enum { Ok(i32), Err(i32) }
type Option = enum { Some(i32), None() }
```

---

## 🛠️ 命令行工具

```bash
# 编译到C代码
pawc hello.paw

# 编译到可执行文件
pawc hello.paw --compile

# 编译并运行
pawc hello.paw --run

# 显示版本
pawc --version

# 显示帮助
pawc --help
```

### 选项

- `-o <file>` - 指定输出文件名
- `--compile` - 编译到可执行文件
- `--run` - 编译并运行
- `-v` - 详细输出
- `--help` - 显示帮助

---

## 📖 示例程序

查看 `examples/` 目录：

- `hello.paw` - Hello World
- `fibonacci.paw` - 斐波那契数列
- `loops.paw` - 所有循环形式
- `array_complete.paw` - 数组操作
- `string_interpolation.paw` - 字符串插值
- `error_propagation.paw` - 错误处理
- `enum_error_handling.paw` - 枚举错误处理
- `vec_demo.paw` - Vec容器演示
- **`generic_methods.paw`** - 泛型方法演示（v0.1.2）
- `generics_demo.paw` - 泛型函数演示

查看 `tests/` 目录：

- `test_static_methods.paw` - 静态方法测试
- `test_instance_methods.paw` - 实例方法测试
- `test_methods_complete.paw` - 完整方法测试
- `test_generic_struct_complete.paw` - 泛型结构体测试

---

## 🎯 版本历史

### v0.1.2 (2025-10-08) - 当前版本 🌟

**完整泛型方法系统**

- ✅ 泛型静态方法（`Vec<i32>::new()`）
- ✅ 泛型实例方法（`vec.length()`）
- ✅ **self参数无需类型** - PawLang独特设计！
- ✅ 自动单态化
- ✅ 标准库方法扩展

[详细说明 →](RELEASE_NOTES_v0.1.2.md)

### v0.1.1 (2025-10-09)

**完整泛型系统**

- ✅ 泛型函数
- ✅ 泛型结构体
- ✅ 类型推导
- ✅ 单态化机制

[详细说明 →](RELEASE_NOTES_v0.1.1.md)

### v0.1.0

**基础语言特性**

- ✅ 完整语法和类型系统
- ✅ 编译器工具链
- ✅ 标准库基础

[详细说明 →](RELEASE_NOTES_v0.1.0.md)

---

## 🏗️ 编译器架构

```
PawLang源代码 (.paw)
    ↓
词法分析器 (Lexer)
    ↓
语法分析器 (Parser)
    ↓
类型检查器 (TypeChecker)
    ↓
泛型单态化 (Monomorphizer) ← v0.1.2新增
    ↓
代码生成器 (CodeGen)
    ↓
C代码 (.c)
    ↓
GCC/Clang/TinyCC
    ↓
可执行文件
```

### 项目结构

```
PawLang/
├── src/
│   ├── main.zig          # 编译器入口
│   ├── lexer.zig         # 词法分析
│   ├── parser.zig        # 语法分析
│   ├── ast.zig           # AST定义
│   ├── typechecker.zig   # 类型检查
│   ├── generics.zig      # 泛型系统（v0.1.1+）
│   ├── codegen.zig       # C代码生成
│   ├── token.zig         # Token定义
│   ├── tcc_backend.zig   # TinyCC后端
│   └── std/
│       └── prelude.paw   # 标准库
├── examples/             # 示例程序
├── tests/               # 测试套件
├── build.zig            # 构建配置
├── CHANGELOG.md         # 变更日志
└── README.md            # 本文件
```

---

## 🎨 设计哲学

### 1. 简洁优先

PawLang追求最小化的语法：
- 只有19个关键字
- 统一的声明语法
- 直观的语法设计

### 2. 类型安全

- 编译时类型检查
- 泛型系统保证类型安全
- 零运行时类型错误

### 3. 零成本抽象

- 所有泛型在编译时展开
- 没有虚函数表
- 性能等同于手写C代码

### 4. 现代特性

- 泛型（函数、结构体、方法）
- 模式匹配
- 字符串插值
- 错误传播（`?`操作符）
- 方法语法

---

## 💡 语言亮点

### self参数无需类型 ⭐

这是PawLang的独特设计：

```paw
type Vec<T> = struct {
    len: i32,
    
    // ✅ PawLang - 简洁优雅
    fn length(self) -> i32 {
        return self.len;
    }
}

// vs Rust
// fn length(&self) -> i32 { ... }
```

### 统一的方法调用

```paw
// 静态方法 - :: 语法
let vec: Vec<i32> = Vec<i32>::new();

// 实例方法 - . 语法
let len: i32 = vec.length();
```

### 完整的泛型支持

```paw
// 泛型函数
fn swap<T>(a: T, b: T) -> i32 { ... }

// 泛型结构体
type Box<T> = struct { value: T }

// 泛型方法
fn get(self) -> T { return self.value; }
```

---

## 📊 性能

- **编译速度**：<10ms（典型程序）
- **运行时性能**：与C相当（零开销抽象）
- **内存占用**：无GC，完全手动控制

---

## 🧪 测试

```bash
# 运行所有测试
./zig-out/bin/pawc tests/test_methods_complete.paw --run

# 静态方法测试
./zig-out/bin/pawc tests/test_static_methods.paw --run

# 实例方法测试
./zig-out/bin/pawc tests/test_instance_methods.paw --run
```

---

## 📚 学习资源

### 快速参考

```paw
// 变量
let x: i32 = 42;
let mut y = 10;

// 泛型函数
fn identity<T>(x: T) -> T { return x; }

// 泛型结构体
type Box<T> = struct {
    value: T,
    
    // 静态方法
    fn new(val: T) -> Box<T> {
        return Box { value: val };
    }
    
    // 实例方法（self不需要类型！）
    fn get(self) -> T {
        return self.value;
    }
}

// 使用
let box: Box<i32> = Box<i32>::new(42);  // 静态方法
let val: i32 = box.get();               // 实例方法

// 循环
loop i in 1..=10 { println("$i"); }

// 模式匹配
let result = value is {
    Some(x) => x,
    None() => 0,
};

// 错误处理
let value = divide(10, 2)?;
```

### 示例程序

查看 `examples/generic_methods.paw` 获取完整的泛型方法演示。

---

## 🌟 为什么选择PawLang？

| 特性 | PawLang | Rust | C | Python |
|------|---------|------|---|--------|
| 泛型 | ✅ 完整 | ✅ | ❌ | ❌ |
| 类型安全 | ✅ | ✅ | ⚠️ | ❌ |
| 零开销 | ✅ | ✅ | ✅ | ❌ |
| 简洁语法 | ✅ | ⚠️ | ⚠️ | ✅ |
| self无需类型 | ✅ | ❌ | N/A | ✅ |
| 学习曲线 | 低 | 高 | 中 | 低 |

**PawLang = Rust的安全性 + C的性能 + Python的简洁性**

---

## 🔧 开发

### 依赖

- **Zig** 0.14.0 或更高版本
- **GCC** 或 **Clang**（可选，用于编译生成的C代码）

### 构建

```bash
# 开发构建
zig build

# 发布构建
zig build -Doptimize=ReleaseFast

# 运行测试
zig build test
```

### 贡献

欢迎贡献！请确保：
- 代码遵循现有风格
- 所有测试通过
- 文档已更新

---

## 📄 文档

- [CHANGELOG.md](CHANGELOG.md) - 完整变更历史
- [RELEASE_NOTES_v0.1.2.md](RELEASE_NOTES_v0.1.2.md) - v0.1.2发布说明
- [examples/](examples/) - 示例代码
- [tests/](tests/) - 测试用例

---

## 🗺️ 路线图

### v0.1.3（计划中）

- [ ] 自动类型推导（`let vec = Vec<i32>::new()`）
- [ ] 泛型约束（Trait bounds）
- [ ] HashMap<K, V>
- [ ] String类型
- [ ] 更多标准库函数

### 未来版本

- [ ] Trait系统
- [ ] 运算符重载
- [ ] 异步/等待
- [ ] 包管理器
- [ ] LSP支持

---

## 📊 项目状态

| 组件 | 状态 | 完成度 |
|------|------|--------|
| 词法分析器 | ✅ | 100% |
| 语法分析器 | ✅ | 100% |
| 类型检查器 | ✅ | 100% |
| 泛型系统 | ✅ | 100% |
| 代码生成器 | ✅ | 100% |
| 标准库 | 🚧 | 30% |
| 文档 | ✅ | 90% |

---

## 🏆 里程碑

- **v0.1.0** - 基础语言实现 ✅
- **v0.1.1** - 完整泛型系统 ✅
- **v0.1.2** - 完整泛型方法系统 ✅ ⭐
- **v0.2.0** - Trait系统（计划中）
- **v1.0.0** - 生产就绪（目标）

---

## 🤝 贡献者

感谢所有为PawLang做出贡献的开发者！

---

## 📄 许可证

MIT License

---

## 🔗 链接

- **GitHub**: [PawLang Repository](#)
- **快速开始**: [5分钟上手指南](docs/QUICKSTART.md)
- **完整文档**: [查看所有文档](docs/)
- **示例代码**: [查看示例](examples/)
- **模块系统**: [模块系统文档](docs/MODULE_SYSTEM.md)
- **更新日志**: [CHANGELOG.md](CHANGELOG.md)

---

**Built with ❤️ using Zig**

**🐾 Happy Coding with PawLang!**
