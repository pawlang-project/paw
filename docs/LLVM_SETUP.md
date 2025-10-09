# LLVM 集成设置指南

**版本**: v0.1.4  
**更新**: 2025-10-09

---

## 🎯 概述

PawLang 现在支持两种 LLVM 后端模式：

1. **文本模式**（默认）- 生成 LLVM IR 文本，无需 LLVM 库
2. **原生模式**（实验性）- 直接调用 LLVM API，需要 LLVM 库

---

## 📦 项目已集成 LLVM

### 自动下载

LLVM 依赖已经配置在 `build.zig.zon` 中：

```zig
.dependencies = .{
    .llvm = .{
        .url = "git+https://github.com/kassane/llvm-zig#...",
        .hash = "...",
    },
},
```

**Zig 会自动下载到**: `~/.cache/zig/p/`

### 优势

✅ **不污染系统** - 只下载到 Zig 缓存  
✅ **版本锁定** - 使用特定提交的 LLVM  
✅ **跨平台** - llvm-zig 自动处理平台差异  
✅ **按需启用** - 默认不使用，按需编译

---

## 🚀 使用方法

### 模式 1: 文本模式（默认）

**不需要 LLVM 库，推荐日常使用**

```bash
# 编译（不启用LLVM库）
zig build

# 生成 LLVM IR 文本
./zig-out/bin/pawc hello.paw --backend=llvm
# → output.ll

# 用 clang 编译（只需要系统的 clang）
clang output.ll -o hello
./hello
```

**特点**:
- ✅ 编译快（~3秒）
- ✅ 二进制小（~2MB）
- ✅ 无需下载 LLVM
- ✅ 适合开发和测试

### 模式 2: 原生模式（实验性）

**启用 LLVM 库，用于性能敏感场景**

```bash
# 编译并启用LLVM原生后端
zig build -Dwith-llvm=true

# 这会：
# 1. 下载 llvm-zig (~50MB)
# 2. 链接 LLVM 库
# 3. 编译时间增加到 ~30秒
```

**特点**:
- ⚡ 运行时优化更好
- 📊 支持 LLVM 优化管线
- 🔧 可以生成机器码
- ⚠️  编译慢，二进制大

---

## 📂 文件位置

### 下载位置

```bash
~/.cache/zig/p/llvm_zig-1.0.0-<hash>/
├── src/
│   ├── llvm-bindings.zig  # LLVM C API 绑定
│   └── ...
└── build.zig
```

### 项目配置

```
PawLang/
├── build.zig         # 构建配置（可选LLVM）
├── build.zig.zon     # 依赖声明（LLVM已配置）
├── vendor/           # 本地vendor目录（可选）
│   └── llvm/         # 如果需要本地LLVM
└── src/
    ├── llvm_backend.zig      # 文本IR生成（默认）
    └── llvm_native.zig       # 原生API（未来）
```

---

## 🔧 高级配置

### 1. 查看LLVM版本

```bash
# 查看llvm-zig使用的LLVM版本
cat ~/.cache/zig/p/llvm_zig-*/build.zig | grep LLVM_VERSION
```

### 2. 清理缓存

```bash
# 清理所有Zig缓存（包括LLVM）
rm -rf ~/.cache/zig/p/
rm -rf .zig-cache/

# 重新下载
zig build -Dwith-llvm=true
```

### 3. 使用本地LLVM（可选）

如果你有本地 LLVM，可以配置：

```bash
# 下载并解压LLVM到vendor/
mkdir -p vendor/llvm
cd vendor/llvm
# 下载你的LLVM版本...

# 修改build.zig指向本地
```

---

## 📊 对比

| 特性 | 文本模式 | 原生模式 |
|------|----------|----------|
| LLVM库 | 不需要 | 需要 |
| 编译时间 | ~3秒 | ~30秒 |
| 二进制大小 | ~2MB | ~20MB |
| 运行性能 | 好 | 更好 |
| 优化选项 | 依赖clang | 完全控制 |
| 调试友好 | ✅ | ⚠️ |
| 推荐用途 | 开发/测试 | 生产 |

---

## 🎓 示例

### 基础使用（文本模式）

```bash
# 1. 编译编译器
zig build

# 2. 编译你的程序
./zig-out/bin/pawc hello.paw --backend=llvm

# 3. 查看生成的IR
cat output.ll

# 4. 编译成可执行文件
clang output.ll -o hello

# 5. 运行
./hello
```

### 高级使用（原生模式 - 未来）

```bash
# 1. 启用LLVM编译编译器
zig build -Dwith-llvm=true

# 2. 使用原生后端
./zig-out/bin/pawc hello.paw --backend=llvm-native -O3

# 3. 直接生成优化的机器码
./hello
```

---

## ⚠️ 常见问题

### Q: 为什么默认不启用LLVM？

**A**: 文本模式已经足够好，而且：
- 编译快
- 不需要大文件下载
- 调试友好
- 跨平台兼容性好

### Q: 什么时候需要原生模式？

**A**: 当你需要：
- 极致性能优化
- 自定义优化管线
- JIT编译
- 不依赖外部编译器（clang）

### Q: LLVM 会占用多少空间？

**A**: 
- llvm-zig: ~50MB（下载一次，全局缓存）
- 编译后的pawc: +18MB（如果启用）

### Q: 如何卸载？

**A**:
```bash
# 删除缓存的LLVM
rm -rf ~/.cache/zig/p/llvm_zig-*

# 恢复到文本模式
zig build  # 不加 -Dwith-llvm
```

---

## 🔮 未来计划

### v0.1.4 (当前)
- ✅ 文本LLVM IR生成
- ✅ llvm-zig集成（可选）
- ⏳ 原生API（实验中）

### v0.2.0
- 🎯 完整原生LLVM后端
- 🎯 优化管线
- 🎯 双模式并存

### v0.3.0
- 🎯 默认使用原生模式
- 🎯 JIT编译支持
- 🎯 预编译LLVM二进制

---

## 📚 参考

- [llvm-zig](https://github.com/kassane/llvm-zig) - Zig的LLVM绑定
- [LLVM官方文档](https://llvm.org/docs/)
- [Zig构建系统](https://ziglang.org/learn/build-system/)

---

## ✅ 推荐方案

**开发阶段**: 使用文本模式（默认）
```bash
zig build
```

**生产部署**: 考虑原生模式（未来）
```bash
zig build -Dwith-llvm=true -Doptimize=ReleaseFast
```

**结论**: 当前项目已经配置好LLVM，按需启用，不污染系统 ✅

