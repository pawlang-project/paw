# Paw 分号规则

## 核心原则

**与 Rust 完全一致** - 语句需要分号，返回值表达式不需要

---

## 基本规则

### ✅ 需要分号的场景（语句）

```paw
// 1. 变量声明
let x = 42;
let mut count = 0;
let y: int = 10;

// 2. 赋值语句
x = 100;
count += 1;

// 3. 函数调用（作为语句）
println("Hello");
process(data);
object.method();

// 4. 表达式语句
value + 1;
compute();
```

### ❌ 不需要分号的场景（表达式）

```paw
// 1. 函数/块的最后一个表达式（返回值）
fn add(x: int, y: int) -> int {
    let sum = x + y;
    sum  // ← 返回值，不需要分号
}

fn get_value() -> int {
    42  // ← 返回值，不需要分号
}

// 2. if/loop 块的最后一个表达式
let result = if x > 0 {
    let temp = x * 2;
    temp  // ← 返回值
} else {
    0  // ← 返回值
};

// 3. 模式匹配的分支
let category = age is {
    0..18 -> "minor"      // ← 返回值
    18..65 -> "adult"     // ← 返回值
    _ -> "senior"         // ← 返回值
};

// 4. 单表达式函数
fn double(x: int) -> int = x * 2  // ← 整个是表达式
```

---

## 详细示例

### 变量声明

```paw
// ✅ 正确
let x = 42;
let mut y = 10;
let z: int = 20;

// ❌ 错误
let x = 42  // 缺少分号
```

### 函数体

```paw
// ✅ 正确
fn calculate(x: int) -> int {
    let doubled = x * 2;      // 语句，需要分号
    let added = doubled + 10; // 语句，需要分号
    added                     // 返回值，不需要分号
}

// ✅ 也正确（如果最后是语句）
fn process(x: int) {
    let result = x * 2;
    println("Result: $result");  // 最后一个语句，需要分号
}

// ❌ 错误
fn calculate(x: int) -> int {
    let doubled = x * 2       // 缺少分号
    doubled
}
```

### 块表达式

```paw
// ✅ 正确
let result = {
    let x = 10;
    let y = 20;
    x + y  // 块的返回值，不需要分号
};

// ✅ if 表达式
let value = if condition {
    let temp = compute();
    temp * 2  // 返回值
} else {
    0  // 返回值
};

// ✅ loop 返回值
let result = loop {
    let x = get_next();
    if x > 100 {
        break x;  // break 后需要分号
    }
};
```

### 模式匹配

```paw
// ✅ 正确
let description = value is {
    0 -> {
        println("Zero");
        "zero"  // 返回值
    }
    1..10 -> "small"    // 简单返回值
    _ -> "large"        // 简单返回值
};

// 注意：整个 is 表达式后需要分号（如果是语句）
value is {
    Some(x) -> println("Got $x")
    None -> println("Nothing")
};  // 这里需要分号
```

### 方法定义

```paw
type Point = struct {
    x: float
    y: float
    
    fn distance(self) -> float {
        let dx = self.x;
        let dy = self.y;
        sqrt(dx * dx + dy * dy)  // 返回值
    }
    
    fn move(mut self, dx: float, dy: float) {
        self.x += dx;  // 语句
        self.y += dy;  // 最后一个语句也需要分号
    }
}
```

---

## 🎯 规则总结

| 情况 | 需要分号 | 示例 |
|------|---------|------|
| 变量声明 | ✅ | `let x = 42;` |
| 赋值语句 | ✅ | `x = 100;` |
| 函数调用（语句） | ✅ | `println("hi");` |
| 表达式语句 | ✅ | `process();` |
| 块/函数返回值 | ❌ | `fn f() -> int { 42 }` |
| if/loop 表达式返回值 | ❌ | `if x { 1 } else { 0 }` |
| 模式分支返回值 | ❌ | `0 -> "zero"` |
| break 语句 | ✅ | `break value;` |
| return 语句 | ✅ | `return value;` |

---

## 📖 完整示例

### 示例 1: 基本函数

```paw
fn factorial(n: int) -> int {
    if n <= 1 {
        1  // 返回值，不需要分号
    } else {
        n * factorial(n - 1)  // 返回值
    }
}

fn main() -> int {
    let result = factorial(5);
    println("5! = $result");
    0  // main 的返回值
}
```

