# 📚 PawLang Prelude v0.2.0

**路径**: `src/prelude/prelude.paw`  
**版本**: v0.2.0  
**状态**: ✅ 已更新并增强

---

## 🎉 重大更新

### 目录重命名

```diff
- src/std/prelude.paw      ← 旧名称（容易与 stdlib/ 混淆）
+ src/prelude/prelude.paw  ← 新名称（更清晰）
```

### 内容大幅扩充

```
之前: 108 行（基础类型定义）
现在: 300+ 行（完整的实用函数）
```

---

## 📦 新增功能

### 1. 字符串操作 ✨

从 `stdlib/string` 中提取最常用的函数到 prelude：

```paw
// 无需 import，自动可用！
let s: string = "hello";

let len: i32 = string_length(s);           // 5
let ch: char = char_at(s, 0);              // 'h'
let eq: bool = string_equals("a", "a");    // true
```

**包含的函数**:
- `string_length(s: string) -> i32`
- `char_at(s: string, index: i32) -> char`
- `string_equals(s1: string, s2: string) -> bool`

---

### 2. 字符判断 ✨

```paw
let digit: char = '5';
let space: char = ' ';
let letter: char = 'A';

is_digit(digit);        // true
is_whitespace(space);   // true
is_alpha(letter);       // true
is_alphanumeric('a');   // true

let num: i32 = char_to_digit('5');  // 5
```

**包含的函数**:
- `is_whitespace(ch: char) -> bool`
- `is_digit(ch: char) -> bool`
- `is_alpha(ch: char) -> bool`
- `is_alphanumeric(ch: char) -> bool`
- `char_to_digit(ch: char) -> i32`
- `char_equals(ch1: char, ch2: char) -> bool`

---

### 3. 数学函数 ✨

```paw
let a: i32 = -10;
let b: i32 = 20;
let c: i32 = 5;

abs(a);      // 10
min(b, c);   // 5
max(b, c);   // 20
```

**包含的函数**:
- `abs(n: i32) -> i32`
- `min(a: i32, b: i32) -> i32`
- `max(a: i32, b: i32) -> i32`

---

### 4. 断言和调试 ✨

```paw
fn divide(a: i32, b: i32) -> i32 {
    assert(b != 0, "Division by zero!");
    return a / b;
}

fn critical_error() -> i32 {
    panic("Something went terribly wrong!");
    return 1;
}
```

**包含的函数**:
- `assert(condition: bool, msg: string) -> i32`
- `panic(msg: string) -> i32`

---

### 5. 类型转换 ✨

```paw
let b: bool = true;
let n: i32 = bool_to_i32(b);  // 1

let zero: i32 = 0;
let f: bool = i32_to_bool(zero);  // false
```

**包含的函数**:
- `bool_to_i32(b: bool) -> i32`
- `i32_to_bool(n: i32) -> bool`

---

### 6. 增强的 Vec<T> ✨

```paw
// 新增方法
let v: Vec<i32> = Vec::new();

let len: i32 = v.length();      // 获取长度
let cap: i32 = v.capacity();    // 获取容量
let empty: bool = v.is_empty(); // 检查是否为空
```

**新增方法**:
- `is_empty(self) -> bool`

---

## 📊 完整功能清单

### 标准输出 (4 个)
- ✅ `println(msg: string)`
- ✅ `print(msg: string)`
- ✅ `eprintln(msg: string)`
- ✅ `eprint(msg: string)`

### 错误处理类型 (2 个)
- ✅ `Result` enum
- ✅ `Option` enum

### 字符串操作 (3 个)
- ✅ `string_length()`
- ✅ `char_at()`
- ✅ `string_equals()`

### 字符判断 (6 个)
- ✅ `is_whitespace()`
- ✅ `is_digit()`
- ✅ `is_alpha()`
- ✅ `is_alphanumeric()`
- ✅ `char_to_digit()`
- ✅ `char_equals()`

### 数学函数 (3 个)
- ✅ `abs()`
- ✅ `min()`
- ✅ `max()`

### 泛型容器 (2 个)
- ✅ `Vec<T>` struct
- ✅ `Box<T>` struct

### 断言调试 (2 个)
- ✅ `assert()`
- ✅ `panic()`

### 类型转换 (2 个)
- ✅ `bool_to_i32()`
- ✅ `i32_to_bool()`

**总计**: 24 个函数/类型，全部自动可用！

---

## 🎯 设计原则

### 包含什么

✅ **应该在 prelude 中**:
- 基础 I/O 函数
- 核心类型（Result, Option）
- 常用字符串操作
- 简单数学函数
- 断言和调试

❌ **不应该在 prelude 中**:
- 复杂字符串操作 → `stdlib/string`
- JSON 解析 → `stdlib/json`
- 文件 I/O → `stdlib/fs`
- 高级数据结构 → `stdlib/collections`

### 原则

1. **最小化** - 只包含 90% 代码会用到的
2. **零依赖** - 不依赖其他模块
3. **高性能** - 简单直接的实现
4. **稳定性** - API 很少变化

---

## 💡 使用示例

### 示例 1: 字符串处理

```paw
fn validate_input(input: string) -> bool {
    let len: i32 = string_length(input);
    
    if len == 0 {
        eprintln("Error: Input is empty");
        return false;
    }
    
    // 检查第一个字符是否为字母
    let first: char = char_at(input, 0);
    if !is_alpha(first) {
        eprintln("Error: Must start with a letter");
        return false;
    }
    
    return true;
}

fn main() -> i32 {
    if validate_input("Hello123") {
        println("Valid input");
        return 0;
    }
    return 1;
}
```

