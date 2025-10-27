# 闭包语法讨论

> 📅 **日期**: 2025-10-27  
> 🎯 **目标**: 确定 PawLang 闭包的最佳语法

---

## 1. 语法方案对比

### 方案 A: Rust 风格 `|x| expr` ⭐⭐⭐⭐⭐

```rust
// 单参数
let double = |x: i32| x * 2;

// 多参数
let add = |x: i32, y: i32| x + y;

// 无参数
let get_value = || 42;

// 块语法
let complex = |x: i32| {
    let doubled = x * 2;
    return doubled + 10;
};

// 类型推导
let numbers = [1, 2, 3];
map(numbers, |x| x * 2);  // x 类型从 numbers 推导
```

**优点**：
- ✅ 简洁、现代
- ✅ 视觉上清晰（`|` 括起参数）
- ✅ Rust 社区熟悉
- ✅ 与 PawLang 的现代定位一致

**缺点**：
- ⚠️ `|` 可能与位运算符混淆
- ⚠️ 需要 Lexer 区分 `|` (位或) 和 `|x|` (闭包)

---

### 方案 B: JavaScript 箭头函数 `x => expr` ⭐⭐⭐⭐

```rust
// 单参数（无括号）
let double = x: i32 => x * 2;

// 多参数（需要括号）
let add = (x: i32, y: i32) => x + y;

// 无参数
let get_value = () => 42;

// 块语法
let complex = (x: i32) => {
    let doubled = x * 2;
    return doubled + 10;
};
```

**优点**：
- ✅ JavaScript/TypeScript 用户熟悉
- ✅ 箭头 `=>` 语义清晰
- ✅ 单参数时可以省略括号

**缺点**：
- ⚠️ 与 match 的 `=>` 冲突（PawLang 已用于 match）
- ⚠️ 单参数和多参数语法不一致
- ⚠️ 可能与类型标注 `: i32` 混淆

**PawLang 问题**：
```rust
// 已有的 match 语法
let result = value is {
    Some(x) => x + 1,  // ❌ 与闭包的 => 冲突！
    None => 0
};
```

---

### 方案 C: 显式 `fn` 关键字 `fn(x) expr` ⭐⭐⭐

```rust
// 单参数
let double = fn(x: i32) x * 2;

// 多参数
let add = fn(x: i32, y: i32) x + y;

// 无参数
let get_value = fn() 42;

// 块语法
let complex = fn(x: i32) {
    let doubled = x * 2;
    return doubled + 10;
};
```

**优点**：
- ✅ 语义明确（这是一个函数）
- ✅ 与函数定义一致
- ✅ 不引入新符号

**缺点**：
- ⚠️ 比较冗长
- ⚠️ 与命名函数定义混淆
- ⚠️ 不够现代

---

### 方案 D: Lambda 关键字 `\x -> expr` ⭐⭐

```rust
// Haskell 风格
let double = \x: i32 -> x * 2;
let add = \x: i32, y: i32 -> x + y;

// 或 Python 风格
let double = lambda x: i32 => x * 2;
```

**优点**：
- ✅ 数学上准确（λ 演算）
- ✅ Haskell 用户熟悉

**缺点**：
- ⚠️ 需要输入 `\` 或 `lambda` 关键字
- ⚠️ 比较冗长
- ⚠️ 不够现代/简洁

---

### 方案 E: Go 风格 `func(x) expr` ⭐⭐

```rust
let double = func(x: i32) { return x * 2; };
let add = func(x: i32, y: i32) { return x + y; };
```

**优点**：
- ✅ Go 用户熟悉
- ✅ 语义明确

**缺点**：
- ⚠️ 必须用块语法，不支持单表达式
- ⚠️ 冗长
- ⚠️ 不够简洁

---

## 2. 推荐方案

### 🏆 方案 A: Rust 风格 `|x| expr`

**理由**：
1. ✅ **最简洁**：符合现代语言趋势
2. ✅ **一致性**：PawLang 已采用 Rust 风格的很多特性
   - `&T` / `&mut T` 引用
   - `T?` 可选类型
   - `match` 模式匹配
   - `Self` / `self` 
3. ✅ **社区认可**：Rust 的闭包语法广受好评
4. ✅ **视觉清晰**：`|` 符号明确标识参数
5. ✅ **扩展性好**：容易添加 `move` 等修饰符

---

## 3. 详细语法规范（方案 A）

### 3.1 基础形式

```rust
// 形式 1: 表达式闭包（最常用）
|param1: Type1, param2: Type2| -> ReturnType expression

