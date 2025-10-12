# 📝 String Module

**路径**: `stdlib/string/mod.paw`  
**版本**: v0.2.0  
**状态**: ✅ 稳定

---

## 📋 概述

字符串模块提供高级字符串操作功能。

### ⚠️ 重要提示

**基础字符串函数已在 Prelude 中**，无需 import：

```paw
// ✅ 这些函数自动可用（来自 Prelude）
let len = string_length("hello");       // 5
let ch = char_at("hello", 0);           // 'h'
let eq = string_equals("a", "a");       // true

let is_d = is_digit('5');               // true
let is_w = is_whitespace(' ');          // true
let is_a = is_alpha('A');               // true

// ❌ 不需要 import string!
```

**stdlib/string 提供的是扩展功能**：

```paw
import string;

// ✅ 这些需要 import
let builder = string::StringBuilder::new();
let num = string::parse_i32("123");
```

---

## 📦 提供的功能

### 1. StringBuilder - 字符串构建器

**用途**: 高效地构建字符串

```paw
import string;

fn build_message() -> string {
    let mut builder = string::StringBuilder::new();
    
    builder.append_string("Hello");
    builder.append_char(' ');
    builder.append_string("World");
    builder.append_char('!');
    builder.append_char('\n');
    builder.append_i32(42);
    
    return "result";  // TODO: 返回 builder 内容
}
```

**方法**:
- `new() -> StringBuilder` - 创建新的构建器
- `append_char(ch: char) -> bool` - 追加字符
- `append_string(s: string) -> bool` - 追加字符串
- `append_i32(n: i32) -> bool` - 追加整数

**限制**:
- 固定缓冲区（4096 字节）
- 超出容量返回 false

---

### 2. 字符串解析

```paw
import string;

fn parse_number(s: string) -> i32 {
    return string::parse_i32(s);
}

fn main() -> i32 {
    let num = parse_number("12345");  // 12345
    let neg = parse_number("-100");   // -100
    return num;
}
```

**函数**:
- `parse_i32(s: string) -> i32` - 解析整数

---

### 3. 字符串比较（高级）

```paw
import string;

// 检查前缀
let has_prefix = string::starts_with("hello world", "hello");  // true

// 检查后缀
let has_suffix = string::ends_with("test.txt", ".txt");  // true

// 大小写不敏感比较
let same = string::equals_ignore_case("Hello", "HELLO");  // true
```

**函数**:
- `starts_with(s: string, prefix: string) -> bool`
- `ends_with(s: string, suffix: string) -> bool`
- `equals_ignore_case(s1: string, s2: string) -> bool`

---

## 🆚 Prelude vs Stdlib

| 功能 | 位置 | 需要 import？ |
|------|------|-------------|
| `string_length()` | Prelude | ❌ 否 |
| `char_at()` | Prelude | ❌ 否 |
| `string_equals()` | Prelude | ❌ 否 |
| `is_digit()` | Prelude | ❌ 否 |
| `is_whitespace()` | Prelude | ❌ 否 |
| `is_alpha()` | Prelude | ❌ 否 |
| `char_to_digit()` | Prelude | ❌ 否 |
| **StringBuilder** | **Stdlib** | **✅ 是** |
| **parse_i32()** | **Stdlib** | **✅ 是** |
| **starts_with()** | **Stdlib** | **✅ 是** |

---

## 💡 使用场景

### 场景 1: 只需要基础操作

```paw
// ✅ 无需 import，使用 Prelude
fn count_letters(s: string) -> i32 {
    let len = string_length(s);
    let mut count: i32 = 0;
    
    let mut i: i32 = 0;
    loop {
        if i >= len { break; }
        
        let ch = char_at(s, i);
        if is_alpha(ch) {
            count += 1;
        }
        
        i += 1;
    }
    
    return count;
}
```

---

### 场景 2: 需要字符串构建

```paw
import string;  // ✅ 需要 import

fn format_list(items: [string; 3]) -> string {
    let mut builder = string::StringBuilder::new();
    
    builder.append_string("Items: ");
    builder.append_string(items[0]);
    builder.append_string(", ");
    builder.append_string(items[1]);
    builder.append_string(", ");
    builder.append_string(items[2]);
    
    return "result";
}
```

---

### 场景 3: 需要解析

```paw
import string;  // ✅ 需要 import

fn calculate(expr: string) -> i32 {
    // TODO: 实际的解析逻辑
    return string::parse_i32(expr);
}
```

---

## 🔮 未来计划

### v0.3.0

**字符串分割**:
```paw
let parts = string::split("a,b,c", ",");  // ["a", "b", "c"]
```

**字符串连接**:
```paw
let items = ["a", "b", "c"];
let result = string::join(items, ",");  // "a,b,c"
```

**字符串替换**:
```paw
let result = string::replace("hello world", "world", "PawLang");
// "hello PawLang"
```

**字符串修剪**:
```paw
let trimmed = string::trim("  hello  ");  // "hello"
let left = string::trim_left("  hello");  // "hello"
let right = string::trim_right("hello  "); // "hello"
```

---

### v0.4.0

**UTF-8 支持**:
```paw
let len = string::char_count("你好");  // 2 (字符数，非字节数)
let ch = string::char_at_utf8("你好", 0);  // '你'
```

**格式化**:
```paw
let msg = string::format("User {}: score {}", ["Alice", "95"]);
// "User Alice: score 95"
```

---

## 📚 完整 API 参考

### StringBuilder

```paw
pub type StringBuilder = struct {
    buffer: [char; 4096],
    length: i32,
    
    pub fn new() -> StringBuilder
    pub fn append_char(mut self, ch: char) -> bool
    pub fn append_string(mut self, s: string) -> bool
    pub fn append_i32(mut self, n: i32) -> bool
    pub fn clear(mut self) -> i32
    pub fn len(self) -> i32
}
```

### 字符串解析

```paw
pub fn parse_i32(s: string) -> i32
// TODO: parse_f64, parse_bool
```

### 字符串比较

```paw
pub fn starts_with(s: string, prefix: string) -> bool
pub fn ends_with(s: string, suffix: string) -> bool
pub fn equals_ignore_case(s1: string, s2: string) -> bool
pub fn contains(s: string, substr: string) -> bool
```

---

## ✅ 总结

### 记住

1. **基础操作在 Prelude** - 无需 import
2. **高级功能在 Stdlib** - 需要 import
3. **StringBuilder 用于动态构建**
4. **parse_i32 用于解析**

### 快速参考

```paw
// Prelude（自动可用）
string_length(), char_at(), string_equals()
is_digit(), is_alpha(), is_whitespace()

// Stdlib（需要 import string）
StringBuilder, parse_i32()
starts_with(), ends_with()
```

---

**维护者**: PawLang 核心开发团队  
**最后更新**: 2025-10-12  
**版本**: v0.2.0  
**许可**: MIT

