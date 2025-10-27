# Phase 3: 语义分析分离 - 设计文档

> 📐 **设计版本**: v1.0  
> 📅 **创建日期**: 2025-10-27  
> 🎯 **目标**: 将语义分析从 CodeGen 中独立出来

---

## 1. 背景与动机

### 1.1 当前问题

**CodeGen 职责过重**：
- CodeGen 既负责语义验证（类型检查、接口验证）
- 又负责 LLVM IR 生成
- 违反单一职责原则

**验证逻辑混杂**：
- `validateInterfaceImpl()` 在 `codegen_stmt.cpp` 中（~100 行）
- 类型检查逻辑散布在代码生成过程中
- 难以复用和扩展

**错误报告时机不一致**：
- 有些错误在解析时报告
- 有些错误在代码生成时报告
- 缺乏统一的语义分析阶段

### 1.2 目标

**建立独立的语义分析模块**：
- 语义分析集中在 `src/sema/` 目录
- 提供统一的 `SemanticAnalyzer` 接口
- 验证逻辑模块化

**改善编译流程**：
```
当前：Lexer → Parser → CodeGen（验证+生成）

目标：Lexer → Parser → SemanticAnalyzer（验证）→ CodeGen（只生成）
```

---

## 2. 设计架构

### 2.1 目录结构

```
src/sema/
├── semantic_analyzer.h/cpp  # 语义分析器（协调器）
├── type_checker.h/cpp       # 类型检查器
└── interface_validator.h/cpp # 接口验证器
```

### 2.2 核心类设计

#### 2.2.1 SemanticAnalyzer（协调器）

```cpp
class SemanticAnalyzer {
public:
    SemanticAnalyzer(TypeSystem* type_system, 
                    DiagnosticEngine* diagnostics,
                    SymbolTable* symbol_table);
    
    /**
     * 分析整个程序
     */
    bool analyze(const Program& program);
    
private:
    TypeSystem* type_system_;
    DiagnosticEngine* diagnostics_;
    SymbolTable* symbol_table_;
    
    std::unique_ptr<TypeChecker> type_checker_;
    std::unique_ptr<InterfaceValidator> interface_validator_;
    
    // 分析各种语句
    bool analyzeStmt(const Stmt* stmt);
    bool analyzeFunctionStmt(const FunctionStmt* stmt);
    bool analyzeStructStmt(const StructStmt* stmt);
    bool analyzeInterfaceStmt(const InterfaceStmt* stmt);
    bool analyzeSupportStmt(const SupportStmt* stmt);
};
```

#### 2.2.2 TypeChecker（类型检查器）

```cpp
class TypeChecker {
public:
    TypeChecker(TypeSystem* type_system, DiagnosticEngine* diagnostics);
    
    /**
     * 检查表达式类型
     */
    types::Type* checkExpr(const Expr* expr);
    
    /**
     * 检查语句类型
     */
    bool checkStmt(const Stmt* stmt);
    
    /**
     * 检查函数类型
     */
    bool checkFunction(const FunctionStmt* func);
    
private:
    TypeSystem* type_system_;
    DiagnosticEngine* diagnostics_;
};
```

#### 2.2.3 InterfaceValidator（接口验证器）

```cpp
class InterfaceValidator {
public:
    InterfaceValidator(TypeSystem* type_system, 
                      DiagnosticEngine* diagnostics,
                      SymbolTable* symbol_table);
    
    /**
     * 验证接口实现
     */
    bool validateImpl(const std::string& type_name,
                     const std::string& interface_name,
                     const std::vector<std::unique_ptr<FunctionStmt>>& methods,
                     const SourceLocation& location);
    
private:
    TypeSystem* type_system_;
    DiagnosticEngine* diagnostics_;
    SymbolTable* symbol_table_;
};
```

---

## 3. 迁移计划

### 3.1 阶段 1：创建 Sema 模块

**任务**：
- [ ] 创建 `src/sema/` 目录
- [ ] 创建 `semantic_analyzer.h/cpp`
- [ ] 创建 `type_checker.h/cpp`
- [ ] 创建 `interface_validator.h/cpp`
- [ ] 更新 `CMakeLists.txt`

**时间**：30 分钟

### 3.2 阶段 2：迁移 InterfaceValidator

**任务**：
- [ ] 从 `codegen_stmt.cpp` 复制 `validateInterfaceImpl()`
- [ ] 重构为 `InterfaceValidator::validateImpl()`
- [ ] 更新依赖（使用 DiagnosticEngine）
- [ ] 测试验证

**时间**：30 分钟

### 3.3 阶段 3：更新编译流程

**任务**：
- [ ] 在 `main.cpp` 中添加语义分析阶段
- [ ] 在 `module_compiler.cpp` 中添加语义分析
- [ ] 从 CodeGen 移除验证逻辑
- [ ] 测试所有示例

**时间**：30 分钟

---

## 4. 编译流程变化

### 当前流程

```cpp
// main.cpp
Parser parser(tokens, &diagnostics, input_file);
Program program = parser.parse();

CodeGenerator codegen(...);
codegen.generate(program);  // 验证 + 生成
```

### 新流程

```cpp
// main.cpp  
Parser parser(tokens, &diagnostics, input_file);
Program program = parser.parse();

// 新增：语义分析阶段
SemanticAnalyzer sema(&type_system, &diagnostics, &symbol_table);
if (!sema.analyze(program)) {
    return 1;  // 语义错误
}

// CodeGen 只负责生成
CodeGenerator codegen(...);
codegen.generate(program);  // 只生成，不验证
```

---

## 5. 风险评估

| 风险 | 等级 | 缓解措施 |
|-----|------|---------|
| 破坏现有验证逻辑 | 中 | 逐步迁移，充分测试 |
| 编译流程复杂化 | 低 | 清晰的阶段划分 |
| 增加编译时间 | 低 | 验证和生成分离不增加时间 |

---

## 6. 预期收益

- ✅ CodeGen 职责单一（只生成 IR）
- ✅ 语义分析逻辑集中
- ✅ 易于添加新的验证规则
- ✅ 符合编译器标准架构

---

**🐾 设计文档完成！准备开始实施！**

*文档版本: v1.0*  
*创建日期: 2025-10-27*

