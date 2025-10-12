# 📁 FileSystem Module

**路径**: `stdlib/fs/mod.paw`  
**版本**: v0.2.0  
**状态**: ✅ API 完成（等待 FFI 集成）

---

## 📋 概述

文件系统模块提供跨平台的文件和目录操作功能。

### 实现层次

```
stdlib/fs/mod.paw (Paw API)
         ↓
src/builtin/fs.zig (Zig 系统调用)
         ↓
操作系统 (Windows/Linux/macOS)
```

---

## 🔧 API 参考

### 文件读写

```paw
import fs;

// 读取文件
let content: string = fs::read_file("data.txt");

// 写入文件（覆盖）
fs::write_file("output.txt", "Hello, World!");

// 追加到文件
fs::append_file("log.txt", "New entry\n");
```

**函数**:
- `read_file(path: string) -> string`
- `write_file(path: string, content: string) -> bool`
- `append_file(path: string, content: string) -> bool`

---

### 文件检查

```paw
import fs;

// 检查文件是否存在
if fs::exists("config.json") {
    let size: i32 = fs::file_size("config.json");
    println("File size: " + size);
}

// 检查是否为目录
if fs::is_dir("data") {
    println("It's a directory");
}
```

**函数**:
- `exists(path: string) -> bool`
- `is_dir(path: string) -> bool`
- `file_size(path: string) -> i32`

---

### 文件操作

```paw
import fs;

// 删除文件
fs::delete_file("temp.txt");

// 重命名/移动
fs::rename("old.txt", "new.txt");

// 复制文件
fs::copy_file("source.txt", "backup.txt");
```

**函数**:
- `delete_file(path: string) -> bool`
- `rename(old_path: string, new_path: string) -> bool`
- `copy_file(src: string, dst: string) -> bool`

---

### 目录操作

```paw
import fs;

// 创建目录
fs::create_dir("output");

// 递归创建
fs::create_dir_all("path/to/deep/dir");

// 删除目录
fs::delete_dir("empty_dir");

// 递归删除
fs::delete_dir_all("old_project");  // ⚠️ 危险
```

**函数**:
- `create_dir(path: string) -> bool`
- `create_dir_all(path: string) -> bool`
- `delete_dir(path: string) -> bool`
- `delete_dir_all(path: string) -> bool`

---

### 路径工具

```paw
import fs;

let ext = fs::extension("document.pdf");      // "pdf"
let name = fs::filename("/path/to/file.txt"); // "file.txt"
let dir = fs::parent("/path/to/file.txt");    // "/path/to"
let full = fs::join("data", "config.json");   // "data/config.json"
```

**函数**:
- `extension(path: string) -> string`
- `filename(path: string) -> string`
- `parent(path: string) -> string`
- `join(base: string, part: string) -> string`
- `normalize(path: string) -> string`

---

## 💡 完整示例

### 示例 1: 配置文件管理

```paw
import fs;

fn load_or_create_config() -> string {
    let config_path = "config.json";
    
    if fs::exists(config_path) {
        return fs::read_file(config_path);
    } else {
        let default_config = "{\"version\": \"1.0\"}";
        fs::write_file(config_path, default_config);
        return default_config;
    }
}

fn main() -> i32 {
    let config = load_or_create_config();
    println("Config loaded");
    return 0;
}
```

---

### 示例 2: 日志系统

```paw
import fs;

fn init_log() -> bool {
    fs::create_dir_all("logs");
    return fs::write_file("logs/app.log", "=== Log Started ===\n");
}

fn log(message: string) -> bool {
    return fs::append_file("logs/app.log", message);
}

fn main() -> i32 {
    init_log();
    log("Application started\n");
    log("Processing data...\n");
    log("Application stopped\n");
    return 0;
}
```

---

### 示例 3: 文件备份

```paw
import fs;

fn backup_important_files() -> i32 {
    // 创建备份目录
    fs::create_dir_all("backups");
    
    // 备份文件
    if fs::exists("important.txt") {
        let backup_name = fs::join("backups", "important_backup.txt");
        fs::copy_file("important.txt", backup_name);
        println("Backup created");
    }
    
    return 0;
}
```

---

### 示例 4: 临时文件清理

```paw
import fs;

fn cleanup() -> i32 {
    if fs::is_dir("temp") {
        let size = fs::file_size("temp/cache.dat");
        
        if size > 1000000 {  // > 1MB
            fs::delete_dir_all("temp");
            println("Temp files cleaned");
        }
    }
    
    return 0;
}
```

---

## 🚧 当前限制

### ⚠️ 等待 FFI 集成

**状态**: 
- ✅ Zig 层完整实现（295 行）
- ✅ Paw 层 API 定义（200 行）
- ⏳ FFI 绑定未完成

**影响**:
- 所有函数当前返回占位符
- 无法实际进行文件 I/O
- 等待编译器 FFI 支持

**预计**: v0.3.0 可用

---

## 🔒 安全考虑

### 1. 路径验证

```paw
// TODO: 实现路径安全检查
fn is_safe_path(path: string) -> bool {
    // 检查路径遍历攻击 (../)
    // 验证在允许的目录内
    return true;
}
```

### 2. 错误处理

```paw
fn safe_read(path: string) -> string {
    if !fs::exists(path) {
        eprintln("Error: File not found");
        return "";
    }
    
    return fs::read_file(path);
}
```

---

## 📚 底层实现

### Zig 函数列表

**文件**: `src/builtin/fs.zig` (295 行)

```zig
export fn paw_read_file(path_ptr, path_len) i64
export fn paw_write_file(path_ptr, path_len, content_ptr, content_len) i32
export fn paw_append_file(path_ptr, path_len, content_ptr, content_len) i32
export fn paw_file_exists(path_ptr, path_len) i32
export fn paw_is_dir(path_ptr, path_len) i32
export fn paw_file_size(path_ptr, path_len) i64
export fn paw_delete_file(path_ptr, path_len) i32
export fn paw_rename_file(old_ptr, old_len, new_ptr, new_len) i32
export fn paw_create_dir(path_ptr, path_len) i32
export fn paw_create_dir_all(path_ptr, path_len) i32
export fn paw_delete_dir(path_ptr, path_len) i32
export fn paw_delete_dir_all(path_ptr, path_len) i32
```

**特性**:
- ✅ 跨平台（使用 Zig std.fs）
- ✅ 内存安全（GPA 分配器）
- ✅ 错误处理（统一返回模式）

---

## 🔮 未来增强

### v0.3.0

**流式 I/O**:
```paw
let file = fs::open("data.txt");
let line = fs::read_line(file);
fs::close(file);
```

**目录遍历**:
```paw
let entries = fs::read_dir(".");
for entry in entries {
    println(entry);
}
```

### v0.4.0

**文件监视**:
```paw
let watcher = fs::watch("config.json");
if watcher.has_changed() {
    reload_config();
}
```

**文件锁**:
```paw
let lock = fs::lock("data.db");
// ... 独占访问
fs::unlock(lock);
```

---

## ✅ 总结

### 当前状态

- ✅ API 设计完成
- ✅ Zig 实现完成
- ⏳ FFI 集成待完成

### 何时可用

- **v0.2.0**: API 定义（当前）
- **v0.3.0**: 完全可用（FFI 后）

---

**维护者**: PawLang 核心开发团队  
**最后更新**: 2025-10-12  
**版本**: v0.2.0  
**许可**: MIT

