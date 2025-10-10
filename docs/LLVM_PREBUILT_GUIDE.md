# LLVM 预编译版本使用指南

本指南介绍如何使用预编译的 LLVM 二进制文件，无需从源码编译。

## 📦 什么是预编译版本？

预编译版本是已经编译好的 LLVM 工具链，包含：
- ✅ Clang 编译器
- ✅ LLVM 核心库
- ✅ LLVM 工具（llc, opt, etc.）
- ✅ 头文件和库文件

**优势：**
- ⚡ **节省时间**：无需编译 2-3 小时
- 💾 **占用空间小**：约 500MB vs 源码 2GB + 编译产物 10GB+
- 🚀 **即下即用**：下载后立即可用

## 🎯 支持的平台

| 平台 | 架构 | 文件 |
|------|------|------|
| macOS | x86_64 (Intel) | clang+llvm-19.1.7-x86_64-apple-darwin23.tar.xz |
| macOS | arm64 (M1/M2/M3) | clang+llvm-19.1.7-arm64-apple-darwin23.tar.xz |
| Linux | x86_64 | clang+llvm-19.1.7-x86_64-linux-gnu-ubuntu-20.04.tar.xz |
| Linux | aarch64 (ARM64) | clang+llvm-19.1.7-aarch64-linux-gnu.tar.xz |
| Windows | x86_64 | clang+llvm-19.1.7-x86_64-pc-windows-msvc.tar.xz |

## 🚀 快速开始

### 方法 1: 使用统一脚本（推荐）

```bash
# 默认使用预编译版本
python scripts/setup_llvm.py

# 或明确指定
python scripts/setup_llvm.py --method=prebuilt
```

### 方法 2: 直接使用下载脚本

```bash
python scripts/download_llvm_prebuilt.py
```

## 📋 详细步骤

### 1. 下载预编译 LLVM

```bash
cd PawLang
python scripts/download_llvm_prebuilt.py
```

脚本会自动：
- ✅ 检测你的操作系统和架构
- ✅ 从 GitHub Releases 下载对应版本
- ✅ 解压到 `llvm/install/` 目录
- ✅ 验证安装是否完整

**下载信息：**
- 下载大小：约 200-300 MB（压缩）
- 解压后大小：约 500-800 MB
- 下载时间：取决于网速（通常 2-5 分钟）

### 2. 构建 PawLang

```bash
zig build
```

构建系统会自动检测 `llvm/install/` 目录并启用 LLVM 后端支持。

### 3. 使用 LLVM 后端

```bash
# 生成 LLVM IR
./zig-out/bin/pawc examples/hello.paw --backend=llvm

# 编译 IR 到可执行文件
llvm/install/bin/clang output.ll -o hello

# 运行
./hello
```

## 🔧 手动安装（高级）

如果你想手动下载和安装：

### 1. 下载预编译包

访问：https://github.com/pawlang-project/llvm-build/releases/tag/19.1.7

根据你的平台下载对应的文件：
- macOS (Intel): `clang+llvm-19.1.7-x86_64-apple-darwin23.tar.xz`
- macOS (Apple Silicon): `clang+llvm-19.1.7-arm64-apple-darwin23.tar.xz`
- Linux (x86_64): `clang+llvm-19.1.7-x86_64-linux-gnu-ubuntu-20.04.tar.xz`
- Linux (ARM64): `clang+llvm-19.1.7-aarch64-linux-gnu.tar.xz`
- Windows (x86_64): `clang+llvm-19.1.7-x86_64-pc-windows-msvc.tar.xz`

### 2. 解压

```bash
# 创建目录
mkdir -p llvm/temp

# 解压
tar -xf clang+llvm-19.1.7-*.tar.xz -C llvm/temp

# 移动到 install 目录
mv llvm/temp/clang+llvm-19.1.7-* llvm/install

# 清理
rm -rf llvm/temp
```

### 3. 验证安装

```bash
llvm/install/bin/clang --version
```

应该输出类似：
```
clang version 19.1.7
Target: x86_64-apple-darwin23.0.0
Thread model: posix
```

## 🆚 预编译 vs 源码编译

| 特性 | 预编译版本 | 源码编译 |
|------|-----------|----------|
| 安装时间 | 5-10 分钟 | 2-3 小时 |
| 磁盘占用 | 约 500 MB | 约 12 GB |
| 自定义选项 | ❌ 固定配置 | ✅ 完全自定义 |
| 难度 | ⭐ 简单 | ⭐⭐⭐⭐ 困难 |
| 适用场景 | 大多数用户 | 需要自定义构建 |

## 📝 常见问题

### Q: 预编译版本包含哪些组件？

A: 包含：
- Clang/LLVM 编译器
- LLVM 静态库和动态库
- LLVM C/C++ 头文件
- LLVM 工具链（llc, opt, llvm-link, etc.）
- LLVM C API 绑定

### Q: 可以同时安装预编译版本和源码版本吗？

A: 不建议。两者会安装到相同位置（`llvm/install/`）。选择其中一种即可。

### Q: 预编译版本的配置是什么？

A: 使用标准配置构建：
- 构建工具：Clang + lld
- 启用项目：clang, lld, compiler-rt, libcxx, libcxxabi
- 构建类型：Release
- 目标架构：与平台对应

### Q: 如何切换回源码编译？

A: 删除 `llvm/install/` 目录，然后：

```bash
# 使用源码编译
python scripts/setup_llvm.py --method=git
python scripts/build_llvm.py
```

### Q: 下载失败怎么办？

A: 可能的原因和解决方案：

1. **网络问题**：
   - 使用 VPN 或代理
   - 手动从 GitHub 下载（见上面"手动安装"部分）

2. **GitHub 访问受限**：
   - 从国内镜像下载（如果有）
   - 使用其他网络环境

3. **磁盘空间不足**：
   - 确保至少有 2GB 可用空间

### Q: 预编译版本适合我的 Linux 发行版吗？

A: Linux 预编译版本基于 Ubuntu 20.04 构建，但通常兼容大多数现代 Linux 发行版：
- ✅ Ubuntu 20.04+
- ✅ Debian 11+
- ✅ Fedora 30+
- ✅ CentOS 8+
- ✅ Arch Linux

如果遇到兼容性问题，建议从源码编译。

## 🔗 相关资源

- **预编译版本仓库**: https://github.com/pawlang-project/llvm-build
- **上游项目**: https://github.com/terralang/llvm-build
- **LLVM 官方**: https://llvm.org/
- **源码编译指南**: [LLVM_BUILD_GUIDE.md](LLVM_BUILD_GUIDE.md)

## 📞 支持

如果遇到问题：
1. 检查 [常见问题](#-常见问题) 部分
2. 查看 GitHub Issues
3. 尝试从源码编译（备选方案）

---

**推荐用法**: 对于大多数用户，使用预编译版本是最简单和最快的选择。只有需要自定义 LLVM 构建配置时才建议从源码编译。

