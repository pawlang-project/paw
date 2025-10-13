# FFI (外部函数接口) 需求文档

**版本**: v0.2.0  
**日期**: 2025-10-12  
**优先级**: ⭐⭐⭐⭐ 高

---

## 🎯 目标

为PawLang添加FFI支持，使其能够调用Zig导出的C ABI函数，实现文件系统、网络等系统级功能。

---

## 📋 需求背景

### 当前状况

PawLang目前有以下内置函数实现方式：

```paw
// 在 prelude.paw 中声明
fn println(msg: string) -> i32 { ... }

// 在 codegen.zig 中特殊处理
else if (std.mem.eql(u8, func_name, "println")) {
    try self.output.appendSlice(self.allocator, "printf(\"%s\\n\", ");
    // ... 生成C代码
}
```

**问题**：
- ❌ 每个内置函数都需要修改codegen
- ❌ 不灵活，难以扩展
- ❌ 标准库无法独立发展

### 需要实现的功能

文件系统API需要调用Zig导出的函数：

```zig
// src/builtin/fs.zig
export fn paw_file_exists(path_ptr: [*]const u8, path_len: usize) i32 {
    // ... Zig实现
}
```

```paw
// stdlib/fs/mod.paw
extern fn paw_file_exists(path_ptr: string, path_len: i32) -> i32;

pub fn exists(path: string) -> bool {
    let len = string_length(path);
    let result = paw_file_exists(path, len);
    return result == 1;
}
```

---

## 🎨 设计方案

### 方案A：extern关键字 (推荐)

#### 语法设计

```paw
// 1. 声明外部函数
extern fn function_name(param: type) -> return_type;

// 2. 使用外部函数
let result = function_name(arg);
```

#### 完整示例

```paw
// stdlib/fs/mod.paw

// 声明Zig导出的函数
extern fn paw_file_exists(path: string, len: i32) -> i32;
extern fn paw_read_file(path: string, len: i32) -> i64;
extern fn paw_write_file(path: string, path_len: i32, 
                          content: string, content_len: i32) -> i32;

// 包装为用户友好的API
pub fn exists(path: string) -> bool {
    let len = string_length(path);
    let result = paw_file_exists(path, len);
    return result == 1;
}

pub fn read_file(path: string) -> string {
    let len = string_length(path);
    let ptr = paw_read_file(path, len);
    // TODO: 如何处理返回的指针？
    return "";  // 临时
}
```

#### 编译器改动

**1. Lexer层** (`src/lexer.zig`)：
```zig
// 添加新关键字
.extern => Token{ .type = .keyword_extern, ... }
```

**2. Parser层** (`src/parser.zig`)：
```zig
// 解析extern函数声明
fn parseFunctionDecl(...) {
    const is_extern = self.match(.keyword_extern);
    // ... 
    return FunctionDecl{
        .is_extern = is_extern,
        // ...
    };
}
```

**3. AST层** (`src/ast.zig`)：
```zig
pub const FunctionDecl = struct {
    is_extern: bool,  // 新增字段
    // ...
};
```

**4. TypeChecker层** (`src/typechecker.zig`)：
```zig
// extern函数跳过body检查
if (func.is_extern) {
    if (func.body) |_| {
        return error.ExternFunctionHasBody;
    }
    return;  // 不检查body
}
```

**5. CodeGen层** (`src/codegen.zig`)：
```zig
fn generateFunctionDecl(func: FunctionDecl) {
    if (func.is_extern) {
        // 生成extern声明
        try self.output.appendSlice("extern ");
    }
    
    // 生成函数签名
    try self.generateFunctionSignature(func);
    
    if (func.is_extern) {
        try self.output.appendSlice(";\n");  // 仅声明
    } else {
        // 生成函数体
        try self.generateFunctionBody(func);
    }
}
```

---

### 方案B：@builtin特殊语法

```paw
// 使用特殊的builtin命名空间
@builtin::fs::file_exists(path)
```

**优点**：
- 明确标识内置函数
- 可以有特殊的处理逻辑

**缺点**：
- 语法复杂
- 不标准

---

## 🔧 实现步骤

### Phase 1: 基础FFI (1-2周)

1. **添加extern关键字**
   - Lexer识别extern
   - Parser解析extern声明
   - AST添加is_extern字段

2. **CodeGen支持**
   - 生成extern声明的C代码
   - 处理extern函数调用

