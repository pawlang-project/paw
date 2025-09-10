mod ast;
mod parse;
mod typecheck;
mod codegen;
mod link_zig;

use std::collections::HashMap;
use std::path::PathBuf;
use anyhow::Result;
use typecheck::{TyCk, FnSig};
use crate::ast::{Expr, Stmt, Ty};
use crate::codegen::CLBackend;

fn main() -> Result<()> {
    // 你的源代码（可读取文件）
    let src = r#"
        const N: Int = 10;

        fn add(a: Int, b: Int) -> Int { a + b }

        fn main() -> Int {
            let x: Int = 32;
            let y: Int = add(x, N);
            if (y > 40) { 0 } else { 1 }
        }
    "#;

    // 解析
    let (funs, tops) = parse::parse_program(src)?;
    println!("top-level lets: {tops:#?}");
    for f in &funs {
        println!("fn {}(...) -> {:?}\n{:#?}", f.name, f.ret, f.body);
    }

    // 1) 函数签名表
    let mut fnsig = HashMap::<String, FnSig>::new();
    for f in &funs {
        fnsig.insert(
            f.name.clone(),
            FnSig {
                params: f.params.iter().map(|(_, t)| t.clone()).collect(),
                ret: f.ret.clone(),
            },
        );
    }

    // 2) 全局符号（仅把顶层 let/const 的类型加入）
    let mut globals = HashMap::<String, Ty>::new();
    for s in &tops {
        if let Stmt::Let { name, ty, .. } = s {
            globals.insert(name.clone(), ty.clone());
        }
    }

    println!("globals: {globals:#?}");

    // 3) 运行类型检查（可先检查全局 init 的表达式类型是否与声明一致——看你是否需要）
    let mut ck = TyCk::new(&fnsig, globals);
    for f in &funs {
        ck.check_fun(f)?;
    }

    // Cranelift 生成 .obj
    let mut cl = CLBackend::new()?;
    cl.set_globals_from_tops(&tops);
    let ids = cl.declare_fns(&funs)?;
    for f in &funs { cl.define_fn(f, &ids)?; }
    let obj_bytes = cl.finish()?;

    let out_obj = PathBuf::from("build/out.o");   // macOS/Linux 为 .o；Windows 用 .obj 也没关系
    let out_exe = if cfg!(target_os = "windows") {
        PathBuf::from("build/pawlang.exe")
    } else {
        PathBuf::from("build/pawlang")
    };

    let (obj_path, exe_path) = link_zig::default_paths("build", "pawlang");
    link_zig::link_with_zig_cc(&obj_bytes, &obj_path, &exe_path, None, &[])?;
    println!("✅ built executable: {}", out_exe.display());
    Ok(())
}