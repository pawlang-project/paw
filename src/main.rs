// src/main.rs
mod ast;
mod codegen;
mod link_zig;
mod parse;
mod typecheck;

use anyhow::{Context, Result};
use ast::{FunDecl, Item};
use link_zig::{LinkInput, PawTarget};
use std::{env, fs, path::PathBuf};

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

fn parse_target_from_args(rest: &[String]) -> PawTarget {
    if let Some(i) = rest.iter().position(|a| a == "--target") {
        if let Some(t) = rest.get(i + 1) {
            return PawTarget::parse(t);
        }
    }
    PawTarget::from_env_or_default()
}

fn main() -> Result<()> {
    // 兼容你原来的三个位置参数：<src.paw> [out_dir] [exe_name]，
    // 并额外支持 --target <triple>
    let mut it = env::args().skip(1);
    let src_path = it.next().unwrap_or_else(|| "examples/main.paw".to_string());
    let out_dir = it.next().unwrap_or_else(|| "build".to_string());
    let exe_name = it.next().unwrap_or_else(|| "pawlang".to_string());
    let rest = it.collect::<Vec<_>>();

    let target = parse_target_from_args(&rest);

    // 读取 & 前端
    let src = fs::read_to_string(&src_path)
        .with_context(|| format!("read_to_string({src_path})"))?;
    let prog = parse::parse_program(&src)?;
    println!("{:#?}", prog);
    let (_fnsig, _globals) = typecheck::typecheck_program(&prog)?;

    // 后端生成对象字节
    let mut be = codegen::CLBackend::new()?;
    be.set_globals_from_program(&prog);
    let funs = collect_fun_decls(&prog);
    let ids = be.declare_fns(&funs)?;
    for f in &funs {
        be.define_fn(f, &ids)?;
    }
    let obj_bytes = be.finish()?; // 这里应已按 target 产出匹配格式（COFF/ELF/Mach-O）

    // 输出路径：对象文件路径 + 可执行路径
    let (obj_stem, exe_path) = link_zig::default_paths(&out_dir, &exe_name);

    // 给对象文件加上平台扩展名（.obj / .o）
    let mut obj_path = obj_stem.clone();
    obj_path.set_extension(target.obj_ext());

    // 落盘对象
    if let Some(parent) = obj_path.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(&obj_path, &obj_bytes)
        .with_context(|| format!("write({})", obj_path.display()))?;

    // 链接（自动附带 libpawrt.a；不再编译 runtime.c）
    let inp = LinkInput {
        obj_files: vec![obj_path.to_string_lossy().into_owned()],
        out_exe: exe_path.to_string_lossy().into_owned(),
        target,
    };
    link_zig::link_with_zig(&inp)?;

    println!(
        "✅ built: {} (target: {})",
        exe_path.display(),
        target.zig_triple()
    );
    Ok(())
}
