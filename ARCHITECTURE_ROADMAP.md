# PawLang 架构改进路线图

> 📋 **v0.2.2 → v0.3.0 架构演进计划**

## 🎯 总体目标

将当前的 **Pipeline 架构** 逐步演进为 **Pass-Based 架构**，提高：
- 代码可维护性
- 模块解耦程度
- 编译器灵活性
- 测试覆盖率

## 📊 当前架构评估

### ✅ 优势
```
✓ Pipeline 清晰：Lexer → Parser → CodeGen
✓ LLVM 集成良好
✓ 模块系统完善
✓ 代码已开始分文件（codegen/ 目录）
```

### ⚠️ 待改进
```
× 语义分析与代码生成混合（CodeGen 职责过重）
× 类型系统分散（ast.h + codegen_stmt.cpp）
× 错误处理不统一（std::cerr + error_reporter）
× 缺少统一的 Pass 抽象
× AST 定义过大（ast.h 300+ 行）
```

---

## 🗓️ 分阶段实施计划

### 📌 Phase 1: 统一诊断系统 (v0.2.3)
**目标**：建立统一的错误/警告报告机制

**时间估计**：1-2 周

#### 任务清单

- [ ] **Step 1.1**: 创建诊断模块
  ```bash
  mkdir -p src/diagnostics
  touch src/diagnostics/diagnostic.h
  touch src/diagnostics/diagnostic_engine.h
  touch src/diagnostics/diagnostic_engine.cpp
  touch src/diagnostics/source_manager.h
  touch src/diagnostics/source_manager.cpp
  ```

- [ ] **Step 1.2**: 实现 `Diagnostic` 类
  ```cpp
  // 错误级别、消息、位置、相关提示
  class Diagnostic {
      DiagnosticLevel level_;
      std::string message_;
      SourceLocation location_;
      std::vector<Note> notes_;
  };
  ```

- [ ] **Step 1.3**: 实现 `DiagnosticEngine`
  ```cpp
  class DiagnosticEngine {
      void reportError(const std::string& msg, SourceLocation loc);
      void reportWarning(const std::string& msg, SourceLocation loc);
      bool hasErrors() const;
      void printAll() const;
  };
  ```

- [ ] **Step 1.4**: 实现 `SourceManager`
  ```cpp
  class SourceManager {
      std::string getSourceLine(int line);
      SourceLocation getLocation(const Token& token);
  };
  ```

- [ ] **Step 1.5**: 替换现有错误报告
  ```cpp
  // 旧：std::cerr << "Error: " << msg << std::endl;
  // 新：diagnostics_->reportError(msg, loc);
  ```
  - 替换 `lexer.cpp` 中的错误报告
  - 替换 `parser.cpp` 中的错误报告
  - 替换 `codegen_*.cpp` 中的错误报告

- [ ] **Step 1.6**: 添加错误恢复机制
  ```cpp
  void Parser::synchronize() {
      // 恢复到下一个同步点
  }
  ```

- [ ] **Step 1.7**: 美化错误输出格式
  ```
  Error: Type mismatch
   --> examples/test.paw:10:5
    |
  10|     let x: i32 = "hello";
    |                  ^^^^^^^ expected i32, found string
    |
  Note: Variable declared here
   --> examples/test.paw:8:9
  ```

#### 验收标准
- ✅ 所有错误/警告使用统一接口
- ✅ 错误信息包含源码位置
- ✅ 彩色输出友好
- ✅ 所有测试通过

---

### 📌 Phase 2: 独立类型系统 (v0.2.4)
**目标**：将类型系统从 AST 和 CodeGen 中分离

**时间估计**：2-3 周

#### 任务清单

- [ ] **Step 2.1**: 创建类型系统模块
  ```bash
  mkdir -p src/types
  touch src/types/type.h
  touch src/types/type_system.h
  touch src/types/type_system.cpp
  touch src/types/type_registry.h
  touch src/types/type_registry.cpp
  ```

