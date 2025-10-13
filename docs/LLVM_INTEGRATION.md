# 将LLVM嵌入到PawLang编译器

**目标**: 让pawc编译器使用预编译的多平台LLVM工具链

---

## 🎯 集成方案

### 方案1: 使用GitHub发布的预编译LLVM（推荐）⭐

**优势**:
- ✅ 无需用户安装LLVM
- ✅ 支持13个平台
- ✅ 开箱即用
- ✅ 可分发

---

## 📦 快速集成步骤

### Step 1: 下载对应平台的LLVM

```bash
# macOS ARM64
wget https://github.com/pawlang-project/llvm-build/releases/download/llvm-21.1.3/llvm-21.1.3-macos-aarch64.tar.gz
tar xzf llvm-21.1.3-macos-aarch64.tar.gz

# Linux x86_64
wget https://github.com/pawlang-project/llvm-build/releases/download/llvm-21.1.3/llvm-21.1.3-linux-x86_64.tar.gz
tar xzf llvm-21.1.3-linux-x86_64.tar.gz
```

### Step 2: 放置到项目中

```bash
# 在PawLang项目中创建llvm目录
mkdir -p vendor/llvm/macos-aarch64
mv install vendor/llvm/macos-aarch64/

# 或直接解压到vendor
tar xzf llvm-21.1.3-macos-aarch64.tar.gz -C vendor/llvm/macos-aarch64/
```

### Step 3: 更新build.zig

修改LLVM检测逻辑，优先使用vendor目录：

```zig
// 在build.zig的LLVM检测部分添加
const vendor_llvm_path = switch (target.result.os.tag) {
    .macos => if (target.result.cpu.arch == .aarch64)
        "vendor/llvm/macos-aarch64/install"
    else
        "vendor/llvm/macos-x86_64/install",
    .linux => "vendor/llvm/linux-x86_64/install",
    .windows => "vendor/llvm/windows-x86_64/install",
    else => null,
};

// 检查vendor LLVM
const has_vendor_llvm = if (vendor_llvm_path) |path| blk: {
    const llvm_lib_path = b.fmt("{s}/lib", .{path});
    std.fs.accessAbsolute(llvm_lib_path, .{}) catch {
        break :blk false;
    };
    break :blk true;
} else false;

// 如果有vendor LLVM，使用它
if (has_vendor_llvm) {
    const vendor_path = vendor_llvm_path.?;
    const llvm_include = b.fmt("{s}/include", .{vendor_path});
    const llvm_lib = b.fmt("{s}/lib", .{vendor_path});
    
    exe.addIncludePath(.{ .cwd_relative = llvm_include });
    exe.addLibraryPath(.{ .cwd_relative = llvm_lib });
    exe.linkSystemLibrary("LLVM-C");
    exe.linkLibCpp();
    
    std.debug.print("✅ Using vendor LLVM: {s}\n", .{vendor_path});
}
```

---

## 🚀 更简单的方案：使用环境变量

### Step 1: 下载并解压LLVM

```bash
# 下载当前平台的LLVM
wget https://github.com/pawlang-project/llvm-build/releases/download/llvm-21.1.3/llvm-21.1.3-$(uname -s | tr '[:upper:]' '[:lower:]')-$(uname -m).tar.gz

# 解压到用户目录
mkdir -p ~/.local/llvm
tar xzf llvm-21.1.3-*.tar.gz -C ~/.local/llvm/

# 或解压到系统目录
sudo tar xzf llvm-21.1.3-*.tar.gz -C /usr/local/
```

### Step 2: 设置环境变量

```bash
# macOS/Linux
export LLVM_PATH="$HOME/.local/llvm/install"
export PATH="$LLVM_PATH/bin:$PATH"
export LD_LIBRARY_PATH="$LLVM_PATH/lib:$LD_LIBRARY_PATH"

# 永久设置
echo 'export LLVM_PATH="$HOME/.local/llvm/install"' >> ~/.bashrc
echo 'export PATH="$LLVM_PATH/bin:$PATH"' >> ~/.bashrc
echo 'export LD_LIBRARY_PATH="$LLVM_PATH/lib:$LD_LIBRARY_PATH"' >> ~/.bashrc
```

