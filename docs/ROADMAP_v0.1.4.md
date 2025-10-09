# 🚀 PawLang v0.1.4 开发路线图

**发布目标**: 2025年11月  
**开发周期**: 2-3周  
**核心目标**: LLVM 后端原型 (双后端并存)

---

## 🎯 版本目标

### 主要目标：LLVM 后端原型

v0.1.4 将引入**实验性的 LLVM 后端**，与现有的 C 后端并存。这是 PawLang 向现代编译器基础设施迁移的第一步。

**设计理念**：
- ✅ **双后端并存** - C 后端保持稳定（默认），LLVM 后端作为实验性选项
- ✅ **渐进式迁移** - 从最简单的功能开始，逐步完善
- ✅ **零破坏性** - 现有代码 100% 兼容，无需修改

---

## 📦 功能清单

### 1. LLVM 后端基础架构 ⭐

#### 1.1 依赖集成
```zig
// build.zig.zon
.dependencies = .{
    .llvm = .{
        .url = "git+https://github.com/kassane/llvm-zig#main",
    },
}
```

**状态**: ✅ 完成

#### 1.2 后端选择
```bash
# C 后端（默认，稳定）
./zig-out/bin/pawc examples/hello.paw

# LLVM 后端（实验性）
./zig-out/bin/pawc examples/hello.paw --backend=llvm
```

**状态**: 🔲 待实现

### 2. 最小可行后端 (MVP)

#### 2.1 Hello World - 返回常量
```paw
fn main() -> i32 {
    return 42;
}
```

**LLVM IR 输出**:
```llvm
define i32 @main() {
entry:
  ret i32 42
}
```

**状态**: 🔲 待实现

#### 2.2 基础算术表达式
```paw
fn main() -> i32 {
    let x = 10;
    let y = 20;
    return x + y;
}
```

**LLVM IR 输出**:
```llvm
define i32 @main() {
entry:
  %x = alloca i32
  %y = alloca i32
  store i32 10, ptr %x
  store i32 20, ptr %y
  %1 = load i32, ptr %x
  %2 = load i32, ptr %y
  %3 = add i32 %1, %2
  ret i32 %3
}
```

**状态**: 🔲 待实现

#### 2.3 简单函数调用
```paw
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

fn main() -> i32 {
    return add(10, 20);
}
```

**LLVM IR 输出**:
```llvm
define i32 @add(i32 %a, i32 %b) {
entry:
  %result = add i32 %a, %b
  ret i32 %result
}

define i32 @main() {
entry:
  %1 = call i32 @add(i32 10, i32 20)
  ret i32 %1
}
```

**状态**: 🔲 待实现

---

## 🏗️ 技术架构

### 文件结构
```
src/
├── main.zig              # 入口，支持后端选择
├── lexer.zig            # 词法分析（不变）
├── parser.zig           # 语法分析（不变）
├── typechecker.zig      # 类型检查（不变）
├── ast.zig              # AST定义（不变）
├── generics.zig         # 泛型系统（不变）
├── codegen.zig          # C后端（保留）
├── llvm_backend.zig     # 🆕 LLVM后端（新增）
└── std/
    └── prelude.paw
```

### 后端接口设计

```zig
// 统一的后端接口
pub const Backend = enum {
    c,
    llvm,
};

pub const BackendInterface = struct {
    generate: *const fn (allocator: std.mem.Allocator, program: ast.Program) anyerror![]const u8,
    
    pub fn forBackend(backend: Backend) BackendInterface {
        return switch (backend) {
            .c => .{ .generate = &cGenerate },
            .llvm => .{ .generate = &llvmGenerate },
        };
    }
};

fn cGenerate(allocator: std.mem.Allocator, program: ast.Program) ![]const u8 {
    var codegen = CodeGen.init(allocator);
    defer codegen.deinit();
    return try codegen.generate(program);
}

fn llvmGenerate(allocator: std.mem.Allocator, program: ast.Program) ![]const u8 {
    var backend = LLVMBackend.init(allocator);
    defer backend.deinit();
    return try backend.generate(program);
}
```

### LLVM 后端骨架

