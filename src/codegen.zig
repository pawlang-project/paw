const std = @import("std");
const ast = @import("ast.zig");

/// C 代码生成器
pub const CodeGen = struct {
    allocator: std.mem.Allocator,
    output: std.ArrayList(u8),
    // 🆕 类型表：变量名 -> 类型名
    var_types: std.StringHashMap([]const u8),
    // 🆕 类型定义表：类型名 -> TypeDecl
    type_decls: std.StringHashMap(ast.TypeDecl),
    // 🆕 enum variant表：variant名 -> enum类型名
    enum_variants: std.StringHashMap([]const u8),

    pub fn init(allocator: std.mem.Allocator) CodeGen {
        return CodeGen{
            .allocator = allocator,
            .output = std.ArrayList(u8).init(allocator),
            .var_types = std.StringHashMap([]const u8).init(allocator),
            .type_decls = std.StringHashMap(ast.TypeDecl).init(allocator),
            .enum_variants = std.StringHashMap([]const u8).init(allocator),
        };
    }

    pub fn deinit(self: *CodeGen) void {
        self.output.deinit();
        self.var_types.deinit();
        self.type_decls.deinit();
        self.enum_variants.deinit();
    }
    
    pub fn generate(self: *CodeGen, program: ast.Program) ![]const u8 {
        // 🆕 第一遍：收集类型定义和enum variants
        for (program.declarations) |decl| {
            if (decl == .type_decl) {
                try self.type_decls.put(decl.type_decl.name, decl.type_decl);
                
                // 收集enum variants
                if (decl.type_decl.kind == .enum_type) {
                    const enum_type = decl.type_decl.kind.enum_type;
                    for (enum_type.variants) |variant| {
                        try self.enum_variants.put(variant.name, decl.type_decl.name);
                    }
                }
            }
        }
        
        // 生成 C 代码头部
        try self.output.appendSlice("#include <stdio.h>\n");
        try self.output.appendSlice("#include <stdlib.h>\n");
        try self.output.appendSlice("#include <stdint.h>\n");
        try self.output.appendSlice("#include <stdbool.h>\n");
        try self.output.appendSlice("\n");
        
        // 第二遍：生成所有声明
        for (program.declarations) |decl| {
            try self.generateDecl(decl);
            try self.output.appendSlice("\n");
        }
        
        return self.output.items;
    }
    
    fn generateDecl(self: *CodeGen, decl: ast.TopLevelDecl) !void {
            switch (decl) {
            .function => |func| try self.generateFunction(func),
            .type_decl => |type_decl| try self.generateTypeDecl(type_decl),
            .struct_decl => |struct_decl| try self.generateStructDecl(struct_decl),
            .enum_decl => |enum_decl| try self.generateEnumDecl(enum_decl),
            .import_decl => |import_decl| {
                // TODO: 处理导入
                _ = import_decl;
            },
            .trait_decl => |trait_decl| {
                // TODO: 处理 trait 声明
                _ = trait_decl;
            },
            .impl_decl => |impl_decl| {
                // TODO: 处理 impl 声明
                _ = impl_decl;
            },
        }
    }
    
    // 🆕 生成enum构造器函数
    fn generateEnumConstructor(self: *CodeGen, enum_name: []const u8, variant: ast.EnumVariant) !void {
        // 函数签名：EnumName EnumName_VariantName(args...)
        try self.output.appendSlice(enum_name);
        try self.output.appendSlice(" ");
        try self.output.appendSlice(enum_name);
        try self.output.appendSlice("_");
        try self.output.appendSlice(variant.name);
        try self.output.appendSlice("(");
        
        // 参数
        for (variant.fields, 0..) |vtype, i| {
            if (i > 0) try self.output.appendSlice(", ");
            try self.output.appendSlice(self.typeToC(vtype));
            const param_name = try std.fmt.allocPrint(self.allocator, " arg{d}", .{i});
            defer self.allocator.free(param_name);
            try self.output.appendSlice(param_name);
        }
        
        try self.output.appendSlice(") {\n");
        try self.output.appendSlice("    ");
        try self.output.appendSlice(enum_name);
        try self.output.appendSlice(" result;\n");
        try self.output.appendSlice("    result.tag = ");
        try self.output.appendSlice(enum_name);
        try self.output.appendSlice("_TAG_");
        try self.output.appendSlice(variant.name);
        try self.output.appendSlice(";\n");
        
        // 设置数据
        if (variant.fields.len > 0) {
            if (variant.fields.len == 1) {
                try self.output.appendSlice("    result.data.");
                try self.output.appendSlice(variant.name);
                try self.output.appendSlice("_value = arg0;\n");
            } else {
                for (0..variant.fields.len) |i| {
                    try self.output.appendSlice("    result.data.");
                    try self.output.appendSlice(variant.name);
                    const field_assign = try std.fmt.allocPrint(
                        self.allocator,
                        "_value.field{d} = arg{d};\n",
                        .{i, i}
                    );
                    defer self.allocator.free(field_assign);
                    try self.output.appendSlice(field_assign);
                }
            }
        }
        
        try self.output.appendSlice("    return result;\n");
        try self.output.appendSlice("}\n\n");
    }
    
    // 🆕 生成方法声明
    fn generateMethodDecl(self: *CodeGen, type_name: []const u8, method: ast.FunctionDecl) !void {
        // 返回类型
        try self.output.appendSlice(self.typeToC(method.return_type));
        try self.output.appendSlice(" ");
        
        // 方法名：TypeName_methodName
        try self.output.appendSlice(type_name);
        try self.output.appendSlice("_");
        try self.output.appendSlice(method.name);
        try self.output.appendSlice("(");
        
        // 参数：第一个参数是 self，转换为 TypeName* self
        for (method.params, 0..) |param, i| {
            if (i > 0) try self.output.appendSlice(", ");
            
            if (std.mem.eql(u8, param.name, "self")) {
                // self 参数转换为指针
                try self.output.appendSlice(type_name);
                try self.output.appendSlice("* ");
                try self.output.appendSlice(param.name);
            } else {
                try self.output.appendSlice(self.typeToC(param.type));
                try self.output.appendSlice(" ");
                try self.output.appendSlice(param.name);
            }
        }
        
        try self.output.appendSlice(");\n");
    }
    
    // 🆕 生成方法实现
    fn generateMethodImpl(self: *CodeGen, type_name: []const u8, method: ast.FunctionDecl) !void {
        // 返回类型
        try self.output.appendSlice(self.typeToC(method.return_type));
        try self.output.appendSlice(" ");
        
        // 方法名：TypeName_methodName
        try self.output.appendSlice(type_name);
        try self.output.appendSlice("_");
        try self.output.appendSlice(method.name);
        try self.output.appendSlice("(");
        
        // 参数
        for (method.params, 0..) |param, i| {
            if (i > 0) try self.output.appendSlice(", ");
            
            if (std.mem.eql(u8, param.name, "self")) {
                try self.output.appendSlice(type_name);
                try self.output.appendSlice("* ");
                try self.output.appendSlice(param.name);
            } else {
                try self.output.appendSlice(self.typeToC(param.type));
                try self.output.appendSlice(" ");
                try self.output.appendSlice(param.name);
            }
        }
        
        try self.output.appendSlice(") {\n");
        
        // 生成方法体
        for (method.body) |stmt| {
            try self.generateStmt(stmt);
        }
        
        try self.output.appendSlice("}\n\n");
    }

    fn generateFunction(self: *CodeGen, func: ast.FunctionDecl) !void {
        // 生成函数签名
        try self.output.appendSlice(self.typeToC(func.return_type));
        try self.output.appendSlice(" ");
        try self.output.appendSlice(func.name);
        try self.output.appendSlice("(");
        
        // 生成参数
        for (func.params, 0..) |param, i| {
            if (i > 0) try self.output.appendSlice(", ");
            try self.output.appendSlice(self.typeToC(param.type));
            try self.output.appendSlice(" ");
            try self.output.appendSlice(param.name);
        }
        
        try self.output.appendSlice(") {\n");
        
        // 生成函数体
        for (func.body) |stmt| {
            try self.generateStmt(stmt);
        }

        try self.output.appendSlice("}\n");
    }
    
    fn generateTypeDecl(self: *CodeGen, type_decl: ast.TypeDecl) !void {
        switch (type_decl.kind) {
            .struct_type => |st| {
                // 🆕 先声明 struct 类型
                try self.output.appendSlice("typedef struct ");
                try self.output.appendSlice(type_decl.name);
                try self.output.appendSlice(" ");
                try self.output.appendSlice(type_decl.name);
                try self.output.appendSlice(";\n\n");
                
                // 生成方法声明（在 struct 定义之前）
                for (st.methods) |method| {
                    try self.generateMethodDecl(type_decl.name, method);
                }
                
                // 生成 struct 定义
                try self.output.appendSlice("struct ");
                try self.output.appendSlice(type_decl.name);
                try self.output.appendSlice(" {\n");
                for (st.fields) |field| {
                    try self.output.appendSlice("    ");
                    try self.output.appendSlice(self.typeToC(field.type));
                    try self.output.appendSlice(" ");
                    try self.output.appendSlice(field.name);
                    try self.output.appendSlice(";\n");
                }
                try self.output.appendSlice("};\n\n");
                
                // 生成方法实现
                for (st.methods) |method| {
                    try self.generateMethodImpl(type_decl.name, method);
                }
            },
            .enum_type => |et| {
                // 🆕 Rust风格的enum需要用tagged union实现
                
                // 1. 生成Tag枚举（使用_TAG后缀避免冲突）
                try self.output.appendSlice("typedef enum {\n");
                for (et.variants) |variant| {
                    try self.output.appendSlice("    ");
                    try self.output.appendSlice(type_decl.name);
                    try self.output.appendSlice("_TAG_");
                    try self.output.appendSlice(variant.name);
                    try self.output.appendSlice(",\n");
                }
                try self.output.appendSlice("} ");
                try self.output.appendSlice(type_decl.name);
                try self.output.appendSlice("_Tag;\n\n");
                
                // 2. 如果有variant带参数，生成union
                var has_data = false;
                for (et.variants) |variant| {
                    if (variant.fields.len > 0) {
                        has_data = true;
                        break;
                    }
                }
                
                if (has_data) {
                    // 生成包含tag和data的struct
                    try self.output.appendSlice("typedef struct {\n");
                    try self.output.appendSlice("    ");
                    try self.output.appendSlice(type_decl.name);
                    try self.output.appendSlice("_Tag tag;\n");
                    try self.output.appendSlice("    union {\n");
                    
                    for (et.variants) |variant| {
                        if (variant.fields.len > 0) {
                            try self.output.appendSlice("        ");
                            if (variant.fields.len == 1) {
                                // 单个参数
                                try self.output.appendSlice(self.typeToC(variant.fields[0]));
                                try self.output.appendSlice(" ");
                                try self.output.appendSlice(variant.name);
                                try self.output.appendSlice("_value;\n");
                            } else {
                                // 多个参数，用struct
                                try self.output.appendSlice("struct { ");
                                for (variant.fields, 0..) |vtype, j| {
                                    if (j > 0) try self.output.appendSlice("; ");
                                    try self.output.appendSlice(self.typeToC(vtype));
                                    const field_name = try std.fmt.allocPrint(self.allocator, " field{d}", .{j});
                                    defer self.allocator.free(field_name);
                                    try self.output.appendSlice(field_name);
                                }
                                try self.output.appendSlice("; } ");
                                try self.output.appendSlice(variant.name);
                                try self.output.appendSlice("_value;\n");
                            }
                        }
                    }
                    
                    try self.output.appendSlice("    } data;\n");
                    try self.output.appendSlice("} ");
                    try self.output.appendSlice(type_decl.name);
                    try self.output.appendSlice(";\n\n");
                    
                    // 3. 生成构造器函数
                    for (et.variants) |variant| {
                        try self.generateEnumConstructor(type_decl.name, variant);
                    }
                } else {
                    // 简单enum（无数据），用typedef即可
                    try self.output.appendSlice("typedef ");
                    try self.output.appendSlice(type_decl.name);
                    try self.output.appendSlice("_Tag ");
                    try self.output.appendSlice(type_decl.name);
                    try self.output.appendSlice(";\n");
                }
            },
            .trait_type => {
                // C 中没有 trait 概念，跳过
            },
        }
    }
    
    fn generateStructDecl(self: *CodeGen, struct_decl: ast.StructDecl) !void {
        try self.output.appendSlice("typedef struct {\n");
        for (struct_decl.fields) |field| {
            try self.output.appendSlice("    ");
            try self.output.appendSlice(self.typeToC(field.type));
            try self.output.appendSlice(" ");
            try self.output.appendSlice(field.name);
            try self.output.appendSlice(";\n");
        }
        try self.output.appendSlice("} ");
        try self.output.appendSlice(struct_decl.name);
        try self.output.appendSlice(";\n");
    }
    
    fn generateEnumDecl(self: *CodeGen, enum_decl: ast.EnumDecl) !void {
        try self.output.appendSlice("typedef enum {\n");
        for (enum_decl.variants, 0..) |variant, i| {
            try self.output.appendSlice("    ");
            try self.output.appendSlice(variant.name);
            if (i < enum_decl.variants.len - 1) try self.output.appendSlice(",");
            try self.output.appendSlice("\n");
        }
        try self.output.appendSlice("} ");
        try self.output.appendSlice(enum_decl.name);
        try self.output.appendSlice(";\n");
    }
    
    fn generateStmt(self: *CodeGen, stmt: ast.Stmt) !void {
        switch (stmt) {
            .expr => |expr| {
                _ = try self.generateExpr(expr);
                try self.output.appendSlice(";\n");
            },
            // 🆕 赋值语句
            .assign => |assign| {
                _ = try self.generateExpr(assign.target);
                try self.output.appendSlice(" = ");
                _ = try self.generateExpr(assign.value);
                try self.output.appendSlice(";\n");
            },
            // 🆕 复合赋值语句
            .compound_assign => |ca| {
                _ = try self.generateExpr(ca.target);
                try self.output.appendSlice(" ");
                try self.output.appendSlice(self.compoundAssignOpToC(ca.op));
                try self.output.appendSlice(" ");
                _ = try self.generateExpr(ca.value);
                try self.output.appendSlice(";\n");
            },
            .return_stmt => |ret_expr| {
                try self.output.appendSlice("return ");
                if (ret_expr) |expr| {
                    _ = try self.generateExpr(expr);
                }
                try self.output.appendSlice(";\n");
            },
            .let_decl => |let| {
                var type_name: ?[]const u8 = null;
                var is_array = false;
                var array_size: ?usize = null;
                
                if (let.type) |type_| {
                    // 🆕 处理数组类型
                    if (type_ == .array) {
                        is_array = true;
                        array_size = type_.array.size;
                        // 生成数组元素类型
                        try self.output.appendSlice(self.typeToC(type_.array.element.*));
                    } else {
                        try self.output.appendSlice(self.typeToC(type_));
                        // 记录变量类型
                        if (type_ == .named) {
                            type_name = type_.named;
                        }
                    }
                } else if (let.init) |init_expr| {
                    // 从初始化表达式推断类型
                    if (init_expr == .array_literal and init_expr.array_literal.len > 0) {
                        is_array = true;
                        array_size = init_expr.array_literal.len;
                        try self.output.appendSlice("int32_t");
                    } else if (init_expr == .struct_init) {
                        try self.output.appendSlice(init_expr.struct_init.type_name);
                        type_name = init_expr.struct_init.type_name;
                    } else if (init_expr == .call and init_expr.call.callee.* == .identifier) {
                        // 🆕 检查是否是enum构造器调用
                        const callee_name = init_expr.call.callee.identifier;
                        if (self.enum_variants.get(callee_name)) |enum_name| {
                            // 是enum构造器，使用enum类型
                            try self.output.appendSlice(enum_name);
                            type_name = enum_name;
                        } else {
                            // 普通函数调用，默认int32_t
                            try self.output.appendSlice("int32_t");
                        }
                    } else {
                        try self.output.appendSlice("int32_t");
                    }
                } else {
                    try self.output.appendSlice("int32_t");
                }
                
                try self.output.appendSlice(" ");
                try self.output.appendSlice(let.name);
                
                // 🆕 数组需要添加大小
                if (is_array) {
                    // 优先使用初始化表达式的大小
                    var actual_size = array_size;
                    if (let.init) |init_expr| {
                        if (init_expr == .array_literal) {
                            actual_size = init_expr.array_literal.len;
                        }
                    }
                    
                    if (actual_size) |size| {
                        const size_str = try std.fmt.allocPrint(self.allocator, "[{d}]", .{size});
                        defer self.allocator.free(size_str);
                        try self.output.appendSlice(size_str);
                    } else {
                        // 动态大小数组，使用指针
                        try self.output.appendSlice("*");
                    }
                }
                
                if (let.init) |init_expr| {
                    try self.output.appendSlice(" = ");
                    _ = try self.generateExpr(init_expr);
                    
                    // 记录struct类型
                    if (init_expr == .struct_init) {
                        type_name = init_expr.struct_init.type_name;
                    }
                }
                try self.output.appendSlice(";\n");
                
                // 存储变量类型信息
                if (type_name) |tn| {
                    try self.var_types.put(let.name, tn);
                }
            },
            .loop_stmt => |loop| {
                if (loop.iterator) |iter| {
                    // 🆕 loop i in collection { }
                    try self.generateLoopIterator(iter, loop.body);
                } else if (loop.condition) |condition| {
                    // loop condition { }
                    try self.output.appendSlice("while (");
                    try self.generateExpr(condition);
                    try self.output.appendSlice(") {\n");
                    for (loop.body) |body_stmt| {
                        try self.generateStmt(body_stmt);
                    }
                    try self.output.appendSlice("}\n");
                } else {
                    // loop { }
                    try self.output.appendSlice("for (;;) {\n");
                    for (loop.body) |body_stmt| {
                        try self.generateStmt(body_stmt);
                    }
                    try self.output.appendSlice("}\n");
                }
            },
            .break_stmt => {
                try self.output.appendSlice("break;\n");
            },
            .continue_stmt => {
                try self.output.appendSlice("continue;\n");
            },
            .while_loop => |while_loop| {
                try self.output.appendSlice("while (");
                _ = try self.generateExpr(while_loop.condition);
                try self.output.appendSlice(") {\n");
                for (while_loop.body) |body_stmt| {
                    try self.generateStmt(body_stmt);
                }
                try self.output.appendSlice("}\n");
            },
            .for_loop => |for_loop| {
                try self.output.appendSlice("for (");
                if (for_loop.init) |init_stmt| {
                    try self.generateStmt(init_stmt.*);
                }
                try self.output.appendSlice("; ");
                if (for_loop.condition) |condition| {
                    _ = try self.generateExpr(condition);
                }
                try self.output.appendSlice("; ");
                if (for_loop.step) |step| {
                    _ = try self.generateExpr(step);
                }
                try self.output.appendSlice(") {\n");
                for (for_loop.body) |body_stmt| {
                    try self.generateStmt(body_stmt);
                }
                try self.output.appendSlice("}\n");
            },
        }
    }

    fn generateExpr(self: *CodeGen, expr: ast.Expr) !void {
        switch (expr) {
            .int_literal => |i| {
                const str = try std.fmt.allocPrint(self.allocator, "{d}", .{i});
                defer self.allocator.free(str);
                try self.output.appendSlice(str);
            },
            .float_literal => |f| {
                const str = try std.fmt.allocPrint(self.allocator, "{d}", .{f});
                defer self.allocator.free(str);
                try self.output.appendSlice(str);
            },
            .string_literal => |s| {
                try self.output.appendSlice("\"");
                try self.output.appendSlice(s);
                try self.output.appendSlice("\"");
            },
            .char_literal => |c| {
                const str = try std.fmt.allocPrint(self.allocator, "'{c}'", .{@as(u8, @intCast(c))});
                defer self.allocator.free(str);
                try self.output.appendSlice(str);
            },
            .bool_literal => |b| try self.output.appendSlice(if (b) "true" else "false"),
            .identifier => |id| try self.output.appendSlice(id),
            .binary => |bin| {
                try self.output.appendSlice("(");
                _ = try self.generateExpr(bin.left.*);
                try self.output.appendSlice(" ");
                try self.output.appendSlice(self.binaryOpToC(bin.op));
                try self.output.appendSlice(" ");
                _ = try self.generateExpr(bin.right.*);
                try self.output.appendSlice(")");
            },
            .unary => |un| {
                try self.output.appendSlice("(");
                try self.output.appendSlice(self.unaryOpToC(un.op));
                _ = try self.generateExpr(un.operand.*);
                try self.output.appendSlice(")");
            },
            .call => |call| {
                // 🆕 检查是否是方法调用 (obj.method 形式)
                if (call.callee.* == .field_access) {
                    const field = call.callee.field_access;
                    
                    // 尝试从变量类型表中查找对象的类型
                    if (field.object.* == .identifier) {
                        const var_name = field.object.identifier;
                        if (self.var_types.get(var_name)) |type_name| {
                            // 找到类型，生成 TypeName_method(&obj, args...)
                            try self.output.appendSlice(type_name);
                            try self.output.appendSlice("_");
                            try self.output.appendSlice(field.field);
                            try self.output.appendSlice("(&");
                            try self.output.appendSlice(var_name);
                            for (call.args) |arg| {
                                try self.output.appendSlice(", ");
                                _ = try self.generateExpr(arg);
                            }
                            try self.output.appendSlice(")");
                            return;
                        }
                    }
                    
                    // 如果找不到类型，降级为普通调用
                    _ = try self.generateExpr(field.object.*);
                    try self.output.appendSlice(".");
                    try self.output.appendSlice(field.field);
                    try self.output.appendSlice("(");
                    for (call.args, 0..) |arg, i| {
                        if (i > 0) try self.output.appendSlice(", ");
                        _ = try self.generateExpr(arg);
                    }
                    try self.output.appendSlice(")");
                } else if (call.callee.* == .identifier) {
                    // 🆕 检查是否是enum构造器
                    const func_name = call.callee.identifier;
                    
                    // 从enum_variants表中查找
                    if (self.enum_variants.get(func_name)) |enum_name| {
                        // 是enum构造器，生成 EnumName_VariantName(args...)
                        try self.output.appendSlice(enum_name);
                        try self.output.appendSlice("_");
                        try self.output.appendSlice(func_name);
                        try self.output.appendSlice("(");
                        for (call.args, 0..) |arg, i| {
                            if (i > 0) try self.output.appendSlice(", ");
                            _ = try self.generateExpr(arg);
                        }
                        try self.output.appendSlice(")");
                    } else {
                        // 普通函数调用
                        try self.output.appendSlice(func_name);
                        try self.output.appendSlice("(");
                        for (call.args, 0..) |arg, i| {
                            if (i > 0) try self.output.appendSlice(", ");
                            _ = try self.generateExpr(arg);
                        }
                        try self.output.appendSlice(")");
                    }
                } else {
                    // 其他形式的调用
                    _ = try self.generateExpr(call.callee.*);
                    try self.output.appendSlice("(");
                    for (call.args, 0..) |arg, i| {
                        if (i > 0) try self.output.appendSlice(", ");
                        _ = try self.generateExpr(arg);
                    }
                    try self.output.appendSlice(")");
                }
            },
            .field_access => |field| {
                // 🆕 检查对象是否是 self（需要用 -> 而不是 .）
                const is_self = field.object.* == .identifier and 
                               std.mem.eql(u8, field.object.identifier, "self");
                
                _ = try self.generateExpr(field.object.*);
                
                if (is_self) {
                    try self.output.appendSlice("->");  // self 是指针
                } else {
                    try self.output.appendSlice(".");
                }
                try self.output.appendSlice(field.field);
            },
            .if_expr => |if_expr| {
                try self.output.appendSlice("(");
                _ = try self.generateExpr(if_expr.condition.*);
                try self.output.appendSlice(" ? ");
                _ = try self.generateExpr(if_expr.then_branch.*);
                try self.output.appendSlice(" : ");
                if (if_expr.else_branch) |else_branch| {
                    _ = try self.generateExpr(else_branch.*);
                } else {
                    try self.output.appendSlice("0");
                }
                try self.output.appendSlice(")");
            },
            .struct_init => |si| {
                // 🆕 生成 struct 初始化
                try self.output.appendSlice("(");
                try self.output.appendSlice(si.type_name);
                try self.output.appendSlice("){");
                for (si.fields, 0..) |field, i| {
                    if (i > 0) try self.output.appendSlice(", ");
                    try self.output.appendSlice(".");
                    try self.output.appendSlice(field.name);
                    try self.output.appendSlice(" = ");
                    _ = try self.generateExpr(field.value);
                }
                try self.output.appendSlice("}");
            },
            .enum_variant => |ev| {
                // 🆕 生成 enum 构造器
                try self.output.appendSlice(ev.variant);
                if (ev.args.len > 0) {
                    try self.output.appendSlice("(");
                    for (ev.args, 0..) |arg, i| {
                        if (i > 0) try self.output.appendSlice(", ");
                        _ = try self.generateExpr(arg);
                    }
                    try self.output.appendSlice(")");
                }
            },
            .array_literal => |elements| {
                // 🆕 生成数组字面量
                try self.output.appendSlice("{");
                for (elements, 0..) |elem, i| {
                    if (i > 0) try self.output.appendSlice(", ");
                    _ = try self.generateExpr(elem);
                }
                try self.output.appendSlice("}");
            },
            .array_index => |ai| {
                // 🆕 生成数组索引
                _ = try self.generateExpr(ai.array.*);
                try self.output.appendSlice("[");
                _ = try self.generateExpr(ai.index.*);
                try self.output.appendSlice("]");
            },
            .block => |block| {
                // TODO: 实现 block 表达式
                _ = block;
                try self.output.appendSlice("0");
            },
            // 🆕 is 表达式（模式匹配）
            .is_expr => |is_match| {
                try self.generateIsExpr(is_match);
            },
            // 🆕 范围表达式（通常不单独使用，在 loop 中会被特殊处理）
            .range => |r| {
                // 范围不能作为普通表达式使用
                // 只在 loop i in range 中有效
                _ = r;
                try self.output.appendSlice("/* range expression */");
            },
            else => {
                // 其他表达式暂时生成 0
                try self.output.appendSlice("0");
            },
        }
    }
    
    // 🆕 生成 loop iterator (loop i in collection)
    fn generateLoopIterator(self: *CodeGen, iter: ast.LoopIterator, body: []ast.Stmt) (std.mem.Allocator.Error)!void {
        // 检查 iterable 是否是范围表达式
        if (iter.iterable == .range) {
            const range = iter.iterable.range;
            
            // 生成 C 风格 for 循环
            try self.output.appendSlice("for (int32_t ");
            try self.output.appendSlice(iter.binding);
            try self.output.appendSlice(" = ");
            try self.generateExpr(range.start.*);
            try self.output.appendSlice("; ");
            try self.output.appendSlice(iter.binding);
            
            if (range.inclusive) {
                // ..= (包含结束)
                try self.output.appendSlice(" <= ");
            } else {
                // .. (不包含结束)
                try self.output.appendSlice(" < ");
            }
            
            try self.generateExpr(range.end.*);
            try self.output.appendSlice("; ");
            try self.output.appendSlice(iter.binding);
            try self.output.appendSlice("++) {\n");
            
            for (body) |stmt| {
                try self.generateStmt(stmt);
            }
            
            try self.output.appendSlice("}\n");
        } else if (iter.iterable == .array_literal) {
            // 🆕 数组字面量遍历：loop item in [1, 2, 3] { }
            // 策略：先声明临时数组，再遍历
            const array_lit = iter.iterable.array_literal;
            const idx_var = "__loop_idx__";
            const arr_var = "__loop_arr__";
            
            try self.output.appendSlice("{\n");
            
            // 声明临时数组
            try self.output.appendSlice("    int32_t ");
            try self.output.appendSlice(arr_var);
            const arr_size = try std.fmt.allocPrint(self.allocator, "[{d}]", .{array_lit.len});
            defer self.allocator.free(arr_size);
            try self.output.appendSlice(arr_size);
            try self.output.appendSlice(" = ");
            try self.generateExpr(iter.iterable);
            try self.output.appendSlice(";\n");
            
            // 生成 for 循环
            try self.output.appendSlice("    for (int32_t ");
            try self.output.appendSlice(idx_var);
            const loop_cond = try std.fmt.allocPrint(
                self.allocator, 
                " = 0; {s} < {d}; {s}++) {{\n", 
                .{idx_var, array_lit.len, idx_var}
            );
            defer self.allocator.free(loop_cond);
            try self.output.appendSlice(loop_cond);
            
            // 声明迭代变量
            try self.output.appendSlice("        int32_t ");
            try self.output.appendSlice(iter.binding);
            try self.output.appendSlice(" = ");
            try self.output.appendSlice(arr_var);
            try self.output.appendSlice("[");
            try self.output.appendSlice(idx_var);
            try self.output.appendSlice("];\n");
            
            // 生成循环体
            for (body) |stmt| {
                try self.output.appendSlice("        ");
                try self.generateStmt(stmt);
            }
            
            try self.output.appendSlice("    }\n");
            try self.output.appendSlice("}\n");
        } else if (iter.iterable == .identifier) {
            // 🆕 数组变量遍历：loop item in arr { }
            const idx_var = "__loop_idx__";
            const len_var = "__loop_len__";
            
            try self.output.appendSlice("{\n");
            
            // 计算数组长度
            try self.output.appendSlice("    int32_t ");
            try self.output.appendSlice(len_var);
            try self.output.appendSlice(" = sizeof(");
            try self.generateExpr(iter.iterable);
            try self.output.appendSlice(") / sizeof((");
            try self.generateExpr(iter.iterable);
            try self.output.appendSlice(")[0]);\n");
            
            // 生成 for 循环
            try self.output.appendSlice("    for (int32_t ");
            try self.output.appendSlice(idx_var);
            try self.output.appendSlice(" = 0; ");
            try self.output.appendSlice(idx_var);
            try self.output.appendSlice(" < ");
            try self.output.appendSlice(len_var);
            try self.output.appendSlice("; ");
            try self.output.appendSlice(idx_var);
            try self.output.appendSlice("++) {\n");
            
            // 声明迭代变量
            try self.output.appendSlice("        int32_t ");
            try self.output.appendSlice(iter.binding);
            try self.output.appendSlice(" = ");
            try self.generateExpr(iter.iterable);
            try self.output.appendSlice("[");
            try self.output.appendSlice(idx_var);
            try self.output.appendSlice("];\n");
            
            // 生成循环体
            for (body) |stmt| {
                try self.output.appendSlice("        ");
                try self.generateStmt(stmt);
            }
            
            try self.output.appendSlice("    }\n");
            try self.output.appendSlice("}\n");
        } else {
            // 其他类型的集合（TODO）
            try self.output.appendSlice("// TODO: unsupported iterator type\n");
            try self.output.appendSlice("for (;;) { break; }\n");
        }
    }
    
    // 🆕 生成 is 表达式（模式匹配）
    // 策略：使用立即执行的 block expression (GCC/Clang extension)
    // ({ int result; switch(...) { ... }; result; })
    fn generateIsExpr(self: *CodeGen, is_match: anytype) (std.mem.Allocator.Error)!void {
        // 开始一个立即执行的代码块（返回值）
        try self.output.appendSlice("({\n");
        
        // 生成临时变量来存储匹配的值
        try self.output.appendSlice("    typeof(");
        try self.generateExpr(is_match.value.*);
        try self.output.appendSlice(") __match_value__ = ");
        try self.generateExpr(is_match.value.*);
        try self.output.appendSlice(";\n");
        
        // 🆕 生成结果变量（简化：使用 int32_t 避免递归推断）
        try self.output.appendSlice("    int32_t __match_result__;\n");
        
        // 检查是否需要生成 switch（enum 模式）还是 if-else（其他模式）
        const use_switch = self.shouldUseSwitch(is_match);
        
        if (use_switch) {
            try self.generateIsExprSwitch(is_match);
        } else {
            try self.generateIsExprIfElse(is_match);
        }
        
        // 返回结果
        try self.output.appendSlice("    __match_result__;\n");
        try self.output.appendSlice("})");
    }
    
    // 判断是否应该使用 switch（enum 模式匹配）
    fn shouldUseSwitch(self: *CodeGen, is_match: anytype) bool {
        _ = self;
        // 简单策略：如果第一个 arm 是 variant 模式，使用 switch
        if (is_match.arms.len > 0) {
            return is_match.arms[0].pattern == .variant;
        }
        return false;
    }
    
    // 使用 switch 生成 is 表达式（enum 模式匹配）
    fn generateIsExprSwitch(self: *CodeGen, is_match: anytype) (std.mem.Allocator.Error)!void {
        try self.output.appendSlice("    switch (__match_value__.tag) {\n");
        
        for (is_match.arms) |arm| {
            if (arm.pattern == .variant) {
                const variant = arm.pattern.variant;
                
                // 需要找到enum类型名
                const enum_name = self.enum_variants.get(variant.name) orelse "Unknown";
                
                // case EnumName_TAG_VariantName:
                try self.output.appendSlice("        case ");
                try self.output.appendSlice(enum_name);
                try self.output.appendSlice("_TAG_");
                try self.output.appendSlice(variant.name);
                try self.output.appendSlice(": {\n");
                
                // 🆕 绑定变量（如果有）
                if (variant.bindings.len > 0) {
                    // 单个参数: Type binding = __match_value__.data.VariantName_value;
                    if (variant.bindings.len == 1) {
                        try self.output.appendSlice("            int32_t ");
                        try self.output.appendSlice(variant.bindings[0]);
                        try self.output.appendSlice(" = __match_value__.data.");
                        try self.output.appendSlice(variant.name);
                        try self.output.appendSlice("_value;\n");
                    } else {
                        // 多个参数: 从 struct 中提取
                        for (variant.bindings, 0..) |binding, i| {
                            try self.output.appendSlice("            int32_t ");
                            try self.output.appendSlice(binding);
                            try self.output.appendSlice(" = __match_value__.data.");
                            try self.output.appendSlice(variant.name);
                            const field_ref = try std.fmt.allocPrint(self.allocator, "_value.field{d};\n", .{i});
                            defer self.allocator.free(field_ref);
                            try self.output.appendSlice(field_ref);
                        }
                    }
                }
                
                // 生成 guard（如果有）
                if (arm.guard) |guard| {
                    try self.output.appendSlice("            if (");
                    try self.generateExpr(guard);
                    try self.output.appendSlice(") {\n");
                    try self.output.appendSlice("                __match_result__ = ");
                    try self.generateExpr(arm.body);
                    try self.output.appendSlice(";\n");
                    try self.output.appendSlice("            }\n");
                } else {
                    // 没有 guard，直接赋值
                    try self.output.appendSlice("            __match_result__ = ");
                    try self.generateExpr(arm.body);
                    try self.output.appendSlice(";\n");
                }
                
                try self.output.appendSlice("            break;\n");
                try self.output.appendSlice("        }\n");
            } else if (arm.pattern == .wildcard) {
                // default case
                try self.output.appendSlice("        default: {\n");
                try self.output.appendSlice("            __match_result__ = ");
                try self.generateExpr(arm.body);
                try self.output.appendSlice(";\n");
                try self.output.appendSlice("            break;\n");
                try self.output.appendSlice("        }\n");
            }
        }
        
        try self.output.appendSlice("    }\n");
    }
    
    // 使用 if-else 生成 is 表达式（常量/标识符模式）
    fn generateIsExprIfElse(self: *CodeGen, is_match: anytype) (std.mem.Allocator.Error)!void {
        for (is_match.arms, 0..) |arm, i| {
            // 确定前缀（是否需要 else）
            const needs_else = i > 0;
            
            if (arm.pattern == .wildcard) {
                // _ 通配符：总是匹配（作为最后的 else）
                if (needs_else) {
                    try self.output.appendSlice("    else {\n");
                } else {
                    try self.output.appendSlice("    {\n");
                }
                // 生成 body
                try self.output.appendSlice("        __match_result__ = ");
                try self.generateExpr(arm.body);
                try self.output.appendSlice(";\n");
                try self.output.appendSlice("    }\n");
            } else if (arm.pattern == .literal) {
                // 字面量模式：比较值
                if (needs_else) {
                    try self.output.appendSlice("    else if (__match_value__ == ");
                } else {
                    try self.output.appendSlice("    if (__match_value__ == ");
                }
                try self.generateExpr(arm.pattern.literal);
                try self.output.appendSlice(")");
                
                // guard
                if (arm.guard) |guard| {
                    try self.output.appendSlice(" && (");
                    try self.generateExpr(guard);
                    try self.output.appendSlice(")");
                }
                
                try self.output.appendSlice(" {\n");
                // 生成 body
                try self.output.appendSlice("        __match_result__ = ");
                try self.generateExpr(arm.body);
                try self.output.appendSlice(";\n");
                try self.output.appendSlice("    }\n");
            } else if (arm.pattern == .identifier) {
                // 标识符模式：绑定并总是匹配
                if (needs_else) {
                    try self.output.appendSlice("    else {\n");
                } else {
                    try self.output.appendSlice("    {\n");
                }
                try self.output.appendSlice("        int32_t ");
                try self.output.appendSlice(arm.pattern.identifier);
                try self.output.appendSlice(" = __match_value__;\n");
                
                // guard
                if (arm.guard) |guard| {
                    try self.output.appendSlice("        if (");
                    try self.generateExpr(guard);
                    try self.output.appendSlice(") {\n");
                    try self.output.appendSlice("            __match_result__ = ");
                    try self.generateExpr(arm.body);
                    try self.output.appendSlice(";\n");
                    try self.output.appendSlice("        }\n");
                } else {
                    // 没有 guard，直接赋值
                    try self.output.appendSlice("        __match_result__ = ");
                    try self.generateExpr(arm.body);
                    try self.output.appendSlice(";\n");
                }
                try self.output.appendSlice("    }\n");
            }
        }
    }
    
    
    fn typeToC(self: *CodeGen, paw_type: ast.Type) []const u8 {
        return switch (paw_type) {
            .i8 => "int8_t",
            .i16 => "int16_t", 
            .i32 => "int32_t",
            .i64 => "int64_t",
            .i128 => "int128_t",
            .u8 => "uint8_t",
            .u16 => "uint16_t",
            .u32 => "uint32_t", 
            .u64 => "uint64_t",
            .u128 => "uint128_t",
            .f32 => "float",
            .f64 => "double",
            .bool => "bool",
            .char => "char",
            .string => "char*",
            .void => "void",
            .generic => "void*",
            .named => |name| name,
            .pointer => |ptr| {
                // TODO: 处理指针类型
                _ = ptr;
                return "void*";
            },
            .array => |arr| {
                // 🆕 数组类型转换
                // [T] -> T* (动态数组，用指针)
                // [T; N] -> T[N] (固定大小数组)
                if (arr.size) |size| {
                    // 固定大小数组：需要返回 "Type[size]"
                    // 但这需要格式化字符串，暂时简化
                    _ = size;
                    return self.typeToC(arr.element.*);  // 简化：返回元素类型
                } else {
                    // 动态数组，用指针
                    return self.typeToC(arr.element.*);  // 简化：返回元素类型
                }
            },
            .function => |func| {
                // TODO: 处理函数类型
                _ = func;
                return "void*";
            },
            .generic_instance => |gi| {
                // TODO: 处理泛型实例
                _ = gi;
                return "void*";
            },
        };
    }
    
    fn binaryOpToC(self: *CodeGen, op: ast.BinaryOp) []const u8 {
        _ = self;
        return switch (op) {
                    .add => "+",
                    .sub => "-",
                    .mul => "*",
                    .div => "/",
                    .mod => "%",
                    .eq => "==",
                    .ne => "!=",
                    .lt => "<",
                    .le => "<=",
                    .gt => ">",
                    .ge => ">=",
                    .and_op => "&&",
                    .or_op => "||",
        };
    }
    
    fn unaryOpToC(self: *CodeGen, op: ast.UnaryOp) []const u8 {
        _ = self;
        return switch (op) {
            .neg => "-",
            .not => "!",
        };
    }
    
    // 🆕 复合赋值操作符转换
    fn compoundAssignOpToC(self: *CodeGen, op: ast.CompoundAssignOp) []const u8 {
        _ = self;
        return switch (op) {
            .add_assign => "+=",
            .sub_assign => "-=",
            .mul_assign => "*=",
            .div_assign => "/=",
            .mod_assign => "%=",
        };
    }
};
