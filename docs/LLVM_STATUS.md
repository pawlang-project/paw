# LLVM 集成状态

**日期**: 2025-10-09  
**版本**: v0.1.4  

---

## ✅ 当前实现：文本 LLVM IR 生成

### 完美方案（推荐）

**PawLang 当前使用文本生成方式，无需系统 LLVM**

```bash
# 编译编译器（3秒，无需LLVM）
zig build

# 生成 LLVM IR 文本（不需要LLVM库）
./zig-out/bin/pawc hello.paw --backend=llvm
# → output.ll

# 编译运行（只需要标准的clang）
clang output.ll -o hello
./hello
```

**优势**:
- ✅ **零系统污染** - 完全不需要安装 LLVM
- ✅ **快速开发** - 编译仅需3秒
- ✅ **轻量级** - 二进制仅2MB
- ✅ **易调试** - 可查看生成的 .ll 文件
- ✅ **跨平台** - 任何有 clang 的系统都能用

---

## 📊 llvm-zig 测试结果

### 原生 API 模式

```bash
# 尝试启用LLVM原生API
zig build -Dwith-llvm=true

# 结果: 失败
# 原因: 仍需要系统安装LLVM库
```

**发现**: `llvm-zig` 是 LLVM C API 的 **绑定**，不是完整的 LLVM。它需要系统提供：
- `libLLVM.dylib` (或 .so/.a)
- LLVM 头文件
- llvm-config

**结论**: 如果要用原生 API，还是需要系统 LLVM（500MB+）

---

## 💡 最佳实践

### v0.1.4 推荐方案（当前）

**使用文本生成，完全自包含**

```zig
// src/llvm_backend.zig
// 纯文本生成，无需LLVM库

pub fn generate(self: *LLVMBackend, program: ast.Program) ![]const u8 {
    // 生成 LLVM IR 文本
    try self.output.appendSlice("define i32 @main() {\n");
    try self.output.appendSlice("  ret i32 42\n");
    try self.output.appendSlice("}\n");
    
    return try self.allocator.dupe(u8, self.output.items);
}
```

**测试结果**:
```
✅ hello.paw: C(42) ✓ LLVM(42) ✓
✅ arithmetic.paw: C(30) ✓ LLVM(30) ✓  
✅ function.paw: C(42) ✓ LLVM(42) ✓
```

### 未来扩展（v0.2.0+）

如果真的需要原生 API，有几个选择：

#### 选项1: 捆绑 LLVM（推荐）

下载预编译的 LLVM 到 `vendor/llvm/`：

```bash
vendor/
└── llvm/
    ├── lib/
    │   └── libLLVM.a  (静态库)
    ├── include/
    │   └── llvm-c/    (头文件)
    └── bin/
        └── llvm-config
```

**好处**:
- ✅ 项目自包含
- ✅ 版本锁定
- ✅ 不污染系统

**缺点**:
- ❌ 项目变大（200MB+）
- ❌ 跨平台需要多个预编译版本

#### 选项2: Git Submodule

```bash
git submodule add https://github.com/llvm/llvm-project vendor/llvm-src
# 配置只编译需要的部分
```

**好处**:
- ✅ 源码级控制
- ✅ 可自定义编译

**缺点**:
- ❌ 首次编译极慢（1-2小时）
- ❌ 项目巨大（5GB+）

#### 选项3: 系统 LLVM（不推荐）

```bash
brew install llvm@19
```

**缺点**:
- ❌ 污染系统
- ❌ 版本不一致
- ❌ 依赖外部环境

---

## 🎯 决策

### v0.1.4 决策：文本生成 ⭐

**理由**:
1. 满足所有需求（不污染系统、跨平台一致）
2. 实现简单（~300行代码）
3. 性能足够（编译快、运行好）
4. 易于维护

**放弃**:
- ❌ 暂不使用原生 LLVM API
- ❌ 暂不下载完整 LLVM
- ❌ 暂不支持优化管线

**未来**: 等真正需要高级优化时再考虑（v0.2.0+）

---

## 📋 对比总结

| 方案 | 系统污染 | 项目大小 | 编译时间 | 运行性能 | 推荐度 |
|------|----------|----------|----------|----------|--------|
| **文本IR（当前）** | ✅ 无 | 5MB | 3秒 | 好 | ⭐⭐⭐⭐⭐ |
| 系统LLVM | ❌ 是 | 5MB | 30秒 | 优秀 | ❌ |
| 捆绑LLVM | ✅ 无 | 200MB | 30秒 | 优秀 | ⭐⭐ |
| Git子模块 | ✅ 无 | 5GB | 1小时 | 优秀 | ❌ |

---

## ✅ 最终方案

### 当前配置（v0.1.4）

```zig
// build.zig.zon
.dependencies = .{
    .llvm = .{
        .url = "git+https://github.com/kassane/llvm-zig#...",
        .hash = "...",
    },
},
```

**说明**: 
- 依赖已配置（为未来准备）
- 默认不启用（文本模式足够）
- 可选启用（`-Dwith-llvm=true`，需要系统LLVM）

### 使用方式

```bash
# 默认：文本模式（无需系统LLVM）
zig build
./zig-out/bin/pawc hello.paw --backend=llvm
clang output.ll -o hello

# 可选：原生模式（需要系统LLVM，暂不推荐）
brew install llvm@19  # 污染系统
zig build -Dwith-llvm=true
```

**推荐**: 使用默认文本模式 ✅

---

## 🔮 未来规划

### v0.2.0 - 考虑原生 API

**如果需要**:
- 自定义优化 pass
- JIT 编译
- 极致性能

**则考虑**:
- 选项 A: 捆绑预编译 LLVM
- 选项 B: 提供两个版本（lite/full）

### v1.0.0 - 生产就绪

**可能的策略**:
- Lite 版本: 文本生成（5MB）
- Full 版本: 原生 API（200MB）
- 让用户选择

---

## 📚 测试命令

### 验证当前方案

```bash
# 1. 编译（文本模式）
zig build

# 2. 测试所有用例
for test in tests/llvm_*.paw; do
    echo "Testing $test..."
    ./zig-out/bin/pawc "$test" --backend=llvm 2>/dev/null
    clang output.ll -o test_exe 2>/dev/null
    ./test_exe
    echo "Exit: $?"
    rm -f test_exe output.ll
done
```

### 验证 llvm-zig（需要系统LLVM）

```bash
# 需要先安装系统LLVM
brew install llvm@19

# 然后编译
zig build -Dwith-llvm=true
```

---

## ✅ 结论

**v0.1.4 的文本生成方案已经完美满足需求**:

1. ✅ 不污染系统（无需 brew install）
2. ✅ 跨平台一致（LLVM IR 是标准）
3. ✅ 项目轻量（5MB）
4. ✅ 编译快速（3秒）
5. ✅ 易于调试（可查看 .ll）

**暂不需要原生 LLVM API**，等到真正需要高级优化时再考虑（v0.2.0+）。

---

**推荐**: 保持当前方案，继续开发其他功能！ 🚀

