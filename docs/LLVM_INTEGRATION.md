# LLVM 集成方案

**当前版本**: v0.1.4  
**更新日期**: 2025-10-09

---

## 🎯 设计目标

- ✅ 不污染系统环境
- ✅ 跨平台一致性
- ✅ 项目自包含
- ✅ 易于部署

---

## 📊 当前实现（v0.1.4）

### 方案：文本 LLVM IR 生成

**架构**:
```
PawLang → LLVM IR 文本 (.ll) → clang/llc → 可执行文件
```

**优势**:
- ✅ 无需链接 LLVM 库
- ✅ 不依赖系统 LLVM 版本
- ✅ 项目轻量（~300 行代码）
- ✅ 易于调试（可查看生成的 .ll 文件）
- ✅ 跨平台（只要有 clang 即可）

**限制**:
- ⚠️ 需要系统安装 clang（用于编译 .ll）
- ⚠️ 不能直接优化 IR
- ⚠️ 文本生成相对较慢

### 使用方式

```bash
# 生成 LLVM IR
pawc hello.paw --backend=llvm
# 输出: output.ll

# 编译成可执行文件（需要 clang）
clang output.ll -o hello
# 或
llc output.ll -o hello.s && gcc hello.s -o hello
```

---

## 🚀 未来方案（v0.2.0+）

### 方案 A：集成 LLVM C API（推荐）

**架构**:
```
PawLang → LLVM C API → LLVM IR (内存) → 优化 → 机器码
```

#### 实现步骤

1. **添加 LLVM 作为构建依赖**

使用 Zig 的包管理器集成预编译的 LLVM：

```zig
// build.zig.zon
.dependencies = .{
    .llvm = .{
        // 选项1: 使用 llvm-zig (Zig 绑定)
        .url = "git+https://github.com/kassane/llvm-zig",
    },
    // 或
    // 选项2: 使用预编译的 LLVM (未来可能)
},
```

2. **创建 LLVM Builder**

```zig
// src/llvm_native_backend.zig
const llvm = @import("llvm");

pub const LLVMNativeBackend = struct {
    context: *llvm.Context,
    module: *llvm.Module,
    builder: *llvm.Builder,
    
    pub fn init(allocator: Allocator) !LLVMNativeBackend {
        return .{
            .context = llvm.Context.create(),
            .module = llvm.Module.createWithName("pawlang"),
            .builder = llvm.Builder.create(),
        };
    }
    
    pub fn generateFunction(self: *Self, func: ast.FunctionDecl) !void {
        // 直接调用 LLVM API 构建 IR
        const fn_type = llvm.FunctionType.create(...);
        const llvm_func = self.module.addFunction(func.name, fn_type);
        // ...
    }
    
    pub fn compile(self: *Self) ![]const u8 {
        // 优化
        const pass_manager = llvm.PassManager.create(self.module);
        pass_manager.addInstructionCombiningPass();
        pass_manager.addReassociatePass();
        // ...
        
        // 生成机器码
        return self.module.emitToMemory();
    }
};
```

3. **集成优化管线**

```zig
pub fn optimizeModule(module: *llvm.Module, level: OptimizationLevel) !void {
    const pm = llvm.PassManager.create(module);
    
    switch (level) {
        .O0 => {}, // 无优化
        .O1 => {
            pm.addInstructionCombiningPass();
            pm.addReassociatePass();
        },
        .O2 => {
            pm.addInstructionCombiningPass();
            pm.addReassociatePass();
            pm.addGVNPass();
            pm.addCFGSimplificationPass();
        },
        .O3 => {
            pm.addInstructionCombiningPass();
            pm.addReassociatePass();
            pm.addGVNPass();
            pm.addCFGSimplificationPass();
            pm.addTailCallEliminationPass();
            pm.addLoopVectorizePass();
        },
    }
    
    pm.run(module);
}
```

#### 优势

- ✅ 直接操作 LLVM IR（更快）
- ✅ 完整的优化管线
- ✅ 可以生成机器码
- ✅ 更好的错误诊断
- ✅ 支持 JIT 编译

#### 部署方案

**方案 1: 静态链接 LLVM**
```bash
# 下载预编译的 LLVM 静态库
# 放在 vendor/llvm/lib/
# 构建时链接
```