- [ ] **Step 2.2**: 实现 `Type` 基类和子类
  ```cpp
  class Type { virtual Kind getKind() const = 0; };
  class PrimitiveType : public Type { ... };
  class NamedType : public Type { ... };
  class ArrayType : public Type { ... };
  class ReferenceType : public Type { ... };
  // ... 其他类型
  ```

- [ ] **Step 2.3**: 实现 `TypeSystem` 管理器
  ```cpp
  class TypeSystem {
      Type* getPrimitiveType(const std::string& name);
      Type* getArrayType(Type* elem, int size);
      Type* getReferenceType(Type* pointee, bool is_mut);
      bool equals(Type* t1, Type* t2);
      bool isAssignable(Type* from, Type* to);
  };
  ```

- [ ] **Step 2.4**: 迁移类型定义
  - 从 `ast.h` 迁移 `Type` 结构体
  - 从 `codegen_stmt.cpp` 迁移 `compareTypes()`
  - 从 `codegen_type.cpp` 迁移类型转换逻辑

- [ ] **Step 2.5**: 更新所有使用处
  - 更新 `Parser` 使用新的 `TypeSystem`
  - 更新 `CodeGen` 使用新的 `TypeSystem`
  - 更新 `SymbolTable` 存储 `Type*`

- [ ] **Step 2.6**: 添加类型推导
  ```cpp
  class TypeInference {
      Type* inferExpr(Expr* expr);
      Type* inferVariable(const std::string& name, Expr* init);
  };
  ```

#### 验收标准
- ✅ 类型定义集中在 `types/` 目录
- ✅ 类型比较逻辑统一
- ✅ 类型创建通过 `TypeSystem`
- ✅ 所有测试通过

---

### 📌 Phase 3: 分离语义分析 (v0.2.5)
**目标**：将语义分析从 CodeGen 中抽离

**时间估计**：2-3 周

#### 任务清单

- [ ] **Step 3.1**: 创建语义分析模块
  ```bash
  mkdir -p src/sema
  touch src/sema/semantic_analyzer.h
  touch src/sema/semantic_analyzer.cpp
  touch src/sema/type_checker.h
  touch src/sema/type_checker.cpp
  touch src/sema/interface_validator.h
  touch src/sema/interface_validator.cpp
  ```

- [ ] **Step 3.2**: 实现 `TypeChecker`
  ```cpp
  class TypeChecker {
      bool checkExpr(Expr* expr);
      bool checkStmt(Stmt* stmt);
      bool checkFunction(FunctionStmt* func);
  };
  ```

- [ ] **Step 3.3**: 实现 `InterfaceValidator`
  ```cpp
  class InterfaceValidator {
      bool validateImpl(const std::string& type_name,
                       const std::string& interface_name);
  };
  ```

- [ ] **Step 3.4**: 从 `CodeGen` 迁移验证逻辑
  - 迁移 `validateInterfaceImpl()` → `InterfaceValidator`
  - 迁移类型检查逻辑 → `TypeChecker`
  - 保留 `CodeGen` 只负责 IR 生成

- [ ] **Step 3.5**: 实现 `SemanticAnalyzer` 协调器
  ```cpp
  class SemanticAnalyzer {
      TypeChecker* type_checker_;
      InterfaceValidator* interface_validator_;
      
      bool analyze(Module* module);
  };
  ```

- [ ] **Step 3.6**: 更新编译流程
  ```cpp
  // main.cpp
  Parser parser(...);
  auto ast = parser.parse();
  
  SemanticAnalyzer sema(...);  // 新增
  if (!sema.analyze(ast)) {
      return 1;
  }
  
  CodeGenerator codegen(...);
  codegen.generate(ast);
  ```

#### 验收标准
- ✅ 语义分析独立于代码生成
- ✅ `CodeGen` 只负责 IR 生成
- ✅ 验证逻辑集中在 `sema/`
- ✅ 所有测试通过

