# PawLang → LLVM 逐步迁移计划

**目标**: 从C后端迁移到LLVM后端  
**策略**: 双后端并存，逐步迁移  
**参考**: https://github.com/kassane/llvm-zig  
**预计时间**: 2-3个月

---

## 🎯 迁移目标

### 为什么迁移到LLVM？

#### 当前架构的局限
```
PawLang → C代码 → GCC/Clang → 可执行文件
```

**问题**:
- 🔴 中间C代码步骤（增加复杂度）
- 🔴 依赖外部C编译器
- 🔴 优化能力受限于C
- 🔴 难以实现高级特性（async, SIMD等）

#### LLVM后端的优势
```
PawLang → LLVM IR → 优化 → 机器码
```

**收益**:
- ✅ 直接生成机器码
- ✅ 强大的优化（-O3级别）
- ✅ 跨平台支持（x86, ARM, RISC-V等）
- ✅ 现代编译器基础设施
- ✅ 与Rust/Swift/Clang同级
- ✅ 支持高级特性（协程、SIMD等）
- ✅ 更好的调试信息（DWARF）

---

## 📋 迁移策略：双后端并存

### 第一阶段：保留C后端
```zig
pub const Backend = enum {
    c_backend,      // 当前的C代码生成
    llvm_backend,   // 新的LLVM后端
};

pub const Compiler = struct {
    backend: Backend,
    // ...
};
```

**使用方式**:
```bash
# 使用C后端（默认，稳定）
pawc file.paw

# 使用LLVM后端（实验性）
pawc file.paw --backend=llvm
```

### 第二阶段：功能对等
逐步实现LLVM后端，直到功能与C后端对等

### 第三阶段：切换默认
LLVM后端稳定后，设为默认

### 第四阶段：移除C后端（可选）
完全迁移后，可选择移除C后端

---

## 🗺️ 详细迁移路线图

### Phase 0: 准备阶段（v0.1.3完成后）

#### 0.1 发布v0.1.3 ⭐
- [ ] 在GitHub发布v0.1.3
- [ ] 庆祝类型推导和工程化模块的完成
- [ ] 收集用户反馈

#### 0.2 学习和调研（1周）
- [ ] 学习LLVM基础知识
- [ ] 阅读llvm-zig文档
- [ ] 研究LLVM IR结构
- [ ] 参考其他语言的LLVM后端（Rust, Zig自己）

