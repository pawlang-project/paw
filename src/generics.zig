//! Generics - Paw Language Generic System
//!
//! 这个模块实现完整的泛型系统，包括：
//! 1. 类型推导引擎 (Type Inference)
//! 2. 单态化转换 (Monomorphization)
//! 3. 代码生成适配
//!
//! 设计理念：
//! - Rust-style 单态化：为每个具体类型生成独立代码
//! - 零运行时开销
//! - 编译时完全确定所有类型

const std = @import("std");
const ast = @import("ast.zig");

// ============================================================================
// 🆕 类型推导辅助函数
// ============================================================================

/// 从表达式推导类型（不依赖外部状态）
fn inferTypeFromExpr(expr: ast.Expr) ast.Type {
    return switch (expr) {
        .int_literal => ast.Type.i32,
        .float_literal => ast.Type.f64,
        .string_literal => ast.Type.string,
        .char_literal => ast.Type.char,
        .bool_literal => ast.Type.bool,
        .identifier => ast.Type.i32,  // 简化：默认 i32
        .call => ast.Type.i32,
        .binary => ast.Type.i32,
        .unary => |un| inferTypeFromExpr(un.operand.*),
        .field_access => ast.Type.i32,
        .array_index => ast.Type.i32,
        .struct_init => |si| ast.Type{ .named = si.type_name },
        .array_literal => ast.Type.i32,
        else => ast.Type.i32,
    };
}

// ============================================================================
// 类型变量和类型环境
// ============================================================================

/// 类型变量：用于类型推导的占位符
pub const TypeVar = struct {
    id: usize,
    name: []const u8,
    bound: ?ast.Type, // 约束（trait bounds）
};

/// 类型替换映射：泛型参数 -> 具体类型
pub const TypeSubstitution = std.StringHashMap(ast.Type);

/// 泛型实例：记录一个泛型的具体实例化
pub const GenericInstance = struct {
    generic_name: []const u8,
    type_args: []ast.Type,
    mangled_name: []const u8, // Vec_i32, HashMap_string_i32
    
    pub fn deinit(self: GenericInstance, allocator: std.mem.Allocator) void {
        allocator.free(self.type_args);
        allocator.free(self.mangled_name);
    }
};

/// 🆕 泛型结构体实例：记录泛型结构体的实例化
pub const GenericStructInstance = struct {
    generic_name: []const u8,        // 原始结构体名 (Box)
    type_args: []ast.Type,            // 类型参数 (i32)
    mangled_name: []const u8,         // 修饰后的名称 (Box_i32)
    
    pub fn deinit(self: GenericStructInstance, allocator: std.mem.Allocator) void {
        allocator.free(self.type_args);
        allocator.free(self.mangled_name);
    }
};

/// 🆕 泛型方法实例：记录泛型方法的实例化
pub const GenericMethodInstance = struct {
    struct_name: []const u8,        // 结构体名 (Vec)
    method_name: []const u8,        // 方法名 (new)
    type_args: []ast.Type,          // 类型参数 (i32)
    mangled_name: []const u8,       // 修饰后的名称 (Vec_i32_new)
    
    pub fn deinit(self: GenericMethodInstance, allocator: std.mem.Allocator) void {
        allocator.free(self.type_args);
        allocator.free(self.mangled_name);
    }
};

// ============================================================================
// 类型推导引擎
// ============================================================================

