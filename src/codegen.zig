const std = @import("std");
const ast = @import("ast.zig");

pub const CodeGen = struct {
    allocator: std.mem.Allocator,
    optimize: bool,
    output: std.ArrayList(u8),
    indent_level: usize,

    pub fn init(allocator: std.mem.Allocator, optimize: bool) CodeGen {
        return CodeGen{
            .allocator = allocator,
            .optimize = optimize,
            .output = .{},
            .indent_level = 0,
        };
    }

    pub fn deinit(self: *CodeGen) void {
        self.output.deinit(self.allocator);
    }

    pub fn generate(self: *CodeGen, program: ast.Program, output_name: []const u8) !void {
        // 生成 C 代码
        try self.writeLine("// 由 Paw 编译器生成");
        try self.writeLine("#include <stdio.h>");
        try self.writeLine("#include <stdlib.h>");
        try self.writeLine("#include <stdint.h>");
        try self.writeLine("#include <stdbool.h>");
        try self.writeLine("#include <string.h>");
        try self.writeLine("");

        // 类型定义
        try self.writeLine("typedef int8_t Byte;");
        try self.writeLine("typedef uint32_t Char;");
        try self.writeLine("typedef int32_t Int;");
        try self.writeLine("typedef int64_t Long;");
        try self.writeLine("typedef float Float;");
        try self.writeLine("typedef double Double;");
        try self.writeLine("typedef char* String;");
        try self.writeLine("");

        // 前向声明
        for (program.declarations) |decl| {
            switch (decl) {
                .function => |func| {
                    try self.generateFunctionDeclaration(func);
                },
                .struct_decl => |s| {
                    try self.generateStructDeclaration(s);
                },
                .enum_decl => |e| {
                    try self.generateEnumDeclaration(e);
                },
                else => {},
            }
        }

        try self.writeLine("");

        // 生成实现
        for (program.declarations) |decl| {
            switch (decl) {
                .function => |func| {
                    try self.generateFunction(func);
                },
                .impl_decl => |impl| {
                    for (impl.methods) |method| {
                        try self.generateFunction(method);
                    }
                },
                else => {},
            }
        }

        // 写入文件
        const c_filename = try std.fmt.allocPrint(self.allocator, "{s}.c", .{output_name});
        defer self.allocator.free(c_filename);

        const file = try std.fs.cwd().createFile(c_filename, .{});
        defer file.close();

        try file.writeAll(self.output.items);

        std.debug.print("生成的 C 代码: {s}\n", .{c_filename});

        // 调用 C 编译器
        try self.compileC(c_filename, output_name);
    }

    fn generateFunctionDeclaration(self: *CodeGen, func: ast.FunctionDecl) !void {
        const return_type = try self.typeToC(func.return_type);
        try self.write(return_type);
        try self.write(" ");
        try self.write(func.name);
        try self.write("(");

        for (func.params, 0..) |param, i| {
            if (i > 0) try self.write(", ");
            const param_type = try self.typeToC(param.type);
            try self.write(param_type);
            try self.write(" ");
            try self.write(param.name);
        }

        try self.writeLine(");");
    }

    fn generateStructDeclaration(self: *CodeGen, s: ast.StructDecl) !void {
        try self.write("typedef struct ");
        try self.write(s.name);
        try self.writeLine(" {");
        
        self.indent_level += 1;
        for (s.fields) |field| {
            try self.writeIndent();
            const field_type = try self.typeToC(field.type);
            try self.write(field_type);
            try self.write(" ");
            try self.write(field.name);
            try self.writeLine(";");
        }
        self.indent_level -= 1;
        
        try self.write("} ");
        try self.write(s.name);
        try self.writeLine(";");
        try self.writeLine("");
    }

    fn generateEnumDeclaration(self: *CodeGen, e: ast.EnumDecl) !void {
        // 为枚举生成标签枚举和联合体
        try self.write("typedef enum ");
        try self.write(e.name);
        try self.writeLine("_Tag {");
        
        self.indent_level += 1;
        for (e.variants, 0..) |variant, i| {
            try self.writeIndent();
            try self.write(e.name);
            try self.write("_");
            try self.write(variant.name);
            if (i < e.variants.len - 1) {
                try self.writeLine(",");
            } else {
                try self.writeLine("");
            }
        }
        self.indent_level -= 1;
        
        try self.write("} ");
        try self.write(e.name);
        try self.writeLine("_Tag;");
        try self.writeLine("");

        // 生成主枚举结构
        try self.write("typedef struct ");
        try self.write(e.name);
        try self.writeLine(" {");
        
        self.indent_level += 1;
        try self.writeIndent();
        try self.write(e.name);
        try self.writeLine("_Tag tag;");
        
        try self.writeIndent();
        try self.writeLine("union {");
        self.indent_level += 1;
        
        for (e.variants) |variant| {
            if (variant.fields.len > 0) {
                try self.writeIndent();
                try self.writeLine("struct {");
                self.indent_level += 1;
                
                for (variant.fields, 0..) |field_type, i| {
                    try self.writeIndent();
                    const c_type = try self.typeToC(field_type);
                    try self.write(c_type);
                    try self.write(" field");
                    const field_num = try std.fmt.allocPrint(self.allocator, "{d}", .{i});
                    defer self.allocator.free(field_num);
                    try self.write(field_num);
                    try self.writeLine(";");
                }
                
                self.indent_level -= 1;
                try self.writeIndent();
                try self.write("} ");
                try self.write(variant.name);
                try self.writeLine(";");
            }
        }
        
        self.indent_level -= 1;
        try self.writeIndent();
        try self.writeLine("} data;");
        
        self.indent_level -= 1;
        try self.write("} ");
        try self.write(e.name);
        try self.writeLine(";");
        try self.writeLine("");
    }

    fn generateFunction(self: *CodeGen, func: ast.FunctionDecl) !void {
        const return_type = try self.typeToC(func.return_type);
        try self.write(return_type);
        try self.write(" ");
        try self.write(func.name);
        try self.write("(");

        for (func.params, 0..) |param, i| {
            if (i > 0) try self.write(", ");
            const param_type = try self.typeToC(param.type);
            try self.write(param_type);
            try self.write(" ");
            try self.write(param.name);
        }

        try self.writeLine(") {");
        self.indent_level += 1;

        for (func.body) |stmt| {
            try self.generateStmt(stmt);
        }

        self.indent_level -= 1;
        try self.writeLine("}");
        try self.writeLine("");
    }

    fn generateStmt(self: *CodeGen, stmt: ast.Stmt) std.mem.Allocator.Error!void {
        switch (stmt) {
            .expr => |expr| {
                try self.writeIndent();
                try self.generateExpr(expr);
                try self.writeLine(";");
            },
            .let_decl => |let| {
                try self.writeIndent();
                
                if (let.type) |var_type| {
                    const c_type = try self.typeToC(var_type);
                    try self.write(c_type);
                } else {
                    try self.write("Int"); // 默认类型
                }
                
                try self.write(" ");
                try self.write(let.name);
                
                if (let.init) |init_expr| {
                    try self.write(" = ");
                    try self.generateExpr(init_expr);
                }
                
                try self.writeLine(";");
            },
            .return_stmt => |ret| {
                try self.writeIndent();
                try self.write("return");
                if (ret) |expr| {
                    try self.write(" ");
                    try self.generateExpr(expr);
                }
                try self.writeLine(";");
            },
            .break_stmt => {
                try self.writeIndent();
                try self.writeLine("break;");
            },
            .continue_stmt => {
                try self.writeIndent();
                try self.writeLine("continue;");
            },
            .while_loop => |loop| {
                try self.writeIndent();
                try self.write("while (");
                try self.generateExpr(loop.condition);
                try self.writeLine(") {");
                
                self.indent_level += 1;
                for (loop.body) |body_stmt| {
                    try self.generateStmt(body_stmt);
                }
                self.indent_level -= 1;
                
                try self.writeIndent();
                try self.writeLine("}");
            },
            .for_loop => |loop| {
                try self.writeIndent();
                try self.write("for (");
                
                if (loop.init) |init_stmt| {
                    // 需要特殊处理，因为 for 的 init 是语句
                    switch (init_stmt.*) {
                        .let_decl => |let| {
                            if (let.type) |var_type| {
                                const c_type = try self.typeToC(var_type);
                                try self.write(c_type);
                            } else {
                                try self.write("Int");
                            }
                            try self.write(" ");
                            try self.write(let.name);
                            if (let.init) |init_expr| {
                                try self.write(" = ");
                                try self.generateExpr(init_expr);
                            }
                        },
                        else => {},
                    }
                }
                try self.write("; ");
                
                if (loop.condition) |cond| {
                    try self.generateExpr(cond);
                }
                try self.write("; ");
                
                if (loop.step) |step| {
                    try self.generateExpr(step);
                }
                
                try self.writeLine(") {");
                
                self.indent_level += 1;
                for (loop.body) |body_stmt| {
                    try self.generateStmt(body_stmt);
                }
                self.indent_level -= 1;
                
                try self.writeIndent();
                try self.writeLine("}");
            },
        }
    }

    fn generateExpr(self: *CodeGen, expr: ast.Expr) std.mem.Allocator.Error!void {
        switch (expr) {
            .int_literal => |val| {
                const str = try std.fmt.allocPrint(self.allocator, "{d}", .{val});
                defer self.allocator.free(str);
                try self.write(str);
            },
            .float_literal => |val| {
                const str = try std.fmt.allocPrint(self.allocator, "{d}", .{val});
                defer self.allocator.free(str);
                try self.write(str);
            },
            .string_literal => |val| {
                try self.write("\"");
                try self.write(val);
                try self.write("\"");
            },
            .char_literal => |val| {
                const str = try std.fmt.allocPrint(self.allocator, "{d}", .{val});
                defer self.allocator.free(str);
                try self.write(str);
            },
            .bool_literal => |val| {
                if (val) {
                    try self.write("true");
                } else {
                    try self.write("false");
                }
            },
            .identifier => |name| {
                try self.write(name);
            },
            .binary => |bin| {
                try self.write("(");
                try self.generateExpr(bin.left.*);
                try self.write(" ");
                try self.write(switch (bin.op) {
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
                });
                try self.write(" ");
                try self.generateExpr(bin.right.*);
                try self.write(")");
            },
            .unary => |un| {
                try self.write(switch (un.op) {
                    .neg => "-",
                    .not => "!",
                });
                try self.write("(");
                try self.generateExpr(un.operand.*);
                try self.write(")");
            },
            .call => |call| {
                try self.generateExpr(call.callee.*);
                try self.write("(");
                for (call.args, 0..) |arg, i| {
                    if (i > 0) try self.write(", ");
                    try self.generateExpr(arg);
                }
                try self.write(")");
            },
            .field_access => |access| {
                try self.generateExpr(access.object.*);
                try self.write(".");
                try self.write(access.field);
            },
            .struct_init => |struct_init| {
                try self.write("(");
                try self.write(struct_init.type_name);
                try self.write("){");
                for (struct_init.fields, 0..) |field, i| {
                    if (i > 0) try self.write(", ");
                    try self.write(".");
                    try self.write(field.name);
                    try self.write(" = ");
                    try self.generateExpr(field.value);
                }
                try self.write("}");
            },
            .enum_variant => |variant| {
                try self.write("(");
                try self.write(variant.enum_name);
                try self.write("){.");
                try self.write("tag = ");
                try self.write(variant.enum_name);
                try self.write("_");
                try self.write(variant.variant);
                
                if (variant.args.len > 0) {
                    try self.write(", .data.");
                    try self.write(variant.variant);
                    try self.write(" = {");
                    for (variant.args, 0..) |arg, i| {
                        if (i > 0) try self.write(", ");
                        try self.generateExpr(arg);
                    }
                    try self.write("}");
                }
                
                try self.write("}");
            },
            .block => |stmts| {
                try self.write("({");
                self.indent_level += 1;
                try self.writeLine("");
                
                for (stmts) |stmt| {
                    try self.generateStmt(stmt);
                }
                
                self.indent_level -= 1;
                try self.writeIndent();
                try self.write("})");
            },
            .if_expr => |if_expr| {
                try self.write("(");
                try self.generateExpr(if_expr.condition.*);
                try self.write(" ? ");
                try self.generateExpr(if_expr.then_branch.*);
                try self.write(" : ");
                if (if_expr.else_branch) |else_branch| {
                    try self.generateExpr(else_branch.*);
                } else {
                    try self.write("0");
                }
                try self.write(")");
            },
            .match_expr => |match| {
                // 简化的 match 实现（使用 if-else 链）
                _ = match;
                try self.write("0"); // 占位符
            },
        }
    }

    fn typeToC(self: *CodeGen, paw_type: ast.Type) ![]const u8 {
        _ = self;
        return switch (paw_type) {
            .void => "void",
            .bool => "bool",
            .byte => "Byte",
            .char => "Char",
            .int => "Int",
            .long => "Long",
            .float => "Float",
            .double => "Double",
            .string => "String",
            .named => |name| name,
            .generic => |name| name,
            .generic_instance => |gi| gi.name,
            else => "void",
        };
    }

    fn compileC(self: *CodeGen, c_filename: []const u8, output_name: []const u8) !void {
        const builtin = @import("builtin");
        const exe_name = if (builtin.os.tag == .windows)
            try std.fmt.allocPrint(self.allocator, "{s}.exe", .{output_name})
        else
            output_name;
        defer if (builtin.os.tag == .windows) self.allocator.free(exe_name);

        // 使用 gcc 或 clang 编译
        var child = std.process.Child.init(
            &[_][]const u8{ "gcc", c_filename, "-o", exe_name, "-std=c11" },
            self.allocator,
        );

        _ = try child.spawnAndWait();
        
        std.debug.print("编译完成: {s}\n", .{exe_name});
    }

    fn write(self: *CodeGen, str: []const u8) !void {
        try self.output.appendSlice(self.allocator, str);
    }

    fn writeLine(self: *CodeGen, str: []const u8) !void {
        try self.output.appendSlice(self.allocator, str);
        try self.output.append(self.allocator, '\n');
    }

    fn writeIndent(self: *CodeGen) !void {
        var i: usize = 0;
        while (i < self.indent_level) : (i += 1) {
            try self.output.appendSlice(self.allocator, "    ");
        }
    }
};

