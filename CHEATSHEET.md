# Paw 语法速查卡

> **一页纸掌握全部语法** - 仅 19 个关键字！

---

## 核心关键字（19个）

```
let  type  fn  import  pub
if  else  loop  break  return
is  as  async  await
self  Self  mut  true  false
```

**说明：**
- 模块由文件系统控制（无需 `mod`）
- `import` 导入模块（语义更清晰）
- `mut` 前置（`let mut x`，`mut self`）

---

## 快速语法

### 变量
```paw
let x = 42               // 不可变
let mut x = 42           // 可变（mut 前置）
let x: int = 42          // 带类型
let (a, b) = (1, 2)      // 解构
let (mut a, b) = (1, 2)  // a 可变，b 不可变
```

### 类型
```paw
type Point = struct {    // 结构体（私有）
    x: float
    y: float
    
    fn new(x: float, y: float) -> Self {  // 方法直接在这里
        Point { x, y }
    }
}

pub type Point = struct { // 公开结构体
    pub x: float          // 公开字段
    pub y: float
    
    pub fn new() -> Self { }  // 公开方法
    fn internal() { }         // 私有方法
}

type Color = enum {      // 枚举
    Red
    Green
    Blue
    RGB(int, int, int)
}

type Display = trait {   // trait
    fn show(self) -> string
}

type ID = int            // 类型别名
```

### 函数
```paw
fn add(x: int, y: int) -> int = x + y        // 单行（私有）
pub fn add(x: int, y: int) -> int = x + y    // 公开
fn process(data: string) { println(data) }   // 多行
fn generic<T>(item: T) -> T { item }         // 泛型
fn fetch(url: string) async -> string { }    // 异步
```

### 方法参数
```paw
fn read(self) -> int { }        // 不可变 self
fn modify(mut self) { }         // 可变 self（mut 前置）
fn consume(self) -> int { }     // 消耗 self
```

### 控制流
```paw
// if 表达式
if x > 0 { "pos" } else { "neg" }

// 模式匹配
value is {
    0 -> "zero"
    1..10 -> "small"
    _ -> "large"
}

// 循环（全用 loop）
loop { break }                 // 无限
loop if cond { }              // 条件
loop for item in items { }    // 遍历
```

### 模块导入
```paw
import user.User                    // 单个
import std.collections.{Vec, HashMap} // 多个
import std.io.*                     // 全部
import database.DB as Database      // 别名
```

### 错误处理
```paw
fn divide(a: int, b: int) -> Result<int, string> {
    if b == 0 { Err("div by zero") }
    else { Ok(a / b) }
}

let x = operation()?          // 传播错误
let y = result else { 0 }     // 默认值
```

### 借用
```paw
let borrowed = &data          // 不可变借用
let mutable = &mut data       // 可变借用
```

---

## 常用模式

### Option 处理
```paw
let value = option else { default }
if value is Some(x) { import(x) }
```

### Result 处理
```paw
let value = result?                    // 传播
let value = result else { default }    // 默认
result is {                            // 匹配
    Ok(v) -> v
    Err(e) -> handle(e)
}
```

### 字符串插值
```paw
let name = "Alice"
"Hello, $name!"                // 简单变量
"2 + 2 = ${2 + 2}"            // 表达式
```

### 迭代器
```paw
items.map(|x| x * 2)          // 转换
items.filter(|x| x > 0)       // 过滤
items.sum()                   // 聚合
items.collect()               // 收集
```

---

## 完整示例

### HTTP API
```paw
import http.{Server, Request, Response}

type User = struct {
    id: int
    name: string
    
    fn validate(self) -> Result<(), string> {
        if self.name.is_empty() { Err("Name required") }
        else { Ok(()) }
    }
}

fn handle(req: Request) async -> Response {
    req.path() is {
        "/users" -> get_users().await
        "/users/{id}" -> get_user(req.param("id")?).await
        _ -> Response.not_found()
    }
}

fn main() async -> Result<(), Error> {
    let server = Server.bind("0.0.0.0:8080")?
    println("Server running")
    server.serve(handle).await
}
```

---

## 语法对照表