### Step 3: 编译pawc

```bash
# build.zig会自动检测LLVM_PATH环境变量
zig build

# 或显式指定LLVM路径
zig build -DLLVM_PATH=$HOME/.local/llvm/install
```

---

## 🎯 推荐方案：打包发布带LLVM的pawc

### 方案A: 完整打包（推荐用户）

创建包含LLVM的完整发布包：

```bash
#!/bin/bash
# create_release.sh

PLATFORM=$(uname -s | tr '[:upper:]' '[:lower:]')
ARCH=$(uname -m)

# 1. 下载对应平台的LLVM
wget https://github.com/pawlang-project/llvm-build/releases/download/llvm-21.1.3/llvm-21.1.3-${PLATFORM}-${ARCH}.tar.gz

# 2. 解压
tar xzf llvm-21.1.3-${PLATFORM}-${ARCH}.tar.gz

# 3. 编译pawc
zig build

# 4. 打包
mkdir -p pawlang-release/bin
mkdir -p pawlang-release/lib
cp zig-out/bin/pawc pawlang-release/bin/
cp -r install/lib/* pawlang-release/lib/
cp -r examples pawlang-release/
cp README.md USAGE.md pawlang-release/

# 5. 创建启动脚本
cat > pawlang-release/pawc << 'SCRIPT'
#!/bin/bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib:$LD_LIBRARY_PATH"
exec "$DIR/bin/pawc" "$@"
SCRIPT
chmod +x pawlang-release/pawc

# 6. 打包
tar czf pawlang-${PLATFORM}-${ARCH}.tar.gz pawlang-release/

echo "✅ 创建发布包: pawlang-${PLATFORM}-${ARCH}.tar.gz"
```

**用户使用**:
```bash
tar xzf pawlang-linux-x86_64.tar.gz
cd pawlang-release
./pawc hello.paw
```

---

## 🔧 方案B: 轻量级（推荐开发者）

让用户自己下载LLVM：

```bash
# 在README.md中说明
## 安装LLVM

下载对应平台的LLVM:
https://github.com/pawlang-project/llvm-build/releases

export PATH="/path/to/llvm/install/bin:$PATH"

然后编译pawc:
zig build
```

---

## 📝 当前build.zig的LLVM检测

当前build.zig已经支持：

1. **自动检测系统LLVM**
   - macOS: `/opt/homebrew/opt/llvm@19/`
   - Linux: `/usr/lib/llvm-19/`
   - Windows: `C:\Program Files\LLVM\`

2. **动态链接**
   - 使用`llvm-config`获取链接参数
   - 自动添加include和library路径

3. **库复制**
   - macOS: 复制dylib到`zig-out/lib/`
   - Linux: 复制so到`zig-out/lib/`
   - Windows: 复制dll到`zig-out/bin/`

---

## 🎯 建议的改进

### 1. 添加vendor LLVM检测

在`build.zig`的第30行后添加:

```zig
// 🆕 检查vendor目录中的LLVM
const vendor_llvm_base = "vendor/llvm";
const vendor_llvm_path = switch (target.result.os.tag) {
    .macos => if (target.result.cpu.arch == .aarch64)
        vendor_llvm_base ++ "/macos-aarch64/install"
    else
        vendor_llvm_base ++ "/macos-x86_64/install",
    .linux => vendor_llvm_base ++ "/linux-x86_64/install",
    .windows => vendor_llvm_base ++ "/windows-x86_64/install",
    else => null,
};

