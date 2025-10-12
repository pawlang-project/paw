# 🏗️ PawLang 分层设计原则

**日期**: 2025-10-12  
**版本**: v0.2.0-dev  

---

## 🎯 核心理念

> **尽可能用 PawLang 本身实现功能，只在必要时使用 Zig。**

这样可以：
1. ✅ 代码更透明（用户可以阅读和修改）
2. ✅ 更好的可移植性
3. ✅ 展示语言能力
4. ✅ 社区可以贡献标准库

---

## 📊 三层架构

```
┌─────────────────────────────────────────────┐
│         第 3 层：纯 PawLang 实现             │
│   (算法、数据结构、业务逻辑)                 │
│                                             │
│  • JSON 解析器                               │
│  • 字符串操作（基于数组）                     │
│  • 数学库                                    │
│  • HTTP 客户端                               │
└─────────────────────────────────────────────┘
                    ↓ 调用
┌─────────────────────────────────────────────┐
│    第 2 层：Paw + 最小 Zig 绑定              │
│   (需要少量系统资源的操作)                   │
│                                             │
│  • 固定缓冲区字符串构建                       │
│  • 栈分配的数据结构                          │
│  • 简单的类型转换                            │
└─────────────────────────────────────────────┘
                    ↓ 调用
┌─────────────────────────────────────────────┐
│       第 1 层：Zig 底层（系统层）            │
│   (必须的系统调用和内存管理)                 │
│                                             │
│  • malloc/free (内存分配)                   │
│  • open/read/write (文件 I/O)               │
│  • socket (网络)                            │
│  • syscall (系统调用)                       │
└─────────────────────────────────────────────┘
```

---

## 🔍 决策树：何时使用 Zig？

```
需要实现新功能
    ↓
能否用纯 Paw + 现有特性实现？
    ├─ 是 → 用 Paw 实现 ✅
    └─ 否 ↓
        需要动态内存分配？
            ├─ 是 ↓
            │   能否用固定缓冲区代替？
            │       ├─ 是 → 用 Paw + 固定数组 ✅
            │       └─ 否 → 用 Zig 包装 malloc ⚠️
            └─ 否 ↓
                需要系统调用？
                    ├─ 是 → 用 Zig 包装系统调用 ⚠️
                    └─ 否 → 重新思考设计 🤔
```

---

## 📝 实际案例

### ✅ 案例 1：字符串长度 - 纯 Paw

**为什么可以用 Paw？**
- 只需要遍历数组
- 不需要内存分配
- 不需要系统调用

```paw
// stdlib/string/mod.paw
pub fn length(s: string) -> i32 {
    let mut len: i32 = 0;
    let mut i: i32 = 0;
    
    loop i in 0..1000000 {
        if s[i] as i32 == 0 {
            break;
        }
        len += 1;
    }
    
    return len;
}
```

### ✅ 案例 2：JSON 解析器 - 纯 Paw

**为什么可以用 Paw？**
- 纯算法，无需系统调用
- 可以用固定缓冲区
- 输入输出都是已存在的字符串

```paw
// stdlib/json/mod.paw
pub fn parse(json_str: string) -> JsonValue {
    let mut parser = Parser::new(json_str);
    return parser.parse_value();
}

// 完全用 Paw 实现 Lexer 和 Parser
```

### ✅ 案例 3：字符串构建器 - Paw + 固定缓冲区

**为什么不需要 Zig？**
- 使用固定大小的数组
- 在栈上分配
- 不需要动态内存

```paw
// stdlib/string/mod.paw
pub type StringBuilder = struct {
    buffer: [char; 4096];  // 固定大小
    length: i32;
    
    pub fn append_string(mut self, s: string) -> bool {
        // 纯 Paw 实现
        let mut i = 0;
        loop i in 0..length(s) {
            if !self.append_char(s[i]) {
                return false;
            }
        }
        return true;
    }
}
```

### ⚠️ 案例 4：文件读取 - 必须用 Zig

**为什么必须用 Zig？**
- 需要调用系统的 `open()` 和 `read()`
- 需要动态分配缓冲区（文件大小未知）
- 涉及操作系统资源

```zig
// src/builtin_fs.zig
pub fn read_file(allocator: std.mem.Allocator, path: []const u8) ![]u8 {
    const file = try std.fs.cwd().openFile(path, .{});
    defer file.close();
    return try file.readToEndAlloc(allocator, 1024 * 1024);
}
```

### ⚠️ 案例 5：动态字符串拼接 - 需要 Zig

**为什么需要 Zig？**
- 结果长度未知
- 需要动态内存分配

```zig
// src/builtin_string.zig
pub fn concat(allocator: std.mem.Allocator, s1: []const u8, s2: []const u8) ![]u8 {
    var result = try allocator.alloc(u8, s1.len + s2.len);
    std.mem.copy(u8, result[0..s1.len], s1);
    std.mem.copy(u8, result[s1.len..], s2);
    return result;
}
```

