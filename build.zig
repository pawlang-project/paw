const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const main_mod = b.createModule(.{
        .root_source_file = .{ .cwd_relative = "src/main.zig" },
        .target = target,
        .optimize = optimize,
    });

    const exe = b.addExecutable(.{
        .name = "pawc",
        .root_module = main_mod,
    });

    // 🆕 集成 LLVM（自动检测并启用）
    // Using direct C API bindings instead of llvm-zig
    const local_llvm = "llvm/install";
    const llvm_config_path = b.fmt("{s}/bin/llvm-config", .{local_llvm});
    
    // 自动检测本地 LLVM
    const has_local_llvm = blk: {
        std.fs.cwd().access(llvm_config_path, .{}) catch {
            break :blk false;
        };
        break :blk true;
    };
    
    if (has_local_llvm) {
        std.debug.print("✓ Local LLVM detected at {s}\n", .{local_llvm});
        
        // Add LLVM library paths and includes
        exe.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/lib", .{local_llvm}) });
        exe.addIncludePath(.{ .cwd_relative = b.fmt("{s}/include", .{local_llvm}) });
        
        // Link essential LLVM static libraries (minimal set)
        exe.linkSystemLibrary("LLVMCore");
        exe.linkSystemLibrary("LLVMSupport");
        exe.linkSystemLibrary("LLVMBinaryFormat");
        exe.linkSystemLibrary("LLVMRemarks");
        exe.linkSystemLibrary("LLVMBitstreamReader");
        exe.linkSystemLibrary("LLVMTargetParser");
        exe.linkSystemLibrary("LLVMDemangle");
        
        // Link C++ standard library (LLVM is C++)
        exe.linkLibCpp();
        
        std.debug.print("✅ LLVM native API enabled (3 backends available)\n", .{});
        std.debug.print("   • C backend (default, stable)\n", .{});
        std.debug.print("   • LLVM text mode (--backend=llvm)\n", .{});
        std.debug.print("   • LLVM native API (--backend=llvm-native)\n", .{});
    } else {
        std.debug.print("ℹ️  LLVM not found - using 2 backends\n", .{});
        std.debug.print("   • C backend (default, stable)\n", .{});
        std.debug.print("   • LLVM text mode (--backend=llvm)\n", .{});
        std.debug.print("   💡 To enable native LLVM:\n", .{});
        std.debug.print("      ./scripts/setup_llvm_source.sh && ./scripts/build_llvm_local.sh\n", .{});
    }
    
    // 链接标准库
    exe.linkLibC();

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "运行 Paw 编译器");
    run_step.dependOn(&run_cmd.step);
    
    // 🆕 LLVM 编译流程步骤
    const llvm_example = b.option([]const u8, "llvm-example", "Compile and run a .paw file with LLVM") orelse "examples/llvm_demo.paw";
    
    // Step 1: 编译 PawLang 到 LLVM IR
    const pawc_to_ir = b.addRunArtifact(exe);
    pawc_to_ir.addArg(llvm_example);
    pawc_to_ir.addArg("--backend=llvm-native");
    pawc_to_ir.addArg("-o");
    pawc_to_ir.addArg("output_zig");
    
    // Step 2: 使用本地 Clang 编译 LLVM IR 到可执行文件
    const clang_path = b.fmt("{s}/bin/clang", .{local_llvm});
    
    const compile_ir = b.addSystemCommand(&[_][]const u8{
        clang_path,
        "output_zig.ll",
        "-o",
        "output_zig_exec",
        "-L/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib",
        "-lSystem",
    });
    compile_ir.step.dependOn(&pawc_to_ir.step);
    
    // Step 3: 运行生成的可执行文件
    const run_compiled = b.addSystemCommand(&[_][]const u8{
        "sh",
        "-c",
        "./output_zig_exec; echo \"Exit code: $?\"",
    });
    run_compiled.step.dependOn(&compile_ir.step);
    
    // 创建便捷命令
    const run_llvm_step = b.step("run-llvm", "Compile and run a PawLang file with LLVM (use -Dllvm-example=file.paw)");
    run_llvm_step.dependOn(&run_compiled.step);
    
    // 只编译不运行的步骤
    const build_llvm_step = b.step("build-llvm", "Compile PawLang file to executable with LLVM");
    build_llvm_step.dependOn(&compile_ir.step);
    
    // 🆕 只编译到 LLVM IR 的步骤
    const compile_to_ir_step = b.step("compile-llvm", "Compile PawLang to LLVM IR only");
    compile_to_ir_step.dependOn(&pawc_to_ir.step);
    
    // 🆕 清理生成的文件
    const clean_llvm = b.addSystemCommand(&[_][]const u8{
        "rm",
        "-f",
        "output_zig.ll",
        "output_zig_exec",
    });
    const clean_llvm_step = b.step("clean-llvm", "Clean LLVM build artifacts");
    clean_llvm_step.dependOn(&clean_llvm.step);
}