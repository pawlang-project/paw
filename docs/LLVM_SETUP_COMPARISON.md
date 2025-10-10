# LLVM 设置方式对比

本文档对比两种 LLVM 设置方式：预编译版本 vs 源码编译。

## 📊 快速对比

| 特性 | 预编译版本 ⭐ | 源码编译 |
|------|------------|---------|
| **安装时间** | 5-10 分钟 | 30-60 分钟 |
| **下载大小** | ~300 MB | ~200 MB (源码压缩) |
| **磁盘占用** | ~500 MB | ~12 GB (源码 2GB + 构建 6GB + 安装 2GB) |
| **依赖要求** | Python 3.6+ | Python, CMake, Ninja, C++ 编译器 |
| **难度** | ⭐ 简单 | ⭐⭐⭐⭐ 困难 |
| **自定义选项** | ❌ 固定配置 | ✅ 完全自定义 |
| **版本** | LLVM 19.1.7 | LLVM 19.1.7 |
| **适用人群** | 大多数用户 | 需要自定义构建的高级用户 |

## 🚀 预编译版本（推荐）

### 优势

✅ **快速安装**
- 5-10 分钟即可完成（取决于网速）
- 无需等待长时间编译

✅ **节省空间**
- 仅需 ~500 MB 磁盘空间
- 不需要保留源码和构建产物

✅ **简单易用**
- 只需 Python 3.6+
- 一条命令完成安装
- 自动检测平台

✅ **即开即用**
- 下载后立即可用
- 无需配置

### 使用方法

```bash
# 一键下载和安装
python3 scripts/download_llvm_prebuilt.py

# 或使用统一脚本
python3 scripts/setup_llvm.py  # 默认使用预编译版本
```

### 支持的平台

- ✅ macOS x86_64 (Intel)
- ✅ macOS arm64 (Apple Silicon M1/M2/M3)
- ✅ Linux x86_64
- ✅ Linux aarch64 (ARM64)
- ✅ Windows x86_64

### 来源

预编译版本来自：https://github.com/pawlang-project/llvm-build/releases/tag/19.1.7

这是 PawLang 官方维护的 LLVM 预编译仓库，基于 https://github.com/terralang/llvm-build

### 适用场景

- ✅ 日常开发
- ✅ 学习和测试
- ✅ CI/CD 环境
- ✅ 快速原型开发
- ✅ 不需要自定义 LLVM 配置

## 🔨 源码编译

### 优势

✅ **完全控制**
- 可以自定义编译选项
- 可以选择启用/禁用特定组件
- 可以针对特定 CPU 优化

✅ **最新版本**
- 可以使用任意 LLVM 版本
- 可以使用开发分支

✅ **学习价值**
- 理解 LLVM 构建过程
- 适合深入学习 LLVM

### 使用方法

```bash
# 1. 下载源码
python3 scripts/setup_llvm.py --method=git

# 2. 编译（30-60 分钟）
python3 scripts/build_llvm.py
```

### 依赖要求

- Python 3.6+
- CMake 3.13+
- Ninja 构建系统
- C++ 编译器 (GCC 7+, Clang 5+, or MSVC 2019+)
- ~8GB RAM（推荐 16GB）
- ~10GB 磁盘空间

### 适用场景

- ✅ 需要自定义 LLVM 配置
- ✅ 需要特定版本的 LLVM
- ✅ 平台不支持预编译版本
- ✅ 学习 LLVM 构建过程
- ✅ 需要针对特定硬件优化

## 🎯 推荐选择

### 大多数用户：预编译版本 ⭐

如果你只是想使用 PawLang 的 LLVM 后端，**强烈推荐使用预编译版本**：

```bash
python3 scripts/download_llvm_prebuilt.py
```

**原因**：
- 节省 2-3 小时编译时间
- 节省 10GB+ 磁盘空间
- 更简单，更快速
- 预编译版本已经过测试和优化

### 高级用户：源码编译

只有在以下情况下才建议源码编译：
- 需要自定义 LLVM 构建配置
- 需要特定版本的 LLVM
- 你的平台不支持预编译版本
- 你想深入学习 LLVM 构建过程

## 📋 详细步骤对比