---

## 🎨 标准库设计指南

### 字符串库分层

```
stdlib/string/mod.paw  (纯 Paw)
├── length(s)          ✅ 纯 Paw
├── char_at(s, i)      ✅ 纯 Paw
├── equals(s1, s2)     ✅ 纯 Paw
├── is_digit(ch)       ✅ 纯 Paw
├── parse_i32(s)       ✅ 纯 Paw
├── StringBuilder      ✅ Paw + 固定缓冲区
└── [将来] concat(s1, s2)  ⚠️  需要 Zig (动态内存)
```

### JSON 库分层

```
stdlib/json/mod.paw  (纯 Paw)
├── Token              ✅ 枚举定义
├── Lexer              ✅ 纯 Paw 实现
│   ├── next_token()   ✅ 字符串遍历
│   └── parse_number() ✅ 字符解析
├── Parser             ✅ 纯 Paw 实现
│   ├── parse_value()  ✅ 递归下降
│   └── parse_object() ✅ 对象解析
└── stringify()        ✅ 使用 StringBuilder
```

### 文件系统库分层

```
stdlib/fs/mod.paw  (Paw 包装)
└── 调用底层 Zig ↓

src/builtin_fs.zig  (Zig 实现)
├── read_file()      ⚠️  系统调用
├── write_file()     ⚠️  系统调用
├── exists()         ⚠️  系统调用
└── file_size()      ⚠️  系统调用
```

---

## 📈 演进路径

### v0.2.0 现状

```
第 3 层 (Paw):  ████░░░░░░ 40%  (基础类型、控制流)
第 2 层 (混合): ██░░░░░░░░ 20%  (部分字符串操作)
第 1 层 (Zig):  ██████████ 100% (系统调用完备)
```

### v0.3.0 目标

```
第 3 层 (Paw):  ████████░░ 80%  (完整标准库)
第 2 层 (混合): ██████░░░░ 60%  (常用工具)
第 1 层 (Zig):  ██████████ 100% (稳定接口)
```

### v1.0.0 愿景

```
第 3 层 (Paw):  ██████████ 100% (自举！)
第 2 层 (混合): ████████░░ 80%  (性能关键路径)
第 1 层 (Zig):  ██████████ 100% (最小必要)
```

---

## 💡 关键洞察

### 1. 固定缓冲区的威力

很多情况下，**固定大小的缓冲区**足够：

```paw
// 4KB 对大多数场景足够
type StringBuilder = struct {
    buffer: [char; 4096];  // 在栈上
    length: i32;
}

// 用于：
// - 构建 JSON 字符串
// - 拼接日志消息
// - 格式化输出
```

### 2. 渐进式增强

不需要一次实现所有功能：

```
v0.2.0: StringBuilder (固定缓冲区)  ← 现在这里
v0.2.1: 添加字符串字面量转义
v0.3.0: String (动态分配)
v0.4.0: 写时复制（COW）优化
```

### 3. 最小 Zig 接口

保持 Zig 层最小：

```zig
// 只暴露这些给 Paw
pub fn malloc(size: usize) -> *anyopaque;
pub fn free(ptr: *anyopaque) -> void;
pub fn file_open(path: []const u8) -> i32;
pub fn file_read(fd: i32, buf: []u8) -> isize;
// ... 仅此而已
```

---

## 🎯 实施建议

### 对于标准库开发者

1. **优先考虑 Paw 实现**
   - 先尝试用纯 Paw
   - 再考虑固定缓冲区
   - 最后才使用 Zig

2. **清晰标注依赖**
   ```paw
   // ✅ 纯 Paw 实现
   pub fn length(s: string) -> i32 { ... }
   
   // ⚠️  需要固定缓冲区
   pub type StringBuilder = struct { ... }
   
   // ❗ 需要 Zig (动态内存)
   // TODO: 等待动态内存支持
   // pub fn concat(s1: string, s2: string) -> string;
   ```

3. **提供多个选项**
   ```paw
   // 方案 A：固定缓冲区（现在可用）
   pub type StringBuilder = struct { ... }
   
   // 方案 B：动态字符串（将来）
   // pub type String = struct { ... }
   ```

---

## 🎉 总结

| 层次 | 实现语言 | 何时使用 | 比例目标 |
|------|---------|---------|----------|
| 第 3 层 | 纯 Paw | 算法、逻辑 | 80%+ |
| 第 2 层 | Paw + 固定资源 | 工具函数 | 15% |
| 第 1 层 | Zig | 系统调用 | <5% |

**目标**: 让用户能用 PawLang 实现 99% 的应用逻辑！

---

**构建者**: PawLang 开发团队  
**许可证**: MIT  

