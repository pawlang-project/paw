# 🔧 PawLang 后端支持说明

## 📋 概述

PawLang 支持双后端架构：
- **C 后端**: 生成可移植的 C 代码
- **LLVM 后端**: 生成优化的 LLVM IR

## 🎯 各平台后端支持情况

### Pre-built 发布版

| 平台 | 架构 | C 后端 | LLVM 后端 | 说明 |
|------|------|--------|-----------|------|
| **Linux** | x86_64 | ✅ | ✅ | 完整支持，自包含 LLVM 库 |
| **Linux** | x86 (32-bit) | ✅ | ❌ | 交叉编译限制 |
| **Linux** | armv7 (ARM32) | ✅ | ❌ | 交叉编译限制 |
| **macOS** | x86_64 (Intel) | ✅ | ✅ | 完整支持，自包含 LLVM 库 |
| **macOS** | ARM64 (Apple Silicon) | ✅ | ✅ | 完整支持，自包含 LLVM 库 |
| **Windows** | x86_64 | ✅ | ✅ | 完整支持，自包含 LLVM DLLs |
| **Windows** | x86 (32-bit) | ✅ | ❌ | 无 32 位 LLVM 库 |

**用户覆盖率**：
- 支持 LLVM 的平台：~95% 用户 (x86_64 + macOS)
- 仅 C 后端的平台：~5% 用户 (32位系统)

### 从源码编译

✅ **所有平台都可以获得 LLVM 支持**

只需在目标设备上：
1. 安装 Zig 0.15.1
2. 安装 LLVM 19.1.7
3. 编译 PawLang

```bash
# 在任何平台上
git clone https://github.com/pawlang-project/paw.git
cd paw

# 安装 LLVM (根据平台)
# Linux ARM32: apt-get install llvm-19-dev
# Linux i386: apt-get install llvm-19-dev

# 编译
zig build

# 现在您的 pawc 包含 LLVM 后端！
./zig-out/bin/pawc --backend=llvm
```

## 🔬 技术限制解释

### 为什么交叉编译无法包含 LLVM？

#### 问题 1: 架构不匹配

```
交叉编译场景:
┌─────────────────┐
│ x86_64 主机     │
│ ↓ 交叉编译      │
│ → ARM 二进制    │  ← 需要链接 ARM LLVM 库
└─────────────────┘
        ↓
   ❌ 但只有 x86_64 LLVM 库
   ❌ 链接器错误: incompatible with armelf_linux_eabi
```

#### 问题 2: 多架构包不可用

Ubuntu 24.04 不提供 32 位多架构包：
- `apt-get install llvm-19-dev:armhf` → 404 Not Found
- `apt-get install llvm-19-dev:i386` → 404 Not Found

#### 问题 3: Zig 预编译版本限制

Zig 0.15.1 没有提供 arm/x86 的预编译下载：
- `zig-linux-arm-0.15.1.tar.xz` → 404 Not Found
- `zig-linux-x86-0.15.1.tar.xz` → 404 Not Found

这使得无法在 CI 中为这些架构进行原生构建。

## 💡 C 后端增强建议

### 方案：打包 GCC 以提高兼容性

对于仅支持 C 后端的平台，可以考虑打包轻量级 C 编译器：

```yaml
# 在 release.yml 中
- name: Package with TinyCC (optional)
  run: |
    # TinyCC 是一个非常小的 C 编译器 (~100KB)
    apt-get install -y tcc
    cp /usr/bin/tcc zig-out/bin/
    
    # 现在用户可以:
    # ./bin/pawc code.paw --backend=c
    # ./bin/tcc output.c -o program
    # ./program
```

**优点**：
- ✅ 用户可以直接编译和运行
- ✅ TinyCC 非常小 (~100KB)
- ✅ 无需系统 GCC

**缺点**：
- ⚠️ TinyCC 优化较少
- ⚠️ 增加包大小
- ⚠️ 额外的依赖管理

### 替代方案：文档说明

在文档中提供清晰的 C 代码编译说明：

