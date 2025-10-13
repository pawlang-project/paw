# PawLang 构建系统使用指南

**版本**: v0.2.0  
**日期**: 2025-10-13

---

## 🚀 快速开始

### 新用户 - 完整安装

```bash
# 1. 克隆仓库
git clone https://github.com/pawlang-project/paw.git
cd paw

# 2. 安装 LLVM（可选，推荐）
zig build setup-llvm

# 3. 构建编译器
zig build

# 4. 测试
./zig-out/bin/pawc examples/hello.paw --run
```

### 已有用户 - 检查安装

```bash
# 检查 LLVM 是否已安装
zig build check-llvm

# 如果未安装，运行
zig build setup-llvm
```

---

## 📋 所有构建命令

### 基础构建

| 命令 | 说明 | 用途 |
|------|------|------|
| `zig build` | 构建编译器 | 最常用 |
| `zig build -Doptimize=ReleaseFast` | 发布版本构建 | 生产环境 |
| `zig build install` | 安装到 zig-out/ | 同 `zig build` |

### LLVM 管理 (🆕 v0.2.0)

| 命令 | 说明 | 平台 |
|------|------|------|
| `zig build setup-llvm` | 自动下载和安装 LLVM | 所有平台 ✅ |
| `zig build check-llvm` | 检查 LLVM 安装状态 | 所有平台 ✅ |

### 运行和测试

| 命令 | 说明 |
|------|------|
| `zig build run` | 构建并运行（需要文件参数） |
| `zig build test` | 运行测试套件 |

### 打包分发

| 命令 | 说明 |
|------|------|
| `zig build dist` | 准备分发文件 |
| `zig build package` | 创建完整发布包 (.tar.gz 或 .zip) |

---

## 🌍 跨平台使用

### Windows

```powershell
# PowerShell

# 安装 LLVM
zig build setup-llvm

# 检查安装
zig build check-llvm

# 构建
zig build

# 运行
.\zig-out\bin\pawc.exe hello.paw --run
```

### macOS

```bash
# Bash

# 安装 LLVM
zig build setup-llvm

# 检查安装
zig build check-llvm

# 构建
zig build

# 运行
./zig-out/bin/pawc hello.paw --run
```

### Linux

```bash
# Bash

# 安装 LLVM
zig build setup-llvm

# 检查安装
zig build check-llvm

# 构建
zig build

# 运行
./zig-out/bin/pawc hello.paw --run
```

---

## 🔧 LLVM 设置详解

### setup-llvm 命令

**功能**:
- 自动检测当前平台和架构
- 从 GitHub Releases 下载预编译的 LLVM
- 解压到 `vendor/llvm/<platform>/install/`
- 验证安装

**支持的平台**:
- ✅ Windows (x86_64, ARM64, x86)
- ✅ macOS (ARM64, x86_64)
- ✅ Linux (x86_64, ARM64, ARM32, RISC-V, 等)

**使用示例**:

```bash
# 一键安装
zig build setup-llvm

# 输出示例：
# ==========================================
#    LLVM Auto-Download
# ==========================================
# 
# Detected platform: macos-aarch64
# 
# 📥 Downloading LLVM 21.1.3 for macos-aarch64...
#    URL: https://github.com/...
# 
# 📦 Extracting...
# 
# ✅ LLVM Installed Successfully!
```

**内部调用**:

- **Windows**: `powershell -ExecutionPolicy Bypass -File setup_llvm.ps1`
- **Unix**: `sh setup_llvm.sh`

### check-llvm 命令

**功能**:
- 检查 LLVM 是否已安装
- 显示安装位置
- 显示 LLVM 版本
- 如果未安装，提供安装指令

**输出示例**:

**已安装**:
```
✅ LLVM is installed
   Location: vendor/llvm/macos-aarch64/install/

clang version 21.1.3

🚀 You can use: zig build
```

**未安装**:
```
❌ LLVM not found
   Expected: vendor/llvm/macos-aarch64/install/bin/clang

📥 To install LLVM, run:
   zig build setup-llvm

   Or manually:
   ./setup_llvm.sh
```

---

## 📖 典型工作流

### 场景1: 新机器首次安装

```bash
# 1. 克隆代码
git clone https://github.com/pawlang-project/paw.git
cd paw

# 2. 检查是否需要 LLVM
zig build check-llvm

# 3. 如果需要，安装 LLVM
zig build setup-llvm

# 4. 构建编译器
zig build

# 5. 测试
./zig-out/bin/pawc examples/hello.paw --backend=llvm --run
```

### 场景2: 只使用 C 后端

```bash
# 不需要 LLVM，直接构建
zig build -Denable-llvm=false

# 使用 C 后端
./zig-out/bin/pawc hello.paw --backend=c --run
```

### 场景3: CI/CD 环境

```bash
# CI 脚本
zig build setup-llvm    # 自动下载 LLVM
zig build               # 构建
zig build test          # 测试
zig build package       # 打包
```

### 场景4: 开发和调试

```bash
# 开发模式（快速迭代）
zig build               # Debug 模式

# 性能测试
zig build -Doptimize=ReleaseFast

# 检查 LLVM
zig build check-llvm
```

---

## ⚙️ 构建选项

### 优化级别

```bash
# Debug (默认) - 快速编译，包含调试信息
zig build

# ReleaseSafe - 优化 + 安全检查
zig build -Doptimize=ReleaseSafe

# ReleaseFast - 最大性能
zig build -Doptimize=ReleaseFast

# ReleaseSmall - 最小体积
zig build -Doptimize=ReleaseSmall
```

