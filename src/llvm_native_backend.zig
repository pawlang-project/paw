//! LLVM Native Backend using direct C API
//! 
//! This backend uses our custom LLVM C API bindings to generate
//! native code directly through LLVM, without generating text IR.

const std = @import("std");
const ast = @import("ast.zig");
const llvm = @import("llvm_c_api.zig");

// 🆕 v0.1.7: LLVM 优化级别
pub const OptLevel = enum {
    O0,  // No optimization
    O1,  // Basic optimization
    O2,  // Standard optimization
    O3,  // Aggressive optimization
};

/// Struct字段信息
pub const FieldInfo = struct {
    name: []const u8,
    field_type: ast.Type,
    index: u32,
};

/// Struct类型信息
pub const StructInfo = struct {
    name: []const u8,
    fields: []FieldInfo,
    llvm_type: ?llvm.TypeRef = null,
};

/// 🆕 v0.2.0: 枚举variant信息
pub const EnumVariantInfo = struct {
    name: []const u8,
    fields: []ast.Type,
    tag: usize,
};

/// 🆕 v0.2.0: 枚举类型信息
pub const EnumInfo = struct {
    name: []const u8,
    variants: []EnumVariantInfo,
    llvm_type: ?llvm.TypeRef = null,
};

pub const LLVMNativeBackend = struct {
    allocator: std.mem.Allocator,
    arena: std.heap.ArenaAllocator,  // 🐛 v0.2.0: Arena for temporary strings
    context: llvm.Context,
    module: llvm.Module,
    builder: llvm.Builder,
    
    // Symbol tables
    functions: std.StringHashMap(llvm.ValueRef),
    variables: std.StringHashMap(llvm.ValueRef),
    variable_types: std.StringHashMap(llvm.TypeRef),  // Track variable types for load/store
    function_return_types: std.StringHashMap(llvm.TypeRef),  // 🐛 修复: 跟踪函数返回类型
    enum_variants: std.StringHashMap([]const u8),  // 🐛 v0.2.0: variant名 -> enum类型名
    
    // 🆕 v0.3.0: Struct类型系统
    struct_types: std.StringHashMap(StructInfo),  // struct名 -> struct信息
    variable_struct_types: std.StringHashMap([]const u8),  // 变量名 -> struct类型名
    
    // 🆕 v0.2.0: Enum类型系统
    enum_types: std.StringHashMap(EnumInfo),  // enum名 -> enum信息
    
    // Current function context
    current_function: ?llvm.ValueRef,
    
    // Loop context for break/continue
    current_loop_exit: ?llvm.BasicBlockRef,
    current_loop_continue: ?llvm.BasicBlockRef,
    
    // 🆕 v0.1.7: Optimization level
    opt_level: OptLevel,
    
    /// 初始化 LLVM 后端
    /// 创建 LLVM 上下文、模块和构建器
    /// 🆕 v0.1.7: 添加优化级别参数
    pub fn init(allocator: std.mem.Allocator, module_name: []const u8, opt_level: OptLevel) !LLVMNativeBackend {
        const context = llvm.Context.create();
        
        const module_name_z = try allocator.dupeZ(u8, module_name);
        defer allocator.free(module_name_z);
        
        const module = context.createModule(module_name_z);
        const builder = context.createBuilder();
        
        const arena = std.heap.ArenaAllocator.init(allocator);
        
        return LLVMNativeBackend{
            .allocator = allocator,
            .arena = arena,  // 🐛 v0.2.0: 初始化 arena
            .context = context,
            .module = module,
            .builder = builder,
            .functions = std.StringHashMap(llvm.ValueRef).init(allocator),
            .variables = std.StringHashMap(llvm.ValueRef).init(allocator),
            .variable_types = std.StringHashMap(llvm.TypeRef).init(allocator),
            .function_return_types = std.StringHashMap(llvm.TypeRef).init(allocator),  // 🐛 修复: 初始化返回类型映射
            .enum_variants = std.StringHashMap([]const u8).init(allocator),  // 🐛 v0.2.0: 初始化variant映射
            .struct_types = std.StringHashMap(StructInfo).init(allocator),  // 🆕 v0.3.0: 初始化struct类型
            .variable_struct_types = std.StringHashMap([]const u8).init(allocator),  // 🆕 v0.3.0: 初始化变量struct类型
            .enum_types = std.StringHashMap(EnumInfo).init(allocator),  // 🆕 v0.2.0: 初始化enum类型
            .current_function = null,
            .current_loop_exit = null,
            .current_loop_continue = null,
            .opt_level = opt_level,  // 🆕 v0.1.7: 保存优化级别
        };
    }
    
    /// 释放 LLVM 后端资源
    pub fn deinit(self: *LLVMNativeBackend) void {
        // 🐛 v0.2.0: 使用 arena 管理所有临时字符串，一次性释放
        self.functions.deinit();
        self.variables.deinit();
        self.variable_types.deinit();
        self.function_return_types.deinit();
        self.enum_variants.deinit();
        self.struct_types.deinit();
        self.variable_struct_types.deinit();
        self.enum_types.deinit();
        
        self.builder.dispose();
        self.module.dispose();
        self.context.dispose();
        
        // 最后释放 arena（释放所有临时分配）
        self.arena.deinit();
    }
    
    // ============================================================================
    // 辅助函数
    // ============================================================================
    
    /// 创建 null-terminated 字符串的辅助函数
    fn createCString(self: *LLVMNativeBackend, str: []const u8) ![:0]const u8 {
        return try self.allocator.dupeZ(u8, str);
    }
    
    /// 创建函数类型的辅助函数
    fn createFunctionType(self: *LLVMNativeBackend, param_count: usize) llvm.TypeRef {
        const i32_type = self.context.i32Type();
        var param_types = std.ArrayList(llvm.TypeRef){};
        defer param_types.deinit(self.allocator);
        
        var i: usize = 0;
        while (i < param_count) : (i += 1) {
            param_types.append(self.allocator, i32_type) catch unreachable;
        }
        
        return llvm.functionType(i32_type, param_types.items, false);
    }
    
    /// 保存和恢复循环上下文
    const LoopContext = struct {
        exit: ?llvm.BasicBlockRef,
        continue_block: ?llvm.BasicBlockRef,
    };
    
    fn saveLoopContext(self: *LLVMNativeBackend) LoopContext {
        return LoopContext{
            .exit = self.current_loop_exit,
            .continue_block = self.current_loop_continue,
        };
    }
    
    fn restoreLoopContext(self: *LLVMNativeBackend, ctx: LoopContext) void {
        self.current_loop_exit = ctx.exit;
        self.current_loop_continue = ctx.continue_block;
    }
    
    // ============================================================================
    // 代码生成主函数
    // ============================================================================
    
    pub fn generate(self: *LLVMNativeBackend, program: ast.Program) ![]const u8 {
        // Generate all declarations
        for (program.declarations) |decl| {
            try self.generateDecl(decl);
        }
        
        // Verify module (disabled for now due to linking complexity)
        // self.module.verify() catch |err| {
        //     std.debug.print("❌ LLVM module verification failed\n", .{});
        //     return err;
        // };
        
        // Get IR string
        const ir = self.module.toString();
        
        // Copy to owned slice (caller must free with LLVMDisposeMessage)
        return try self.allocator.dupe(u8, ir);
    }
    
    fn generateDecl(self: *LLVMNativeBackend, decl: ast.TopLevelDecl) !void {
        switch (decl) {
            .function => |func| try self.generateFunction(func),
            .type_decl => |type_decl| {
                // 处理type声明
                switch (type_decl.kind) {
                    .enum_type => |enum_type| {
                        // 🆕 v0.2.0: 先注册枚举类型信息
                        try self.registerEnumType(type_decl.name, enum_type);
                        
                        // 生成enum构造器函数
                        for (enum_type.variants, 0..) |variant, variant_index| {
                            try self.generateEnumConstructor(type_decl.name, variant, variant_index);
                        }
                    },
                    .struct_type => |struct_type| {
                        // 🆕 v0.3.0: 注册struct类型信息
                        try self.registerStructType(type_decl.name, struct_type);
                    },
                    else => {
                        // 其他类型声明暂时跳过
                    },
                }
            },
            else => {
                // TODO: Handle other declaration types
            },
        }
    }
    
    /// 🆕 v0.2.0: 注册枚举类型信息
    fn registerEnumType(self: *LLVMNativeBackend, enum_name: []const u8, enum_type: anytype) !void {
        const arena_alloc = self.arena.allocator();
        
        // 收集variant信息
        var variants_list = std.ArrayList(EnumVariantInfo){};
        defer variants_list.deinit(arena_alloc);
        
        for (enum_type.variants, 0..) |variant, index| {
            try variants_list.append(arena_alloc, EnumVariantInfo{
                .name = variant.name,
                .fields = variant.fields,
                .tag = index,
            });
        }
        
        const variants_owned = try variants_list.toOwnedSlice(arena_alloc);
        
        // 创建LLVM枚举类型: struct { tag: i32, data: [32 x i8] }
        // 简化实现：固定32字节union存储数据
        const tag_type = self.context.i32Type();
        const data_type = llvm.LLVMArrayType(self.context.i8Type(), 32);
        
        var struct_fields = [_]llvm.TypeRef{ tag_type, data_type };
        const llvm_enum_type = llvm.LLVMStructType(&struct_fields, 2, 0);
        
        const enum_info = EnumInfo{
            .name = enum_name,
            .variants = variants_owned,
            .llvm_type = llvm_enum_type,
        };
        
        try self.enum_types.put(enum_name, enum_info);
    }
    
    /// 🆕 v0.3.0: 注册struct类型信息
    fn registerStructType(self: *LLVMNativeBackend, struct_name: []const u8, struct_type: anytype) !void {
        // 🐛 v0.2.0: 使用 arena allocator 创建字段信息数组
        const arena_alloc = self.arena.allocator();
        var fields = std.ArrayList(FieldInfo){};
        defer fields.deinit(arena_alloc);
        
        for (struct_type.fields, 0..) |field, index| {
            try fields.append(arena_alloc, FieldInfo{
                .name = field.name,
                .field_type = field.type,
                .index = @intCast(index),
            });
        }
        
        // 使用相同的 arena allocator
        const fields_owned = try fields.toOwnedSlice(arena_alloc);
        
        // 创建LLVM struct类型
        var field_types = std.ArrayList(llvm.TypeRef){};
        defer field_types.deinit(self.allocator);
        
        for (struct_type.fields) |field| {
            const llvm_type = try self.toLLVMType(field.type);
            try field_types.append(self.allocator, llvm_type);
        }
        
        const llvm_struct_type = llvm.LLVMStructType(
            field_types.items.ptr,
            @intCast(field_types.items.len),
            0  // not packed
        );
        
        // 保存struct信息
        const struct_info = StructInfo{
            .name = struct_name,
            .fields = fields_owned,
            .llvm_type = llvm_struct_type,
        };
        
        try self.struct_types.put(struct_name, struct_info);
        
        // 🆕 v0.3.0: 生成struct的方法
        // 暂时跳过泛型struct（Vec<T>等）
        const is_generic = std.mem.indexOf(u8, struct_name, "<") != null;
        if (!is_generic) {
            for (struct_type.methods) |method| {
                try self.generateStructMethod(struct_name, method, struct_info.llvm_type.?);
            }
        }
    }
    
    /// 🆕 v0.3.0: 生成struct方法
    fn generateStructMethod(self: *LLVMNativeBackend, struct_name: []const u8, method: ast.FunctionDecl, struct_llvm_type: llvm.TypeRef) !void {
        _ = struct_llvm_type;  // 暂时未使用
        
        // 构造方法名: StructName_methodName
        var method_name_builder = std.ArrayList(u8){};
        defer method_name_builder.deinit(self.allocator);
        
        try method_name_builder.appendSlice(self.allocator, struct_name);
        try method_name_builder.appendSlice(self.allocator, "_");
        try method_name_builder.appendSlice(self.allocator, method.name);
        
        const full_method_name_temp = try method_name_builder.toOwnedSlice(self.allocator);
        defer self.allocator.free(full_method_name_temp);
        
        // 🐛 v0.2.0: 使用 arena allocator（自动管理内存）
        const full_method_name = try self.arena.allocator().dupe(u8, full_method_name_temp);
        
        // 获取返回类型
        // 🆕 v0.3.0: 如果返回类型是struct（或泛型struct），返回指针
        var return_type = try self.toLLVMType(method.return_type);
        
        // 🆕 v0.3.0: 检查返回类型是否是struct（精确策略）
        if (method.return_type == .generic_instance) {
            // 泛型实例（如Vec<T>）返回指针
            return_type = self.context.pointerType(0);
        } else if (method.return_type == .named) {
            const return_type_name = method.return_type.named;
            // 精确检查：必须是已注册的struct或当前struct
            if (self.struct_types.get(return_type_name)) |_| {
                return_type = self.context.pointerType(0);
            } else if (std.mem.eql(u8, return_type_name, struct_name)) {
                // 返回当前struct类型
                return_type = self.context.pointerType(0);
            }
            // 不使用首字母大写或包含<的策略，避免与enum冲突
        }
        
        // 获取参数类型（self参数特殊处理）
        var param_types = std.ArrayList(llvm.TypeRef){};
        defer param_types.deinit(self.allocator);
        
        for (method.params) |param| {
            if (std.mem.eql(u8, param.name, "self")) {
                // self参数是指向struct的指针
                try param_types.append(self.allocator, self.context.pointerType(0));
            } else {
                const param_type = try self.toLLVMType(param.type);
                try param_types.append(self.allocator, param_type);
            }
        }
        
        // 创建函数类型
        const func_type = llvm.functionType(return_type, param_types.items, false);
        
        // 创建函数
        const func_name_z = try self.allocator.dupeZ(u8, full_method_name);
        defer self.allocator.free(func_name_z);
        
        const llvm_func = self.module.addFunction(func_name_z, func_type);
        try self.functions.put(full_method_name, llvm_func);
        try self.function_return_types.put(full_method_name, return_type);
        
        // 设置当前函数上下文
        self.current_function = llvm_func;
        
        // 创建entry基本块
        const entry_block = llvm.appendBasicBlock(self.context, llvm_func, "entry");
        self.builder.positionAtEnd(entry_block);
        
        // 为每个参数创建alloca和store
        for (method.params, 0..) |param, i| {
            const param_value = llvm.LLVMGetParam(llvm_func, @intCast(i));
            
            const param_type = if (std.mem.eql(u8, param.name, "self"))
                self.context.pointerType(0)
            else
                try self.toLLVMType(param.type);
            
            // Allocate space for parameter and store it
            const alloca_name_z = try self.allocator.dupeZ(u8, param.name);
            defer self.allocator.free(alloca_name_z);
            const alloca = self.builder.buildAlloca(param_type, alloca_name_z);
            _ = self.builder.buildStore(param_value, alloca);
            
            try self.variables.put(param.name, alloca);
            try self.variable_types.put(param.name, param_type);
            
            // 🆕 如果是self参数，注册struct类型
            if (std.mem.eql(u8, param.name, "self")) {
                try self.variable_struct_types.put(param.name, struct_name);
            }
        }
        
        // 生成方法体
        for (method.body, 0..) |stmt, i| {
            const is_last = (i == method.body.len - 1);
            const is_non_void = method.return_type != .void;
            
            if (is_last and stmt == .expr and is_non_void) {
                const ret_value = try self.generateExpr(stmt.expr);
                _ = self.builder.buildRet(ret_value);
            } else {
                try self.generateStmt(stmt);
            }
        }
        
        // 清理上下文（关键！）
        self.current_function = null;
        self.variables.clearRetainingCapacity();
        self.variable_types.clearRetainingCapacity();
    }
    
    /// 🆕 v0.2.0: 生成enum构造器函数（完整实现）
    /// 例如: Ok(42) -> 生成返回完整enum struct的函数
    fn generateEnumConstructor(self: *LLVMNativeBackend, enum_name: []const u8, variant: ast.EnumVariant, variant_index: usize) !void {
        // 构造函数名: EnumName_VariantName
        var constructor_name = std.ArrayList(u8){};
        defer constructor_name.deinit(self.allocator);
        
        try constructor_name.appendSlice(self.allocator, enum_name);
        try constructor_name.appendSlice(self.allocator, "_");
        try constructor_name.appendSlice(self.allocator, variant.name);
        
        const func_name_temp = try constructor_name.toOwnedSlice(self.allocator);
        defer self.allocator.free(func_name_temp);
        
        const func_name = try self.arena.allocator().dupe(u8, func_name_temp);
        
        // 获取枚举类型信息
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
        
        // 🆕 返回类型：完整的枚举struct
        const func_type = llvm.functionType(enum_type, param_types.items, false);
        
        // 创建函数
        const func_name_z = try self.allocator.dupeZ(u8, func_name);
        defer self.allocator.free(func_name_z);
        
        const llvm_func = self.module.addFunction(func_name_z, func_type);
        try self.functions.put(func_name, llvm_func);
        try self.function_return_types.put(func_name, enum_type);
        
        // 注册variant名到enum类型名的映射
        try self.enum_variants.put(variant.name, enum_name);
        
        // 生成函数体
        const entry_block = llvm.appendBasicBlock(self.context, llvm_func, "entry");
        self.builder.positionAtEnd(entry_block);
        
        // 🆕 v0.2.0: 创建完整的enum struct
        // 分配临时变量存储enum
        const result_alloca = self.builder.buildAlloca(enum_type, "enum_result");
        
        // 设置tag字段 (field 0)
        const tag_ptr = self.builder.buildStructGEP(enum_type, result_alloca, 0, "tag_ptr");
        const tag_value = llvm.constI32(self.context, @intCast(variant_index));
        _ = self.builder.buildStore(tag_value, tag_ptr);
        
        // 设置data字段 (field 1) - 如果有数据
        if (variant.fields.len > 0) {
            const data_ptr = self.builder.buildStructGEP(enum_type, result_alloca, 1, "data_ptr");
            
            // 存储每个参数到data数组中
            for (variant.fields, 0..) |field_type, i| {
                const param_value = llvm.LLVMGetParam(llvm_func, @intCast(i));
                const param_llvm_type = try self.toLLVMType(field_type);
                
                // 计算偏移（简化：每个i32占4字节）
                const offset = i * 4;
                var indices = [_]llvm.ValueRef{
                    llvm.constI32(self.context, 0),
                    llvm.constI32(self.context, @intCast(offset)),
                };
                
                const field_ptr_z = try self.allocator.dupeZ(u8, "field_ptr");
                defer self.allocator.free(field_ptr_z);
                
                const field_ptr = self.builder.buildGEP(
                    llvm.LLVMArrayType(self.context.i8Type(), 32),
                    data_ptr,
                    &indices,
                    field_ptr_z
                );
                
                // bitcast到正确的类型指针
                const typed_ptr = self.builder.buildBitCast(
                    field_ptr,
                    llvm.LLVMPointerType(param_llvm_type, 0),
                    "typed_ptr"
                );
                
                _ = self.builder.buildStore(param_value, typed_ptr);
            }
        }
        
        // 加载并返回完整的enum struct
        const result = self.builder.buildLoad(enum_type, result_alloca, "result");
        _ = self.builder.buildRet(result);
    }
    
    fn generateFunction(self: *LLVMNativeBackend, func: ast.FunctionDecl) !void {
        // Get return type
        // 🆕 v0.3.0: 如果返回类型是struct（或泛型struct），返回指针
        var return_type = try self.toLLVMType(func.return_type);
        
        // 🆕 v0.3.0: 检查返回类型是否是struct（精确策略）
        if (func.return_type == .generic_instance) {
            // 泛型实例返回指针
            return_type = self.context.pointerType(0);
        } else if (func.return_type == .named) {
            const return_type_name = func.return_type.named;
            // 精确检查：必须是已注册的struct类型
            if (self.struct_types.get(return_type_name)) |_| {
                return_type = self.context.pointerType(0);
            }
            // 不使用首字母大写策略，避免与enum冲突
        }
        
        // Get parameter types
        var param_types = std.ArrayList(llvm.TypeRef){};
        defer param_types.deinit(self.allocator);
        
        for (func.params) |param| {
            const param_type = try self.toLLVMType(param.type);
            try param_types.append(self.allocator, param_type);
        }
        
        // Create function type
        const func_type = llvm.functionType(return_type, param_types.items, false);
        
        // Create null-terminated function name
        const func_name_z = try self.allocator.dupeZ(u8, func.name);
        defer self.allocator.free(func_name_z);
        
        // Add function to module
        const llvm_func = self.module.addFunction(func_name_z, func_type);
        try self.functions.put(func.name, llvm_func);
        try self.function_return_types.put(func.name, return_type);  // 🐛 修复: 存储返回类型
        
        // Set current function context
        self.current_function = llvm_func;
        
        // Create entry basic block
        const entry_block = llvm.appendBasicBlock(self.context, llvm_func, "entry");
        self.builder.positionAtEnd(entry_block);
        
        // Store parameters in variables map
        self.variables.clearRetainingCapacity();
        self.variable_types.clearRetainingCapacity();
        for (func.params, 0..) |param, i| {
            const param_value = llvm.LLVMGetParam(llvm_func, @intCast(i));
            const param_type = try self.toLLVMType(param.type);
            
            // Allocate space for parameter and store it
            const alloca_name_z = try self.allocator.dupeZ(u8, param.name);
            defer self.allocator.free(alloca_name_z);
            const alloca = self.builder.buildAlloca(param_type, alloca_name_z);
            _ = self.builder.buildStore(param_value, alloca);
            
            try self.variables.put(param.name, alloca);
            try self.variable_types.put(param.name, param_type);
        }
        
        // Generate function body
        // 🆕 v0.1.6: 特殊处理最后一个表达式语句 - 应该生成 return
        for (func.body, 0..) |stmt, i| {
            const is_last = (i == func.body.len - 1);
            const is_non_void = func.return_type != .void;
            
            // 如果是最后一个语句，且是表达式语句，且函数返回非void，生成return
            if (is_last and stmt == .expr and is_non_void) {
                const ret_value = try self.generateExpr(stmt.expr);
                _ = self.builder.buildRet(ret_value);
            } else {
                try self.generateStmt(stmt);
            }
        }
        
        // Clear function context
        self.current_function = null;
    }
    
    fn generateStmt(self: *LLVMNativeBackend, stmt: ast.Stmt) anyerror!void {
        switch (stmt) {
            .return_stmt => |maybe_val| {
                if (maybe_val) |val| {
                    const ret_value = try self.generateExpr(val);
                    _ = self.builder.buildRet(ret_value);
                } else {
                    _ = self.builder.buildRetVoid();
                }
            },
            .let_decl => |let_stmt| {
                if (let_stmt.init) |init_expr| {
                    var init_value = try self.generateExpr(init_expr);
                    
                    // Determine variable type
                    // 🆕 v0.3.0: 如果是struct_init，直接使用init_value的类型（避免转换问题）
                    const var_type = if (init_expr == .struct_init)
                        llvm.LLVMTypeOf(init_value)
                    else if (let_stmt.type) |typ|
                        try self.toLLVMType(typ)
                    else
                        llvm.LLVMTypeOf(init_value);
                    
                    // 🆕 v0.2.0: 如果初始值类型与变量类型不匹配，进行转换
                    const init_type = llvm.LLVMTypeOf(init_value);
                    if (init_type != var_type) {
                        // 检查是否都是指针类型
                        const init_is_ptr = llvm.LLVMGetTypeKind(init_type) == llvm.LLVMPointerTypeKind;
                        const var_is_ptr = llvm.LLVMGetTypeKind(var_type) == llvm.LLVMPointerTypeKind;
                        
                        if (init_is_ptr and var_is_ptr) {
                            // 都是指针，直接使用（不需要转换）
                            // 指针之间可以直接赋值
                        } else {
                            // 需要类型转换
                            const cast_name_z = try self.allocator.dupeZ(u8, "auto_cast");
                            defer self.allocator.free(cast_name_z);
                            
                            // 简化：假设 i8 -> i32 的扩展（最常见情况）
                            init_value = self.builder.buildZExt(init_value, var_type, cast_name_z);
                        }
                    }
                    
                    // Allocate space for variable
                    const alloca_name_z = try self.allocator.dupeZ(u8, let_stmt.name);
                    defer self.allocator.free(alloca_name_z);
                    const alloca = self.builder.buildAlloca(var_type, alloca_name_z);
                    
                    // Store initial value (已转换到正确类型)
                    _ = self.builder.buildStore(init_value, alloca);
                    
                    // Store pointer in variables map
                    try self.variables.put(let_stmt.name, alloca);
                    try self.variable_types.put(let_stmt.name, var_type);
                    
                    // 🆕 v0.3.0: 如果是struct类型，注册struct类型名
                    if (let_stmt.type) |typ| {
                        if (typ == .named) {
                            try self.variable_struct_types.put(let_stmt.name, typ.named);
                        }
                    } else if (init_expr == .struct_init) {
                        // 从struct_init推断类型名
                        const struct_name = init_expr.struct_init.type_name;
                        try self.variable_struct_types.put(let_stmt.name, struct_name);
                    }
                }
            },
            .assign => |assign_stmt| {
                // Handle assignment to existing variable
                if (assign_stmt.target == .identifier) {
                    const var_name = assign_stmt.target.identifier;
                    if (self.variables.get(var_name)) |var_ptr| {
                        const new_value = try self.generateExpr(assign_stmt.value);
                        _ = self.builder.buildStore(new_value, var_ptr);
                    } else {
                        std.debug.print("⚠️  Undefined variable in assignment: {s}\n", .{var_name});
                    }
                } else {
                    std.debug.print("⚠️  Complex assignment target not yet supported\n", .{});
                }
            },
            .compound_assign => |compound_stmt| {
                // Handle compound assignment (+=, -=, etc.)
                if (compound_stmt.target == .identifier) {
                    const var_name = compound_stmt.target.identifier;
                    if (self.variables.get(var_name)) |var_ptr| {
                        if (self.variable_types.get(var_name)) |var_type| {
                            // Load current value
                            const load_name_z = try self.allocator.dupeZ(u8, var_name);
                            defer self.allocator.free(load_name_z);
                            const current_value = self.builder.buildLoad(var_type, var_ptr, load_name_z);
                            
                            // Generate right-hand side value
                            const rhs_value = try self.generateExpr(compound_stmt.value);
                            
                            // Perform operation
                            const op_name_z = try self.allocator.dupeZ(u8, "compound_op");
                            defer self.allocator.free(op_name_z);
                            
                            const result = switch (compound_stmt.op) {
                                .add_assign => self.builder.buildAdd(current_value, rhs_value, op_name_z),
                                .sub_assign => self.builder.buildSub(current_value, rhs_value, op_name_z),
                                .mul_assign => self.builder.buildMul(current_value, rhs_value, op_name_z),
                                .div_assign => self.builder.buildSDiv(current_value, rhs_value, op_name_z),
                                else => current_value,
                            };
                            
                            // Store result back
                            _ = self.builder.buildStore(result, var_ptr);
                        }
                    } else {
                        std.debug.print("⚠️  Undefined variable in compound assignment: {s}\n", .{var_name});
                    }
                } else {
                    std.debug.print("⚠️  Complex compound assignment target not yet supported\n", .{});
                }
            },
            .expr => |expr| {
                _ = try self.generateExpr(expr);
            },
            .while_loop => |while_stmt| {
                try self.generateWhileLoop(.{
                    .condition = while_stmt.condition,
                    .body = while_stmt.body,
                });
            },
            .loop_stmt => |loop_stmt| {
                if (loop_stmt.condition) |cond| {
                    // loop condition { } - 条件循环
                    try self.generateWhileLoop(.{
                        .condition = cond,
                        .body = loop_stmt.body,
                    });
                } else if (loop_stmt.iterator) |iter| {
                    // loop item in collection { } - 迭代循环
                    try self.generateLoopIterator(iter, loop_stmt.body);
                } else {
                    // loop { } - 无限循环
                    try self.generateInfiniteLoop(loop_stmt.body);
                }
            },
            .break_stmt => |_| {
                if (self.current_loop_exit) |exit_block| {
                    _ = self.builder.buildBr(exit_block);
                }
            },
            .continue_stmt => {
                if (self.current_loop_continue) |continue_block| {
                    _ = self.builder.buildBr(continue_block);
                }
            },
            else => {
                // TODO: Handle other statement types
            },
        }
    }
    
    /// 生成 while 风格的条件循环
    /// 生成: while.cond -> while.body -> while.cond (循环) | while.exit
    fn generateWhileLoop(self: *LLVMNativeBackend, loop: struct { condition: ast.Expr, body: []ast.Stmt }) !void {
        const func = self.current_function orelse return error.NoCurrentFunction;
        
        // 创建基本块
        const cond_block = llvm.appendBasicBlock(self.context, func, "while.cond");
        const body_block = llvm.appendBasicBlock(self.context, func, "while.body");
        const exit_block = llvm.appendBasicBlock(self.context, func, "while.exit");
        
        // 保存并设置循环上下文
        const saved_ctx = self.saveLoopContext();
        defer self.restoreLoopContext(saved_ctx);
        
        self.current_loop_exit = exit_block;
        self.current_loop_continue = cond_block;
        
        // 跳转到条件块
        _ = self.builder.buildBr(cond_block);
        
        // 生成条件块
        self.builder.positionAtEnd(cond_block);
        const cond_value_raw = try self.generateExpr(loop.condition);
        // 🐛 修复: 确保条件是i1类型
        const cond_value = self.ensureI1ForBranch(cond_value_raw);
        _ = llvm.LLVMBuildCondBr(self.builder.ref, cond_value, body_block, exit_block);
        
        // 生成循环体
        self.builder.positionAtEnd(body_block);
        for (loop.body) |stmt| {
            try self.generateStmt(stmt);
        }
        _ = self.builder.buildBr(cond_block);
        
        // 继续从退出块执行
        self.builder.positionAtEnd(exit_block);
    }
    
    /// 生成 loop 迭代器（范围迭代）
    /// 生成: loop.cond -> loop.body -> loop.incr -> loop.cond (循环) | loop.exit
    fn generateLoopIterator(self: *LLVMNativeBackend, iter: ast.LoopIterator, body: []ast.Stmt) !void {
        const func = self.current_function orelse return error.NoCurrentFunction;
        
        // 只支持范围表达式
        if (iter.iterable != .range) {
            std.debug.print("⚠️  Only range iterators are supported in LLVM backend\n", .{});
            return;
        }
        
        const range = iter.iterable.range;
        const i32_type = self.context.i32Type();
        
        // 创建并初始化循环变量
        const iter_name_z = try self.createCString(iter.binding);
        defer self.allocator.free(iter_name_z);
        
        const iter_var = self.builder.buildAlloca(i32_type, iter_name_z);
        const start_value = try self.generateExpr(range.start.*);
        _ = self.builder.buildStore(start_value, iter_var);
        
        // 注册循环变量（作用域内有效）
        try self.variables.put(iter.binding, iter_var);
        try self.variable_types.put(iter.binding, i32_type);
        defer {
            _ = self.variables.remove(iter.binding);
            _ = self.variable_types.remove(iter.binding);
        }
        
        // 生成结束值
        const end_value = try self.generateExpr(range.end.*);
        
        // 创建基本块
        const cond_block = llvm.appendBasicBlock(self.context, func, "loop.cond");
        const body_block = llvm.appendBasicBlock(self.context, func, "loop.body");
        const incr_block = llvm.appendBasicBlock(self.context, func, "loop.incr");
        const exit_block = llvm.appendBasicBlock(self.context, func, "loop.exit");
        
        // 保存并设置循环上下文
        const saved_ctx = self.saveLoopContext();
        defer self.restoreLoopContext(saved_ctx);
        
        self.current_loop_exit = exit_block;
        self.current_loop_continue = incr_block;
        
        // 跳转到条件块
        _ = self.builder.buildBr(cond_block);
        
        // 生成条件块：检查 i < end 或 i <= end
        self.builder.positionAtEnd(cond_block);
        const current_value = self.builder.buildLoad(i32_type, iter_var, iter_name_z);
        const predicate = if (range.inclusive) llvm.IntPredicate.SLE else llvm.IntPredicate.SLT;
        const cond_value = self.builder.buildICmp(predicate, current_value, end_value, "loop_cond");
        _ = llvm.LLVMBuildCondBr(self.builder.ref, cond_value, body_block, exit_block);
        
        // 生成循环体
        self.builder.positionAtEnd(body_block);
        for (body) |stmt| {
            try self.generateStmt(stmt);
        }
        _ = self.builder.buildBr(incr_block);
        
        // 生成递增块：i = i + 1
        self.builder.positionAtEnd(incr_block);
        const current_value2 = self.builder.buildLoad(i32_type, iter_var, iter_name_z);
        const one = llvm.constI32(self.context, 1);
        const next_value = self.builder.buildAdd(current_value2, one, "loop_incr");
        _ = self.builder.buildStore(next_value, iter_var);
        _ = self.builder.buildBr(cond_block);
        
        // 继续从退出块执行
        self.builder.positionAtEnd(exit_block);
    }
    
    /// 生成无限循环
    /// 生成: loop.body -> loop.body (无限循环，只能通过 break 退出)
    fn generateInfiniteLoop(self: *LLVMNativeBackend, body: []ast.Stmt) !void {
        const func = self.current_function orelse return error.NoCurrentFunction;
        
        // 创建基本块
        const body_block = llvm.appendBasicBlock(self.context, func, "loop.body");
        const exit_block = llvm.appendBasicBlock(self.context, func, "loop.exit");
        
        // 保存并设置循环上下文
        const saved_ctx = self.saveLoopContext();
        defer self.restoreLoopContext(saved_ctx);
        
        self.current_loop_exit = exit_block;
        self.current_loop_continue = body_block;
        
        // 跳转到循环体
        _ = self.builder.buildBr(body_block);
        
        // 生成循环体（无限循环回自己）
        self.builder.positionAtEnd(body_block);
        for (body) |stmt| {
            try self.generateStmt(stmt);
        }
        _ = self.builder.buildBr(body_block);
        
        // 退出块（只能通过 break 到达）
        self.builder.positionAtEnd(exit_block);
    }
    
    fn generateExpr(self: *LLVMNativeBackend, expr: ast.Expr) anyerror!llvm.ValueRef {
        return switch (expr) {
            .int_literal => |val| blk: {
                const i32_type = self.context.i32Type();
                break :blk llvm.LLVMConstInt(i32_type, @intCast(val), 1);
            },
            .float_literal => |val| blk: {
                break :blk llvm.constDouble(self.context, val);
            },
            .bool_literal => |val| blk: {
                // 🐛 修复: bool应该使用i8而不是i1，与toLLVMType保持一致
                const i8_type = self.context.i8Type();
                break :blk llvm.LLVMConstInt(i8_type, if (val) 1 else 0, 0);
            },
            .char_literal => |val| blk: {
                const i32_type = self.context.i32Type();
                break :blk llvm.LLVMConstInt(i32_type, @intCast(val), 0);
            },
            .string_literal => |str| blk: {
                // Create null-terminated string
                const str_z = try self.allocator.dupeZ(u8, str);
                defer self.allocator.free(str_z);
                
                const name_z = try self.allocator.dupeZ(u8, "str");
                defer self.allocator.free(name_z);
                
                // Build global string pointer
                break :blk self.builder.buildGlobalStringPtr(str_z, name_z);
            },
            .identifier => |name| blk: {
                if (self.variables.get(name)) |var_ptr| {
                    // Load value from pointer
                    if (self.variable_types.get(name)) |var_type| {
                        const load_name_z = try self.allocator.dupeZ(u8, name);
                        defer self.allocator.free(load_name_z);
                        break :blk self.builder.buildLoad(var_type, var_ptr, load_name_z);
                    } else {
                        // Fallback: assume it's a direct value (for backward compatibility)
                        break :blk var_ptr;
                    }
                } else {
                    std.debug.print("⚠️  Undefined variable: {s}\n", .{name});
                    break :blk llvm.constI32(self.context, 0);
                }
            },
            .binary => |binop| blk: {
                const lhs_raw = try self.generateExpr(binop.left.*);
                const rhs_raw = try self.generateExpr(binop.right.*);
                
                // 🐛 修复: 确保操作数类型一致
                const lhs_type = llvm.LLVMTypeOf(lhs_raw);
                const rhs_type = llvm.LLVMTypeOf(rhs_raw);
                
                var lhs = lhs_raw;
                var rhs = rhs_raw;
                
                // 如果类型不同，尝试转换
                if (lhs_type != rhs_type) {
                    // 简化：将小类型扩展到大类型
                    const lhs_bits = llvm.LLVMGetIntTypeWidth(lhs_type);
                    const rhs_bits = llvm.LLVMGetIntTypeWidth(rhs_type);
                    
                    if (lhs_bits < rhs_bits) {
                        // 扩展lhs到rhs的类型
                        lhs = self.builder.buildSExt(lhs, rhs_type, "ext");
                    } else if (rhs_bits < lhs_bits) {
                        // 扩展rhs到lhs的类型
                        rhs = self.builder.buildSExt(rhs, lhs_type, "ext");
                    }
                }
                
                const result_name_z = try self.allocator.dupeZ(u8, "binop");
                defer self.allocator.free(result_name_z);
                
                const result = switch (binop.op) {
                    .add => self.builder.buildAdd(lhs, rhs, result_name_z),
                    .sub => self.builder.buildSub(lhs, rhs, result_name_z),
                    .mul => self.builder.buildMul(lhs, rhs, result_name_z),
                    .div => self.builder.buildSDiv(lhs, rhs, result_name_z),
                    // Comparison operators (返回i1，需要扩展为i8以匹配bool类型)
                    .eq => eq_blk: {
                        const i1_result = self.builder.buildICmp(.EQ, lhs, rhs, result_name_z);
                        // 🐛 修复: 将i1扩展为i8以匹配bool类型
                        const i8_type = self.context.i8Type();
                        break :eq_blk self.builder.buildZExt(i1_result, i8_type, "bool_ext");
                    },
                    .ne => ne_blk: {
                        const i1_result = self.builder.buildICmp(.NE, lhs, rhs, result_name_z);
                        const i8_type = self.context.i8Type();
                        break :ne_blk self.builder.buildZExt(i1_result, i8_type, "bool_ext");
                    },
                    .lt => lt_blk: {
                        const i1_result = self.builder.buildICmp(.SLT, lhs, rhs, result_name_z);
                        const i8_type = self.context.i8Type();
                        break :lt_blk self.builder.buildZExt(i1_result, i8_type, "bool_ext");
                    },
                    .le => le_blk: {
                        const i1_result = self.builder.buildICmp(.SLE, lhs, rhs, result_name_z);
                        const i8_type = self.context.i8Type();
                        break :le_blk self.builder.buildZExt(i1_result, i8_type, "bool_ext");
                    },
                    .gt => gt_blk: {
                        const i1_result = self.builder.buildICmp(.SGT, lhs, rhs, result_name_z);
                        const i8_type = self.context.i8Type();
                        break :gt_blk self.builder.buildZExt(i1_result, i8_type, "bool_ext");
                    },
                    .ge => ge_blk: {
                        const i1_result = self.builder.buildICmp(.SGE, lhs, rhs, result_name_z);
                        const i8_type = self.context.i8Type();
                        break :ge_blk self.builder.buildZExt(i1_result, i8_type, "bool_ext");
                    },
                    // Logical operators
                    .and_op => self.builder.buildAnd(lhs, rhs, result_name_z),
                    .or_op => self.builder.buildOr(lhs, rhs, result_name_z),
                    else => llvm.constI32(self.context, 0),
                };
                break :blk result;
            },
            .unary => |unop| blk: {
                const operand = try self.generateExpr(unop.operand.*);
                
                const result_name_z = try self.allocator.dupeZ(u8, "unop");
                defer self.allocator.free(result_name_z);
                
                const result = switch (unop.op) {
                    .neg => self.builder.buildNeg(operand, result_name_z),
                    .not => self.builder.buildNot(operand, result_name_z),
                };
                break :blk result;
            },
            .if_expr => |if_expr| blk: {
                const func = self.current_function orelse {
                    break :blk llvm.constI32(self.context, 0);
                };
                
                // Generate condition
                const cond_value_raw = try self.generateExpr(if_expr.condition.*);
                
                // 🐛 修复: 如果条件是i8 (bool)，需要截断为i1用于br指令
                const cond_value = self.ensureI1ForBranch(cond_value_raw);
                
                // Create basic blocks
                const then_block = llvm.appendBasicBlock(self.context, func, "if.then");
                const else_block = llvm.appendBasicBlock(self.context, func, "if.else");
                const cont_block = llvm.appendBasicBlock(self.context, func, "if.cont");
                
                // Build conditional branch
                _ = llvm.LLVMBuildCondBr(self.builder.ref, cond_value, then_block, else_block);
                
                // Generate then branch
                self.builder.positionAtEnd(then_block);
                const then_value = try self.generateExpr(if_expr.then_branch.*);
                const then_end_block = self.builder.getInsertBlock();
                // 🆕 v0.2.0: 只有当块没有终止符时才添加跳转
                const then_has_terminator = llvm.Builder.blockHasTerminator(then_end_block);
                if (!then_has_terminator) {
                    _ = self.builder.buildBr(cont_block);
                }
                
                // Generate else branch
                self.builder.positionAtEnd(else_block);
                const else_value = if (if_expr.else_branch) |else_br|
                    try self.generateExpr(else_br.*)
                else
                    llvm.constI32(self.context, 0);
                const else_end_block = self.builder.getInsertBlock();
                // 🆕 v0.2.0: 只有当块没有终止符时才添加跳转
                const else_has_terminator = llvm.Builder.blockHasTerminator(else_end_block);
                if (!else_has_terminator) {
                    _ = self.builder.buildBr(cont_block);
                }
                
                // Continue block with PHI node
                self.builder.positionAtEnd(cont_block);
                
                // 🆕 v0.2.0: 只为实际到达的分支创建 PHI
                // 如果两个分支都终止了，cont_block 不可达
                if (then_has_terminator and else_has_terminator) {
                    // 两个分支都终止，返回默认值（cont_block 不可达）
                    break :blk llvm.constI32(self.context, 0);
                }
                
                // Create PHI node to merge values from both branches
                const result_type = llvm.LLVMTypeOf(then_value);
                const phi_name_z = try self.allocator.dupeZ(u8, "if.result");
                defer self.allocator.free(phi_name_z);
                const phi = self.builder.buildPhi(result_type, phi_name_z);
                
                // 🆕 v0.2.0: 只添加未终止的分支到 PHI
                if (!then_has_terminator and !else_has_terminator) {
                    // 两个分支都未终止
                    var incoming_values = [_]llvm.ValueRef{ then_value, else_value };
                    var incoming_blocks = [_]llvm.BasicBlockRef{ then_end_block, else_end_block };
                    llvm.LLVMAddIncoming(phi, &incoming_values, &incoming_blocks, 2);
                } else if (!then_has_terminator) {
                    // 只有 then 分支未终止
                    var incoming_values = [_]llvm.ValueRef{then_value};
                    var incoming_blocks = [_]llvm.BasicBlockRef{then_end_block};
                    llvm.LLVMAddIncoming(phi, &incoming_values, &incoming_blocks, 1);
                } else {
                    // 只有 else 分支未终止
                    var incoming_values = [_]llvm.ValueRef{else_value};
                    var incoming_blocks = [_]llvm.BasicBlockRef{else_end_block};
                    llvm.LLVMAddIncoming(phi, &incoming_values, &incoming_blocks, 1);
                }
                
                break :blk phi;
            },
            .block => |stmts| blk: {
                // Execute all statements in the block
                var last_value: ?llvm.ValueRef = null;
                for (stmts) |stmt| {
                    switch (stmt) {
                        .expr => |block_expr| {
                            // Save the last expression value as the block result
                            last_value = try self.generateExpr(block_expr);
                        },
                        else => {
                            try self.generateStmt(stmt);
                        },
                    }
                }
                // Return the last expression value, or 0 if none
                break :blk last_value orelse llvm.constI32(self.context, 0);
            },
            .array_index => |index_expr| blk: {
                // Array/string indexing: arr[index]
                const array_value = try self.generateExpr(index_expr.array.*);
                const index_value = try self.generateExpr(index_expr.index.*);
                
                // 🆕 v0.2.0: 检测是否是字符串索引
                // 如果 array 是字符串字面量或字符串类型，使用 i8
                const is_string = index_expr.array.* == .string_literal or
                                 index_expr.array.* == .identifier;  // 简化判断
                
                const element_type = if (is_string) 
                    self.context.i8Type()  // 字符串 -> i8 (char)
                else 
                    self.context.i32Type();  // 数组 -> i32
                
                // Build GEP instruction
                var indices = [_]llvm.ValueRef{ llvm.constI32(self.context, 0), index_value };
                
                const gep_name_z = try self.allocator.dupeZ(u8, "index");
                defer self.allocator.free(gep_name_z);
                
                const array_type = llvm.arrayType(element_type, 0);  // Size doesn't matter for GEP
                
                const gep = self.builder.buildInBoundsGEP(array_type, array_value, &indices, gep_name_z);
                
                // Load the value
                const load_name_z = try self.allocator.dupeZ(u8, "elem");
                defer self.allocator.free(load_name_z);
                break :blk self.builder.buildLoad(element_type, gep, load_name_z);
            },
            .call => |call_expr| blk: {
                // 🆕 v0.3.0: 检查是否是实例方法调用 (obj.method 形式)
                if (call_expr.callee.* == .field_access) {
                    const field = call_expr.callee.field_access;
                    
                    // 获取对象的struct类型名
                    if (field.object.* == .identifier) {
                        const var_name = field.object.identifier;
                        
                        // 🆕 v0.3.0: 从variable_struct_types查找struct类型名
                        if (self.variable_struct_types.get(var_name)) |struct_type_name| {
                            // 构造完整方法名: StructName_methodName
                            var method_name = std.ArrayList(u8){};
                            defer method_name.deinit(self.allocator);
                            
                            try method_name.appendSlice(self.allocator, struct_type_name);
                            try method_name.appendSlice(self.allocator, "_");
                            try method_name.appendSlice(self.allocator, field.field);
                            
                            const full_method_name = try method_name.toOwnedSlice(self.allocator);
                            defer self.allocator.free(full_method_name);
                            
                            // 查找方法
                            if (self.functions.get(full_method_name)) |func| {
                                // 🐛 修复: 查找方法的实际返回类型
                                const method_return_type = self.function_return_types.get(full_method_name) orelse self.context.i32Type();
                                
                                // 生成参数：第一个参数是 self（对象指针）
                                var args = std.ArrayList(llvm.ValueRef){};
                                defer args.deinit(self.allocator);
                                
                                // 添加 self 参数（对象指针）
                                // 🆕 v0.3.0: 获取变量的值（load），因为struct变量存储的是指针
                                const obj_var_ptr = self.variables.get(var_name) orelse {
                                    std.debug.print("⚠️  Variable not found: {s}\n", .{var_name});
                                    break :blk llvm.constI32(self.context, 0);
                                };
                                const obj_var_type = self.variable_types.get(var_name) orelse self.context.pointerType(0);
                                const load_name_z = try self.allocator.dupeZ(u8, var_name);
                                defer self.allocator.free(load_name_z);
                                const obj_ptr = self.builder.buildLoad(obj_var_type, obj_var_ptr, load_name_z);
                                try args.append(self.allocator, obj_ptr);
                                
                                // 添加其他参数
                                for (call_expr.args) |arg| {
                                    const arg_value = try self.generateExpr(arg);
                                    try args.append(self.allocator, arg_value);
                                }
                                
                                // 🐛 修复: 从LLVM函数获取参数类型
                                var param_types = std.ArrayList(llvm.TypeRef){};
                                defer param_types.deinit(self.allocator);
                                
                                const param_count = llvm.LLVMCountParams(func);
                                var param_idx: u32 = 0;
                                while (param_idx < param_count) : (param_idx += 1) {
                                    const param = llvm.LLVMGetParam(func, param_idx);
                                    const param_type = llvm.LLVMTypeOf(param);
                                    try param_types.append(self.allocator, param_type);
                                }
                                
                                // 🐛 修复: 使用实际的返回类型
                                const func_type = llvm.functionType(method_return_type, param_types.items, false);
                                
                                // 构建调用
                                const call_name_z = try self.allocator.dupeZ(u8, "method_call");
                                defer self.allocator.free(call_name_z);
                                
                                const result = self.builder.buildCall(func_type, func, args.items, call_name_z);
                                break :blk result;
                            }
                        }
                    }
                }
                
                // 普通函数调用
                const func_name = if (call_expr.callee.* == .identifier)
                    call_expr.callee.identifier
                else
                    "unknown";
                
                // Look up function
                // 查找函数 - 支持enum构造器简写
                var func: ?llvm.ValueRef = self.functions.get(func_name);
                var func_return_type: llvm.TypeRef = self.context.i32Type();
                
                if (func != null) {
                    // 直接找到了
                    func_return_type = self.function_return_types.get(func_name) orelse self.context.i32Type();
                } else {
                    // 🐛 v0.2.0: 检查是否是enum构造器简写 (Ok -> Result_Ok)
                    if (self.enum_variants.get(func_name)) |enum_name| {
                        var full_name_buf = std.ArrayList(u8){};
                        defer full_name_buf.deinit(self.allocator);
                        
                        try full_name_buf.appendSlice(self.allocator, enum_name);
                        try full_name_buf.appendSlice(self.allocator, "_");
                        try full_name_buf.appendSlice(self.allocator, func_name);
                        
                        const full_name = try full_name_buf.toOwnedSlice(self.allocator);
                        defer self.allocator.free(full_name);
                        
                        if (self.functions.get(full_name)) |found_func| {
                            func = found_func;
                            func_return_type = self.function_return_types.get(full_name) orelse self.context.i32Type();
                        }
                    }
                }
                
                const llvm_func = func orelse {
                    std.debug.print("⚠️  Undefined function: {s}\n", .{func_name});
                    break :blk llvm.constI32(self.context, 0);
                };
                
                // Generate arguments
                var args = std.ArrayList(llvm.ValueRef){};
                defer args.deinit(self.allocator);
                
                for (call_expr.args) |arg| {
                    const arg_value = try self.generateExpr(arg);
                    try args.append(self.allocator, arg_value);
                }
                
                // 🐛 修复: 从LLVM函数获取参数类型
                var param_types = std.ArrayList(llvm.TypeRef){};
                defer param_types.deinit(self.allocator);
                
                const param_count = llvm.LLVMCountParams(llvm_func);
                var i: u32 = 0;
                while (i < param_count) : (i += 1) {
                    const param = llvm.LLVMGetParam(llvm_func, i);
                    const param_type = llvm.LLVMTypeOf(param);
                    try param_types.append(self.allocator, param_type);
                }
                
                // 🐛 修复: 使用实际的返回类型构建函数类型
                const func_type = llvm.functionType(func_return_type, param_types.items, false);
                
                // Build call
                const call_name_z = try self.allocator.dupeZ(u8, "call");
                defer self.allocator.free(call_name_z);
                
                const result = self.builder.buildCall(func_type, llvm_func, args.items, call_name_z);
                break :blk result;
            },
            .static_method_call => |smc| blk: {
                // 🆕 静态方法调用：Type<T>::method()
                // 生成修饰后的函数名：Type_T_method
                var func_name = std.ArrayList(u8){};
                defer func_name.deinit(self.allocator);
                
                // 添加类型名
                try func_name.appendSlice(self.allocator, smc.type_name);
                
                // 添加类型参数
                for (smc.type_args) |type_arg| {
                    try func_name.appendSlice(self.allocator, "_");
                    const type_name = try self.getSimpleTypeName(type_arg);
                    // 只有 generic_instance 返回的是需要释放的内存
                    const needs_free = type_arg == .generic_instance;
                    defer if (needs_free) self.allocator.free(type_name);
                    try func_name.appendSlice(self.allocator, type_name);
                }
                
                // 添加方法名
                try func_name.appendSlice(self.allocator, "_");
                try func_name.appendSlice(self.allocator, smc.method_name);
                
                const mangled_name = try func_name.toOwnedSlice(self.allocator);
                defer self.allocator.free(mangled_name);
                
                // 查找函数
                const func = self.functions.get(mangled_name) orelse {
                    std.debug.print("⚠️  Undefined static method: {s}\n", .{mangled_name});
                    break :blk llvm.constI32(self.context, 0);
                };
                
                // 🐛 修复: 查找静态方法的实际返回类型
                const static_return_type = self.function_return_types.get(mangled_name) orelse self.context.i32Type();
                
                // 生成参数
                var args = std.ArrayList(llvm.ValueRef){};
                defer args.deinit(self.allocator);
                
                for (smc.args) |arg| {
                    const arg_value = try self.generateExpr(arg);
                    try args.append(self.allocator, arg_value);
                }
                
                // 🐛 修复: 从LLVM函数获取参数类型
                var param_types = std.ArrayList(llvm.TypeRef){};
                defer param_types.deinit(self.allocator);
                
                const param_count = llvm.LLVMCountParams(func);
                var param_idx: u32 = 0;
                while (param_idx < param_count) : (param_idx += 1) {
                    const param = llvm.LLVMGetParam(func, param_idx);
                    const param_type = llvm.LLVMTypeOf(param);
                    try param_types.append(self.allocator, param_type);
                }
                
                // 🐛 修复: 使用实际的返回类型
                const func_type = llvm.functionType(static_return_type, param_types.items, false);
                
                // 构建调用
                const call_name_z = try self.allocator.dupeZ(u8, "static_call");
                defer self.allocator.free(call_name_z);
                
                const result = self.builder.buildCall(func_type, func, args.items, call_name_z);
                break :blk result;
            },
            .array_literal => |elements| blk: {
                // 🆕 数组字面量：[1, 2, 3]
                // 简化实现：返回第一个元素的值
                if (elements.len == 0) {
                    break :blk llvm.constI32(self.context, 0);
                }
                
                // 返回第一个元素的值（简化实现）
                const first_element = try self.generateExpr(elements[0]);
                break :blk first_element;
            },
            // 🆕 v0.1.7: as 类型转换
            .as_expr => |as_cast| blk: {
                const value = try self.generateExpr(as_cast.value.*);
                const target_llvm_type = try self.toLLVMType(as_cast.target_type);
                
                // 生成类型转换指令
                break :blk try self.generateCast(value, as_cast.value, as_cast.target_type, target_llvm_type);
            },
            // 🆕 v0.2.0: 错误传播 (? 操作符)
            .try_expr => |inner| blk: {
                break :blk try self.generateTryExpr(inner.*);
            },
            // 🆕 v0.2.0: 枚举variant构造 (Ok(42))
            .enum_variant => |enum_var| blk: {
                break :blk try self.generateEnumVariant(enum_var);
            },
            .is_expr => |is_match| blk: {
                // 🐛 v0.2.0: 实现pattern matching
                break :blk try self.generateIsExpr(is_match);
            },
            .struct_init => |struct_init| blk: {
                // 🆕 v0.3.0: 实现struct初始化
                break :blk try self.generateStructInit(struct_init);
            },
            .field_access => |field_access| blk: {
                // 🆕 v0.3.0: 实现字段访问
                break :blk try self.generateFieldAccess(field_access);
            },
            else => llvm.constI32(self.context, 0),
        };
    }
    
    fn toLLVMType(self: *LLVMNativeBackend, paw_type: ast.Type) !llvm.TypeRef {
        return switch (paw_type) {
            // 🆕 v0.1.7: 完整的类型映射（支持 as 转换）
            .i8, .u8, .bool, .char => self.context.i8Type(),
            .i16, .u16 => self.context.i16Type(),
            .i32, .u32 => self.context.i32Type(),
            .i64, .u64 => self.context.i64Type(),
            .i128, .u128 => self.context.i128Type(),
            .f32 => self.context.floatType(),
            .f64 => self.context.doubleType(),
            .void => self.context.voidType(),
            .string => self.context.pointerType(0),
            
            .named => |name| blk: {
                // 特殊类型名处理
                if (std.mem.eql(u8, name, "i32") or std.mem.eql(u8, name, "int")) {
                    break :blk self.context.i32Type();
                } else if (std.mem.eql(u8, name, "i64")) {
                    break :blk self.context.i64Type();
                } else if (std.mem.eql(u8, name, "f64") or std.mem.eql(u8, name, "double")) {
                    break :blk self.context.doubleType();
                } else if (std.mem.eql(u8, name, "void")) {
                    break :blk self.context.voidType();
                }
                
                // 🆕 v0.2.0: Enum类型检测
                if (self.enum_types.get(name)) |enum_info| {
                    break :blk enum_info.llvm_type.?;
                }
                
                // 🆕 v0.3.0: Struct类型检测（仅用于变量声明）
                if (self.struct_types.get(name)) |_| {
                    break :blk self.context.pointerType(0);
                }
                
                // 默认：未知类型返回i32
                break :blk self.context.i32Type();
            },
            else => self.context.i32Type(), // Default
        };
    }
    
    /// 获取类型的简化名（用于name mangling）
    /// 注意：对于 generic_instance，调用者需要负责释放返回的字符串
    fn getSimpleTypeName(self: *LLVMNativeBackend, paw_type: ast.Type) ![]const u8 {
        return switch (paw_type) {
            .i8 => "i8",
            .i16 => "i16",
            .i32 => "i32",
            .i64 => "i64",
            .i128 => "i128",
            .u8 => "u8",
            .u16 => "u16",
            .u32 => "u32",
            .u64 => "u64",
            .u128 => "u128",
            .f32 => "f32",
            .f64 => "f64",
            .bool => "bool",
            .char => "char",
            .string => "string",
            .void => "void",
            .generic => |name| name,
            .named => |name| name,
            .generic_instance => |gi| blk: {
                // 🆕 处理泛型实例：Vec<i32> -> Vec_i32
                // 注意：这会分配新内存，调用者需要释放
                var buf = std.ArrayList(u8){};
                errdefer buf.deinit(self.allocator);
                
                try buf.appendSlice(self.allocator, gi.name);
                for (gi.type_args) |arg| {
                    try buf.appendSlice(self.allocator, "_");
                    const type_name = try self.getSimpleTypeName(arg);
                    // 只有 generic_instance 返回的是需要释放的内存
                    const needs_free = arg == .generic_instance;
                    defer if (needs_free) self.allocator.free(type_name);
                    try buf.appendSlice(self.allocator, type_name);
                }
                break :blk try buf.toOwnedSlice(self.allocator);
            },
            else => "unknown",
        };
    }
    
    // ============================================================================
    // 🆕 v0.1.7: Optimization Support
    // ============================================================================
    
    /// 获取优化级别对应的 clang 参数提示
    pub fn getOptLevelString(self: *LLVMNativeBackend) []const u8 {
        return switch (self.opt_level) {
            .O0 => "-O0",
            .O1 => "-O1",
            .O2 => "-O2",
            .O3 => "-O3",
        };
    }
    
    // ============================================================================
    // 🆕 v0.1.7: Type Cast Support
    // ============================================================================
    
    /// 生成类型转换指令
    fn generateCast(
        self: *LLVMNativeBackend,
        value: llvm.ValueRef,
        source_expr: *ast.Expr,
        target_type: ast.Type,
        target_llvm_type: llvm.TypeRef,
    ) !llvm.ValueRef {
        // 获取源类型（简化：从表达式推断）
        const source_type = try self.inferExprType(source_expr.*);
        
        const cast_name_z = try self.allocator.dupeZ(u8, "cast");
        defer self.allocator.free(cast_name_z);
        
        // 判断源类型和目标类型的类别
        const is_source_int = self.isIntType(source_type);
        const is_source_float = self.isFloatType(source_type);
        const is_target_int = self.isIntType(target_type);
        const is_target_float = self.isFloatType(target_type);
        
        // 根据源类型和目标类型选择转换指令
        if (is_source_int and is_target_int) {
            // 整数 -> 整数
            const source_bits = self.getTypeBits(source_type);
            const target_bits = self.getTypeBits(target_type);
            
            if (source_bits < target_bits) {
                // 扩展
                const is_signed = self.isSignedIntType(source_type);
                if (is_signed) {
                    return self.builder.buildSExt(value, target_llvm_type, cast_name_z); // 符号扩展
                } else {
                    return self.builder.buildZExt(value, target_llvm_type, cast_name_z); // 零扩展
                }
            } else if (source_bits > target_bits) {
                // 截断
                return self.builder.buildTrunc(value, target_llvm_type, cast_name_z);
            } else {
                // 同样大小，可能有符号变无符号（bitcast）
                return value;
            }
        } else if (is_source_int and is_target_float) {
            // 整数 -> 浮点
            const is_signed = self.isSignedIntType(source_type);
            if (is_signed) {
                return self.builder.buildSIToFP(value, target_llvm_type, cast_name_z);
            } else {
                return self.builder.buildUIToFP(value, target_llvm_type, cast_name_z);
            }
        } else if (is_source_float and is_target_int) {
            // 浮点 -> 整数
            const is_signed = self.isSignedIntType(target_type);
            if (is_signed) {
                return self.builder.buildFPToSI(value, target_llvm_type, cast_name_z);
            } else {
                return self.builder.buildFPToUI(value, target_llvm_type, cast_name_z);
            }
        } else if (is_source_float and is_target_float) {
            // 浮点 -> 浮点
            const source_bits = self.getTypeBits(source_type);
            const target_bits = self.getTypeBits(target_type);
            
            if (source_bits < target_bits) {
                // f32 -> f64
                return self.builder.buildFPExt(value, target_llvm_type, cast_name_z);
            } else if (source_bits > target_bits) {
                // f64 -> f32
                return self.builder.buildFPTrunc(value, target_llvm_type, cast_name_z);
            } else {
                return value;
            }
        } else if (source_type == .bool and is_target_int) {
            // bool -> 整数
            return self.builder.buildZExt(value, target_llvm_type, cast_name_z);
        } else if (source_type == .char and is_target_int) {
            // char -> 整数
            const is_signed = self.isSignedIntType(target_type);
            if (is_signed) {
                return self.builder.buildSExt(value, target_llvm_type, cast_name_z);
            } else {
                return self.builder.buildZExt(value, target_llvm_type, cast_name_z);
            }
        } else if (is_source_int and target_type == .char) {
            // 整数 -> char
            return self.builder.buildTrunc(value, target_llvm_type, cast_name_z);
        } else {
            // 未知转换，返回原值
            return value;
        }
    }
    
    /// 推断表达式的类型（简化版）
    fn inferExprType(self: *LLVMNativeBackend, expr: ast.Expr) !ast.Type {
        _ = self;
        return switch (expr) {
            .int_literal => ast.Type.i32,
            .float_literal => ast.Type.f64,
            .bool_literal => ast.Type.bool,
            .char_literal => ast.Type.char,
            .string_literal => ast.Type.string,
            .identifier => ast.Type.i32,  // 简化
            else => ast.Type.i32,
        };
    }
    
    /// 检查是否是整数类型
    fn isIntType(_: *LLVMNativeBackend, t: ast.Type) bool {
        return switch (t) {
            .i8, .i16, .i32, .i64, .i128,
            .u8, .u16, .u32, .u64, .u128 => true,
            else => false,
        };
    }
    
    /// 检查是否是浮点类型
    fn isFloatType(_: *LLVMNativeBackend, t: ast.Type) bool {
        return switch (t) {
            .f32, .f64 => true,
            else => false,
        };
    }
    
    /// 检查是否是有符号整数类型
    fn isSignedIntType(_: *LLVMNativeBackend, t: ast.Type) bool {
        return switch (t) {
            .i8, .i16, .i32, .i64, .i128 => true,
            else => false,
        };
    }
    
    /// 确保值是i1类型（用于条件分支）
    /// 如果值是i8(bool)，截断为i1；如果已经是i1，直接返回
    fn ensureI1ForBranch(self: *LLVMNativeBackend, value: llvm.ValueRef) llvm.ValueRef {
        const value_type = llvm.LLVMTypeOf(value);
        const i1_type = self.context.i1Type();
        const i8_type = self.context.i8Type();
        
        // 如果已经是i1，直接返回
        if (value_type == i1_type) {
            return value;
        }
        
        // 如果是i8，截断为i1（比较 value != 0）
        if (value_type == i8_type) {
            const zero = llvm.LLVMConstInt(i8_type, 0, 0);
            return self.builder.buildICmp(.NE, value, zero, "to_i1");
        }
        
        // 其他整数类型也截断为i1
        const zero_val = llvm.LLVMConstInt(value_type, 0, 0);
        return self.builder.buildICmp(.NE, value, zero_val, "to_i1");
    }
    
    /// 🆕 v0.2.0: 生成枚举variant构造调用 (Ok(42))
    fn generateEnumVariant(self: *LLVMNativeBackend, enum_var: anytype) !llvm.ValueRef {
        // 查找构造器函数 - 尝试两种可能的名称
        var constructor_func: ?llvm.ValueRef = null;
        var return_type: llvm.TypeRef = self.context.i32Type();
        
        // 方式1: 完整名称 EnumName_VariantName
        {
            var buf = std.ArrayList(u8){};
            defer buf.deinit(self.allocator);
            
            try buf.appendSlice(self.allocator, enum_var.enum_name);
            try buf.appendSlice(self.allocator, "_");
            try buf.appendSlice(self.allocator, enum_var.variant);
            
            const name = try buf.toOwnedSlice(self.allocator);
            defer self.allocator.free(name);
            
            if (self.functions.get(name)) |func| {
                constructor_func = func;
                return_type = self.function_return_types.get(name) orelse self.context.i32Type();
            }
        }
        
        // 方式2: 简写 - 通过variant名查找
        if (constructor_func == null) {
            if (self.enum_variants.get(enum_var.variant)) |enum_name| {
                var buf = std.ArrayList(u8){};
                defer buf.deinit(self.allocator);
                
                try buf.appendSlice(self.allocator, enum_name);
                try buf.appendSlice(self.allocator, "_");
                try buf.appendSlice(self.allocator, enum_var.variant);
                
                const name = try buf.toOwnedSlice(self.allocator);
                defer self.allocator.free(name);
                
                if (self.functions.get(name)) |func| {
                    constructor_func = func;
                    return_type = self.function_return_types.get(name) orelse self.context.i32Type();
                }
            }
        }
        
        if (constructor_func == null) {
            return error.ConstructorNotFound;
        }
        
        // 生成参数值
        var args = std.ArrayList(llvm.ValueRef){};
        defer args.deinit(self.allocator);
        
        for (enum_var.args) |arg| {
            const arg_value = try self.generateExpr(arg);
            try args.append(self.allocator, arg_value);
        }
        
        // 调用构造器函数
        const call_name_z = try self.allocator.dupeZ(u8, "enum_call");
        defer self.allocator.free(call_name_z);
        
        const call_result = self.builder.buildCall(
            return_type,
            constructor_func.?,
            args.items,
            call_name_z
        );
        
        return call_result;
    }
    
    /// 🆕 v0.2.0: 生成错误传播表达式 (? 操作符)
    /// 策略：
    /// 1. 评估内部表达式得到 Result (完整 struct { tag, data })
    /// 2. 提取 tag 字段，检查是否为 Err (tag == 1)
    /// 3. 如果是 Err，提前返回该 Result
    /// 4. 否则从 data 中提取 Ok 的值
    fn generateTryExpr(self: *LLVMNativeBackend, inner: ast.Expr) !llvm.ValueRef {
        const func = self.current_function orelse {
            return llvm.constI32(self.context, 0);
        };
        
        // 评估内部表达式 (返回 Result struct)
        const result_value = try self.generateExpr(inner);
        const result_type = llvm.LLVMTypeOf(result_value);
        
        // 分配临时变量存储结果
        const result_alloca_z = try self.allocator.dupeZ(u8, "__try_result__");
        defer self.allocator.free(result_alloca_z);
        const result_alloca = self.builder.buildAlloca(result_type, result_alloca_z);
        _ = self.builder.buildStore(result_value, result_alloca);
        
        // 创建基本块
        const check_block = llvm.appendBasicBlock(self.context, func, "try.check");
        const err_block = llvm.appendBasicBlock(self.context, func, "try.err");
        const ok_block = llvm.appendBasicBlock(self.context, func, "try.ok");
        
        // 跳转到检查块
        _ = self.builder.buildBr(check_block);
        
        // 检查块：提取并比较 tag 字段
        self.builder.positionAtEnd(check_block);
        
        // 提取tag字段 (field 0)
        const tag_ptr = self.builder.buildStructGEP(result_type, result_alloca, 0, "tag_ptr");
        const tag_value = self.builder.buildLoad(self.context.i32Type(), tag_ptr, "tag");
        
        // 比较 tag 是否为 Err (1)
        const err_tag = llvm.constI32(self.context, 1);
        const is_err = self.builder.buildICmp(.EQ, tag_value, err_tag, "is_err");
        
        // 转换为i1用于分支
        const is_err_i1 = self.ensureI1ForBranch(is_err);
        _ = llvm.LLVMBuildCondBr(self.builder.ref, is_err_i1, err_block, ok_block);
        
        // Err块：返回完整的错误结果
        self.builder.positionAtEnd(err_block);
        const err_result = self.builder.buildLoad(result_type, result_alloca, "err_result");
        _ = self.builder.buildRet(err_result);
        
        // Ok块：从data中提取值并返回
        self.builder.positionAtEnd(ok_block);
        
        // 提取data字段 (field 1)
        const data_ptr = self.builder.buildStructGEP(result_type, result_alloca, 1, "data_ptr");
        
        // 从data数组开头提取i32值（简化：假设Ok(i32)）
        var indices = [_]llvm.ValueRef{
            llvm.constI32(self.context, 0),
            llvm.constI32(self.context, 0),
        };
        
        const value_ptr_z = try self.allocator.dupeZ(u8, "value_ptr");
        defer self.allocator.free(value_ptr_z);
        
        const value_ptr = self.builder.buildGEP(
            llvm.LLVMArrayType(self.context.i8Type(), 32),
            data_ptr,
            &indices,
            value_ptr_z
        );
        
        // bitcast到i32*并加载
        const typed_ptr = self.builder.buildBitCast(
            value_ptr,
            llvm.LLVMPointerType(self.context.i32Type(), 0),
            "typed_ptr"
        );
        const ok_value = self.builder.buildLoad(self.context.i32Type(), typed_ptr, "ok_value");
        
        return ok_value;
    }
    
    /// 🐛 v0.2.0: 生成is表达式（pattern matching）
    /// 简化实现：使用if-else链比较tag值
    fn generateIsExpr(self: *LLVMNativeBackend, is_match: anytype) anyerror!llvm.ValueRef {
        const func = self.current_function orelse {
            return llvm.constI32(self.context, 0);
        };
        
        // 计算match的值（enum tag，表示为i32）
        const match_value = try self.generateExpr(is_match.value.*);
        
        // 推断结果类型（从第一个arm的body）
        // 简化：假设所有arm返回相同类型
        const result_type = if (is_match.arms.len > 0) 
            self.inferExprTypeFromExpr(is_match.arms[0].body)
        else 
            self.context.i32Type();
        
        // 创建result变量来存储最终值
        const result_ptr = self.builder.buildAlloca(result_type, "match_result");
        
        // 创建exit基本块（所有匹配成功后跳到这里）
        const exit_block = llvm.appendBasicBlock(self.context, func, "match.exit");
        
        // 为每个arm生成if-else链
        var next_block: ?llvm.BasicBlockRef = null;
        
        for (is_match.arms, 0..) |arm, i| {
            const is_last = (i == is_match.arms.len - 1);
            
            if (arm.pattern == .wildcard) {
                // 通配符：直接生成值，不需要比较
                const body_value = try self.generateExpr(arm.body);
                _ = self.builder.buildStore(body_value, result_ptr);
                _ = self.builder.buildBr(exit_block);
            } else if (arm.pattern == .variant) {
                // Enum variant模式：比较tag值
                const variant = arm.pattern.variant;
                _ = variant;  // TODO: 使用variant.name查找正确的tag
                
                // 查找这个variant的索引（tag值）
                // 简化：假设arm的顺序就是tag的顺序
                const tag_value = llvm.constI32(self.context, @intCast(i));  // 简化
                
                // 比较: match_value == tag_value
                const cond = self.builder.buildICmp(.EQ, match_value, tag_value, "match_cond");
                const cond_i1 = self.ensureI1ForBranch(cond);
                
                // 创建then和else基本块
                const then_block = llvm.appendBasicBlock(self.context, func, "match.then");
                next_block = if (!is_last) 
                    llvm.appendBasicBlock(self.context, func, "match.next") 
                else 
                    exit_block;  // 最后一个arm失败时跳到exit
                
                // 条件分支
                _ = llvm.LLVMBuildCondBr(self.builder.ref, cond_i1, then_block, next_block.?);
                
                // 生成then块
                self.builder.positionAtEnd(then_block);
                const body_value = try self.generateExpr(arm.body);
                _ = self.builder.buildStore(body_value, result_ptr);
                _ = self.builder.buildBr(exit_block);  // 匹配成功后跳到exit
                
                // 如果不是最后一个arm，继续处理下一个
                if (!is_last) {
                    self.builder.positionAtEnd(next_block.?);
                }
            }
        }
        
        // 生成exit块
        self.builder.positionAtEnd(exit_block);
        
        // 加载最终结果
        return self.builder.buildLoad(result_type, result_ptr, "match_result");
    }
    
    /// 从表达式推断LLVM类型
    fn inferExprTypeFromExpr(self: *LLVMNativeBackend, expr: ast.Expr) llvm.TypeRef {
        return switch (expr) {
            .int_literal => self.context.i32Type(),
            .float_literal => self.context.doubleType(),
            .bool_literal => self.context.i8Type(),  // bool是i8
            .char_literal => self.context.i8Type(),
            .string_literal => self.context.pointerType(0),  // i8* (string)
            else => self.context.i32Type(),  // 默认
        };
    }
    
    /// 🆕 v0.3.0: 生成字段访问
    fn generateFieldAccess(self: *LLVMNativeBackend, field_access: anytype) !llvm.ValueRef {
        // 获取对象（应该是变量或self）
        const object_value = try self.generateExpr(field_access.object.*);
        
        // 获取对象的struct类型名
        const struct_type_name = if (field_access.object.* == .identifier) blk: {
            const var_name = field_access.object.identifier;
            break :blk self.variable_struct_types.get(var_name);
        } else null;
        
        if (struct_type_name == null) {
            // 无法确定struct类型，返回默认值
            std.debug.print("⚠️  Cannot determine struct type for field access\n", .{});
            return llvm.constI32(self.context, 0);
        }
        
        // 获取struct信息
        const struct_info = self.struct_types.get(struct_type_name.?) orelse {
            std.debug.print("⚠️  Struct type not found: {s}\n", .{struct_type_name.?});
            return llvm.constI32(self.context, 0);
        };
        
        // 查找字段索引
        var field_index: u32 = 0;
        var field_type_opt: ?ast.Type = null;
        for (struct_info.fields) |field| {
            if (std.mem.eql(u8, field.name, field_access.field)) {
                field_index = field.index;
                field_type_opt = field.field_type;
                break;
            }
        }
        
        const field_type = field_type_opt orelse {
            std.debug.print("⚠️  Field not found: {s}\n", .{field_access.field});
            return llvm.constI32(self.context, 0);
        };
        
        // 使用GEP指令访问字段
        // object_value是指针，使用GEP获取字段指针
        var indices = [_]llvm.ValueRef{
            llvm.constI32(self.context, 0),  // 解引用struct指针
            llvm.constI32(self.context, @intCast(field_index)),  // 字段索引
        };
        
        const field_name_z = try self.allocator.dupeZ(u8, field_access.field);
        defer self.allocator.free(field_name_z);
        
        const field_ptr = llvm.LLVMBuildGEP2(
            self.builder.ref,
            struct_info.llvm_type.?,
            object_value,
            &indices,
            2,
            field_name_z
        );
        
        // 加载字段值
        const field_llvm_type = try self.toLLVMType(field_type);
        return self.builder.buildLoad(field_llvm_type, field_ptr, field_name_z);
    }
    
    /// 🆕 v0.3.0: 生成struct初始化
    fn generateStructInit(self: *LLVMNativeBackend, struct_init: anytype) !llvm.ValueRef {
        // 获取struct类型信息
        const struct_info = self.struct_types.get(struct_init.type_name) orelse {
            std.debug.print("⚠️  Struct type not found: {s}\n", .{struct_init.type_name});
            return llvm.constI32(self.context, 0);
        };
        
        // 分配stack空间
        const struct_name_z = try self.allocator.dupeZ(u8, struct_init.type_name);
        defer self.allocator.free(struct_name_z);
        
        const struct_ptr = self.builder.buildAlloca(struct_info.llvm_type.?, struct_name_z);
        
        // 为每个字段生成store指令
        for (struct_init.fields) |field_init| {
            // 查找字段索引
            var field_index: u32 = 0;
            for (struct_info.fields) |field| {
                if (std.mem.eql(u8, field.name, field_init.name)) {
                    field_index = field.index;
                    break;
                }
            }
            
            // 生成字段值
            const field_value = try self.generateExpr(field_init.value);
            
            // 使用GEP获取字段指针
            var indices = [_]llvm.ValueRef{
                llvm.constI32(self.context, 0),
                llvm.constI32(self.context, @intCast(field_index)),
            };
            
            const field_name_z = try self.allocator.dupeZ(u8, field_init.name);
            defer self.allocator.free(field_name_z);
            
            const field_ptr = llvm.LLVMBuildGEP2(
                self.builder.ref,
                struct_info.llvm_type.?,
                struct_ptr,
                &indices,
                2,
                field_name_z
            );
            
            // 存储字段值
            _ = self.builder.buildStore(field_value, field_ptr);
        }
        
        // 返回struct指针
        return struct_ptr;
    }
    
    /// 获取类型的位数
    fn getTypeBits(_: *LLVMNativeBackend, t: ast.Type) u32 {
        return switch (t) {
            .i8, .u8, .bool, .char => 8,
            .i16, .u16 => 16,
            .i32, .u32, .f32 => 32,
            .i64, .u64, .f64 => 64,
            .i128, .u128 => 128,
            else => 32,  // 默认
        };
    }
};

