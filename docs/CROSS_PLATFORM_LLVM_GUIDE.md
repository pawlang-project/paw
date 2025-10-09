# 🌍 跨平台 LLVM 集成指南

**版本**: LLVM 19.1.6  
**支持平台**: macOS, Linux, Windows, FreeBSD

---

## 🎯 支持的平台

### ✅ 完全支持

| 平台 | 架构 | 预编译 | 源码编译 | 推荐方式 |
|------|------|--------|----------|----------|
| **macOS** | ARM64 (M1/M2/M3) | ✅ | ✅ | 预编译 |
| **macOS** | X64 (Intel) | ✅ | ✅ | 预编译 |
| **Linux** | X64 | ✅ | ✅ | 预编译 |
| **Linux** | ARM64 | ✅ | ✅ | 预编译 |
| **Windows** | X64 | ✅ | ✅ | 预编译 |
| **Windows** | X86 | ✅ | ✅ | 预编译 |

### ⚠️ 部分支持

| 平台 | 架构 | 预编译 | 源码编译 | 推荐方式 |
|------|------|--------|----------|----------|
| **FreeBSD** | X64 | ❌ | ✅ | 源码编译 |
| **FreeBSD** | ARM64 | ❌ | ✅ | 源码编译 |
| **Linux** | ARM32 | ❌ | ✅ | 源码编译 |

---

## 🚀 快速开始

### 方式 1: 预编译版本（推荐）

```bash
# 自动检测平台并下载
./scripts/download_llvm_cross_platform.sh
```

### 方式 2: 源码编译

```bash
# 自动检测平台并编译
./scripts/build_llvm_cross_platform.sh
```

---

## 📋 平台特定说明

### 🍎 macOS

#### 依赖安装
```bash
# 使用 Homebrew
brew install cmake ninja

# 验证
cmake --version
ninja --version
```

#### 架构支持
- **ARM64 (Apple Silicon)**: 原生支持，性能最佳
- **X64 (Intel)**: 完全支持，兼容性好

#### 特殊配置
```bash
# 自动设置部署目标 (10.15+)
# 自动配置架构
# 优化 macOS 特定选项
```

### 🐧 Linux

#### 依赖安装

**Ubuntu/Debian**:
```bash
sudo apt update
sudo apt install cmake ninja-build build-essential
```

**CentOS/RHEL**:
```bash
sudo yum install cmake ninja-build gcc-c++
# 或使用 dnf (较新版本)
sudo dnf install cmake ninja-build gcc-c++
```

**Arch Linux**:
```bash
sudo pacman -S cmake ninja gcc
```

**Alpine Linux**:
```bash
sudo apk add cmake ninja gcc g++ musl-dev
```

#### 架构支持
- **X64**: 完全支持，性能最佳
- **ARM64**: 完全支持，适用于服务器
- **ARM32**: 需要源码编译

### 🪟 Windows

#### 环境选择

**推荐: WSL2 (Ubuntu)**
```bash
# 在 WSL2 中运行
sudo apt install cmake ninja-build build-essential
./scripts/download_llvm_cross_platform.sh
```

**MSYS2**
```bash
# 安装依赖
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gcc

# 运行脚本
./scripts/download_llvm_cross_platform.sh
```

**Visual Studio**
```bash
# 需要手动配置
# 推荐使用 WSL2 或 MSYS2
```

#### 架构支持
- **X64**: 完全支持
- **X86**: 支持，但性能较低

### 🐧 FreeBSD

#### 依赖安装
```bash
sudo pkg install cmake ninja gcc
```

#### 特殊说明
- **仅支持源码编译**（无预编译版本）
- 需要较长的编译时间
- 建议使用 `build_llvm_cross_platform.sh`

---

## 🔧 构建配置

### 自动配置

脚本会自动检测并配置：

```bash
# 平台检测
🖥️  Platform: macOS-ARM64
🔧 CPU cores: 8

# 自动 CMake 配置
-DCMAKE_BUILD_TYPE=Release
-DCMAKE_INSTALL_PREFIX=llvm/install
-DLLVM_ENABLE_PROJECTS=clang
-DLLVM_TARGETS_TO_BUILD=AArch64;X86
-DCMAKE_OSX_DEPLOYMENT_TARGET=10.15  # macOS
-DCMAKE_OSX_ARCHITECTURES=arm64      # macOS ARM64
```

### 手动配置

如果需要自定义配置：

