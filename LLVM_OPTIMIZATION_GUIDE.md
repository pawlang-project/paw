# PawLang LLVM 优化指南

> 📊 **版本**: v1.0  
> 📅 **创建日期**: 2025-10-27  
> 🎯 **目标**: 通过 LLVM/Clang 优化标志实现高性能代码生成

---

## 1. 当前实现方案

### 方案选择：Clang 优化标志

我们采用**在调用 clang 时传递优化标志**的方式，这是最简单、最可靠的方案：

```cpp
// 在 CodeGenerator::compileToObject 中
std::string opt_flag;
switch (optimization_level) {
    case 0:  opt_flag = "-O0"; break;  // 无优化
    case 1:  opt_flag = "-O1"; break;  // 基本优化
    case 2:  opt_flag = "-O2"; break;  // 标准优化
    case 3:  opt_flag = "-O3"; break;  // 激进优化
    case -1: opt_flag = "-Os"; break;  // 大小优化
}

clang_cmd += " " + opt_flag;
```

---

## 2. 优化级别详解

### -O0（默认）
**特点**：
- 无优化
- 最快的编译速度
- 保留所有调试信息
- 代码结构与源代码一致

**适用场景**：
- 开发和调试
- 快速迭代

**性能**：基准

---

### -O1（基本优化）
**特点**：
- 基本优化 Pass
- 较快的编译速度
- 轻微的性能提升

**启用的优化**：
- 常量折叠
- 死代码消除
- 基本内联
- 简单的寄存器分配

**性能提升**：10-30%

---

### -O2（标准优化）⭐ 推荐
**特点**：
- 平衡编译时间和运行性能
- 生产环境推荐

**启用的优化**：
- -O1 的所有优化
- 循环优化
- 向量化
- GVN（全局值编号）
- 指令组合
- CFG 简化

**性能提升**：50-200%

---

### -O3（激进优化）
**特点**：
- 最激进的优化
- 较长的编译时间
- 最高的运行性能

**额外优化**：
- -O2 的所有优化
- 循环展开
- 更激进的内联
- 更多的向量化
- 预测性优化

**性能提升**：100-500%（某些场景）

**注意**：可能增加代码体积

---

### -Os（大小优化）
**特点**：
- 优化代码体积
- 保持合理的性能

**适用场景**：
- 嵌入式系统
- 容器化应用
- 对二进制大小敏感的场景

**效果**：
- 代码体积：-10-30%
- 性能：接近 -O2

---

## 3. 实施方案

### 3.1 修改 CodeGenerator

```cpp
// codegen.h
class CodeGenerator {
public:
    void setOptimizationLevel(int level) { 
        optimization_level_ = level; 
    }
    
private:
    int optimization_level_ = 0;  // 默认 -O0
};
```

```cpp
// codegen.cpp
bool CodeGenerator::compileToObject(const std::string& filename) {
    // ... 现有代码 ...
    
    // 添加优化标志
    std::string opt_flag;
    switch (optimization_level_) {
        case 0:  opt_flag = "-O0"; break;
        case 1:  opt_flag = "-O1"; break;
        case 2:  opt_flag = "-O2"; break;
        case 3:  opt_flag = "-O3"; break;
        case -1: opt_flag = "-Os"; break;
        default: opt_flag = "-O0"; break;
    }
    
    clang_cmd += " " + opt_flag;
    
    // ... 继续编译 ...
}
```

### 3.2 在 main.cpp 中传递优化级别

```cpp
// main.cpp
pawc::CodeGenerator codegen("pawc_module");
codegen.setOptimizationLevel(opt_level);  // 传递用户指定的优化级别

if (!codegen.generate(program)) {
    // 错误处理
}
```

---

## 4. 测试对比

### 4.1 编译速度测试