---

### 📌 Phase 4: Pass-Based 架构 (v0.3.0)
**目标**：引入 Pass 抽象，灵活化编译流程

**时间估计**：3-4 周

#### 任务清单

- [ ] **Step 4.1**: 定义 Pass 接口
  ```cpp
  class CompilerPass {
  public:
      virtual bool run(Module* module) = 0;
      virtual const char* getName() const = 0;
      virtual ~CompilerPass() = default;
  };
  ```

- [ ] **Step 4.2**: 实现 PassManager
  ```cpp
  class PassManager {
      std::vector<std::unique_ptr<CompilerPass>> passes_;
      
      void addPass(std::unique_ptr<CompilerPass> pass);
      bool runAll(Module* module);
  };
  ```

- [ ] **Step 4.3**: 将现有阶段改为 Pass
  ```cpp
  class ModuleResolutionPass : public CompilerPass { ... };
  class TypeInferencePass : public CompilerPass { ... };
  class TypeCheckPass : public CompilerPass { ... };
  class InterfaceValidationPass : public CompilerPass { ... };
  class CodeGenPass : public CompilerPass { ... };
  ```

- [ ] **Step 4.4**: 重构主流程
  ```cpp
  PassManager pm;
  pm.addPass(std::make_unique<ModuleResolutionPass>());
  pm.addPass(std::make_unique<TypeInferencePass>());
  pm.addPass(std::make_unique<TypeCheckPass>());
  pm.addPass(std::make_unique<InterfaceValidationPass>());
  pm.addPass(std::make_unique<CodeGenPass>());
  
  if (!pm.runAll(module)) {
      return 1;
  }
  ```

- [ ] **Step 4.5**: 添加 Pass 配置
  ```cpp
  // 可选的 Pass（如优化）
  if (options.optimize) {
      pm.addPass(std::make_unique<DeadCodeEliminationPass>());
      pm.addPass(std::make_unique<ConstantFoldingPass>());
  }
  ```

- [ ] **Step 4.6**: 实现 Pass 依赖管理
  ```cpp
  class CompilerPass {
      virtual std::vector<const char*> getRequiredPasses() const {
          return {};
      }
  };
  ```

#### 验收标准
- ✅ 所有编译阶段都是 Pass
- ✅ 可灵活添加/删除 Pass
- ✅ Pass 顺序可配置
- ✅ 所有测试通过

---

## 🧪 测试策略

### 单元测试覆盖

| 模块 | 测试文件 | 测试内容 |
|-----|---------|---------|
| Lexer | `lexer_test.cpp` | Token 识别、关键字、字面量 |
| Parser | `parser_test.cpp` | 各类语句、表达式解析 |
| TypeSystem | `type_system_test.cpp` | 类型创建、比较、转换 |
| TypeChecker | `type_checker_test.cpp` | 类型检查逻辑 |
| InterfaceValidator | `interface_validator_test.cpp` | 接口验证 |
| SymbolTable | `symbol_table_test.cpp` | 符号注册、查询 |

### 集成测试

```bash
# tests/run_tests.sh
for file in examples/*.paw; do
    ./build/pawc "$file" -o test_output
    if [ $? -ne 0 ]; then
        echo "FAIL: $file"
        exit 1
    fi
done
echo "All tests passed!"
```

---

## 📈 性能基准

### 编译时性能目标

| 文件大小 | 当前时间 | 目标时间 | 优化手段 |
|---------|---------|---------|---------|
| 100 行 | ~50ms | ~30ms | 内存池、字符串驻留 |
| 1000 行 | ~500ms | ~300ms | 增量编译 |
| 10000 行 | ~5s | ~2s | 并行编译 |

### 优化方向

1. **内存分配优化**
   - Arena 内存池（减少 malloc 调用）
   - 字符串驻留（减少字符串比较时间）

