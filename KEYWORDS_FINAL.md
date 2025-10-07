# Paw 关键字最终版本

## 🎯 仅 19 个关键字！

```
┌─────────────────────────────────────────┐
│ Paw 完整关键字列表（19个）             │
├─────────────────────────────────────────┤
│                                         │
│ 声明 (2):    let, type                 │
│ 函数 (1):    fn                        │
│ 控制 (5):    if, else, loop,           │
│              break, return              │
│ 模式 (2):    is, as                    │
│ 异步 (2):    async, await              │
│ 导入 (1):    import                    │
│ 其他 (6):    pub, self, Self,          │
│              mut, true, false           │
│                                         │
└─────────────────────────────────────────┘
```

---

## 关键字详解

### 1. `let` - 变量声明

```paw
let x = 42              // 不可变
let mut x = 42          // 可变
let x: int = 42         // 带类型
let (a, b) = (1, 2)     // 解构
```

### 2. `type` - 类型定义

```paw
type Point = struct { x: int, y: int }
type Color = enum { Red, Green, Blue }
type Show = trait { fn show(self) -> string }
type ID = int
```

### 3. `fn` - 函数定义

```paw
fn add(x: int) -> int = x + 1
fn process(mut self) { }
fn fetch() async -> string { }
```

### 4-8. 控制流

```paw
if condition { }        // if
else { }                // else
loop { }                // loop
break value             // break
return value            // return
```

### 9. `is` - 模式匹配

```paw
value is {              // 匹配表达式
    pattern -> result
}

if value is Some(x) { } // 模式判断
```

### 10. `as` - 类型转换

```paw
let num = value as int
let text = 42 as string
```

### 11-12. 异步

```paw
fn fetch() async { }    // async
let data = fetch().await// await
```

### 13. `import` - 导入模块

```paw
import std.collections.Vec
import user.{User, UserService}
import math.*
import db.Database as DB
```

### 14. `pub` - 公开声明

```paw
pub type User = struct { }
pub fn api() { }
```

### 15-17. 特殊标识符

```paw
self                    // 实例引用
Self                    // 类型引用
mut                     // 可变标记
```

### 18-19. 布尔字面量

```paw
true
false
```

---

## 模块系统（文件控制）

### 文件结构
```
src/
├── main.paw           # 主模块
├── user.paw          # user 模块
└── utils/            # utils 模块（目录）
    ├── mod.paw       # 入口
    └── string.paw    # 子模块
```

### 导入方式
```paw
import user.User
import utils.string.format
```

**无需 `mod` 关键字！** 文件系统即模块系统 ✨

---

## mut 位置规则

### ✅ 正确用法（mut 前置）

```paw
let mut x = 42              // 变量
let mut count = 0

fn modify(mut self) { }     // 方法参数
fn process(mut data: Vec<int>) { }

let (mut a, b) = (1, 2)     // 解构
```

### ❌ 旧语法（已废弃）

```paw
let x mut = 42              // ❌ 错误
self mut                    // ❌ 错误
```

---

## 与其他语言对比

| 语言 | 关键字数 | 模块系统 | 可变性 |
|------|---------|---------|--------|
| Rust | 50+ | 文件 + `mod` | `let mut` |
| Go | 25 | 文件 + `package` | 无不可变 |
| Swift | 40+ | 文件 + `import` | `var`/`let` |
| Paw | **19** ⭐ | **文件** | **`let mut`** |

---

## 完整关键字用法

```paw
import std.collections.Vec       // import

pub type User = struct {         // pub, type
    pub id: int
    pub name: string
    
    pub fn new(name: string) -> Self {  // pub, fn, Self
        let mut user = User {    // let, mut
            id: 0
            name
        }
        user
    }
    
    fn is_valid(self) -> bool {  // fn, self
        if self.name.is_empty() {// if
            false                // false
        } else {                 // else
            true                 // true
        }
    }
}

fn main() async -> int {         // fn, async
    let user = User.new("Alice")
    
    user.is_valid() is {         // is
        true -> {
            println("Valid user")
            let result = 0
            return result        // return
        }
        false -> {
            loop {               // loop
                println("Invalid")
                break 1          // break
            }
        }
    } as int                     // as
}
```

**这段代码使用了所有 19 个关键字！**

---

## 记忆口诀

```
声明用 let 和 type，
函数统一用 fn，
导入记得用 import，
模式全部用 is，
循环只要记 loop，
可变就加 mut 前缀。

十九个关键字，
三天就掌握，
文件即模块，
简洁又优雅！
```

---

## 总结

**Paw 最终版本：**
- ✅ 19 个关键字（最少）
- ✅ `mut` 前置（更符合直觉）
- ✅ `import` 导入（更清晰）
- ✅ 文件即模块（更简洁）
- ✅ 完全统一的风格

**再次减少 1 个关键字！从 20 → 19！** 🎉✨

