//! CodeGen - C Code Generator
//!
//! This module generates C code from the Paw AST.
//!
//! Structure:
//!   - CodeGen struct (lines 1-100)
//!   - Declaration generation (lines 100-400)
//!   - Statement generation (lines 400-600)
//!   - Expression generation (lines 600-900)
//!   - Helper functions (lines 900-1200)
//!
//! Features:
//!   - Rust-style enum to C tagged union
//!   - Method calls to function calls
//!   - String interpolation
//!   - Error propagation (?) operator
//!   - Pattern matching (is expression)

const std = @import("std");
const ast = @import("ast.zig");
const generics = @import("generics.zig");

// ============================================================================
// CodeGen Structure
// ============================================================================

/// C Code Generator
pub const CodeGen = struct {
    allocator: std.mem.Allocator,
    /// 🆕 Arena allocator for temporary strings (mangled names, etc.)
    arena: std.heap.ArenaAllocator,
    output: std.ArrayList(u8),
    // 🆕 类型表：变量名 -> 类型名
    var_types: std.StringHashMap([]const u8),
    // 🆕 类型定义表：类型名 -> TypeDecl
    type_decls: std.StringHashMap(ast.TypeDecl),
    // 🆕 enum variant表：variant名 -> enum类型名
    enum_variants: std.StringHashMap([]const u8),
    // 🆕 泛型上下文
    generic_context: generics.GenericContext,
    // 🆕 函数表：函数名 -> FunctionDecl（用于泛型实例化）
    function_table: std.StringHashMap(ast.FunctionDecl),
    // 🆕 当前方法上下文：用于生成方法体时的类型替换
    current_method_context: ?struct {
        struct_name: []const u8,      // 原始struct名 (Vec)
        mangled_name: []const u8,     // 单态化名 (Vec_i32)
        type_params: [][]const u8,    // 类型参数 ([T])
        type_args: []ast.Type,        // 具体类型 ([i32])
    },

    pub fn init(allocator: std.mem.Allocator) CodeGen {
        var output = std.ArrayList(u8).init(allocator);
        // 🚀 Performance: Pre-allocate 64KB buffer to reduce reallocations
        output.ensureTotalCapacity(64 * 1024) catch {};
        
        return CodeGen{
            .allocator = allocator,
            .arena = std.heap.ArenaAllocator.init(allocator),
            .output = output,
            .var_types = std.StringHashMap([]const u8).init(allocator),
            .type_decls = std.StringHashMap(ast.TypeDecl).init(allocator),
            .enum_variants = std.StringHashMap([]const u8).init(allocator),
            .generic_context = generics.GenericContext.init(allocator),
            .function_table = std.StringHashMap(ast.FunctionDecl).init(allocator),
            .current_method_context = null,
        };
    }

    pub fn deinit(self: *CodeGen) void {
        self.output.deinit();
        self.var_types.deinit();
        self.type_decls.deinit();
        self.enum_variants.deinit();
        self.generic_context.deinit();
        self.function_table.deinit();
        self.arena.deinit();  // 🆕 释放所有arena分配
    }
    
    pub fn generate(self: *CodeGen, program: ast.Program) ![]const u8 {
        // 🆕 第一遍：收集类型定义、函数和enum variants
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
            } else if (decl == .function) {
                // 🆕 收集函数定义（用于泛型实例化）
                try self.function_table.put(decl.function.name, decl.function);
            }
        }
        
        // 🆕 设置泛型上下文的函数表引用
        self.generic_context.function_table = &self.function_table;
        
        // 🆕 第二遍：收集所有泛型函数调用和泛型结构体实例
        try self.generic_context.collectGenericCalls(program);
        try self.collectGenericStructInstances(program);
        
        // 生成 C 代码头部
        try self.output.appendSlice("#include <stdio.h>\n");
        try self.output.appendSlice("#include <stdlib.h>\n");
        try self.output.appendSlice("#include <stdint.h>\n");
        try self.output.appendSlice("#include <stdbool.h>\n");
        try self.output.appendSlice("#include <string.h>\n");  // For string interpolation
        try self.output.appendSlice("\n");
        try self.output.appendSlice("// Generic function forward declarations\n");
        
        // 🆕 第三遍：生成单态化函数的前向声明和泛型结构体定义
        try self.generateMonomorphizedDeclarations();
        
        // 第四遍：生成所有声明
        for (program.declarations) |decl| {
            try self.generateDecl(decl);
            try self.output.appendSlice("\n");
        }
        
        // 🆕 第五遍：生成泛型实例化的函数实现
        try self.generateMonomorphizedFunctions();
        
        // 🔧 v0.1.4: Return a copy to avoid use-after-free
        return try self.allocator.dupe(u8, self.output.items);
    }
    
    // ============================================================================
    // Declaration Generation
    // ============================================================================
    
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
            try self.output.appendSlice(" arg");
            try self.output.writer().print("{d}", .{i});
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
                    try self.output.writer().print("_value.field{d} = arg{d};\n", .{i, i});
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
        // 🆕 跳过泛型函数（需要实例化后才能生成）
        if (func.type_params.len > 0) {
            // 泛型函数：跳过，等待实例化
            return;
        }
        
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
                // 🆕 如果是泛型结构体，跳过（等待单态化）
                if (type_decl.type_params.len > 0) {
                    return;
                }
                
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
                                    try self.output.appendSlice(" field");
                                    try self.output.writer().print("{d}", .{j});
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
    
    // ============================================================================
    // Statement Generation
    // ============================================================================
    
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
                        // 🆕 记录变量类型（包括generic_instance）
                        if (type_ == .named) {
                            type_name = type_.named;
                        } else if (type_ == .generic_instance) {
                            // Vec<i32> -> Vec_i32
                            const gi = type_.generic_instance;
                            var buf = std.ArrayList(u8).init(self.arena.allocator());
                            try buf.appendSlice(gi.name);
                            for (gi.type_args) |arg| {
                                try buf.appendSlice("_");
                                try buf.appendSlice(self.getSimpleTypeName(arg));
                            }
                            type_name = try buf.toOwnedSlice();
                        }
                    }
                } else if (let.init) |init_expr| {
                    // 从初始化表达式推断类型
                    if (init_expr == .array_literal and init_expr.array_literal.len > 0) {
                        is_array = true;
                        array_size = init_expr.array_literal.len;
                        try self.output.appendSlice("int32_t");
                    } else if (init_expr == .struct_init) {
                        // 🆕 检查是否是泛型结构体实例化
                        const si = init_expr.struct_init;
                        const actual_name = blk: {
                            if (self.type_decls.get(si.type_name)) |type_decl| {
                                if (type_decl.type_params.len > 0) {
                                    // 是泛型结构体，需要推导类型参数
                                    var type_args = std.ArrayList(ast.Type).init(self.allocator);
                                    defer type_args.deinit();
                                    
                                    // 🆕 从所有字段值推导类型（支持多类型参数）
                                    for (si.fields) |field| {
                                        const arg_type = self.inferExprType(field.value);
                                        try type_args.append(arg_type);
                                    }
                                    
                                    // 获取修饰后的名称
                                    const mangled = try self.generic_context.monomorphizer.recordStructInstance(
                                        si.type_name,
                                        try type_args.toOwnedSlice(),
                                    );
                                    break :blk mangled;
                                }
                            }
                            break :blk si.type_name;
                        };
                        
                        try self.output.appendSlice(actual_name);
                        type_name = actual_name;
                    } else if (init_expr == .string_literal) {
                        // 🆕 字符串字面量返回 char*
                        try self.output.appendSlice("char*");
                    } else if (init_expr == .string_interp) {
                        // 🆕 字符串插值返回 char*
                        try self.output.appendSlice("char*");
                    } else if (init_expr == .static_method_call) {
                        // 🆕 静态方法调用：Vec<i32>::new() → Vec_i32
                        const smc = init_expr.static_method_call;
                        if (smc.type_args.len > 0) {
                            // 构建mangled name
                            var buf = std.ArrayList(u8).init(self.arena.allocator());
                            try buf.appendSlice(smc.type_name);
                            for (smc.type_args) |arg| {
                                try buf.appendSlice("_");
                                try buf.appendSlice(self.getSimpleTypeName(arg));
                            }
                            const mangled = try buf.toOwnedSlice();
                            try self.output.appendSlice(mangled);
                            type_name = mangled;
                        } else {
                            try self.output.appendSlice(smc.type_name);
                            type_name = smc.type_name;
                        }
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
                        try self.output.writer().print("[{d}]", .{size});
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
                    } else if (std.mem.eql(u8, func_name, "println")) {
                        // 🆕 内置函数 println
                        try self.output.appendSlice("printf(\"%s\\n\", ");
                        if (call.args.len > 0) {
                            _ = try self.generateExpr(call.args[0]);
                        } else {
                            try self.output.appendSlice("\"\"");
                        }
                        try self.output.appendSlice(")");
                    } else if (std.mem.eql(u8, func_name, "print")) {
                        // 🆕 内置函数 print
                        try self.output.appendSlice("printf(\"%s\", ");
                        if (call.args.len > 0) {
                            _ = try self.generateExpr(call.args[0]);
                        } else {
                            try self.output.appendSlice("\"\"");
                        }
                        try self.output.appendSlice(")");
                    } else if (std.mem.eql(u8, func_name, "eprintln")) {
                        // 🆕 内置函数 eprintln
                        try self.output.appendSlice("fprintf(stderr, \"%s\\n\", ");
                        if (call.args.len > 0) {
                            _ = try self.generateExpr(call.args[0]);
                        } else {
                            try self.output.appendSlice("\"\"");
                        }
                        try self.output.appendSlice(")");
                    } else if (std.mem.eql(u8, func_name, "eprint")) {
                        // 🆕 内置函数 eprint
                        try self.output.appendSlice("fprintf(stderr, \"%s\", ");
                        if (call.args.len > 0) {
                            _ = try self.generateExpr(call.args[0]);
                        } else {
                            try self.output.appendSlice("\"\"");
                        }
                        try self.output.appendSlice(")");
                    } else {
                        // 普通函数调用（可能是泛型）
                        // 🆕 检查是否是泛型函数
                        const actual_func_name = blk: {
                            if (self.function_table.get(func_name)) |func| {
                                if (func.type_params.len > 0) {
                                    // 泛型函数：收集参数类型并获取修饰后的名称
                                    var arg_types = std.ArrayList(ast.Type).init(self.allocator);
                                    defer arg_types.deinit();
                                    
                                    // 🆕 从实际参数推导类型
                                    for (call.args) |arg| {
                                        const arg_type = self.inferExprType(arg);
                                        try arg_types.append(arg_type);
                                    }
                                    
                                    const mangled = self.generic_context.inferGenericInstance(
                                        func_name,
                                        try arg_types.toOwnedSlice(),
                                    ) catch func_name;
                                    
                                    break :blk mangled;
                                }
                            }
                            break :blk func_name;
                        };
                        
                        try self.output.appendSlice(actual_func_name);
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
            .static_method_call => |smc| {
                // 🆕 静态方法调用：Type<T>::method()
                // 生成修饰后的函数名：Type_T_method
                try self.output.appendSlice(smc.type_name);
                for (smc.type_args) |type_arg| {
                    try self.output.appendSlice("_");
                    // 🆕 使用简化名保持与mangling一致
                    try self.output.appendSlice(self.getSimpleTypeName(type_arg));
                }
                try self.output.appendSlice("_");
                try self.output.appendSlice(smc.method_name);
                try self.output.appendSlice("(");
                for (smc.args, 0..) |arg, i| {
                    if (i > 0) try self.output.appendSlice(", ");
                    _ = try self.generateExpr(arg);
                }
                try self.output.appendSlice(")");
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
                // 检查是否在方法上下文中，且是当前struct的初始化
                const actual_name = blk: {
                    // 🆕 优先检查方法上下文
                    if (self.current_method_context) |ctx| {
                        if (std.mem.eql(u8, si.type_name, ctx.struct_name)) {
                            // 在方法体中初始化当前struct，使用mangled名字
                            // 直接构建：struct_name + type_args
                            var buf = std.ArrayList(u8).init(self.arena.allocator());
                            buf.appendSlice(ctx.struct_name) catch break :blk si.type_name;
                            for (ctx.type_args) |arg| {
                                buf.appendSlice("_") catch break :blk si.type_name;
                                buf.appendSlice(self.getSimpleTypeName(arg)) catch break :blk si.type_name;
                            }
                            break :blk buf.toOwnedSlice() catch si.type_name;
                        }
                    }
                    
                    // 检查是否是泛型结构体实例化
                    if (self.type_decls.get(si.type_name)) |type_decl| {
                        if (type_decl.type_params.len > 0) {
                            // 是泛型结构体，需要推导类型参数
                            var type_args = std.ArrayList(ast.Type).init(self.allocator);
                            defer type_args.deinit();
                            
                            // 🆕 只从泛型类型参数对应的字段推导类型
                            for (type_decl.kind.struct_type.fields, 0..) |struct_field, idx| {
                                // 检查字段类型是否是泛型参数（T, U, A, B）
                                const is_generic_param = blk2: {
                                    if (struct_field.type == .generic) {
                                        break :blk2 true;
                                    } else if (struct_field.type == .named) {
                                        // 检查这个名称是否在 type_params 中
                                        for (type_decl.type_params) |param_name| {
                                            if (std.mem.eql(u8, param_name, struct_field.type.named)) {
                                                break :blk2 true;
                                            }
                                        }
                                    }
                                    break :blk2 false;
                                };
                                
                                if (is_generic_param) {
                                    // 这是一个泛型字段，从对应的初始化值推导类型
                                    if (idx < si.fields.len) {
                                        const arg_type = self.inferExprType(si.fields[idx].value);
                                        try type_args.append(arg_type);
                                    }
                                }
                            }
                            
                            // 记录泛型结构体实例化
                            const mangled = try self.generic_context.monomorphizer.recordStructInstance(
                                si.type_name,
                                try type_args.toOwnedSlice(),
                            );
                            break :blk mangled;
                        }
                    }
                    break :blk si.type_name;
                };
                
                try self.output.appendSlice("(");
                try self.output.appendSlice(actual_name);
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
            .block => |stmts| {
                // 🆕 实现 block 表达式
                // 简化实现：只返回最后一个表达式的值
                // 注意：不支持 block 中的变量声明（需要 GCC/Clang 扩展）
                if (stmts.len > 0) {
                    const last_stmt = stmts[stmts.len - 1];
                    if (last_stmt == .expr) {
                        // 返回最后一个表达式
                        try self.generateExpr(last_stmt.expr);
                    } else {
                        // 如果最后不是表达式，返回 0
                        try self.output.appendSlice("0");
                    }
                } else {
                    try self.output.appendSlice("0");
                }
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
            // 🆕 字符串插值
            .string_interp => |si| {
                try self.generateStringInterpolation(si.parts);
            },
            // 🆕 错误传播 (expr?)
            .try_expr => |inner| {
                try self.generateTryExpr(inner.*);
            },
            else => {
                // 其他表达式暂时生成 0
                try self.output.appendSlice("0");
            },
        }
    }
    
    // 🆕 生成错误传播代码
    // 策略：使用 statement expression 检查 Result，如果是 Err 则提前返回
    fn generateTryExpr(self: *CodeGen, inner: ast.Expr) (std.mem.Allocator.Error)!void {
        // 简化实现：假设 Result 类型是 enum
        // Result<T, E> 有两个 variant：Ok(T) 和 Err(E)
        
        try self.output.appendSlice("({\n");
        
        // 评估内部表达式并存储到临时变量（暂时硬编码为 Result）
        try self.output.appendSlice("    Result __try_result__ = ");
        try self.generateExpr(inner);
        try self.output.appendSlice(";\n");
        
        // 检查是否是 Err，如果是则提前返回
        try self.output.appendSlice("    if (__try_result__.tag == Result_TAG_Err) {\n");
        try self.output.appendSlice("        return __try_result__;\n");
        try self.output.appendSlice("    }\n");
        
        // 返回 Ok 中的值
        try self.output.appendSlice("    __try_result__.data.Ok_value;\n");
        try self.output.appendSlice("})");
    }
    
    // 🆕 生成字符串插值代码
    // 策略：使用 sprintf 拼接字符串
    fn generateStringInterpolation(self: *CodeGen, parts: []ast.StringInterpPart) (std.mem.Allocator.Error)!void {
        // 简化实现：生成立即执行的代码块，返回拼接后的字符串
        try self.output.appendSlice("({\n");
        try self.output.appendSlice("    static char __str_buf__[1024];\n");
        try self.output.appendSlice("    __str_buf__[0] = '\\0';\n");
        
        // 逐个拼接每个部分
        for (parts) |part| {
            switch (part) {
                .literal => |lit| {
                    if (lit.len > 0) {
                        try self.output.appendSlice("    strcat(__str_buf__, \"");
                        try self.output.appendSlice(lit);
                        try self.output.appendSlice("\");\n");
                    }
                },
                .expr => |expr| {
                    // 将表达式转换为字符串并拼接
                    try self.output.appendSlice("    {\n");
                    try self.output.appendSlice("        char __tmp__[64];\n");
                    try self.output.appendSlice("        sprintf(__tmp__, \"%d\", ");
                    try self.generateExpr(expr);
                    try self.output.appendSlice(");\n");
                    try self.output.appendSlice("        strcat(__str_buf__, __tmp__);\n");
                    try self.output.appendSlice("    }\n");
                },
            }
        }
        
        try self.output.appendSlice("    __str_buf__;\n");
        try self.output.appendSlice("})");
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
    
    
    // ============================================================================
    // Helper Functions
    // ============================================================================
    
    /// 获取类型的简化名（用于name mangling）
    fn getSimpleTypeName(self: *CodeGen, paw_type: ast.Type) []const u8 {
        _ = self;
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
            else => "unknown",
        };
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
            .generic => |name| name,  // 🆕 泛型类型：直接使用类型参数名（T, U, etc）
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
                // 🆕 处理泛型实例：Vec<i32> -> Vec_i32
                // 使用arena allocator，generate结束时自动释放
                var buf = std.ArrayList(u8).init(self.arena.allocator());
                buf.appendSlice(gi.name) catch return "void*";
                for (gi.type_args) |arg| {
                    buf.appendSlice("_") catch return "void*";
                    buf.appendSlice(self.getSimpleTypeName(arg)) catch return "void*";
                }
                return buf.toOwnedSlice() catch "void*";
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

    // ============================================================================
    // 🆕 类型推导辅助函数
    // ============================================================================
    
    /// 从表达式推导类型
    fn inferExprType(self: *CodeGen, expr: ast.Expr) ast.Type {
        return switch (expr) {
            .int_literal => ast.Type.i32,
            .float_literal => ast.Type.f64,
            .string_literal => ast.Type.string,
            .char_literal => ast.Type.char,
            .bool_literal => ast.Type.bool,
            .identifier => |name| blk: {
                // 查询变量类型
                if (self.var_types.get(name)) |type_name| {
                    break :blk ast.Type{ .named = type_name };
                }
                break :blk ast.Type.i32;  // 默认
            },
            .call => ast.Type.i32,  // 简化：函数调用返回 i32
            .binary => ast.Type.i32,
            .unary => |un| self.inferExprType(un.operand.*),
            .field_access => ast.Type.i32,
            .array_index => ast.Type.i32,
            .struct_init => |si| ast.Type{ .named = si.type_name },
            .array_literal => ast.Type.i32,  // 简化：数组返回 i32
            else => ast.Type.i32,
        };
    }

    // ============================================================================
    // 🆕 泛型单态化函数生成
    // ============================================================================

    /// 生成单态化函数的前向声明
    fn generateMonomorphizedDeclarations(self: *CodeGen) !void {
        // 🆕 1. 生成泛型结构体定义，并自动记录所有实例方法
        const struct_instances = self.generic_context.monomorphizer.struct_instances.items;
        for (struct_instances) |instance| {
            if (self.type_decls.get(instance.generic_name)) |type_decl| {
                if (type_decl.kind == .struct_type) {
                    const st = type_decl.kind.struct_type;
                    
                    // 🆕 自动记录该struct的所有实例方法
                    for (st.methods) |method| {
                        if (method.params.len > 0 and 
                            std.mem.eql(u8, method.params[0].name, "self")) {
                            // 这是实例方法
                            var method_type_args = std.ArrayList(ast.Type).init(self.allocator);
                            for (instance.type_args) |arg| {
                                try method_type_args.append(arg);
                            }
                            
                            _ = try self.generic_context.monomorphizer.recordMethodInstance(
                                instance.generic_name,
                                method.name,
                                try method_type_args.toOwnedSlice(),
                            );
                        }
                    }
                    
                    // 生成 typedef struct
                    try self.output.appendSlice("typedef struct ");
                    try self.output.appendSlice(instance.mangled_name);
                    try self.output.appendSlice(" ");
                    try self.output.appendSlice(instance.mangled_name);
                    try self.output.appendSlice(";\n\n");
                    
                    // 生成 struct 定义
                    try self.output.appendSlice("struct ");
                    try self.output.appendSlice(instance.mangled_name);
                    try self.output.appendSlice(" {\n");
                    
                    // 生成字段（用具体类型替换泛型类型参数）
                    for (st.fields) |field| {
                        try self.output.appendSlice("    ");
                        
                        // 🆕 替换泛型类型参数
                        // 检查字段类型是否是泛型参数（T, U, A, B, etc）
                        const field_type_to_use = blk: {
                            if (field.type == .named) {
                                // 检查这个名称是否是泛型参数
                                for (type_decl.type_params, 0..) |param_name, param_idx| {
                                    if (std.mem.eql(u8, param_name, field.type.named)) {
                                        // 是泛型参数，使用对应的具体类型
                                        if (param_idx < instance.type_args.len) {
                                            break :blk instance.type_args[param_idx];
                                        }
                                    }
                                }
                            } else if (field.type == .generic and instance.type_args.len > 0) {
                                // 直接是 .generic 类型
                                break :blk instance.type_args[0];
                            }
                            // 不是泛型参数，使用原始类型
                            break :blk field.type;
                        };
                        
                        try self.output.appendSlice(self.typeToC(field_type_to_use));
                        try self.output.appendSlice(" ");
                        try self.output.appendSlice(field.name);
                        try self.output.appendSlice(";\n");
                    }
                    
                    try self.output.appendSlice("};\n\n");
                }
            }
        }
        
        // 🆕 2. 生成泛型函数前向声明
        const instances = self.generic_context.monomorphizer.instances.items;
        
        for (instances) |instance| {
            if (self.function_table.get(instance.generic_name)) |generic_func| {
                if (generic_func.type_params.len > 0 and instance.type_args.len > 0) {
                    // 🆕 返回类型：使用第一个类型参数（简化）
                    const return_type = instance.type_args[0];
                    
                    // 生成前向声明
                    try self.output.appendSlice(self.typeToC(return_type));
                    try self.output.appendSlice(" ");
                    try self.output.appendSlice(instance.mangled_name);
                    try self.output.appendSlice("(");
                    
                    // 🆕 参数类型：使用对应的类型参数
                    for (generic_func.params, 0..) |param, i| {
                        if (i > 0) try self.output.appendSlice(", ");
                        
                        // 如果有足够的类型参数，使用对应的类型
                        const param_type = if (i < instance.type_args.len)
                            instance.type_args[i]
                        else
                            instance.type_args[0];  // 降级：重复使用第一个
                        
                        try self.output.appendSlice(self.typeToC(param_type));
                        try self.output.appendSlice(" ");
                        try self.output.appendSlice(param.name);
                    }
                    
                    try self.output.appendSlice(");\n");
                }
            }
        }
        try self.output.appendSlice("\n");
        
        // 🆕 3. 生成泛型方法前向声明
        const method_instances = self.generic_context.monomorphizer.method_instances.items;
        
        for (method_instances) |method_instance| {
            // 查找原始类型声明
            if (self.type_decls.get(method_instance.struct_name)) |type_decl| {
                if (type_decl.kind == .struct_type) {
                    const st = type_decl.kind.struct_type;
                    
                    // 查找对应的方法
                    for (st.methods) |method| {
                        if (std.mem.eql(u8, method.name, method_instance.method_name)) {
                            // 生成返回类型（替换泛型参数）
                            const return_type = try self.substituteGenericType(
                                method.return_type,
                                type_decl.type_params,
                                method_instance.type_args,
                            );
                            try self.output.appendSlice(self.typeToC(return_type));
                            try self.output.appendSlice(" ");
                            
                            // 生成方法名（mangled）
                            try self.output.appendSlice(method_instance.mangled_name);
                            try self.output.appendSlice("(");
                            
                            // 生成参数（替换泛型参数）
                            for (method.params, 0..) |param, i| {
                                if (i > 0) try self.output.appendSlice(", ");
                                
                                const param_type = try self.substituteGenericType(
                                    param.type,
                                    type_decl.type_params,
                                    method_instance.type_args,
                                );
                                
                                // 如果参数名是 self，转换为指针
                                if (std.mem.eql(u8, param.name, "self")) {
                                    try self.output.appendSlice(method_instance.struct_name);
                                    for (method_instance.type_args) |arg| {
                                        try self.output.appendSlice("_");
                                        try self.output.appendSlice(self.getSimpleTypeName(arg));
                                    }
                                    try self.output.appendSlice("* self");
                                } else {
                                    try self.output.appendSlice(self.typeToC(param_type));
                                    try self.output.appendSlice(" ");
                                    try self.output.appendSlice(param.name);
                                }
                            }
                            
                            try self.output.appendSlice(");\n");
                            break;
                        }
                    }
                }
            }
        }
        try self.output.appendSlice("\n");
    }

    /// 替换类型中的泛型参数
    fn substituteGenericType(
        self: *CodeGen,
        ty: ast.Type,
        type_params: [][]const u8,
        type_args: []ast.Type,
    ) !ast.Type {
        switch (ty) {
            .generic => |name| {
                // 查找对应的类型参数
                for (type_params, 0..) |param_name, idx| {
                    if (std.mem.eql(u8, name, param_name) and idx < type_args.len) {
                        return type_args[idx];
                    }
                }
                return ty;
            },
            .named => |name| {
                // 检查这个名称是否是泛型参数
                for (type_params, 0..) |param_name, idx| {
                    if (std.mem.eql(u8, name, param_name) and idx < type_args.len) {
                        return type_args[idx];
                    }
                }
                return ty;
            },
            .generic_instance => |gi| {
                // 🆕 处理泛型实例类型（Vec<T>）
                // 需要替换类型参数，生成具体的泛型实例
                // 使用arena allocator，generate结束时自动释放
                var new_type_args = std.ArrayList(ast.Type).init(self.arena.allocator());
                for (gi.type_args) |arg| {
                    const substituted = try self.substituteGenericType(arg, type_params, type_args);
                    try new_type_args.append(substituted);
                }
                return ast.Type{
                    .generic_instance = .{
                        .name = gi.name,
                        .type_args = try new_type_args.toOwnedSlice(),
                    },
                };
            },
            else => return ty,
        }
    }

    /// 生成所有单态化的泛型函数
    fn generateMonomorphizedFunctions(self: *CodeGen) !void {
        const instances = self.generic_context.monomorphizer.instances.items;
        
        for (instances) |instance| {
            // 获取原始泛型函数
            if (self.function_table.get(instance.generic_name)) |generic_func| {
                if (generic_func.type_params.len > 0 and instance.type_args.len > 0) {
                    // 🆕 返回类型：使用第一个类型参数
                    const return_type = instance.type_args[0];
                    
                    // 生成函数签名
                    try self.output.appendSlice(self.typeToC(return_type));
                    try self.output.appendSlice(" ");
                    try self.output.appendSlice(instance.mangled_name);
                    try self.output.appendSlice("(");
                    
                    // 🆕 生成参数：使用对应的类型参数
                    for (generic_func.params, 0..) |param, i| {
                        if (i > 0) try self.output.appendSlice(", ");
                        
                        // 如果有足够的类型参数，使用对应的类型
                        const param_type = if (i < instance.type_args.len)
                            instance.type_args[i]
                        else
                            instance.type_args[0];  // 降级：重复使用第一个
                        
                        try self.output.appendSlice(self.typeToC(param_type));
                        try self.output.appendSlice(" ");
                        try self.output.appendSlice(param.name);
                    }
                    
                    try self.output.appendSlice(") {\n");
                    
                    // 生成函数体
                    for (generic_func.body) |stmt| {
                        try self.generateStmt(stmt);
                    }
                    
                    try self.output.appendSlice("}\n\n");
                }
            }
        }
        
        // 🆕 生成泛型方法实现
        const method_instances = self.generic_context.monomorphizer.method_instances.items;
        
        for (method_instances) |method_instance| {
            // 查找原始类型声明
            if (self.type_decls.get(method_instance.struct_name)) |type_decl| {
                if (type_decl.kind == .struct_type) {
                    const st = type_decl.kind.struct_type;
                    
                    // 查找对应的方法
                    for (st.methods) |method| {
                        if (std.mem.eql(u8, method.name, method_instance.method_name)) {
                            // 生成返回类型（替换泛型参数）
                            const return_type = try self.substituteGenericType(
                                method.return_type,
                                type_decl.type_params,
                                method_instance.type_args,
                            );
                            try self.output.appendSlice(self.typeToC(return_type));
                            try self.output.appendSlice(" ");
                            
                            // 生成方法名（mangled）
                            try self.output.appendSlice(method_instance.mangled_name);
                            try self.output.appendSlice("(");
                            
                            // 生成参数（替换泛型参数）
                            for (method.params, 0..) |param, i| {
                                if (i > 0) try self.output.appendSlice(", ");
                                
                                const param_type = try self.substituteGenericType(
                                    param.type,
                                    type_decl.type_params,
                                    method_instance.type_args,
                                );
                                
                                // 如果参数名是 self，转换为指针
                                if (std.mem.eql(u8, param.name, "self")) {
                                    try self.output.appendSlice(method_instance.struct_name);
                                    for (method_instance.type_args) |arg| {
                                        try self.output.appendSlice("_");
                                        try self.output.appendSlice(self.getSimpleTypeName(arg));
                                    }
                                    try self.output.appendSlice("* self");
                                } else {
                                    try self.output.appendSlice(self.typeToC(param_type));
                                    try self.output.appendSlice(" ");
                                    try self.output.appendSlice(param.name);
                                }
                            }
                            
                            try self.output.appendSlice(") {\n");
                            
                            // 🆕 设置方法上下文，用于方法体生成时的类型替换
                            self.current_method_context = .{
                                .struct_name = method_instance.struct_name,
                                .mangled_name = method_instance.mangled_name,
                                .type_params = type_decl.type_params,
                                .type_args = method_instance.type_args,
                            };
                            
                            // 生成方法体
                            for (method.body) |stmt| {
                                try self.generateStmt(stmt);
                            }
                            
                            // 清除方法上下文
                            self.current_method_context = null;
                            
                            try self.output.appendSlice("}\n\n");
                            break;
                        }
                    }
                }
            }
        }
    }

    // ============================================================================
    // 🆕 收集泛型结构体实例
    // ============================================================================
    
    /// 收集所有泛型结构体实例化
    fn collectGenericStructInstances(self: *CodeGen, program: ast.Program) !void {
        for (program.declarations) |decl| {
            if (decl == .function) {
                // 遍历函数体中的语句
                try self.collectStructInstancesInStmts(decl.function.body);
            }
        }
    }
    
    fn collectStructInstancesInStmts(self: *CodeGen, stmts: []ast.Stmt) (std.mem.Allocator.Error)!void {
        for (stmts) |stmt| {
            try self.collectStructInstancesInStmt(stmt);
        }
    }
    
    fn collectStructInstancesInStmt(self: *CodeGen, stmt: ast.Stmt) (std.mem.Allocator.Error)!void {
        switch (stmt) {
            .let_decl => |let| {
                if (let.init) |init_expr| {
                    try self.collectStructInstancesInExpr(init_expr);
                }
            },
            .assign => |assign| {
                try self.collectStructInstancesInExpr(assign.value);
            },
            .compound_assign => |ca| {
                try self.collectStructInstancesInExpr(ca.value);
            },
            .return_stmt => |ret| {
                if (ret) |expr| {
                    try self.collectStructInstancesInExpr(expr);
                }
            },
            .expr => |expr| {
                try self.collectStructInstancesInExpr(expr);
            },
            .loop_stmt => |loop| {
                try self.collectStructInstancesInStmts(loop.body);
            },
            .while_loop => |while_loop| {
                try self.collectStructInstancesInStmts(while_loop.body);
            },
            .for_loop => |for_loop| {
                try self.collectStructInstancesInStmts(for_loop.body);
            },
            else => {},
        }
    }
    
    fn collectStructInstancesInExpr(self: *CodeGen, expr: ast.Expr) (std.mem.Allocator.Error)!void {
        switch (expr) {
            .struct_init => |si| {
                // 检查是否是泛型结构体实例化
                if (self.type_decls.get(si.type_name)) |type_decl| {
                    if (type_decl.type_params.len > 0) {
                        // 是泛型结构体，需要推导类型参数
                        var type_args = std.ArrayList(ast.Type).init(self.allocator);
                        defer type_args.deinit();
                        
                        // 🆕 只从泛型类型参数对应的字段推导类型
                        // 需要匹配 struct 定义中的字段类型
                        for (type_decl.kind.struct_type.fields, 0..) |struct_field, idx| {
                            // 检查字段类型是否是泛型参数（T, U, A, B）
                            const is_generic_param = blk: {
                                if (struct_field.type == .generic) {
                                    break :blk true;
                                } else if (struct_field.type == .named) {
                                    // 检查这个名称是否在 type_params 中
                                    for (type_decl.type_params) |param_name| {
                                        if (std.mem.eql(u8, param_name, struct_field.type.named)) {
                                            break :blk true;
                                        }
                                    }
                                }
                                break :blk false;
                            };
                            
                            if (is_generic_param) {
                                // 这是一个泛型字段，从对应的初始化值推导类型
                                if (idx < si.fields.len) {
                                    const arg_type = self.inferExprType(si.fields[idx].value);
                                    try type_args.append(arg_type);
                                }
                            }
                        }
                        
                        // 🆕 先保存type_args的副本（因为recordStructInstance会接管所有权）
                        var saved_type_args = std.ArrayList(ast.Type).init(self.allocator);
                        for (type_args.items) |arg| {
                            try saved_type_args.append(arg);
                        }
                        
                        // 记录泛型结构体实例化
                        _ = try self.generic_context.monomorphizer.recordStructInstance(
                            si.type_name,
                            try type_args.toOwnedSlice(),
                        );
                        
                        // 🆕 同时记录该struct的所有实例方法
                        if (type_decl.kind == .struct_type) {
                            const st = type_decl.kind.struct_type;
                            for (st.methods) |method| {
                                // 检查是否有self参数（实例方法）
                                if (method.params.len > 0 and 
                                    std.mem.eql(u8, method.params[0].name, "self")) {
                                    // 这是实例方法，记录它（使用相同的type_args）
                                    var method_type_args = std.ArrayList(ast.Type).init(self.allocator);
                                    for (saved_type_args.items) |arg| {
                                        try method_type_args.append(arg);
                                    }
                                    
                                    _ = try self.generic_context.monomorphizer.recordMethodInstance(
                                        si.type_name,
                                        method.name,
                                        try method_type_args.toOwnedSlice(),
                                    );
                                }
                            }
                        }
                    }
                }
                
                // 递归收集字段值中的实例
                for (si.fields) |field| {
                    try self.collectStructInstancesInExpr(field.value);
                }
            },
            .call => |call| {
                try self.collectStructInstancesInExpr(call.callee.*);
                for (call.args) |arg| {
                    try self.collectStructInstancesInExpr(arg);
                }
            },
            .binary => |bin| {
                try self.collectStructInstancesInExpr(bin.left.*);
                try self.collectStructInstancesInExpr(bin.right.*);
            },
            .unary => |un| {
                try self.collectStructInstancesInExpr(un.operand.*);
            },
            .field_access => |fa| {
                try self.collectStructInstancesInExpr(fa.object.*);
            },
            .array_index => |ai| {
                try self.collectStructInstancesInExpr(ai.array.*);
                try self.collectStructInstancesInExpr(ai.index.*);
            },
            .if_expr => |if_expr| {
                try self.collectStructInstancesInExpr(if_expr.condition.*);
                try self.collectStructInstancesInExpr(if_expr.then_branch.*);
                if (if_expr.else_branch) |else_branch| {
                    try self.collectStructInstancesInExpr(else_branch.*);
                }
            },
            .array_literal => |elements| {
                for (elements) |elem| {
                    try self.collectStructInstancesInExpr(elem);
                }
            },
            else => {},
        }
    }
};
