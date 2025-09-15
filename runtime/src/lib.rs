#![allow(clippy::not_unsafe_ptr_arg_deref)]

use std::{ffi::CStr, io::{self, Write}, mem, os::raw::{c_char, c_int}};


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
pub extern "C" fn print_int(v: i32) -> () {
    write_str_no_nl(&format!("{}", v));
}

#[unsafe(no_mangle)]
pub extern "C" fn println_int(v: i32) -> () {
    write_line(&format!("{}", v));
}

#[unsafe(no_mangle)]
pub extern "C" fn print_long(v: i64) -> () {
    write_str_no_nl(&format!("{}", v));
}

#[unsafe(no_mangle)]
pub extern "C" fn println_long(v: i64) -> () {
    write_line(&format!("{}", v));
}

#[unsafe(no_mangle)]
pub extern "C" fn print_bool(v: i8) -> () {
    let b = v != 0;
    write_str_no_nl(if b { "true" } else { "false" });
}

#[unsafe(no_mangle)]
pub extern "C" fn println_bool(v: i8) -> () {
    let b = v != 0;
    write_line(if b { "true" } else { "false" });
}

#[unsafe(no_mangle)]
pub extern "C" fn print_char(ch: i32) -> () {
    let c = char::from_u32(ch as u32).unwrap_or('\u{FFFD}');
    write_str_no_nl(&c.to_string());
}

#[unsafe(no_mangle)]
pub extern "C" fn println_char(ch: i32) -> () {
    let c = char::from_u32(ch as u32).unwrap_or('\u{FFFD}');
    write_line(&c.to_string());
}

#[unsafe(no_mangle)]
pub extern "C" fn print_float(v: f32) -> () {
    write_str_no_nl(&format!("{}", v));
}

#[unsafe(no_mangle)]
pub extern "C" fn println_float(v: f32) -> () {
    write_line(&format!("{}", v));
}

#[unsafe(no_mangle)]
pub extern "C" fn print_double(v: f64) -> () {
    write_str_no_nl(&format!("{}", v));
}

#[unsafe(no_mangle)]
pub extern "C" fn println_double(v: f64) -> () {
    write_line(&format!("{}", v));
}

