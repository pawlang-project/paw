const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // 创建 Module
    const main_mod = b.createModule(.{
        .root_source_file = .{ .cwd_relative = "src/main.zig" },
        .target = target,
        .optimize = optimize,
    });

    // 创建可执行文件
    const exe = b.addExecutable(.{
        .name = "pawc",
        .root_module = main_mod,
    });

    b.installArtifact(exe);

    // 运行命令
    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "运行 Paw 编译器");
    run_step.dependOn(&run_cmd.step);
}
