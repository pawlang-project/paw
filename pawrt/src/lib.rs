#![allow(clippy::not_unsafe_ptr_arg_deref)]

use std::{
    ffi::{CStr, CString},
    fs,
    io::{self, Write},
    os::raw::{c_char, c_int, c_uchar},
    ptr,
};

#[unsafe(no_mangle)]
pub extern "C" fn paw_print_cstr(s: *const c_char) -> i64 {
    if s.is_null() {
        return 0;
    }
    // 打印不带换行（保持与很多 C 运行时例子的习惯一致）
    unsafe {
        match CStr::from_ptr(s).to_str() {
            Ok(text) => {
                // stdout 不加锁 flush 太频繁可能影响性能，这里主动 flush
                let _ = io::stdout().write_all(text.as_bytes());
                let _ = io::stdout().flush();
            }
            Err(_) => {
                // 非 UTF-8：按字节打印
                let bytes = CStr::from_ptr(s).to_bytes();
                let _ = io::stdout().write_all(bytes);
                let _ = io::stdout().flush();
            }
        }
    }
    0
}

/// 读取整个文件，返回一个以 '\0' 结尾的堆分配字节串指针（C 字符串）。
/// 返回值：非空指针；失败时返回 NULL。
/// 所有权：调用方负责调用 `paw_free(ptr, len)` 释放（len 可传 0）。
#[unsafe(no_mangle)]
pub extern "C" fn paw_read_file_cstr(path: *const c_char) -> *mut c_uchar {
    if path.is_null() {
        return ptr::null_mut();
    }
    // 读取路径
    let p = unsafe { CStr::from_ptr(path) };
    let path_str = match p.to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };

    match fs::read(path_str) {
        Ok(mut bytes) => {
            // 追加 '\0'，让它可作为 C 字符串使用
            bytes.push(0);
            let mut boxed = bytes.into_boxed_slice();
            let ptr = boxed.as_mut_ptr();
            std::mem::forget(boxed); // 将所有权交给 FFI 调用方
            ptr
        }
        Err(_) => ptr::null_mut(),
    }
}

/// 释放由 `paw_read_file_cstr`（或其他 paw_* 分配函数）返回的缓冲区。
#[unsafe(no_mangle)]
pub extern "C" fn paw_free(ptr_: *mut c_uchar, _len: usize) {
    if ptr_.is_null() {
        return;
    }
    unsafe {
        // 因为不知道原长度（我们在 read_file_cstr 追加过 '\0'），
        // 这里退而求其次：按 C 字符串重新计算长度再回收。
        // 若你在语言层能保存长度，建议把 _len 真实传入，再用 Vec::from_raw_parts 回收。
        let c_str = CStr::from_ptr(ptr_ as *const c_char);
        let bytes = c_str.to_bytes_with_nul();
        let len = bytes.len();
        let _ = Vec::from_raw_parts(ptr_, len, len); // 交还给 Rust 分配器
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_exit(code: c_int) {
    std::process::exit(code);
}

// Windows
// # 安装目标（如未安装）
// rustup target add x86_64-pc-windows-gnu
// # 构建静态库
// cargo build -p pawrt --release --target x86_64-pc-windows-gnu
// # 产物位置：
// #   pawrt/target/x86_64-pc-windows-gnu/release/libpawrt.a


// Linux
// rustup target add x86_64-unknown-linux-gnu
// cargo build -p pawrt --release --target x86_64-unknown-linux-gnu
// # 产物：
// #   pawrt/target/x86_64-unknown-linux-gnu/release/libpawrt.a

// macOS
// rustup target add x86_64-apple-darwin aarch64-apple-darwin
// cargo build -p pawrt --release --target x86_64-apple-darwin
// cargo build -p pawrt --release --target aarch64-apple-darwin
// # 产物：
// #   pawrt/target/<triple>/release/libpawrt.a