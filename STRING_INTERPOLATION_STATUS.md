# 字符串插值 (String Interpolation) - 实现状态

> 📅 **日期**: 2025-10-27  
> 🎯 **目标**: 实现 f"..." 语法  
> 📊 **进度**: 70% (功能完成，Parser集成需调试)

---

## ✅ 已完成

### 1. Lexer 层面（100%）
- ✅ 添加 `F_STRING` token类型到 `TokenType`
- ✅ 实现 `Lexer::fstring()` 方法
- ✅ 支持插值表达式提取 `{expr}`
- ✅ 支持转义 `{{` 和 `}}`
- ✅ 编码格式：`<count>|part0|expr0|part1|...`
- ✅ 错误处理：未闭合的 `{`, 空表达式

**文件**: 
- `include/pawc/common.h` (TokenType::F_STRING)
- `src/lexer/lexer.h` (fstring声明)
- `src/lexer/lexer.cpp` (fstring实现, 120行)

**测试**:
```bash
# Lexer 正确识别 f"Hello, {name}!"
✓ Token类型识别正常
✓ 表达式提取正常
✓ 转义处理正常
```

---

### 2. AST 层面（100%）
- ✅ 添加 `Expr::Kind::FString`
- ✅ 定义 `FStringExpr` 结构
  ```cpp
  struct FStringExpr {
      std::vector<std::string> parts;
      std::vector<ExprPtr> expressions;
  }
  ```

**文件**:
- `src/parser/ast.h` (FStringExpr定义)

---

### 3. Parser 层面（90%）
- ✅ 实现 `Parser::parseFString()` 方法
- ✅ 解析 f-string 编码
- ✅ 递归解析内部表达式（临时Lexer+Parser）
- ⚠️ **需要调试**: `parseFString()` 未被正确调用

**文件**:
- `src/parser/parser.h` (parseFString声明)
- `src/parser/parser_fstring.cpp` (实现, 95行)
- `CMakeLists.txt` (添加编译源)

**问题**:
```
// Parser 未处理 F_STRING token
// 需要在 primary() 或类似函数中添加:
if (match({TokenType::F_STRING})) {
    return parseFString();
}
```

---

### 4. CodeGen 层面（100%）
- ✅ 实现 `generateFStringExpr()`
- ✅ 字符串拼接（`concatenateStrings`）
- ✅ 类型转字符串（`convertToString`）
- ✅ 基础类型转换：
  - `i32 -> string` (intToString)
  - `f64 -> string` (floatToString)
  - `bool -> string` (true/false)
  - `char -> string` (charToString)
- ✅ LLVM 21+ API 兼容（PointerType::get）

**文件**:
- `src/codegen/codegen.h` (函数声明)
- `src/codegen/codegen_expr.cpp` (case FString)
- `src/codegen/codegen_fstring.cpp` (实现, 350行)
- `CMakeLists.txt` (添加编译源)

**实现细节**:
```cpp
// 字符串拼接：malloc + strcpy + strcat
// i32 -> string: malloc + snprintf("%d", ...)
// f64 -> string: malloc + snprintf("%g", ...)
// char -> string: malloc + buf[0]=char, buf[1]='\0'
```

---

### 5. 文档（100%）
- ✅ 设计文档 `STRING_INTERPOLATION_DESIGN.md`
  - 语法设计
  - 实现方案
  - 测试用例
  - 性能考虑

---

## ❌ 未完成

### 1. Parser 集成（10%剩余）
**问题**: Parser 无法识别 F_STRING token

**错误信息**:
```
error: Expected expression
 --> examples/fstring_simple.paw:5:15
  |
5 |     let msg = f"{x}";
  |               ^^^^^
```

**诊断**:
- Lexer 正确生成了 F_STRING token（从 "118 tokens" 可见）
- Parser 在 `primary()` 或 `unary()` 中未处理 F_STRING

**需要的修复**:
1. 找到Parser中处理 STRING token的位置
2. 在同样位置添加 F_STRING 处理
3. 调用 `parseFString()`

**预期位置** (parser.cpp):
```cpp
// 某个 primary() 或 unary() 函数
if (match({TokenType::STRING})) {
    return std::make_unique<StringExpr>(previous().value, previous().location);
}

// 添加：
if (match({TokenType::F_STRING})) {
    return parseFString();
}
```

---

### 2. 测试和验证（0%）
- ❌ 基础变量插值测试
- ❌ 表达式插值测试
- ❌ 类型转换测试
- ❌ 转义测试
- ❌ 空f-string测试

**测试文件准备**:
- `examples/string_interpolation_test.paw` (完整测试)
- `examples/fstring_simple.paw` (简单测试)

---

## 🐛 当前问题

### 问题 1: Parser未调用parseFString
**症状**: 编译报错 "Expected expression"  
**原因**: F_STRING token未在Parser表达式解析中处理  
**影响**: 无法编译任何f-string代码  
**优先级**: P0 (阻塞)

**修复方案**:
1. 搜索 parser.cpp 中 STRING 的处理位置
2. 在相同位置添加 F_STRING 处理
3. 测试

---

## 📊 文件清单

| 文件 | 状态 | 行数 | 说明 |
|------|------|------|------|
| `include/pawc/common.h` | ✅ | +1 | F_STRING token |
| `src/lexer/lexer.h` | ✅ | +1 | fstring声明 |
| `src/lexer/lexer.cpp` | ✅ | +120 | fstring实现 |
| `src/parser/ast.h` | ✅ | +8 | FStringExpr定义 |
| `src/parser/parser.h` | ✅ | +1 | parseFString声明 |
| `src/parser/parser_fstring.cpp` | ✅ | +95 | Parser实现 |
| `src/parser/parser.cpp` | ⚠️ | +0 | 需要添加调用 |
| `src/codegen/codegen.h` | ✅ | +7 | 函数声明 |
| `src/codegen/codegen_expr.cpp` | ✅ | +3 | case FString |
| `src/codegen/codegen_fstring.cpp` | ✅ | +350 | CodeGen实现 |
| `STRING_INTERPOLATION_DESIGN.md` | ✅ | +635 | 设计文档 |
| **总计** | **96%** | **~1220** | |

---

## 🚀 下一步

### 立即行动
1. 修复 Parser 集成（预计10分钟）
2. 测试基础用例（预计5分钟）
3. 修复发现的bug（预计20分钟）

### 后续优化（可选）
1. 性能优化：预分配字符串缓冲区
2. 支持自定义类型（to_string方法）
3. 支持Display接口
4. 编译时常量折叠

---

## 📝 提交信息

```bash
git commit -m "feat: 字符串插值 (String Interpolation) - WIP

- 添加 F_STRING token类型
- 实现 Lexer f-string 识别和解析（支持 {expr}、{{、}}）
- 添加 FStringExpr AST 节点
- 实现 Parser f-string 解析逻辑
- 实现 CodeGen 字符串拼接和类型转换
- 支持基础类型转字符串（i32, f64, bool, char）
- 添加设计文档 STRING_INTERPOLATION_DESIGN.md

注: 功能完成约70%，Parser集成尚需调试"
```

---

**🐾 字符串插值实现进展：70% 完成！**

*最后更新: 2025-10-27*

