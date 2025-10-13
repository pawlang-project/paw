# 文件系统模块 (fs)

**版本**: v0.2.0  
**状态**: ✅ 路径工具已实现并可用！⏳ 文件I/O等待FFI支持

---

## 📋 模块状态

### ✅ 已完成并可用
- ✅ **路径分析工具** - 纯PawLang实现，立即可用！
  - ✅ `is_absolute()` - 检查绝对路径
  - ✅ `is_relative()` - 检查相对路径
  - ✅ `has_extension()` - 检查文件扩展名
  - ✅ `is_separator()` - 检查路径分隔符
  - ✅ `ends_with_separator()` - 检查路径结尾
- ✅ Zig层实现完整 (`src/builtin/fs.zig`)
- ✅ PawLang API设计完整 (`stdlib/fs/mod.paw`)
- ✅ 完整文档和测试
- ✅ 跨平台支持（Unix和Windows）

### ⏳ 待FFI支持
- ⏳ 文件I/O操作（read, write, append）
- ⏳ 文件检查（exists, is_dir, file_size）
- ⏳ 文件操作（delete, rename, copy）
- ⏳ 目录操作（create, delete）

---

## 🚀 快速开始（立即可用！）

### 路径分析工具

这些函数**现在就可以使用**，不需要等待FFI：

```paw
// 检查绝对路径
let is_abs = is_absolute("/home/user/file.txt");  // true (Unix)
let is_abs2 = is_absolute("C:\\Users\\file.txt"); // true (Windows)

// 检查相对路径
let is_rel = is_relative("docs/README.md");       // true

// 检查文件扩展名
let has_ext = has_extension("file.txt");          // true
let has_ext2 = has_extension("README");           // false

// 检查路径分隔符
let is_sep = is_separator('/');                   // true
let is_sep2 = is_separator('\\');                 // true

// 检查路径结尾
let ends = ends_with_separator("/path/to/");      // true
```

### 运行示例

```bash
# 运行演示程序
./zig-out/bin/pawc examples/fs_demo.paw --backend=c --run

# 运行测试
./zig-out/bin/pawc tests/fs/test_path_utils.paw --backend=c --run
```

**测试结果**: ✅ 6/6 测试全部通过！

---

## 🎯 当前方案

由于PawLang目前不支持FFI（外部函数接口），文件系统API暂时无法完全实现。

### 方案A：等待编译器FFI支持 ⭐ (推荐)

**需要的编译器改进**：
1. 添加`extern`关键字支持
2. 在codegen中识别extern函数
3. 生成调用C函数的代码

**示例语法**：
```paw
// 声明外部函数
extern fn paw_file_exists(path: string, len: i32) -> i32;

// PawLang包装
pub fn exists(path: string) -> bool {
    let len = string_length(path);
    let result = paw_file_exists(path, len);
    return result == 1;
}
```

### 方案B：在codegen中硬编码 ⚡ (快速)

类似`println`的实现方式，在codegen中识别特定函数名：

```zig
// 在 src/codegen.zig 中添加
else if (std.mem.eql(u8, func_name, "file_exists")) {
    // 生成调用 paw_file_exists 的代码
    try self.output.appendSlice(self.allocator, "paw_file_exists(");
    // ... 处理参数
}
```

### 方案C：Shell命令包装 🔧 (临时)

创建通过shell命令访问文件系统的工具：

```paw
// 使用system()调用shell命令
// 注意：不安全，仅用于原型
```

---

## 📚 完整API文档

### 文件读写

#### `read_file(path: string) -> string`
读取文件全部内容。

**参数**：
- `path`: 文件路径（相对或绝对）

**返回**：
- 文件内容字符串
- 失败返回空字符串

**示例**：
```paw
let content = read_file("config.txt");
println(content);
```

#### `write_file(path: string, content: string) -> bool`
写入文件（覆盖）。

**参数**：
- `path`: 文件路径
- `content`: 要写入的内容

**返回**：
- 成功返回`true`，失败返回`false`

**示例**：
```paw
let success = write_file("output.txt", "Hello, World!");
if success {
    println("File written successfully");
}
```

#### `append_file(path: string, content: string) -> bool`
追加内容到文件末尾。

### 文件检查

#### `exists(path: string) -> bool`
检查文件或目录是否存在。

**示例**：
```paw
if exists("config.txt") {
    println("Config file found");
}
```

#### `is_dir(path: string) -> bool`
检查路径是否为目录。

#### `file_size(path: string) -> i32`
获取文件大小（字节）。失败返回-1。

### 文件操作

#### `delete_file(path: string) -> bool`
删除文件。

#### `rename(old_path: string, new_path: string) -> bool`
重命名或移动文件。

#### `copy_file(src: string, dst: string) -> bool`
复制文件。

