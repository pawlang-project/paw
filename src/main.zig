const std = @import("std");
const Lexer = @import("lexer.zig").Lexer;
const Parser = @import("parser.zig").Parser;
const TypeChecker = @import("typechecker.zig").TypeChecker;
const CodeGen = @import("codegen.zig").CodeGen;

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    if (args.len < 2) {
        std.debug.print("用法: pawc <源文件.paw> [选项]\n", .{});
        std.debug.print("选项:\n", .{});
        std.debug.print("  -o <输出文件>  指定输出文件名\n", .{});
        std.debug.print("  -O             启用优化\n", .{});
        std.debug.print("  -v             详细输出\n", .{});
        return;
    }

    const source_file = args[1];
    var output_file: ?[]const u8 = null;
    var optimize = false;
    var verbose = false;

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

    // 4. 代码生成
    var codegen = CodeGen.init(allocator, optimize);
    defer codegen.deinit();
    
    const output_name = output_file orelse "output";
    try codegen.generate(ast, output_name);
    
    const total_time = std.time.nanoTimestamp();
    std.debug.print("编译完成: {s} -> {s} ({d:.2}s)\n", .{
        source_file,
        output_name,
        @as(f64, @floatFromInt(total_time - start_time)) / 1_000_000_000.0,
    });
}