```zig
// src/llvm_backend.zig
const std = @import("std");
const ast = @import("ast.zig");
const llvm = @import("llvm");

pub const LLVMBackend = struct {
    allocator: std.mem.Allocator,
    context: *llvm.Context,
    module: *llvm.Module,
    builder: *llvm.Builder,
    
    // 符号表：变量名 -> LLVM Value
    variables: std.StringHashMap(*llvm.Value),
    // 函数表：函数名 -> LLVM Function
    functions: std.StringHashMap(*llvm.Function),
    
    pub fn init(allocator: std.mem.Allocator) !LLVMBackend {
        const context = llvm.Context.create();
        const module = llvm.Module.createWithName("pawlang", context);
        const builder = llvm.Builder.create(context);
        
        return LLVMBackend{
            .allocator = allocator,
            .context = context,
            .module = module,
            .builder = builder,
            .variables = std.StringHashMap(*llvm.Value).init(allocator),
            .functions = std.StringHashMap(*llvm.Function).init(allocator),
        };
    }
    
    pub fn deinit(self: *LLVMBackend) void {
        self.variables.deinit();
        self.functions.deinit();
        self.builder.dispose();
        self.module.dispose();
        self.context.dispose();
    }
    
    pub fn generate(self: *LLVMBackend, program: ast.Program) ![]const u8 {
        // 生成所有声明
        for (program.declarations) |decl| {
            try self.generateDecl(decl);
        }
        
        // 返回 LLVM IR 文本
        return self.module.printToString();
    }
    
    fn generateDecl(self: *LLVMBackend, decl: ast.TopLevelDecl) !void {
        switch (decl) {
            .function => |func| try self.generateFunction(func),
            .type_decl => {}, // v0.1.4 暂不支持
            .import => {},    // v0.1.4 暂不支持
        }
    }
    
    fn generateFunction(self: *LLVMBackend, func: ast.FunctionDecl) !void {
        // 创建函数类型
        const return_type = try self.toLLVMType(func.return_type);
        
        var param_types = std.ArrayList(*llvm.Type).init(self.allocator);
        defer param_types.deinit();
        
        for (func.params) |param| {
            const param_type = try self.toLLVMType(param.type);
            try param_types.append(param_type);
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
        
        try self.functions.put(func.name, llvm_func);
        
        // 创建入口基本块
        const entry = llvm.BasicBlock.appendInContext(
            self.context,
            llvm_func,
            "entry"
        );
        self.builder.positionBuilderAtEnd(entry);
        
        // TODO: 设置参数名称和分配局部变量
        
        // 生成函数体
        for (func.body) |stmt| {
            try self.generateStmt(stmt);
        }
    }
    
    fn generateStmt(self: *LLVMBackend, stmt: ast.Statement) !void {
        switch (stmt) {
            .ret => |ret_stmt| {
                if (ret_stmt.value) |val| {
                    const ret_val = try self.generateExpr(val);
                    _ = self.builder.buildRet(ret_val);
                } else {
                    _ = self.builder.buildRetVoid();
                }
            },
            .let => |let_stmt| {
                // TODO: 实现变量声明
            },
            // 其他语句...
            else => {},
        }
    }
    
    fn generateExpr(self: *LLVMBackend, expr: ast.Expression) !*llvm.Value {
        return switch (expr) {
            .int_literal => |val| {
                const int_type = llvm.Type.int32Type(self.context);
                return llvm.Value.constInt(int_type, @intCast(val), false);
            },
            .binary_op => |binop| {
                const lhs = try self.generateExpr(binop.left.*);
                const rhs = try self.generateExpr(binop.right.*);
                
                return switch (binop.op) {
                    .add => self.builder.buildAdd(lhs, rhs, "add"),
                    .sub => self.builder.buildSub(lhs, rhs, "sub"),
                    .mul => self.builder.buildMul(lhs, rhs, "mul"),
                    .div => self.builder.buildSDiv(lhs, rhs, "div"),
                    else => error.UnsupportedOperation,
                };
            },
            // 其他表达式...
            else => error.UnsupportedExpression,
        };
    }
    
    fn toLLVMType(self: *LLVMBackend, ty: ast.Type) !*llvm.Type {
        return switch (ty) {
            .i32 => llvm.Type.int32Type(self.context),
            .i64 => llvm.Type.int64Type(self.context),
            .f32 => llvm.Type.floatType(self.context),
            .f64 => llvm.Type.doubleType(self.context),
            .bool => llvm.Type.int1Type(self.context),
            .void_type => llvm.Type.voidType(self.context),
            .string => llvm.Type.pointerType(llvm.Type.int8Type(self.context), 0),
            else => error.UnsupportedType,
        };
    }
};
```