pub const TypeInference = struct {
    allocator: std.mem.Allocator,
    substitutions: TypeSubstitution,
    next_var_id: usize,

    pub fn init(allocator: std.mem.Allocator) TypeInference {
        return TypeInference{
            .allocator = allocator,
            .substitutions = TypeSubstitution.init(allocator),
            .next_var_id = 0,
        };
    }

    pub fn deinit(self: *TypeInference) void {
        self.substitutions.deinit();
    }

    /// 创建新的类型变量
    pub fn freshTypeVar(self: *TypeInference, name: []const u8) !TypeVar {
        const id = self.next_var_id;
        self.next_var_id += 1;
        return TypeVar{
            .id = id,
            .name = name,
            .bound = null,
        };
    }

    /// 类型统一算法 (Unification)
    /// 尝试让两个类型相等，通过推导类型变量
    pub fn unify(self: *TypeInference, t1: ast.Type, t2: ast.Type) !bool {
        // 处理泛型类型变量
        if (t1 == .generic) {
            try self.substitutions.put(t1.generic, t2);
            return true;
        }
        if (t2 == .generic) {
            try self.substitutions.put(t2.generic, t1);
            return true;
        }

        // 具体类型必须完全匹配
        if (!t1.eql(t2)) {
            return false;
        }

        return true;
    }

    /// 从调用处推导泛型参数
    /// 例如: identity<T>(42) -> T = i32
    pub fn inferFromCall(
        self: *TypeInference,
        type_params: [][]const u8,
        param_types: []ast.Type,
        arg_types: []ast.Type,
    ) !TypeSubstitution {
        if (param_types.len != arg_types.len) {
            return error.ArgumentCountMismatch;
        }

        // 初始化替换表
        var result = TypeSubstitution.init(self.allocator);

        // 对每个参数进行统一
        for (param_types, arg_types) |param_type, arg_type| {
            _ = try self.unifyWithSubst(param_type, arg_type, &result);
        }

        // 验证所有类型参数都被推导出来
        for (type_params) |tp| {
            if (!result.contains(tp)) {
                std.debug.print("Error: Cannot infer type parameter '{s}'\n", .{tp});
                return error.TypeInferenceFailed;
            }
        }

        return result;
    }

    /// 带替换表的统一算法
    fn unifyWithSubst(
        self: *TypeInference,
        t1: ast.Type,
        t2: ast.Type,
        subst: *TypeSubstitution,
    ) !bool {
        // 如果 t1 是泛型参数
        if (t1 == .generic) {
            if (subst.get(t1.generic)) |existing| {
                // 已经有替换，检查一致性
                return existing.eql(t2);
            } else {
                // 新的替换
                try subst.put(t1.generic, t2);
                return true;
            }
        }

        // 如果 t2 是泛型参数
        if (t2 == .generic) {
            if (subst.get(t2.generic)) |existing| {
                return existing.eql(t1);
            } else {
                try subst.put(t2.generic, t1);
                return true;
            }
        }

        // 递归处理复合类型
        switch (t1) {
            .array => |arr1| {
                if (t2 != .array) return false;
                return try self.unifyWithSubst(arr1.element.*, t2.array.element.*, subst);
            },
            .pointer => |ptr1| {
                if (t2 != .pointer) return false;
                return try self.unifyWithSubst(ptr1.*, t2.pointer.*, subst);
            },
            .generic_instance => |gi1| {
                if (t2 != .generic_instance) return false;
                const gi2 = t2.generic_instance;
                
                if (!std.mem.eql(u8, gi1.name, gi2.name)) return false;
                if (gi1.type_args.len != gi2.type_args.len) return false;
                
                for (gi1.type_args, gi2.type_args) |ta1, ta2| {
                    if (!try self.unifyWithSubst(ta1, ta2, subst)) {
                        return false;
                    }
                }
                return true;
            },
            else => {
                // 基础类型：必须完全相等
                return t1.eql(t2);
            },
        }
    }

    /// 应用类型替换
    pub fn applySubstitution(
        self: *TypeInference,
        ty: ast.Type,
        subst: *const TypeSubstitution,
    ) !ast.Type {
        return switch (ty) {
            .generic => |name| subst.get(name) orelse ty,
            .array => |arr| blk: {
                const elem = try self.applySubstitution(arr.element.*, subst);
                const elem_ptr = try self.allocator.create(ast.Type);
                elem_ptr.* = elem;
                break :blk ast.Type{
                    .array = .{
                        .element = elem_ptr,
                        .size = arr.size,
                    },
                };
            },
            .pointer => |ptr| blk: {
                const inner = try self.applySubstitution(ptr.*, subst);
                const inner_ptr = try self.allocator.create(ast.Type);
                inner_ptr.* = inner;
                break :blk ast.Type{ .pointer = inner_ptr };
            },
            .generic_instance => |gi| blk: {
                var new_args = try std.ArrayList(ast.Type).initCapacity(self.allocator, gi.type_args.len);
                for (gi.type_args) |arg| {
                    const new_arg = try self.applySubstitution(arg, subst);
                    try new_args.append(new_arg);
                }
                break :blk ast.Type{
                    .generic_instance = .{
                        .name = gi.name,
                        .type_args = try new_args.toOwnedSlice(),
                    },
                };
            },
            else => ty,
        };
    }
};

// ============================================================================
// 单态化引擎
// ============================================================================

