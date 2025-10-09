# 🏗️ 使用本地 LLVM 源码构建指南

**版本**: LLVM 19.1.6  
**位置**: `llvm/19.1.6/`  
**大小**: 1.9GB（源码）

---

## ✅ 您已经完成

✓ LLVM 19.1.6 源码已放置在 `llvm/19.1.6/`

---

## 🚀 快速构建（推荐）

### 步骤 1: 安装构建工具

```bash
# macOS
brew install cmake ninja

# Ubuntu/Debian
sudo apt install cmake ninja-build
```

### 步骤 2: 构建 LLVM

```bash
# 运行构建脚本（需要30-60分钟）
./scripts/build_llvm.sh
```

脚本会：
1. 配置 CMake（只编译需要的部分）
2. 使用 Ninja 并行构建
3. 安装到 `llvm/install/`
4. 验证安装

### 步骤 3: 构建 PawLang

```bash
# 使用本地 LLVM 编译
zig build -Dwith-llvm=true
```

build.zig 会自动检测并使用 `llvm/install/`

---

## 📊 构建配置

### 优化后的配置

脚本使用的 CMake 配置：

```cmake
-DCMAKE_BUILD_TYPE=Release          # 发布版本
-DLLVM_ENABLE_PROJECTS=clang        # 只编译LLVM和Clang
-DLLVM_TARGETS_TO_BUILD=AArch64;X86 # 只编译ARM64和x86
-DLLVM_BUILD_TESTS=OFF              # 不编译测试
-DLLVM_BUILD_EXAMPLES=OFF           # 不编译示例
```

**好处**:
- ✅ 编译时间减少 50%（30分钟 vs 60分钟）
- ✅ 磁盘占用减少 60%（2GB vs 5GB）
- ✅ 只包含PawLang需要的部分

---

## 📂 目录结构

```
llvm/
├── 19.1.6/              # 源码（1.9GB，您已添加）
│   ├── llvm/
│   ├── clang/
│   └── ...
├── build/               # 构建目录（自动生成，~3GB）
│   └── ...
└── install/             # 安装目录（自动生成，~2GB）
    ├── bin/
    │   ├── llvm-config
    │   └── clang
    ├── lib/
    │   └── libLLVM*.a
    └── include/
        └── llvm-c/
```

**总大小**: ~7GB

---

## ⚙️ 手动构建（高级）

如果不想用脚本，可以手动操作：

```bash
# 1. 创建构建目录
mkdir -p llvm/build
cd llvm/build

# 2. 配置CMake
cmake ../19.1.6/llvm \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=../install \
    -DLLVM_ENABLE_PROJECTS=clang \
    -DLLVM_TARGETS_TO_BUILD="AArch64;X86" \
    -G Ninja

# 3. 构建
ninja -j$(sysctl -n hw.ncpu)

# 4. 安装
ninja install

# 5. 验证
../install/bin/llvm-config --version
```

---

## 🔧 集成到 PawLang

### build.zig 已配置

```zig
if (b.option(bool, "with-llvm", "...") orelse false) {
    const local_llvm = "llvm/install";
    
    // 自动检测本地LLVM
    if (std.fs.cwd().access(llvm_config_path, .{})) {
        exe.addLibraryPath(...);
        exe.addIncludePath(...);
        exe.linkSystemLibrary("LLVM");
    }
}
```

### 使用

```bash
# 编译 PawLang（使用本地LLVM）
zig build -Dwith-llvm=true

# 使用原生LLVM后端（未来功能）
./zig-out/bin/pawc hello.paw --backend=llvm-native
```

---

## ⏱️ 构建时间估算

### macOS (M1/M2/M3)

| 配置 | 时间 | 大小 |
|------|------|------|
| 完整构建 | 60分钟 | 5GB |
| 优化构建（推荐）| 30分钟 | 2GB |
| 最小构建 | 20分钟 | 1.5GB |

### 建议

- 使用 Ninja（比 Make 快 3倍）
- 并行构建（`-j$(nproc)`）
- 只编译需要的目标

---

## 📋 检查清单

### 构建前

- [ ] 安装 cmake（`brew install cmake`）
- [ ] 安装 ninja（`brew install ninja`）
- [ ] 确保有 7GB 磁盘空间
- [ ] LLVM 源码在 `llvm/19.1.6/`

### 构建中

- [ ] 运行 `./scripts/build_llvm.sh`
- [ ] 等待 30-60 分钟
- [ ] 不要中断构建

### 构建后

- [ ] 检查 `llvm/install/bin/llvm-config` 存在
- [ ] 运行 `llvm/install/bin/llvm-config --version`
- [ ] 查看大小 `du -sh llvm/install/`

---

## 🧪 测试

### 验证LLVM安装

```bash
# 检查版本
llvm/install/bin/llvm-config --version
# 应输出: 19.1.6

# 检查库
ls llvm/install/lib/libLLVM*

# 检查头文件
ls llvm/install/include/llvm-c/
```

### 测试 PawLang 集成

```bash
# 编译 PawLang（使用本地LLVM）
zig build -Dwith-llvm=true

# 应该看到:
✓ Using local LLVM from llvm/install

# 测试
./zig-out/bin/pawc tests/llvm_hello.paw --backend=llvm
```

---

## ⚠️ 常见问题

### Q: 构建失败怎么办？

**A**: 
```bash
# 清理重试
rm -rf llvm/build llvm/install
./scripts/build_llvm.sh
```

### Q: 磁盘空间不够

**A**: 使用预编译版本或文本模式：
- 文本模式：无需LLVM（5MB）
- Vendor模式：下载预编译（900MB）
- 源码构建：需要7GB

### Q: 构建太慢

**A**: 
- 减少目标：只编译 AArch64 或 X86
- 使用预编译版本
- 使用文本模式（推荐）

---

## 💡 推荐

### 对于大多数用户

**使用文本模式**（默认）
```bash
zig build
./zig-out/bin/pawc hello.paw --backend=llvm
```

不需要构建 LLVM，功能已经足够！

### 对于需要原生 API 的用户

**构建本地 LLVM**
```bash
./scripts/build_llvm.sh  # 一次性，30-60分钟
zig build -Dwith-llvm=true
```

获得完整的 LLVM 功能！

---

## 📚 参考

- `scripts/build_llvm.sh` - 自动构建脚本
- `VENDOR_LLVM_SETUP.md` - Vendor 方案
- `LLVM_README.md` - 总体说明

---

## ✅ 总结

**您已经有 LLVM 源码了！**

**下一步**:
1. 运行 `./scripts/build_llvm.sh` 编译（30-60分钟）
2. 或者继续使用文本模式（已经很好了）

**推荐**: v0.1.4 先使用文本模式，等 v0.2.0 需要高级功能时再构建 LLVM。

