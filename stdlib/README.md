# 📚 PawLang Standard Library (stdlib)

**版本**: v0.2.0  
**状态**: 活跃开发中

---

## 📋 概述

PawLang 标准库提供了常用功能的扩展实现。与自动导入的 **Prelude** 不同，stdlib 中的模块需要**手动导入**。

### Prelude vs Stdlib

| 特性 | Prelude | Stdlib |
|------|---------|--------|
| 导入 | 自动 | 手动 `import` |
| 内容 | 基础功能 | 扩展功能 |
| 大小 | 精简 (~10KB) | 较大 (按需) |
| 位置 | `src/prelude/` | `stdlib/` |

---

## 📦 可用模块

### 1. string - 字符串操作 ✅

**路径**: `stdlib/string/mod.paw`  
**状态**: ✅ 稳定

**功能**:
```paw
import string;

// 高级字符串操作
let builder = string::StringBuilder::new();
builder.append_string("Hello");
builder.append_char(' ');
builder.append_i32(42);

// 字符串解析
let num = string::parse_i32("12345");

// 注意：基础操作（length, char_at 等）在 Prelude 中，无需 import
```

**包含**:
- ✅ StringBuilder（固定缓冲区）
- ✅ 字符串解析（parse_i32）
- ✅ 字符串比较（starts_with, ends_with）

**Prelude 已提供**:
- `string_length()`
- `char_at()`
- `string_equals()`
- `is_whitespace()` / `is_digit()` / `is_alpha()`

---

### 2. json - JSON 解析 🚧

**路径**: `stdlib/json/`  
**状态**: 🚧 开发中

**文件**:
- `simple.paw` - 简化版（基础类型）✅
- `mod.paw` - 完整版（待完善）🚧

**功能**:
```paw
import json;

// 基础类型解析（v0.2.0）
let null_val = json::parse_null();
let bool_val = json::parse_bool(true);
let num_val = json::parse_number(42);
let str_val = json::parse_string("hello");

// 序列化
let json_str = json::stringify(num_val);
```

**当前限制**:
- ⚠️ 不支持嵌套数组
- ⚠️ 不支持嵌套对象
- ⚠️ 等待完整的 Lexer/Parser 实现

**计划** (v0.3.0):
- 完整的 JSON 解析器
- 嵌套数组和对象支持
- 完整的转义处理

---

### 3. fs - 文件系统 ✅

**路径**: `stdlib/fs/mod.paw`  
**状态**: ✅ API 定义完成（等待 FFI）

**功能**:
```paw
import fs;

// 文件读写
let content = fs::read_file("data.txt");
fs::write_file("output.txt", "Hello");
fs::append_file("log.txt", "Entry\n");

// 文件检查
if fs::exists("config.json") {
    let size = fs::file_size("config.json");
}

// 目录操作
fs::create_dir("output");
fs::create_dir_all("path/to/deep/dir");
fs::delete_dir_all("temp");
```

**包含**:
- ✅ 文件读写（read, write, append）
- ✅ 文件检查（exists, is_dir, file_size）
- ✅ 文件操作（delete, rename, copy）
- ✅ 目录操作（create, delete）
- ✅ 路径工具（extension, filename, join）

**底层实现**: `src/builtin/fs.zig` (295 行 Zig 代码)

**当前限制**:
- ⚠️ 等待编译器 FFI 支持
- ⚠️ API 可用，但当前返回占位符

---

### 4. collections - 数据结构 🚧

**路径**: `stdlib/collections/`  
**状态**: 🚧 部分完成

**文件**:
- `vec.paw` - 动态数组 🚧
- `box.paw` - 智能指针 ✅

**功能**:
```paw
import collections;

// Vec<T> - 动态数组
// 注意：基础定义在 Prelude，无需 import
let v: Vec<i32> = Vec::new();

// Box<T> - 智能指针
let boxed: Box<i32> = Box::new(42);
let value: i32 = boxed.get();
```

**当前限制**:
- ⚠️ Vec<T> 需要动态内存支持
- ⚠️ 等待 FFI 集成

**计划** (v0.3.0):
- HashMap<K, V>
- HashSet<T>
- LinkedList<T>

---

### 5. io - 输入输出 ✅

**路径**: `stdlib/io/`  
**状态**: ✅ 基础完成

**功能**:
```paw
// 注意：基础 I/O 在 Prelude 中
// println, print, eprintln, eprint 自动可用

// stdlib/io 提供扩展功能（未来）
// - 格式化输出
// - 缓冲 I/O
// - 文件流
```

**当前状态**:
- ✅ `print.paw` - 基础打印（已整合到 Prelude）

---

## 🚀 使用示例

### 示例 1: 配置文件管理

```paw
import fs;
import json;

fn load_config() -> string {
    if fs::exists("config.json") {
        return fs::read_file("config.json");
    } else {
        let default_config = "{\"version\": \"1.0\"}";
        fs::write_file("config.json", default_config);
        return default_config;
    }
}

fn main() -> i32 {
    let config = load_config();
    println(config);
    return 0;
}
```

---

### 示例 2: 日志系统

