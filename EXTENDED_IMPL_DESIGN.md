# 扩展接口实现系统设计

> 📊 **版本**: v1.0  
> 📅 **创建日期**: 2025-10-27  
> 🎯 **目标**: 支持为任意类型（包括内置类型）实现接口

---

## 1. 功能目标

### 当前限制 ❌

```paw
// ❌ 无法为内置类型实现接口
support Display for i32 {
    fn to_string(self) -> string { ... }
}

// ❌ 无法为标准库类型实现接口
support Display for Vec<i32> {
    fn to_string(self) -> string { ... }
}

// ❌ 无法实现泛型接口
support<T: Display> Display for Vec<T> {
    fn to_string(self) -> string { ... }
}
```

### 新增功能 ✅

```paw
// ✅ 为内置类型实现接口
support Display for i32 {
    fn to_string(self) -> string {
        // 调用内置的 int_to_string
        return int_to_string(self);
    }
}

// ✅ 为标准库类型实现接口
support Display for Vec<i32> {
    fn to_string(self) -> string {
        let mut result = "[";
        for i in 0..self.len() {
            if i > 0 { result = result + ", "; }
            result = result + int_to_string(self.get(i));
        }
        return result + "]";
    }
}

// ✅ 泛型接口实现（Phase 2）
support<T: Display> Display for Vec<T> {
    fn to_string(self) -> string {
        let mut result = "[";
        for i in 0..self.len() {
            if i > 0 { result = result + ", "; }
            result = result + self.get(i).to_string();  // 调用 T 的 to_string
        }
        return result + "]";
    }
}

// ✅ 使用
let x = 42;
println(x.to_string());  // "42"

let v = vec![1, 2, 3];
println(v.to_string());  // "[1, 2, 3]"
```

---

## 2. 语法设计

### 2.1 基础语法（已有）

```paw
support InterfaceName for TypeName {
    // 方法实现
}
```

### 2.2 泛型接口实现（新增）

```paw
support<T> InterfaceName for TypeName<T> {
    // 可以使用 T
}

support<T: Constraint> InterfaceName for TypeName<T> {
    // T 必须满足 Constraint
}

support<T: C1 + C2> InterfaceName for TypeName<T> {
    // T 必须满足多个约束
}
```

---

## 3. 孤儿规则（Orphan Rule）

### 规则定义

**为了防止冲突，实现接口需要满足以下条件之一**：

1. **你拥有类型**：在当前模块定义的类型
2. **你拥有接口**：在当前模块定义的接口
3. **特殊权限**：标准库可以为内置类型实现标准接口

### 示例

```paw
// ✅ 允许：你定义了 Point
type Point = struct { x: i32, y: i32 }
support Display for Point { ... }

// ✅ 允许：你定义了 MyDisplay
type MyDisplay = interface { ... }
support MyDisplay for i32 { ... }

// ❌ 禁止：你既不拥有 Display 也不拥有 i32（如果在用户代码中）
support Display for i32 { ... }  // 只有标准库可以

// ✅ 允许：你拥有 MyType
type MyType = struct { ... }
support Display for MyType { ... }  // Display 是标准库，但你拥有 MyType
```

### 实现策略

**Phase 1**：暂不强制孤儿规则，允许任意实现
- 简化实现
- 快速验证功能

**Phase 2**（未来）：强制孤儿规则
- 添加模块所有权检查
- 防止冲突

---

## 4. 技术实现

### 4.1 SymbolTable 扩展

#### 当前结构

```cpp
class SymbolTable {
    // 当前：type_name -> [interface_names]
    std::map<std::string, std::vector<std::string>> type_interfaces_;
};
```

#### 新增结构

```cpp
class SymbolTable {
    // 类型 -> 接口 -> 实现信息
    struct InterfaceImpl {
        std::string type_name;          // "i32", "Vec<i32>"
        std::string interface_name;     // "Display"
        const SupportStmt* impl_stmt;   // 实现语句
        bool is_generic;                // 是否是泛型实现
        std::vector<std::string> generic_params;  // ["T"]
        std::vector<std::string> constraints;     // ["Display"]
    };
    
    // type_name -> interface_name -> InterfaceImpl
    std::map<std::string, std::map<std::string, InterfaceImpl>> interface_impls_;
    
    // 泛型实现：interface_name -> [InterfaceImpl]
    std::map<std::string, std::vector<InterfaceImpl>> generic_impls_;
};
```

