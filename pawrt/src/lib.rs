#![allow(clippy::not_unsafe_ptr_arg_deref)]

use std::{
    ffi::{CStr, CString},
    fs,
    io::{self, Write},
    os::raw::{c_char, c_int, c_uchar},
    ptr,
};

#[inline]
fn write_bytes_no_nl(buf: &[u8]) {
    let _ = io::stdout().write_all(buf);
    let _ = io::stdout().flush();
}

#[inline]
fn write_str_no_nl(s: &str) {
    let _ = io::stdout().write_all(s.as_bytes());
    let _ = io::stdout().flush();
}

#[inline]
fn write_line(s: &str) {
    let _ = io::stdout().write_all(s.as_bytes());
    let _ = io::stdout().write_all(b"\n");
    let _ = io::stdout().flush();
}

#[unsafe(no_mangle)]
pub extern "C" fn print_int(v: i64) -> i64 {
    write_str_no_nl(&format!("{}", v));
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn println_int(v: i64) -> i64 {
    write_line(&format!("{}", v));
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn print_bool(v: i8) -> i8 {
    let b = (v & 1) != 0;
    write_str_no_nl(if b { "true" } else { "false" });
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn println_bool(v: i8) -> i8 {
    let b = (v & 1) != 0;
    write_line(if b { "true" } else { "false" });
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn print_str(s: *const c_char) -> i64 {
    if s.is_null() { return 0; }
    unsafe {
        match CStr::from_ptr(s).to_str() {
            Ok(text) => write_str_no_nl(text),
            Err(_) => write_bytes_no_nl(CStr::from_ptr(s).to_bytes()),
        }
    }
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn println_str(s: *const c_char) -> i64 {
    if s.is_null() { return 0; }
    unsafe {
        match CStr::from_ptr(s).to_str() {
            Ok(text) => write_line(text),
            Err(_) => {
                write_bytes_no_nl(CStr::from_ptr(s).to_bytes());
                write_bytes_no_nl(b"\n");
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