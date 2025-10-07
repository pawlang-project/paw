# Paw 语法问题分析与解决方案

## 🔍 潜在问题识别

### 1. 单行代码歧义 ⚠️

**问题：**
```paw
// 这样写会产生歧义
let x = 42 let y = 10
if x > 0 { println(x) } if y > 0 { println(y) }
```

**解析器困惑：**
- 在哪里结束第一个 `let`？
- `if` 后面的块属于哪个语句？

---

### 2. 表达式边界问题 ⚠️

**问题：**
```paw
// 这是两个表达式还是一个？
f(x) g(y)

// 这是什么？
a + b c + d
```

---

### 3. 块表达式歧义 ⚠️

**问题：**
```paw
// 这是函数调用还是块？
process { x: 10, y: 20 }

// 这是结构体还是块？
Point { x: 10, y: 20 } + 1
```

---

### 4. 链式调用歧义 ⚠️

**问题：**
```paw
// 换行后是否继续？
user
    .name
    
user
.name  // 这是继续链式调用还是新语句？
```

---

### 5. 条件表达式嵌套 ⚠️

**问题：**
```paw
// 哪个 else 属于哪个 if？
if a { if b { 1 } else { 2 } } else { 3 }
```

---

## ✅ 解决方案

### 方案 1: 分号作为语句终结符（推荐）

**规则：** 语句必须用分号或换行符结束

```paw
// ✅ 清晰 - 用分号
let x = 42;
let y = 10;

// ✅ 清晰 - 用换行
let x = 42
let y = 10

// ✅ 单行多语句 - 必须用分号
let x = 42; let y = 10; println(x + y)

// ❌ 错误 - 单行多语句无分号
let x = 42 let y = 10  // 解析错误
```

**块表达式的特殊规则：**
```paw
// 块后不需要分号（块本身是明确边界）
if x > 0 { println(x) }  // ✅ OK
if x > 0 { println(x) } if y > 0 { println(y) }  // ✅ 分号分隔更清晰

// 推荐：一行一个语句
if x > 0 {
    println(x)
}
```

---

### 方案 2: 明确的表达式结束规则

**自动分号插入（ASI）规则：**

1. **换行符 = 隐式分号**（在某些情况下）
   ```paw
   let x = 42        // 换行 = 分号
   let y = 10        // 换行 = 分号
   ```

2. **块结束 = 语句结束**
   ```paw
   if x > 0 { do_something() }  // } 后自动结束
   ```

3. **需要显式分号的场景：**
   ```paw
   // 单行多语句
   let x = 1; let y = 2; let z = 3
   
   // 表达式语句在同一行
   process(x); process(y)
   ```

---

### 方案 3: 换行符敏感（推荐）⭐

**设计原则：** 换行符有意义，但允许显式续行

```paw
// ✅ 清晰 - 每行一个语句
let x = 42
let y = 10
println(x + y)

// ✅ 链式调用 - 运算符/点号开头继续
let result = numbers
    .filter(|x| x > 0)
    .map(|x| x * 2)
    .sum()

let value = x
    + y
    + z

// ✅ 长表达式 - 括号/块内可以换行
let sum = add(
    very_long_expression,
    another_long_expression
)

// ❌ 歧义 - 需要显式分号或换行
let x = 42 let y = 10  // 错误
```

**续行规则：**

行尾是这些符号时，自动续行：
- 运算符：`+`, `-`, `*`, `/`, `&&`, `||`
- 点号：`.`
- 逗号：`,`
- 开括号：`(`, `{`, `[`

行首是这些符号时，与上一行连接：
- 点号：`.`
- 某些运算符

```paw
// ✅ 自动续行
let x = 1 +
    2 +
    3

let result = object
    .method1()
    .method2()

// ✅ 括号内换行
let sum = add(
    a,
    b,
    c
)

// ❌ 需要续行符或重新组织
let x = 42
+ 10  // 这会被解析为新语句！
```

---

### 方案 4: 混合策略（最终推荐）⭐⭐⭐

**核心规则：**

