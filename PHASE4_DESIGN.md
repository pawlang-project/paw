# Phase 4: Pass-Based 架构 - 设计文档

> 📐 **设计版本**: v1.0  
> 📅 **创建日期**: 2025-10-27  
> 🎯 **目标**: 引入 Pass 抽象，建立灵活的编译流程

---

## 1. 背景与动机

### 1.1 当前问题

**编译流程固定**：
```cpp
Lexer → Parser → SemanticAnalyzer → CodeGen
```
- 硬编码的阶段顺序
- 难以插入新的分析/优化阶段
- 无法灵活配置

**缺乏抽象**：
- 每个阶段都是独立的类
- 没有统一的 Pass 接口
- 难以管理和组合

### 1.2 目标

**引入 Pass 抽象**：
```cpp
class CompilerPass {
    virtual bool run(Module* module) = 0;
    virtual const char* getName() const = 0;
};
```

**灵活的编译流程**：
```cpp
PassManager pm;
pm.addPass(std::make_unique<ModuleResolutionPass>());
pm.addPass(std::make_unique<SemanticAnalysisPass>());
pm.addPass(std::make_unique<CodeGenPass>());
pm.runAll(module);
```

---

## 2. 设计架构

### 2.1 目录结构

```
src/passes/
├── compiler_pass.h          # CompilerPass 基类
├── pass_manager.h/cpp       # PassManager 管理器
├── semantic_pass.h/cpp      # 语义分析 Pass
└── codegen_pass.h/cpp       # 代码生成 Pass
```

### 2.2 核心类设计

#### 2.2.1 CompilerPass（基类）

```cpp
class CompilerPass {
public:
    virtual ~CompilerPass() = default;
    
    /**
     * 运行 Pass
     */
    virtual bool run(CompilationUnit* unit) = 0;
    
    /**
     * 获取 Pass 名称
     */
    virtual const char* getName() const = 0;
    
    /**
     * 是否是分析 Pass（不修改 AST）
     */
    virtual bool isAnalysisPass() const { return false; }
};
```

#### 2.2.2 CompilationUnit（编译单元）

```cpp
struct CompilationUnit {
    std::string filename;
    Program* program;           // AST
    TypeSystem* type_system;
    DiagnosticEngine* diagnostics;
    SymbolTable* symbol_table;
    llvm::Module* ir_module;    // 生成的 IR
};
```

#### 2.2.3 PassManager（Pass 管理器）

```cpp
class PassManager {
public:
    void addPass(std::unique_ptr<CompilerPass> pass);
    
    bool runAll(CompilationUnit* unit);
    
    const std::vector<std::unique_ptr<CompilerPass>>& getPasses() const;
    
private:
    std::vector<std::unique_ptr<CompilerPass>> passes_;
};
```

---

## 3. Pass 实现

### 3.1 SemanticAnalysisPass

```cpp
class SemanticAnalysisPass : public CompilerPass {
public:
    bool run(CompilationUnit* unit) override {
        SemanticAnalyzer sema(
            unit->type_system,
            unit->diagnostics,
            unit->symbol_table
        );
        return sema.analyze(*unit->program);
    }
    
    const char* getName() const override {
        return "SemanticAnalysis";
    }
    
    bool isAnalysisPass() const override { return true; }
};
```

### 3.2 CodeGenPass

```cpp
class CodeGenPass : public CompilerPass {
public:
    bool run(CompilationUnit* unit) override {
        CodeGenerator codegen(unit->filename, unit->symbol_table);
        if (!codegen.generate(*unit->program)) {
            return false;
        }
        unit->ir_module = codegen.getModule();
        return true;
    }
    
    const char* getName() const override {
        return "CodeGen";
    }
};
```

---

## 4. 迁移计划

### 阶段 1：创建 Pass 框架

- [ ] 创建 `src/passes/` 目录
- [ ] 实现 `CompilerPass` 接口
- [ ] 实现 `CompilationUnit` 结构
- [ ] 实现 `PassManager`
- [ ] 更新 CMakeLists.txt

### 阶段 2：实现具体 Pass

- [ ] 实现 `SemanticAnalysisPass`
- [ ] 实现 `CodeGenPass`
- [ ] 测试 Pass 独立运行

### 阶段 3：更新主流程

- [ ] 修改 `main.cpp` 使用 PassManager
- [ ] 测试所有示例
- [ ] 提交到 Git

---

## 5. 预期收益

- ✅ 高度模块化的编译流程
- ✅ 易于添加新的分析/优化阶段
- ✅ 可配置的 Pass 顺序
- ✅ 便于单独测试和调试

---

**🐾 设计文档完成！准备实施！**

*文档版本: v1.0*  
*创建日期: 2025-10-27*

