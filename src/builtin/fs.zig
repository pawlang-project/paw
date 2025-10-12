// 文件系统内置函数
// PawLang v0.2.0
// 提供跨平台的文件 I/O 操作

const std = @import("std");

// 全局分配器（用于文件内容缓冲区）
var gpa = std.heap.GeneralPurposeAllocator(.{}){};
const allocator = gpa.allocator();

// ============================================================================
// 文件读取
// ============================================================================

/// 读取文件内容
/// @param path_ptr 文件路径的指针
/// @param path_len 路径长度
/// @return 文件内容的指针（i64 编码），失败返回 0
export fn paw_read_file(path_ptr: [*]const u8, path_len: usize) i64 {
    // 转换路径为 slice
    const path_slice = path_ptr[0..path_len];
    
    // 打开文件
    const file = std.fs.cwd().openFile(path_slice, .{}) catch {
        return 0; // 文件不存在或无法打开
    };
    defer file.close();
    
    // 读取文件大小
    const file_size = file.getEndPos() catch {
        return 0;
    };
    
    // 分配缓冲区
    const buffer = allocator.alloc(u8, file_size) catch {
        return 0;
    };
    
    // 读取内容
    const bytes_read = file.readAll(buffer) catch {
        allocator.free(buffer);
        return 0;
    };
    
    if (bytes_read != file_size) {
        allocator.free(buffer);
        return 0;
    }
    
    // 返回缓冲区指针（编码为 i64）
    return @intCast(@intFromPtr(buffer.ptr));
}

/// 获取读取的文件大小
/// @param content_ptr 文件内容指针（来自 paw_read_file）
/// @return 文件大小
export fn paw_file_size(path_ptr: [*]const u8, path_len: usize) i64 {
    const path_slice = path_ptr[0..path_len];
    
    const file = std.fs.cwd().openFile(path_slice, .{}) catch {
        return -1;
    };
    defer file.close();
    
    const size = file.getEndPos() catch {
        return -1;
    };
    
    return @intCast(size);
}

// ============================================================================
// 文件写入
// ============================================================================

/// 写入文件
/// @param path_ptr 文件路径指针
/// @param path_len 路径长度
/// @param content_ptr 内容指针
/// @param content_len 内容长度
/// @return 成功返回 1，失败返回 0
export fn paw_write_file(
    path_ptr: [*]const u8,
    path_len: usize,
    content_ptr: [*]const u8,
    content_len: usize,
) i32 {
    const path_slice = path_ptr[0..path_len];
    const content_slice = content_ptr[0..content_len];
    
    // 创建或覆盖文件
    const file = std.fs.cwd().createFile(path_slice, .{}) catch {
        return 0;
    };
    defer file.close();
    
    // 写入内容
    file.writeAll(content_slice) catch {
        return 0;
    };
    
    return 1;
}

/// 追加到文件
/// @param path_ptr 文件路径指针
/// @param path_len 路径长度
/// @param content_ptr 内容指针
/// @param content_len 内容长度
/// @return 成功返回 1，失败返回 0
export fn paw_append_file(
    path_ptr: [*]const u8,
    path_len: usize,
    content_ptr: [*]const u8,
    content_len: usize,
) i32 {
    const path_slice = path_ptr[0..path_len];
    const content_slice = content_ptr[0..content_len];
    
    // 打开文件以追加模式
    const file = std.fs.cwd().openFile(path_slice, .{
        .mode = .read_write,
    }) catch {
        // 如果文件不存在，创建它
        return paw_write_file(path_ptr, path_len, content_ptr, content_len);
    };
    defer file.close();
    
    // 移动到文件末尾
    file.seekFromEnd(0) catch {
        return 0;
    };
    
    // 写入内容
    file.writeAll(content_slice) catch {
        return 0;
    };
    
    return 1;
}

// ============================================================================
// 文件检查
// ============================================================================

