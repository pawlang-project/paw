# 闭包系统设计文档

> 📊 **版本**: v1.0  
> 📅 **创建日期**: 2025-10-27  
> 🎯 **目标**: 为 PawLang 设计并实现完整的闭包系统

---

## 1. 闭包概述

### 1.1 什么是闭包？

**闭包（Closure）** = 函数 + 捕获的环境

```rust
let x = 10;
let add_x = |y| y + x;  // 闭包捕获了变量 x
println(f"{add_x(5)}");  // 输出: 15
```

### 1.2 为什么需要闭包？

1. ✅ **函数式编程**：map, filter, reduce
2. ✅ **回调函数**：事件处理、异步编程
3. ✅ **代码复用**：封装重复逻辑
4. ✅ **延迟计算**：lazy evaluation

---

## 2. 语法设计

### 2.1 基础语法

```rust
// 形式 1：单表达式闭包
let add = |x: i32, y: i32| x + y;

// 形式 2：块闭包
let complex = |x: i32| {
    let doubled = x * 2;
    return doubled + 10;
};

// 形式 3：无参数闭包
let get_value = || 42;

// 形式 4：类型推导
let add = |x, y| x + y;  // 从使用中推导类型
```

### 2.2 环境捕获

```rust
// 按值捕获（默认）
let x = 10;
let f = |y| x + y;  // x 被复制到闭包中

// 按引用捕获（显式）
let mut x = 10;
let f = |y| {
    x = x + y;  // 修改外部变量
    return x;
};

// 移动捕获（显式）
let s = "hello";
let f = move |suffix| s + suffix;  // s 被移动到闭包
// println(s);  // 错误：s 已被移动
```

### 2.3 闭包作为参数

```rust
// 函数接受闭包
fn map<T, U>(arr: [T], f: fn(T) -> U) -> [U] {
    let mut result = [];
    for item in arr {
        result.push(f(item));
    }
    return result;
}

let numbers = [1, 2, 3, 4, 5];
let doubled = map(numbers, |x| x * 2);
```

### 2.4 闭包作为返回值

```rust
// 返回闭包
fn make_adder(n: i32) -> fn(i32) -> i32 {
    return |x| x + n;  // 返回捕获了 n 的闭包
}

let add5 = make_adder(5);
println(f"{add5(10)}");  // 15
```

---

## 3. 类型系统

### 3.1 闭包类型

```rust
// 闭包类型语法
fn(T1, T2) -> R  // 函数类型（不捕获环境）
|T1, T2| -> R    // 闭包类型（可能捕获环境）

// 示例
type Adder = fn(i32, i32) -> i32;      // 普通函数
type Closure = |i32| -> i32;           // 闭包
```

### 3.2 类型推导

```rust
// 编译器推导闭包类型
let numbers = [1, 2, 3];
let doubled = map(numbers, |x| x * 2);
// 编译器推导：
// - x 的类型是 i32（从 numbers 的元素类型）
// - 闭包类型是 fn(i32) -> i32
```

### 3.3 泛型闭包

```rust
// 泛型高阶函数
fn apply<T, U>(value: T, f: fn(T) -> U) -> U {
    return f(value);
}

let result = apply(42, |x| x * 2);        // i32 -> i32
let text = apply(42, |x| f"Number: {x}"); // i32 -> string
```

---

## 4. LLVM 实现策略

### 4.1 闭包表示

**方案 A：函数指针 + 环境结构**

```llvm
; 闭包的内部表示
%Closure = type {
    ptr,     ; 函数指针
    ptr      ; 环境指针
}

; 闭包 |y| x + y，捕获 x=10
%Env = type { i32 }  ; 环境包含 x

; 闭包函数
define i32 @closure_fn(ptr %env, i32 %y) {
    %x_ptr = getelementptr %Env, ptr %env, i32 0, i32 0
    %x = load i32, ptr %x_ptr
    %result = add i32 %x, %y
    ret i32 %result
}
```

**方案 B：统一函数指针**（推荐）

```llvm
; 所有闭包都是 (env*, args...) -> result
define i32 @closure_fn(ptr %env, i32 %y) {
    ; 从 env 中提取捕获的变量
    %x = load i32, ptr %env
    %result = add i32 %x, %y
    ret i32 %result
}
```

### 4.2 调用约定