1. **换行符通常终结语句**
2. **分号显式终结语句**（可选但推荐用于单行多语句）
3. **某些情况自动续行**（智能识别）
4. **块表达式自成边界**

**具体规则：**

```paw
// 规则 1: 换行 = 语句结束
let x = 42
let y = 10

// 规则 2: 显式分号（单行多语句）
let x = 42; let y = 10; println(x)

// 规则 3: 自动续行（运算符）
let sum = a
    + b
    + c  // + 在行首，自动续行

let result = object
    .method1()
    .method2()  // . 在行首，自动续行

// 规则 4: 括号/块内自由换行
let sum = add(
    very_long_argument1,
    very_long_argument2,
    very_long_argument3
)

if condition {
    statement1
    statement2
}

// 规则 5: 逗号续行
let tuple = (
    field1,
    field2,
    field3,
)
```

---

## 📋 完整的语法歧义消除规则

### 1. 语句分隔

```paw
// ✅ 推荐：一行一语句（自动分隔）
let x = 42
let y = 10

// ✅ 允许：显式分号
let x = 42; let y = 10

// ❌ 错误：单行多语句无分隔
let x = 42 let y = 10
```

### 2. 表达式续行

```paw
// ✅ 运算符续行
let result = a
    + b
    + c

// ✅ 方法链续行
let data = items
    .filter(|x| x > 0)
    .map(|x| x * 2)

// ⚠️ 注意：运算符必须在行首或行尾
let x = a +  // ✅ 行尾运算符
    b

let x = a
    + b      // ✅ 行首运算符

let x = a
b + c        // ❌ 会被解析为两个表达式
```

### 3. 块边界明确

```paw
// ✅ 块自成边界
if condition {
    statement
}  // } 后自动结束，不需要分号

// ✅ 连续块
if a { } if b { }  // 可以，但不推荐

// ✅ 推荐：分行写
if a {
    process_a()
}
if b {
    process_b()
}
```

### 4. 函数调用消歧

```paw
// ✅ 明确的函数调用
function(arg1, arg2)
function { field: value }  // 单参数结构体

// ✅ 方法调用
object.method()
object.method { field: value }

// ⚠️ 避免歧义
Point { x: 10, y: 20 }  // 结构体字面量
f{ x: 10, y: 20 }       // 函数调用（需要空格或明确）
```

---

## 🎯 最佳实践建议

### 1. 格式化规范

```paw
// ✅ 推荐：每行一语句
let x = 42
let y = 10
let z = 20

// ✅ 接受：块在同一行（简短时）
if x > 0 { return 1 }

// ✅ 推荐：复杂块分行
if condition {
    statement1
    statement2
}

// ✅ 链式调用对齐
let result = data
    .filter(|x| x > 0)
    .map(|x| x * 2)
    .sum()
```

### 2. 分号使用指南

**必须使用分号：**
```paw
// 单行多语句
let x = 1; let y = 2; let z = 3

// 表达式语句后接其他语句（同一行）
process(); another_process()
```

**不需要分号：**
```paw
// 每行一语句
let x = 42
let y = 10

// 块表达式
if condition { value }

// 函数定义
fn add(x: int) -> int = x + 1
```

**可选分号：**
```paw
// 为了明确性可以加
let x = complex_expression();
```

---

## 🔧 解决方案总结

### 采用的策略

```paw
1. 换行符敏感（默认）
   - 每行一语句
   - 自动识别续行

2. 分号可选但推荐
   - 单行多语句必须用分号
   - 其他场景可选

3. 智能续行
   - 运算符在行首/行尾
   - 方法链（.）
   - 括号内自由换行

4. 块边界明确
   - { } 自成边界
   - 不需要额外分隔
```

---

## 📝 完整的语法规则

### 语句终结规则

| 场景 | 规则 | 示例 |
|------|------|------|
| 每行一语句 | 换行终结 | `let x = 42\nlet y = 10` |
| 单行多语句 | 分号必需 | `let x = 1; let y = 2` |
| 块表达式 | `}` 自动终结 | `if x { } if y { }` |
| 表达式语句 | 换行或分号 | `process()\nanother()` |

