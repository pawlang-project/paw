const std = @import("std");
const Lexer = @import("lexer.zig").Lexer;
const Parser = @import("parser.zig").Parser;
const TypeChecker = @import("typechecker.zig").TypeChecker;
const CodeGen = @import("codegen.zig").CodeGen;
const LLVMBackend = @import("llvm_backend.zig").LLVMBackend; // 🆕 v0.1.4 - Text mode
const LLVMNativeBackend = @import("llvm_native_backend.zig").LLVMNativeBackend; // 🆕 v0.1.4 - Native API
const TccBackend = @import("tcc_backend.zig").TccBackend;
const ModuleLoader = @import("module.zig").ModuleLoader;
const ast_mod = @import("ast.zig");

const builtin = @import("builtin");

const VERSION = "0.1.4-dev";

// 🆕 v0.1.4: Backend选择
const Backend = enum {
    c,      // C backend (default, stable)
    llvm,   // LLVM backend (auto: native API if available, else text IR)
};

// 🆕 check command: type checking only
fn checkFile(allocator: std.mem.Allocator, source_file: []const u8) !void {
    std.debug.print("🔍 Checking: {s}\n", .{source_file});
    
    const source = std.fs.cwd().readFileAlloc(allocator, source_file, 1024 * 1024) catch |err| {
        std.debug.print("Error: Cannot read file {s}: {any}\n", .{source_file, err});
        return;
    };
    defer allocator.free(source);
    
    // Load standard library
    const prelude_source = @embedFile("std/prelude.paw");
    const combined_source = try std.fmt.allocPrint(allocator, "{s}\n\n{s}", .{prelude_source, source});
    defer allocator.free(combined_source);
    
    // Lexical analysis
    var lexer = Lexer.init(allocator, combined_source);
    defer lexer.deinit();
    const tokens = try lexer.tokenize();
    
    // Parsing
    var parser = Parser.init(allocator, tokens);
    defer parser.deinit();
    const ast = try parser.parse();
    
    // Type checking
    var type_checker = TypeChecker.init(allocator);
    defer type_checker.deinit();
    try type_checker.check(ast);
    
    std.debug.print("✅ Type checking passed!\n", .{});
}