---

## 📊 实施计划

### Phase 1: 准备工作（Day 1-2）

#### Day 1: 依赖配置
- [x] 创建 `build.zig.zon`
- [x] 添加 llvm-zig 依赖
- [ ] 更新 `build.zig`
- [ ] 测试依赖拉取：`zig build`

#### Day 2: 项目结构
- [ ] 创建 `src/llvm_backend.zig`
- [ ] 实现基础结构体
- [ ] 添加后端选择参数解析

### Phase 2: 最小可行实现（Day 3-7）

#### Day 3: Hello World
```paw
fn main() -> i32 { return 42; }
```
- [ ] 实现函数声明
- [ ] 实现 return 语句
- [ ] 实现整数字面量
- [ ] 生成可执行文件

**测试**:
```bash
./zig-out/bin/pawc tests/llvm_hello.paw --backend=llvm
./output
echo $?  # 应该输出 42
```

#### Day 4-5: 变量和算术
```paw
fn main() -> i32 {
    let x = 10;
    let y = 20;
    return x + y;
}
```
- [ ] 实现 let 声明（alloca, store）
- [ ] 实现变量引用（load）
- [ ] 实现二元运算（add, sub, mul, div）

**测试**:
```bash
./zig-out/bin/pawc tests/llvm_arithmetic.paw --backend=llvm
./output
echo $?  # 应该输出 30
```

#### Day 6-7: 函数调用
```paw
fn add(a: i32, b: i32) -> i32 { return a + b; }
fn main() -> i32 { return add(10, 20); }
```
- [ ] 实现多函数定义
- [ ] 实现函数调用
- [ ] 实现参数传递

**测试**:
```bash
./zig-out/bin/pawc tests/llvm_function.paw --backend=llvm
./output
echo $?  # 应该输出 30
```

### Phase 3: 测试和完善（Day 8-10）

#### Day 8: 双后端测试
- [ ] 对比 C 后端和 LLVM 后端输出
- [ ] 确保所有基础测试在两个后端都通过
- [ ] 修复发现的问题

#### Day 9: 文档编写
- [ ] 编写 LLVM 后端使用文档
- [ ] 更新 README.md
- [ ] 编写 CHANGELOG.md
- [ ] 创建 RELEASE_NOTES_v0.1.4.md

#### Day 10: 发布准备
- [ ] 运行完整测试套件
- [ ] 创建测试示例
- [ ] 打标签并发布

---

## 🧪 测试策略

### 测试文件

#### tests/llvm_hello.paw
```paw
fn main() -> i32 {
    return 42;
}
```

#### tests/llvm_arithmetic.paw
```paw
fn main() -> i32 {
    let a = 10;
    let b = 20;
    let sum = a + b;
    return sum;
}
```

#### tests/llvm_function.paw
```paw
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

fn multiply(a: i32, b: i32) -> i32 {
    return a * b;
}

fn main() -> i32 {
    let x = add(10, 20);
    let y = multiply(3, 4);
    return x + y;
}
```

### 自动化测试脚本

```bash
#!/bin/bash
# test_backends.sh - 测试两个后端的一致性

echo "Testing C backend vs LLVM backend..."

for test in tests/llvm_*.paw; do
    echo "Testing $test..."
    
    # C 后端
    ./zig-out/bin/pawc "$test"
    gcc output.c -o output_c
    result_c=$(./output_c; echo $?)
    
    # LLVM 后端
    ./zig-out/bin/pawc "$test" --backend=llvm
    result_llvm=$(./output; echo $?)
    
    # 对比结果
    if [ "$result_c" -eq "$result_llvm" ]; then
        echo "✓ $test: PASS (C: $result_c, LLVM: $result_llvm)"
    else
        echo "✗ $test: FAIL (C: $result_c, LLVM: $result_llvm)"
        exit 1
    fi
done

echo "All tests passed!"
```

