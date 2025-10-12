# 📁 PawLang 文件系统 API 文档

**版本**: v0.2.0  
**状态**: ✅ 已实现（Zig 层）  
**待完成**: FFI 集成

---

## 📋 概述

PawLang 提供了完整的跨平台文件系统 API，包括：

- ✅ 文件读写
- ✅ 文件检查
- ✅ 目录操作
- ✅ 路径处理

### 架构

```
┌─────────────────────────────────┐
│   Paw Layer (stdlib/fs/mod.paw) │  ← 用户 API
│   - 高级封装                     │
│   - 类型安全                     │
│   - 错误处理                     │
└─────────────────────────────────┘
           ↓ (FFI Call)
┌─────────────────────────────────┐
│   Zig Layer (src/builtin_fs.zig)│  ← 系统调用
│   - 跨平台实现                   │
│   - 内存管理                     │
│   - 错误转换                     │
└─────────────────────────────────┘
           ↓ (std.fs)
┌─────────────────────────────────┐
│   Operating System               │
│   - Windows / Linux / macOS      │
└─────────────────────────────────┘
```

---

## 🔧 API 参考

### 文件读写

#### `read_file`

读取文件全部内容。

```paw
pub fn read_file(path: string) -> string
```

**参数**:
- `path`: 文件路径

**返回**:
- 文件内容字符串

**示例**:
```paw
let content: string = read_file("data.txt");
println(content);
```

**Zig 实现**:
```zig
export fn paw_read_file(path_ptr: [*]const u8, path_len: usize) i64 {
    // 打开文件
    const file = std.fs.cwd().openFile(path_slice, .{}) catch {
        return 0;
    };
    defer file.close();
    
    // 读取内容到缓冲区
    const buffer = allocator.alloc(u8, file_size) catch {
        return 0;
    };
    
    // 返回指针
    return @intCast(@intFromPtr(buffer.ptr));
}
```

---

#### `write_file`

写入文件（覆盖模式）。

```paw
pub fn write_file(path: string, content: string) -> bool
```

**参数**:
- `path`: 文件路径
- `content`: 文件内容

**返回**:
- 成功返回 `true`

**示例**:
```paw
let success: bool = write_file("output.txt", "Hello, PawLang!");
if success {
    println("文件写入成功");
}
```

**Zig 实现**:
```zig
export fn paw_write_file(
    path_ptr: [*]const u8,
    path_len: usize,
    content_ptr: [*]const u8,
    content_len: usize,
) i32 {
    const file = std.fs.cwd().createFile(path_slice, .{}) catch {
        return 0;
    };
    defer file.close();
    
    file.writeAll(content_slice) catch {
        return 0;
    };
    
    return 1;
}
```

---

#### `append_file`

追加到文件末尾。

```paw
pub fn append_file(path: string, content: string) -> bool
```

**参数**:
- `path`: 文件路径
- `content`: 要追加的内容

**返回**:
- 成功返回 `true`

**示例**:
```paw
append_file("log.txt", "New log entry\n");
```

---

### 文件检查

#### `exists`

检查文件或目录是否存在。

```paw
pub fn exists(path: string) -> bool
```

**示例**:
```paw
if exists("config.json") {
    let config: string = read_file("config.json");
} else {
    println("配置文件不存在");
}
```

---

#### `is_dir`

检查路径是否为目录。

```paw
pub fn is_dir(path: string) -> bool
```

**示例**:
```paw
if is_dir("data") {
    println("这是一个目录");
}
```

---

#### `file_size`

获取文件大小（字节）。

```paw
pub fn file_size(path: string) -> i32
```

**返回**:
- 文件大小（字节），失败返回 `-1`

**示例**:
```paw
let size: i32 = file_size("data.bin");
println("文件大小: ${size} 字节");
```

---

### 文件操作

#### `delete_file`

删除文件。

```paw
pub fn delete_file(path: string) -> bool
```

