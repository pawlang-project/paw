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

    // 🆕 集成 LLVM（可选，用于未来原生后端）
    // Using direct C API bindings instead of llvm-zig
    if (b.option(bool, "with-llvm", "Enable native LLVM backend (experimental)") orelse false) {
        const local_llvm = "llvm/install";
        
        // Check if local LLVM exists
        const llvm_config_path = b.fmt("{s}/bin/llvm-config", .{local_llvm});
        if (std.fs.cwd().access(llvm_config_path, .{})) {
            std.debug.print("✓ Using local LLVM from {s}\n", .{local_llvm});
            
            // Add LLVM library paths and includes
            exe.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/lib", .{local_llvm}) });
            exe.addIncludePath(.{ .cwd_relative = b.fmt("{s}/include", .{local_llvm}) });
            
            // Link essential LLVM static libraries
            exe.linkSystemLibrary("LLVMCore");
            exe.linkSystemLibrary("LLVMSupport");
            exe.linkSystemLibrary("LLVMTargetParser");
            exe.linkSystemLibrary("LLVMBinaryFormat");
            exe.linkSystemLibrary("LLVMRemarks");
            exe.linkSystemLibrary("LLVMBitstreamReader");
            exe.linkSystemLibrary("LLVMDemangle");
            
            // Link C++ standard library (LLVM is C++)
            exe.linkLibCpp();
            
            std.debug.print("✅ LLVM native API enabled (direct C bindings)\n", .{});
        } else |_| {
            std.debug.print("⚠️  Local LLVM not found, build it first:\n", .{});
            std.debug.print("   ./scripts/setup_llvm_source.sh\n", .{});
            std.debug.print("   ./scripts/build_llvm_local.sh\n", .{});
        }
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
}