# Paw 分号规则 - 简洁明确的语句终结

## 核心原则

**可选但推荐** - 平衡简洁性和明确性

---

## 基本规则

### ✅ 不需要分号的场景（90%）

```paw
// 1. 每行一语句（最常见）
let x = 42
let y = 10
println(x + y)

// 2. 函数定义
fn add(x: int, y: int) -> int = x + 1

fn process(data: string) {
    println(data)
}

// 3. 类型定义
type Point = struct {
    x: float
    y: float
}

// 4. 块表达式
if condition {
    do_something()
}

loop for item in items {
    process(item)
}

// 5. 模式匹配
value is {
    0 -> "zero"
    _ -> "other"
}

// 6. 返回值（块的最后一个表达式）
fn get_value() -> int {
    let x = 42
    x  // 不需要分号，这是返回值
}
```

---

### ⚠️ 必须使用分号的场景

```paw
// 1. 单行多语句
let x = 1; let y = 2; let z = 3

// 2. 表达式语句在同一行
process(x); process(y); process(z)

// 3. 歧义消除
let x = complex_call()
(y + z).process()  // 如果不加分号，可能被解析为 complex_call()(y + z)

// 应该写为：
let x = complex_call();
(y + z).process()

// 或者换行：
let x = complex_call()

(y + z).process()
```

---

### 🔵 可选分号的场景（风格选择）

```paw
// 为了明确性可以加分号（不强制）
let x = very_complex_expression();
let y = another_complex_call();

// 或者不加（也OK）
let x = very_complex_expression()
let y = another_complex_call()
```

---

## 详细规则

### 规则 1: 换行符 = 语句终结

```paw
// ✅ 标准格式
let x = 42
let y = 10
let z = 20

// 等价于（加分号）
let x = 42;
let y = 10;
let z = 20;
```

### 规则 2: 续行检测

**自动续行的情况：**

```paw
// A. 运算符在行尾
let sum = a +
    b +
    c

// B. 点号在行首
let result = object
    .method1()
    .method2()

// C. 括号未闭合
let value = function(
    arg1,
    arg2,
    arg3
)

// D. 数组/块未闭合
let arr = [
    1,
    2,
    3
]

// E. 逗号在行尾
let tuple = (
    field1,
    field2,
)
```

### 规则 3: 块边界

```paw
// 块 } 后自动终结，不需要分号
if condition {
    statement
}  // 这里不需要分号

// 多个块可以连续
if a { }
if b { }
if c { }

// 或者在同一行（不推荐，但合法）
if a { } if b { } if c { }
```

### 规则 4: 表达式语句

```paw
// 表达式作为语句时，换行终结
process(data)
another_process()

// 同一行需要分号
process(data); another_process()
```

---

## 🎨 风格指南

### 推荐风格（pawfmt）

```paw
// ✅ 标准格式：不使用分号
import std.collections.Vec

pub type User = struct {
    pub id: int
    pub name: string
    
    pub fn new(name: string) -> Self {
        User { id: 0, name }
    }
}

fn main() -> int {
    let mut users = Vec.new()
    
    let user1 = User.new("Alice")
    let user2 = User.new("Bob")
    
    users.push(user1)
    users.push(user2)
    
    let result = users
        .filter(|u| !u.name.is_empty())
        .map(|u| u.name.len())
        .sum()
    
    result
}
```

### 紧凑风格（允许，但不推荐）

```paw
// 可以，但降低可读性
fn main() -> int { let x = 1; let y = 2; x + y }
```

---

## 🔍 特殊情况

### 1. 立即执行的括号

```paw
// ⚠️ 潜在歧义
let x = foo()
(bar + baz).process()

// 可能被解析为：
let x = foo()(bar + baz).process()

// ✅ 解决方案 1：加分号
let x = foo();
(bar + baz).process()

// ✅ 解决方案 2：加空行
let x = foo()

(bar + baz).process()
```

### 2. 数组索引

```paw
// ⚠️ 潜在歧义
let x = arr
[0]

// 可能被解析为新数组？

// ✅ 解决方案：保持在同一行
let x = arr[0]

// 或者用分号
let x = arr;
[0]  // 这是新数组
```

### 3. 链式调用

```paw
// ✅ 推荐：. 在行首
let result = data
    .filter(|x| x > 0)
    .map(|x| x * 2)
    .sum()

// ⚠️ 避免：可能产生歧义
let x = obj.
    method()  // 不推荐

// ✅ 或者：全部在同一行
let x = obj.method().another().final()
```

---

## 📝 格式化工具行为（pawfmt）

### 自动处理

```paw
// 输入（不规范）：
let x = 42;let y=10;let z =20;

// 输出（格式化后）：
let x = 42
let y = 10
let z = 20
```

### 保持的格式

```paw
// 输入：
let x = 1; let y = 2; let z = 3  // 单行多语句

// 输出（保持）：
let x = 1; let y = 2; let z = 3
```

### 链式调用对齐

```paw
// 输入：
let result = data.filter(|x| x > 0).map(|x| x * 2).sum()

// 输出（格式化）：
let result = data
    .filter(|x| x > 0)
    .map(|x| x * 2)
    .sum()
```

---

## 🎯 最佳实践总结

### DO（推荐）

```paw
✅ 每行一语句
let x = 42
let y = 10

✅ 链式调用分行
let result = data
    .filter(predicate)
    .map(transform)

✅ 块表达式不加分号
if condition { value }

✅ 换行保持可读性
let user = User.new("Alice")
    .with_email("alice@example.com")
    .build()
```

### DON'T（避免）

```paw
❌ 单行塞太多语句
let x = 1; let y = 2; let z = 3; println(x + y + z)

❌ 不必要的分号
let x = 42;
let y = 10;

❌ 混乱的续行
let x = a
+ b
+ c  // 应该对齐

❌ 歧义的换行
let x = foo()
(bar).method()  // 容易产生歧义
```

---

## 总结

**Paw 的分号规则：**

1. **默认不需要** - 换行符终结语句
2. **必要时使用** - 单行多语句
3. **自动识别续行** - 智能解析
4. **工具辅助** - pawfmt 自动格式化

**平衡点：**
- 90% 代码不需要分号（简洁）
- 10% 代码需要分号（明确）
- 格式化工具保证一致性

**与其他语言对比：**
- Rust: 必须分号（严格）
- Go: 自动分号插入（智能）
- JavaScript: 可选分号（混乱）
- **Paw: 智能识别 + 可选分号（平衡）** ⭐