// 形式 2: 块闭包
|param1: Type1, param2: Type2| -> ReturnType {
    // statements
    return value;
}

// 简化形式（类型可选）
|x| x + 1                    // 单表达式，类型推导
|x: i32| x + 1              // 显式参数类型
|| 42                        // 无参数
|x, y| x + y                // 多参数，类型推导
```

### 3.2 与现有语法的协调

**问题 1: `|` 与位或运算符冲突**

```rust
// 位运算
let a = x | y;           // 二元运算符

// 闭包
let f = |x| x + 1;       // 闭包参数

// 区分方法：
// - 位运算：| 两边都是表达式
// - 闭包：| 开始标识符列表，第二个 | 结束
```

**Lexer 策略**：
```cpp
// 在 Lexer 中：
if (current == '|') {
    if (peek_ahead() 是标识符 或 ')') {
        // 可能是闭包
        return PIPE;
    } else {
        // 位或运算符
        return BIT_OR;
    }
}
```

**或者统一处理**（推荐）：
```cpp
// PIPE 和 BIT_OR 都用同一个 token
// Parser 根据上下文区分
TokenType::PIPE  // 既是 | 也是闭包标记
```

---

**问题 2: 闭包调用语法**

```rust
// 定义
let add = |x, y| x + y;

// 调用（像普通函数）
let result = add(10, 20);
```

无冲突，完美！

---

**问题 3: 闭包作为参数**

```rust
// 函数签名
fn map<T, U>(arr: [T], f: fn(T) -> U) -> [U]

// 调用时传闭包
map(numbers, |x| x * 2)
```

完美！

---

### 3.3 捕获语法

```rust
// 默认：按值捕获
let x = 10;
let f = |y| x + y;  // x 被复制

// 显式移动（Phase 4）
let s = "hello";
let f = move |suffix| s + suffix;  // s 被移动

// 按引用捕获（Phase 4+）
let mut x = 0;
let f = || { x = x + 1; return x; };  // 修改外部 x
```

---

### 3.4 类型标注

```rust
// 完整标注
let f: fn(i32, i32) -> i32 = |x: i32, y: i32| -> i32 { x + y };

// 省略返回类型（从 body 推导）
let f: fn(i32, i32) -> i32 = |x: i32, y: i32| { x + y };

// 省略参数类型（从使用推导）
map([1, 2, 3], |x| x * 2);  // x: i32 从数组元素类型推导

// 完全推导（未来）
let f = |x, y| x + y;  // 需要复杂的类型推导
```

---

## 4. 实现难点和解决方案

### 4.1 Lexer 处理 `|`

**挑战**：区分位或 `|` 和闭包参数 `|x|`

**方案 1：上下文敏感（简单）**
```cpp
// Lexer 始终返回 PIPE token
// Parser 根据上下文决定含义

if (check(PIPE)) {
    // 可能是闭包
    if (lookahead_is_closure_pattern()) {
        return parseClosure();
    } else {
        // 位或运算
        return parseBinary();
    }
}
```

**方案 2：统一 token（推荐）**
```cpp
// PIPE 既可以是位或，也可以是闭包分隔符
// Parser 自然处理

// 在 primary() 中
if (match({PIPE})) {
    return parseClosure();  // 在表达式开始位置的 | 是闭包
}

// 在 binary() 中
if (match({PIPE})) {
    return parseBitOr();    // 在二元运算中的 | 是位或
}
```

---

### 4.2 Parser 歧义

**情况 1：空闭包 vs 位或**
```rust
let f = || 42;      // 闭包
let a = x || y;     // 逻辑或（PawLang 用 OR 关键字）
```
✅ 无冲突（PawLang 用 `or` 关键字表示逻辑或）

**情况 2：单参数 vs 绝对值**
```rust
let f = |x| x + 1;  // 闭包
let a = |x| + 1;    // 位或（不合法，| 是二元运算符）
```
✅ 无冲突

---

### 4.3 优先级

```rust
// 闭包优先级最低
let f = |x| x + 1 * 2;  // = |x| (x + (1 * 2))
let g = |x| if x > 0 { x } else { -x };  // OK

