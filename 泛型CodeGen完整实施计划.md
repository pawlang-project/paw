# 泛型CodeGen完整实施计划

**开始时间**: 2025-11-02（当前时刻）  
**预计时长**: 10-16小时  
**目标**: 完成所有泛型功能的CodeGen

---

## 📋 任务清单

### 1. 泛型Struct CodeGen ⏳ (2-3小时)
**目标**: `Box { value: 42 }` 构造可用

**需要实现**:
- TypeChecker处理泛型StructLiteral
- CodeGen复用现有struct构造逻辑

---

### 2. 泛型函数完整实现 ⏳ (5-8小时)
**目标**: `identity(42)` 或 `identity<i32>(42)` 可用

**需要实现**:
- Parser解析函数调用的泛型参数 `f<i32>(x)`
- TypeChecker类型参数推导
- 函数实例化
- CodeGen生成实例化函数

---

### 3. 泛型Interface完整实现 ⏳ (3-5小时)
**目标**: Support块可用

**需要实现**:
- Interface实例化
- Support块泛型参数处理
- TypeChecker集成
- CodeGen生成vtable

---

## 🎯 实施顺序

**阶段1**: 泛型Struct CodeGen (2-3h)  
**阶段2**: 泛型函数完整实现 (5-8h)  
**阶段3**: 泛型Interface（如有时间）(3-5h)

---

## 🚀 开始实施阶段1：泛型Struct CodeGen

