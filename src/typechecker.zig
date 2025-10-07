const std = @import("std");
const ast = @import("ast.zig");

pub const TypeChecker = struct {
    allocator: std.mem.Allocator,
    errors: std.ArrayList([]const u8),
    symbol_table: std.StringHashMap(ast.Type),
    function_table: std.StringHashMap(ast.FunctionDecl),

    pub fn init(allocator: std.mem.Allocator) TypeChecker {
        return TypeChecker{
            .allocator = allocator,
            .errors = .{},
            .symbol_table = std.StringHashMap(ast.Type).init(allocator),
            .function_table = std.StringHashMap(ast.FunctionDecl).init(allocator),
        };
    }

    pub fn deinit(self: *TypeChecker) void {
        self.errors.deinit(self.allocator);
        self.symbol_table.deinit();
        self.function_table.deinit();
    }

    pub fn check(self: *TypeChecker, program: ast.Program) !void {
        for (program.declarations) |decl| {
            switch (decl) {
                .function => |func| {
                    try self.function_table.put(func.name, func);
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

        for (program.declarations) |decl| {
            try self.checkDecl(decl);
        }

        if (!self.function_table.contains("main")) {
            try self.errors.append(self.allocator, "Error: missing main function");
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
            else => {},
        }
    }

    fn checkFunction(self: *TypeChecker, func: ast.FunctionDecl) !void {
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
            .let_decl => |let| {
                if (let.init) |init_expr| {
                    const init_type = try self.checkExpr(init_expr, scope);
                    
                    if (let.type) |declared_type| {
                        if (!init_type.eql(declared_type)) {
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

    fn checkExpr(self: *TypeChecker, expr: ast.Expr, scope: *std.StringHashMap(ast.Type)) (std.mem.Allocator.Error || error{TypeCheckFailed})!ast.Type {
        return switch (expr) {
            .int_literal => ast.Type.int,
            .float_literal => ast.Type.double,
            .string_literal => ast.Type.string,
            .char_literal => ast.Type.char,
            .bool_literal => ast.Type.bool,
            .identifier => |name| blk: {
                if (scope.get(name)) |var_type| {
                    break :blk var_type;
                } else if (self.symbol_table.get(name)) |sym_type| {
                    break :blk sym_type;
                } else {
                    try self.errors.append(self.allocator, "Error: undefined identifier");
                    break :blk ast.Type.void;
                }
            },
            .binary => |bin| blk: {
                const left_type = try self.checkExpr(bin.left.*, scope);
                const right_type = try self.checkExpr(bin.right.*, scope);
                
                switch (bin.op) {
                    .add, .sub, .mul, .div, .mod => {
                        if (!left_type.eql(right_type)) {
                            try self.errors.append(self.allocator, "Type error: binary operator types must match");
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
                _ = call;
                break :blk ast.Type.int;
            },
            .field_access => |access| blk: {
                _ = access;
                break :blk ast.Type.int;
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
};
