# 将 LLVM 集成到项目 vendor 目录

**目标**: 下载预编译的 LLVM 到 `vendor/llvm/`，实现项目完全自包含

---

## 🎯 方案说明

### 为什么放在 vendor？

```
PawLang/
├── vendor/
│   └── llvm/              # 预编译的 LLVM（不在 git 中）
│       ├── lib/           # 库文件
│       ├── include/       # 头文件
│       └── bin/           # 工具
├── src/
└── build.zig
```

**优势**:
- ✅ 项目自包含（克隆后只需下载一次）
- ✅ 不污染系统
- ✅ 版本锁定（LLVM 19）
- ✅ 团队统一版本

---

## 📥 下载方法

### 方法 1: 使用脚本（推荐）

```bash
# 运行下载脚本
chmod +x scripts/download_llvm.sh
./scripts/download_llvm.sh
```

脚本会：
1. 检测平台（macOS arm64/x86_64, Linux x86_64）
2. 下载对应的预编译包
3. 解压到 `vendor/llvm/`
4. 验证安装

### 方法 2: 手动下载

#### macOS ARM64 (Apple Silicon)

```bash
# 1. 访问 LLVM releases
open https://github.com/llvm/llvm-project/releases/tag/llvmorg-19.1.3

# 2. 下载文件
#    clang+llvm-19.1.3-arm64-apple-darwin22.0.tar.xz
#    大小: ~220MB

# 3. 解压到项目
cd /Users/haojunhuang/RustroverProjects/PawLang
mkdir -p vendor
tar xf ~/Downloads/clang+llvm-19.1.3-*.tar.xz -C vendor/
mv vendor/clang+llvm-* vendor/llvm

# 4. 验证
ls vendor/llvm/
```

#### macOS x86_64 (Intel)

下载: `clang+llvm-19.1.3-x86_64-apple-darwin.tar.xz`

#### Linux x86_64

下载: `clang+llvm-19.1.3-x86_64-linux-gnu-ubuntu-22.04.tar.xz`

---

## 🔧 配置 build.zig

下载后，需要配置构建系统使用 vendor 中的 LLVM：

```zig
// build.zig
pub fn build(b: *std.Build) void {
    // ...
    
    // 🆕 使用 vendor 中的 LLVM
    if (b.option(bool, "with-llvm", "Enable native LLVM backend") orelse false) {
        const llvm_dep = b.dependency("llvm", .{
            .target = target,
            .optimize = optimize,
        });
        const llvm_mod = llvm_dep.module("llvm");
        exe.root_module.addImport("llvm", llvm_mod);
        
        // 指向 vendor/llvm
        exe.addLibraryPath(.{ .cwd_relative = "vendor/llvm/lib" });
        exe.addIncludePath(.{ .cwd_relative = "vendor/llvm/include" });
        exe.linkSystemLibrary("LLVM");
        
        std.debug.print("✓ LLVM native backend enabled (vendor)\n", .{});
    }
    
    // ...
}
```

---

## 📊 空间占用

| 项 | 大小 |
|-----|------|
| 下载文件 (.tar.xz) | ~220MB |
| 解压后 (vendor/llvm) | ~900MB |
| **项目总大小** | **~905MB** |

**建议**: 添加到 `.gitignore`，不提交到 git

```bash
# .gitignore
vendor/llvm/
vendor/*.tar.xz
```

---

## 🚀 完整使用流程

### 首次设置

```bash
# 1. 克隆项目
git clone <your-repo>
cd PawLang

# 2. 下载 LLVM（一次性）
./scripts/download_llvm.sh
# 或手动下载到 vendor/llvm/

# 3. 编译（启用LLVM）
zig build -Dwith-llvm=true
```

### 日常开发

```bash
# 默认模式（文本IR，不需要vendor/llvm）
zig build
./zig-out/bin/pawc hello.paw --backend=llvm

# 原生模式（需要vendor/llvm）
zig build -Dwith-llvm=true
./zig-out/bin/pawc hello.paw --backend=llvm-native
```