// 🆕 init command: create new project
fn initProject(allocator: std.mem.Allocator, project_name: []const u8) !void {
    std.debug.print("📦 Creating project: {s}\n", .{project_name});
    
    // Create project directory
    std.fs.cwd().makeDir(project_name) catch |err| {
        std.debug.print("Error: Cannot create directory: {any}\n", .{err});
        return;
    };
    
    // Create main.paw
    const main_content =
        \\// {s} - Paw Project
        \\
        \\fn main() -> i32 {{
        \\    println("Hello from {s}!");
        \\    return 0;
        \\}}
        \\
    ;
    
    const main_path = try std.fmt.allocPrint(allocator, "{s}/main.paw", .{project_name});
    defer allocator.free(main_path);
    
    const main_file = try std.fs.cwd().createFile(main_path, .{});
    defer main_file.close();
    
    const formatted_content = try std.fmt.allocPrint(allocator, main_content, .{project_name, project_name});
    defer allocator.free(formatted_content);
    
    try main_file.writeAll(formatted_content);
    
    std.debug.print("✅ Project created successfully!\n", .{});
    std.debug.print("\nNext steps:\n", .{});
    std.debug.print("  cd {s}\n", .{project_name});
    std.debug.print("  pawc main.paw --run\n", .{});
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    if (args.len < 2) {
        printUsage();
        return;
    }

    // 🆕 Handle --version / -v
    if (std.mem.eql(u8, args[1], "--version") or std.mem.eql(u8, args[1], "-v")) {
        std.debug.print("pawc 0.1.1\n", .{});
        std.debug.print("Paw Programming Language Compiler\n", .{});
        std.debug.print("Generics: Functions + Structs\n", .{});
        return;
    }

    // 🆕 Handle --help / -h
    if (std.mem.eql(u8, args[1], "--help") or std.mem.eql(u8, args[1], "-h")) {
        printUsage();
        return;
    }

    // 🆕 Handle check command
    if (std.mem.eql(u8, args[1], "check")) {
        if (args.len < 3) {
            std.debug.print("Error: check command requires a file\n", .{});
            std.debug.print("Usage: pawc check <file.paw>\n", .{});
            return;
        }
        try checkFile(allocator, args[2]);
        return;
    }

    // 🆕 Handle init command
    if (std.mem.eql(u8, args[1], "init")) {
        if (args.len < 3) {
            std.debug.print("Error: init command requires a project name\n", .{});
            std.debug.print("Usage: pawc init <project_name>\n", .{});
            return;
        }
        try initProject(allocator, args[2]);
        return;
    }

    const source_file = args[1];
    var output_file: ?[]const u8 = null;
    var optimize = false;
    var verbose = false;
    var should_run = false;      // 是否运行
    var should_compile = false;  // 是否编译为可执行文件
    var backend = Backend.c;     // 🆕 v0.1.4: 后端选择，默认C后端

    // 解析命令行选项
    var i: usize = 2;
    while (i < args.len) : (i += 1) {
        const arg = args[i];
        if (std.mem.eql(u8, arg, "-o") and i + 1 < args.len) {
            i += 1;
            output_file = args[i];
        } else if (std.mem.eql(u8, arg, "-O")) {
            optimize = true;
        } else if (std.mem.eql(u8, arg, "-v")) {
            verbose = true;
        } else if (std.mem.eql(u8, arg, "--run")) {
            should_run = true;
            should_compile = true;
        } else if (std.mem.eql(u8, arg, "--compile")) {
            should_compile = true;
            } else if (std.mem.eql(u8, arg, "--backend=llvm")) {
                // 🆕 v0.1.4: LLVM后端 (自动选择最佳模式)
                backend = .llvm;
            } else if (std.mem.eql(u8, arg, "--backend=c")) {
                backend = .c;
            }
    }

    // 读取源文件
    const source = try std.fs.cwd().readFileAlloc(allocator, source_file, 10 * 1024 * 1024);
    defer allocator.free(source);

    if (verbose) {
        std.debug.print("Compiling: {s}\n", .{source_file});
    }

    // 编译流程
    const start_time = std.time.nanoTimestamp();

    // 🆕 0. 自动加载标准库 prelude（嵌入到可执行文件中）
    const prelude_source = @embedFile("std/prelude.paw");
    
    // 合并 prelude 和用户代码
    const combined_source = try std.fmt.allocPrint(allocator, "{s}\n\n{s}", .{prelude_source, source});
    defer allocator.free(combined_source);
    
    // 1. Lexical analysis
    var lexer = Lexer.init(allocator, combined_source);
    defer lexer.deinit();
    
    const tokens = try lexer.tokenize();
    if (verbose) {
        const lex_time = std.time.nanoTimestamp();
        std.debug.print("[PERF] Lexical analysis: {d}μs\n", .{@divTrunc(lex_time - start_time, 1000)});
    }

    // 2. Parsing
    var parser = Parser.init(allocator, tokens);
    defer parser.deinit();  // 这会自动释放所有 AST 内存（通过 arena）
    
    const ast_result = try parser.parse();
    // 注意: ast_result 的内存由 parser.arena 管理，不需要单独 deinit
    // AST 会在 parser.deinit() 时自动释放
    
    if (verbose) {
        const parse_time = std.time.nanoTimestamp();
        std.debug.print("[PERF] Parsing: {d}μs\n", .{@divTrunc(parse_time - start_time, 1000)});
    }

    // 2.5. 🆕 处理导入（模块系统）
    var module_loader = ModuleLoader.init(allocator);
    defer module_loader.deinit();
    
    var resolved_declarations = std.ArrayList(ast_mod.TopLevelDecl).init(allocator);
    defer resolved_declarations.deinit();
    
    for (ast_result.declarations) |decl| {
        if (decl == .import_decl) {
            const import_decl = decl.import_decl;
            
            // 🆕 处理单项或多项导入
            switch (import_decl.items) {
                .single => |item_name| {
                    // 单项导入：import math.add;
                    const imported_item = module_loader.getImportedItem(
                        import_decl.module_path,
                        item_name,
                    ) catch |err| {
                        std.debug.print("Error: Failed to import {s}.{s}: {any}\n", 
                            .{import_decl.module_path, item_name, err});
                        continue;
                    };
                    try resolved_declarations.append(imported_item);
                },
                .multiple => |item_names| {
                    // 多项导入：import math.{add, sub, Vec2};
                    for (item_names) |item_name| {
                        const imported_item = module_loader.getImportedItem(
                            import_decl.module_path,
                            item_name,
                        ) catch |err| {
                            std.debug.print("Error: Failed to import {s}.{s}: {any}\n", 
                                .{import_decl.module_path, item_name, err});
                            continue;
                        };
                        try resolved_declarations.append(imported_item);
                    }
                },
            }
            
            // 注意：module_path会在ast_result.deinit()中释放，这里不释放
        } else {
            // 非import声明，直接添加
            try resolved_declarations.append(decl);
        }
    }
    
    // 创建新的AST（包含导入的声明）
    const ast = ast_mod.Program{
        .declarations = try resolved_declarations.toOwnedSlice(),
    };
    defer {
        // 只释放declarations数组，不递归释放内容
        // 因为内容来自ast_result或module_loader，已有自己的生命周期管理
        allocator.free(ast.declarations);
    }
    
    if (verbose) {
        const import_time = std.time.nanoTimestamp();
        std.debug.print("[PERF] Module resolution: {d}μs\n", .{@divTrunc(import_time - start_time, 1000)});
    }

    // 3. Type checking
    var type_checker = TypeChecker.init(allocator);
    defer type_checker.deinit();
    
    try type_checker.check(ast);
    if (verbose) {
        const typecheck_time = std.time.nanoTimestamp();
        std.debug.print("[PERF] Type checking: {d}μs\n", .{@divTrunc(typecheck_time - start_time, 1000)});
    }

        // 4. Code generation - 🆕 v0.1.4: 支持多后端
        const output_code = switch (backend) {
            .c => blk: {
                var codegen = CodeGen.init(allocator);
                defer codegen.deinit();
                break :blk try codegen.generate(ast);
            },
            .llvm => blk: {
                // 🆕 智能选择: 优先使用原生 API，否则使用文本模式
                const has_llvm_native = comptime blk_check: {
                    break :blk_check @hasDecl(@This(), "LLVMNativeBackend");
                };
                
                if (has_llvm_native) {
                    // 使用原生 API (更快，更好的优化)
                    if (verbose) {
                        std.debug.print("[INFO] Using LLVM native API\n", .{});
                    }
                    var llvm_native = try LLVMNativeBackend.init(allocator, "pawlang_module");
                    defer llvm_native.deinit();
                    break :blk try llvm_native.generate(ast);
                } else {
                    // 降级到文本模式 (无需 LLVM)
                    if (verbose) {
                        std.debug.print("[INFO] Using LLVM text mode (native API not available)\n", .{});
                    }
                    var llvm_codegen = LLVMBackend.init(allocator);
                    defer llvm_codegen.deinit();
                    break :blk try llvm_codegen.generate(ast);
                }
            },
        };
    defer allocator.free(output_code);  // 🔧 释放生成的代码（来自codegen/llvm_backend）
    
    const total_time = std.time.nanoTimestamp();
    
    // Backend name already printed in code generation if verbose
    
    // 5. Output based on options
    if (should_compile) {
        const output_name = output_file orelse "output";
        
        // 检查是否有本地 LLVM/Clang
        const local_clang_path = "llvm/install/bin/clang";
        const has_local_clang = blk: {
            std.fs.cwd().access(local_clang_path, .{}) catch {
                break :blk false;
            };
            break :blk true;
        };
        
        if (has_local_clang and backend == .c) {
            // 使用本地 Clang 编译 C 代码
            if (verbose) {
                std.debug.print("🔨 Using local Clang for compilation\n", .{});
            }
            
            // 写入 C 代码到临时文件
            const temp_c_file = try std.fmt.allocPrint(allocator, "{s}.c", .{output_name});
            defer allocator.free(temp_c_file);
            
            const c_file = try std.fs.cwd().createFile(temp_c_file, .{});
            defer c_file.close();
            try c_file.writeAll(output_code);
            
            // 使用本地 Clang 编译
            // 需要指定 SDK 路径 (macOS)
            var clang_args = std.ArrayList([]const u8).init(allocator);
            defer clang_args.deinit();
            
            try clang_args.append(local_clang_path);
            try clang_args.append(temp_c_file);
            try clang_args.append("-o");
            try clang_args.append(output_name);
            try clang_args.append("-O2");
            
            // macOS: 添加 SDK 路径
            if (builtin.os.tag == .macos) {
                try clang_args.append("-isysroot");
                try clang_args.append("/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk");
            }
            
            var child = std.process.Child.init(clang_args.items, allocator);
            
            const result = try child.spawnAndWait();
            
            if (result != .Exited or result.Exited != 0) {
                std.debug.print("❌ Compilation failed\n", .{});
                return;
            }
            
            if (verbose) {
                std.debug.print("✅ Compilation complete: {s} -> {s}\n", .{ source_file, output_name });
            }
            
            // 如果需要运行
            if (should_run) {
                if (verbose) {
                    std.debug.print("🔥 Running: {s}\n", .{output_name});
                }
                
                const run_path = try std.fmt.allocPrint(allocator, "./{s}", .{output_name});
                defer allocator.free(run_path);
                
                var run_child = std.process.Child.init(&[_][]const u8{run_path}, allocator);
                const run_result = try run_child.spawnAndWait();
                
                if (verbose) {
                    std.debug.print("Exit code: {any}\n", .{run_result});
                }
            }
            
            // 清理临时文件
            if (!verbose) {
                std.fs.cwd().deleteFile(temp_c_file) catch {};
            }
            
        } else if (backend == .llvm) {
            // LLVM 后端: 生成 IR 然后用 Clang 编译
            std.debug.print("❌ Error: LLVM backend does not support --compile/--run yet\n", .{});
            std.debug.print("💡 Use manual workflow:\n", .{});
            std.debug.print("   pawc file.paw --backend=llvm\n", .{});
            std.debug.print("   llvm/install/bin/clang output.ll -o program\n", .{});
            return;
        } else {
            // 回退到 TCC（如果没有本地 Clang）
            if (verbose) {
                std.debug.print("🔨 Using TCC for compilation\n", .{});
            }
            
            var tcc_backend = TccBackend.init(allocator);
            
            if (should_run) {
                std.debug.print("🔥 Compiling and running: {s}\n", .{source_file});
                try tcc_backend.compileAndRun(output_code);
            } else {
                try tcc_backend.compile(output_code, output_name);
            }
            
            if (verbose) {
                std.debug.print("\n✅ Compilation complete: {s} -> {s} ({d:.2}s)\n", .{
                    source_file,
                    output_name,
                    @as(f64, @floatFromInt(total_time - start_time)) / 1_000_000_000.0,
                });
            }
        }
    } else {
            // Generate code (C or LLVM IR)
            const output_name = output_file orelse "output";
            const output_ext = switch (backend) {
                .c => ".c",
                .llvm => ".ll", // LLVM IR文件扩展名
            };
        const code_filename = try std.fmt.allocPrint(allocator, "{s}{s}", .{output_name, output_ext});
        defer allocator.free(code_filename);
        
        const code_file = std.fs.cwd().createFile(code_filename, .{}) catch |err| {
            std.debug.print("❌ Failed to create file {s}: {}\n", .{ code_filename, err });
            return;
        };
        defer code_file.close();
        
        try code_file.writeAll(output_code);
        
        if (verbose) {
            std.debug.print("Compilation complete: {s} -> {s} ({d:.2}s)\n", .{
                source_file,
                code_filename,
                @as(f64, @floatFromInt(total_time - start_time)) / 1_000_000_000.0,
            });
            
                switch (backend) {
                    .c => {
                        std.debug.print("✅ C code generated: {s}\n", .{code_filename});
                        std.debug.print("💡 Hints:\n", .{});
                        std.debug.print("   • Compile: gcc {s} -o {s}\n", .{ code_filename, output_name });
                        std.debug.print("   • Or with local Clang: llvm/install/bin/clang {s} -o {s}\n", .{ code_filename, output_name });
                        std.debug.print("   • Run: ./{s}\n", .{output_name});
                    },
                    .llvm => {
                        std.debug.print("✅ LLVM IR generated: {s}\n", .{code_filename});
                        std.debug.print("💡 Hints:\n", .{});
                        std.debug.print("   • Compile: llvm/install/bin/clang {s} -o {s}\n", .{ code_filename, output_name });
                        std.debug.print("   • Or: clang {s} -o {s} (system clang)\n", .{ code_filename, output_name });
                        std.debug.print("   • Run: ./{s}\n", .{output_name});
                    },
                }
        } else {
            std.debug.print("✅ {s} -> {s}\n", .{source_file, code_filename});
        }
    }
}

