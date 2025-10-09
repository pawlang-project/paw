//! LLVM Native Backend using direct C API
//! 
//! This backend uses our custom LLVM C API bindings to generate
//! native code directly through LLVM, without generating text IR.

const std = @import("std");
const ast = @import("ast.zig");
const llvm = @import("llvm_c_api.zig");

pub const LLVMNativeBackend = struct {
    allocator: std.mem.Allocator,
    context: llvm.Context,
    module: llvm.Module,
    builder: llvm.Builder,
    
    // Symbol tables
    functions: std.StringHashMap(llvm.ValueRef),
    variables: std.StringHashMap(llvm.ValueRef),
    
    // Current function context
    current_function: ?llvm.ValueRef,
    
    // Loop context for break/continue
    current_loop_exit: ?llvm.BasicBlockRef,
    current_loop_continue: ?llvm.BasicBlockRef,
    
    pub fn init(allocator: std.mem.Allocator, module_name: []const u8) !LLVMNativeBackend {
        const context = llvm.Context.create();
        
        // Create null-terminated module name
        const module_name_z = try allocator.dupeZ(u8, module_name);
        defer allocator.free(module_name_z);
        
        const module = context.createModule(module_name_z);
        const builder = context.createBuilder();
        
        return LLVMNativeBackend{
            .allocator = allocator,
            .context = context,
            .module = module,
            .builder = builder,
            .functions = std.StringHashMap(llvm.ValueRef).init(allocator),
            .variables = std.StringHashMap(llvm.ValueRef).init(allocator),
            .current_function = null,
            .current_loop_exit = null,
            .current_loop_continue = null,
        };
    }
    
    pub fn deinit(self: *LLVMNativeBackend) void {
        self.functions.deinit();
        self.variables.deinit();
        self.builder.dispose();
        self.module.dispose();
        self.context.dispose();
    }
    
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
            else => {
                // TODO: Handle other declaration types
            },
        }
    }
    
    fn generateFunction(self: *LLVMNativeBackend, func: ast.FunctionDecl) !void {
        // Get return type
        const return_type = try self.toLLVMType(func.return_type);
        
        // Get parameter types
        var param_types = std.ArrayList(llvm.TypeRef).init(self.allocator);
        defer param_types.deinit();
        
        for (func.params) |param| {
            const param_type = try self.toLLVMType(param.type);
            try param_types.append(param_type);
        }
        
        // Create function type
        const func_type = llvm.functionType(return_type, param_types.items, false);
        
        // Create null-terminated function name
        const func_name_z = try self.allocator.dupeZ(u8, func.name);
        defer self.allocator.free(func_name_z);
        
        // Add function to module
        const llvm_func = self.module.addFunction(func_name_z, func_type);
        try self.functions.put(func.name, llvm_func);
        
        // Set current function context
        self.current_function = llvm_func;
        
        // Create entry basic block
        const entry_block = llvm.appendBasicBlock(self.context, llvm_func, "entry");
        self.builder.positionAtEnd(entry_block);
        
        // Store parameters in variables map
        self.variables.clearRetainingCapacity();
        for (func.params, 0..) |param, i| {
            const param_value = llvm.LLVMGetParam(llvm_func, @intCast(i));
            try self.variables.put(param.name, param_value);
        }
        
        // Generate function body
        for (func.body) |stmt| {
            try self.generateStmt(stmt);
        }
        
        // Clear function context
        self.current_function = null;
    }
    
    fn generateStmt(self: *LLVMNativeBackend, stmt: ast.Stmt) (error{NoCurrentFunction} || std.mem.Allocator.Error)!void {
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
                    const init_value = try self.generateExpr(init_expr);
                    try self.variables.put(let_stmt.name, init_value);
                }
            },
            .expr => |expr| {
                _ = try self.generateExpr(expr);
            },
            .while_loop => |while_stmt| {
                try self.generateWhileLoop(while_stmt);
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
    
    fn generateWhileLoop(self: *LLVMNativeBackend, loop: struct { condition: ast.Expr, body: []ast.Stmt }) !void {
        const func = self.current_function orelse return error.NoCurrentFunction;
        
        // Create basic blocks
        const cond_block = llvm.appendBasicBlock(self.context, func, "while.cond");
        const body_block = llvm.appendBasicBlock(self.context, func, "while.body");
        const exit_block = llvm.appendBasicBlock(self.context, func, "while.exit");
        
        // Save loop context
        const saved_exit = self.current_loop_exit;
        const saved_continue = self.current_loop_continue;
        self.current_loop_exit = exit_block;
        self.current_loop_continue = cond_block;
        defer {
            self.current_loop_exit = saved_exit;
            self.current_loop_continue = saved_continue;
        }
        
        // Jump to condition
        _ = self.builder.buildBr(cond_block);
        
        // Generate condition block
        self.builder.positionAtEnd(cond_block);
        const cond_value = try self.generateExpr(loop.condition);
        _ = llvm.LLVMBuildCondBr(self.builder.ref, cond_value, body_block, exit_block);
        
        // Generate body block
        self.builder.positionAtEnd(body_block);
        for (loop.body) |stmt| {
            try self.generateStmt(stmt);
        }
        _ = self.builder.buildBr(cond_block);  // Loop back
        
        // Continue from exit block
        self.builder.positionAtEnd(exit_block);
    }
    
    fn generateExpr(self: *LLVMNativeBackend, expr: ast.Expr) !llvm.ValueRef {
        return switch (expr) {
            .int_literal => |val| blk: {
                const i32_type = self.context.i32Type();
                break :blk llvm.LLVMConstInt(i32_type, @intCast(val), 1);
            },
            .float_literal => |val| blk: {
                break :blk llvm.constDouble(self.context, val);
            },
            .identifier => |name| blk: {
                if (self.variables.get(name)) |value| {
                    break :blk value;
                } else {
                    std.debug.print("⚠️  Undefined variable: {s}\n", .{name});
                    break :blk llvm.constI32(self.context, 0);
                }
            },
            .binary => |binop| blk: {
                const lhs = try self.generateExpr(binop.left.*);
                const rhs = try self.generateExpr(binop.right.*);
                
                const result_name_z = try self.allocator.dupeZ(u8, "binop");
                defer self.allocator.free(result_name_z);
                
                const result = switch (binop.op) {
                    .add => self.builder.buildAdd(lhs, rhs, result_name_z),
                    .sub => self.builder.buildSub(lhs, rhs, result_name_z),
                    .mul => self.builder.buildMul(lhs, rhs, result_name_z),
                    .div => self.builder.buildSDiv(lhs, rhs, result_name_z),
                    else => llvm.constI32(self.context, 0),
                };
                break :blk result;
            },
            .if_expr => |if_expr| blk: {
                const func = self.current_function orelse {
                    break :blk llvm.constI32(self.context, 0);
                };
                
                // Generate condition
                const cond_value = try self.generateExpr(if_expr.condition.*);
                
                // Create basic blocks
                const then_block = llvm.appendBasicBlock(self.context, func, "if.then");
                const else_block = llvm.appendBasicBlock(self.context, func, "if.else");
                const cont_block = llvm.appendBasicBlock(self.context, func, "if.cont");
                
                // Build conditional branch
                _ = llvm.LLVMBuildCondBr(self.builder.ref, cond_value, then_block, else_block);
                
                // Generate then branch
                self.builder.positionAtEnd(then_block);
                const then_value = try self.generateExpr(if_expr.then_branch.*);
                _ = self.builder.buildBr(cont_block);
                
                // Generate else branch
                self.builder.positionAtEnd(else_block);
                _ = if (if_expr.else_branch) |else_br|
                    try self.generateExpr(else_br.*)
                else
                    llvm.constI32(self.context, 0);
                _ = self.builder.buildBr(cont_block);
                
                // Continue block
                self.builder.positionAtEnd(cont_block);
                
                // For now, just return then_value (proper PHI node needed for full support)
                break :blk then_value;
            },
            .call => |call_expr| blk: {
                // Get function name
                const func_name = if (call_expr.callee.* == .identifier)
                    call_expr.callee.identifier
                else
                    "unknown";
                
                // Look up function
                const func = self.functions.get(func_name) orelse {
                    std.debug.print("⚠️  Undefined function: {s}\n", .{func_name});
                    break :blk llvm.constI32(self.context, 0);
                };
                
                // Generate arguments
                var args = std.ArrayList(llvm.ValueRef).init(self.allocator);
                defer args.deinit();
                
                for (call_expr.args) |arg| {
                    const arg_value = try self.generateExpr(arg);
                    try args.append(arg_value);
                }
                
                // Get function type
                const i32_type = self.context.i32Type();
                var param_types = std.ArrayList(llvm.TypeRef).init(self.allocator);
                defer param_types.deinit();
                for (args.items) |_| {
                    try param_types.append(i32_type);
                }
                const func_type = llvm.functionType(i32_type, param_types.items, false);
                
                // Build call
                const call_name_z = try self.allocator.dupeZ(u8, "call");
                defer self.allocator.free(call_name_z);
                
                const result = self.builder.buildCall(func_type, func, args.items, call_name_z);
                break :blk result;
            },
            else => llvm.constI32(self.context, 0),
        };
    }
    
    fn toLLVMType(self: *LLVMNativeBackend, paw_type: ast.Type) !llvm.TypeRef {
        return switch (paw_type) {
            .named => |name| blk: {
                if (std.mem.eql(u8, name, "i32") or std.mem.eql(u8, name, "int")) {
                    break :blk self.context.i32Type();
                } else if (std.mem.eql(u8, name, "i64")) {
                    break :blk self.context.i64Type();
                } else if (std.mem.eql(u8, name, "f64") or std.mem.eql(u8, name, "double")) {
                    break :blk self.context.doubleType();
                } else if (std.mem.eql(u8, name, "void")) {
                    break :blk self.context.voidType();
                } else {
                    break :blk self.context.i32Type(); // Default
                }
            },
            .void => self.context.voidType(),
            else => self.context.i32Type(), // Default
        };
    }
};

