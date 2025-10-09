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
    // 当前文本后端不需要，但预先配置好
    if (b.option(bool, "with-llvm", "Enable native LLVM backend (experimental)") orelse false) {
        const llvm_dep = b.dependency("llvm", .{
            .target = target,
            .optimize = optimize,
        });
        const llvm_mod = llvm_dep.module("llvm");
        exe.root_module.addImport("llvm", llvm_mod);
        
        std.debug.print("✓ LLVM native backend enabled\n", .{});
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