# 泛型函数和Interface完整实现 - 重新启动

**时间**: 2025-11-02（续）  
**策略**: 简化方案，分步验证

---

## 🎯 核心问题分析

### 当前状态
- ✅ Parser可以解析泛型调用语法
- ✅ TypeChecker可以推导返回类型
- ❌ 错误: "Undefined identifier: i32"

### 问题根因
这个错误发生在**编译阶段**，说明：
1. Parser解析 `identity<i32>(42)` 时，`i32`可能被错误处理
2. 或者TypeChecker在检查函数体时出错

---

## 🔍 诊断步骤

### 步骤1: 验证Parser是否正确解析
测试最简单的泛型函数声明（不调用）：
```paw
fn identity<T>(x: T) -> T {
    return x;
}

fn main() {
    println(42);  // 不调用泛型函数
}
```

### 步骤2: 验证TypeChecker
如果步骤1成功，说明解析没问题，问题在TypeChecker或CodeGen

### 步骤3: 简化泛型函数实现方案

**新方案**: 不在CodeGen阶段实例化，而是：
1. 在Monomorphization Pass中实例化
2. 或者先实现非泛型函数的功能，确保基础正确

---

## 📋 新的实施计划

### Phase 1: 基础验证 (30分钟)
1. 测试泛型函数定义（不调用）
2. 确认Parser和AST正确
3. 确认TypeChecker不崩溃

### Phase 2: 简化实现 (2-3小时)
采用**最简单**的实现：
- 先跳过泛型函数CodeGen
- 专注完成Interface实现
- 回头再处理泛型函数

### Phase 3: Interface完整实现 (4-6小时)
1. Interface实例化
2. Support块集成
3. CodeGen vtable

---

## 🚀 立即开始！