// 优先使用vendor LLVM
const llvm_config_path = blk: {
    if (!enable_llvm) break :blk null;
    
    // 1. 检查vendor LLVM
    if (vendor_llvm_path) |vpath| {
        const vendor_bin = b.fmt("{s}/bin/llvm-config", .{vpath});
        std.fs.accessAbsolute(vendor_bin, .{}) catch {
            // vendor LLVM不存在，继续检查系统LLVM
        } else {
            std.debug.print("✅ Using vendor LLVM: {s}\n", .{vpath});
            break :blk vendor_bin;
        }
    }
    
    // 2. 检查系统LLVM（现有代码）
    for (llvm_config_paths) |path| {
        // ... 现有检测逻辑
    }
    break :blk null;
};
```

### 2. 添加LLVM_PATH环境变量支持

```zig
// 支持LLVM_PATH环境变量
const env_llvm_path = std.process.getEnvVarOwned(b.allocator, "LLVM_PATH") catch null;
if (env_llvm_path) |llvm_path| {
    defer b.allocator.free(llvm_path);
    std.debug.print("✅ Using LLVM from LLVM_PATH: {s}\n", .{llvm_path});
    
    const llvm_inc = b.fmt("{s}/include", .{llvm_path});
    const llvm_lib = b.fmt("{s}/lib", .{llvm_path});
    
    exe.addIncludePath(.{ .cwd_relative = llvm_inc });
    exe.addLibraryPath(.{ .cwd_relative = llvm_lib });
}
```

---

## 💡 推荐的完整集成方案

### 目录结构

```
PawLang/
├── vendor/
│   └── llvm/
│       ├── download.sh          # 下载脚本
│       ├── macos-aarch64/
│       │   └── install/         # 解压的LLVM
│       ├── macos-x86_64/
│       ├── linux-x86_64/
│       └── windows-x86_64/
├── src/
├── build.zig                   # 更新LLVM检测
└── README.md
```

### 下载脚本

创建 `vendor/llvm/download.sh`:

```bash
#!/bin/bash
# 自动下载当前平台的LLVM

PLATFORM=$(uname -s | tr '[:upper:]' '[:lower:]')
ARCH=$(uname -m)

case "$PLATFORM-$ARCH" in
  darwin-arm64)
    TARGET="macos-aarch64"
    ;;
  darwin-x86_64)
    TARGET="macos-x86_64"
    ;;
  linux-x86_64)
    TARGET="linux-x86_64"
    ;;
  linux-aarch64)
    TARGET="linux-aarch64"
    ;;
  *)
    echo "❌ 不支持的平台: $PLATFORM-$ARCH"
    exit 1
    ;;
esac

URL="https://github.com/pawlang-project/llvm-build/releases/download/llvm-21.1.3/llvm-21.1.3-${TARGET}.tar.gz"

echo "📥 下载 LLVM 21.1.3 for $TARGET..."
wget -O llvm.tar.gz "$URL"

echo "📦 解压..."
mkdir -p "$TARGET"
tar xzf llvm.tar.gz -C "$TARGET"/

rm llvm.tar.gz

echo "✅ LLVM已安装到: vendor/llvm/$TARGET/install/"
echo ""
echo "现在可以编译pawc:"
echo "  zig build"
```

---

## 🚀 用户使用流程

### 方法1: 自动下载LLVM

```bash
# 克隆PawLang
git clone https://github.com/pawlang-project/PawLang.git
cd PawLang

# 下载LLVM
cd vendor/llvm
./download.sh
cd ../..

# 编译
zig build

# 使用
./zig-out/bin/pawc hello.paw --backend=llvm
```

### 方法2: 手动指定LLVM路径

```bash
# 下载LLVM到任意位置
wget https://github.com/pawlang-project/llvm-build/releases/download/llvm-21.1.3/llvm-21.1.3-linux-x86_64.tar.gz
tar xzf llvm-21.1.3-linux-x86_64.tar.gz -C ~/

# 设置环境变量
export LLVM_PATH="$HOME/install"

# 编译pawc
zig build

# pawc会自动找到LLVM
```

---

## 🔧 build.zig完整改进

```zig
// 在build.zig开头添加
const VENDOR_LLVM_VERSION = "21.1.3";

// LLVM检测优先级:
// 1. 环境变量 LLVM_PATH
// 2. vendor/llvm/{platform}/install
// 3. 系统安装的LLVM

fn detectLLVM(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    enable_llvm: bool
) ?[]const u8 {
    if (!enable_llvm) return null;
    
    // 1. 检查LLVM_PATH环境变量
    if (std.process.getEnvVarOwned(b.allocator, "LLVM_PATH")) |llvm_path| {
        std.debug.print("✅ Using LLVM_PATH: {s}\n", .{llvm_path});
        return llvm_path;
    } else |_| {}
    
    // 2. 检查vendor目录
    const platform_name = getPlatformName(target);
    const vendor_path = b.fmt("vendor/llvm/{s}/install", .{platform_name});
    
    std.fs.accessAbsolute(vendor_path, .{}) catch {
        // vendor不存在，继续
    } else {
        std.debug.print("✅ Using vendor LLVM: {s}\n", .{vendor_path});
        return vendor_path;
    }
    
    // 3. 检查系统LLVM（现有逻辑）
    // ... 现有的llvm-config检测代码
    
    return null;
}

