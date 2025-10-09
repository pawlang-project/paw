// 测试 llvm-zig 绑定是否工作
// 注意: 需要系统安装 LLVM 才能编译
// 编译: zig build-exe test_llvm_native.zig -Dwith-llvm=true

const std = @import("std");

// 这是一个示例，演示如何使用 LLVM C API
// 由于需要系统 LLVM，v0.1.4 暂不启用

pub fn main() !void {
    std.debug.print("LLVM Native API Test\n", .{});
    std.debug.print("Status: Requires system LLVM\n", .{});
    std.debug.print("Current: Using text IR generation instead\n", .{});
    
    // 未来如果启用 LLVM 原生API:
    // const llvm = @import("llvm");
    // const context = llvm.Context.create();
    // const module = llvm.Module.createWithName("test", context);
    // ...
}

