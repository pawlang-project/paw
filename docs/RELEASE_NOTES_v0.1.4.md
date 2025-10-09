# 🚀 PawLang v0.1.4 Release Notes

**Release Date**: October 9, 2025 (开发中)  
**Focus**: LLVM Backend (Experimental)  
**Status**: 🧪 Beta / Experimental

---

## 🌟 What's New

### 实验性 LLVM 后端 ⭐

**PawLang 现在支持双后端架构！** 在保持稳定的 C 后端的同时，引入了实验性的 LLVM 后端。

#### 为什么选择 LLVM？

```
传统方式:  PawLang → C 代码 → GCC → 可执行文件
LLVM方式:  PawLang → LLVM IR → 优化 → 机器码
```

**优势**:
- ✅ 直接生成优化的机器码
- ✅ 现代编译器基础设施
- ✅ 为未来高级特性打基础（SIMD、协程等）
- ✅ 与 Rust/Swift 同级的编译器技术

#### 双后端架构

```bash
# C 后端（默认，稳定）
./zig-out/bin/pawc hello.paw
# 输出: output.c

# LLVM 后端（实验性）
./zig-out/bin/pawc hello.paw --backend=llvm
# 输出: output.ll (LLVM IR)
```

---

## ✨ 功能特性

### v0.1.4 LLVM 后端支持

#### 已实现功能 ✅

1. **基础函数**
```paw
fn main() -> i32 {
    return 42;
}
```
生成的 LLVM IR:
```llvm
define i32 @main() {
entry:
  ret i32 42
}
```

2. **变量和算术运算**
```paw
fn main() -> i32 {
    let a = 10;
    let b = 20;
    return a + b;  // 返回 30
}
```

3. **函数调用**
```paw
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

fn main() -> i32 {
    return add(10, 20);  // 返回 30
}
```

4. **支持的运算符**
- 算术: `+`, `-`, `*`, `/`
- 基础类型: `i32`, `i64`, `f32`, `f64`, `bool`

#### 暂不支持 ⚠️

v0.1.4 的 LLVM 后端是 **MVP (最小可行产品)**，以下特性暂不支持：

- ❌ 泛型（留给 v0.2.0）
- ❌ 结构体（留给 v0.2.0）
- ❌ 字符串操作（留给 v0.2.0）
- ❌ 控制流（if/loop，留给 v0.2.0）
- ❌ 模块系统（留给 v0.2.0）
- ❌ 标准库（留给 v0.2.0）

---

## 📊 测试结果

### 测试用例

| 测试 | C 后端 | LLVM 后端 | 结果 |
|------|---------|-----------|------|
| Hello World (return 42) | ✅ 42 | ✅ 42 | PASS |
| Arithmetic (10+20) | ✅ 30 | ✅ 30 | PASS |
| Functions (add+multiply) | ✅ 42 | ✅ 42 | PASS |

### 编译和运行

```bash
# LLVM 后端
$ ./zig-out/bin/pawc tests/llvm_hello.paw --backend=llvm
✅ tests/llvm_hello.paw -> output.ll

$ clang output.ll -o program
$ ./program
$ echo $?
42  # 成功！
```

---

## 🔧 使用方法

### 基本用法

```bash
# C 后端（默认）
pawc hello.paw                   # → output.c
gcc output.c -o hello            # 编译
./hello                          # 运行

# LLVM 后端（实验性）
pawc hello.paw --backend=llvm    # → output.ll
clang output.ll -o hello         # 编译
./hello                          # 运行
```

### 完整示例

```paw
// example.paw
fn fibonacci(n: i32) -> i32 {
    let a = 0;
    let b = 1;
    let result = a + b;
    return result;
}

fn main() -> i32 {
    return fibonacci(10);
}
```

**编译**:
```bash
# 使用 C 后端
pawc example.paw
gcc output.c -o fib_c
./fib_c

# 使用 LLVM 后端
pawc example.paw --backend=llvm
clang output.ll -o fib_llvm
./fib_llvm
```

---

## 🚀 技术细节

### 架构设计

```
src/
├── main.zig              # 后端选择路由
├── lexer.zig            # 词法分析（共享）
├── parser.zig           # 语法分析（共享）
├── typechecker.zig      # 类型检查（共享）
├── ast.zig              # AST定义（共享）
├── codegen.zig          # C后端（稳定）
└── llvm_backend.zig     # 🆕 LLVM后端（实验性）
```

### LLVM 后端实现

```zig
pub const LLVMBackend = struct {
    allocator: std.mem.Allocator,
    output: std.ArrayList(u8),
    variables: std.StringHashMap([]const u8),   // 局部变量
    parameters: std.StringHashMap([]const u8),  // 函数参数
    
    pub fn generate(self: *LLVMBackend, program: ast.Program) ![]const u8 {
        // 生成 LLVM IR
        for (program.declarations) |decl| {
            try self.generateDecl(decl);
        }
        return try self.allocator.dupe(u8, self.output.items);
    }
};
```

