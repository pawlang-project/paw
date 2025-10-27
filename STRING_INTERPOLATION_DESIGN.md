# 字符串插值（String Interpolation）设计文档

> 📊 **版本**: v1.0  
> 📅 **创建日期**: 2025-10-27  
> 🎯 **目标**: 实现 f"..." 字符串插值语法

---

## 1. 功能概述

### 1.1 语法设计

**基础语法**：
```rust
let name = "Alice";
let age = 30;
let msg = f"Hello, {name}! You are {age} years old.";
// 结果: "Hello, Alice! You are 30 years old."
```

**支持的特性**：
1. ✅ 变量插值：`{name}`
2. ✅ 表达式插值：`{1 + 2}`
3. ✅ 方法调用：`{point.to_string()}`
4. ✅ 字段访问：`{person.name}`
5. ✅ 嵌套调用：`{arr[i]}`

---

## 2. 语法示例

### 2.1 基础用法

```rust
// 变量插值
let x = 42;
println(f"The answer is {x}");
// 输出: The answer is 42

// 多个变量
let name = "Bob";
let score = 95;
println(f"{name} scored {score} points");
// 输出: Bob scored 95 points
```

### 2.2 表达式插值

```rust
// 算术表达式
let a = 10;
let b = 20;
println(f"Sum: {a + b}, Product: {a * b}");
// 输出: Sum: 30, Product: 200

// 函数调用
fn double(x: i32) -> i32 { return x * 2; }
println(f"Result: {double(21)}");
// 输出: Result: 42
```

### 2.3 复杂插值

```rust
// 方法调用
type Point = struct {
    x: i32,
    y: i32,
    
    fn to_string(self) -> string {
        return f"({self.x}, {self.y})";  // 嵌套使用
    }
}

let p = Point { x: 10, y: 20 };
println(f"Point: {p.to_string()}");
// 输出: Point: (10, 20)

// 数组索引
let arr = [1, 2, 3];
println(f"First: {arr[0]}, Last: {arr[2]}");
// 输出: First: 1, Last: 3
```

### 2.4 转义

```rust
// 字面量花括号
let msg = f"Use {{curly braces}} for literal";
// 输出: Use {curly braces} for literal

// 混合
let x = 42;
let msg = f"Value is {x}, not {{x}}";
// 输出: Value is 42, not {x}
```

---

## 3. 实现方案

### 3.1 词法分析（Lexer）

**新增 Token 类型**：
```cpp
enum class TokenType {
    // ... 现有 ...
    F_STRING,      // f"..."
};
```

**识别 f-string**：
```cpp
Token Lexer::string() {
    // 检查是否是 f-string
    if (previous() == 'f') {
        return fstring();  // 调用 f-string 处理
    }
    
    // 普通字符串
    // ...
}

Token Lexer::fstring() {
    std::string content;
    std::vector<std::string> parts;      // 字符串片段
    std::vector<std::string> expressions; // 插值表达式
    
    while (!isAtEnd() && peek() != '"') {
        if (peek() == '{') {
            if (peekNext() == '{') {
                // {{  -> 字面量 {
                content += '{';
                advance(); advance();
            } else {
                // 表达式开始
                parts.push_back(content);
                content = "";
                
                advance();  // 跳过 {
                std::string expr;
                while (peek() != '}') {
                    expr += advance();
                }
                advance();  // 跳过 }
                
                expressions.push_back(expr);
            }
        } else {
            content += advance();
        }
    }
    
    parts.push_back(content);
    
    // 返回特殊的 F_STRING token
    // value 中编码 parts 和 expressions
    return Token(TokenType::F_STRING, encodeInterpolation(parts, expressions), ...);
}
```

---

### 3.2 语法分析（Parser）

**方案 A：在 Lexer 中解析表达式**（简单）

- Lexer 直接解析 `{expr}` 中的表达式
- 返回已分解的字符串和表达式列表

**方案 B：在 Parser 中处理**（灵活）

- Lexer 只识别 f-string 边界
- Parser 解析插值表达式
- 更符合编译原理

**推荐方案 B**。

