# 📦 将 LLVM 集成到项目（Vendor 方案）

**目标**: 预下载 LLVM 到项目，不依赖系统环境

---

## 🎯 快速开始

### 方法：手动下载（推荐）

由于 GitHub releases 的直接下载需要浏览器重定向，建议手动下载：

#### 步骤 1: 访问 LLVM Releases

打开浏览器访问：
```
https://github.com/llvm/llvm-project/releases/tag/llvmorg-19.1.3
```

#### 步骤 2: 下载对应平台

**您的平台**: macOS ARM64 (Apple Silicon)

下载文件：
```
clang+llvm-19.1.3-arm64-apple-darwin22.0.tar.xz
大小: ~220MB
```

**其他平台**:
- macOS Intel: `clang+llvm-19.1.3-x86_64-apple-darwin.tar.xz`
- Linux: `clang+llvm-19.1.3-x86_64-linux-gnu-ubuntu-22.04.tar.xz`

#### 步骤 3: 解压到项目

```bash
cd /Users/haojunhuang/RustroverProjects/PawLang

# 创建 vendor 目录
mkdir -p vendor

# 解压下载的文件
tar xf ~/Downloads/clang+llvm-19.1.3-arm64-apple-darwin22.0.tar.xz -C vendor/

# 重命名为简单的 llvm
mv vendor/clang+llvm-* vendor/llvm

# 验证
ls vendor/llvm/
# 应该看到: bin/ include/ lib/ ...
```

#### 步骤 4: 配置 build.zig

```zig
// 已配置好，只需更新路径
if (b.option(bool, "with-llvm", "...") orelse false) {
    // ... llvm-zig module ...
    
    // 🆕 添加 vendor/llvm 路径
    exe.addLibraryPath(.{ .cwd_relative = "vendor/llvm/lib" });
    exe.addIncludePath(.{ .cwd_relative = "vendor/llvm/include" });
    exe.linkSystemLibrary("LLVM");
}
```

#### 步骤 5: 测试

```bash
# 编译（使用 vendor 中的 LLVM）
zig build -Dwith-llvm=true

# 如果成功，应该看到:
✓ LLVM native backend enabled (vendor)

# 测试
./zig-out/bin/pawc tests/llvm_hello.paw --backend=llvm-native
```

---

## 📊 对比方案

| 方案 | 系统污染 | 项目大小 | 优点 | 缺点 |
|------|----------|----------|------|------|
| **文本IR（当前）** | ✅ 无 | 5MB | 简单快速 | 无高级优化 |
| **Vendor LLVM** | ✅ 无 | 905MB | 完全自包含 | 项目变大 |
| **系统LLVM** | ❌ 是 | 5MB | 共享库 | 污染系统 |

---

## 💡 推荐

### 对于普通开发

**继续使用文本模式**（当前默认）
```bash
zig build
./zig-out/bin/pawc hello.paw --backend=llvm
clang output.ll -o hello
```

**理由**:
- ✅ 够用（基础功能完整）
- ✅ 快速（3秒编译）
- ✅ 轻量（5MB）

### 对于需要高级功能

**下载 LLVM 到 vendor**
```bash
# 下载一次
./scripts/download_llvm.sh

# 以后使用原生API
zig build -Dwith-llvm=true
```

**适用场景**:
- 需要 LLVM 优化管线
- 需要 JIT 编译
- 需要自定义 pass

---

## 🔗 相关文档

- `scripts/download_llvm.sh` - 自动下载脚本
- `docs/LLVM_STATUS.md` - LLVM 集成状态
- `docs/LLVM_INTEGRATION.md` - 集成策略
- `docs/LLVM_SETUP.md` - 设置指南

---

## ✅ 结论

**v0.1.4 当前方案已经很好**:
- 文本 IR 生成完美工作
- 无需下载 900MB 的 LLVM
- 性能足够好

**如果未来需要**: 可以随时下载 LLVM 到 vendor，配置已准备好。

---

**下一步**: 继续开发其他功能（控制流、结构体等），暂不需要原生 LLVM API。

