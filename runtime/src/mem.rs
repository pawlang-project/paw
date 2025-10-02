#![allow(non_snake_case)]
use core::ffi::{c_char, c_void};
use std::sync::atomic::{AtomicU64, Ordering};

type PawByte  = i8;   // Paw Byte  -> I8
type PawBool  = i8;   // Paw Bool  -> I8 (0/1)
type PawInt   = i32;  // Paw Int   -> I32
type PawLong  = i64;  // Paw Long  -> I64
type PawChar  = i32;  // Paw Char  -> I32 (Unicode scalar as i32)
type PawFloat = f32;  // Paw Float -> F32
type PawDouble= f64;  // Paw Double-> F64
type PawStr   = *mut c_char; // Paw String -> I64 pointer

// ======== 生成每种 T 的 Rc/Arc 后备结构 ========
macro_rules! gen_rc_arc {
    ($modname:ident, $T:ty) => {
        mod $modname {
            use super::*;
            #[repr(C)]
            pub struct RcCell { pub cnt: u64, pub val: $T }
            #[repr(C)]
            pub struct ArcCell { pub cnt: AtomicU64, pub val: $T }

            // ---------- Box<T> ----------
            #[unsafe(export_name = concat!("box_new_", stringify!($modname)))]
            pub extern "C" fn box_new(x: $T) -> *mut c_void {
                let b = Box::new(x);
                Box::into_raw(b) as *mut c_void
            }
            #[unsafe(export_name = concat!("box_get_", stringify!($modname)))]
            pub extern "C" fn box_get(b: *mut c_void) -> $T {
                unsafe { *(b as *mut $T) }
            }
            #[unsafe(export_name = concat!("box_set_", stringify!($modname)))]
            pub extern "C" fn box_set(b: *mut c_void, x: $T) {
                unsafe { *(b as *mut $T) = x; }
            }
            #[unsafe(export_name = concat!("box_free_", stringify!($modname)))]
            pub extern "C" fn box_free(b: *mut c_void) {
                if !b.is_null() {
                    unsafe { let _ = Box::from_raw(b as *mut $T); } // drop
                }
            }

            // ---------- Rc<T> ----------
            #[unsafe(export_name = concat!("rc_new_", stringify!($modname)))]
            pub extern "C" fn rc_new(x: $T) -> *mut c_void {
                let cell = RcCell { cnt: 1, val: x };
                Box::into_raw(Box::new(cell)) as *mut c_void
            }
            #[unsafe(export_name = concat!("rc_clone_", stringify!($modname)))]
            pub extern "C" fn rc_clone(b: *mut c_void) -> *mut c_void {
                unsafe {
                    let p = b as *mut RcCell;
                    (*p).cnt = (*p).cnt.saturating_add(1);
                }
                b
            }
            #[unsafe(export_name = concat!("rc_get_", stringify!($modname)))]
            pub extern "C" fn rc_get(b: *mut c_void) -> $T {
                unsafe { (*(b as *mut RcCell)).val }
            }
            #[unsafe(export_name = concat!("rc_set_", stringify!($modname)))]
            pub extern "C" fn rc_set(b: *mut c_void, x: $T) {
                unsafe { (*(b as *mut RcCell)).val = x; }
            }
            #[unsafe(export_name = concat!("rc_drop_", stringify!($modname)))]
            pub extern "C" fn rc_drop(b: *mut c_void) {
                if b.is_null() { return; }
                unsafe {
                    let p = b as *mut RcCell;
                    (*p).cnt -= 1;
                    if (*p).cnt == 0 {
                        let _ = Box::from_raw(p); // drop
                    }
                }
            }
            #[unsafe(export_name = concat!("rc_strong_count_", stringify!($modname)))]
            pub extern "C" fn rc_strong_count(b: *mut c_void) -> i32 {
                if b.is_null() { return 0; }
                unsafe { (*(b as *mut RcCell)).cnt as i32 }
            }

            // ---------- Arc<T> ----------
            #[unsafe(export_name = concat!("arc_new_", stringify!($modname)))]
            pub extern "C" fn arc_new(x: $T) -> *mut c_void {
                let cell = ArcCell { cnt: AtomicU64::new(1), val: x };
                Box::into_raw(Box::new(cell)) as *mut c_void
            }
            #[unsafe(export_name = concat!("arc_clone_", stringify!($modname)))]
            pub extern "C" fn arc_clone(b: *mut c_void) -> *mut c_void {
                if !b.is_null() {
                    unsafe { (*(b as *mut ArcCell)).cnt.fetch_add(1, Ordering::Relaxed); }
                }
                b
            }
            #[unsafe(export_name = concat!("arc_get_", stringify!($modname)))]
            pub extern "C" fn arc_get(b: *mut c_void) -> $T {
                unsafe { (*(b as *mut ArcCell)).val }
            }
            #[unsafe(export_name = concat!("arc_set_", stringify!($modname)))]
            pub extern "C" fn arc_set(b: *mut c_void, x: $T) {
                unsafe { (*(b as *mut ArcCell)).val = x; }
            }
            #[unsafe(export_name = concat!("arc_drop_", stringify!($modname)))]
            pub extern "C" fn arc_drop(b: *mut c_void) {
                if b.is_null() { return; }
                unsafe {
                    let p = b as *mut ArcCell;
                    if (*p).cnt.fetch_sub(1, Ordering::AcqRel) == 1 {
                        let _ = Box::from_raw(p); // drop
                    }
                }
            }
            #[unsafe(export_name = concat!("arc_strong_count_", stringify!($modname)))]
            pub extern "C" fn arc_strong_count(b: *mut c_void) -> i32 {
                if b.is_null() { return 0; }
                unsafe { (*(b as *mut ArcCell)).cnt.load(Ordering::Relaxed) as i32 }
            }
        }
    };
}

// 依次为每种类型实例化：
// 注意：模块名用于组成符号后缀；必须与 Paw 侧后缀一致：Byte/Int/Long/Bool/Float/Double/Char/String
gen_rc_arc!(Byte,   PawByte);
gen_rc_arc!(Int,    PawInt);
gen_rc_arc!(Long,   PawLong);
gen_rc_arc!(Bool,   PawBool);
gen_rc_arc!(Float,  PawFloat);
gen_rc_arc!(Double, PawDouble);
gen_rc_arc!(Char,   PawChar);
gen_rc_arc!(String, PawStr);
