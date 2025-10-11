//! Module System - PawLang模块加载和解析
//!
//! 支持语法：
//!   import math.add;      // 导入math模块的add函数
//!   import math.Vec2;     // 导入math模块的Vec2类型
//!
//! 模块查找规则：
//!   1. import math.add -> 查找 math.paw
//!   2. import math.vec.Vec2 -> 查找 math/vec.paw
//!
//! 只有标记为pub的声明才能被导入

const std = @import("std");
const ast = @import("ast.zig");
const Lexer = @import("lexer.zig").Lexer;
const Parser = @import("parser.zig").Parser;

/// 模块信息
pub const Module = struct {
    path: []const u8,                         // 模块路径（math）
    source_file: []const u8,                  // 源文件路径（math.paw）
    source: []const u8,                       // 源代码（需要保留）
    declarations: []ast.TopLevelDecl,         // 所有声明
    public_items: std.StringHashMap(usize),   // pub项的索引（名称->索引）
    
    pub fn deinit(self: *Module, allocator: std.mem.Allocator) void {
        // 释放public_items中的键
        var it = self.public_items.iterator();
        while (it.next()) |entry| {
            allocator.free(entry.key_ptr.*);
        }
        self.public_items.deinit();
        
        allocator.free(self.path);
        allocator.free(self.source_file);
        allocator.free(self.source);
        // declarations不在这里释放，因为它们可能被导入到主程序中使用
    }
};

/// 模块加载器
pub const ModuleLoader = struct {
    allocator: std.mem.Allocator,
    modules: std.StringHashMap(Module),
    
    pub fn init(allocator: std.mem.Allocator) ModuleLoader {
        return ModuleLoader{
            .allocator = allocator,
            .modules = std.StringHashMap(Module).init(allocator),
        };
    }
    
    pub fn deinit(self: *ModuleLoader) void {
        var it = self.modules.iterator();
        while (it.next()) |entry| {
            entry.value_ptr.deinit(self.allocator);
        }
        self.modules.deinit();
    }
    
    /// 从模块中获取导入项
    pub fn getImportedItem(
        self: *ModuleLoader,
        module_path: []const u8,
        item_name: []const u8,
    ) !ast.TopLevelDecl {
        // 如果模块未加载，先加载
        if (!self.modules.contains(module_path)) {
            try self.loadModuleInternal(module_path);
        }
        
        // 获取模块
        const module_ptr = self.modules.getPtr(module_path).?;
        
        if (module_ptr.public_items.get(item_name)) |idx| {
            return module_ptr.declarations[idx];
        }
        
        // 没找到
        std.debug.print("Error: Item '{s}' not found in module '{s}'\n", .{item_name, module_path});
        std.debug.print("  Available public items:\n", .{});
        var it = module_ptr.public_items.iterator();
        while (it.next()) |entry| {
            std.debug.print("    - {s}\n", .{entry.key_ptr.*});
        }
        
        return error.ItemNotFound;
    }
    
    /// 内部方法：加载模块
    fn loadModuleInternal(self: *ModuleLoader, module_path: []const u8) !void {
        // 查找模块文件
        const source_file = try self.findModuleFile(module_path);
        defer self.allocator.free(source_file);
        
        // 读取源文件（不释放，保留在模块中）
        const source = try std.fs.cwd().readFileAlloc(
            self.allocator,
            source_file,
            10 * 1024 * 1024,
        );
        
        // 解析模块
        var lexer = Lexer.init(self.allocator, source, source_file);
        defer lexer.deinit();  // 🆕 v0.1.8: 确保清理
        const tokens = try lexer.tokenize();
        
        var parser = Parser.init(self.allocator, tokens);
        const program = try parser.parse();
        
        // 收集pub声明
        var public_items = std.StringHashMap(usize).init(self.allocator);
        for (program.declarations, 0..) |decl, idx| {
            const name = switch (decl) {
                .function => |f| if (f.is_public) f.name else null,
                .type_decl => |td| if (td.is_public) td.name else null,
                else => null,
            };
            
            if (name) |n| {
                const name_copy = try self.allocator.dupe(u8, n);
                try public_items.put(name_copy, idx);
            }
        }
        
        // 创建模块
        const module = Module{
            .path = try self.allocator.dupe(u8, module_path),
            .source_file = try self.allocator.dupe(u8, source_file),
            .source = source,
            .declarations = program.declarations,
            .public_items = public_items,
        };
        
        try self.modules.put(try self.allocator.dupe(u8, module_path), module);
    }
    
    /// 查找模块文件
    fn findModuleFile(self: *ModuleLoader, module_path: []const u8) ![]const u8 {
        // 尝试1: module_path.paw
        var buf = std.ArrayList(u8).init(self.allocator);
        try buf.appendSlice(module_path);
        try buf.appendSlice(".paw");
        const file1 = try buf.toOwnedSlice();
        
        if (std.fs.cwd().access(file1, .{})) {
            return file1;
        } else |_| {
            self.allocator.free(file1);
        }
        
        // 尝试2: module_path/mod.paw
        buf = std.ArrayList(u8).init(self.allocator);
        try buf.appendSlice(module_path);
        try buf.appendSlice("/mod.paw");
        const file2 = try buf.toOwnedSlice();
        
        if (std.fs.cwd().access(file2, .{})) {
            return file2;
        } else |_| {
            self.allocator.free(file2);
        }
        
        // 都找不到
        std.debug.print("Error: Module not found: {s}\n", .{module_path});
        std.debug.print("  Tried: {s}.paw\n", .{module_path});
        std.debug.print("  Tried: {s}/mod.paw\n", .{module_path});
        return error.ModuleNotFound;
    }
};
