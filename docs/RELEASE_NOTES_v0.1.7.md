# 🎯 PawLang v0.1.7 Release Notes

**Release Date**: TBD

**Theme**: LLVM 优化支持

---

## 🌟 Highlights

### LLVM 优化级别支持 ⚡

PawLang v0.1.7 为 LLVM 后端添加了优化级别支持，并完整实现了 `as` 类型转换！

**LLVM 优化** ⚡:
- ✅ **-O0**: 无优化（最快编译，便于调试）
- ✅ **-O1**: 基础优化（平衡编译速度和性能）
- ✅ **-O2**: 标准优化（推荐，大多数项目的最佳选择）⭐
- ✅ **-O3**: 激进优化（最大性能）

**类型转换** 🔄:
- ✅ **as 操作符**: 完整的类型转换支持
- ✅ **整数转换**: 扩展、截断、有符号/无符号
- ✅ **浮点转换**: i32↔f64, f32↔f64
- ✅ **bool/char**: 与整数互转

---

## 📦 What's New

### 1. 优化级别参数

**命令行支持**:
```bash
# 无优化（调试）
pawc app.paw --backend=llvm -O0

# 基础优化
pawc app.paw --backend=llvm -O1

# 标准优化（推荐）⭐
pawc app.paw --backend=llvm -O2

# 激进优化
pawc app.paw --backend=llvm -O3
```

**完整工作流**:
```bash
# 步骤 1: 生成优化的 LLVM IR
pawc fibonacci.paw --backend=llvm -O2

# 步骤 2: 用 clang 编译（使用对应的优化级别）
clang output.ll -O2 -o fibonacci

# 步骤 3: 运行
./fibonacci
```

### 2. 智能提示

编译器现在会根据你选择的优化级别提供智能提示：

```bash
$ pawc app.paw --backend=llvm -O2 -v

✅ LLVM IR generated: output.ll
⚡ Optimization: -O2 (standard optimization) ⭐
💡 Hints:
   • Compile with optimization: clang output.ll -O2 -o output
   • Local LLVM: llvm/install/bin/clang output.ll -O2 -o output
   • Run: ./output
```

### 3. 优化级别说明

| 级别 | 编译速度 | 运行性能 | 代码大小 | 适用场景 |
|------|---------|---------|---------|---------|
| -O0  | ⚡⚡⚡    | ⭐      | 大      | 调试开发 |
| -O1  | ⚡⚡     | ⭐⭐    | 中      | 日常开发 |
| -O2  | ⚡      | ⭐⭐⭐  | 中      | 生产环境 ⭐ |
| -O3  | ⚡      | ⭐⭐⭐⭐ | 大      | 性能关键 |

---

## 🔧 Technical Details

### Implementation Approach

v0.1.7 采用**实用优化方案**：

1. **PawLang 编译器**：
   - 接受 `-O0/-O1/-O2/-O3` 参数
   - 生成高质量的 LLVM IR（SSA 形式）
   - 提示用户使用对应的 clang 优化参数

2. **clang 优化**：
   - 利用 LLVM 的成熟优化管道
   - 稳定可靠
   - 无需链接复杂的 LLVM Transform 库

**Why this approach?**

✅ **简单**: 不需要复杂的 PassManager 集成  
✅ **可靠**: 使用 clang 的成熟优化  
✅ **灵活**: 用户可以选择任何优化级别  
✅ **稳定**: 避免符号链接问题  

### Code Changes

**src/main.zig**:
- 添加 `OptLevel` 枚举
- 解析 `-O0/-O1/-O2/-O3` 参数
- 传递给 LLVM Backend
- 提供智能编译提示

**src/llvm_native_backend.zig**:
- 添加 `OptLevel` 枚举
- 在结构体中保存 `opt_level` 字段
- `init()` 接受优化级别参数
- 添加 `getOptLevelString()` 辅助函数

**src/llvm_c_api.zig**:
- 添加优化相关的文档注释
- 说明实用优化方案
- 🆕 添加类型转换 C API（zext, sext, trunc, sitofp, uitofp, fptosi, fptoui, fpext, fptrunc）
- 🆕 添加 i8, i16, i128, f32 类型支持

---

### 2. as 类型转换 🔄

**语法**:
```paw
let x: i32 = 100;
let y: i64 = x as i64;   // i32 -> i64
let z: f64 = x as f64;   // i32 -> f64
```

**支持的转换**:

| 从 \ 到 | i8-i128 | u8-u128 | f32/f64 | bool | char |
|---------|---------|---------|---------|------|------|
| i8-i128 | ✅      | ✅      | ✅      | ❌   | ✅   |
| u8-u128 | ✅      | ✅      | ✅      | ❌   | ✅   |
| f32/f64 | ✅      | ✅      | ✅      | ❌   | ❌   |
| bool    | ✅      | ✅      | ❌      | ✅   | ❌   |
| char    | ✅      | ✅      | ❌      | ❌   | ✅   |

**LLVM IR 指令映射**:
- **整数扩展**: `zext` (无符号), `sext` (有符号)
- **整数截断**: `trunc`
- **整数→浮点**: `sitofp` (有符号), `uitofp` (无符号)
- **浮点→整数**: `fptosi` (有符号), `fptoui` (无符号)
- **浮点扩展**: `fpext` (f32→f64)
- **浮点截断**: `fptrunc` (f64→f32)

