# 🔧 交叉编译 LLVM 支持方案

## 📋 问题说明

交叉编译时，编译器二进制需要链接 LLVM 库。但 Ubuntu 24.04 不提供 armhf 和 i386 的多架构 LLVM 包，导致无法将 x86_64 的 LLVM 库链接到 ARM 或 i386 的二进制中。

## 🎯 解决方案

### 方案 1: 原生构建（在目标架构上）✅ 推荐

使用 QEMU/Docker 在目标架构上原生构建：

```yaml
# ARM32 原生构建
- uses: uraimo/run-on-arch-action@v2
  with:
    arch: armv7
    distro: ubuntu22.04
    run: |
      # 在 ARM 容器中原生构建
      # 安装 ARM 版本的 Zig 和 LLVM
      # 构建包含 LLVM 的 pawc
```

**优点**：
- ✅ 真正的原生二进制，包含 LLVM
- ✅ 完整的 LLVM 后端支持
- ✅ 自包含分发

**缺点**：
- ⚠️ 构建时间较长（QEMU 模拟慢 5-10倍）
- ⚠️ 需要下载目标架构的 Zig 和 LLVM

### 方案 2: 静态链接 LLVM 🔬 实验性

修改 build.zig 使用静态链接：

```zig
// 当前: --link-shared
const link_result = b.run(&[_][]const u8{
    config_path,
    "--link-shared",  // 改为 --link-static
    "--ldflags",
    "--libs",
    "--system-libs",
});
```

**优点**：
- ✅ 可以在 x86_64 主机上交叉编译
- ✅ 二进制完全自包含

**缺点**：
- ⚠️ 仍需要目标架构的 LLVM 静态库
- ⚠️ 二进制非常大（~100MB+）
- ⚠️ Ubuntu 可能不提供静态库包

### 方案 3: 预编译 LLVM 库 📦

为每个目标架构预编译 LLVM 库并缓存：

```bash
# 为 armv7 预编译 LLVM
docker run --platform linux/arm/v7 ubuntu:22.04 bash -c "
  apt-get update
  apt-get install -y build-essential cmake
  # 编译 LLVM 19.1.7 for armv7
  # 打包并上传到 GitHub Releases
"
```

**优点**：
- ✅ 一次编译，多次使用
- ✅ 不影响 CI 速度

**缺点**：
- ⚠️ 初始设置复杂
- ⚠️ 需要维护预编译库

### 方案 4: 接受现实，明确文档 📝

在文档中明确说明：

- **Pre-built 发布版**: 只有 x86_64 包含 LLVM
- **从源码编译**: 所有架构都可以有 LLVM（在目标设备上编译）

**优点**：
- ✅ 简单直接
- ✅ 不增加复杂度
- ✅ 用户可以自行编译获得 LLVM

**缺点**：
- ⚠️ Pre-built 发布不完整

## 🚀 推荐实现

我建议采用 **方案 1 (原生构建)** 的混合方案：

1. **主要发布**: 使用交叉编译（快速，无 LLVM）
2. **额外发布**: 使用 QEMU/Docker 原生构建（慢但有 LLVM）

这样用户可以选择：
- 快速下载：armv7/x86 无 LLVM（~2MB）
- 完整版本：armv7/x86 含 LLVM（~50MB）

## 🔧 实现代码

我已经创建了 `.github/workflows/release-cross-llvm.yml` 来测试这个方案。

要启用此功能：

```bash
# 手动触发测试
gh workflow run release-cross-llvm.yml --repo pawlang-project/paw
```

## 📊 预期结果

如果成功，我们将获得：
- `pawlang-linux-armv7-llvm.tar.gz` - 包含 LLVM 的 ARM32 版本
- `pawlang-linux-x86-llvm.tar.gz` - 包含 LLVM 的 i386 版本

这将证明技术可行性！

