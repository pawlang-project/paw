//! Test direct LLVM C API bindings
//!
//! This test creates a simple function using LLVM C API
//! to verify our bindings work correctly.

const std = @import("std");
const llvm = @import("../src/llvm_c_api.zig");

test "LLVM C API - create simple function" {
    // Create LLVM context
    var context = llvm.Context.create();
    defer context.dispose();

    // Create module
    var module = context.createModule("test_module");
    defer module.dispose();

    // Create function type: i32 add(i32 %a, i32 %b)
    const i32_type = context.i32Type();
    var param_types = [_]llvm.TypeRef{ i32_type, i32_type };
    const func_type = llvm.functionType(i32_type, &param_types, false);

    // Add function to module
    const func = module.addFunction("add", func_type);

    // Create entry basic block
    const entry_block = llvm.appendBasicBlock(context, func, "entry");

    // Create builder and position at end of entry block
    var builder = context.createBuilder();
    defer builder.dispose();
    builder.positionAtEnd(entry_block);

    // Get function parameters
    const param_a = llvm.LLVMGetParam(func, 0);
    const param_b = llvm.LLVMGetParam(func, 1);

    // Build: %result = add i32 %a, %b
    const result = builder.buildAdd(param_a, param_b, "result");

    // Build: ret i32 %result
    _ = builder.buildRet(result);

    // Verify module
    try module.verify();

    // Print module IR
    const ir = module.toString();
    defer llvm.LLVMDisposeMessage(@constCast(ir.ptr));

    std.debug.print("\n=== Generated LLVM IR ===\n{s}\n", .{ir});

    // Basic check that IR contains expected content
    try std.testing.expect(std.mem.indexOf(u8, ir, "define") != null);
    try std.testing.expect(std.mem.indexOf(u8, ir, "add") != null);
    try std.testing.expect(std.mem.indexOf(u8, ir, "ret") != null);
}

test "LLVM C API - create main function returning 42" {
    // Create LLVM context
    var context = llvm.Context.create();
    defer context.dispose();

    // Create module
    var module = context.createModule("test_main");
    defer module.dispose();

    // Create function type: i32 main()
    const i32_type = context.i32Type();
    const func_type = llvm.functionType(i32_type, &[_]llvm.TypeRef{}, false);

    // Add function to module
    const func = module.addFunction("main", func_type);

    // Create entry basic block
    const entry_block = llvm.appendBasicBlock(context, func, "entry");

    // Create builder
    var builder = context.createBuilder();
    defer builder.dispose();
    builder.positionAtEnd(entry_block);

    // Build: ret i32 42
    const ret_val = llvm.constI32(context, 42);
    _ = builder.buildRet(ret_val);

    // Verify module
    try module.verify();

    // Print module IR
    const ir = module.toString();
    defer llvm.LLVMDisposeMessage(@constCast(ir.ptr));

    std.debug.print("\n=== Generated main() returning 42 ===\n{s}\n", .{ir});

    try std.testing.expect(std.mem.indexOf(u8, ir, "i32 42") != null);
}

