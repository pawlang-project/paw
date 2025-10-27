# Phase 2.5: 运行时泛型匹配设计

> 📊 **版本**: v1.0  
> 📅 **创建日期**: 2025-10-27  
> 🎯 **目标**: 实现运行时泛型接口方法调用

---

## 1. 问题分析

### 当前状态

**Phase 2 已完成**：
```paw
// ✅ 语法支持，可以编译
support<T: Display> Display for Box<T> {
    fn to_string(self) -> string { return "Box"; }
}
```

**Phase 2.5 要解决的问题**：
```paw
let b = Box { value: Point { x: 10, y: 20 } };
b.to_string();  // ❌ 无法找到匹配的实现
```

### 根本原因

**当前查找机制**（精确匹配）：
```
查找 "Box<Point>" 的实现
→ SymbolTable.interface_impls_["Box<Point>"]
→ 找不到（因为注册的是 "Box<T>"）
```

**需要实现**（模式匹配）：
```
查找 "Box<Point>" 的实现
→ 查找 "Box<T>" 的实现
→ 检查约束：Point 是否实现了 Display
→ 如果满足，使用该实现
```

---

## 2. 设计方案

### 2.1 类型模式解析

**输入**: 类型名字符串  
**输出**: 类型模式结构

```cpp
struct TypePattern {
    std::string base;                     // "Box", "Vec", "Pair"
    std::vector<std::string> args;       // ["Point"], ["Point", "i32"]
    bool has_generics;                   // true if has '<...>'
};

TypePattern parseTypePattern(const std::string& type_name);
```

**示例**：
```cpp
parseTypePattern("Box<Point>")    → { base: "Box", args: ["Point"], has_generics: true }
parseTypePattern("Pair<i32, f64>") → { base: "Pair", args: ["i32", "f64"], has_generics: true }
parseTypePattern("Box")           → { base: "Box", args: [], has_generics: false }
```

### 2.2 泛型匹配算法

**输入**: 具体类型名 + 泛型模式 + 约束  
**输出**: 是否匹配

```cpp
bool matchesGenericPattern(
    const std::string& concrete_type,  // "Box<Point>"
    const std::string& pattern,        // "Box<T>"
    const std::vector<std::string>& constraints  // ["Display"]
);
```

**算法**：
1. 解析两个类型的模式
2. 检查 base 是否相同
3. 检查参数数量是否相同
4. 检查每个参数是否满足约束（如果指定）

**示例**：
```cpp
matchesGenericPattern("Box<Point>", "Box<T>", ["Display"])
→ 解析: concrete = { base: "Box", args: ["Point"] }
→ 解析: pattern = { base: "Box", args: ["T"] }
→ 检查: base 相同 ✓
→ 检查: args 数量相同（1） ✓
→ 检查: "Point" 是否实现 "Display" ✓
→ 返回: true
```

### 2.3 约束检查

**输入**: 类型名 + 约束列表  
**输出**: 是否满足所有约束

```cpp
bool checkConstraints(
    const std::string& type_name,
    const std::vector<std::string>& constraints
);
```

**算法**：
1. 遍历每个约束
2. 检查 `type_name` 是否实现了该接口
3. 如果有任何一个约束不满足，返回 false

**示例**：
```cpp
checkConstraints("Point", ["Display", "Clone"])
→ 检查: "Point" 是否实现 "Display" → true
→ 检查: "Point" 是否实现 "Clone" → true
→ 返回: true
```

---

## 3. 实现计划

### 3.1 TypePattern 结构

**位置**: `src/module/symbol_table.h`

```cpp
struct TypePattern {
    std::string base;
    std::vector<std::string> args;
    bool has_generics;
    
    TypePattern() : has_generics(false) {}
};
```

### 3.2 辅助函数

**位置**: `src/module/symbol_table.cpp`

```cpp
// 解析类型模式
TypePattern parseTypePattern(const std::string& type_name);

// 泛型匹配
bool SymbolTable::matchesGenericPatternImpl(
    const std::string& concrete_type,
    const std::string& pattern,
    const std::vector<std::string>& constraints
) const;

// 约束检查
bool SymbolTable::checkConstraintsImpl(
    const std::string& type_name,
    const std::vector<std::string>& constraints
) const;
```

### 3.3 修改 getInterfaceImpl

**逻辑**：
1. 先尝试精确匹配
2. 如果失败，尝试泛型匹配
3. 对每个泛型实现，调用 `matchesGenericPatternImpl`
4. 返回第一个匹配的实现

```cpp
const SymbolTable::InterfaceImpl* SymbolTable::getInterfaceImpl(
    const std::string& type_name,
    const std::string& interface_name
) const {
    // 1. 精确匹配
    auto type_it = interface_impls_.find(type_name);
    if (type_it != interface_impls_.end()) {
        auto interface_it = type_it->second.find(interface_name);
        if (interface_it != type_it->second.end()) {
            return &interface_it->second;
        }
    }
    
    // 2. 泛型匹配（新增）
    auto generic_it = generic_impls_.find(interface_name);
    if (generic_it != generic_impls_.end()) {
        for (const auto& impl : generic_it->second) {
            if (matchesGenericPatternImpl(type_name, impl.type_name, impl.constraints)) {
                return &impl;
            }
        }
    }
    
    return nullptr;
}
```

