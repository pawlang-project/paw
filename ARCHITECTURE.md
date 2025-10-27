# PawLang 编译器架构设计 (v0.2.2+)

> 🏗️ **软件架构规划与设计指南**  
> 本文档定义了 PawLang 编译器的架构模式、设计原则和未来演进方向

## 📋 目录

- [1. 架构概览](#1-架构概览)
- [2. 当前架构分析](#2-当前架构分析)
- [3. 设计原则](#3-设计原则)
- [4. 核心架构模式](#4-核心架构模式)
- [5. 模块设计](#5-模块设计)
- [6. 类型系统架构](#6-类型系统架构)
- [7. 错误处理架构](#7-错误处理架构)
- [8. 测试架构](#8-测试架构)
- [9. 性能优化架构](#9-性能优化架构)
- [10. 未来演进路线](#10-未来演进路线)

---

## 1. 架构概览

### 1.1 总体架构

```
┌─────────────────────────────────────────────────────────────┐
│                     PawLang 编译器 (pawc)                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌──────────┐   │
│  │ Lexer   │──▶│ Parser  │──▶│Semantic │──▶│ CodeGen  │   │
│  │ 词法分析 │   │ 语法分析 │   │ 语义分析 │   │ 代码生成  │   │
│  └─────────┘   └─────────┘   └─────────┘   └──────────┘   │
│       │             │             │             │          │
│       ▼             ▼             ▼             ▼          │
│   Tokens         AST      SymbolTable    LLVM IR          │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│                      支撑系统                                │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐             │
│  │ TypeSystem │ │ErrorReporter│ │ModuleSystem│             │
│  │  类型系统   │ │ 错误报告系统 │ │  模块系统   │             │
│  └────────────┘ └────────────┘ └────────────┘             │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│                      LLVM 后端                               │
│  Optimization → Target Code Gen → Linking                   │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 数据流

```
Source Code (.paw)
      ↓
  [Lexer]
      ↓
   Tokens
      ↓
  [Parser]
      ↓
    AST (Abstract Syntax Tree)
      ↓
  [Module Resolution]
      ↓
  Symbol Table + Type Information
      ↓
  [Semantic Analysis]
      ↓
  Validated AST + Type-checked
      ↓
  [Code Generation]
      ↓
   LLVM IR
      ↓
  [LLVM Optimization]
      ↓
  Machine Code (.o)
      ↓
  [Linking]
      ↓
  Executable
```

---

## 2. 当前架构分析

### 2.1 现有结构

```
src/
├── lexer/           # 词法分析器
│   ├── lexer.h      # Token 定义 + Lexer 类
│   └── lexer.cpp    # 实现
├── parser/          # 语法分析器
│   ├── ast.h        # AST 节点定义（300+ 行）
│   ├── parser.h     # Parser 类
│   └── parser.cpp   # 实现（2000+ 行）
├── module/          # 模块系统
│   ├── module_loader.h/cpp    # 模块加载
│   ├── module_compiler.h/cpp  # 模块编译
│   └── symbol_table.h/cpp     # 符号表
├── codegen/         # 代码生成（已开始分文件）
│   ├── codegen.h              # CodeGenerator 类
│   ├── codegen.cpp            # 核心逻辑
│   ├── codegen_expr.cpp       # 表达式生成
│   ├── codegen_stmt.cpp       # 语句生成
│   ├── codegen_struct.cpp     # 结构体生成
│   ├── codegen_match.cpp      # 模式匹配生成
│   └── codegen_type.cpp       # 类型转换
├── builtins/        # 内建函数
│   ├── builtins.h
│   └── builtins.cpp
├── error_reporter.cpp  # 错误报告
├── colors.cpp          # 颜色输出
├── toml_parser.cpp     # 配置解析
└── main.cpp            # 入口
```

### 2.2 当前架构优点 ✅

1. **清晰的 Pipeline 结构**
   - Lexer → Parser → CodeGen 分离明确
   - 单向数据流，易于理解

2. **模块化开始显现**
   - `codegen/` 目录已经开始按功能分文件
   - 模块系统独立管理

3. **符号表设计良好**
   - 集中管理符号（函数、类型、接口）
   - 支持跨模块查询

4. **LLVM 集成清晰**
   - 使用 LLVM C++ API
   - 标准的 IR 生成流程

### 2.3 当前架构待改进点 ⚠️

#### 问题 1：语义分析未独立

**现状**：  
- 类型检查、接口验证等语义分析逻辑混在 `CodeGen` 中
- `codegen_stmt.cpp` 中有大量 `validateInterfaceImpl()` 等验证代码

**影响**：
- CodeGen 职责过重（生成 + 验证）
- 难以复用验证逻辑
- 错误报告时机不统一

#### 问题 2：类型系统分散

**现状**：
- 类型定义在 `ast.h` 的 `Type` 结构体中
- 类型比较逻辑在 `codegen_stmt.cpp` 的 `compareTypes()`
- 类型信息散布在多个地方

**影响**：
- 类型操作代码重复
- 类型系统扩展困难
- 类型推导逻辑不集中

#### 问题 3：错误处理不统一

**现状**：
- 有些地方用 `std::cerr`
- 有些地方用 `error_reporter.cpp`
- 缺少统一的错误收集机制

**影响**：
- 错误信息风格不一致
- 难以实现"继续编译，收集所有错误"
- IDE 集成困难

#### 问题 4：AST 定义过大

**现状**：
- `ast.h` 包含所有 AST 节点（300+ 行，还在增长）
- 表达式、语句、类型定义混在一起

**影响**：
- 文件过大，难以维护
- 编译依赖广泛
- 增加新节点类型时影响面大

#### 问题 5：缺少 Pass 抽象

**现状**：
- 所有优化和验证逻辑直接在主流程中
- 没有统一的 Pass 接口

**影响**：
- 难以插入新的分析/优化阶段
- 无法灵活配置编译流程
- 调试困难（无法单独运行某个 Pass）

---

## 3. 设计原则

### 3.1 核心原则

#### ✅ 单一职责原则 (SRP)

```cpp
// ❌ 不好：一个类做太多事
class CodeGenerator {
    void generateStmt(...);     // 生成代码
    bool validateInterface(...); // 验证接口
    Type* inferType(...);       // 类型推导
    void reportError(...);      // 错误报告
};

// ✅ 好：职责分离
class CodeGenerator {
    void generateStmt(...);  // 只负责生成代码
};

class SemanticAnalyzer {
    bool validateInterface(...);  // 语义验证
    Type* inferType(...);        // 类型推导
};

class DiagnosticEngine {
    void reportError(...);  // 统一错误报告
};
```

#### ✅ 开闭原则 (OCP)

```cpp
// ✅ 通过继承/接口扩展，无需修改现有代码
class CompilerPass {
public:
    virtual bool run(Module* module) = 0;
    virtual const char* getName() const = 0;
};

class TypeCheckPass : public CompilerPass { ... };
class InterfaceValidationPass : public CompilerPass { ... };
class DeadCodeEliminationPass : public CompilerPass { ... };

// 添加新 Pass 无需修改 PassManager
class PassManager {
    void addPass(std::unique_ptr<CompilerPass> pass);
    bool runAll();
};
```

#### ✅ 依赖倒置原则 (DIP)

```cpp
// ✅ 依赖抽象，不依赖具体实现
class CodeGenerator {
    TypeSystem* type_system_;       // 依赖接口
    DiagnosticEngine* diagnostics_; // 依赖接口
    
    // 而不是依赖具体类：
    // ConcreteTypeChecker checker_;
    // ConcreteErrorReporter reporter_;
};
```

### 3.2 架构原则

1. **分层清晰**：Frontend (Lexer/Parser) → Middle-end (Semantic) → Backend (CodeGen)
2. **接口明确**：每个模块有清晰的公共接口
3. **低耦合**：模块间通过接口通信，不直接依赖实现
4. **高内聚**：相关功能集中在同一模块
5. **易测试**：每个模块可独立单元测试

---

## 4. 核心架构模式

### 4.1 Pipeline 架构（当前）

```
Input → Stage1 → Stage2 → Stage3 → Output
```

**优点**：
- 简单清晰
- 易于理解和实现

**缺点**：
- 难以插入新阶段
- 缺乏灵活性

### 4.2 Pass-Based 架构（推荐）

```cpp
class CompilerPass {
public:
    virtual ~CompilerPass() = default;
    virtual bool run(Module* module) = 0;
    virtual const char* getName() const = 0;
    virtual bool isAnalysisPass() const { return false; }
};

class PassManager {
    std::vector<std::unique_ptr<CompilerPass>> passes_;
    
public:
    void addPass(std::unique_ptr<CompilerPass> pass) {
        passes_.push_back(std::move(pass));
    }
    
    bool runAll(Module* module) {
        for (auto& pass : passes_) {
            if (!pass->run(module)) {
                return false;
            }
        }
        return true;
    }
};
```

**使用示例**：

```cpp
PassManager pm;
pm.addPass(std::make_unique<ModuleResolutionPass>());
pm.addPass(std::make_unique<TypeInferencePass>());
pm.addPass(std::make_unique<TypeCheckPass>());
pm.addPass(std::make_unique<InterfaceValidationPass>());
pm.addPass(std::make_unique<BorrowCheckPass>());
pm.addPass(std::make_unique<CodeGenPass>());
pm.runAll(module);
```

**优点**：
- 高度模块化
- 易于添加/删除阶段
- 可配置的编译流程
- 便于单独测试和调试

### 4.3 Visitor 模式（AST 遍历）

```cpp
// 统一的 AST 访问器接口
class ASTVisitor {
public:
    virtual void visit(IntLiteral* node) = 0;
    virtual void visit(BinaryExpr* node) = 0;
    virtual void visit(FunctionStmt* node) = 0;
    // ... 其他节点类型
};

// 具体的访问器实现
class TypeChecker : public ASTVisitor {
    void visit(BinaryExpr* node) override {
        // 类型检查逻辑
    }
};

class PrettyPrinter : public ASTVisitor {
    void visit(BinaryExpr* node) override {
        // 打印逻辑
    }
};
```

---

## 5. 模块设计

### 5.1 推荐的目录结构

```
src/
├── frontend/           # 前端
│   ├── lexer/
│   │   ├── token.h             # Token 定义
│   │   ├── lexer.h/cpp         # 词法分析器
│   │   └── lexer_test.cpp      # 单元测试
│   ├── parser/
│   │   ├── ast/
│   │   │   ├── expr.h          # 表达式节点
│   │   │   ├── stmt.h          # 语句节点
│   │   │   ├── decl.h          # 声明节点
│   │   │   └── type.h          # 类型节点
│   │   ├── parser.h/cpp        # 解析器
│   │   └── parser_test.cpp
│   └── module/
│       ├── module_loader.h/cpp
│       ├── module_resolver.h/cpp
│       └── import_graph.h/cpp
│
├── middleend/          # 中间层（语义分析）
│   ├── sema/
│   │   ├── semantic_analyzer.h/cpp  # 语义分析器
│   │   ├── type_checker.h/cpp       # 类型检查
│   │   ├── interface_validator.h/cpp # 接口验证
│   │   └── borrow_checker.h/cpp     # 借用检查（未来）
│   ├── types/
│   │   ├── type_system.h/cpp        # 类型系统核心
│   │   ├── type_registry.h/cpp      # 类型注册表
│   │   ├── type_inference.h/cpp     # 类型推导
│   │   └── type_unification.h/cpp   # 类型统一
│   └── symbol/
│       ├── symbol_table.h/cpp       # 符号表
│       ├── scope.h/cpp              # 作用域管理
│       └── name_resolution.h/cpp    # 名称解析
│
├── backend/            # 后端
│   ├── codegen/
│   │   ├── codegen.h/cpp           # 代码生成器核心
│   │   ├── expr_codegen.h/cpp      # 表达式代码生成
│   │   ├── stmt_codegen.h/cpp      # 语句代码生成
│   │   ├── type_codegen.h/cpp      # 类型代码生成
│   │   ├── struct_codegen.h/cpp    # 结构体代码生成
│   │   ├── match_codegen.h/cpp     # 模式匹配代码生成
│   │   └── generic_codegen.h/cpp   # 泛型代码生成
│   ├── builtins/
│   │   ├── builtin_functions.h/cpp
│   │   └── builtin_types.h/cpp
│   └── optimization/
│       ├── pass_manager.h/cpp
│       └── passes/
│           ├── dead_code_elim.h/cpp
│           └── constant_folding.h/cpp
│
├── diagnostics/        # 诊断系统
│   ├── diagnostic_engine.h/cpp  # 诊断引擎
│   ├── diagnostic.h            # 诊断信息定义
│   ├── source_manager.h/cpp    # 源码管理
│   └── error_formatter.h/cpp   # 错误格式化
│
├── utils/              # 工具类
│   ├── arena.h/cpp            # 内存池
│   ├── string_interner.h/cpp  # 字符串驻留
│   ├── colors.h/cpp           # 颜色输出
│   └── profiler.h/cpp         # 性能分析
│
└── driver/             # 驱动器
    ├── compiler.h/cpp         # 编译器主类
    ├── driver.h/cpp           # 命令行驱动
    └── main.cpp               # 入口
```

### 5.2 模块职责定义

#### Frontend 前端

| 模块 | 职责 | 输入 | 输出 |
|-----|-----|-----|-----|
| **Lexer** | 词法分析 | 源代码字符串 | Token 流 |
| **Parser** | 语法分析 | Token 流 | AST |
| **Module Loader** | 模块加载 | 文件路径 | 模块信息 |

#### Middle-end 中间层

| 模块 | 职责 | 输入 | 输出 |
|-----|-----|-----|-----|
| **Semantic Analyzer** | 语义分析协调 | AST | 验证后的 AST |
| **Type System** | 类型系统核心 | 类型信息 | 类型操作接口 |
| **Type Checker** | 类型检查 | AST + SymbolTable | 类型错误列表 |
| **Interface Validator** | 接口验证 | 接口实现 | 验证结果 |
| **Symbol Table** | 符号管理 | 声明 | 符号查询接口 |

#### Backend 后端

| 模块 | 职责 | 输入 | 输出 |
|-----|-----|-----|-----|
| **CodeGen** | 代码生成 | 验证后的 AST | LLVM IR |
| **Builtins** | 内建函数 | 调用请求 | LLVM 函数 |
| **Optimization** | 优化 Pass | LLVM IR | 优化后的 IR |

#### Support 支撑

| 模块 | 职责 | 输入 | 输出 |
|-----|-----|-----|-----|
| **Diagnostic Engine** | 错误报告 | 错误信息 | 格式化的错误输出 |
| **Source Manager** | 源码管理 | 文件路径 | 源码定位信息 |

---

## 6. 类型系统架构

### 6.1 类型系统独立化

**当前问题**：类型定义散布在 `ast.h`、`codegen.h` 等多处

**解决方案**：建立独立的类型系统模块

```cpp
// types/type_system.h

namespace pawc {

// 类型基类
class Type {
public:
    enum Kind {
        Primitive,   // i32, f64, bool, char, string
        Named,       // 用户定义类型
        Array,       // [T; N]
        Slice,       // [T]
        Reference,   // &T, &mut T
        Optional,    // T?
        Tuple,       // (T1, T2, ...)
        Function,    // fn(T1, T2) -> T3
        Generic,     // T (未实例化)
        SelfType,    // self
    };
    
    virtual Kind getKind() const = 0;
    virtual bool equals(const Type* other) const = 0;
    virtual std::string toString() const = 0;
    virtual ~Type() = default;
};

// 具体类型类
class PrimitiveType : public Type { ... };
class NamedType : public Type { ... };
class ArrayType : public Type { ... };
// ...

// 类型系统管理器
class TypeSystem {
    std::map<std::string, Type*> named_types_;
    std::vector<std::unique_ptr<Type>> type_pool_;
    
public:
    // 类型创建
    Type* getPrimitiveType(const std::string& name);
    Type* getArrayType(Type* element, int size);
    Type* getReferenceType(Type* pointee, bool is_mut);
    Type* getOptionalType(Type* inner);
    
    // 类型查询
    Type* lookupType(const std::string& name);
    bool isAssignable(Type* from, Type* to);
    Type* commonType(Type* t1, Type* t2);
    
    // 类型比较
    bool equals(Type* t1, Type* t2);
    bool isSubtype(Type* sub, Type* super);
    
    // 类型注册
    void registerType(const std::string& name, Type* type);
};

} // namespace pawc
```

### 6.2 类型推导架构

```cpp
// types/type_inference.h

class TypeInference {
    TypeSystem* type_system_;
    SymbolTable* symbol_table_;
    
public:
    // 表达式类型推导
    Type* inferExpr(Expr* expr);
    
    // 变量类型推导
    Type* inferVariable(const std::string& name, Expr* init);
    
    // 函数返回类型推导
    Type* inferFunctionReturn(FunctionStmt* func);
    
    // 泛型类型实例化
    Type* instantiateGeneric(Type* generic, 
                            const std::vector<Type*>& args);
};
```

---

## 7. 错误处理架构

### 7.1 统一的诊断系统

```cpp
// diagnostics/diagnostic.h

enum class DiagnosticLevel {
    Note,      // 提示信息
    Warning,   // 警告
    Error,     // 错误
    Fatal      // 致命错误
};

struct SourceLocation {
    std::string file;
    int line;
    int column;
};

class Diagnostic {
    DiagnosticLevel level_;
    std::string message_;
    SourceLocation location_;
    std::vector<SourceLocation> notes_;  // 相关位置
    
public:
    Diagnostic(DiagnosticLevel level, 
              const std::string& msg,
              SourceLocation loc);
              
    void addNote(const std::string& note, SourceLocation loc);
    void format(std::ostream& out) const;
};
```

```cpp
// diagnostics/diagnostic_engine.h

class DiagnosticEngine {
    std::vector<Diagnostic> diagnostics_;
    SourceManager* source_manager_;
    bool has_errors_ = false;
    
public:
    void report(DiagnosticLevel level,
               const std::string& message,
               SourceLocation loc);
               
    void reportError(const std::string& msg, SourceLocation loc) {
        report(DiagnosticLevel::Error, msg, loc);
        has_errors_ = true;
    }
    
    void reportWarning(const std::string& msg, SourceLocation loc) {
        report(DiagnosticLevel::Warning, msg, loc);
    }
    
    bool hasErrors() const { return has_errors_; }
    
    void printAll() const;
    void clear();
};
```

### 7.2 错误恢复策略

```cpp
// parser/parser.cpp

// ✅ 在 Parser 中实现错误恢复
void Parser::synchronize() {
    advance();
    while (!isAtEnd()) {
        if (previous().type == TokenType::SEMICOLON) return;
        
        switch (peek().type) {
            case TokenType::KW_FN:
            case TokenType::KW_TYPE:
            case TokenType::KW_LET:
            case TokenType::KW_IF:
            case TokenType::KW_WHILE:
            case TokenType::KW_RETURN:
                return;
            default:
                advance();
        }
    }
}

StmtPtr Parser::statement() {
    try {
        // 正常解析
        return parseStatement();
    } catch (const ParseError& e) {
        diagnostics_->reportError(e.what(), getCurrentLocation());
        synchronize();  // 恢复到下一个同步点
        return nullptr; // 返回空节点继续解析
    }
}
```

---

## 8. 测试架构

### 8.1 测试层次

```
┌─────────────────────────────────────┐
│        End-to-End Tests             │  测试完整编译流程
│    (examples/*.paw)                 │
├─────────────────────────────────────┤
│      Integration Tests              │  测试模块集成
│  (模块间交互、链接)                  │
├─────────────────────────────────────┤
│        Unit Tests                   │  测试单个模块
│  (Lexer, Parser, TypeChecker)      │
├─────────────────────────────────────┤
│     Component Tests                 │  测试独立组件
│  (SymbolTable, TypeSystem)         │
└─────────────────────────────────────┘
```

### 8.2 测试工具建议

```cmake
# CMakeLists.txt

# 使用 GoogleTest
find_package(GTest REQUIRED)
enable_testing()

# Lexer 单元测试
add_executable(lexer_test src/frontend/lexer/lexer_test.cpp)
target_link_libraries(lexer_test GTest::GTest pawc_lexer)
add_test(NAME lexer_test COMMAND lexer_test)

# Parser 单元测试
add_executable(parser_test src/frontend/parser/parser_test.cpp)
target_link_libraries(parser_test GTest::GTest pawc_parser)
add_test(NAME parser_test COMMAND parser_test)

# 集成测试
add_test(
    NAME integration_test
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.sh
)
```

### 8.3 测试示例

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
    EXPECT_EQ(tokens[2].type, TokenType::EQUAL);
    EXPECT_EQ(tokens[3].type, TokenType::INT_LITERAL);
    EXPECT_EQ(tokens[4].type, TokenType::SEMICOLON);
}

TEST(LexerTest, StringLiterals) {
    Lexer lexer("\"hello world\"");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::STRING_LITERAL);
    EXPECT_EQ(tokens[0].lexeme, "hello world");
}
```

---

## 9. 性能优化架构

### 9.1 编译性能优化

#### 9.1.1 内存池（Arena Allocator）

```cpp
// utils/arena.h

class Arena {
    std::vector<std::unique_ptr<char[]>> blocks_;
    char* current_block_ = nullptr;
    size_t current_offset_ = 0;
    static constexpr size_t BLOCK_SIZE = 64 * 1024; // 64KB
    
public:
    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        void* mem = allocateRaw(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }
    
    void* allocateRaw(size_t size, size_t alignment);
    void reset();  // 清空所有分配（用于编译单个文件后）
};
```

**用途**：
- AST 节点分配
- Type 对象分配
- Symbol 对象分配

#### 9.1.2 字符串驻留（String Interning）

```cpp
// utils/string_interner.h

class StringInterner {
    std::unordered_set<std::string> strings_;
    
public:
    const char* intern(const std::string& str) {
        auto [it, inserted] = strings_.insert(str);
        return it->c_str();
    }
};
```

**优势**：
- 标识符比较 O(1)（指针比较）
- 减少内存占用

### 9.2 运行时性能

- 静态分发（接口调用零成本）
- 内联优化
- LLVM 优化 Pass

---

## 10. 未来演进路线

### 10.1 短期目标（v0.2.3 - v0.3.0）

#### Phase 1: 语义分析独立化
- [ ] 创建 `middleend/sema/` 目录
- [ ] 实现 `SemanticAnalyzer` 类
- [ ] 从 `CodeGen` 中抽离类型检查逻辑
- [ ] 从 `CodeGen` 中抽离接口验证逻辑

#### Phase 2: 类型系统重构
- [ ] 创建 `middleend/types/` 目录
- [ ] 实现 `TypeSystem` 类
- [ ] 实现 `TypeInference` 类
- [ ] 统一类型比较逻辑

#### Phase 3: 诊断系统完善
- [ ] 创建 `diagnostics/` 目录
- [ ] 实现 `DiagnosticEngine` 类
- [ ] 替换所有 `std::cerr` 为统一接口
- [ ] 实现错误恢复机制

### 10.2 中期目标（v0.3.0 - v0.4.0）

#### Phase 4: Pass-Based 架构
- [ ] 实现 `CompilerPass` 接口
- [ ] 实现 `PassManager`
- [ ] 重构为多个独立 Pass：
  - `ModuleResolutionPass`
  - `TypeInferencePass`
  - `TypeCheckPass`
  - `InterfaceValidationPass`
  - `CodeGenPass`

#### Phase 5: 测试框架
- [ ] 集成 GoogleTest
- [ ] 为每个模块添加单元测试
- [ ] 建立 CI/CD 流程

#### Phase 6: 性能优化
- [ ] 实现 Arena 内存池
- [ ] 实现字符串驻留
- [ ] 添加编译性能分析工具

### 10.3 长期目标（v0.4.0+）

- [ ] **增量编译**：只重新编译修改的模块
- [ ] **并行编译**：多线程编译多个模块
- [ ] **LSP 支持**：实现 Language Server Protocol
- [ ] **调试信息**：生成 DWARF 调试信息
- [ ] **更多优化 Pass**：死代码消除、常量折叠等
- [ ] **借用检查器**（如果需要引用语义）

---

## 11. 实施建议

### 11.1 重构策略

#### ✅ 增量式重构（推荐）

**原则**：不破坏现有功能，逐步改进

1. **新增而非替换**：
   - 先创建新模块（如 `TypeSystem`）
   - 新代码使用新模块
   - 旧代码逐步迁移

2. **保持测试通过**：
   - 每次重构后运行测试
   - 确保所有 examples 仍能编译

3. **分阶段进行**：
   - 一次只重构一个模块
   - 完成一个阶段再进入下一个

#### ❌ 大规模重写（不推荐）

- 风险高
- 引入新 bug
- 开发周期长

### 11.2 优先级排序

| 优先级 | 任务 | 理由 |
|-------|-----|-----|
| 🔴 **P0** | 诊断系统统一 | 改善开发体验，便于调试 |
| 🟠 **P1** | 类型系统独立 | 为后续特性（trait、高级类型）打基础 |
| 🟡 **P2** | 语义分析分离 | 解耦 CodeGen，提高可维护性 |
| 🟢 **P3** | Pass 架构 | 提高灵活性，但不紧急 |
| 🔵 **P4** | 性能优化 | 当前性能尚可，可延后 |

### 11.3 开始建议

**第一步：创建架构文档**（✅ 已完成）
- 本文档作为未来开发的指导

**第二步：建立测试框架**
```bash
# 安装 GoogleTest
cd /path/to/paw
mkdir third-party
cd third-party
git clone https://github.com/google/googletest.git
```

**第三步：实现诊断系统**
```cpp
// 创建新文件
touch src/diagnostics/diagnostic_engine.h
touch src/diagnostics/diagnostic_engine.cpp
touch src/diagnostics/diagnostic.h
```

**第四步：逐步替换错误报告**
```cpp
// 旧代码
std::cerr << "Error: " << msg << std::endl;

// 新代码
diagnostics_->reportError(msg, loc);
```

---

## 12. 总结

### 当前状态 ✅

PawLang v0.2.2 已经具备：
- ✅ 清晰的 Pipeline 结构
- ✅ 良好的模块划分（Lexer/Parser/CodeGen 分离）
- ✅ 完整的功能实现（接口、泛型、模块系统）

### 架构改进方向 🎯

优先级从高到低：

1. **统一诊断系统** → 改善开发体验
2. **独立类型系统** → 为未来扩展打基础
3. **分离语义分析** → 提高可维护性
4. **Pass-Based 架构** → 提高灵活性
5. **性能优化** → 提升编译速度

### 核心原则 📐

- **增量式改进**：不破坏现有功能
- **保持测试通过**：每次改动后验证
- **文档驱动**：先设计，后实现
- **代码复用**：避免重复实现

---

## 附录：参考资料

### A. 编译器架构参考

- **Rust 编译器**：Pass-Based 架构的优秀实践
  - [rustc dev guide](https://rustc-dev-guide.rust-lang.org/)
- **LLVM**：标准的编译器基础设施
  - [LLVM Programmer's Manual](https://llvm.org/docs/ProgrammersManual.html)
- **Clang**：现代 C++ 编译器
  - [Clang Internals](https://clang.llvm.org/docs/InternalsManual.html)

### B. 设计模式

- **Visitor 模式**：AST 遍历
- **Strategy 模式**：不同的代码生成策略
- **Factory 模式**：类型创建
- **Observer 模式**：诊断信息收集

### C. 工具推荐

- **GoogleTest**：C++ 单元测试框架
- **Valgrind**：内存泄漏检测
- **perf**：性能分析
- **clang-tidy**：静态代码分析

---

**本文档将持续更新，记录 PawLang 编译器的架构演进 🐾**

*最后更新：2025-10-27*  
*版本：v0.2.2*