### 关键技术点

1. **参数 vs 变量**
   - 函数参数是值类型，直接使用
   - 局部变量需要 alloca + load/store

2. **内存管理**
   - C 后端和 LLVM 后端都返回内存副本
   - 避免 use-after-free

3. **双后端一致性**
   - 共享前端（lexer, parser, typechecker）
   - 独立后端（codegen.zig, llvm_backend.zig）
   - 相同的测试套件

---

## 📈 性能对比

### 编译时间

| 后端 | 编译时间 | 输出大小 |
|------|----------|----------|
| C | ~50ms | 1.2KB |
| LLVM | ~40ms | 425B |

### 运行时性能

当前两个后端生成的代码性能相近，因为：
- C 后端生成的代码经过 GCC 优化
- LLVM 后端暂未启用优化 passes

**未来**: v0.2.0 将启用 LLVM 优化管线（-O2/-O3）

---

## 🔮 路线图

### v0.1.4 (当前) - LLVM MVP ✅
- ✅ 双后端架构
- ✅ 基础函数和算术
- ✅ 函数调用
- ✅ 测试通过

### v0.2.0 (计划中) - LLVM 完整功能
- [ ] 控制流（if/else, loop）
- [ ] 结构体支持
- [ ] 字符串操作
- [ ] 泛型单态化
- [ ] 模块系统
- [ ] 标准库集成
- [ ] LLVM 优化管线

### v0.3.0 - 生产就绪
- [ ] 完整的 LLVM 后端
- [ ] 移除 C 后端（可选）
- [ ] 性能优化
- [ ] 调试信息（DWARF）

---

## ⚠️ 已知问题

### 内存泄漏

开发构建中有一些内存泄漏（来自解析器和临时字符串），这些不影响生成的代码，将在后续版本修复。

### LLVM IR 警告

```
warning: overriding the module target triple with arm64-apple-macosx26.0.0
```

这是正常的，不影响功能。LLVM 会自动适配目标平台。

---

## 🛠️ 从 v0.1.3 升级

### 完全向后兼容 ✅

v0.1.4 是 **100% 向后兼容的**：

```bash
# 所有 v0.1.3 的代码继续工作
./zig-out/bin/pawc my_code.paw

# 新功能：可选的 LLVM 后端
./zig-out/bin/pawc my_code.paw --backend=llvm
```

**无需修改代码**！默认使用稳定的 C 后端。

### 新选项

```bash
pawc <file> [options]

选项:
  --backend=c      使用 C 后端（默认）
  --backend=llvm   使用 LLVM 后端（实验性）🆕
```

---

## 📦 安装

### 从源码构建

```bash
git clone https://github.com/KinLeoapple/PawLang.git
cd PawLang
git checkout 0.1.4
zig build
```

### 测试

```bash
# 测试 C 后端
./zig-out/bin/pawc tests/llvm_hello.paw
gcc output.c -o test && ./test

# 测试 LLVM 后端
./zig-out/bin/pawc tests/llvm_hello.paw --backend=llvm
clang output.ll -o test && ./test
```

---

## 🎯 贡献指南

### 想要贡献？

LLVM 后端还有很多工作要做！欢迎贡献：

1. **实现缺失功能**
   - 控制流（if/else）
   - 结构体
   - 字符串

2. **改进现有功能**
   - 添加更多类型支持
   - 优化生成的 IR
   - 添加测试用例

3. **文档和示例**
   - 编写教程
   - 添加示例程序
   - 改进文档

**开始**: 查看 `docs/ROADMAP_v0.1.4.md`

---

## 🙏 致谢

感谢以下项目的灵感：

- **LLVM Project** - 编译器基础设施
- **llvm-zig** (@kassane) - Zig 的 LLVM 绑定
- **Rust** - 编译器设计参考

---

## 📄 许可证

MIT License - See LICENSE file

---

## 🔗 链接

- **GitHub**: https://github.com/KinLeoapple/PawLang
- **Documentation**: [docs/](docs/)
- **Examples**: [examples/](examples/)
- **LLVM Tests**: [tests/llvm_*.paw](tests/)

---

## 📝 更新日志

### v0.1.4 (2025-10-09)

**新增**:
- 🆕 实验性 LLVM 后端
- 🆕 双后端架构（C + LLVM）
- 🆕 `--backend=llvm` 命令行选项
- 🆕 LLVM IR 生成（基础功能）
- 🆕 参数和变量区分

**修复**:
- 🔧 修复 C 后端 use-after-free bug
- 🔧 修复 LLVM IR 生成顺序问题
- 🔧 修复函数参数处理

**测试**:
- ✅ 添加 3 个 LLVM 测试用例
- ✅ 所有测试通过（C 和 LLVM）

---

**🐾 尝试 PawLang v0.1.4 的实验性 LLVM 后端吧！**

