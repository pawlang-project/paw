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
    
    /// 初始化 LLVM 后端
    /// 创建 LLVM 上下文、模块和构建器
    pub fn init(allocator: std.mem.Allocator, module_name: []const u8) !LLVMNativeBackend {
        const context = llvm.Context.create();
        
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
    
    /// 释放 LLVM 后端资源
    pub fn deinit(self: *LLVMNativeBackend) void {
        self.functions.deinit();
        self.variables.deinit();
        self.variable_types.deinit();
        self.builder.dispose();
        self.module.dispose();
        self.context.dispose();
    }
    
    // ============================================================================
    // 辅助函数
    // ============================================================================
    
    /// 创建 null-terminated 字符串的辅助函数
    fn createCString(self: *LLVMNativeBackend, str: []const u8) ![:0]const u8 {
        return try self.allocator.dupeZ(u8, str);
    }
    
    /// 创建函数类型的辅助函数
    fn createFunctionType(self: *LLVMNativeBackend, param_count: usize) llvm.TypeRef {
        const i32_type = self.context.i32Type();
        var param_types = std.ArrayList(llvm.TypeRef).init(self.allocator);
        defer param_types.deinit();
        
        var i: usize = 0;
        while (i < param_count) : (i += 1) {
            param_types.append(i32_type) catch unreachable;
        }
        
        return llvm.functionType(i32_type, param_types.items, false);
    }
    
    /// 保存和恢复循环上下文
    const LoopContext = struct {
        exit: ?llvm.BasicBlockRef,
        continue_block: ?llvm.BasicBlockRef,
    };
    
    fn saveLoopContext(self: *LLVMNativeBackend) LoopContext {
        return LoopContext{
            .exit = self.current_loop_exit,
            .continue_block = self.current_loop_continue,
        };
    }
    
    fn restoreLoopContext(self: *LLVMNativeBackend, ctx: LoopContext) void {
        self.current_loop_exit = ctx.exit;
        self.current_loop_continue = ctx.continue_block;
    }
    
    // ============================================================================
    // 代码生成主函数
    // ============================================================================
    
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
                } else if (loop_stmt.iterator) |iter| {
                    // loop item in collection { } - 迭代循环
                    try self.generateLoopIterator(iter, loop_stmt.body);
                } else {
                    // loop { } - 无限循环
                    try self.generateInfiniteLoop(loop_stmt.body);
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
    
    /// 生成 while 风格的条件循环
    /// 生成: while.cond -> while.body -> while.cond (循环) | while.exit
    fn generateWhileLoop(self: *LLVMNativeBackend, loop: struct { condition: ast.Expr, body: []ast.Stmt }) !void {
        const func = self.current_function orelse return error.NoCurrentFunction;
        
        // 创建基本块
        const cond_block = llvm.appendBasicBlock(self.context, func, "while.cond");
        const body_block = llvm.appendBasicBlock(self.context, func, "while.body");
        const exit_block = llvm.appendBasicBlock(self.context, func, "while.exit");
        
        // 保存并设置循环上下文
        const saved_ctx = self.saveLoopContext();
        defer self.restoreLoopContext(saved_ctx);
        
        self.current_loop_exit = exit_block;
        self.current_loop_continue = cond_block;
        
        // 跳转到条件块
        _ = self.builder.buildBr(cond_block);
        
        // 生成条件块
        self.builder.positionAtEnd(cond_block);
        const cond_value = try self.generateExpr(loop.condition);
        _ = llvm.LLVMBuildCondBr(self.builder.ref, cond_value, body_block, exit_block);
        
        // 生成循环体
        self.builder.positionAtEnd(body_block);
        for (loop.body) |stmt| {
            try self.generateStmt(stmt);
        }
        _ = self.builder.buildBr(cond_block);
        
        // 继续从退出块执行
        self.builder.positionAtEnd(exit_block);
    }
    
    /// 生成 loop 迭代器（范围迭代）
    /// 生成: loop.cond -> loop.body -> loop.incr -> loop.cond (循环) | loop.exit
    fn generateLoopIterator(self: *LLVMNativeBackend, iter: ast.LoopIterator, body: []ast.Stmt) !void {
        const func = self.current_function orelse return error.NoCurrentFunction;
        
        // 只支持范围表达式
        if (iter.iterable != .range) {
            std.debug.print("⚠️  Only range iterators are supported in LLVM backend\n", .{});
            return;
        }
        
        const range = iter.iterable.range;
        const i32_type = self.context.i32Type();
        
        // 创建并初始化循环变量
        const iter_name_z = try self.createCString(iter.binding);
        defer self.allocator.free(iter_name_z);
        
        const iter_var = self.builder.buildAlloca(i32_type, iter_name_z);
        const start_value = try self.generateExpr(range.start.*);
        _ = self.builder.buildStore(start_value, iter_var);
        
        // 注册循环变量（作用域内有效）
        try self.variables.put(iter.binding, iter_var);
        try self.variable_types.put(iter.binding, i32_type);
        defer {
            _ = self.variables.remove(iter.binding);
            _ = self.variable_types.remove(iter.binding);
        }
        
        // 生成结束值
        const end_value = try self.generateExpr(range.end.*);
        
        // 创建基本块
        const cond_block = llvm.appendBasicBlock(self.context, func, "loop.cond");
        const body_block = llvm.appendBasicBlock(self.context, func, "loop.body");
        const incr_block = llvm.appendBasicBlock(self.context, func, "loop.incr");
        const exit_block = llvm.appendBasicBlock(self.context, func, "loop.exit");
        
        // 保存并设置循环上下文
        const saved_ctx = self.saveLoopContext();
        defer self.restoreLoopContext(saved_ctx);
        
        self.current_loop_exit = exit_block;
        self.current_loop_continue = incr_block;
        
        // 跳转到条件块
        _ = self.builder.buildBr(cond_block);
        
        // 生成条件块：检查 i < end 或 i <= end
        self.builder.positionAtEnd(cond_block);
        const current_value = self.builder.buildLoad(i32_type, iter_var, iter_name_z);
        const predicate = if (range.inclusive) llvm.IntPredicate.SLE else llvm.IntPredicate.SLT;
        const cond_value = self.builder.buildICmp(predicate, current_value, end_value, "loop_cond");
        _ = llvm.LLVMBuildCondBr(self.builder.ref, cond_value, body_block, exit_block);
        
        // 生成循环体
        self.builder.positionAtEnd(body_block);
        for (body) |stmt| {
            try self.generateStmt(stmt);
        }
        _ = self.builder.buildBr(incr_block);
        
        // 生成递增块：i = i + 1
        self.builder.positionAtEnd(incr_block);
        const current_value2 = self.builder.buildLoad(i32_type, iter_var, iter_name_z);
        const one = llvm.constI32(self.context, 1);
        const next_value = self.builder.buildAdd(current_value2, one, "loop_incr");
        _ = self.builder.buildStore(next_value, iter_var);
        _ = self.builder.buildBr(cond_block);
        
        // 继续从退出块执行
        self.builder.positionAtEnd(exit_block);
    }
    
    /// 生成无限循环
    /// 生成: loop.body -> loop.body (无限循环，只能通过 break 退出)
    fn generateInfiniteLoop(self: *LLVMNativeBackend, body: []ast.Stmt) !void {
        const func = self.current_function orelse return error.NoCurrentFunction;
        
        // 创建基本块
        const body_block = llvm.appendBasicBlock(self.context, func, "loop.body");
        const exit_block = llvm.appendBasicBlock(self.context, func, "loop.exit");
        
        // 保存并设置循环上下文
        const saved_ctx = self.saveLoopContext();
        defer self.restoreLoopContext(saved_ctx);
        
        self.current_loop_exit = exit_block;
        self.current_loop_continue = body_block;
        
        // 跳转到循环体
        _ = self.builder.buildBr(body_block);
        
        // 生成循环体（无限循环回自己）
        self.builder.positionAtEnd(body_block);
        for (body) |stmt| {
            try self.generateStmt(stmt);
        }
        _ = self.builder.buildBr(body_block);
        
        // 退出块（只能通过 break 到达）
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
            .string_literal => |str| blk: {
                // Create null-terminated string
                const str_z = try self.allocator.dupeZ(u8, str);
                defer self.allocator.free(str_z);
                
                const name_z = try self.allocator.dupeZ(u8, "str");
                defer self.allocator.free(name_z);
                
                // Build global string pointer
                break :blk self.builder.buildGlobalStringPtr(str_z, name_z);
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
            .array_index => |index_expr| blk: {
                // Array/string indexing: arr[index]
                const array_value = try self.generateExpr(index_expr.array.*);
                const index_value = try self.generateExpr(index_expr.index.*);
                
                // Build GEP instruction
                var indices = [_]llvm.ValueRef{ llvm.constI32(self.context, 0), index_value };
                
                const gep_name_z = try self.allocator.dupeZ(u8, "index");
                defer self.allocator.free(gep_name_z);
                
                // Get element type (assuming i32 for now, should be improved)
                const element_type = self.context.i32Type();
                const array_type = llvm.arrayType(element_type, 0);  // Size doesn't matter for GEP
                
                const gep = self.builder.buildInBoundsGEP(array_type, array_value, &indices, gep_name_z);
                
                // Load the value
                const load_name_z = try self.allocator.dupeZ(u8, "elem");
                defer self.allocator.free(load_name_z);
                break :blk self.builder.buildLoad(element_type, gep, load_name_z);
            },
            .field_access => |field_expr| blk: {
                // Struct field access: obj.field
                _ = try self.generateExpr(field_expr.object.*);
                
                // For now, return a placeholder
                // TODO: Need type information to determine field index
                std.debug.print("⚠️  Field access not yet fully implemented: {s}\n", .{field_expr.field});
                break :blk llvm.constI32(self.context, 0);
            },
            .call => |call_expr| blk: {
                // 🆕 检查是否是实例方法调用 (obj.method 形式)
                if (call_expr.callee.* == .field_access) {
                    const field = call_expr.callee.field_access;
                    
                    // 尝试获取对象类型和生成修饰后的方法名
                    if (field.object.* == .identifier) {
                        const var_name = field.object.identifier;
                        // 简化实现：假设对象是 i32 类型，方法名是 TypeName_method
                        // TODO: 需要类型系统支持才能正确查找类型
                        
                        // 生成修饰后的方法名（简化版）
                        var method_name = std.ArrayList(u8).init(self.allocator);
                        defer method_name.deinit();
                        
                        // 假设类型名就是变量名的首字母大写形式
                        try method_name.appendSlice(var_name);
                        try method_name.appendSlice("_");
                        try method_name.appendSlice(field.field);
                        
                        const mangled_method_name = try method_name.toOwnedSlice();
                        defer self.allocator.free(mangled_method_name);
                        
                        // 查找方法
                        if (self.functions.get(mangled_method_name)) |func| {
                            // 生成参数：第一个参数是 self
                            var args = std.ArrayList(llvm.ValueRef).init(self.allocator);
                            defer args.deinit();
                            
                            // 添加 self 参数（对象本身）
                            const obj_value = try self.generateExpr(field.object.*);
                            try args.append(obj_value);
                            
                            // 添加其他参数
                            for (call_expr.args) |arg| {
                                const arg_value = try self.generateExpr(arg);
                                try args.append(arg_value);
                            }
                            
                            // 获取函数类型
                            const i32_type = self.context.i32Type();
                            var param_types = std.ArrayList(llvm.TypeRef).init(self.allocator);
                            defer param_types.deinit();
                            for (args.items) |_| {
                                try param_types.append(i32_type);
                            }
                            const func_type = llvm.functionType(i32_type, param_types.items, false);
                            
                            // 构建调用
                            const call_name_z = try self.allocator.dupeZ(u8, "method_call");
                            defer self.allocator.free(call_name_z);
                            
                            const result = self.builder.buildCall(func_type, func, args.items, call_name_z);
                            break :blk result;
                        }
                    }
                }
                
                // 普通函数调用
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
            .static_method_call => |smc| blk: {
                // 🆕 静态方法调用：Type<T>::method()
                // 生成修饰后的函数名：Type_T_method
                var func_name = std.ArrayList(u8).init(self.allocator);
                defer func_name.deinit();
                
                // 添加类型名
                try func_name.appendSlice(smc.type_name);
                
                // 添加类型参数
                for (smc.type_args) |type_arg| {
                    try func_name.appendSlice("_");
                    const type_name = try self.getSimpleTypeName(type_arg);
                    // 只有 generic_instance 返回的是需要释放的内存
                    const needs_free = type_arg == .generic_instance;
                    defer if (needs_free) self.allocator.free(type_name);
                    try func_name.appendSlice(type_name);
                }
                
                // 添加方法名
                try func_name.appendSlice("_");
                try func_name.appendSlice(smc.method_name);
                
                const mangled_name = try func_name.toOwnedSlice();
                defer self.allocator.free(mangled_name);
                
                // 查找函数
                const func = self.functions.get(mangled_name) orelse {
                    std.debug.print("⚠️  Undefined static method: {s}\n", .{mangled_name});
                    break :blk llvm.constI32(self.context, 0);
                };
                
                // 生成参数
                var args = std.ArrayList(llvm.ValueRef).init(self.allocator);
                defer args.deinit();
                
                for (smc.args) |arg| {
                    const arg_value = try self.generateExpr(arg);
                    try args.append(arg_value);
                }
                
                // 获取函数类型
                const i32_type = self.context.i32Type();
                var param_types = std.ArrayList(llvm.TypeRef).init(self.allocator);
                defer param_types.deinit();
                for (args.items) |_| {
                    try param_types.append(i32_type);
                }
                const func_type = llvm.functionType(i32_type, param_types.items, false);
                
                // 构建调用
                const call_name_z = try self.allocator.dupeZ(u8, "static_call");
                defer self.allocator.free(call_name_z);
                
                const result = self.builder.buildCall(func_type, func, args.items, call_name_z);
                break :blk result;
            },
            .array_literal => |elements| blk: {
                // 🆕 数组字面量：[1, 2, 3]
                // 简化实现：返回第一个元素的值
                if (elements.len == 0) {
                    break :blk llvm.constI32(self.context, 0);
                }
                
                // 返回第一个元素的值（简化实现）
                const first_element = try self.generateExpr(elements[0]);
                break :blk first_element;
            },
            .struct_init => |si| blk: {
                // 🆕 结构体字面量：Point { x: 1, y: 2 }
                // 简化实现：返回第一个字段的值
                if (si.fields.len == 0) {
                    break :blk llvm.constI32(self.context, 0);
                }
                
                // 返回第一个字段的值（简化实现）
                const first_field_value = try self.generateExpr(si.fields[0].value);
                break :blk first_field_value;
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
    
    /// 获取类型的简化名（用于name mangling）
    /// 注意：对于 generic_instance，调用者需要负责释放返回的字符串
    fn getSimpleTypeName(self: *LLVMNativeBackend, paw_type: ast.Type) ![]const u8 {
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
            .generic_instance => |gi| blk: {
                // 🆕 处理泛型实例：Vec<i32> -> Vec_i32
                // 注意：这会分配新内存，调用者需要释放
                var buf = std.ArrayList(u8).init(self.allocator);
                errdefer buf.deinit();
                
                try buf.appendSlice(gi.name);
                for (gi.type_args) |arg| {
                    try buf.appendSlice("_");
                    const type_name = try self.getSimpleTypeName(arg);
                    // 只有 generic_instance 返回的是需要释放的内存
                    const needs_free = arg == .generic_instance;
                    defer if (needs_free) self.allocator.free(type_name);
                    try buf.appendSlice(type_name);
                }
                break :blk try buf.toOwnedSlice();
            },
            else => "unknown",
        };
    }
};