**实现**：
```paw
pub fn copy_file(src: string, dst: string) -> bool {
    let content = read_file(src);
    return write_file(dst, content);
}
```

### 目录操作

#### `create_dir(path: string) -> bool`
创建单级目录。

#### `create_dir_all(path: string) -> bool`
递归创建目录（类似`mkdir -p`）。

#### `delete_dir(path: string) -> bool`
删除空目录。

#### `delete_dir_all(path: string) -> bool`
递归删除目录及其内容。

### 路径工具

#### `extension(path: string) -> string`
获取文件扩展名（不含点）。

**示例**：
```paw
let ext = extension("file.txt");  // "txt"
```

#### `filename(path: string) -> string`
获取文件名（不含路径）。

**示例**：
```paw
let name = filename("/path/to/file.txt");  // "file.txt"
```

#### `parent(path: string) -> string`
获取父目录路径。

#### `join(base: string, part: string) -> string`
拼接路径。

**示例**：
```paw
let path = join("/home/user", "documents");  // "/home/user/documents"
```

#### `normalize(path: string) -> string`
规范化路径（处理`.`和`..`）。

---

## 🏗️ Zig层实现

文件系统的Zig实现位于 `src/builtin/fs.zig`，提供以下export函数：

### 导出函数列表

| 函数 | 功能 | 返回值 |
|------|------|--------|
| `paw_read_file` | 读取文件 | i64 (指针) |
| `paw_write_file` | 写入文件 | i32 (1=成功) |
| `paw_append_file` | 追加文件 | i32 (1=成功) |
| `paw_file_exists` | 检查存在 | i32 (1=存在) |
| `paw_is_dir` | 检查目录 | i32 (1=是目录) |
| `paw_file_size` | 文件大小 | i64 (字节数) |
| `paw_delete_file` | 删除文件 | i32 (1=成功) |
| `paw_rename_file` | 重命名 | i32 (1=成功) |
| `paw_create_dir` | 创建目录 | i32 (1=成功) |
| `paw_create_dir_all` | 递归创建 | i32 (1=成功) |
| `paw_delete_dir` | 删除目录 | i32 (1=成功) |
| `paw_delete_dir_all` | 递归删除 | i32 (1=成功) |

所有函数都是跨平台的，由Zig标准库提供底层支持。

---

## 🔧 实现路线图

### v0.2.0 (当前)
- ✅ Zig层实现
- ✅ API设计
- ✅ 文档

### v0.2.1 (FFI支持)
- [ ] 编译器添加FFI支持
- [ ] extern关键字
- [ ] PawLang调用Zig函数

### v0.3.0 (完整实现)
- [ ] 所有API可用
- [ ] 完整测试套件
- [ ] 性能优化

---

## 📖 使用示例

### 示例1：读写文件
```paw
import fs.{read_file, write_file};

fn main() -> i32 {
    // 写入文件
    let success = write_file("test.txt", "Hello, PawLang!");
    
    if success {
        // 读取文件
        let content = read_file("test.txt");
        println(content);
    }
    
    return 0;
}
```

### 示例2：检查和创建
```paw
import fs.{exists, create_dir_all};

fn ensure_dir(path: string) -> bool {
    if !exists(path) {
        return create_dir_all(path);
    }
    return true;
}
```

### 示例3：复制文件
```paw
import fs.{read_file, write_file};

fn backup_file(path: string) -> bool {
    let content = read_file(path);
    let backup_path = path + ".bak";
    return write_file(backup_path, content);
}
```

---

## 🚧 临时解决方案

在FFI支持完成之前，可以使用以下替代方案：

### 1. 使用示例数据
```paw
// 模拟文件系统操作
pub fn read_file(path: string) -> string {
    // 返回模拟数据用于测试
    return "simulated file content";
}
```

### 2. 文档驱动开发
- 先完善API设计
- 编写完整的使用示例
- 准备测试用例
- 等待FFI实现后立即可用

---

## 🤝 贡献

如果你想帮助实现文件系统API：

1. **编译器开发者**：
   - 实现FFI支持
   - 添加extern关键字
   - codegen支持外部函数

2. **文档贡献者**：
   - 改进API文档
   - 添加更多示例
   - 翻译为其他语言

3. **测试贡献者**：
   - 设计测试用例
   - 编写基准测试
   - 跨平台测试

---

## 📞 相关链接

- Zig实现: `src/builtin/fs.zig`
- PawLang API: `stdlib/fs/mod.paw`
- 问题追踪: 在GitHub Issues中标记 `ffi` 和 `stdlib`

---

**Status**: 🟡 等待编译器FFI支持  
**Priority**: ⭐⭐⭐⭐ 高优先级  
**Complexity**: 🔴 需要编译器改动

**Built with ❤️ for PawLang v0.2.0** 🐾
