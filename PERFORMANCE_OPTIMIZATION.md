# PawLang 性能优化计划

> 📊 **版本**: v1.0  
> 📅 **创建日期**: 2025-10-27  
> 🎯 **目标**: 全面优化编译器和生成代码的性能

---

## 1. 性能分析

### 1.1 当前状态

**编译器性能瓶颈**：
- 类型比较可能重复计算
- 符号表查找是线性的
- AST 遍历多次
- 没有缓存机制

**生成代码性能**：
- LLVM 优化级别：O0（默认）
- 没有启用优化 Pass
- 调试信息可能影响性能

---

## 2. 优化策略

### 2.1 编译器性能优化

#### 优化 1：类型系统缓存 ⭐⭐⭐
**问题**：类型比较和字符串化重复计算

**解决方案**：
```cpp
class TypeSystem {
    // 缓存类型字符串表示
    mutable std::unordered_map<Type*, std::string> string_cache_;
    
    // 缓存类型比较结果
    mutable std::unordered_map<std::pair<Type*, Type*>, bool> equals_cache_;
};
```

**预期收益**：30-50% 类型操作加速

#### 优化 2：符号表索引 ⭐⭐⭐
**问题**：符号查找是线性的

**解决方案**：
```cpp
class SymbolTable {
    std::unordered_map<std::string, Symbol*> symbol_index_;
    std::unordered_map<std::string, InterfaceStmt*> interface_index_;
};
```

**预期收益**：O(n) → O(1) 查找复杂度

#### 优化 3：AST 节点池 ⭐⭐
**问题**：频繁的内存分配/释放

**解决方案**：
```cpp
class ASTNodePool {
    std::vector<std::unique_ptr<char[]>> memory_blocks_;
    void* allocate(size_t size);
};
```

**预期收益**：20-30% 解析速度提升

### 2.2 代码生成优化

#### 优化 4：LLVM 优化 Pass ⭐⭐⭐
**问题**：生成的代码未优化

**解决方案**：
```cpp
// 添加 LLVM 优化 Pass
PassManager pm;
pm.add(createInstructionCombiningPass());
pm.add(createReassociatePass());
pm.add(createGVNPass());
pm.add(createCFGSimplificationPass());
```

**优化级别**：
- `-O0`：无优化（调试）
- `-O1`：基本优化
- `-O2`：标准优化 ⭐ 推荐
- `-O3`：激进优化
- `-Os`：大小优化

**预期收益**：2-5x 运行速度提升

#### 优化 5：内联优化 ⭐⭐
**问题**：小函数调用开销大

**解决方案**：
```cpp
// 标记内联候选
llvm::Function* func = ...;
func->addFnAttr(llvm::Attribute::AlwaysInline);
```

**预期收益**：10-20% 小函数密集代码加速

### 2.3 标准库优化

#### 优化 6：编译器内置函数 ⭐⭐
**问题**：常用函数（print, len）有调用开销

**解决方案**：
```cpp
// 直接生成 LLVM IR，不调用函数
if (func_name == "len") {
    // 对于字符串，直接读取长度字段
    // 对于数组，使用编译时常量
}
```

**预期收益**：特定操作 50-100% 加速

---

## 3. 实施计划

### Phase 1：基础优化（高优先级）⭐⭐⭐

**任务列表**：
- [ ] 为 `TypeSystem` 添加字符串缓存
- [ ] 为 `SymbolTable` 添加哈希索引
- [ ] 添加 `-O` 命令行选项
- [ ] 集成 LLVM 优化 Pass

**预计时间**：1-2 小时  
**预期收益**：2-3x 整体性能提升

### Phase 2：高级优化（中优先级）⭐⭐

**任务列表**：
- [ ] 实现类型比较结果缓存
- [ ] 添加内联优化标记
- [ ] 优化 AST 节点分配

**预计时间**：1-2 小时  
**预期收益**：额外 30-50% 提升

### Phase 3：极致优化（低优先级）⭐

**任务列表**：
- [ ] 实现 AST 节点池
- [ ] 添加编译器内置函数优化
- [ ] 实现增量编译

**预计时间**：2-3 小时  
**预期收益**：额外 20-30% 提升

---

## 4. 性能测试

### 4.1 测试基准

**编译器性能测试**：
```bash
# 大文件编译时间
time ./pawc large_file.paw -o test

# 批量编译
time for f in examples/*.paw; do ./pawc $f -o out; done
```

**生成代码性能测试**：
```paw
// fibonacci.paw - CPU 密集
fn fib(n: i32) -> i32 {
    if n <= 1 { return n; }
    return fib(n-1) + fib(n-2);
}

// 测试：time ./fib
```

### 4.2 性能指标

**编译器指标**：
- 编译速度（文件/秒）
- 内存使用（MB）
- 符号查找速度（查询/秒）

**生成代码指标**：
- 运行时间（秒）
- 可执行文件大小（KB）
- 内存占用（MB）

---

## 5. 优化前后对比

### 预期改进

| 指标 | 优化前 | 优化后 | 提升 |
|------|--------|--------|------|
| 编译速度 | 基准 | 1.5-2x | +50-100% |
| 运行速度（O2） | 基准 | 2-5x | +100-400% |
| 内存使用 | 基准 | 0.8-0.9x | -10-20% |
| 可执行文件大小 | 基准 | 0.7-0.9x | -10-30% |

---

## 6. 实施细节

### 6.1 命令行选项

```bash
# 优化级别
pawc input.paw -O0  # 无优化（调试）
pawc input.paw -O1  # 基本优化
pawc input.paw -O2  # 标准优化（默认）
pawc input.paw -O3  # 激进优化
pawc input.paw -Os  # 大小优化

# 性能分析
pawc input.paw --time       # 显示编译时间
pawc input.paw --profile    # 性能分析
```

### 6.2 配置文件

```toml
[optimization]
level = 2              # 0-3
inline_threshold = 100  # 内联大小阈值
cache_enabled = true    # 启用缓存
```

---

## 7. 风险与缓解

**风险**：
- 优化可能引入 bug
- 编译时间可能增加
- 调试困难

**缓解措施**：
- 保持 `-O0` 用于调试
- 全面的测试套件
- 详细的性能日志

---

**🐾 性能优化计划制定完成！**

*文档版本: v1.0*  
*创建日期: 2025-10-27*

