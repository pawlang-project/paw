# PawLang 错误报告系统

## 概述

PawLang编译器现在配备了现代化的错误报告系统（`ErrorReporter`），提供：

- ✅ **彩色输出** - 使用ANSI颜色增强可读性
- ✅ **代码片段** - 显示出错的代码行
- ✅ **位置指示** - 使用^和~标记出错位置
- ✅ **修复建议** - 提供有用的帮助信息
- ✅ **多级别** - Error、Warning、Note

## 使用方法

### 基础用法

```cpp
#include "pawc/error_reporter.h"

// 创建报告器
pawc::ErrorReporter reporter;

// 设置源代码（用于显示代码片段）
reporter.setSourceCode("example.paw", source_code);

// 报告错误
pawc::SourceLocation loc("example.paw", 10, 15);
reporter.reportError("unexpected token", loc);

// 带修复建议的错误
std::vector<pawc::ErrorHint> hints = {
    pawc::ErrorHint("expected ';' after statement"),
    pawc::ErrorHint("try adding a semicolon here")
};
reporter.reportError("syntax error", loc, hints);

// 报告警告
reporter.reportWarning("unused variable", loc);

// 检查是否有错误
if (reporter.hasErrors()) {
    reporter.printSummary();
    return 1;
}
```

### 错误输出示例

```
error: mismatched types
  --> example.paw:2:18
   |
 2 |     let x: i32 = "hello";
   |                  ^~~~~~~
  = help: expected type 'i32', found type 'string'
  = help: try using an integer literal like '42' instead

warning: unused variable 'x'
  --> example.paw:2:9
   |
 2 |     let x: i32 = "hello";
   |         ^
  = help: consider prefixing with an underscore: '_x'

error: could not compile due to 1 error
warning: 1 warning emitted
```

## API文档

### ErrorReporter类

#### 主要方法

**reportError(message, location, hints = {})**
- 报告编译错误
- 自动递增错误计数
- 显示代码片段和修复建议

**reportWarning(message, location, hints = {})**
- 报告编译警告
- 自动递增警告计数

**reportNote(message, location = {})**
- 报告提示信息
- 通常用于补充说明

**setSourceCode(filename, code)**
- 设置源代码
- 用于提取和显示代码片段

**hasErrors() -> bool**
- 检查是否有错误

**getErrorCount() -> int**
- 获取错误总数

**getWarningCount() -> int**
- 获取警告总数

**printSummary()**
- 打印错误和警告摘要

**clear()**
- 清空所有错误和警告

### ErrorHint结构

```cpp
struct ErrorHint {
    std::string message;
    std::optional<SourceLocation> location;
};
```

用于提供修复建议和补充说明。

## 集成到编译器

### 当前状态

ErrorReporter基础设施已完成，但尚未完全集成到：
- Parser（语法分析）
- CodeGen（代码生成）

### 未来改进

1. **Parser集成** - 替换当前的CompilerError系统
2. **CodeGen集成** - 为类型错误、未定义符号等提供更好的错误消息
3. **增强建议** - 基于上下文的智能修复建议
4. **错误恢复** - 更好的错误恢复机制
5. **批量错误** - 一次编译显示多个错误

## 示例程序

查看 `examples/error_reporter_demo.cpp` 获取完整示例。

## 编译

ErrorReporter已集成到CMake构建系统：

```bash
cmake --build build
```

## 设计理念

参考Rust和Clang的错误报告系统：
- 清晰的视觉层次
- 有用的修复建议
- 完整的上下文信息
- 友好的用户体验

---

**版本**: PawLang v0.2.1  
**状态**: 基础设施完成，等待集成

