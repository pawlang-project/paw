mod ast;
mod codegen;
mod link_zig;
mod parse;
mod typecheck;

use anyhow::Result;
use ast::{FunDecl, Item};
use std::{env, fs};

fn collect_fun_decls(p: &ast::Program) -> Vec<FunDecl> {
    p.items
        .iter()
        .filter_map(|it| {
            if let Item::Fun(f) = it {
                Some(f.clone())
            } else {
                None
            }
        })
        .collect()
}

fn main() -> Result<()> {
    let mut args = env::args().skip(1);
    let src_path = args
        .next()
        .unwrap_or_else(|| "examples/main.paw".to_string());
    let out_dir = args.next().unwrap_or_else(|| "build".to_string());
    let exe_name = args.next().unwrap_or_else(|| "pawlang".to_string());

    let src = fs::read_to_string(&src_path)?;
    let prog = parse::parse_program(&src)?;
    println!("{:#?}", prog);
    let (_fnsig, _globals) = typecheck::typecheck_program(&prog)?;

    let mut be = codegen::CLBackend::new()?;
    be.set_globals_from_program(&prog);

    let funs = collect_fun_decls(&prog);
    let ids = be.declare_fns(&funs)?;
    for f in &funs {
        be.define_fn(f, &ids)?;
    }
    let obj_bytes = be.finish()?;

    let (obj_path, exe_path) = link_zig::default_paths(&out_dir, &exe_name);
    // 直接把 runtime.c 作为输入给 zig cc（让它一并编译链接）
    link_zig::link_with_zig_cc(&obj_bytes, &obj_path, &exe_path, &["src/runtime/runtime.c"])?;
    println!("✅ built: {}", exe_path.display());
    Ok(())
}
