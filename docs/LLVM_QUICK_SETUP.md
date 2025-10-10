# LLVM 快速设置指南

最简单的 LLVM 设置方式，让你在 **10 分钟内**开始使用 PawLang LLVM 后端！

## 🚀 一行命令完成设置

### macOS / Linux

```bash
python3 scripts/download_llvm_prebuilt.py && zig build
```

### Windows

```powershell
python scripts\download_llvm_prebuilt.py; zig build
```

就这么简单！✨

## 📋 详细步骤

### 步骤 1: 下载预编译 LLVM

```bash
python3 scripts/download_llvm_prebuilt.py
```

脚本会自动：
- ✅ 检测你的操作系统和架构
- ✅ 从 GitHub 下载对应版本（~300 MB）
- ✅ 解压到 `llvm/install/` 目录
- ✅ 验证安装

**预计时间**: 5-10 分钟（取决于网速）

### 步骤 2: 构建 PawLang

```bash
zig build
```

**预计时间**: < 1 分钟

### 步骤 3: 测试 LLVM 后端

```bash
# 生成 LLVM IR
./zig-out/bin/pawc examples/hello.paw --backend=llvm

# 编译到可执行文件
llvm/install/bin/clang output.ll -o hello

# 运行
./hello
```

输出:
```
Hello, PawLang! 🐾
```

## ✅ 验证安装

检查 LLVM 是否正确安装：

```bash
# 查看 Clang 版本
llvm/install/bin/clang --version

# 应该输出类似:
# clang version 19.1.7
# Target: arm64-apple-darwin23.0.0
```

## 🎯 完整示例

创建一个测试文件 `test.paw`:

```paw
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

fn main() -> i32 {
    let result = add(10, 20);
    println("Result: $result");
    return 0;
}
```

编译并运行：

```bash
# 使用 LLVM 后端
./zig-out/bin/pawc test.paw --backend=llvm

# 编译 LLVM IR
llvm/install/bin/clang output.ll -o test

# 运行
./test
```

输出：
```
Result: 30
```

## 💡 实用技巧

### 技巧 1: 一行命令编译和运行

```bash
./zig-out/bin/pawc test.paw --backend=llvm && llvm/install/bin/clang output.ll -o test && ./test
```

### 技巧 2: 查看生成的 LLVM IR

```bash
# 生成 LLVM IR
./zig-out/bin/pawc examples/hello.paw --backend=llvm

# 查看 IR 代码
cat output.ll
```

## 🐛 常见问题

### Q: 下载速度慢？

A: 使用代理或 VPN。或者手动下载：
1. 访问: https://github.com/pawlang-project/llvm-build/releases/tag/19.1.7
2. 下载对应平台的文件
3. 解压到 `llvm/install/`

### Q: 提示找不到 Python？

**macOS / Linux**:
```bash
# 检查 Python
python3 --version

# 如果没有，安装:
# macOS
brew install python

# Ubuntu/Debian
sudo apt install python3
```

**Windows**:
```powershell
# 检查 Python
python --version

# 如果没有，安装:
choco install python
# 或从 https://www.python.org/downloads/ 下载
```

### Q: 提示找不到 Zig？

```bash
# 检查 Zig
zig version

# 如果没有，访问: https://ziglang.org/download/
```

### Q: 已经安装了系统 LLVM，会冲突吗？

A: 不会。本地 LLVM 安装在 `llvm/install/` 目录，不影响系统 LLVM。

### Q: 想切换回 C 后端？

```bash
# C 后端（默认）
./zig-out/bin/pawc test.paw
# 或明确指定
./zig-out/bin/pawc test.paw --backend=c

# 编译生成的 C 代码
gcc output.c -o test
./test
```

## 🎓 下一步

恭喜！你已经成功设置 LLVM 后端。接下来可以：

1. **学习 PawLang**: 查看 [Quick Start Guide](QUICKSTART.md)
2. **查看示例**: 浏览 `examples/` 目录
3. **深入 LLVM**: 阅读 [LLVM Prebuilt Guide](LLVM_PREBUILT_GUIDE.md)
4. **对比后端**: 查看 [LLVM Setup Comparison](LLVM_SETUP_COMPARISON.md)

## 📚 相关文档

- [LLVM_PREBUILT_GUIDE.md](LLVM_PREBUILT_GUIDE.md) - 详细的预编译指南
- [LLVM_SETUP_COMPARISON.md](LLVM_SETUP_COMPARISON.md) - 预编译 vs 源码对比
- [LLVM_BUILD_GUIDE.md](LLVM_BUILD_GUIDE.md) - 源码编译指南
- [QUICKSTART.md](QUICKSTART.md) - PawLang 快速入门
- [README.md](../README.md) - 项目主页

## 🏁 总结

使用预编译 LLVM，你只需要：

```bash
# 1. 一行命令下载 LLVM
python3 scripts/download_llvm_prebuilt.py

# 2. 构建 PawLang
zig build

# 3. 开始使用！
./zig-out/bin/pawc your_program.paw --backend=llvm
```

**总时间**: ~10 分钟

**占用空间**: ~500 MB

**难度**: ⭐ 超简单

---

**Happy Coding with PawLang! 🐾**