**示例**:
```paw
if delete_file("temp.txt") {
    println("临时文件已删除");
}
```

---

#### `rename`

重命名或移动文件。

```paw
pub fn rename(old_path: string, new_path: string) -> bool
```

**示例**:
```paw
// 重命名
rename("old.txt", "new.txt");

// 移动到另一个目录
rename("file.txt", "backup/file.txt");
```

---

#### `copy_file`

复制文件。

```paw
pub fn copy_file(src: string, dst: string) -> bool
```

**示例**:
```paw
copy_file("original.txt", "copy.txt");
```

**实现**:
```paw
pub fn copy_file(src: string, dst: string) -> bool {
    let content: string = read_file(src);
    return write_file(dst, content);
}
```

---

### 目录操作

#### `create_dir`

创建目录。

```paw
pub fn create_dir(path: string) -> bool
```

**示例**:
```paw
create_dir("output");
```

---

#### `create_dir_all`

递归创建目录（包括父目录）。

```paw
pub fn create_dir_all(path: string) -> bool
```

**示例**:
```paw
create_dir_all("data/images/thumbnails");
```

---

#### `delete_dir`

删除空目录。

```paw
pub fn delete_dir(path: string) -> bool
```

**示例**:
```paw
delete_dir("empty_dir");
```

---

#### `delete_dir_all`

递归删除目录及其所有内容。

```paw
pub fn delete_dir_all(path: string) -> bool
```

**⚠️ 警告**: 此操作不可恢复！

**示例**:
```paw
// 删除整个目录树
delete_dir_all("old_project");
```

---

### 路径工具

#### `extension`

获取文件扩展名。

```paw
pub fn extension(path: string) -> string
```

**示例**:
```paw
let ext: string = extension("document.pdf");  // "pdf"
```

---

#### `filename`

获取文件名（不含路径）。

```paw
pub fn filename(path: string) -> string
```

**示例**:
```paw
let name: string = filename("/path/to/file.txt");  // "file.txt"
```

---

#### `parent`

获取父目录路径。

```paw
pub fn parent(path: string) -> string
```

**示例**:
```paw
let dir: string = parent("/path/to/file.txt");  // "/path/to"
```

---

#### `join`

拼接路径。

```paw
pub fn join(base: string, part: string) -> string
```

**示例**:
```paw
let path: string = join("data", "config.json");  // "data/config.json"
```

---

## 💡 使用示例

### 示例 1: 配置文件管理

```paw
fn load_config() -> string {
    let config_path: string = "config.json";
    
    if exists(config_path) {
        return read_file(config_path);
    } else {
        // 创建默认配置
        let default_config: string = "{\"version\": \"1.0\"}";
        write_file(config_path, default_config);
        return default_config;
    }
}

fn main() -> i32 {
    let config: string = load_config();
    println(config);
    return 0;
}
```

---

### 示例 2: 日志系统

```paw
fn log(message: string) -> bool {
    let log_file: string = "app.log";
    
    // 追加日志条目
    return append_file(log_file, message);
}

fn main() -> i32 {
    log("应用启动\n");
    log("执行操作 1\n");
    log("应用关闭\n");
    
    return 0;
}
```

---

### 示例 3: 备份工具

```paw
fn backup_file(source: string) -> bool {
    if !exists(source) {
        return false;
    }
    
    // 创建备份目录
    create_dir_all("backups");
    
    // 生成备份文件名
    let backup_name: string = join("backups", filename(source));
    
    // 复制文件
    return copy_file(source, backup_name);
}

fn main() -> i32 {
    let success: bool = backup_file("important.txt");
    
    if success {
        println("备份成功");
        return 0;
    } else {
        println("备份失败");
        return 1;
    }
}
```

---

### 示例 4: 临时文件清理

