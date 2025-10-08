# 泛型方法实现状态

## 当前进度：50%

### ✅ 已完成（v0.1.1）

1. **AST 扩展**
   - `static_method_call` 节点已添加
   - 包含：`type_name`, `type_args`, `method_name`, `args`
   - 位置：`src/ast.zig`

2. **Parser 基础**
   - 可以解析 `Type<T>::method()` 语法
   - 位置：`src/parser.zig:1377`
   - 代码：
     ```zig
     if (self.match(.double_colon)) {
         const method_name = try self.consume(.identifier);
         // ... 生成 static_method_call
     }
     ```

3. **TypeChecker 基础**
   - 添加了 `static_method_call` 的类型检查
   - 位置：`src/typechecker.zig:384`

4. **CodeGen 基础**
   - 生成静态方法调用代码
   - 位置：`src/codegen.zig:813`
   - 代码：
     ```zig
     .static_method_call => |smc| {
         try self.output.appendSlice(smc.type_name);
         for (smc.type_args) |type_arg| {
             try self.output.appendSlice("_");
             // 类型名简化
         }
         try self.output.appendSlice("_");
         try self.output.appendSlice(smc.method_name);
         // ... 参数
     }
     ```

5. **Monomorphizer 扩展**
   - `GenericMethodInstance` 结构已定义
   - 位置：`src/generics.zig:78-89`
   - 包含：`struct_name`, `method_name`, `type_args`, `mangled_name`

### ⏳ 还需要实现

1. **Parser 扩展**
   - [ ] 正确解析 struct 中的方法定义
   - [ ] 当前：方法被解析了但没有关联到 struct
   - [ ] 需要：将方法存储在 `TypeDecl.kind.struct_type.methods`

2. **收集方法调用**
   - [ ] 遍历 AST 找到所有 `static_method_call`
   - [ ] 调用 `recordMethodInstance` 记录实例
   - [ ] 位置：需要在 `collectGenericStructInstances` 附近添加

3. **生成单态化方法**
   - [ ] 为每个 `GenericMethodInstance` 生成 C 函数
   - [ ] 处理返回类型替换（`Vec<T>` -> `Vec_i32`）
   - [ ] 处理 self 参数（如果有）
   - [ ] 位置：`generateMonomorphizedDeclarations` 中添加

4. **标准库实现**
   - [ ] 在 `src/std/prelude.paw` 中添加方法定义
   - [ ] `Vec<T>::new()`
   - [ ] `Box<T>::new(value: T)`

5. **测试**
   - [ ] 创建测试文件
   - [ ] 验证零内存泄漏
   - [ ] 验证所有功能正常

## 关键文件位置

- AST 定义：`src/ast.zig`
- Parser：`src/parser.zig`
- TypeChecker：`src/typechecker.zig`
- CodeGen：`src/codegen.zig`
- Generics：`src/generics.zig`
- 标准库：`src/std/prelude.paw`

## 技术难点

1. **方法定义存储**
   - Parser 已经解析了方法，但存储位置不正确
   - 需要确保方法与 struct 正确关联

2. **类型参数替换**
   - 方法返回类型 `Vec<T>` 需要替换为 `Vec_i32`
   - 需要在单态化时处理所有泛型类型引用

3. **self 参数**
   - 实例方法的 `self` 参数需要转换为指针
   - C 代码：`void StructName_method(StructName* self, ...)`

## 下一步行动

建议在新会话中继续，因为：
- 当前会话已经很长（125K+ tokens）
- 需要清晰的上下文和更好的性能
- 实现泛型方法需要大量代码编写和调试

## 已知问题

- Parser 中的方法定义可能没有正确存储到 `TypeDecl`
- 需要验证 `TypeDecl.kind.struct_type.methods` 是否正确填充
- 可能需要调试方法收集逻辑