**AST 节点**：
```cpp
// ast.h
struct FStringExpr : public Expr {
    std::vector<std::string> parts;        // 字符串片段
    std::vector<ExprPtr> expressions;      // 插值表达式
    
    FStringExpr(std::vector<std::string> p, std::vector<ExprPtr> e)
        : Expr(Kind::FString), parts(std::move(p)), expressions(std::move(e)) {}
};
```

**Parser 逻辑**：
```cpp
ExprPtr Parser::fstring() {
    Token token = previous();  // F_STRING token
    
    // 解析 token.value 中的 parts 和 expression strings
    auto [parts, expr_strings] = decodeInterpolation(token.value);
    
    // 解析每个表达式字符串
    std::vector<ExprPtr> expressions;
    for (const auto& expr_str : expr_strings) {
        // 创建临时 lexer 和 parser 解析表达式
        Lexer expr_lexer(expr_str, filename_);
        auto expr_tokens = expr_lexer.tokenize();
        
        Parser expr_parser(expr_tokens, diag_engine_, filename_);
        auto expr = expr_parser.expression();
        expressions.push_back(std::move(expr));
    }
    
    return std::make_unique<FStringExpr>(std::move(parts), std::move(expressions));
}
```

---

### 3.3 代码生成（CodeGen）

**转换为字符串拼接**：

```cpp
llvm::Value* CodeGenerator::generateFStringExpr(const FStringExpr* expr) {
    // f"Hello, {name}! Age: {age}" 
    // 转换为：
    // "Hello, " + to_string(name) + "! Age: " + to_string(age)
    
    llvm::Value* result = nullptr;
    
    for (size_t i = 0; i < expr->parts.size(); ++i) {
        // 添加字符串片段
        if (!expr->parts[i].empty()) {
            llvm::Value* part = createStringConstant(expr->parts[i]);
            result = result ? concatenateStrings(result, part) : part;
        }
        
        // 添加表达式（如果有）
        if (i < expr->expressions.size()) {
            llvm::Value* expr_val = generateExpr(expr->expressions[i].get());
            
            // 转换为字符串
            llvm::Value* str_val = convertToString(expr_val, 
                                                    expr->expressions[i]->inferred_type);
            
            result = result ? concatenateStrings(result, str_val) : str_val;
        }
    }
    
    return result ? result : createStringConstant("");
}

// 类型转换为字符串
llvm::Value* CodeGenerator::convertToString(llvm::Value* value, Type* type) {
    // i32 -> string
    if (isPrimitiveType(type, PrimitiveType::I32)) {
        return callBuiltin("int_to_string", {value});
    }
    
    // f64 -> string
    if (isPrimitiveType(type, PrimitiveType::F64)) {
        return callBuiltin("float_to_string", {value});
    }
    
    // bool -> string
    if (isPrimitiveType(type, PrimitiveType::BOOL)) {
        return builder_->CreateSelect(value,
            createStringConstant("true"),
            createStringConstant("false"));
    }
    
    // string -> string（不变）
    if (isStringType(type)) {
        return value;
    }
    
    // char -> string
    if (isPrimitiveType(type, PrimitiveType::CHAR)) {
        return callBuiltin("char_to_string", {value});
    }
    
    // 其他类型：调用 to_string() 方法（如果有）
    // 或使用默认表示
    return createStringConstant("<value>");
}
```

---

## 4. 需要的辅助函数

### 4.1 类型转字符串

```cpp
// builtins.cpp
void Builtins::registerStringConversions() {
    // int_to_string
    registerFunction("int_to_string", 
        llvm::FunctionType::get(string_type, {i32_type}, false),
        [](CodeGenerator* cg, const std::vector<llvm::Value*>& args) {
            // 调用 snprintf 或类似函数
            // 或直接生成 LLVM IR
        });
    
    // float_to_string
    // char_to_string
    // bool_to_string（或直接在 CodeGen 中处理）
}
```

---

## 5. 实施计划

### 阶段 1：基础实现（1周）

- [ ] 更新 TokenType（F_STRING）
- [ ] Lexer 识别 f"..." 语法
- [ ] Lexer 分解字符串和表达式
- [ ] 测试 Lexer