### 续行规则

| 场景 | 规则 | 示例 |
|------|------|------|
| 运算符 | 行尾或行首 | `a +\n    b` 或 `a\n    + b` |
| 方法链 | `.` 在行首 | `obj\n    .method()` |
| 函数调用 | `(` 未闭合 | `f(\n    arg\n)` |
| 数组/块 | `{[` 未闭合 | `[\n    1,\n    2\n]` |

---

## 🎨 推荐的代码风格

```paw
// ===== 变量声明 =====
let x = 42
let y = 10
let z = x + y

// ===== 函数定义 =====
fn add(x: int, y: int) -> int = x + y

fn complex_function(data: string) -> Result<int, Error> {
    let parsed = parse(data)?
    let processed = process(parsed)
    Ok(processed)
}

// ===== 类型定义 =====
pub type User = struct {
    pub id: int
    pub name: string
    
    pub fn new(name: string) -> Self {
        User { id: 0, name }
    }
}

// ===== 控制流 =====
// 简单条件 - 可以单行
if x > 0 { return 1 }

// 复杂条件 - 分行
if condition {
    let result = compute()
    process(result)
    return result
}

// ===== 循环 =====
loop for item in items {
    process(item)
}

// ===== 链式调用 =====
let result = numbers
    .filter(|x| x > 0)
    .map(|x| x * 2)
    .sum()

// ===== 模式匹配 =====
value is {
    0 -> "zero"
    1..10 -> "small"
    _ -> "large"
}
```

---

## 🚨 其他潜在问题

### 3. 泛型语法歧义

**问题：**
```paw
// < 是小于还是泛型开始？
let x = a < b > c
let x = Vec<int>
```

**解决方案：**
- 泛型后必须跟标识符、`{` 或 `(`
- 小于号后跟表达式
- 编译器根据上下文区分

```paw
// ✅ 明确的泛型
Vec<int>
Option<User>
fn process<T>(item: T)

// ✅ 明确的比较
if a < b { }
let x = (a < b) and (c > d)
```

---

### 4. 结构体字面量 vs 块

**问题：**
```paw
// 这是什么？
Point { x: 10, y: 20 }
```

**解决方案：**
- 类型名开头 + `{}` = 结构体字面量
- 单独的 `{}` = 块表达式

```paw
// ✅ 结构体字面量（类型名开头）
Point { x: 10, y: 20 }
User { name: "Alice", email: "alice@example.com" }

// ✅ 块表达式（无类型名）
{
    let x = 10
    x + 20
}

// ✅ 函数调用+结构体
process(Point { x: 10, y: 20 })
```

---

### 5. 字符串插值歧义

**问题：**
```paw
// $ 何时是插值，何时是普通字符？
let x = "$100"  // 这是插值还是字面量？
```

**解决方案：**
- `$identifier` = 插值
- `${expression}` = 表达式插值
- `$$` = 转义为字面 `$`

```paw
// ✅ 插值
let name = "Alice"
let msg = "Hello, $name"          // 插值
let calc = "Result: ${2 + 2}"     // 表达式插值

// ✅ 字面量 $
let price = "$$100"               // 转义 -> "$100"
let regex = r"$\d+"               // 原始字符串，$ 是字面量
```

---

### 6. 闭包 vs 块

**问题：**
```paw
// 这是闭包还是块？
|x| { x + 1 }
```

**解决方案：**
- `|params|` 开头 = 闭包
- 单独 `{}` = 块

```paw
// ✅ 闭包
let f = |x| x + 1
let f = |x| { x + 1 }
let f = |x, y| { x + y }

// ✅ 块
let result = {
    let x = 10
    x + 20
}
```

---

## 📜 最终语法规则（正式版）

### 1. 语句终结

```paw
规则：
  - 换行符终结语句（默认）
  - 分号显式终结语句（推荐用于单行多语句）
  - 块 } 后自动终结

示例：
  let x = 42          // 换行终结
  let y = 10; let z = 20  // 分号终结
  if x { }            // } 终结
```

### 2. 续行识别