| 概念 | 语法 | 示例 |
|------|------|------|
| 不可变变量 | `let name = value` | `let x = 42` |
| 可变变量 | `let mut name = value` | `let mut x = 0` |
| 结构体 | `type Name = struct { }` | `type Point = struct { x: int }` |
| 公开结构体 | `pub type Name = struct { }` | `pub type Point = struct { pub x: int }` |
| 枚举 | `type Name = enum { }` | `type Color = enum { Red, Blue }` |
| Trait | `type Name = trait { }` | `type Show = trait { fn show(self) }` |
| 公开函数 | `pub fn name() { }` | `pub fn api() -> int = 42` |
| 函数 | `fn name() -> T { }` | `fn add(x: int) -> int = x + 1` |
| 泛型 | `fn name<T>() { }` | `fn id<T>(x: T) -> T = x` |
| 异步 | `fn name() async { }` | `fn fetch() async -> string { }` |
| 条件 | `if cond { } else { }` | `if x > 0 { 1 } else { 0 }` |
| 匹配 | `value is { }` | `x is { 0 -> "zero", _ -> "other" }` |
| 无限循环 | `loop { }` | `loop { if done { break } }` |
| 条件循环 | `loop if cond { }` | `loop if x < 10 { x += 1 }` |
| 遍历 | `loop for item in { }` | `loop for x in items { println(x) }` |
| 导入 | `import path.Name` | `import std.collections.Vec` |
| 多个导入 | `import path.{A, B}` | `import std.{io, fs}` |
| 借用 | `&value` | `process(&data)` |
| 可变借用 | `&mut value` | `modify(&mut data)` |
| 错误传播 | `expr?` | `let x = divide(10, 2)?` |
| 类型转换 | `value as Type` | `42 as float` |

---

## 记忆技巧

### 3个统一原则

1. **声明统一** - 用 `let` 和 `type`
   ```paw
   let x = value           // 变量
   type T = definition     // 类型
   ```

2. **模式统一** - 用 `is`
   ```paw
   value is { patterns }   // 匹配
   if x is Pattern { }     // 判断
   ```

3. **循环统一** - 用 `loop`
   ```paw
   loop { }                // 基础
   loop if/for { }         // 扩展
   ```

### mut 规则

```paw
let mut x = 5           // 变量可变（前置）
let (mut a, b) = (1, 2) // 解构中的可变
fn modify(mut self) { } // 方法参数（前置）
```

### 模块规则

```
文件即模块
import 导入
.paw 扩展名
mod.paw 目录入口
```

---

## 最小示例集

### 1. Hello World
```paw
fn main() -> int {
    println("Hello, World!")
    0
}
```

### 2. 函数和变量
```paw
fn double(x: int) -> int = x * 2

fn main() -> int {
    let x = 21
    let result = double(x)
    println("$x * 2 = $result")
    0
}
```

### 3. 结构体和方法
```paw
type Point = struct {
    x: int
    y: int
    
    fn sum(self) -> int = self.x + self.y
}

fn main() -> int {
    let p = Point { x: 10, y: 20 }
    p.sum()
}
```

### 4. 可变性
```paw
fn main() -> int {
    let mut count = 0
    
    loop if count < 10 {
        count += 1
    }
    
    count
}
```

### 5. 模块导入
```paw
// math.paw
pub fn add(x: int, y: int) -> int = x + y

// main.paw
import math.add

fn main() -> int {
    add(2, 3)
}
```

---

## 记住这些，你就掌握了 Paw！

### 核心公式
```
  let + type + import  = 所有声明
  is                   = 所有模式
  loop + if/for        = 所有循环
  fn + async           = 所有函数
  mut self             = 可变方法
  & + mut              = 所有借用
```

### 学习路径
```
第1天: let, type, fn, if, loop        ← 5个关键字
第2天: is, as, import, &, ?           ← 5个概念
第3天: async, await                   ← 异步
第4天: pub, trait                     ← 高级特性
第5天: 实战项目                        ← 整合应用

总计: 5天入门，1周精通！ ⭐
```

---

**打印此页，贴在墙上！** 📄✨