### 示例 2: 复杂逻辑

```paw
fn process_user(user: User) -> Result<(), Error> {
    // 所有语句都需要分号
    user.validate()?;
    
    let mut updated = user;
    updated.normalize();
    
    save_to_database(updated)?;
    
    // 最后的返回值不需要分号
    Ok(())
}
```

### 示例 3: 循环和迭代

```paw
fn sum_evens(numbers: [int]) -> int {
    let mut sum = 0;
    
    loop for num in numbers {
        if num % 2 == 0 {
            sum += num;  // 语句，需要分号
        }
    }
    
    sum  // 返回值，不需要分号
}
```

### 示例 4: 模式匹配

```paw
fn describe(color: Color) -> string {
    let description = color is {
        Red -> "红色"        // 返回值
        Green -> {
            let msg = "绿色";
            msg  // 块的返回值
        }
        RGB(r, g, b) -> "RGB($r, $g, $b)"
    };
    
    description  // 函数返回值
}
```

---

## 🎨 代码风格

### 推荐格式

```paw
// ✅ 清晰的格式
fn calculate(x: int, y: int) -> int {
    let sum = x + y;
    let doubled = sum * 2;
    let result = doubled + 10;
    result
}

// ✅ 也可以（更紧凑）
fn calculate(x: int, y: int) -> int {
    let sum = x + y;
    sum * 2 + 10
}

// ✅ 单表达式（最简洁）
fn calculate(x: int, y: int) -> int = (x + y) * 2 + 10
```

---

## ⚠️ 常见错误

### 错误 1: 返回值加分号

```paw
// ❌ 错误
fn add(x: int, y: int) -> int {
    x + y;  // 加了分号，返回 ()，类型不匹配
}

// ✅ 正确
fn add(x: int, y: int) -> int {
    x + y  // 不加分号，这是返回值
}
```

### 错误 2: 语句缺少分号

```paw
// ❌ 错误
fn process() {
    let x = 42
    let y = 10  // 缺少分号，解析错误
    println(x + y)
}

// ✅ 正确
fn process() {
    let x = 42;
    let y = 10;
    println(x + y);
}
```

### 错误 3: break/return 缺少分号

```paw
// ❌ 错误
loop {
    if should_stop {
        break  // 缺少分号
    }
}

// ✅ 正确
loop {
    if should_stop {
        break;
    }
}

// ✅ break 带值
loop {
    if found {
        break value;  // 需要分号
    }
}
```

---

## 🔄 与 Rust 的对比

| 场景 | Rust | Paw | 说明 |
|------|------|-----|------|
| 变量声明 | `let x = 42;` | `let x = 42;` | ✅ 相同 |
| 函数返回 | `x + y` | `x + y` | ✅ 相同 |
| 表达式语句 | `process();` | `process();` | ✅ 相同 |
| if 表达式 | `if x { 1 } else { 0 }` | `if x { 1 } else { 0 }` | ✅ 相同 |
| 块返回值 | `{ ...; value }` | `{ ...; value }` | ✅ 相同 |

**完全兼容 Rust 的分号规则！** ✅

---

## 📋 检查清单

### 编写代码时

- [ ] 变量声明后加分号
- [ ] 表达式语句后加分号
- [ ] 函数返回值不加分号
- [ ] 块返回值不加分号
- [ ] break/return 后加分号

### 审查代码时

- [ ] 所有语句都有分号？
- [ ] 返回值没有分号？
- [ ] 没有多余的分号？

---

## ✨ 总结

**Paw 采用 Rust 风格的分号规则：**

```
语句 → 需要分号
返回值表达式 → 不需要分号
```

**优势：**
1. ✅ 完全无歧义
2. ✅ 与 Rust 一致（降低学习成本）
3. ✅ 明确的语句边界
4. ✅ 清晰的表达式 vs 语句区分

**示例：**
```paw
fn example() -> int {
    let x = 10;        // 语句，需要分号
    let y = 20;        // 语句，需要分号
    x + y              // 返回值，不需要分号
}
```

**简单规则：最后要返回的值不加分号，其他都加！** 🎯✨