pub const Monomorphizer = struct {
    allocator: std.mem.Allocator,
    instances: std.ArrayList(GenericInstance),
    /// 记录已实例化的泛型，避免重复
    seen: std.StringHashMap(void),
    /// 🆕 泛型结构体实例
    struct_instances: std.ArrayList(GenericStructInstance),
    struct_seen: std.StringHashMap(void),
    /// 🆕 泛型方法实例
    method_instances: std.ArrayList(GenericMethodInstance),
    method_seen: std.StringHashMap(void),

    pub fn init(allocator: std.mem.Allocator) Monomorphizer {
        return Monomorphizer{
            .allocator = allocator,
            .instances = std.ArrayList(GenericInstance).init(allocator),
            .seen = std.StringHashMap(void).init(allocator),
            .struct_instances = std.ArrayList(GenericStructInstance).init(allocator),
            .struct_seen = std.StringHashMap(void).init(allocator),
            .method_instances = std.ArrayList(GenericMethodInstance).init(allocator),
            .method_seen = std.StringHashMap(void).init(allocator),
        };
    }

    pub fn deinit(self: *Monomorphizer) void {
        // 释放所有实例
        for (self.instances.items) |instance| {
            instance.deinit(self.allocator);
        }
        for (self.struct_instances.items) |instance| {
            instance.deinit(self.allocator);
        }
        for (self.method_instances.items) |instance| {
            instance.deinit(self.allocator);
        }
        
        self.instances.deinit();
        self.seen.deinit();
        self.struct_instances.deinit();
        self.struct_seen.deinit();
        self.method_instances.deinit();
        self.method_seen.deinit();
    }

    /// 记录一个泛型实例化
    pub fn recordInstance(
        self: *Monomorphizer,
        generic_name: []const u8,
        type_args: []ast.Type,
    ) ![]const u8 {
        const mangled = try self.mangleName(generic_name, type_args);
        
        // 检查是否已经实例化过
        if (self.seen.contains(mangled)) {
            // 已存在，释放传入的 type_args 和 mangled
            self.allocator.free(type_args);
            self.allocator.free(mangled);
            // 返回已存在的 mangled name
            for (self.instances.items) |instance| {
                if (std.mem.eql(u8, instance.generic_name, generic_name) and 
                    instance.type_args.len == type_args.len) {
                    return instance.mangled_name;
                }
            }
            return generic_name; // fallback
        }

        try self.seen.put(mangled, {});
        try self.instances.append(GenericInstance{
            .generic_name = generic_name,
            .type_args = type_args,
            .mangled_name = mangled,
        });

        return mangled;
    }

    /// 🆕 记录一个泛型结构体实例化
    pub fn recordStructInstance(
        self: *Monomorphizer,
        struct_name: []const u8,
        type_args: []ast.Type,
    ) ![]const u8 {
        const mangled = try self.mangleName(struct_name, type_args);
        
        // 检查是否已经实例化过
        if (self.struct_seen.contains(mangled)) {
            // 已存在，释放传入的 type_args 和 mangled
            self.allocator.free(type_args);
            self.allocator.free(mangled);
            // 返回已存在的 mangled name
            for (self.struct_instances.items) |instance| {
                if (std.mem.eql(u8, instance.generic_name, struct_name)) {
                    // 检查 type_args 是否匹配
                    if (instance.type_args.len == type_args.len) {
                        return instance.mangled_name;
                    }
                }
            }
            return mangled; // fallback
        }

        try self.struct_seen.put(mangled, {});
        try self.struct_instances.append(GenericStructInstance{
            .generic_name = struct_name,
            .type_args = type_args,
            .mangled_name = mangled,
        });

        return mangled;
    }

    /// 🆕 记录一个泛型方法实例化
    pub fn recordMethodInstance(
        self: *Monomorphizer,
        struct_name: []const u8,
        method_name: []const u8,
        type_args: []ast.Type,
    ) ![]const u8 {
        // 生成方法的修饰名称: Vec_i32_new
        const struct_mangled = try self.mangleName(struct_name, type_args);
        defer self.allocator.free(struct_mangled);
        
        var mangled = std.ArrayList(u8).init(self.allocator);
        try mangled.appendSlice(struct_mangled);
        try mangled.append('_');
        try mangled.appendSlice(method_name);
        const mangled_name = try mangled.toOwnedSlice();
        
        // 检查是否已经实例化过
        if (self.method_seen.contains(mangled_name)) {
            // 已存在，释放传入的 type_args 和 mangled_name
            self.allocator.free(type_args);
            self.allocator.free(mangled_name);
            // 返回已存在的 mangled name
            for (self.method_instances.items) |instance| {
                if (std.mem.eql(u8, instance.struct_name, struct_name) and
                    std.mem.eql(u8, instance.method_name, method_name)) {
                    return instance.mangled_name;
                }
            }
            return mangled_name; // fallback
        }

        try self.method_seen.put(mangled_name, {});
        try self.method_instances.append(GenericMethodInstance{
            .struct_name = struct_name,
            .method_name = method_name,
            .type_args = type_args,
            .mangled_name = mangled_name,
        });

        return mangled_name;
    }

    /// 名称修饰 (Name Mangling)
    /// Vec<i32> -> Vec_i32
    /// HashMap<string, i32> -> HashMap_string_i32
    fn mangleName(
        self: *Monomorphizer,
        base_name: []const u8,
        type_args: []ast.Type,
    ) ![]const u8 {
        // 预估大小以减少重新分配
        const estimated_size = base_name.len + type_args.len * 8;
        var result = try std.ArrayList(u8).initCapacity(self.allocator, estimated_size);
        errdefer result.deinit();
        
        try result.appendSlice(base_name);

        for (type_args) |ty| {
            try result.append('_');
            try self.appendTypeName(&result, ty);
        }

        return try result.toOwnedSlice();
    }

    fn appendTypeName(self: *Monomorphizer, buf: *std.ArrayList(u8), ty: ast.Type) !void {
        switch (ty) {
            .i8 => try buf.appendSlice("i8"),
            .i16 => try buf.appendSlice("i16"),
            .i32 => try buf.appendSlice("i32"),
            .i64 => try buf.appendSlice("i64"),
            .i128 => try buf.appendSlice("i128"),
            .u8 => try buf.appendSlice("u8"),
            .u16 => try buf.appendSlice("u16"),
            .u32 => try buf.appendSlice("u32"),
            .u64 => try buf.appendSlice("u64"),
            .u128 => try buf.appendSlice("u128"),
            .f32 => try buf.appendSlice("f32"),
            .f64 => try buf.appendSlice("f64"),
            .bool => try buf.appendSlice("bool"),
            .char => try buf.appendSlice("char"),
            .string => try buf.appendSlice("string"),
            .void => try buf.appendSlice("void"),
            .named => |name| try buf.appendSlice(name),
            .generic => |name| try buf.appendSlice(name),
            .pointer => |ptr| {
                try buf.appendSlice("ptr_");
                try self.appendTypeName(buf, ptr.*);
            },
            .array => |arr| {
                try buf.appendSlice("arr_");
                try self.appendTypeName(buf, arr.element.*);
            },
            .generic_instance => |gi| {
                try buf.appendSlice(gi.name);
                for (gi.type_args) |arg| {
                    try buf.append('_');
                    try self.appendTypeName(buf, arg);
                }
            },
            else => try buf.appendSlice("unknown"),
        }
    }

    /// 实例化一个泛型函数
    pub fn instantiateFunction(
        self: *Monomorphizer,
        func: ast.FunctionDecl,
        subst: *const TypeSubstitution,
    ) !ast.FunctionDecl {
        var inference = TypeInference.init(self.allocator);
        defer inference.deinit();

        // 替换返回类型
        const new_return_type = try inference.applySubstitution(func.return_type, subst);

        // 替换参数类型（预分配容量）
        var new_params = try std.ArrayList(ast.Param).initCapacity(self.allocator, func.params.len);
        for (func.params) |param| {
            const new_type = try inference.applySubstitution(param.type, subst);
            try new_params.append(ast.Param{
                .name = param.name,
                .type = new_type,
            });
        }

        // 替换函数体（递归处理表达式和语句）
        var new_body = std.ArrayList(ast.Stmt).init(self.allocator);
        for (func.body) |stmt| {
            const new_stmt = try self.instantiateStmt(stmt, subst);
            try new_body.append(new_stmt);
        }

        // 生成修饰后的名称
        var type_args = std.ArrayList(ast.Type).init(self.allocator);
        defer type_args.deinit();
        for (func.type_params) |tp| {
            if (subst.get(tp)) |ty| {
                try type_args.append(ty);
            }
        }
        const mangled_name = try self.mangleName(func.name, try type_args.toOwnedSlice());

        return ast.FunctionDecl{
            .name = mangled_name,
            .type_params = &[_][]const u8{}, // 实例化后没有类型参数
            .params = try new_params.toOwnedSlice(),
            .return_type = new_return_type,
            .body = try new_body.toOwnedSlice(),
            .is_public = func.is_public,
            .is_async = func.is_async,
        };
    }

    fn instantiateStmt(
        self: *Monomorphizer,
        stmt: ast.Stmt,
        subst: *const TypeSubstitution,
    ) error{OutOfMemory}!ast.Stmt {
        // 简化实现：暂时不处理类型替换
        // 完整实现需要递归处理所有表达式中的类型
        _ = self;
        _ = subst;
        return stmt;
    }
};