```paw
规则：
  - 运算符在行首或行尾 → 续行
  - . 在行首 → 续行
  - (, {, [ 未闭合 → 续行
  - 逗号在行尾 → 续行

示例：
  let x = a +         // 行尾运算符
      b
  
  let y = obj         // . 在行首
      .method()
  
  let z = func(       // 括号未闭合
      arg
  )
```

### 3. 歧义消除

```paw
规则：
  - 类型名 + {} = 结构体字面量
  - |params| = 闭包
  - < 后跟类型 = 泛型
  - $identifier/${expr} = 字符串插值

示例：
  Point { x: 1 }      // 结构体
  |x| x + 1           // 闭包
  Vec<int>            // 泛型
  "Hello, $name"      // 插值
```

---

## 🎯 推荐的格式化规则

### 基本原则

1. **一行一语句**（主要原则）
2. **分号仅用于单行多语句**
3. **链式调用对齐**
4. **块使用 4 空格缩进**

### 示例

```paw
// ✅ 好的风格
import std.collections.Vec

pub type User = struct {
    pub id: int
    pub name: string
    
    pub fn new(name: string) -> Self {
        User { id: 0, name }
    }
    
    pub fn process(mut self, data: string) -> Result<(), Error> {
        let parsed = parse(data)?
        self.apply(parsed)
        Ok(())
    }
}

fn main() -> int {
    let mut users = Vec.new()
    
    let user = User.new("Alice")
    users.push(user)
    
    let result = users
        .filter(|u| !u.name.is_empty())
        .map(|u| u.name.len())
        .sum()
    
    result
}
```

---

## 🔧 解析器实现建议

### 词法分析阶段

```
识别 token：
  - 关键字
  - 标识符
  - 运算符
  - 换行符（作为特殊 token）
  - 分号
```

### 语法分析阶段

```
规则：
  1. 读取语句直到遇到：
     - 换行符（且不是续行情况）
     - 分号
     - 块结束 }

  2. 续行检测：
     - 前一个 token 是运算符/逗号/点号
     - 括号/块未闭合
     - 下一行以 . 或运算符开始

  3. 歧义处理：
     - 向前查看（lookahead）
     - 上下文感知
```

---

## 📊 问题优先级

| 问题 | 严重性 | 解决方案 | 状态 |
|------|--------|---------|------|
| 单行多语句歧义 | 高 | 要求分号 | ✅ 已解决 |
| 表达式边界 | 高 | 换行符敏感 | ✅ 已解决 |
| 链式调用续行 | 中 | 智能识别 | ✅ 已解决 |
| 泛型 vs 比较 | 中 | 上下文区分 | ✅ 已解决 |
| 结构体 vs 块 | 低 | 类型名识别 | ✅ 已解决 |
| 字符串插值 | 低 | `$$` 转义 | ✅ 已解决 |

---

## 📖 更新的语法规范

### 分号规则（新增）

```paw
// 可选分号规则：
//   1. 每行一语句：不需要分号
//   2. 单行多语句：必须用分号
//   3. 块表达式后：不需要分号

// 示例：
let x = 42          // ✅ 不需要分号
let y = 10          // ✅ 不需要分号

let x = 42; let y = 10  // ✅ 单行需要分号

if x > 0 { return 1 }   // ✅ 块后不需要分号

// 可以加分号（不强制，但可以提高清晰度）
let x = complex_expression();  // ✅ 可选分号
```

---

## ✅ 最终建议

### 语法规则

1. **换行符敏感** - 通常终结语句
2. **分号可选** - 单行多语句时必需
3. **智能续行** - 运算符/方法链自动识别
4. **块边界明确** - `}` 自动终结

### 代码风格

1. **一行一语句**（99% 的情况）
2. **链式调用分行对齐**
3. **复杂表达式使用括号**
4. **单行多语句避免使用**（除非非常简单）

### 格式化工具

建议提供 `pawfmt` 格式化工具，自动处理：
- 分号插入/删除
- 对齐和缩进
- 续行识别

---

**通过这些规则，Paw 的语法既简洁又无歧义！** ✨

