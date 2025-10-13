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

    // 🆕 仅检测vendor LLVM - 不依赖系统LLVM
    const vendor_llvm_platform = switch (target.result.os.tag) {
        .macos => if (target.result.cpu.arch == .aarch64)
            "macos-aarch64"
        else
            "macos-x86_64",
        .linux => switch (target.result.cpu.arch) {
            .x86_64 => "linux-x86_64",
            .aarch64 => "linux-aarch64",
            .arm => "linux-arm",
            .riscv64 => "linux-riscv64",
            else => "linux-x86_64",
        },
        .windows => if (target.result.cpu.arch == .aarch64)
            "windows-aarch64"
        else
            "windows-x86_64",
        else => "unknown",
    };
    
    const vendor_llvm_path = b.fmt("vendor/llvm/{s}/install", .{vendor_llvm_platform});
    
    // 检查vendor LLVM是否存在
    const llvm_config_path = blk: {
        if (!enable_llvm) break :blk null;
        
        // 检查vendor LLVM目录（使用相对路径）
        const llvm_lib_dir = b.fmt("{s}/lib", .{vendor_llvm_path});
        std.fs.cwd().access(llvm_lib_dir, .{}) catch {
            break :blk null;  // vendor LLVM不存在
        };
        
        // vendor LLVM存在
        const llvm_config = b.fmt("{s}/bin/llvm-config", .{vendor_llvm_path});
        break :blk llvm_config;
    };
    
    const has_llvm = llvm_config_path != null;
    
    // 🆕 直接使用vendor路径，不需要llvm-config
    var llvm_include_path: ?[]const u8 = null;
    var llvm_lib_path: ?[]const u8 = null;
    
    if (has_llvm) {
        llvm_include_path = b.fmt("{s}/include", .{vendor_llvm_path});
        llvm_lib_path = b.fmt("{s}/lib", .{vendor_llvm_path});
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
        std.debug.print("Location: {s}\n", .{vendor_llvm_path});
        std.debug.print("Platform: {s}\n", .{vendor_llvm_platform});
        std.debug.print("Version: 21.1.3\n", .{});
        std.debug.print("\n", .{});
        
        std.debug.print("Available Backends:\n", .{});
        std.debug.print("  - C backend    (default) -> --backend=c\n", .{});
        std.debug.print("  - LLVM backend (enabled) -> --backend=llvm\n", .{});
        
        // Add LLVM include and library paths
        if (llvm_include_path) |include_path| {
            exe.addIncludePath(.{ .cwd_relative = include_path });
        }
        
        if (llvm_lib_path) |lib_path| {
            exe.addLibraryPath(.{ .cwd_relative = lib_path });
        }
        
        // Link LLVM libraries (静态链接)
        // 使用静态库避免动态库依赖
        const llvm_libs = [_][]const u8{
            "LLVMCore",
            "LLVMSupport", 
            "LLVMBitReader",
            "LLVMBitWriter",
            "LLVMTarget",
            "LLVMTargetParser",
            "LLVMAArch64CodeGen",
            "LLVMAArch64AsmParser",
            "LLVMAArch64Desc",
            "LLVMAArch64Info",
            "LLVMAArch64Utils",
            "LLVMCodeGen",
            "LLVMScalarOpts",
            "LLVMInstCombine",
            "LLVMTransformUtils",
            "LLVMAnalysis",
            "LLVMObject",
            "LLVMMCParser",
            "LLVMMC",
            "LLVMBinaryFormat",
            "LLVMRemarks",
            "LLVMBitstreamReader",
            "LLVMTextAPI",
            "LLVMProfileData",
            "LLVMSymbolize",
            "LLVMDebugInfoDWARF",
            "LLVMDemangle",
        };
        
        // 链接静态库（必须直接指定 .a 文件，因为 LLVM 包只包含静态库）
        for (llvm_libs) |lib| {
            const lib_path = b.fmt("{s}/lib{s}.a", .{llvm_lib_path.?, lib});
            exe.addObjectFile(b.path(lib_path));
        }
        
        exe.linkLibCpp();
        exe.linkSystemLibrary("z");  // zlib
        exe.linkSystemLibrary("curses");  // ncurses
        
    } else {
        std.debug.print("--- LLVM Configuration ---\n", .{});
        if (!enable_llvm) {
            std.debug.print("Status: LLVM backend disabled by user\n", .{});
            std.debug.print("Tip: Remove -Denable-llvm=false to enable\n", .{});
        } else {
            std.debug.print("Status: LLVM not found in vendor/\n", .{});
            std.debug.print("Platform: {s}\n", .{vendor_llvm_platform});
        }
        std.debug.print("\n", .{});
        
        std.debug.print("Available Backends:\n", .{});
        std.debug.print("  - C backend    (default) -> --backend=c\n", .{});
        std.debug.print("  - LLVM backend (unavailable)\n\n", .{});
        
        if (enable_llvm) {
            std.debug.print("📥 Download LLVM to enable LLVM backend:\n", .{});
            std.debug.print("   ./setup_llvm.sh\n", .{});
            std.debug.print("\n", .{});
            std.debug.print("Or download manually from:\n", .{});
            std.debug.print("   https://github.com/pawlang-project/llvm-build/releases/tag/llvm-21.1.3\n", .{});
        }
    }
    
    std.debug.print("\n===========================================\n", .{});
    std.debug.print("   Building pawc compiler...\n", .{});
    std.debug.print("===========================================\n", .{});
    
    // 链接标准库
    exe.linkLibC();

    const install_artifact = b.addInstallArtifact(exe, .{});
    
    // 🆕 静态链接 LLVM 库，无需复制动态库文件
    if (has_llvm) {
        std.debug.print("\n💡 LLVM libraries are statically linked (no runtime dependencies)\n", .{});
        b.getInstallStep().dependOn(&install_artifact.step);
    } else {
        // 🆕 v0.1.8: 无LLVM时的提示信息
        const info_step = switch (target.result.os.tag) {
            .linux => blk: {
                const cmd = b.addSystemCommand(&[_][]const u8{
                    "sh", "-c",
                    "echo '💡 C backend build: Generated C code can be compiled with any C compiler'; " ++
                    "echo '   Recommended: gcc output.c -o program'",
                });
                cmd.step.dependOn(&install_artifact.step);
                std.debug.print("\n💡 C backend only (LLVM not available)\n", .{});
                std.debug.print("   Users can compile generated C code with: gcc output.c -o program\n", .{});
                break :blk cmd;
            },
            .windows => blk: {
                const cmd = b.addSystemCommand(&[_][]const u8{
                    "powershell", "-Command",
                    "Write-Host '💡 C backend build: Generated C code is portable'; " ++
                    "Write-Host '   Compile with: gcc output.c -o program.exe (MinGW)'; " ++
                    "Write-Host '   Or: cl output.c /Fe:program.exe (MSVC)'",
                });
                cmd.step.dependOn(&install_artifact.step);
                std.debug.print("\n💡 C backend only (LLVM not available)\n", .{});
                std.debug.print("   Users can compile generated C code with GCC/MSVC\n", .{});
                break :blk cmd;
            },
            else => null,
        };
        
        if (info_step) |step| {
            b.getInstallStep().dependOn(&step.step);
        } else {
            b.getInstallStep().dependOn(&install_artifact.step);
        }
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
        // Copy examples and documentation for distribution
        const copy_dist_files = switch (target.result.os.tag) {
            .windows => b.addSystemCommand(&[_][]const u8{
                "powershell", "-Command",
                "if (Test-Path zig-out\\examples) { Remove-Item -Recurse zig-out\\examples }; " ++
                "Copy-Item -Recurse examples zig-out\\examples; " ++
                "Copy-Item README.md zig-out\\README.md -Force; " ++
                "Copy-Item USAGE.md zig-out\\USAGE.md -Force; " ++
                "Copy-Item LICENSE zig-out\\LICENSE -Force -ErrorAction SilentlyContinue; " ++
                "Write-Host '✅ Copied examples and documentation'",
            }),
            else => b.addSystemCommand(&[_][]const u8{
                "sh", "-c",
                "rm -rf zig-out/examples && cp -r examples zig-out/examples && " ++
                "cp README.md zig-out/README.md && " ++
                "cp USAGE.md zig-out/USAGE.md && " ++
                "cp LICENSE zig-out/LICENSE 2>/dev/null || true && " ++
                "echo '✅ Copied examples and documentation'",
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
                "Write-Host '  • examples/ + documentation'; " ++
                "Write-Host ''; " ++
                "$dllCount = (Get-ChildItem zig-out\\bin\\*.dll | Measure-Object).Count; " ++
                "Write-Host \"  ✅ Total: $dllCount DLL files bundled\"; " ++
                "Write-Host ''; Write-Host '📂 Location: zig-out\\'; " ++
                "Write-Host ''; Write-Host '🚀 Users run: bin\\pawc.exe file.paw'; " ++
                "Write-Host '   (Both C and LLVM backends work out-of-the-box!)'; " ++
                "Write-Host ''; Write-Host '💡 Next: Run \"zig build package\" to create .zip'",
            }),
            .macos => b.addSystemCommand(&[_][]const u8{
                "sh", "-c",
                "echo ''; echo '📦 macOS Distribution Package Prepared'; " ++
                "echo ''; echo 'Contents:'; " ++
                "echo '  • bin/pawc (compiler with @rpath fixed)'; " ++
                "echo '  • lib/libLLVM*.dylib (LLVM libraries)'; " ++
                "echo '  • examples/ + documentation'; " ++
                "echo ''; echo '📂 Location: zig-out/'; " ++
                "echo ''; echo '🚀 Users run: ./bin/pawc file.paw'; " ++
                "echo '   (Both C and LLVM backends work automatically!)'; " ++
                "echo ''; echo '💡 Next: Run \"zig build package\" to create .tar.gz'",
            }),
            .linux => b.addSystemCommand(&[_][]const u8{
                "sh", "-c",
                "echo ''; echo '📦 Linux Distribution Package Prepared'; " ++
                "echo ''; echo 'Contents:'; " ++
                "echo '  • bin/pawc (compiler executable)'; " ++
                "echo '  • lib/libLLVM.so* (LLVM libraries)'; " ++
                "echo '  • examples/ + documentation'; " ++
                "echo ''; echo '📂 Location: zig-out/'; " ++
                "echo ''; echo '🚀 Users run: LD_LIBRARY_PATH=lib ./bin/pawc file.paw'; " ++
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
                "Compress-Archive -Path zig-out\\bin,zig-out\\examples,zig-out\\README.md,zig-out\\USAGE.md,zig-out\\LICENSE -DestinationPath pawlang-windows.zip -Force -ErrorAction SilentlyContinue; " ++
                "Write-Host ''; Write-Host '✅ Created: pawlang-windows.zip'; " ++
                "$size = (Get-Item pawlang-windows.zip).Length / 1MB; " ++
                "Write-Host \"📦 Size: $([math]::Round($size, 2)) MB\"; " ++
                "Write-Host ''; Write-Host '📋 Complete Package:'; " ++
                "Write-Host '  • bin/pawc.exe + All LLVM DLLs'; " ++
                "Write-Host '  • Examples + Documentation'; " ++
                "Write-Host '  • Both C and LLVM backends included'; " ++
                "Write-Host ''; Write-Host '🚀 Users extract and run: bin\\pawc.exe hello.paw'; " ++
                "Write-Host '   No installation needed!'"
            }),
            .macos => b.addSystemCommand(&[_][]const u8{
                "sh", "-c",
                "tar -czf pawlang-macos.tar.gz -C zig-out bin lib examples README.md USAGE.md LICENSE 2>/dev/null || " ++
                "tar -czf pawlang-macos.tar.gz -C zig-out bin lib examples README.md USAGE.md; " ++
                "echo ''; echo '✅ Created: pawlang-macos.tar.gz'; " ++
                "ls -lh pawlang-macos.tar.gz | awk '{print \"📦 Size: \" $5}'; " ++
                "echo ''; echo '📋 Complete Package:'; " ++
                "echo '  • bin/pawc + LLVM libraries (with @rpath)'; " ++
                "echo '  • Examples + Documentation'; " ++
                "echo '  • Both C and LLVM backends included'; " ++
                "echo ''; echo '🚀 Users extract and run: ./bin/pawc hello.paw'; " ++
                "echo '   No LLVM installation needed!'"
            }),
            .linux => b.addSystemCommand(&[_][]const u8{
                "sh", "-c",
                "tar -czf pawlang-linux.tar.gz -C zig-out bin lib examples README.md USAGE.md LICENSE 2>/dev/null || " ++
                "tar -czf pawlang-linux.tar.gz -C zig-out bin lib examples README.md USAGE.md; " ++
                "echo ''; echo '✅ Created: pawlang-linux.tar.gz'; " ++
                "ls -lh pawlang-linux.tar.gz | awk '{print \"📦 Size: \" $5}'; " ++
                "echo ''; echo '📋 Complete Package:'; " ++
                "echo '  • bin/pawc + LLVM libraries'; " ++
                "echo '  • Examples + Documentation'; " ++
                "echo '  • Both C and LLVM backends included'; " ++
                "echo ''; echo '🚀 Users extract and run: ./bin/pawc hello.paw'; " ++
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
    
    // 🆕 v0.2.0: LLVM 自动下载步骤
    const setup_llvm_step = b.step("setup-llvm", "Download and setup LLVM for current platform");
    
    const setup_llvm_cmd = switch (target.result.os.tag) {
        .windows => blk: {
            // Windows: 使用 PowerShell 脚本
            const ps_script = b.addSystemCommand(&[_][]const u8{
                "powershell", "-ExecutionPolicy", "Bypass", "-File", "setup_llvm.ps1"
            });
            break :blk ps_script;
        },
        .macos, .linux => blk: {
            // Unix: 使用 bash 脚本
            const sh_script = b.addSystemCommand(&[_][]const u8{
                "sh", "setup_llvm.sh"
            });
            break :blk sh_script;
        },
        else => blk: {
            const error_cmd = b.addSystemCommand(&[_][]const u8{
                "echo", "❌ Platform not supported for automatic LLVM setup"
            });
            break :blk error_cmd;
        },
    };
    
    setup_llvm_step.dependOn(&setup_llvm_cmd.step);
    
    // 🆕 v0.2.0: 检查 LLVM 步骤（如果不存在则提示）
    const check_llvm_step = b.step("check-llvm", "Check if LLVM is installed");
    
    const check_llvm_cmd = switch (target.result.os.tag) {
        .windows => b.addSystemCommand(&[_][]const u8{
            "powershell", "-Command",
            b.fmt(
                "if (Test-Path '{s}\\install\\bin\\clang.exe') {{ " ++
                "Write-Host ''; Write-Host '✅ LLVM is installed' -ForegroundColor Green; " ++
                "Write-Host '   Location: {s}\\install\\' -ForegroundColor Cyan; " ++
                "Write-Host ''; & '{s}\\install\\bin\\clang.exe' --version | Select-Object -First 1; " ++
                "Write-Host ''; Write-Host '🚀 You can use: zig build' -ForegroundColor Green; " ++
                "}} else {{ " ++
                "Write-Host ''; Write-Host '❌ LLVM not found' -ForegroundColor Red; " ++
                "Write-Host ''; Write-Host '📥 To install LLVM, run:' -ForegroundColor Yellow; " ++
                "Write-Host '   zig build setup-llvm'; " ++
                "Write-Host ''; Write-Host '   Or manually:'; " ++
                "Write-Host '   .\\setup_llvm.ps1'; " ++
                "}}",
                .{ vendor_llvm_path, vendor_llvm_path, vendor_llvm_path }
            ),
        }),
        .macos, .linux => b.addSystemCommand(&[_][]const u8{
            "sh", "-c",
            b.fmt(
                "CLANG_PATH='{s}/bin/clang'; " ++
                "if [ -f \"$CLANG_PATH\" ]; then " ++
                "echo ''; echo '✅ LLVM is installed'; " ++
                "echo '   Location: {s}/'; " ++
                "echo ''; \"$CLANG_PATH\" --version | head -1; " ++
                "echo ''; echo '🚀 You can use: zig build'; " ++
                "else " ++
                "echo ''; echo '❌ LLVM not found'; " ++
                "echo '   Expected: '$CLANG_PATH; " ++
                "echo ''; echo '📥 To install LLVM, run:'; " ++
                "echo '   zig build setup-llvm'; " ++
                "echo ''; echo '   Or manually:'; " ++
                "echo '   ./setup_llvm.sh'; " ++
                "fi",
                .{ vendor_llvm_path, vendor_llvm_path }
            ),
        }),
        else => b.addSystemCommand(&[_][]const u8{ "echo", "Platform not supported" }),
    };
    
    check_llvm_step.dependOn(&check_llvm_cmd.step);
}