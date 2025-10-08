# Paw 语法速查卡

> **一页纸掌握全部语法** - 19 关键字 + 18 类型！

---

## 核心关键字（19个）

```
let  type  fn  import  pub
if  else  loop  break  return
is  as  async  await
self  Self  mut  true  false
```

**说明：**
- 业界最少的关键字设计
- 高度统一的语法原则
- `mut` 前置（`let mut x`，`mut self`）

---

## 类型系统（18个精确类型）⭐

### Rust 风格，无别名

```
整数（有符号）: i8, i16, i32, i64, i128
整数（无符号）: u8, u16, u32, u64, u128
浮点类型:       f32, f64
其他:           bool, char, string, void
```

**默认类型：**
- 整数字面量 → `i32`
- 浮点字面量 → `f64`

---

## 快速语法

### 变量

```paw
let x = 42               // i32（默认）
let mut x = 42           // 可变
let x: i64 = 42          // 显式类型
let tiny: i8 = 127       // 8位
let huge: i128 = 1000    // 128位
let byte: u8 = 255       // 无符号
let pi: f64 = 3.14       // 浮点
```

### 类型定义

```paw
// 结构体
type Point = struct {
    x: f64
    y: f64
    
    fn new(x: f64, y: f64) -> Self {
        Point { x, y }
    }
    
    fn distance(self) -> f64 {
        sqrt(self.x * self.x + self.y * self.y)
    }
}

// 公开结构体
pub type Color = struct {
    pub r: u8     // 0-255
    pub g: u8
    pub b: u8
    pub a: u8
    
    pub fn new(r: u8, g: u8, b: u8) -> Self {
        Color { r, g, b, a: 255 }
    }
}

// 枚举
type Option<T> = enum {
    Some(T)
    None
}

type Result<T, E> = enum {
    Ok(T)
    Err(E)
}

// Trait
type Display = trait {
    fn display(self) -> string
}
```

### 函数

```paw
fn add(x: i32, y: i32) -> i32 {     // 基础
    x + y
}

pub fn multiply(x: i64, y: i64) -> i64 {  // 公开
    x * y
}

fn generic<T>(item: T) -> T {       // 泛型
    item
}

fn fetch(url: string) async -> string {   // 异步
    await http.get(url)
}
```

### 方法参数

```paw
fn read(self) -> i32 { }        // 不可变 self
fn modify(mut self) { }         // 可变 self
fn consume(self) -> i32 { }     // 消耗 self
```

### 控制流

```paw
// if 表达式
let result = if x > 0 { "pos" } else { "neg" };

// 模式匹配（is）
let description = count is {
    0 -> "zero"
    1..10 -> "small"
    _ -> "large"
};

// 循环（统一用 loop）
loop {                          // 无限循环
    if should_break { break; }
}

loop count < 10 {            // 条件循环
    count += 1;
}

loop item in items {        // 遍历循环
    process(item);
}
```

### 类型转换

```paw
let i: i32 = 42;
let f = i as f64;               // i32 → f64
let tiny = i as i8;             // i32 → i8
let huge = i as i128;           // i32 → i128
let unsigned = i as u32;        // i32 → u32
```

### 模块导入

```paw
import user.User                          // 单个
import std.collections.{Vec, HashMap}     // 多个
import std.io.*                           // 全部
import database.DB as Database            // 别名
```

### 错误处理

```paw
fn divide(a: i32, b: i32) -> Result<i32, string> {
    if b == 0 { 
        Err("division by zero") 
    } else { 
        Ok(a / b) 
    }
}

let x = operation()?              // 传播错误
let y = result else { 0 }         // 默认值
```

---

## 类型使用模式

### 整数类型选择

```paw
// 小范围（-128 to 127）
let flags: i8 = 0;

// 端口号（0 to 65535）
let port: u16 = 8080;

// 常规整数（默认）
let count: i32 = 1000000;

// 大整数
let timestamp: i64 = 1234567890;

// 超大整数（密码学、UUID）
let hash: u128 = 123456789012345678901234567890;
```

### 网络编程

```paw
type IPv6Address = struct {
    addr: u128    // IPv6 正好 128 位
}

type SocketAddr = struct {
    ip: IPv6Address
    port: u16
}
```

### 图形编程

```paw
type Color = struct {
    r: u8    // 0-255
    g: u8
    b: u8
    a: u8
}

type Position = struct {
    x: f32
    y: f32
    z: f32
}
```

---

## 常用模式

### Option 处理

```paw
let value = option else { default };
if value is Some(x) { process(x); }

value is {
    Some(x) -> use(x)
    None -> default_value()
}
```

### Result 处理

```paw
let value = result?;                    // 传播
let value = result else { default };    // 默认

result is {
    Ok(v) -> v
    Err(e) -> handle(e)
}
```

