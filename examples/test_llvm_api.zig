//! Simple test to verify LLVM C API bindings work
//!
//! Compile: zig build-exe examples/test_llvm_api.zig -I llvm/install/include -L llvm/install/lib -lLLVMCore -lLLVMSupport -lLLVMTargetParser -lLLVMBinaryFormat -lLLVMRemarks -lLLVMBitstreamReader -lLLVMDemangle -lc++

const std = @import("std");
const llvm = @import("../src/llvm_c_api.zig");

pub fn main() !void {
    std.debug.print("🚀 Testing LLVM C API Bindings\n\n", .{});

    // Create LLVM context
    std.debug.print("Creating LLVM context...\n", .{});
    var context = llvm.Context.create();
    defer context.dispose();
    std.debug.print("✅ Context created\n\n", .{});

    // Create module
    std.debug.print("Creating module 'test_module'...\n", .{});
    var module = context.createModule("test_module");
    defer module.dispose();
    std.debug.print("✅ Module created\n\n", .{});

    // Create function type: i32 add(i32 %a, i32 %b)
    std.debug.print("Creating function 'add'...\n", .{});
    const i32_type = context.i32Type();
    var param_types = [_]llvm.TypeRef{ i32_type, i32_type };
    const func_type = llvm.functionType(i32_type, &param_types, false);

    // Add function to module
    const func = module.addFunction("add", func_type);
    std.debug.print("✅ Function signature created\n\n", .{});

    // Create entry basic block
    std.debug.print("Creating basic block 'entry'...\n", .{});
    const entry_block = llvm.appendBasicBlock(context, func, "entry");
    std.debug.print("✅ Basic block created\n\n", .{});

    // Create builder and position at end of entry block
    std.debug.print("Creating IR builder...\n", .{});
    var builder = context.createBuilder();
    defer builder.dispose();
    builder.positionAtEnd(entry_block);
    std.debug.print("✅ Builder positioned\n\n", .{});

    // Get function parameters
    const param_a = llvm.LLVMGetParam(func, 0);
    const param_b = llvm.LLVMGetParam(func, 1);

    // Build: %result = add i32 %a, %b
    std.debug.print("Building add instruction...\n", .{});
    const result = builder.buildAdd(param_a, param_b, "result");
    std.debug.print("✅ Add instruction built\n\n", .{});

    // Build: ret i32 %result
    std.debug.print("Building return instruction...\n", .{});
    _ = builder.buildRet(result);
    std.debug.print("✅ Return instruction built\n\n", .{});

    // Verify module
    std.debug.print("Verifying module...\n", .{});
    try module.verify();
    std.debug.print("✅ Module verified successfully\n\n", .{});

    // Print module IR
    std.debug.print("╔══════════════════════════════════════════════════════════════╗\n", .{});
    std.debug.print("║           Generated LLVM IR                                  ║\n", .{});
    std.debug.print("╚══════════════════════════════════════════════════════════════╝\n\n", .{});
    
    const ir = module.toString();
    defer llvm.LLVMDisposeMessage(@constCast(ir.ptr));
    std.debug.print("{s}\n", .{ir});

    std.debug.print("\n╔══════════════════════════════════════════════════════════════╗\n", .{});
    std.debug.print("║           ✅ Success! LLVM C API is working!                ║\n", .{});
    std.debug.print("╚══════════════════════════════════════════════════════════════╝\n", .{});
}