```rust
// PawLang 代码
let x = 10;
let f = |y| x + y;
let result = f(5);
```

```llvm
; LLVM IR
; 1. 分配环境
%env = alloca { i32 }
store i32 10, ptr %env

; 2. 创建闭包
%closure = alloca %Closure
%fn_ptr = getelementptr %Closure, ptr %closure, i32 0, i32 0
store ptr @closure_fn, ptr %fn_ptr
%env_ptr = getelementptr %Closure, ptr %closure, i32 0, i32 1
store ptr %env, ptr %env_ptr

; 3. 调用闭包
%fn = load ptr, ptr %fn_ptr
%captured_env = load ptr, ptr %env_ptr
%result = call i32 %fn(ptr %captured_env, i32 5)
```

---

## 5. 实现计划

### Phase 1: 基础闭包 (1周)

**目标**：支持简单闭包，不捕获环境

```rust
// 支持
let add = |x: i32, y: i32| x + y;
let result = add(10, 20);

// 不支持（下个阶段）
let z = 5;
let add_z = |x| x + z;  // 捕获 z
```

**任务**：
- [ ] Lexer: 识别 `|`, `||` 语法
- [ ] Parser: 解析闭包表达式
- [ ] AST: ClosureExpr 节点
- [ ] CodeGen: 生成函数指针
- [ ] 测试: 基础闭包调用

**时间**: 3-5天

---

### Phase 2: 环境捕获 (1周)

**目标**：支持按值捕获

```rust
// 支持
let x = 10;
let add_x = |y| x + y;  // 捕获 x
let result = add_x(5);  // 15
```

**任务**：
- [ ] AST: 分析闭包捕获的变量
- [ ] CodeGen: 创建环境结构体
- [ ] CodeGen: 传递环境指针
- [ ] 测试: 捕获单个/多个变量

**时间**: 4-6天

---

### Phase 3: 高阶函数 (3-5天)

**目标**：闭包作为参数/返回值

```rust
// 支持
fn map<T, U>(arr: [T], f: fn(T) -> U) -> [U] { ... }
let doubled = map([1, 2, 3], |x| x * 2);
```

**任务**：
- [ ] Parser: 闭包类型语法
- [ ] TypeSystem: fn(T) -> U 类型
- [ ] CodeGen: 传递闭包参数
- [ ] 测试: map, filter, reduce

**时间**: 3-5天

---

### Phase 4: 高级特性 (可选，3-5天)

**目标**：按引用捕获、move语义

```rust
// 支持
let mut x = 0;
let increment = || { x = x + 1; return x; };
```

**任务**：
- [ ] 按引用捕获
- [ ] move 关键字
- [ ] 生命周期检查（简化版）

**时间**: 3-5天

---

## 6. 语法细节

### 6.1 Lexer 修改

```cpp
// common.h
enum class TokenType {
    // ... 现有 ...
    PIPE,           // |
    ARROW_THIN,     // ->（已有）
    KW_MOVE,        // move（可选）
};
```

### 6.2 AST 节点

```cpp
// ast.h
struct ClosureExpr : Expr {
    std::vector<Parameter> params;      // 参数列表
    TypePtr return_type;                // 返回类型（可选）
    ExprPtr body;                       // 单表达式
    StmtPtr block_body;                 // 或块语句
    std::vector<std::string> captures;  // 捕获的变量
    bool is_move;                       // move 捕获
    
    ClosureExpr(...) : Expr(Kind::Closure, loc) {}
};

struct Parameter {
    std::string name;
    TypePtr type;  // 可选
};
```

### 6.3 Parser 修改

