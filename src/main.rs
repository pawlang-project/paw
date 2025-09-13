// src/main.rs
use anyhow::{Context, Result};
use std::fs;
use std::path::{Path, PathBuf};
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
use codegen::CLBackend;
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

    // B) 初始化后端，收集可内联常量
    let mut be = CLBackend::new()?;
    be.set_globals_from_program(prog);

    // C) 声明所有“非泛型/extern”函数（含运行时外部 API）
    let funs: Vec<_> = prog
        .items
        .iter()
        .filter_map(|it| if let Item::Fun(f) = it { Some(f.clone()) } else { None })
        .collect();
    let base_ids = be.declare_fns(&funs)?;

    // D) 声明所有 impl 方法符号（__impl_{Trait}${Args}__{method}）
    be.declare_impls_from_program(prog)?;

    // E) 扫描所有**带显式类型实参**的泛型调用，声明对应的单态化实例函数
    be.declare_mono_from_program(prog)?;

    // F) 定义普通（非泛型、非 extern）函数体
    for f in &funs {
        be.define_fn(f, &base_ids)?;
    }

    // G) 定义 impl 方法体（把每个 ImplMethod 降为我们在 C 步骤声明的自由函数）
    be.define_impls_from_program(prog)?;

    // H) 定义之前声明的所有单态化实例函数体
    be.define_mono_from_program(prog, &base_ids)?;

    // I) 产出 .o 的字节
    be.finish()
}

fn main() -> Result<()> {
    // 仅支持：pawc build dev | pawc build release
    let mut args = env::args().skip(1).collect::<Vec<_>>();
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

    // 6) 调用 zig 链接（自动附带 libpawrt.a）
    let inp = LinkInput {
        obj_files: vec![obj_path.to_string_lossy().to_string()],
        out_exe: exe_path.to_string_lossy().to_string(),
        target,
    };
    link_with_zig(&inp).context("link step failed")?;

    eprintln!("OK: {} -> {}", entry.display(), exe_path.display());
    Ok(())
}
