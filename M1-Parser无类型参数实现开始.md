# M1: Parser支持无类型参数 - 实施开始

**预计时间**: 1小时  
**难度**: 🟡 中等

---

## 🎯 目标

**实现前**:
```paw
let f = (x: i32, y: i32) -> i32 { x + y };  // ✅ 必须有类型
let f = (x, y) -> i32 { x + y };            // ❌ 不支持
```

**实现后**:
```paw
let f = (x: i32, y: i32) -> i32 { x + y };  // ✅ 仍支持
let f = (x, y) -> i32 { x + y };            // ✅ 新支持（类型待推导）
```

---

## 🔍 当前状态

**ClosureExpr::Param结构**:
```cpp
struct Param {
    std::string name;
    Type* type;       // 可以为nullptr表示待推导
    bool is_mutable;
};
```

**已支持**: type字段可以为nullptr ✅

---

## 🔧 需要修改的位置

### 1. Parser::parseClosureParam()

需要找到解析闭包参数的代码并修改为：
```cpp
Type* param_type = nullptr;
if (match(TokenType::COLON)) {
    param_type = parseType();  // 有冒号，解析类型
}
// 无冒号，param_type保持nullptr，等待推导
```

---

## 📝 实施步骤

1. [x] 找到ClosureExpr定义 ✅
2. [ ] 找到Parser中解析closure的代码
3. [ ] 修改参数解析逻辑
4. [ ] 测试编译
5. [ ] 验证功能

**开始查找Parser代码...**

