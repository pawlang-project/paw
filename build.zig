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
    if (b.option(bool, "with-llvm", "Enable native LLVM backend (experimental)") orelse false) {
        // 优先使用本地编译的 LLVM
        const local_llvm = "llvm/install";
        
        // 检查本地LLVM是否存在
        const llvm_config_path = b.fmt("{s}/bin/llvm-config", .{local_llvm});
        if (std.fs.cwd().access(llvm_config_path, .{})) {
            std.debug.print("✓ Using local LLVM from {s}\n", .{local_llvm});
            
            // 添加本地LLVM的路径
            exe.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/lib", .{local_llvm}) });
            exe.addIncludePath(.{ .cwd_relative = b.fmt("{s}/include", .{local_llvm}) });
            exe.linkSystemLibrary("LLVM");
            
            // 添加 llvm-zig 模块
            const llvm_dep = b.dependency("llvm", .{
                .target = target,
                .optimize = optimize,
            });
            const llvm_mod = llvm_dep.module("llvm");
            exe.root_module.addImport("llvm", llvm_mod);
        } else |_| {
            std.debug.print("⚠️  Local LLVM not found, build it first:\n", .{});
            std.debug.print("   ./scripts/build_llvm.sh\n", .{});
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