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
    variable_types: std.StringHashMap(llvm.TypeRef),  // Track variable types for load/store
    
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
            .variable_types = std.StringHashMap(llvm.TypeRef).init(allocator),
            .current_function = null,
            .current_loop_exit = null,
            .current_loop_continue = null,
        };
    }
    
    pub fn deinit(self: *LLVMNativeBackend) void {
        self.functions.deinit();
        self.variables.deinit();
        self.variable_types.deinit();
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
                    
                    // Determine variable type
                    const var_type = if (let_stmt.type) |typ|
                        try self.toLLVMType(typ)
                    else
                        llvm.LLVMTypeOf(init_value);
                    
                    // Allocate space for variable
                    const alloca_name_z = try self.allocator.dupeZ(u8, let_stmt.name);
                    defer self.allocator.free(alloca_name_z);
                    const alloca = self.builder.buildAlloca(var_type, alloca_name_z);
                    
                    // Store initial value
                    _ = self.builder.buildStore(init_value, alloca);
                    
                    // Store pointer in variables map
                    try self.variables.put(let_stmt.name, alloca);
                    try self.variable_types.put(let_stmt.name, var_type);
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
                } else if (loop_stmt.iterator) |_| {
                    // loop item in collection { } - 迭代循环 (TODO: 未实现)
                    std.debug.print("⚠️  Loop iterators not yet implemented in LLVM backend\n", .{});
                } else {
                    // loop { } - 无限循环 (TODO: 未实现)
                    std.debug.print("⚠️  Infinite loops not yet implemented in LLVM backend\n", .{});
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
            .bool_literal => |val| blk: {
                const i1_type = self.context.i1Type();
                break :blk llvm.LLVMConstInt(i1_type, if (val) 1 else 0, 0);
            },
            .char_literal => |val| blk: {
                const i32_type = self.context.i32Type();
                break :blk llvm.LLVMConstInt(i32_type, @intCast(val), 0);
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
                const lhs = try self.generateExpr(binop.left.*);
                const rhs = try self.generateExpr(binop.right.*);
                
                const result_name_z = try self.allocator.dupeZ(u8, "binop");
                defer self.allocator.free(result_name_z);
                
                const result = switch (binop.op) {
                    .add => self.builder.buildAdd(lhs, rhs, result_name_z),
                    .sub => self.builder.buildSub(lhs, rhs, result_name_z),
                    .mul => self.builder.buildMul(lhs, rhs, result_name_z),
                    .div => self.builder.buildSDiv(lhs, rhs, result_name_z),
                    // Comparison operators
                    .eq => self.builder.buildICmp(.EQ, lhs, rhs, result_name_z),
                    .ne => self.builder.buildICmp(.NE, lhs, rhs, result_name_z),
                    .lt => self.builder.buildICmp(.SLT, lhs, rhs, result_name_z),
                    .le => self.builder.buildICmp(.SLE, lhs, rhs, result_name_z),
                    .gt => self.builder.buildICmp(.SGT, lhs, rhs, result_name_z),
                    .ge => self.builder.buildICmp(.SGE, lhs, rhs, result_name_z),
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
                const then_end_block = self.builder.getInsertBlock();
                _ = self.builder.buildBr(cont_block);
                
                // Generate else branch
                self.builder.positionAtEnd(else_block);
                const else_value = if (if_expr.else_branch) |else_br|
                    try self.generateExpr(else_br.*)
                else
                    llvm.constI32(self.context, 0);
                const else_end_block = self.builder.getInsertBlock();
                _ = self.builder.buildBr(cont_block);
                
                // Continue block with PHI node
                self.builder.positionAtEnd(cont_block);
                
                // Create PHI node to merge values from both branches
                const result_type = llvm.LLVMTypeOf(then_value);
                const phi_name_z = try self.allocator.dupeZ(u8, "if.result");
                defer self.allocator.free(phi_name_z);
                const phi = self.builder.buildPhi(result_type, phi_name_z);
                
                // Add incoming values
                var incoming_values = [_]llvm.ValueRef{ then_value, else_value };
                var incoming_blocks = [_]llvm.BasicBlockRef{ then_end_block, else_end_block };
                llvm.LLVMAddIncoming(phi, &incoming_values, &incoming_blocks, 2);
                
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

