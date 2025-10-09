const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // 🆕 Build options for conditional compilation
    const build_options = b.addOptions();
    
    const main_mod = b.createModule(.{
        .root_source_file = .{ .cwd_relative = "src/main.zig" },
        .target = target,
        .optimize = optimize,
    });
    
    // Add build_options to main module
    main_mod.addOptions("build_options", build_options);

    const exe = b.addExecutable(.{
        .name = "pawc",
        .root_module = main_mod,
    });
    
    // Windows: Use system linker (MinGW ld) instead of lld-link for C++ compatibility
    if (target.result.os.tag == .windows) {
        exe.use_lld = false;
        exe.use_llvm = false;
    }

    // 🆕 集成 LLVM（自动检测并启用）
    // Using direct C API bindings instead of llvm-zig
    const local_llvm = "llvm/install";
    
    // 自动检测本地 LLVM（Windows needs .exe extension）
    const llvm_config_name = if (target.result.os.tag == .windows) "llvm-config.exe" else "llvm-config";
    const llvm_config_path = b.fmt("{s}/bin/{s}", .{local_llvm, llvm_config_name});
    
    const has_local_llvm = blk: {
        std.fs.cwd().access(llvm_config_path, .{}) catch {
            break :blk false;
        };
        break :blk true;
    };
    
    // LLVM native backend is now available on all platforms
    const enable_llvm_native = has_local_llvm;
    build_options.addOption(bool, "llvm_native_available", enable_llvm_native);
    
    if (has_local_llvm) {
        std.debug.print("✓ Local LLVM detected at {s}\n", .{local_llvm});
        std.debug.print("✅ LLVM native API enabled\n", .{});
        std.debug.print("   • C backend: --backend=c (default)\n", .{});
        std.debug.print("   • LLVM backend: --backend=llvm (native API)\n", .{});
        
        // Add LLVM library paths and includes
        exe.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/lib", .{local_llvm}) });
        exe.addIncludePath(.{ .cwd_relative = b.fmt("{s}/include", .{local_llvm}) });
        
        // Link comprehensive set of LLVM libraries
        const llvm_libs = [_][]const u8{
            // Core libraries
            "LLVMCore",
            "LLVMSupport",
            "LLVMDemangle",
            
            // Analysis and optimization
            "LLVMAnalysis",
            "LLVMTransformUtils",
            "LLVMScalarOpts",
            "LLVMInstCombine",
            "LLVMAggressiveInstCombine",
            "LLVMipo",
            "LLVMVectorize",
            
            // Target support
            "LLVMTarget",
            "LLVMTargetParser",
            "LLVMMC",
            "LLVMMCParser",
            "LLVMBitReader",
            "LLVMBitWriter",
            "LLVMAsmParser",
            "LLVMAsmPrinter",
            
            // Code generation for x86
            "LLVMX86CodeGen",
            "LLVMX86AsmParser",
            "LLVMX86Desc",
            "LLVMX86Info",
            "LLVMX86Disassembler",
            
            // Utilities
            "LLVMBinaryFormat",
            "LLVMRemarks",
            "LLVMBitstreamReader",
            "LLVMTextAPI",
            "LLVMProfileData",
            "LLVMDebugInfoDWARF",
            "LLVMDebugInfoCodeView",
            "LLVMDebugInfoMSF",
            "LLVMGlobalISel",
            "LLVMSelectionDAG",
            "LLVMCodeGen",
            "LLVMObjCARCOpts",
            "LLVMCoroutines",
            "LLVMCFGuard",
        };
        
        for (llvm_libs) |lib| {
            exe.linkSystemLibrary(lib);
        }
        
        // Windows-specific: Link MinGW C++ runtime libraries
        if (target.result.os.tag == .windows) {
            // Use MinGW's libstdc++ (must be before other system libs)
            exe.linkSystemLibrary("stdc++");
            exe.linkSystemLibrary("gcc_s");
            exe.linkSystemLibrary("gcc");
            exe.linkSystemLibrary("pthread");
            
            // Windows system libraries needed by LLVM
            exe.linkSystemLibrary("ole32");
            exe.linkSystemLibrary("uuid");
            exe.linkSystemLibrary("version");
            exe.linkSystemLibrary("psapi");
            exe.linkSystemLibrary("shell32");
            exe.linkSystemLibrary("advapi32");
            
            std.debug.print("   🔧 Using MinGW C++ runtime\n", .{});
        } else {
            // Unix: Use system C++ library
            exe.linkLibCpp();
        }
    } else {
        build_options.addOption(bool, "llvm_native_available", false);
        std.debug.print("ℹ️  LLVM not found\n", .{});
        std.debug.print("   • C backend: --backend=c (default)\n", .{});
        std.debug.print("   • LLVM backend: --backend=llvm (text mode)\n", .{});
        std.debug.print("   💡 For setup: python scripts/setup_llvm.py && python scripts/build_llvm.py\n", .{});
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