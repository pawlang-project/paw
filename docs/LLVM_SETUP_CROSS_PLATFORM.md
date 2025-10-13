# LLVM 跨平台安装指南

**版本**: v0.2.0  
**LLVM版本**: 21.1.3  
**日期**: 2025-10-13

---

## 🌍 支持的平台

### macOS
- ✅ Apple Silicon (ARM64)
- ✅ Intel (x86_64)

### Linux
- ✅ x86_64 (主流)
- ✅ ARM64 (aarch64)
- ✅ ARM32 (armv7)
- ✅ RISC-V 64
- ✅ PowerPC 64 LE
- ✅ LoongArch 64
- ✅ S390X

### Windows
- ✅ x86_64 (主流)
- ✅ ARM64
- ✅ x86 (32位)

---

## 📥 快速安装

### macOS / Linux

```bash
# 一键安装
./setup_llvm.sh

# 手动步骤（如果需要）
chmod +x setup_llvm.sh
./setup_llvm.sh
```

### Windows

**方式1: PowerShell (推荐)** ⭐

```powershell
# 以管理员身份运行 PowerShell
.\setup_llvm.ps1
```

**方式2: 批处理文件**

```cmd
REM 双击运行或在命令行执行
setup_llvm.bat
```

---

## 🔧 详细步骤

### 1. macOS 安装

#### 方法A: 自动脚本 (推荐)

```bash
# 克隆仓库
git clone https://github.com/pawlang-project/paw.git
cd paw

# 运行安装脚本
./setup_llvm.sh

# 等待下载和解压（~500MB，视网速而定）
```

#### 方法B: 手动下载

```bash
# Apple Silicon
curl -L -O https://github.com/pawlang-project/llvm-build/releases/download/llvm-21.1.3/llvm-21.1.3-macos-aarch64.tar.gz
mkdir -p vendor/llvm/macos-aarch64
tar xzf llvm-21.1.3-macos-aarch64.tar.gz -C vendor/llvm/macos-aarch64/
rm llvm-21.1.3-macos-aarch64.tar.gz

# Intel
curl -L -O https://github.com/pawlang-project/llvm-build/releases/download/llvm-21.1.3/llvm-21.1.3-macos-x86_64.tar.gz
mkdir -p vendor/llvm/macos-x86_64
tar xzf llvm-21.1.3-macos-x86_64.tar.gz -C vendor/llvm/macos-x86_64/
rm llvm-21.1.3-macos-x86_64.tar.gz
```

---

### 2. Linux 安装

#### 方法A: 自动脚本 (推荐)

```bash
# 克隆仓库
git clone https://github.com/pawlang-project/paw.git
cd paw

# 运行安装脚本
./setup_llvm.sh

# 等待下载和解压
```

#### 方法B: 手动下载

```bash
# x86_64 (最常见)
wget https://github.com/pawlang-project/llvm-build/releases/download/llvm-21.1.3/llvm-21.1.3-linux-x86_64.tar.gz
mkdir -p vendor/llvm/linux-x86_64
tar xzf llvm-21.1.3-linux-x86_64.tar.gz -C vendor/llvm/linux-x86_64/
rm llvm-21.1.3-linux-x86_64.tar.gz

# ARM64
wget https://github.com/pawlang-project/llvm-build/releases/download/llvm-21.1.3/llvm-21.1.3-linux-aarch64.tar.gz
mkdir -p vendor/llvm/linux-aarch64
tar xzf llvm-21.1.3-linux-aarch64.tar.gz -C vendor/llvm/linux-aarch64/
rm llvm-21.1.3-linux-aarch64.tar.gz
```

---

### 3. Windows 安装

#### 方法A: PowerShell (推荐) ⭐

```powershell
# 以管理员身份打开 PowerShell

# 克隆仓库
git clone https://github.com/pawlang-project/paw.git
cd paw

# 运行安装脚本
.\setup_llvm.ps1

# 等待下载和解压（~500MB）
```

#### 方法B: 批处理文件

```cmd
REM 克隆仓库
git clone https://github.com/pawlang-project/paw.git
cd paw

REM 运行安装脚本
setup_llvm.bat

REM 等待完成
```

#### 方法C: 手动下载

```powershell
# 下载（使用浏览器或 PowerShell）
$url = "https://github.com/pawlang-project/llvm-build/releases/download/llvm-21.1.3/llvm-21.1.3-windows-x86_64.tar.gz"
$output = "llvm-21.1.3-windows-x86_64.tar.gz"
(New-Object System.Net.WebClient).DownloadFile($url, $output)

# 创建目录
New-Item -ItemType Directory -Force -Path vendor\llvm\windows-x86_64

# 解压（使用 tar 或 7-Zip）
tar -xzf llvm-21.1.3-windows-x86_64.tar.gz -C vendor\llvm\windows-x86_64\

# 清理
Remove-Item llvm-21.1.3-windows-x86_64.tar.gz
```