### 预编译版本安装步骤

```bash
# 步骤 1: 下载预编译 LLVM（5-10 分钟）
python3 scripts/download_llvm_prebuilt.py

# 步骤 2: 构建 PawLang
zig build

# 步骤 3: 使用 LLVM 后端
./zig-out/bin/pawc examples/hello.paw --backend=llvm
llvm/install/bin/clang output.ll -o hello
./hello
```

**总时间**: ~10 分钟（主要是下载时间）

---

### 源码编译安装步骤

```bash
# 步骤 1: 安装依赖（取决于系统，可能需要 10-30 分钟）
# macOS
brew install cmake ninja

# Ubuntu/Debian
sudo apt install cmake ninja-build build-essential

# 步骤 2: 下载 LLVM 源码（5-10 分钟）
python3 scripts/setup_llvm.py --method=git

# 步骤 3: 编译 LLVM（30-60 分钟，取决于 CPU）
python3 scripts/build_llvm.py

# 步骤 4: 构建 PawLang
zig build

# 步骤 5: 使用 LLVM 后端
./zig-out/bin/pawc examples/hello.paw --backend=llvm
llvm/install/bin/clang output.ll -o hello
./hello
```

**总时间**: ~45-100 分钟（取决于系统和硬件）

## 💡 最佳实践

### 1. 初次使用 PawLang？

**使用预编译版本**。这样可以快速开始，专注于学习 PawLang 语言本身。

```bash
python3 scripts/download_llvm_prebuilt.py
```

### 2. 在 CI/CD 中使用？

**使用预编译版本**。大大加快 CI 构建速度，节省资源。

```yaml
# .github/workflows/build.yml 示例
- name: Setup LLVM
  run: python3 scripts/download_llvm_prebuilt.py
```

### 3. 开发 PawLang 编译器？

**预编译版本足够**。除非你需要调试或修改 LLVM 本身，否则预编译版本完全满足需求。

### 4. 需要特殊的 LLVM 配置？

**源码编译**。修改 `scripts/build_llvm.py` 中的 CMake 配置选项。

## 🔄 切换方式

### 从预编译切换到源码编译

```bash
# 1. 删除预编译版本
rm -rf llvm/install

# 2. 下载源码并编译
python3 scripts/setup_llvm.py --method=git
python3 scripts/build_llvm.py
```

### 从源码编译切换到预编译

```bash
# 1. 删除现有安装
rm -rf llvm/install
rm -rf llvm/build  # 可选，节省空间

# 2. 下载预编译版本
python3 scripts/download_llvm_prebuilt.py
```

## ❓ 常见问题

### Q: 预编译版本性能如何？

A: 预编译版本使用 Release 模式构建，性能完全等同于自己编译的 Release 版本。

### Q: 预编译版本安全吗？

A: 预编译版本来自可信的开源仓库（terralang/llvm-build），已被广泛使用。你也可以查看其构建脚本验证构建过程。

### Q: 可以同时安装两种版本吗？

A: 不行。两者都安装到 `llvm/install/` 目录。选择一种即可。

### Q: 预编译版本会定期更新吗？

A: 是的，预编译仓库会跟随 LLVM 官方发布更新。

### Q: 我的平台不支持预编译版本怎么办？

A: 使用源码编译。如果遇到困难，请参考 [LLVM Build Guide](LLVM_BUILD_GUIDE.md)。

## 📚 相关文档

- [LLVM Prebuilt Guide](LLVM_PREBUILT_GUIDE.md) - 预编译版本详细指南
- [LLVM Build Guide](LLVM_BUILD_GUIDE.md) - 源码编译详细指南
- [Scripts README](../scripts/README.md) - 脚本使用说明
- [Quick Start](QUICKSTART.md) - PawLang 快速开始

## 🎓 总结

**预编译版本**是大多数用户的最佳选择：
- ✅ 快速、简单、节省空间
- ✅ 适合 95% 的使用场景
- ✅ 推荐给所有新用户

**源码编译**适合特殊需求：
- ✅ 需要自定义配置
- ✅ 需要特定版本
- ✅ 学习目的

**建议**: 除非有明确的理由需要源码编译，否则使用预编译版本。