```bash
# 测试不同优化级别的编译时间
time ./pawc fibonacci.paw -O0 -o fib_o0
time ./pawc fibonacci.paw -O2 -o fib_o2
time ./pawc fibonacci.paw -O3 -o fib_o3
```

### 4.2 运行性能测试

```bash
# 测试运行时性能
time ./fib_o0
time ./fib_o2
time ./fib_o3
```

### 4.3 代码体积测试

```bash
# 比较可执行文件大小
ls -lh fib_o0 fib_o2 fib_o3 fib_os
```

---

## 5. 性能测试示例

### fibonacci.paw（CPU 密集）

**测试代码**：
```paw
fn fib(n: i32) -> i32 {
    if n <= 1 {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

fn main() {
    let result = fib(40);
    println("Result: {}", result);
}
```

**预期结果**：
```
-O0:  ~5.0 秒
-O1:  ~3.5 秒 (30% 提升)
-O2:  ~1.0 秒 (80% 提升) ⭐
-O3:  ~0.8 秒 (84% 提升)
```

---

## 6. 优化建议

### 开发阶段
```bash
pawc program.paw -O0  # 快速编译，易于调试
```

### 测试阶段
```bash
pawc program.paw -O1  # 基本优化，发现性能问题
```

### 生产环境
```bash
pawc program.paw -O2 -o app  # 平衡性能，推荐 ⭐
```

### 高性能场景
```bash
pawc program.paw -O3 -o app  # 最高性能
```

### 嵌入式/容器
```bash
pawc program.paw -Os -o app  # 最小体积
```

---

## 7. 实施步骤

### 步骤 1：添加 optimization_level 成员
```cpp
// src/codegen/codegen.h
private:
    int optimization_level_ = 0;
```

### 步骤 2：添加 setter 方法
```cpp
// src/codegen/codegen.h
public:
    void setOptimizationLevel(int level);
```

### 步骤 3：在 compileToObject 中使用
```cpp
// src/codegen/codegen.cpp
std::string opt_flag = getOptimizationFlag(optimization_level_);
clang_cmd += " " + opt_flag;
```

### 步骤 4：在 main.cpp 中传递
```cpp
// src/main.cpp
codegen.setOptimizationLevel(opt_level);
```

---

## 8. 其他优化选项

### 链接时优化（LTO）
```cpp
// 添加到 clang 命令
clang_cmd += " -flto";  // Link-Time Optimization
```

**效果**：额外 5-15% 性能提升

### 特定 CPU 优化
```cpp
// 为本机 CPU 优化
clang_cmd += " -march=native";
```

**效果**：利用 CPU 特定指令集

### 快速数学
```cpp
// 放宽浮点运算精度
clang_cmd += " -ffast-math";
```

**效果**：浮点运算加速 10-30%

---

## 9. 预期收益

| 优化级别 | 编译时间 | 运行速度 | 代码体积 | 调试性 |
|---------|---------|---------|---------|--------|
| -O0     | 1x      | 1x      | 1x      | ⭐⭐⭐⭐⭐ |
| -O1     | 1.2x    | 1.3x    | 0.95x   | ⭐⭐⭐⭐ |
| -O2     | 1.5x    | 2-3x    | 0.9x    | ⭐⭐⭐ |
| -O3     | 2x      | 3-5x    | 1.1x    | ⭐⭐ |
| -Os     | 1.5x    | 2x      | 0.7x    | ⭐⭐⭐ |

---

## 10. 注意事项

⚠️ **优化可能引入的问题**：
1. 更长的编译时间
2. 调试困难（栈信息不准确）
3. 某些边界情况行为改变
4. 浮点运算精度变化（-ffast-math）

✅ **最佳实践**：
1. 开发用 `-O0`
2. 发布用 `-O2`
3. 性能关键用 `-O3`
4. 始终测试优化后的代码

---

**🐾 LLVM 优化集成完成！简单、可靠、高效！**

*文档版本: v1.0*  
*创建日期: 2025-10-27*

