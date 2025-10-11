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
    
    // Note: Let Zig use default linker (lld) for all platforms
    // This ensures compatibility without external dependencies like MinGW

    // 🆕 检测系统 LLVM - 定义路径列表
    const llvm_config_paths: []const []const u8 = if (target.result.os.tag == .macos)
        &[_][]const u8{
            "llvm-config",
            "/opt/homebrew/opt/llvm@19/bin/llvm-config",  // ARM64 macOS
            "/usr/local/opt/llvm@19/bin/llvm-config",     // Intel macOS
        }
    else if (target.result.os.tag == .linux)
        &[_][]const u8{
            "llvm-config",
            "/usr/bin/llvm-config-19",
            "/usr/lib/llvm-19/bin/llvm-config",
        }
    else if (target.result.os.tag == .windows)
        &[_][]const u8{
            "llvm-config.exe",
            "C:\\Program Files\\LLVM\\bin\\llvm-config.exe",
        }
    else
        &[_][]const u8{"llvm-config"};
    
    // Try to find llvm-config (Unix) or check LLVM directory (Windows)
    const llvm_config_path = blk: {
        // Windows: llvm-config.exe doesn't exist in official builds, check directory directly
        if (target.result.os.tag == .windows) {
            const llvm_dir = "C:\\Program Files\\LLVM";
            const llvm_bin = b.fmt("{s}\\bin", .{llvm_dir});
            
            // Check if LLVM directory exists by trying to access clang.exe
            const clang_path = b.fmt("{s}\\clang.exe", .{llvm_bin});
            std.fs.accessAbsolute(clang_path, .{}) catch {
                break :blk null;  // LLVM not found
            };
            
            // LLVM found, return a marker (we'll handle Windows specially later)
            break :blk "windows_llvm";
        }
        
        // Unix: use llvm-config
        for (llvm_config_paths) |path| {
            const result = std.process.Child.run(.{
                .allocator = b.allocator,
                .argv = &[_][]const u8{ path, "--version" },
            }) catch continue;
            defer b.allocator.free(result.stdout);
            defer b.allocator.free(result.stderr);
            if (result.term.Exited == 0) {
                break :blk path;
            }
        }
        break :blk null;
    };
    
    const has_llvm = llvm_config_path != null;
    const is_windows_llvm = if (llvm_config_path) |path| std.mem.eql(u8, path, "windows_llvm") else false;
    
    // Get LLVM configuration from llvm-config
    var llvm_link_flags: ?[]const u8 = null;
    var llvm_include_path: ?[]const u8 = null;
    
    // Configure LLVM paths and flags
    if (llvm_config_path) |config_path| {
        if (is_windows_llvm) {
            // Windows: Use hardcoded paths since llvm-config.exe doesn't exist
            llvm_include_path = "C:\\Program Files\\LLVM\\include";
        } else {
            // Unix: Use llvm-config
            const link_result = b.run(&[_][]const u8{
                config_path,
                "--link-shared",
                "--ldflags",
                "--libs",
                "--system-libs",
            });
            llvm_link_flags = link_result;
            
            // Get include path
            llvm_include_path = b.run(&[_][]const u8{
                config_path,
                "--includedir",
            });
        }
    }
    
    build_options.addOption(bool, "llvm_native_available", has_llvm);
    
    // Print build configuration header
    std.debug.print("\n╭─────────────────────────────────────────╮\n", .{});
    std.debug.print("│   🐾 PawLang Compiler Build System     │\n", .{});
    std.debug.print("╰─────────────────────────────────────────╯\n\n", .{});
    
    // Build target info
    std.debug.print("🎯 Target: {s}-{s}\n", .{
        @tagName(target.result.cpu.arch),
        @tagName(target.result.os.tag),
    });
    std.debug.print("⚡ Optimize: {s}\n\n", .{@tagName(optimize)});
    
    // Print LLVM configuration
    if (has_llvm) {
        std.debug.print("┌─ LLVM Configuration ────────────────────\n", .{});
        if (is_windows_llvm) {
            std.debug.print("│ 📦 Location: C:\\Program Files\\LLVM\n", .{});
            std.debug.print("│ 🔍 Detection: clang.exe found\n", .{});
            std.debug.print("│ 🔗 Linking: LLVM-C + libc++\n", .{});
        } else if (llvm_config_path) |config_path| {
            std.debug.print("│ 📦 Config: {s}\n", .{config_path});
            std.debug.print("│ 🔍 Detection: llvm-config\n", .{});
            std.debug.print("│ 🔗 Linking: shared libraries\n", .{});
        }
        std.debug.print("└─────────────────────────────────────────\n\n", .{});
        
        std.debug.print("✅ Available Backends:\n", .{});
        std.debug.print("   • C backend    (default) → --backend=c\n", .{});
        std.debug.print("   • LLVM backend (enabled) → --backend=llvm\n", .{});
        
        // Add LLVM include path
        if (llvm_include_path) |include_path| {
            const trimmed = std.mem.trim(u8, include_path, " \n\r\t");
            exe.addIncludePath(.{ .cwd_relative = trimmed });
        }
        
        // Windows: Link LLVM directly without llvm-config
        if (is_windows_llvm) {
            // Add LLVM library path
            exe.addLibraryPath(.{ .cwd_relative = "C:\\Program Files\\LLVM\\lib" });
            
            // Link main LLVM library
            exe.linkSystemLibrary("LLVM-C");
            
            // Windows C++ runtime (use linkLibCpp for proper MSVC linking)
            exe.linkLibCpp();
        } else {
            // Unix: Use llvm-config output for all linking
            if (llvm_link_flags) |flags| {
                // Parse and add flags from llvm-config
                var iter = std.mem.tokenizeAny(u8, flags, " \n\r\t");
                while (iter.next()) |flag| {
                    if (std.mem.startsWith(u8, flag, "-l")) {
                        const lib_name = flag[2..];
                        exe.linkSystemLibrary(lib_name);
                    } else if (std.mem.startsWith(u8, flag, "-L")) {
                        const lib_path = flag[2..];
                        exe.addLibraryPath(.{ .cwd_relative = lib_path });
                    }
                }
            }
            
            // Platform-specific C++ runtime
            if (target.result.os.tag == .linux) {
                exe.linkLibCpp();
            } else if (target.result.os.tag == .macos) {
                exe.linkLibCpp();
            }
        }
    } else {
        build_options.addOption(bool, "llvm_native_available", false);
        std.debug.print("┌─ LLVM Configuration ────────────────────\n", .{});
        std.debug.print("│ ⚠️  LLVM not detected\n", .{});
        std.debug.print("└─────────────────────────────────────────\n\n", .{});
        
        std.debug.print("✅ Available Backends:\n", .{});
        std.debug.print("   • C backend    (default) → --backend=c\n", .{});
        std.debug.print("   • LLVM backend (unavailable)\n\n", .{});
        
        std.debug.print("💡 Install LLVM to enable LLVM backend:\n", .{});
        const os_tag = target.result.os.tag;
        if (os_tag == .windows) {
            std.debug.print("   → choco install llvm --version=19.1.7\n", .{});
        } else if (os_tag == .macos) {
            std.debug.print("   → brew install llvm@19\n", .{});
        } else if (os_tag == .linux) {
            std.debug.print("   → sudo apt install llvm-19-dev\n", .{});
        }
    }
    
    std.debug.print("\n╭─────────────────────────────────────────╮\n", .{});
    std.debug.print("│   🚀 Building pawc compiler...          │\n", .{});
    std.debug.print("╰─────────────────────────────────────────╯\n", .{});
    
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
    
    // Step 2: 使用系统 Clang 编译 LLVM IR 到可执行文件
    const compile_ir = b.addSystemCommand(&[_][]const u8{
        "clang",
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