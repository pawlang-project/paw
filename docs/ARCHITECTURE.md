# PawLang 编译器完整架构设计 (v1.4.0)

> 🏗️ **软件架构规划与设计指南**  
> 本文档定义了 PawLang 编译器的完整架构、设计原则、实现细节和未来演进方向

**版本**: v1.4.0  
**最后更新**: 2025-10-31  
**状态**: 架构设计完成，待实施

---

## 📋 目录

- [1. 架构概览](#1-架构概览)
- [2. 项目结构](#2-项目结构)
- [3. 核心架构设计](#3-核心架构设计)
- [4. Pass系统架构](#4-pass系统架构)
- [5. 前端架构](#5-前端架构)
- [6. 中端架构](#6-中端架构)
- [7. 后端架构](#7-后端架构)
- [8. 支持系统](#8-支持系统)
- [9. 内存管理](#9-内存管理)
- [10. 构建系统](#10-构建系统)
- [11. 测试架构](#11-测试架构)
- [12. 性能优化](#12-性能优化)
- [13. 实施路线](#13-实施路线)

---

## 1. 架构概览

### 1.1 总体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                     PawLang 编译器 (pawc)                        │
│                     v1.4.0 - Pass-Based架构                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌───────────────┐   ┌────────────────┐   ┌─────────────────┐ │
│  │   Frontend    │──▶│   Middleend    │──▶│    Backend      │ │
│  │               │   │                │   │                 │ │
│  │ Lexer+Parser  │   │ Sema+Types     │   │ CodeGen+LLVM    │ │
│  └───────────────┘   └────────────────┘   └─────────────────┘ │
│         │                    │                      │          │
│         ▼                    ▼                      ▼          │
│      Tokens               AST+Types              LLVM IR       │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│                         Pass Manager                            │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │  Analysis → Transform → Optimization → CodeGen          │  │
│  │  可配置、可扩展、支持并行                                │  │
│  └─────────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                        支撑系统                                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐      │
│  │TypeSystem│  │SymbolTbl │  │Diagnostic│  │ModuleLoad│      │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘      │
├─────────────────────────────────────────────────────────────────┤
│                      LLVM工具链（集成）                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                     │
│  │  LLVM 21 │  │  Clang   │  │   LLD    │                     │
│  └──────────┘  └──────────┘  └──────────┘                     │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 核心特性

```
✅ Pass-Based架构      - 模块化、可扩展、可并行
✅ 无GC内存管理        - RAII + 栈为主 + 显式堆分配
✅ 完整类型系统        - 28种类型 + 泛型 + 接口
✅ 强大的模式匹配      - 穷尽性检查 + 结构体解构
✅ LLVM后端           - 优化 + 多平台支持
✅ 静态链接           - 无外部依赖的独立二进制
```

### 1.3 编译流程

```
Source Code (.paw)
      ↓
  [Lexer] ────────────────────── Token流
      ↓
  [Parser] ───────────────────── AST
      ↓
╔═══════════════════════════════════════════════╗
║            Pass Manager 执行流程               ║
╠═══════════════════════════════════════════════╣
║                                               ║
║  [ModuleResolutionPass]                       ║
║      ↓                                        ║
║  [TypeInferencePass]                          ║
║      ↓                                        ║
║  [TypeCheckPass]                              ║
║      ↓                                        ║
║  [InterfaceValidationPass]                    ║
║      ↓                                        ║
║  [PatternExhaustivenessPass]                  ║
║      ↓                                        ║
║  ────────── 转换Pass ──────────               ║
║      ↓                                        ║
║  [GenericInstantiationPass]                   ║
║      ↓                                        ║
║  [ConstantFoldingPass] (可选)                 ║
║      ↓                                        ║
║  [DeadCodeEliminationPass] (可选)             ║
║      ↓                                        ║
║  ────────── 代码生成 ──────────               ║
║      ↓                                        ║
║  [LLVMIRGenPass]                              ║
║      ↓                                        ║
║  [LLVMOptimizationPass] (可选)                ║
║      ↓                                        ║
║  [ObjectGenPass]                              ║
║                                               ║
╚═══════════════════════════════════════════════╝
      ↓
  Executable
```

---

## 2. 项目结构

### 2.1 完整目录结构

```
paw/
├── llvm/                    # LLVM 21源码（子项目）
├── clang/                   # Clang源码
├── lld/                     # LLD链接器
│
├── cmake/                   # LLVM通用CMake工具
│   ├── Modules/             # 14个CMake模块
│   │   ├── LLVMVersion.cmake
│   │   ├── LLVMCheckCompilerLinkerFlag.cmake
│   │   └── ...
│   └── README.rst
│
├── third-party/             # 第三方依赖
│   └── siphash/             # SipHash哈希算法（header-only）
│       └── include/siphash/
│           └── SipHash.h
│
├── src/                     # PawLang编译器源码
│   ├── frontend/            # 前端
│   │   ├── lexer/
│   │   │   ├── token.h               # Token定义
│   │   │   ├── lexer.h/cpp          # 词法分析器
│   │   │   └── CMakeLists.txt
│   │   │
│   │   ├── parser/
│   │   │   ├── ast/                  # AST定义（分模块）
│   │   │   │   ├── ast_base.h       # AST基类
│   │   │   │   ├── expr.h           # 表达式节点
│   │   │   │   ├── stmt.h           # 语句节点
│   │   │   │   ├── decl.h           # 声明节点
│   │   │   │   ├── type.h           # 类型节点
│   │   │   │   └── pattern.h        # 模式节点
│   │   │   ├── visitor.h            # Visitor接口
│   │   │   ├── parser.h/cpp         # 语法分析器
│   │   │   └── CMakeLists.txt
│   │   │
│   │   └── module/
│   │       ├── module.h/cpp         # 模块定义
│   │       ├── module_loader.h/cpp  # 模块加载
│   │       └── CMakeLists.txt
│   │
│   ├── middleend/           # 中端
│   │   ├── sema/                     # 语义分析
│   │   │   ├── semantic_context.h/cpp
│   │   │   ├── type_checker.h/cpp
│   │   │   ├── type_inference.h/cpp
│   │   │   ├── interface_validator.h/cpp
│   │   │   ├── pattern_checker.h/cpp
│   │   │   └── CMakeLists.txt
│   │   │
│   │   ├── types/                    # 类型系统
│   │   │   ├── type_system.h/cpp
│   │   │   ├── primitive_types.h/cpp
│   │   │   ├── composite_types.h/cpp
│   │   │   ├── generic_types.h/cpp
│   │   │   ├── type_registry.h/cpp
│   │   │   └── CMakeLists.txt
│   │   │
│   │   ├── symbol/                   # 符号表
│   │   │   ├── symbol_table.h/cpp
│   │   │   ├── scope.h/cpp
│   │   │   └── CMakeLists.txt
│   │   │
│   │   └── CMakeLists.txt
│   │
│   ├── backend/             # 后端
│   │   ├── codegen/                  # 代码生成核心
│   │   │   ├── codegen_context.h/cpp
│   │   │   ├── codegen_base.h/cpp
│   │   │   ├── llvm_context.h/cpp
│   │   │   │
│   │   │   ├── expr/                 # 表达式代码生成
│   │   │   │   ├── expr_codegen.h/cpp
│   │   │   │   ├── literal_codegen.cpp
│   │   │   │   ├── binary_codegen.cpp
│   │   │   │   ├── call_codegen.cpp
│   │   │   │   └── match_codegen.cpp
│   │   │   │
│   │   │   ├── stmt/                 # 语句代码生成
│   │   │   │   ├── stmt_codegen.h/cpp
│   │   │   │   ├── function_codegen.cpp
│   │   │   │   └── control_flow_codegen.cpp
│   │   │   │
│   │   │   ├── type/                 # 类型代码生成
│   │   │   │   ├── type_codegen.h/cpp
│   │   │   │   ├── struct_codegen.cpp
│   │   │   │   ├── enum_codegen.cpp
│   │   │   │   ├── optional_codegen.cpp
│   │   │   │   └── result_codegen.cpp
│   │   │   │
│   │   │   ├── generic/              # 泛型代码生成
│   │   │   │   ├── generic_instantiator.h/cpp
│   │   │   │   ├── monomorphization.h/cpp
│   │   │   │   └── mangling.h/cpp
│   │   │   │
│   │   │   ├── interface/            # 接口代码生成
│   │   │   │   └── interface_codegen.h/cpp
│   │   │   │
│   │   │   ├── memory/               # 内存管理
│   │   │   │   └── memory_manager.h/cpp
│   │   │   │
│   │   │   └── CMakeLists.txt
│   │   │
│   │   ├── builtins/                 # 内建函数
│   │   │   ├── builtin_registry.h/cpp
│   │   │   ├── io_builtins.cpp
│   │   │   ├── array_builtins.cpp
│   │   │   └── CMakeLists.txt
│   │   │
│   │   ├── target/                   # 目标平台
│   │   │   ├── target_info.h/cpp
│   │   │   ├── object_writer.h/cpp
│   │   │   └── CMakeLists.txt
│   │   │
│   │   └── CMakeLists.txt
│   │
│   ├── pass/                # Pass系统核心
│   │   ├── pass.h                    # Pass基类（CRTP）
│   │   ├── pass_manager.h/cpp        # Pass管理器
│   │   ├── pass_context.h/cpp        # Pass上下文
│   │   └── CMakeLists.txt
│   │
│   ├── passes/              # 具体Pass实现
│   │   ├── analysis/
│   │   │   ├── module_resolution_pass.h/cpp
│   │   │   ├── type_inference_pass.h/cpp
│   │   │   ├── type_check_pass.h/cpp
│   │   │   ├── interface_validation_pass.h/cpp
│   │   │   └── pattern_exhaustiveness_pass.h/cpp
│   │   │
│   │   ├── transform/
│   │   │   ├── generic_instantiation_pass.h/cpp
│   │   │   ├── constant_folding_pass.h/cpp
│   │   │   └── dead_code_elimination_pass.h/cpp
│   │   │
│   │   ├── codegen/
│   │   │   ├── llvm_ir_gen_pass.h/cpp
│   │   │   ├── llvm_optimization_pass.h/cpp
│   │   │   └── object_gen_pass.h/cpp
│   │   │
│   │   └── CMakeLists.txt
│   │
│   ├── diagnostics/         # 诊断系统
│   │   ├── diagnostic_engine.h/cpp
│   │   ├── diagnostic.h
│   │   ├── source_manager.h/cpp
│   │   ├── source_location.h
│   │   └── CMakeLists.txt
│   │
│   ├── utils/               # 工具库
│   │   ├── arena.h/cpp              # 内存池
│   │   ├── string_interner.h/cpp    # 字符串驻留
│   │   ├── hash.h                   # 哈希（使用SipHash）
│   │   ├── colors.h/cpp             # 颜色输出
│   │   └── CMakeLists.txt
│   │
│   ├── driver/              # 编译器驱动
│   │   ├── compiler.h/cpp           # 编译器主类
│   │   ├── driver.h/cpp             # 命令行驱动
│   │   ├── options.h/cpp            # 编译选项
│   │   ├── main.cpp                 # 入口点
│   │   └── CMakeLists.txt
│   │
│   └── runtime/             # 运行时库（无GC）
│       ├── panic.h/cpp              # panic处理
│       ├── builtin_runtime.h/cpp    # 运行时辅助
│       └── CMakeLists.txt
│
├── include/                 # 公共头文件
│   └── pawc/
│       ├── version.h.in             # 版本信息模板
│       └── config.h.in              # 配置信息模板
│
├── examples/                # 示例程序
│   ├── hello.paw
│   ├── generics.paw
│   └── ...
│
├── docs/                    # 文档
│   ├── PawLang完整语法报告.md
│   ├── PawLang完整类型系统报告.md
│   └── ...
│
├── tests/                   # 测试
│   ├── unit/                # 单元测试
│   ├── integration/         # 集成测试
│   └── e2e/                 # 端到端测试
│
├── build                    # 构建脚本
├── build.bat                # Windows构建脚本
├── CMakeLists.txt           # 主CMake配置
├── .gitignore
├── README.md
└── paw.toml
```

### 2.2 模块依赖关系

```
                   ┌─────────┐
                   │ driver  │
                   └────┬────┘
                        │
           ┌────────────┼────────────┐
           ↓            ↓            ↓
      ┌────────┐   ┌────────┐   ┌────────┐
      │frontend│   │middleend│   │backend │
      └───┬────┘   └────┬────┘   └────┬───┘
          │             │              │
          ↓             ↓              ↓
      ┌────────────────────────────────────┐
      │         支撑系统                    │
      │  diagnostics + utils + pass        │
      └────────────────────────────────────┘
                      ↓
      ┌────────────────────────────────────┐
      │         LLVM工具链                  │
      │  LLVM + Clang + LLD + SipHash      │
      └────────────────────────────────────┘
```

---

## 3. 核心架构设计

### 3.1 设计原则

#### SOLID原则

```cpp
// 1. 单一职责原则 (SRP)
class CodeGenerator {
    void generateStmt(...);  // 只负责代码生成
};

class TypeChecker {
    bool checkType(...);     // 只负责类型检查
};

// 2. 开闭原则 (OCP)
class CompilerPass {
    virtual bool run(Module*) = 0;  // 对扩展开放
};

// 3. 里氏替换原则 (LSP)
class TypeCheckPass : public PassBase<TypeCheckPass> {
    // 可以替换父类
};

// 4. 接口隔离原则 (ISP)
class ASTVisitor {
    virtual void visit(Expr*) = 0;
    virtual void visit(Stmt*) = 0;
    // 接口最小化
};

// 5. 依赖倒置原则 (DIP)
class CodeGenPass {
    TypeSystem* type_system_;        // 依赖抽象
    DiagnosticEngine* diagnostics_;  // 不依赖具体实现
};
```

#### 架构原则

1. **分层清晰**: Frontend → Middleend → Backend
2. **接口明确**: 每个模块有清晰的公共接口
3. **低耦合**: 模块间通过接口通信
4. **高内聚**: 相关功能集中在同一模块
5. **易测试**: 每个模块可独立单元测试

### 3.2 核心架构模式

#### Pass-Based架构

```cpp
// Pass基类（使用CRTP优化虚函数调用）
template<typename Derived>
class PassBase {
public:
    PassResult run(Module* module, PassContext& context) {
        return static_cast<Derived*>(this)->runImpl(module, context);
    }
    
    virtual const char* getName() const = 0;
    virtual PassKind getKind() const = 0;
    virtual bool isThreadSafe() const { return false; }
};

// PassManager
class PassManager {
    std::vector<std::unique_ptr<PassHolder>> passes_;
    
public:
    template<typename PassT>
    void addPass(Args&&... args);
    
    bool runAll(Module* module);
    bool runParallel(Module* module);  // 并行执行
};
```

#### Visitor模式（AST遍历）

```cpp
class ASTVisitor {
public:
    virtual void visit(IntLiteral*) = 0;
    virtual void visit(BinaryExpr*) = 0;
    virtual void visit(FunctionStmt*) = 0;
    // ...
};

class TypeChecker : public ASTVisitor {
    void visit(BinaryExpr* node) override {
        // 类型检查逻辑
    }
};
```

---

## 4. Pass系统架构

### 4.1 Pass基础设施

#### Pass基类

```cpp
// src/pass/pass.h

template<typename Derived>
class PassBase {
public:
    PassResult run(Module* module, PassContext& context);
    
    virtual const char* getName() const = 0;
    virtual const char* getDescription() const { return ""; }
    
    enum class PassKind {
        Analysis,    // 只分析，不修改
        Transform,   // 转换优化
        CodeGen      // 代码生成
    };
    virtual PassKind getKind() const = 0;
    
    // 依赖管理
    virtual std::vector<const char*> getRequiredPasses() const {
        return {};
    }
    
    // 并行化支持
    virtual bool isThreadSafe() const { return false; }
};

// 便利宏
#define DEFINE_PASS(ClassName, Kind) \
    bool runImpl(Module* module, PassContext& context); \
    const char* getName() const override { return #ClassName; } \
    PassKind getKind() const override { return PassKind::Kind; }
```

#### PassContext

```cpp
// src/pass/pass_context.h

class PassContext {
    DiagnosticEngine* diagnostics_;
    SymbolTable* symbol_table_;
    TypeSystem* type_system_;
    
    // Pass结果缓存
    std::unordered_map<std::string, std::any> analysis_cache_;
    
public:
    DiagnosticEngine* getDiagnostics();
    TypeSystem* getTypeSystem();
    SymbolTable* getSymbolTable();
    
    // 缓存管理
    template<typename T>
    void cacheAnalysisResult(const std::string& pass, const T& result);
    
    template<typename T>
    bool getCachedResult(const std::string& pass, T& result);
};
```

#### PassManager

```cpp
// src/pass/pass_manager.h

class PassManager {
    std::vector<std::unique_ptr<PassHolder>> passes_;
    PassContext* context_;
    
public:
    template<typename PassT, typename... Args>
    void addPass(Args&&... args);
    
    bool runAll(Module* module);
    bool runParallel(Module* module);  // 并行执行
    
    void printStatistics() const;
    
private:
    std::vector<size_t> topologicalSort();  // 依赖排序
    std::vector<std::vector<size_t>> computeParallelGroups();
};
```

### 4.2 Pass执行流程

```cpp
// driver/compiler.cpp

bool Compiler::compile(const std::string& source_file) {
    // 创建Pass上下文
    PassContext context(&diagnostics_, &symbol_table_, &type_system_);
    context.setOptLevel(options_.opt_level);
    context.setVerbose(options_.verbose);
    
    // 创建PassManager
    PassManager pm(&context);
    
    // === 分析Pass ===
    pm.addPass<ModuleResolutionPass>(&module_loader_);
    pm.addPass<TypeInferencePass>();
    pm.addPass<TypeCheckPass>();
    pm.addPass<InterfaceValidationPass>();
    pm.addPass<PatternExhaustivenessPass>();
    
    // === 转换Pass ===
    pm.addPass<GenericInstantiationPass>();
    
    if (options_.opt_level > 0) {
        pm.addPass<ConstantFoldingPass>();
        pm.addPass<DeadCodeEliminationPass>();
    }
    
    // === 代码生成Pass ===
    pm.addPass<LLVMIRGenPass>();
    
    if (options_.opt_level > 0) {
        pm.addPass<LLVMOptimizationPass>();
    }
    
    pm.addPass<ObjectGenPass>(options_.output_file);
    
    // 运行所有Pass
    return pm.runAll(module_);
}
```

---

## 5. 前端架构

### 5.1 词法分析器（Lexer）

```cpp
// src/frontend/lexer/lexer.h

class Lexer {
    std::string source_;
    size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
    
public:
    explicit Lexer(const std::string& source);
    
    std::vector<Token> tokenize();
    Token nextToken();
    
private:
    char peek();
    char advance();
    void skipWhitespace();
    Token number();
    Token string();
    Token identifier();
};
```

### 5.2 语法分析器（Parser）

```cpp
// src/frontend/parser/parser.h

class Parser {
    std::vector<Token> tokens_;
    size_t current_ = 0;
    DiagnosticEngine* diagnostics_;
    Arena* arena_;  // AST节点内存池
    
public:
    Parser(std::vector<Token> tokens, DiagnosticEngine* diag);
    
    std::unique_ptr<Module> parse();
    
private:
    // 表达式解析
    ExprPtr expression();
    ExprPtr primary();
    ExprPtr binary();
    ExprPtr match();
    
    // 语句解析
    StmtPtr statement();
    StmtPtr functionDecl();
    StmtPtr typeDecl();
    
    // 错误恢复
    void synchronize();
};
```

### 5.3 AST定义

```cpp
// src/frontend/parser/ast/ast_base.h

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor* visitor) = 0;
    
    SourceLocation getLocation() const { return location_; }
    
protected:
    SourceLocation location_;
};

// src/frontend/parser/ast/expr.h
class Expr : public ASTNode {
public:
    Type* getType() const { return type_; }
    void setType(Type* type) { type_ = type; }
    
private:
    Type* type_ = nullptr;
};

class IntLiteral : public Expr { ... };
class BinaryExpr : public Expr { ... };
class CallExpr : public Expr { ... };
```

---

## 6. 中端架构

### 6.1 类型系统

```cpp
// src/middleend/types/type_system.h

class Type {
public:
    enum Kind {
        Primitive,   // i32, f64, bool, char, string
        Named,       // 用户定义类型
        Array,       // [T; N]
        Slice,       // [T]
        Tuple,       // (T1, T2, ...)
        Optional,    // T?
        Result,      // T!
        Reference,   // &T, &~T
        Function,    // fn(T1, T2) -> T3
        Generic,     // T (未实例化)
    };
    
    virtual Kind getKind() const = 0;
    virtual bool equals(const Type* other) const = 0;
    virtual std::string toString() const = 0;
};

class TypeSystem {
    std::map<std::string, Type*> named_types_;
    std::vector<std::unique_ptr<Type>> type_pool_;
    
public:
    // 类型创建
    Type* getPrimitiveType(const std::string& name);
    Type* getArrayType(Type* element, int size);
    Type* getOptionalType(Type* inner);
    Type* getResultType(Type* ok_type);
    
    // 类型查询
    Type* lookupType(const std::string& name);
    bool isAssignable(Type* from, Type* to);
    bool equals(Type* t1, Type* t2);
    
    // 类型注册
    void registerType(const std::string& name, Type* type);
};
```

### 6.2 语义分析

```cpp
// src/middleend/sema/type_checker.h

class TypeChecker : public ASTVisitor {
    TypeSystem* type_system_;
    DiagnosticEngine* diagnostics_;
    
public:
    void visit(BinaryExpr* node) override {
        node->left->accept(this);
        node->right->accept(this);
        
        Type* left_type = node->left->getType();
        Type* right_type = node->right->getType();
        
        if (!type_system_->areCompatible(left_type, right_type)) {
            diagnostics_->reportError(
                "Type mismatch in binary expression",
                node->getLocation()
            );
        }
    }
};
```

### 6.3 符号表

```cpp
// src/middleend/symbol/symbol_table.h

class SymbolTable {
    struct Scope {
        std::unordered_map<std::string, Symbol*> symbols;
        Scope* parent = nullptr;
    };
    
    Scope* current_scope_;
    std::vector<std::unique_ptr<Scope>> scopes_;
    
public:
    void enterScope();
    void exitScope();
    
    void define(const std::string& name, Symbol* symbol);
    Symbol* lookup(const std::string& name);
    
    void defineFunction(const std::string& name, FunctionSymbol* func);
    void defineType(const std::string& name, TypeSymbol* type);
};
```

---

## 7. 后端架构

### 7.1 CodeGen上下文

```cpp
// src/backend/codegen/codegen_context.h

class CodeGenContext {
    std::unique_ptr<llvm::LLVMContext> llvm_context_;
    std::unique_ptr<llvm::Module> llvm_module_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;
    
    TypeSystem* type_system_;
    SymbolTable* symbol_table_;
    
    // 类型映射缓存
    std::unordered_map<const Type*, llvm::Type*> type_cache_;
    
    // 符号表
    std::vector<Scope> scope_stack_;
    std::unordered_map<std::string, llvm::Function*> functions_;
    
    // 泛型实例缓存
    std::unordered_map<std::string, GenericInstance> generic_cache_;
    
public:
    CodeGenContext(TypeSystem* ts, SymbolTable* st, 
                   const std::string& module_name);
    
    llvm::LLVMContext& getLLVMContext();
    llvm::Module* getModule();
    llvm::IRBuilder<>& getBuilder();
    
    // 作用域管理
    void pushScope();
    void popScope();
    void defineLocal(const std::string& name, llvm::Value* value);
    llvm::Value* lookupLocal(const std::string& name);
    
    // 类型映射
    llvm::Type* getLLVMType(const Type* paw_type);
};
```

### 7.2 表达式代码生成

```cpp
// src/backend/codegen/expr/expr_codegen.h

class ExprCodeGen : public CodeGenBase {
public:
    llvm::Value* generate(ASTNode* node) override;
    
    llvm::Value* generateIntLiteral(IntLiteral* node);
    llvm::Value* generateBinaryExpr(BinaryExpr* node);
    llvm::Value* generateCallExpr(CallExpr* node);
    llvm::Value* generateMatchExpr(MatchExpr* node);
    
    // Optional/Result类型
    llvm::Value* generateOptionalValue(Expr* node);
    llvm::Value* generateResultValue(Expr* node);
    llvm::Value* generateTryExpr(TryExpr* node);
};
```

### 7.3 类型代码生成

```cpp
// src/backend/codegen/type/type_codegen.h

class TypeCodeGen {
    CodeGenContext* context_;
    
public:
    llvm::Type* mapType(const Type* paw_type);
    
    llvm::Type* mapPrimitiveType(const PrimitiveType* type);
    llvm::StructType* mapStructType(const StructType* type);
    llvm::Type* mapEnumType(const EnumType* type);
    llvm::StructType* mapOptionalType(const OptionalType* type);
    llvm::StructType* mapResultType(const ResultType* type);
};
```

### 7.4 泛型单态化

```cpp
// src/backend/codegen/generic/monomorphization.h

class Monomorphization {
    CodeGenContext* context_;
    
public:
    // 泛型函数实例化
    llvm::Function* instantiateFunction(
        FunctionStmt* generic_func,
        const std::vector<Type*>& type_args
    );
    
    // 泛型类型实例化
    llvm::Type* instantiateType(
        TypeDecl* generic_type,
        const std::vector<Type*>& type_args
    );
    
private:
    std::string mangleName(const std::string& name,
                          const std::vector<Type*>& type_args);
};
```

---

## 8. 支持系统

### 8.1 诊断系统

```cpp
// src/diagnostics/diagnostic_engine.h

enum class DiagnosticLevel {
    Note, Warning, Error, Fatal
};

class DiagnosticEngine {
    std::vector<Diagnostic> diagnostics_;
    SourceManager* source_manager_;
    bool has_errors_ = false;
    
public:
    void report(DiagnosticLevel level,
               const std::string& message,
               SourceLocation loc);
    
    void reportError(const std::string& msg, SourceLocation loc);
    void reportWarning(const std::string& msg, SourceLocation loc);
    
    bool hasErrors() const { return has_errors_; }
    void printAll() const;
};
```

### 8.2 内存池

```cpp
// src/utils/arena.h

class Arena {
    std::vector<std::unique_ptr<char[]>> blocks_;
    static constexpr size_t BLOCK_SIZE = 64 * 1024;
    
public:
    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        void* mem = allocateRaw(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }
    
    void reset();  // 清空所有分配
};
```

### 8.3 字符串驻留

```cpp
// src/utils/string_interner.h

class StringInterner {
    std::unordered_set<std::string> strings_;
    
public:
    const char* intern(const std::string& str) {
        auto [it, inserted] = strings_.insert(str);
        return it->c_str();
    }
};
```

### 8.4 哈希工具

```cpp
// src/utils/hash.h

#include <siphash/SipHash.h>

class Hash {
public:
    static uint64_t hash64(const std::string& str) {
        unsigned char out[8];
        siphash<2, 4, 8>(
            reinterpret_cast<const unsigned char*>(str.data()),
            str.size(),
            key,
            out
        );
        return /* convert to uint64_t */;
    }
};
```

---

## 9. 内存管理

### 9.1 内存管理策略

PawLang **不使用GC**，采用以下策略：

```
┌─────────────────────────────────────────┐
│     PawLang内存管理模型（无GC）          │
├─────────────────────────────────────────┤
│                                         │
│  1. 栈分配优先 - 默认所有值在栈上       │
│  2. 值语义 - struct/enum按值传递        │
│  3. 显式堆分配 - 使用Box<T>             │
│  4. RAII原则 - 作用域结束自动清理        │
│  5. 无GC - 编译时确定生命周期           │
│                                         │
└─────────────────────────────────────────┘
```

### 9.2 内存管理实现

```cpp
// src/backend/codegen/memory/memory_manager.h

class MemoryManager {
    llvm::IRBuilder<>& builder_;
    llvm::Module* module_;
    
public:
    // 栈分配（默认）
    llvm::AllocaInst* allocateOnStack(llvm::Type* type, 
                                      const std::string& name);
    
    // 堆分配（显式）
    llvm::Value* allocateOnHeap(llvm::Type* type, 
                                const std::string& name);
    
    // 释放堆内存
    void deallocate(llvm::Value* ptr);
    
    // RAII: 生成析构函数调用
    void generateDestructor(llvm::Value* value, Type* type);
};
```

### 9.3 运行时支持

```cpp
// src/runtime/builtin_runtime.h

extern "C" {
    // 内存分配
    void* paw_malloc(size_t size);
    void paw_free(void* ptr);
    
    // 边界检查
    void paw_bounds_check(int64_t index, int64_t length,
                          const char* file, int line);
    
    // Panic处理
    [[noreturn]] void paw_panic(const char* message,
                                const char* file, int line);
    
    // 字符串操作
    char* paw_string_concat(const char* s1, const char* s2);
}
```

---

## 10. 构建系统

### 10.1 CMake配置

```cmake
# CMakeLists.txt

cmake_minimum_required(VERSION 3.20)
project(PawLang VERSION 1.4.0 LANGUAGES CXX C)

# 使用LLVM通用CMake工具
set(LLVM_COMMON_CMAKE_UTILS ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
list(INSERT CMAKE_MODULE_PATH 0 "${LLVM_COMMON_CMAKE_UTILS}/Modules")

# LLVM配置（最小化）
set(LLVM_TARGETS_TO_BUILD "X86;AArch64;ARM;RISCV" CACHE STRING "")
set(LLVM_ENABLE_PROJECTS "clang;lld" CACHE STRING "")
set(LLVM_ENABLE_RTTI ON CACHE BOOL "")
set(LLVM_INCLUDE_TESTS OFF CACHE BOOL "")
set(LLVM_BUILD_TOOLS OFF CACHE BOOL "")

add_subdirectory(llvm)

# PawLang编译器
add_subdirectory(src/frontend)
add_subdirectory(src/middleend)
add_subdirectory(src/backend)
add_subdirectory(src/pass)
add_subdirectory(src/passes)
add_subdirectory(src/diagnostics)
add_subdirectory(src/utils)
add_subdirectory(src/driver)
add_subdirectory(src/runtime)

add_executable(pawc src/driver/main.cpp)
target_link_libraries(pawc PRIVATE
    pawc_driver pawc_passes pawc_pass
    pawc_backend pawc_middleend pawc_frontend
    pawc_diagnostics pawc_utils pawc_runtime
    ${LLVM_LIBS} ${LLD_LIBS}
)
```

### 10.2 构建脚本

```bash
#!/bin/bash
# build - PawLang构建脚本

TARGET="${1:-native}"
CPU="${2:-native}"
JOBS=$(nproc 2>/dev/null || echo 4)

mkdir -p build
cd build

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_TARGETS_TO_BUILD="X86;AArch64;ARM;RISCV"

make llvm -j$JOBS
make clang -j$JOBS
make lld -j$JOBS
make pawc -j$JOBS

echo "✅ Build complete: build/bin/pawc"
```

---

## 11. 测试架构

### 11.1 测试层次

```
┌─────────────────────────────────────┐
│   End-to-End Tests                  │  完整编译流程
├─────────────────────────────────────┤
│   Integration Tests                 │  模块集成
├─────────────────────────────────────┤
│   Unit Tests                        │  单个模块
├─────────────────────────────────────┤
│   Component Tests                   │  独立组件
└─────────────────────────────────────┘
```

### 11.2 单元测试示例

```cpp
// src/frontend/lexer/lexer_test.cpp

#include <gtest/gtest.h>
#include "lexer.h"

TEST(LexerTest, BasicTokens) {
    Lexer lexer("let x = 42;");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 5);
    EXPECT_EQ(tokens[0].type, TokenType::KW_LET);
    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER);
}
```

---

## 12. 性能优化

### 12.1 编译时优化

```
✅ Arena内存池 - 快速分配，统一释放
✅ 字符串驻留 - O(1)比较
✅ 类型缓存 - 避免重复映射
✅ 泛型实例缓存 - 避免重复实例化
✅ Pass并行执行 - 利用多核
```

### 12.2 运行时优化

```
✅ 静态分发 - 接口调用零成本
✅ 泛型单态化 - 编译时特化
✅ LLVM优化 - 标准优化Pass
✅ 无GC - 零GC开销
```

---

## 13. 实施路线

### 13.1 Phase 1: 基础设施（0-2周）

```
✅ 创建目录结构
✅ 配置CMake
✅ 实现Pass系统基础
✅ 实现诊断系统
✅ 实现工具库（Arena, StringInterner等）
```

### 13.2 Phase 2: 前端（2-4周）

```
✅ 实现Lexer
✅ 实现Parser
✅ 定义完整AST
✅ 实现模块加载
```

### 13.3 Phase 3: 中端（4-8周）

```
✅ 实现类型系统
✅ 实现类型检查Pass
✅ 实现类型推导Pass
✅ 实现接口验证Pass
✅ 实现模式匹配检查Pass
```

### 13.4 Phase 4: 后端（8-12周）

```
✅ 实现CodeGen基础
✅ 实现表达式代码生成
✅ 实现语句代码生成
✅ 实现类型代码生成
✅ 实现泛型单态化
✅ 实现LLVM IR生成Pass
```

### 13.5 Phase 5: 集成和测试（12-16周）

```
✅ 集成所有模块
✅ 编写单元测试
✅ 编写集成测试
✅ 测试示例程序
✅ 性能优化
```

---

## 附录

### A. 关键特性对比

| 特性 | 传统架构 | PawLang架构 |
|------|---------|------------|
| 架构模式 | Pipeline | Pass-Based ✅ |
| 内存管理 | GC | RAII + 栈 ✅ |
| 可扩展性 | 低 | 高 ✅ |
| 并行编译 | 否 | 是 ✅ |
| 类型系统 | 分散 | 独立 ✅ |
| 错误处理 | 不统一 | 统一 ✅ |

### B. 技术选型

```
语言: C++17
构建: CMake 3.20+
编译器: GCC 7+ / Clang 5+ / MSVC 2019+
后端: LLVM 19
链接器: LLD
测试: GoogleTest
哈希: SipHash
```

### C. 参考资料

- **LLVM**: https://llvm.org/docs/
- **Rust编译器**: https://rustc-dev-guide.rust-lang.org/
- **Clang**: https://clang.llvm.org/docs/InternalsManual.html
- **zig-bootstrap**: https://github.com/ziglang/zig-bootstrap

---

## 总结

```
╔════════════════════════════════════════════════════════════╗
║              PawLang v1.4.0 架构特点                        ║
╠════════════════════════════════════════════════════════════╣
║                                                            ║
║  ✅ Pass-Based架构 - 模块化、可扩展、高性能                ║
║  ✅ 无GC内存管理 - RAII + 确定性析构                       ║
║  ✅ 完整类型系统 - 28种类型 + 泛型 + 接口                  ║
║  ✅ LLVM集成 - LLVM 19 + Clang + LLD                      ║
║  ✅ 跨平台支持 - Linux/macOS/Windows                       ║
║  ✅ 生产级设计 - 完整的错误处理和测试                      ║
║                                                            ║
╚════════════════════════════════════════════════════════════╝
```

**这是一个完整的、生产级的编译器架构！**

---

*文档版本: v1.4.0*  
*最后更新: 2025-10-31*  
*作者: PawLang Team*  
*状态: 架构设计完成 ✅*