### LLVM 后端控制

```bash
# 启用 LLVM (默认)
zig build

# 禁用 LLVM (纯 C 后端)
zig build -Denable-llvm=false
```

---

## 🎯 常见任务

### 安装 LLVM

```bash
# 方式1: 通过 zig build (推荐)
zig build setup-llvm

# 方式2: 直接运行脚本
# Windows
.\setup_llvm.ps1

# macOS/Linux
./setup_llvm.sh
```

### 验证安装

```bash
# 检查 LLVM
zig build check-llvm

# 手动检查
# Windows
.\vendor\llvm\windows-x86_64\install\bin\clang.exe --version

# macOS/Linux
./vendor/llvm/*/install/bin/clang --version
```

### 清理和重新构建

```bash
# 清理构建产物
rm -rf zig-cache zig-out

# 重新构建
zig build
```

### 创建分发包

```bash
# 准备分发文件
zig build dist

# 创建压缩包
zig build package

# 结果:
# Windows: pawlang-windows.zip
# macOS: pawlang-macos.tar.gz
# Linux: pawlang-linux.tar.gz
```

---

## 🐛 故障排除

### 问题1: setup-llvm 失败

**症状**: 下载失败或网络错误

**解决**:
```bash
# 手动运行脚本（更详细的错误信息）
# Windows
.\setup_llvm.ps1

# macOS/Linux
./setup_llvm.sh
```

### 问题2: PowerShell 执行策略限制 (Windows)

**症状**: `无法加载文件，因为在此系统上禁止运行脚本`

**解决**:
```powershell
# 临时允许
Set-ExecutionPolicy -ExecutionPolicy Bypass -Scope Process
zig build setup-llvm

# 或直接运行
powershell -ExecutionPolicy Bypass -File setup_llvm.ps1
```

### 问题3: check-llvm 显示未安装（但实际已安装）

**解决**:
```bash
# 检查实际路径
ls -la vendor/llvm/*/install/bin/clang

# 重新运行 setup-llvm
zig build setup-llvm
```

### 问题4: 构建时找不到 LLVM

**症状**: `LLVM backend not available`

**解决**:
```bash
# 1. 检查 LLVM
zig build check-llvm

# 2. 如果需要，安装
zig build setup-llvm

# 3. 重新构建
zig build
```

---

## 📂 目录结构

```
PawLang/
├── build.zig                 # 构建配置（已更新）
├── setup_llvm.sh             # Unix 安装脚本
├── setup_llvm.ps1            # Windows PowerShell 脚本
├── setup_llvm.bat            # Windows 批处理脚本
├── vendor/
│   └── llvm/
│       ├── macos-aarch64/    # macOS ARM64
│       ├── macos-x86_64/     # macOS Intel
│       ├── linux-x86_64/     # Linux x86_64
│       ├── windows-x86_64/   # Windows x86_64
│       └── ...               # 其他平台
├── src/
│   └── main.zig              # 编译器源码
└── zig-out/
    ├── bin/
    │   └── pawc              # 编译后的可执行文件
    └── lib/                  # LLVM 库（macOS/Linux）
```

---

## 🎯 推荐工作流

### 开发者工作流

```bash
# 1. 首次设置
git clone https://github.com/pawlang-project/paw.git
cd paw
zig build check-llvm          # 检查状态
zig build setup-llvm          # 如果需要

# 2. 日常开发
zig build                     # 编译
./zig-out/bin/pawc test.paw --backend=llvm  # 测试

# 3. 提交前
zig build test                # 运行测试
zig build -Doptimize=ReleaseFast  # 确保 release 能构建
```

### 用户工作流

```bash
# 1. 下载预编译版本
wget https://github.com/.../pawlang-linux.tar.gz
tar xzf pawlang-linux.tar.gz

# 2. 直接使用（无需安装）
cd pawlang
./bin/pawc hello.paw --run

# 或者从源码构建
git clone ...
cd paw
zig build setup-llvm
zig build
```

---

## 💡 最佳实践

### DO ✅

- ✅ 使用 `zig build setup-llvm` 自动安装
- ✅ 使用 `zig build check-llvm` 验证
- ✅ 优先使用 LLVM 后端（性能更好）
- ✅ CI/CD 中自动下载 LLVM
- ✅ 创建分发包前运行测试

### DON'T ❌

- ❌ 不要手动修改 vendor/llvm/ 目录
- ❌ 不要提交 vendor/llvm/ 到 git（已在 .gitignore）
- ❌ 不要使用系统 LLVM（不兼容）
- ❌ 不要跳过 LLVM 安装（除非只用 C 后端）

---

## 🔗 相关文档

- [LLVM_SETUP_CROSS_PLATFORM.md](LLVM_SETUP_CROSS_PLATFORM.md) - 详细安装指南
- [QUICKSTART.md](QUICKSTART.md) - 快速入门
- [README.md](../README.md) - 项目主页

---

## 📞 获取帮助

遇到问题？

1. 运行 `zig build check-llvm` 诊断
2. 查看本文档的"故障排除"部分
3. 查看 [LLVM_SETUP_CROSS_PLATFORM.md](LLVM_SETUP_CROSS_PLATFORM.md)
4. 在 GitHub 提 Issue

---

**享受使用 PawLang 构建系统！** 🚀

