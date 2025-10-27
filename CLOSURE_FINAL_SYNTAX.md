# 闭包最终语法规范

> 📅 **日期**: 2025-10-27  
> ✅ **确定**: `(x, y) -> { body }` 支持类型推导

---

## 1. 最终语法

### 1.1 完整形式

```rust
// 形式 1: 完全显式（新手友好）
(x: i32, y: i32) -> i32 { x + y }

// 形式 2: 推导返回类型
(x: i32, y: i32) -> { x + y }

// 形式 3: 推导参数类型
(x, y) -> i32 { x + y }

// 形式 4: 全部推导（最简洁）
(x, y) -> { x + y }

// 形式 5: 无参数
() -> { 42 }
() -> i32 { 42 }
```

### 1.2 语法规则

```ebnf
closure := '(' params? ')' '->' type? block

params  := param (',' param)*
param   := IDENTIFIER (':' type)?    # 类型可选！

type    := ... (已有的类型语法)

block   := '{' statements '}'
```

---

## 2. 类型推导策略

### 2.1 推导方向

**从使用上下文推导**：

```rust
// 示例 1: 从函数签名推导
fn map<T, U>(arr: [T], f: fn(T) -> U) -> [U] { ... }

let numbers: [i32] = [1, 2, 3];
let doubled = map(numbers, (x) -> { x * 2 });
//                         ↑
//  推导：x: i32 (从 numbers 的元素类型)
//        返回 i32 (从 x * 2)
```

```rust
// 示例 2: 从赋值推导
let add: fn(i32, i32) -> i32 = (x, y) -> { x + y };
//       ↑ 类型标注              ↑ 推导 x: i32, y: i32
```

```rust
// 示例 3: 从 body 推导返回类型
let f = (x: i32) -> { x + 1 };
//                    ↑ 推导返回 i32
```

### 2.2 推导失败的情况

```rust
// 错误：无法推导
let f = (x) -> { x + 1 };  // ❌ x 的类型不明确
//      ↑ 需要更多上下文

// 修复 1: 显式标注参数
let f = (x: i32) -> { x + 1 };  // ✅

// 修复 2: 提供使用上下文
let f: fn(i32) -> i32 = (x) -> { x + 1 };  // ✅

// 修复 3: 从使用推导
map([1, 2, 3], (x) -> { x + 1 });  // ✅ 从数组元素类型推导
```

---

## 3. 完整示例

### 3.1 类型推导的各种情况

```rust
// ===== 场景 1: 高阶函数（最常见）=====

let numbers = [1, 2, 3, 4, 5];

// 完全推导
map(numbers, (x) -> { x * 2 })
//           ↑ x: i32 从 numbers 推导

// 部分显式
map(numbers, (x: i32) -> { x * 2 })

// 完全显式
map(numbers, (x: i32) -> i32 { x * 2 })


// ===== 场景 2: 变量赋值 =====

// 需要类型标注
let f: fn(i32) -> i32 = (x) -> { x + 1 };
//     ↑ 类型标注        ↑ 从这里推导

// 或参数显式
let f = (x: i32) -> { x + 1 };
//      ↑ 参数类型   ↑ 返回类型推导


// ===== 场景 3: 立即调用 =====

let result = ((x: i32) -> { x * 2 })(10);
//            ↑ 必须显式，因为无上下文


// ===== 场景 4: 环境捕获 =====

let factor = 10;
let multiply = (x) -> { x * factor };
//             ↑ 可以推导吗？

// 选项 A: 可以推导（从 factor 推导 x 为 i32）
// 选项 B: 必须显式（推导规则复杂）
```

---

## 4. 实现阶段

### Phase 1: 必须显式类型（简单）

```rust
// ✅ 支持
(x: i32, y: i32) -> i32 { x + y }
(x: i32) -> i32 { x * 2 }
() -> i32 { 42 }

// ✅ 支持（返回类型推导）
(x: i32) -> { x + 1 }

// ❌ 不支持（参数类型推导）
(x, y) -> { x + y }  // 编译错误：必须标注参数类型
```

**时间**: 3-5天  
**难度**: 简单

---

### Phase 2: 从上下文推导（中等）

```rust
// ✅ 支持（从函数签名推导）
map(numbers, (x) -> { x * 2 })

// ✅ 支持（从变量类型推导）
let f: fn(i32) -> i32 = (x) -> { x + 1 };

// ❌ 仍不支持（无上下文）
let f = (x) -> { x + 1 };  // 错误：需要类型标注
```

**时间**: 5-7天  
**难度**: 中等

---

### Phase 3: 完全推导（复杂）