### 字符串插值

```paw
let name = "Alice";
let age: i32 = 30;

"Hello, $name!"                // 简单变量
"Age: $age"                    // 整数
"Sum: ${2 + 2}"               // 表达式
```

---

## 完整示例

### HTTP API 服务器

```paw
import http.{Server, Request, Response}

type User = struct {
    id: i32
    name: string
    age: i32
    
    fn validate(self) -> Result<(), string> {
        if self.name.is_empty() { 
            Err("Name required") 
        } else if self.age < 0 or self.age > 150 {
            Err("Invalid age")
        } else { 
            Ok(()) 
        }
    }
}

fn handle(req: Request) async -> Response {
    req.method is {
        GET -> get_users().await
        POST -> create_user(req).await
        _ -> Response.not_found()
    }
}

fn main() async -> i32 {
    let server = Server.bind("0.0.0.0:8080");
    println("Server running on :8080");
    await server.serve(handle);
    0
}
```

---

## 语法对照表

| 概念 | 语法 | 示例 |
|------|------|------|
| 不可变变量 | `let name = value` | `let x = 42` |
| 可变变量 | `let mut name = value` | `let mut x: i32 = 0` |
| 结构体 | `type Name = struct { }` | `type Point = struct { x: f64 }` |
| 枚举 | `type Name = enum { }` | `type Color = enum { Red, Blue }` |
| Trait | `type Name = trait { }` | `type Show = trait { fn show(self) }` |
| 函数 | `fn name() -> T { }` | `fn add(x: i32) -> i32 { x + 1 }` |
| 泛型 | `fn name<T>() { }` | `fn id<T>(x: T) -> T { x }` |
| 异步 | `fn name() async { }` | `fn fetch() async -> string { }` |
| 条件 | `if cond { } else { }` | `if x > 0 { 1 } else { 0 }` |
| 匹配 | `value is { }` | `x is { 0 -> "zero", _ -> "other" }` |
| 无限循环 | `loop { }` | `loop { if done { break; } }` |
| 条件循环 | `loop cond { }` | `loop x < 10 { x += 1; }` |
| 遍历 | `loop item in { }` | `loop x in items { use(x); }` |
| 类型转换 | `value as Type` | `42 as f64` |
| 错误传播 | `expr?` | `let x = divide(10, 2)?` |

---

## 记忆技巧

### 3个统一原则

1. **声明统一** - `let` + `type`
   ```paw
   let x = value           // 变量
   type T = definition     // 类型
   ```

2. **模式统一** - `is`
   ```paw
   value is { patterns }   // 匹配
   if x is Pattern { }     // 判断
   ```

3. **循环统一** - `loop`
   ```paw
   loop { }                // 无限
   loop cond { }           // 条件（🆕 简化！）
   loop x in iter { }  // 遍历
   ```

### 类型记忆

```
有符号整数: i + 位数 (i8, i32, i128)
无符号整数: u + 位数 (u8, u32, u128)
浮点类型:   f + 位数 (f32, f64)
```

---

## 最小示例集

### 1. Hello World

```paw
fn main() -> i32 {
    println("Hello, World!");
    0
}
```

### 2. 类型和函数

```paw
fn double(x: i32) -> i32 {
    x * 2
}

fn main() -> i32 {
    let x: i32 = 21;
    let result = double(x);
    println("$x * 2 = $result");
    0
}
```

### 3. 结构体和方法

```paw
type Point = struct {
    x: i32
    y: i32
    
    fn sum(self) -> i32 {
        self.x + self.y
    }
}

fn main() -> i32 {
    let p = Point { x: 10, y: 20 };
    p.sum()
}
```

### 4. 128位大数

```paw
fn main() -> i32 {
    let huge: i128 = 170141183460469231731687303715884105727;
    let hash: u128 = 340282366920938463463374607431768211455;
    
    println("i128 max: $huge");
    println("u128 max: $hash");
    0
}
```

---

## 核心公式

```
  19 关键字 + 18 类型 = 完整语言
  
  let + type + import  = 所有声明
  is                   = 所有模式
  loop + if/for        = 所有循环
  fn + async           = 所有函数
  i8-i128, u8-u128     = 所有整数
```

### 学习路径

```
第1天: let, type, fn, if, loop        ← 5个关键字
第2天: is, as, import, i32, f64       ← 类型系统
第3天: async, await, 错误处理          ← 异步
第4天: pub, trait, 泛型               ← 高级特性
第5天: 实战项目                        ← 整合应用

总计: 5天入门，1周精通！ ⭐
```

---

**打印此页，贴在墙上！** 📄✨

*Paw = Rust 类型 + 19 关键字 + 统一语法* 🐾