```cpp
// parser.cpp
ExprPtr Parser::primary() {
    // ...
    
    // 闭包: |x, y| expr 或 |x, y| { ... }
    if (match({TokenType::PIPE})) {
        return parseClosure();
    }
    
    // ...
}

ExprPtr Parser::parseClosure() {
    // 1. 解析参数: x, y
    std::vector<Parameter> params;
    if (!check(TokenType::PIPE)) {
        do {
            Token name = consume(TokenType::IDENTIFIER, "Expected parameter");
            TypePtr type = nullptr;
            if (match({TokenType::COLON})) {
                type = parseType();
            }
            params.push_back({name.value, std::move(type)});
        } while (match({TokenType::COMMA}));
    }
    consume(TokenType::PIPE, "Expected '|'");
    
    // 2. 可选返回类型: -> T
    TypePtr return_type = nullptr;
    if (match({TokenType::ARROW_THIN})) {
        return_type = parseType();
    }
    
    // 3. 解析 body
    ExprPtr body = nullptr;
    StmtPtr block_body = nullptr;
    
    if (match({TokenType::LBRACE})) {
        // 块闭包
        block_body = blockStatement();
    } else {
        // 表达式闭包
        body = expression();
    }
    
    // 4. 分析捕获的变量（后续）
    std::vector<std::string> captures = analyzeCapturedVars(body, block_body);
    
    return std::make_unique<ClosureExpr>(
        std::move(params),
        std::move(return_type),
        std::move(body),
        std::move(block_body),
        std::move(captures),
        false,  // is_move
        previous().location
    );
}
```

---

## 7. CodeGen 实现

### 7.1 无捕获闭包

```cpp
// codegen_closure.cpp
llvm::Value* CodeGenerator::generateClosureExpr(const ClosureExpr* expr) {
    if (expr->captures.empty()) {
        // 无捕获：直接生成普通函数
        return generateSimpleClosure(expr);
    } else {
        // 有捕获：生成闭包结构
        return generateCapturingClosure(expr);
    }
}

llvm::Value* CodeGenerator::generateSimpleClosure(const ClosureExpr* expr) {
    // 1. 创建匿名函数
    std::string fn_name = "closure_" + std::to_string(closure_counter_++);
    
    // 2. 参数类型
    std::vector<llvm::Type*> param_types;
    for (const auto& param : expr->params) {
        param_types.push_back(getLLVMType(param.type.get()));
    }
    
    // 3. 返回类型
    llvm::Type* return_type = expr->return_type 
        ? getLLVMType(expr->return_type.get())
        : llvm::Type::getVoidTy(*context_);
    
    // 4. 创建函数
    llvm::FunctionType* fn_type = llvm::FunctionType::get(
        return_type, param_types, false
    );
    llvm::Function* fn = llvm::Function::Create(
        fn_type, llvm::Function::InternalLinkage, fn_name, module_.get()
    );
    
    // 5. 生成函数体
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", fn);
    auto saved_block = builder_->GetInsertBlock();
    builder_->SetInsertPoint(entry);
    
    // 绑定参数
    size_t i = 0;
    for (auto& arg : fn->args()) {
        std::string param_name = expr->params[i].name;
        arg.setName(param_name);
        
        // 在 named_values_ 中注册
        llvm::AllocaInst* alloca = builder_->CreateAlloca(
            arg.getType(), nullptr, param_name
        );
        builder_->CreateStore(&arg, alloca);
        named_values_[param_name] = alloca;
        i++;
    }
    
    // 生成 body
    if (expr->body) {
        // 表达式闭包
        llvm::Value* result = generateExpr(expr->body.get());
        builder_->CreateRet(result);
    } else {
        // 块闭包
        generateStmt(expr->block_body.get());
        // 如果没有显式return，添加默认return
        if (!builder_->GetInsertBlock()->getTerminator()) {
            if (return_type->isVoidTy()) {
                builder_->CreateRetVoid();
            }
        }
    }
    
    builder_->SetInsertPoint(saved_block);
    
    // 6. 返回函数指针
    return fn;
}
```

### 7.2 捕获环境的闭包

