# PawLang 性能优化 Phase 2 - 深度优化

> 📊 **版本**: v2.0  
> 📅 **创建日期**: 2025-10-27  
> 🎯 **目标**: 实施高级性能优化，进一步提升编译器和运行时性能

---

## 1. Phase 2 优化目标

### 已完成（Phase 1）
- ✅ TypeSystem 字符串缓存
- ✅ SymbolTable 索引结构
- ✅ -O0/1/2/3/s 命令行支持

### Phase 2 任务
- [ ] 类型比较结果缓存
- [ ] LLVM 优化 Pass 集成
- [ ] 内联优化标记
- [ ] 符号表哈希查找实现

---

## 2. 优化 1：类型比较结果缓存

### 问题分析
类型比较（`TypeSystem::equals`）在接口验证、类型检查中频繁调用，相同的类型对可能被比较多次。

### 解决方案
```cpp
class TypeSystem {
private:
    // 缓存类型比较结果
    struct TypePair {
        const types::Type* a;
        const types::Type* b;
        
        bool operator==(const TypePair& other) const {
            return a == other.a && b == other.b;
        }
    };
    
    struct TypePairHash {
        size_t operator()(const TypePair& p) const {
            return std::hash<const void*>()(p.a) ^ 
                   (std::hash<const void*>()(p.b) << 1);
        }
    };
    
    mutable std::unordered_map<TypePair, bool, TypePairHash> equals_cache_;
};
```

### 预期收益
- **30-50% 类型比较加速**
- 减少递归比较次数
- 特别适合复杂泛型类型

---

## 3. 优化 2：LLVM 优化 Pass

### 问题分析
当前生成的 LLVM IR 没有经过优化，运行效率低。

### 解决方案
```cpp
// 在 CodeGenerator 中添加优化
void CodeGenerator::applyOptimizations(int level) {
    if (level == 0) return;  // -O0: 无优化
    
    llvm::PassBuilder PB;
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;
    
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
    
    llvm::ModulePassManager MPM;
    
    switch (level) {
    case 1:  // -O1
        MPM = PB.buildPerModuleDefaultPipeline(
            llvm::OptimizationLevel::O1);
        break;
    case 2:  // -O2
        MPM = PB.buildPerModuleDefaultPipeline(
            llvm::OptimizationLevel::O2);
        break;
    case 3:  // -O3
        MPM = PB.buildPerModuleDefaultPipeline(
            llvm::OptimizationLevel::O3);
        break;
    case -1: // -Os
        MPM = PB.buildPerModuleDefaultPipeline(
            llvm::OptimizationLevel::Os);
        break;
    }
    
    MPM.run(*module_, MAM);
}
```

### 预期收益
- **2-5x 运行时性能提升**（-O2）
- **5-10x 提升**（-O3，某些场景）
- 更小的可执行文件（-Os）

---

## 4. 优化 3：符号表哈希查找

### 问题分析
当前 `SymbolTable::lookup` 是线性搜索，O(n) 复杂度。

### 解决方案
```cpp
// 在注册时更新索引
void SymbolTable::registerFunction(...) {
    // 原有逻辑...
    
    // 更新索引
    std::string full_name = module + "::" + name;
    symbol_index_[full_name] = &module_symbols_[module][name];
}

// 快速查找
Symbol* SymbolTable::lookupFast(const std::string& module, 
                                const std::string& name) {
    std::string full_name = module + "::" + name;
    auto it = symbol_index_.find(full_name);
    return it != symbol_index_.end() ? it->second : nullptr;
}
```

### 预期收益
- **O(n) → O(1) 查找**
- 大型项目中显著加速
- 减少符号解析时间

---

## 5. 实施计划

### 任务列表
- [ ] 实现类型比较缓存
- [ ] 集成 LLVM 优化 Pass
- [ ] 实现符号表快速查找
- [ ] 性能基准测试
- [ ] 提交到 Git

### 预计时间
~1.5-2 小时

### 预期总体提升
- 编译速度：+20-30%
- 运行速度：+200-400%（-O2/O3）
- 内存使用：-5-10%

---

**🐾 Phase 2 设计完成，开始实施！**

*文档版本: v2.0*  
*创建日期: 2025-10-27*