---

## ⚙️ CI/CD 配置

### GitHub Actions

```yaml
# .github/workflows/build.yml
name: Build

on: [push, pull_request]

jobs:
  build:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Setup Zig
        uses: goto-bus-stop/setup-zig@v2
        with:
          version: 0.14.0
      
      - name: Cache LLVM
        uses: actions/cache@v3
        with:
          path: vendor/llvm
          key: llvm-19-${{ runner.os }}-${{ runner.arch }}
      
      - name: Download LLVM
        run: |
          if [ ! -d "vendor/llvm" ]; then
            ./scripts/download_llvm.sh
          fi
      
      - name: Build (text mode)
        run: zig build
      
      - name: Build (native mode)
        run: zig build -Dwith-llvm=true
      
      - name: Test
        run: |
          ./zig-out/bin/pawc tests/llvm_hello.paw --backend=llvm
          clang output.ll -o test && ./test
```

---

## 🔍 验证安装

### 检查 LLVM

```bash
# 检查是否正确安装
ls -lh vendor/llvm/
# 应该看到:
# bin/      - LLVM 工具
# lib/      - 库文件
# include/  - 头文件

# 检查版本
vendor/llvm/bin/llvm-config --version
# 应该输出: 19.1.3

# 检查库
ls vendor/llvm/lib/libLLVM*
```

### 测试编译

```bash
# 测试能否链接
zig build -Dwith-llvm=true

# 如果成功，应该看到:
✓ LLVM native backend enabled (vendor)
```

---

## 📋 团队协作

### .gitignore 配置

```bash
# 添加到 .gitignore
vendor/llvm/
vendor/*.tar.xz
vendor/*.tar.gz
```

### README 说明

```markdown
## 设置开发环境

1. 克隆仓库
2. 下载 LLVM（可选，用于原生模式）:
   ```bash
   ./scripts/download_llvm.sh
   ```
3. 编译:
   ```bash
   zig build              # 文本模式
   zig build -Dwith-llvm  # 原生模式（需要步骤2）
   ```
```

---

## ⚠️ 注意事项

### 磁盘空间

预编译 LLVM 需要 ~900MB，确保有足够空间。

### 网络

首次下载需要约 220MB 流量。

### 平台

目前支持：
- ✅ macOS ARM64 (Apple Silicon)
- ✅ macOS x86_64 (Intel)
- ✅ Linux x86_64

其他平台需要自行编译 LLVM。

---

## 💡 推荐策略

### 对于开发者

**选项 A**: 文本模式（推荐）
- 不下载 LLVM
- 快速开发
- 足够使用

**选项 B**: 原生模式（高级）
- 下载 LLVM 到 vendor
- 更好的性能
- 更多控制

### 对于用户

分发两个版本：
- **pawc-lite**: 文本模式（5MB）
- **pawc-full**: 捆绑LLVM（200MB）

---

## 🔗 下载链接

### LLVM 19.1.3 Releases

主页: https://github.com/llvm/llvm-project/releases/tag/llvmorg-19.1.3

**macOS ARM64**:  
`clang+llvm-19.1.3-arm64-apple-darwin22.0.tar.xz` (~220MB)

**macOS x86_64**:  
`clang+llvm-19.1.3-x86_64-apple-darwin.tar.xz` (~230MB)

**Linux x86_64**:  
`clang+llvm-19.1.3-x86_64-linux-gnu-ubuntu-22.04.tar.xz` (~260MB)

---

## ✅ 总结

**当前最佳方案**: 

1. **默认**: 使用文本 LLVM IR（不下载 LLVM）
2. **可选**: 下载 LLVM 到 vendor（~900MB）
3. **未来**: 提供预编译版本（lite + full）

**v0.1.4 推荐**: 继续使用文本模式，性能已经足够好！

