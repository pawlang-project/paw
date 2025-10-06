use std::ffi::{c_char, CStr};
use once_cell::sync::Lazy;
use std::io::{self, BufWriter, Write};
use std::sync::Mutex;

/// 64 KiB 的缓冲（可按需调大/调小）
const BUF_CAP: usize = 64 * 1024;

static STDOUT_BUF: Lazy<Mutex<BufWriter<io::Stdout>>> =
    Lazy::new(|| Mutex::new(BufWriter::with_capacity(BUF_CAP, io::stdout())));
static STDERR_BUF: Lazy<Mutex<BufWriter<io::Stderr>>> =
    Lazy::new(|| Mutex::new(BufWriter::with_capacity(BUF_CAP, io::stderr())));

#[inline]
fn with_stdout<F: FnOnce(&mut BufWriter<io::Stdout>)>(f: F) {
    if let Ok(mut w) = STDOUT_BUF.lock() { let _ = f(&mut *w); }
}
#[inline]
fn with_stderr<F: FnOnce(&mut BufWriter<io::Stderr>)>(f: F) {
    if let Ok(mut w) = STDERR_BUF.lock() { let _ = f(&mut *w); }
}

/* ---------------------------
 * 供后端（extern "C"）调用的 API
 * --------------------------- */

#[unsafe(no_mangle)]
pub extern "C" fn print_u8(x: u8) { with_stdout(|w| { let _ = write!(w, "{x}"); }); }

#[unsafe(no_mangle)]
pub extern "C" fn print_i32(x: i32) { with_stdout(|w| { let _ = write!(w, "{x}"); }); }

#[unsafe(no_mangle)]
pub extern "C" fn print_i64(x: i64) { with_stdout(|w| { let _ = write!(w, "{x}"); }); }

#[unsafe(no_mangle)]
pub extern "C" fn print_f32(x: f32) { with_stdout(|w| { let _ = write!(w, "{}", x); }); }

#[unsafe(no_mangle)]
pub extern "C" fn print_f64(x: f64) { with_stdout(|w| { let _ = write!(w, "{}", x); }); }

#[unsafe(no_mangle)]
pub extern "C" fn print_bool(b: u8) {
    // 约定：0 = false, 非0 = true
    with_stdout(|w| { let _ = w.write_all(if b == 0 { b"false" } else { b"true" }); });
}

#[unsafe(no_mangle)]
pub extern "C" fn print_char(u: u32) {
    if let Some(ch) = char::from_u32(u) {
        let mut buf = [0_u8; 4];
        let s = ch.encode_utf8(&mut buf);
        with_stdout(|w| { let _ = w.write_all(s.as_bytes()); });
    }
}

/// 打印 UTF-8 字符串（指针须指向以 NUL 结尾的内存；由后端生成的字符串常量或切片）
#[unsafe(no_mangle)]
pub unsafe extern "C" fn print_str(s: *const c_char) {
    if !s.is_null() {
        unsafe {
            let s = match CStr::from_ptr(s).to_str() {
                Ok(text) => std::slice::from_raw_parts(text.as_ptr(), text.len()),
                Err(_) => {
                    let bytes = CStr::from_ptr(s).to_bytes();
                    std::slice::from_raw_parts(bytes.as_ptr(), bytes.len())
                },
            };
            with_stdout(|w| { let _ = w.write_all(s); });
        }
    }
}

/// 等价 println：仅写入 '\n'，**不** flush
#[unsafe(no_mangle)]
pub extern "C" fn rt_println() {
    with_stdout(|w| { let _ = w.write_all(b"\n"); });
}

/// 写入到 stderr 的一行（常用于报错），**不** flush
#[unsafe(no_mangle)]
pub extern "C" fn rt_eprintln() {
    with_stderr(|w| { let _ = w.write_all(b"\n"); });
}

/* ---- 手动 flush（按需调用） ---- */

#[unsafe(no_mangle)]
pub extern "C" fn flush_stdout() {
    if let Ok(mut w) = STDOUT_BUF.lock() { let _ = w.flush(); }
}

#[unsafe(no_mangle)]
pub extern "C" fn flush_stderr() {
    if let Ok(mut w) = STDERR_BUF.lock() { let _ = w.flush(); }
}

#[unsafe(no_mangle)]
pub extern "C" fn at_exit_flush() {
    // 尽量把缓冲的数据推到 OS
    if let Ok(mut w) = STDOUT_BUF.lock() { let _ = w.flush(); }
    if let Ok(mut w) = STDERR_BUF.lock() { let _ = w.flush(); }
}