### 阶段 2：Parser 集成（3-4天）

- [ ] 添加 FStringExpr AST 节点
- [ ] Parser 解析插值表达式
- [ ] 处理转义（{{, }}）
- [ ] 测试 Parser

### 阶段 3：CodeGen 实现（3-4天）

- [ ] 实现 generateFStringExpr
- [ ] 实现 convertToString
- [ ] 添加辅助 builtin 函数
- [ ] 测试代码生成

### 阶段 4：测试和文档（2-3天）

- [ ] 创建测试用例
- [ ] 更新文档
- [ ] 提交到 Git

**总计**：1.5-2周

---

## 6. 测试用例

```rust
// test_string_interpolation.paw

fn main() -> i32 {
    // 基础变量
    let name = "Alice";
    println(f"Hello, {name}!");
    // 预期: Hello, Alice!
    
    // 多个变量
    let x = 10;
    let y = 20;
    println(f"x={x}, y={y}, sum={x+y}");
    // 预期: x=10, y=20, sum=30
    
    // 不同类型
    let i: i32 = 42;
    let f: f64 = 3.14;
    let b: bool = true;
    let c: char = 'A';
    println(f"i={i}, f={f}, b={b}, c={c}");
    // 预期: i=42, f=3.14, b=true, c=A
    
    // 表达式
    println(f"2 + 3 = {2 + 3}");
    // 预期: 2 + 3 = 5
    
    // 方法调用
    type Point = struct {
        x: i32,
        y: i32,
        fn display(self) -> string {
            return f"({self.x}, {self.y})";
        }
    }
    
    let p = Point { x: 1, y: 2 };
    println(f"Point: {p.display()}");
    // 预期: Point: (1, 2)
    
    // 转义
    println(f"Use {{braces}} for literal");
    // 预期: Use {braces} for literal
    
    return 0;
}
```

---

## 7. 性能考虑

### 7.1 编译时优化

**常量插值**：
```rust
let msg = f"Hello, {42}";
// 优化为编译时常量
// let msg = "Hello, 42";
```

**内联小函数**：
```rust
fn get_name() -> string { return "Alice"; }
let msg = f"Hello, {get_name()}";
// 如果 get_name 很小，内联后可能编译时求值
```

### 7.2 运行时性能

**避免多次分配**：
```cpp
// 预估最终字符串大小
size_t total_size = 0;
for (const auto& part : parts) {
    total_size += part.size();
}
for (const auto& expr : expressions) {
    total_size += estimated_expr_size;  // 估计
}

// 预分配
llvm::Value* buffer = allocateString(total_size);
// 然后拼接
```

---

## 8. 与其他语言对比

| 语言 | 语法 | 支持 |
|------|------|------|
| Python | `f"Hello, {name}"` | ✅ |
| JavaScript | `` `Hello, ${name}` `` | ✅ |
| Swift | `"Hello, \(name)"` | ✅ |
| C# | `$"Hello, {name}"` | ✅ |
| Rust | `format!("Hello, {}", name)` | ✅ (宏) |
| Go | `fmt.Sprintf("Hello, %s", name)` | ✅ (函数) |
| **PawLang** | `f"Hello, {name}"` | 🎯 实现中 |

**PawLang 选择**：
- 类似 Python 的 f-string
- 直观易用
- 支持任意表达式

---

## 9. 实现细节

### 9.1 Lexer 状态机

```
状态 0: 普通代码
  ↓ 遇到 'f"'
状态 1: f-string 内容
  ↓ 遇到 '{'
状态 2: 表达式
  ↓ 遇到 '}'
返回状态 1
  ↓ 遇到 '"'
返回状态 0
```

### 9.2 编码格式

**Token.value 编码**：
```
格式: <part_count>|part1|expr1|part2|expr2|...

示例: f"Hello, {name}! Age: {age}"
编码: 3|Hello, |name|! Age: |age|
```

---

## 10. 错误处理

**错误 1：未闭合的花括号**
```rust
let msg = f"Hello, {name";
// ❌ error: unclosed interpolation expression
//    expected '}', got end of string
```