**资源**:
- [LLVM官方文档](https://llvm.org/docs/)
- [llvm-zig项目](https://github.com/kassane/llvm-zig)
- [LLVM Kaleidoscope教程](https://llvm.org/docs/tutorial/)

#### 0.3 项目设置（2-3天）
- [ ] 添加llvm-zig依赖到`build.zig.zon`
- [ ] 配置build.zig
- [ ] 创建`src/llvm_backend.zig`
- [ ] 基础项目结构

---

### Phase 1: 最小可行后端（2-3周）

**目标**: 编译最简单的程序

#### 1.1 Hello World（Week 1）
```paw
fn main() -> i32 {
    return 42;
}
```

**任务**:
- [ ] 创建LLVM模块
- [ ] 生成main函数
- [ ] 返回常量值
- [ ] 编译为可执行文件

**LLVM IR示例**:
```llvm
define i32 @main() {
  ret i32 42
}
```

#### 1.2 基础表达式（Week 2）
```paw
fn main() -> i32 {
    let x = 10;
    let y = 20;
    return x + y;
}
```

**任务**:
- [ ] 变量声明（alloca）
- [ ] 常量加载
- [ ] 二元运算（add, sub, mul, div）
- [ ] 局部变量

**LLVM IR示例**:
```llvm
define i32 @main() {
entry:
  %x = alloca i32
  %y = alloca i32
  store i32 10, i32* %x
  store i32 20, i32* %y
  %1 = load i32, i32* %x
  %2 = load i32, i32* %y
  %3 = add i32 %1, %2
  ret i32 %3
}
```

#### 1.3 函数调用（Week 3）
```paw
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

fn main() -> i32 {
    return add(10, 20);
}
```

**任务**:
- [ ] 函数定义
- [ ] 参数传递
- [ ] 函数调用
- [ ] 返回值

---

### Phase 2: 类型系统（3-4周）

#### 2.1 基础类型（Week 4-5）
- [ ] 整数类型（i8, i16, i32, i64）
- [ ] 浮点类型（f32, f64）
- [ ] 布尔类型
- [ ] 字符串类型（i8*）

#### 2.2 复合类型（Week 6）
- [ ] 结构体（struct）
- [ ] 数组（array）
- [ ] 指针（pointer）

#### 2.3 控制流（Week 7）
- [ ] if/else（br, phi）
- [ ] loop（br, cond）
- [ ] return

---

### Phase 3: 泛型支持（3-4周）

#### 3.1 泛型函数
```paw
fn identity<T>(x: T) -> T {
    return x;
}
```

**策略**: 单态化（与当前相同）
- 为每个具体类型生成独立的LLVM函数
- `identity_i32`, `identity_string`

#### 3.2 泛型结构体
```paw
type Vec<T> = struct {
    ptr: i32,
    len: i32,
}
```

**策略**: 生成具体类型的结构体
- `Vec_i32`, `Vec_string`

#### 3.3 泛型方法
类似泛型函数处理

---

### Phase 4: 高级特性（2-3周）

#### 4.1 模式匹配
- [ ] switch/match表达式
- [ ] 枚举类型

#### 4.2 闭包和高阶函数
- [ ] 函数指针
- [ ] 闭包捕获

#### 4.3 异步支持（可选）
- [ ] async/await
- [ ] 协程（LLVM coroutines）

---

## 🔧 实现细节

### 项目结构
```
src/
├── main.zig
├── lexer.zig
├── parser.zig
├── typechecker.zig
├── ast.zig
├── generics.zig
├── codegen.zig         # C后端（保留）
├── llvm_backend.zig    # 🆕 LLVM后端（新增）
└── std/
    └── prelude.paw
```

### build.zig.zon配置
```zig
.{
    .name = "PawLang",
    .version = "0.2.0",
    .dependencies = .{
        .llvm = .{
            .url = "https://github.com/kassane/llvm-zig/archive/refs/heads/main.tar.gz",
            // 或使用git
            .url = "git+https://github.com/kassane/llvm-zig",
        },
    },
}
```

### build.zig配置
```zig
const llvm_dep = b.dependency("llvm", .{
    .target = target,
    .optimize = optimize,
});
const llvm_mod = llvm_dep.module("llvm");

exe.root_module.addImport("llvm", llvm_mod);
exe.linkSystemLibrary("LLVM");
```

### llvm_backend.zig骨架
```zig
const std = @import("std");
const ast = @import("ast.zig");
const llvm = @import("llvm");

pub const LLVMBackend = struct {
    allocator: std.mem.Allocator,
    context: *llvm.Context,
    module: *llvm.Module,
    builder: *llvm.Builder,
    
    pub fn init(allocator: std.mem.Allocator) !LLVMBackend {
        const context = llvm.Context.create();
        const module = llvm.Module.createWithName("pawlang", context);
        const builder = llvm.Builder.create(context);
        
        return LLVMBackend{
            .allocator = allocator,
            .context = context,
            .module = module,
            .builder = builder,
        };
    }
    
    pub fn deinit(self: *LLVMBackend) void {
        self.builder.dispose();
        self.module.dispose();
        self.context.dispose();
    }
    
    pub fn generate(self: *LLVMBackend, program: ast.Program) ![]const u8 {
        // 生成LLVM IR
        for (program.declarations) |decl| {
            try self.generateDecl(decl);
        }
        
        // 返回LLVM IR文本
        return self.module.printToString();
    }
    
    fn generateDecl(self: *LLVMBackend, decl: ast.TopLevelDecl) !void {
        switch (decl) {
            .function => |func| try self.generateFunction(func),
            // ...
        }
    }
    
    fn generateFunction(self: *LLVMBackend, func: ast.FunctionDecl) !void {
        // 创建函数类型
        const return_type = self.toLLVMType(func.return_type);
        var param_types = std.ArrayList(*llvm.Type).init(self.allocator);
        defer param_types.deinit();
        
        for (func.params) |param| {
            try param_types.append(self.toLLVMType(param.type));
        }
        
        const func_type = llvm.FunctionType.create(
            return_type,
            param_types.items.ptr,
            @intCast(param_types.items.len),
            false
        );
        
        // 创建函数
        const llvm_func = llvm.Function.addFunction(
            self.module,
            func.name.ptr,
            func_type
        );
        
        // 创建基本块
        const entry = llvm.BasicBlock.appendInContext(
            self.context,
            llvm_func,
            "entry"
        );
        self.builder.positionBuilderAtEnd(entry);
        
        // 生成函数体
        for (func.body) |stmt| {
            try self.generateStmt(stmt);
        }
    }
    
    fn toLLVMType(self: *LLVMBackend, ty: ast.Type) *llvm.Type {
        return switch (ty) {
            .i32 => llvm.Type.int32Type(self.context),
            .i64 => llvm.Type.int64Type(self.context),
            .f32 => llvm.Type.floatType(self.context),
            .f64 => llvm.Type.doubleType(self.context),
            .bool => llvm.Type.int1Type(self.context),
            .string => llvm.Type.pointerType(llvm.Type.int8Type(self.context), 0),
            .void => llvm.Type.voidType(self.context),
            // ...
        };
    }
};
```

---

## 🗓️ 迁移时间表

### 立即行动（今天）
1. ✅ 发布v0.1.3
   - 类型推导
   - 泛型类型系统
   - 工程化模块

### 短期（1-2周）
2. 🔬 学习和实验
   - 学习LLVM基础
   - 实验llvm-zig绑定
   - 编译Hello World

### 中期（v0.2.0 - 1-2个月）
3. 🏗️ 实现LLVM后端基础
   - 基础表达式
   - 函数调用
   - 简单类型
   - 与C后端并存

### 长期（v0.3.0 - 3-4个月）
4. 🚀 完整LLVM后端
   - 泛型支持
   - 完整类型系统
   - 优化管线
   - 设为默认后端

---

## 📊 版本规划

### v0.1.3（当前）- 立即发布 ⭐
**内容**:
- ✅ 类型推导
- ✅ 泛型类型系统
- ✅ 工程化模块（多项导入 + mod.paw）

**状态**: 完全就绪，今天发布

**后端**: C后端（稳定）

---

### v0.2.0（1-2个月）- 双后端实验

**核心目标**: LLVM后端原型 + Trait系统

**LLVM部分**:
- [ ] 添加llvm-zig依赖
- [ ] 实现LLVMBackend基础
- [ ] 支持简单程序（Hello World, 算术）
- [ ] 双后端切换（--backend=llvm）

**语言特性**:
- [ ] Trait系统基础
- [ ] 泛型约束
- [ ] 增强标准库

**发布标准**:
- C后端：完整功能（默认）
- LLVM后端：实验性（可选）
- Trait系统：基础可用

---

### v0.3.0（3-4个月）- LLVM后端完整

**核心目标**: LLVM后端功能完整

**LLVM部分**:
- [ ] 完整类型系统支持
- [ ] 泛型单态化
- [ ] 模块系统
- [ ] 优化管线
- [ ] 设为默认后端

**语言特性**:
- [ ] 完整Trait系统
- [ ] 错误处理（Result, Option）
- [ ] 包管理器

**发布标准**:
- LLVM后端：完整功能（默认）
- C后端：备选（--backend=c）
- 所有特性在两个后端都可用

---

### v1.0.0（6个月+）- 生产就绪

**目标**: 稳定的生产级编译器

**LLVM部分**:
- [ ] 高级优化
- [ ] 增量编译
- [ ] LSP支持
- [ ] 调试器集成

**语言特性**:
- [ ] 异步编程
- [ ] 宏系统
- [ ] 完整工具链

---

## 🔧 技术实现

### 依赖管理

#### build.zig.zon
```zig
.{
    .name = "PawLang",
    .version = "0.2.0",
    .paths = .{
        "build.zig",
        "build.zig.zon",
        "src",
        "stdlib",
    },
    .dependencies = .{
        .llvm = .{
            .url = "git+https://github.com/kassane/llvm-zig#main",
        },
    },
}
```

#### build.zig修改
```zig
pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    
    const exe = b.addExecutable(.{
        .name = "pawc",
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });
    
    // 🆕 添加LLVM依赖
    const llvm_dep = b.dependency("llvm", .{
        .target = target,
        .optimize = optimize,
    });
    const llvm_mod = llvm_dep.module("llvm");
    exe.root_module.addImport("llvm", llvm_mod);
    
    // 🆕 链接LLVM库
    exe.linkSystemLibrary("LLVM");
    
    b.installArtifact(exe);
}
```

### 后端选择

#### main.zig修改
```zig
const Backend = enum {
    c,
    llvm,
};

pub fn main() !void {
    // ... 参数解析 ...
    
    var backend = Backend.c;  // 默认C后端
    
    for (args) |arg| {
        if (std.mem.eql(u8, arg, "--backend=llvm")) {
            backend = .llvm;
        }
    }
    
    // ... 词法分析、解析、类型检查 ...
    
    // 代码生成
    const output = switch (backend) {
        .c => blk: {
            var codegen = CodeGen.init(allocator);
            defer codegen.deinit();
            break :blk try codegen.generate(ast);
        },
        .llvm => blk: {
            var llvm_backend = LLVMBackend.init(allocator);
            defer llvm_backend.deinit();
            break :blk try llvm_backend.generate(ast);
        },
    };
    
    // 写入输出
    // ...
}
```

---

## 🧪 测试策略

### 双后端测试
```bash
# 测试C后端
pawc test.paw
gcc output.c -o test_c
./test_c

# 测试LLVM后端
pawc test.paw --backend=llvm
# 直接生成可执行文件
./output

# 对比结果
echo "C backend: $?"
echo "LLVM backend: $?"
```

### 自动化测试
```bash
# 所有测试在两个后端都运行
for backend in c llvm; do
    for test in tests/*.paw; do
        pawc $test --backend=$backend
        # 验证结果
    done
done
```

---

## 📚 学习资源

### LLVM基础
1. **LLVM官方教程**
   - [Kaleidoscope教程](https://llvm.org/docs/tutorial/)
   - 从零实现一个语言的LLVM后端

2. **llvm-zig文档**
   - https://github.com/kassane/llvm-zig
   - Zig的LLVM C API绑定

3. **参考实现**
   - Zig编译器自身（也用LLVM）
   - Rust编译器（rustc）
   - Swift编译器

### LLVM IR
```llvm
; 示例：简单加法
define i32 @add(i32 %a, i32 %b) {
entry:
  %result = add i32 %a, %b
  ret i32 %result
}
```

---

## 💡 建议的工作流程

### Week 0（今天）
```bash
1. 发布v0.1.3到GitHub ⭐
2. 休息庆祝！🎉
3. 阅读LLVM基础文档
```

### Week 1-2（学习）
```bash
1. LLVM官方教程
2. 实验llvm-zig
3. 编译Hello World
4. 理解LLVM IR结构
```

### Week 3-6（v0.2.0开发）
```bash
1. 创建llvm_backend.zig
2. 实现基础表达式
3. 实现函数调用
4. 开发Trait系统（并行）
5. 测试和文档
```

### Week 7-12（v0.3.0开发）
```bash
1. 完整泛型支持
2. 优化管线
3. 完整类型系统
4. 设LLVM为默认
5. 发布v0.3.0
```

---

## 🎯 里程碑

### Milestone 1: LLVM Hello World
```paw
fn main() -> i32 { 42 }
```
编译运行成功 → 🎉 基础可行

### Milestone 2: 基础程序
```paw
fn add(a: i32, b: i32) -> i32 { a + b }
fn main() -> i32 { add(10, 20) }
```
功能正常 → 🎉 核心架构完成

### Milestone 3: 泛型支持
```paw
fn identity<T>(x: T) -> T { x }
let x = identity(42);
```
泛型工作 → 🎉 类型系统就绪

### Milestone 4: 功能对等
所有v0.1.3特性在LLVM后端都能工作 → 🎉 可以切换

### Milestone 5: 默认切换
LLVM成为默认后端 → 🎉 迁移完成

---

## ⚖️ 风险和缓解

### 风险1: LLVM复杂度高
**缓解**: 
- 从最简单的Hello World开始
- 参考成熟项目（Zig, Rust）
- 保留C后端作为备份

### 风险2: 开发时间长
**缓解**:
- 双后端并存，逐步迁移
- 先发布v0.1.3，不阻塞用户
- 分阶段实现

### 风险3: 功能回退
**缓解**:
- 所有测试在两个后端都运行
- 功能对等后才切换
- C后端保留作为fallback

---

## 🎬 立即行动计划

### 今天
```
1. ✅ 发布v0.1.3
   - 类型推导 + 泛型系统 + 工程化模块
   - 给用户带来价值
   - https://github.com/KinLeoapple/PawLang/releases/new?tag=v0.1.3

2. 📚 开始学习LLVM
   - 阅读Kaleidoscope教程
   - 实验llvm-zig示例
```

### 本周
```
1. 完成LLVM基础学习
2. 成功编译Hello World (LLVM)
3. 规划v0.2.0详细任务
```

### 下周
```
1. 创建llvm_backend.zig
2. 添加llvm-zig依赖
3. 实现最小可行后端
```

---

## 🏁 总结

### 当前状态
- ✅ v0.1.3完全就绪（类型推导+泛型+模块）
- ✅ C后端稳定工作
- ✅ 27/27测试通过

### 下一步
- 🚀 今天：发布v0.1.3
- 📚 本周：学习LLVM
- 🏗️ v0.2.0：LLVM后端原型 + Trait系统
- 🎯 v0.3.0：LLVM后端完整

### 迁移策略
- ✅ 双后端并存（降低风险）
- ✅ 逐步迁移（确保质量）
- ✅ 功能对等后切换（用户无感知）

---

**建议**: 先发布v0.1.3，让用户享受新特性，然后从容开始LLVM迁移！

**参考资源**:
- https://github.com/kassane/llvm-zig
- https://llvm.org/docs/tutorial/