#### 新增方法

```cpp
// 注册接口实现（支持泛型）
void registerInterfaceImplExtended(
    const std::string& type_name,
    const std::string& interface_name,
    const SupportStmt* impl_stmt,
    bool is_generic = false,
    const std::vector<std::string>& generic_params = {},
    const std::vector<std::string>& constraints = {}
);

// 查询接口实现（考虑泛型）
const InterfaceImpl* getInterfaceImpl(
    const std::string& type_name,
    const std::string& interface_name
) const;

// 检查类型是否实现了接口（考虑泛型匹配）
bool typeImplementsInterfaceExtended(
    const std::string& type_name,
    const std::string& interface_name
) const;
```

---

### 4.2 CodeGen 修改

#### 当前问题

```cpp
// generateMemberAccessExpr 中
// 当前只处理 struct 成员和方法
if (auto struct_def = ...) {
    // 查找方法
}
```

#### 新增逻辑

```cpp
llvm::Value* CodeGenerator::generateMemberAccessExpr(const MemberAccessExpr* expr) {
    auto obj_value = generateExpr(expr->object.get());
    
    // 1. 尝试作为 struct 成员/方法
    if (/* struct member/method */) {
        // 现有逻辑
    }
    
    // 2. 尝试作为接口方法（新增）
    std::string obj_type = getExprType(expr->object.get());
    if (symbol_table_->typeImplementsInterfaceExtended(obj_type, /* ... */)) {
        // 查找接口方法实现
        auto impl = symbol_table_->getInterfaceImpl(obj_type, /* ... */);
        
        // 生成接口方法调用
        return generateInterfaceMethodCall(impl, expr->member, obj_value);
    }
    
    // 3. 错误
    reportError(...);
}
```

#### 生成接口方法调用

```cpp
llvm::Value* CodeGenerator::generateInterfaceMethodCall(
    const InterfaceImpl* impl,
    const std::string& method_name,
    llvm::Value* self_value
) {
    // 1. 查找方法定义
    auto support_stmt = impl->impl_stmt;
    FunctionStmt* method = nullptr;
    for (auto& func : support_stmt->methods) {
        if (func->name == method_name) {
            method = func.get();
            break;
        }
    }
    
    // 2. 生成方法调用
    std::string mangled_name = mangleInterfaceMethod(
        impl->type_name, impl->interface_name, method_name
    );
    
    auto func = module_->getFunction(mangled_name);
    if (!func) {
        // 第一次调用，生成函数
        func = generateSupportMethod(support_stmt, method);
    }
    
    // 3. 调用
    return builder_->CreateCall(func, {self_value});
}
```

---

### 4.3 类型匹配算法

#### 精确匹配

```cpp
bool exactMatch(const std::string& type_name, const std::string& pattern) {
    return type_name == pattern;
}
```

#### 泛型匹配（Phase 2）

```cpp
bool genericMatch(
    const std::string& type_name,      // "Vec<i32>"
    const std::string& pattern,        // "Vec<T>"
    const std::vector<std::string>& constraints  // ["Display"]
) {
    // 1. 解析类型
    // Vec<i32> -> base="Vec", args=["i32"]
    // Vec<T>   -> base="Vec", params=["T"]
    
    // 2. 匹配 base
    if (base1 != base2) return false;
    
    // 3. 检查泛型参数约束
    for (auto& arg : args) {
        for (auto& constraint : constraints) {
            if (!typeImplementsInterface(arg, constraint)) {
                return false;
            }
        }
    }
    
    return true;
}
```

---

## 5. 实施计划

### Phase 1：基础扩展实现（1周）

**目标**：支持为内置类型和具体标准库类型实现接口

#### 任务

1. **SymbolTable 扩展**（1天）
   - 添加 `InterfaceImpl` 结构
   - 添加 `interface_impls_` 存储
   - 实现 `registerInterfaceImplExtended`
   - 实现 `getInterfaceImpl`
   - 实现 `typeImplementsInterfaceExtended`（精确匹配）

2. **CodeGen 修改**（2天）
   - 修改 `generateSupportStmt` 注册时使用新方法
   - 修改 `generateMemberAccessExpr` 支持接口方法查找
   - 实现 `generateInterfaceMethodCall`
   - 实现 `mangleInterfaceMethod`（名称修饰）