fn getPlatformName(target: std.Build.ResolvedTarget) []const u8 {
    return switch (target.result.os.tag) {
        .macos => if (target.result.cpu.arch == .aarch64)
            "macos-aarch64"
        else
            "macos-x86_64",
        .linux => switch (target.result.cpu.arch) {
            .x86_64 => "linux-x86_64",
            .aarch64 => "linux-aarch64",
            .arm => "linux-arm",
            .riscv64 => "linux-riscv64",
            else => "linux-x86_64",
        },
        .windows => if (target.result.cpu.arch == .aarch64)
            "windows-aarch64"
        else
            "windows-x86_64",
        else => "unknown",
    };
}
```

---

## 📦 分发方案

### 选项1: 完整打包（含LLVM）

**优点**: 用户开箱即用  
**缺点**: 包体积大（~600MB压缩后）

```bash
# 打包脚本
tar czf pawlang-with-llvm-macos-arm64.tar.gz \
  zig-out/bin/pawc \
  vendor/llvm/macos-aarch64/install/ \
  examples/ \
  README.md \
  USAGE.md
```

### 选项2: 分离打包（推荐）⭐

**pawc包** (小，~5MB):
- 只包含编译器
- C backend开箱即用
- LLVM backend需额外下载

**LLVM包** (大，~600MB):
- 从GitHub Releases下载
- 用户按需安装

**使用方式**:

```bash
# 安装pawc（轻量）
wget https://github.com/pawlang-project/PawLang/releases/pawc-linux-x86_64.tar.gz
tar xzf pawc-linux-x86_64.tar.gz

# 使用C backend（无需LLVM）
./pawc hello.paw

# 可选：安装LLVM支持
wget https://github.com/pawlang-project/llvm-build/releases/download/llvm-21.1.3/llvm-21.1.3-linux-x86_64.tar.gz
tar xzf llvm-21.1.3-linux-x86_64.tar.gz
export LLVM_PATH="$PWD/install"

# 使用LLVM backend
./pawc hello.paw --backend=llvm
```

---

## 🎯 最佳实践

### 1. 在README中说明

```markdown
## 安装

### 快速开始（C backend）
下载pawc即可使用，无需额外依赖。

### 可选：启用LLVM backend
下载对应平台的LLVM:
https://github.com/pawlang-project/llvm-build/releases

export LLVM_PATH=/path/to/llvm/install
```

### 2. 提供setup脚本

```bash
#!/bin/bash
# setup.sh

echo "PawLang Setup"
echo ""
read -p "需要LLVM支持吗？[y/N]: " answer

if [ "$answer" = "y" ]; then
    ./vendor/llvm/download.sh
    echo "✅ LLVM已安装"
fi

zig build
echo "✅ pawc编译完成"
```

---

## 📊 各方案对比

| 方案 | 包大小 | 安装复杂度 | LLVM支持 | 推荐 |
|------|-------|-----------|---------|------|
| 完整打包 | ~600MB | 低 | ✅ | 一般用户 |
| 分离打包 | ~5MB | 中 | 按需 | ⭐ 推荐 |
| 仅C backend | ~5MB | 低 | ❌ | 轻量用户 |
| 依赖系统LLVM | ~5MB | 高 | ✅ | 开发者 |

---

## 💡 建议

**对于PawLang**:

1. **主发布**: 分离打包（pawc小包 + 可选LLVM）
2. **文档**: 清晰说明如何启用LLVM
3. **vendor**: 提供自动下载脚本

**优势**:
- ✅ 快速下载（主包5MB）
- ✅ 按需安装LLVM
- ✅ 支持13个平台
- ✅ 统一的LLVM版本

---

**下一步**: 要我帮你实现这个集成方案吗？ 🚀