/// 检查文件是否存在
/// @param path_ptr 文件路径指针
/// @param path_len 路径长度
/// @return 存在返回 1，不存在返回 0
export fn paw_file_exists(path_ptr: [*]const u8, path_len: usize) i32 {
    const path_slice = path_ptr[0..path_len];
    
    // 尝试访问文件
    std.fs.cwd().access(path_slice, .{}) catch {
        return 0;
    };
    
    return 1;
}

/// 检查是否为目录
/// @param path_ptr 路径指针
/// @param path_len 路径长度
/// @return 是目录返回 1，否则返回 0
export fn paw_is_dir(path_ptr: [*]const u8, path_len: usize) i32 {
    const path_slice = path_ptr[0..path_len];
    
    const file = std.fs.cwd().openFile(path_slice, .{}) catch {
        return 0;
    };
    defer file.close();
    
    const stat = file.stat() catch {
        return 0;
    };
    
    return if (stat.kind == .directory) 1 else 0;
}

// ============================================================================
// 文件操作
// ============================================================================

/// 删除文件
/// @param path_ptr 文件路径指针
/// @param path_len 路径长度
/// @return 成功返回 1，失败返回 0
export fn paw_delete_file(path_ptr: [*]const u8, path_len: usize) i32 {
    const path_slice = path_ptr[0..path_len];
    
    std.fs.cwd().deleteFile(path_slice) catch {
        return 0;
    };
    
    return 1;
}

/// 重命名/移动文件
/// @param old_path_ptr 旧路径指针
/// @param old_path_len 旧路径长度
/// @param new_path_ptr 新路径指针
/// @param new_path_len 新路径长度
/// @return 成功返回 1，失败返回 0
export fn paw_rename_file(
    old_path_ptr: [*]const u8,
    old_path_len: usize,
    new_path_ptr: [*]const u8,
    new_path_len: usize,
) i32 {
    const old_path = old_path_ptr[0..old_path_len];
    const new_path = new_path_ptr[0..new_path_len];
    
    std.fs.cwd().rename(old_path, new_path) catch {
        return 0;
    };
    
    return 1;
}

// ============================================================================
// 目录操作
// ============================================================================

/// 创建目录
/// @param path_ptr 目录路径指针
/// @param path_len 路径长度
/// @return 成功返回 1，失败返回 0
export fn paw_create_dir(path_ptr: [*]const u8, path_len: usize) i32 {
    const path_slice = path_ptr[0..path_len];
    
    std.fs.cwd().makeDir(path_slice) catch {
        return 0;
    };
    
    return 1;
}

/// 递归创建目录
/// @param path_ptr 目录路径指针
/// @param path_len 路径长度
/// @return 成功返回 1，失败返回 0
export fn paw_create_dir_all(path_ptr: [*]const u8, path_len: usize) i32 {
    const path_slice = path_ptr[0..path_len];
    
    std.fs.cwd().makePath(path_slice) catch {
        return 0;
    };
    
    return 1;
}

/// 删除空目录
/// @param path_ptr 目录路径指针
/// @param path_len 路径长度
/// @return 成功返回 1，失败返回 0
export fn paw_delete_dir(path_ptr: [*]const u8, path_len: usize) i32 {
    const path_slice = path_ptr[0..path_len];
    
    std.fs.cwd().deleteDir(path_slice) catch {
        return 0;
    };
    
    return 1;
}

/// 递归删除目录及其内容
/// @param path_ptr 目录路径指针
/// @param path_len 路径长度
/// @return 成功返回 1，失败返回 0
export fn paw_delete_dir_all(path_ptr: [*]const u8, path_len: usize) i32 {
    const path_slice = path_ptr[0..path_len];
    
    std.fs.cwd().deleteTree(path_slice) catch {
        return 0;
    };
    
    return 1;
}

// ============================================================================
// 辅助函数
// ============================================================================

/// 释放由 paw_read_file 分配的内存
/// @param ptr 指针
/// @param len 长度
export fn paw_free_file_content(ptr: i64, len: usize) void {
    if (ptr == 0) return;
    
    const buf_ptr: [*]u8 = @ptrFromInt(@as(usize, @intCast(ptr)));
    const buffer = buf_ptr[0..len];
    allocator.free(buffer);
}

