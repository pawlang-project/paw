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
    c,           // C backend (default, stable)
    llvm,        // LLVM backend (text IR, experimental)
    llvm_native, // LLVM native API (requires -Dwith-llvm=true)
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
                // 🆕 v0.1.4: LLVM后端 (文本IR)
                backend = .llvm;
            } else if (std.mem.eql(u8, arg, "--backend=llvm-native")) {
                // 🆕 v0.1.4: LLVM原生API
                backend = .llvm_native;
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
    defer parser.deinit();
    
    var ast_result = try parser.parse();
    defer ast_result.deinit(allocator);
    
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
                var llvm_codegen = LLVMBackend.init(allocator);
                defer llvm_codegen.deinit();
                break :blk try llvm_codegen.generate(ast);
            },
            .llvm_native => blk: {
                // Check if LLVM native API is available
                const has_llvm = comptime blk_check: {
                    break :blk_check @hasDecl(@This(), "LLVMNativeBackend");
                };
                
                if (!has_llvm) {
                    std.debug.print("❌ LLVM native backend not available\n", .{});
                    std.debug.print("💡 Rebuild with: zig build -Dwith-llvm=true\n", .{});
                    return;
                }
                
                var llvm_native = try LLVMNativeBackend.init(allocator, "pawlang_module");
                defer llvm_native.deinit();
                break :blk try llvm_native.generate(ast);
            },
        };
    
    const total_time = std.time.nanoTimestamp();
    
    // 🆕 显示使用的后端
    if (verbose) {
        const backend_name = switch (backend) {
            .c => "C",
            .llvm => "LLVM (text IR)",
            .llvm_native => "LLVM (native API)",
        };
        std.debug.print("[INFO] Backend: {s}\n", .{backend_name});
    }
    
    // 5. Output based on options
    if (should_compile) {
            // 🆕 v0.1.4: LLVM后端暂不支持直接编译
            if (backend == .llvm or backend == .llvm_native) {
                std.debug.print("❌ Error: LLVM backends do not support --compile/--run yet\n", .{});
                std.debug.print("💡 Use: pawc file.paw --backend=llvm (generates LLVM IR)\n", .{});
                std.debug.print("   Or: pawc file.paw --backend=llvm-native (generates LLVM IR via native API)\n", .{});
                return;
            }
        
        // 编译为可执行文件（仅C后端）
        const output_name = output_file orelse "output";
        
        var tcc_backend = TccBackend.init(allocator);
        
        if (should_run) {
            // 编译并运行
            std.debug.print("🔥 Compiling and running: {s}\n", .{source_file});
            try tcc_backend.compileAndRun(output_code);
        } else {
            // 只编译
            try tcc_backend.compile(output_code, output_name);
        }
        
        if (verbose) {
            std.debug.print("\n✅ Compilation complete: {s} -> {s} ({d:.2}s)\n", .{
                source_file,
                output_name,
                @as(f64, @floatFromInt(total_time - start_time)) / 1_000_000_000.0,
            });
        }
    } else {
            // Generate code (C or LLVM IR)
            const output_name = output_file orelse "output";
            const output_ext = switch (backend) {
                .c => ".c",
                .llvm, .llvm_native => ".ll", // LLVM IR文件扩展名
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
                        std.debug.print("   • Run: ./{s}\n", .{output_name});
                    },
                    .llvm => {
                        std.debug.print("✅ LLVM IR generated (text mode): {s}\n", .{code_filename});
                        std.debug.print("💡 Hints (experimental):\n", .{});
                        std.debug.print("   • Compile: llc {s} -o {s}.s && gcc {s}.s -o {s}\n", 
                            .{ code_filename, output_name, output_name, output_name });
                        std.debug.print("   • Or: clang {s} -o {s}\n", .{ code_filename, output_name });
                    },
                    .llvm_native => {
                        std.debug.print("✅ LLVM IR generated (native API): {s}\n", .{code_filename});
                        std.debug.print("💡 Hints (experimental):\n", .{});
                        std.debug.print("   • Compile: llc {s} -o {s}.s && gcc {s}.s -o {s}\n", 
                            .{ code_filename, output_name, output_name, output_name });
                        std.debug.print("   • Or: clang {s} -o {s}\n", .{ code_filename, output_name });
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
    std.debug.print("  --backend=c      Use C backend (default, stable)\n", .{});
    std.debug.print("  --backend=llvm   Use LLVM backend (experimental) 🆕\n", .{});
    std.debug.print("\n", .{});
    std.debug.print("Examples:\n", .{});
    std.debug.print("  pawc hello.paw                  Generate C code -> output.c\n", .{});
    std.debug.print("  pawc hello.paw --compile        Compile to executable -> output\n", .{});
    std.debug.print("  pawc hello.paw --run            Compile and run\n", .{});
    std.debug.print("  pawc hello.paw --backend=llvm   Generate LLVM IR -> output.ll 🆕\n", .{});
    std.debug.print("  pawc check hello.paw            Type check only\n", .{});
    std.debug.print("  pawc init my_project            Create new project\n", .{});
    std.debug.print("\n", .{});
}

