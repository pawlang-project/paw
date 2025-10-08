# Paw 快速开始

## 🚀 30秒了解 Paw

```paw
import std.io.println

fn main() -> i32 {
    let mut count: i32 = 0;
    
    loop count < 5 {
        println("Count: $count");
        count += 1;
    }
    
    0
}
```

**特点：**
- 仅 19 个关键字（业界最少）
- 18 个精确类型（Rust 风格）
- 极简但强大

---

## ⚡ 3分钟上手

### 步骤 1: 了解关键字（1分钟）

```
仅需记住 19 个：

let, type, fn, import, pub,
if, else, loop, break, return,
is, as, async, await,
self, Self, mut, true, false
```

### 步骤 2: 记住类型系统（1分钟）

```
18 个精确类型（Rust 风格，无别名）：

整数（有符号）: i8, i16, i32, i64, i128
整数（无符号）: u8, u16, u32, u64, u128
浮点类型:       f32, f64
其他:           bool, char, string, void
```

### 步骤 3: 记住 3 大规则（1分钟）

```paw
1. 声明统一
   let x = 5;
   type T = struct { };

2. 模式统一
   value is { pattern -> result }

3. 循环统一
   loop { }
   loop condition { }
   loop item in items { }
```

---

## 📝 第一个程序

### 创建 hello.paw

```paw
fn main() -> i32 {
    let name = "World";
    println("Hello, $name!");
    0
}
```

### 编译运行

```bash
zig build
./zig-out/bin/pawc hello.paw -o hello -v
```

---

## 💡 类型示例

### 基础类型

```paw
// 整数（默认 i32）
let count = 42;           // i32
let tiny: i8 = 127;       // 8位
let huge: i128 = 1000000; // 128位

// 无符号
let byte: u8 = 255;
let large: u64 = 1000000;

// 浮点（默认 f64）
let pi = 3.14;            // f64
let single: f32 = 3.14;   // 32位
```

### 结构体示例

```paw
type Point = struct {
    x: f64
    y: f64
    
    fn distance(self) -> f64 {
        sqrt(self.x * self.x + self.y * self.y)
    }
}

fn main() -> i32 {
    let p = Point { x: 3.0, y: 4.0 };
    let d = p.distance();
    0
}
```

---

## 📚 接下来

### 5分钟速查
→ [CHEATSHEET.md](CHEATSHEET.md)

### 了解类型系统
→ [TYPE_SYSTEM.md](TYPE_SYSTEM.md)

### 1小时系统学习
→ [SYNTAX.md](SYNTAX.md)

### 查看所有文档
→ [DOCS.md](DOCS.md)

---

**就这么简单！** 🎉

*极简关键字 + 纯粹类型 = 完美！* 🐾✨