---

## 📈 功能范围

### v0.1.4 支持的功能 ✅

- ✅ 函数声明（无泛型）
- ✅ 基础类型（i32, i64, f32, f64, bool）
- ✅ 整数/浮点字面量
- ✅ 变量声明和赋值
- ✅ 二元运算（+, -, *, /）
- ✅ 函数调用
- ✅ return 语句

### v0.1.4 暂不支持 ❌

- ❌ 泛型（留给 v0.2.0）
- ❌ 结构体（留给 v0.2.0）
- ❌ 字符串（留给 v0.2.0）
- ❌ 数组（留给 v0.2.0）
- ❌ 控制流（if/loop，留给 v0.2.0）
- ❌ 模块系统（留给 v0.2.0）
- ❌ 标准库调用（留给 v0.2.0）

**原则**: 先做最简单的，确保基础架构正确，再逐步扩展。

---

## 📋 发布检查清单

### 代码质量
- [ ] LLVM 后端编译通过
- [ ] C 后端仍然正常工作
- [ ] 3个基础测试通过（hello, arithmetic, function）
- [ ] 无内存泄漏

### 文档完整性
- [ ] README.md 更新
- [ ] CHANGELOG.md 完整
- [ ] RELEASE_NOTES_v0.1.4.md 详细
- [ ] LLVM 后端使用指南

### 发布流程
- [ ] 所有更改已提交
- [ ] 创建 v0.1.4 标签
- [ ] 推送到 GitHub
- [ ] 创建 GitHub Release

---

## 🎯 成功标准

### MVP 成功标准
1. ✅ 能用 LLVM 后端编译最简单的程序
2. ✅ 生成的程序能正确执行
3. ✅ C 后端和 LLVM 后端结果一致
4. ✅ 构建系统正常工作

### 质量标准
1. ✅ 零崩溃
2. ✅ 清晰的错误信息
3. ✅ 完整的文档
4. ✅ 可重现的构建

---

## 🔮 后续计划（v0.2.0）

v0.1.4 完成后，v0.2.0 将继续完善 LLVM 后端：

### v0.2.0 目标
- 控制流（if/else, loop）
- 结构体和类型系统
- 字符串支持
- 泛型单态化
- 模块系统
- 标准库集成

**预计时间**: 1-2 个月

---

## 💡 设计决策

### 为什么从最简单的开始？

1. **降低风险** - 小步快跑，快速验证
2. **快速反馈** - 尽早发现问题
3. **构建信心** - 成功经验激励继续
4. **清晰架构** - 简单案例理清设计

### 为什么保留 C 后端？

1. **稳定性** - C 后端已验证
2. **对比测试** - 验证 LLVM 后端正确性
3. **回退方案** - LLVM 有问题可切回
4. **渐进迁移** - 给用户适应时间

### 为什么不支持泛型？

泛型涉及：
- 单态化逻辑
- 名称修饰
- 复杂的类型系统

先把基础打牢，再处理复杂特性。

---

## 📚 参考资源

### 学习材料
- [LLVM Kaleidoscope 教程](https://llvm.org/docs/tutorial/)
- [llvm-zig 仓库](https://github.com/kassane/llvm-zig)
- [LLVM C API 文档](https://llvm.org/doxygen/group__LLVMC.html)

### 示例项目
- Zig 编译器（自身使用 LLVM）
- Rust 编译器（rustc）
- Crystal 语言

---

## 🚀 开始实施

### 今天的行动
```bash
# 1. 拉取 LLVM 依赖
zig fetch --save=llvm git+https://github.com/kassane/llvm-zig

# 2. 更新 build.zig
# 3. 创建 llvm_backend.zig
# 4. 实现最小 Hello World
```

### 本周目标
- ✅ Day 1-2: 配置完成
- 🎯 Day 3: Hello World 工作
- 🎯 Day 4-5: 算术表达式工作
- 🎯 Day 6-7: 函数调用工作

### 发布目标
**v0.1.4 Beta**: 10天后  
**v0.1.4 正式版**: 2-3周后

---

**让我们开始构建 PawLang 的 LLVM 后端吧！** 🎉

