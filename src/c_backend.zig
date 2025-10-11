const std = @import("std");
const ast = @import("ast.zig");
const CodeGen = @import("codegen.zig").CodeGen;

/// C Backend - Compiles and executes C code using GCC
/// Generates portable C code that can be compiled with any C compiler
pub const CBackend = struct {
    allocator: std.mem.Allocator,
    
    pub fn init(allocator: std.mem.Allocator) CBackend {
        return CBackend{
            .allocator = allocator,
        };
    }
    
    /// Compile C code to executable using GCC
    /// Falls back to clang if GCC is not available
    pub fn compile(
        self: *CBackend,
        c_code: []const u8,
        output_file: []const u8,
    ) !void {
        // 1. Write C code to temporary file
        const temp_c_file = try std.fmt.allocPrint(
            self.allocator,
            "{s}.c",
            .{output_file},
        );
        defer self.allocator.free(temp_c_file);
        
        const c_file = try std.fs.cwd().createFile(temp_c_file, .{});
        defer c_file.close();
        _ = try c_file.write(c_code);
        
        // 2. Compile with GCC (or clang as fallback)
        try self.compileWithGcc(temp_c_file, output_file);
    }
    
           /// Compile using system C compiler (Zig CC -> GCC -> Clang)
           fn compileWithGcc(
               self: *CBackend,
               c_file: []const u8,
               output_file: []const u8,
           ) !void {
               // Try to find available C compiler
               // Priority: Zig CC (if building from source) -> GCC -> Clang
               var compiler: []const u8 = "gcc";
               var use_zig_cc = false;
               
               // 1. Try Zig CC first (for users who build from source)
               if (std.process.Child.run(.{
                   .allocator = self.allocator,
                   .argv = &[_][]const u8{ "zig", "cc", "--version" },
               })) |zig_result| {
                   self.allocator.free(zig_result.stdout);
                   self.allocator.free(zig_result.stderr);
                   compiler = "zig";
                   use_zig_cc = true;
                   std.debug.print("🚀 Compiling with Zig CC (Clang 20.1.2, best performance)...\n", .{});
               } else |_| {
                   // 2. Try gcc
                   if (std.process.Child.run(.{
                       .allocator = self.allocator,
                       .argv = &[_][]const u8{ "gcc", "--version" },
                   })) |gcc_result| {
                       self.allocator.free(gcc_result.stdout);
                       self.allocator.free(gcc_result.stderr);
                       compiler = "gcc";
                       std.debug.print("🔧 Compiling with GCC...\n", .{});
                   } else |_| {
                       // 3. Try clang as fallback
                       if (std.process.Child.run(.{
                           .allocator = self.allocator,
                           .argv = &[_][]const u8{ "clang", "--version" },
                       })) |clang_result| {
                           self.allocator.free(clang_result.stdout);
                           self.allocator.free(clang_result.stderr);
                           compiler = "clang";
                           std.debug.print("🔧 Compiling with Clang...\n", .{});
                       } else |_| {
                           std.debug.print("❌ No C compiler found (zig/gcc/clang)\n", .{});
                           std.debug.print("💡 Please install a C compiler:\n", .{});
                           std.debug.print("   • Zig (recommended): Already available if you built from source\n", .{});
                           std.debug.print("   • Linux:   sudo apt-get install gcc\n", .{});
                           std.debug.print("   • macOS:   brew install gcc or xcode-select --install\n", .{});
                           std.debug.print("   • Windows: Install MinGW or MSVC\n", .{});
                           return error.NoCompilerFound;
                       }
                   }
               }
               
               // Build command line arguments
               const argv = if (use_zig_cc) &[_][]const u8{
                   "zig", "cc", "-o", output_file, c_file,
               } else &[_][]const u8{
                   compiler, "-o", output_file, c_file,
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