3. **标准库实现**（2天）
   - 为 i32, i64, f64, string 实现 Display
   - 为 Vec<i32> 实现 Display
   - 为 Box<T> 实现必要接口

4. **测试**（1天）
   - 测试内置类型接口调用
   - 测试标准库类型接口调用
   - 测试方法链式调用

**预期成果**：

```paw
// ✅ 可以工作
support Display for i32 {
    fn to_string(self) -> string {
        return int_to_string(self);
    }
}

let x = 42;
println(x.to_string());  // "42"
```

---

### Phase 2：泛型接口实现（1周，可选）

**目标**：支持带泛型参数的接口实现

#### 任务

1. **Parser 扩展**（2天）
   - 修改 `supportDeclaration` 解析泛型参数
   - 支持 `support<T: Constraint> I for Type<T>` 语法
   - 存储泛型参数和约束到 `SupportStmt`

2. **类型匹配算法**（2天）
   - 实现类型模式解析（`Vec<T>` -> `Vec` + `[T]`）
   - 实现泛型匹配算法
   - 实现约束检查

3. **CodeGen 支持**（2天）
   - 修改 `generateSupportStmt` 处理泛型实现
   - 实现泛型实例化
   - 生成特化版本

4. **测试**（1天）
   - 测试 `support<T> I for Vec<T>`
   - 测试约束检查
   - 测试多层泛型

**预期成果**：

```paw
// ✅ 可以工作
support<T: Display> Display for Vec<T> {
    fn to_string(self) -> string {
        // ...
    }
}

let v = vec![1, 2, 3];  // Vec<i32>，i32 实现了 Display
println(v.to_string());  // "[1, 2, 3]"
```

---

## 6. 标准库扩展

### 6.1 为内置类型实现接口

**文件**：`stdlib/std/impls/primitives.paw`

```paw
// 为整数类型实现 Display
support Display for i32 {
    fn to_string(self) -> string {
        return int_to_string(self);
    }
}

support Display for i64 {
    fn to_string(self) -> string {
        return int_to_string(self);
    }
}

support Display for f64 {
    fn to_string(self) -> string {
        return float_to_string(self);
    }
}

support Display for string {
    fn to_string(self) -> string {
        return self;  // 字符串直接返回
    }
}

support Display for char {
    fn to_string(self) -> string {
        return char_to_string(self);
    }
}

support Display for bool {
    fn to_string(self) -> string {
        if self {
            return "true";
        } else {
            return "false";
        }
    }
}
```

### 6.2 为集合类型实现接口

**文件**：`stdlib/std/impls/collections.paw`

```paw
// 为 Vec<i32> 实现 Display（Phase 1）
support Display for Vec<i32> {
    fn to_string(self) -> string {
        let mut result = "[";
        for i in 0..self.len() {
            if i > 0 {
                result = result + ", ";
            }
            result = result + int_to_string(self.get(i));
        }
        return result + "]";
    }
}

// 泛型实现（Phase 2）
support<T: Display> Display for Vec<T> {
    fn to_string(self) -> string {
        let mut result = "[";
        for i in 0..self.len() {
            if i > 0 {
                result = result + ", ";
            }
            result = result + self.get(i).to_string();
        }
        return result + "]";
    }
}
```

### 6.3 为其他类型实现接口

```paw
// Box<T>
support<T: Display> Display for Box<T> {
    fn to_string(self) -> string {
        return "Box(" + self.value.to_string() + ")";
    }
}

// Range<T>
support<T: Display> Display for Range<T> {
    fn to_string(self) -> string {
        return self.start.to_string() + ".." + self.end.to_string();
    }
}

// Option<T> (T?)
support<T: Display> Display for T? {
    fn to_string(self) -> string {
        self is {
            ok(value) => "Some(" + value.to_string() + ")",
            err(_) => "None",
        }
    }
}
```

---

## 7. 测试用例

### 7.1 基础测试

**文件**：`examples/extended_impl_basic.paw`

```paw
import "std::interfaces";
import "std::impls::primitives";

fn main() -> i32 {
    // 测试内置类型
    let x = 42;
    println(x.to_string());  // "42"
    
    let f = 3.14;
    println(f.to_string());  // "3.14"
    
    let b = true;
    println(b.to_string());  // "true"
    
    let s = "hello";
    println(s.to_string());  // "hello"
    
    return 0;
}
```

### 7.2 集合测试

