// PawLang Built-in Functions Module
// v0.2.0
//
// 此模块导出所有内置函数，包括：
// - 内存管理 (memory.zig)
// - 文件系统 (fs.zig)

pub const memory = @import("memory.zig");
pub const fs = @import("fs.zig");

