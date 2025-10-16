# CodeGen重构计划

## 当前状态
- **文件**: `src/codegen/codegen.cpp`
- **行数**: 3936行
- **问题**: 单文件过大，难以维护

## 拆分方案

### 文件结构

```
src/codegen/
├── codegen.h              (保持不变 - 145行)
├── codegen.cpp            (核心 - ~300行) ✨
├── codegen_expr.cpp       (表达式 - ~1000行) 🆕
├── codegen_stmt.cpp       (语句 - ~800行) 🆕
├── codegen_type.cpp       (类型和泛型 - ~1000行) 🆕
├── codegen_struct.cpp     (Struct/Enum - ~700行) 🆕
└── codegen_match.cpp      (模式匹配 - ~300行) 🆕
```

### 1. codegen.cpp (核心 - ~300行)

**保留内容**:
- 文件头注释
- #include语句
- CodeGenerator构造函数（2个）
- generate() - 主入口
- printIR()
- saveIR()
- compileToObject()
- importTypeFromModule()

### 2. codegen_expr.cpp (表达式 - ~1000行)

**迁移函数** (行号范围):
- generateExpr() - 405-462
- generateIdentifierExpr() - 3390-3442
- generateBinaryExpr() - 463-532
- generateUnaryExpr() - 533-546
- generateCallExpr() - 547-843
- generateArgumentValue() - 844-863
- generateBuiltinCall() - 864-898
- generateIndexExpr() - 2584-2624
- generateCastExpr() - 3495-3606
- generateIfExpr() - 3443-3494
- generateTryExpr() - 3607-3666
- generateOkExpr() - 3667-3721
- generateErrExpr() - 3722-3784

### 3. codegen_stmt.cpp (语句 - ~800行)

**迁移函数** (行号范围):
- generateStmt() - 899-951
- generateLetStmt() - 1066-1331
- generateReturnStmt() - 1332-1350
- generateIfStmt() - 1351-1483
- generateLoopStmt() - 1484-1688
- generateBreakStmt() - 1689-1699
- generateContinueStmt() - 1700-1710
- generateBlockStmt() - 1711-1716
- generateExprStmt() - 1717-1720

### 4. codegen_type.cpp (类型和泛型 - ~1000行)

**迁移函数** (行号范围):
- convertType() - 141-278
- convertPrimitiveType() - 280-309
- getOrCreateStructType() - 311-362
- getEnumType() - 364-403
- createOptionalType() - 3535-3564
- ensureOptionalEnumDef() - 3565-3654
- resolveGenericType() - 2763-2783
- mangleGenericName() - 2731-2760
- resolveGenericStructName() - 2786-2825
- isGenericFunction() - 2827-2833
- instantiateGenericFunction() - 2835-3052
- instantiateGenericStruct() - 3054-3130
- instantiateGenericEnum() - 3132-3179
- instantiateGenericStructMethods() - 3790-3934

### 5. codegen_struct.cpp (Struct/Enum - ~700行)

**迁移函数** (行号范围):
- generateFunctionStmt() - 952-1064
- generateExternStmt() - 1725-1768
- generateStructStmt() - 1769-1820
- generateEnumStmt() - 1821-1838
- generateImplStmt() - 1840-1858
- generateAssignExpr() - 1859-2017
- generateMemberAccessExpr() - 2018-2154
- generateStructLiteralExpr() - 2155-2232
- generateEnumVariantExpr() - 2233-2288
- generateArrayLiteralExpr() - 2570-2583

### 6. codegen_match.cpp (模式匹配 - ~300行)

**迁移函数** (行号范围):
- generateMatchExpr() - 2289-2440
- generateIsExpr() - 2441-2569
- matchPattern() - 2625-2729

## 实施步骤

1. ✅ 备份当前codegen.cpp
2. 🔨 创建6个新文件
3. 📝 添加Doxygen注释头
4. ✂️ 迁移函数到对应文件
5. 🔧 更新CMakeLists.txt
6. ✅ 测试编译
7. 📊 验证功能
8. 💾 Git提交

## Doxygen注释风格

```cpp
/**
 * @brief 生成二元表达式的LLVM IR
 * 
 * @param expr 二元表达式AST节点
 * @return llvm::Value* 生成的LLVM值，失败返回nullptr
 * 
 * 支持的操作:
 * - 算术: +, -, *, /, %
 * - 比较: ==, !=, <, <=, >, >=
 * - 逻辑: &&, ||
 * 
 * @note 自动处理整数类型提升和字符串拼接
 */
llvm::Value* CodeGenerator::generateBinaryExpr(const BinaryExpr* expr) {
    // ...
}
```

## 预期结果

- 每个文件 < 1000行
- 清晰的功能分离
- 完整的Doxygen注释
- 编译并行化
- 易于维护和扩展

---

**开始时间**: 2025-10-16  
**预计完成**: 2-3小时  
**状态**: 进行中