```paw
fn cleanup_temp_files() -> i32 {
    let temp_dir: string = "temp";
    
    if is_dir(temp_dir) {
        // 删除整个临时目录
        if delete_dir_all(temp_dir) {
            println("临时文件已清理");
            return 1;
        }
    }
    
    return 0;
}

fn main() -> i32 {
    return cleanup_temp_files();
}
```

---

## 🔒 安全考虑

### 1. 路径验证

```paw
fn is_safe_path(path: string) -> bool {
    // TODO: 实现路径验证
    // - 检查路径遍历攻击 (../)
    // - 验证路径在允许的目录内
    // - 检查符号链接
    return true;
}

fn safe_read_file(path: string) -> string {
    if !is_safe_path(path) {
        return "";
    }
    
    return read_file(path);
}
```

---

### 2. 错误处理

```paw
fn safe_write(path: string, content: string) -> i32 {
    if !write_file(path, content) {
        println("错误：无法写入文件");
        return -1;
    }
    
    return 0;
}
```

---

### 3. 权限检查

在实际使用中，应该：
- ✅ 检查文件权限
- ✅ 验证用户输入
- ✅ 使用绝对路径
- ✅ 记录文件操作日志

---

## 🚀 性能考虑

### 大文件处理

对于大文件，考虑：

1. **分块读取**（未来版本）:
```paw
// 未来 API
fn read_file_chunked(path: string, chunk_size: i32) -> Vec<string>
```

2. **流式处理**（未来版本）:
```paw
// 未来 API
fn open_file(path: string) -> FileHandle
fn read_line(handle: FileHandle) -> string
fn close_file(handle: FileHandle) -> bool
```

---

## 📊 实现状态

| 功能 | Zig 层 | Paw 层 | FFI | 测试 |
|------|--------|--------|-----|------|
| read_file | ✅ | ✅ | ⏳ | ⏳ |
| write_file | ✅ | ✅ | ⏳ | ⏳ |
| append_file | ✅ | ✅ | ⏳ | ⏳ |
| exists | ✅ | ✅ | ⏳ | ⏳ |
| is_dir | ✅ | ✅ | ⏳ | ⏳ |
| file_size | ✅ | ✅ | ⏳ | ⏳ |
| delete_file | ✅ | ✅ | ⏳ | ⏳ |
| rename | ✅ | ✅ | ⏳ | ⏳ |
| copy_file | ✅ | ✅ | ⏳ | ⏳ |
| create_dir | ✅ | ✅ | ⏳ | ⏳ |
| create_dir_all | ✅ | ✅ | ⏳ | ⏳ |
| delete_dir | ✅ | ✅ | ⏳ | ⏳ |
| delete_dir_all | ✅ | ✅ | ⏳ | ⏳ |

**说明**:
- ✅ Zig 层: 完整实现
- ✅ Paw 层: API 定义完成
- ⏳ FFI: 等待编译器 FFI 支持
- ⏳ 测试: 等待 FFI 集成后测试

---

## 🔜 未来增强

### v0.3.0 计划

1. **流式 I/O**
   - 文件流
   - 缓冲读写
   - 逐行读取

2. **高级功能**
   - 文件监视
   - 文件锁
   - 内存映射文件

3. **路径库增强**
   - 路径规范化
   - 相对路径转换
   - 路径匹配（glob）

4. **目录遍历**
   - 递归列出文件
   - 过滤器支持
   - 符号链接处理

---

## 📚 参考资料

### Zig 标准库

- [std.fs](https://ziglang.org/documentation/master/std/#A;std:fs)
- [std.fs.File](https://ziglang.org/documentation/master/std/#A;std:fs.File)
- [std.fs.Dir](https://ziglang.org/documentation/master/std/#A;std:fs.Dir)

### 相关文档

- `docs/LAYERED_DESIGN.md` - 分层架构
- `src/builtin_fs.zig` - Zig 实现
- `stdlib/fs/mod.paw` - Paw API

---

**作者**: PawLang 核心开发团队  
**日期**: 2025-10-12  
**版本**: v0.2.0  
**许可**: MIT

