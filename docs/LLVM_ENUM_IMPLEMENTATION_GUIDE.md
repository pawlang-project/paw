# LLVM 后端完整枚举实现指南

**日期**: 2025-10-13  
**目标**: 实现完整的枚举数据结构支持  
**预计时间**: 4-6小时（剩余工作）

---

## 📋 已完成的工作

✅ **数据结构设计**:
```zig
// 已添加到 llvm_native_backend.zig

pub const EnumVariantInfo = struct {
    name: []const u8,
    fields: []ast.Type,
    tag: usize,
};

pub const EnumInfo = struct {
    name: []const u8,
    variants: []EnumVariantInfo,
    llvm_type: ?llvm.TypeRef = null,
};

// 添加到 LLVMNativeBackend:
enum_types: std.StringHashMap(EnumInfo),
```

✅ **初始化和清理**: 已添加到 init() 和 deinit()

---

## 🔧 需要实现的功能

### 1. 注册枚举类型 (registerEnumType)

**位置**: `src/llvm_native_backend.zig` - 在 `registerStructType` 附近

**代码**:
```zig
/// 🆕 v0.2.0: 注册枚举类型信息
fn registerEnumType(self: *LLVMNativeBackend, enum_name: []const u8, enum_type: anytype) !void {
    const arena_alloc = self.arena.allocator();
    
    // 收集variant信息
    var variants = std.ArrayList(EnumVariantInfo){};
    defer variants.deinit(arena_alloc);
    
    for (enum_type.variants, 0..) |variant, index| {
        try variants.append(arena_alloc, EnumVariantInfo{
            .name = variant.name,
            .fields = variant.fields,
            .tag = index,
        });
    }
    
    const variants_owned = try variants.toOwnedSlice(arena_alloc);
    
    // 创建LLVM枚举类型 (struct { tag: i32, data: [largest_size x i8] })
    // 简化实现：固定大小union（假设最大32字节）
    const tag_type = self.context.i32Type();
    const data_type = llvm.LLVMArrayType(self.context.i8Type(), 32);  // 32字节union
    
    var struct_fields = [_]llvm.TypeRef{ tag_type, data_type };
    const llvm_enum_type = llvm.LLVMStructType(&struct_fields, 2, 0);  // non-packed
    
    const enum_info = EnumInfo{
        .name = enum_name,
        .variants = variants_owned,
        .llvm_type = llvm_enum_type,
    };
    
    try self.enum_types.put(enum_name, enum_info);
}
```

**调用位置**: 在 `generateDecl` 中处理 enum 定义时：
```zig
.type_decl => |type_decl| {
    switch (type_decl.value) {
        .enum_type => |enum_type| {
            // 注册枚举类型
            try self.registerEnumType(type_decl.name, enum_type);
            
            // 为每个variant生成构造器
            for (enum_type.variants, 0..) |variant, variant_index| {
                try self.generateEnumConstructor(type_decl.name, variant, variant_index);
            }
        },
        // ... struct等其他类型
    }
}
```

---

### 2. 更新枚举构造器 (generateEnumConstructor)

**替换现有的简化实现**:

