# 闭包 Phase 3：参数类型推导设计

> 📅 **日期**: 2025-10-27  
> 🎯 **目标**: 实现闭包参数的类型推导

---

## 1. 目标

### 当前（需要显式类型）
```rust
let doubled = map(numbers, (x: i32) -> { x * 2 });
//                         ↑ 必须写 i32
```

### 目标（类型推导）
```rust
let doubled = map(numbers, (x) -> { x * 2 });
//                         ↑ 从 numbers 的元素类型推导
```

---

## 2. 推导策略

### 策略 1: 从赋值上下文推导

```rust
// 变量有类型标注
let f: fn(i32, i32) -> i32 = (x, y) -> { x + y };
//     ↑ 类型标注              ↑ 推导 x: i32, y: i32
```

**实现**：
- Parser 在解析 Let 语句时传递期望类型
- ClosureExpr 记录期望的函数类型
- CodeGen 时从期望类型获取参数类型

### 策略 2: 从函数参数推导

```rust
fn map<T, U>(arr: [T], f: fn(T) -> U) -> [U] { ... }

let numbers = [1, 2, 3];
map(numbers, (x) -> { x * 2 });
//           ↑ 推导：T = i32（从 numbers）
//           ↑ f: fn(i32) -> U
//           ↑ x: i32
```

**实现**：
- 函数调用时，从形参类型推导实参期望类型
- 传递期望类型给闭包表达式
- CodeGen 时使用期望类型

---

## 3. AST 扩展

```cpp
// ast.h
struct ClosureExpr : Expr {
    std::vector<ClosureParam> params;
    TypePtr return_type;
    StmtPtr body;
    std::vector<std::string> captures;
    
    // 新增：期望的函数类型（用于推导）
    TypePtr expected_fn_type;  // fn(i32, i32) -> i32
    
    ClosureExpr(...) : Expr(Kind::Closure, loc) {}
};
```

---

## 4. Parser 修改

### 4.1 传递期望类型

```cpp
// parser.cpp
ExprPtr Parser::assignment() {
    // ...
    if (match({TokenType::ASSIGN})) {
        // ...
        
        // 如果左边有类型标注，传递给右边
        TypePtr expected_type = getExpectedType(left);
        ExprPtr value = expression(expected_type);  // 新参数
        // ...
    }
}

ExprPtr Parser::expression(TypePtr expected_type) {
    // 传递期望类型
    // ...
}
```

### 4.2 闭包记录期望类型

```cpp
ExprPtr Parser::parseClosure(TypePtr expected_type) {
    // 解析闭包
    auto closure = std::make_unique<ClosureExpr>(...);
    
    // 记录期望类型
    closure->expected_fn_type = std::move(expected_type);
    
    return closure;
}
```

---

## 5. CodeGen 修改

### 5.1 从期望类型获取参数类型

```cpp
llvm::Value* CodeGenerator::generateCapturingClosure(...) {
    // ...
    
    for (size_t i = 0; i < expr->params.size(); ++i) {
        const auto& param = expr->params[i];
        
        if (!param.type) {
            // 需要推导
            llvm::Type* deduced_type = deduceParamType(expr, i);
            if (!deduced_type) {
                std::cerr << "Error: Cannot deduce type for parameter: " 
                          << param.name << std::endl;
                return nullptr;
            }
            param_types.push_back(deduced_type);
        } else {
            // 显式类型
            param_types.push_back(convertType(param.type.get()));
        }
    }
    
    // ...
}

llvm::Type* CodeGenerator::deduceParamType(const ClosureExpr* expr, size_t param_idx) {
    // 从 expected_fn_type 获取参数类型
    if (expr->expected_fn_type && 
        expr->expected_fn_type->kind == Type::Kind::Function) {
        // TODO: 解析函数类型的参数类型
    }
    
    // 从变量类型推导
    // let f: fn(i32) -> i32 = (x) -> { ... }
    
    return nullptr;
}
```

---

## 6. 简化方案（Phase 3）

由于完整的类型推导非常复杂，Phase 3 采用简化方案：

### 仅支持场景 1: 变量类型标注

```rust
// ✅ 支持
let f: fn(i32, i32) -> i32 = (x, y) -> { x + y };

// ❌ 暂不支持（需要更复杂的类型推导）
map(numbers, (x) -> { x * 2 });
```

---

## 7. 实施步骤

### Step 1: AST 添加 expected_fn_type
- 修改 ClosureExpr

### Step 2: Parser 传递期望类型
- 修改 Let 语句解析
- 传递类型给闭包

### Step 3: CodeGen 使用期望类型
- 修改 generateClosureExpr
- 实现 deduceParamType

### Step 4: 测试
- 创建测试用例
- 验证推导正确性

**预计时间**: 2-3天

---

**🎯 开始实现！**