```markdown
## 使用 C 后端

PawLang C 后端生成标准 C 代码，可用任何 C 编译器编译：

### Linux/macOS
```bash
# 生成 C 代码
./bin/pawc program.paw --backend=c

# 使用系统 GCC 编译
gcc output.c -o program

# 运行
./program
```

### Windows
```cmd
REM 生成 C 代码
bin\pawc.exe program.paw --backend=c

REM 使用 MinGW/MSVC 编译
gcc output.c -o program.exe

REM 运行
program.exe
```
```

## 📊 性能对比

### LLVM 后端 vs C 后端

| 特性 | LLVM 后端 | C 后端 |
|------|----------|--------|
| **编译速度** | 快 | 中等 (需要 C 编译器) |
| **运行性能** | 优秀 (LLVM 优化) | 优秀 (GCC -O3 优化) |
| **可移植性** | 受限于 LLVM | 极好 (任何有 C 编译器的平台) |
| **二进制大小** | 中等 | 小 |
| **调试能力** | LLVM IR | C 代码 (更易读) |

### 推荐使用场景

**使用 LLVM 后端**：
- ✅ 需要最佳性能
- ✅ 在支持的平台上 (x86_64, macOS, Windows x64)
- ✅ 需要 LLVM 优化级别 (-O0/-O1/-O2/-O3)

**使用 C 后端**：
- ✅ 需要最大可移植性
- ✅ 在 32 位平台上 (x86, armv7)
- ✅ 需要调试生成的代码
- ✅ 目标平台没有 LLVM

## 🚀 未来改进

### 可能的增强

1. **静态链接 LLVM** 📦
   - 将 LLVM 静态链接到编译器中
   - 二进制更大 (~100MB) 但完全自包含
   - 所有平台都能有 LLVM

2. **JIT 后端** ⚡
   - 直接执行代码，无需编译
   - 适合脚本和快速原型

3. **WebAssembly 后端** 🌐
   - 编译到 WASM
   - 在浏览器中运行

4. **自定义后端** 🔧
   - 直接生成机器码
   - 完全控制代码生成

## 📝 开发者注意事项

### 为新平台添加 LLVM 支持

如果您想为特定平台添加 LLVM 支持：

1. **确保该平台有 LLVM 19.1.7**
2. **修改 build.zig 检测逻辑**
3. **在 CI 中添加测试**
4. **更新文档**

### 条件编译示例

```zig
// src/main.zig
const build_options = @import("build_options");

if (build_options.llvm_native_available) {
    // LLVM 可用
    std.debug.print("LLVM backend available\n", .{});
} else {
    // 仅 C 后端
    std.debug.print("C backend only\n", .{});
}
```

## 🔧 C 后端编译器要求

PawLang 的 C 后端生成标准 C 代码，需要系统 C 编译器来编译：

### 编译器优先级:
1. **GCC** (首选) - `gcc output.c -o program`
2. **Clang** (备选) - `clang output.c -o program`

### 安装 GCC:

**Linux**:
```bash
sudo apt-get install gcc
```

**macOS**:
```bash
brew install gcc
# 或使用 Xcode Clang
xcode-select --install
```

**Windows**:
```bash
# MinGW (推荐)
choco install mingw

# 或使用 MSVC
cl output.c /Fe:program.exe
```

### 为何不自包含 GCC?

- **包大小**: GCC 及其依赖非常大（数百 MB）
- **系统集成**: 大多数开发环境已安装 GCC/Clang
- **标准 C**: 生成的代码可在任何系统上编译

对于 C-only 平台（Linux x86/armv7, Windows x86），用户需要：
- 安装系统 GCC/Clang 编译生成的 C 代码
- 或在目标设备上从源码编译 PawLang（获得 LLVM 支持）

## 📚 相关文档

- [README.md](../README.md) - 快速开始
- [QUICKSTART.md](QUICKSTART.md) - 详细教程
- [VERSION_REQUIREMENTS.md](VERSION_REQUIREMENTS.md) - 版本要求
- [ARCHITECTURE_SUPPORT.md](../ARCHITECTURE_SUPPORT.md) - 平台支持

---

**Built with ❤️ using Zig and LLVM**