```zig
fn generateEnumConstructor(self: *LLVMNativeBackend, enum_name: []const u8, variant: ast.EnumVariant, variant_index: usize) !void {
    // 构造函数名
    var constructor_name = std.ArrayList(u8){};
    defer constructor_name.deinit(self.allocator);
    
    try constructor_name.appendSlice(self.allocator, enum_name);
    try constructor_name.appendSlice(self.allocator, "_");
    try constructor_name.appendSlice(self.allocator, variant.name);
    
    const func_name_temp = try constructor_name.toOwnedSlice(self.allocator);
    defer self.allocator.free(func_name_temp);
    const func_name = try self.arena.allocator().dupe(u8, func_name_temp);
    
    // 获取枚举类型
    const enum_info = self.enum_types.get(enum_name) orelse {
        return error.EnumNotFound;
    };
    const enum_type = enum_info.llvm_type.?;
    
    // 参数类型
    var param_types = std.ArrayList(llvm.TypeRef){};
    defer param_types.deinit(self.allocator);
    
    for (variant.fields) |field_type| {
        const llvm_type = try self.toLLVMType(field_type);
        try param_types.append(self.allocator, llvm_type);
    }
    
    // 返回类型：完整的枚举struct
    const func_type = llvm.functionType(enum_type, param_types.items, false);
    
    // 创建函数
    const func_name_z = try self.allocator.dupeZ(u8, func_name);
    defer self.allocator.free(func_name_z);
    
    const llvm_func = self.module.addFunction(func_name_z, func_type);
    try self.functions.put(func_name, llvm_func);
    try self.function_return_types.put(func_name, enum_type);
    try self.enum_variants.put(variant.name, enum_name);
    
    // 生成函数体
    const entry_block = llvm.appendBasicBlock(self.context, llvm_func, "entry");
    self.builder.positionAtEnd(entry_block);
    
    // 分配临时变量存储enum
    const result_alloca = self.builder.buildAlloca(enum_type, "enum_result");
    
    // 设置tag字段
    const tag_ptr = self.builder.buildStructGEP(enum_type, result_alloca, 0, "tag_ptr");
    const tag_value = llvm.constI32(self.context, @intCast(variant_index));
    _ = self.builder.buildStore(tag_value, tag_ptr);
    
    // 设置data字段（如果有数据）
    if (variant.fields.len > 0) {
        const data_ptr = self.builder.buildStructGEP(enum_type, result_alloca, 1, "data_ptr");
        
        // 对于每个参数，存储到data中
        for (variant.fields, 0..) |_, i| {
            const param_value = llvm.LLVMGetParam(llvm_func, @intCast(i));
            
            // 计算data中的偏移（简化：按顺序存储）
            const offset_gep = self.builder.buildGEP(
                llvm.LLVMArrayType(self.context.i8Type(), 32),
                data_ptr,
                &[_]llvm.ValueRef{
                    llvm.constI32(self.context, 0),
                    llvm.constI32(self.context, @intCast(i * 4)),  // 假设每个字段4字节
                },
                2,
                "data_field_ptr"
            );
            
            // 存储值（需要bitcast）
            const dest_ptr = self.builder.buildBitCast(offset_gep, llvm.LLVMPointerType(llvm.LLVMTypeOf(param_value), 0), "dest_ptr");
            _ = self.builder.buildStore(param_value, dest_ptr);
        }
    }
    
    // 加载并返回结果
    const result = self.builder.buildLoad(enum_type, result_alloca, "result");
    _ = self.builder.buildRet(result);
}
```

---

### 3. 实现 enum_variant 表达式处理

**在 `generateExpr` 中添加**:

```zig
.enum_variant => |enum_var| blk: {
    // 构造enum variant调用：EnumName_VariantName(args...)
    
    // 构建函数名
    var func_name_buf = std.ArrayList(u8).init(self.allocator);
    defer func_name_buf.deinit();
    
    try func_name_buf.appendSlice(enum_var.enum_name);
    try func_name_buf.appendSlice("_");
    try func_name_buf.appendSlice(enum_var.variant);
    
    const func_name = try func_name_buf.toOwnedSlice();
    defer self.allocator.free(func_name);
    
    // 查找构造器函数
    const constructor_func = self.functions.get(func_name) orelse {
        return error.ConstructorNotFound;
    };
    
    // 生成参数
    var args = std.ArrayList(llvm.ValueRef).init(self.allocator);
    defer args.deinit();
    
    for (enum_var.args) |arg| {
        const arg_value = try self.generateExpr(arg);
        try args.append(arg_value);
    }
    
    // 调用构造器
    const result = self.builder.buildCall(
        llvm.LLVMGetElementType(llvm.LLVMTypeOf(constructor_func)),
        constructor_func,
        args.items.ptr,
        @intCast(args.items.len),
        "enum_call"
    );
    
    break :blk result;
},
```

---

### 4. 更新模式匹配支持数据提取

**修改 `generateIsExpr`**:

在匹配成功后，如果pattern有数据绑定，需要提取：

```zig
// 在 generateIsExpr 中，匹配成功的分支：

// 如果pattern需要提取数据
if (pattern需要提取数据) {
    // 获取data指针
    const enum_ptr = ...;  // 指向enum值的指针
    const enum_type = ...;  // enum的LLVM类型
    
    const data_ptr = self.builder.buildStructGEP(enum_type, enum_ptr, 1, "data_ptr");
    
    // 提取字段值
    // (具体实现取决于pattern的类型)
}
```

---

### 5. 更新 toLLVMType 处理枚举类型