```paw
import fs;

fn log_message(level: string, msg: string) -> i32 {
    // 使用 Prelude 的字符串函数（无需 import）
    let timestamp = "2025-10-12 16:00:00";  // TODO: 实际时间戳
    
    let entry = timestamp;
    // TODO: 字符串拼接
    
    fs::append_file("app.log", entry);
    return 0;
}

fn main() -> i32 {
    log_message("INFO", "Application started");
    log_message("DEBUG", "Processing request");
    log_message("INFO", "Application stopped");
    return 0;
}
```

---

### 示例 3: 文本处理

```paw
import string;

fn process_text(input: string) -> string {
    // 使用 Prelude 的基础函数（无需 import）
    let len = string_length(input);
    
    // 使用 stdlib/string 的高级功能
    let mut builder = string::StringBuilder::new();
    
    let mut i: i32 = 0;
    loop {
        if i >= len {
            break;
        }
        
        let ch = char_at(input, i);
        
        // 使用 Prelude 的字符判断（无需 import）
        if is_alpha(ch) {
            builder.append_char(ch);
        }
        
        i += 1;
    }
    
    return "processed";  // TODO: 返回 builder 结果
}

fn main() -> i32 {
    let result = process_text("Hello123World!");
    println(result);
    return 0;
}
```

---

## 📊 模块状态总览

| 模块 | 状态 | 完成度 | FFI 需求 |
|------|------|--------|---------|
| **string** | ✅ | 80% | 部分 |
| **json** | 🚧 | 60% | 否 |
| **fs** | ✅ | 100% (API) | 是 |
| **collections** | 🚧 | 40% | 是 |
| **io** | ✅ | 80% | 否 |

**图例**:
- ✅ 可用
- 🚧 开发中
- ⏳ 计划中

---

## 🔧 开发指南

### 添加新模块

1. **创建目录**
   ```bash
   mkdir -p stdlib/mymodule
   ```

2. **创建模块文件**
   ```paw
   // stdlib/mymodule/mod.paw
   
   pub fn my_function() -> i32 {
       return 0;
   }
   ```

3. **编写测试**
   ```paw
   // examples/test_mymodule.paw
   import mymodule;
   
   fn main() -> i32 {
       return mymodule::my_function();
   }
   ```

4. **更新文档**
   - 添加到 `stdlib/README.md`
   - 创建 `stdlib/mymodule/README.md`

---

### 设计原则

1. **分层明确**
   - Prelude: 90% 代码会用的
   - Stdlib: 特定场景需要的

2. **纯 Paw 优先**
   - 尽量用 PawLang 实现
   - 只在必要时用 Zig

3. **性能优先**
   - 避免不必要的复制
   - 使用固定缓冲区
   - 延迟分配

4. **API 稳定**
   - 一旦发布，尽量不改
   - 遵循语义化版本

---

## 📚 相关文档

### Prelude
- `src/prelude/prelude.paw` - Prelude 实现
- `docs/PRELUDE_V0.2.0.md` - Prelude 文档
- `docs/PRELUDE_EXPLANATION.md` - Prelude 说明

### Stdlib 模块
- `stdlib/string/mod.paw` - 字符串模块
- `stdlib/json/simple.paw` - JSON 简化版
- `stdlib/fs/mod.paw` - 文件系统 API

### 底层实现
- `src/builtin/memory.zig` - 内存管理
- `src/builtin/fs.zig` - 文件系统底层
- `docs/FILESYSTEM_API.md` - 文件系统文档

---

## 🔮 未来计划

### v0.3.0

**优先级 1: FFI 集成**
- 使 stdlib/fs 可用
- 启用动态内存
- 完善 collections

**优先级 2: 字符串增强**
- 字符串分割
- 字符串拼接
- 正则表达式（简化版）

**优先级 3: JSON 完善**
- 完整的 Parser
- 嵌套结构支持
- 转义处理

### v0.4.0

**新模块**:
- `stdlib/net` - 网络操作
- `stdlib/time` - 时间日期
- `stdlib/crypto` - 加密哈希
- `stdlib/regex` - 正则表达式

**增强现有模块**:
- `stdlib/string` - UTF-8 支持
- `stdlib/fs` - 流式 I/O
- `stdlib/collections` - 更多数据结构

---

## 💡 贡献指南

### 如何贡献

1. **选择模块** - 查看计划中的功能
2. **设计 API** - 确保简洁易用
3. **实现功能** - 优先纯 Paw
4. **编写测试** - 确保功能正确
5. **更新文档** - 添加示例和说明

### 代码规范

- ✅ 使用 `snake_case` 命名
- ✅ 添加注释和文档
- ✅ 提供使用示例
- ✅ 保持简单直接

---

## 📞 联系方式

- **GitHub**: [PawLang Repository]
- **文档**: `docs/` 目录
- **示例**: `examples/` 目录

---

## ✅ 总结

### 当前状态

- ✅ Prelude 完善（24 个函数/类型）
- ✅ 5 个 stdlib 模块基础完成
- 🚧 等待 FFI 集成
- 📚 文档完整

### 关键特性

- 🎯 Prelude 自动导入
- 📦 Stdlib 按需使用
- 🚀 纯 Paw 实现优先
- 🔧 清晰的分层设计

---

**PawLang Stdlib - Powerful, Simple, Efficient!** 🚀

---

**维护者**: PawLang 核心开发团队  
**最后更新**: 2025-10-12  
**版本**: v0.2.0  
**许可**: MIT