// 需要括号的情况
let result = (|x| x + 1)(10);  // 立即调用
```

---

## 5. 其他语言对比

| 语言 | 闭包语法 | 优点 | 缺点 |
|------|----------|------|------|
| **Rust** | `\|x\| x + 1` | 简洁、现代 | 与位或冲突（已解决）|
| **JavaScript** | `x => x + 1` | 箭头直观 | 与 match `=>` 冲突 |
| **Python** | `lambda x: x + 1` | 显式 | 冗长 |
| **C++** | `[](int x) { return x+1; }` | 灵活捕获 | 复杂 |
| **Go** | `func(x int) int { return x+1 }` | 清晰 | 冗长，无表达式形式 |
| **Swift** | `{ x in x + 1 }` | 简洁 | 需要 `in` 关键字 |
| **Kotlin** | `{ x -> x + 1 }` | 简洁 | `->` 可能冲突 |

---

## 6. 完整示例

使用方案 A (Rust 风格) 的完整示例：

```rust
// ===== 基础闭包 =====

// 表达式闭包
let double = |x: i32| x * 2;
let add = |x: i32, y: i32| x + y;

// 块闭包
let complex = |x: i32| {
    let doubled = x * 2;
    if doubled > 10 {
        return doubled;
    } else {
        return doubled + 10;
    }
};

// 无参数闭包
let get_value = || 42;
let print_hello = || { println("Hello!"); };


// ===== 捕获变量 =====

let x = 10;
let add_x = |y| x + y;           // 捕获 x
println(f"{add_x(5)}");          // 15

let factor = 2;
let multiply = |n| n * factor;   // 捕获 factor
println(f"{multiply(10)}");      // 20


// ===== 高阶函数 =====

fn map<T, U>(arr: [T], f: fn(T) -> U) -> [U] {
    let mut result = [];
    for item in arr {
        result.push(f(item));
    }
    return result;
}

let numbers = [1, 2, 3, 4, 5];
let doubled = map(numbers, |x| x * 2);
println(f"{doubled}");  // [2, 4, 6, 8, 10]


// ===== 闭包作为返回值 =====

fn make_adder(n: i32) -> fn(i32) -> i32 {
    return |x| x + n;  // 返回捕获 n 的闭包
}

let add5 = make_adder(5);
println(f"{add5(10)}");  // 15


// ===== 组合使用 =====

let numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

// 过滤 + 映射
let result = map(
    filter(numbers, |x| x % 2 == 0),  // 偶数
    |x| x * x                          // 平方
);
println(f"{result}");  // [4, 16, 36, 64, 100]


// ===== 与 f-string 结合 =====

let operations = [
    ("double", |x: i32| x * 2),
    ("square", |x: i32| x * x),
    ("plus10", |x: i32| x + 10)
];

let value = 5;
for (name, op) in operations {
    println(f"{name}({value}) = {op(value)}");
}
// 输出:
// double(5) = 10
// square(5) = 25
// plus10(5) = 15
```

---

## 7. 决策建议

### 推荐：方案 A (Rust 风格)

**理由总结**：
1. ✅ **简洁性**：最短、最现代
2. ✅ **一致性**：与 PawLang 现有 Rust 风格特性一致
3. ✅ **社区**：Rust 社区已证明这是好设计
4. ✅ **可实现性**：技术难点可解决
5. ✅ **扩展性**：易于添加 `move` 等特性

**语法定义**：
```
closure := '|' params? '|' ('->' type)? (expr | block)
params  := param (',' param)*
param   := IDENTIFIER (':' type)?
```

---

## 8. 备选方案

如果方案 A 有问题，备选方案：

### 备选 1: 混合语法

```rust
// 使用 \| 代替 |（避免冲突）
let f = \|x| x + 1;

// 或使用 fn 关键字
let f = fn |x| x + 1;
```

### 备选 2: 括号形式

```rust
// 始终用括号
let f = (|x| x + 1);
let g = (|x, y| x + y);
```

---

## 9. 需要讨论的问题

1. **是否采用方案 A (Rust 风格)?**
   - 如果是，确认无异议
   - 如果否，选择其他方案

2. **类型标注的要求？**
   - 必须显式标注：`|x: i32|`
   - 可以推导：`|x|`（实现复杂）
   
3. **是否支持 `move` 关键字？**
   - Phase 1: 不支持
   - Phase 4: 添加

4. **捕获默认方式？**
   - 按值捕获（推荐）
   - 按引用捕获
   - 自动推导（复杂）

---

**🤔 请选择您偏好的方案，或提出新的想法！**