2. **并行化**
   - 并行解析多个模块
   - 并行运行独立的 Pass

3. **增量编译**
   - 缓存 AST
   - 只重新编译修改的文件

---

## 🎯 里程碑

### v0.2.3 - 诊断系统
- ✅ 统一错误报告接口
- ✅ 彩色错误输出
- ✅ 源码位置追踪

### v0.2.4 - 类型系统
- ✅ 独立的类型系统模块
- ✅ 统一的类型操作接口
- ✅ 类型推导支持

### v0.2.5 - 语义分析
- ✅ 语义分析与代码生成分离
- ✅ 独立的类型检查器
- ✅ 独立的接口验证器

### v0.3.0 - Pass 架构
- ✅ Pass-Based 编译流程
- ✅ 可配置的 Pass 管道
- ✅ 完整的单元测试覆盖

---

## 🚀 快速开始

### 立即可做的事情

1. **创建架构文档** ✅（已完成）
   ```bash
   # ARCHITECTURE.md 已创建
   ```

2. **添加第一个单元测试**
   ```bash
   # 安装 GoogleTest
   cd /path/to/paw/third-party
   git clone https://github.com/google/googletest.git
   
   # 修改 CMakeLists.txt
   find_package(GTest REQUIRED)
   add_executable(lexer_test src/lexer/lexer_test.cpp)
   ```

3. **创建诊断模块目录**
   ```bash
   mkdir -p src/diagnostics
   touch src/diagnostics/diagnostic.h
   touch src/diagnostics/diagnostic_engine.h
   ```

4. **开始替换错误报告**
   ```cpp
   // 找到所有 std::cerr
   grep -r "std::cerr" src/
   
   // 逐步替换为统一接口
   ```

---

## 💡 最佳实践

### 重构原则

1. **小步迭代**
   - 每次只改一个模块
   - 频繁运行测试
   - 保持代码可编译

2. **测试驱动**
   - 重构前：确保现有测试通过
   - 重构中：保持测试通过
   - 重构后：添加新测试

3. **代码审查**
   - 重要重构前先设计
   - 写设计文档
   - 讨论权衡

### 文档更新

每完成一个 Phase：
- 更新 `ARCHITECTURE.md`
- 更新 `README.md`（如有新特性）
- 更新 `CHANGELOG.md`
- 写博客文章（可选）

---

## 📚 参考资料

### 推荐阅读

1. **编译器架构**
   - [Crafting Interpreters](https://craftinginterpreters.com/)
   - [LLVM Programmer's Manual](https://llvm.org/docs/ProgrammersManual.html)

2. **设计模式**
   - Visitor Pattern (AST 遍历)
   - Strategy Pattern (不同的代码生成策略)
   - Factory Pattern (类型创建)

3. **类型系统**
   - [Types and Programming Languages](https://www.cis.upenn.edu/~bcpierce/tapl/)
   - [Hindley-Milner Type Inference](https://en.wikipedia.org/wiki/Hindley%E2%80%93Milner_type_system)

### 开源编译器学习

- **Rust**: Pass-Based 架构典范
- **Clang**: 诊断系统设计优秀
- **GCC**: 成熟的 Pass 管理
- **Swift**: 类型系统设计先进

---

## ✅ 结论

PawLang v0.2.2 已经具备良好的基础，通过 **4 个 Phase** 的渐进式重构，将演进为一个：

- 🏗️ **架构清晰**：Pass-Based + 分层明确
- 🔧 **易于维护**：模块解耦 + 职责单一
- 🚀 **高性能**：内存优化 + 并行编译
- 🧪 **测试完善**：单元测试 + 集成测试

**预计时间线**：8-12 周完成 v0.3.0

**风险**：低（增量式重构，每步可验证）

---

**🐾 让我们开始构建更好的 PawLang 编译器！**

*最后更新：2025-10-27*  
*当前版本：v0.2.2*  
*目标版本：v0.3.0*

