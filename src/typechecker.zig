const std = @import("std");
const ast = @import("ast.zig");
const generics = @import("generics.zig");
const diagnostic = @import("diagnostic.zig");  // 🆕 v0.1.8
const Diagnostic = diagnostic.Diagnostic;
const DiagnosticLevel = diagnostic.DiagnosticLevel;
const Span = diagnostic.Span;

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

const Token = @import("token.zig").Token;  // 🆕 v0.1.8

pub const TypeChecker = struct {
    allocator: std.mem.Allocator,
    arena: std.heap.ArenaAllocator,  // 🆕 Arena allocator for temporary types
    errors: std.ArrayList([]const u8),  // 保留旧的错误列表用于兼容
    diagnostics: std.ArrayList(Diagnostic),  // 🆕 v0.1.8: 新的诊断系统
    symbol_table: std.StringHashMap(ast.Type),
    function_table: std.StringHashMap(ast.FunctionDecl),
    type_table: std.StringHashMap(ast.TypeDecl),  // 存储 type 声明
    trait_table: std.StringHashMap(TraitDef),      // 新增：存储 trait 定义
    type_methods: std.StringHashMap(TypeMethods),  // 新增：存储类型的方法
    current_function_is_async: bool,  // 追踪当前函数是否异步
    generic_context: generics.GenericContext,  // 🆕 泛型上下文
    mutable_vars: std.StringHashMap(bool),  // 🆕 v0.1.6: 跟踪可变变量 (变量名 -> 是否可变)
    source_file: []const u8,  // 🆕 v0.1.8: 当前处理的源文件名
    tokens: []Token,  // 🆕 v0.1.8: Token 数组用于位置查找
    identifier_tokens: std.StringHashMap(Token),  // 🆕 v0.1.8: 标识符名 -> Token 映射

    pub fn init(allocator: std.mem.Allocator, source_file: []const u8, tokens: []Token) TypeChecker {
        return TypeChecker{
            .allocator = allocator,
            .arena = std.heap.ArenaAllocator.init(allocator),
            .errors = std.ArrayList([]const u8){},
            .diagnostics = std.ArrayList(Diagnostic){},  // 🆕 v0.1.8
            .symbol_table = std.StringHashMap(ast.Type).init(allocator),
            .function_table = std.StringHashMap(ast.FunctionDecl).init(allocator),
            .type_table = std.StringHashMap(ast.TypeDecl).init(allocator),
            .trait_table = std.StringHashMap(TraitDef).init(allocator),
            .type_methods = std.StringHashMap(TypeMethods).init(allocator),
            .current_function_is_async = false,
            .generic_context = generics.GenericContext.init(allocator),  // 🆕 初始化泛型上下文
            .mutable_vars = std.StringHashMap(bool).init(allocator),  // 🆕 v0.1.6: 初始化可变变量表
            .source_file = source_file,  // 🆕 v0.1.8
            .tokens = tokens,  // 🆕 v0.1.8
            .identifier_tokens = std.StringHashMap(Token).init(allocator),  // 🆕 v0.1.8
        };
    }

    pub fn deinit(self: *TypeChecker) void {
        // 🆕 v0.1.6: 释放错误消息内存
        for (self.errors.items) |error_msg| {
            self.allocator.free(error_msg);
        }
        self.errors.deinit(self.allocator);
        
        // 🆕 v0.1.8: 释放诊断消息内存
        for (self.diagnostics.items) |diag| {
            self.allocator.free(diag.message);
            if (diag.notes.len > 0) {
                for (diag.notes) |note| {
                    self.allocator.free(note);
                }
                self.allocator.free(diag.notes);
            }
            if (diag.help) |help| {
                self.allocator.free(help);
            }
        }
        self.diagnostics.deinit(self.allocator);
        self.identifier_tokens.deinit();
        
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
        
        // 🆕 清理泛型上下文
        self.generic_context.deinit();
        
        // 🆕 v0.1.6: 清理可变变量表
        self.mutable_vars.deinit();
        
        // 🆕 释放 arena（自动释放所有临时类型分配）
        self.arena.deinit();
    }

    pub fn check(self: *TypeChecker, program: ast.Program) !void {
        // 🆕 v0.1.8: 构建标识符 token 映射
        for (self.tokens) |token| {
            if (token.type == .identifier) {
                try self.identifier_tokens.put(token.lexeme, token);
            }
        }
        
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
            try self.errors.append(self.allocator, "Error: missing main function");
        }

        // 🆕 v0.1.8: 打印增强的诊断消息
        if (self.diagnostics.items.len > 0) {
            for (self.diagnostics.items) |diag| {
                try diag.print(self.allocator);
            }
            return error.TypeCheckFailed;
        }
        
        // 兼容：打印旧的简单错误
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
        
        // 🆕 v0.1.6: 清空可变变量表（每个函数有自己的作用域）
        self.mutable_vars.clearRetainingCapacity();
        
        var local_scope = std.StringHashMap(ast.Type).init(self.allocator);
        defer local_scope.deinit();

        // 🆕 如果是泛型函数，将类型参数添加到作用域
        for (func.type_params) |type_param| {
            try local_scope.put(type_param, ast.Type{ .generic = type_param });
        }

        // 🆕 v0.1.6: 记录函数参数的可变性
        for (func.params) |param| {
            try local_scope.put(param.name, param.type);
            try self.mutable_vars.put(param.name, param.is_mut);  // 使用参数的 is_mut
        }

        for (func.body) |stmt| {
            try self.checkStmt(stmt, &local_scope);
        }
    }

    // ============================================================================
    // Helper Functions
    // ============================================================================
    
    /// 🆕 v0.1.6: 检查表达式是否可变（用于赋值检查）
    fn checkMutability(self: *TypeChecker, expr: ast.Expr) !void {
        switch (expr) {
            .identifier => |name| {
                // 检查变量是否存在
                if (self.mutable_vars.get(name)) |is_mut| {
                    if (!is_mut) {
                        const error_msg = try std.fmt.allocPrint(
                            self.allocator,
                            "Error: Cannot assign to immutable variable '{s}'. Use 'let mut {s}' to make it mutable.",
                            .{name, name}
                        );
                        try self.errors.append(self.allocator, error_msg);
                    }
                } else {
                    // 变量不存在（这应该在其他地方被捕获）
                    const error_msg = try std.fmt.allocPrint(
                        self.allocator,
                        "Error: Variable '{s}' not found.",
                        .{name}
                    );
                    try self.errors.append(self.allocator, error_msg);
                }
            },
            .field_access => {
                // 字段访问：暂时允许（将来可以添加结构体字段可变性检查）
            },
            .array_index => {
                // 数组索引：暂时允许（将来可以添加数组可变性检查）
            },
            else => {
                try self.errors.append(self.allocator, "Error: Invalid assignment target.");
            },
        }
    }
    
    /// 创建子作用域（复制父作用域）
    fn createChildScope(self: *TypeChecker, parent: *std.StringHashMap(ast.Type)) !std.StringHashMap(ast.Type) {
        var child = std.StringHashMap(ast.Type).init(self.allocator);
        var iter = parent.iterator();
        while (iter.next()) |entry| {
            try child.put(entry.key_ptr.*, entry.value_ptr.*);
        }
        return child;
    }
    
    // ============================================================================
    // Statement Checking
    // ============================================================================
    
    fn checkStmt(self: *TypeChecker, stmt: ast.Stmt, scope: *std.StringHashMap(ast.Type)) (std.mem.Allocator.Error || error{TypeCheckFailed})!void {
        switch (stmt) {
            .expr => |expr| {
                _ = try self.checkExpr(expr, scope);
            },
            // 🆕 赋值语句
            .assign => |assign| {
                // 🆕 v0.1.6: 检查目标是否可变
                try self.checkMutability(assign.target);
                
                const target_type = try self.checkExpr(assign.target, scope);
                const value_type = try self.checkExpr(assign.value, scope);
                if (!target_type.eql(value_type)) {
                    try self.errors.append(self.allocator, "Type error: assignment type mismatch");
                }
            },
            // 🆕 复合赋值语句
            .compound_assign => |ca| {
                // 🆕 v0.1.6: 检查目标是否可变
                try self.checkMutability(ca.target);
                
                const target_type = try self.checkExpr(ca.target, scope);
                const value_type = try self.checkExpr(ca.value, scope);
                // 复合赋值要求类型匹配且支持相应运算
                if (!target_type.eql(value_type)) {
                    try self.errors.append(self.allocator, "Type error: compound assignment type mismatch");
                }
            },
            .let_decl => |let| {
                // 🆕 v0.1.6: 记录变量的可变性
                try self.mutable_vars.put(let.name, let.is_mut);
                
                if (let.init) |init_expr| {
                    const init_type = try self.checkExpr(init_expr, scope);
                    
                    if (let.type) |declared_type| {
                        // 🆕 改进类型兼容性检查
                        if (!self.isTypeCompatible(init_type, declared_type)) {
                            try self.errors.append(self.allocator, "Type error: variable type mismatch");
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
                        try self.errors.append(self.allocator, "Type error: loop condition must be Bool");
                    }
                }
                
                // 处理 loop for 迭代器
                if (loop.iterator) |iter| {
                    // 检查可迭代对象的类型
                    const iter_type = try self.checkExpr(iter.iterable, scope);
                    _ = iter_type;
                    
                    // 为循环变量创建新的作用域
                    var loop_scope = try self.createChildScope(scope);
                    defer loop_scope.deinit();
                    
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
                    try self.errors.append(self.allocator, "Type error: while condition must be Bool");
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
                        try self.errors.append(self.allocator, "Type error: for condition must be Bool");
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
        
        // 🆕 泛型类型兼容：任何类型都可以赋值给泛型类型参数
        if (to_type == .generic) return true;
        if (from_type == .generic) return true;
        
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
    
    /// 🆕 从函数调用推导泛型类型参数
    fn inferGenericTypes(
        self: *TypeChecker,
        func: ast.FunctionDecl,
        call_args: []ast.Expr,
        scope: *std.StringHashMap(ast.Type)
    ) ![]ast.Type {
        var type_map = std.StringHashMap(ast.Type).init(self.allocator);
        defer type_map.deinit();
        
        // 从每个参数推导类型
        for (func.params, call_args) |param, arg| {
            const arg_type = try self.checkExpr(arg, scope);
            
            if (param.type == .generic) {
                const type_param_name = param.type.generic;
                
                if (type_map.get(type_param_name)) |existing| {
                    // 类型参数已推导，检查一致性
                    if (!existing.eql(arg_type)) {
                        const err_msg = try std.fmt.allocPrint(
                            self.allocator,
                            "Error: Type parameter '{s}' cannot be both {s} and {s}",
                            .{type_param_name, @tagName(existing), @tagName(arg_type)}
                        );
                        try self.errors.append(self.allocator, err_msg);
                    }
                } else {
                    // 第一次推导此类型参数
                    try type_map.put(type_param_name, arg_type);
                }
            }
        }
        
        // 按顺序收集推导的类型
        var inferred_types = std.ArrayList(ast.Type){};
        for (func.type_params) |param_name| {
            if (type_map.get(param_name)) |inferred| {
                try inferred_types.append(self.allocator, inferred);
            } else {
                // 无法推导此类型参数，使用i32作为默认
                try inferred_types.append(self.allocator, ast.Type.i32);
            }
        }
        
        return inferred_types.toOwnedSlice(self.allocator);
    }
    
    /// 🆕 将泛型类型参数替换为具体类型
    fn substituteType(
        self: *TypeChecker,
        ty: ast.Type,
        type_params: [][]const u8,
        type_args: []ast.Type
    ) !ast.Type {
        _ = self;
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
            else => return ty,
        }
    }
    
    // ============================================================================
    // Expression Checking
    // ============================================================================
    
    fn checkExpr(self: *TypeChecker, expr: ast.Expr, scope: *std.StringHashMap(ast.Type)) (std.mem.Allocator.Error || error{TypeCheckFailed})!ast.Type {
        return switch (expr) {
            .int_literal => ast.Type.i32,      // 整数字面量默认 i32
            .float_literal => ast.Type.f64,    // 浮点字面量默认 f64
            .string_literal => ast.Type.string,
            .char_literal => ast.Type.char,
            .bool_literal => ast.Type.bool,
            .static_method_call => |smc| blk: {
                // 🆕 静态方法调用：Type<T>::method()
                // 简化：返回泛型实例类型或 i32
                if (smc.type_args.len > 0) {
                    break :blk ast.Type{ .generic_instance = .{
                        .name = smc.type_name,
                        .type_args = smc.type_args,
                    }};
                }
                break :blk ast.Type.i32;
            },
            .identifier => |name| blk: {
                if (scope.get(name)) |var_type| {
                    break :blk var_type;
                } else if (self.symbol_table.get(name)) |sym_type| {
                    break :blk sym_type;
                } else {
                    // 🆕 v0.1.8: Enhanced error message for undefined identifier
                    if (self.identifier_tokens.get(name)) |token| {
                        const error_msg = try std.fmt.allocPrint(
                            self.allocator,
                            "undefined variable '{s}'",
                            .{name}
                        );
                        const span = Span.fromPosition(token.filename, token.line, token.column);
                        const note_msg = try std.fmt.allocPrint(
                            self.allocator,
                            "variable '{s}' is not declared in this scope",
                            .{name}
                        );
                        const notes = try self.allocator.alloc([]const u8, 1);
                        notes[0] = note_msg;
                        
                        // 🆕 v0.1.8: Smart suggestion - find similar variable
                        var help: ?[]const u8 = null;
                        if (self.findSimilarVariable(name, scope)) |similar| {
                            help = try std.fmt.allocPrint(
                                self.allocator,
                                "did you mean '{s}'?",
                                .{similar}
                            );
                        }
                        
                        const diag = Diagnostic.init(.Error, error_msg, span, notes, help);
                        try self.diagnostics.append(self.allocator, diag);
                    } else {
                        try self.errors.append(self.allocator, "Error: undefined identifier");
                    }
                    break :blk ast.Type.void;
                }
            },
            .binary => |bin| blk: {
                const left_type = try self.checkExpr(bin.left.*, scope);
                const right_type = try self.checkExpr(bin.right.*, scope);
                
                switch (bin.op) {
                    .add, .sub, .mul, .div, .mod => {
                        if (!left_type.eql(right_type)) {
                            // 🆕 v0.1.8: Enhanced error for type mismatch
                            const error_msg = try std.fmt.allocPrint(
                                self.allocator,
                                "type mismatch: expected '{s}', found '{s}'",
                                .{self.typeToString(left_type), self.typeToString(right_type)}
                            );
                            const note_msg = try std.fmt.allocPrint(
                                self.allocator,
                                "binary operator requires both operands to have the same type",
                                .{}
                            );
                            const notes = try self.allocator.alloc([]const u8, 1);
                            notes[0] = note_msg;
                            const diag = Diagnostic.init(.Error, error_msg, null, notes, null);
                            try self.diagnostics.append(self.allocator, diag);
                        }
                        break :blk left_type;
                    },
                    .eq, .ne, .lt, .le, .gt, .ge => {
                        if (!left_type.eql(right_type)) {
                            try self.errors.append(self.allocator, "Type error: comparison types must match");
                        }
                        break :blk ast.Type.bool;
                    },
                    .and_op, .or_op => {
                        if (!left_type.eql(ast.Type.bool) or !right_type.eql(ast.Type.bool)) {
                            try self.errors.append(self.allocator, "Type error: logical ops require Bool");
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
                            try self.errors.append(self.allocator, "Type error: ! requires Bool");
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
                        // 🆕 检查参数数量
                        if (call.args.len != func.params.len) {
                            const err_msg = try std.fmt.allocPrint(
                                self.allocator,
                                "Error: Function '{s}' expects {d} arguments, but got {d}",
                                .{func_name, func.params.len, call.args.len}
                            );
                            try self.errors.append(self.allocator, err_msg);
                            break :blk ast.Type.void;
                        }
                        
                        if (func.type_params.len > 0) {
                            // 🆕 泛型函数：推导类型参数
                            const inferred_types = try self.inferGenericTypes(func, call.args, scope);
                            defer self.allocator.free(inferred_types);
                            
                            // 返回替换后的返回类型
                            const return_type = try self.substituteType(
                                func.return_type,
                                func.type_params,
                                inferred_types
                            );
                            break :blk return_type;
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
                                    try self.errors.append(self.allocator, err_msg);
                                }
                            }
                            
                            break :blk func.return_type;
                        }
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
                    try self.errors.append(self.allocator, "Type error: if condition must be Bool");
                }
                
                const then_type = try self.checkExpr(if_expr.then_branch.*, scope);
                
                if (if_expr.else_branch) |else_branch| {
                    const else_type = try self.checkExpr(else_branch.*, scope);
                    if (!then_type.eql(else_type)) {
                        try self.errors.append(self.allocator, "Type error: if-else branches must match");
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
                            try self.errors.append(self.allocator, "Type error: is guard must be Bool");
                        }
                    }
                    
                    // 检查分支体的类型
                    const arm_type = try self.checkExpr(arm.body, &arm_scope);
                    
                    if (result_type) |rt| {
                        if (!self.isTypeCompatible(arm_type, rt) and !self.isTypeCompatible(rt, arm_type)) {
                            // 使用更宽松的类型兼容性检查
                            // try self.errors.append(self.allocator, "Type error: is arms must have same type");
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
                    // try self.errors.append(self.allocator, "Warning: is expression may not be exhaustive");
                }
                
                break :blk result_type orelse ast.Type.void;
            },
            // 新增：as 表达式（类型转换）
            .as_expr => |as_cast| blk: {
                const from_type = try self.checkExpr(as_cast.value.*, scope);
                const to_type = as_cast.target_type;
                
                // 🆕 v0.1.7: 改进的类型转换验证
                const is_numeric_from = switch (from_type) {
                    .i8, .i16, .i32, .i64, .i128,
                    .u8, .u16, .u32, .u64, .u128,
                    .f32, .f64, .bool, .char => true,  // 🆕 包含 bool 和 char
                    else => false,
                };
                
                const is_numeric_to = switch (to_type) {
                    .i8, .i16, .i32, .i64, .i128,
                    .u8, .u16, .u32, .u64, .u128,
                    .f32, .f64, .bool, .char => true,  // 🆕 包含 bool 和 char
                    else => false,
                };
                
                if (!is_numeric_from or !is_numeric_to) {
                    // 只允许数值类型（包括 bool/char）之间转换
                    if (!from_type.eql(to_type)) {
                        try self.errors.append(self.allocator, "Type error: invalid type conversion");
                    }
                }
                
                break :blk as_cast.target_type;
            },
            // 新增：await 表达式
            .await_expr => |await_expr| blk: {
                // 验证 await 只能在 async 函数中使用
                if (!self.current_function_is_async) {
                    try self.errors.append(self.allocator, "Type error: await can only be used in async functions");
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
                        try self.errors.append(self.allocator, "Type error: array elements must have same type");
                    }
                }
                
                // 返回数组类型
                const elem_type_ptr = try self.arena.allocator().create(ast.Type);
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
                    try self.errors.append(self.allocator, "Type error: array index must be integer");
                }
                
                // 🆕 支持字符串索引：s[i] 返回 char
                if (array_type == .string) {
                    break :blk ast.Type.char;
                }
                
                // 返回数组元素类型
                if (array_type == .array) {
                    break :blk array_type.array.element.*;
                } else {
                    try self.errors.append(self.allocator, "Type error: index on non-array type");
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
                    try self.errors.append(self.allocator, "Type error: range bounds must be integers");
                }
                
                // 范围表达式的类型暂定为 void（实际上是迭代器）
                break :blk ast.Type.void;
            },
            // 🆕 字符串插值
            .string_interp => |si| blk: {
                // 检查所有表达式部分的类型
                for (si.parts) |part| {
                    if (part == .expr) {
                        _ = try self.checkExpr(part.expr, scope);
                    }
                }
                // 字符串插值的结果类型是 string
                break :blk ast.Type.string;
            },
            // 🆕 错误传播 (expr?)
            .try_expr => |inner| blk: {
                const inner_type = try self.checkExpr(inner.*, scope);
                
                // 检查是否是 Result 类型（简化：暂时不检查）
                // TODO: 完整实现需要检查 inner_type 是 Result<T, E>
                // 并返回 T，同时验证当前函数返回类型也是 Result
                
                // 暂时简化：返回 inner_type
                break :blk inner_type;
            },
            .match_expr => |match| blk: {
                _ = try self.checkExpr(match.value.*, scope);
                
                var result_type: ?ast.Type = null;
                for (match.arms) |arm| {
                    const arm_type = try self.checkExpr(arm.body, scope);
                    if (result_type) |rt| {
                        if (!rt.eql(arm_type)) {
                            try self.errors.append(self.allocator, "Type error: match arms must have same type");
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
                try self.errors.append(self.allocator, err_msg);
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
                        try self.errors.append(self.allocator, err_msg);
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
            try self.errors.append(self.allocator, err_msg);
            return false;
        };
        
        const type_methods = self.type_methods.get(type_name) orelse {
            const err_msg = try std.fmt.allocPrint(
                self.allocator,
                "Error: type '{s}' has no methods",
                .{type_name}
            );
            try self.errors.append(self.allocator, err_msg);
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
                    try self.errors.append(self.allocator, err_msg);
                    all_implemented = false;
                }
            } else {
                const err_msg = try std.fmt.allocPrint(
                    self.allocator,
                    "Error: missing trait method '{s}' in type '{s}' (required by trait '{s}')",
                    .{trait_method.name, type_name, trait_name}
                );
                try self.errors.append(self.allocator, err_msg);
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
                try self.errors.append(self.allocator, "Error: cannot call method on non-named type");
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
                    try self.errors.append(self.allocator, err_msg);
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
                            try self.errors.append(self.allocator, err_msg);
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
        try self.errors.append(self.allocator, err_msg);
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
                try self.errors.append(self.allocator, "Error: cannot access field on non-named type");
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
                    try self.errors.append(self.allocator, "Error: cannot access fields on enum type");
                    return ast.Type.void;
                },
                .trait_type => {
                    try self.errors.append(self.allocator, "Error: cannot access fields on trait type");
                    return ast.Type.void;
                },
            }
        }
        
        const err_msg = try std.fmt.allocPrint(
            self.allocator,
            "Error: field '{s}' not found on type '{s}'",
            .{field_name, type_name}
        );
        try self.errors.append(self.allocator, err_msg);
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
                                try self.errors.append(self.allocator, err_msg);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // ============================================================================
    // 🆕 v0.1.8: Enhanced Diagnostic Helpers
    // ============================================================================
    
    /// 将类型转换为字符串（用于错误消息）
    fn typeToString(_: *TypeChecker, t: ast.Type) []const u8 {
        return switch (t) {
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
            else => "unknown",
        };
    }
    
    /// 🆕 v0.1.8: 计算 Levenshtein 距离（用于相似变量名建议）
    fn levenshteinDistance(_: *TypeChecker, s1: []const u8, s2: []const u8) usize {
        if (s1.len == 0) return s2.len;
        if (s2.len == 0) return s1.len;
        
        var matrix: [100][100]usize = undefined;
        
        // 初始化第一行和第一列
        for (0..s1.len + 1) |i| {
            matrix[i][0] = i;
        }
        for (0..s2.len + 1) |j| {
            matrix[0][j] = j;
        }
        
        // 计算距离
        for (1..s1.len + 1) |i| {
            for (1..s2.len + 1) |j| {
                const cost: usize = if (s1[i - 1] == s2[j - 1]) 0 else 1;
                const deletion = matrix[i - 1][j] + 1;
                const insertion = matrix[i][j - 1] + 1;
                const substitution = matrix[i - 1][j - 1] + cost;
                
                matrix[i][j] = @min(@min(deletion, insertion), substitution);
            }
        }
        
        return matrix[s1.len][s2.len];
    }
    
    /// 🆕 v0.1.8: 查找相似的变量名建议
    fn findSimilarVariable(self: *TypeChecker, name: []const u8, scope: *std.StringHashMap(ast.Type)) ?[]const u8 {
        var best_match: ?[]const u8 = null;
        var best_distance: usize = 999;
        
        // 在局部作用域中查找
        var scope_iter = scope.iterator();
        while (scope_iter.next()) |entry| {
            const candidate = entry.key_ptr.*;
            const distance = self.levenshteinDistance(name, candidate);
            if (distance < best_distance and distance <= 2) {  // 最多 2 个字符差异
                best_distance = distance;
                best_match = candidate;
            }
        }
        
        // 在全局符号表中查找
        var symbol_iter = self.symbol_table.iterator();
        while (symbol_iter.next()) |entry| {
            const candidate = entry.key_ptr.*;
            const distance = self.levenshteinDistance(name, candidate);
            if (distance < best_distance and distance <= 2) {
                best_distance = distance;
                best_match = candidate;
            }
        }
        
        return best_match;
    }
    
    /// 报告一个简单错误（使用新的诊断系统）
    fn reportError(
        self: *TypeChecker,
        message: []const u8,
        line: usize,
        col: usize,
    ) !void {
        const span = Span.init(self.source_file, line, col, line, col);
        const diag = Diagnostic.init(.Error, message, span, &[_][]const u8{}, null);
        try self.diagnostics.append(self.allocator, diag);
    }
    
    /// 报告错误并附带帮助信息
    fn reportErrorWithHelp(
        self: *TypeChecker,
        message: []const u8,
        line: usize,
        col: usize,
        help: []const u8,
    ) !void {
        const span = Span.init(self.source_file, line, col, line, col);
        const diag = Diagnostic.init(.Error, message, span, &[_][]const u8{}, help);
        try self.diagnostics.append(self.allocator, diag);
    }
    
    /// 报告错误并附带注释和帮助
    fn reportErrorFull(
        self: *TypeChecker,
        message: []const u8,
        line: usize,
        col: usize,
        notes: []const []const u8,
        help: ?[]const u8,
    ) !void {
        const span = Span.init(self.source_file, line, col, line, col);
        const diag = Diagnostic.init(.Error, message, span, notes, help);
        try self.diagnostics.append(self.allocator, diag);
    }
};
