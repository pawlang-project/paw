// src/main.rs
use anyhow::{Context, Result};
use std::fs;
use std::path::Path;
use std::{env, process};

mod ast;
mod typecheck;
mod codegen;
mod mangle;
mod parser;
mod passes;
mod link_zig;
mod project;

use ast::{Item, Program};
use link_zig::{link_with_zig, LinkInput, PawTarget};
use project::{BuildProfile, Project};
use typecheck::typecheck_program;

/// 读取 entry 源码 -> 解析 -> 以工程根目录为搜索根展开 import
fn load_and_expand_program(entry: &Path, project_root: &Path) -> Result<Program> {
    let src = fs::read_to_string(entry)
        .with_context(|| format!("read_to_string({}) failed", entry.display()))?;

    let root = parser::parse_program(&src)
        .with_context(|| format!("parse `{}` failed", entry.display()))?;

    let merged = passes::expand_imports_with_loader(&root, project_root)
        .context("expand imports failed")?;

    Ok(merged)
}

/// 把完整 Program 编译为 object bytes
fn build_object(prog: &Program) -> Result<Vec<u8>> {
    // A) 类型检查：尽早发现 where / trait / impl / 泛型 等问题
    let _ = typecheck_program(prog)?;

    // B) 交给后端的统一入口（内部已经按正确顺序：
    //    set_globals -> declare_fns -> declare_impls -> declare_mono(显式) ->
    //    define_fns -> define_impls -> define_mono(显式) -> finish；
    //    同时支持 codegen 阶段对 println 等“隐式泛型”进行按需单态化）
    codegen::compile_program(prog)
}

fn main() -> Result<()> {
    // 仅支持：pawc build dev | pawc build release
    let args = env::args().skip(1).collect::<Vec<_>>();
    if args.len() != 2 || args[0].as_str() != "build" {
        eprintln!("usage: pawc build <dev|release>");
        process::exit(1);
    }

    let profile = match args[1].as_str() {
        "dev" => BuildProfile::Dev,
        "release" => BuildProfile::Release,
        _ => {
            eprintln!("usage: pawc build <dev|release>");
            process::exit(1);
        }
    };

    // 1) 载入工程（强依赖 project）—— 工程根目录 = 当前工作目录；入口 = <root>/main.paw
    let proj: Project = project::load_from_cwd()
        .context("failed to load project (Paw.toml or defaults)")?;

    // 2) 解析 + 展开 import（以工程根目录为搜索根）
    let entry = proj.entry.clone(); // e.g. <root>/main.paw
    let program = load_and_expand_program(&entry, &proj.root)?;

    // 3) 编译成 object bytes
    let obj_bytes = build_object(&program)?;

    // 4) 准备输出目录 layout：<root>/build/<profile>/
    let profile_dir = match profile {
        BuildProfile::Dev => "dev",
        BuildProfile::Release => "release",
    };
    let out_dir = proj.root.join("build").join(profile_dir);
    fs::create_dir_all(&out_dir)
        .with_context(|| format!("create_dir_all({})", out_dir.display()))?;

    // 5) 写对象文件与可执行文件
    let target = PawTarget::from_env_or_default();
    let obj_ext = target.obj_ext();
    let exe_ext = target.exe_ext();

    // 对象文件固定名 out.<ext>
    let obj_path = out_dir.join(format!("out.{}", obj_ext));
    fs::write(&obj_path, &obj_bytes)
        .with_context(|| format!("write object `{}` failed", obj_path.display()))?;

    // 可执行名：包名（或 fallback 到 "app"）
    let exe_stem = if proj.name.trim().is_empty() {
        "app".to_string()
    } else {
        proj.name.clone()
    };
    let exe_file = if exe_ext.is_empty() {
        exe_stem.clone()
    } else {
        format!("{}.{}", exe_stem, exe_ext)
    };
    let exe_path = out_dir.join(exe_file);

    // 6) 调用 zig 链接（自动附带 libruntime.a）
    let inp = LinkInput {
        obj_files: vec![obj_path.to_string_lossy().to_string()],
        out_exe: exe_path.to_string_lossy().to_string(),
        target,
    };
    link_with_zig(&inp).context("link step failed")?;

    eprintln!("OK: {} -> {}", entry.display(), exe_path.display());
    Ok(())
}