fn printUsage() void {
    std.debug.print("\n", .{});
    std.debug.print("╔═══════════════════════════════════════════════════════════════╗\n", .{});
    std.debug.print("║        pawc - Paw Language Compiler v0.1.4-dev               ║\n", .{});
    std.debug.print("╚═══════════════════════════════════════════════════════════════╝\n", .{});
    std.debug.print("\n", .{});
    std.debug.print("Usage:\n", .{});
    std.debug.print("  pawc <file.paw> [options]       Compile Paw source file\n", .{});
    std.debug.print("  pawc check <file>               Type check only\n", .{});
    std.debug.print("  pawc init <name>                Create new project\n", .{});
    std.debug.print("  pawc --version, -v              Show version\n", .{});
    std.debug.print("  pawc --help, -h                 Show this help\n", .{});
    std.debug.print("\n", .{});
    std.debug.print("Options:\n", .{});
    std.debug.print("  -o <file>        Specify output file name\n", .{});
    std.debug.print("  -O               Enable optimization (not implemented)\n", .{});
    std.debug.print("  -v               Verbose output\n", .{});
    std.debug.print("  --compile        Compile to executable (C backend only)\n", .{});
    std.debug.print("  --run            Compile and run immediately (C backend only)\n", .{});
    std.debug.print("\n", .{});
    std.debug.print("Backends:\n", .{});
    std.debug.print("  --backend=c              Use C backend (default, stable)\n", .{});
    std.debug.print("  --backend=llvm           Use LLVM backend (auto: native API or text) 🆕\n", .{});
    std.debug.print("\n", .{});
    std.debug.print("Examples:\n", .{});
    std.debug.print("  pawc hello.paw                       Generate C code -> output.c\n", .{});
    std.debug.print("  pawc hello.paw --compile             Compile to executable -> output\n", .{});
    std.debug.print("  pawc hello.paw --run                 Compile and run\n", .{});
    std.debug.print("  pawc hello.paw --backend=llvm        Generate LLVM IR -> output.ll 🚀\n", .{});
    std.debug.print("  pawc check hello.paw                 Type check only\n", .{});
    std.debug.print("  pawc init my_project                 Create new project\n", .{});
    std.debug.print("\n", .{});
    std.debug.print("Build with LLVM:\n", .{});
    std.debug.print("  zig build                            Auto-detect and use LLVM if available\n", .{});
    std.debug.print("  zig build run-llvm                   Quick test with LLVM backend\n", .{});
    std.debug.print("\n", .{});
}

