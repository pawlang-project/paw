const std = @import("std");
const Lexer = @import("lexer.zig").Lexer;
const Parser = @import("parser.zig").Parser;
const TypeChecker = @import("typechecker.zig").TypeChecker;
const CodeGen = @import("codegen.zig").CodeGen;
const CBackend = @import("c_backend.zig").CBackend;
const ModuleLoader = @import("module.zig").ModuleLoader;
const ast_mod = @import("ast.zig");
const REPL = @import("repl.zig").REPL;  // 🆕 v0.1.9

const builtin = @import("builtin");
const build_options = @import("build_options");

// 🆕 v0.1.4: LLVM backend (条件编译)
const llvm_available = build_options.llvm_native_available;
const llvm_backend = if (llvm_available) @import("llvm_native_backend.zig") else struct {
    pub const LLVMNativeBackend = void;
    pub const OptLevel = enum { O0, O1, O2, O3 };
};
const LLVMNativeBackend = llvm_backend.LLVMNativeBackend;
const LLVMOptLevel = llvm_backend.OptLevel; // 🆕 v0.1.7

const VERSION = "0.1.9-dev";

// 🆕 v0.1.9: 编译时间分析
const CompilationTimer = struct {
    total_start: i64,
    lexer_time: i64 = 0,
    parser_time: i64 = 0,
    typecheck_time: i64 = 0,
    codegen_time: i64 = 0,
    
    pub fn init() CompilationTimer {
        return CompilationTimer{
            .total_start = std.time.milliTimestamp(),
        };
    }
    
    pub fn printStats(self: CompilationTimer) void {
        const total_time = std.time.milliTimestamp() - self.total_start;
        
        std.debug.print("\n", .{});
        std.debug.print("╭─────────────────────────────────────╮\n", .{});
        std.debug.print("│  ⏱️  Compilation Time Analysis      │\n", .{});
        std.debug.print("├─────────────────────────────────────┤\n", .{});
        std.debug.print("│  Lexer:        {d:>6}ms  ({d:>5.1}%)  │\n", .{
            self.lexer_time,
            @as(f64, @floatFromInt(self.lexer_time)) / @as(f64, @floatFromInt(total_time)) * 100.0,
        });
        std.debug.print("│  Parser:       {d:>6}ms  ({d:>5.1}%)  │\n", .{
            self.parser_time,
            @as(f64, @floatFromInt(self.parser_time)) / @as(f64, @floatFromInt(total_time)) * 100.0,
        });
        std.debug.print("│  Type Check:   {d:>6}ms  ({d:>5.1}%)  │\n", .{
            self.typecheck_time,
            @as(f64, @floatFromInt(self.typecheck_time)) / @as(f64, @floatFromInt(total_time)) * 100.0,
        });
        std.debug.print("│  Code Gen:     {d:>6}ms  ({d:>5.1}%)  │\n", .{
            self.codegen_time,
            @as(f64, @floatFromInt(self.codegen_time)) / @as(f64, @floatFromInt(total_time)) * 100.0,
        });
        std.debug.print("├─────────────────────────────────────┤\n", .{});
        std.debug.print("│  Total:        {d:>6}ms  (100.0%)  │\n", .{total_time});
        std.debug.print("╰─────────────────────────────────────╯\n", .{});
        std.debug.print("\n", .{});
    }
};

// 🆕 v0.1.4: Simplified backend selection
const Backend = enum {
    c,      // C backend (default, stable)
    llvm,   // LLVM native backend
};