**方案 2: 作为子模块编译**
```bash
# 将 LLVM 源码作为子模块
git submodule add https://github.com/llvm/llvm-project vendor/llvm
# 配置只编译需要的部分
```

**方案 3: 使用 llvm-zig**
```bash
# 使用社区维护的 Zig 绑定
# 自动处理跨平台构建
```

---

### 方案 B：混合方案（推荐 v0.2.0）

**策略**: 双模式支持

```bash
# 文本模式（默认，无需 LLVM 库）
pawc hello.paw --backend=llvm

# 原生模式（需要 LLVM 库，更快更强）
pawc hello.paw --backend=llvm-native -O2
```

**好处**:
- ✅ 兼容性：文本模式适合开发和简单部署
- ✅ 性能：原生模式适合生产环境
- ✅ 灵活性：用户可选择

---

## 📋 实施计划

### Phase 1: v0.1.4（当前）✅

- ✅ 实现文本 LLVM IR 生成
- ✅ 支持基础功能
- ✅ 双后端架构

### Phase 2: v0.2.0（计划中）

**时间**: 1-2 个月

**目标**: 完善文本生成 + 实验原生 API

1. **完善文本生成后端**
   - 支持控制流
   - 支持结构体
   - 支持泛型

2. **实验 LLVM C API**
   - 集成 llvm-zig
   - 实现基础 API 调用
   - 对比性能

### Phase 3: v0.3.0

**时间**: 2-3 个月

**目标**: 原生 LLVM 后端成为主要方式

1. **完整原生实现**
   - 所有功能
   - 优化管线
   - JIT 支持

2. **部署方案**
   - 静态链接 LLVM
   - 预编译二进制
   - 跨平台支持

---

## 🔧 技术选型对比

### 文本生成 vs 原生 API

| 特性 | 文本生成 | 原生 API |
|------|----------|----------|
| 无需 LLVM 库 | ✅ | ❌ |
| 生成速度 | 快 | 更快 |
| 优化能力 | 依赖 clang | 完整控制 |
| 调试友好 | ✅ (.ll 可读) | ⚠️ |
| 部署简单 | ✅ | 需要 LLVM |
| JIT 支持 | ❌ | ✅ |
| 跨平台 | ✅ | 需要配置 |

### 依赖管理方案对比

| 方案 | 优势 | 劣势 |
|------|------|------|
| 系统 LLVM (brew) | 简单 | 污染系统 |
| 静态链接 | 自包含 | 大小增加 |
| llvm-zig | Zig 原生 | 社区维护 |
| 子模块编译 | 完全控制 | 编译慢 |

---

## 💡 推荐方案

### 短期（v0.1.4 - v0.2.0）

**使用文本生成方式**

- ✅ 快速迭代
- ✅ 简单部署
- ✅ 易于调试

### 中期（v0.2.0 - v0.3.0）

**集成 llvm-zig + 双模式**

```bash
# 开发/测试：文本模式
pawc --backend=llvm

# 生产：原生模式
pawc --backend=llvm-native -O3
```

### 长期（v1.0+）

**完全原生 + 静态链接**

- 预编译 LLVM 静态库
- 项目自包含
- 不依赖系统环境

---

## 📚 参考资源

### LLVM 官方

- [LLVM Tutorial](https://llvm.org/docs/tutorial/)
- [LLVM C API](https://llvm.org/doxygen/group__LLVMC.html)

### Zig 生态

- [llvm-zig](https://github.com/kassane/llvm-zig) - Zig 的 LLVM 绑定
- [Zig 编译器](https://github.com/ziglang/zig) - Zig 自身使用 LLVM

### 其他语言实现

- [Rust (rustc)](https://github.com/rust-lang/rust) - 使用 LLVM
- [Swift](https://github.com/apple/swift) - 使用 LLVM
- [Crystal](https://github.com/crystal-lang/crystal) - 使用 LLVM

---

## 🎯 结论

**当前 v0.1.4 的文本生成方案是最佳选择**：

1. ✅ 不污染系统
2. ✅ 易于开发和调试
3. ✅ 跨平台支持好
4. ✅ 代码简洁（~300 行）

**未来会逐步集成原生 LLVM**，但保持文本模式作为备选方案，确保灵活性和兼容性。

---

**更新**: 该文档会随着项目进展持续更新。

