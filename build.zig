const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // 🆕 Build options for conditional compilation
    const build_options = b.addOptions();
    
    // 🆕 LLVM后端支持（可选，用于测试纯C后端）
    const enable_llvm = b.option(bool, "enable-llvm", "Enable LLVM backend support (default: true)") orelse true;
    
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
        // 如果用户禁用LLVM，跳过检测
        if (!enable_llvm) {
            break :blk null;
        }
        
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
    std.debug.print("\n===========================================\n", .{});
    std.debug.print("   PawLang Compiler Build System\n", .{});
    std.debug.print("===========================================\n\n", .{});
    
    // Build target info
    std.debug.print("Target: {s}-{s}\n", .{
        @tagName(target.result.cpu.arch),
        @tagName(target.result.os.tag),
    });
    std.debug.print("Optimize: {s}\n\n", .{@tagName(optimize)});
    
    // Print LLVM configuration
    if (has_llvm) {
        std.debug.print("--- LLVM Configuration ---\n", .{});
        if (is_windows_llvm) {
            std.debug.print("Location: C:\\Program Files\\LLVM\n", .{});
            std.debug.print("Detection: clang.exe found\n", .{});
            std.debug.print("Linking: LLVM-C + libc++\n", .{});
        } else if (llvm_config_path) |config_path| {
            std.debug.print("Config: {s}\n", .{config_path});
            std.debug.print("Detection: llvm-config\n", .{});
            std.debug.print("Linking: shared libraries\n", .{});
        }
        std.debug.print("\n", .{});
        
        std.debug.print("Available Backends:\n", .{});
        std.debug.print("  - C backend    (default) -> --backend=c\n", .{});
        std.debug.print("  - LLVM backend (enabled) -> --backend=llvm\n", .{});
        
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
        std.debug.print("--- LLVM Configuration ---\n", .{});
        if (!enable_llvm) {
            std.debug.print("Status: LLVM backend disabled by user\n", .{});
            std.debug.print("Tip: Remove -Denable-llvm=false to enable\n", .{});
        } else {
            std.debug.print("Status: LLVM not detected\n", .{});
        }
        std.debug.print("\n", .{});
        
        std.debug.print("Available Backends:\n", .{});
        std.debug.print("  - C backend    (default) -> --backend=c\n", .{});
        std.debug.print("  - LLVM backend (unavailable)\n\n", .{});
        
        if (enable_llvm) {
            std.debug.print("Install LLVM to enable LLVM backend:\n", .{});
            const os_tag = target.result.os.tag;
            if (os_tag == .windows) {
                std.debug.print("  -> choco install llvm --version=19.1.7\n", .{});
            } else if (os_tag == .macos) {
                std.debug.print("  -> brew install llvm@19\n", .{});
            } else if (os_tag == .linux) {
                std.debug.print("  -> sudo apt install llvm-19-dev\n", .{});
            }
        }
    }
    
    std.debug.print("\n===========================================\n", .{});
    std.debug.print("   Building pawc compiler...\n", .{});
    std.debug.print("===========================================\n", .{});
    
    // 链接标准库
    exe.linkLibC();

    const install_artifact = b.addInstallArtifact(exe, .{});
    
    // 🆕 Cross-platform: Auto-copy LLVM libraries to output directory (for distribution)
    if (has_llvm) {
        const copy_libs_step = switch (target.result.os.tag) {
            .windows => blk: {
                // Windows: Copy all DLL files
                const cmd = b.addSystemCommand(&[_][]const u8{
                    "powershell", "-Command",
                    "Copy-Item 'C:\\Program Files\\LLVM\\bin\\*.dll' 'zig-out\\bin\\' -ErrorAction SilentlyContinue; if ($?) { Write-Host '✅ Copied LLVM DLLs' }",
                });
                cmd.step.dependOn(&install_artifact.step);
                std.debug.print("\n[Windows] LLVM DLLs will be auto-copied to output directory\n", .{});
                break :blk cmd;
            },
            .macos => blk: {
                // macOS: Copy LLVM dylib files and fix paths for portability
                const llvm_lib_path = if (target.result.cpu.arch == .aarch64)
                    "/opt/homebrew/opt/llvm@19/lib"
                else
                    "/usr/local/opt/llvm@19/lib";
                
                const cmd = b.addSystemCommand(&[_][]const u8{
                    "sh", "-c",
                    b.fmt("mkdir -p zig-out/lib && " ++
                          // Copy libraries
                          "cp -f {s}/libLLVM-C.dylib zig-out/lib/ 2>/dev/null && " ++
                          "cp -f {s}/libLLVM.dylib zig-out/lib/ 2>/dev/null && " ++
                          // Fix install names for portability
                          "install_name_tool -add_rpath @executable_path/../lib zig-out/bin/pawc 2>/dev/null || true && " ++
                          "install_name_tool -change {s}/libLLVM.dylib @rpath/libLLVM.dylib zig-out/bin/pawc 2>/dev/null || true && " ++
                          "echo '✅ Copied LLVM libraries and fixed paths' || true", 
                          .{llvm_lib_path, llvm_lib_path, llvm_lib_path}),
                });
                cmd.step.dependOn(&install_artifact.step);
                std.debug.print("\n💡 macOS: LLVM libraries will be auto-copied to zig-out/lib/\n", .{});
                std.debug.print("💡 macOS: Binary paths will be fixed for portability\n", .{});
                break :blk cmd;
            },
            .linux => blk: {
                // Linux: Copy LLVM .so files
                const cmd = b.addSystemCommand(&[_][]const u8{
                    "sh", "-c",
                    "mkdir -p zig-out/lib && cp -f /usr/lib/llvm-19/lib/libLLVM-*.so* zig-out/lib/ 2>/dev/null && echo '✅ Copied LLVM libraries' || cp -f /usr/lib/x86_64-linux-gnu/libLLVM-*.so* zig-out/lib/ 2>/dev/null && echo '✅ Copied LLVM libraries' || true",
                });
                cmd.step.dependOn(&install_artifact.step);
                std.debug.print("\n💡 Linux: LLVM libraries will be auto-copied to zig-out/lib/\n", .{});
                break :blk cmd;
            },
            else => null,
        };
        
        if (copy_libs_step) |step| {
            b.getInstallStep().dependOn(&step.step);
        } else {
            b.getInstallStep().dependOn(&install_artifact.step);
        }
    } else {
        // 无LLVM: 只添加artifact安装
        b.getInstallStep().dependOn(&install_artifact.step);
    }

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run Paw compiler");
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
    
    // 🆕 Help command for distribution
    const help_dist = b.step("help-dist", "Show distribution packaging instructions");
    const help_cmd = b.addSystemCommand(&[_][]const u8{
        "sh", "-c",
        "echo ''; echo '📦 PawLang Distribution Commands:'; echo ''; " ++
        "echo '  zig build dist    - Prepare distribution files'; " ++
        "echo '  zig build package - Create platform-specific archive'; " ++
        "echo ''; echo '📋 Output:'; " ++
        "echo '  • Windows: pawlang-windows.zip (~200MB with all DLLs)'; " ++
        "echo '  • macOS:   pawlang-macos.tar.gz (~1MB compressed)'; " ++
        "echo '  • Linux:   pawlang-linux.tar.gz (~1MB compressed)'; " ++
        "echo ''; echo '✨ All packages include:'; " ++
        "echo '  • Compiler executable'; " ++
        "echo '  • LLVM libraries (bundled)'; " ++
        "echo '  • Launcher script'; " ++
        "echo '  • Examples + Documentation'; " ++
        "echo '  • Both C and LLVM backends ready to use'; " ++
        "echo ''",
    });
    help_dist.dependOn(&help_cmd.step);
    
    // 🆕 Create distribution package step
    const dist_step = b.step("dist", "Create distribution package with all dependencies");
    
    if (has_llvm) {
        // Copy examples, documentation and launcher scripts for distribution
        const copy_dist_files = switch (target.result.os.tag) {
            .windows => b.addSystemCommand(&[_][]const u8{
                "powershell", "-Command",
                "if (Test-Path zig-out\\examples) { Remove-Item -Recurse zig-out\\examples }; " ++
                "Copy-Item -Recurse examples zig-out\\examples; " ++
                "Copy-Item README.md zig-out\\README.md -Force; " ++
                "Copy-Item USAGE.md zig-out\\USAGE.md -Force; " ++
                "Copy-Item LICENSE zig-out\\LICENSE -Force -ErrorAction SilentlyContinue; " ++
                "Copy-Item scripts\\pawc.bat zig-out\\pawc.bat -Force; " ++
                "Write-Host '✅ Copied examples, docs and launcher script'",
            }),
            else => b.addSystemCommand(&[_][]const u8{
                "sh", "-c",
                "rm -rf zig-out/examples && cp -r examples zig-out/examples && " ++
                "cp README.md zig-out/README.md && " ++
                "cp USAGE.md zig-out/USAGE.md && " ++
                "cp LICENSE zig-out/LICENSE 2>/dev/null || true && " ++
                "cp scripts/pawc.sh zig-out/pawc && chmod +x zig-out/pawc && " ++
                "echo '✅ Copied examples, docs and launcher script'",
            }),
        };
        copy_dist_files.step.dependOn(b.getInstallStep());
        
        // Platform-specific info display
        const package_cmd = switch (target.result.os.tag) {
            .windows => b.addSystemCommand(&[_][]const u8{
                "powershell", "-Command", 
                "Write-Host ''; Write-Host '📦 Windows Distribution Package Prepared'; " ++
                "Write-Host ''; Write-Host 'Contents:'; " ++
                "Write-Host '  • bin/pawc.exe (compiler)'; " ++
                "Write-Host '  • bin/*.dll (LLVM libraries ~200MB)'; " ++
                "Write-Host '  • pawc.bat (launcher script)'; " ++
                "Write-Host '  • examples/ + documentation'; " ++
                "Write-Host ''; " ++
                "$dllCount = (Get-ChildItem zig-out\\bin\\*.dll | Measure-Object).Count; " ++
                "Write-Host \"  ✅ Total: $dllCount DLL files bundled\"; " ++
                "Write-Host ''; Write-Host '📂 Location: zig-out\\'; " ++
                "Write-Host ''; Write-Host '🚀 Users run: pawc.bat file.paw'; " ++
                "Write-Host '   (Both C and LLVM backends work out-of-the-box!)'; " ++
                "Write-Host ''; Write-Host '💡 Next: Run \"zig build package\" to create .zip'",
            }),
            .macos => b.addSystemCommand(&[_][]const u8{
                "sh", "-c",
                "echo ''; echo '📦 macOS Distribution Package Prepared'; " ++
                "echo ''; echo 'Contents:'; " ++
                "echo '  • bin/pawc (compiler executable)'; " ++
                "echo '  • lib/libLLVM-C.dylib (LLVM library)'; " ++
                "echo '  • pawc (launcher script with auto-path setup)'; " ++
                "echo '  • examples/ + documentation'; " ++
                "echo ''; echo '📂 Location: zig-out/'; " ++
                "echo ''; echo '🚀 Users run: ./pawc file.paw'; " ++
                "echo '   (Both C and LLVM backends work automatically!)'; " ++
                "echo ''; echo '💡 Next: Run \"zig build package\" to create .tar.gz'",
            }),
            .linux => b.addSystemCommand(&[_][]const u8{
                "sh", "-c",
                "echo ''; echo '📦 Linux Distribution Package Prepared'; " ++
                "echo ''; echo 'Contents:'; " ++
                "echo '  • bin/pawc (compiler executable)'; " ++
                "echo '  • lib/*.so (LLVM libraries)'; " ++
                "echo '  • pawc (launcher script with auto-path setup)'; " ++
                "echo '  • examples/ + documentation'; " ++
                "echo ''; echo '📂 Location: zig-out/'; " ++
                "echo ''; echo '🚀 Users run: ./pawc file.paw'; " ++
                "echo '   (Both C and LLVM backends work automatically!)'; " ++
                "echo ''; echo '💡 Next: Run \"zig build package\" to create .tar.gz'",
            }),
            else => b.addSystemCommand(&[_][]const u8{ "echo", "Platform not supported for dist" }),
        };
        
        package_cmd.step.dependOn(&copy_dist_files.step);
        dist_step.dependOn(&package_cmd.step);
        
        // 🆕 Actual packaging step - Creates ready-to-distribute archive
        const package_step = b.step("package", "Build and package for distribution (creates archive with all dependencies)");
        const create_archive = switch (target.result.os.tag) {
            .windows => b.addSystemCommand(&[_][]const u8{
                "powershell", "-Command",
                "Compress-Archive -Path zig-out\\bin,zig-out\\examples,zig-out\\README.md,zig-out\\USAGE.md,zig-out\\LICENSE,zig-out\\pawc.bat -DestinationPath pawlang-windows.zip -Force -ErrorAction SilentlyContinue; " ++
                "Write-Host ''; Write-Host '✅ Created: pawlang-windows.zip'; " ++
                "$size = (Get-Item pawlang-windows.zip).Length / 1MB; " ++
                "Write-Host \"📦 Size: $([math]::Round($size, 2)) MB\"; " ++
                "Write-Host ''; Write-Host '📋 Complete Package:'; " ++
                "Write-Host '  • pawc.exe + All LLVM DLLs'; " ++
                "Write-Host '  • Launcher script (pawc.bat)'; " ++
                "Write-Host '  • Examples + Documentation'; " ++
                "Write-Host '  • Both C and LLVM backends included'; " ++
                "Write-Host ''; Write-Host '🚀 Users extract and run: pawc.bat hello.paw'; " ++
                "Write-Host '   No installation needed!'"
            }),
            .macos => b.addSystemCommand(&[_][]const u8{
                "sh", "-c",
                "tar -czf pawlang-macos.tar.gz -C zig-out bin lib examples pawc README.md USAGE.md LICENSE 2>/dev/null || " ++
                "tar -czf pawlang-macos.tar.gz -C zig-out bin lib examples pawc README.md USAGE.md; " ++
                "echo ''; echo '✅ Created: pawlang-macos.tar.gz'; " ++
                "ls -lh pawlang-macos.tar.gz | awk '{print \"📦 Size: \" $5}'; " ++
                "echo ''; echo '📋 Complete Package:'; " ++
                "echo '  • pawc + LLVM libraries'; " ++
                "echo '  • Launcher script (auto-sets library path)'; " ++
                "echo '  • Examples + Documentation'; " ++
                "echo '  • Both C and LLVM backends included'; " ++
                "echo ''; echo '🚀 Users extract and run: ./pawc hello.paw'; " ++
                "echo '   No LLVM installation needed!'"
            }),
            .linux => b.addSystemCommand(&[_][]const u8{
                "sh", "-c",
                "tar -czf pawlang-linux.tar.gz -C zig-out bin lib examples pawc README.md USAGE.md LICENSE 2>/dev/null || " ++
                "tar -czf pawlang-linux.tar.gz -C zig-out bin lib examples pawc README.md USAGE.md; " ++
                "echo ''; echo '✅ Created: pawlang-linux.tar.gz'; " ++
                "ls -lh pawlang-linux.tar.gz | awk '{print \"📦 Size: \" $5}'; " ++
                "echo ''; echo '📋 Complete Package:'; " ++
                "echo '  • pawc + LLVM libraries'; " ++
                "echo '  • Launcher script (auto-sets library path)'; " ++
                "echo '  • Examples + Documentation'; " ++
                "echo '  • Both C and LLVM backends included'; " ++
                "echo ''; echo '🚀 Users extract and run: ./pawc hello.paw'; " ++
                "echo '   No LLVM installation needed!'"
            }),
            else => b.addSystemCommand(&[_][]const u8{ "echo", "Platform not supported" }),
        };
        create_archive.step.dependOn(&copy_dist_files.step);
        package_step.dependOn(&create_archive.step);
    } else {
        const no_llvm_info = b.addSystemCommand(&[_][]const u8{
            "sh", "-c",
            "echo ''; echo '📦 Creating lightweight distribution (C backend only)...'; " ++
            "echo '✅ Zero dependencies - single executable!'; " ++
            "echo '📂 Location: zig-out/bin/pawc'; " ++
            "echo ''; echo '💡 Just distribute the pawc executable'",
        });
        no_llvm_info.step.dependOn(b.getInstallStep());
        dist_step.dependOn(&no_llvm_info.step);
    }
}