```bash
# 编辑脚本中的 get_cmake_options 函数
# 添加或修改 CMake 选项
```

---

## 📊 性能对比

### 编译时间估算

| 平台 | 预编译下载 | 源码编译 | 推荐 |
|------|------------|----------|------|
| **macOS M1** | 2分钟 | 30分钟 | 预编译 |
| **macOS Intel** | 2分钟 | 45分钟 | 预编译 |
| **Linux X64** | 2分钟 | 35分钟 | 预编译 |
| **Linux ARM64** | 2分钟 | 60分钟 | 预编译 |
| **Windows WSL** | 2分钟 | 40分钟 | 预编译 |
| **FreeBSD** | N/A | 50分钟 | 源码 |

### 空间占用

| 方式 | 大小 | 说明 |
|------|------|------|
| **预编译** | 1-2GB | 包含完整工具链 |
| **源码编译** | 7GB | 源码+构建+安装 |
| **文本模式** | 5MB | 无需 LLVM |

---

## 🛠️ 故障排除

### 常见问题

#### 1. 依赖缺失
```bash
❌ Missing dependencies: cmake ninja

# 解决方案
# macOS: brew install cmake ninja
# Ubuntu: sudo apt install cmake ninja-build
# CentOS: sudo yum install cmake ninja-build
```

#### 2. 架构不支持
```bash
❌ Unsupported macOS architecture: i386

# 解决方案
# 使用支持的架构: arm64, x86_64
```

#### 3. 网络问题
```bash
❌ Download failed

# 解决方案
# 检查网络连接
# 使用代理或镜像
# 尝试源码编译
```

#### 4. 权限问题
```bash
❌ Permission denied

# 解决方案
chmod +x scripts/*.sh
```

### 平台特定问题

#### macOS
- **Xcode 命令行工具**: `xcode-select --install`
- **Homebrew**: 确保使用最新版本
- **架构兼容**: 自动处理 Rosetta 2

#### Linux
- **GCC 版本**: 需要 GCC 7.0+
- **内存要求**: 至少 4GB RAM
- **磁盘空间**: 至少 10GB 可用空间

#### Windows
- **WSL2**: 推荐使用 Ubuntu 20.04+
- **MSYS2**: 确保使用正确的架构包
- **路径问题**: 避免空格和特殊字符

#### FreeBSD
- **Ports**: 可能需要从 ports 编译
- **内存**: 建议 8GB+ RAM
- **时间**: 编译时间较长

---

## 🔄 更新和维护

### 更新 LLVM 版本

```bash
# 1. 更新源码
cd llvm/19.1.6
git pull origin release/19.x

# 2. 重新编译
./scripts/build_llvm_cross_platform.sh
```

### 清理构建

```bash
# 清理构建目录
rm -rf llvm/build llvm/install

# 重新开始
./scripts/download_llvm_cross_platform.sh
```

### 验证安装

```bash
# 检查版本
llvm/install/bin/llvm-config --version

# 检查工具
ls llvm/install/bin/

# 测试 PawLang 集成
zig build -Dwith-llvm=true
```

---

## 📚 参考资源

### 官方文档
- [LLVM 官方文档](https://llvm.org/docs/)
- [CMake 跨平台指南](https://cmake.org/cmake/help/latest/guide/user-interaction/index.html)
- [Ninja 构建系统](https://ninja-build.org/)

### 平台特定
- [macOS 开发指南](https://developer.apple.com/documentation/)
- [Linux 发行版文档](https://www.linux.org/docs/)
- [Windows 开发环境](https://docs.microsoft.com/en-us/windows/dev-environment/)
- [FreeBSD 手册](https://docs.freebsd.org/)

---

## ✅ 总结

### 推荐策略

1. **首选**: 预编译版本（快速、可靠）
2. **备选**: 源码编译（完整控制）
3. **轻量**: 文本模式（无需 LLVM）

### 平台优先级

1. **macOS**: 预编译 ARM64 > 预编译 X64 > 源码
2. **Linux**: 预编译 X64 > 预编译 ARM64 > 源码
3. **Windows**: WSL2 预编译 > MSYS2 预编译 > 源码
4. **FreeBSD**: 源码编译

### 最佳实践

- ✅ 使用自动化脚本
- ✅ 检查依赖完整性
- ✅ 验证安装结果
- ✅ 保持版本一致性
- ✅ 定期更新维护

---

**🌍 PawLang 现在支持全平台 LLVM 集成！**
