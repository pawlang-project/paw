// src/main.rs
mod ast;
mod codegen;
mod link_zig;
mod parse;
mod typecheck;
mod desugar;

use anyhow::{anyhow, Context, Result};
use ast::{FunDecl, Item, Program};
use link_zig::{LinkInput, PawTarget};
use std::{
    collections::HashSet,
    env, fs,
    path::{Path, PathBuf},
};

// -------------------- 新增：import 递归展开 --------------------
fn parse_file(path: &Path) -> Result<Program> {
    let src = fs::read_to_string(path)
        .with_context(|| format!("read_to_string({})", path.display()))?;
    parse::parse_program(&src)
}

fn expand_file(path: &Path, visited: &mut HashSet<PathBuf>) -> Result<Vec<Item>> {
    let canon = fs::canonicalize(path).unwrap_or_else(|_| path.to_path_buf());
    if !visited.insert(canon.clone()) {
        return Err(anyhow!("cyclic import detected at {}", path.display()));
    }

    let prog0 = parse_file(path)?;
    let prog = desugar::desugar_program(prog0);
    println!("{:#?}", prog);
    let mut out = Vec::new();
    for it in prog.items {
        match it {
            Item::Import(spec) => {
                let base = path.parent().unwrap_or_else(|| Path::new("."));
                let target = {
                    let p = Path::new(&spec);
                    if p.is_absolute() { p.to_path_buf() } else { base.join(p) }
                };
                let mut sub = expand_file(&target, visited)
                    .with_context(|| format!("while importing `{}` from {}", spec, path.display()))?;
                out.append(&mut sub);
            }
            other => out.push(other),
        }
    }

    visited.remove(&canon);
    Ok(out)
}

fn load_program_with_imports(entry: &Path) -> Result<Program> {
    let mut visited = HashSet::new();
    let items = expand_file(entry, &mut visited)?;
    Ok(Program { items })
}

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
    // 兼容原来的三个位置参数：<src.paw> [out_dir] [exe_name]，并额外支持 --target <triple>
    let mut it = env::args().skip(1);
    let src_path = it.next().unwrap_or_else(|| "paw/main.paw".to_string());
    let out_dir = it.next().unwrap_or_else(|| "build".to_string());
    let exe_name = it.next().unwrap_or_else(|| "pawlang".to_string());
    let rest = it.collect::<Vec<_>>();

    let target = parse_target_from_args(&rest);

    // —— 改动点：用“带 import 展开”的加载函数 —— //
    let src_pathbuf = PathBuf::from(&src_path);
    let prog = load_program_with_imports(&src_pathbuf)?;
    println!("{:#?}", prog);

    // 类型检查
    let (_fnsig, _globals) = typecheck::typecheck_program(&prog)?;

    // 后端生成对象字节
    let mut be = codegen::CLBackend::new()?;
    be.set_globals_from_program(&prog);
    let funs = collect_fun_decls(&prog);
    let ids = be.declare_fns(&funs)?;
    for f in &funs {
        be.define_fn(f, &ids)?;
    }
    let obj_bytes = be.finish()?; // 已按 target 产出匹配格式（COFF/ELF/Mach-O）

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

    // 链接（自动附带 libpawrt；不再编译 runtime.c）
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