// ============================================================================
// 泛型上下文：整合所有泛型处理
// ============================================================================

pub const GenericContext = struct {
    allocator: std.mem.Allocator,
    inference: TypeInference,
    monomorphizer: Monomorphizer,
    /// 函数表：用于获取泛型函数的定义
    function_table: *std.StringHashMap(ast.FunctionDecl),

    pub fn init(allocator: std.mem.Allocator) GenericContext {
        return GenericContext{
            .allocator = allocator,
            .inference = TypeInference.init(allocator),
            .monomorphizer = Monomorphizer.init(allocator),
            .function_table = undefined, // 需要外部设置
        };
    }

    pub fn deinit(self: *GenericContext) void {
        self.inference.deinit();
        self.monomorphizer.deinit();
    }

    /// 处理泛型函数调用，返回实例化后的函数名
    pub fn processGenericCall(
        self: *GenericContext,
        func_name: []const u8,
        type_params: [][]const u8,
        param_types: []ast.Type,
        arg_types: []ast.Type,
    ) ![]const u8 {
        // 1. 类型推导
        const subst = try self.inference.inferFromCall(type_params, param_types, arg_types);
        defer subst.deinit();

        // 2. 收集类型参数
        var type_args = std.ArrayList(ast.Type).init(self.allocator);
        defer type_args.deinit();
        for (type_params) |tp| {
            if (subst.get(tp)) |ty| {
                try type_args.append(ty);
            }
        }

        // 3. 记录实例化
        const mangled_name = try self.monomorphizer.recordInstance(
            func_name,
            try type_args.toOwnedSlice(),
        );

        return mangled_name;
    }

    /// 🆕 从参数类型推导泛型实例
    pub fn inferGenericInstance(
        self: *GenericContext,
        func_name: []const u8,
        arg_types: []ast.Type,
    ) ![]const u8 {
        if (arg_types.len == 0) {
            self.allocator.free(arg_types);
            return func_name; // 没有参数，不是泛型调用
        }

        // recordInstance 会接管 arg_types 的所有权
        const mangled_name = try self.monomorphizer.recordInstance(func_name, arg_types);
        return mangled_name;
    }

    /// 🆕 收集 Program 中所有的泛型调用
    pub fn collectGenericCalls(self: *GenericContext, program: ast.Program) !void {
        for (program.declarations) |decl| {
            try self.collectDeclCalls(decl);
        }
    }

    fn collectDeclCalls(self: *GenericContext, decl: ast.TopLevelDecl) !void {
        switch (decl) {
            .function => |func| {
                for (func.body) |stmt| {
                    try self.collectStmtCalls(stmt);
                }
            },
            .type_decl => |td| {
                switch (td.kind) {
                    .struct_type => |st| {
                        for (st.methods) |method| {
                            for (method.body) |stmt| {
                                try self.collectStmtCalls(stmt);
                            }
                        }
                    },
                    .enum_type => |et| {
                        for (et.methods) |method| {
                            for (method.body) |stmt| {
                                try self.collectStmtCalls(stmt);
                            }
                        }
                    },
                    else => {},
                }
            },
            else => {},
        }
    }

    fn collectStmtCalls(self: *GenericContext, stmt: ast.Stmt) error{OutOfMemory}!void {
        switch (stmt) {
            .expr => |expr| try self.collectExprCalls(expr),
            .let_decl => |let| {
                if (let.init) |init_expr| {
                    try self.collectExprCalls(init_expr);
                }
            },
            .assign => |assign| {
                try self.collectExprCalls(assign.target);
                try self.collectExprCalls(assign.value);
            },
            .compound_assign => |ca| {
                try self.collectExprCalls(ca.target);
                try self.collectExprCalls(ca.value);
            },
            .return_stmt => |ret| {
                if (ret) |expr| {
                    try self.collectExprCalls(expr);
                }
            },
            .loop_stmt => |loop| {
                if (loop.condition) |cond| {
                    try self.collectExprCalls(cond);
                }
                if (loop.iterator) |iter| {
                    try self.collectExprCalls(iter.iterable);
                }
                for (loop.body) |body_stmt| {
                    try self.collectStmtCalls(body_stmt);
                }
            },
            else => {},
        }
    }

    fn collectExprCalls(self: *GenericContext, expr: ast.Expr) error{OutOfMemory}!void {
        switch (expr) {
            .call => |call| {
                // 检查 callee
                try self.collectExprCalls(call.callee.*);

                // 检查参数
                for (call.args) |arg| {
                    try self.collectExprCalls(arg);
                }

                // 🆕 如果是泛型函数调用，记录实例化
                if (call.callee.* == .identifier) {
                    const func_name = call.callee.identifier;
                    if (self.function_table.get(func_name)) |func| {
                        if (func.type_params.len > 0) {
                            // 这是泛型函数！收集参数类型
                            var arg_types = std.ArrayList(ast.Type).init(self.allocator);
                            defer arg_types.deinit();
                            
                            // 🆕 从参数表达式推导类型
                            for (call.args) |arg| {
                                const arg_type = inferTypeFromExpr(arg);
                                try arg_types.append(arg_type);
                            }

                            _ = try self.inferGenericInstance(func_name, try arg_types.toOwnedSlice());
                        }
                    }
                }
            },
            .static_method_call => |smc| {
                // 🆕 收集泛型方法调用
                // Vec<i32>::new() -> 记录 Vec, new, [i32]
                
                // 检查参数
                for (smc.args) |arg| {
                    try self.collectExprCalls(arg);
                }
                
                // 如果有类型参数，记录这个方法实例
                if (smc.type_args.len > 0) {
                    var type_args = std.ArrayList(ast.Type).init(self.allocator);
                    for (smc.type_args) |type_arg| {
                        try type_args.append(type_arg);
                    }
                    
                    _ = try self.monomorphizer.recordMethodInstance(
                        smc.type_name,
                        smc.method_name,
                        try type_args.toOwnedSlice(),
                    );
                    
                    // 同时记录结构体实例（如果还没有）
                    var struct_type_args = std.ArrayList(ast.Type).init(self.allocator);
                    for (smc.type_args) |type_arg| {
                        try struct_type_args.append(type_arg);
                    }
                    
                    _ = try self.monomorphizer.recordStructInstance(
                        smc.type_name,
                        try struct_type_args.toOwnedSlice(),
                    );
                }
            },
            .binary => |bin| {
                try self.collectExprCalls(bin.left.*);
                try self.collectExprCalls(bin.right.*);
            },
            .unary => |un| {
                try self.collectExprCalls(un.operand.*);
            },
            .field_access => |fa| {
                try self.collectExprCalls(fa.object.*);
            },
            .array_index => |ai| {
                try self.collectExprCalls(ai.array.*);
                try self.collectExprCalls(ai.index.*);
            },
            .struct_init => |si| {
                for (si.fields) |field| {
                    try self.collectExprCalls(field.value);
                }
            },
            .if_expr => |if_expr| {
                try self.collectExprCalls(if_expr.condition.*);
                try self.collectExprCalls(if_expr.then_branch.*);
                if (if_expr.else_branch) |else_br| {
                    try self.collectExprCalls(else_br.*);
                }
            },
            .is_expr => |is_expr| {
                try self.collectExprCalls(is_expr.value.*);
            },
            .block => |stmts| {
                for (stmts) |stmt| {
                    try self.collectStmtCalls(stmt);
                }
            },
            .array_literal => |elems| {
                for (elems) |elem| {
                    try self.collectExprCalls(elem);
                }
            },
            .string_interp => |si| {
                for (si.parts) |part| {
                    if (part == .expr) {
                        try self.collectExprCalls(part.expr);
                    }
                }
            },
            .try_expr => |inner| {
                try self.collectExprCalls(inner.*);
            },
            else => {},
        }
    }
};

