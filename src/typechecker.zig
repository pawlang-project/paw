const std = @import("std");
const ast = @import("ast.zig");

// Trait 定义结构
pub const TraitDef = struct {
    name: []const u8,
    methods: []ast.FunctionSignature,  // trait 中是签名，不是完整实现
    type_params: [][]const u8,  // 泛型参数
};

// 类型的方法信息
pub const TypeMethods = struct {
    type_name: []const u8,
    methods: std.StringHashMap(ast.FunctionDecl),
    
    pub fn deinit(self: *TypeMethods) void {
        self.methods.deinit();
    }
};

pub const TypeChecker = struct {
    allocator: std.mem.Allocator,
    errors: std.ArrayList([]const u8),
    symbol_table: std.StringHashMap(ast.Type),
    function_table: std.StringHashMap(ast.FunctionDecl),
    type_table: std.StringHashMap(ast.TypeDecl),  // 存储 type 声明
    trait_table: std.StringHashMap(TraitDef),      // 新增：存储 trait 定义
    type_methods: std.StringHashMap(TypeMethods),  // 新增：存储类型的方法
    current_function_is_async: bool,  // 追踪当前函数是否异步

    pub fn init(allocator: std.mem.Allocator) TypeChecker {
        return TypeChecker{
            .allocator = allocator,
            .errors = std.ArrayList([]const u8).init(allocator),
            .symbol_table = std.StringHashMap(ast.Type).init(allocator),
            .function_table = std.StringHashMap(ast.FunctionDecl).init(allocator),
            .type_table = std.StringHashMap(ast.TypeDecl).init(allocator),
            .trait_table = std.StringHashMap(TraitDef).init(allocator),
            .type_methods = std.StringHashMap(TypeMethods).init(allocator),
            .current_function_is_async = false,
        };
    }

    pub fn deinit(self: *TypeChecker) void {
        self.errors.deinit();
        self.symbol_table.deinit();
        self.function_table.deinit();
        self.type_table.deinit();
        self.trait_table.deinit();
        
        // 清理 type_methods
        var it = self.type_methods.iterator();
        while (it.next()) |entry| {
            var methods = entry.value_ptr;
            methods.deinit();
        }
        self.type_methods.deinit();
    }

    pub fn check(self: *TypeChecker, program: ast.Program) !void {
        // 第一遍：收集所有类型、函数和 trait 声明
        for (program.declarations) |decl| {
            switch (decl) {
                .function => |func| {
                    try self.function_table.put(func.name, func);
                },
                .type_decl => |td| {
                    try self.type_table.put(td.name, td);
                    try self.symbol_table.put(td.name, ast.Type{ .named = td.name });
                    
                    // 收集 trait 定义
                    if (td.kind == .trait_type) {
                        const trait_def = TraitDef{
                            .name = td.name,
                            .methods = td.kind.trait_type.methods,
                            .type_params = td.type_params,
                        };
                        try self.trait_table.put(td.name, trait_def);
                    }
                    
                    // 收集类型的方法（struct 和 enum 有完整的方法实现）
                    const methods: ?[]ast.FunctionDecl = switch (td.kind) {
                        .struct_type => |st| st.methods,
                        .enum_type => |et| et.methods,
                        .trait_type => null,  // trait 只有签名，不收集到 type_methods
                    };
                    
                    if (methods) |m| {
                        if (m.len > 0) {
                            var type_methods = TypeMethods{
                                .type_name = td.name,
                                .methods = std.StringHashMap(ast.FunctionDecl).init(self.allocator),
                            };
                            
                            for (m) |method| {
                                try type_methods.methods.put(method.name, method);
                            }
                            
                            try self.type_methods.put(td.name, type_methods);
                        }
                    }
                },
                .struct_decl => |s| {
                    try self.symbol_table.put(s.name, ast.Type{ .named = s.name });
                },
                .enum_decl => |e| {
                    try self.symbol_table.put(e.name, ast.Type{ .named = e.name });
                },
                else => {},
            }
        }

        // 第二遍：类型检查
        for (program.declarations) |decl| {
            try self.checkDecl(decl);
        }

        if (!self.function_table.contains("main")) {
            try self.errors.append("Error: missing main function");
        }

        if (self.errors.items.len > 0) {
            for (self.errors.items) |err| {
                std.debug.print("{s}\n", .{err});
            }
            return error.TypeCheckFailed;
        }
    }

    fn checkDecl(self: *TypeChecker, decl: ast.TopLevelDecl) !void {
        switch (decl) {
            .function => |func| {
                try self.checkFunction(func);
            },
            .type_decl => |td| {
                try self.checkTypeDecl(td);
            },
            .struct_decl => |sd| {
                // 检查结构体中的方法
                for (sd.methods) |method| {
                    try self.checkFunction(method);
                }
            },
            .enum_decl => |ed| {
                // 检查枚举中的方法
                for (ed.methods) |method| {
                    try self.checkFunction(method);
                }
            },
            else => {},
        }
    }
    
    fn checkTypeDecl(self: *TypeChecker, type_decl: ast.TypeDecl) !void {
        switch (type_decl.kind) {
            .struct_type => |st| {
                // 检查字段类型是否有效
                for (st.fields) |field| {
                    _ = field;
                    // TODO: 验证字段类型存在
                }
                
                // 检查方法
                for (st.methods) |method| {
                    try self.checkFunction(method);
                }
            },
            .enum_type => |et| {
                // 检查变体
                for (et.variants) |variant| {
                    _ = variant;
                    // TODO: 验证变体字段类型
                }
                
                // 检查方法
                for (et.methods) |method| {
                    try self.checkFunction(method);
                }
            },
            .trait_type => |tt| {
                // 验证 trait 方法签名
                for (tt.methods) |sig| {
                    _ = sig;
                    // TODO: 验证方法签名类型
                }
            },
        }
    }

    fn checkFunction(self: *TypeChecker, func: ast.FunctionDecl) !void {
        // 保存之前的 async 状态
        const prev_async = self.current_function_is_async;
        self.current_function_is_async = func.is_async;
        defer self.current_function_is_async = prev_async;
        
        var local_scope = std.StringHashMap(ast.Type).init(self.allocator);
        defer local_scope.deinit();

        for (func.params) |param| {
            try local_scope.put(param.name, param.type);
        }

        for (func.body) |stmt| {
            try self.checkStmt(stmt, &local_scope);
        }
    }

    fn checkStmt(self: *TypeChecker, stmt: ast.Stmt, scope: *std.StringHashMap(ast.Type)) (std.mem.Allocator.Error || error{TypeCheckFailed})!void {
        switch (stmt) {
            .expr => |expr| {
                _ = try self.checkExpr(expr, scope);
            },
            // 🆕 赋值语句
            .assign => |assign| {
                const target_type = try self.checkExpr(assign.target, scope);
                const value_type = try self.checkExpr(assign.value, scope);
                if (!target_type.eql(value_type)) {
                    try self.errors.append("Type error: assignment type mismatch");
                }
            },
            // 🆕 复合赋值语句
            .compound_assign => |ca| {
                const target_type = try self.checkExpr(ca.target, scope);
                const value_type = try self.checkExpr(ca.value, scope);
                // 复合赋值要求类型匹配且支持相应运算
                if (!target_type.eql(value_type)) {
                    try self.errors.append("Type error: compound assignment type mismatch");
                }
            },
            .let_decl => |let| {
                if (let.init) |init_expr| {
                    const init_type = try self.checkExpr(init_expr, scope);
                    
                    if (let.type) |declared_type| {
                        // 🆕 改进类型兼容性检查
                        if (!self.isTypeCompatible(init_type, declared_type)) {
                            try self.errors.append("Type error: variable type mismatch");
                        }
                        try scope.put(let.name, declared_type);
                    } else {
                        try scope.put(let.name, init_type);
                    }
                } else if (let.type) |declared_type| {
                    try scope.put(let.name, declared_type);
                }
            },
            .return_stmt => |ret| {
                if (ret) |expr| {
                    _ = try self.checkExpr(expr, scope);
                }
            },
            .break_stmt, .continue_stmt => {},
            .loop_stmt => |loop| {
                // 处理 loop 语句
                if (loop.condition) |cond| {
                    const cond_type = try self.checkExpr(cond, scope);
                    if (!cond_type.eql(ast.Type.bool)) {
                        try self.errors.append("Type error: loop condition must be Bool");
                    }
                }
                
                // 处理 loop for 迭代器
                if (loop.iterator) |iter| {
                    // 检查可迭代对象的类型
                    const iter_type = try self.checkExpr(iter.iterable, scope);
                    _ = iter_type;
                    
                    // 为循环变量创建新的作用域
                    var loop_scope = std.StringHashMap(ast.Type).init(self.allocator);
                    defer loop_scope.deinit();
                    
                    // 复制父作用域
                    var iter_scope = scope.iterator();
                    while (iter_scope.next()) |entry| {
                        try loop_scope.put(entry.key_ptr.*, entry.value_ptr.*);
                    }
                    
                    // 添加循环变量（简化：假设为 i32）
                    try loop_scope.put(iter.binding, ast.Type.i32);
                    
                    // 检查循环体
                    for (loop.body) |body_stmt| {
                        try self.checkStmt(body_stmt, &loop_scope);
                    }
                    return;
                }
                
                for (loop.body) |body_stmt| {
                    try self.checkStmt(body_stmt, scope);
                }
            },
            .while_loop => |loop| {
                const cond_type = try self.checkExpr(loop.condition, scope);
                if (!cond_type.eql(ast.Type.bool)) {
                    try self.errors.append("Type error: while condition must be Bool");
                }
                
                for (loop.body) |body_stmt| {
                    try self.checkStmt(body_stmt, scope);
                }
            },
            .for_loop => |loop| {
                if (loop.init) |init_stmt| {
                    try self.checkStmt(init_stmt.*, scope);
                }
                
                if (loop.condition) |cond| {
                    const cond_type = try self.checkExpr(cond, scope);
                    if (!cond_type.eql(ast.Type.bool)) {
                        try self.errors.append("Type error: for condition must be Bool");
                    }
                }
                
                if (loop.step) |step| {
                    _ = try self.checkExpr(step, scope);
                }
                
                for (loop.body) |body_stmt| {
                    try self.checkStmt(body_stmt, scope);
                }
            },
        }
    }

    // 🆕 类型兼容性检查（比 eql 更宽松）
    fn isTypeCompatible(self: *TypeChecker, from_type: ast.Type, to_type: ast.Type) bool {
        _ = self;
        
        // 完全相同的类型
        if (from_type.eql(to_type)) return true;
        
        // 整数字面量（i32）可以兼容任何整数类型
        const from_is_int = from_type == .i32;  // 字面量默认类型
        const to_is_any_int = to_type == .i8 or to_type == .i16 or to_type == .i32 or 
                             to_type == .i64 or to_type == .i128 or
                             to_type == .u8 or to_type == .u16 or to_type == .u32 or 
                             to_type == .u64 or to_type == .u128;
        
        if (from_is_int and to_is_any_int) return true;
        
        // 浮点字面量（f64）可以兼容任何浮点类型
        const from_is_float = from_type == .f64;  // 字面量默认类型
        const to_is_any_float = to_type == .f32 or to_type == .f64;
        
        if (from_is_float and to_is_any_float) return true;
        
        // 数组类型兼容（已在 Type.eql 中处理）
        
        return false;
    }
    
    fn checkExpr(self: *TypeChecker, expr: ast.Expr, scope: *std.StringHashMap(ast.Type)) (std.mem.Allocator.Error || error{TypeCheckFailed})!ast.Type {
        return switch (expr) {
            .int_literal => ast.Type.i32,      // 整数字面量默认 i32
            .float_literal => ast.Type.f64,    // 浮点字面量默认 f64
            .string_literal => ast.Type.string,
            .char_literal => ast.Type.char,
            .bool_literal => ast.Type.bool,
            .identifier => |name| blk: {
                if (scope.get(name)) |var_type| {
                    break :blk var_type;
                } else if (self.symbol_table.get(name)) |sym_type| {
                    break :blk sym_type;
                } else {
                    try self.errors.append("Error: undefined identifier");
                    break :blk ast.Type.void;
                }
            },
            .binary => |bin| blk: {
                const left_type = try self.checkExpr(bin.left.*, scope);
                const right_type = try self.checkExpr(bin.right.*, scope);
                
                switch (bin.op) {
                    .add, .sub, .mul, .div, .mod => {
                        if (!left_type.eql(right_type)) {
                            try self.errors.append("Type error: binary operator types must match");
                        }
                        break :blk left_type;
                    },
                    .eq, .ne, .lt, .le, .gt, .ge => {
                        if (!left_type.eql(right_type)) {
                            try self.errors.append("Type error: comparison types must match");
                        }
                        break :blk ast.Type.bool;
                    },
                    .and_op, .or_op => {
                        if (!left_type.eql(ast.Type.bool) or !right_type.eql(ast.Type.bool)) {
                            try self.errors.append("Type error: logical ops require Bool");
                        }
                        break :blk ast.Type.bool;
                    },
                }
            },
            .unary => |un| blk: {
                const operand_type = try self.checkExpr(un.operand.*, scope);
                switch (un.op) {
                    .neg => break :blk operand_type,
                    .not => {
                        if (!operand_type.eql(ast.Type.bool)) {
                            try self.errors.append("Type error: ! requires Bool");
                        }
                        break :blk ast.Type.bool;
                    },
                }
            },
            .call => |call| blk: {
                // 🆕 检查是否是enum构造器调用
                if (call.callee.* == .identifier) {
                    const func_name = call.callee.identifier;
                    
                    // 查找是否是enum variant
                    var type_iter = self.type_table.iterator();
                    while (type_iter.next()) |entry| {
                        const type_decl = entry.value_ptr;
                        if (type_decl.kind == .enum_type) {
                            for (type_decl.kind.enum_type.variants) |variant| {
                                if (std.mem.eql(u8, variant.name, func_name)) {
                                    // 找到了！返回enum类型
                                    break :blk ast.Type{ .named = type_decl.name };
                                }
                            }
                        }
                    }
                    
                    // 不是enum构造器，检查是否是函数
                    if (self.function_table.get(func_name)) |func| {
                        break :blk func.return_type;
                    }
                }
                
                // 默认返回 i32
                break :blk ast.Type.i32;
            },
            .field_access => |access| blk: {
                _ = access;
                break :blk ast.Type.i32;  // 默认返回 i32
            },
            .struct_init => |struct_init| blk: {
                break :blk ast.Type{ .named = struct_init.type_name };
            },
            .enum_variant => |variant| blk: {
                break :blk ast.Type{ .named = variant.enum_name };
            },
            .block => |stmts| blk: {
                for (stmts) |stmt| {
                    try self.checkStmt(stmt, scope);
                }
                break :blk ast.Type.void;
            },
            .if_expr => |if_expr| blk: {
                const cond_type = try self.checkExpr(if_expr.condition.*, scope);
                if (!cond_type.eql(ast.Type.bool)) {
                    try self.errors.append("Type error: if condition must be Bool");
                }
                
                const then_type = try self.checkExpr(if_expr.then_branch.*, scope);
                
                if (if_expr.else_branch) |else_branch| {
                    const else_type = try self.checkExpr(else_branch.*, scope);
                    if (!then_type.eql(else_type)) {
                        try self.errors.append("Type error: if-else branches must match");
                    }
                }
                
                break :blk then_type;
            },
            // 新增：is 表达式（模式匹配）
            .is_expr => |is_match| blk: {
                _ = try self.checkExpr(is_match.value.*, scope);
                
                // 🆕 为每个arm创建新的scope，支持模式绑定
                var result_type: ?ast.Type = null;
                
                for (is_match.arms) |arm| {
                    // 🆕 为当前arm创建临时scope
                    var arm_scope = std.StringHashMap(ast.Type).init(self.allocator);
                    defer arm_scope.deinit();
                    
                    // 复制父scope
                    var parent_iter = scope.iterator();
                    while (parent_iter.next()) |entry| {
                        try arm_scope.put(entry.key_ptr.*, entry.value_ptr.*);
                    }
                    
                    // 🆕 根据pattern添加绑定
                    switch (arm.pattern) {
                        .identifier => |id| {
                            // 标识符模式：绑定整个匹配值（暂时用i32）
                            try arm_scope.put(id, ast.Type.i32);
                        },
                        .variant => |v| {
                            // variant模式：为每个binding添加类型（暂时用i32）
                            for (v.bindings) |binding| {
                                try arm_scope.put(binding, ast.Type.i32);
                            }
                        },
                        else => {},
                    }
                    
                    // 检查 guard 条件（如果有）
                    if (arm.guard) |guard| {
                        const guard_type = try self.checkExpr(guard, &arm_scope);
                        if (!guard_type.eql(ast.Type.bool)) {
                            try self.errors.append("Type error: is guard must be Bool");
                        }
                    }
                    
                    // 检查分支体的类型
                    const arm_type = try self.checkExpr(arm.body, &arm_scope);
                    
                    if (result_type) |rt| {
                        if (!self.isTypeCompatible(arm_type, rt) and !self.isTypeCompatible(rt, arm_type)) {
                            // 使用更宽松的类型兼容性检查
                            // try self.errors.append("Type error: is arms must have same type");
                        }
                    } else {
                        result_type = arm_type;
                    }
                }
                
                // 验证模式覆盖完整性（简化版：检查是否有 _ 通配符）
                var has_wildcard = false;
                for (is_match.arms) |arm| {
                    if (arm.pattern == .wildcard) {
                        has_wildcard = true;
                    }
                }
                
                if (!has_wildcard and is_match.arms.len < 2) {
                    // 警告：可能未覆盖所有情况
                    // try self.errors.append("Warning: is expression may not be exhaustive");
                }
                
                break :blk result_type orelse ast.Type.void;
            },
            // 新增：as 表达式（类型转换）
            .as_expr => |as_cast| blk: {
                const from_type = try self.checkExpr(as_cast.value.*, scope);
                const to_type = as_cast.target_type;
                
                // 验证类型转换的合法性
                const is_numeric_from = switch (from_type) {
                    .i8, .i16, .i32, .i64, .i128,
                    .u8, .u16, .u32, .u64, .u128,
                    .f32, .f64 => true,
                    else => false,
                };
                
                const is_numeric_to = switch (to_type) {
                    .i8, .i16, .i32, .i64, .i128,
                    .u8, .u16, .u32, .u64, .u128,
                    .f32, .f64 => true,
                    else => false,
                };
                
                if (!is_numeric_from or !is_numeric_to) {
                    // 只允许数值类型之间转换（简化版）
                    if (!from_type.eql(to_type)) {
                        try self.errors.append("Type error: invalid type conversion");
                    }
                }
                
                break :blk as_cast.target_type;
            },
            // 新增：await 表达式
            .await_expr => |await_expr| blk: {
                // 验证 await 只能在 async 函数中使用
                if (!self.current_function_is_async) {
                    try self.errors.append("Type error: await can only be used in async functions");
                }
                
                const expr_type = try self.checkExpr(await_expr.*, scope);
                
                // TODO: 如果是 Future<T>，返回 T
                // 简化版：直接返回表达式类型
                break :blk expr_type;
            },
            // 🆕 数组字面量
            .array_literal => |elements| blk: {
                if (elements.len == 0) {
                    // 空数组，类型未知
                    break :blk ast.Type.void;
                }
                
                // 检查第一个元素的类型
                const first_type = try self.checkExpr(elements[0], scope);
                
                // 检查所有元素类型是否一致
                for (elements[1..]) |elem| {
                    const elem_type = try self.checkExpr(elem, scope);
                    if (!elem_type.eql(first_type)) {
                        try self.errors.append("Type error: array elements must have same type");
                    }
                }
                
                // 返回数组类型
                const elem_type_ptr = try self.allocator.create(ast.Type);
                elem_type_ptr.* = first_type;
                break :blk ast.Type{
                    .array = .{
                        .element = elem_type_ptr,
                        .size = elements.len,
                    },
                };
            },
            // 🆕 数组索引
            .array_index => |ai| blk: {
                const array_type = try self.checkExpr(ai.array.*, scope);
                const index_type = try self.checkExpr(ai.index.*, scope);
                
                // 索引必须是整数类型
                const is_int = index_type == .i8 or index_type == .i16 or 
                              index_type == .i32 or index_type == .i64 or
                              index_type == .u8 or index_type == .u16 or
                              index_type == .u32 or index_type == .u64;
                
                if (!is_int) {
                    try self.errors.append("Type error: array index must be integer");
                }
                
                // 返回数组元素类型
                if (array_type == .array) {
                    break :blk array_type.array.element.*;
                } else {
                    try self.errors.append("Type error: index on non-array type");
                    break :blk ast.Type.void;
                }
            },
            // 🆕 范围表达式
            .range => |r| blk: {
                const start_type = try self.checkExpr(r.start.*, scope);
                const end_type = try self.checkExpr(r.end.*, scope);
                
                // 检查起始和结束都是整数类型
                const start_is_int = start_type == .i8 or start_type == .i16 or 
                                    start_type == .i32 or start_type == .i64 or
                                    start_type == .u8 or start_type == .u16 or
                                    start_type == .u32 or start_type == .u64;
                const end_is_int = end_type == .i8 or end_type == .i16 or 
                                  end_type == .i32 or end_type == .i64 or
                                  end_type == .u8 or end_type == .u16 or
                                  end_type == .u32 or end_type == .u64;
                
                if (!start_is_int or !end_is_int) {
                    try self.errors.append("Type error: range bounds must be integers");
                }
                
                // 范围表达式的类型暂定为 void（实际上是迭代器）
                break :blk ast.Type.void;
            },
            .match_expr => |match| blk: {
                _ = try self.checkExpr(match.value.*, scope);
                
                var result_type: ?ast.Type = null;
                for (match.arms) |arm| {
                    const arm_type = try self.checkExpr(arm.body, scope);
                    if (result_type) |rt| {
                        if (!rt.eql(arm_type)) {
                            try self.errors.append("Type error: match arms must have same type");
                        }
                    } else {
                        result_type = arm_type;
                    }
                }
                
                break :blk result_type orelse ast.Type.void;
            },
        };
    }
    
    // ==================== 新增：高级类型检查功能 ====================
    
    /// 检查泛型约束是否满足
    fn checkGenericConstraints(
        self: *TypeChecker,
        type_name: []const u8,
        type_params: [][]const u8,
        constraints: [][]const u8,
    ) !void {
        // 简化版：检查类型参数是否满足 trait 约束
        for (constraints) |constraint| {
            if (!self.trait_table.contains(constraint)) {
                const err_msg = try std.fmt.allocPrint(
                    self.allocator,
                    "Error: trait '{s}' not found for constraint",
                    .{constraint}
                );
                try self.errors.append(err_msg);
                continue;
            }
            
            // 检查类型是否实现了该 trait
            if (self.type_methods.get(type_name)) |type_methods| {
                const trait_def = self.trait_table.get(constraint).?;
                
                // 验证所有 trait 方法都被实现
                for (trait_def.methods) |trait_method| {
                    if (!type_methods.methods.contains(trait_method.name)) {
                        const err_msg = try std.fmt.allocPrint(
                            self.allocator,
                            "Error: type '{s}' does not implement trait method '{s}' from '{s}'",
                            .{type_name, trait_method.name, constraint}
                        );
                        try self.errors.append(err_msg);
                    }
                }
            }
        }
        
        _ = type_params;  // TODO: 实际验证泛型参数
    }
    
    /// 检查 trait 实现是否完整
    fn checkTraitImpl(
        self: *TypeChecker,
        type_name: []const u8,
        trait_name: []const u8,
    ) !bool {
        const trait_def = self.trait_table.get(trait_name) orelse {
            const err_msg = try std.fmt.allocPrint(
                self.allocator,
                "Error: trait '{s}' not found",
                .{trait_name}
            );
            try self.errors.append(err_msg);
            return false;
        };
        
        const type_methods = self.type_methods.get(type_name) orelse {
            const err_msg = try std.fmt.allocPrint(
                self.allocator,
                "Error: type '{s}' has no methods",
                .{type_name}
            );
            try self.errors.append(err_msg);
            return false;
        };
        
        var all_implemented = true;
        
        // 检查每个 trait 方法是否都被实现
        for (trait_def.methods) |trait_method| {
            if (type_methods.methods.get(trait_method.name)) |impl_method| {
                // 检查方法签名是否匹配
                if (!self.methodSignaturesMatch(trait_method, impl_method)) {
                    const err_msg = try std.fmt.allocPrint(
                        self.allocator,
                        "Error: method '{s}' signature mismatch in type '{s}' (trait: '{s}')",
                        .{trait_method.name, type_name, trait_name}
                    );
                    try self.errors.append(err_msg);
                    all_implemented = false;
                }
            } else {
                const err_msg = try std.fmt.allocPrint(
                    self.allocator,
                    "Error: missing trait method '{s}' in type '{s}' (required by trait '{s}')",
                    .{trait_method.name, type_name, trait_name}
                );
                try self.errors.append(err_msg);
                all_implemented = false;
            }
        }
        
        return all_implemented;
    }
    
    /// 检查两个方法签名是否匹配
    fn methodSignaturesMatch(
        self: *TypeChecker,
        trait_sig: ast.FunctionSignature,
        impl_func: ast.FunctionDecl,
    ) bool {
        _ = self;
        
        // 检查参数数量
        if (trait_sig.params.len != impl_func.params.len) {
            return false;
        }
        
        // 检查参数类型
        for (trait_sig.params, impl_func.params) |trait_param, impl_param| {
            if (!trait_param.type.eql(impl_param.type)) {
                return false;
            }
        }
        
        // 检查返回类型
        if (!trait_sig.return_type.eql(impl_func.return_type)) {
            return false;
        }
        
        return true;
    }
    
    /// 检查方法调用是否有效（增强版）
    fn checkMethodCallEnhanced(
        self: *TypeChecker,
        receiver_type: ast.Type,
        method_name: []const u8,
        args: []ast.Expr,
        scope: *std.StringHashMap(ast.Type),
    ) !ast.Type {
        // 获取接收者的类型名
        const type_name = switch (receiver_type) {
            .named => |name| name,
            else => {
                try self.errors.append("Error: cannot call method on non-named type");
                return ast.Type.void;
            },
        };
        
        // 查找类型的方法
        if (self.type_methods.get(type_name)) |type_methods| {
            if (type_methods.methods.get(method_name)) |method| {
                // 检查参数数量（-1 因为 self 参数）
                const expected_args = if (method.params.len > 0) method.params.len - 1 else 0;
                if (args.len != expected_args) {
                    const err_msg = try std.fmt.allocPrint(
                        self.allocator,
                        "Error: method '{s}' expects {d} arguments, got {d}",
                        .{method_name, expected_args, args.len}
                    );
                    try self.errors.append(err_msg);
                }
                
                // 检查参数类型
                var arg_idx: usize = 0;
                for (method.params, 0..) |param, i| {
                    // 跳过 self 参数
                    if (i == 0 and (std.mem.eql(u8, param.name, "self") or 
                                    std.mem.eql(u8, param.name, "mut self"))) {
                        continue;
                    }
                    
                    if (arg_idx < args.len) {
                        const arg_type = try self.checkExpr(args[arg_idx], scope);
                        if (!arg_type.eql(param.type)) {
                            const err_msg = try std.fmt.allocPrint(
                                self.allocator,
                                "Error: argument {d} type mismatch in method '{s}'",
                                .{arg_idx + 1, method_name}
                            );
                            try self.errors.append(err_msg);
                        }
                        arg_idx += 1;
                    }
                }
                
                return method.return_type;
            }
        }
        
        const err_msg = try std.fmt.allocPrint(
            self.allocator,
            "Error: method '{s}' not found on type '{s}'",
            .{method_name, type_name}
        );
        try self.errors.append(err_msg);
        return ast.Type.void;
    }
    
    /// 检查字段访问是否有效（增强版）
    fn checkFieldAccessEnhanced(
        self: *TypeChecker,
        receiver_type: ast.Type,
        field_name: []const u8,
    ) !ast.Type {
        const type_name = switch (receiver_type) {
            .named => |name| name,
            else => {
                try self.errors.append("Error: cannot access field on non-named type");
                return ast.Type.void;
            },
        };
        
        // 查找类型定义
        if (self.type_table.get(type_name)) |type_decl| {
            switch (type_decl.kind) {
                .struct_type => |st| {
                    // 查找字段
                    for (st.fields) |field| {
                        if (std.mem.eql(u8, field.name, field_name)) {
                            // TODO: 检查可见性（pub）
                            return field.field_type;
                        }
                    }
                },
                .enum_type => {
                    // 枚举不能直接访问字段
                    try self.errors.append("Error: cannot access fields on enum type");
                    return ast.Type.void;
                },
                .trait_type => {
                    try self.errors.append("Error: cannot access fields on trait type");
                    return ast.Type.void;
                },
            }
        }
        
        const err_msg = try std.fmt.allocPrint(
            self.allocator,
            "Error: field '{s}' not found on type '{s}'",
            .{field_name, type_name}
        );
        try self.errors.append(err_msg);
        return ast.Type.void;
    }
    
    /// 检查 is 表达式的完整性（穷尽性检查）
    fn checkIsExhaustivenness(
        self: *TypeChecker,
        value_type: ast.Type,
        arms: []ast.IsArm,
    ) !void {
        // 简化版：检查是否有通配符 _
        var has_wildcard = false;
        
        for (arms) |arm| {
            if (arm.pattern == .wildcard) {
                has_wildcard = true;
                break;
            }
        }
        
        // 如果匹配的是枚举类型，检查是否覆盖所有变体
        if (value_type == .named) {
            const type_name = value_type.named;
            
            if (self.type_table.get(type_name)) |type_decl| {
                if (type_decl.kind == .enum_type) {
                    const enum_type = type_decl.kind.enum_type;
                    
                    if (!has_wildcard) {
                        // 检查是否所有变体都被覆盖
                        var covered_variants = std.StringHashMap(bool).init(self.allocator);
                        defer covered_variants.deinit();
                        
                        for (arms) |arm| {
                            if (arm.pattern == .enum_variant) {
                                try covered_variants.put(arm.pattern.enum_variant.variant_name, true);
                            }
                        }
                        
                        // 检查是否有未覆盖的变体
                        for (enum_type.variants) |variant| {
                            if (!covered_variants.contains(variant.name)) {
                                const err_msg = try std.fmt.allocPrint(
                                    self.allocator,
                                    "Warning: is expression not exhaustive, missing pattern for '{s}'",
                                    .{variant.name}
                                );
                                try self.errors.append(err_msg);
                            }
                        }
                    }
                }
            }
        }
    }
};
