# 泛型类型系统完善计划

**目标**: 实现完整的泛型类型检查，支持函数调用参数验证  
**版本**: v0.1.3  
**预计时间**: 2-3小时

---

## 🎯 目标

### 要支持的场景

```paw
// 1. 泛型函数调用
fn identity<T>(x: T) -> T { x }

let x = identity(42);     // T = i32, 验证参数数量和类型
let y = identity(42, 10); // 错误: 参数过多

// 2. 泛型函数类型检查
fn add<T>(a: T, b: T) -> T { a + b }

let sum = add(10, 20);      // OK: 两个i32
let bad = add(10, "hello"); // 错误: T不能同时是i32和string

// 3. 非泛型函数验证（已实现）
fn multiply(a: i32, b: i32) -> i32 { a * b }

let product = multiply(5);  // 错误: 参数不足
```

---

## 📋 实现计划

### Phase 1: 泛型类型推导和统一

#### 1.1 添加泛型类型推导
在TypeChecker中添加类型推导函数：

```zig
/// 从函数调用推导泛型类型参数
fn inferGenericTypes(
    self: *TypeChecker,
    func: FunctionDecl,
    call_args: []Expr,
    scope: *StringHashMap(Type)
) ![]Type {
    var inferred_types = ArrayList(Type).init(self.allocator);
    
    // 为每个类型参数创建映射
    var type_map = StringHashMap(Type).init(self.allocator);
    defer type_map.deinit();
    
    // 从参数推导类型
    for (func.params, call_args) |param, arg| {
        const arg_type = try self.checkExpr(arg, scope);
        
        if (param.type == .generic) {
            const type_param_name = param.type.generic;
            
            if (type_map.get(type_param_name)) |existing| {
                // 类型参数已推导，检查一致性
                if (!existing.eql(arg_type)) {
                    return error.TypeParameterMismatch;
                }
            } else {
                // 第一次推导此类型参数
                try type_map.put(type_param_name, arg_type);
            }
        }
    }
    
    // 按顺序收集推导的类型
    for (func.type_params) |param_name| {
        if (type_map.get(param_name)) |inferred| {
            try inferred_types.append(inferred);
        }
    }
    
    return inferred_types.toOwnedSlice();
}
```

#### 1.2 更新函数调用检查

```zig
.call => |call| blk: {
    if (call.callee.* == .identifier) {
        const func_name = call.callee.identifier;
        
        if (self.function_table.get(func_name)) |func| {
            // 检查参数数量
            if (call.args.len != func.params.len) {
                const err_msg = try std.fmt.allocPrint(
                    self.allocator,
                    "Error: Function '{s}' expects {d} arguments, but got {d}",
                    .{func_name, func.params.len, call.args.len}
                );
                try self.errors.append(err_msg);
                break :blk ast.Type.void;
            }
            
            if (func.type_params.len > 0) {
                // 泛型函数：推导类型参数
                const inferred = try self.inferGenericTypes(func, call.args, scope);
                defer self.allocator.free(inferred);
                
                // 返回替换后的返回类型
                break :blk try self.substituteType(func.return_type, func.type_params, inferred);
            } else {
                // 非泛型函数：检查参数类型
                for (call.args, 0..) |arg, i| {
                    const arg_type = try self.checkExpr(arg, scope);
                    const param_type = func.params[i].type;
                    
                    if (!self.isTypeCompatible(arg_type, param_type)) {
                        const err_msg = try std.fmt.allocPrint(
                            self.allocator,
                            "Error: Argument {d} type mismatch in '{s}'",
                            .{i + 1, func_name}
                        );
                        try self.errors.append(err_msg);
                    }
                }
                
                break :blk func.return_type;
            }
        }
    }
    
    break :blk ast.Type.i32;
},
```

#### 1.3 添加类型替换函数