```zig
fn toLLVMType(self: *LLVMNativeBackend, paw_type: ast.Type) !llvm.TypeRef {
    return switch (paw_type) {
        // ... 现有类型
        
        .named => |name| {
            // 检查是否是枚举类型
            if (self.enum_types.get(name)) |enum_info| {
                return enum_info.llvm_type.?;
            }
            
            // 检查是否是struct类型
            if (self.struct_types.get(name)) |struct_info| {
                return struct_info.llvm_type.?;
            }
            
            // 默认为i32
            return self.context.i32Type();
        },
        
        // ... 其他类型
    };
}
```

---

## 📝 实现顺序

1. ✅ 添加数据结构（已完成）
2. ⏳ 实现 `registerEnumType` 
3. ⏳ 更新 `generateEnumConstructor`
4. ⏳ 实现 `enum_variant` 表达式处理
5. ⏳ 更新 `toLLVMType`
6. ⏳ 更新 `generateIsExpr` 支持数据提取
7. ⏳ 测试基础功能
8. ⏳ 测试复杂场景

---

## 🧪 测试计划

### 测试1: 简单枚举
```paw
type Simple = enum {
    A(),
    B(),
}

fn test() -> i32 {
    let s = A();  // 应该工作
    return 0;
}
```

### 测试2: 带数据枚举
```paw
type Result = enum {
    Ok(i32),
    Err(i32),
}

fn test() -> i32 {
    let r = Ok(42);  // 应该工作
    return 0;
}
```

### 测试3: 模式匹配
```paw
fn test() -> i32 {
    let r = Ok(42);
    let value = r is {
        Ok(x) => x,      // 应该提取42
        Err(e) => 0,
    };
    return value;
}
```

### 测试4: 错误传播
```paw
fn get_value() -> Result {
    return Ok(42);
}

fn process() -> Result {
    let value = get_value()?;  // 应该工作
    return Ok(value + 10);
}
```

---

## ⚠️  已知限制和简化

### 当前实现的简化：

1. **Union大小固定**: 32字节固定大小
   - 优点：简单实现
   - 缺点：浪费内存
   - 改进：可以计算最大variant大小

2. **数据存储顺序**: 按参数顺序简单存储
   - 优点：实现简单
   - 缺点：对齐可能不正确
   - 改进：使用LLVM的union类型

3. **数据提取**: 简化的偏移计算
   - 优点：快速实现
   - 缺点：可能不支持所有类型
   - 改进：完整的类型系统集成

---

## 📊 工作量估算

| 任务 | 时间 | 难度 |
|------|------|------|
| registerEnumType | 30-45分钟 | 中 |
| generateEnumConstructor | 1-1.5小时 | 中高 |
| enum_variant处理 | 30-45分钟 | 中 |
| toLLVMType更新 | 15-20分钟 | 低 |
| 模式匹配数据提取 | 1-2小时 | 高 |
| 测试和调试 | 1-2小时 | 中 |
| **总计** | **4-6小时** | |

---

## 💡 替代方案

### 如果时间紧迫，可以采用渐进策略：

#### 阶段1: 最小可行实现 (1-2小时)
- ✅ 只实现enum_variant表达式处理
- ✅ 构造器返回简单标记
- ✅ 不支持数据提取
- ✅ 可以调用Ok(42)不崩溃

#### 阶段2: 基础数据支持 (2-3小时)
- ✅ 实现完整的构造器
- ✅ 存储数据到struct
- ✅ 简单的数据提取

#### 阶段3: 完整实现 (1-2小时)
- ✅ 复杂模式匹配
- ✅ 多字段variant
- ✅ 所有边界情况

---

## 🎯 建议

对于v0.2.0发布，推荐：

**选项A**: 只实现阶段1 (1-2小时)
- 修复构造器调用崩溃
- 基本功能可用
- 文档说明限制

**选项B**: 实现阶段1+2 (3-5小时)
- 基础数据支持
- 大部分场景可用
- 仍有少数限制

**选项C**: 完整实现 (6-8小时)
- 所有功能完整
- 无已知限制
- 可能引入新bug

---

## 📁 相关文件

**需要修改**:
- `src/llvm_native_backend.zig` - 主要实现
- `src/llvm_c_api.zig` - 可能需要新的API绑定

**参考**:
- `src/codegen.zig` - C后端的完整实现
- `src/ast.zig` - 枚举的AST定义

---

**当前状态**: 数据结构已添加，准备开始实现。

**下一步**: 选择一个方案并开始实现相应的功能。