```rust
// ✅ 支持（从 body 推导）
let f = (x: i32) -> { x + 1 };  // 推导返回 i32

// ✅ 支持（从使用推导）
let doubled = map([1, 2, 3], (x) -> { x * 2 });

// ✅ 支持（从捕获推导）
let factor: i32 = 10;
let multiply = (x) -> { x * factor };  // 推导 x: i32
```

**时间**: 3-5天  
**难度**: 中等偏难

---

## 5. 推导算法

### 5.1 返回类型推导（Phase 1）

```cpp
TypePtr deduceReturnType(BlockStmt* block) {
    // 1. 查找所有 return 语句
    // 2. 推导每个 return 的表达式类型
    // 3. 统一所有类型
    // 4. 如果无 return，检查最后一个表达式
    
    // 示例
    // { x + 1 }  → return type = i32（如果 x: i32）
    // { return x * 2; } → return type = i32
    // { println("hi"); } → return type = void
}
```

### 5.2 参数类型推导（Phase 2）

```cpp
TypePtr deduceParamType(ClosureExpr* closure, size_t param_index, Context* ctx) {
    // 策略 1: 从函数签名推导
    if (ctx->expected_type && ctx->expected_type->kind == Type::Kind::Function) {
        FunctionType* fn_type = (FunctionType*)ctx->expected_type;
        return fn_type->param_types[param_index];
    }
    
    // 策略 2: 从变量类型推导
    // let f: fn(i32) -> i32 = (x) -> { ... }
    //        ↑ 从这里推导
    
    // 策略 3: 从使用推导
    // map([1, 2, 3], (x) -> { x * 2 })
    //     ↑ i32[]     ↑ 推导 x: i32
    
    return nullptr;  // 无法推导
}
```

### 5.3 捕获变量推导（Phase 3）

```cpp
TypePtr deduceFromCaptures(ClosureExpr* closure) {
    // 示例
    // let factor: i32 = 10;
    // let f = (x) -> { x * factor };
    //              ↑ factor: i32 → x 必须支持 * 运算 → x: i32
    
    // 策略：统一约束
    // 1. x * factor 要求 x 和 factor 类型兼容
    // 2. factor: i32 → x: i32
}
```

---

## 6. 语法对比

| 写法 | Phase 1 | Phase 2 | Phase 3 |
|------|---------|---------|---------|
| `(x: i32, y: i32) -> i32 { x + y }` | ✅ | ✅ | ✅ |
| `(x: i32, y: i32) -> { x + y }` | ✅ | ✅ | ✅ |
| `(x, y) -> i32 { x + y }` | ❌ | ✅* | ✅ |
| `(x, y) -> { x + y }` | ❌ | ✅* | ✅ |
| `map(arr, (x) -> { x * 2 })` | ❌ | ✅ | ✅ |

*需要上下文（函数签名或变量类型）

---

## 7. 实施建议

### 推荐路线

**Phase 1** (3-5天): 基础闭包
```rust
// 只支持显式类型
(x: i32) -> i32 { x + 1 }
(x: i32) -> { x + 1 }  // 返回类型可推导
```

**Phase 2** (5-7天): 上下文推导
```rust
// 支持从使用推导
map(numbers, (x) -> { x * 2 })
let f: fn(i32) -> i32 = (x) -> { x + 1 };
```

**Phase 3** (3-5天): 完全推导
```rust
// 支持从捕获推导
let factor = 10;
let f = (x) -> { x * factor };  // 推导 x: i32
```

**总计**: 2-3周

---

## 8. 最终确认

基于您的要求，**最终的闭包语法**为：

```rust
// 完整语法
(param1: Type1?, param2: Type2?, ...) -> ReturnType? { body }

// 规则：
// 1. 参数类型可选（推导或显式）
// 2. 返回类型可选（推导或显式）
// 3. 必须有括号 ()
// 4. 必须有箭头 ->
// 5. 必须有花括号 {}

// 示例
(x: i32, y: i32) -> i32 { x + y }  // 全显式
(x: i32, y: i32) -> { x + y }      // 推导返回
(x, y) -> i32 { x + y }            // 推导参数（需要上下文）
(x, y) -> { x + y }                // 全推导（需要上下文）
```

---

**🎯 这个设计非常完美！**

平衡了：
- ✅ 灵活性（可推导）
- ✅ 清晰性（可显式）
- ✅ 一致性（与函数语法统一）
- ✅ 渐进性（Phase 1 → Phase 3）

**准备开始实现吗？** 🚀

我建议：
1. **Phase 1**: 先实现显式类型，快速出功能（3-5天）
2. **Phase 2-3**: 再添加类型推导（1-2周）

这样可以渐进式开发，每个阶段都有可用的功能。您觉得如何？

