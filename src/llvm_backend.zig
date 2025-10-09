//! LLVM Backend for PawLang
//! 
//! This module generates LLVM IR from the Paw AST.
//! 
//! Status: v0.1.4 - Experimental/Prototype
//! 
//! Roadmap:
//!   v0.1.4: Basic functions, arithmetic, simple expressions
//!   v0.2.0: Control flow, structs, generics
//!   v0.3.0: Full feature parity with C backend

const std = @import("std");
const ast = @import("ast.zig");

/// LLVM Backend Code Generator
pub const LLVMBackend = struct {
    allocator: std.mem.Allocator,
    output: std.ArrayList(u8),
    
    // Symbol tables
    variables: std.StringHashMap([]const u8),
    functions: std.StringHashMap([]const u8),
    
    // IR generation state
    indent_level: u32 = 0,
    next_temp: u32 = 0,
    
    pub fn init(allocator: std.mem.Allocator) LLVMBackend {
        return LLVMBackend{
            .allocator = allocator,
            .output = std.ArrayList(u8).init(allocator),
            .variables = std.StringHashMap([]const u8).init(allocator),
            .functions = std.StringHashMap([]const u8).init(allocator),
        };
    }
    
    pub fn deinit(self: *LLVMBackend) void {
        self.output.deinit();
        self.variables.deinit();
        self.functions.deinit();
    }
    
    /// Generate LLVM IR from AST
    pub fn generate(self: *LLVMBackend, program: ast.Program) ![]const u8 {
        // Emit LLVM IR header
        try self.output.appendSlice("; PawLang v0.1.4 - LLVM IR Output\n");
        try self.output.appendSlice("; Target: x86_64 (experimental)\n\n");
        
        // Process all declarations
        for (program.declarations) |decl| {
            try self.generateDecl(decl);
        }
        
        return self.output.items;
    }
    
    fn generateDecl(self: *LLVMBackend, decl: ast.TopLevelDecl) !void {
        switch (decl) {
            .function => |func| try self.generateFunction(func),
            .type_decl => {
                // v0.1.4: 暂不支持类型声明
                try self.output.appendSlice("; TODO: type declaration\n");
            },
            .import_decl => {
                // v0.1.4: 暂不支持导入
                try self.output.appendSlice("; TODO: import\n");
            },
            .struct_decl => {
                // v0.1.4: 暂不支持结构体声明
                try self.output.appendSlice("; TODO: struct declaration\n");
            },
            .enum_decl => {
                // v0.1.4: 暂不支持枚举声明
                try self.output.appendSlice("; TODO: enum declaration\n");
            },
            .trait_decl => {
                // v0.1.4: 暂不支持trait声明
                try self.output.appendSlice("; TODO: trait declaration\n");
            },
            .impl_decl => {
                // v0.1.4: 暂不支持impl声明
                try self.output.appendSlice("; TODO: impl declaration\n");
            },
        }
    }
    
    fn generateFunction(self: *LLVMBackend, func: ast.FunctionDecl) !void {
        // Reset temp counter for each function
        self.next_temp = 1;
        
        // Function signature
        try self.output.appendSlice("\ndefine ");
        try self.output.appendSlice(try self.toLLVMType(func.return_type));
        try self.output.appendSlice(" @");
        try self.output.appendSlice(func.name);
        try self.output.appendSlice("(");
        
        // Parameters
        for (func.params, 0..) |param, i| {
            if (i > 0) try self.output.appendSlice(", ");
            try self.output.appendSlice(try self.toLLVMType(param.type));
            try self.output.appendSlice(" %");
            try self.output.appendSlice(param.name);
        }
        
        try self.output.appendSlice(") {\n");
        
        // Entry block
        try self.output.appendSlice("entry:\n");
        
        // Function body
        for (func.body) |stmt| {
            try self.generateStmt(stmt);
        }
        
        try self.output.appendSlice("}\n");
    }
    
    fn generateStmt(self: *LLVMBackend, stmt: ast.Stmt) !void {
        switch (stmt) {
            .return_stmt => |maybe_val| {
                try self.output.appendSlice("  ret ");
                if (maybe_val) |val| {
                    const temp = try self.generateExpr(val);
                    try self.output.appendSlice(try self.getExprType(val));
                    try self.output.appendSlice(" ");
                    try self.output.appendSlice(temp);
                } else {
                    try self.output.appendSlice("void");
                }
                try self.output.appendSlice("\n");
            },
            .let_decl => |let_stmt| {
                // Allocate local variable
                try self.output.appendSlice("  %");
                try self.output.appendSlice(let_stmt.name);
                try self.output.appendSlice(" = alloca ");
                
                // Get type from initializer
                if (let_stmt.init) |init_expr| {
                    try self.output.appendSlice(try self.getExprType(init_expr));
                    try self.output.appendSlice("\n");
                    
                    // Generate initializer and store
                    const init_temp = try self.generateExpr(init_expr);
                    try self.output.appendSlice("  store ");
                    try self.output.appendSlice(try self.getExprType(init_expr));
                    try self.output.appendSlice(" ");
                    try self.output.appendSlice(init_temp);
                    try self.output.appendSlice(", ptr %");
                    try self.output.appendSlice(let_stmt.name);
                    try self.output.appendSlice("\n");
                } else {
                    try self.output.appendSlice("i32\n"); // default
                }
                
                try self.variables.put(let_stmt.name, let_stmt.name);
            },
            .expr => |expr| {
                _ = try self.generateExpr(expr);
            },
            else => {
                try self.output.appendSlice("  ; TODO: unsupported statement\n");
            },
        }
    }
    
    fn generateExpr(self: *LLVMBackend, expr: ast.Expr) ![]const u8 {
        return switch (expr) {
            .int_literal => |val| {
                return try std.fmt.allocPrint(self.allocator, "{d}", .{val});
            },
            .float_literal => |val| {
                return try std.fmt.allocPrint(self.allocator, "{d}", .{val});
            },
            .identifier => |name| {
                // Load variable
                const temp = try self.nextTemp();
                try self.output.appendSlice("  ");
                try self.output.appendSlice(temp);
                try self.output.appendSlice(" = load i32, ptr %");
                try self.output.appendSlice(name);
                try self.output.appendSlice("\n");
                return temp;
            },
            .binary => |binop| {
                const lhs = try self.generateExpr(binop.left.*);
                const rhs = try self.generateExpr(binop.right.*);
                const temp = try self.nextTemp();
                
                try self.output.appendSlice("  ");
                try self.output.appendSlice(temp);
                try self.output.appendSlice(" = ");
                
                switch (binop.op) {
                    .add => try self.output.appendSlice("add"),
                    .sub => try self.output.appendSlice("sub"),
                    .mul => try self.output.appendSlice("mul"),
                    .div => try self.output.appendSlice("sdiv"),
                    else => try self.output.appendSlice("add"), // fallback
                }
                
                try self.output.appendSlice(" i32 ");
                try self.output.appendSlice(lhs);
                try self.output.appendSlice(", ");
                try self.output.appendSlice(rhs);
                try self.output.appendSlice("\n");
                
                return temp;
            },
            .call => |call_expr| {
                // v0.1.4: 简化处理，假设callee是identifier
                const temp = try self.nextTemp();
                try self.output.appendSlice("  ");
                try self.output.appendSlice(temp);
                try self.output.appendSlice(" = call i32 @");
                
                // 获取函数名
                if (call_expr.callee.* == .identifier) {
                    try self.output.appendSlice(call_expr.callee.identifier);
                } else {
                    try self.output.appendSlice("unknown");
                }
                
                try self.output.appendSlice("(");
                
                for (call_expr.args, 0..) |arg, i| {
                    if (i > 0) try self.output.appendSlice(", ");
                    const arg_temp = try self.generateExpr(arg);
                    try self.output.appendSlice("i32 ");
                    try self.output.appendSlice(arg_temp);
                }
                
                try self.output.appendSlice(")\n");
                return temp;
            },
            else => {
                return try std.fmt.allocPrint(self.allocator, "0", .{});
            },
        };
    }
    
    fn getExprType(self: *LLVMBackend, expr: ast.Expr) ![]const u8 {
        _ = self;
        return switch (expr) {
            .int_literal => "i32",
            .float_literal => "double",
            .identifier => "i32", // TODO: lookup actual type
            .binary => "i32",
            .call => "i32",
            else => "i32",
        };
    }
    
    fn nextTemp(self: *LLVMBackend) ![]const u8 {
        const temp = try std.fmt.allocPrint(self.allocator, "%{d}", .{self.next_temp});
        self.next_temp += 1;
        return temp;
    }
    
    fn toLLVMType(self: *LLVMBackend, ty: ast.Type) ![]const u8 {
        _ = self;
        return switch (ty) {
            .i32 => "i32",
            .i64 => "i64",
            .f32 => "float",
            .f64 => "double",
            .bool => "i1",
            .void => "void",
            .string => "ptr",
            else => "i32", // fallback
        };
    }
};