---

## 🔍 安装验证

### macOS / Linux

```bash
# 验证 clang
./vendor/llvm/*/install/bin/clang --version

# 应该显示 LLVM 21.1.3
```

### Windows

```powershell
# 验证 clang
.\vendor\llvm\windows-x86_64\install\bin\clang.exe --version

# 应该显示 LLVM 21.1.3
```

---

## 🚀 构建 PawLang

### 所有平台

```bash
# 构建编译器
zig build

# 测试 LLVM 后端
# macOS/Linux
./zig-out/bin/pawc examples/hello.paw --backend=llvm

# Windows
.\zig-out\bin\pawc.exe examples\hello.paw --backend=llvm
```

---

## ⚠️ 常见问题

### 问题1: PowerShell 执行策略限制

**错误**: `无法加载文件 setup_llvm.ps1，因为在此系统上禁止运行脚本`

**解决**:
```powershell
# 临时允许执行（推荐）
Set-ExecutionPolicy -ExecutionPolicy Bypass -Scope Process

# 然后运行脚本
.\setup_llvm.ps1

# 或者直接运行
powershell -ExecutionPolicy Bypass -File .\setup_llvm.ps1
```

### 问题2: tar 命令不可用 (Windows 7/8)

**错误**: `'tar' 不是内部或外部命令`

**解决**:
```
1. 升级到 Windows 10/11 (推荐)
2. 或安装 7-Zip: https://www.7-zip.org/
3. 或使用 Git Bash (自带 tar)
```

### 问题3: 下载速度慢

**解决**:
```bash
# 使用国内镜像（如果可用）
# 或者使用代理

# macOS/Linux
export https_proxy=http://your-proxy:port
./setup_llvm.sh

# Windows PowerShell
$env:HTTPS_PROXY="http://your-proxy:port"
.\setup_llvm.ps1
```

### 问题4: 磁盘空间不足

**需求**: 
- 下载文件: ~500MB
- 解压后: ~2GB
- 总计: ~2.5GB

**解决**: 清理磁盘空间或使用外部存储

### 问题5: 权限不足

**Windows**:
```
以管理员身份运行 PowerShell 或 CMD
```

**macOS/Linux**:
```bash
# 如果需要 sudo
sudo ./setup_llvm.sh

# 或修改目录权限
sudo chown -R $USER vendor/
```

---

## 📂 目录结构

安装后的目录结构：

```
vendor/llvm/
├── macos-aarch64/          # macOS ARM64
│   └── install/
│       ├── bin/            # clang, llc, lld 等
│       ├── lib/            # LLVM 库
│       └── include/        # LLVM 头文件
├── macos-x86_64/           # macOS Intel
│   └── install/
├── linux-x86_64/           # Linux x86_64
│   └── install/
├── windows-x86_64/         # Windows x86_64
│   └── install/
│       ├── bin/            # .exe 文件
│       ├── lib/            # .lib 和 .dll 文件
│       └── include/
└── README.md
```

---

## 🎯 不同平台的特点

### macOS
- ✅ 最简单：直接运行脚本
- ✅ 自带 tar 和 curl
- ✅ LLVM 库使用 @rpath

### Linux
- ✅ 简单：直接运行脚本
- ✅ 自带 tar 和 wget/curl
- ✅ 多架构支持

### Windows
- ⚠️ 需要 Windows 10+ (自带 tar)
- ⚠️ 可能需要管理员权限
- ✅ 提供3种安装方式
- ✅ PowerShell 和 Batch 脚本

---

## 📝 脚本对比

| 特性 | setup_llvm.sh | setup_llvm.ps1 | setup_llvm.bat |
|------|---------------|----------------|----------------|
| 平台 | macOS/Linux | Windows | Windows |
| 语言 | Bash | PowerShell | Batch |
| 推荐度 | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ |
| 易用性 | 简单 | 简单 | 中等 |
| 颜色输出 | ✅ | ✅ | 部分 |
| 进度显示 | ✅ | ✅ | 基础 |

**推荐使用**:
- **macOS/Linux**: `setup_llvm.sh`
- **Windows**: `setup_llvm.ps1` (PowerShell)

---

## 🔗 相关资源

- **LLVM Build 仓库**: https://github.com/pawlang-project/llvm-build
- **PawLang 主仓库**: https://github.com/pawlang-project/paw
- **问题反馈**: https://github.com/pawlang-project/paw/issues

---

## 📞 获取帮助

如果安装遇到问题：

1. 查看本文档的"常见问题"部分
2. 查看 vendor/llvm/README.md
3. 在 GitHub 提 Issue
4. 检查网络连接和防火墙设置

---

**祝安装顺利！享受 PawLang 的 LLVM 后端带来的高性能！** 🚀