```cpp
llvm::Value* CodeGenerator::generateCapturingClosure(const ClosureExpr* expr) {
    // 1. 分析捕获的变量
    std::vector<llvm::Type*> env_types;
    std::vector<llvm::Value*> env_values;
    
    for (const auto& var_name : expr->captures) {
        auto it = named_values_.find(var_name);
        if (it != named_values_.end()) {
            llvm::Value* var_ptr = it->second;
            llvm::Value* var_val = builder_->CreateLoad(
                var_ptr->getType()->getPointerElementType(),
                var_ptr
            );
            env_types.push_back(var_val->getType());
            env_values.push_back(var_val);
        }
    }
    
    // 2. 创建环境结构体类型
    llvm::StructType* env_type = llvm::StructType::create(
        *context_, env_types, "closure_env"
    );
    
    // 3. 分配并填充环境
    llvm::Value* env = builder_->CreateAlloca(env_type);
    for (size_t i = 0; i < env_values.size(); ++i) {
        llvm::Value* field_ptr = builder_->CreateStructGEP(env_type, env, i);
        builder_->CreateStore(env_values[i], field_ptr);
    }
    
    // 4. 创建闭包函数（第一个参数是 env*）
    std::vector<llvm::Type*> param_types;
    param_types.push_back(llvm::PointerType::get(*context_, 0)); // env*
    for (const auto& param : expr->params) {
        param_types.push_back(getLLVMType(param.type.get()));
    }
    
    llvm::Type* return_type = expr->return_type 
        ? getLLVMType(expr->return_type.get())
        : llvm::Type::getVoidTy(*context_);
    
    llvm::FunctionType* fn_type = llvm::FunctionType::get(
        return_type, param_types, false
    );
    
    std::string fn_name = "closure_" + std::to_string(closure_counter_++);
    llvm::Function* fn = llvm::Function::Create(
        fn_type, llvm::Function::InternalLinkage, fn_name, module_.get()
    );
    
    // 5. 生成函数体（可以访问 env）
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", fn);
    auto saved_block = builder_->GetInsertBlock();
    builder_->SetInsertPoint(entry);
    
    // 从 env 中恢复捕获的变量
    auto env_arg = fn->arg_begin();
    for (size_t i = 0; i < expr->captures.size(); ++i) {
        llvm::Value* field_ptr = builder_->CreateStructGEP(
            env_type, env_arg, i
        );
        llvm::Value* val = builder_->CreateLoad(env_types[i], field_ptr);
        
        // 注册到 named_values_
        llvm::AllocaInst* alloca = builder_->CreateAlloca(env_types[i]);
        builder_->CreateStore(val, alloca);
        named_values_[expr->captures[i]] = alloca;
    }
    
    // 绑定其他参数
    auto arg_it = fn->arg_begin();
    ++arg_it;  // 跳过 env
    for (size_t i = 0; i < expr->params.size(); ++i, ++arg_it) {
        std::string param_name = expr->params[i].name;
        arg_it->setName(param_name);
        
        llvm::AllocaInst* alloca = builder_->CreateAlloca(
            arg_it->getType(), nullptr, param_name
        );
        builder_->CreateStore(&(*arg_it), alloca);
        named_values_[param_name] = alloca;
    }
    
    // 生成 body
    if (expr->body) {
        llvm::Value* result = generateExpr(expr->body.get());
        builder_->CreateRet(result);
    } else {
        generateStmt(expr->block_body.get());
        if (!builder_->GetInsertBlock()->getTerminator()) {
            if (return_type->isVoidTy()) {
                builder_->CreateRetVoid();
            }
        }
    }
    
    builder_->SetInsertPoint(saved_block);
    
    // 6. 创建闭包结构 {fn_ptr, env_ptr}
    llvm::StructType* closure_type = llvm::StructType::create(
        *context_,
        {llvm::PointerType::get(*context_, 0), 
         llvm::PointerType::get(*context_, 0)},
        "Closure"
    );
    
    llvm::Value* closure = builder_->CreateAlloca(closure_type);
    llvm::Value* fn_ptr_field = builder_->CreateStructGEP(closure_type, closure, 0);
    builder_->CreateStore(fn, fn_ptr_field);
    llvm::Value* env_ptr_field = builder_->CreateStructGEP(closure_type, closure, 1);
    builder_->CreateStore(env, env_ptr_field);
    
    return closure;
}
```

---

## 8. 测试用例

### 8.1 基础闭包（Phase 1）

```rust
// test_closure_basic.paw
fn main() -> i32 {
    // 1. 简单闭包
    let add = |x: i32, y: i32| x + y;
    println(f"add(10, 20) = {add(10, 20)}");  // 30
    
    // 2. 无参数闭包
    let get_value = || 42;
    println(f"get_value() = {get_value()}");  // 42
    
    // 3. 块闭包
    let complex = |x: i32| {
        let doubled = x * 2;
        return doubled + 10;
    };
    println(f"complex(5) = {complex(5)}");  // 20
    
    return 0;
}
```

### 8.2 环境捕获（Phase 2）

