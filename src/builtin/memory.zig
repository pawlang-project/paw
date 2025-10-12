//! Built-in Memory Management Functions for PawLang
//! 
//! 为 PawLang 提供最小的动态内存支持
//! 只暴露 malloc、free 等基础函数，让 Paw 层实现 Vec<T> 等数据结构

const std = @import("std");

// ============================================================================
// 内存分配函数 - C ABI 导出
// ============================================================================

/// 分配内存
/// 参数: size - 字节数
/// 返回: 指针（作为 i64 返回，PawLang 中作为 i64 使用）
export fn paw_malloc(size: usize) i64 {
    const ptr = std.c.malloc(size) orelse return 0;
    const ptr_val: usize = @intFromPtr(ptr);
    return @bitCast(ptr_val);
}

/// 释放内存
/// 参数: ptr - 指针（PawLang 传入 i64，转换为指针）
export fn paw_free(ptr: i64) void {
    if (ptr == 0) return;
    const ptr_val: usize = @bitCast(ptr);
    const actual_ptr: *anyopaque = @ptrFromInt(ptr_val);
    std.c.free(actual_ptr);
}

/// 内存清零
/// 参数: ptr - 指针，size - 字节数
export fn paw_memset(ptr: i64, value: u8, size: usize) void {
    if (ptr == 0) return;
    const ptr_val: usize = @bitCast(ptr);
    const actual_ptr: [*]u8 = @ptrFromInt(ptr_val);
    @memset(actual_ptr[0..size], value);
}

/// 内存复制
/// 参数: dest - 目标指针，src - 源指针，size - 字节数
export fn paw_memcpy(dest: i64, src: i64, size: usize) void {
    if (dest == 0 or src == 0) return;
    const dest_val: usize = @bitCast(dest);
    const src_val: usize = @bitCast(src);
    const dest_ptr: [*]u8 = @ptrFromInt(dest_val);
    const src_ptr: [*]u8 = @ptrFromInt(src_val);
    @memcpy(dest_ptr[0..size], src_ptr[0..size]);
}

// ============================================================================
// 类型化读写函数 - 简化 PawLang 使用
// ============================================================================

/// 写入 i32 到指针
export fn paw_write_i32(ptr: i64, offset: i32, value: i32) void {
    if (ptr == 0) return;
    const ptr_val: usize = @bitCast(ptr);
    const actual_ptr: [*]i32 = @ptrFromInt(ptr_val);
    actual_ptr[@intCast(offset)] = value;
}

/// 从指针读取 i32
export fn paw_read_i32(ptr: i64, offset: i32) i32 {
    if (ptr == 0) return 0;
    const ptr_val: usize = @bitCast(ptr);
    const actual_ptr: [*]i32 = @ptrFromInt(ptr_val);
    return actual_ptr[@intCast(offset)];
}

/// 写入 f64 到指针
export fn paw_write_f64(ptr: i64, offset: i32, value: f64) void {
    if (ptr == 0) return;
    const ptr_val: usize = @bitCast(ptr);
    const actual_ptr: [*]f64 = @ptrFromInt(ptr_val);
    actual_ptr[@intCast(offset)] = value;
}

/// 从指针读取 f64
export fn paw_read_f64(ptr: i64, offset: i32) f64 {
    if (ptr == 0) return 0.0;
    const ptr_val: usize = @bitCast(ptr);
    const actual_ptr: [*]f64 = @ptrFromInt(ptr_val);
    return actual_ptr[@intCast(offset)];
}

// ============================================================================
// 调试辅助
// ============================================================================

/// 打印指针地址（调试用）
export fn paw_debug_ptr(ptr: i64) void {
    std.debug.print("Pointer: 0x{x}\n", .{ptr});
}

// ============================================================================
// 测试
// ============================================================================

test "Memory allocation" {
    const ptr = paw_malloc(100);
    try std.testing.expect(ptr != 0);
    
    paw_write_i32(ptr, 0, 42);
    const value = paw_read_i32(ptr, 0);
    try std.testing.expectEqual(@as(i32, 42), value);
    
    paw_free(ptr);
}