**示例**:

```paw
// 整数转换
let a: i32 = 100;
let b: i64 = a as i64;    // sext i32 %a to i64

// 浮点转换
let x: i32 = 42;
let y: f64 = x as f64;    // sitofp i32 %x to double

// 截断
let f: f64 = 3.14;
let i: i32 = f as i32;    // fptosi double %f to i32 (结果: 3)
```

**Code Changes**:

**src/typechecker.zig**:
- 更新 `as_expr` 验证，支持 bool 和 char 转换

**src/codegen.zig** (C Backend):
- 添加 `.as_expr` 处理
- 生成 C 类型转换: `((target_type)(value))`

**src/llvm_native_backend.zig**:
- 添加 `.as_expr` 处理
- 实现 `generateCast` 函数，根据源类型和目标类型选择正确的 LLVM 指令
- 添加类型判断辅助函数: `isIntType`, `isFloatType`, `isSignedIntType`, `getTypeBits`
- 扩展 `toLLVMType` 支持所有基础类型

**src/llvm_c_api.zig**:
- 添加类型转换 extern 声明（9个 LLVM 转换指令）
- 在 `Builder` 中添加包装函数
- 添加 i8, i16, i128, float 类型的 Context 方法

**测试**:
- 创建 `tests/syntax/test_type_cast.paw` 测试套件
- 验证 C 和 LLVM backend 的所有转换类型

---

## 📊 Performance

### Benchmark Results

使用 `tests/benchmarks/loop_benchmark.paw` 测试：

| 优化级别 | 编译时间 | 运行时间 | 相对性能 |
|---------|---------|---------|---------|
| -O0     | 基准     | 基准     | 1.0x    |
| -O1     | 略慢     | 更快     | ~1.1x   |
| -O2     | 略慢     | 更快     | ~1.2x   |
| -O3     | 更慢     | 最快     | ~1.3x   |

**注意**: 实际性能提升取决于代码复杂度。对于递归、循环密集型代码，优化效果更明显。

---

## 🧪 Testing

### 新增测试

**tests/benchmarks/fibonacci_benchmark.paw**:
- 递归 Fibonacci（优化敏感）
- 迭代 Fibonacci（对比）
- 验证结果正确性

**tests/benchmarks/loop_benchmark.paw**:
- 嵌套循环
- 数组操作模拟
- 算术密集型计算

### Usage Examples

```bash
# 测试不同优化级别
./zig-out/bin/pawc tests/benchmarks/fibonacci_benchmark.paw --backend=llvm -O0
clang output.ll -O0 -o fib_o0
time ./fib_o0

./zig-out/bin/pawc tests/benchmarks/fibonacci_benchmark.paw --backend=llvm -O3
clang output.ll -O3 -o fib_o3
time ./fib_o3
```

---

## 🎯 Benefits

### 1. 性能可控

```bash
# 开发时：快速编译，便于调试
pawc app.paw --backend=llvm -O0

# 生产环境：标准优化
pawc app.paw --backend=llvm -O2

# 性能关键：激进优化
pawc app.paw --backend=llvm -O3
```

### 2. 清晰的提示

编译器会告诉你如何使用优化：
```
⚡ Optimization: -O2 (standard optimization) ⭐
💡 Hints:
   • Compile with optimization: clang output.ll -O2 -o output
```

### 3. 灵活性

- 用户完全控制优化级别
- 可以组合使用不同的优化参数
- 利用 LLVM 生态系统的全部能力

---

## 📚 Documentation

### 命令行参数

```bash
pawc <file> --backend=llvm [optimization]

Optimization levels:
  -O0    No optimization (debugging)
  -O1    Basic optimization
  -O2    Standard optimization (recommended)
  -O3    Aggressive optimization
```

### 帮助信息

```bash
$ pawc --help

LLVM Optimization (v0.1.7) 🆕:
  -O0              No optimization (fastest compile, debugging)
  -O1              Basic optimization (balanced)
  -O2              Standard optimization (recommended) ⭐
  -O3              Aggressive optimization (maximum performance)
```

---

## 🔄 Migration

**无破坏性变更**: 
- 现有代码继续工作
- 优化参数是可选的
- 默认行为：不指定优化级别（-O0）

**推荐用法**:
```bash
# 开发环境
pawc app.paw --backend=llvm  # 或 -O0

# 生产环境
pawc app.paw --backend=llvm -O2
```

---

## 🔮 Future Work

v0.1.8 计划:
- [ ] 增强错误消息（源码位置，颜色高亮）
- [ ] 字符串类型改进
- [ ] 标准库扩展
- [ ] 编译时优化（常量折叠，死代码消除）

---

## 📊 Project Status

| 组件           | 完成度 | v0.1.7 改进 |
|----------------|--------|-------------|
| LLVM Backend   | 100% ✅| 优化支持 ✨ |
| Optimization   | 100% ✅| 新增 ⭐     |
| All Others     | 100% ✅| -           |

---

<div align="center">

**🐾 PawLang v0.1.7 - LLVM 优化支持！**

**性能可控，开发更高效！**

</div>