```rust
// test_closure_capture.paw
fn main() -> i32 {
    // 1. 捕获单个变量
    let x = 10;
    let add_x = |y| x + y;
    println(f"add_x(5) = {add_x(5)}");  // 15
    
    // 2. 捕获多个变量
    let a = 10;
    let b = 20;
    let combine = |x| (a + b) * x;
    println(f"combine(2) = {combine(2)}");  // 60
    
    // 3. 嵌套闭包
    let outer = 5;
    let make_closure = || {
        let inner = |x| x + outer;
        return inner;
    };
    let closure = make_closure();
    println(f"closure(10) = {closure(10)}");  // 15
    
    return 0;
}
```

### 8.3 高阶函数（Phase 3）

```rust
// test_closure_hof.paw
fn map<T, U>(arr: [T], f: fn(T) -> U) -> [U] {
    let mut result = [];
    for item in arr {
        result.push(f(item));
    }
    return result;
}

fn filter<T>(arr: [T], predicate: fn(T) -> bool) -> [T] {
    let mut result = [];
    for item in arr {
        if predicate(item) {
            result.push(item);
        }
    }
    return result;
}

fn main() -> i32 {
    let numbers = [1, 2, 3, 4, 5];
    
    // map
    let doubled = map(numbers, |x| x * 2);
    println(f"doubled: {doubled}");  // [2, 4, 6, 8, 10]
    
    // filter
    let evens = filter(numbers, |x| x % 2 == 0);
    println(f"evens: {evens}");  // [2, 4]
    
    // 组合
    let result = map(filter(numbers, |x| x > 2), |x| x * x);
    println(f"result: {result}");  // [9, 16, 25]
    
    return 0;
}
```

---

## 9. 与其他语言对比

| 语言 | 闭包语法 | 捕获方式 | PawLang |
|------|----------|----------|---------|
| Rust | `\|x\| x + 1` | 自动推导 | ✅ 类似 |
| JavaScript | `x => x + 1` | 按引用 | ⏳ 计划 |
| Python | `lambda x: x + 1` | 按引用 | ⏳ 计划 |
| Go | `func(x int) int { return x + 1 }` | 显式 | ❌ 不同 |
| C++ | `[](int x) { return x + 1; }` | `[=]` / `[&]` | ⏳ 计划 |

**PawLang 选择**：
- Rust风格的 `|x| expr` 语法
- 默认按值捕获
- 可选 `move` 关键字

---

## 10. 实现难点

### 10.1 类型推导

**挑战**：闭包参数类型可能需要推导

```rust
let f = |x| x + 1;  // x 的类型？
```

**解决方案**：
1. 要求显式类型（简单）：`|x: i32| x + 1`
2. 从使用中推导（复杂）：`map([1, 2], |x| x + 1)` → x: i32

**Phase 1**: 要求显式类型  
**Phase 4**: 实现类型推导

---

### 10.2 生命周期

**挑战**：捕获的变量可能已失效

```rust
fn make_closure() -> fn() -> i32 {
    let x = 10;
    return || x;  // 危险：x 的生命周期已结束！
}
```

**解决方案**：
1. **简化版**（Phase 1-3）：不检查，信任程序员
2. **完整版**（Phase 4+）：生命周期分析

---

### 10.3 性能

**挑战**：闭包调用可能比函数调用慢

**优化**：
1. 无捕获闭包 → 直接函数指针（零开销）
2. 内联小闭包
3. 逃逸分析（栈分配 vs 堆分配）

---

## 11. 总结

### 11.1 时间表

| 阶段 | 功能 | 时间 | 累计 |
|------|------|------|------|
| Phase 1 | 基础闭包 | 3-5天 | 5天 |
| Phase 2 | 环境捕获 | 4-6天 | 11天 |
| Phase 3 | 高阶函数 | 3-5天 | 16天 |
| Phase 4 | 高级特性 | 3-5天 | 21天 |
| **总计** | | **2-3周** | |

### 11.2 里程碑

✅ **Phase 1 完成**：
- 可以定义和调用简单闭包
- 支持表达式和块语法

✅ **Phase 2 完成**：
- 闭包可以捕获外部变量
- 核心功能可用

✅ **Phase 3 完成**：
- 支持map, filter等高阶函数
- 闭包系统基本完整

✅ **Phase 4 完成**：
- 支持按引用捕获和move
- 生产级质量

---

**🐾 闭包系统设计完成！准备开始实现！**

*创建日期: 2025-10-27*