---

## 4. 实施步骤

### Step 1: 添加 TypePattern 结构 ✅

### Step 2: 实现 parseTypePattern() 🔄

**算法**：
1. 查找 `<` 位置
2. 如果找到，分割 base 和 args
3. 解析 args（可能包含嵌套泛型）
4. 返回模式

**实现**：
```cpp
TypePattern SymbolTable::parseTypePattern(const std::string& type_name) const {
    TypePattern pattern;
    
    size_t lt_pos = type_name.find('<');
    
    if (lt_pos == std::string::npos) {
        // 没有泛型参数
        pattern.base = type_name;
        pattern.has_generics = false;
        return pattern;
    }
    
    // 有泛型参数
    pattern.base = type_name.substr(0, lt_pos);
    pattern.has_generics = true;
    
    // 提取泛型参数部分
    std::string args_str = type_name.substr(lt_pos + 1);
    args_str = args_str.substr(0, args_str.size() - 1);  // 去掉末尾的 '>'
    
    // 解析参数（处理嵌套泛型）
    std::vector<std::string> args;
    int depth = 0;
    std::string current_arg;
    
    for (char c : args_str) {
        if (c == '<') {
            depth++;
            current_arg += c;
        } else if (c == '>') {
            depth--;
            current_arg += c;
        } else if (c == ',' && depth == 0) {
            args.push_back(current_arg);
            current_arg = "";
        } else {
            current_arg += c;
        }
    }
    if (!current_arg.empty()) {
        args.push_back(current_arg);
    }
    
    pattern.args = args;
    return pattern;
}
```

### Step 3: 实现泛型匹配算法 ⏳

**实现**：
```cpp
bool SymbolTable::matchesGenericPatternImpl(
    const std::string& concrete_type,
    const std::string& pattern,
    const std::vector<std::string>& constraints
) const {
    // 解析两个类型
    TypePattern concrete_pattern = parseTypePattern(concrete_type);
    TypePattern pattern_parsed = parseTypePattern(pattern);
    
    // 检查 base 是否相同
    if (concrete_pattern.base != pattern_parsed.base) {
        return false;
    }
    
    // 检查参数数量
    if (concrete_pattern.args.size() != pattern_parsed.args.size()) {
        return false;
    }
    
    // 检查约束（如果有）
    if (!constraints.empty()) {
        for (const auto& arg : concrete_pattern.args) {
            if (!checkConstraintsImpl(arg, constraints)) {
                return false;
            }
        }
    }
    
    return true;
}
```

### Step 4: 实现约束检查 ⏳

**实现**：
```cpp
bool SymbolTable::checkConstraintsImpl(
    const std::string& type_name,
    const std::vector<std::string>& constraints
) const {
    for (const auto& constraint : constraints) {
        if (!typeImplementsInterface(type_name, constraint)) {
            return false;
        }
    }
    return true;
}
```

### Step 5: 修改 getInterfaceImpl() ⏳

**实现**：见 3.3

### Step 6: 测试 ⏳

**测试代码**：
```paw
support<T: Display> Display for Box<T> {
    fn to_string(self) -> string { return "Box"; }
}

type Point = struct(Display) {
    fn to_string(self) -> string { return "Point"; }
}

fn main() -> i32 {
    let b = Box { value: Point };
    println(b.to_string());  // 应该输出 "Box"
    return 0;
}
```

---

## 5. 时间表

| 步骤 | 预计时间 | 状态 |
|------|----------|------|
| Step 1: TypePattern 结构 | 10分钟 | ✅ 完成 |
| Step 2: parseTypePattern() | 1小时 | 🔄 进行中 |
| Step 3: matchesGenericPattern() | 2小时 | ⏳ 待开始 |
| Step 4: checkConstraints() | 30分钟 | ⏳ 待开始 |
| Step 5: 修改 getInterfaceImpl() | 1小时 | ⏳ 待开始 |
| Step 6: 测试 | 2小时 | ⏳ 待开始 |
| **总计** | **~7小时** | |

---

## 6. 风险和挑战

### 6.1 嵌套泛型

**问题**: `Box<Vec<Point>>` 这种嵌套泛型

**解决**: `parseTypePattern()` 使用 depth 计数正确处理

### 6.2 约束传播

**问题**: `T: Display`，但 `T = Vec<Point>` 时，需要递归检查

**解决**: `checkConstraintsImpl()` 可以递归调用自己

### 6.3 性能

**问题**: 泛型匹配可能较慢（需要遍历所有泛型实现）

**解决**: 先精确匹配，只有失败时才尝试泛型匹配

---

## 7. 成功标准

### 编译成功 ✅

测试代码能够成功编译

### 运行时正确 ✅

泛型接口方法调用能正确找到实现并执行

### 约束检查 ✅

如果约束不满足，编译器报告错误

---

**🐾 设计完成，开始实施！**

*文档版本: v1.0*  
*创建日期: 2025-10-27*