3. **基础测试**
   - 简单的extern函数测试
   - 验证编译和链接

### Phase 2: 类型映射 (1周)

4. **PawLang类型 → C类型映射**
   ```
   i32     → int32_t
   i64     → int64_t
   f64     → double
   bool    → uint8_t (1/0)
   string  → const char*
   ```

5. **指针处理**
   - string自动传递为指针
   - 长度参数自动计算

### Phase 3: 高级特性 (1-2周)

6. **自动参数转换**
   ```paw
   extern fn paw_file_exists(path: string, len: i32) -> i32;
   
   // 用户调用时自动插入len参数
   let result = paw_file_exists(path);  // 自动添加 string_length(path)
   ```

7. **返回值处理**
   - 指针返回值的包装
   - 错误处理

---

## 📊 优先级评估

| 任务 | 重要性 | 紧急性 | 难度 | 优先级 |
|------|--------|--------|------|--------|
| extern关键字 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐ | P0 |
| CodeGen支持 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ | P0 |
| 类型映射 | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ | P1 |
| 自动转换 | ⭐⭐⭐ | ⭐⭐ | ⭐⭐⭐⭐ | P2 |

---

## 🎯 预期成果

实现FFI后，PawLang将能够：

### 1. 文件系统API
```paw
import fs.{read_file, write_file, exists};

if exists("config.txt") {
    let content = read_file("config.txt");
    println(content);
}
```

### 2. 网络API (未来)
```paw
import net.{TcpListener, TcpStream};

let listener = TcpListener::bind("127.0.0.1:8080");
```

### 3. 系统API (未来)
```paw
import sys.{args, env, exit};

let home = env("HOME");
```

---

## 📚 参考资料

### 其他语言的FFI实现

**Rust FFI**:
```rust
extern "C" {
    fn abs(input: i32) -> i32;
}

unsafe {
    let result = abs(-42);
}
```

**Zig extern**:
```zig
extern fn printf(format: [*:0]const u8, ...) c_int;
```

**Go cgo**:
```go
// #include <stdlib.h>
import "C"
```

---

## 🐛 潜在问题

### 1. 内存管理
**问题**: Zig分配的内存如何在PawLang中释放？

**解决方案**:
```paw
extern fn paw_read_file(path: string, len: i32) -> i64;
extern fn paw_free_file_content(ptr: i64, len: i32) -> void;

pub fn read_file(path: string) -> string {
    let ptr = paw_read_file(path, string_length(path));
    // TODO: 如何自动管理内存？
    // 需要RAII或defer支持
}
```

### 2. 字符串编码
**问题**: PawLang string vs C char*

**解决方案**: 确保都使用UTF-8

### 3. 错误处理
**问题**: C函数返回错误码，如何映射到PawLang？

**解决方案**:
```paw
pub type FsResult = enum {
    Ok(string),
    Err(i32),  // 错误码
}

pub fn read_file(path: string) -> FsResult {
    let result = paw_read_file(...);
    if result == 0 {
        return Err(-1);
    }
    return Ok(content);
}
```

---

## ✅ 验收标准

FFI实现完成需满足：

1. ✅ **能够声明extern函数**
2. ✅ **能够调用extern函数**
3. ✅ **类型正确映射**
4. ✅ **链接成功**
5. ✅ **至少3个文件系统函数可用**
6. ✅ **文档完整**
7. ✅ **测试通过**

---

## 📅 时间估算

| 阶段 | 工作量 | 时间 |
|------|--------|------|
| 设计 | 完成 | ✅ |
| Phase 1 | 中等 | 1-2周 |
| Phase 2 | 较小 | 1周 |
| Phase 3 | 较大 | 1-2周 |
| 测试和文档 | 中等 | 1周 |
| **总计** | - | **4-6周** |

---

## 🤝 需要的支持

1. **编译器开发者**
   - 实现extern关键字
   - 修改codegen

2. **测试工程师**
   - 设计测试用例
   - 跨平台测试

3. **文档工程师**
   - FFI使用指南
   - API文档

---

## 📝 后续任务

FFI完成后的扩展：

1. ✅ 文件系统API完整实现
2. 🔜 网络API
3. 🔜 系统调用API
4. 🔜 第三方C库集成
5. 🔜 性能优化

---

**Status**: 📋 需求文档已完成  
**Next**: 等待编译器团队评审和排期  
**Contact**: 通过GitHub Issues讨论

**Created for PawLang v0.2.0** 🐾