```zig
/// 将泛型类型参数替换为具体类型
fn substituteType(
    self: *TypeChecker,
    ty: Type,
    type_params: [][]const u8,
    type_args: []Type
) !Type {
    switch (ty) {
        .generic => |name| {
            // 查找对应的类型参数
            for (type_params, type_args) |param, arg| {
                if (std.mem.eql(u8, name, param)) {
                    return arg;
                }
            }
            return ty;
        },
        .generic_instance => |gi| {
            // 递归替换类型参数
            var new_args = ArrayList(Type).init(self.allocator);
            for (gi.type_args) |arg| {
                const substituted = try self.substituteType(arg, type_params, type_args);
                try new_args.append(substituted);
            }
            return Type{
                .generic_instance = .{
                    .name = gi.name,
                    .type_args = try new_args.toOwnedSlice(),
                },
            };
        },
        else => return ty,
    }
}
```

---

## 🧪 测试用例

### 测试1: 泛型函数参数数量检查
```paw
fn identity<T>(x: T) -> T { x }

let x = identity(42);      // ✅ OK
let y = identity(42, 10);  // ❌ Error: expects 1 argument, got 2
```

### 测试2: 泛型类型统一
```paw
fn add<T>(a: T, b: T) -> T { a + b }

let sum1 = add(10, 20);      // ✅ OK: T = i32
let sum2 = add(10, "hello"); // ❌ Error: T cannot be both i32 and string
```

### 测试3: 混合泛型和具体类型
```paw
fn wrap<T>(value: T, count: i32) -> T { value }

let x = wrap(42, 10);       // ✅ OK: T = i32, count = i32
let y = wrap(42);           // ❌ Error: expects 2 arguments
let z = wrap(42, "hi");     // ❌ Error: count expects i32, got string
```

---

## 🔧 实现步骤

### Step 1: 添加类型推导函数（30分钟）
- [ ] 实现 `inferGenericTypes()`
- [ ] 处理类型参数映射
- [ ] 检查类型一致性

### Step 2: 添加类型替换函数（20分钟）
- [ ] 实现 `substituteType()`
- [ ] 处理嵌套泛型
- [ ] 递归替换

### Step 3: 更新函数调用检查（30分钟）
- [ ] 修改 `.call` 分支
- [ ] 区分泛型和非泛型函数
- [ ] 添加详细错误消息

### Step 4: 测试和修复（60分钟）
- [ ] 创建测试文件
- [ ] 运行所有测试
- [ ] 修复发现的问题
- [ ] 确保25/25通过

### Step 5: 文档更新（20分钟）
- [ ] 更新 CHANGELOG.md
- [ ] 更新 RELEASE_NOTES_v0.1.3.md
- [ ] 添加使用示例

---

## 📝 错误消息设计

### 参数数量错误
```
Error: Function 'identity' expects 1 argument, but got 2
  let y = identity(42, 10);
          ^^^^^^^^^^^^^^^^
```

### 类型不匹配错误
```
Error: Type parameter 'T' cannot be unified
  In function 'add<T>(a: T, b: T) -> T'
  Argument 1: i32
  Argument 2: string
  
  let bad = add(10, "hello");
            ^^^^^^^^^^^^^^^^
```

### 具体参数类型错误
```
Error: Argument 2 type mismatch in 'wrap'
  Expected: i32
  Got: string
  
  let z = wrap(42, "hi");
               ^^  ^^^^
```

---

## ⚠️ 注意事项

### 1. 保持向后兼容
所有现有测试必须继续通过

### 2. 泛型实例化
不影响现有的泛型实例化逻辑（在generics.zig中）

### 3. 错误恢复
即使类型检查失败，也要继续检查其他代码

---

## 🎯 成功标准

- [ ] 25/25 测试通过
- [ ] 参数数量验证工作
- [ ] 泛型类型统一工作
- [ ] 错误消息清晰
- [ ] 无性能退化
- [ ] 文档完整

---

## 🚀 开始实现

准备好了吗？让我们完善泛型类型系统！

