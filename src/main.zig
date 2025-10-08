const std = @import("std");
const Lexer = @import("lexer.zig").Lexer;
const Parser = @import("parser.zig").Parser;
const TypeChecker = @import("typechecker.zig").TypeChecker;
const CodeGen = @import("codegen.zig").CodeGen;
const TccBackend = @import("tcc_backend.zig").TccBackend;

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

    // 检查特殊命令
    if (std.mem.eql(u8, args[1], "--help") or std.mem.eql(u8, args[1], "-h")) {
        printUsage();
        return;
    }

    if (std.mem.eql(u8, args[1], "--version")) {
        std.debug.print("pawc 0.0.3 (TinyCC Backend)\n", .{});
        return;
    }

    const source_file = args[1];
    var output_file: ?[]const u8 = null;
    var optimize = false;
    var verbose = false;
    var should_run = false;      // 新增：是否运行
    var should_compile = false;  // 新增：是否编译为可执行文件

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
        }
    }

    // 读取源文件
    const source = try std.fs.cwd().readFileAlloc(allocator, source_file, 10 * 1024 * 1024);
    defer allocator.free(source);

    if (verbose) {
        std.debug.print("正在编译: {s}\n", .{source_file});
    }

    // 编译流程
    const start_time = std.time.nanoTimestamp();

    // 1. 词法分析
    var lexer = Lexer.init(allocator, source);
    defer lexer.deinit();
    
    const tokens = try lexer.tokenize();
    if (verbose) {
        const lex_time = std.time.nanoTimestamp();
        std.debug.print("[PERF] 词法分析: {d}μs\n", .{@divTrunc(lex_time - start_time, 1000)});
    }

    // 2. 语法分析
    var parser = Parser.init(allocator, tokens);
    defer parser.deinit();
    
    const ast = try parser.parse();
    if (verbose) {
        const parse_time = std.time.nanoTimestamp();
        std.debug.print("[PERF] 语法分析: {d}μs\n", .{@divTrunc(parse_time - start_time, 1000)});
    }

    // 3. 类型检查
    var type_checker = TypeChecker.init(allocator);
    defer type_checker.deinit();
    
    try type_checker.check(ast);
    if (verbose) {
        const typecheck_time = std.time.nanoTimestamp();
        std.debug.print("[PERF] 类型检查: {d}μs\n", .{@divTrunc(typecheck_time - start_time, 1000)});
    }

    // 4. C 代码生成
    var codegen = CodeGen.init(allocator);
    defer codegen.deinit();
    
    const c_code = try codegen.generate(ast);
    
    const total_time = std.time.nanoTimestamp();
    
    // 5. 根据选项决定输出方式
    if (should_compile) {
        // 编译为可执行文件
        const output_name = output_file orelse "output";
        
        var tcc_backend = TccBackend.init(allocator);
        
        if (should_run) {
            // 编译并运行
            std.debug.print("🔥 编译并运行: {s}\n", .{source_file});
            try tcc_backend.compileAndRun(c_code);
        } else {
            // 只编译
            try tcc_backend.compile(c_code, output_name);
        }
        
        std.debug.print("\n编译完成: {s} -> {s} ({d:.2}s)\n", .{
            source_file,
            output_name,
            @as(f64, @floatFromInt(total_time - start_time)) / 1_000_000_000.0,
        });
    } else {
        // 只生成 C 代码（默认行为）
        const output_name = output_file orelse "output";
        const c_filename = try std.fmt.allocPrint(allocator, "{s}.c", .{output_name});
        defer allocator.free(c_filename);
        
        const c_file = std.fs.cwd().createFile(c_filename, .{}) catch |err| {
            std.debug.print("❌ 无法创建文件 {s}: {}\n", .{ c_filename, err });
            return;
        };
        defer c_file.close();
        
        _ = try c_file.write(c_code);
        
        std.debug.print("编译完成: {s} -> {s} ({d:.2}s)\n", .{
            source_file,
            c_filename,
            @as(f64, @floatFromInt(total_time - start_time)) / 1_000_000_000.0,
        });
        
        std.debug.print("✅ C 代码已生成: {s}\n", .{c_filename});
        std.debug.print("💡 提示:\n", .{});
        std.debug.print("   • 编译: pawc {s} --compile -o {s}\n", .{ source_file, output_name });
        std.debug.print("   • 运行: pawc {s} --run\n", .{source_file});
        std.debug.print("   • 手动: gcc {s} -o {s}\n", .{ c_filename, output_name });
    }
}

fn printUsage() void {
    std.debug.print("\n", .{});
    std.debug.print("╔═══════════════════════════════════════════════════════════════╗\n", .{});
    std.debug.print("║        pawc - Paw Language Compiler                          ║\n", .{});
    std.debug.print("╚═══════════════════════════════════════════════════════════════╝\n", .{});
    std.debug.print("\n", .{});
    std.debug.print("用法:\n", .{});
    std.debug.print("  pawc <源文件.paw> [选项]        编译 Paw 源文件\n", .{});
    std.debug.print("  pawc --version                  显示版本信息\n", .{});
    std.debug.print("  pawc --help                     显示此帮助信息\n", .{});
    std.debug.print("\n", .{});
    std.debug.print("编译选项:\n", .{});
    std.debug.print("  -o <输出文件>    指定输出文件名\n", .{});
    std.debug.print("  -O               启用优化（暂未实现）\n", .{});
    std.debug.print("  -v               详细输出\n", .{});
    std.debug.print("  --compile        编译为可执行文件（使用 TCC/GCC/Clang）\n", .{});
    std.debug.print("  --run            编译并立即运行程序\n", .{});
    std.debug.print("\n", .{});
    std.debug.print("示例:\n", .{});
    std.debug.print("  pawc hello.paw                  生成 C 代码 -> output.c\n", .{});
    std.debug.print("  pawc hello.paw --compile        编译为可执行文件 -> output\n", .{});
    std.debug.print("  pawc hello.paw --run            编译并运行\n", .{});
    std.debug.print("  pawc hello.paw -o hello --run   编译为 hello 并运行\n", .{});
    std.debug.print("\n", .{});
}

