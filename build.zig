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
    // Note: Native LLVM API integration is experimental and requires system LLVM
    // The text-based IR generation (default) works without any LLVM installation
    if (b.option(bool, "with-llvm", "Enable native LLVM backend (experimental)") orelse false) {
        std.debug.print("⚠️  Native LLVM API integration is not yet supported.\n", .{});
        std.debug.print("💡 Use text mode instead (default):\n", .{});
        std.debug.print("   zig build\n", .{});
        std.debug.print("   ./zig-out/bin/pawc hello.paw --backend=llvm\n", .{});
        std.debug.print("\n", .{});
        std.debug.print("📝 Native API requires:\n", .{});
        std.debug.print("   - LLVM dynamic library (libLLVM.dylib)\n", .{});
        std.debug.print("   - Compatible llvm-zig bindings\n", .{});
        std.debug.print("   - Planned for v0.1.5+\n", .{});
        std.debug.print("\n", .{});
        return;
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