// 🆕 v0.1.7: LLVM optimization levels
const OptLevel = enum {
    O0,  // No optimization (debugging)
    O1,  // Basic optimization
    O2,  // Standard optimization (recommended)
    O3,  // Aggressive optimization
    
    pub fn fromString(s: []const u8) ?OptLevel {
        if (std.mem.eql(u8, s, "-O0")) return .O0;
        if (std.mem.eql(u8, s, "-O1")) return .O1;
        if (std.mem.eql(u8, s, "-O2")) return .O2;
        if (std.mem.eql(u8, s, "-O3")) return .O3;
        return null;
    }
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
    const prelude_source = @embedFile("prelude/prelude.paw");
    const combined_source = try std.fmt.allocPrint(allocator, "{s}\n\n{s}", .{prelude_source, source});
    defer allocator.free(combined_source);
    
    // Lexical analysis
    var lexer = Lexer.init(allocator, combined_source, source_file);
    defer lexer.deinit();
    const tokens = try lexer.tokenize();
    
    // Parsing
    var parser = Parser.init(allocator, tokens);
    defer parser.deinit();
    const ast = try parser.parse();
    
    // Type checking
    var type_checker = TypeChecker.init(allocator, source_file, tokens);
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
        std.debug.print("pawc 0.1.9\n", .{});
        std.debug.print("Paw Programming Language Compiler\n", .{});
        std.debug.print("🆕 New in v0.1.9: Enhanced errors, --time analysis, VSCode support, REPL\n", .{});
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
    
    // 🆕 v0.1.9: Handle repl command
    if (std.mem.eql(u8, args[1], "repl")) {
        var repl = REPL.init(allocator);
        defer repl.deinit();
        try repl.run();
        return;
    }

    const source_file = args[1];
    var output_file: ?[]const u8 = null;
    var optimize = false;
    var verbose = false;
    var should_run = false;      // 是否运行
    var should_compile = false;  // 是否编译为可执行文件
    var backend: ?Backend = null;     // 🆕 v0.1.8: 后端选择，null = 自动检测
    var opt_level: ?OptLevel = null;  // 🆕 v0.1.7: LLVM 优化级别
    var show_timing = false;          // 🆕 v0.1.9: 显示编译时间分析

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
        } else if (std.mem.eql(u8, arg, "--time")) {
            show_timing = true;  // 🆕 v0.1.9: 显示编译时间分析
        } else if (std.mem.eql(u8, arg, "--run")) {
            should_run = true;
            should_compile = true;
        } else if (std.mem.eql(u8, arg, "--compile")) {
            should_compile = true;
        } else if (std.mem.eql(u8, arg, "--backend=llvm")) {
            // 🆕 v0.1.4: LLVM后端 (自动选择最佳模式)
            if (!llvm_available) {
                std.debug.print("❌ Error: LLVM backend not available in this build\n", .{});
                std.debug.print("💡 Rebuild with: zig build (LLVM auto-detected)\n", .{});
                std.debug.print("   Or use C backend: --backend=c\n", .{});
                return;
            }
            backend = .llvm;
        } else if (std.mem.eql(u8, arg, "--backend=c")) {
            backend = .c;
        } else if (std.mem.eql(u8, arg, "--backend=auto")) {
            // 🆕 v0.1.8: 显式自动检测
            backend = null;
        } else if (OptLevel.fromString(arg)) |level| {
            // 🆕 v0.1.7: 优化级别 (-O0, -O1, -O2, -O3)
            opt_level = level;
            // 优化标志检查会在后端确定后进行
        }
    }
    
    // 🆕 v0.1.8: 自动检测后端 (如果用户未指定)
    if (backend == null) {
        if (llvm_available) {
            backend = .llvm;
            if (verbose) {
                std.debug.print("🚀 Auto-selected LLVM backend (available and optimal)\n", .{});
            }
        } else {
            backend = .c;
            if (verbose) {
                std.debug.print("📝 Auto-selected C backend (LLVM not available)\n", .{});
            }
        }
    }
    
    const selected_backend = backend.?; // 现在肯定有值了
    
    // 🆕 v0.1.8: 检查优化标志是否适用于当前后端
    if (opt_level != null and selected_backend != .llvm) {
        std.debug.print("⚠️  Warning: Optimization flags (-O0/-O1/-O2/-O3) only work with LLVM backend\n", .{});
        std.debug.print("💡 Tip: Remove optimization flag or use --backend=llvm\n", .{});
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
    const prelude_source = @embedFile("prelude/prelude.paw");
    
    // 🆕 v0.1.8: 计算 prelude 行数用于行号偏移
    var prelude_lines: usize = 0;
    for (prelude_source) |c| {
        if (c == '\n') prelude_lines += 1;
    }
    prelude_lines += 2;  // 加上分隔的两个换行符
    
    // 合并 prelude 和用户代码
    const combined_source = try std.fmt.allocPrint(allocator, "{s}\n\n{s}", .{prelude_source, source});
    defer allocator.free(combined_source);
    
    // 🆕 v0.1.9: 初始化编译时间分析器
    var timer = if (show_timing) CompilationTimer.init() else undefined;
    
    // 1. Lexical analysis
    const lexer_start = std.time.milliTimestamp();
    var lexer = Lexer.init(allocator, combined_source, source_file);
    lexer.setLineOffset(prelude_lines);  // 🆕 v0.1.8: 设置行号偏移
    defer lexer.deinit();
    
    const tokens = try lexer.tokenize();
    if (show_timing) {
        timer.lexer_time = std.time.milliTimestamp() - lexer_start;
    }
    if (verbose) {
        const lex_time = std.time.nanoTimestamp();
        std.debug.print("[PERF] Lexical analysis: {d}μs\n", .{@divTrunc(lex_time - start_time, 1000)});
    }

    // 2. Parsing
    const parser_start = std.time.milliTimestamp();
    var parser = Parser.init(allocator, tokens);
    defer parser.deinit();  // 这会自动释放所有 AST 内存（通过 arena）
    
    const ast_result = try parser.parse();
    if (show_timing) {
        timer.parser_time = std.time.milliTimestamp() - parser_start;
    }
    // 注意: ast_result 的内存由 parser.arena 管理，不需要单独 deinit
    // AST 会在 parser.deinit() 时自动释放
    
    if (verbose) {
        const parse_time = std.time.nanoTimestamp();
        std.debug.print("[PERF] Parsing: {d}μs\n", .{@divTrunc(parse_time - start_time, 1000)});
    }

    // 2.5. 🆕 处理导入（模块系统）
    var module_loader = ModuleLoader.init(allocator);
    defer module_loader.deinit();
    
    var resolved_declarations = std.ArrayList(ast_mod.TopLevelDecl){};
    defer resolved_declarations.deinit(allocator);
    
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
                    try resolved_declarations.append(allocator, imported_item);
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
                        try resolved_declarations.append(allocator, imported_item);
                    }
                },
            }
            
            // 注意：module_path会在ast_result.deinit()中释放，这里不释放
        } else {
            // 非import声明，直接添加
            try resolved_declarations.append(allocator, decl);
        }
    }
    
    // 创建新的AST（包含导入的声明）
    const ast = ast_mod.Program{
        .declarations = try resolved_declarations.toOwnedSlice(allocator),
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
    const typecheck_start = std.time.milliTimestamp();
    var type_checker = TypeChecker.init(allocator, source_file, tokens);
    defer type_checker.deinit();
    
    try type_checker.check(ast);
    if (show_timing) {
        timer.typecheck_time = std.time.milliTimestamp() - typecheck_start;
    }
    if (verbose) {
        const typecheck_time = std.time.nanoTimestamp();
        std.debug.print("[PERF] Type checking: {d}μs\n", .{@divTrunc(typecheck_time - start_time, 1000)});
    }

        // 4. Code generation - 🆕 v0.1.4: 双后端架构 (C + LLVM Native)
        const codegen_start = std.time.milliTimestamp();
        const output_code = switch (selected_backend) {
            .c => blk: {
                var codegen = CodeGen.init(allocator);
                defer codegen.deinit();
                break :blk try codegen.generate(ast);
            },
            .llvm => blk: {
                if (!llvm_available) {
                    std.debug.print("❌ Error: LLVM backend not available in this build\n", .{});
                    return error.LLVMNotAvailable;
                }
                
                // Use LLVM native API
                if (verbose) {
                    std.debug.print("[INFO] Using LLVM native backend\n", .{});
                }
                // 🆕 v0.1.7: 传递优化级别，默认 O0
                const llvm_opt_level: LLVMOptLevel = if (opt_level) |level| switch (level) {
                    .O0 => .O0,
                    .O1 => .O1,
                    .O2 => .O2,
                    .O3 => .O3,
                } else .O0;
                
                var llvm_native = try LLVMNativeBackend.init(allocator, "pawlang_module", llvm_opt_level);
                defer llvm_native.deinit();
                break :blk try llvm_native.generate(ast);
            },
        };
    defer allocator.free(output_code);  // 🔧 释放生成的代码（来自 codegen 或 llvm_native_backend）
    
    if (show_timing) {
        timer.codegen_time = std.time.milliTimestamp() - codegen_start;
    }
    
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
        
        if (has_local_clang and selected_backend == .c) {
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
            var clang_args = std.ArrayList([]const u8){};
            defer clang_args.deinit(allocator);
            
            try clang_args.append(allocator, local_clang_path);
            try clang_args.append(allocator, temp_c_file);
            try clang_args.append(allocator, "-o");
            try clang_args.append(allocator, output_name);
            try clang_args.append(allocator, "-O2");
            
            // macOS: 添加 SDK 路径
            if (builtin.os.tag == .macos) {
                try clang_args.append(allocator, "-isysroot");
                try clang_args.append(allocator, "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk");
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
            
        } else if (selected_backend == .llvm) {
            // LLVM 后端: 生成 IR 然后用 Clang 编译
            std.debug.print("❌ Error: LLVM backend does not support --compile/--run yet\n", .{});
            std.debug.print("💡 Use manual workflow:\n", .{});
            std.debug.print("   pawc file.paw --backend=llvm\n", .{});
            std.debug.print("   llvm/install/bin/clang output.ll -o program\n", .{});
            return;
        } else {
            // Fallback to system C compiler
            if (verbose) {
                std.debug.print("🔨 Using system C compiler\n", .{});
            }
            
            var c_backend = CBackend.init(allocator);
            
            if (should_run) {
                std.debug.print("🔥 Compiling and running: {s}\n", .{source_file});
                try c_backend.compileAndRun(output_code);
            } else {
                try c_backend.compile(output_code, output_name);
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
            const output_ext = switch (selected_backend) {
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
            
                switch (selected_backend) {
                    .c => {
                        std.debug.print("✅ C code generated: {s}\n", .{code_filename});
                        std.debug.print("💡 Hints:\n", .{});
                        std.debug.print("   • Compile: gcc {s} -o {s}\n", .{ code_filename, output_name });
                        std.debug.print("   • Or with local Clang: llvm/install/bin/clang {s} -o {s}\n", .{ code_filename, output_name });
                        std.debug.print("   • Run: ./{s}\n", .{output_name});
                    },
                    .llvm => {
                        std.debug.print("✅ LLVM IR generated: {s}\n", .{code_filename});
                        
                        // 🆕 v0.1.7: 显示优化级别信息
                        if (opt_level) |level| {
                            const opt_str = switch (level) {
                                .O0 => "-O0 (no optimization)",
                                .O1 => "-O1 (basic optimization)",
                                .O2 => "-O2 (standard optimization) ⭐",
                                .O3 => "-O3 (aggressive optimization)",
                            };
                            std.debug.print("⚡ Optimization: {s}\n", .{opt_str});
                        }
                        
                        std.debug.print("💡 Hints:\n", .{});
                        
                        // 🆕 v0.1.7: 根据优化级别提供不同的编译建议
                        if (opt_level) |level| {
                            const clang_opt = switch (level) {
                                .O0 => "-O0",
                                .O1 => "-O1",
                                .O2 => "-O2",
                                .O3 => "-O3",
                            };
                            std.debug.print("   • Compile with optimization: clang {s} {s} -o {s}\n", .{ code_filename, clang_opt, output_name });
                            std.debug.print("   • Local LLVM: llvm/install/bin/clang {s} {s} -o {s}\n", .{ code_filename, clang_opt, output_name });
                        } else {
                            std.debug.print("   • Compile: llvm/install/bin/clang {s} -o {s}\n", .{ code_filename, output_name });
                            std.debug.print("   • Or: clang {s} -o {s} (system clang)\n", .{ code_filename, output_name });
                        }
                        std.debug.print("   • Run: ./{s}\n", .{output_name});
                    },
                }
        } else {
            std.debug.print("✅ {s} -> {s}\n", .{source_file, code_filename});
        }
        
        // 🆕 v0.1.9: 显示编译时间分析
        if (show_timing) {
            timer.printStats();
        }
    }
}

fn printUsage() void {
    std.debug.print("\n", .{});
    std.debug.print("╔═══════════════════════════════════════════════════════════════╗\n", .{});
    std.debug.print("║        pawc - Paw Language Compiler v0.1.9                   ║\n", .{});
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
    std.debug.print("  -v               Verbose output\n", .{});
    std.debug.print("  --time           Show compilation time analysis 🆕\n", .{});
    std.debug.print("  --compile        Compile to executable (C backend only)\n", .{});
    std.debug.print("  --run            Compile and run immediately (C backend only)\n", .{});
    std.debug.print("\n", .{});
    std.debug.print("Backends:\n", .{});
    std.debug.print("  --backend=c              Use C backend\n", .{});
    std.debug.print("  --backend=llvm           Use LLVM native backend\n", .{});
    std.debug.print("  --backend=auto           Auto-detect best backend (default) 🆕\n", .{});
    std.debug.print("\n", .{});
    std.debug.print("LLVM Optimization (v0.1.7) 🆕:\n", .{});
    std.debug.print("  -O0              No optimization (fastest compile, debugging)\n", .{});
    std.debug.print("  -O1              Basic optimization (balanced)\n", .{});
    std.debug.print("  -O2              Standard optimization (recommended) ⭐\n", .{});
    std.debug.print("  -O3              Aggressive optimization (maximum performance)\n", .{});
    std.debug.print("\n", .{});
    std.debug.print("Examples:\n", .{});
    std.debug.print("  pawc hello.paw                       Auto-detect backend (LLVM if available) 🆕\n", .{});
    std.debug.print("  pawc hello.paw --compile             Compile to executable\n", .{});
    std.debug.print("  pawc hello.paw --run                 Compile and run\n", .{});
    std.debug.print("  pawc hello.paw --backend=c           Force C backend\n", .{});
    std.debug.print("  pawc hello.paw --backend=llvm        Force LLVM backend\n", .{});
    std.debug.print("  pawc hello.paw -O2                   Auto-detect + optimization ⚡\n", .{});
    std.debug.print("  pawc fibonacci.paw -O3               Auto-detect + max optimization 🚀\n", .{});
    std.debug.print("  pawc check hello.paw                 Type check only\n", .{});
    std.debug.print("  pawc init my_project                 Create new project\n", .{});
    std.debug.print("\n", .{});
    std.debug.print("Build with LLVM:\n", .{});
    std.debug.print("  zig build                            Auto-detect and use LLVM if available\n", .{});
    std.debug.print("  zig build run-llvm                   Quick test with LLVM backend\n", .{});
    std.debug.print("\n", .{});
}

