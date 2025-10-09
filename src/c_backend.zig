const std = @import("std");
const ast = @import("ast.zig");
const CodeGen = @import("codegen.zig").CodeGen;

/// C Backend - Compiles and executes C code using available system compiler
pub const CBackend = struct {
    allocator: std.mem.Allocator,
    tcc_path: ?[]const u8, // TCC 可执行文件路径（如果有）
    
    pub fn init(allocator: std.mem.Allocator) CBackend {
        return CBackend{
            .allocator = allocator,
            .tcc_path = null,
        };
    }
    
    /// 检测系统是否安装了 TCC
    pub fn detectTcc(self: *TccBackend) !bool {
        // 尝试运行 tcc --version
        const result = std.process.Child.run(.{
            .allocator = self.allocator,
            .argv = &[_][]const u8{ "tcc", "--version" },
        }) catch {
            return false;
        };
        defer self.allocator.free(result.stdout);
        defer self.allocator.free(result.stderr);
        
        if (result.term.Exited == 0) {
            self.tcc_path = "tcc";
            return true;
        }
        
        return false;
    }
    
    /// 使用 C 编译器编译 C 代码为可执行文件
    pub fn compile(
        self: *CBackend,
        c_code: []const u8,
        output_file: []const u8,
    ) !void {
        // 1. 将 C 代码写入临时文件
        const temp_c_file = try std.fmt.allocPrint(
            self.allocator,
            "{s}.c",
            .{output_file},
        );
        defer self.allocator.free(temp_c_file);
        
        const c_file = try std.fs.cwd().createFile(temp_c_file, .{});
        defer c_file.close();
        _ = try c_file.write(c_code);
        
        // 2. 检测 TCC
        const has_tcc = try self.detectTcc();
        
        if (has_tcc) {
            // 使用 TCC 编译
            std.debug.print("🔧 Compiling with TinyCC...\n", .{});
            try self.compileWithTcc(temp_c_file, output_file);
        } else {
            // 回退到 GCC/Clang
            std.debug.print("⚠️  TinyCC not found, using system C compiler...\n", .{});
            try self.compileWithSystemCompiler(temp_c_file, output_file);
        }
    }
    
    /// 使用 TCC 编译
    fn compileWithTcc(
        self: *CBackend,
        c_file: []const u8,
        output_file: []const u8,
    ) !void {
        const argv = &[_][]const u8{
            "tcc",
            "-o",
            output_file,
            c_file,
        };
        
        const result = try std.process.Child.run(.{
            .allocator = self.allocator,
            .argv = argv,
        });
        defer self.allocator.free(result.stdout);
        defer self.allocator.free(result.stderr);
        
        if (result.term.Exited != 0) {
            std.debug.print("❌ TCC compilation failed:\n{s}\n", .{result.stderr});
            return error.CompilationFailed;
        }
        
        std.debug.print("✅ Compilation successful: {s}\n", .{output_file});
    }
    
    /// 使用系统 C 编译器（GCC/Clang）
    fn compileWithSystemCompiler(
        self: *CBackend,
        c_file: []const u8,
        output_file: []const u8,
    ) !void {
        // 尝试查找可用的 C 编译器
        var compiler: []const u8 = "gcc";
        
        // 先尝试 gcc
        if (std.process.Child.run(.{
            .allocator = self.allocator,
            .argv = &[_][]const u8{ "gcc", "--version" },
        })) |gcc_result| {
            self.allocator.free(gcc_result.stdout);
            self.allocator.free(gcc_result.stderr);
            compiler = "gcc";
        } else |_| {
            // gcc 不可用，尝试 clang
            if (std.process.Child.run(.{
                .allocator = self.allocator,
                .argv = &[_][]const u8{ "clang", "--version" },
            })) |clang_result| {
                self.allocator.free(clang_result.stdout);
                self.allocator.free(clang_result.stderr);
                compiler = "clang";
            } else |_| {
                std.debug.print("❌ C compiler not found (gcc/clang/tcc)\n", .{});
                std.debug.print("💡 Please install one of the following compilers:\n", .{});
                std.debug.print("   • TinyCC:  brew install tcc (recommended, fast)\n", .{});
                std.debug.print("   • GCC:     brew install gcc\n", .{});
                std.debug.print("   • Clang:   xcode-select --install\n", .{});
                return error.NoCompilerFound;
            }
        }
        
        const argv = &[_][]const u8{
            compiler,
            "-o",
            output_file,
            c_file,
        };
        
        const compile_result = try std.process.Child.run(.{
            .allocator = self.allocator,
            .argv = argv,
        });
        defer self.allocator.free(compile_result.stdout);
        defer self.allocator.free(compile_result.stderr);
        
        if (compile_result.term.Exited != 0) {
            std.debug.print("❌ {s} compilation failed:\n{s}\n", .{ compiler, compile_result.stderr });
            return error.CompilationFailed;
        }
        
        std.debug.print("✅ Compilation successful (using {s}): {s}\n", .{ compiler, output_file });
    }
    
    /// Compile and run (for REPL or quick testing)
    pub fn compileAndRun(
        self: *CBackend,
        c_code: []const u8,
    ) !void {
        const temp_output = "temp_paw_output";
        try self.compile(c_code, temp_output);
        
        // Run program
        std.debug.print("\n🚀 Running program:\n", .{});
        std.debug.print("─────────────────────────────────────────\n", .{});
        
        // 使用绝对路径运行
        const abs_path = try std.fmt.allocPrint(
            self.allocator,
            "./{s}",
            .{temp_output},
        );
        defer self.allocator.free(abs_path);
        
        const result = try std.process.Child.run(.{
            .allocator = self.allocator,
            .argv = &[_][]const u8{abs_path},
        });
        defer self.allocator.free(result.stdout);
        defer self.allocator.free(result.stderr);
        
        std.debug.print("{s}", .{result.stdout});
        if (result.stderr.len > 0) {
            std.debug.print("stderr: {s}", .{result.stderr});
        }
        std.debug.print("─────────────────────────────────────────\n", .{});
        std.debug.print("Exit code: {d}\n", .{result.term.Exited});
        
        // 清理临时文件
        std.fs.cwd().deleteFile(temp_output) catch {};
        const temp_c_output = try std.fmt.allocPrint(self.allocator, "{s}.c", .{temp_output});
        defer self.allocator.free(temp_c_output);
        std.fs.cwd().deleteFile(temp_c_output) catch {};
    }
};