---

### 示例 2: 数学计算

```paw
fn clamp(value: i32, min_val: i32, max_val: i32) -> i32 {
    let v: i32 = max(value, min_val);
    return min(v, max_val);
}

fn distance(a: i32, b: i32) -> i32 {
    return abs(a - b);
}

fn main() -> i32 {
    let clamped: i32 = clamp(150, 0, 100);  // 100
    let dist: i32 = distance(-10, 20);      // 30
    
    return clamped + dist;  // 130
}
```

---

### 示例 3: 错误处理

```paw
fn parse_number(s: string) -> Result {
    let len: i32 = string_length(s);
    
    if len == 0 {
        return Result::Err(1);
    }
    
    // 检查所有字符是否为数字
    let mut i: i32 = 0;
    loop {
        if i >= len {
            break;
        }
        
        let ch: char = char_at(s, i);
        if !is_digit(ch) {
            return Result::Err(2);
        }
        
        i += 1;
    }
    
    return Result::Ok(0);
}

fn main() -> i32 {
    let result: Result = parse_number("12345");
    
    return result is {
        Ok(v) => {
            println("Valid number");
            return 0;
        },
        Err(e) => {
            eprintln("Invalid number");
            return e;
        },
        _ => 99,
    };
}
```

---

### 示例 4: 断言和调试

```paw
fn safe_divide(a: i32, b: i32) -> i32 {
    assert(b != 0, "Division by zero");
    return a / b;
}

fn process_data(data: string) -> i32 {
    let len: i32 = string_length(data);
    
    if len == 0 {
        panic("Empty data not allowed");
    }
    
    return len;
}

fn main() -> i32 {
    let result: i32 = safe_divide(10, 2);  // 5
    
    // 这会触发 panic
    // process_data("");
    
    return result;
}
```

---

## 🔍 与 stdlib 的关系

### 层次结构

```
┌─────────────────────────────┐
│   Prelude (自动导入)         │  ← 基础功能
│   - 常用函数                 │
│   - 核心类型                 │
└─────────────────────────────┘
           ↓ 提供基础
┌─────────────────────────────┐
│   stdlib (手动导入)          │  ← 扩展功能
│   - string (高级字符串)       │
│   - json (JSON 解析)          │
│   - fs (文件系统)             │
│   - collections (数据结构)    │
└─────────────────────────────┘
```

### 功能分布

| 功能 | Prelude | stdlib |
|------|---------|--------|
| 字符串长度 | ✅ | ✅ |
| 字符串分割 | ❌ | ✅ stdlib/string |
| 字符判断 | ✅ | ✅ |
| JSON 解析 | ❌ | ✅ stdlib/json |
| 文件读写 | ❌ | ✅ stdlib/fs |
| Vec<T> 定义 | ✅ | ✅ stdlib/collections |
| Vec<T> 完整实现 | ❌ | ✅ stdlib/collections |

---

## 📈 版本历史

### v0.1.x
- 基础类型定义（Result, Option, Vec, Box）
- 标准输出函数

### v0.2.0 (当前)
- ✨ 重命名 `src/std/` → `src/prelude/`
- ✨ 新增字符串操作函数
- ✨ 新增字符判断函数
- ✨ 新增数学函数
- ✨ 新增断言和调试函数
- ✨ 新增类型转换函数
- ✨ 增强 Vec<T> 和 Box<T>

### v0.3.0 (计划)
- 可配置 prelude
- 更多数学函数
- 迭代器基础类型

---

## 🚀 性能特性

### 零开销

Prelude 中的所有函数都是：
- ✅ 内联候选（小函数）
- ✅ 零运行时开销
- ✅ 编译时展开

### 示例

```paw
let n: i32 = abs(-10);

// 编译后的 C 代码:
// int32_t n = ((-10) < 0) ? (-(-10)) : (-10);
```

**结果**: 直接内联，无函数调用开销！

---

## 📚 参考

### 其他语言的 Prelude

| 语言 | Prelude 内容 |
|------|-------------|
| Rust | 基础类型、Option、Result、Vec、println! |
| Haskell | 基础函数、列表操作、类型类 |
| PawLang | 基础类型、常用函数、I/O ✅ |

### 相关文档

- `docs/PRELUDE_EXPLANATION.md` - Prelude 详细说明
- `stdlib/string/mod.paw` - 完整字符串库
- `stdlib/collections/vec.paw` - 完整 Vec 实现

---

## ✅ 总结

### v0.2.0 Prelude 特性

- ✅ 24 个函数/类型
- ✅ 从 108 行扩展到 300+ 行
- ✅ 涵盖 90% 常见用例
- ✅ 零依赖，高性能
- ✅ 与 stdlib 协同工作

### 关键改进

1. **更清晰的位置** - `src/prelude/` 而非 `src/std/`
2. **更丰富的功能** - 从 stdlib 提取常用函数
3. **更好的文档** - 完整的使用示例
4. **更好的设计** - 明确的包含原则

---

**状态**: ✅ v0.2.0 Prelude 已完成  
**文件大小**: ~10KB  
**功能**: 24 个自动可用的函数/类型

**PawLang Prelude - Making programming easier!** 🚀

---

**作者**: PawLang 核心开发团队  
**日期**: 2025-10-12  
**版本**: v0.2.0  
**许可**: MIT