**错误 2：空表达式**
```rust
let msg = f"Hello, {}";
// ❌ error: empty interpolation expression
```

**错误 3：无效表达式**
```rust
let msg = f"Value: {+}";
// ❌ error: invalid expression in interpolation
```

**错误 4：类型不可转换**
```rust
type Custom = struct { ... }
let c = Custom { ... };
let msg = f"Value: {c}";
// ❌ error: type 'Custom' cannot be interpolated
//    help: implement to_string() method or use {c.to_string()}
```

---

## 11. 优先级和复杂度

### 11.1 分阶段实现

**Phase 1：基础支持**（1周）
- [ ] 变量插值
- [ ] 简单表达式（+, -, *, /）
- [ ] 基础类型（i32, f64, bool, char, string）

**Phase 2：高级支持**（3-5天）
- [ ] 方法调用
- [ ] 字段访问
- [ ] 数组索引
- [ ] 嵌套插值

**Phase 3：优化和完善**（2-3天）
- [ ] 转义支持（{{, }}）
- [ ] 错误消息改进
- [ ] 性能优化

**总计**：1.5-2周

---

## 12. 优势和收益

### 12.1 用户体验提升

**之前**：
```rust
let name = "Alice";
let age = 30;
let msg = "Hello, " + name + "! You are " + age + " years old.";
// 繁琐，类型转换麻烦
```

**之后**：
```rust
let name = "Alice";
let age = 30;
let msg = f"Hello, {name}! You are {age} years old.";
// 简洁优雅
```

### 12.2 代码可读性

**之前**：
```rust
println("Point: (" + p.x + ", " + p.y + ")");
// 难以阅读
```

**之后**：
```rust
println(f"Point: ({p.x}, {p.y})");
// 一目了然
```

### 12.3 减少错误

**之前**：
```rust
// 容易忘记空格和分隔符
let msg = "Value:" + x + "Result:" + y;
// 输出: Value:10Result:20 ❌
```

**之后**：
```rust
let msg = f"Value: {x} Result: {y}";
// 输出: Value: 10 Result: 20 ✅
```

---

## 13. 实现难点

### 13.1 表达式解析

**挑战**：如何在 Lexer 中解析 `{expr}`

**解决方案**：
1. **方案 A**：Lexer 只提取表达式文本，Parser 再解析
2. **方案 B**：递归调用 Lexer/Parser
3. **方案 C**：手写简单的表达式解析器

**推荐**：方案 A（最简单）

### 13.2 类型转换

**挑战**：如何将各种类型转为字符串

**解决方案**：
```cpp
// 基础类型：内置转换
i32 -> string:  itoa / to_string
f64 -> string:  ftoa / to_string
bool -> string: "true" / "false"
char -> string: 单字符字符串

// 自定义类型：
1. 如果有 to_string() 方法，调用它
2. 如果实现了 Display 接口，调用 display()
3. 否则，编译错误或默认表示
```

---

## 14. 兼容性

### 14.1 向后兼容

**普通字符串不受影响**：
```rust
let s1 = "normal string";  // ✅ 照常工作
let s2 = f"f-string";      // 🆕 新语法
```

**'f' 标识符不冲突**：
```rust
let f = 42;  // ✅ 变量名可以是 f
let s = f"value: {f}";  // ✅ 可以插值 f 变量
```

---

## 15. 总结

### 15.1 为什么实现字符串插值

**优先级高的原因**：
1. ✅ 工作量小（1.5-2周）
2. ✅ 用户体验提升大
3. ✅ 所有现代语言都有
4. ✅ 实现相对简单
5. ✅ 立即可用

### 15.2 实施步骤

1. **Lexer**：识别 f"..." 并分解
2. **Parser**：创建 FStringExpr 节点
3. **CodeGen**：生成字符串拼接代码
4. **Builtins**：添加类型转换函数
5. **测试**：全面测试

**预计时间**：1.5-2周  
**收益**：极大的用户体验提升

---

**🐾 字符串插值设计完成！准备实施！**

*文档版本: v1.0*  
*创建日期: 2025-10-27*