**文件**：`examples/extended_impl_collections.paw`

```paw
import "std::interfaces";
import "std::impls::primitives";
import "std::impls::collections";
import "std::collections::types";

fn main() -> i32 {
    // 测试 Vec<i32>
    let mut v = Vec::new();
    v = v.push(1);
    v = v.push(2);
    v = v.push(3);
    
    println(v.to_string());  // "[1, 2, 3]"
    
    return 0;
}
```

### 7.3 泛型测试（Phase 2）

**文件**：`examples/extended_impl_generic.paw`

```paw
import "std::interfaces";
import "std::impls::primitives";
import "std::impls::collections";

// 泛型函数使用 Display 约束
fn print_any<T: Display>(value: T) {
    println(value.to_string());
}

fn main() -> i32 {
    // 可以打印任何实现了 Display 的类型
    print_any(42);           // i32
    print_any(3.14);         // f64
    print_any("hello");      // string
    print_any(vec![1, 2]);   // Vec<i32>
    
    return 0;
}
```

---

## 8. 名称修饰（Name Mangling）

### 规则

```
interface_method_name = "{type}::{interface}::{method}"
```

### 示例

```cpp
// i32 的 Display::to_string
// mangled: "i32::Display::to_string"

// Vec<i32> 的 Display::to_string
// mangled: "Vec<i32>::Display::to_string"

// Vec<T> 的 Display::to_string（泛型）
// mangled: "Vec<T>::Display::to_string"
```

### 实现

```cpp
std::string CodeGenerator::mangleInterfaceMethod(
    const std::string& type_name,
    const std::string& interface_name,
    const std::string& method_name
) {
    return type_name + "::" + interface_name + "::" + method_name;
}
```

---

## 9. 风险和挑战

### 9.1 类型系统复杂度 ⚠️

**问题**：泛型接口实现大幅增加类型系统复杂度

**解决**：
- Phase 1 只支持具体类型
- Phase 2 再添加泛型支持
- 充分测试边界情况

### 9.2 编译时间 ⚠️

**问题**：泛型实例化可能增加编译时间

**解决**：
- 缓存已实例化的版本
- 惰性实例化（按需生成）

### 9.3 名称冲突 ⚠️

**问题**：接口方法和 struct 方法可能重名

**解决**：
- 优先级：struct 方法 > 接口方法
- 明确的查找顺序

---

## 10. 成功标准

### Phase 1 成功标准

- ✅ 可以为 i32, f64, string 实现 Display
- ✅ 可以调用 `42.to_string()`
- ✅ 可以为 Vec<i32> 实现 Display
- ✅ 标准库提供常用接口实现
- ✅ 所有测试通过

### Phase 2 成功标准（可选）

- ✅ 支持 `support<T: Display> Display for Vec<T>`
- ✅ 约束检查正确
- ✅ 泛型实例化正确
- ✅ 所有测试通过

---

## 11. 时间表

| 阶段 | 任务 | 时间 | 状态 |
|------|------|------|------|
| Phase 1.1 | SymbolTable 扩展 | 1天 | 待开始 |
| Phase 1.2 | CodeGen 修改 | 2天 | 待开始 |
| Phase 1.3 | 标准库实现 | 2天 | 待开始 |
| Phase 1.4 | 测试 | 1天 | 待开始 |
| **Phase 1 总计** | | **6天** | |
| Phase 2.1 | Parser 扩展 | 2天 | 可选 |
| Phase 2.2 | 类型匹配 | 2天 | 可选 |
| Phase 2.3 | CodeGen 支持 | 2天 | 可选 |
| Phase 2.4 | 测试 | 1天 | 可选 |
| **Phase 2 总计** | | **7天** | |

**总计**：Phase 1（1周）+ Phase 2（1周，可选）

---

## 12. 总结

### 价值

- 🎯 **灵活性**：可以为任何类型实现接口
- 🔧 **扩展性**：用户可以扩展标准库类型
- 📚 **统一性**：所有类型通过接口统一处理
- 🚀 **实用性**：实现真正的多态和泛型编程

### 优先级

**Phase 1**：高（实用价值大）
**Phase 2**：中（锦上添花）

### 建议

先实现 Phase 1，验证基础功能和架构设计，再考虑 Phase 2 的泛型支持。

---

**🐾 设计完成，准备实施！**

*文档版本: v1.0*  
*创建日期: 2025-10-27*

