# Paw 快速开始

## 🚀 30秒了解 Paw

```paw
import std.io.println;

fn main() -> int {
    let mut count = 0;
    
    loop if count < 5 {
        println("Count: $count");
        count += 1;
    }
    
    0
}
```

**特点：**
- 仅 19 个关键字
- 分号与 Rust 一致
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

### 步骤 2: 记住 3 大规则（1分钟）

```paw
1. 声明统一
   let x = 5;
   type T = struct { };

2. 模式统一
   value is { pattern -> result }

3. 循环统一
   loop { }
   loop if condition { }
   loop for item in items { }
```

### 步骤 3: 理解分号（1分钟）

```paw
// 语句 → 需要分号
let x = 42;
println("Hi");

// 返回值 → 不需要分号
fn get() -> int {
    42  // 返回值
}
```

---

## 📝 第一个程序

### 创建 hello.paw

```paw
fn main() -> int {
    let name = "World";
    println("Hello, $name!");
    0
}
```

### 编译运行

```bash
zig build
./zig-out/bin/pawc hello.paw -o hello
./hello
```

---

## 📚 接下来

### 5分钟入门
→ [START_HERE.md](START_HERE.md)

### 10分钟速查
→ [CHEATSHEET.md](CHEATSHEET.md)

### 1小时系统学习
→ [SYNTAX.md](SYNTAX.md)

---

**就这么简单！** 🎉