#[unsafe(no_mangle)]
pub extern "C" fn print_str(s: *const c_char) -> () {
    if s.is_null() { return; }
    unsafe {
        match CStr::from_ptr(s).to_str() {
            Ok(text) => write_str_no_nl(text),
            Err(_) => write_bytes_no_nl(CStr::from_ptr(s).to_bytes()),
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn println_str(s: *const c_char) -> () {
    if s.is_null() { return; }
    unsafe {
        match CStr::from_ptr(s).to_str() {
            Ok(text) => write_line(text),
            Err(_) => {
                write_bytes_no_nl(CStr::from_ptr(s).to_bytes());
                write_bytes_no_nl(b"\n");
            }
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_malloc(size: u64) -> u64 {
    // Vec<u8> 分配一块堆内存并泄漏，返回裸指针
    let mut v = Vec::<u8>::with_capacity(size as usize);
    let ptr = v.as_mut_ptr();
    mem::forget(v);
    ptr as u64
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_free(ptr_u: u64, size: u64) {
    if ptr_u == 0 { return; }
    unsafe {
        // 尽可能回收（需要容量信息；这里至少不会泄漏）
        let _ = Vec::from_raw_parts(ptr_u as *mut u8, 0, size as usize);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_realloc(ptr_u: u64, old_cap: u64, new_cap: u64) -> u64 {
    unsafe {
        if ptr_u == 0 {
            return paw_malloc(new_cap);
        }
        let mut v = Vec::<u8>::from_raw_parts(ptr_u as *mut u8, 0, old_cap as usize);
        v.reserve_exact((new_cap as usize).saturating_sub(old_cap as usize));
        let new_ptr = v.as_mut_ptr();
        mem::forget(v);
        new_ptr as u64
    }
}

// ---------- 可变 String（UTF-8），句柄为 *mut Vec<u8> ----------

#[unsafe(no_mangle)]
pub extern "C" fn paw_string_new() -> u64 {
    let buf: Box<Vec<u8>> = Box::new(Vec::new());
    Box::into_raw(buf) as u64
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_string_from_cstr(c_ptr: u64) -> u64 {
    if c_ptr == 0 { return 0; }
    unsafe {
        let s = CStr::from_ptr(c_ptr as *const c_char);
        let mut v = s.to_bytes().to_vec();
        // 不带结尾 0，内部保持纯字节；导出 cstr 时再补 0
        let buf: Box<Vec<u8>> = Box::new(v.drain(..).collect());
        Box::into_raw(buf) as u64
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_string_push_cstr(handle: u64, c_ptr: u64) -> u64 {
    if handle == 0 || c_ptr == 0 { return 0; }
    unsafe {
        let v = &mut *(handle as *mut Vec<u8>);
        let s = CStr::from_ptr(c_ptr as *const c_char);
        v.extend_from_slice(s.to_bytes());
        v.len() as u64
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_string_push_char(handle: u64, ch: i64) -> u64 {
    if handle == 0 { return 0; }
    unsafe {
        let v = &mut *(handle as *mut Vec<u8>);
        // ch 视为 Unicode Scalar Value（基础先支持 ASCII）
        let c = char::from_u32(ch as u32).unwrap_or('\u{FFFD}');
        let mut buf = [0u8; 4];
        let s = c.encode_utf8(&mut buf);
        v.extend_from_slice(s.as_bytes());
        v.len() as u64
    }
}

/// 确保末尾有 '\0'，返回可直接给 print_str 使用的 C 字符串指针
#[unsafe(no_mangle)]
pub extern "C" fn paw_string_as_cstr(handle: u64) -> u64 {
    if handle == 0 { return 0; }
    unsafe {
        let v = &mut *(handle as *mut Vec<u8>);
        if v.last().copied() != Some(0) {
            v.push(0);
        }
        v.as_ptr() as u64
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_string_len(handle: u64) -> u64 {
    if handle == 0 { return 0; }
    unsafe { (&*(handle as *const Vec<u8>)).len() as u64 }
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_string_clear(handle: u64) {
    if handle == 0 { return; }
    unsafe { (&mut *(handle as *mut Vec<u8>)).clear(); }
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_string_free(handle: u64) {
    if handle == 0 { return; }
    unsafe {
        drop(Box::from_raw(handle as *mut Vec<u8>));
    }
}

// ---------- Vec<u8>：词法缓冲区 ----------

#[unsafe(no_mangle)]
pub extern "C" fn paw_vec_u8_new() -> u64 {
    Box::into_raw(Box::new(Vec::<u8>::new())) as u64
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_vec_u8_push(handle: u64, byte_: i64) -> u64 {
    if handle == 0 { return 0; }
    unsafe {
        let v = &mut *(handle as *mut Vec<u8>);
        v.push((byte_ & 0xFF) as u8);
        v.len() as u64
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_vec_u8_len(handle: u64) -> u64 {
    if handle == 0 { return 0; }
    unsafe { (&*(handle as *const Vec<u8>)).len() as u64 }
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_vec_u8_data_ptr(handle: u64) -> u64 {
    if handle == 0 { return 0; }
    unsafe { (&*(handle as *const Vec<u8>)).as_ptr() as u64 }
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_vec_u8_free(handle: u64) {
    if handle == 0 { return; }
    unsafe { drop(Box::from_raw(handle as *mut Vec<u8>)); }
}

// ---------- Vec<i64>：通用栈/表 ----------

#[unsafe(no_mangle)]
pub extern "C" fn paw_vec_i64_new() -> u64 {
    Box::into_raw(Box::new(Vec::<i64>::new())) as u64
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_vec_i64_push(handle: u64, v: i64) -> u64 {
    if handle == 0 { return 0; }
    unsafe {
        let vec = &mut *(handle as *mut Vec<i64>);
        vec.push(v);
        vec.len() as u64
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_vec_i64_pop(handle: u64, out_ok: *mut i64) -> i64 {
    if handle == 0 { return 0; }
    unsafe {
        let vec = &mut *(handle as *mut Vec<i64>);
        if let Some(x) = vec.pop() {
            if !out_ok.is_null() { *out_ok = x; }
            1 // true
        } else {
            0 // false
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_vec_i64_len(handle: u64) -> u64 {
    if handle == 0 { return 0; }
    unsafe { (&*(handle as *const Vec<i64>)).len() as u64 }
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_vec_i64_get(handle: u64, idx: u64, out_ok: *mut i64) -> i64 {
    if handle == 0 { return 0; }
    unsafe {
        let vec = &*(handle as *const Vec<i64>);
        if let Some(x) = vec.get(idx as usize) {
            if !out_ok.is_null() { *out_ok = *x; }
            1
        } else { 0 }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_vec_i64_free(handle: u64) {
    if handle == 0 { return; }
    unsafe { drop(Box::from_raw(handle as *mut Vec<i64>)); }
}

#[unsafe(no_mangle)]
pub extern "C" fn paw_exit(code: c_int) {
    std::process::exit(code);
}

// Windows
// # 安装目标（如未安装）
// rustup target add x86_64-pc-windows-gnu
// # 构建静态库
// cargo build -p runtime --release --target x86_64-pc-windows-gnu
// # 产物位置：
// #   runtime/target/x86_64-pc-windows-gnu/release/libruntime.a


// Linux
// rustup target add x86_64-unknown-linux-gnu
// cargo build -p runtime --release --target x86_64-unknown-linux-gnu
// # 产物：
// #   runtime/target/x86_64-unknown-linux-gnu/release/libruntime.a

// macOS
// rustup target add x86_64-apple-darwin aarch64-apple-darwin
// cargo build -p runtime --release --target x86_64-apple-darwin
// cargo build -p runtime --release --target aarch64-apple-darwin
// # 产物：
// #   runtime/target/<triple>/release/libruntime.a