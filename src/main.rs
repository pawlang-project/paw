mod ast;
mod parse;
mod typecheck;
mod codegen;

use std::collections::HashMap;
use anyhow::Result;
use typecheck::{TyCk, FnSig};
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

    // 函数签名表 + 类型检查
    let mut fnsig = HashMap::<String, FnSig>::new();
    for f in &funs {
        fnsig.insert(f.name.clone(), FnSig {
            params: f.params.iter().map(|(_,t)| t.clone()).collect(),
            ret: f.ret.clone(),
        });
    }
    let mut ck = TyCk::new(&fnsig);
    for f in &funs { ck.check_fun(f)?; }

    // Cranelift 生成 .obj
    let mut cl = CLBackend::new()?;
    let ids = cl.declare_fns(&funs)?;
    for f in &funs { cl.define_fn(f, &ids)?; }
    let obj = cl.finish()?;
    std::fs::write("out.obj", obj)?;

    // 调 clang 链接成可执行文件（Windows）
    std::process::Command::new("clang")
        .args(["out.obj", "-o", "a.exe"])
        .status()?
        .success()
        .then_some(())
        .expect("clang link failed");
    println!("✅ built a.exe  （退出码可用 %ERRORLEVEL% 查看）");
    Ok(())
}